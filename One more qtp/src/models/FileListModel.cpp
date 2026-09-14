#include "models/FileListModel.h"

#include "models/FileCategory.h"
#include "storage/MetadataStore.h"

#include <QDateTime>
#include <QVariant>
#include <QVariantMap>

namespace litapp {

// ============================================================
// Constructor
// ============================================================

FileListModel::FileListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

// ============================================================
// rowCount
// ============================================================

int FileListModel::rowCount(
    const QModelIndex &parent) const
{
    /*
     * QAbstractListModel 是一维列表模型。
     *
     * 不处理 Qt 的层级 parent QModelIndex。
     *
     * 应用自己的层级关系由：
     *
     *     FileItem::parentId
     *
     * 表示。
     */
    if (parent.isValid()) {
        return 0;
    }

    return m_files.size();
}

// ============================================================
// data
// ============================================================

QVariant FileListModel::data(
    const QModelIndex &index,
    int role) const
{
    if (!index.isValid()) {
        return {};
    }

    const int row = index.row();

    if (row < 0 || row >= m_files.size()) {
        return {};
    }

    const FileItem &item = m_files.at(row);

    switch (role) {

        // --------------------------------------------------------
        // Identity
        // --------------------------------------------------------

    case IdRole:
        return item.id;

    case NameRole:
        return item.name;

        // --------------------------------------------------------
        // Paths
        // --------------------------------------------------------

    case LocalPathRole:
        return item.localPath;

    case RemotePathRole:
        return item.remotePath;

        // --------------------------------------------------------
        // Category
        // --------------------------------------------------------

    case CategoryRole:
        return static_cast<int>(item.category);

    case CategoryTextRole:
        return fileCategoryToString(item.category);

        // --------------------------------------------------------
        // Tags
        // --------------------------------------------------------

    case TagsRole:
        return item.tags;

        // --------------------------------------------------------
        // New hierarchy
        // --------------------------------------------------------

    case SpaceRole:
        return static_cast<int>(item.space);

    case ObjectTypeRole:
        return static_cast<int>(item.objectType);

    case ParentIdRole:
        return item.parentId;

    case SourceIdRole:
        return item.sourceId;

    case GroupIdRole:
        return item.groupId;

    case IsReadOnlyRole:
        return item.isReadOnly();

        // --------------------------------------------------------
        // Sync
        // --------------------------------------------------------

    case SyncStateRole:
        return static_cast<int>(item.syncState);

    case SyncErrorRole:
        return item.syncError;

        // --------------------------------------------------------
        // Other metadata
        // --------------------------------------------------------

    case AnnotationsPathRole:
        return item.annotationsPath;

    case ExtensionRole:
        return item.extension;

    case SizeRole:
        return item.size;

    case ModifiedTimeRole:
        return item.modifiedTime;

        // --------------------------------------------------------
        // Trash
        // --------------------------------------------------------

        /*
     * FileItem 本身目前没有直接暴露：
     *
     *     isTrashed
     *     originalLocalPath
     *
     * 这些信息由 MetadataStore 内部的回收站状态维护。
     *
     * 因此正常 FileItem 模型中暂时返回默认值。
     *
     * 真正的回收站模型应该使用：
     *
     *     MetadataStore::trashFiles()
     *
     * 单独加载。
     */

    case IsTrashedRole:
        return false;

    case OriginalLocalPathRole:
        return QString();

    default:
        return {};
    }
}

// ============================================================
// roleNames
// ============================================================

QHash<int, QByteArray> FileListModel::roleNames() const
{
    QHash<int, QByteArray> roles;

    roles.insert(
        IdRole,
        QByteArrayLiteral("fileId"));

    roles.insert(
        NameRole,
        QByteArrayLiteral("fileName"));

    roles.insert(
        LocalPathRole,
        QByteArrayLiteral("localPath"));

    roles.insert(
        RemotePathRole,
        QByteArrayLiteral("remotePath"));

    roles.insert(
        CategoryRole,
        QByteArrayLiteral("category"));

    roles.insert(
        CategoryTextRole,
        QByteArrayLiteral("categoryText"));

    roles.insert(
        TagsRole,
        QByteArrayLiteral("tags"));

    roles.insert(
        SpaceRole,
        QByteArrayLiteral("space"));

    roles.insert(
        ObjectTypeRole,
        QByteArrayLiteral("objectType"));

    roles.insert(
        ParentIdRole,
        QByteArrayLiteral("parentId"));

    roles.insert(
        SourceIdRole,
        QByteArrayLiteral("sourceId"));

    roles.insert(
        GroupIdRole,
        QByteArrayLiteral("groupId"));

    roles.insert(
        IsReadOnlyRole,
        QByteArrayLiteral("isReadOnly"));

    roles.insert(
        SyncStateRole,
        QByteArrayLiteral("syncState"));

    roles.insert(
        SyncErrorRole,
        QByteArrayLiteral("syncError"));

    roles.insert(
        AnnotationsPathRole,
        QByteArrayLiteral("annotationsPath"));

    roles.insert(
        ExtensionRole,
        QByteArrayLiteral("extension"));

    roles.insert(
        SizeRole,
        QByteArrayLiteral("size"));

    roles.insert(
        ModifiedTimeRole,
        QByteArrayLiteral("modifiedTime"));

    roles.insert(
        IsTrashedRole,
        QByteArrayLiteral("isTrashed"));

    roles.insert(
        OriginalLocalPathRole,
        QByteArrayLiteral("originalLocalPath"));

    return roles;
}

// ============================================================
// setFiles
// ============================================================

void FileListModel::setFiles(
    const QVector<FileItem> &files)
{
    beginResetModel();

    m_files = files;

    endResetModel();
}

// ============================================================
// clear
// ============================================================

void FileListModel::clear()
{
    beginResetModel();

    m_files.clear();

    endResetModel();
}

// ============================================================
// reloadFromStore
// ============================================================

void FileListModel::reloadFromStore(
    const QString &databasePath,
    int space)
{
    /*
     * 防止 QML 传入非法 Space。
     *
     * FileSpace 当前只有：
     *
     *     0 = MyDoc
     *     1 = GroupMe
     *     2 = GroupUs
     */
    if (space < static_cast<int>(FileSpace::MyDoc)
        || space > static_cast<int>(FileSpace::GroupUs)) {

        clear();

        return;
    }

    MetadataStore store;

    if (!store.open(databasePath)) {
        clear();
        return;
    }

    const FileSpace fileSpace =
        static_cast<FileSpace>(space);

    setFiles(
        store.files(fileSpace));
}

// ============================================================
// reloadAllFromStore
// ============================================================

void FileListModel::reloadAllFromStore(
    const QString &databasePath)
{
    MetadataStore store;

    if (!store.open(databasePath)) {
        clear();
        return;
    }

    QVector<FileItem> all;

    /*
     * 明确按照应用左侧三个逻辑空间排列。
     *
     * 注意：
     *
     * 这里的顺序只是 UI 数据顺序，
     * 并不代表 WebDAV 的物理目录结构。
     */
    const QVector<FileItem> myDoc =
        store.files(FileSpace::MyDoc);

    const QVector<FileItem> groupMe =
        store.files(FileSpace::GroupMe);

    const QVector<FileItem> groupUs =
        store.files(FileSpace::GroupUs);

    all.reserve(
        myDoc.size()
        + groupMe.size()
        + groupUs.size());

    all += myDoc;
    all += groupMe;
    all += groupUs;

    setFiles(all);
}

// ============================================================
// toVariantList
// ============================================================

QVariantList FileListModel::toVariantList() const
{
    QVariantList result;

    result.reserve(m_files.size());

    for (const FileItem &item : m_files) {

        QVariantMap map;

        // ----------------------------------------------------
        // Identity
        // ----------------------------------------------------

        map.insert(
            QStringLiteral("fileId"),
            item.id);

        map.insert(
            QStringLiteral("fileName"),
            item.name);

        // ----------------------------------------------------
        // Hierarchy
        // ----------------------------------------------------

        map.insert(
            QStringLiteral("parentId"),
            item.parentId);

        map.insert(
            QStringLiteral("space"),
            static_cast<int>(item.space));

        map.insert(
            QStringLiteral("objectType"),
            static_cast<int>(item.objectType));

        map.insert(
            QStringLiteral("sourceId"),
            item.sourceId);

        map.insert(
            QStringLiteral("groupId"),
            item.groupId);

        // ----------------------------------------------------
        // Paths
        // ----------------------------------------------------

        map.insert(
            QStringLiteral("localPath"),
            item.localPath);

        map.insert(
            QStringLiteral("remotePath"),
            item.remotePath);

        // ----------------------------------------------------
        // Metadata
        // ----------------------------------------------------

        map.insert(
            QStringLiteral("extension"),
            item.extension);

        map.insert(
            QStringLiteral("size"),
            item.size);

        map.insert(
            QStringLiteral("modifiedTime"),
            item.modifiedTime);

        map.insert(
            QStringLiteral("category"),
            static_cast<int>(item.category));

        map.insert(
            QStringLiteral("categoryText"),
            fileCategoryToString(item.category));

        map.insert(
            QStringLiteral("tags"),
            item.tags);

        map.insert(
            QStringLiteral("annotationsPath"),
            item.annotationsPath);

        // ----------------------------------------------------
        // Sync
        // ----------------------------------------------------

        map.insert(
            QStringLiteral("syncState"),
            static_cast<int>(item.syncState));

        map.insert(
            QStringLiteral("syncError"),
            item.syncError);

        // ----------------------------------------------------
        // Convenience flags for QML
        // ----------------------------------------------------

        map.insert(
            QStringLiteral("isCloud"),
            item.localPath.isEmpty());

        map.insert(
            QStringLiteral("isFolder"),
            item.isFolder());

        map.insert(
            QStringLiteral("isAtomicFolder"),
            item.isAtomicFolder());

        map.insert(
            QStringLiteral("isAtomicFile"),
            item.isAtomicFile());

        map.insert(
            QStringLiteral("isReadOnly"),
            item.isReadOnly());

        result.append(map);
    }

    return result;
}

} // namespace litapp