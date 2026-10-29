#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QList>

// 定义一个结构体来映射数据库中的一行任务数据
struct TaskData {
    int id;
    QString content;
    bool isCompleted;
    bool isPinned;
};

class DatabaseManager : public QObject {
    Q_OBJECT

public:
    // 单例模式，方便在 Model 和全局中调用
    static DatabaseManager& instance();

    // 初始化数据库连接与表结构
    bool initDatabase();

    // 核心 CRUD 操作
    QList<TaskData> getTasksByDate(const QString& date);
    bool addTask(const QString& date, const QString& content);
    bool deleteTask(int id);
    bool updateTaskStatus(int id, bool isCompleted);
    bool updateTaskContent(int id, const QString& content);
    bool toggleTaskPin(int id, bool isPinned);

    // ================= 新增 =================
    // 检查并执行跨日任务继承
    bool rolloverTasksForToday();
    // =======================================

    // 在系统资源管理器中打开“数据库文件所在”目录（用于查看/备份数据）
    Q_INVOKABLE bool openDatabaseFolder();

private:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    // 禁用拷贝构造和赋值操作以确保单例的安全
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    QSqlDatabase m_db;
};