#include "core/AppLayout.h"

#include <QDir>
#include <QStandardPaths>
#include <QUrl>

namespace litapp {
namespace layout {

// ============================================================
// 常量
// ============================================================

namespace {

const QString kShareRoot = QStringLiteral("zesbks");
const QString kGroupContainer = QStringLiteral("group");
const QString kGroupMeBase = QStringLiteral("group me");

} // namespace

// ============================================================
// 远端：我的账号
// ============================================================

QString remoteShareRoot()
{
    return QStringLiteral("/") + kShareRoot;
}

QString remoteGroupRoot()
{
    return remoteShareRoot()
        + QLatin1Char('/')
        + kGroupContainer;
}

QString groupMeDirectoryName(int groupIndex)
{
    if (groupIndex <= 0) {
        return kGroupMeBase;
    }

    return QStringLiteral("group me %1")
        .arg(groupIndex + 1);
}

QString remoteGroupMeRoot(int groupIndex)
{
    return remoteGroupRoot()
        + QLatin1Char('/')
        + groupMeDirectoryName(groupIndex);
}

bool isReservedShareEntry(const QString &name)
{
    const QString trimmed = name.trimmed();

    if (trimmed.isEmpty()) {
        return false;
    }

    // 群组容器目录本身
    if (trimmed.compare(
            kGroupContainer,
            Qt::CaseInsensitive) == 0) {

        return true;
    }

    // 直接挂在 zesbks 下的 group me / group me 2 ...
    if (trimmed.compare(
            kGroupMeBase,
            Qt::CaseInsensitive) == 0) {

        return true;
    }

    return trimmed.startsWith(
        kGroupMeBase + QLatin1Char(' '),
        Qt::CaseInsensitive);
}

// ============================================================
// 远端：原子对象
// ============================================================

QString remoteAtomicFolderPath(
    const QString &remoteRoot,
    const QString &atomicFolderName)
{
    const QString root =
        normalizeRemotePath(remoteRoot);

    const QString name =
        atomicFolderName.trimmed();

    if (name.isEmpty()) {
        return QString();
    }

    if (root == QStringLiteral("/")) {
        return normalizeRemotePath(
            QLatin1Char('/') + name);
    }

    return normalizeRemotePath(
        root + QLatin1Char('/') + name);
}

QString remoteAtomicFilePath(
    const QString &remoteRoot,
    const QString &atomicFolderName,
    const QString &atomicFileName)
{
    const QString folder =
        remoteAtomicFolderPath(
            remoteRoot,
            atomicFolderName);

    const QString name =
        atomicFileName.trimmed();

    if (folder.isEmpty() || name.isEmpty()) {
        return QString();
    }

    return normalizeRemotePath(
        folder + QLatin1Char('/') + name);
}

// ============================================================
// 远端：路径工具
// ============================================================

QString normalizeRemotePath(const QString &path)
{
    if (path.isEmpty()) {
        return QStringLiteral("/");
    }

    QString result = path.trimmed();

    // 允许传入完整 URL。
    if (result.startsWith(
            QStringLiteral("http://"),
            Qt::CaseInsensitive)
        || result.startsWith(
            QStringLiteral("https://"),
            Qt::CaseInsensitive)) {

        const QUrl url(result);

        if (url.isValid()) {
            result = url.path();
        }
    }

    // URL decode。
    result = QUrl::fromPercentEncoding(
        result.toUtf8());

    result.replace(
        QLatin1Char('\\'),
        QLatin1Char('/'));

    if (!result.startsWith(
            QLatin1Char('/'))) {

        result.prepend(
            QLatin1Char('/'));
    }

    while (result.contains(
        QStringLiteral("//"))) {

        result.replace(
            QStringLiteral("//"),
            QStringLiteral("/"));
    }

    if (result.length() > 1
        && result.endsWith(
            QLatin1Char('/'))) {

        result.chop(1);
    }

    return result;
}

QString remoteParentPath(
    const QString &remotePath)
{
    const QString normalized =
        normalizeRemotePath(remotePath);

    if (normalized == QStringLiteral("/")) {
        return QStringLiteral("/");
    }

    const int slash =
        normalized.lastIndexOf(
            QLatin1Char('/'));

    if (slash <= 0) {
        return QStringLiteral("/");
    }

    return normalized.left(slash);
}

bool isRemotePathInside(
    const QString &remotePath,
    const QString &root)
{
    const QString normalizedRoot =
        normalizeRemotePath(root);

    const QString normalizedPath =
        normalizeRemotePath(remotePath);

    if (normalizedPath == normalizedRoot) {
        return false;
    }

    if (normalizedRoot == QStringLiteral("/")) {
        return normalizedPath != QStringLiteral("/");
    }

    return normalizedPath.startsWith(
        normalizedRoot + QLatin1Char('/'));
}

QString firstLevelNameUnder(
    const QString &remotePath,
    const QString &root)
{
    const QString normalizedRoot =
        normalizeRemotePath(root);

    const QString normalizedPath =
        normalizeRemotePath(remotePath);

    if (!isRemotePathInside(
            normalizedPath,
            normalizedRoot)) {

        return QString();
    }

    QString relative =
        normalizedPath.mid(
            normalizedRoot.length());

    while (relative.startsWith(
        QLatin1Char('/'))) {

        relative.remove(0, 1);
    }

    const int slash =
        relative.indexOf(
            QLatin1Char('/'));

    if (slash < 0) {
        return relative;
    }

    return relative.left(slash);
}

// ============================================================
// 本地：镜像缓存
// ============================================================

QString localBaseRoot()
{
    return QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);
}

QString localLibraryRoot()
{
    return QDir(localBaseRoot()).filePath(
        QStringLiteral("LitApp"));
}

QString localCacheRoot()
{
    return QDir(localLibraryRoot()).filePath(
        QStringLiteral("cache"));
}

QString localOwnMirrorRoot()
{
    return QDir(localCacheRoot()).filePath(
        kShareRoot);
}

QString sanitizeFileSystemComponent(
    const QString &value)
{
    QString result = value.trimmed();

    if (result.isEmpty()) {
        return QStringLiteral("default");
    }

    static const QChar invalidCharacters[] = {
        QLatin1Char('<'), QLatin1Char('>'), QLatin1Char(':'),
        QLatin1Char('"'), QLatin1Char('/'), QLatin1Char('\\'),
        QLatin1Char('|'), QLatin1Char('?'), QLatin1Char('*')
    };

    for (const QChar &ch : invalidCharacters) {

        result.replace(
            ch,
            QLatin1Char('_'));
    }

    while (result.endsWith(
        QLatin1Char('.'))) {

        result.chop(1);
    }

    if (result.isEmpty()) {
        return QStringLiteral("default");
    }

    return result;
}

QString localSourceMirrorRoot(
    const QString &sourceId)
{
    return QDir(
               QDir(localCacheRoot()).filePath(
                   QStringLiteral("sources")))
        .filePath(
            QDir(sanitizeFileSystemComponent(
                     sourceId))
                .filePath(QStringLiteral("zesbks")));
}

QString localMirrorRoot(
    const QString &sourceId)
{
    if (sourceId.trimmed().isEmpty()) {
        return localOwnMirrorRoot();
    }

    return localSourceMirrorRoot(sourceId);
}

QString localPathForRemote(
    const QString &remotePath,
    const QString &sourceId)
{
    const QString normalized =
        normalizeRemotePath(remotePath);

    const QString shareRoot =
        remoteShareRoot();

    const QString mirrorRoot =
        localMirrorRoot(sourceId);

    if (normalized == shareRoot) {
        return mirrorRoot;
    }

    if (!isRemotePathInside(
            normalized,
            shareRoot)) {

        /*
         * 远端路径不在 /zesbks 之下。
         *
         * 这种情况下退化为直接拼接到镜像根，
         * 保证仍然得到一个确定的位置。
         */
        QString relative = normalized;

        while (relative.startsWith(
            QLatin1Char('/'))) {

            relative.remove(0, 1);
        }

        if (relative.isEmpty()) {
            return mirrorRoot;
        }

        return QDir(mirrorRoot).filePath(
            relative);
    }

    const QString relative =
        normalized.mid(shareRoot.length());

    return QDir::cleanPath(
        mirrorRoot + relative);
}

QString remotePathForLocal(
    const QString &localPath,
    const QString &sourceId)
{
    if (localPath.trimmed().isEmpty()) {
        return QString();
    }

    const QString mirrorRoot =
        QDir::cleanPath(
            localMirrorRoot(sourceId));

    const QString local =
        QDir::cleanPath(localPath);

    const QString shareRoot =
        remoteShareRoot();

    if (local == mirrorRoot) {
        return shareRoot;
    }

    if (!local.startsWith(
            mirrorRoot + QLatin1Char('/'))) {

        return QString();
    }

    QString relative =
        local.mid(mirrorRoot.length());

    if (relative.isEmpty()) {
        return shareRoot;
    }

    if (!relative.startsWith(
            QLatin1Char('/'))) {

        relative.prepend(QLatin1Char('/'));
    }

    return normalizeRemotePath(
        shareRoot + relative);
}

} // namespace layout
} // namespace litapp
