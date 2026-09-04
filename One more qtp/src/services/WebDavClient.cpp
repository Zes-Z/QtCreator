#include "services/WebDavClient.h"

#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QXmlStreamReader>

namespace litapp {

WebDavClient::WebDavClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

// ============================================================
// Server / Authentication
// ============================================================

void WebDavClient::setServer(const QString &serverUrl,
                             const QString &username,
                             const QString &password)
{
    m_serverUrl = QUrl(serverUrl);
    m_username = username;
    m_password = password;

    QSettings settings;

    settings.setValue(
        QStringLiteral("webdav/serverUrl"),
        serverUrl);

    settings.setValue(
        QStringLiteral("webdav/username"),
        username);

    settings.setValue(
        QStringLiteral("webdav/password"),
        password);
}

void WebDavClient::setToken(const QString &token)
{
    m_token = token;
}

void WebDavClient::restoreSettings()
{
    QSettings settings;

    m_serverUrl =
        QUrl(settings.value(
                 QStringLiteral("webdav/serverUrl"))
                 .toString());

    m_username =
        settings.value(
            QStringLiteral("webdav/username"))
            .toString();

    m_password =
        settings.value(
            QStringLiteral("webdav/password"))
            .toString();
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

// ============================================================
// Remote URL
// ============================================================

static QUrl urlForPath(
    const QUrl &baseUrl,
    const QString &remotePath)
{
    if (remotePath.isEmpty() ||
        remotePath == QStringLiteral("/")) {

        return baseUrl;
    }

    QString base =
        baseUrl.toString();

    if (!base.endsWith(QLatin1Char('/'))) {
        base += QLatin1Char('/');
    }

    QString path = remotePath;

    while (path.startsWith(QLatin1Char('/'))) {
        path.remove(0, 1);
    }

    return QUrl(base + path);
}

// ============================================================
// Send WebDAV request
// ============================================================

QNetworkReply *WebDavClient::sendRequest(
    const QString &method,
    const QUrl &url,
    const QByteArray &body)
{
    QNetworkRequest request(url);

    /*
     * PROPFIND 使用 Depth: 1。
     *
     * 对其他请求设置 Depth 不会产生实际影响，
     * 但为了保持 WebDAV 行为明确，
     * DELETE / PUT / GET 等请求仍然可以正常工作。
     */
    if (method == QStringLiteral("PROPFIND")) {
        request.setRawHeader("Depth", "1");
    }

    /*
     * Authentication
     */

    if (!m_username.isEmpty()) {

        const QByteArray credentials =
            (m_username + ":" + m_password)
                .toUtf8()
                .toBase64();

        request.setRawHeader(
            "Authorization",
            "Basic " + credentials);

    } else if (!m_token.isEmpty()) {

        request.setRawHeader(
            "Authorization",
            "Bearer " + m_token.toUtf8());
    }

    // ========================================================
    // PROPFIND
    // ========================================================

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

        request.setHeader(
            QNetworkRequest::ContentTypeHeader,
            QStringLiteral("application/xml"));

        request.setRawHeader(
            "Depth",
            "1");

        return m_network->sendCustomRequest(
            request,
            "PROPFIND",
            propfindBody);
    }

    // ========================================================
    // MKCOL
    // ========================================================

    if (method == QStringLiteral("MKCOL")) {

        return m_network->sendCustomRequest(
            request,
            "MKCOL",
            body);
    }

    // ========================================================
    // DELETE
    // ========================================================

    if (method == QStringLiteral("DELETE")) {

        /*
         * WebDAV DELETE 的关键点：
         *
         * DELETE 一个 Collection（目录）时，
         * WebDAV 服务器会删除这个 Collection 及其内容。
         *
         * 因此：
         *
         * /zesbks/my doc/文件1/
         *
         * 可以一次删除：
         *
         * 文件1.pdf
         * notes.json
         * annotations.json
         * tags.json
         * 以及其他附件
         *
         * 这正好对应我们的 Atomic Folder。
         */

        return m_network->sendCustomRequest(
            request,
            "DELETE",
            body);
    }

    // ========================================================
    // PUT
    // ========================================================

    if (method == QStringLiteral("PUT")) {

        request.setHeader(
            QNetworkRequest::ContentTypeHeader,
            QStringLiteral("application/octet-stream"));

        return m_network->put(
            request,
            body);
    }

    // ========================================================
    // GET
    // ========================================================

    if (method == QStringLiteral("GET")) {

        return m_network->get(request);
    }

    return nullptr;
}

// ============================================================
// List directory
// ============================================================

void WebDavClient::listDirectory(
    const QString &remotePath)
{
    const QString path =
        remotePath.isEmpty()
            ? QStringLiteral("/")
            : remotePath;

    QNetworkReply *reply =
        sendRequest(
            QStringLiteral("PROPFIND"),
            urlForPath(
                m_serverUrl,
                path));

    if (!reply) {

        emit directoryListed(
            path,
            {},
            QStringLiteral(
                "无法创建网络请求"));

        return;
    }

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, path]() {

            reply->deleteLater();

            if (reply->error() !=
                QNetworkReply::NoError) {

                emit directoryListed(
                    path,
                    {},
                    reply->errorString());

                return;
            }

            QVariantList entries;

            QXmlStreamReader xml(
                reply->readAll());

            QString currentHref;

            bool currentIsDirectory = false;

            qint64 currentSize = 0;

            bool inResponse = false;

            while (!xml.atEnd() &&
                   !xml.hasError()) {

                xml.readNext();

                if (xml.isStartElement()) {

                    const auto name =
                        xml.name();

                    if (name ==
                        QLatin1String("response")) {

                        inResponse = true;

                        currentHref.clear();

                        currentIsDirectory =
                            false;

                        currentSize = 0;

                    } else if (
                        inResponse &&
                        name ==
                            QStringLiteral("href")) {

                        currentHref =
                            xml.readElementText();

                    } else if (
                        inResponse &&
                        name ==
                            QStringLiteral(
                                "collection")) {

                        currentIsDirectory =
                            true;

                    } else if (
                        inResponse &&
                        name ==
                            QStringLiteral(
                                "getcontentlength")) {

                        currentSize =
                            xml.readElementText()
                                .toLongLong();
                    }

                } else if (
                    xml.isEndElement() &&
                    xml.name() ==
                        QLatin1String("response")) {

                    if (!currentHref.isEmpty()) {

                        QVariantMap entry;

                        entry.insert(
                            QStringLiteral("name"),
                            QFileInfo(
                                currentHref)
                                .fileName());

                        entry.insert(
                            QStringLiteral("path"),
                            currentHref);

                        entry.insert(
                            QStringLiteral(
                                "isDirectory"),
                            currentIsDirectory);

                        entry.insert(
                            QStringLiteral("size"),
                            currentSize);

                        entries.append(entry);
                    }

                    inResponse = false;
                }
            }

            emit directoryListed(
                path,
                entries,
                xml.hasError()
                    ? QStringLiteral(
                          "XML 解析失败")
                    : QString());
        });
}

// ============================================================
// Upload file
// ============================================================

void WebDavClient::uploadFile(
    const QString &localPath,
    const QString &remotePath)
{
    QFile file(localPath);

    if (!file.open(QIODevice::ReadOnly)) {

        emit uploadFinished(
            remotePath,
            false,
            QStringLiteral(
                "无法打开本地文件: %1")
                .arg(localPath));

        return;
    }

    const QByteArray data =
        file.readAll();

    file.close();

    QNetworkReply *reply =
        sendRequest(
            QStringLiteral("PUT"),
            urlForPath(
                m_serverUrl,
                remotePath),
            data);

    if (!reply) {

        emit uploadFinished(
            remotePath,
            false,
            QStringLiteral(
                "无法创建网络请求"));

        return;
    }

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, remotePath]() {

            reply->deleteLater();

            const int status =
                reply->attribute(
                    QNetworkRequest::
                        HttpStatusCodeAttribute)
                    .toInt();

            const bool ok =
                reply->error() ==
                    QNetworkReply::NoError &&
                status >= 200 &&
                status < 300;

            emit uploadFinished(
                remotePath,
                ok,
                ok
                    ? QString()
                    : reply->errorString());
        });
}

// ============================================================
// Download file
// ============================================================

void WebDavClient::downloadFile(
    const QString &remotePath,
    const QString &localPath)
{
    QNetworkReply *reply =
        sendRequest(
            QStringLiteral("GET"),
            urlForPath(
                m_serverUrl,
                remotePath));

    if (!reply) {

        emit downloadFinished(
            remotePath,
            localPath,
            false,
            QStringLiteral(
                "无法创建网络请求"));

        return;
    }

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this,
         reply,
         remotePath,
         localPath]() {

            reply->deleteLater();

            if (reply->error() !=
                QNetworkReply::NoError) {

                emit downloadFinished(
                    remotePath,
                    localPath,
                    false,
                    reply->errorString());

                return;
            }

            QFile file(localPath);

            if (!file.open(
                    QIODevice::WriteOnly)) {

                emit downloadFinished(
                    remotePath,
                    localPath,
                    false,
                    QStringLiteral(
                        "无法写入本地文件: %1")
                        .arg(localPath));

                return;
            }

            file.write(
                reply->readAll());

            file.close();

            emit downloadFinished(
                remotePath,
                localPath,
                true,
                QString());
        });
}

// ============================================================
// Create directory
// ============================================================

void WebDavClient::createDirectory(
    const QString &remotePath)
{
    QNetworkReply *reply =
        sendRequest(
            QStringLiteral("MKCOL"),
            urlForPath(
                m_serverUrl,
                remotePath));

    if (!reply) {

        emit directoryCreated(
            remotePath,
            false,
            QStringLiteral(
                "无法创建网络请求"));

        return;
    }

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, remotePath]() {

            reply->deleteLater();

            const int status =
                reply->attribute(
                    QNetworkRequest::
                        HttpStatusCodeAttribute)
                    .toInt();

            /*
             * 201 Created：
             *     创建成功
             *
             * 405 Method Not Allowed：
             *     目录已经存在
             *
             * 409 Conflict：
             *     父目录不存在
             *
             * 其他：
             *     创建失败
             */

            const bool ok =
                reply->error() ==
                    QNetworkReply::NoError
                || status == 405;

            QString error;

            if (!ok) {

                error =
                    reply->errorString();

                if (error.isEmpty()) {

                    error =
                        QStringLiteral(
                            "创建远程目录失败，HTTP 状态码: %1")
                            .arg(status);
                }
            }

            emit directoryCreated(
                remotePath,
                ok,
                error);
        });
}

// ============================================================
// Remove file / Atomic Folder
// ============================================================

void WebDavClient::removeFile(
    const QString &remotePath)
{
    /*
     * 注意：
     *
     * 函数名仍然叫 removeFile，
     * 因为这是现有 WebDavClient.h 的公开接口。
     *
     * 但 remotePath 不一定是普通文件。
     *
     * 它也可以是：
     *
     *     /zesbks/my doc/文件1/
     *
     * 当 remotePath 指向 WebDAV Collection 时，
     * DELETE 会删除整个 Collection。
     *
     * 因此本函数同时支持：
     *
     * 1. 删除普通文件
     * 2. 删除整个 Atomic Folder
     */

    QString path = remotePath.trimmed();

    if (path.isEmpty()) {

        emit removeFinished(
            remotePath,
            false,
            QStringLiteral(
                "远程删除路径为空"));

        return;
    }

    /*
     * Atomic Folder 必须保持结尾的 '/'。
     *
     * 例如：
     *
     *     /zesbks/my doc/文件1/
     *
     * 而不是：
     *
     *     /zesbks/my doc/文件1
     *
     * 对 WebDAV Collection 来说，
     * 明确使用目录 URL 更安全。
     *
     * 如果传入的是普通文件，
     * 则不强制添加 '/'。
     */

    const QUrl url =
        urlForPath(
            m_serverUrl,
            path);

    QNetworkReply *reply =
        sendRequest(
            QStringLiteral("DELETE"),
            url);

    if (!reply) {

        emit removeFinished(
            remotePath,
            false,
            QStringLiteral(
                "无法创建网络请求"));

        return;
    }

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, remotePath]() {

            reply->deleteLater();

            const int status =
                reply->attribute(
                    QNetworkRequest::
                        HttpStatusCodeAttribute)
                    .toInt();

            /*
             * WebDAV DELETE 常见成功状态：
             *
             * 200 OK
             * 202 Accepted
             * 204 No Content
             *
             * 某些服务器在资源已经不存在时返回：
             *
             * 404 Not Found
             *
             * 对同步系统来说，如果远程资源本来
             * 就不存在，那么最终状态已经是：
             *
             *     “不存在”
             *
             * 因此 404 也可以视为删除成功。
             */

            const bool httpSuccess =
                status == 200 ||
                status == 202 ||
                status == 204 ||
                status == 404;

            const bool networkSuccess =
                reply->error() ==
                QNetworkReply::NoError;

            const bool ok =
                networkSuccess ||
                httpSuccess;

            QString error;

            if (!ok) {

                error =
                    reply->errorString();

                if (error.isEmpty()) {

                    error =
                        QStringLiteral(
                            "WebDAV 删除失败，HTTP 状态码: %1")
                            .arg(status);
                }
            }

            emit removeFinished(
                remotePath,
                ok,
                error);
        });
}

} // namespace litapp