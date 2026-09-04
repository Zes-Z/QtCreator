#include "services/FileService.h"

#include "storage/MetadataStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <QVariantMap>

namespace litapp {

FileService::FileService(QObject *parent)
    : QObject(parent)
{
}

// ============================================================================
// Database
// ============================================================================

void FileService::setDatabasePath(
    const QString &databasePath)
{
    if (!databasePath.trimmed().isEmpty()) {
        m_databasePath = databasePath;
    }
}

QString FileService::databasePath() const
{
    return m_databasePath;
}

// ============================================================================
// Space helpers
// ============================================================================

bool FileService::isValidSpace(int space) const
{
    return space >= static_cast<int>(FileSpace::MyDoc)
        && space <= static_cast<int>(FileSpace::GroupUs);
}

FileSpace FileService::toFileSpace(int space) const
{
    if (!isValidSpace(space)) {
        return FileSpace::MyDoc;
    }

    return static_cast<FileSpace>(space);
}

QString FileService::spaceRootName(
    FileSpace space) const
{
    switch (space) {
    case FileSpace::MyDoc:
        return QStringLiteral("my doc");

    case FileSpace::GroupMe:
        return QStringLiteral("group me");

    case FileSpace::GroupUs:
        return QStringLiteral("group us");
    }

    return QStringLiteral("my doc");
}

// ============================================================================
// Local filesystem helpers
// ============================================================================

QString FileService::buildAtomicFolderLocalPath(
    const QString &baseDirectory,
    const QString &atomicFolderName) const
{
    return QDir(baseDirectory).filePath(
        atomicFolderName);
}

QString FileService::buildAtomicFileLocalPath(
    const QString &atomicFolderPath,
    const QString &atomicFileName) const
{
    return QDir(atomicFolderPath).filePath(
        atomicFileName);
}

bool FileService::copyLocalFile(
    const QString &sourcePath,
    const QString &destinationPath) const
{
    QFileInfo sourceInfo(sourcePath);

    if (!sourceInfo.exists()
        || !sourceInfo.isFile()) {
        return false;
    }

    if (QFileInfo::exists(destinationPath)) {
        return false;
    }

    const QFileInfo destinationInfo(destinationPath);

    const QString parentDirectory =
        destinationInfo.absolutePath();

    if (!QDir().mkpath(parentDirectory)) {
        return false;
    }

    return QFile::copy(
        sourcePath,
        destinationPath);
}

bool FileService::copyLocalDirectory(
    const QString &sourcePath,
    const QString &destinationPath) const
{
    QDir sourceDir(sourcePath);

    if (!sourceDir.exists()) {
        return false;
    }

    if (QFileInfo::exists(destinationPath)) {
        return false;
    }

    if (!QDir().mkpath(destinationPath)) {
        return false;
    }

    const QStringList entries =
        sourceDir.entryList(
            QDir::NoDotAndDotDot
            | QDir::AllEntries
            | QDir::Hidden
            | QDir::System);

    for (const QString &entry : entries) {

        const QString source =
            sourceDir.filePath(entry);

        const QString destination =
            QDir(destinationPath).filePath(entry);

        const QFileInfo info(source);

        if (info.isDir()) {

            if (!copyLocalDirectory(
                    source,
                    destination)) {

                return false;
            }

        } else {

            if (!copyLocalFile(
                    source,
                    destination)) {

                return false;
            }
        }
    }

    return true;
}

bool FileService::moveLocalPath(
    const QString &sourcePath,
    const QString &destinationPath) const
{
    if (sourcePath.isEmpty()
        || destinationPath.isEmpty()) {
        return false;
    }

    const QString absoluteSource =
        QFileInfo(sourcePath).absoluteFilePath();

    const QString absoluteDestination =
        QFileInfo(destinationPath).absoluteFilePath();

    if (absoluteSource == absoluteDestination) {
        return true;
    }

    if (!QFileInfo::exists(absoluteSource)) {
        return false;
    }

    if (QFileInfo::exists(absoluteDestination)) {
        return false;
    }

    const QFileInfo destinationInfo(
        absoluteDestination);

    const QString parentDirectory =
        destinationInfo.absolutePath();

    if (!QDir().mkpath(parentDirectory)) {
        return false;
    }

    if (QFile::rename(
            absoluteSource,
            absoluteDestination)) {

        return true;
    }

    /*
     * QFile::rename() 在跨磁盘情况下可能失败。
     *
     * 此时使用：
     *
     *     copy + remove
     */

    const QFileInfo sourceInfo(
        absoluteSource);

    if (sourceInfo.isDir()) {

        if (!copyLocalDirectory(
                absoluteSource,
                absoluteDestination)) {

            return false;
        }

        if (!removeLocalPath(
                absoluteSource)) {

            removeLocalPath(
                absoluteDestination);

            return false;
        }

        return true;
    }

    if (!QFile::copy(
            absoluteSource,
            absoluteDestination)) {

        return false;
    }

    if (!QFile::remove(
            absoluteSource)) {

        QFile::remove(
            absoluteDestination);

        return false;
    }

    return true;
}

bool FileService::removeLocalPath(
    const QString &path) const
{
    if (path.isEmpty()) {
        return true;
    }

    QFileInfo info(path);

    if (!info.exists()) {
        return true;
    }

    if (info.isDir()) {
        return QDir(path).removeRecursively();
    }

    return QFile::remove(path);
}

// ============================================================================
// Metadata helpers
// ============================================================================

bool FileService::loadItem(
    const QString &fileId,
    FileItem &item) const
{
    MetadataStore store;

    if (!store.open(m_databasePath)) {
        return false;
    }

    item = store.fileById(fileId);

    return item.isValid();
}

bool FileService::saveItem(
    const FileItem &item)
{
    MetadataStore store;

    if (!store.open(m_databasePath)) {
        return false;
    }

    return store.updateFile(item);
}

bool FileService::removeItemFromStore(
    const QString &fileId)
{
    MetadataStore store;

    if (!store.open(m_databasePath)) {
        return false;
    }

    return store.removeFile(fileId);
}

// ============================================================================
// Name helpers
// ============================================================================

QString FileService::uniqueName(
    const QString &name,
    const QString &parentId,
    FileSpace space,
    const QString &sourceId) const
{
    MetadataStore store;

    if (!store.open(m_databasePath)) {
        return name;
    }

    QVector<FileItem> siblings;

    if (parentId.isEmpty()) {
        siblings =
            store.rootFiles(space);
    } else {
        siblings =
            store.childrenOf(
                parentId,
                space);
    }

    auto exists =
        [&siblings, &sourceId](
            const QString &candidate) {

        for (const FileItem &item :
             siblings) {

            if (item.sourceId != sourceId) {
                continue;
            }

            if (item.name.compare(
                    candidate,
                    Qt::CaseInsensitive) == 0) {

                return true;
            }
        }

        return false;
    };

    if (!exists(name)) {
        return name;
    }

    const QFileInfo info(name);

    const QString baseName =
        info.completeBaseName();

    const QString suffix =
        info.suffix();

    for (int i = 2; ; ++i) {

        QString candidate;

        if (suffix.isEmpty()) {

            candidate =
                QStringLiteral("%1 (%2)")
                    .arg(baseName)
                    .arg(i);

        } else {

            candidate =
                QStringLiteral("%1 (%2).%3")
                    .arg(baseName)
                    .arg(i)
                    .arg(suffix);
        }

        if (!exists(candidate)) {
            return candidate;
        }
    }
}

QString FileService::uniqueCopyName(
    const QString &name,
    const QString &parentId,
    FileSpace space,
    const QString &sourceId) const
{
    return uniqueName(
        name
        + QStringLiteral(" copy"),
        parentId,
        space,
        sourceId);
}

// ============================================================================
// Atomic Folder / Atomic File
// ============================================================================

FileItem FileService::findAtomicFolderForFile(
    const FileItem &atomicFile) const
{
    if (!atomicFile.isAtomicFile()
        || atomicFile.parentId.isEmpty()) {

        return {};
    }

    FileItem folder;

    if (!loadItem(
            atomicFile.parentId,
            folder)) {

        return {};
    }

    if (!folder.isAtomicFolder()) {
        return {};
    }

    return folder;
}

QVector<FileItem> FileService::atomicFilesForFolder(
    const FileItem &atomicFolder) const
{
    if (!atomicFolder.isAtomicFolder()) {
        return {};
    }

    MetadataStore store;

    if (!store.open(m_databasePath)) {
        return {};
    }

    const QVector<FileItem> children =
        store.childrenOf(
            atomicFolder.id,
            atomicFolder.space);

    QVector<FileItem> result;

    for (const FileItem &child :
         children) {

        if (child.objectType
            == FileObjectType::AtomicFile) {

            result.append(child);
        }
    }

    return result;
}

bool FileService::createAtomicFolderForFile(
    const QString &filePath,
    const QString &parentId,
    FileSpace space,
    const QString &sourceId,
    int category,
    const QStringList &tags,
    FileItem *atomicFolder,
    FileItem *atomicFile)
{
    if (!atomicFolder
        || !atomicFile) {

        return false;
    }

    QFileInfo sourceInfo(filePath);

    if (!sourceInfo.exists()
        || !sourceInfo.isFile()) {

        return false;
    }

    const QString absoluteSourcePath =
        sourceInfo.absoluteFilePath();

    /*
     * ========================================================================
     * 1. AtomicFolder 名称
     * ========================================================================
     *
     * 例如：
     *
     *     paper.pdf
     *
     * AtomicFolder:
     *
     *     paper
     *
     * AtomicFile:
     *
     *     paper.pdf
     *
     * 两者名称相互独立。
     */

    QString atomicFolderName =
        sourceInfo.completeBaseName();

    if (atomicFolderName.isEmpty()) {
        atomicFolderName =
            sourceInfo.fileName();
    }

    atomicFolderName =
        uniqueName(
            atomicFolderName,
            parentId,
            space,
            sourceId);

    /*
     * ========================================================================
     * 2. LitApp 本地存储根目录
     * ========================================================================
     */

    const QString documentsRoot =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation);

    if (documentsRoot.isEmpty()) {
        return false;
    }

    const QString libraryRoot =
        QDir(documentsRoot).filePath(
            QStringLiteral(
                "LitApp/zesbks/%1")
                .arg(
                    spaceRootName(space)));

    if (!QDir().mkpath(
            libraryRoot)) {

        return false;
    }

    /*
     * ========================================================================
     * 3. 确定 AtomicFolder 的逻辑父目录
     * ========================================================================
     *
     * parentId == "":
     *
     *     直接放在 space root。
     *
     * parentId != "":
     *
     *     必须指向普通 Folder。
     *
     * AtomicFolder 不允许直接嵌套在 AtomicFolder 下面。
     */

    QString parentLocalDirectory =
        libraryRoot;

    if (!parentId.isEmpty()) {

        FileItem parent;

        if (!loadItem(
                parentId,
                parent)) {

            return false;
        }

        if (!parent.isFolder()) {
            return false;
        }

        if (parent.localPath.isEmpty()) {
            return false;
        }

        parentLocalDirectory =
            QFileInfo(
                parent.localPath)
                .absoluteFilePath();

        if (!QFileInfo(
                parentLocalDirectory).isDir()) {

            if (!QDir().mkpath(
                    parentLocalDirectory)) {

                return false;
            }
        }
    }

    /*
     * ========================================================================
     * 4. 创建 AtomicFolder 物理目录
     * ========================================================================
     */

    const QString atomicFolderPath =
        buildAtomicFolderLocalPath(
            parentLocalDirectory,
            atomicFolderName);

    if (QFileInfo::exists(
            atomicFolderPath)) {

        atomicFolderName =
            uniqueName(
                atomicFolderName
                + QStringLiteral(" copy"),
                parentId,
                space,
                sourceId);
    }

    const QString finalAtomicFolderPath =
        buildAtomicFolderLocalPath(
            parentLocalDirectory,
            atomicFolderName);

    if (QFileInfo::exists(
            finalAtomicFolderPath)) {

        return false;
    }

    if (!QDir().mkpath(
            finalAtomicFolderPath)) {

        return false;
    }

    /*
     * ========================================================================
     * 5. 创建 AtomicFile 的物理文件
     * ========================================================================
     */

    const QString atomicFileName =
        sourceInfo.fileName();

    const QString destinationFilePath =
        buildAtomicFileLocalPath(
            finalAtomicFolderPath,
            atomicFileName);

    const QString absoluteDestinationPath =
        QFileInfo(
            destinationFilePath)
            .absoluteFilePath();

    /*
     * 如果用户选择的本来就是 LitApp 里的目标文件，
     * 不需要复制。
     */

    if (absoluteSourcePath
        != absoluteDestinationPath) {

        if (!copyLocalFile(
                absoluteSourcePath,
                absoluteDestinationPath)) {

            removeLocalPath(
                finalAtomicFolderPath);

            return false;
        }
    }

    /*
     * ========================================================================
     * 重要：
     *
     * 不删除用户原文件。
     *
     * “加入文件”现在采用 COPY 语义：
     *
     *     原文件
     *        │
     *        └──── copy ────> LitApp AtomicFolder
     *
     * 这样即使后面的 metadata 写入失败，
     * 用户原始文件也不会丢失。
     * ========================================================================
     */

    const QFileInfo destinationInfo(
        absoluteDestinationPath);

    if (!destinationInfo.exists()
        || !destinationInfo.isFile()) {

        removeLocalPath(
            finalAtomicFolderPath);

        return false;
    }

    /*
     * ========================================================================
     * 6. 创建 AtomicFolder metadata
     * ========================================================================
     */

    FileItem folder;

    folder.id =
        QUuid::createUuid().toString(
            QUuid::WithoutBraces);

    folder.parentId =
        parentId;

    folder.name =
        atomicFolderName;

    folder.objectType =
        FileObjectType::AtomicFolder;

    folder.space =
        space;

    folder.sourceId =
        sourceId;

    folder.localPath =
        finalAtomicFolderPath;

    /*
     * remotePath 暂时只保存同步映射结果。
     *
     * SyncService 后续可以根据 space/sourceId
     * 重新计算真正的 WebDAV 路径。
     */

    folder.remotePath =
        QStringLiteral(
            "/zesbks/%1")
            .arg(
                atomicFolderName);

    folder.extension =
        QString();

    folder.annotationsPath =
        QString();

    folder.size =
        0;

    folder.modifiedTime =
        QDateTime::currentDateTime();

    folder.category =
        static_cast<FileCategory>(
            category);

    folder.tags =
        tags;

    folder.syncState =
        SyncState::LocalOnly;

    /*
     * ========================================================================
     * 7. 创建 AtomicFile metadata
     * ========================================================================
     */

    FileItem file;

    file.id =
        QUuid::createUuid().toString(
            QUuid::WithoutBraces);

    file.parentId =
        folder.id;

    file.name =
        atomicFileName;

    file.objectType =
        FileObjectType::AtomicFile;

    file.space =
        space;

    file.sourceId =
        sourceId;

    file.localPath =
        absoluteDestinationPath;

    file.remotePath =
        QStringLiteral(
            "/zesbks/%1/%2")
            .arg(
                atomicFolderName,
                atomicFileName);

    file.extension =
        destinationInfo.suffix()
            .toLower();

    file.annotationsPath =
        QString();

    file.size =
        destinationInfo.size();

    file.modifiedTime =
        destinationInfo.lastModified();

    file.category =
        static_cast<FileCategory>(
            category);

    file.tags =
        tags;

    file.syncState =
        SyncState::LocalOnly;

    /*
     * ========================================================================
     * 8. 写入 SQLite
     * ========================================================================
     */

    MetadataStore store;

    if (!store.open(
            m_databasePath)) {

        removeLocalPath(
            finalAtomicFolderPath);

        return false;
    }

    /*
     * 先写 AtomicFolder。
     */

    if (!store.addFile(
            folder)) {

        removeLocalPath(
            finalAtomicFolderPath);

        return false;
    }

    /*
     * 再写 AtomicFile。
     */

    if (!store.addFile(
            file)) {

        store.removeFile(
            folder.id);

        removeLocalPath(
            finalAtomicFolderPath);

        return false;
    }

    /*
     * ========================================================================
     * 9. 最终一致性检查
     * ========================================================================
     *
     * 到这里必须同时存在：
     *
     *     SQLite AtomicFolder
     *     SQLite AtomicFile
     *     物理 AtomicFolder
     *     物理 AtomicFile
     */

    if (!QFileInfo(
            folder.localPath).isDir()
        || !QFileInfo(
                file.localPath).isFile()) {

        store.removeFile(
            file.id);

        store.removeFile(
            folder.id);

        removeLocalPath(
            finalAtomicFolderPath);

        return false;
    }

    *atomicFolder =
        folder;

    *atomicFile =
        file;

    return true;
}

// ============================================================================
// Add local file
// ============================================================================

bool FileService::addLocalFile(
    const QString &filePath,
    int space,
    const QString &parentId,
    const QString &sourceId,
    int category,
    const QString &tagsCsv)
{
    if (!isValidSpace(space)) {
        return false;
    }

    QString localFilePath =
        filePath.trimmed();

    /*
     * 兼容 QML FileDialog：
     *
     *     file:///C:/xxx/test.pdf
     */

    if (localFilePath.startsWith(
            QStringLiteral("file:"))) {

        localFilePath =
            QUrl(localFilePath)
                .toLocalFile();
    }

    QFileInfo sourceInfo(localFilePath);

    if (!sourceInfo.exists()
        || !sourceInfo.isFile()) {

        return false;
    }

    const FileSpace fileSpace =
        toFileSpace(space);

    /*
     * GroupUs 必须有 sourceId。
     */

    if (fileSpace == FileSpace::GroupUs
        && sourceId.trimmed().isEmpty()) {

        return false;
    }

    /*
     * MyDoc / GroupMe 不强制要求 sourceId。
     */

    /*
     * ========================================================================
     * Tags
     * ========================================================================
     */

    QStringList tags;

    const QStringList parts =
        tagsCsv.split(
            QLatin1Char(','),
            Qt::SkipEmptyParts);

    for (QString tag : parts) {

        tag = tag.trimmed();

        if (!tag.isEmpty()
            && !tags.contains(tag)) {

            tags.append(tag);
        }
    }

    /*
     * ========================================================================
     * 真正导入
     * ========================================================================
     *
     *     AtomicFolder
     *          │
     *          └── AtomicFile
     */

    FileItem atomicFolder;
    FileItem atomicFile;

    if (!createAtomicFolderForFile(
            localFilePath,
            parentId,
            fileSpace,
            sourceId,
            category,
            tags,
            &atomicFolder,
            &atomicFile)) {

        return false;
    }

    /*
     * 通知 UI。
     *
     * 注意：
     *
     * fileAdded() 本身只是 signal，
     * FilePage / FileListModel 必须响应它并 reload。
     */

    emit fileAdded(
        atomicFolder.id);

    emit fileAdded(
        atomicFile.id);

    return true;
}

// ============================================================================
// Remove
// ============================================================================

bool FileService::removeFile(
    const QString &fileId)
{
    FileItem item;

    if (!loadItem(
            fileId,
            item)) {

        return false;
    }

    MetadataStore store;

    if (!store.open(
            m_databasePath)) {

        return false;
    }

    /*
     * AtomicFile：
     *
     * 只删除自己。
     */

    if (item.isAtomicFile()) {

        const QString localPath =
            item.localPath;

        if (!store.removeFile(
                item.id)) {

            return false;
        }

        if (!removeLocalPath(
                localPath)) {

            return false;
        }

        emit fileRemoved(
            item.id);

        return true;
    }

    /*
     * AtomicFolder：
     *
     * 删除 AtomicFile + AtomicFolder。
     */

    if (item.isAtomicFolder()) {

        const QVector<FileItem> files =
            atomicFilesForFolder(item);

        const QString localPath =
            item.localPath;

        for (const FileItem &child :
             files) {

            if (!store.removeFile(
                    child.id)) {

                return false;
            }

            emit fileRemoved(
                child.id);
        }

        if (!store.removeFile(
                item.id)) {

            return false;
        }

        removeLocalPath(
            localPath);

        emit fileRemoved(
            item.id);

        return true;
    }

    /*
     * 普通 Folder：
     *
     * 递归删除整个逻辑子树。
     */

    if (item.isFolder()) {

        const QVector<FileItem> tree =
            descendants(
                item.id,
                item.space,
                item.sourceId);

        for (const FileItem &child :
             tree) {

            if (!store.removeFile(
                    child.id)) {

                return false;
            }

            emit fileRemoved(
                child.id);
        }

        const QString localPath =
            item.localPath;

        if (!store.removeFile(
                item.id)) {

            return false;
        }

        removeLocalPath(
            localPath);

        emit fileRemoved(
            item.id);

        return true;
    }

    return false;
}

// ============================================================================
// Mark downloaded
// ============================================================================

bool FileService::markDownloaded(
    const QString &fileId,
    const QString &localPath)
{
    const QString normalizedPath =
        localPath.trimmed();

    if (normalizedPath.isEmpty()) {
        return false;
    }

    MetadataStore store;

    if (!store.open(
            m_databasePath)) {

        return false;
    }

    FileItem item =
        store.fileById(fileId);

    if (!item.isValid()) {
        return false;
    }

    if (!store.updateLocalPath(
            fileId,
            normalizedPath)) {

        return false;
    }

    emit fileUpdated(
        fileId);

    return true;
}

// ============================================================================
// Rename
// ============================================================================

bool FileService::renameItem(
    const QString &fileId,
    const QString &newName)
{
    const QString trimmedName =
        newName.trimmed();

    if (trimmedName.isEmpty()) {
        return false;
    }

    FileItem item;

    if (!loadItem(
            fileId,
            item)) {

        return false;
    }

    MetadataStore store;

    if (!store.open(
            m_databasePath)) {

        return false;
    }

    /*
     * ========================================================================
     * AtomicFile
     * ========================================================================
     */

    if (item.isAtomicFile()) {

        const QString oldPath =
            item.localPath;

        if (oldPath.isEmpty()) {
            return false;
        }

        const QString parentDirectory =
            QFileInfo(oldPath)
                .absolutePath();

        const QString newPath =
            QDir(parentDirectory)
                .filePath(trimmedName);

        if (oldPath != newPath) {

            if (QFileInfo::exists(
                    newPath)) {

                return false;
            }

            if (!moveLocalPath(
                    oldPath,
                    newPath)) {

                return false;
            }
        }

        item.name =
            trimmedName;

        item.localPath =
            QFileInfo(newPath)
                .absoluteFilePath();

        item.extension =
            QFileInfo(newPath)
                .suffix()
                .toLower();

        const FileItem folder =
            findAtomicFolderForFile(item);

        if (!folder.isValid()) {
            return false;
        }

        item.remotePath =
            QStringLiteral(
                "/zesbks/%1/%2")
                .arg(
                    folder.name,
                    item.name);

        if (!store.updateFile(
                item)) {

            return false;
        }

        emit fileUpdated(
            item.id);

        return true;
    }

    /*
     * ========================================================================
     * AtomicFolder
     * ========================================================================
     */

    if (item.isAtomicFolder()) {

        const QString oldDirectory =
            item.localPath;

        if (oldDirectory.isEmpty()) {
            return false;
        }

        const QString parentDirectory =
            QFileInfo(oldDirectory)
                .absolutePath();

        const QString newDirectory =
            QDir(parentDirectory)
                .filePath(trimmedName);

        if (oldDirectory != newDirectory) {

            if (QFileInfo::exists(
                    newDirectory)) {

                return false;
            }

            if (!moveLocalPath(
                    oldDirectory,
                    newDirectory)) {

                return false;
            }
        }

        item.name =
            trimmedName;

        item.localPath =
            QFileInfo(newDirectory)
                .absoluteFilePath();

        item.remotePath =
            QStringLiteral(
                "/zesbks/%1")
                .arg(item.name);

        if (!store.updateFile(
                item)) {

            return false;
        }

        /*
         * AtomicFile 的名称不改变。
         *
         * 只更新它的物理路径和 remotePath。
         */

        const QVector<FileItem> files =
            atomicFilesForFolder(item);

        for (FileItem child :
             files) {

            child.localPath =
                buildAtomicFileLocalPath(
                    item.localPath,
                    child.name);

            child.remotePath =
                QStringLiteral(
                    "/zesbks/%1/%2")
                    .arg(
                        item.name,
                        child.name);

            if (!store.updateFile(
                    child)) {

                return false;
            }

            emit fileUpdated(
                child.id);
        }

        emit fileUpdated(
            item.id);

        return true;
    }

    /*
     * ========================================================================
     * 普通 Folder
     * ========================================================================
     */

    if (item.isFolder()) {

        const QString oldDirectory =
            item.localPath;

        if (oldDirectory.isEmpty()) {
            return false;
        }

        const QString parentDirectory =
            QFileInfo(oldDirectory)
                .absolutePath();

        const QString newDirectory =
            QDir(parentDirectory)
                .filePath(trimmedName);

        if (oldDirectory != newDirectory) {

            if (QFileInfo::exists(
                    newDirectory)) {

                return false;
            }

            if (!moveLocalPath(
                    oldDirectory,
                    newDirectory)) {

                return false;
            }
        }

        item.name =
            trimmedName;

        item.localPath =
            QFileInfo(newDirectory)
                .absoluteFilePath();

        if (!store.updateFile(
                item)) {

            return false;
        }

        /*
         * Folder 内部所有对象的 localPath
         * 都需要跟随目录移动。
         */

        const QVector<FileItem> tree =
            descendants(
                item.id,
                item.space,
                item.sourceId);

        for (FileItem child :
             tree) {

            if (child.localPath.isEmpty()) {
                continue;
            }

            const QString relativePath =
                QDir(oldDirectory)
                    .relativeFilePath(
                        child.localPath);

            child.localPath =
                QDir(newDirectory)
                    .filePath(
                        relativePath);

            if (!store.updateFile(
                    child)) {

                return false;
            }

            emit fileUpdated(
                child.id);
        }

        emit fileUpdated(
            item.id);

        return true;
    }

    return false;
}

// ============================================================================
// Trash
// ============================================================================

bool FileService::moveToTrash(
    const QString &fileId)
{
    FileItem item;

    if (!loadItem(
            fileId,
            item)) {

        return false;
    }

    MetadataStore store;

    if (!store.open(
            m_databasePath)) {

        return false;
    }

    const QString sourcePath =
        item.localPath;

    if (sourcePath.isEmpty()) {
        return false;
    }

    const QString documentsRoot =
        QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation);

    const QString trashRoot =
        QDir(documentsRoot).filePath(
            QStringLiteral(
                "LitApp/.trash"));

    if (!QDir().mkpath(
            trashRoot)) {

        return false;
    }

    /*
     * ========================================================================
     * AtomicFile
     * ========================================================================
     */

    if (item.isAtomicFile()) {

        const QString trashName =
            QStringLiteral("%1_%2")
                .arg(
                    item.name,
                    QString::number(
                        QDateTime::
                            currentMSecsSinceEpoch()));

        const QString trashPath =
            QDir(trashRoot)
                .filePath(trashName);

        if (!moveLocalPath(
                sourcePath,
                trashPath)) {

            return false;
        }

        if (!store.moveToTrash(
                item.id,
                trashPath,
                sourcePath)) {

            moveLocalPath(
                trashPath,
                sourcePath);

            return false;
        }

        emit fileMovedToTrash(
            item.id);

        return true;
    }

    /*
     * ========================================================================
     * AtomicFolder / Folder
     * ========================================================================
     */

    if (item.isAtomicFolder()
        || item.isFolder()) {

        const QString trashName =
            QStringLiteral("%1_%2")
                .arg(
                    item.name,
                    QString::number(
                        QDateTime::
                            currentMSecsSinceEpoch()));

        const QString trashPath =
            QDir(trashRoot)
                .filePath(trashName);

        if (!moveLocalPath(
                sourcePath,
                trashPath)) {

            return false;
        }

        if (!store.moveToTrash(
                item.id,
                trashPath,
                sourcePath)) {

            moveLocalPath(
                trashPath,
                sourcePath);

            return false;
        }

        const QVector<FileItem> tree =
            descendants(
                item.id,
                item.space,
                item.sourceId);

        for (FileItem child :
             tree) {

            if (child.localPath.isEmpty()) {
                continue;
            }

            const QString relativePath =
                QDir(sourcePath)
                    .relativeFilePath(
                        child.localPath);

            const QString childTrashPath =
                QDir(trashPath)
                    .filePath(relativePath);

            if (!store.moveToTrash(
                    child.id,
                    childTrashPath,
                    child.localPath)) {

                return false;
            }
        }

        emit fileMovedToTrash(
            item.id);

        return true;
    }

    return false;
}

// ============================================================================
// Restore
// ============================================================================

bool FileService::restoreFromTrash(
    const QString &fileId)
{
    FileItem item;

    if (!loadItem(
            fileId,
            item)) {

        return false;
    }

    MetadataStore store;

    if (!store.open(
            m_databasePath)) {

        return false;
    }

    const QString originalPath =
        store.originalLocalPath(
            fileId);

    if (originalPath.isEmpty()) {
        return false;
    }

    const QString trashPath =
        item.localPath;

    if (trashPath.isEmpty()) {
        return false;
    }

    if (QFileInfo::exists(
            originalPath)) {

        return false;
    }

    if (!moveLocalPath(
            trashPath,
            originalPath)) {

        return false;
    }

    if (!store.restoreFromTrash(
            fileId,
            originalPath)) {

        moveLocalPath(
            originalPath,
            trashPath);

        return false;
    }

    emit fileRestored(
        fileId);

    return true;
}

// ============================================================================
// Permanent delete
// ============================================================================

bool FileService::permanentlyDelete(
    const QString &fileId)
{
    FileItem item;

    if (!loadItem(
            fileId,
            item)) {

        return false;
    }

    MetadataStore store;

    if (!store.open(
            m_databasePath)) {

        return false;
    }

    /*
     * AtomicFolder：
     *
     * AtomicFile + AtomicFolder 一起永久删除。
     */

    if (item.isAtomicFolder()) {

        const QVector<FileItem> files =
            atomicFilesForFolder(item);

        for (const FileItem &child :
             files) {

            if (!store.permanentlyDelete(
                    child.id)) {

                return false;
            }

            emit fileRemoved(
                child.id);
        }

        const QString localPath =
            item.localPath;

        if (!store.permanentlyDelete(
                item.id)) {

            return false;
        }

        removeLocalPath(
            localPath);

        emit fileRemoved(
            item.id);

        return true;
    }

    /*
     * 普通 Folder：
     *
     * 递归永久删除。
     */

    if (item.isFolder()) {

        const QVector<FileItem> tree =
            descendants(
                item.id,
                item.space,
                item.sourceId);

        for (const FileItem &child :
             tree) {

            if (!store.permanentlyDelete(
                    child.id)) {

                return false;
            }

            emit fileRemoved(
                child.id);
        }

        const QString localPath =
            item.localPath;

        if (!store.permanentlyDelete(
                item.id)) {

            return false;
        }

        removeLocalPath(
            localPath);

        emit fileRemoved(
            item.id);

        return true;
    }

    /*
     * AtomicFile / 其他对象。
     */

    if (!store.permanentlyDelete(
            item.id)) {

        return false;
    }

    removeLocalPath(
        item.localPath);

    emit fileRemoved(
        item.id);

    return true;
}

// ============================================================================
// Descendants
// ============================================================================

QVector<FileItem> FileService::descendants(
    const QString &parentId,
    FileSpace space,
    const QString &sourceId) const
{
    MetadataStore store;

    if (!store.open(
            m_databasePath)) {

        return {};
    }

    QVector<FileItem> result;

    const QVector<FileItem> children =
        store.childrenOf(
            parentId,
            space);

    for (const FileItem &child :
         children) {

        if (child.sourceId != sourceId) {
            continue;
        }

        result.append(child);

        /*
         * 只有普通 Folder 才继续向下递归。
         *
         * AtomicFolder 的 AtomicFile
         * 不需要再通过 descendants()
         * 展开。
         */

        if (child.isFolder()) {

            const QVector<FileItem> nested =
                descendants(
                    child.id,
                    space,
                    sourceId);

            result += nested;
        }
    }

    return result;
}

bool FileService::moveDescendants(
    const QString &oldParentId,
    const QString &newParentId,
    FileSpace space,
    const QString &sourceId)
{
    MetadataStore store;

    if (!store.open(
            m_databasePath)) {

        return false;
    }

    const QVector<FileItem> children =
        store.childrenOf(
            oldParentId,
            space);

    for (FileItem child :
         children) {

        if (child.sourceId != sourceId) {
            continue;
        }

        child.parentId =
            newParentId;

        if (!store.updateFile(
                child)) {

            return false;
        }
    }

    return true;
}

// ============================================================================
// Copy subtree
// ============================================================================

bool FileService::copySubtree(
    const FileItem &source,
    const QString &destinationParentId,
    FileSpace destinationSpace,
    const QString &destinationSourceId,
    QString *newRootId)
{
    MetadataStore store;

    if (!store.open(
            m_databasePath)) {

        return false;
    }

    /*
     * ========================================================================
     * AtomicFile
     * ========================================================================
     *
     * 注意：
     *
     * AtomicFile 单独复制时，
     * 它不再属于原来的 AtomicFolder。
     *
     * 因此这里创建的是一个独立 AtomicFile metadata。
     */

    if (source.isAtomicFile()) {

        if (source.localPath.isEmpty()
            || !QFileInfo(
                    source.localPath).isFile()) {

            return false;
        }

        QString destinationDirectory;

        if (destinationParentId.isEmpty()) {

            const QString documentsRoot =
                QStandardPaths::
                    writableLocation(
                        QStandardPaths::
                            DocumentsLocation);

            destinationDirectory =
                QDir(documentsRoot).filePath(
                    QStringLiteral(
                        "LitApp/zesbks/%1")
                        .arg(
                            spaceRootName(
                                destinationSpace)));

        } else {

            FileItem parent;

            if (!loadItem(
                    destinationParentId,
                    parent)) {

                return false;
            }

            if (!parent.isFolder()) {
                return false;
            }

            destinationDirectory =
                parent.localPath;
        }

        if (!QDir().mkpath(
                destinationDirectory)) {

            return false;
        }

        const QString newName =
            uniqueCopyName(
                source.name,
                destinationParentId,
                destinationSpace,
                destinationSourceId);

        const QString destinationPath =
            QDir(destinationDirectory)
                .filePath(newName);

        if (!copyLocalFile(
                source.localPath,
                destinationPath)) {

            return false;
        }

        FileItem copy =
            source;

        copy.id =
            QUuid::createUuid().toString(
                QUuid::WithoutBraces);

        copy.parentId =
            destinationParentId;

        copy.name =
            newName;

        copy.space =
            destinationSpace;

        copy.sourceId =
            destinationSourceId;

        copy.localPath =
            QFileInfo(destinationPath)
                .absoluteFilePath();

        copy.remotePath =
            QStringLiteral(
                "/zesbks/%1")
                .arg(copy.name);

        copy.syncState =
            SyncState::LocalOnly;

        if (!store.addFile(
                copy)) {

            removeLocalPath(
                destinationPath);

            return false;
        }

        if (newRootId) {
            *newRootId =
                copy.id;
        }

        return true;
    }

    /*
     * ========================================================================
     * AtomicFolder
     * ========================================================================
     */

    if (source.isAtomicFolder()) {

        if (source.localPath.isEmpty()
            || !QFileInfo(
                    source.localPath).isDir()) {

            return false;
        }

        QString destinationDirectory;

        if (destinationParentId.isEmpty()) {

            const QString documentsRoot =
                QStandardPaths::
                    writableLocation(
                        QStandardPaths::
                            DocumentsLocation);

            destinationDirectory =
                QDir(documentsRoot).filePath(
                    QStringLiteral(
                        "LitApp/zesbks/%1")
                        .arg(
                            spaceRootName(
                                destinationSpace)));

        } else {

            FileItem parent;

            if (!loadItem(
                    destinationParentId,
                    parent)) {

                return false;
            }

            if (!parent.isFolder()) {
                return false;
            }

            destinationDirectory =
                parent.localPath;
        }

        if (!QDir().mkpath(
                destinationDirectory)) {

            return false;
        }

        const QString newFolderName =
            uniqueCopyName(
                source.name,
                destinationParentId,
                destinationSpace,
                destinationSourceId);

        const QString destinationFolderPath =
            QDir(destinationDirectory)
                .filePath(
                    newFolderName);

        if (!copyLocalDirectory(
                source.localPath,
                destinationFolderPath)) {

            return false;
        }

        FileItem newFolder =
            source;

        newFolder.id =
            QUuid::createUuid().toString(
                QUuid::WithoutBraces);

        newFolder.parentId =
            destinationParentId;

        newFolder.name =
            newFolderName;

        newFolder.space =
            destinationSpace;

        newFolder.sourceId =
            destinationSourceId;

        newFolder.localPath =
            QFileInfo(
                destinationFolderPath)
                .absoluteFilePath();

        newFolder.remotePath =
            QStringLiteral(
                "/zesbks/%1")
                .arg(newFolder.name);

        newFolder.syncState =
            SyncState::LocalOnly;

        if (!store.addFile(
                newFolder)) {

            removeLocalPath(
                destinationFolderPath);

            return false;
        }

        const QVector<FileItem> files =
            atomicFilesForFolder(source);

        for (const FileItem &sourceFile :
             files) {

            FileItem newFile =
                sourceFile;

            newFile.id =
                QUuid::createUuid().toString(
                    QUuid::WithoutBraces);

            newFile.parentId =
                newFolder.id;

            newFile.space =
                destinationSpace;

            newFile.sourceId =
                destinationSourceId;

            newFile.localPath =
                buildAtomicFileLocalPath(
                    destinationFolderPath,
                    newFile.name);

            newFile.remotePath =
                QStringLiteral(
                    "/zesbks/%1/%2")
                    .arg(
                        newFolder.name,
                        newFile.name);

            newFile.syncState =
                SyncState::LocalOnly;

            if (!store.addFile(
                    newFile)) {

                removeLocalPath(
                    destinationFolderPath);

                store.removeFile(
                    newFolder.id);

                return false;
            }
        }

        if (newRootId) {
            *newRootId =
                newFolder.id;
        }

        return true;
    }

    /*
     * ========================================================================
     * 普通 Folder
     * ========================================================================
     */

    if (source.isFolder()) {

        if (source.localPath.isEmpty()
            || !QFileInfo(
                    source.localPath).isDir()) {

            return false;
        }

        QString destinationDirectory;

        if (destinationParentId.isEmpty()) {

            const QString documentsRoot =
                QStandardPaths::
                    writableLocation(
                        QStandardPaths::
                            DocumentsLocation);

            destinationDirectory =
                QDir(documentsRoot).filePath(
                    QStringLiteral(
                        "LitApp/zesbks/%1")
                        .arg(
                            spaceRootName(
                                destinationSpace)));

        } else {

            FileItem parent;

            if (!loadItem(
                    destinationParentId,
                    parent)) {

                return false;
            }

            if (!parent.isFolder()) {
                return false;
            }

            destinationDirectory =
                parent.localPath;
        }

        if (!QDir().mkpath(
                destinationDirectory)) {

            return false;
        }

        const QString newFolderName =
            uniqueCopyName(
                source.name,
                destinationParentId,
                destinationSpace,
                destinationSourceId);

        const QString destinationFolder =
            QDir(destinationDirectory)
                .filePath(
                    newFolderName);

        if (!copyLocalDirectory(
                source.localPath,
                destinationFolder)) {

            return false;
        }

        FileItem newFolder =
            source;

        newFolder.id =
            QUuid::createUuid().toString(
                QUuid::WithoutBraces);

        newFolder.parentId =
            destinationParentId;

        newFolder.name =
            newFolderName;

        newFolder.space =
            destinationSpace;

        newFolder.sourceId =
            destinationSourceId;

        newFolder.localPath =
            QFileInfo(destinationFolder)
                .absoluteFilePath();

        newFolder.remotePath =
            QStringLiteral(
                "/zesbks/%1")
                .arg(newFolder.name);

        newFolder.syncState =
            SyncState::LocalOnly;

        if (!store.addFile(
                newFolder)) {

            removeLocalPath(
                destinationFolder);

            return false;
        }

        const QVector<FileItem> children =
            store.childrenOf(
                source.id,
                source.space);

        for (const FileItem &child :
             children) {

            QString childNewId;

            if (!copySubtree(
                    child,
                    newFolder.id,
                    destinationSpace,
                    destinationSourceId,
                    &childNewId)) {

                removeLocalPath(
                    destinationFolder);

                /*
                 * 已经写入的子对象也需要删除。
                 */

                const QVector<FileItem> created =
                    descendants(
                        newFolder.id,
                        destinationSpace,
                        destinationSourceId);

                for (const FileItem &createdChild :
                     created) {

                    store.removeFile(
                        createdChild.id);
                }

                store.removeFile(
                    newFolder.id);

                return false;
            }
        }

        if (newRootId) {
            *newRootId =
                newFolder.id;
        }

        return true;
    }

    return false;
}

// ============================================================================
// Copy item
// ============================================================================

bool FileService::copyItem(
    const QString &fileId,
    int destinationSpace,
    const QString &destinationParentId,
    const QString &destinationSourceId)
{
    if (!isValidSpace(
            destinationSpace)) {

        return false;
    }

    const FileSpace targetSpace =
        toFileSpace(
            destinationSpace);

    if (targetSpace == FileSpace::GroupUs
        && destinationSourceId.trimmed().isEmpty()) {

        return false;
    }

    FileItem source;

    if (!loadItem(
            fileId,
            source)) {

        return false;
    }

    QString newRootId;

    if (!copySubtree(
            source,
            destinationParentId,
            targetSpace,
            destinationSourceId,
            &newRootId)) {

        return false;
    }

    emit fileCopied(
        newRootId,
        fileId);

    return true;
}

// ============================================================================
// Move item
// ============================================================================

bool FileService::moveItemTo(
    const QString &fileId,
    int destinationSpace,
    const QString &destinationParentId,
    const QString &destinationSourceId)
{
    if (!isValidSpace(
            destinationSpace)) {

        return false;
    }

    const FileSpace targetSpace =
        toFileSpace(
            destinationSpace);

    if (targetSpace == FileSpace::GroupUs
        && destinationSourceId.trimmed().isEmpty()) {

        return false;
    }

    FileItem item;

    if (!loadItem(
            fileId,
            item)) {

        return false;
    }

    MetadataStore store;

    if (!store.open(
            m_databasePath)) {

        return false;
    }

    const QString oldParentId =
        item.parentId;

    /*
     * 必须在修改 item.space / sourceId
     * 之前保存旧值。
     */

    const FileSpace oldSpace =
        item.space;

    const QString oldSourceId =
        item.sourceId;

    /*
     * ========================================================================
     * AtomicFile
     * ========================================================================
     */

    if (item.isAtomicFile()) {

        QString destinationDirectory;

        if (destinationParentId.isEmpty()) {

            const QString documentsRoot =
                QStandardPaths::
                    writableLocation(
                        QStandardPaths::
                            DocumentsLocation);

            destinationDirectory =
                QDir(documentsRoot).filePath(
                    QStringLiteral(
                        "LitApp/zesbks/%1")
                        .arg(
                            spaceRootName(
                                targetSpace)));

        } else {

            FileItem parent;

            if (!loadItem(
                    destinationParentId,
                    parent)) {

                return false;
            }

            if (!parent.isFolder()) {
                return false;
            }

            destinationDirectory =
                parent.localPath;
        }

        if (!QDir().mkpath(
                destinationDirectory)) {

            return false;
        }

        const QString newName =
            uniqueName(
                item.name,
                destinationParentId,
                targetSpace,
                destinationSourceId);

        const QString destinationPath =
            QDir(destinationDirectory)
                .filePath(newName);

        if (!moveLocalPath(
                item.localPath,
                destinationPath)) {

            return false;
        }

        item.parentId =
            destinationParentId;

        item.space =
            targetSpace;

        item.sourceId =
            destinationSourceId;

        item.name =
            newName;

        item.localPath =
            QFileInfo(destinationPath)
                .absoluteFilePath();

        item.remotePath =
            QStringLiteral(
                "/zesbks/%1")
                .arg(item.name);

        if (!store.updateFile(
                item)) {

            return false;
        }

        emit fileMoved(
            item.id,
            oldParentId,
            destinationParentId);

        return true;
    }

    /*
     * ========================================================================
     * AtomicFolder
     * ========================================================================
     */

    if (item.isAtomicFolder()) {

        QString destinationDirectory;

        if (destinationParentId.isEmpty()) {

            const QString documentsRoot =
                QStandardPaths::
                    writableLocation(
                        QStandardPaths::
                            DocumentsLocation);

            destinationDirectory =
                QDir(documentsRoot).filePath(
                    QStringLiteral(
                        "LitApp/zesbks/%1")
                        .arg(
                            spaceRootName(
                                targetSpace)));

        } else {

            FileItem parent;

            if (!loadItem(
                    destinationParentId,
                    parent)) {

                return false;
            }

            if (!parent.isFolder()) {
                return false;
            }

            destinationDirectory =
                parent.localPath;
        }

        if (!QDir().mkpath(
                destinationDirectory)) {

            return false;
        }

        const QString newName =
            uniqueName(
                item.name,
                destinationParentId,
                targetSpace,
                destinationSourceId);

        const QString destinationFolder =
            QDir(destinationDirectory)
                .filePath(newName);

        if (!moveLocalPath(
                item.localPath,
                destinationFolder)) {

            return false;
        }

        item.parentId =
            destinationParentId;

        item.space =
            targetSpace;

        item.sourceId =
            destinationSourceId;

        item.name =
            newName;

        item.localPath =
            QFileInfo(destinationFolder)
                .absoluteFilePath();

        item.remotePath =
            QStringLiteral(
                "/zesbks/%1")
                .arg(item.name);

        if (!store.updateFile(
                item)) {

            return false;
        }

        const QVector<FileItem> files =
            atomicFilesForFolder(item);

        for (FileItem child :
             files) {

            child.space =
                targetSpace;

            child.sourceId =
                destinationSourceId;

            child.localPath =
                buildAtomicFileLocalPath(
                    item.localPath,
                    child.name);

            child.remotePath =
                QStringLiteral(
                    "/zesbks/%1/%2")
                    .arg(
                        item.name,
                        child.name);

            if (!store.updateFile(
                    child)) {

                return false;
            }

            emit fileUpdated(
                child.id);
        }

        emit fileMoved(
            item.id,
            oldParentId,
            destinationParentId);

        return true;
    }

    /*
     * ========================================================================
     * 普通 Folder
     * ========================================================================
     */

    if (item.isFolder()) {

        /*
         * 防止把 Folder 移动到自己的子目录。
         */

        if (!destinationParentId.isEmpty()) {

            const QVector<FileItem> tree =
                descendants(
                    item.id,
                    oldSpace,
                    oldSourceId);

            for (const FileItem &child :
                 tree) {

                if (child.id
                    == destinationParentId) {

                    return false;
                }
            }
        }

        QString destinationDirectory;

        if (destinationParentId.isEmpty()) {

            const QString documentsRoot =
                QStandardPaths::
                    writableLocation(
                        QStandardPaths::
                            DocumentsLocation);

            destinationDirectory =
                QDir(documentsRoot).filePath(
                    QStringLiteral(
                        "LitApp/zesbks/%1")
                        .arg(
                            spaceRootName(
                                targetSpace)));

        } else {

            FileItem parent;

            if (!loadItem(
                    destinationParentId,
                    parent)) {

                return false;
            }

            if (!parent.isFolder()) {
                return false;
            }

            destinationDirectory =
                parent.localPath;
        }

        if (!QDir().mkpath(
                destinationDirectory)) {

            return false;
        }

        const QString newName =
            uniqueName(
                item.name,
                destinationParentId,
                targetSpace,
                destinationSourceId);

        const QString oldDirectory =
            item.localPath;

        const QString destinationFolder =
            QDir(destinationDirectory)
                .filePath(newName);

        if (!moveLocalPath(
                oldDirectory,
                destinationFolder)) {

            return false;
        }

        /*
         * 在更新 metadata 之前，
         * 先保存旧目录位置。
         */

        item.parentId =
            destinationParentId;

        item.space =
            targetSpace;

        item.sourceId =
            destinationSourceId;

        item.name =
            newName;

        item.localPath =
            QFileInfo(destinationFolder)
                .absoluteFilePath();

        item.remotePath =
            QStringLiteral(
                "/zesbks/%1")
                .arg(item.name);

        if (!store.updateFile(
                item)) {

            return false;
        }

        /*
         * ====================================================================
         * 更新整个子树
         * ====================================================================
         *
         * 注意：
         *
         * descendants() 必须使用：
         *
         *     oldSpace
         *     oldSourceId
         *
         * 因为 item 已经改成 targetSpace。
         */

        const QVector<FileItem> descendantsList =
            descendants(
                fileId,
                oldSpace,
                oldSourceId);

        for (FileItem child :
             descendantsList) {

            if (!child.localPath.isEmpty()) {

                /*
                 * child.localPath 原本在：
                 *
                 *     oldDirectory/...
                 *
                 * 现在整个目录已经移动到：
                 *
                 *     destinationFolder/...
                 */

                const QString relativePath =
                    QDir(oldDirectory)
                        .relativeFilePath(
                            child.localPath);

                child.localPath =
                    QDir(destinationFolder)
                        .filePath(
                            relativePath);
            }

            child.space =
                targetSpace;

            child.sourceId =
                destinationSourceId;

            /*
             * AtomicFile / AtomicFolder 的 remotePath
             * 需要跟随 AtomicFolder 名称变化。
             */

            if (child.isAtomicFolder()) {

                child.remotePath =
                    QStringLiteral(
                        "/zesbks/%1")
                        .arg(child.name);

            } else if (child.isAtomicFile()) {

                FileItem atomicFolder =
                    findAtomicFolderForFile(
                        child);

                if (atomicFolder.isValid()) {

                    child.remotePath =
                        QStringLiteral(
                            "/zesbks/%1/%2")
                            .arg(
                                atomicFolder.name,
                                child.name);
                }
            }

            if (!store.updateFile(
                    child)) {

                return false;
            }

            emit fileUpdated(
                child.id);
        }

        emit fileMoved(
            item.id,
            oldParentId,
            destinationParentId);

        return true;
    }

    return false;
}

// ============================================================================
// Query helpers
// ============================================================================

QVariantMap FileService::fileInfo(
    const QString &fileId) const
{
    FileItem item;

    if (!loadItem(
            fileId,
            item)) {

        return {};
    }

    QVariantMap result;

    result.insert(
        QStringLiteral("fileId"),
        item.id);

    result.insert(
        QStringLiteral("fileName"),
        item.name);

    result.insert(
        QStringLiteral("parentId"),
        item.parentId);

    result.insert(
        QStringLiteral("space"),
        static_cast<int>(
            item.space));

    result.insert(
        QStringLiteral("objectType"),
        static_cast<int>(
            item.objectType));

    result.insert(
        QStringLiteral("sourceId"),
        item.sourceId);

    result.insert(
        QStringLiteral("localPath"),
        item.localPath);

    result.insert(
        QStringLiteral("remotePath"),
        item.remotePath);

    result.insert(
        QStringLiteral("extension"),
        item.extension);

    result.insert(
        QStringLiteral("size"),
        item.size);

    result.insert(
        QStringLiteral("modifiedTime"),
        item.modifiedTime);

    result.insert(
        QStringLiteral("category"),
        static_cast<int>(
            item.category));

    result.insert(
        QStringLiteral("tags"),
        item.tags);

    result.insert(
        QStringLiteral("annotationsPath"),
        item.annotationsPath);

    result.insert(
        QStringLiteral("syncState"),
        static_cast<int>(
            item.syncState));

    result.insert(
        QStringLiteral("syncError"),
        item.syncError);

    result.insert(
        QStringLiteral("isFolder"),
        item.isFolder());

    result.insert(
        QStringLiteral("isAtomicFolder"),
        item.isAtomicFolder());

    result.insert(
        QStringLiteral("isAtomicFile"),
        item.isAtomicFile());

    return result;
}

QVariantList FileService::children(
    const QString &parentId,
    int space,
    const QString &sourceId) const
{
    if (!isValidSpace(space)) {
        return {};
    }

    MetadataStore store;

    if (!store.open(
            m_databasePath)) {

        return {};
    }

    const QVector<FileItem> items =
        store.childrenOf(
            parentId,
            toFileSpace(space));

    QVariantList result;

    for (const FileItem &item :
         items) {

        if (!sourceId.isEmpty()
            && item.sourceId != sourceId) {

            continue;
        }

        QVariantMap map;

        map.insert(
            QStringLiteral("fileId"),
            item.id);

        map.insert(
            QStringLiteral("fileName"),
            item.name);

        map.insert(
            QStringLiteral("parentId"),
            item.parentId);

        map.insert(
            QStringLiteral("space"),
            static_cast<int>(
                item.space));

        map.insert(
            QStringLiteral("objectType"),
            static_cast<int>(
                item.objectType));

        map.insert(
            QStringLiteral("sourceId"),
            item.sourceId);

        map.insert(
            QStringLiteral("localPath"),
            item.localPath);

        map.insert(
            QStringLiteral("remotePath"),
            item.remotePath);

        map.insert(
            QStringLiteral("extension"),
            item.extension);

        map.insert(
            QStringLiteral("size"),
            item.size);

        map.insert(
            QStringLiteral("modifiedTime"),
            item.modifiedTime);

        map.insert(
            QStringLiteral("isFolder"),
            item.isFolder());

        map.insert(
            QStringLiteral("isAtomicFolder"),
            item.isAtomicFolder());

        map.insert(
            QStringLiteral("isAtomicFile"),
            item.isAtomicFile());

        result.append(map);
    }

    return result;
}

QVariantList FileService::rootItems(
    int space,
    const QString &sourceId) const
{
    if (!isValidSpace(space)) {
        return {};
    }

    MetadataStore store;

    if (!store.open(
            m_databasePath)) {

        return {};
    }

    const QVector<FileItem> items =
        store.rootFiles(
            toFileSpace(space));

    QVariantList result;

    for (const FileItem &item :
         items) {

        if (!sourceId.isEmpty()
            && item.sourceId != sourceId) {

            continue;
        }

        QVariantMap map;

        map.insert(
            QStringLiteral("fileId"),
            item.id);

        map.insert(
            QStringLiteral("fileName"),
            item.name);

        map.insert(
            QStringLiteral("parentId"),
            item.parentId);

        map.insert(
            QStringLiteral("space"),
            static_cast<int>(
                item.space));

        map.insert(
            QStringLiteral("objectType"),
            static_cast<int>(
                item.objectType));

        map.insert(
            QStringLiteral("sourceId"),
            item.sourceId);

        map.insert(
            QStringLiteral("localPath"),
            item.localPath);

        map.insert(
            QStringLiteral("remotePath"),
            item.remotePath);

        map.insert(
            QStringLiteral("extension"),
            item.extension);

        map.insert(
            QStringLiteral("size"),
            item.size);

        map.insert(
            QStringLiteral("modifiedTime"),
            item.modifiedTime);

        map.insert(
            QStringLiteral("isFolder"),
            item.isFolder());

        map.insert(
            QStringLiteral("isAtomicFolder"),
            item.isAtomicFolder());

        map.insert(
            QStringLiteral("isAtomicFile"),
            item.isAtomicFile());

        result.append(map);
    }

    return result;
}

QVariantList FileService::trashItems(
    int space,
    const QString &sourceId) const
{
    if (!isValidSpace(space)) {
        return {};
    }

    MetadataStore store;

    if (!store.open(
            m_databasePath)) {

        return {};
    }

    const QVector<FileItem> items =
        store.trashFiles(
            toFileSpace(space));

    QVariantList result;

    for (const FileItem &item :
         items) {

        if (!sourceId.isEmpty()
            && item.sourceId != sourceId) {

            continue;
        }

        QVariantMap map;

        map.insert(
            QStringLiteral("fileId"),
            item.id);

        map.insert(
            QStringLiteral("fileName"),
            item.name);

        map.insert(
            QStringLiteral("parentId"),
            item.parentId);

        map.insert(
            QStringLiteral("space"),
            static_cast<int>(
                item.space));

        map.insert(
            QStringLiteral("objectType"),
            static_cast<int>(
                item.objectType));

        map.insert(
            QStringLiteral("sourceId"),
            item.sourceId);

        map.insert(
            QStringLiteral("localPath"),
            item.localPath);

        map.insert(
            QStringLiteral("remotePath"),
            item.remotePath);

        map.insert(
            QStringLiteral("extension"),
            item.extension);

        map.insert(
            QStringLiteral("size"),
            item.size);

        map.insert(
            QStringLiteral("modifiedTime"),
            item.modifiedTime);

        result.append(map);
    }

    return result;
}

// ============================================================================
// Type helpers
// ============================================================================

bool FileService::isFolder(
    const QString &fileId) const
{
    FileItem item;

    return loadItem(
               fileId,
               item)
        && item.isFolder();
}

bool FileService::isAtomicFolder(
    const QString &fileId) const
{
    FileItem item;

    return loadItem(
               fileId,
               item)
        && item.isAtomicFolder();
}

bool FileService::isAtomicFile(
    const QString &fileId) const
{
    FileItem item;

    return loadItem(
               fileId,
               item)
        && item.isAtomicFile();
}

int FileService::spaceOf(
    const QString &fileId) const
{
    FileItem item;

    if (!loadItem(
            fileId,
            item)) {

        return -1;
    }

    return static_cast<int>(
        item.space);
}

QString FileService::sourceIdOf(
    const QString &fileId) const
{
    FileItem item;

    if (!loadItem(
            fileId,
            item)) {

        return {};
    }

    return item.sourceId;
}

} // namespace litapp