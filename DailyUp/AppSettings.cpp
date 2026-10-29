#include "AppSettings.h"

#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QStringList>

namespace {
const QStringList kThemes = { QStringLiteral("mist"),
                              QStringLiteral("mint"),
                              QStringLiteral("sunset"),
                              QStringLiteral("pink"),
                              QStringLiteral("gold"),
                              QStringLiteral("custom") };
const QString kDefaultCustomAccent = QStringLiteral("#D95C82");
constexpr int kDefaultWidth = 360;
constexpr int kDefaultHeight = 640;
} // namespace

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
}

QColor AppSettings::customAccent() const
{
    const QColor color(m_settings.value(QStringLiteral("customAccent"), kDefaultCustomAccent).toString());
    return color.isValid() ? color : QColor(kDefaultCustomAccent);
}

void AppSettings::setCustomAccent(const QColor &color)
{
    if (!color.isValid() || customAccent() == color)
        return;

    m_settings.setValue(QStringLiteral("customAccent"), color.name(QColor::HexRgb));
    m_settings.sync();
    emit customAccentChanged();
}

QString AppSettings::theme() const
{
    const QString value = m_settings.value(QStringLiteral("theme"), QStringLiteral("mist")).toString();
    return isValidTheme(value) ? value : QStringLiteral("mist");
}

void AppSettings::setTheme(const QString &theme)
{
    if (!isValidTheme(theme) || theme == this->theme())
        return;

    m_settings.setValue(QStringLiteral("theme"), theme);
    m_settings.sync();
    emit themeChanged();
}

QVariantMap AppSettings::windowGeometry() const
{
    QVariantMap geometry;
    geometry.insert(QStringLiteral("width"),
                    m_settings.value(QStringLiteral("window/width"), kDefaultWidth).toInt());
    geometry.insert(QStringLiteral("height"),
                    m_settings.value(QStringLiteral("window/height"), kDefaultHeight).toInt());
    return geometry;
}

void AppSettings::saveWindowGeometry(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    if (m_settings.value(QStringLiteral("window/width")).toInt() == width
        && m_settings.value(QStringLiteral("window/height")).toInt() == height) {
        return;
    }

    m_settings.setValue(QStringLiteral("window/width"), width);
    m_settings.setValue(QStringLiteral("window/height"), height);
    emit windowGeometryChanged();
}

QPoint AppSettings::cursorPosition() const
{
    return QCursor::pos();
}

QRect AppSettings::availableGeometryAt(int x, int y) const
{
    QScreen *screen = QGuiApplication::screenAt(QPoint(x, y));
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    return screen ? screen->availableGeometry() : QRect();
}


bool AppSettings::isValidTheme(const QString &theme)
{
    return kThemes.contains(theme);
}