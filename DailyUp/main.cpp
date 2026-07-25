#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include "TaskManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    // 实例化任务管理器后端
    TaskManager taskManager;

    // 将后端对象注册到 QML 上下文中，供前端调用
    engine.rootContext()->setContextProperty("taskManager", &taskManager);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("DailyUp", "Main");

    return QGuiApplication::exec();
}