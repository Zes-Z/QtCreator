#pragma once

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
 * WebDAV 同步模型：
 *
 *     /zesbks/
 *          │
 *          ├── AtomicFolder1/
 *          │      ├── source.pdf
 *          │      ├── notes.json
 *          │      └── annotations.json
 *          │
 *          ├── AtomicFolder2/
 *          │      ├── source.pdf
 *          │      └── notes.json
 *          │
 *          └── AtomicFolder3/
 *                 └── source.pdf
 *
 * 每一个 zesbks 一级子目录都是一个 Atomic Folder。
 *
 * Atomic Folder 是一个原子文件及其全部附属文件的最小
 * 同步单位。
 *
 * 分类、子分类、标签等应用内部逻辑不参与 WebDAV 目录结构。
 *
 * WebDAV 只负责：
 *
 *     zesbks/
 *         ├── AtomicFolder1/
 *         ├── AtomicFolder2/
 *         └── AtomicFolder3/
 *
 * 以及每个 Atomic Folder 内部文件的完整同步。
 */
class SyncService : public QObject
{
    Q_OBJECT

public:

    explicit SyncService(
        WebDavClient *client,
        QObject *parent = nullptr);


    /*
     * 同步指定 zone。
     *
     * zone 的具体业务含义仍由应用原有逻辑决定。
     * SyncService 不负责理解分类、子分类等业务概念。
     */
    Q_INVOKABLE void syncZone(int zone);


    /*
     * 开启自动同步。
     */
    Q_INVOKABLE void startAutoSync(
        int intervalMs = 180000);


    /*
     * 停止自动同步。
     */
    Q_INVOKABLE void stopAutoSync();


    /*
     * 设置当前活动 zone。
     */
    Q_INVOKABLE void setActiveZone(int zone);


signals:

    void syncFinished(
        bool ok,
        const QString &message);


private slots:

    /*
     * 自动同步定时器。
     */
    void autoSync();


    /*
     * WebDAV PROPFIND 返回目录内容。
     *
     * path:
     *     当前查询的远程目录
     *
     * entries:
     *     当前目录中的文件 / 目录
     *
     * error:
     *     错误信息
     */
    void handleDirectoryListed(
        const QString &path,
        const QVariantList &entries,
        const QString &error);


    /*
     * WebDAV 创建目录完成。
     */
    void handleDirectoryCreated(
        const QString &remotePath,
        bool ok,
        const QString &error);


    /*
     * 单个文件上传完成。
     */
    void handleUploadFinished(
        const QString &remotePath,
        bool ok,
        const QString &error);


    /*
     * 文件下载完成。
     *
     * 当前同步策略仍然保留这个接口，
     * 后续可用于远程文件 → 本地 Atomic Folder。
     */
    void handleDownloadFinished(
        const QString &remotePath,
        const QString &localPath,
        bool ok,
        const QString &error);


private:

    /*
     * ========================================================
     * 同步流程
     * ========================================================
     */

    /*
     * 结束本次同步。
     */
    void finish(
        bool ok,
        const QString &message);


    /*
     * 更新当前异步操作状态。
     *
     * 保留原有接口，供同步状态机使用。
     */
    void trackOperation();


    /*
     * --------------------------------------------------------
     * 第一阶段：准备远程目录
     * --------------------------------------------------------
     */

    /*
     * 开始处理远程目录创建队列。
     */
    void startDirectoryCreation();


    /*
     * 创建下一个远程目录。
     */
    void createNextRemoteDirectory();


    /*
     * --------------------------------------------------------
     * 第二阶段：上传 Atomic Folder
     * --------------------------------------------------------
     */

    /*
     * 所有需要的远程目录准备完成以后，
     * 开始处理 Atomic Folder 上传。
     */
    void startUploads();


    /*
     * 上传队列中的下一个文件。
     *
     * 注意：
     *
     * 上传队列中的文件属于当前正在同步的
     * Atomic Folder。
     */
    void uploadNextFile();


    /*
     * --------------------------------------------------------
     * 本地文件扫描
     * --------------------------------------------------------
     */

    /*
     * 扫描本地 zesbks。
     *
     * 扫描的基本单位不是普通文件，
     * 而是 zesbks 下的 Atomic Folder。
     */
    void scanLocalFiles();


    /*
     * 获取当前 zone 的本地根目录。
     */
    QString localZoneRoot(
        int zone) const;


    /*
     * 获取当前 zone 的 WebDAV 根目录。
     *
     * 例如：
     *
     *     /zesbks
     */
    QString remoteZoneRoot(
        int zone) const;


    /*
     * ========================================================
     * Atomic Folder
     * ========================================================
     */

    /*
     * 判断一个本地目录是否为 Atomic Folder。
     *
     * Atomic Folder 的定义：
     *
     *     zesbks/
     *         └── 某个一级目录/
     *
     * 该目录代表一个原子文件及其全部附件。
     */
    bool isAtomicFolder(
        const QString &localPath) const;


    /*
     * 获取 zesbks 下所有 Atomic Folder。
     *
     * 这里只识别 zesbks 的一级目录。
     *
     * 不把 Atomic Folder 内部的子目录当成新的
     * Atomic Folder。
     */
    QStringList findAtomicFolders(
        const QString &zoneRoot) const;


    /*
     * 递归获取一个 Atomic Folder 中的所有普通文件。
     *
     * 例如：
     *
     *     文件1/
     *         文件1.pdf
     *         notes.json
     *         annotations/
     *             page1.json
     *
     * 会得到三个文件。
     */
    QStringList collectAtomicFolderFiles(
        const QString &atomicFolder) const;


    /*
     * 将本地 Atomic Folder 中的文件路径转换为
     * WebDAV 路径。
     *
     * 例如：
     *
     *     本地：
     *     C:/.../zesbks/文件1/文件1.pdf
     *
     *     远程：
     *     /zesbks/文件1/文件1.pdf
     */
    QString remotePathForLocalPath(
        const QString &localPath,
        int zone) const;


    /*
     * 获取一个本地文件所属的 Atomic Folder。
     *
     * 例如：
     *
     *     .../zesbks/文件1/notes.json
     *
     * 返回：
     *
     *     .../zesbks/文件1
     */
    QString atomicFolderForLocalFile(
        const QString &localPath,
        int zone) const;


    /*
     * 获取 Atomic Folder 的远程路径。
     *
     * 例如：
     *
     *     /zesbks/文件1
     */
    QString remoteAtomicFolderPath(
        const QString &atomicFolder,
        int zone) const;


    /*
     * ========================================================
     * Remote Path utilities
     * ========================================================
     */

    /*
     * 标准化 WebDAV 路径。
     *
     * 保证：
     *
     *     /zesbks/文件1
     *
     * 而不是：
     *
     *     //zesbks//文件1/
     */
    QString normalizeRemotePath(
        const QString &path) const;


    /*
     * 获取 remotePath 的父目录。
     */
    QString remoteParentPath(
        const QString &remotePath) const;


    /*
     * 为某个 remotePath 把所有父目录加入创建队列。
     *
     * 例如：
     *
     *     /zesbks/文件1/attachments/a.pdf
     *
     * 会确保队列包含：
     *
     *     /zesbks
     *     /zesbks/文件1
     *     /zesbks/文件1/attachments
     */
    void addParentDirectories(
        const QString &remotePath);


    /*
     * 为一个 Atomic Folder 建立所需要的全部远程目录。
     */
    void addAtomicFolderDirectories(
        const QString &atomicFolder,
        int zone);


private:

    /*
     * ========================================================
     * WebDAV
     * ========================================================
     */

    WebDavClient *m_client = nullptr;


    /*
     * 自动同步定时器。
     */
    QTimer *m_timer = nullptr;


    /*
     * 当前 zone。
     */
    int m_zone = 0;


    /*
     * 当前正在等待完成的异步操作数量。
     */
    int m_pendingOps = 0;


    /*
     * 当前正在等待完成的远程目录查询数量。
     */
    int m_pendingDirectoryListings = 0;


    /*
     * 当前同步过程中是否发生错误。
     */
    bool m_hasError = false;


    /*
     * 是否正在同步。
     *
     * 防止自动同步定时器在一次同步尚未结束时
     * 再次启动新的同步任务。
     */
    bool m_syncInProgress = false;


    /*
     * ========================================================
     * Remote state
     * ========================================================
     */

    /*
     * 远程文件名集合。
     *
     * 保留这个成员以兼容现有代码。
     *
     * 新同步逻辑不再仅依靠文件名判断文件是否存在。
     */
    QSet<QString> m_remoteNames;


    /*
     * 当前已经获取到的远程目录内容。
     */
    QVariantList m_remoteEntries;


    /*
     * 已经访问过的远程目录。
     *
     * 用于避免递归 PROPFIND 时重复访问。
     */
    QSet<QString> m_visitedDirs;


    /*
     * ========================================================
     * Remote directory creation queue
     * ========================================================
     */

    /*
     * 待创建的远程目录。
     *
     * Atomic Folder 的所有父目录都会先进入这里。
     */
    QStringList m_directoryQueue;


    /*
     * 当前创建到哪个目录。
     */
    int m_directoryQueueIndex = 0;


    /*
     * ========================================================
     * Atomic Folder queue
     * ========================================================
     */

    /*
     * 当前 zone 中需要处理的 Atomic Folder。
     *
     * 例如：
     *
     *     /local/zesbks/
     *         ├── 文件1
     *         ├── 文件2
     *         └── 文件3
     *
     * 队列：
     *
     *     文件1
     *     文件2
     *     文件3
     */
    QStringList m_atomicFolderQueue;


    /*
     * 当前处理到第几个 Atomic Folder。
     */
    int m_atomicFolderQueueIndex = 0;


    /*
     * 当前正在处理的 Atomic Folder。
     */
    QString m_currentAtomicFolder;


    /*
     * ========================================================
     * File upload queue
     * ========================================================
     *
     * 这里仍然以“文件”为 WebDAV 的实际上传操作，
     * 但这些文件属于同一个 Atomic Folder。
     *
     * 因此：
     *
     *     Atomic Folder
     *          ↓
     *     多个 upload operation
     *
     * 而不是：
     *
     *     整个应用
     *          ↓
     *     一堆互不相关的文件
     */
    QList<QPair<QString, QString>> m_uploadQueue;


    /*
     * 当前 Atomic Folder 中上传到第几个文件。
     */
    int m_uploadQueueIndex = 0;


    /*
     * ========================================================
     * Database
     * ========================================================
     */

    /*
     * 保留现有 metadata database 路径。
     */
    QString m_databasePath =
        QStringLiteral("litapp_metadata.db");
};

} // namespace litapp