#include "TaskManager.h"
#include <QDebug>
#include <QDate>

TaskManager::TaskManager(QObject *parent) : QAbstractListModel(parent) {
    // 记录启动时的日期
    m_lastCheckedDate = QDate::currentDate();

    // 初始化每分钟轮询的定时器，用于安全检测跨天（避免因电脑休眠导致单次长定时器失效）
    m_checkTimer = new QTimer(this);
    m_checkTimer->setInterval(60000); // 60000 毫秒 = 1 分钟
    connect(m_checkTimer, &QTimer::timeout, this, &TaskManager::checkDateRollover);
    m_checkTimer->start();
}


// 安全可靠的跨日检测与结转逻辑
void TaskManager::checkDateRollover() {

    QDate currentDate = QDate::currentDate();

    // 如果系统的当前日期大于上一次记录的日期，说明跨天了
    if (currentDate > m_lastCheckedDate) {

        qDebug() << "检测到跨天，执行零点结转。旧日期:"
                 << m_lastCheckedDate.toString()
                 << "新日期:"
                 << currentDate.toString();


        // 1. 让数据库处理跨日结转（如复制昨天任务等）
        DatabaseManager::instance().rolloverTasksForToday();


        // 2. 更新记录的日期
        m_lastCheckedDate = currentDate;


        // 3. 自动切换到今天
        QString newTodayStr =
            currentDate.toString("yyyy-MM-dd");

        loadTasks(newTodayStr);


        // 4. 通知 QML 日期栏刷新
        emit dateRollover();
    }
}


int TaskManager::completedCount() const {
    int completed = 0;
    for (const TaskData &task : m_tasks) {
        if (task.isCompleted)
            ++completed;
    }
    return completed;
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

    case ContentRole:
        return task.content;

    case IsCompletedRole:
        return task.isCompleted;

    case IsPinnedRole:
        return task.isPinned;

    default:
        return QVariant();
    }
}


QHash<int, QByteArray> TaskManager::roleNames() const {

    QHash<int, QByteArray> roles;

    roles[ContentRole] = "taskContent";
    roles[IsCompletedRole] = "isCompleted";
    roles[IsPinnedRole] = "isPinned";

    return roles;
}




// 根据日期加载任务（实现每天数据的严格独立隔离）
void TaskManager::loadTasks(const QString &date) {

    m_currentDate = date;


    // 通知 QML 模型即将重置
    beginResetModel();


    // 仅从数据库拉取匹配当前日期的任务列表
    m_tasks =
        DatabaseManager::instance().getTasksByDate(date);


    // 通知 QML 模型重置完毕，自动渲染该日期的专属任务
    endResetModel();
    emit countsChanged();
}


// 添加新任务到指定日期
void TaskManager::addTask(const QString &date, const QString &content) {

    QString targetDate =
        date.isEmpty() ? m_currentDate : date;


    if (content.trimmed().isEmpty())
        return;


    if (!DatabaseManager::instance().addTask(targetDate, content)) {
        emit errorOccurred(QStringLiteral("任务没有保存：数据库不可写。"));
        return;
    }

    // 如果当前正好在看这个日期，则直接刷新列表
    if (targetDate == m_currentDate)
        loadTasks(m_currentDate);
}


// 切换特定任务的完成状态
void TaskManager::toggleTaskStatus(int index) {

    if (index < 0 || index >= m_tasks.count())
        return;


    TaskData &task = m_tasks[index];

    bool newStatus =
        !task.isCompleted;


    if (!DatabaseManager::instance().updateTaskStatus(task.id, newStatus)) {
        emit errorOccurred(QStringLiteral("状态没有保存：数据库不可写。"));
        return;
    }

    task.isCompleted = newStatus;

    QModelIndex modelIndex =
        createIndex(index, 0);

    emit dataChanged(
        modelIndex,
        modelIndex,
        {IsCompletedRole}
        );
    emit countsChanged();
}


// 删除特定任务
void TaskManager::deleteTask(int index) {

    if (index < 0 || index >= m_tasks.count())
        return;


    int taskId =
        m_tasks[index].id;


    if (!DatabaseManager::instance().deleteTask(taskId)) {
        emit errorOccurred(QStringLiteral("任务没有删除：数据库不可写。"));
        return;
    }

    beginRemoveRows(
        QModelIndex(),
        index,
        index
        );

    m_tasks.removeAt(index);

    endRemoveRows();
    emit countsChanged();
}


// 更新任务内容
void TaskManager::updateTaskContent(int index, const QString &content) {

    if (index < 0 ||
        index >= m_tasks.count() ||
        content.trimmed().isEmpty())
        return;


    TaskData &task =
        m_tasks[index];


    if (!DatabaseManager::instance().updateTaskContent(task.id, content)) {
        emit errorOccurred(QStringLiteral("修改没有保存：数据库不可写。"));
        return;
    }

    task.content = content;

    QModelIndex modelIndex =
        createIndex(index, 0);

    emit dataChanged(
        modelIndex,
        modelIndex,
        {ContentRole}
        );
}


// 切换置顶状态
void TaskManager::toggleTaskPin(int index) {

    if (index < 0 || index >= m_tasks.count())
        return;


    TaskData &task =
        m_tasks[index];


    bool newPinStatus =
        !task.isPinned;


    if (!DatabaseManager::instance().toggleTaskPin(task.id, newPinStatus)) {
        emit errorOccurred(QStringLiteral("置顶没有保存：数据库不可写。"));
        return;
    }

    // 置顶状态改变后，由于排序会变，重新加载当前日期任务
    loadTasks(m_currentDate);
}

QString TaskManager::contentAt(int index) const {
    if (index < 0 || index >= m_tasks.size())
        return {};
    return m_tasks.at(index).content;
}

// 打开数据库文件所在目录（供 QML 按钮调用）
void TaskManager::openDatabaseFolder() {
    DatabaseManager::instance().openDatabaseFolder();
}