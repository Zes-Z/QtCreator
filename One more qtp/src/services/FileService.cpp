#include "services/FileService.h"

#include "storage/MetadataStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

namespace litapp {

FileService::FileService(QObject *parent)
    : QObject(parent)
{
}

bool FileService::addLocalFile(const QString &filePath,
                               int zone,
                               int category,
                               const QString &tagsCsv)
{
    QString localPath = filePath;

    // 兼容 QML FileDialog 返回的 file:// 路径
    if (localPath.startsWith(QStringLiteral("file:"))) {
        localPath = QUrl(localPath).toLocalFile();
    }

    const QFileInfo sourceInfo(localPath);

    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return false;
    }

    /*
     * ============================================================
     * Atomic Folder
     * ============================================================
     *
     * 一个导入文件对应一个原子文件夹。
     *
     * 例如：
     *
     *   xxx.pdf
     *
     * 会被存储为：
     *
     *   zesbks/
     *       xxx/
     *           xxx.pdf
     *
     * 后续这个目录可以继续容纳：
     *
     *   notes.json
     *   annotations.json
     *   tags.json
     *   attachments/
     *   ...
     *
     * 分类、子分类等应用内部逻辑不参与这里的目录结构。
     */

    const QString folderName =
        sourceInfo.completeBaseName();

    const QString zoneDir =
        zone == 0
            ? QStringLiteral("my doc")
            : QStringLiteral("group doc");

    /*
     * 本地同步根目录：
     *
     * Documents/LitApp/zesbks/my doc
     *
     * 或：
     *
     * Documents/LitApp/zesbks/group doc
     */
    const QString libraryRoot =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation)
        + QStringLiteral("/LitApp/zesbks/%1")
              .arg(zoneDir);

    if (!QDir().mkpath(libraryRoot)) {
        return false;
    }

    /*
     * 创建 Atomic Folder。
     *
     * 例如：
     *
     *   .../my doc/文件1/
     */
    const QString atomicFolder =
        libraryRoot
        + QLatin1Char('/')
        + folderName;

    if (!QDir().mkpath(atomicFolder)) {
        return false;
    }

    /*
     * 主文件进入 Atomic Folder。
     *
     *   .../文件1/文件1.pdf
     */
    const QString destPath =
        atomicFolder
        + QLatin1Char('/')
        + sourceInfo.fileName();

    /*
     * 如果源文件本身就是目标文件，则无需移动。
     */
    const QString absoluteSourcePath =
        sourceInfo.absoluteFilePath();

    const QString absoluteDestPath =
        QFileInfo(destPath).absoluteFilePath();

    if (absoluteSourcePath != absoluteDestPath) {

        /*
         * 如果目标已经存在，不覆盖。
         */
        if (!QFile::exists(destPath)) {

            bool moved =
                QFile::rename(
                    absoluteSourcePath,
                    destPath
                );

            /*
             * 某些情况下 rename 可能因为跨磁盘失败，
             * 使用 copy + remove 作为 fallback。
             */
            if (!moved) {

                moved =
                    QFile::copy(
                        absoluteSourcePath,
                        destPath
                    );

                if (moved) {
                    QFile::remove(
                        absoluteSourcePath
                    );
                }
            }

            if (!moved) {
                /*
                 * 如果文件已经成功建立了空的 Atomic Folder，
                 * 这里清理掉空目录。
                 */
                QDir atomicDir(atomicFolder);

                if (atomicDir.entryList(
                        QDir::NoDotAndDotDot
                        | QDir::AllEntries).isEmpty()) {

                    QDir().rmdir(
                        atomicFolder
                    );
                }

                return false;
            }
        }
    }

    /*
     * ============================================================
     * 不再创建 .prop
     * ============================================================
     *
     * 以前这里会生成：
     *
     *   文件1.prop
     *
     * 现在不再需要。
     *
     * Atomic Folder 本身已经明确表示：
     *
     *   文件1/
     *
     * 是文件1的完整同步单元。
     *
     * 如果以后需要保存批注、笔记、标签等同步数据，
     * 应该直接作为 Atomic Folder 中的独立文件：
     *
     *   文件1/
     *       文件1.pdf
     *       notes.json
     *       annotations.json
     *       tags.json
     */

    /*
     * ============================================================
     * 创建 FileItem
     * ============================================================
     */

    FileItem item;

    item.id =
        QUuid::createUuid().toString(
            QUuid::WithoutBraces
        );

    item.name =
        sourceInfo.fileName();

    /*
     * 数据库中的 localPath 指向真正的主文件。
     *
     * 例如：
     *
     * .../zesbks/my doc/文件1/文件1.pdf
     */
    item.localPath =
        destPath;

    /*
     * WebDAV 中的 remotePath 指向 Atomic Folder 中的
     * 主文件，而不是过去的：
     *
     * /zesbks/my doc/文件1.pdf
     *
     * 现在是：
     *
     * /zesbks/my doc/文件1/文件1.pdf
     */
    item.remotePath =
        QStringLiteral(
            "/zesbks/%1/%2/%3")
            .arg(
                zoneDir,
                folderName,
                sourceInfo.fileName()
            );

    item.extension =
        sourceInfo.suffix().toLower();

    /*
     * folderName 现在明确表示 Atomic Folder 的名字。
     */
    item.folderName =
        folderName;

    /*
     * .prop 已经废弃，因此这里不再指向 .prop。
     *
     * 暂时留空，保持 FileItem 数据结构兼容。
     */
    item.annotationsPath =
        QString();

    const QFileInfo destInfo(destPath);

    item.size =
        destInfo.size();

    item.modifiedTime =
        destInfo.lastModified();

    item.zone =
        static_cast<FileZone>(zone);

    item.category =
        static_cast<FileCategory>(category);

    item.syncState =
        SyncState::LocalOnly;

    /*
     * 处理标签。
     *
     * 标签仍然属于应用数据库的数据，
     * 不改变 WebDAV 的 Atomic Folder 结构。
     */
    const QStringList parts =
        tagsCsv.split(
            QLatin1Char(','),
            Qt::SkipEmptyParts
        );

    for (QString tag : parts) {

        tag = tag.trimmed();

        if (!tag.isEmpty() &&
            !item.tags.contains(tag)) {

            item.tags.append(tag);
        }
    }

    /*
     * 保存到 MetadataStore。
     */
    MetadataStore store;

    if (!store.open(m_databasePath)) {
        return false;
    }

    return store.addFile(item);
}


bool FileService::removeFile(const QString &fileId)
{
    MetadataStore store;

    if (!store.open(m_databasePath)) {
        return false;
    }

    return store.removeFile(fileId);
}


bool FileService::markDownloaded(
    const QString &fileId,
    const QString &localPath)
{
    MetadataStore store;

    if (!store.open(m_databasePath)) {
        return false;
    }

    return store.updateLocalPath(
        fileId,
        localPath
    );
}


bool FileService::renameItem(
    const QString &fileId,
    const QString &newBaseName)
{
    MetadataStore store;

    if (!store.open(m_databasePath)) {
        return false;
    }

    FileItem item =
        store.fileById(fileId);

    if (!item.isValid() ||
        newBaseName.trimmed().isEmpty()) {

        return false;
    }

    /*
     * Atomic Folder 当前所在的位置。
     *
     * 例如：
     *
     * .../my doc/文件1
     */
    const QString oldFolder =
        QFileInfo(
            item.localPath
        ).absolutePath();

    const QString newFolder =
        QFileInfo(oldFolder).absolutePath()
        + QLatin1Char('/')
        + newBaseName.trimmed();

    /*
     * 重命名整个 Atomic Folder。
     */
    if (!QDir().rename(
            oldFolder,
            newFolder)) {

        return false;
    }

    /*
     * 更新数据库。
     */
    item.folderName =
        newBaseName.trimmed();

    item.localPath =
        newFolder
        + QLatin1Char('/')
        + item.name;

    const QString zoneDir =
        item.zone == FileZone::MyFiles
            ? QStringLiteral("my doc")
            : QStringLiteral("group doc");

    /*
     * 更新远程主文件路径：
     *
     * /zesbks/my doc/新名称/文件.pdf
     */
    item.remotePath =
        QStringLiteral(
            "/zesbks/%1/%2/%3")
            .arg(
                zoneDir,
                item.folderName,
                item.name
            );

    /*
     * .prop 已废弃。
     */
    item.annotationsPath =
        QString();

    return store.updateFile(item);
}


bool FileService::moveToTrash(
    const QString &fileId)
{
    MetadataStore store;

    if (!store.open(m_databasePath)) {
        return false;
    }

    FileItem item =
        store.fileById(fileId);

    if (!item.isValid()) {
        return false;
    }

    /*
     * 注意：
     *
     * 这里移动的是整个 Atomic Folder，
     * 而不是单独移动 PDF。
     */
    const QString oldFolder =
        QFileInfo(
            item.localPath
        ).absolutePath();

    const QString trashRoot =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation)
        + QStringLiteral(
            "/LitApp/.trash");

    if (!QDir().mkpath(trashRoot)) {
        return false;
    }

    const QString trashFolder =
        trashRoot
        + QLatin1Char('/')
        + item.folderName
        + QStringLiteral(
            "_%1")
              .arg(
                  QDateTime::currentMSecsSinceEpoch()
              );

    if (!QDir().rename(
            oldFolder,
            trashFolder)) {

        return false;
    }

    return store.removeFile(
        fileId
    );
}


bool FileService::copyItem(
    const QString &fileId)
{
    MetadataStore store;

    if (!store.open(m_databasePath)) {
        return false;
    }

    FileItem item =
        store.fileById(fileId);

    if (!item.isValid()) {
        return false;
    }

    /*
     * 整个 Atomic Folder 一起复制。
     */
    const QString srcFolder =
        QFileInfo(
            item.localPath
        ).absolutePath();

    const QString newBase =
        item.folderName
        + QStringLiteral(" copy");

    const QString dstFolder =
        QFileInfo(srcFolder).absolutePath()
        + QLatin1Char('/')
        + newBase;

    if (!copyDirectory(
            srcFolder,
            dstFolder)) {

        return false;
    }

    FileItem newItem =
        item;

    newItem.id =
        QUuid::createUuid().toString(
            QUuid::WithoutBraces
        );

    newItem.folderName =
        newBase;

    newItem.localPath =
        dstFolder
        + QLatin1Char('/')
        + item.name;

    const QString zoneDir =
        item.zone == FileZone::MyFiles
            ? QStringLiteral("my doc")
            : QStringLiteral("group doc");

    newItem.remotePath =
        QStringLiteral(
            "/zesbks/%1/%2/%3")
            .arg(
                zoneDir,
                newBase,
                item.name
            );

    newItem.annotationsPath =
        QString();

    newItem.syncState =
        SyncState::LocalOnly;

    return store.addFile(
        newItem
    );
}


bool FileService::moveItem(
    const QString &fileId,
    int destZone)
{
    MetadataStore store;

    if (!store.open(m_databasePath)) {
        return false;
    }

    FileItem item =
        store.fileById(fileId);

    if (!item.isValid()) {
        return false;
    }

    /*
     * 移动整个 Atomic Folder。
     */
    const QString srcFolder =
        QFileInfo(
            item.localPath
        ).absolutePath();

    const QString zoneDir =
        destZone == 0
            ? QStringLiteral("my doc")
            : QStringLiteral("group doc");

    const QString destRoot =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation)
        + QStringLiteral(
            "/LitApp/zesbks/%1")
              .arg(zoneDir);

    if (!QDir().mkpath(
            destRoot)) {

        return false;
    }

    const QString dstFolder =
        destRoot
        + QLatin1Char('/')
        + item.folderName;

    if (srcFolder == dstFolder) {
        return true;
    }

    if (!QDir().rename(
            srcFolder,
            dstFolder)) {

        return false;
    }

    item.zone =
        static_cast<FileZone>(
            destZone
        );

    item.localPath =
        dstFolder
        + QLatin1Char('/')
        + item.name;

    item.remotePath =
        QStringLiteral(
            "/zesbks/%1/%2/%3")
            .arg(
                zoneDir,
                item.folderName,
                item.name
            );

    item.annotationsPath =
        QString();

    return store.updateFile(
        item
    );
}


bool FileService::copyDirectory(
    const QString &src,
    const QString &dst)
{
    QDir sourceDir(src);

    if (!sourceDir.exists()) {
        return false;
    }

    QDir targetDir(dst);

    if (!targetDir.mkpath(dst)) {
        return false;
    }

    const auto entries =
        sourceDir.entryList(
            QDir::Files
            | QDir::Dirs
            | QDir::NoDotAndDotDot
        );

    for (const QString &entry : entries) {

        const QString srcPath =
            src
            + QLatin1Char('/')
            + entry;

        const QString dstPath =
            dst
            + QLatin1Char('/')
            + entry;

        if (QFileInfo(srcPath).isDir()) {

            if (!copyDirectory(
                    srcPath,
                    dstPath)) {

                return false;
            }

        } else {

            if (!QFile::copy(
                    srcPath,
                    dstPath)) {

                return false;
            }
        }
    }

    return true;
}

} // namespace litapp