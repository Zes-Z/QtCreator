#pragma once

#include "models/FileItem.h"

#include <QDateTime>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

class QSqlDatabase;

namespace litapp {

/*
 * ============================================================
 * GroupSource
 * ============================================================
 *
 * 一个「他人群组」数据源。
 *
 * 对应 FileSpace::GroupUs 的一个来源：
 *
 *     用户加入了别人的群组，
 *     因此需要对方 WebDAV 账号的
 *     地址 / 账号 / 密码，
 *     且只读取对方的群组目录。
 *
 * 重要：
 *
 *     readOnly 恒为 true。
 *
 *     本应用永远不会向他人账号
 *     执行 PUT / MKCOL / DELETE。
 */
struct GroupSource {
    QString sourceId;
    QString displayName;
    QString serverUrl;
    QString username;
    QString password;

    /*
     * 对方账号上的群组根目录。
     *
     * 例如：
     *
     *     /zesbks/group/group me
     *     /zesbks/group/group me 2
     */
    QString remoteRoot;

    bool readOnly = true;

    QDateTime lastSync;

    bool isValid() const
    {
        return !sourceId.isEmpty()
            && !serverUrl.isEmpty();
    }
};

class MetadataStore
{
public:
    MetadataStore();
    ~MetadataStore();

    bool open(const QString &databasePath);
    void close();

    bool addFile(const FileItem &item);
    bool updateFile(const FileItem &item);
    bool updateLocalPath(
        const QString &fileId,
        const QString &localPath);

    bool removeFile(
        const QString &fileId);

    bool moveToTrash(
        const QString &fileId,
        const QString &trashLocalPath,
        const QString &originalLocalPath);

    bool restoreFromTrash(
        const QString &fileId,
        const QString &localPath);

    bool permanentlyDelete(
        const QString &fileId);

    QVector<FileItem> files(
        FileSpace space) const;

    QVector<FileItem> childrenOf(
        const QString &parentId,
        FileSpace space) const;

    QVector<FileItem> rootFiles(
        FileSpace space) const;

    QVector<FileItem> trashFiles(
        FileSpace space) const;

    bool isTrashed(
        const QString &fileId) const;

    QString originalLocalPath(
        const QString &fileId) const;

    FileItem fileById(
        const QString &fileId) const;

    QVector<FileItem> filesByCategory(
        FileCategory category,
        FileSpace space) const;

    QVector<FileItem> filesByTag(
        const QString &tag,
        FileSpace space) const;

    // ========================================================
    // 他人群组数据源（GroupUs）
    // ========================================================

    QVector<GroupSource> sources() const;

    GroupSource sourceById(
        const QString &sourceId) const;

    bool upsertSource(
        const GroupSource &source);

    bool removeSource(
        const QString &sourceId);

    // 更新某个数据源的最后同步时间。
    bool touchSourceSyncTime(
        const QString &sourceId);

    // ========================================================
    // 待删除的远端路径（墓碑）
    // ========================================================
    //
    // 本地删除或重命名一个远端对象之后，
    // 远端仍然残留旧路径。
    //
    // 如果不记录，下一次同步会把它当成
    // 「远端新增」重新拉回来。
    //
    // 因此这里留下墓碑，
    // 由 SyncService 在下一轮同步开头真正删除。
    // ========================================================

    bool addPendingDelete(
        const QString &remotePath,
        const QString &sourceId,
        bool isCollection = false);

    // 返回 (远端路径, 是否为集合) 列表。
    QVector<QPair<QString, bool>> pendingDeletes(
        const QString &sourceId) const;

    bool clearPendingDelete(
        const QString &remotePath);

private:
    bool createTables();

    QSqlDatabase *m_db = nullptr;
};

} // namespace litapp