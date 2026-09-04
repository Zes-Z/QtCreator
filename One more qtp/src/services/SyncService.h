#pragma once

#include "models/FileItem.h"

#include <QList>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

namespace litapp {

class WebDavClient;

/*
 * ============================================================
 * SyncService
 * ============================================================
 *
 * WebDAV 与 App 内部逻辑树是两个不同的层。
 *
 * App 内部：
 *
 *     my doc
 *         ├── folder A
 *         │    ├── file 1
 *         │    └── file 2
 *         └── folder B
 *
 *     group me
 *         ├── folder C
 *         └── file 3
 *
 *     group us
 *         ├── folder D
 *         └── file 4
 *
 *
 * WebDAV：
 *
 *     /zesbks/
 *         ├── AtomicFolder1/
 *         │    └── AtomicFile1.pdf
 *         ├── AtomicFolder2/
 *         │    └── AtomicFile2.docx
 *         └── group/
 *              ├── AtomicFolder3/
 *              │    └── AtomicFile3.pdf
 *              └── AtomicFolder4/
 *                   └── AtomicFile4.docx
 *
 *
 * 重要原则：
 *
 * 1. App 内部的 folder hierarchy 不直接映射成 WebDAV hierarchy。
 *
 * 2. my doc：
 *
 *       WebDAV /zesbks/
 *
 *    但不读取：
 *
 *       /zesbks/group/
 *
 * 3. group me：
 *
 *       WebDAV /zesbks/group/
 *
 * 4. group us：
 *
 *       WebDAV 某个远程账号的 /zesbks/group/
 *
 * 5. Atomic Folder 是 WebDAV 上的同步原子单位。
 *
 *       AtomicFolder/
 *           └── AtomicFile
 *
 * 6. Atomic Folder 的名字与 Atomic File 的名字彼此独立。
 *
 * 7. App 中的分类 category、tag 等元数据不决定 WebDAV
 *    的物理目录结构。
 *
 * 8. group doc 旧概念彻底废弃。
 */
class SyncService : public QObject
{
    Q_OBJECT

public:

    explicit SyncService(
        WebDavClient *client,
        QObject *parent = nullptr);

    /*
     * ========================================================
     * Sync
     * ========================================================
     */

    /*
     * 同步指定的 App 空间。
     *
     * space：
     *
     *     FileSpace::MyDoc
     *     FileSpace::GroupMe
     *     FileSpace::GroupUs
     *
     * sourceId：
     *
     *     对 MyDoc / GroupMe 通常为空。
     *
     *     对 GroupUs：
     *     用于标识具体的远程群组来源。
     */
    Q_INVOKABLE void syncSpace(
        int space,
        const QString &sourceId = QString());

    /*
     * 设置当前活动同步空间。
     */
    Q_INVOKABLE void setActiveSpace(
        int space,
        const QString &sourceId = QString());

    /*
     * 自动同步。
     */
    Q_INVOKABLE void startAutoSync(
        int intervalMs = 180000);

    Q_INVOKABLE void stopAutoSync();

    /*
     * ========================================================
     * Remote deletion
     * ========================================================
     */

    /*
     * 删除一个远程 Atomic Folder。
     *
     * 合法：
     *
     *     /zesbks/AtomicFolder
     *
     *     /zesbks/group/AtomicFolder
     *
     * 非法：
     *
     *     /zesbks
     *
     *     /zesbks/group
     *
     *     /zesbks/AtomicFolder/file.pdf
     */
    Q_INVOKABLE void deleteRemoteAtomicFolder(
        const QString &remoteAtomicFolderPath,
        const QString &sourceId = QString());

    /*
     * 批量删除远程 Atomic Folder。
     */
    Q_INVOKABLE void deleteRemoteAtomicFolders(
        const QStringList &remoteAtomicFolderPaths,
        const QString &sourceId = QString());

signals:

    /*
     * 普通同步完成。
     */
    void syncFinished(
        bool ok,
        const QString &message);

    /*
     * 远程 Atomic Folder 删除完成。
     */
    void remoteDeletionFinished(
        bool ok,
        const QString &message);

private slots:

    /*
     * 自动同步。
     */
    void autoSync();

    /*
     * ========================================================
     * WebDAV callbacks
     * ========================================================
     */

    void handleDirectoryListed(
        const QString &path,
        const QVariantList &entries,
        const QString &error);

    void handleDirectoryCreated(
        const QString &remotePath,
        bool ok,
        const QString &error);

    void handleUploadFinished(
        const QString &remotePath,
        bool ok,
        const QString &error);

    void handleDownloadFinished(
        const QString &remotePath,
        const QString &localPath,
        bool ok,
        const QString &error);

    void handleRemoveFinished(
        const QString &remotePath,
        bool ok,
        const QString &error);

private:

    /*
     * ========================================================
     * Sync state
     * ========================================================
     */

    FileSpace currentSpace() const;

    QString currentSourceId() const;

    void finish(
        bool ok,
        const QString &message);

    /*
     * ========================================================
     * Local roots
     * ========================================================
     *
     * 本地目录结构与 WebDAV 结构分离。
     *
     * 这里的 root 只是 App 本地工作区。
     */

    QString localSpaceRoot(
        FileSpace space,
        const QString &sourceId = QString()) const;

    /*
     * ========================================================
     * Remote roots
     * ========================================================
     *
     * WebDAV 物理结构：
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
     *     /zesbks/group/
     *
     * 注意：
     *
     * GroupUs 的 sourceId 决定使用哪个远程账号，
     * 但不会改变该账号内部的 WebDAV 路径。
     */

    QString remoteSpaceRoot(
        FileSpace space) const;

    /*
     * ========================================================
     * Metadata
     * ========================================================
     */

    QVector<FileItem> loadSpaceItems(
        FileSpace space,
        const QString &sourceId = QString()) const;

    QVector<FileItem> atomicFilesForFolder(
        const FileItem &atomicFolder) const;

    QVector<FileItem> descendantItems(
        const QString &parentId,
        FileSpace space,
        const QString &sourceId = QString()) const;

    /*
     * ========================================================
     * Atomic Folder / Atomic File
     * ========================================================
     */

    bool isAtomicFolder(
        const FileItem &item) const;

    bool isAtomicFile(
        const FileItem &item) const;

    bool isLocalAtomicFolder(
        const QString &path) const;

    /*
     * 获取某个 Atomic Folder 下的所有 Atomic File。
     */
    QStringList collectAtomicFolderFiles(
        const QString &atomicFolderPath) const;

    /*
     * 根据本地 Atomic File 找到所属 Atomic Folder。
     */
    QString atomicFolderForLocalFile(
        const QString &localFilePath) const;

    /*
     * ========================================================
     * Remote path mapping
     * ========================================================
     *
     * 这里是整个新架构最重要的部分。
     *
     * App hierarchy：
     *
     *     parentId
     *
     * 不直接转换成：
     *
     *     /zesbks/parent/child
     *
     * WebDAV 只根据 Atomic Folder / Atomic File 关系
     * 构建物理路径。
     */

    QString remoteAtomicFolderPathForItem(
        const FileItem &item) const;

    QString remoteAtomicFilePathForItem(
        const FileItem &item) const;

    /*
     * ========================================================
     * Upload
     * ========================================================
     */

    void enqueueAtomicFolderUpload(
        const FileItem &atomicFolder);

    void startUploads();

    void uploadNextFile();



    void scanLocalFiles();
    void trackOperation();


    /*
     * ========================================================
     * Remote scan
     * ========================================================
     */

    void startRemoteScan();

    void scanRemoteDirectory(
        const QString &remotePath);

    void processRemoteAtomicFolder(
        const QString &remotePath,
        const QVariantMap &entry);

    /*
     * ========================================================
     * Remote path utilities
     * ========================================================
     */

    QString normalizeRemotePath(
        const QString &path) const;

    QString remoteParentPath(
        const QString &remotePath) const;

    /*
     * 把某个远程文件需要的父目录加入创建队列。
     */
    void addParentDirectories(
        const QString &remotePath);

    /*
     * 为 Atomic Folder 加入必要目录。
     */
    void addAtomicFolderDirectories(
        const FileItem &atomicFolder);

    /*
     * ========================================================
     * Directory creation
     * ========================================================
     */

    void startDirectoryCreation();

    void createNextRemoteDirectory();

    /*
     * ========================================================
     * Remote deletion
     * ========================================================
     */

    void enqueueRemoteAtomicFolderDeletion(
        const QString &remoteAtomicFolderPath);

    void startRemoteDeletion();

    void deleteNextRemoteAtomicFolder();

    /*
     * ========================================================
     * Helpers
     * ========================================================
     */

    /*
     * 返回指定 Atomic Folder 的本地所有文件。
     */
    QStringList atomicFolderFiles(
        const FileItem &atomicFolder) const;

    /*
     * 把本地 Atomic Folder 转换成远程 Atomic Folder。
     */
    QString remoteAtomicFolderPathForLocalPath(
        const QString &localAtomicFolderPath) const;

    /*
     * 检查远程路径是否属于当前同步空间。
     */
    bool isRemotePathInCurrentSpace(
        const QString &remotePath) const;

private:

    /*
     * ========================================================
     * WebDAV client
     * ========================================================
     */

    WebDavClient *m_client = nullptr;

    /*
     * ========================================================
     * Auto sync
     * ========================================================
     */

    QTimer *m_timer = nullptr;

    /*
     * ========================================================
     * Active space
     * ========================================================
     */

    FileSpace m_activeSpace =
        FileSpace::MyDoc;

    /*
     * GroupUs 时表示具体的远程来源。
     *
     * MyDoc / GroupMe 通常为空。
     */
    QString m_activeSourceId;

    /*
     * ========================================================
     * Database
     * ========================================================
     */

    QString m_databasePath;

    /*
     * ========================================================
     * Sync state
     * ========================================================
     */

    bool m_syncInProgress = false;

    bool m_hasError = false;

    QString m_syncError;

    /*
     * 当前尚未完成的异步操作。
     */
    int m_pendingOps = 0;

    /*
     * 当前尚未完成的远程目录查询。
     */
    int m_pendingDirectoryListings = 0;

    /*
     * ========================================================
     * Remote state
     * ========================================================
     */

    /*
     * 已经发现的远程路径。
     */
    QSet<QString> m_remoteNames;

    /*
     * PROPFIND 得到的远程对象。
     */
    QVariantList m_remoteEntries;

    /*
     * 已经扫描过的目录。
     */
    QSet<QString> m_visitedDirs;

    /*
     * ========================================================
     * Remote directory creation queue
     * ========================================================
     */

    QStringList m_directoryQueue;

    int m_directoryQueueIndex = 0;

    /*
     * ========================================================
     * Atomic Folder queue
     * ========================================================
     */

    QStringList m_atomicFolderQueue;

    int m_atomicFolderQueueIndex = 0;

    QString m_currentAtomicFolder;

    /*
     * ========================================================
     * Upload queue
     * ========================================================
     *
     * <localPath, remotePath>
     */

    QList<QPair<QString, QString>> m_uploadQueue;

    int m_uploadQueueIndex = 0;

    /*
     * ========================================================
     * Download queue
     * ========================================================
     *
     * <remotePath, localPath>
     */

    QList<QPair<QString, QString>> m_downloadQueue;

    int m_downloadQueueIndex = 0;

    /*
     * ========================================================
     * Remote deletion queue
     * ========================================================
     */

    QStringList m_remoteDeleteQueue;

    int m_remoteDeleteQueueIndex = 0;

    bool m_remoteDeletionInProgress = false;

    bool m_remoteDeletionHasError = false;

    QString m_remoteDeletionError;
};

} // namespace litapp