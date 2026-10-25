#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QTimer>
#include <QDate>
#include "databasemanager.h"

class TaskManager : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString currentDate READ currentDate WRITE setCurrentDate NOTIFY currentDateChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countsChanged)
    Q_PROPERTY(int completedCount READ completedCount NOTIFY countsChanged)

public:
    enum TaskRoles {
        IdRole = Qt::UserRole + 1,
        DateRole,
        ContentRole,
        IsCompletedRole,
        IsPinnedRole
    };

    explicit TaskManager(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString currentDate() const { return m_currentDate; }
    void setCurrentDate(const QString &date);

    int totalCount() const { return m_tasks.size(); }
    int completedCount() const;

    Q_INVOKABLE void loadTasks(const QString &date);
    Q_INVOKABLE void addTask(const QString &date, const QString &content);
    Q_INVOKABLE void toggleTaskStatus(int index);
    Q_INVOKABLE void deleteTask(int index);
    Q_INVOKABLE void updateTaskContent(int index, const QString &content);
    Q_INVOKABLE void toggleTaskPin(int index);
    Q_INVOKABLE void openDatabaseFolder();
    Q_INVOKABLE QString contentAt(int index) const;

signals:
    void currentDateChanged();
    void dateRollover();
    void countsChanged();
    void errorOccurred(const QString &message);

private slots:
    // 改为更安全的日期检查槽函数
    void checkDateRollover();

private:
    QList<TaskData> m_tasks;
    QString m_currentDate;

    // 定时器与日期追踪
    QTimer* m_checkTimer;
    QDate m_lastCheckedDate;
};