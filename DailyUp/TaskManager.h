#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>

class TaskManager : public QObject {
    Q_OBJECT
public:
    explicit TaskManager(QObject *parent = nullptr);
    ~TaskManager();

    // 供 QML 调用的核心方法
    Q_INVOKABLE bool initDatabase();
    Q_INVOKABLE QVariantList getTasksByDate(const QString &dateStr);
    Q_INVOKABLE bool addTask(const QString &dateStr, const QString &taskText);
    Q_INVOKABLE bool updateTaskStatus(int id, bool isCompleted);
    Q_INVOKABLE bool updateTaskText(int id, const QString &taskText);
    Q_INVOKABLE bool deleteTask(int id);

private:
    QSqlDatabase m_db;
};