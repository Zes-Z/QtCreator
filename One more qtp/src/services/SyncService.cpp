#include "services/SyncService.h"

#include "services/WebDavClient.h"
#include "storage/MetadataStore.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <QDebug>

namespace litapp {

SyncService::SyncService(WebDavClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_timer(new QTimer(this))
{
    if (m_client) {
        connect(m_client,
                &WebDavClient::directoryListed,
                this,
                &SyncService::handleDirectoryListed);

        connect(m_client,
                &WebDavClient::directoryCreated,
                this,
                &SyncService::handleDirectoryCreated);

        connect(m_client,
                &WebDavClient::uploadFinished,
                this,
                &SyncService::handleUploadFinished);

        connect(m_client,
                &WebDavClient::downloadFinished,
                this,
                &SyncService::handleDownloadFinished);
    }

    m_timer->setInterval(180000);

    connect(m_timer,
            &QTimer::timeout,
            this,
            &SyncService::autoSync);
}


/*
 * ============================================================
 * syncZone
 * ============================================================
 *
 * 新架构：
 *
 *     本地
 *
 *     Documents/LitApp/zesbks/
 *             │
 *             ├── 文件1/
 *             │    ├── 文件1.pdf
 *             │    └── notes.json
 *             │
 *             ├── 文件2/
 *             │    └── 文件2.pdf
 *             │
 *             └── 文件3/
 *                  └── 文件3.pdf
 *
 *
 *     WebDAV
 *
 *     /zesbks/
 *             │
 *             ├── 文件1/
 *             │    ├── 文件1.pdf
 *             │    └── notes.json
 *             │
 *             ├── 文件2/
 *             │    └── 文件2.pdf
 *             │
 *             └── 文件3/
 *                  └── 文件3.pdf
 *
 *
 * zone 不再决定 WebDAV 目录。
 *
 * 分类、子分类等属于应用内部逻辑。
 */
void SyncService::syncZone(int zone)
{
    if (m_syncInProgress) {
        qDebug() << "同步已经在进行中，忽略本次请求。";
        return;
    }

    m_zone = zone;

    m_pendingOps = 0;
    m_pendingDirectoryListings = 0;

    m_hasError = false;
    m_syncInProgress = true;

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

    if (!m_client) {
        finish(false,
               QStringLiteral("WebDAV 客户端不可用"));
        return;
    }

    const QString localRoot = localZoneRoot(zone);

    if (!QDir(localRoot).exists()) {
        qDebug() << "本地 zesbks 根目录不存在，尝试创建:"
                 << localRoot;

        if (!QDir().mkpath(localRoot)) {
            finish(false,
                   QStringLiteral("无法创建本地 zesbks 目录：%1")
                       .arg(localRoot));
            return;
        }
    }

    /*
     * WebDAV 永远只有一个同步根：
     *
     *     /zesbks/
     */
    const QString remoteRoot =
        normalizeRemotePath(remoteZoneRoot(zone));

    qDebug() << "========================================";
    qDebug() << "开始同步";
    qDebug() << "zone =" << zone;
    qDebug() << "本地根目录 =" << localRoot;
    qDebug() << "远程根目录 =" << remoteRoot;
    qDebug() << "========================================";

    /*
     * 先递归读取远程 zesbks。
     *
     * 目的：
     *     1. 获取远程已经存在的文件
     *     2. 防止每次同步都重新上传全部文件
     *     3. 获取完整 remotePath，而不是只看文件名
     */
    m_visitedDirs.insert(remoteRoot);

    ++m_pendingDirectoryListings;

    m_client->listDirectory(remoteRoot);
}


/*
 * ============================================================
 * Auto Sync
 * ============================================================
 */

void SyncService::startAutoSync(int intervalMs)
{
    if (intervalMs <= 0) {
        intervalMs = 180000;
    }

    m_timer->setInterval(intervalMs);
    m_timer->start();

    qDebug() << "WebDAV 自动同步已启动，间隔:"
             << intervalMs << "ms";
}


void SyncService::stopAutoSync()
{
    m_timer->stop();

    qDebug() << "WebDAV 自动同步已停止";
}


void SyncService::setActiveZone(int zone)
{
    m_zone = zone;
}


void SyncService::autoSync()
{
    if (m_syncInProgress) {
        qDebug() << "自动同步触发，但上一次同步仍未完成。";
        return;
    }

    syncZone(m_zone);
}


/*
 * ============================================================
 * Remote directory listing
 * ============================================================
 */

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

    /*
     * WebDAV 的 /zesbks 如果还不存在，
     * 第一次同步时可能会返回 404。
     *
     * 这种情况下不能直接终止同步。
     *
     * 因为我们随后会通过 MKCOL 创建 /zesbks。
     */
    if (!error.isEmpty()) {

        qWarning() << "远程目录读取失败:"
                   << normalizedPath
                   << error;

        /*
         * 如果只是远程目录不存在：
         * 继续执行本地扫描。
         */
        m_hasError = false;
    } else {

        /*
         * 读取当前目录。
         */
        for (const QVariant &value : entries) {

            const QVariantMap map = value.toMap();

            bool isDirectory =
                map.value(QStringLiteral("isDirectory"))
                    .toBool();

            QString entryPath =
                map.value(QStringLiteral("path"))
                    .toString();

            QString entryName =
                map.value(QStringLiteral("name"))
                    .toString();

            entryPath =
                normalizeRemotePath(entryPath);

            if (entryPath.isEmpty()) {
                continue;
            }

            /*
             * 防止 WebDAV 返回自己：
             *
             * /zesbks/
             *
             * 再次进入 visitedDirs。
             */
            if (isDirectory) {

                if (!m_visitedDirs.contains(entryPath)) {

                    m_visitedDirs.insert(entryPath);

                    ++m_pendingDirectoryListings;

                    qDebug() << "递归读取远程目录:"
                             << entryPath;

                    m_client->listDirectory(entryPath);
                }

            } else {

                /*
                 * 保存远程文件完整路径。
                 *
                 * 不再使用：
                 *
                 *     filename
                 *
                 * 作为唯一判断依据。
                 */
                m_remoteEntries.append(map);

                m_remoteNames.insert(entryPath);

                qDebug() << "发现远程文件:"
                         << entryPath;
            }
        }
    }

    /*
     * 如果还有目录正在读取，
     * 等全部 PROPFIND 完成以后再进入下一阶段。
     */
    if (m_pendingDirectoryListings > 0) {
        return;
    }

    /*
     * 所有远程目录读取完成。
     *
     * 现在开始扫描本地 Atomic Folder。
     */
    scanLocalFiles();

    /*
     * 为所有 Atomic Folder 创建所需的远程目录。
     */
    for (const QString &atomicFolder :
         std::as_const(m_atomicFolderQueue)) {

        addAtomicFolderDirectories(
            atomicFolder,
            m_zone);
    }

    /*
     * 如果没有任何本地 Atomic Folder，
     * 也需要正常结束。
     */
    if (m_atomicFolderQueue.isEmpty()) {

        finish(
            !m_hasError,
            m_hasError
                ? QStringLiteral("同步完成，但存在错误")
                : QStringLiteral("同步完成：没有需要同步的文件"));

        return;
    }

    /*
     * 开始创建远程目录。
     */
    startDirectoryCreation();
}


/*
 * ============================================================
 * Remote directory creation
 * ============================================================
 */

void SyncService::startDirectoryCreation()
{
    m_directoryQueueIndex = 0;

    /*
     * 没有目录需要创建。
     */
    if (m_directoryQueue.isEmpty()) {

        startUploads();
        return;
    }

    qDebug() << "需要准备远程目录数量:"
             << m_directoryQueue.size();

    createNextRemoteDirectory();
}


void SyncService::createNextRemoteDirectory()
{
    if (!m_syncInProgress) {
        return;
    }

    /*
     * 所有目录已经创建完成。
     */
    if (m_directoryQueueIndex >=
        m_directoryQueue.size()) {

        qDebug() << "所有远程目录准备完成。";

        startUploads();
        return;
    }

    const QString remotePath =
        normalizeRemotePath(
            m_directoryQueue.at(
                m_directoryQueueIndex));

    ++m_directoryQueueIndex;

    qDebug() << "创建远程目录:"
             << remotePath;

    /*
     * WebDavClient 已经处理：
     *
     * HTTP 405 = 目录已经存在 = success
     */
    m_client->createDirectory(remotePath);
}


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
         * 目录创建失败。
         *
         * 不立即终止整个同步。
         * 继续处理剩余目录。
         *
         * 后续上传如果因此失败，
         * 再由 uploadFinished 标记错误。
         */
        m_hasError = true;

        qWarning() << "远程目录创建失败:"
                   << remotePath
                   << error;
    } else {

        qDebug() << "远程目录已准备:"
                 << remotePath;
    }

    /*
     * 继续创建下一个目录。
     */
    createNextRemoteDirectory();
}


/*
 * ============================================================
 * Upload
 * ============================================================
 */

void SyncService::startUploads()
{
    /*
     * uploadQueue 在 scanLocalFiles()
     * 中已经建立。
     */
    m_uploadQueueIndex = 0;

    qDebug() << "========================================";
    qDebug() << "开始上传";
    qDebug() << "待上传文件数量:"
             << m_uploadQueue.size();
    qDebug() << "========================================";

    if (m_uploadQueue.isEmpty()) {

        finish(
            !m_hasError,
            m_hasError
                ? QStringLiteral("同步完成，但存在错误")
                : QStringLiteral("同步完成：所有文件已经是最新状态"));

        return;
    }

    uploadNextFile();
}


void SyncService::uploadNextFile()
{
    if (!m_syncInProgress) {
        return;
    }

    /*
     * 所有文件上传完毕。
     */
    if (m_uploadQueueIndex >=
        m_uploadQueue.size()) {

        qDebug() << "========================================";
        qDebug() << "全部文件上传完成";
        qDebug() << "========================================";

        finish(
            !m_hasError,
            m_hasError
                ? QStringLiteral("部分文件同步失败，请查看日志")
                : QStringLiteral("同步完成"));

        return;
    }

    const auto pair =
        m_uploadQueue.at(m_uploadQueueIndex);

    const QString localPath = pair.first;
    const QString remotePath = pair.second;

    qDebug() << "上传文件 ["
             << (m_uploadQueueIndex + 1)
             << "/"
             << m_uploadQueue.size()
             << "]";

    qDebug() << "本地:"
             << localPath;

    qDebug() << "远程:"
             << remotePath;

    /*
     * 一个文件一个文件上传。
     *
     * 这样可以保证：
     *
     * 文件1完成
     *     ↓
     * 文件2
     *     ↓
     * 文件3
     *
     * 不会因为第一个异步请求结束，
     * 就误判整个同步已经结束。
     */
    m_pendingOps = 1;

    m_client->uploadFile(
        localPath,
        remotePath);
}


void SyncService::handleUploadFinished(
    const QString &remotePath,
    bool ok,
    const QString &error)
{
    if (!m_syncInProgress) {
        return;
    }

    if (!ok) {

        m_hasError = true;

        qWarning() << "文件上传失败:"
                   << remotePath
                   << error;

    } else {

        qDebug() << "文件上传成功:"
                 << remotePath;
    }

    /*
     * 当前文件完成。
     */
    m_pendingOps = 0;

    /*
     * 非常重要：
     *
     * 当前文件完成以后，
     * 必须进入下一个文件。
     *
     * 原来的代码在这里调用 trackOperation()
     * 后直接 finish()，
     * 这就是“只上传文件1”的重要原因之一。
     */
    ++m_uploadQueueIndex;

    uploadNextFile();
}


/*
 * ============================================================
 * Local scanning
 * ============================================================
 */

void SyncService::scanLocalFiles()
{
    m_atomicFolderQueue.clear();
    m_atomicFolderQueueIndex = 0;

    m_uploadQueue.clear();
    m_uploadQueueIndex = 0;

    const QString root =
        localZoneRoot(m_zone);

    qDebug() << "========================================";
    qDebug() << "扫描本地 Atomic Folder";
    qDebug() << "本地根目录:" << root;
    qDebug() << "========================================";

    /*
     * zesbks 不存在：
     *
     * 自动创建。
     */
    if (!QDir(root).exists()) {

        if (!QDir().mkpath(root)) {

            m_hasError = true;

            qWarning() << "无法创建本地根目录:"
                       << root;

            return;
        }
    }

    /*
     * 只读取 zesbks 的一级目录。
     *
     * 例如：
     *
     * zesbks/
     *   文件1/
     *   文件2/
     *   文件3/
     *
     * 文件1内部的子目录不会被认为是新的 Atomic Folder。
     */
    const QStringList atomicFolders =
        findAtomicFolders(root);

    for (const QString &folder :
         atomicFolders) {

        m_atomicFolderQueue.append(folder);

        qDebug() << "发现 Atomic Folder:"
                 << folder;
    }

    /*
     * 对每一个 Atomic Folder 递归扫描。
     */
    for (const QString &atomicFolder :
         m_atomicFolderQueue) {

        const QStringList files =
            collectAtomicFolderFiles(
                atomicFolder);

        for (const QString &localFile :
             files) {

            const QString remotePath =
                remotePathForLocalPath(
                    localFile,
                    m_zone);

            /*
             * 只根据完整 remotePath 判断。
             *
             * 例如：
             *
             * /zesbks/文件1/a.pdf
             *
             * 和：
             *
             * /zesbks/文件2/a.pdf
             *
             * 是两个完全不同的文件。
             */
            bool remoteExists = false;

            for (const QVariant &value :
                 std::as_const(m_remoteEntries)) {

                const QVariantMap map =
                    value.toMap();

                QString remoteExistingPath =
                    map.value(
                        QStringLiteral("path"))
                        .toString();

                remoteExistingPath =
                    normalizeRemotePath(
                        remoteExistingPath);

                if (remoteExistingPath ==
                    remotePath) {

                    remoteExists = true;
                    break;
                }
            }

            if (!remoteExists) {

                m_uploadQueue.append(
                    qMakePair(
                        localFile,
                        remotePath));

                qDebug() << "加入上传队列:";
                qDebug() << "  local ="
                         << localFile;
                qDebug() << "  remote ="
                         << remotePath;
            }
        }
    }

    qDebug() << "Atomic Folder 数量:"
             << m_atomicFolderQueue.size();

    qDebug() << "需要上传文件数量:"
             << m_uploadQueue.size();
}


/*
 * ============================================================
 * Local root
 * ============================================================
 */

QString SyncService::localZoneRoot(int zone) const
{
    Q_UNUSED(zone);

    /*
     * 新架构：
     *
     * Documents/LitApp/zesbks/
     *
     * zone 不再产生：
     *
     *     my doc/
     *     group doc/
     *
     * 这样的 WebDAV 目录层级。
     */
    return QStandardPaths::writableLocation(
               QStandardPaths::DocumentsLocation)
           + QStringLiteral("/LitApp/zesbks");
}


/*
 * ============================================================
 * Remote root
 * ============================================================
 */

QString SyncService::remoteZoneRoot(int zone) const
{
    Q_UNUSED(zone);

    /*
     * 所有 Atomic Folder 都直接放在：
     *
     *     /zesbks/
     */
    return QStringLiteral("/zesbks/");
}


/*
 * ============================================================
 * Atomic Folder
 * ============================================================
 */

bool SyncService::isAtomicFolder(
    const QString &localPath) const
{
    const QFileInfo info(localPath);

    return info.exists()
           && info.isDir();
}


QStringList SyncService::findAtomicFolders(
    const QString &zoneRoot) const
{
    QStringList result;

    QDir dir(zoneRoot);

    if (!dir.exists()) {
        return result;
    }

    const QFileInfoList entries =
        dir.entryInfoList(
            QDir::Dirs
            | QDir::NoDotAndDotDot,
            QDir::Name);

    for (const QFileInfo &info :
         entries) {

        if (!info.isDir()) {
            continue;
        }

        /*
         * zesbks 的一级目录就是 Atomic Folder。
         */
        result.append(
            QDir::cleanPath(
                info.absoluteFilePath()));
    }

    return result;
}


QStringList SyncService::collectAtomicFolderFiles(
    const QString &atomicFolder) const
{
    QStringList result;

    QDir root(atomicFolder);

    if (!root.exists()) {
        return result;
    }

    /*
     * 递归扫描。
     *
     * 支持任意层级：
     *
     * 文件1/
     *   文件1.pdf
     *   notes/
     *       note.json
     *   annotations/
     *       page1.json
     *       page2.json
     */
    QDirIterator iterator(
        atomicFolder,
        QDir::Files
            | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);

    while (iterator.hasNext()) {

        const QString filePath =
            QDir::cleanPath(
                iterator.next());

        const QFileInfo info(filePath);

        if (!info.exists() ||
            !info.isFile()) {
            continue;
        }

        result.append(filePath);
    }

    return result;
}


QString SyncService::atomicFolderForLocalFile(
    const QString &localPath,
    int zone) const
{
    Q_UNUSED(zone);

    const QString root =
        QDir::cleanPath(
            localZoneRoot(m_zone));

    const QString file =
        QDir::cleanPath(localPath);

    if (!file.startsWith(
            root + QLatin1Char('/'))) {

        return QString();
    }

    const QString relative =
        file.mid(root.length() + 1);

    const int slashIndex =
        relative.indexOf(QLatin1Char('/'));

    if (slashIndex < 0) {
        return QString();
    }

    const QString folderName =
        relative.left(slashIndex);

    return QDir::cleanPath(
        root
        + QLatin1Char('/')
        + folderName);
}


QString SyncService::remoteAtomicFolderPath(
    const QString &atomicFolder,
    int zone) const
{
    Q_UNUSED(zone);

    const QString root =
        QDir::cleanPath(
            localZoneRoot(m_zone));

    const QString folder =
        QDir::cleanPath(
            atomicFolder);

    if (!folder.startsWith(
            root + QLatin1Char('/'))) {

        return QString();
    }

    const QString folderName =
        folder.mid(root.length() + 1);

    if (folderName.isEmpty() ||
        folderName.contains(QLatin1Char('/'))) {

        return QString();
    }

    return normalizeRemotePath(
        QStringLiteral("/zesbks/%1")
            .arg(folderName));
}


/*
 * ============================================================
 * Local → Remote path
 * ============================================================
 */

QString SyncService::remotePathForLocalPath(
    const QString &localPath,
    int zone) const
{
    Q_UNUSED(zone);

    const QString root =
        QDir::cleanPath(
            localZoneRoot(m_zone));

    const QString file =
        QDir::cleanPath(localPath);

    if (!file.startsWith(
            root + QLatin1Char('/'))) {

        return QString();
    }

    /*
     * 例如：
     *
     * local:
     *
     * C:/Users/xxx/Documents/LitApp/
     *     zesbks/文件1/annotations/page1.json
     *
     * relative:
     *
     * 文件1/annotations/page1.json
     *
     * remote:
     *
     * /zesbks/文件1/annotations/page1.json
     */
    const QString relative =
        file.mid(root.length() + 1);

    return normalizeRemotePath(
        QStringLiteral("/zesbks/%1")
            .arg(relative));
}


/*
 * ============================================================
 * Remote path utilities
 * ============================================================
 */

QString SyncService::normalizeRemotePath(
    const QString &path) const
{
    if (path.isEmpty()) {
        return QStringLiteral("/");
    }

    QString result = path.trimmed();

    /*
     * 如果 WebDAV 返回完整 URL：
     *
     * https://example.com/dav/zesbks/a.pdf
     *
     * 尝试只取 URL path。
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
     *
     * WebDAV XML 中可能出现：
     *
     *     %20
     *
     * 之类的编码。
     */
    result =
        QUrl::fromPercentEncoding(
            result.toUtf8());

    /*
     * 所有反斜杠统一成 /
     */
    result.replace(
        QLatin1Char('\\'),
        QLatin1Char('/'));

    /*
     * 保证从 / 开始。
     */
    if (!result.startsWith(
            QLatin1Char('/'))) {

        result.prepend(
            QLatin1Char('/'));
    }

    /*
     * 压缩连续的 //。
     */
    while (result.contains(
               QStringLiteral("//"))) {

        result.replace(
            QStringLiteral("//"),
            QStringLiteral("/"));
    }

    /*
     * 去掉结尾 /
     *
     * 根目录 / 除外。
     */
    if (result.length() > 1 &&
        result.endsWith(
            QLatin1Char('/'))) {

        result.chop(1);
    }

    return result;
}


QString SyncService::remoteParentPath(
    const QString &remotePath) const
{
    const QString normalized =
        normalizeRemotePath(remotePath);

    const int slash =
        normalized.lastIndexOf(
            QLatin1Char('/'));

    if (slash <= 0) {
        return QStringLiteral("/");
    }

    return normalized.left(slash);
}


/*
 * ============================================================
 * Remote directory queue
 * ============================================================
 */

void SyncService::addParentDirectories(
    const QString &remotePath)
{
    QString parent =
        remoteParentPath(remotePath);

    QStringList parents;

    while (!parent.isEmpty() &&
           parent != QStringLiteral("/")) {

        parents.prepend(parent);

        const QString next =
            remoteParentPath(parent);

        if (next == parent) {
            break;
        }

        parent = next;
    }

    /*
     * 最终可能得到：
     *
     * /zesbks
     * /zesbks/文件1
     * /zesbks/文件1/attachments
     */
    for (const QString &dir :
         parents) {

        const QString normalized =
            normalizeRemotePath(dir);

        if (!m_directoryQueue.contains(
                normalized)) {

            m_directoryQueue.append(
                normalized);
        }
    }
}


void SyncService::addAtomicFolderDirectories(
    const QString &atomicFolder,
    int zone)
{
    Q_UNUSED(zone);

    /*
     * Atomic Folder 自己：
     *
     * /zesbks/文件1
     */
    const QString atomicRemote =
        remoteAtomicFolderPath(
            atomicFolder,
            m_zone);

    if (atomicRemote.isEmpty()) {
        return;
    }

    /*
     * Atomic Folder 本身及其内部所有
     * 子目录都需要创建。
     *
     * 我们通过文件的 remotePath
     * 自动获得所有父目录。
     */
    const QStringList files =
        collectAtomicFolderFiles(
            atomicFolder);

    /*
     * 即使 Atomic Folder 暂时为空，
     * 也需要创建 Atomic Folder 本身。
     */
    if (files.isEmpty()) {

        if (!m_directoryQueue.contains(
                atomicRemote)) {

            addParentDirectories(
                atomicRemote);

            if (!m_directoryQueue.contains(
                    atomicRemote)) {

                m_directoryQueue.append(
                    atomicRemote);
            }
        }

        return;
    }

    for (const QString &localFile :
         files) {

        const QString remoteFile =
            remotePathForLocalPath(
                localFile,
                m_zone);

        if (remoteFile.isEmpty()) {
            continue;
        }

        addParentDirectories(
            remoteFile);
    }
}


/*
 * ============================================================
 * Download
 * ============================================================
 *
 * 当前仍然保持你原来的策略：
 *
 * WebDAV 远程文件不主动下载。
 *
 * 如果以后需要：
 *
 *     打开文件
 *         ↓
 *     downloadFile()
 *
 * 可以继续使用这里。
 */

void SyncService::handleDownloadFinished(
    const QString &remotePath,
    const QString &localPath,
    bool ok,
    const QString &error)
{
    if (!m_syncInProgress) {
        return;
    }

    Q_UNUSED(remotePath)
    Q_UNUSED(localPath)

    if (!ok) {

        m_hasError = true;

        qWarning() << "Download failed:"
                   << error;
    }

    trackOperation();
}


/*
 * ============================================================
 * Operation tracking
 * ============================================================
 *
 * 这个函数保留，用于兼容现有结构。
 *
 * 文件上传现在由 uploadNextFile()
 * 自己控制队列，不再依赖这里结束整个同步。
 */

void SyncService::trackOperation()
{
    if (m_pendingOps > 0) {
        --m_pendingOps;
    }

    /*
     * 不在这里直接 finish()。
     *
     * 因为：
     *
     * uploadFinished()
     *     ↓
     * uploadNextFile()
     *
     * 必须继续处理剩余文件。
     */
}


/*
 * ============================================================
 * Finish
 * ============================================================
 */

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

    qDebug() << "========================================";
    qDebug() << "WebDAV 同步结束";
    qDebug() << "结果:"
             << (ok ? "成功" : "失败");
    qDebug() << "消息:"
             << message;
    qDebug() << "========================================";

    emit syncFinished(
        ok,
        message);
}


} // namespace litapp