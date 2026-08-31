#include "storage/MetadataStore.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace litapp {

MetadataStore::MetadataStore() = default;

MetadataStore::~MetadataStore()
{
    close();
}

bool MetadataStore::open(const QString &databasePath)
{
    if (m_db) {
        close();
    }

    m_db = new QSqlDatabase(QSqlDatabase::addDatabase(QStringLiteral("QSQLITE")));
    m_db->setDatabaseName(databasePath);
    if (!m_db->open()) {
        delete m_db;
        m_db = nullptr;
        return false;
    }

    return createTables();
}

void MetadataStore::close()
{
    if (m_db) {
        const QString connectionName = m_db->connectionName();
        m_db->close();
        delete m_db;
        m_db = nullptr;
        QSqlDatabase::removeDatabase(connectionName);
    }
}

bool MetadataStore::createTables()
{
    if (!m_db) {
        return false;
    }

    QSqlQuery query(*m_db);
    const bool filesTableOk = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS files ("
        "  id TEXT PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  local_path TEXT,"
        "  remote_path TEXT,"
        "  extension TEXT,"
        "  size INTEGER DEFAULT 0,"
        "  modified_time TEXT,"
        "  zone INTEGER NOT NULL,"
        "  category INTEGER NOT NULL,"
        "  sync_state INTEGER DEFAULT 0,"
        "  sync_error TEXT,"
        "  folder_name TEXT,"
        "  annotations_path TEXT"
        ")"));

    const bool tagsTableOk = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS tags ("
        "  file_id TEXT NOT NULL,"
        "  tag TEXT NOT NULL,"
        "  PRIMARY KEY (file_id, tag)"
        ")"));

    // 兼容旧数据库：补充新增列
    query.exec(QStringLiteral("ALTER TABLE files ADD COLUMN folder_name TEXT"));
    query.exec(QStringLiteral("ALTER TABLE files ADD COLUMN annotations_path TEXT"));

    return filesTableOk && tagsTableOk;
}

bool MetadataStore::addFile(const FileItem &item)
{
    if (!m_db) {
        return false;
    }

    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO files (id, name, local_path, remote_path, extension, size, modified_time, zone, category, sync_state, sync_error, folder_name, annotations_path) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(item.id);
    query.addBindValue(item.name);
    query.addBindValue(item.localPath);
    query.addBindValue(item.remotePath);
    query.addBindValue(item.extension);
    query.addBindValue(item.size);
    query.addBindValue(item.modifiedTime.toString(Qt::ISODate));
    query.addBindValue(static_cast<int>(item.zone));
    query.addBindValue(static_cast<int>(item.category));
    query.addBindValue(static_cast<int>(item.syncState));
    query.addBindValue(item.syncError);
    query.addBindValue(item.folderName);
    query.addBindValue(item.annotationsPath);

    if (!query.exec()) {
        return false;
    }

    QSqlQuery tagQuery(*m_db);
    tagQuery.prepare(QStringLiteral("INSERT INTO tags (file_id, tag) VALUES (?, ?)"));
    for (const QString &tag : item.tags) {
        tagQuery.addBindValue(item.id);
        tagQuery.addBindValue(tag);
        if (!tagQuery.exec()) {
            return false;
        }
    }
    return true;
}

bool MetadataStore::updateFile(const FileItem &item)
{
    if (!m_db) {
        return false;
    }

    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral(
        "UPDATE files SET name = ?, local_path = ?, remote_path = ?, extension = ?, size = ?, "
        "modified_time = ?, zone = ?, category = ?, sync_state = ?, sync_error = ?, folder_name = ?, annotations_path = ? WHERE id = ?"));
    query.addBindValue(item.name);
    query.addBindValue(item.localPath);
    query.addBindValue(item.remotePath);
    query.addBindValue(item.extension);
    query.addBindValue(item.size);
    query.addBindValue(item.modifiedTime.toString(Qt::ISODate));
    query.addBindValue(static_cast<int>(item.zone));
    query.addBindValue(static_cast<int>(item.category));
    query.addBindValue(static_cast<int>(item.syncState));
    query.addBindValue(item.syncError);
    query.addBindValue(item.folderName);
    query.addBindValue(item.annotationsPath);
    query.addBindValue(item.id);

    if (!query.exec()) {
        return false;
    }

    QSqlQuery deleteTags(*m_db);
    deleteTags.prepare(QStringLiteral("DELETE FROM tags WHERE file_id = ?"));
    deleteTags.addBindValue(item.id);
    if (!deleteTags.exec()) {
        return false;
    }

    QSqlQuery tagQuery(*m_db);
    tagQuery.prepare(QStringLiteral("INSERT INTO tags (file_id, tag) VALUES (?, ?)"));
    for (const QString &tag : item.tags) {
        tagQuery.addBindValue(item.id);
        tagQuery.addBindValue(tag);
        if (!tagQuery.exec()) {
            return false;
        }
    }
    return true;
}

bool MetadataStore::updateLocalPath(const QString &fileId, const QString &localPath)
{
    if (!m_db) {
        return false;
    }

    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral("UPDATE files SET local_path = ?, sync_state = ? WHERE id = ?"));
    query.addBindValue(localPath);
    query.addBindValue(static_cast<int>(SyncState::Synced));
    query.addBindValue(fileId);
    return query.exec();
}

bool MetadataStore::removeFile(const QString &fileId)
{
    if (!m_db) {
        return false;
    }

    QSqlQuery tags(*m_db);
    tags.prepare(QStringLiteral("DELETE FROM tags WHERE file_id = ?"));
    tags.addBindValue(fileId);
    if (!tags.exec()) {
        return false;
    }

    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral("DELETE FROM files WHERE id = ?"));
    query.addBindValue(fileId);
    return query.exec();
}

QVector<FileItem> MetadataStore::files(FileZone zone) const
{
    QVector<FileItem> result;
    if (!m_db) {
        return result;
    }

    QSqlQuery query(*m_db);
    query.prepare(QStringLiteral(
        "SELECT f.*, GROUP_CONCAT(t.tag, ',') AS tags FROM files f "
        "LEFT JOIN tags t ON t.file_id = f.id "
        "WHERE f.zone = ? "
        "GROUP BY f.id ORDER BY f.modified_time DESC"));
    query.addBindValue(static_cast<int>(zone));

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        FileItem item;
        item.id = query.value(QStringLiteral("id")).toString();
        item.name = query.value(QStringLiteral("name")).toString();
        item.localPath = query.value(QStringLiteral("local_path")).toString();
        item.remotePath = query.value(QStringLiteral("remote_path")).toString();
        item.extension = query.value(QStringLiteral("extension")).toString();
        item.size = query.value(QStringLiteral("size")).toLongLong();
        item.modifiedTime = QDateTime::fromString(query.value(QStringLiteral("modified_time")).toString(), Qt::ISODate);
        item.zone = static_cast<FileZone>(query.value(QStringLiteral("zone")).toInt());
        item.category = static_cast<FileCategory>(query.value(QStringLiteral("category")).toInt());
        item.syncState = static_cast<SyncState>(query.value(QStringLiteral("sync_state")).toInt());
        item.syncError = query.value(QStringLiteral("sync_error")).toString();
        item.folderName = query.value(QStringLiteral("folder_name")).toString();
        item.annotationsPath = query.value(QStringLiteral("annotations_path")).toString();

        const QString tags = query.value(QStringLiteral("tags")).toString();
        if (!tags.isEmpty()) {
            item.tags = tags.split(QLatin1Char(','));
        }
        result.append(item);
    }
    return result;
}

FileItem MetadataStore::fileById(const QString &fileId) const
{
    const auto allMy = files(FileZone::MyFiles);
    for (const FileItem &item : allMy) {
        if (item.id == fileId) {
            return item;
        }
    }
    const auto allGroup = files(FileZone::GroupFiles);
    for (const FileItem &item : allGroup) {
        if (item.id == fileId) {
            return item;
        }
    }
    return FileItem{};
}

QVector<FileItem> MetadataStore::filesByCategory(FileCategory category, FileZone zone) const
{
    QVector<FileItem> result;
    const auto all = files(zone);
    for (const FileItem &item : all) {
        if (item.category == category) {
            result.append(item);
        }
    }
    return result;
}

QVector<FileItem> MetadataStore::filesByTag(const QString &tag, FileZone zone) const
{
    QVector<FileItem> result;
    const auto all = files(zone);
    for (const FileItem &item : all) {
        if (item.tags.contains(tag)) {
            result.append(item);
        }
    }
    return result;
}

} // namespace litapp
