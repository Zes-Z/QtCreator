#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;

namespace litapp {

// 使用邮箱 + 密码登录/注册，并管理本地会话 Token。
class AuthService : public QObject
{
    Q_OBJECT
public:
    explicit AuthService(QObject *parent = nullptr);

    Q_INVOKABLE void login(const QString &email, const QString &password);
    Q_INVOKABLE void registerAccount(const QString &email, const QString &password);
    Q_INVOKABLE void logout();

    Q_INVOKABLE bool isLoggedIn() const;
    Q_INVOKABLE QString token() const;
    Q_INVOKABLE QString email() const;

signals:
    void loginFinished(bool ok, const QString &email, const QString &error);
    void registerFinished(bool ok, const QString &email, const QString &error);
    void logoutFinished();

private:
    void persistSession(const QString &token, const QString &email);
    void clearSession();

    QNetworkAccessManager *m_network = nullptr;
    QString m_token;
    QString m_email;
};

} // namespace litapp
