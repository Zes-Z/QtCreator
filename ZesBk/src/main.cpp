#include "models/FileListModel.h"
#include "services/AuthService.h"
#include "services/FileService.h"
#include "services/SyncService.h"
#include "services/WebDavClient.h"

#include <QDir>
#include <QGuiApplication>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QStyleHints>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("LitApp");
    app.setOrganizationName("LitApp");
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    /*
     * 强制浅色主题。
     *
     * Qt 6 默认跟随系统配色：系统处于深色模式时，
     * 控件、菜单、对话框、弹窗都会自动变暗。
     *
     * 必须在第一个窗口显示之前设置。
     */
    if (QGuiApplication::styleHints()) {
        QGuiApplication::styleHints()->setColorScheme(
            Qt::ColorScheme::Light);
    }

    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window, QColor("#ffffff"));
    lightPalette.setColor(QPalette::WindowText, QColor("#222222"));
    lightPalette.setColor(QPalette::Base, QColor("#ffffff"));
    lightPalette.setColor(QPalette::AlternateBase, QColor("#f4f6f8"));
    lightPalette.setColor(QPalette::Text, QColor("#222222"));
    lightPalette.setColor(QPalette::Button, QColor("#e8e8e8"));
    lightPalette.setColor(QPalette::ButtonText, QColor("#222222"));
    lightPalette.setColor(QPalette::Highlight, QColor("#3080d0"));
    lightPalette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    app.setPalette(lightPalette);

    /*
     * 元数据数据库路径。
     *
     * 统一放在应用数据目录下，
     * 避免因为工作目录不同而产生多个数据库。
     */
    const QString appDataRoot =
        QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);

    QDir().mkpath(appDataRoot);

    const QString databasePath =
        QDir(appDataRoot).filePath(
            QStringLiteral("litapp_metadata.db"));


    litapp::AuthService authService;
    litapp::WebDavClient webDavClient;
    litapp::SyncService syncService(&webDavClient);
    litapp::FileListModel fileListModel;
    litapp::FileService fileService;

    /*
     * 两个服务必须使用同一个数据库。
     */
    syncService.setDatabasePath(databasePath);
    fileService.setDatabasePath(databasePath);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("authService", &authService);
    engine.rootContext()->setContextProperty("webDavClient", &webDavClient);
    engine.rootContext()->setContextProperty("syncService", &syncService);
    engine.rootContext()->setContextProperty("fileListModel", &fileListModel);
    engine.rootContext()->setContextProperty("fileService", &fileService);
    engine.rootContext()->setContextProperty("appDatabasePath", databasePath);

#ifdef LITAPP_HAS_QTPDF
    engine.rootContext()->setContextProperty("hasPdfSupport", true);
#else
    engine.rootContext()->setContextProperty("hasPdfSupport", false);
#endif

    engine.loadFromModule("LitApp", "Main");
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
