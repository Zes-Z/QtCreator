#include <QCommandLineParser>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSettings>
#include <QStyleHints>
#include <QTimer>
#include <QDebug>

#include "AppSettings.h"
#include "TaskManager.h"
#include "databasemanager.h"

namespace {

QSize parseSize(const QString &value)
{
    const QStringList parts = value.split(QLatin1Char('x'), Qt::SkipEmptyParts);
    if (parts.size() != 2)
        return {};
    bool okWidth = false;
    bool okHeight = false;
    const int width = parts.at(0).toInt(&okWidth);
    const int height = parts.at(1).toInt(&okHeight);
    if (!okWidth || !okHeight || width < 200 || height < 200)
        return {};
    return QSize(width, height);
}

// 仅用于截图 / 演示，不写入真实数据目录时使用 --demo 触发。
void seedDemoTasks()
{
    auto &db = DatabaseManager::instance();
    const QString today = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    if (!db.getTasksByDate(today).isEmpty())
        return;

    db.addTask(today, QStringLiteral("喝一杯温水"));
    db.addTask(today, QStringLiteral("读 20 页书"));
    db.addTask(today, QStringLiteral("完成今天最重要的一件事"));
    db.addTask(today, QStringLiteral("晚饭后散步 20 分钟"));

    const QList<TaskData> tasks = db.getTasksByDate(today);
    if (tasks.size() >= 4) {
        db.updateTaskStatus(tasks.at(0).id, true);
        db.updateTaskStatus(tasks.at(1).id, true);
        db.toggleTaskPin(tasks.at(3).id, true);
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("DailyUp"));
    app.setApplicationDisplayName(QStringLiteral("DailyUp"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("Zes"));
    app.setOrganizationDomain(QStringLiteral("zes.com"));

#if defined(Q_OS_MACOS) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // 界面为固定浅色设计，避免系统深色模式影响原生控件（滚动条、菜单等）。
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("DailyUp - 每日打卡 / 任务清单"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                              QStringLiteral("渲染后保存窗口截图并退出。"),
                                              QStringLiteral("path"));
    const QCommandLineOption sizeOption(QStringLiteral("size"),
                                        QStringLiteral("指定窗口尺寸，例如 360x640。"),
                                        QStringLiteral("WxH"));
    const QCommandLineOption themeOption(QStringLiteral("theme"),
                                         QStringLiteral("启动主题：mist / mint / sunset / pink / gold / custom。"),
                                         QStringLiteral("name"));
    const QCommandLineOption demoOption(QStringLiteral("demo"),
                                        QStringLiteral("填充演示任务（配合测试数据目录使用）。"));
    const QCommandLineOption delayOption(QStringLiteral("delay"),
                                         QStringLiteral("截图前等待毫秒数，默认 900。"),
                                         QStringLiteral("ms"));
    const QCommandLineOption openOption(QStringLiteral("open"),
                                        QStringLiteral("启动后打开界面：theme / add / edit。"),
                                        QStringLiteral("target"));
    const QCommandLineOption switchThemeOption(QStringLiteral("switch-theme"),
                                               QStringLiteral("启动后切换到指定主题（验证实时换肤）。"),
                                               QStringLiteral("name"));
    parser.addOption(screenshotOption);
    parser.addOption(sizeOption);
    parser.addOption(themeOption);
    parser.addOption(demoOption);
    parser.addOption(delayOption);
    parser.addOption(openOption);
    parser.addOption(switchThemeOption);
    parser.process(app);

    if (parser.isSet(themeOption)) {
        QSettings settings;
        settings.setValue(QStringLiteral("theme"), parser.value(themeOption));
        settings.sync();
    }

    if (!DatabaseManager::instance().initDatabase()) {
        qCritical() << "Fatal Error: Database initialization failed.";
        return -1;
    }

    DatabaseManager::instance().rolloverTasksForToday();
    if (parser.isSet(demoOption))
        seedDemoTasks();

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // 显式注册设置单例：Qt 6.12 的自动注册对带 QObject 父构造的类不够稳定。
    auto *appSettings = new AppSettings(&app);
    qmlRegisterSingletonInstance("DailyUp", 1, 0, "AppSettings", appSettings);
    qmlRegisterType<TaskManager>("DailyUp", 1, 0, "TaskManager");
    engine.loadFromModule("DailyUp", "Main");

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!window)
        return -1;

    if (parser.isSet(sizeOption)) {
        const QSize size = parseSize(parser.value(sizeOption));
        if (size.isValid())
            window->resize(size);
    }

    if (QScreen *screen = window->screen()) {
        const QRect available = screen->availableGeometry();
        window->setPosition(available.center()
                            - QPoint(window->width() / 2, window->height() / 2));
    }

    if (parser.isSet(switchThemeOption)) {
        const QString key = parser.value(switchThemeOption);
        QTimer::singleShot(450, appSettings, [appSettings, key]() {
            appSettings->setTheme(key);
        });
    }

    if (parser.isSet(openOption)) {
        const QString target = parser.value(openOption);
        QTimer::singleShot(300, window, [window, target]() {
            const char *method = nullptr;
            if (target == QLatin1String("theme"))
                method = "showThemePicker";
            else if (target == QLatin1String("add"))
                method = "showAddDialog";
            else if (target == QLatin1String("edit"))
                method = "showEditDialog";
            if (method) {
                const bool ok = QMetaObject::invokeMethod(window, method);
                qInfo() << "open" << target << "->" << method << ok;
            }
        });
    }

    if (parser.isSet(screenshotOption)) {
        const QString path = parser.value(screenshotOption);
        const int delay = parser.isSet(delayOption)
                              ? qMax(200, parser.value(delayOption).toInt())
                              : 900;
        QTimer::singleShot(delay, &app, [&app, window, path]() {
            const QImage frame = window->grabWindow();
            if (frame.isNull() || !frame.save(path))
                qWarning() << "Failed to save screenshot to" << path;
            else
                qInfo() << "Screenshot saved to" << path;
            app.quit();
        });
    }

    return app.exec();
}