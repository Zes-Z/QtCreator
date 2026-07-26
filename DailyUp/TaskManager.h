#pragma once

#include <QAbstractListModel>
#include <QString>
#include "databasemanager.h"

class TaskManager : public QAbstractListModel {
    Q_OBJECT
    // 注册当前选中的日期属性，方便 QML 绑定或获取
    Q_PROPERTY(QString currentDate READ currentDate WRITE setCurrentDate NOTIFY currentDateChanged)

public:
    // 定义 QML 中可访问的 Role (属性名)
    enum TaskRoles {
        IdRole = Qt::UserRole + 1,
        DateRole,
        ContentRole,
        IsCompletedRole,
        IsPinnedRole
    };

    explicit TaskManager(QObject *parent = nullptr);

    // 必须重写的 QAbstractListModel 接口
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString currentDate() const { return m_currentDate; }
    void setCurrentDate(const QString &date);

    // 暴露给 QML 调用的接口 (使用 Q_INVOKABLE 宏)
    Q_INVOKABLE void loadTasks(const QString &date);
    Q_INVOKABLE void addTask(const QString &date, const QString &content);
    Q_INVOKABLE void toggleTaskStatus(int index);
    Q_INVOKABLE void deleteTask(int index);
    Q_INVOKABLE void updateTaskContent(int index, const QString &content);
    Q_INVOKABLE void toggleTaskPin(int index);

signals:
    void currentDateChanged();

private:
    QList<TaskData> m_tasks;
    QString m_currentDate;
};