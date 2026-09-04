#pragma once

#include "models/FileItem.h"

#include <QObject>
#include <QString>

namespace litapp {

class FileService : public QObject
{
    Q_OBJECT

public:
    explicit FileService(QObject *parent = nullptr);

    // ---------------------------------------------------------------------
    // 文件导入
    // ---------------------------------------------------------------------

    // 将本地文件导入指定的逻辑空间。
    //
    // 导入后：
    //
    //   AtomicFolder
    //       └── AtomicFile
    //
    // AtomicFolder 与 AtomicFile 的名称彼此独立。
    //
    // parentId:
    //   空字符串表示放到对应空间的根目录；
    //   非空表示放入指定的普通 Folder 中。
    //
    // space:
    //   FileSpace::MyDoc
    //   FileSpace::GroupMe
    //   FileSpace::GroupUs
    //
    // sourceId:
    //   GroupUs 用于标识具体的远程群组来源；
    //   MyDoc / GroupMe 可以为空。
    Q_INVOKABLE bool addLocalFile(
        const QString &filePath,
        int space,
        const QString &parentId = QString(),
        const QString &sourceId = QString(),
        int category = static_cast<int>(FileCategory::Other),
        const QString &tagsCsv = QString());

    // ---------------------------------------------------------------------
    // 文件 / 文件夹基本操作
    // ---------------------------------------------------------------------

    // 根据 ID 删除一个逻辑对象。
    //
    // 注意：
    // - 普通 Folder 删除时，需要由实现决定是否递归处理子树；
    // - AtomicFolder / AtomicFile 需要保持两者之间的数据一致性。
    Q_INVOKABLE bool removeFile(
        const QString &fileId);

    // 将已经下载的文件登记/更新到本地。
    Q_INVOKABLE bool markDownloaded(
        const QString &fileId,
        const QString &localPath);

    // 重命名一个对象。
    //
    // AtomicFolder 与 AtomicFile 的名称相互独立：
    // 重命名 AtomicFolder 不会修改 AtomicFile；
    // 重命名 AtomicFile 不会修改 AtomicFolder。
    Q_INVOKABLE bool renameItem(
        const QString &fileId,
        const QString &newName);

    // ---------------------------------------------------------------------
    // 回收站
    // ---------------------------------------------------------------------

    // 将对象移动到回收站。
    Q_INVOKABLE bool moveToTrash(
        const QString &fileId);

    // 从回收站恢复对象。
    Q_INVOKABLE bool restoreFromTrash(
        const QString &fileId);

    // 永久删除对象。
    //
    // 对于已经同步到 WebDAV 的 AtomicFolder，
    // 远程删除由 SyncService 负责。
    Q_INVOKABLE bool permanentlyDelete(
        const QString &fileId);

    // ---------------------------------------------------------------------
    // 复制
    // ---------------------------------------------------------------------

    // 复制对象到指定目标 Folder。
    //
    // Folder:
    //   复制整个子树。
    //
    // AtomicFolder:
    //   复制完整 AtomicFolder + AtomicFile。
    //
    // AtomicFile:
    //   只复制 AtomicFile 本身。
    //
    // destinationSpace:
    //   目标逻辑空间。
    //
    // destinationParentId:
    //   空字符串表示目标空间根目录。
    Q_INVOKABLE bool copyItem(
        const QString &fileId,
        int destinationSpace,
        const QString &destinationParentId = QString(),
        const QString &destinationSourceId = QString());

    // ---------------------------------------------------------------------
    // 移动
    // ---------------------------------------------------------------------

    // 移动一个对象到目标空间 / Folder。
    //
    // Folder：
    //   整个子树一起移动。
    //
    // AtomicFolder：
    //   AtomicFolder + AtomicFile 一起移动。
    //
    // AtomicFile：
    //   只移动 AtomicFile。
    //
    // 不会因为移动 AtomicFile 而自动移动其 AtomicFolder。
    Q_INVOKABLE bool moveItemTo(
        const QString &fileId,
        int destinationSpace,
        const QString &destinationParentId = QString(),
        const QString &destinationSourceId = QString());

    // ---------------------------------------------------------------------
    // 查询
    // ---------------------------------------------------------------------

    Q_INVOKABLE QVariantMap fileInfo(
        const QString &fileId) const;

    Q_INVOKABLE QVariantList children(
        const QString &parentId,
        int space,
        const QString &sourceId = QString()) const;

    Q_INVOKABLE QVariantList rootItems(
        int space,
        const QString &sourceId = QString()) const;

    Q_INVOKABLE QVariantList trashItems(
        int space,
        const QString &sourceId = QString()) const;

    // ---------------------------------------------------------------------
    // 辅助操作
    // ---------------------------------------------------------------------

    // 判断对象类型。
    Q_INVOKABLE bool isFolder(
        const QString &fileId) const;

    Q_INVOKABLE bool isAtomicFolder(
        const QString &fileId) const;

    Q_INVOKABLE bool isAtomicFile(
        const QString &fileId) const;

    // 返回指定对象所属的逻辑空间。
    Q_INVOKABLE int spaceOf(
        const QString &fileId) const;

    // 返回指定对象的 sourceId。
    Q_INVOKABLE QString sourceIdOf(
        const QString &fileId) const;

    // 设置 MetadataStore 使用的数据库路径。
    void setDatabasePath(
        const QString &databasePath);

    QString databasePath() const;

signals:

    // 数据发生变化，QML 可以据此刷新文件树。
    void fileAdded(
        const QString &fileId);

    void fileUpdated(
        const QString &fileId);

    void fileRemoved(
        const QString &fileId);

    void fileMoved(
        const QString &fileId,
        const QString &oldParentId,
        const QString &newParentId);

    void fileCopied(
        const QString &newFileId,
        const QString &sourceFileId);

    void fileRestored(
        const QString &fileId);

    void fileMovedToTrash(
        const QString &fileId);

private:

    // ---------------------------------------------------------------------
    // 数据库辅助
    // ---------------------------------------------------------------------

    bool loadItem(
        const QString &fileId,
        FileItem &item) const;

    bool saveItem(
        const FileItem &item);

    bool removeItemFromStore(
        const QString &fileId);

    // ---------------------------------------------------------------------
    // 名称 / 路径辅助
    // ---------------------------------------------------------------------

    QString uniqueName(
        const QString &name,
        const QString &parentId,
        FileSpace space,
        const QString &sourceId) const;

    QString uniqueCopyName(
        const QString &name,
        const QString &parentId,
        FileSpace space,
        const QString &sourceId) const;

    QString buildAtomicFolderLocalPath(
        const QString &baseDirectory,
        const QString &atomicFolderName) const;

    QString buildAtomicFileLocalPath(
        const QString &atomicFolderPath,
        const QString &atomicFileName) const;

    // ---------------------------------------------------------------------
    // 子树操作
    // ---------------------------------------------------------------------

    QVector<FileItem> descendants(
        const QString &parentId,
        FileSpace space,
        const QString &sourceId) const;

    bool moveDescendants(
        const QString &oldParentId,
        const QString &newParentId,
        FileSpace space,
        const QString &sourceId);

    bool copySubtree(
        const FileItem &source,
        const QString &destinationParentId,
        FileSpace destinationSpace,
        const QString &destinationSourceId,
        QString *newRootId = nullptr);

    // ---------------------------------------------------------------------
    // AtomicFolder / AtomicFile
    // ---------------------------------------------------------------------

    FileItem findAtomicFolderForFile(
        const FileItem &atomicFile) const;

    QVector<FileItem> atomicFilesForFolder(
        const FileItem &atomicFolder) const;

    bool createAtomicFolderForFile(
        const QString &filePath,
        const QString &parentId,
        FileSpace space,
        const QString &sourceId,
        int category,
        const QStringList &tags,
        FileItem *atomicFolder,
        FileItem *atomicFile);

    // ---------------------------------------------------------------------
    // 本地文件系统
    // ---------------------------------------------------------------------

    bool copyLocalFile(
        const QString &sourcePath,
        const QString &destinationPath) const;

    bool copyLocalDirectory(
        const QString &sourcePath,
        const QString &destinationPath) const;

    bool moveLocalPath(
        const QString &sourcePath,
        const QString &destinationPath) const;

    bool removeLocalPath(
        const QString &path) const;

    // ---------------------------------------------------------------------
    // 空间 / source 辅助
    // ---------------------------------------------------------------------

    bool isValidSpace(
        int space) const;

    FileSpace toFileSpace(
        int space) const;

    QString spaceRootName(
        FileSpace space) const;

private:

    QString m_databasePath = QStringLiteral("litapp_metadata.db");
};

} // namespace litapp