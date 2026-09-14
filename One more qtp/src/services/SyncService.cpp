#include "services/SyncService.h"

#include "core/AppLayout.h"
#include "services/WebDavClient.h"
#include "storage/MetadataStore.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>

namespace litapp {

namespace {

// ============================================================
// HTTP 日期
// ============================================================

QDateTime parseHttpDate(const QString &value)
{
    const QString trimmed = value.trimmed();

    if (trimmed.isEmpty()) {
        return QDateTime();
    }

    // "Wed, 21 Oct 2015 07:28:00 GMT"
    QDateTime result =
        QDateTime::fromString(
            trimmed,
            Qt::RFC2822Date);

    if (result.isValid()) {
        return result;
    }

    // ISO 8601
    result = QDateTime::fromString(
        trimmed,
        Qt::ISODate);

    if (result.isValid()) {
        return result;
    }

    const QStringList formats {
        QStringLiteral("ddd, dd MMM yyyy HH:mm:ss 'GMT'"),
        QStringLiteral("dddd, dd-MMM-yy HH:mm:ss 'GMT'"),
        QStringLiteral("ddd MMM d HH:mm:ss yyyy"),
        QStringLiteral("yyyy-MM-ddTHH:mm:ssZ")
    };

    for (const QString &format : formats) {

        result = QDateTime::fromString(
            trimmed,
            format);

        if (result.isValid()) {
            return result;
        }
    }

    return QDateTime();
}

// ============================================================
// 名称安全
// ============================================================

// /a/b/c -> { /a, /a/b, /a/b/c }
QStringList ancestorPaths(const QString &remotePath)
{
    const QString normalized =
        layout::normalizeRemotePath(remotePath);

    const QStringList parts =
        normalized.split(
            QLatin1Char('/'),
            Qt::SkipEmptyParts);

    QStringList result;
    QString current;

    for (const QString &part : parts) {

        current += QLatin1Char('/') + part;
        result.append(current);
    }

    return result;
}

bool isHiddenOrSystemEntry(const QString &name)
{
    return name.startsWith(QLatin1Char('.'))
        || name.compare(
               QStringLiteral("desktop.ini"),
               Qt::CaseInsensitive) == 0;
}

} // namespace

// ============================================================
// Constructor / Destructor
// ============================================================

SyncService::SyncService(
    WebDavClient *ownClient,
    QObject *parent)
    : QObject(parent)
    , m_ownClient(ownClient)
    , m_timer(new QTimer(this))
{
    if (m_ownClient) {

        connect(
            m_ownClient,
            &WebDavClient::directoryListed,
            this,
            &SyncService::onDirectoryListed);

        connect(
            m_ownClient,
            &WebDavClient::directoryCreated,
            this,
            &SyncService::onDirectoryCreated);

        connect(
            m_ownClient,
            &WebDavClient::uploadFinished,
            this,
            &SyncService::onUploadFinished);

        connect(
            m_ownClient,
            &WebDavClient::downloadFinished,
            this,
            &SyncService::onDownloadFinished);

        connect(
            m_ownClient,
            &WebDavClient::removeFinished,
            this,
            &SyncService::onRemoveFinished);
    }

    m_timer->setInterval(180000);

    connect(
        m_timer,
        &QTimer::timeout,
        this,
        &SyncService::onAutoSyncTimeout);
}

SyncService::~SyncService() = default;

// ============================================================
// Database
// ============================================================

void SyncService::setDatabasePath(
    const QString &databasePath)
{
    if (!databasePath.trimmed().isEmpty()) {
        m_databasePath = databasePath;
    }
}

QString SyncService::databasePath() const
{
    return m_databasePath;
}

bool SyncService::openStore(
    MetadataStore &store) const
{
    return store.open(m_databasePath);
}

// ============================================================
// WebDavClient 解析
// ============================================================

WebDavClient *SyncService::clientForSource(
    const QString &sourceId)
{
    if (sourceId.trimmed().isEmpty()) {
        return m_ownClient;
    }

    if (m_sourceClients.contains(sourceId)) {
        return m_sourceClients.value(sourceId);
    }

    MetadataStore store;

    if (!openStore(store)) {
        return nullptr;
    }

    const GroupSource source =
        store.sourceById(sourceId);

    if (!source.isValid()) {
        return nullptr;
    }

    auto *client = new WebDavClient(this);

    client->configureTransient(
        source.serverUrl,
        source.username,
        source.password);

    connect(
        client,
        &WebDavClient::directoryListed,
        this,
        &SyncService::onDirectoryListed);

    connect(
        client,
        &WebDavClient::directoryCreated,
        this,
        &SyncService::onDirectoryCreated);

    connect(
        client,
        &WebDavClient::uploadFinished,
        this,
        &SyncService::onUploadFinished);

    connect(
        client,
        &WebDavClient::downloadFinished,
        this,
        &SyncService::onDownloadFinished);

    connect(
        client,
        &WebDavClient::removeFinished,
        this,
        &SyncService::onRemoveFinished);

    m_sourceClients.insert(
        sourceId,
        client);

    return client;
}

bool SyncService::isJobClient() const
{
    return m_job.active
        && m_job.client != nullptr
        && sender() == m_job.client;
}

bool SyncService::isSyncing() const
{
    return m_job.active;
}

QString SyncService::jobLabel() const
{
    switch (m_job.space) {

    case FileSpace::MyDoc:
        return QStringLiteral("my doc");

    case FileSpace::GroupMe:
        return m_job.groupId.isEmpty()
            ? QStringLiteral("group me")
            : m_job.groupId;

    case FileSpace::GroupUs:
        return QStringLiteral("group us / %1")
            .arg(m_job.sourceId);
    }

    return QStringLiteral("unknown");
}

// ============================================================
// 同步入口
// ============================================================

void SyncService::syncMyDoc()
{
    startJob(
        FileSpace::MyDoc,
        QString(),
        QString(),
        false);
}

void SyncService::syncGroupMe(
    const QString &groupId)
{
    const QString directory =
        groupId.trimmed().isEmpty()
            ? layout::groupMeDirectoryName(0)
            : groupId.trimmed();

    startJob(
        FileSpace::GroupMe,
        directory,
        QString(),
        false);
}

void SyncService::syncGroupUs(
    const QString &sourceId)
{
    if (sourceId.trimmed().isEmpty()) {

        emit syncFinished(
            false,
            QStringLiteral(
                "group us 缺少来源标识"));

        return;
    }

    startJob(
        FileSpace::GroupUs,
        QString(),
        sourceId.trimmed(),
        true);
}

void SyncService::enqueueJob(
    FileSpace space,
    const QString &groupId,
    const QString &sourceId,
    bool readOnly)
{
    PendingJob job;

    job.space = space;
    job.groupId = groupId;
    job.sourceId = sourceId;
    job.readOnly = readOnly;

    m_pendingJobs.append(job);

    startNextQueuedJob();
}

void SyncService::startNextQueuedJob()
{
    if (m_job.active
        || m_deleting
        || m_pendingJobs.isEmpty()) {

        return;
    }

    const PendingJob next =
        m_pendingJobs.takeFirst();

    startJob(
        next.space,
        next.groupId,
        next.sourceId,
        next.readOnly);
}

/*
 * 我的账号：
 *
 *     my doc
 *     + 每一个已存在的 group me 目录
 */
void SyncService::syncMyAccount()
{
    if (m_job.active || m_deleting) {
        return;
    }

    m_pendingJobs.clear();

    enqueueJob(
        FileSpace::MyDoc,
        QString(),
        QString(),
        false);

    // ---- 已存在的群组目录 ----

    QStringList groups;

    MetadataStore store;

    if (openStore(store)) {

        const QVector<FileItem> items =
            store.files(FileSpace::GroupMe);

        for (const FileItem &item : items) {

            if (item.groupId.isEmpty()) {
                continue;
            }

            if (!groups.contains(item.groupId)) {
                groups.append(item.groupId);
            }
        }
    }

    if (groups.isEmpty()) {
        groups.append(layout::groupMeDirectoryName(0));
    }

    for (const QString &groupId : groups) {

        enqueueJob(
            FileSpace::GroupMe,
            groupId,
            QString(),
            false);
    }
}

void SyncService::syncAll()
{
    syncMyAccount();
}

void SyncService::syncSpace(
    int space,
    const QString &sourceId)
{
    switch (static_cast<FileSpace>(space)) {

    case FileSpace::MyDoc:
        syncMyDoc();
        return;

    case FileSpace::GroupMe:
        syncGroupMe(sourceId);
        return;

    case FileSpace::GroupUs:
        syncGroupUs(sourceId);
        return;
    }

    emit syncFinished(
        false,
        QStringLiteral("无效的同步空间"));
}

/*
 * 群组目录的远端根。
 *
 * 兼容两种布局：
 *
 *     zesbks/group/group me/   （默认，见 AppLayout）
 *     zesbks/group me/
 *
 * 依据最近一次扫描 /zesbks 时实际看到的子目录判断。
 * 没扫描过时用默认布局。
 */
QString SyncService::groupRootFor(
    const QString &groupId) const
{
    const QString share =
        layout::remoteShareRoot();

    const QString directory =
        groupId.trimmed().isEmpty()
            ? layout::groupMeDirectoryName(0)
            : groupId.trimmed();

    const QString container =
        QStringLiteral("group");

    if (m_shareRootChildren.contains(container)) {

        return share + QLatin1Char('/')
            + container + QLatin1Char('/') + directory;
    }

    if (m_shareRootChildren.contains(directory)) {

        return share + QLatin1Char('/') + directory;
    }

    // 默认布局
    return share + QLatin1Char('/')
        + container + QLatin1Char('/') + directory;
}

// ============================================================
// 启动一次任务
// ============================================================

bool SyncService::startJob(
    FileSpace space,
    const QString &groupId,
    const QString &sourceId,
    bool readOnly)
{
    if (m_job.active) {

        qWarning()
            << "SyncService: 已有同步任务在进行";

        return false;
    }

    if (m_deleting) {

        qWarning()
            << "SyncService: 正在执行远端删除";

        return false;
    }

    WebDavClient *client =
        clientForSource(sourceId);

    if (!client) {

        emit syncFinished(
            false,
            QStringLiteral(
                "无法建立 WebDAV 连接（来源：%1）")
                .arg(
                    sourceId.isEmpty()
                        ? QStringLiteral("我的账号")
                        : sourceId));

        return false;
    }

    QString remoteRoot;

    switch (space) {

    case FileSpace::MyDoc:
        remoteRoot = layout::remoteShareRoot();
        break;

    case FileSpace::GroupMe:
        remoteRoot = groupRootFor(
            groupId.isEmpty()
                ? layout::groupMeDirectoryName(0)
                : groupId);
        break;

    case FileSpace::GroupUs:
    {
        MetadataStore store;

        if (!openStore(store)) {

            emit syncFinished(
                false,
                QStringLiteral("无法打开本地数据库"));

            return false;
        }

        const GroupSource source =
            store.sourceById(sourceId);

        if (!source.isValid() || source.remoteRoot.isEmpty()) {

            emit syncFinished(
                false,
                QStringLiteral(
                    "群组来源 %1 未正确配置")
                    .arg(sourceId));

            return false;
        }

        remoteRoot = source.remoteRoot;
        break;
    }
    }

    remoteRoot =
        layout::normalizeRemotePath(remoteRoot);

    // ---- 重置任务 ----

    m_job = Job();

    m_job.active = true;
    m_job.space = space;
    m_job.groupId =
        space == FileSpace::GroupMe
            ? (groupId.isEmpty()
                   ? layout::groupMeDirectoryName(0)
                   : groupId)
            : QString();
    m_job.sourceId = sourceId;
    m_job.remoteRoot = remoteRoot;
    m_job.readOnly = readOnly
        || space == FileSpace::GroupUs;
    m_job.client = client;

    qDebug()
        << "========================================";
    qDebug() << "SyncService: 开始同步" << jobLabel();
    qDebug() << "remoteRoot =" << remoteRoot;
    qDebug() << "readOnly   =" << m_job.readOnly;
    qDebug() << "========================================";

    beginRemoteScan();

    return true;
}

// ============================================================
// 远端扫描
// ============================================================

void SyncService::beginRemoteScan()
{
    if (!m_job.active || !m_job.client) {
        return;
    }

    m_job.visited.clear();
    m_job.folders.clear();
    m_job.folderIndexByPath.clear();
    m_job.pendingListings = 0;

    requestFolderListing(
        m_job.remoteRoot);
}

void SyncService::requestFolderListing(
    const QString &remotePath)
{
    const QString normalized =
        layout::normalizeRemotePath(remotePath);

    if (m_job.visited.contains(normalized)) {
        return;
    }

    m_job.visited.insert(normalized);

    ++m_job.pendingListings;

    m_job.client->listDirectory(normalized);
}

QVector<SyncService::RemoteEntry> SyncService::toRemoteEntries(
    const QVariantList &entries)
{
    QVector<RemoteEntry> result;

    result.reserve(entries.size());

    for (const QVariant &value : entries) {

        const QVariantMap map =
            value.toMap();

        const QString rawPath =
            map.value(
                   QStringLiteral("path"))
                .toString();

        if (rawPath.isEmpty()) {
            continue;
        }

        RemoteEntry entry;

        entry.path =
            layout::normalizeRemotePath(rawPath);

        entry.name =
            map.value(
                   QStringLiteral("name"))
                .toString();

        if (entry.name.isEmpty()) {

            const int slash =
                entry.path.lastIndexOf(
                    QLatin1Char('/'));

            entry.name =
                slash >= 0
                    ? entry.path.mid(slash + 1)
                    : entry.path;
        }

        entry.isDirectory =
            map.value(
                   QStringLiteral("isDirectory"))
                .toBool();

        entry.size =
            map.value(
                   QStringLiteral("size"))
                .toLongLong();

        entry.modified =
            parseHttpDate(
                map.value(
                       QStringLiteral("modified"))
                    .toString());

        result.append(entry);
    }

    return result;
}

void SyncService::onDirectoryListed(
    const QString &path,
    const QVariantList &entries,
    const QString &error)
{
    if (!isJobClient()) {
        return;
    }

    if (m_job.pendingListings > 0) {
        --m_job.pendingListings;
    }

    const QString normalized =
        layout::normalizeRemotePath(path);

    const bool isRootListing =
        normalized == m_job.remoteRoot;

    if (!error.isEmpty()) {

        qWarning()
            << "SyncService: PROPFIND 失败"
            << normalized
            << error;

        if (isRootListing) {

            /*
             * 自己的账号：根目录读不到，最常见的原因是
             * 它还没有在远端创建。
             *
             * 先把中间层级补齐，再重扫一次，
             * 这样首次连接、或群组布局不同都能自愈。
             */
            if (!m_job.readOnly
                && !m_job.bootstrappedRoot) {

                m_job.bootstrappedRoot = true;

                const QStringList ancestors =
                    ancestorPaths(m_job.remoteRoot);

                if (!ancestors.isEmpty()) {

                    qDebug()
                        << "SyncService: 尝试补建远端根目录"
                        << ancestors;

                    m_job.directoryQueue = ancestors;
                    m_job.directoryIndex = 0;
                    m_job.bootstrappingRoot = true;

                    createNextDirectory();

                    return;
                }
            }

            /*
             * 补建之后仍然读不到，如实上报。
             */
            m_job.hasError = true;

            if (m_job.error.isEmpty()) {

                QString message =
                    QStringLiteral("远端根目录 %1 读取失败：%2")
                        .arg(
                            normalized,
                            error);

                const bool looksLikeWebPage =
                    error.contains(
                        QStringLiteral("不是 WebDAV multistatus"))
                    || error.contains(
                        QStringLiteral("不是有效的 XML"));

                if (looksLikeWebPage) {

                    message += QStringLiteral(
                        "\n提示：该地址返回的是网页而不是 WebDAV 内容,"
                        " 请确认服务器地址指向 DAV 入口。");
                }

                m_job.error = message;
            }
        }

    } else {

        const QVector<RemoteEntry> remoteEntries =
            toRemoteEntries(entries);

        if (isRootListing) {

            /*
             * 记录 /zesbks 下实际存在的子目录，
             * 用来判断群组目录的真实布局。
             */
            if (normalized == layout::remoteShareRoot()) {

                QSet<QString> children;

                for (const RemoteEntry &entry : remoteEntries) {

                    if (entry.isDirectory
                        && entry.path != normalized) {

                        children.insert(entry.name);
                    }
                }

                m_shareRootChildren = children;
            }

            for (const RemoteEntry &entry : remoteEntries) {

                if (!entry.isDirectory) {
                    continue;
                }

                if (entry.path == normalized) {
                    continue;
                }

                /*
                 * my doc 的根是 /zesbks，
                 * 它下面还有 /zesbks/group。
                 *
                 * 这个容器不是原子文件夹。
                 */
                if (layout::isReservedShareEntry(entry.name)) {
                    continue;
                }

                RemoteFolder folder;

                folder.name = entry.name;
                folder.remotePath = entry.path;

                m_job.folderIndexByPath.insert(
                    entry.path,
                    m_job.folders.size());

                m_job.folders.append(folder);

                requestFolderListing(entry.path);
            }

        } else {

            const int index =
                m_job.folderIndexByPath.value(
                    normalized,
                    -1);

            if (index >= 0
                && index < m_job.folders.size()) {

                QVector<RemoteEntry> files;

                for (const RemoteEntry &entry : remoteEntries) {

                    if (entry.isDirectory) {
                        continue;
                    }

                    if (entry.path == normalized) {
                        continue;
                    }

                    if (isHiddenOrSystemEntry(entry.name)) {
                        continue;
                    }

                    files.append(entry);
                }

                m_job.folders[index].files = files;
            }
        }
    }

    if (m_job.pendingListings > 0) {
        return;
    }

    afterListingComplete();
}

// ============================================================
// 扫描结束
// ============================================================

void SyncService::afterListingComplete()
{
    if (!m_job.active) {
        return;
    }

    qDebug()
        << "SyncService: 远端发现"
        << m_job.folders.size()
        << "个原子文件夹";

    // ---- 只读来源：只拉取，到此结束 ----

    if (m_job.readOnly) {

        reconcileWithStore();

        if (m_job.hasError) {

            finishJob(false, m_job.error);

        } else {

            finishJob(
                true,
                QStringLiteral("%1 已更新（只读）")
                    .arg(jobLabel()));
        }

        return;
    }

    /*
     * ---- 先清理墓碑 ----
     *
     * 本地删除 / 重命名后残留的远端路径
     * 必须先删掉，否则 reconcile 会把它们
     * 当成「远端新增」重新拉回应用。
     */

    m_job.pendingDeleteQueue.clear();
    m_job.pendingDeleteIndex = 0;
    m_job.drainingDeletes = false;

    MetadataStore store;

    if (openStore(store)) {

        m_job.pendingDeleteQueue =
            store.pendingDeletes(m_job.sourceId);
    }

    if (!m_job.pendingDeleteQueue.isEmpty()) {

        m_job.drainingDeletes = true;
        deleteNextPendingDelete();

        return;
    }

    continueAfterDeletes();
}

// ============================================================
// 墓碑删除之后继续同步
// ============================================================

void SyncService::continueAfterDeletes()
{
    if (!m_job.active) {
        return;
    }

    // ---- 远端 -> 应用内 ----

    reconcileWithStore();

    // ---- 本地 -> 远端 ----

    prepareUploads();

    if (!m_job.directoryQueue.isEmpty()) {

        startDirectoryCreation();

        return;
    }

    startUploads();
}

// ============================================================
// 删除墓碑指向的远端残留
// ============================================================

void SyncService::deleteNextPendingDelete()
{
    if (!m_job.active || !m_job.drainingDeletes) {
        return;
    }

    if (m_job.pendingDeleteIndex
        >= m_job.pendingDeleteQueue.size()) {

        m_job.drainingDeletes = false;

        continueAfterDeletes();

        return;
    }

    const QPair<QString, bool> entry =
        m_job.pendingDeleteQueue.at(
            m_job.pendingDeleteIndex);

    ++m_job.pendingDeleteIndex;

    qDebug()
        << "SyncService: 清理远端残留"
        << entry.first
        << (entry.second ? "(目录)" : "(文件)");

    m_job.client->removeFile(
        entry.first,
        entry.second);
}

// ============================================================
// 查询辅助
// ============================================================

QVector<FileItem> SyncService::storeChildren(
    const QString &parentId,
    FileSpace space,
    const QString &sourceId,
    const QString &groupId) const
{
    QVector<FileItem> result;

    MetadataStore store;

    if (!openStore(store)) {
        return result;
    }

    const QVector<FileItem> children =
        store.childrenOf(
            parentId,
            space);

    for (const FileItem &child : children) {

        if ((child.sourceId) != sourceId) {
            continue;
        }

        if (space == FileSpace::GroupMe
            && child.groupId != groupId) {
            continue;
        }

        result.append(child);
    }

    return result;
}

QVector<FileItem> SyncService::allItemsOfJob() const
{
    QVector<FileItem> result;

    MetadataStore store;

    if (!openStore(store)) {
        return result;
    }

    const QVector<FileItem> all =
        store.files(m_job.space);

    for (const FileItem &item : all) {

        if (item.sourceId != m_job.sourceId) {
            continue;
        }

        if (m_job.space == FileSpace::GroupMe
            && item.groupId != m_job.groupId) {
            continue;
        }

        result.append(item);
    }

    return result;
}

FileItem SyncService::findAtomicFolderByName(
    const QString &name) const
{
    const QVector<FileItem> items =
        allItemsOfJob();

    for (const FileItem &item : items) {

        if (item.objectType != FileObjectType::AtomicFolder) {
            continue;
        }

        if (item.name.compare(
                name,
                Qt::CaseSensitive) == 0) {

            return item;
        }
    }

    return FileItem();
}

// ============================================================
// 物化：远端 -> SQLite
// ============================================================

void SyncService::reconcileWithStore()
{
    MetadataStore store;

    if (!openStore(store)) {
        return;
    }

    /*
     * 只有在远端扫描可信时才允许「删除本地」。
     *
     * 否则一旦服务器返回的是登录页、错误页或任何
     * 非 WebDAV 内容，扫描结果会被当成「远端为空」，
     * 把用户整个库的元数据清空。
     */
    const bool mayPrune = !m_job.hasError;

    if (!mayPrune) {
        qWarning()
            << "SyncService: 远端扫描不可信，跳过本地清理";
    }

    /*
     * 应用内的原子文件夹一律挂在
     * 对应空间的根上（parentId 为空）。
     *
     * 用户之后可以把它移进子分类文件夹，
     * 那只是改 parentId，与远端无关。
     */

    // ---- 现有的一级对象 ----

    QHash<QString, FileItem> existingFolders;

    const QVector<FileItem> roots =
        storeChildren(
            QString(),
            m_job.space,
            m_job.sourceId,
            m_job.groupId);

    for (const FileItem &item : roots) {

        if (item.objectType
            != FileObjectType::AtomicFolder) {
            continue;
        }

        existingFolders.insert(
            item.name,
            item);
    }

    /*
     * 仍带墓碑的远端路径暂时不算「远端存在」，
     * 避免删除失败时被重新拉回应用。
     */
    QSet<QString> tombstoned;

    {
        const auto pending =
            store.pendingDeletes(m_job.sourceId);

        for (const auto &entry : pending) {
            tombstoned.insert(entry.first);
        }
    }

    QSet<QString> remoteNames;

    for (const RemoteFolder &folder : m_job.folders) {

        if (tombstoned.contains(folder.remotePath)) {
            continue;
        }

        remoteNames.insert(folder.name);
    }

    // ---- 远端有 / 本地没有 -> 新增；两边都有 -> 更新 ----

    for (const RemoteFolder &folder : m_job.folders) {

        if (tombstoned.contains(folder.remotePath)) {
            continue;
        }

        FileItem atomicFolder =
            existingFolders.value(folder.name);

        if (atomicFolder.isValid()) {

            atomicFolder.remotePath = folder.remotePath;
            atomicFolder.syncState = SyncState::Synced;
            atomicFolder.syncError.clear();

            store.updateFile(atomicFolder);

        } else {

            atomicFolder = FileItem();

            atomicFolder.id =
                QUuid::createUuid().toString(
                    QUuid::WithoutBraces);

            atomicFolder.parentId = QString();

            atomicFolder.name = folder.name;

            atomicFolder.objectType =
                FileObjectType::AtomicFolder;

            atomicFolder.space = m_job.space;
            atomicFolder.sourceId = m_job.sourceId;
            atomicFolder.groupId = m_job.groupId;

            atomicFolder.remotePath = folder.remotePath;

            /*
             * localPath 为空表示：
             *
             *     尚未下载，属于云端条目。
             *
             * 双击时才按镜像路径下载。
             */
            atomicFolder.localPath = QString();

            atomicFolder.syncState = SyncState::Synced;

            atomicFolder.modifiedTime =
                QDateTime::currentDateTime();

            store.addFile(atomicFolder);
        }

        // ---- 原子文件 ----

        QHash<QString, FileItem> existingFiles;

        const QVector<FileItem> children =
            storeChildren(
                atomicFolder.id,
                m_job.space,
                m_job.sourceId,
                m_job.groupId);

        for (const FileItem &child : children) {

            if (child.objectType
                != FileObjectType::AtomicFile) {
                continue;
            }

            existingFiles.insert(
                child.name,
                child);
        }

        QSet<QString> remoteFileNames;

        for (const RemoteEntry &file : folder.files) {

            remoteFileNames.insert(file.name);

            FileItem atomicFile =
                existingFiles.value(file.name);

            if (atomicFile.isValid()) {

                atomicFile.remotePath = file.path;
                atomicFile.size = file.size;
                atomicFile.syncState = SyncState::Synced;

                store.updateFile(atomicFile);

                continue;
            }

            atomicFile = FileItem();

            atomicFile.id =
                QUuid::createUuid().toString(
                    QUuid::WithoutBraces);

            atomicFile.parentId = atomicFolder.id;
            atomicFile.name = file.name;

            atomicFile.objectType =
                FileObjectType::AtomicFile;

            atomicFile.space = m_job.space;
            atomicFile.sourceId = m_job.sourceId;
            atomicFile.groupId = m_job.groupId;

            atomicFile.remotePath = file.path;
            atomicFile.localPath = QString();

            atomicFile.extension =
                QFileInfo(file.name)
                    .suffix()
                    .toLower();

            atomicFile.size = file.size;
            atomicFile.modifiedTime =
                file.modified.isValid()
                    ? file.modified
                    : QDateTime::currentDateTime();

            atomicFile.syncState = SyncState::Synced;

            store.addFile(atomicFile);
        }

        // ---- 远端已删除的子文件 ----

        if (!mayPrune) {
            continue;
        }

        for (const FileItem &child : children) {

            if (child.objectType
                != FileObjectType::AtomicFile) {
                continue;
            }

            if (remoteFileNames.contains(child.name)) {
                continue;
            }

            if (child.syncState == SyncState::LocalOnly
                || child.syncState == SyncState::Uploading) {
                continue;
            }

            store.removeFile(child.id);
        }
    }

    // ---- 远端已删除的原子文件夹 ----

    if (!mayPrune) {
        return;
    }

    for (const FileItem &item : roots) {

        if (item.objectType
            != FileObjectType::AtomicFolder) {
            continue;
        }

        if (remoteNames.contains(item.name)) {
            continue;
        }

        if (item.syncState == SyncState::LocalOnly
            || item.syncState == SyncState::Uploading) {
            continue;
        }

        const QVector<FileItem> children =
            storeChildren(
                item.id,
                m_job.space,
                m_job.sourceId,
                m_job.groupId);

        for (const FileItem &child : children) {

            if (!child.localPath.isEmpty()) {

                QFile::remove(child.localPath);
            }

            store.removeFile(child.id);
        }

        if (!item.localPath.isEmpty()) {

            QDir(item.localPath).removeRecursively();
        }

        store.removeFile(item.id);
    }
}

// ============================================================
// 上传准备
// ============================================================

void SyncService::prepareUploads()
{
    m_job.directoryQueue.clear();
    m_job.directoryIndex = 0;
    m_job.uploadQueue.clear();
    m_job.uploadIndex = 0;

    if (m_job.readOnly) {
        return;
    }

    // ---- 远端已有文件的索引 ----

    QHash<QString, RemoteEntry> remoteFiles;

    for (const RemoteFolder &folder : m_job.folders) {

        for (const RemoteEntry &file : folder.files) {

            remoteFiles.insert(
                file.path,
                file);
        }
    }

    QSet<QString> remoteFolderNames;

    for (const RemoteFolder &folder : m_job.folders) {
        remoteFolderNames.insert(folder.name);
    }

    const QVector<FileItem> items =
        allItemsOfJob();

    for (const FileItem &item : items) {

        if (item.objectType
            != FileObjectType::AtomicFolder) {
            continue;
        }

        const QString remoteFolder =
            layout::remoteAtomicFolderPath(
                m_job.remoteRoot,
                item.name);

        if (remoteFolder.isEmpty()) {
            continue;
        }

        /*
         * 本地目录来源：
         *
         *   1. 已经导入 / 已下载 -> localPath
         *   2. 否则退回镜像候选路径
         */
        QString localDirectory =
            item.localPath;

        if (localDirectory.isEmpty()) {

            localDirectory =
                layout::localPathForRemote(
                    remoteFolder,
                    m_job.sourceId);
        }

        const QFileInfo localInfo(localDirectory);

        if (!localInfo.exists()
            || !localInfo.isDir()) {

            continue;
        }

        QStringList localFiles;

        const QFileInfoList entries =
            QDir(localDirectory).entryInfoList(
                QDir::Files
                    | QDir::NoDotAndDotDot,
                QDir::Name);

        for (const QFileInfo &info : entries) {

            if (isHiddenOrSystemEntry(
                    info.fileName())) {
                continue;
            }

            localFiles.append(
                info.absoluteFilePath());
        }

        if (localFiles.isEmpty()) {
            continue;
        }

        if (!remoteFolderNames.contains(item.name)) {

            if (!m_job.directoryQueue.contains(
                    remoteFolder)) {

                m_job.directoryQueue.append(
                    remoteFolder);
            }
        }

        for (const QString &localFile : localFiles) {

            const QString fileName =
                QFileInfo(localFile).fileName();

            const QString remoteFile =
                layout::remoteAtomicFilePath(
                    m_job.remoteRoot,
                    item.name,
                    fileName);

            if (remoteFile.isEmpty()) {
                continue;
            }

            // ---- 远端已有且更新 -> 不上传 ----

            const auto remoteIt =
                remoteFiles.constFind(remoteFile);

            if (remoteIt != remoteFiles.constEnd()) {

                const QDateTime remoteModified =
                    remoteIt->modified;

                const QDateTime localModified =
                    QFileInfo(localFile).lastModified();

                if (remoteModified.isValid()
                    && localModified.isValid()
                    && localModified <= remoteModified) {

                    continue;
                }
            }

            m_job.uploadQueue.append(
                qMakePair(
                    localFile,
                    remoteFile));
        }
    }

    qDebug()
        << "SyncService: 待创建目录"
        << m_job.directoryQueue.size()
        << "待上传文件"
        << m_job.uploadQueue.size();
}

// ============================================================
// 目录创建
// ============================================================

void SyncService::startDirectoryCreation()
{
    m_job.directoryIndex = 0;

    if (m_job.directoryQueue.isEmpty()) {

        startUploads();

        return;
    }

    createNextDirectory();
}

void SyncService::createNextDirectory()
{
    if (!m_job.active) {
        return;
    }

    if (m_job.directoryIndex
        >= m_job.directoryQueue.size()) {

        // 补建根目录之后重新扫描一次。
        if (m_job.bootstrappingRoot) {

            m_job.bootstrappingRoot = false;
            m_job.directoryQueue.clear();
            m_job.directoryIndex = 0;

            beginRemoteScan();

            return;
        }

        startUploads();

        return;
    }

    const QString path =
        m_job.directoryQueue.at(
            m_job.directoryIndex);

    ++m_job.directoryIndex;

    m_job.client->createDirectory(path);
}

void SyncService::onDirectoryCreated(
    const QString &remotePath,
    bool ok,
    const QString &error)
{
    // ---- 远端骨架创建 ----

    if (m_layoutCreationInProgress) {

        if (!ok) {

            qWarning()
                << "SyncService: 创建远端骨架失败"
                << remotePath
                << error;
        }

        ++m_layoutIndex;

        ensureRemoteLayout();

        return;
    }

    // ---- 普通同步 ----

    if (!isJobClient()) {
        return;
    }

    if (!ok) {

        qWarning()
            << "SyncService: 创建目录失败"
            << remotePath
            << error;

        /*
         * 补建根目录时的失败先不判死，
         * 重扫之后仍然读不到才会上报。
         */
        if (!m_job.bootstrappingRoot) {

            m_job.hasError = true;

            if (m_job.error.isEmpty()) {
                m_job.error = error;
            }
        }
    }

    createNextDirectory();
}

// ============================================================
// 上传
// ============================================================

void SyncService::startUploads()
{
    m_job.uploadIndex = 0;

    if (m_job.uploadQueue.isEmpty()) {

        finishJob(
            !m_job.hasError,
            m_job.hasError
                ? m_job.error
                : QStringLiteral("%1 同步完成")
                      .arg(jobLabel()));

        return;
    }

    uploadNextFile();
}

void SyncService::uploadNextFile()
{
    if (!m_job.active) {
        return;
    }

    if (m_job.uploadIndex
        >= m_job.uploadQueue.size()) {

        finishJob(
            !m_job.hasError,
            m_job.hasError
                ? m_job.error
                : QStringLiteral("%1 同步完成")
                      .arg(jobLabel()));

        return;
    }

    const QPair<QString, QString> pair =
        m_job.uploadQueue.at(
            m_job.uploadIndex);

    ++m_job.uploadIndex;

    if (!QFileInfo::exists(pair.first)) {

        qWarning()
            << "SyncService: 本地文件已消失"
            << pair.first;

        uploadNextFile();

        return;
    }

    m_job.client->uploadFile(
        pair.first,
        pair.second);
}

void SyncService::onUploadFinished(
    const QString &remotePath,
    bool ok,
    const QString &error)
{
    if (!isJobClient()) {
        return;
    }

    if (!ok) {

        qWarning()
            << "SyncService: 上传失败"
            << remotePath
            << error;

        m_job.hasError = true;

        if (m_job.error.isEmpty()) {
            m_job.error = error;
        }
    }

    uploadNextFile();
}

// ============================================================
// 结束任务
// ============================================================

void SyncService::finishJob(
    bool ok,
    const QString &message)
{
    if (!m_job.active) {
        return;
    }

    const QString label = jobLabel();

    const FileSpace space = m_job.space;
    const QString sourceId = m_job.sourceId;
    const QString groupId = m_job.groupId;

    m_job.active = false;
    m_job.client = nullptr;

    m_job.folders.clear();
    m_job.folderIndexByPath.clear();
    m_job.visited.clear();
    m_job.pendingListings = 0;
    m_job.directoryQueue.clear();
    m_job.uploadQueue.clear();

    qDebug()
        << "SyncService: 同步结束" << label
        << (ok ? "success" : "failed")
        << message;

    emit syncFinished(ok, message);
    emit libraryChanged();

    // ---- 继续队列中的下一个空间 ----

    startNextQueuedJob();
}

// ============================================================
// 自动同步
// ============================================================

void SyncService::startAutoSync(int intervalMs)
{
    if (intervalMs <= 0) {
        intervalMs = 180000;
    }

    m_timer->setInterval(intervalMs);
    m_timer->start();
}

void SyncService::stopAutoSync()
{
    m_timer->stop();
}

void SyncService::onAutoSyncTimeout()
{
    if (m_job.active || m_deleting) {
        return;
    }

    syncMyAccount();
}

// ============================================================
// 远端骨架
// ============================================================

void SyncService::ensureRemoteLayout()
{
    if (!m_ownClient) {
        return;
    }

    if (m_layoutQueue.isEmpty()) {

        m_layoutIndex = 0;

        /*
         * 只创建顶层共享根。
         *
         * 群组目录的层级取决于远端实际布局
         * （zesbks/group/group me 或 zesbks/group me），
         * 交给每个同步任务的「根目录补建」按需创建。
         */
        m_layoutQueue
            << layout::remoteShareRoot();

        m_layoutCreationInProgress = true;
    }

    if (m_layoutIndex >= m_layoutQueue.size()) {

        m_layoutCreationInProgress = false;
        m_layoutQueue.clear();
        m_layoutIndex = 0;

        emit statusMessage(
            QStringLiteral("远端目录已就绪"));

        emit remoteLayoutReady();

        /*
         * 验证并建好骨架之后，
         * 立刻做一次首次同步，
         * 把远端已有的原子对象拉进应用。
         */
        if (!m_job.active && !m_deleting) {
            syncMyAccount();
        }

        return;
    }

    m_ownClient->createDirectory(
        m_layoutQueue.at(m_layoutIndex));
}

// ============================================================
// 他人群组数据源
// ============================================================

QVariantList SyncService::groupSources() const
{
    QVariantList result;

    MetadataStore store;

    if (!openStore(store)) {
        return result;
    }

    const QVector<GroupSource> all =
        store.sources();

    for (const GroupSource &source : all) {

        QVariantMap map;

        map.insert(
            QStringLiteral("sourceId"),
            source.sourceId);

        map.insert(
            QStringLiteral("displayName"),
            source.displayName.isEmpty()
                ? source.sourceId
                : source.displayName);

        map.insert(
            QStringLiteral("serverUrl"),
            source.serverUrl);

        map.insert(
            QStringLiteral("username"),
            source.username);

        map.insert(
            QStringLiteral("password"),
            source.password);

        map.insert(
            QStringLiteral("remoteRoot"),
            source.remoteRoot.isEmpty()
                ? layout::remoteGroupMeRoot(0)
                : source.remoteRoot);

        map.insert(
            QStringLiteral("lastSync"),
            source.lastSync);

        result.append(map);
    }

    return result;
}

bool SyncService::saveGroupSource(
    const QString &sourceId,
    const QString &displayName,
    const QString &serverUrl,
    const QString &username,
    const QString &password,
    const QString &remoteRoot)
{
    const QString id = sourceId.trimmed();

    if (id.isEmpty() || serverUrl.trimmed().isEmpty()) {
        return false;
    }

    MetadataStore store;

    if (!openStore(store)) {
        return false;
    }

    GroupSource source;

    // 保留已有配置中未修改的字段。
    const GroupSource existing =
        store.sourceById(id);

    source.sourceId = id;

    source.displayName =
        displayName.trimmed().isEmpty()
            ? id
            : displayName.trimmed();

    source.serverUrl = serverUrl.trimmed();

    source.username =
        username.trimmed().isEmpty()
            ? existing.username
            : username.trimmed();

    source.password =
        password.isEmpty()
            ? existing.password
            : password;

    source.remoteRoot =
        layout::normalizeRemotePath(
            remoteRoot.trimmed().isEmpty()
                ? layout::remoteGroupMeRoot(0)
                : remoteRoot.trimmed());

    source.readOnly = true;
    source.lastSync = existing.lastSync;

    if (!store.upsertSource(source)) {
        return false;
    }

    // 凭据可能已变化，丢弃缓存客户端。
    if (m_sourceClients.contains(id)) {

        WebDavClient *cached =
            m_sourceClients.take(id);

        cached->deleteLater();
    }

    return true;
}

bool SyncService::removeGroupSource(
    const QString &sourceId)
{
    MetadataStore store;

    if (!openStore(store)) {
        return false;
    }

    if (m_sourceClients.contains(sourceId)) {

        WebDavClient *cached =
            m_sourceClients.take(sourceId);

        cached->deleteLater();
    }

    return store.removeSource(sourceId);
}

// ============================================================
// 按需下载
// ============================================================

bool SyncService::downloadItem(
    const QString &fileId)
{
    MetadataStore store;

    if (!openStore(store)) {
        return false;
    }

    const FileItem item =
        store.fileById(fileId);

    if (!item.isValid()
        || item.objectType != FileObjectType::AtomicFile) {

        return false;
    }

    if (item.remotePath.isEmpty()) {
        return false;
    }

    // 已经在本地。
    if (!item.localPath.isEmpty()
        && QFileInfo::exists(item.localPath)) {

        emit itemDownloaded(
            fileId,
            item.remotePath,
            item.localPath,
            true,
            QString());

        return true;
    }

    WebDavClient *client =
        clientForSource(item.sourceId);

    if (!client) {
        return false;
    }

    const QString localPath =
        layout::localPathForRemote(
            item.remotePath,
            item.sourceId);

    if (!QDir().mkpath(
            QFileInfo(localPath).absolutePath())) {

        return false;
    }

    m_pendingDownloadIds.insert(
        item.remotePath,
        fileId);

    client->downloadFile(
        item.remotePath,
        localPath);

    return true;
}

void SyncService::onDownloadFinished(
    const QString &remotePath,
    const QString &localPath,
    bool ok,
    const QString &error)
{
    const QString fileId =
        m_pendingDownloadIds.take(remotePath);

    if (fileId.isEmpty()) {

        /*
         * 不是本服务发起的下载，
         * 例如 UI 直接调用 WebDavClient。
         */
        return;
    }

    if (ok) {

        MetadataStore store;

        if (openStore(store)) {

            store.updateLocalPath(
                fileId,
                localPath);

            store.touchSourceSyncTime(
                QString());
        }
    }

    emit itemDownloaded(
        fileId,
        remotePath,
        localPath,
        ok,
        error);

    if (ok) {
        emit libraryChanged();
    }
}

// ============================================================
// 回推单个条目
// ============================================================

bool SyncService::uploadItem(
    const QString &fileId)
{
    MetadataStore store;

    if (!openStore(store)) {
        return false;
    }

    const FileItem item =
        store.fileById(fileId);

    if (!item.isValid() || item.isReadOnly()) {
        return false;
    }

    if (item.remotePath.isEmpty()) {
        return false;
    }

    if (item.localPath.isEmpty()
        || !QFileInfo::exists(item.localPath)) {

        return false;
    }

    WebDavClient *client =
        clientForSource(item.sourceId);

    if (!client) {
        return false;
    }

    client->uploadFile(
        item.localPath,
        item.remotePath);

    return true;
}

// ============================================================
// 远端删除
// ============================================================

void SyncService::deleteRemoteAtomicFolder(
    const QString &fileId)
{
    if (m_job.active || m_deleting) {

        emit remoteDeletionFinished(
            false,
            QStringLiteral("当前有同步任务在进行"));

        return;
    }

    MetadataStore store;

    if (!openStore(store)) {

        emit remoteDeletionFinished(
            false,
            QStringLiteral("无法打开本地数据库"));

        return;
    }

    const FileItem item =
        store.fileById(fileId);

    if (!item.isValid()
        || item.objectType != FileObjectType::AtomicFolder) {

        emit remoteDeletionFinished(
            false,
            QStringLiteral("只能删除原子文件夹"));

        return;
    }

    if (item.isReadOnly()) {

        emit remoteDeletionFinished(
            false,
            QStringLiteral(
                "group us 的内容来自他人账号，不可删除"));

        return;
    }

    if (item.remotePath.isEmpty()) {

        emit remoteDeletionFinished(
            false,
            QStringLiteral("该对象没有远端路径"));

        return;
    }

    const QString remotePath =
        layout::normalizeRemotePath(
            item.remotePath);

    QString root;

    switch (item.space) {

    case FileSpace::MyDoc:
        root = layout::remoteShareRoot();
        break;

    case FileSpace::GroupMe:
        root = groupRootFor(
            item.groupId.isEmpty()
                ? layout::groupMeDirectoryName(0)
                : item.groupId);
        break;

    case FileSpace::GroupUs:
        emit remoteDeletionFinished(
            false,
            QStringLiteral(
                "group us 的内容不可删除"));

        return;
    }

    root = layout::normalizeRemotePath(root);

    const QString relative =
        remotePath.mid(root.length());

    if (relative.isEmpty()
        || relative.count(QLatin1Char('/')) != 1) {

        emit remoteDeletionFinished(
            false,
            QStringLiteral(
                "只允许删除同步根下的一级原子文件夹：%1")
                .arg(remotePath));

        return;
    }

    m_deleteQueue.clear();
    m_deleteQueue << remotePath;

    m_deleteFolderIds.clear();
    m_deleteFolderIds.insert(remotePath, item.id);

    m_deleteIndex = 0;
    m_deleteHasError = false;
    m_deleteError.clear();
    m_deleting = true;

    deleteNextRemoteFolder();
}

void SyncService::deleteNextRemoteFolder()
{
    if (!m_deleting) {
        return;
    }

    if (m_deleteIndex >= m_deleteQueue.size()) {

        m_deleting = false;

        const bool ok = !m_deleteHasError;

        emit remoteDeletionFinished(
            ok,
            ok
                ? QStringLiteral("远端原子文件夹已删除")
                : (m_deleteError.isEmpty()
                       ? QStringLiteral("远端删除失败")
                       : m_deleteError));

        if (ok) {
            emit libraryChanged();
        }

        m_deleteQueue.clear();
        m_deleteIndex = 0;

        return;
    }

    const QString path =
        m_deleteQueue.at(m_deleteIndex);

    if (!m_ownClient) {

        m_deleteHasError = true;
        m_deleteError = QStringLiteral("WebDAV 客户端不可用");

        ++m_deleteIndex;
        deleteNextRemoteFolder();

        return;
    }

    // 远端删除的目标始终是原子文件夹（集合）。
    m_ownClient->removeFile(path, true);
}

void SyncService::onRemoveFinished(
    const QString &remotePath,
    bool ok,
    const QString &error)
{
    const QString normalized =
        layout::normalizeRemotePath(remotePath);

    // ---- 同步过程中的墓碑清理 ----

    if (m_job.active && m_job.drainingDeletes) {

        MetadataStore store;

        if (openStore(store)) {

            if (ok) {

                store.clearPendingDelete(normalized);

            } else {

                /*
                 * 删除失败时保留墓碑，
                 * 下一轮同步重试。
                 */
                qWarning()
                    << "SyncService: 清理远端残留失败"
                    << normalized
                    << error;

                m_job.hasError = true;

                if (m_job.error.isEmpty()) {
                    m_job.error = error;
                }
            }
        }

        deleteNextPendingDelete();

        return;
    }

    // ---- 用户主动的远端删除 ----

    if (!m_deleting) {
        return;
    }

    if (!ok) {

        qWarning()
            << "SyncService: 远端删除失败"
            << normalized
            << error;

        m_deleteHasError = true;

        if (m_deleteError.isEmpty()) {
            m_deleteError = error;
        }

    } else {

        // ---- 同步清理本地元数据与缓存 ----

        const QString fileId =
            m_deleteFolderIds.take(normalized);

        if (!fileId.isEmpty()) {

            MetadataStore store;

            if (openStore(store)) {

                const FileItem folder =
                    store.fileById(fileId);

                if (folder.isValid()) {

                    const QVector<FileItem> children =
                        store.childrenOf(
                            folder.id,
                            folder.space);

                    for (const FileItem &child : children) {

                        if (!child.localPath.isEmpty()) {
                            QFile::remove(child.localPath);
                        }

                        store.removeFile(child.id);
                    }

                    if (!folder.localPath.isEmpty()) {

                        QDir(folder.localPath)
                            .removeRecursively();
                    }

                    store.removeFile(folder.id);
                }
            }
        }
    }

    ++m_deleteIndex;

    deleteNextRemoteFolder();
}

} // namespace litapp
