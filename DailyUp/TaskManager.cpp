#include "TaskManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

TaskManager::TaskManager(QObject *parent) : QObject(parent) {
    initDatabase();
}

TaskManager::~TaskManager() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool TaskManager::initDatabase() {
    m_db = QSqlDatabase::addDatabase("QSQLITE");

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(path);
    }

    QString dbPath = path + QDir::separator() + "dailyup.db";
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qDebug() << "Error: Failed to connect database." << m_db.lastError().text();
        return false;
    }

    QSqlQuery query;
    QString createTableSql = "CREATE TABLE IF NOT EXISTS tasks ("
                             "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                             "task_date TEXT NOT NULL, "
                             "task_text TEXT NOT NULL, "
                             "is_completed INTEGER NOT NULL);";

    if (!query.exec(createTableSql)) {
        qDebug() << "Error: Failed to create table." << query.lastError().text();
        return false;
    }

    qDebug() << "Database initialized successfully at:" << dbPath;
    return true;
}

QVariantList TaskManager::getTasksByDate(const QString &dateStr) {
    QVariantList taskList;
    QSqlQuery query;
    query.prepare("SELECT id, task_text, is_completed FROM tasks WHERE task_date = :dateStr");
    query.bindValue(":dateStr", dateStr);

    if (query.exec()) {
        while (query.next()) {
            QVariantMap task;
            task["id"] = query.value("id");
            task["taskText"] = query.value("task_text");
            task["isCompleted"] = query.value("is_completed").toBool();
            taskList.append(task);
        }
    } else {
        qDebug() << "Error: Failed to query tasks." << query.lastError().text();
    }

    return taskList;
}

bool TaskManager::addTask(const QString &dateStr, const QString &taskText) {
    QSqlQuery query;
    query.prepare("INSERT INTO tasks (task_date, task_text, is_completed) VALUES (:dateStr, :taskText, 0)");
    query.bindValue(":dateStr", dateStr);
    query.bindValue(":taskText", taskText);

    if (!query.exec()) {
        qDebug() << "Error: Failed to add task." << query.lastError().text();
        return false;
    }
    return true;
}

bool TaskManager::updateTaskStatus(int id, bool isCompleted) {
    QSqlQuery query;
    query.prepare("UPDATE tasks SET is_completed = :isCompleted WHERE id = :id");
    query.bindValue(":isCompleted", isCompleted ? 1 : 0);
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "Error: Failed to update task status." << query.lastError().text();
        return false;
    }
    return true;
}

bool TaskManager::updateTaskText(int id, const QString &taskText) {
    QSqlQuery query;
    query.prepare("UPDATE tasks SET task_text = :taskText WHERE id = :id");
    query.bindValue(":taskText", taskText);
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "Error: Failed to update task text." << query.lastError().text();
        return false;
    }
    return true;
}

bool TaskManager::deleteTask(int id) {
    QSqlQuery query;
    query.prepare("DELETE FROM tasks WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "Error: Failed to delete task." << query.lastError().text();
        return false;
    }
    return true;
}