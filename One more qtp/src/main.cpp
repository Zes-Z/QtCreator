#include "models/FileListModel.h"
#include "services/AuthService.h"
#include "services/FileService.h"
#include "services/SyncService.h"
#include "services/WebDavClient.h"

#include <QGuiApplication>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("LitApp");
    app.setOrganizationName("LitApp");
    QQuickStyle::setStyle(QStringLiteral("Basic"));

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

    litapp::AuthService authService;
    litapp::WebDavClient webDavClient;
    litapp::SyncService syncService(&webDavClient);
    litapp::FileListModel fileListModel;
    litapp::FileService fileService;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("authService", &authService);
    engine.rootContext()->setContextProperty("webDavClient", &webDavClient);
    engine.rootContext()->setContextProperty("syncService", &syncService);
    engine.rootContext()->setContextProperty("fileListModel", &fileListModel);
    engine.rootContext()->setContextProperty("fileService", &fileService);

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
