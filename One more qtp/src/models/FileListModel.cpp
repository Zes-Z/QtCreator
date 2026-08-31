#include "models/FileListModel.h"

#include "models/FileCategory.h"
#include "storage/MetadataStore.h"

namespace litapp {

FileListModel::FileListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FileListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_files.size();
}

QVariant FileListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_files.size()) {
        return {};
    }

    const FileItem &item = m_files.at(index.row());
    switch (role) {
    case IdRole:
        return item.id;
    case NameRole:
        return item.name;
    case LocalPathRole:
        return item.localPath;
    case RemotePathRole:
        return item.remotePath;
    case CategoryRole:
        return static_cast<int>(item.category);
    case CategoryTextRole:
        return fileCategoryToString(item.category);
    case TagsRole:
        return item.tags;
    case ZoneRole:
        return static_cast<int>(item.zone);
    case SyncStateRole:
        return static_cast<int>(item.syncState);
    case FolderNameRole:
        return item.folderName;
    case AnnotationsPathRole:
        return item.annotationsPath;
    default:
        return {};
    }
}

QHash<int, QByteArray> FileListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles.insert(IdRole, "fileId");
    roles.insert(NameRole, "fileName");
    roles.insert(LocalPathRole, "localPath");
    roles.insert(RemotePathRole, "remotePath");
    roles.insert(CategoryRole, "category");
    roles.insert(CategoryTextRole, "categoryText");
    roles.insert(TagsRole, "tags");
    roles.insert(ZoneRole, "zone");
    roles.insert(SyncStateRole, "syncState");
    roles.insert(FolderNameRole, "folderName");
    roles.insert(AnnotationsPathRole, "annotationsPath");
    return roles;
}

void FileListModel::setFiles(const QVector<FileItem> &files)
{
    beginResetModel();
    m_files = files;
    endResetModel();
}

void FileListModel::clear()
{
    beginResetModel();
    m_files.clear();
    endResetModel();
}

void FileListModel::reloadFromStore(const QString &databasePath, int zone)
{
    MetadataStore store;
    if (!store.open(databasePath)) {
        clear();
        return;
    }
    setFiles(store.files(static_cast<FileZone>(zone)));
}

void FileListModel::reloadAllFromStore(const QString &databasePath)
{
    MetadataStore store;
    if (!store.open(databasePath)) {
        clear();
        return;
    }
    QVector<FileItem> all;
    all += store.files(FileZone::MyFiles);
    all += store.files(FileZone::GroupFiles);
    setFiles(all);
}

QVariantList FileListModel::toVariantList() const
{
    QVariantList result;
    for (const FileItem &item : m_files) {
        QVariantMap map;
        map.insert(QStringLiteral("fileId"), item.id);
        map.insert(QStringLiteral("fileName"), item.name);
        map.insert(QStringLiteral("localPath"), item.localPath);
        map.insert(QStringLiteral("remotePath"), item.remotePath);
        map.insert(QStringLiteral("folderName"), item.folderName);
        map.insert(QStringLiteral("categoryText"), fileCategoryToString(item.category));
        map.insert(QStringLiteral("annotationsPath"), item.annotationsPath);
        map.insert(QStringLiteral("isCloud"), item.localPath.isEmpty());
        map.insert(QStringLiteral("zone"), static_cast<int>(item.zone));
        result.append(map);
    }
    return result;
}

} // namespace litapp
