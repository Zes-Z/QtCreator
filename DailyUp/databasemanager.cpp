#include "databasemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QDate> // 新增：用于获取当前日期
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QFile>

#ifdef Q_OS_MACOS
#include <sys/xattr.h>
#endif

namespace {

// 数据文件可能从只读位置复制过来（模式 444），或被 Gatekeeper 打上隔离属性。
// SQLite 打不开写事务时任务会"保存了但没出现"，所以启动前主动修复。
void repairWritableFile(const QString &path)
{
    QFile file(path);
    if (!file.exists())
        return;

    const QFile::Permissions perms = file.permissions();
    if (!(perms & QFileDevice::WriteOwner)) {
        const QFile::Permissions updated =
            perms | QFileDevice::ReadOwner | QFileDevice::WriteOwner;
        if (file.setPermissions(updated))
            qInfo() << "Restored write permission:" << path;
        else
            qWarning() << "Could not restore write permission:" << path;
    }

#ifdef Q_OS_MACOS
    const QByteArray nativePath = QFile::encodeName(path);
    removexattr(nativePath.constData(), "com.apple.quarantine", 0);
#endif
}

void ensureDatabaseWritable(const QString &dataDir, const QString &dbPath)
{
    QFile directory(dataDir);
    if (directory.exists()) {
        const QFile::Permissions perms = directory.permissions();
        if (!(perms & QFileDevice::WriteOwner)) {
            directory.setPermissions(perms
                                     | QFileDevice::ReadOwner
                                     | QFileDevice::WriteOwner
                                     | QFileDevice::ExeOwner);
        }
    }

    repairWritableFile(dbPath);
    repairWritableFile(dbPath + QStringLiteral("-wal"));
    repairWritableFile(dbPath + QStringLiteral("-shm"));
}

// 1.0 之前可执行文件名为 appDailyUp，数据目录也随之不同。
// 首次启动新版本时把旧库复制过来，避免用户看不到历史打卡。
void migrateLegacyDatabase(const QString &dataDir, const QString &dbPath)
{
    if (QFileInfo::exists(dbPath))
        return;

    const QFileInfo dirInfo(dataDir);
    if (dirInfo.fileName() != QStringLiteral("DailyUp"))
        return;

    // Qt 的 AppDataLocation 在 macOS 上包含组织名：
    //   ~/Library/Application Support/Zes/DailyUp
    // 旧版本（可执行文件 appDailyUp）没有组织目录，可能在上一级或再上一级。
    const QDir appDir = dirInfo.dir();
    const QDir parentDir(QDir::cleanPath(appDir.absolutePath() + QStringLiteral("/..")));
    const QStringList legacyDirs = {
        appDir.filePath(QStringLiteral("appDailyUp")),
        parentDir.filePath(QStringLiteral("appDailyUp")),
        parentDir.filePath(QStringLiteral("DailyUp"))
    };

    QString legacyDir;
    QString legacyDb;
    for (const QString &candidate : legacyDirs) {
        if (QDir::cleanPath(candidate) == QDir::cleanPath(dataDir))
            continue;
        const QString candidateDb = QDir(candidate).filePath(QStringLiteral("dailyup.db"));
        if (QFileInfo::exists(candidateDb)) {
            legacyDir = candidate;
            legacyDb = candidateDb;
            break;
        }
    }
    if (legacyDir.isEmpty())
        return;

    QDir().mkpath(dataDir);
    if (!QFile::copy(legacyDb, dbPath)) {
        qWarning() << "Failed to migrate legacy database from" << legacyDb;
        return;
    }
    for (const QString &suffix : { QStringLiteral("-wal"), QStringLiteral("-shm") }) {
        const QString source = legacyDb + suffix;
        if (QFileInfo::exists(source))
            QFile::copy(source, dbPath + suffix);
    }
    qInfo() << "Migrated legacy database from" << legacyDb << "to" << dbPath;
}
} // namespace

DatabaseManager& DatabaseManager::instance() {
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent) {
}

DatabaseManager::~DatabaseManager() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::initDatabase() {
    // 防止重复添加默认连接导致警告
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        m_db = QSqlDatabase::database("qt_sql_default_connection");
    } else {
        m_db = QSqlDatabase::addDatabase("QSQLITE");
    }

    // 系统标准应用数据目录（macOS: ~/Library/Application Support/Zes/DailyUp）
    // DAILYUP_DATA_DIR 仅用于开发 / 截图 / 自测时隔离数据。
    QString dataDir = qEnvironmentVariable("DAILYUP_DATA_DIR");
    if (dataDir.isEmpty()) {
        dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 设置数据库文件路径
    const QString dbPath = dir.filePath(QStringLiteral("dailyup.db"));
    migrateLegacyDatabase(dataDir, dbPath);
    ensureDatabaseWritable(dataDir, dbPath);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qCritical() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }

    qDebug() << "Database opened successfully at:" << dbPath;

    // 性能：桌面单用户场景，WAL + NORMAL 可显著降低每次写操作的磁盘刷写与锁开销
    {
        QSqlQuery pragma(m_db);
        pragma.exec("PRAGMA journal_mode = WAL");
        pragma.exec("PRAGMA synchronous = NORMAL");
        pragma.exec("PRAGMA busy_timeout = 3000");
    }

    // 明确探测一次写事务；失败时继续以只读方式运行，并在用户保存任务时提示。
    {
        QSqlQuery writable(m_db);
        if (writable.exec(QStringLiteral("BEGIN IMMEDIATE"))) {
            writable.exec(QStringLiteral("ROLLBACK"));
        } else {
            qWarning() << "Database is not writable:" << writable.lastError().text();
        }
    }

    // 创建 Tasks 表
    QSqlQuery query;
    QString createTableQuery = R"(
        CREATE TABLE IF NOT EXISTS Tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            date TEXT NOT NULL,
            content TEXT NOT NULL,
            is_completed INTEGER DEFAULT 0,
            is_pinned INTEGER DEFAULT 0
        )
    )";

    if (!query.exec(createTableQuery)) {
        qCritical() << "Failed to create table:" << query.lastError().text();
        return false;
    }

    // 性能：任务按日期查询/排序很频繁，为 date 建索引（含置顶排序的预读）
    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_tasks_date ON Tasks(date)")) {
        qWarning() << "Failed to create index:" << query.lastError().text();
    }

    return true;
}

bool DatabaseManager::addTask(const QString& date, const QString& content) {
    QSqlQuery query;
    query.prepare("INSERT INTO Tasks (date, content, is_completed, is_pinned) VALUES (:date, :content, 0, 0)");
    query.bindValue(":date", date);
    query.bindValue(":content", content);

    if (!query.exec()) {
        qWarning() << "Failed to add task:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<TaskData> DatabaseManager::getTasksByDate(const QString& date) {
    QList<TaskData> taskList;
    QSqlQuery query;

    // 查询指定日期的任务，优先展示置顶任务，然后按 ID 排序
    query.prepare("SELECT id, content, is_completed, is_pinned FROM Tasks WHERE date = :date ORDER BY is_pinned DESC, id ASC");
    query.bindValue(":date", date);

    if (query.exec()) {
        while (query.next()) {
            TaskData task;
            task.id = query.value("id").toInt();
            task.content = query.value("content").toString();
            task.isCompleted = query.value("is_completed").toBool();
            task.isPinned = query.value("is_pinned").toBool();
            taskList.append(task);
        }
    } else {
        qWarning() << "Failed to fetch tasks for date" << date << ":" << query.lastError().text();
    }

    return taskList;
}

bool DatabaseManager::updateTaskStatus(int id, bool isCompleted) {
    QSqlQuery query;
    query.prepare("UPDATE Tasks SET is_completed = :is_completed WHERE id = :id");
    query.bindValue(":is_completed", isCompleted ? 1 : 0);
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "Failed to update task status:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::deleteTask(int id) {
    QSqlQuery query;
    query.prepare("DELETE FROM Tasks WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "Failed to delete task:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::updateTaskContent(int id, const QString& content) {
    QSqlQuery query;
    query.prepare("UPDATE Tasks SET content = :content WHERE id = :id");
    query.bindValue(":content", content);
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "Failed to update task content:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::toggleTaskPin(int id, bool isPinned) {
    QSqlQuery query;
    query.prepare("UPDATE Tasks SET is_pinned = :is_pinned WHERE id = :id");
    query.bindValue(":is_pinned", isPinned ? 1 : 0);
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "Failed to toggle task pin status:" << query.lastError().text();
        return false;
    }
    return true;
}

// ================= 新增：检查并执行跨日任务继承 =================
bool DatabaseManager::rolloverTasksForToday() {
    // 获取系统的 "今天" 日期
    QString todayStr = QDate::currentDate().toString("yyyy-MM-dd");

    // 1. 先检查今天是否已经有任务了 (避免重复复制)
    QSqlQuery checkQuery;
    checkQuery.prepare("SELECT COUNT(*) FROM Tasks WHERE date = :today");
    checkQuery.bindValue(":today", todayStr);

    if (checkQuery.exec() && checkQuery.next()) {
        if (checkQuery.value(0).toInt() > 0) {
            // 今天已经有数据了，直接返回 false，表示不需要进行跨日操作
            return false;
        }
    }

    // 2. 寻找数据库中离今天最近的、且有记录的历史日期 (上一次使用软件的日期)
    QSqlQuery lastDateQuery;
    lastDateQuery.prepare("SELECT date FROM Tasks WHERE date < :today ORDER BY date DESC LIMIT 1");
    lastDateQuery.bindValue(":today", todayStr);

    QString lastDate;
    if (lastDateQuery.exec() && lastDateQuery.next()) {
        lastDate = lastDateQuery.value(0).toString();
    } else {
        // 如果数据库完全是空的，或者没有任何早于今天的记录，也不需要复制
        return false;
    }

    // 3. 核心逻辑：只继承任务内容，不继承任何状态。
    // 完成与置顶都从 0 开始，新的一天是一张干净清单。
    QSqlQuery copyQuery;
    copyQuery.prepare(R"(
        INSERT INTO Tasks (date, content, is_completed, is_pinned)
        SELECT :today, content, 0, 0
        FROM Tasks
        WHERE date = :lastDate
    )");
    copyQuery.bindValue(":today", todayStr);
    copyQuery.bindValue(":lastDate", lastDate);

    if (!copyQuery.exec()) {
        qWarning() << "Failed to rollover tasks:" << copyQuery.lastError().text();
        return false;
    }

    qDebug() << "Successfully rolled over tasks from" << lastDate << "to" << todayStr;
    return true; // 跨日复制成功
}

// 在系统资源管理器中打开数据库文件所在的目录
bool DatabaseManager::openDatabaseFolder() {
    if (!m_db.isOpen()) {
        qWarning() << "Database is not open.";
        return false;
    }

    const QString dbFile = m_db.databaseName();
    if (dbFile.isEmpty()) {
        qWarning() << "Database file path is empty.";
        return false;
    }

    // 打开包含 dailyup.db 的目录
    const QUrl folderUrl = QUrl::fromLocalFile(QFileInfo(dbFile).absolutePath());
    if (!QDesktopServices::openUrl(folderUrl)) {
        qWarning() << "Failed to open folder:" << folderUrl;
        return false;
    }
    return true;
}