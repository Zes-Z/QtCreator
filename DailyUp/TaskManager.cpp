#include "TaskManager.h"

TaskManager::TaskManager(QObject *parent) : QAbstractListModel(parent) {
}

int TaskManager::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_tasks.count();
}

QVariant TaskManager::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_tasks.count())
        return QVariant();

    const TaskData &task = m_tasks[index.row()];

    switch (role) {
    case IdRole: return task.id;
    case DateRole: return task.date;
    case ContentRole: return task.content;
    case IsCompletedRole: return task.isCompleted;
    case IsPinnedRole: return task.isPinned;
    default: return QVariant();
    }
}

QHash<int, QByteArray> TaskManager::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[IdRole] = "taskId";
    roles[DateRole] = "taskDate";
    roles[ContentRole] = "taskContent";
    roles[IsCompletedRole] = "isCompleted";
    roles[IsPinnedRole] = "isPinned";
    return roles;
}

void TaskManager::setCurrentDate(const QString &date) {
    if (m_currentDate != date) {
        m_currentDate = date;
        emit currentDateChanged();
        loadTasks(date);
    }
}

// 根据日期加载任务（实现每天数据的严格独立隔离）
void TaskManager::loadTasks(const QString &date) {
    m_currentDate = date;

    // 通知 QML 模型即将重置
    beginResetModel();
    // 仅从数据库拉取匹配当前日期的任务列表
    m_tasks = DatabaseManager::instance().getTasksByDate(date);
    // 通知 QML 模型重置完毕，自动渲染该日期的专属任务
    endResetModel();
}

// 添加新任务到指定日期
void TaskManager::addTask(const QString &date, const QString &content) {
    QString targetDate = date.isEmpty() ? m_currentDate : date;
    if (content.trimmed().isEmpty()) return;

    if (DatabaseManager::instance().addTask(targetDate, content)) {
        // 如果当前正好在看这个日期，则直接刷新列表
        if (targetDate == m_currentDate) {
            loadTasks(m_currentDate);
        }
    }
}

// 切换特定任务的完成状态
void TaskManager::toggleTaskStatus(int index) {
    if (index < 0 || index >= m_tasks.count()) return;

    TaskData &task = m_tasks[index];
    bool newStatus = !task.isCompleted;

    if (DatabaseManager::instance().updateTaskStatus(task.id, newStatus)) {
        task.isCompleted = newStatus;
        QModelIndex modelIndex = createIndex(index, 0);
        emit dataChanged(modelIndex, modelIndex, {IsCompletedRole});
    }
}

// 删除特定任务
void TaskManager::deleteTask(int index) {
    if (index < 0 || index >= m_tasks.count()) return;

    int taskId = m_tasks[index].id;
    if (DatabaseManager::instance().deleteTask(taskId)) {
        beginRemoveRows(QModelIndex(), index, index);
        m_tasks.removeAt(index);
        endRemoveRows();
    }
}

// 更新任务内容
void TaskManager::updateTaskContent(int index, const QString &content) {
    if (index < 0 || index >= m_tasks.count() || content.trimmed().isEmpty()) return;

    TaskData &task = m_tasks[index];
    if (DatabaseManager::instance().updateTaskContent(task.id, content)) {
        task.content = content;
        QModelIndex modelIndex = createIndex(index, 0);
        emit dataChanged(modelIndex, modelIndex, {ContentRole});
    }
}

// 切换置顶状态
void TaskManager::toggleTaskPin(int index) {
    if (index < 0 || index >= m_tasks.count()) return;

    TaskData &task = m_tasks[index];
    bool newPinStatus = !task.isPinned;

    if (DatabaseManager::instance().toggleTaskPin(task.id, newPinStatus)) {
        // 置顶状态改变后，由于排序会变，最稳妥的做法是重新加载当前日期任务
        loadTasks(m_currentDate);
    }
}