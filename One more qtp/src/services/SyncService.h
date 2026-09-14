#pragma once

#include "models/FileItem.h"
#include "storage/MetadataStore.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

namespace litapp {

class WebDavClient;

/*
 * ================================================================
 * SyncService
 * ================================================================
 *
 * 远端是「云仓库」，是原子对象的事实来源。
 *
 * 本服务的职责：
 *
 *   1. 保证远端骨架存在
 *          /zesbks/
 *          /zesbks/group/
 *          /zesbks/group/group me/
 *
 *   2. 远端 -> 应用内  发现（index）
 *          PROPFIND 远端原子文件夹，
 *          把结果物化成本地 SQLite 中的
 *          AtomicFolder / AtomicFile 条目。
 *
 *   3. 本地 -> 远端  上传
 *          仅对「我自己的账号」，
 *          且只上传本地确实存在的文件。
 *
 *   4. 按需下载
 *          双击远端条目时才 GET 到本地缓存。
 *
 *   5. 远端删除
 *          只允许删除「我自己的账号」下的
 *          一级 Atomic Folder。
 *
 * ================================================================
 * 重要：远端没有分类文件夹层级
 * ================================================================
 *
 * 应用内的子分类文件夹是纯逻辑概念，
 * 只存在于 SQLite 的 parent_id，
 * 绝不映射为远端目录。
 *
 * 因此「移动」不会产生任何 WebDAV 操作。
 *
 * ================================================================
 * 只读来源
 * ================================================================
 *
 * FileSpace::GroupUs 指向他人账号。
 *
 * 对本服务而言该 Job 是只读的：
 *
 *     只 PROPFIND
 *     永不 MKCOL / PUT / DELETE
 */
class SyncService : public QObject
{
    Q_OBJECT

public:
    explicit SyncService(
        WebDavClient *ownClient,
        QObject *parent = nullptr);

    ~SyncService() override;

    // ========================================================
    // Database
    // ========================================================

    void setDatabasePath(
        const QString &databasePath);

    QString databasePath() const;

    // ========================================================
    // 同步入口
    // ========================================================

    // 我的账号：my doc
    Q_INVOKABLE void syncMyDoc();

    // 我的账号：group me
    Q_INVOKABLE void syncGroupMe(
        const QString &groupId = QString());

    // 他人账号：group us（只读）
    Q_INVOKABLE void syncGroupUs(
        const QString &sourceId);

    // 我的账号：my doc + 全部 group me
    Q_INVOKABLE void syncMyAccount();

    // 全部空间
    Q_INVOKABLE void syncAll();

    // 兼容入口：
    //   space = 0 -> MyDoc
    //   space = 1 -> GroupMe
    //   space = 2 -> GroupUs
    Q_INVOKABLE void syncSpace(
        int space,
        const QString &sourceId = QString());

    Q_INVOKABLE bool isSyncing() const;

    // ========================================================
    // 自动同步
    // ========================================================

    Q_INVOKABLE void startAutoSync(
        int intervalMs = 180000);

    Q_INVOKABLE void stopAutoSync();

    // ========================================================
    // 远端骨架
    // ========================================================

    /*
     * 幂等创建：
     *
     *     /zesbks/
     *     /zesbks/group/
     *     /zesbks/group/group me/
     *
     * WebDAV 验证成功后由 UI 调用。
     */
    Q_INVOKABLE void ensureRemoteLayout();

    // ========================================================
    // 他人群组数据源（GroupUs）
    // ========================================================

    /*
     * 已配置的 group us 来源。
     *
     * 每项：
     *     { sourceId, displayName, serverUrl,
     *       username, password, remoteRoot, lastSync }
     */
    Q_INVOKABLE QVariantList groupSources() const;

    /*
     * 新增 / 更新一个只读群组来源。
     *
     * remoteRoot 为空时使用默认的
     * /zesbks/group/group me
     */
    Q_INVOKABLE bool saveGroupSource(
        const QString &sourceId,
        const QString &displayName,
        const QString &serverUrl,
        const QString &username,
        const QString &password,
        const QString &remoteRoot);

    Q_INVOKABLE bool removeGroupSource(
        const QString &sourceId);

    // ========================================================
    // 按需下载
    // ========================================================

    /*
     * 把远端 AtomicFile 下载到本地镜像缓存。
     *
     * 成功后 FileItem.localPath 会指向缓存文件。
     */
    Q_INVOKABLE bool downloadItem(
        const QString &fileId);

    /*
     * 本地文件被修改后回推远端。
     */
    Q_INVOKABLE bool uploadItem(
        const QString &fileId);

    // ========================================================
    // 远端删除
    // ========================================================

    /*
     * 删除一个远端 Atomic Folder。
     *
     * fileId 必须是：
     *
     *     space = MyDoc 或 GroupMe
     *     objectType = AtomicFolder
     *
     * GroupUs 永远是只读的。
     */
    Q_INVOKABLE void deleteRemoteAtomicFolder(
        const QString &fileId);

signals:

    void syncFinished(
        bool ok,
        const QString &message);

    // 远端骨架（zesbks/ 与 group 根）已就绪。
    void remoteLayoutReady();

    void remoteDeletionFinished(
        bool ok,
        const QString &message);

    void itemDownloaded(
        const QString &fileId,
        const QString &remotePath,
        const QString &localPath,
        bool ok,
        const QString &error);

    void libraryChanged();

    void statusMessage(
        const QString &message);

private slots:

    void onDirectoryListed(
        const QString &path,
        const QVariantList &entries,
        const QString &error);

    void onDirectoryCreated(
        const QString &remotePath,
        bool ok,
        const QString &error);

    void onUploadFinished(
        const QString &remotePath,
        bool ok,
        const QString &error);

    void onDownloadFinished(
        const QString &remotePath,
        const QString &localPath,
        bool ok,
        const QString &error);

    void onRemoveFinished(
        const QString &remotePath,
        bool ok,
        const QString &error);

    void onAutoSyncTimeout();

private:

    // ========================================================
    // 远端条目
    // ========================================================

    struct RemoteEntry {
        QString path;
        QString name;
        bool isDirectory = false;
        qint64 size = 0;
        QDateTime modified;
    };

    /*
     * 远端的一个原子文件夹。
     *
     *     /zesbks/paper/
     *         paper.pdf
     */
    struct RemoteFolder {
        QString name;
        QString remotePath;
        QVector<RemoteEntry> files;
    };

    // ========================================================
    // 一次同步任务
    // ========================================================

    struct Job {
        bool active = false;

        FileSpace space = FileSpace::MyDoc;

        // MyDoc 为空；GroupMe 为群组目录名
        QString groupId;

        // GroupUs 的数据源标识；其余为空
        QString sourceId;

        // 该 Job 对应的远端根
        QString remoteRoot;

        // GroupUs 恒为 true
        bool readOnly = false;

        WebDavClient *client = nullptr;

        bool hasError = false;
        QString error;

        // ---- 远端发现 ----
        int pendingListings = 0;
        QSet<QString> visited;
        QVector<RemoteFolder> folders;
        QHash<QString, int> folderIndexByPath;

        // ---- 目录创建 ----
        int directoryIndex = 0;
        QStringList directoryQueue;

        // ---- 上传 ----
        int uploadIndex = 0;
        QList<QPair<QString, QString>> uploadQueue;

        // ---- 远端根目录补建 ----
        bool bootstrappedRoot = false;
        bool bootstrappingRoot = false;

        // ---- 待删除的远端残留（墓碑） ----
        bool drainingDeletes = false;
        int pendingDeleteIndex = 0;
        QList<QPair<QString, bool>> pendingDeleteQueue;
    };

    // ========================================================
    // Job 生命周期
    // ========================================================

    struct PendingJob {
        FileSpace space = FileSpace::MyDoc;
        QString groupId;
        QString sourceId;
        bool readOnly = false;
    };

    bool startJob(
        FileSpace space,
        const QString &groupId,
        const QString &sourceId,
        bool readOnly);

    void enqueueJob(
        FileSpace space,
        const QString &groupId,
        const QString &sourceId,
        bool readOnly);

    void startNextQueuedJob();

    void finishJob(
        bool ok,
        const QString &message);

    // ========================================================
    // 远端发现
    // ========================================================

    void beginRemoteScan();

    void requestFolderListing(
        const QString &remotePath);

    void afterListingComplete();

    void continueAfterDeletes();

    void deleteNextPendingDelete();

    // ========================================================
    // 物化到 SQLite
    // ========================================================

    void reconcileWithStore();

    QVector<FileItem> storeChildren(
        const QString &parentId,
        FileSpace space,
        const QString &sourceId,
        const QString &groupId) const;

    // ========================================================
    // 上传
    // ========================================================

    void prepareUploads();

    void startDirectoryCreation();

    void createNextDirectory();

    void startUploads();

    void uploadNextFile();

    // ========================================================
    // 删除
    // ========================================================

    void deleteNextRemoteFolder();

    // ========================================================
    // 辅助
    // ========================================================

    WebDavClient *clientForSource(
        const QString &sourceId);

    bool isJobClient() const;

    QString jobLabel() const;

    // 群组目录的远端根（自动适配两种情况）
    QString groupRootFor(
        const QString &groupId) const;

    bool openStore(
        MetadataStore &store) const;

    QVector<FileItem> allItemsOfJob() const;

    FileItem findAtomicFolderByName(
        const QString &name) const;

    static QVector<RemoteEntry> toRemoteEntries(
        const QVariantList &entries);

private:

    WebDavClient *m_ownClient = nullptr;

    QHash<QString, WebDavClient *> m_sourceClients;

    QTimer *m_timer = nullptr;

    QString m_databasePath =
        QStringLiteral("litapp_metadata.db");

    Job m_job;

    // 顺序执行的待办任务队列
    QList<PendingJob> m_pendingJobs;

    /*
     * 最近一次扫描 /zesbks 时看到的子目录名。
     *
     * 用来判断远端群组目录的真实布局：
     *
     *     zesbks/group/group me/
     *     zesbks/group me/
     *
     * 两种都能自动适配。
     */
    QSet<QString> m_shareRootChildren;

    // ---- 按需下载 ----
    QHash<QString, QString> m_pendingDownloadIds;

    // ---- 远端删除 ----
    bool m_deleting = false;
    int m_deleteIndex = 0;
    QStringList m_deleteQueue;
    QHash<QString, QString> m_deleteFolderIds;
    bool m_deleteHasError = false;
    QString m_deleteError;

    // ---- 骨架创建 ----
    bool m_layoutCreationInProgress = false;
    int m_layoutIndex = 0;
    QStringList m_layoutQueue;
};

} // namespace litapp
