#include "services/AuthService.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>

namespace litapp {

AuthService::AuthService(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    QSettings settings;
    m_token = settings.value(QStringLiteral("auth/token")).toString();
    m_email = settings.value(QStringLiteral("auth/email")).toString();
}

void AuthService::login(const QString &email, const QString &password)
{
    // TODO: 接入真实认证 API。
    // 当前为占位实现：任意非空邮箱/密码视为登录成功，并生成本地模拟 Token。
    Q_UNUSED(password)

    if (email.isEmpty()) {
        emit loginFinished(false, email, QStringLiteral("邮箱不能为空"));
        return;
    }

    const QString token = QStringLiteral("dev-token-") + email;
    persistSession(token, email);
    emit loginFinished(true, email, QString());
}

void AuthService::registerAccount(const QString &email, const QString &password)
{
    // TODO: 接入真实注册 API。
    Q_UNUSED(password)

    if (email.isEmpty()) {
        emit registerFinished(false, email, QStringLiteral("邮箱不能为空"));
        return;
    }

    const QString token = QStringLiteral("dev-token-") + email;
    persistSession(token, email);
    emit registerFinished(true, email, QString());
}

void AuthService::logout()
{
    clearSession();
    emit logoutFinished();
}

bool AuthService::isLoggedIn() const
{
    return !m_token.isEmpty();
}

QString AuthService::token() const
{
    return m_token;
}

QString AuthService::email() const
{
    return m_email;
}

void AuthService::persistSession(const QString &token, const QString &email)
{
    m_token = token;
    m_email = email;

    QSettings settings;
    settings.setValue(QStringLiteral("auth/token"), m_token);
    settings.setValue(QStringLiteral("auth/email"), m_email);
}

void AuthService::clearSession()
{
    m_token.clear();
    m_email.clear();

    QSettings settings;
    settings.remove(QStringLiteral("auth"));
}

} // namespace litapp
