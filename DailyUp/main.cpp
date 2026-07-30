#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDebug>
#include <QQmlContext> // 引入此头文件
#include "databasemanager.h"
#include "TaskManager.h" // 引入模型头文件

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // 1. 初始化数据库
    if (!DatabaseManager::instance().initDatabase()) {
        qCritical() << "Fatal Error: Database initialization failed.";
        return -1;
    }

    // ================= 新增：启动时立即执行跨日继承 =================
    // 确保在 QML 实例化 TaskManager 前，数据库中今天的数据已经准备完毕
    DatabaseManager::instance().rolloverTasksForToday();
    // ==============================================================

    // 2. 将 TaskManager 注册到 QML 系统中
    // 包名为 DailyUp，版本 1.0，QML中的类名为 TaskManager
    qmlRegisterType<TaskManager>("DailyUp", 1, 0, "TaskManager");

    QQmlApplicationEngine engine;

    // 3. 监听引擎对象创建失败的信号，安全退出程序
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // 4. 加载主 QML 界面 (Qt6 现代写法)
    // 注意：此处的 "DailyUp" 需与你 CMakeLists.txt 中 qt_add_qml_module 的 URI 保持一致
    // "Main" 对应你的主 QML 文件 (无需加 .qml 后缀)
    engine.loadFromModule("DailyUp", "Main");

    return app.exec();
}