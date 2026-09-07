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

    // 获取系统标准应用数据目录 (跨平台支持：Windows/macOS/Linux)
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 设置数据库文件路径
    QString dbPath = dataDir + "/dailyup.db";
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
    query.prepare("SELECT id, date, content, is_completed, is_pinned FROM Tasks WHERE date = :date ORDER BY is_pinned DESC, id ASC");
    query.bindValue(":date", date);

    if (query.exec()) {
        while (query.next()) {
            TaskData task;
            task.id = query.value("id").toInt();
            task.date = query.value("date").toString();
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

    // 3. 核心逻辑：将最近一天的任务复制到今天！
    // 强制把 is_completed 设为 0。is_pinned (置顶状态) 我们让它继承昨天的设置。
    QSqlQuery copyQuery;
    copyQuery.prepare(R"(
        INSERT INTO Tasks (date, content, is_completed, is_pinned)
        SELECT :today, content, 0, is_pinned
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