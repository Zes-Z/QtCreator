#include "storage/MetadataStore.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace litapp {

// ============================================================
// Constructor / Destructor
// ============================================================

MetadataStore::MetadataStore() = default;

MetadataStore::~MetadataStore()
{
    close();
}

// ============================================================
// Database
// ============================================================

bool MetadataStore::open(const QString &databasePath)
{
    if (m_db) {
        close();
    }

    const QString connectionName =
        QStringLiteral("LitAppMetadata_%1")
            .arg(reinterpret_cast<quintptr>(this));

    m_db = new QSqlDatabase(
        QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName));

    m_db->setDatabaseName(databasePath);

    if (!m_db->open()) {
        delete m_db;
        m_db = nullptr;

        QSqlDatabase::removeDatabase(connectionName);
        return false;
    }

    return createTables();
}

void MetadataStore::close()
{
    if (!m_db) {
        return;
    }

    const QString connectionName = m_db->connectionName();

    m_db->close();

    delete m_db;
    m_db = nullptr;

    QSqlDatabase::removeDatabase(connectionName);
}

// ============================================================
// Table creation / migration
// ============================================================

bool MetadataStore::createTables()
{
    if (!m_db) {
        return false;
    }

    QSqlQuery query(*m_db);

    // ========================================================
    // files
    // ========================================================
    //
    // 当前模型：
    //
    // id
    // parent_id
    // name
    // object_type
    // space
    // source_id
    // local_path
    // remote_path
    // extension
    // size
    // modified_time
    // category
    // sync_state
    // sync_error
    // annotations_path
    // is_trashed
    // original_local_path
    //
    // 注意：
    //
    // App 内部的 parent_id / space
    // 不直接决定 WebDAV 的物理目录结构。
    //
    // WebDAV 映射由 SyncService 负责。
    // ========================================================

    const bool filesTableOk = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS files ("
        "  id TEXT PRIMARY KEY,"
        "  parent_id TEXT,"
        "  name TEXT NOT NULL,"
        "  object_type INTEGER NOT NULL DEFAULT 2,"
        "  space INTEGER NOT NULL DEFAULT 0,"
        "  source_id TEXT,"
        "  group_id TEXT,"
        "  local_path TEXT,"
        "  remote_path TEXT,"
        "  extension TEXT,"
        "  size INTEGER DEFAULT 0,"
        "  modified_time TEXT,"
        "  category INTEGER NOT NULL DEFAULT 0,"
        "  sync_state INTEGER DEFAULT 0,"
        "  sync_error TEXT,"
        "  annotations_path TEXT,"
        "  is_trashed INTEGER DEFAULT 0,"
        "  original_local_path TEXT"
        ")"));

    if (!filesTableOk) {
        return false;
    }

    // ========================================================
    // tags
    // ========================================================

    const bool tagsTableOk = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS tags ("
        "  file_id TEXT NOT NULL,"
        "  tag TEXT NOT NULL,"
        "  PRIMARY KEY (file_id, tag)"
        ")"));

    if (!tagsTableOk) {
        return false;
    }

    // ========================================================
    // sources
    // ========================================================
    //
    // 他人群组数据源（FileSpace::GroupUs）。
    //
    // 每个来源保存对方 WebDAV 的：
    //
    //     地址 / 账号 / 密码 / 群组根目录
    //
    // 本表内容永远只用于读取，
    // 不会用于向对方账号写入。
    // ========================================================

    const bool sourcesTableOk = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS sources ("
        "  source_id TEXT PRIMARY KEY,"
        "  display_name TEXT,"
        "  server_url TEXT,"
        "  username TEXT,"
        "  password TEXT,"
        "  remote_root TEXT,"
        "  read_only INTEGER DEFAULT 1,"
        "  last_sync TEXT"
        ")"));

    if (!sourcesTableOk) {
        return false;
    }

    // ========================================================
    // pending_deletes
    // ========================================================

    const bool pendingTableOk = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS pending_deletes ("
        "  remote_path TEXT PRIMARY KEY,"
        "  source_id TEXT,"
        "  is_collection INTEGER DEFAULT 0"
        ")"));

    if (!pendingTableOk) {
        return false;
    }

    // ========================================================
    // Migration
    // ========================================================
    //
    // SQLite 不支持：
    //
    // ALTER TABLE ... ADD COLUMN IF NOT EXISTS
    //
    // 因此采用：
    //
    //     ALTER TABLE ... ADD COLUMN ...
    //
    // 如果字段已经存在，则忽略错误。
    //
    // ========================================================

    // parent_id
    query.exec(QStringLiteral(
        "ALTER TABLE files "
        "ADD COLUMN parent_id TEXT"));

    // object_type
    query.exec(QStringLiteral(
        "ALTER TABLE files "
        "ADD COLUMN object_type INTEGER "
        "DEFAULT 2"));

    // space
    query.exec(QStringLiteral(
        "ALTER TABLE files "
        "ADD COLUMN space INTEGER "
        "DEFAULT 0"));

    // source_id
    query.exec(QStringLiteral(
        "ALTER TABLE files "
        "ADD COLUMN source_id TEXT"));

    // group_id
    query.exec(QStringLiteral(
        "ALTER TABLE files "
        "ADD COLUMN group_id TEXT"));

    // pending_deletes.is_collection
    query.exec(QStringLiteral(
        "ALTER TABLE pending_deletes "
        "ADD COLUMN is_collection INTEGER DEFAULT 0"));

    // annotations_path
    query.exec(QStringLiteral(
        "ALTER TABLE files "
        "ADD COLUMN annotations_path TEXT"));

    // is_trashed
    query.exec(QStringLiteral(
        "ALTER TABLE files "
        "ADD COLUMN is_trashed INTEGER "
        "DEFAULT 0"));

    // original_local_path
    query.exec(QStringLiteral(
        "ALTER TABLE files "
        "ADD COLUMN original_local_path TEXT"));

    // ========================================================
    // Legacy column
    // ========================================================
    //
    // 老版本使用 zone：
    //
    //     0 = MyFiles
    //     1 = GroupFiles
    //
    // 新版本使用 space：
    //
    //     0 = MyDoc
    //     1 = GroupMe
    //     2 = GroupUs
    //
    // 这里只保留 zone 是为了让旧数据库能够迁移。
    // 新代码不再使用 zone。
    // ========================================================

    query.exec(QStringLiteral(
        "ALTER TABLE files "
        "ADD COLUMN zone INTEGER "
        "DEFAULT 0"));

    // ========================================================
    // Normalize NULL values
    // ========================================================

    query.exec(QStringLiteral(
        "UPDATE files "
        "SET is_trashed = 0 "
        "WHERE is_trashed IS NULL"));

    query.exec(QStringLiteral(
        "UPDATE files "
        "SET space = 0 "
        "WHERE space IS NULL"));

    query.exec(QStringLiteral(
        "UPDATE files "
        "SET object_type = 2 "
        "WHERE object_type IS NULL"));

    // ========================================================
    // Migrate old zone -> new space
    // ========================================================
    //
    // 旧：
    //
    //     zone = 0 -> MyFiles
    //     zone = 1 -> GroupFiles
    //
    // 新：
    //
    //     space = 0 -> MyDoc
    //     space = 1 -> GroupMe
    //     space = 2 -> GroupUs
    //
    // 旧数据库不存在 GroupUs，因此不会生成 space = 2。
    //
    // 对旧记录：
    //
    //     zone = 1 -> GroupMe
    //     zone = 0 -> MyDoc
    //
    // ========================================================

    query.exec(QStringLiteral(
        "UPDATE files "
        "SET space = CASE "
        "    WHEN zone = 1 THEN 1 "
        "    ELSE 0 "
        "END "
        "WHERE space IS NULL "
        "   OR space = 0"));

    // ========================================================
    // Normalize old records
    // ========================================================
    //
    // 老版本 FileItem 没有：
    //
    //     parentId
    //     objectType
    //     sourceId
    //
    // 因此旧文件统一作为：
    //
    //     root object
    //     AtomicFile
    //
    // ========================================================

    query.exec(QStringLiteral(
        "UPDATE files "
        "SET parent_id = NULL "
        "WHERE parent_id = ''"));

    query.exec(QStringLiteral(
        "UPDATE files "
        "SET object_type = 2 "
        "WHERE object_type IS NULL"));

    // ========================================================
    // Indexes
    // ========================================================

    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS "
        "idx_files_parent_space "
        "ON files(parent_id, space)"));

    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS "
        "idx_files_space "
        "ON files(space)"));

    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS "
        "idx_files_source_space "
        "ON files(source_id, space)"));

    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS "
        "idx_files_trashed "
        "ON files(is_trashed)"));

    return true;
}

// ============================================================
// Add file
// ============================================================

bool MetadataStore::addFile(const FileItem &item)
{
    if (!m_db || !item.isValid()) {
        return false;
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "INSERT INTO files ("
        "  id,"
        "  parent_id,"
        "  name,"
        "  object_type,"
        "  space,"
        "  source_id,"
        "  group_id,"
        "  local_path,"
        "  remote_path,"
        "  extension,"
        "  size,"
        "  modified_time,"
        "  category,"
        "  sync_state,"
        "  sync_error,"
        "  annotations_path,"
        "  is_trashed,"
        "  original_local_path"
        ") VALUES ("
        "  ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?"
        ")"));

    query.addBindValue(item.id);
    query.addBindValue(item.parentId);
    query.addBindValue(item.name);
    query.addBindValue(static_cast<int>(item.objectType));
    query.addBindValue(static_cast<int>(item.space));
    query.addBindValue(item.sourceId);
    query.addBindValue(item.groupId);
    query.addBindValue(item.localPath);
    query.addBindValue(item.remotePath);
    query.addBindValue(item.extension);
    query.addBindValue(item.size);

    query.addBindValue(
        item.modifiedTime.isValid()
            ? item.modifiedTime.toString(Qt::ISODate)
            : QString());

    query.addBindValue(static_cast<int>(item.category));
    query.addBindValue(static_cast<int>(item.syncState));
    query.addBindValue(item.syncError);
    query.addBindValue(item.annotationsPath);

    // 新对象默认不在回收站。
    query.addBindValue(0);

    // 正常对象没有 original_local_path。
    query.addBindValue(QString());

    if (!query.exec()) {
        return false;
    }

    // ========================================================
    // Tags
    // ========================================================

    QSqlQuery tagQuery(*m_db);

    tagQuery.prepare(QStringLiteral(
        "INSERT INTO tags (file_id, tag) "
        "VALUES (?, ?)"));

    for (const QString &tag : item.tags) {
        if (tag.isEmpty()) {
            continue;
        }

        tagQuery.addBindValue(item.id);
        tagQuery.addBindValue(tag);

        if (!tagQuery.exec()) {
            return false;
        }
    }

    return true;
}

// ============================================================
// Update file
// ============================================================

bool MetadataStore::updateFile(const FileItem &item)
{
    if (!m_db || !item.isValid()) {
        return false;
    }

    // ========================================================
    // 注意：
    //
    // is_trashed
    // original_local_path
    //
    // 属于回收站状态。
    //
    // 普通 metadata 更新不修改这两个字段。
    // ========================================================

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "UPDATE files SET "
        "  parent_id = ?, "
        "  name = ?, "
        "  object_type = ?, "
        "  space = ?, "
        "  source_id = ?, "
        "  group_id = ?, "
        "  local_path = ?, "
        "  remote_path = ?, "
        "  extension = ?, "
        "  size = ?, "
        "  modified_time = ?, "
        "  category = ?, "
        "  sync_state = ?, "
        "  sync_error = ?, "
        "  annotations_path = ? "
        "WHERE id = ?"));

    query.addBindValue(item.parentId);
    query.addBindValue(item.name);
    query.addBindValue(static_cast<int>(item.objectType));
    query.addBindValue(static_cast<int>(item.space));
    query.addBindValue(item.sourceId);
    query.addBindValue(item.groupId);
    query.addBindValue(item.localPath);
    query.addBindValue(item.remotePath);
    query.addBindValue(item.extension);
    query.addBindValue(item.size);

    query.addBindValue(
        item.modifiedTime.isValid()
            ? item.modifiedTime.toString(Qt::ISODate)
            : QString());

    query.addBindValue(static_cast<int>(item.category));
    query.addBindValue(static_cast<int>(item.syncState));
    query.addBindValue(item.syncError);
    query.addBindValue(item.annotationsPath);
    query.addBindValue(item.id);

    if (!query.exec()) {
        return false;
    }

    if (query.numRowsAffected() == 0) {
        return false;
    }

    // ========================================================
    // Replace tags
    // ========================================================

    QSqlQuery deleteTags(*m_db);

    deleteTags.prepare(QStringLiteral(
        "DELETE FROM tags "
        "WHERE file_id = ?"));

    deleteTags.addBindValue(item.id);

    if (!deleteTags.exec()) {
        return false;
    }

    QSqlQuery tagQuery(*m_db);

    tagQuery.prepare(QStringLiteral(
        "INSERT INTO tags (file_id, tag) "
        "VALUES (?, ?)"));

    for (const QString &tag : item.tags) {
        if (tag.isEmpty()) {
            continue;
        }

        tagQuery.addBindValue(item.id);
        tagQuery.addBindValue(tag);

        if (!tagQuery.exec()) {
            return false;
        }
    }

    return true;
}

// ============================================================
// Update local path
// ============================================================

bool MetadataStore::updateLocalPath(
    const QString &fileId,
    const QString &localPath)
{
    if (!m_db || fileId.isEmpty()) {
        return false;
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "UPDATE files "
        "SET local_path = ?, "
        "    sync_state = ? "
        "WHERE id = ?"));

    query.addBindValue(localPath);

    query.addBindValue(
        static_cast<int>(SyncState::Synced));

    query.addBindValue(fileId);

    if (!query.exec()) {
        return false;
    }

    return query.numRowsAffected() > 0;
}

// ============================================================
// Move to trash
// ============================================================

bool MetadataStore::moveToTrash(
    const QString &fileId,
    const QString &trashLocalPath,
    const QString &originalLocalPath)
{
    if (!m_db || fileId.isEmpty()) {
        return false;
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "UPDATE files "
        "SET "
        "  local_path = ?, "
        "  original_local_path = ?, "
        "  is_trashed = 1 "
        "WHERE id = ?"));

    query.addBindValue(trashLocalPath);
    query.addBindValue(originalLocalPath);
    query.addBindValue(fileId);

    if (!query.exec()) {
        return false;
    }

    return query.numRowsAffected() > 0;
}

// ============================================================
// Restore from trash
// ============================================================

bool MetadataStore::restoreFromTrash(
    const QString &fileId,
    const QString &localPath)
{
    if (!m_db || fileId.isEmpty()) {
        return false;
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "UPDATE files "
        "SET "
        "  local_path = ?, "
        "  is_trashed = 0, "
        "  original_local_path = NULL "
        "WHERE id = ?"));

    query.addBindValue(localPath);
    query.addBindValue(fileId);

    if (!query.exec()) {
        return false;
    }

    return query.numRowsAffected() > 0;
}

// ============================================================
// Permanently delete
// ============================================================

bool MetadataStore::permanentlyDelete(
    const QString &fileId)
{
    if (!m_db || fileId.isEmpty()) {
        return false;
    }

    // ========================================================
    // 先删除 tags。
    // ========================================================

    QSqlQuery tags(*m_db);

    tags.prepare(QStringLiteral(
        "DELETE FROM tags "
        "WHERE file_id = ?"));

    tags.addBindValue(fileId);

    if (!tags.exec()) {
        return false;
    }

    // ========================================================
    // 再删除 files。
    // ========================================================

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "DELETE FROM files "
        "WHERE id = ?"));

    query.addBindValue(fileId);

    if (!query.exec()) {
        return false;
    }

    return query.numRowsAffected() > 0;
}

// ============================================================
// Legacy removeFile
// ============================================================

bool MetadataStore::removeFile(
    const QString &fileId)
{
    // 保留旧接口。
    //
    // removeFile()
    //     =
    // permanentlyDelete()
    //
    // 正常回收站流程：
    //
    //     moveToTrash()
    //
    // 真正永久删除：
    //
    //     permanentlyDelete()
    //

    return permanentlyDelete(fileId);
}

// ============================================================
// Helper: build FileItem
// ============================================================

static FileItem fileItemFromQuery(
    const QSqlQuery &query)
{
    FileItem item;

    item.id =
        query.value(QStringLiteral("id")).toString();

    item.parentId =
        query.value(QStringLiteral("parent_id")).toString();

    item.name =
        query.value(QStringLiteral("name")).toString();

    item.objectType =
        static_cast<FileObjectType>(
            query.value(
                     QStringLiteral("object_type")).toInt());

    item.space =
        static_cast<FileSpace>(
            query.value(
                     QStringLiteral("space")).toInt());

    item.sourceId =
        query.value(
                 QStringLiteral("source_id")).toString();

    item.groupId =
        query.value(
                 QStringLiteral("group_id")).toString();

    item.localPath =
        query.value(
                 QStringLiteral("local_path")).toString();

    item.remotePath =
        query.value(
                 QStringLiteral("remote_path")).toString();

    item.extension =
        query.value(
                 QStringLiteral("extension")).toString();

    item.size =
        query.value(
                 QStringLiteral("size")).toLongLong();

    item.modifiedTime =
        QDateTime::fromString(
            query.value(
                     QStringLiteral("modified_time")).toString(),
            Qt::ISODate);

    item.category =
        static_cast<FileCategory>(
            query.value(
                     QStringLiteral("category")).toInt());

    item.syncState =
        static_cast<SyncState>(
            query.value(
                     QStringLiteral("sync_state")).toInt());

    item.syncError =
        query.value(
                 QStringLiteral("sync_error")).toString();

    item.annotationsPath =
        query.value(
                 QStringLiteral("annotations_path")).toString();

    const QString tags =
        query.value(
                 QStringLiteral("tags")).toString();

    if (!tags.isEmpty()) {
        item.tags =
            tags.split(
                QLatin1Char(','),
                Qt::SkipEmptyParts);
    }

    return item;
}

// ============================================================
// Normal files
// ============================================================

QVector<FileItem> MetadataStore::files(
    FileSpace space) const
{
    QVector<FileItem> result;

    if (!m_db) {
        return result;
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "SELECT "
        "  f.id, "
        "  f.parent_id, "
        "  f.name, "
        "  f.object_type, "
        "  f.space, "
        "  f.source_id, "
        "  f.group_id, "
        "  f.local_path, "
        "  f.remote_path, "
        "  f.extension, "
        "  f.size, "
        "  f.modified_time, "
        "  f.category, "
        "  f.sync_state, "
        "  f.sync_error, "
        "  f.annotations_path, "
        "  GROUP_CONCAT(t.tag, ',') AS tags "
        "FROM files f "
        "LEFT JOIN tags t "
        "  ON t.file_id = f.id "
        "WHERE f.space = ? "
        "  AND COALESCE(f.is_trashed, 0) = 0 "
        "GROUP BY f.id "
        "ORDER BY f.modified_time DESC"));

    query.addBindValue(
        static_cast<int>(space));

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        result.append(
            fileItemFromQuery(query));
    }

    return result;
}

// ============================================================
// Children
// ============================================================

QVector<FileItem> MetadataStore::childrenOf(
    const QString &parentId,
    FileSpace space) const
{
    QVector<FileItem> result;

    if (!m_db) {
        return result;
    }

    QSqlQuery query(*m_db);

    if (parentId.isEmpty()) {

        // ====================================================
        // Root
        // ====================================================

        query.prepare(QStringLiteral(
            "SELECT "
            "  f.id, "
            "  f.parent_id, "
            "  f.name, "
            "  f.object_type, "
            "  f.space, "
            "  f.source_id, "
            "  f.group_id, "
            "  f.local_path, "
            "  f.remote_path, "
            "  f.extension, "
            "  f.size, "
            "  f.modified_time, "
            "  f.category, "
            "  f.sync_state, "
            "  f.sync_error, "
            "  f.annotations_path, "
            "  GROUP_CONCAT(t.tag, ',') AS tags "
            "FROM files f "
            "LEFT JOIN tags t "
            "  ON t.file_id = f.id "
            "WHERE f.space = ? "
            "  AND (f.parent_id IS NULL "
            "       OR f.parent_id = '') "
            "  AND COALESCE(f.is_trashed, 0) = 0 "
            "GROUP BY f.id "
            "ORDER BY "
            "  f.object_type ASC, "
            "  f.name COLLATE NOCASE ASC"));

    } else {

        // ====================================================
        // Child objects
        // ====================================================

        query.prepare(QStringLiteral(
            "SELECT "
            "  f.id, "
            "  f.parent_id, "
            "  f.name, "
            "  f.object_type, "
            "  f.space, "
            "  f.source_id, "
            "  f.group_id, "
            "  f.local_path, "
            "  f.remote_path, "
            "  f.extension, "
            "  f.size, "
            "  f.modified_time, "
            "  f.category, "
            "  f.sync_state, "
            "  f.sync_error, "
            "  f.annotations_path, "
            "  GROUP_CONCAT(t.tag, ',') AS tags "
            "FROM files f "
            "LEFT JOIN tags t "
            "  ON t.file_id = f.id "
            "WHERE f.parent_id = ? "
            "  AND f.space = ? "
            "  AND COALESCE(f.is_trashed, 0) = 0 "
            "GROUP BY f.id "
            "ORDER BY "
            "  f.object_type ASC, "
            "  f.name COLLATE NOCASE ASC"));
    }

    if (parentId.isEmpty()) {
        query.addBindValue(
            static_cast<int>(space));
    } else {
        query.addBindValue(parentId);
        query.addBindValue(
            static_cast<int>(space));
    }

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        result.append(
            fileItemFromQuery(query));
    }

    return result;
}

// ============================================================
// Root files
// ============================================================

QVector<FileItem> MetadataStore::rootFiles(
    FileSpace space) const
{
    return childrenOf(
        QString(),
        space);
}

// ============================================================
// Trash files
// ============================================================

QVector<FileItem> MetadataStore::trashFiles(
    FileSpace space) const
{
    QVector<FileItem> result;

    if (!m_db) {
        return result;
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "SELECT "
        "  f.id, "
        "  f.parent_id, "
        "  f.name, "
        "  f.object_type, "
        "  f.space, "
        "  f.source_id, "
        "  f.group_id, "
        "  f.local_path, "
        "  f.remote_path, "
        "  f.extension, "
        "  f.size, "
        "  f.modified_time, "
        "  f.category, "
        "  f.sync_state, "
        "  f.sync_error, "
        "  f.annotations_path, "
        "  GROUP_CONCAT(t.tag, ',') AS tags "
        "FROM files f "
        "LEFT JOIN tags t "
        "  ON t.file_id = f.id "
        "WHERE f.space = ? "
        "  AND COALESCE(f.is_trashed, 0) = 1 "
        "GROUP BY f.id "
        "ORDER BY f.modified_time DESC"));

    query.addBindValue(
        static_cast<int>(space));

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        result.append(
            fileItemFromQuery(query));
    }

    return result;
}

// ============================================================
// Is trashed
// ============================================================

bool MetadataStore::isTrashed(
    const QString &fileId) const
{
    if (!m_db || fileId.isEmpty()) {
        return false;
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "SELECT COALESCE(is_trashed, 0) "
        "FROM files "
        "WHERE id = ?"));

    query.addBindValue(fileId);

    if (!query.exec()) {
        return false;
    }

    if (!query.next()) {
        return false;
    }

    return query.value(0).toInt() != 0;
}

// ============================================================
// Original local path
// ============================================================

QString MetadataStore::originalLocalPath(
    const QString &fileId) const
{
    if (!m_db || fileId.isEmpty()) {
        return QString();
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "SELECT original_local_path "
        "FROM files "
        "WHERE id = ?"));

    query.addBindValue(fileId);

    if (!query.exec()) {
        return QString();
    }

    if (!query.next()) {
        return QString();
    }

    return query.value(0).toString();
}

// ============================================================
// File by ID
// ============================================================

FileItem MetadataStore::fileById(
    const QString &fileId) const
{
    if (!m_db || fileId.isEmpty()) {
        return FileItem{};
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "SELECT "
        "  f.id, "
        "  f.parent_id, "
        "  f.name, "
        "  f.object_type, "
        "  f.space, "
        "  f.source_id, "
        "  f.group_id, "
        "  f.local_path, "
        "  f.remote_path, "
        "  f.extension, "
        "  f.size, "
        "  f.modified_time, "
        "  f.category, "
        "  f.sync_state, "
        "  f.sync_error, "
        "  f.annotations_path, "
        "  GROUP_CONCAT(t.tag, ',') AS tags "
        "FROM files f "
        "LEFT JOIN tags t "
        "  ON t.file_id = f.id "
        "WHERE f.id = ? "
        "GROUP BY f.id"));

    query.addBindValue(fileId);

    if (!query.exec()) {
        return FileItem{};
    }

    if (!query.next()) {
        return FileItem{};
    }

    return fileItemFromQuery(query);
}

// ============================================================
// Files by category
// ============================================================

QVector<FileItem> MetadataStore::filesByCategory(
    FileCategory category,
    FileSpace space) const
{
    QVector<FileItem> result;

    const QVector<FileItem> all =
        files(space);

    for (const FileItem &item : all) {
        if (item.category == category) {
            result.append(item);
        }
    }

    return result;
}

// ============================================================
// Files by tag
// ============================================================

QVector<FileItem> MetadataStore::filesByTag(
    const QString &tag,
    FileSpace space) const
{
    QVector<FileItem> result;

    const QVector<FileItem> all =
        files(space);

    for (const FileItem &item : all) {
        if (item.tags.contains(tag)) {
            result.append(item);
        }
    }

    return result;
}

// ============================================================
// Helper: build GroupSource
// ============================================================

static GroupSource groupSourceFromQuery(
    const QSqlQuery &query)
{
    GroupSource source;

    source.sourceId =
        query.value(
                 QStringLiteral("source_id")).toString();

    source.displayName =
        query.value(
                 QStringLiteral("display_name")).toString();

    source.serverUrl =
        query.value(
                 QStringLiteral("server_url")).toString();

    source.username =
        query.value(
                 QStringLiteral("username")).toString();

    source.password =
        query.value(
                 QStringLiteral("password")).toString();

    source.remoteRoot =
        query.value(
                 QStringLiteral("remote_root")).toString();

    source.readOnly =
        query.value(
                 QStringLiteral("read_only")).toInt() != 0;

    source.lastSync =
        QDateTime::fromString(
            query.value(
                QStringLiteral("last_sync")).toString(),
            Qt::ISODate);

    return source;
}

// ============================================================
// Sources
// ============================================================

QVector<GroupSource> MetadataStore::sources() const
{
    QVector<GroupSource> result;

    if (!m_db) {
        return result;
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "SELECT "
        "  source_id, "
        "  display_name, "
        "  server_url, "
        "  username, "
        "  password, "
        "  remote_root, "
        "  read_only, "
        "  last_sync "
        "FROM sources "
        "ORDER BY display_name COLLATE NOCASE ASC"));

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {
        result.append(
            groupSourceFromQuery(query));
    }

    return result;
}

GroupSource MetadataStore::sourceById(
    const QString &sourceId) const
{
    if (!m_db || sourceId.isEmpty()) {
        return GroupSource{};
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "SELECT "
        "  source_id, "
        "  display_name, "
        "  server_url, "
        "  username, "
        "  password, "
        "  remote_root, "
        "  read_only, "
        "  last_sync "
        "FROM sources "
        "WHERE source_id = ?"));

    query.addBindValue(sourceId);

    if (!query.exec()) {
        return GroupSource{};
    }

    if (!query.next()) {
        return GroupSource{};
    }

    return groupSourceFromQuery(query);
}

bool MetadataStore::upsertSource(
    const GroupSource &source)
{
    if (!m_db || !source.isValid()) {
        return false;
    }

    QSqlQuery update(*m_db);

    update.prepare(QStringLiteral(
        "UPDATE sources SET "
        "  display_name = ?, "
        "  server_url = ?, "
        "  username = ?, "
        "  password = ?, "
        "  remote_root = ?, "
        "  read_only = ? "
        "WHERE source_id = ?"));

    update.addBindValue(source.displayName);
    update.addBindValue(source.serverUrl);
    update.addBindValue(source.username);
    update.addBindValue(source.password);
    update.addBindValue(source.remoteRoot);

    // 他人群组永远是只读的。
    update.addBindValue(1);

    update.addBindValue(source.sourceId);

    if (!update.exec()) {
        return false;
    }

    if (update.numRowsAffected() > 0) {
        return true;
    }

    QSqlQuery insert(*m_db);

    insert.prepare(QStringLiteral(
        "INSERT INTO sources ("
        "  source_id, "
        "  display_name, "
        "  server_url, "
        "  username, "
        "  password, "
        "  remote_root, "
        "  read_only, "
        "  last_sync"
        ") VALUES (?, ?, ?, ?, ?, ?, 1, ?)"));

    insert.addBindValue(source.sourceId);
    insert.addBindValue(source.displayName);
    insert.addBindValue(source.serverUrl);
    insert.addBindValue(source.username);
    insert.addBindValue(source.password);
    insert.addBindValue(source.remoteRoot);

    insert.addBindValue(
        source.lastSync.isValid()
            ? source.lastSync.toString(Qt::ISODate)
            : QVariant());

    return insert.exec();
}

bool MetadataStore::removeSource(
    const QString &sourceId)
{
    if (!m_db || sourceId.isEmpty()) {
        return false;
    }

    // ========================================================
    // 同时清理该来源在 files 中的全部条目。
    // ========================================================

    QSqlQuery files(*m_db);

    files.prepare(QStringLiteral(
        "DELETE FROM files "
        "WHERE source_id = ?"));

    files.addBindValue(sourceId);

    if (!files.exec()) {
        return false;
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "DELETE FROM sources "
        "WHERE source_id = ?"));

    query.addBindValue(sourceId);

    if (!query.exec()) {
        return false;
    }

    return query.numRowsAffected() > 0;
}

// ============================================================
// Pending remote deletes
// ============================================================

bool MetadataStore::addPendingDelete(
    const QString &remotePath,
    const QString &sourceId,
    bool isCollection)
{
    if (!m_db || remotePath.trimmed().isEmpty()) {
        return false;
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO pending_deletes ("
        "  remote_path, source_id, is_collection"
        ") VALUES (?, ?, ?)"));

    query.addBindValue(remotePath.trimmed());
    query.addBindValue(sourceId);
    query.addBindValue(isCollection ? 1 : 0);

    return query.exec();
}

QVector<QPair<QString, bool>> MetadataStore::pendingDeletes(
    const QString &sourceId) const
{
    QVector<QPair<QString, bool>> result;

    if (!m_db) {
        return result;
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "SELECT remote_path, COALESCE(is_collection, 0) "
        "FROM pending_deletes "
        "WHERE COALESCE(source_id, '') = ? "
        "ORDER BY remote_path"));

    query.addBindValue(sourceId);

    if (!query.exec()) {
        return result;
    }

    while (query.next()) {

        result.append(qMakePair(
            query.value(0).toString(),
            query.value(1).toInt() != 0));
    }

    return result;
}

bool MetadataStore::clearPendingDelete(
    const QString &remotePath)
{
    if (!m_db || remotePath.isEmpty()) {
        return false;
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "DELETE FROM pending_deletes "
        "WHERE remote_path = ?"));

    query.addBindValue(remotePath);

    return query.exec();
}

bool MetadataStore::touchSourceSyncTime(
    const QString &sourceId)
{
    if (!m_db || sourceId.isEmpty()) {
        return false;
    }

    QSqlQuery query(*m_db);

    query.prepare(QStringLiteral(
        "UPDATE sources "
        "SET last_sync = ? "
        "WHERE source_id = ?"));

    query.addBindValue(
        QDateTime::currentDateTime()
            .toString(Qt::ISODate));

    query.addBindValue(sourceId);

    if (!query.exec()) {
        return false;
    }

    return query.numRowsAffected() > 0;
}

} // namespace litapp