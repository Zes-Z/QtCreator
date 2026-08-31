#include "services/WebDavClient.h"

#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QXmlStreamReader>

namespace litapp {

WebDavClient::WebDavClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

void WebDavClient::setServer(const QString &serverUrl,
                             const QString &username,
                             const QString &password)
{
    m_serverUrl = QUrl(serverUrl);
    m_username = username;
    m_password = password;

    QSettings settings;
    settings.setValue(QStringLiteral("webdav/serverUrl"), serverUrl);
    settings.setValue(QStringLiteral("webdav/username"), username);
    settings.setValue(QStringLiteral("webdav/password"), password);
}

void WebDavClient::setToken(const QString &token)
{
    m_token = token;
}

void WebDavClient::restoreSettings()
{
    QSettings settings;
    m_serverUrl = QUrl(settings.value(QStringLiteral("webdav/serverUrl")).toString());
    m_username = settings.value(QStringLiteral("webdav/username")).toString();
    m_password = settings.value(QStringLiteral("webdav/password")).toString();
}

QString WebDavClient::serverUrl() const
{
    return m_serverUrl.toString();
}

QString WebDavClient::username() const
{
    return m_username;
}

QString WebDavClient::password() const
{
    return m_password;
}

static QUrl urlForPath(const QUrl &baseUrl, const QString &remotePath)
{
    if (remotePath.isEmpty() || remotePath == QStringLiteral("/")) {
        return baseUrl;
    }

    QString base = baseUrl.toString();
    if (!base.endsWith(QLatin1Char('/'))) {
        base += QLatin1Char('/');
    }

    QString path = remotePath;
    while (path.startsWith(QLatin1Char('/'))) {
        path.remove(0, 1);
    }

    return QUrl(base + path);
}

QNetworkReply *WebDavClient::sendRequest(const QString &method, const QUrl &url, const QByteArray &body)
{
    QNetworkRequest request(url);
    request.setRawHeader("Depth", "1");

    if (!m_username.isEmpty()) {
        // 简易 Basic Auth；若使用 Token，可在此改为 Authorization: Bearer
        const QByteArray credentials = (m_username + ":" + m_password).toUtf8().toBase64();
        request.setRawHeader("Authorization", "Basic " + credentials);
    } else if (!m_token.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    }

    if (method == QStringLiteral("PROPFIND")) {
        const QByteArray propfindBody =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
            "<d:propfind xmlns:d=\"DAV:\">"
            "<d:prop>"
            "<d:resourcetype/>"
            "<d:getcontentlength/>"
            "<d:displayname/>"
            "</d:prop>"
            "</d:propfind>";
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/xml"));
        request.setRawHeader("Depth", "1");
        return m_network->sendCustomRequest(request, "PROPFIND", propfindBody);
    }
    if (method == QStringLiteral("MKCOL")) {
        return m_network->sendCustomRequest(request, "MKCOL", body);
    }
    if (method == QStringLiteral("DELETE")) {
        return m_network->sendCustomRequest(request, "DELETE", body);
    }
    if (method == QStringLiteral("PUT")) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
        return m_network->put(request, body);
    }
    if (method == QStringLiteral("GET")) {
        return m_network->get(request);
    }
    return nullptr;
}

void WebDavClient::listDirectory(const QString &remotePath)
{
    const QString path = remotePath.isEmpty() ? QStringLiteral("/") : remotePath;
    QNetworkReply *reply = sendRequest(QStringLiteral("PROPFIND"), urlForPath(m_serverUrl, path));
    if (!reply) {
        emit directoryListed(path, {}, QStringLiteral("无法创建网络请求"));
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, path]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit directoryListed(path, {}, reply->errorString());
            return;
        }

        QVariantList entries;
        QXmlStreamReader xml(reply->readAll());

        QString currentHref;
        bool currentIsDirectory = false;
        qint64 currentSize = 0;
        bool inResponse = false;

        while (!xml.atEnd() && !xml.hasError()) {
            xml.readNext();

            if (xml.isStartElement()) {
                const auto name = xml.name();
                if (name == QLatin1String("response")) {
                    inResponse = true;
                    currentHref.clear();
                    currentIsDirectory = false;
                    currentSize = 0;
                } else if (inResponse && name == QStringLiteral("href")) {
                    currentHref = xml.readElementText();
                } else if (inResponse && name == QStringLiteral("collection")) {
                    currentIsDirectory = true;
                } else if (inResponse && name == QStringLiteral("getcontentlength")) {
                    currentSize = xml.readElementText().toLongLong();
                }
            } else if (xml.isEndElement() && xml.name() == QLatin1String("response")) {
                if (!currentHref.isEmpty()) {
                    QVariantMap entry;
                    entry.insert(QStringLiteral("name"), QFileInfo(currentHref).fileName());
                    entry.insert(QStringLiteral("path"), currentHref);
                    entry.insert(QStringLiteral("isDirectory"), currentIsDirectory);
                    entry.insert(QStringLiteral("size"), currentSize);
                    entries.append(entry);
                }
                inResponse = false;
            }
        }

        emit directoryListed(path, entries, xml.hasError() ? QStringLiteral("XML 解析失败") : QString());
    });
}

void WebDavClient::uploadFile(const QString &localPath, const QString &remotePath)
{
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit uploadFinished(remotePath, false, QStringLiteral("无法打开本地文件: %1").arg(localPath));
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    QNetworkReply *reply = sendRequest(QStringLiteral("PUT"), urlForPath(m_serverUrl, remotePath), data);
    if (!reply) {
        emit uploadFinished(remotePath, false, QStringLiteral("无法创建网络请求"));
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, remotePath]() {
        reply->deleteLater();
        const bool ok = reply->error() == QNetworkReply::NoError;
        emit uploadFinished(remotePath, ok, ok ? QString() : reply->errorString());
    });
}

void WebDavClient::downloadFile(const QString &remotePath, const QString &localPath)
{
    QNetworkReply *reply = sendRequest(QStringLiteral("GET"), urlForPath(m_serverUrl, remotePath));
    if (!reply) {
        emit downloadFinished(remotePath, localPath, false, QStringLiteral("无法创建网络请求"));
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, remotePath, localPath]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit downloadFinished(remotePath, localPath, false, reply->errorString());
            return;
        }

        QFile file(localPath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit downloadFinished(remotePath, localPath, false, QStringLiteral("无法写入本地文件: %1").arg(localPath));
            return;
        }
        file.write(reply->readAll());
        file.close();
        emit downloadFinished(remotePath, localPath, true, QString());
    });
}

void WebDavClient::createDirectory(const QString &remotePath)
{
    QNetworkReply *reply = sendRequest(QStringLiteral("MKCOL"), urlForPath(m_serverUrl, remotePath));
    if (!reply) {
        emit directoryCreated(remotePath, false, QStringLiteral("无法创建网络请求"));
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, remotePath]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 405 Method Not Allowed 表示目录已存在，视为成功
        const bool ok = reply->error() == QNetworkReply::NoError || status == 405;
        emit directoryCreated(remotePath, ok, ok ? QString() : reply->errorString());
    });
}

void WebDavClient::removeFile(const QString &remotePath)
{
    QNetworkReply *reply = sendRequest(QStringLiteral("DELETE"), urlForPath(m_serverUrl, remotePath));
    if (!reply) {
        emit removeFinished(remotePath, false, QStringLiteral("无法创建网络请求"));
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, remotePath]() {
        reply->deleteLater();
        const bool ok = reply->error() == QNetworkReply::NoError;
        emit removeFinished(remotePath, ok, ok ? QString() : reply->errorString());
    });
}

} // namespace litapp
