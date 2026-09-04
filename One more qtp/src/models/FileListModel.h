#pragma once

#include "models/FileItem.h"

#include <QAbstractListModel>
#include <QHash>
#include <QVariant>
#include <QVector>

namespace litapp {

/*
 * FileListModel
 *
 * 将 MetadataStore 中的 FileItem 暴露给 QML。
 *
 * 新架构：
 *
 *   FileSpace
 *     ├── MyDoc
 *     ├── GroupMe
 *     └── GroupUs
 *
 *   FileObjectType
 *     ├── Folder
 *     ├── AtomicFolder
 *     └── AtomicFile
 *
 * 注意：
 *
 *   这里仅负责：
 *       MetadataStore <-> QML
 *
 *   不负责：
 *       WebDAV 路径计算
 *       本地文件复制
 *       Atomic Folder 创建
 *       同步
 *
 *   这些职责分别由：
 *       FileService
 *       SyncService
 *
 *   负责。
 */
class FileListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,

        NameRole,

        LocalPathRole,
        RemotePathRole,

        CategoryRole,
        CategoryTextRole,

        TagsRole,

        SpaceRole,
        ObjectTypeRole,

        ParentIdRole,
        SourceIdRole,

        SyncStateRole,
        SyncErrorRole,

        AnnotationsPathRole,

        ExtensionRole,
        SizeRole,
        ModifiedTimeRole,

        IsTrashedRole,
        OriginalLocalPathRole
    };

    Q_ENUM(Roles)

    explicit FileListModel(QObject *parent = nullptr);

    int rowCount(
        const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(
        const QModelIndex &index,
        int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    /*
     * 直接设置模型数据。
     */
    void setFiles(const QVector<FileItem> &files);

    /*
     * 清空模型。
     */
    Q_INVOKABLE void clear();

    /*
     * 从指定 FileSpace 加载文件。
     *
     * space:
     *   0 = MyDoc
     *   1 = GroupMe
     *   2 = GroupUs
     */
    Q_INVOKABLE void reloadFromStore(
        const QString &databasePath,
        int space);

    /*
     * 加载全部三个 Space。
     *
     * 顺序：
     *   MyDoc
     *   GroupMe
     *   GroupUs
     */
    Q_INVOKABLE void reloadAllFromStore(
        const QString &databasePath);

    /*
     * 返回 QML 方便处理的 QVariantList。
     */
    Q_INVOKABLE QVariantList toVariantList() const;

private:
    QVector<FileItem> m_files;
};

} // namespace litapp