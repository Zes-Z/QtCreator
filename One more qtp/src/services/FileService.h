#pragma once

#include "models/FileItem.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace litapp {

/*
 * ================================================================
 * FileService
 * ================================================================
 *
 * 负责应用内「逻辑文件系统」。
 *
 * ================================================================
 * 两条必须记住的规则
 * ================================================================
 *
 * 规则一：远端是扁平的
 *
 *     远端只有：
 *
 *         <空间根>/原子文件夹/原子文件
 *
 *     原子文件夹之间永远平级。
 *
 *     本地缓存镜像远端结构：
 *
 *         <cache>/zesbks/...                （我的账号）
 *         <cache>/sources/<id>/zesbks/...   （他人账号）
 *
 *     因此原子文件夹 / 原子文件的本地路径
 *     完全由「远端根 + 名称」决定，
 *     与它在应用树中的位置无关。
 *
 * 规则二：子分类文件夹是纯逻辑的
 *
 *     普通 Folder 只存在于 SQLite 的 parent_id。
 *
 *     它：
 *
 *         没有本地目录
 *         没有远端目录
 *
 *     移动它不会产生任何文件系统或 WebDAV 操作。
 */
class FileService : public QObject
{
    Q_OBJECT

public:
    explicit FileService(QObject *parent = nullptr);

    // ========================================================
    // Database
    // ========================================================

    void setDatabasePath(
        const QString &databasePath);

    QString databasePath() const;

    // ========================================================
    // 导入
    // ========================================================

    /*
     * 导入一个本地文件。
     *
     * 结果：
     *
     *     AtomicFolder
     *         └── AtomicFile
     *
     * 物理文件被复制到本地镜像缓存，
     * 并标记为 LocalOnly，
     * 下一次同步会上传到远端。
     */
    Q_INVOKABLE bool addLocalFile(
        const QString &filePath,
        int space,
        const QString &parentId = QString(),
        const QString &sourceId = QString(),
        const QString &groupId = QString(),
        int category = static_cast<int>(FileCategory::Other),
        const QString &tagsCsv = QString());

    // ========================================================
    // 纯逻辑文件夹
    // ========================================================

    Q_INVOKABLE bool createFolder(
        const QString &name,
        int space,
        const QString &parentId = QString(),
        const QString &sourceId = QString(),
        const QString &groupId = QString());

    // ========================================================
    // 基本操作
    // ========================================================

    /*
     * 重命名。
     *
     * AtomicFolder：
     *     重命名本地镜像目录，
     *     远端路径随之改变，
     *     标记为 Uploading 等待同步。
     *
     * AtomicFile：
     *     重命名镜像中的文件。
     *
     * Folder：
     *     只改名称。
     */
    Q_INVOKABLE bool renameItem(
        const QString &fileId,
        const QString &newName);

    /*
     * 移动一个对象到目标文件夹。
     *
     * 只允许在同一空间内移动。
     *
     * 语义：
     *
     *     只修改 parentId
     *
     *     向下级联（子树通过 parentId 隐式跟随）
     *     向上不影响父节点
     *
     * 不产生任何文件系统或 WebDAV 操作。
     */
    Q_INVOKABLE bool moveItemTo(
        const QString &fileId,
        const QString &destinationParentId);

    /*
     * 复制一个对象到目标文件夹。
     */
    Q_INVOKABLE bool copyItem(
        const QString &fileId,
        const QString &destinationParentId);

    // ========================================================
    // 回收站
    // ========================================================

    Q_INVOKABLE bool removeFile(
        const QString &fileId);

    Q_INVOKABLE bool moveToTrash(
        const QString &fileId);

    Q_INVOKABLE bool restoreFromTrash(
        const QString &fileId);

    Q_INVOKABLE bool permanentlyDelete(
        const QString &fileId);

    // ========================================================
    // 查询
    // ========================================================

    Q_INVOKABLE QVariantMap fileInfo(
        const QString &fileId) const;

    Q_INVOKABLE QVariantList children(
        const QString &parentId,
        int space,
        const QString &sourceId = QString(),
        const QString &groupId = QString()) const;

    Q_INVOKABLE QVariantList rootItems(
        int space,
        const QString &sourceId = QString(),
        const QString &groupId = QString()) const;

    Q_INVOKABLE QVariantList trashItems(
        int space,
        const QString &sourceId = QString(),
        const QString &groupId = QString()) const;

    /*
     * 可选目标文件夹，用于「移动」对话框。
     *
     * 返回扁平列表，每项：
     *
     *     { fileId, name, depth, path }
     */
    Q_INVOKABLE QVariantList folderChoices(
        int space,
        const QString &sourceId = QString(),
        const QString &groupId = QString(),
        const QString &excludeSubtreeOf = QString()) const;

    /*
     * 某个对象所属空间的远端根。
     */
    Q_INVOKABLE QString remoteRootOf(
        const QString &fileId) const;

    Q_INVOKABLE bool isFolder(
        const QString &fileId) const;

    Q_INVOKABLE bool isAtomicFolder(
        const QString &fileId) const;

    Q_INVOKABLE bool isAtomicFile(
        const QString &fileId) const;

    Q_INVOKABLE int spaceOf(
        const QString &fileId) const;

    Q_INVOKABLE QString sourceIdOf(
        const QString &fileId) const;

    Q_INVOKABLE QString groupIdOf(
        const QString &fileId) const;

    // ========================================================
    // 本地镜像
    // ========================================================

    /*
     * 某个 AtomicFile 在本地镜像中的目标路径。
     *
     * 无论是否已经下载，都会返回确定路径。
     */
    Q_INVOKABLE QString localMirrorPathOf(
        const QString &fileId) const;

    /*
     * 某个对象当前是否已经存在于本地。
     */
    Q_INVOKABLE bool existsLocally(
        const QString &fileId) const;

signals:

    void fileAdded(const QString &fileId);
    void fileUpdated(const QString &fileId);
    void fileRemoved(const QString &fileId);

    void fileMoved(
        const QString &fileId,
        const QString &oldParentId,
        const QString &newParentId);

    void fileCopied(
        const QString &newFileId,
        const QString &sourceFileId);

    void fileRestored(const QString &fileId);

    void fileMovedToTrash(const QString &fileId);

private:

    // ---- 数据库 ----

    bool openStore(
        class MetadataStore &store) const;

    bool loadItem(
        const QString &fileId,
        FileItem &item) const;

    bool saveItem(
        const FileItem &item);

    QVector<FileItem> descendants(
        const QString &parentId,
        FileSpace space,
        const QString &sourceId,
        const QString &groupId) const;

    // ---- 远端根 ----

    QString remoteRootFor(
        FileSpace space,
        const QString &groupId) const;

    QString remoteFolderPath(
        FileSpace space,
        const QString &groupId,
        const QString &atomicFolderName) const;

    QString remoteFilePath(
        FileSpace space,
        const QString &groupId,
        const QString &atomicFolderName,
        const QString &atomicFileName) const;

    // ---- 本地镜像 ----

    QString mirrorFolderPath(
        const QString &remoteFolderPath,
        const QString &sourceId) const;

    QString mirrorFilePath(
        const QString &remoteFolderPath,
        const QString &sourceId,
        const QString &fileName) const;

    // ---- 名称 ----

    QString uniqueName(
        const QString &name,
        const QString &parentId,
        FileSpace space,
        const QString &sourceId,
        const QString &groupId) const;

    QString uniqueCopyName(
        const QString &name,
        const QString &parentId,
        FileSpace space,
        const QString &sourceId,
        const QString &groupId) const;

    // ---- 辅助 ----

    bool isValidSpace(int space) const;

    FileSpace toFileSpace(int space) const;

    bool isDescendantOf(
        const QString &candidateId,
        const QString &ancestorId) const;

    bool copyLocalFile(
        const QString &sourcePath,
        const QString &destinationPath) const;

    bool removeLocalPath(
        const QString &path) const;

    bool copyItemRecursive(
        const FileItem &source,
        const QString &destinationParentId,
        QString *newRootId);

private:

    QString m_databasePath =
        QStringLiteral("litapp_metadata.db");
};

} // namespace litapp
