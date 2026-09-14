#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

class QNetworkAccessManager;
class QNetworkReply;

namespace litapp {

struct WebDavEntry {
    QString name;
    QString path;
    bool isDirectory = false;
    qint64 size = 0;
};

// 基于 WebDAV 的云同步客户端。
// 第一阶段使用 WebDAV，后续可替换为自建服务器存储。
class WebDavClient : public QObject
{
    Q_OBJECT
public:
    explicit WebDavClient(QObject *parent = nullptr);

    Q_INVOKABLE void setServer(const QString &serverUrl,
                               const QString &username,
                               const QString &password);

    // 仅在本对象内存中配置连接信息，不写入 QSettings。
    // 用于 GroupUs 这类「他人账号」的临时客户端。
    void configureTransient(const QString &serverUrl,
                            const QString &username,
                            const QString &password);

    Q_INVOKABLE void setToken(const QString &token);

    Q_INVOKABLE void restoreSettings();
    Q_INVOKABLE QString serverUrl() const;
    Q_INVOKABLE QString username() const;
    Q_INVOKABLE QString password() const;

    Q_INVOKABLE void listDirectory(const QString &remotePath = QStringLiteral("/"));
    Q_INVOKABLE void uploadFile(const QString &localPath, const QString &remotePath);
    Q_INVOKABLE void downloadFile(const QString &remotePath, const QString &localPath);
    Q_INVOKABLE void createDirectory(const QString &remotePath);
    // 删除远端资源。
    //
    // isCollection = true 时会补上结尾斜杠：
    // Apache 对缺少斜杠的目录会先 301，
    // 而 Qt 跟随时会把 DELETE 降级成 GET，
    // 导致「没删掉却返回 200」被误判为成功。
    Q_INVOKABLE void removeFile(const QString &remotePath,
                                bool isCollection = false);

signals:
    // entries 为 QVariantMap 列表：{ name, path, isDirectory, size }
    void directoryListed(const QString &path, const QVariantList &entries, const QString &error);
    void uploadFinished(const QString &remotePath, bool ok, const QString &error);
    void downloadFinished(const QString &remotePath, const QString &localPath, bool ok, const QString &error);
    void directoryCreated(const QString &remotePath, bool ok, const QString &error);
    void removeFinished(const QString &remotePath, bool ok, const QString &error);

private:
    QNetworkReply *sendRequest(const QString &method, const QUrl &url, const QByteArray &body = {});
    QNetworkAccessManager *m_network = nullptr;
    QUrl m_serverUrl;
    QString m_username;
    QString m_password;
    QString m_token;
};

} // namespace litapp
