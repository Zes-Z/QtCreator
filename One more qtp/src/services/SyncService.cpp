#include "services/SyncService.h"

#include "services/WebDavClient.h"
#include "storage/MetadataStore.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

#include <algorithm>

namespace litapp {

// ============================================================
// Constructor
// ============================================================

SyncService::SyncService(
    WebDavClient *client,
    QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_timer(new QTimer(this))
{
    if (m_client) {

        connect(
            m_client,
            &WebDavClient::directoryListed,
            this,
            &SyncService::handleDirectoryListed);

        connect(
            m_client,
            &WebDavClient::directoryCreated,
            this,
            &SyncService::handleDirectoryCreated);

        connect(
            m_client,
            &WebDavClient::uploadFinished,
            this,
            &SyncService::handleUploadFinished);

        connect(
            m_client,
            &WebDavClient::downloadFinished,
            this,
            &SyncService::handleDownloadFinished);

        connect(
            m_client,
            &WebDavClient::removeFinished,
            this,
            &SyncService::handleRemoveFinished);
    }

    m_timer->setInterval(180000);

    connect(
        m_timer,
        &QTimer::timeout,
        this,
        &SyncService::autoSync);
}


// ============================================================
// Current space
// ============================================================

FileSpace SyncService::currentSpace() const
{
    return m_activeSpace;
}

QString SyncService::currentSourceId() const
{
    return m_activeSourceId;
}


// ============================================================
// Sync
// ============================================================

void SyncService::syncSpace(
    int space,
    const QString &sourceId)
{
    if (m_syncInProgress) {
        qWarning()
        << "Sync already in progress.";
        return;
    }

    if (m_remoteDeletionInProgress) {
        qWarning()
        << "Remote deletion is in progress.";
        return;
    }

    if (!m_client) {
        emit syncFinished(
            false,
            QStringLiteral(
                "WebDAV 客户端不可用"));
        return;
    }

    if (space < static_cast<int>(FileSpace::MyDoc)
        || space > static_cast<int>(FileSpace::GroupUs)) {

        emit syncFinished(
            false,
            QStringLiteral(
                "无效的同步空间"));

        return;
    }

    m_activeSpace =
        static_cast<FileSpace>(space);

    m_activeSourceId = sourceId;

    /*
     * GroupUs 必须具有 sourceId。
     *
     * MyDoc / GroupMe 不要求 sourceId。
     */
    if (m_activeSpace == FileSpace::GroupUs
        && m_activeSourceId.isEmpty()) {

        emit syncFinished(
            false,
            QStringLiteral(
                "GroupUs 缺少 sourceId"));

        return;
    }

    m_syncInProgress = true;

    m_hasError = false;
    m_syncError.clear();

    m_pendingOps = 0;
    m_pendingDirectoryListings = 0;

    m_remoteNames.clear();
    m_remoteEntries.clear();
    m_visitedDirs.clear();

    m_directoryQueue.clear();
    m_directoryQueueIndex = 0;

    m_atomicFolderQueue.clear();
    m_atomicFolderQueueIndex = 0;

    m_currentAtomicFolder.clear();

    m_uploadQueue.clear();
    m_uploadQueueIndex = 0;

    m_downloadQueue.clear();
    m_downloadQueueIndex = 0;

    const QString localRoot =
        localSpaceRoot(
            m_activeSpace,
            m_activeSourceId);

    if (!QDir(localRoot).exists()) {

        if (!QDir().mkpath(localRoot)) {

            finish(
                false,
                QStringLiteral(
                    "无法创建本地同步目录：%1")
                    .arg(localRoot));

            return;
        }
    }

    const QString remoteRoot =
        normalizeRemotePath(
            remoteSpaceRoot(
                m_activeSpace));

    qDebug()
        << "========================================";

    qDebug()
        << "开始 WebDAV 同步";

    qDebug()
        << "space ="
        << static_cast<int>(m_activeSpace);

    qDebug()
        << "sourceId ="
        << m_activeSourceId;

    qDebug()
        << "localRoot ="
        << localRoot;

    qDebug()
        << "remoteRoot ="
        << remoteRoot;

    qDebug()
        << "========================================";

    /*
     * 先扫描远程空间。
     *
     * MyDoc:
     *
     *     /zesbks/
     *
     * GroupMe:
     *
     *     /zesbks/group/
     *
     * GroupUs:
     *
     *     远程账号的 /zesbks/group/
     */
    startRemoteScan();
}


// ============================================================
// Active space
// ============================================================

void SyncService::setActiveSpace(
    int space,
    const QString &sourceId)
{
    if (space < static_cast<int>(FileSpace::MyDoc)
        || space > static_cast<int>(FileSpace::GroupUs)) {
        return;
    }

    m_activeSpace =
        static_cast<FileSpace>(space);

    m_activeSourceId = sourceId;
}


// ============================================================
// Auto sync
// ============================================================

void SyncService::startAutoSync(
    int intervalMs)
{
    if (intervalMs <= 0) {
        intervalMs = 180000;
    }

    m_timer->setInterval(intervalMs);
    m_timer->start();

    qDebug()
        << "WebDAV 自动同步已启动:"
        << intervalMs
        << "ms";
}

void SyncService::stopAutoSync()
{
    m_timer->stop();

    qDebug()
        << "WebDAV 自动同步已停止";
}

void SyncService::autoSync()
{
    if (m_syncInProgress) {
        return;
    }

    if (m_remoteDeletionInProgress) {
        return;
    }

    syncSpace(
        static_cast<int>(m_activeSpace),
        m_activeSourceId);
}


// ============================================================
// Local space root
// ============================================================

QString SyncService::localSpaceRoot(
    FileSpace space,
    const QString &sourceId) const
{
    const QString base =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation)
        + QStringLiteral("/LitApp/zesbks");

    switch (space) {

    case FileSpace::MyDoc:
        return QDir::cleanPath(
            base + QStringLiteral("/my_doc"));

    case FileSpace::GroupMe:
        return QDir::cleanPath(
            base + QStringLiteral("/group_me"));

    case FileSpace::GroupUs:
    {
        /*
         * sourceId 是逻辑来源标识。
         *
         * 注意：
         * 这里仅用于本地隔离。
         *
         * 它不参与 WebDAV 的物理路径。
         */
        QString safeSourceId =
            sourceId.trimmed();

        if (safeSourceId.isEmpty()) {
            safeSourceId =
                QStringLiteral("default");
        }

        safeSourceId.replace(
            QLatin1Char('/'),
            QLatin1Char('_'));

        safeSourceId.replace(
            QLatin1Char('\\'),
            QLatin1Char('_'));

        return QDir::cleanPath(
            base
            + QStringLiteral("/group_us/")
            + safeSourceId);
    }
    }

    return QDir::cleanPath(base);
}


// ============================================================
// Remote space root
// ============================================================

QString SyncService::remoteSpaceRoot(
    FileSpace space) const
{
    switch (space) {

    case FileSpace::MyDoc:
        return QStringLiteral("/zesbks");

    case FileSpace::GroupMe:
        return QStringLiteral("/zesbks/group");

    case FileSpace::GroupUs:
        /*
         * GroupUs 使用被连接账号的 /zesbks/group/。
         *
         * sourceId 不改变远程账号内部的路径。
         */
        return QStringLiteral("/zesbks/group");
    }

    return QStringLiteral("/zesbks");
}


// ============================================================
// Metadata loading
// ============================================================

QVector<FileItem> SyncService::loadSpaceItems(
    FileSpace space,
    const QString &sourceId) const
{
    QVector<FileItem> result;

    MetadataStore store;

    if (!store.open(m_databasePath)) {
        return result;
    }

    const QVector<FileItem> all =
        store.files(space);

    /*
     * MyDoc / GroupMe：
     *
     * 直接使用对应 space。
     *
     * GroupUs：
     *
     * 必须再按照 sourceId 隔离。
     */
    for (const FileItem &item : all) {

        if (space == FileSpace::GroupUs) {

            if (item.sourceId != sourceId) {
                continue;
            }
        }

        result.append(item);
    }

    return result;
}


// ============================================================
// Descendants
// ============================================================

QVector<FileItem> SyncService::descendantItems(
    const QString &parentId,
    FileSpace space,
    const QString &sourceId) const
{
    QVector<FileItem> result;

    const QVector<FileItem> all =
        loadSpaceItems(
            space,
            sourceId);

    /*
     * parentId 是全局唯一对象 ID。
     *
     * 因此不需要通过 WebDAV 路径寻找。
     */

    QList<QString> queue;

    queue.append(parentId);

    while (!queue.isEmpty()) {

        const QString currentParent =
            queue.takeFirst();

        for (const FileItem &item : all) {

            if (item.parentId != currentParent) {
                continue;
            }

            result.append(item);

            if (item.isFolder()) {
                queue.append(item.id);
            }
        }
    }

    return result;
}


// ============================================================
// Atomic Folder
// ============================================================

bool SyncService::isAtomicFolder(
    const FileItem &item) const
{
    return item.objectType ==
           FileObjectType::AtomicFolder;
}

bool SyncService::isAtomicFile(
    const FileItem &item) const
{
    return item.objectType ==
           FileObjectType::AtomicFile;
}

bool SyncService::isLocalAtomicFolder(
    const QString &path) const
{
    const QFileInfo info(path);

    return info.exists()
           && info.isDir();
}


// ============================================================
// Atomic Folder files
// ============================================================

QStringList SyncService::collectAtomicFolderFiles(
    const QString &atomicFolderPath) const
{
    QStringList result;

    if (!isLocalAtomicFolder(
            atomicFolderPath)) {
        return result;
    }

    QDirIterator iterator(
        atomicFolderPath,
        QDir::Files
            | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);

    while (iterator.hasNext()) {

        const QString path =
            QDir::cleanPath(
                iterator.next());

        const QFileInfo info(path);

        if (!info.exists()
            || !info.isFile()) {
            continue;
        }

        result.append(path);
    }

    return result;
}

QStringList SyncService::atomicFolderFiles(
    const FileItem &atomicFolder) const
{
    QString folderPath;

    /*
     * FileService 当前设计中：
     *
     * AtomicFolder.localPath
     *
     * 指向 Atomic File。
     *
     * 因此这里取其 parent directory。
     */
    if (!atomicFolder.localPath.isEmpty()) {

        const QFileInfo info(
            atomicFolder.localPath);

        if (info.exists()
            && info.isFile()) {

            folderPath =
                info.absolutePath();

        } else if (info.exists()
                   && info.isDir()) {

            folderPath =
                info.absoluteFilePath();

        } else {

            /*
             * 文件不存在时仍然根据 path 推导。
             */
            folderPath =
                info.absolutePath();
        }
    }

    if (folderPath.isEmpty()) {
        return {};
    }

    return collectAtomicFolderFiles(
        folderPath);
}


// ============================================================
// Find local Atomic Folder
// ============================================================

QString SyncService::atomicFolderForLocalFile(
    const QString &localFilePath) const
{
    const QString root =
        QDir::cleanPath(
            localSpaceRoot(
                m_activeSpace,
                m_activeSourceId));

    const QString file =
        QDir::cleanPath(
            localFilePath);

    if (!file.startsWith(
            root + QLatin1Char('/'))) {
        return QString();
    }

    const QString relative =
        file.mid(
            root.length() + 1);

    const int slash =
        relative.indexOf(
            QLatin1Char('/'));

    if (slash < 0) {
        return QString();
    }

    const QString firstComponent =
        relative.left(slash);

    if (firstComponent.isEmpty()) {
        return QString();
    }

    return QDir::cleanPath(
        root
        + QLatin1Char('/')
        + firstComponent);
}


// ============================================================
// Remote Atomic Folder path
// ============================================================

QString SyncService::remoteAtomicFolderPathForItem(
    const FileItem &item) const
{
    if (!isAtomicFolder(item)) {
        return QString();
    }

    const QString root =
        normalizeRemotePath(
            remoteSpaceRoot(
                item.space));

    if (root == QStringLiteral("/")) {
        return QString();
    }

    if (item.name.isEmpty()) {
        return QString();
    }

    return normalizeRemotePath(
        root
        + QLatin1Char('/')
        + item.name);
}


// ============================================================
// Remote Atomic File path
// ============================================================

QString SyncService::remoteAtomicFilePathForItem(
    const FileItem &item) const
{
    if (!isAtomicFile(item)) {
        return QString();
    }

    /*
     * AtomicFile 自身不通过 parentId 直接映射 App hierarchy。
     *
     * 找到其 Atomic Folder 后：
     *
     *     /AtomicFolder/AtomicFile
     */

    const QVector<FileItem> all =
        loadSpaceItems(
            item.space,
            item.sourceId);

    FileItem atomicFolder;

    for (const FileItem &candidate : all) {

        if (!isAtomicFolder(candidate)) {
            continue;
        }

        if (candidate.id == item.parentId) {
            atomicFolder = candidate;
            break;
        }
    }

    if (!atomicFolder.isValid()) {
        return QString();
    }

    const QString folderPath =
        remoteAtomicFolderPathForItem(
            atomicFolder);

    if (folderPath.isEmpty()) {
        return QString();
    }

    return normalizeRemotePath(
        folderPath
        + QLatin1Char('/')
        + item.name);
}


// ============================================================
// Remote Atomic Folder path from local path
// ============================================================

QString SyncService::remoteAtomicFolderPathForLocalPath(
    const QString &localAtomicFolderPath) const
{
    const QString root =
        QDir::cleanPath(
            localSpaceRoot(
                m_activeSpace,
                m_activeSourceId));

    const QString folder =
        QDir::cleanPath(
            localAtomicFolderPath);

    if (!folder.startsWith(
            root + QLatin1Char('/'))) {
        return QString();
    }

    const QString relative =
        folder.mid(
            root.length() + 1);

    /*
     * 本地 Atomic Folder 作为本地 workspace
     * 的一级物理目录。
     *
     * 注意：
     *
     * 这里只是 Atomic Folder 的物理容器，
     * 不是 App folder hierarchy。
     */
    if (relative.isEmpty()
        || relative.contains(
            QLatin1Char('/'))) {
        return QString();
    }

    return normalizeRemotePath(
        remoteSpaceRoot(
            m_activeSpace)
        + QLatin1Char('/')
        + relative);
}


// ============================================================
// Start remote scan
// ============================================================

void SyncService::startRemoteScan()
{
    const QString root =
        normalizeRemotePath(
            remoteSpaceRoot(
                m_activeSpace));

    if (m_visitedDirs.contains(root)) {
        return;
    }

    m_visitedDirs.insert(root);

    ++m_pendingDirectoryListings;

    m_client->listDirectory(root);
}


// ============================================================
// Scan remote directory
// ============================================================

void SyncService::scanRemoteDirectory(
    const QString &remotePath)
{
    const QString normalized =
        normalizeRemotePath(remotePath);

    if (m_visitedDirs.contains(normalized)) {
        return;
    }

    m_visitedDirs.insert(normalized);

    ++m_pendingDirectoryListings;

    m_client->listDirectory(normalized);
}


// ============================================================
// Remote directory listed
// ============================================================

void SyncService::handleDirectoryListed(
    const QString &path,
    const QVariantList &entries,
    const QString &error)
{
    if (!m_syncInProgress) {
        return;
    }

    if (m_pendingDirectoryListings > 0) {
        --m_pendingDirectoryListings;
    }

    const QString normalizedPath =
        normalizeRemotePath(path);

    if (!error.isEmpty()) {

        /*
         * /zesbks 或 /zesbks/group 不存在：
         *
         * 不立即认为同步失败。
         *
         * 后续上传阶段会尝试创建。
         */
        qWarning()
            << "远程目录读取失败:"
            << normalizedPath
            << error;

    } else {

        for (const QVariant &value : entries) {

            const QVariantMap map =
                value.toMap();

            QString entryPath =
                map.value(
                       QStringLiteral("path"))
                    .toString();

            if (entryPath.isEmpty()) {
                continue;
            }

            entryPath =
                normalizeRemotePath(
                    entryPath);

            const bool isDirectory =
                map.value(
                       QStringLiteral("isDirectory"))
                    .toBool();

            if (isDirectory) {

                /*
                 * 当前空间的根目录下面：
                 *
                 * Atomic Folder
                 *
                 * 但 GroupMe / GroupUs 的 root
                 * 本身已经是 /zesbks/group。
                 *
                 * Atomic Folder 内部目录还需要递归。
                 */
                if (!m_visitedDirs.contains(
                        entryPath)) {

                    m_visitedDirs.insert(
                        entryPath);

                    ++m_pendingDirectoryListings;

                    m_client->listDirectory(
                        entryPath);
                }

            } else {

                /*
                 * 只记录当前同步空间下的文件。
                 */
                if (!isRemotePathInCurrentSpace(
                        entryPath)) {
                    continue;
                }

                m_remoteEntries.append(map);

                m_remoteNames.insert(
                    entryPath);
            }
        }
    }

    /*
     * 所有远程目录扫描结束。
     */
    if (m_pendingDirectoryListings > 0) {
        return;
    }

    /*
     * 远程扫描完成。
     *
     * 开始处理本地 Atomic Folder。
     */
    scanLocalFiles();

    /*
     * 准备需要创建的远程目录。
     */
    for (const QString &folderPath :
         std::as_const(
             m_atomicFolderQueue)) {

        /*
         * 这里 folderPath 是本地路径。
         */
        const QVector<FileItem> items =
            loadSpaceItems(
                m_activeSpace,
                m_activeSourceId);

        for (const FileItem &item : items) {

            if (!isAtomicFolder(item)) {
                continue;
            }

            QString localFolder;

            if (!item.localPath.isEmpty()) {

                const QFileInfo info(
                    item.localPath);

                if (info.exists()
                    && info.isFile()) {

                    localFolder =
                        info.absolutePath();

                } else if (info.exists()
                           && info.isDir()) {

                    localFolder =
                        info.absoluteFilePath();
                } else {

                    localFolder =
                        info.absolutePath();
                }
            }

            if (QDir::cleanPath(localFolder)
                != QDir::cleanPath(folderPath)) {
                continue;
            }

            addAtomicFolderDirectories(item);
            break;
        }
    }

    /*
     * 没有需要创建的目录。
     */
    if (m_directoryQueue.isEmpty()) {

        startUploads();

        return;
    }

    startDirectoryCreation();
}


// ============================================================
// Scan local files
// ============================================================

void SyncService::scanLocalFiles()
{
    m_atomicFolderQueue.clear();
    m_atomicFolderQueueIndex = 0;

    m_uploadQueue.clear();
    m_uploadQueueIndex = 0;

    const QVector<FileItem> items =
        loadSpaceItems(
            m_activeSpace,
            m_activeSourceId);

    /*
     * 优先使用 MetadataStore。
     *
     * AtomicFolder 是逻辑对象。
     */
    for (const FileItem &item : items) {

        if (!isAtomicFolder(item)) {
            continue;
        }

        if (item.localPath.isEmpty()) {
            continue;
        }

        const QFileInfo info(
            item.localPath);

        QString localFolder;

        if (info.exists()
            && info.isFile()) {

            localFolder =
                info.absolutePath();

        } else if (info.exists()
                   && info.isDir()) {

            localFolder =
                info.absoluteFilePath();

        } else {

            localFolder =
                info.absolutePath();
        }

        if (!localFolder.isEmpty()
            && isLocalAtomicFolder(
                localFolder)) {

            if (!m_atomicFolderQueue.contains(
                    localFolder)) {

                m_atomicFolderQueue.append(
                    localFolder);
            }
        }
    }

    /*
     * 兼容本地 workspace 中已经存在、
     * 但 metadata 尚未建立的 Atomic Folder。
     *
     * 只扫描 workspace 一级目录。
     */
    const QString root =
        localSpaceRoot(
            m_activeSpace,
            m_activeSourceId);

    QDir rootDir(root);

    const QFileInfoList dirs =
        rootDir.entryInfoList(
            QDir::Dirs
                | QDir::NoDotAndDotDot,
            QDir::Name);

    for (const QFileInfo &info : dirs) {

        const QString folder =
            QDir::cleanPath(
                info.absoluteFilePath());

        if (!m_atomicFolderQueue.contains(
                folder)) {

            /*
             * 这里只将一级目录作为
             * Atomic Folder candidate。
             */
            m_atomicFolderQueue.append(
                folder);
        }
    }

    /*
     * 为每一个 Atomic Folder 构建上传队列。
     */
    for (const QString &folderPath :
         std::as_const(
             m_atomicFolderQueue)) {

        const QStringList files =
            collectAtomicFolderFiles(
                folderPath);

        for (const QString &localFile :
             files) {

            const QString remotePath =
                remoteAtomicFolderPathForLocalPath(
                    folderPath);

            if (remotePath.isEmpty()) {
                continue;
            }

            const QString fileName =
                QFileInfo(localFile).fileName();

            const QString remoteFile =
                normalizeRemotePath(
                    remotePath
                    + QLatin1Char('/')
                    + fileName);

            if (!m_remoteNames.contains(
                    remoteFile)) {

                m_uploadQueue.append(
                    qMakePair(
                        localFile,
                        remoteFile));
            }
        }
    }

    qDebug()
        << "Atomic Folder:"
        << m_atomicFolderQueue.size();

    qDebug()
        << "Upload queue:"
        << m_uploadQueue.size();
}


// ============================================================
// Add Atomic Folder directories
// ============================================================

void SyncService::addAtomicFolderDirectories(
    const FileItem &atomicFolder)
{
    if (!isAtomicFolder(
            atomicFolder)) {
        return;
    }

    const QString remoteFolder =
        remoteAtomicFolderPathForItem(
            atomicFolder);

    if (remoteFolder.isEmpty()) {
        return;
    }

    addParentDirectories(
        remoteFolder);

    if (!m_directoryQueue.contains(
            remoteFolder)) {

        m_directoryQueue.append(
            remoteFolder);
    }

    /*
     * Atomic Folder 内可能还有：
     *
     * annotations/
     * assets/
     * ...
     *
     * 根据本地文件路径创建对应父目录。
     */
    const QStringList localFiles =
        atomicFolderFiles(
            atomicFolder);

    for (const QString &localFile :
         localFiles) {

        const QFileInfo info(localFile);

        const QString relative =
            QDir(
                QFileInfo(
                    localFile).absolutePath())
                .relativeFilePath(
                    localFile);

        Q_UNUSED(relative);

        /*
         * 当前 Atomic File 通常直接位于
         * Atomic Folder 下。
         *
         * 如果未来允许附属文件夹，
         * 这里根据本地 relative path
         * 创建对应远程目录。
         */
        Q_UNUSED(info);
    }
}


// ============================================================
// Directory queue
// ============================================================

void SyncService::addParentDirectories(
    const QString &remotePath)
{
    QString parent =
        remoteParentPath(
            remotePath);

    QStringList parents;

    while (!parent.isEmpty()
           && parent != QStringLiteral("/")) {

        parents.prepend(parent);

        const QString next =
            remoteParentPath(parent);

        if (next == parent) {
            break;
        }

        parent = next;
    }

    /*
     * 不重复创建。
     */
    for (const QString &dir : parents) {

        const QString normalized =
            normalizeRemotePath(dir);

        /*
         * 当前同步根本身不需要重新创建。
         */
        const QString root =
            normalizeRemotePath(
                remoteSpaceRoot(
                    m_activeSpace));

        if (normalized == root) {
            continue;
        }

        if (!m_directoryQueue.contains(
                normalized)) {

            m_directoryQueue.append(
                normalized);
        }
    }
}


// ============================================================
// Start directory creation
// ============================================================

void SyncService::startDirectoryCreation()
{
    m_directoryQueueIndex = 0;

    if (m_directoryQueue.isEmpty()) {

        startUploads();

        return;
    }

    /*
     * 保证父目录在子目录之前。
     */
    std::sort(
        m_directoryQueue.begin(),
        m_directoryQueue.end(),
        [](const QString &a,
           const QString &b) {

            return a.count(
                       QLatin1Char('/'))
                   <
                   b.count(
                       QLatin1Char('/'));
        });

    createNextRemoteDirectory();
}

void SyncService::createNextRemoteDirectory()
{
    if (!m_syncInProgress) {
        return;
    }

    if (m_directoryQueueIndex
        >= m_directoryQueue.size()) {

        startUploads();

        return;
    }

    const QString path =
        normalizeRemotePath(
            m_directoryQueue.at(
                m_directoryQueueIndex));

    ++m_directoryQueueIndex;

    /*
     * 如果远程扫描已经发现这个目录，
     * 就不需要重新 MKCOL。
     */
    bool exists = false;

    for (const QVariant &value :
         std::as_const(
             m_remoteEntries)) {

        const QVariantMap map =
            value.toMap();

        QString remotePath =
            map.value(
                   QStringLiteral("path"))
                .toString();

        remotePath =
            normalizeRemotePath(
                remotePath);

        if (remotePath == path) {
            exists = true;
            break;
        }
    }

    if (exists) {

        createNextRemoteDirectory();

        return;
    }

    qDebug()
        << "创建远程目录:"
        << path;

    m_client->createDirectory(path);
}


// ============================================================
// Directory created
// ============================================================

void SyncService::handleDirectoryCreated(
    const QString &remotePath,
    bool ok,
    const QString &error)
{
    if (!m_syncInProgress) {
        return;
    }

    if (!ok) {

        /*
         * 目录已经存在时，WebDAV 服务端可能返回
         * 4xx。
         *
         * 由于同步前已经扫描过远程目录，
         * 这里将错误记录下来，但继续处理。
         */
        qWarning()
            << "创建远程目录失败:"
            << remotePath
            << error;

        m_hasError = true;

        if (m_syncError.isEmpty()) {
            m_syncError =
                QStringLiteral(
                    "创建远程目录失败：%1")
                    .arg(error);
        }

    } else {

        qDebug()
        << "远程目录创建成功:"
        << remotePath;
    }

    createNextRemoteDirectory();
}


// ============================================================
// Upload
// ============================================================

void SyncService::startUploads()
{
    m_uploadQueueIndex = 0;

    if (m_uploadQueue.isEmpty()) {

        /*
         * 当前没有本地文件需要上传。
         */
        finish(
            !m_hasError,
            m_hasError
                ? m_syncError
                : QStringLiteral(
                      "同步完成"));

        return;
    }

    uploadNextFile();
}

void SyncService::uploadNextFile()
{
    if (!m_syncInProgress) {
        return;
    }

    if (m_uploadQueueIndex
        >= m_uploadQueue.size()) {

        finish(
            !m_hasError,
            m_hasError
                ? m_syncError
                : QStringLiteral(
                      "同步完成"));

        return;
    }

    const QPair<QString, QString> pair =
        m_uploadQueue.at(
            m_uploadQueueIndex);

    const QString localPath =
        pair.first;

    const QString remotePath =
        pair.second;

    ++m_uploadQueueIndex;

    if (!QFileInfo::exists(localPath)) {

        qWarning()
        << "本地文件不存在:"
        << localPath;

        m_hasError = true;

        if (m_syncError.isEmpty()) {
            m_syncError =
                QStringLiteral(
                    "本地文件不存在：%1")
                    .arg(localPath);
        }

        uploadNextFile();

        return;
    }

    ++m_pendingOps;

    qDebug()
        << "上传:"
        << localPath
        << "->"
        << remotePath;

    m_client->uploadFile(
        localPath,
        remotePath);
}


// ============================================================
// Upload finished
// ============================================================

void SyncService::handleUploadFinished(
    const QString &remotePath,
    bool ok,
    const QString &error)
{
    if (!m_syncInProgress) {
        return;
    }

    if (m_pendingOps > 0) {
        --m_pendingOps;
    }

    if (!ok) {

        m_hasError = true;

        if (m_syncError.isEmpty()) {
            m_syncError =
                QStringLiteral(
                    "上传失败：%1")
                    .arg(error);
        }

        qWarning()
            << "上传失败:"
            << remotePath
            << error;

    } else {

        qDebug()
        << "上传完成:"
        << remotePath;
    }

    uploadNextFile();
}


// ============================================================
// Download finished
// ============================================================

void SyncService::handleDownloadFinished(
    const QString &remotePath,
    const QString &localPath,
    bool ok,
    const QString &error)
{
    if (!m_syncInProgress) {
        return;
    }

    if (m_pendingOps > 0) {
        --m_pendingOps;
    }

    if (!ok) {

        m_hasError = true;

        if (m_syncError.isEmpty()) {
            m_syncError =
                QStringLiteral(
                    "下载失败：%1")
                    .arg(error);
        }

        qWarning()
            << "下载失败:"
            << remotePath
            << error;

    } else {

        qDebug()
        << "下载完成:"
        << remotePath
        << "->"
        << localPath;
    }
}


// ============================================================
// Process remote Atomic Folder
// ============================================================

void SyncService::processRemoteAtomicFolder(
    const QString &remotePath,
    const QVariantMap &entry)
{
    Q_UNUSED(entry);

    const QString normalized =
        normalizeRemotePath(
            remotePath);

    if (!isRemotePathInCurrentSpace(
            normalized)) {
        return;
    }

    /*
     * 远程 Atomic Folder 本身是目录。
     *
     * 这里不创建 App folder hierarchy。
     *
     * 后续可以通过 metadata / UUID
     * 建立 Atomic Folder 对象。
     */
}


// ============================================================
// Remote path mapping
// ============================================================

QString SyncService::normalizeRemotePath(
    const QString &path) const
{
    if (path.isEmpty()) {
        return QStringLiteral("/");
    }

    QString result =
        path.trimmed();

    /*
     * 允许 WebDavClient 返回完整 URL。
     */
    if (result.startsWith(
            QStringLiteral("http://"),
            Qt::CaseInsensitive)
        ||
        result.startsWith(
            QStringLiteral("https://"),
            Qt::CaseInsensitive)) {

        const QUrl url(result);

        if (url.isValid()) {
            result = url.path();
        }
    }

    /*
     * URL decode。
     */
    result =
        QUrl::fromPercentEncoding(
            result.toUtf8());

    result.replace(
        QLatin1Char('\\'),
        QLatin1Char('/'));

    if (!result.startsWith(
            QLatin1Char('/'))) {

        result.prepend(
            QLatin1Char('/'));
    }

    while (result.contains(
        QStringLiteral("//"))) {

        result.replace(
            QStringLiteral("//"),
            QStringLiteral("/"));
    }

    if (result.length() > 1
        && result.endsWith(
            QLatin1Char('/'))) {

        result.chop(1);
    }

    return result;
}


// ============================================================
// Remote parent
// ============================================================

QString SyncService::remoteParentPath(
    const QString &remotePath) const
{
    const QString normalized =
        normalizeRemotePath(
            remotePath);

    if (normalized == QStringLiteral("/")) {
        return QStringLiteral("/");
    }

    const int slash =
        normalized.lastIndexOf(
            QLatin1Char('/'));

    if (slash <= 0) {
        return QStringLiteral("/");
    }

    return normalized.left(slash);
}


// ============================================================
// Check remote path
// ============================================================

bool SyncService::isRemotePathInCurrentSpace(
    const QString &remotePath) const
{
    const QString root =
        normalizeRemotePath(
            remoteSpaceRoot(
                m_activeSpace));

    const QString path =
        normalizeRemotePath(
            remotePath);

    if (path == root) {
        return false;
    }

    return path.startsWith(
        root + QLatin1Char('/'));
}


// ============================================================
// Finish
// ============================================================

void SyncService::finish(
    bool ok,
    const QString &message)
{
    if (!m_syncInProgress) {
        return;
    }

    m_syncInProgress = false;

    m_pendingOps = 0;
    m_pendingDirectoryListings = 0;

    qDebug()
        << "========================================";

    qDebug()
        << "WebDAV 同步结束";

    qDebug()
        << "space ="
        << static_cast<int>(
               m_activeSpace);

    qDebug()
        << "sourceId ="
        << m_activeSourceId;

    qDebug()
        << "result ="
        << (ok ? "success" : "failed");

    qDebug()
        << "message ="
        << message;

    qDebug()
        << "========================================";

    emit syncFinished(
        ok,
        message);
}


// ============================================================
// Operation tracking
// ============================================================

void SyncService::trackOperation()
{
    if (m_pendingOps > 0) {
        --m_pendingOps;
    }
}


// ============================================================
// Remote deletion
// ============================================================

void SyncService::enqueueRemoteAtomicFolderDeletion(
    const QString &remoteAtomicFolderPath)
{
    const QString normalized =
        normalizeRemotePath(
            remoteAtomicFolderPath);

    const QString root =
        normalizeRemotePath(
            remoteSpaceRoot(
                m_activeSpace));

    /*
     * 绝对禁止删除：
     *
     *     /
     *
     *     /zesbks
     *
     *     /zesbks/group
     *
     * 即当前同步空间根目录。
     */
    if (normalized == QStringLiteral("/")
        || normalized == root) {

        m_remoteDeletionHasError = true;

        if (m_remoteDeletionError.isEmpty()) {
            m_remoteDeletionError =
                QStringLiteral(
                    "不能删除同步空间根目录");
        }

        qWarning()
            << "拒绝删除同步根目录:"
            << normalized;

        return;
    }

    /*
     * 必须属于当前同步 root。
     */
    if (!normalized.startsWith(
            root + QLatin1Char('/'))) {

        m_remoteDeletionHasError = true;

        if (m_remoteDeletionError.isEmpty()) {
            m_remoteDeletionError =
                QStringLiteral(
                    "删除路径不属于当前同步空间");
        }

        qWarning()
            << "拒绝删除当前空间之外的路径:"
            << normalized;

        return;
    }

    /*
     * Atomic Folder 必须是 root 的直接子目录。
     *
     * 对：
     *
     *     /zesbks/
     *
     * 合法：
     *
     *     /zesbks/A
     *
     * 对：
     *
     *     /zesbks/group/
     *
     * 合法：
     *
     *     /zesbks/group/A
     */
    const QString relative =
        normalized.mid(
            root.length() + 1);

    if (relative.isEmpty()
        || relative.contains(
            QLatin1Char('/'))) {

        m_remoteDeletionHasError = true;

        if (m_remoteDeletionError.isEmpty()) {
            m_remoteDeletionError =
                QStringLiteral(
                    "删除目标不是当前空间的 Atomic Folder");
        }

        qWarning()
            << "拒绝删除非 Atomic Folder:"
            << normalized;

        return;
    }

    if (!m_remoteDeleteQueue.contains(
            normalized)) {

        m_remoteDeleteQueue.append(
            normalized);
    }
}


// ============================================================
// Delete one
// ============================================================

void SyncService::deleteRemoteAtomicFolder(
    const QString &remoteAtomicFolderPath,
    const QString &sourceId)
{
    if (m_syncInProgress) {

        emit remoteDeletionFinished(
            false,
            QStringLiteral(
                "当前正在同步，请稍后再删除"));

        return;
    }

    if (m_remoteDeletionInProgress) {

        emit remoteDeletionFinished(
            false,
            QStringLiteral(
                "已有远程删除任务正在执行"));

        return;
    }

    if (!m_client) {

        emit remoteDeletionFinished(
            false,
            QStringLiteral(
                "WebDAV 客户端不可用"));

        return;
    }

    if (!sourceId.isEmpty()) {
        m_activeSourceId = sourceId;
    }

    m_remoteDeleteQueue.clear();
    m_remoteDeleteQueueIndex = 0;

    m_remoteDeletionHasError = false;
    m_remoteDeletionError.clear();

    enqueueRemoteAtomicFolderDeletion(
        remoteAtomicFolderPath);

    if (m_remoteDeleteQueue.isEmpty()) {

        emit remoteDeletionFinished(
            false,
            m_remoteDeletionError.isEmpty()
                ? QStringLiteral(
                      "没有有效的删除目标")
                : m_remoteDeletionError);

        return;
    }

    startRemoteDeletion();
}


// ============================================================
// Delete multiple
// ============================================================

void SyncService::deleteRemoteAtomicFolders(
    const QStringList &remoteAtomicFolderPaths,
    const QString &sourceId)
{
    if (m_syncInProgress) {

        emit remoteDeletionFinished(
            false,
            QStringLiteral(
                "当前正在同步，请稍后再删除"));

        return;
    }

    if (m_remoteDeletionInProgress) {

        emit remoteDeletionFinished(
            false,
            QStringLiteral(
                "已有远程删除任务正在执行"));

        return;
    }

    if (!m_client) {

        emit remoteDeletionFinished(
            false,
            QStringLiteral(
                "WebDAV 客户端不可用"));

        return;
    }

    if (!sourceId.isEmpty()) {
        m_activeSourceId = sourceId;
    }

    m_remoteDeleteQueue.clear();
    m_remoteDeleteQueueIndex = 0;

    m_remoteDeletionHasError = false;
    m_remoteDeletionError.clear();

    for (const QString &path :
         remoteAtomicFolderPaths) {

        enqueueRemoteAtomicFolderDeletion(
            path);
    }

    if (m_remoteDeleteQueue.isEmpty()) {

        emit remoteDeletionFinished(
            false,
            m_remoteDeletionError.isEmpty()
                ? QStringLiteral(
                      "没有有效的删除目标")
                : m_remoteDeletionError);

        return;
    }

    startRemoteDeletion();
}


// ============================================================
// Start remote deletion
// ============================================================

void SyncService::startRemoteDeletion()
{
    if (m_remoteDeletionInProgress) {
        return;
    }

    if (!m_client) {

        m_remoteDeletionHasError = true;

        m_remoteDeletionError =
            QStringLiteral(
                "WebDAV 客户端不可用");

        emit remoteDeletionFinished(
            false,
            m_remoteDeletionError);

        return;
    }

    m_remoteDeleteQueueIndex = 0;

    m_remoteDeletionInProgress = true;

    qDebug()
        << "========================================";

    qDebug()
        << "开始删除 WebDAV Atomic Folder";

    qDebug()
        << "count ="
        << m_remoteDeleteQueue.size();

    qDebug()
        << "root ="
        << remoteSpaceRoot(
               m_activeSpace);

    qDebug()
        << "========================================";

    deleteNextRemoteAtomicFolder();
}


// ============================================================
// Delete next
// ============================================================

void SyncService::deleteNextRemoteAtomicFolder()
{
    if (!m_remoteDeletionInProgress) {
        return;
    }

    if (m_remoteDeleteQueueIndex
        >= m_remoteDeleteQueue.size()) {

        const bool ok =
            !m_remoteDeletionHasError;

        const QString message =
            ok
                ? QStringLiteral(
                      "WebDAV Atomic Folder 删除完成")
                : (m_remoteDeletionError.isEmpty()
                       ? QStringLiteral(
                             "部分 Atomic Folder 删除失败")
                       : m_remoteDeletionError);

        m_remoteDeletionInProgress = false;

        emit remoteDeletionFinished(
            ok,
            message);

        m_remoteDeleteQueue.clear();
        m_remoteDeleteQueueIndex = 0;

        return;
    }

    const QString remotePath =
        normalizeRemotePath(
            m_remoteDeleteQueue.at(
                m_remoteDeleteQueueIndex));

    const QString root =
        normalizeRemotePath(
            remoteSpaceRoot(
                m_activeSpace));

    /*
     * 最后一道安全检查。
     */
    if (remotePath == QStringLiteral("/")
        || remotePath == root
        || !remotePath.startsWith(
            root + QLatin1Char('/'))) {

        m_remoteDeletionHasError = true;

        if (m_remoteDeletionError.isEmpty()) {
            m_remoteDeletionError =
                QStringLiteral(
                    "检测到非法删除路径：%1")
                    .arg(remotePath);
        }

        ++m_remoteDeleteQueueIndex;

        deleteNextRemoteAtomicFolder();

        return;
    }

    /*
     * Atomic Folder 必须直接位于
     * 当前空间 root 下。
     *
     * MyDoc:
     *
     *     /zesbks/A
     *
     * Group:
     *
     *     /zesbks/group/A
     */
    const QString relative =
        remotePath.mid(
            root.length() + 1);

    if (relative.isEmpty()
        || relative.contains(
            QLatin1Char('/'))) {

        m_remoteDeletionHasError = true;

        if (m_remoteDeletionError.isEmpty()) {
            m_remoteDeletionError =
                QStringLiteral(
                    "检测到非法 Atomic Folder 路径：%1")
                    .arg(remotePath);
        }

        ++m_remoteDeleteQueueIndex;

        deleteNextRemoteAtomicFolder();

        return;
    }

    qDebug()
        << "删除远程 Atomic Folder:"
        << remotePath;

    /*
     * WebDavClient::removeFile()
     *
     * 对目录执行 HTTP DELETE。
     *
     * WebDAV server 会删除该 collection
     * 及其内部内容。
     */
    m_client->removeFile(
        remotePath);
}


// ============================================================
// Remove finished
// ============================================================

void SyncService::handleRemoveFinished(
    const QString &remotePath,
    bool ok,
    const QString &error)
{
    if (!m_remoteDeletionInProgress) {
        return;
    }

    const QString normalized =
        normalizeRemotePath(
            remotePath);

    if (!ok) {

        m_remoteDeletionHasError = true;

        if (m_remoteDeletionError.isEmpty()) {
            m_remoteDeletionError =
                QStringLiteral(
                    "删除 %1 失败：%2")
                    .arg(
                        normalized,
                        error);
        }

        qWarning()
            << "WebDAV 删除失败:"
            << normalized
            << error;

    } else {

        qDebug()
        << "WebDAV 删除成功:"
        << normalized;
    }

    ++m_remoteDeleteQueueIndex;

    deleteNextRemoteAtomicFolder();
}

} // namespace litapp