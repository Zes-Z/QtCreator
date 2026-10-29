#pragma once

#include <QColor>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSettings>
#include <QString>
#include <QVariantMap>

// QML 单例：DailyUp 的本地设置（macOS 下写入 ~/Library/Preferences）。
// 负责主题选择与窗口尺寸记忆，避免在 QML 里直接依赖平台 API。
class AppSettings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QColor customAccent READ customAccent WRITE setCustomAccent NOTIFY customAccentChanged)
    Q_PROPERTY(QVariantMap windowGeometry READ windowGeometry NOTIFY windowGeometryChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString theme() const;
    void setTheme(const QString &theme);

    QColor customAccent() const;
    void setCustomAccent(const QColor &color);

    QVariantMap windowGeometry() const;
    Q_INVOKABLE void saveWindowGeometry(int width, int height);

    // 拖动窗口与边界回弹使用：全局鼠标位置 + 鼠标所在屏幕的可用区域
    Q_INVOKABLE QPoint cursorPosition() const;
    Q_INVOKABLE QRect availableGeometryAt(int x, int y) const;


signals:
    void themeChanged();
    void customAccentChanged();
    void windowGeometryChanged();

private:
    static bool isValidTheme(const QString &theme);

    mutable QSettings m_settings;
};