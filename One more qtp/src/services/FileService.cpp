#include "services/FileService.h"

#include "core/AppLayout.h"
#include "storage/MetadataStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QUrl>
#include <QUuid>
#include <QVariantMap>

namespace litapp {

namespace {

const QString kTrashDirectoryName =
    QStringLiteral(".trash");

QString normalizedTagList(const QString &tagsCsv)
{
    Q_UNUSED(tagsCsv)
    return QString();
}

} // namespace

// ============================================================================
// Constructor
// ============================================================================

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

bool FileService::openStore(
    MetadataStore &store) const
{
    return store.open(m_databasePath);
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

// ============================================================================
// 远端根
// ============================================================================

QString FileService::remoteRootFor(
    FileSpace space,
    const QString &groupId) const
{
    switch (space) {

    case FileSpace::MyDoc:
        return layout::remoteShareRoot();

    case FileSpace::GroupMe:
        return layout::remoteGroupRoot()
            + QLatin1Char('/')
            + (groupId.trimmed().isEmpty()
                   ? layout::groupMeDirectoryName(0)
                   : groupId.trimmed());

    case FileSpace::GroupUs:
        /*
         * 他人账号的群组根由 GroupSource 配置决定，
         * FileService 不在这里推导。
         */
        return QString();
    }

    return QString();
}

QString FileService::remoteFolderPath(
    FileSpace space,
    const QString &groupId,
    const QString &atomicFolderName) const
{
    const QString root =
        remoteRootFor(space, groupId);

    if (root.isEmpty()) {
        return QString();
    }

    return layout::remoteAtomicFolderPath(
        root,
        atomicFolderName);
}

QString FileService::remoteFilePath(
    FileSpace space,
    const QString &groupId,
    const QString &atomicFolderName,
    const QString &atomicFileName) const
{
    const QString root =
        remoteRootFor(space, groupId);

    if (root.isEmpty()) {
        return QString();
    }

    return layout::remoteAtomicFilePath(
        root,
        atomicFolderName,
        atomicFileName);
}

// ============================================================================
// 本地镜像
// ============================================================================

QString FileService::mirrorFolderPath(
    const QString &remoteFolderPath,
    const QString &sourceId) const
{
    if (remoteFolderPath.isEmpty()) {
        return QString();
    }

    return layout::localPathForRemote(
        remoteFolderPath,
        sourceId);
}

QString FileService::mirrorFilePath(
    const QString &remoteFolderPath,
    const QString &sourceId,
    const QString &fileName) const
{
    const QString folder =
        mirrorFolderPath(
            remoteFolderPath,
            sourceId);

    if (folder.isEmpty() || fileName.isEmpty()) {
        return QString();
    }

    return QDir(folder).filePath(fileName);
}

// ============================================================================
// 本地文件系统
// ============================================================================

bool FileService::copyLocalFile(
    const QString &sourcePath,
    const QString &destinationPath) const
{
    const QFileInfo sourceInfo(sourcePath);

    if (!sourceInfo.exists()
        || !sourceInfo.isFile()) {

        return false;
    }

    if (QFileInfo::exists(destinationPath)) {
        return false;
    }

    if (!QDir().mkpath(
            QFileInfo(destinationPath).absolutePath())) {

        return false;
    }

    return QFile::copy(
        sourcePath,
        destinationPath);
}

bool FileService::removeLocalPath(
    const QString &path) const
{
    if (path.isEmpty()) {
        return true;
    }

    const QFileInfo info(path);

    if (!info.exists()) {
        return true;
    }

    if (info.isDir()) {
        return QDir(path).removeRecursively();
    }

    return QFile::remove(path);
}

static bool moveLocalPathStatic(
    const QString &sourcePath,
    const QString &destinationPath)
{
    if (sourcePath.isEmpty()
        || destinationPath.isEmpty()) {

        return false;
    }

    const QString source =
        QFileInfo(sourcePath).absoluteFilePath();

    const QString destination =
        QFileInfo(destinationPath).absoluteFilePath();

    if (source == destination) {
        return true;
    }

    if (!QFileInfo::exists(source)) {
        return false;
    }

    if (QFileInfo::exists(destination)) {
        return false;
    }

    if (!QDir().mkpath(
            QFileInfo(destination).absolutePath())) {

        return false;
    }

    if (QFile::rename(source, destination)) {
        return true;
    }

    // 跨卷回退。
    const QFileInfo info(source);

    if (info.isDir()) {

        if (!QDir(source).rename(source, destination)) {
            return false;
        }

        return true;
    }

    if (!QFile::copy(source, destination)) {
        return false;
    }

    if (!QFile::remove(source)) {

        QFile::remove(destination);
        return false;
    }

    return true;
}

// ============================================================================
// 元数据
// ============================================================================

bool FileService::loadItem(
    const QString &fileId,
    FileItem &item) const
{
    MetadataStore store;

    if (!openStore(store)) {
        return false;
    }

    item = store.fileById(fileId);

    return item.isValid();
}

bool FileService::saveItem(
    const FileItem &item)
{
    MetadataStore store;

    if (!openStore(store)) {
        return false;
    }

    return store.updateFile(item);
}

QVector<FileItem> FileService::descendants(
    const QString &parentId,
    FileSpace space,
    const QString &sourceId,
    const QString &groupId) const
{
    QVector<FileItem> result;

    MetadataStore store;

    if (!openStore(store)) {
        return result;
    }

    QList<QString> queue;
    queue.append(parentId);

    QSet<QString> visited;

    while (!queue.isEmpty()) {

        const QString current =
            queue.takeFirst();

        if (visited.contains(current)) {
            continue;
        }

        visited.insert(current);

        const QVector<FileItem> children =
            store.childrenOf(current, space);

        for (const FileItem &child : children) {

            if (child.sourceId != sourceId) {
                continue;
            }

            if (space == FileSpace::GroupMe
                && child.groupId != groupId) {
                continue;
            }

            result.append(child);

            queue.append(child.id);
        }
    }

    return result;
}

// ============================================================================
// 名称唯一化
// ============================================================================

QString FileService::uniqueName(
    const QString &name,
    const QString &parentId,
    FileSpace space,
    const QString &sourceId,
    const QString &groupId) const
{
    MetadataStore store;

    if (!openStore(store)) {
        return name;
    }

    QVector<FileItem> siblings;

    const QVector<FileItem> raw =
        parentId.isEmpty()
            ? store.rootFiles(space)
            : store.childrenOf(parentId, space);

    for (const FileItem &item : raw) {

        if (item.sourceId != sourceId) {
            continue;
        }

        if (space == FileSpace::GroupMe
            && item.groupId != groupId) {
            continue;
        }

        siblings.append(item);
    }

    const auto exists =
        [&siblings](const QString &candidate) {

            for (const FileItem &item : siblings) {

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
        info.completeBaseName().isEmpty()
            ? name
            : info.completeBaseName();

    const QString suffix = info.suffix();

    for (int i = 2; ; ++i) {

        const QString candidate =
            suffix.isEmpty()
                ? QStringLiteral("%1 (%2)")
                      .arg(baseName)
                      .arg(i)
                : QStringLiteral("%1 (%2).%3")
                      .arg(baseName)
                      .arg(i)
                      .arg(suffix);

        if (!exists(candidate)) {
            return candidate;
        }
    }
}

QString FileService::uniqueCopyName(
    const QString &name,
    const QString &parentId,
    FileSpace space,
    const QString &sourceId,
    const QString &groupId) const
{
    return uniqueName(
        name + QStringLiteral(" copy"),
        parentId,
        space,
        sourceId,
        groupId);
}

// ============================================================================
// 导入
// ============================================================================

bool FileService::addLocalFile(
    const QString &filePath,
    int space,
    const QString &parentId,
    const QString &sourceId,
    const QString &groupId,
    int category,
    const QString &tagsCsv)
{
    if (!isValidSpace(space)) {
        return false;
    }

    const FileSpace fileSpace =
        toFileSpace(space);

    /*
     * 他人账号是只读的，
     * 不允许把本地文件导入进去。
     */
    if (fileSpace == FileSpace::GroupUs) {
        return false;
    }

    // 兼容 QML FileDialog 的 file:// URL。
    QString sourceFilePath = filePath.trimmed();

    if (sourceFilePath.startsWith(
            QStringLiteral("file:"))) {

        sourceFilePath =
            QUrl(sourceFilePath).toLocalFile();
    }

    const QFileInfo sourceInfo(sourceFilePath);

    if (!sourceInfo.exists()
        || !sourceInfo.isFile()) {

        return false;
    }

    const QString effectiveGroupId =
        fileSpace == FileSpace::GroupMe
            ? (groupId.trimmed().isEmpty()
                   ? layout::groupMeDirectoryName(0)
                   : groupId.trimmed())
            : QString();

    // ---- 标签 ----

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

    // ---- 原子文件夹名称 ----

    QString atomicFolderName =
        sourceInfo.completeBaseName();

    if (atomicFolderName.isEmpty()) {
        atomicFolderName = sourceInfo.fileName();
    }

    atomicFolderName =
        uniqueName(
            atomicFolderName,
            parentId,
            fileSpace,
            sourceId,
            effectiveGroupId);

    const QString remoteFolder =
        remoteFolderPath(
            fileSpace,
            effectiveGroupId,
            atomicFolderName);

    if (remoteFolder.isEmpty()) {
        return false;
    }

    const QString localFolder =
        mirrorFolderPath(remoteFolder, sourceId);

    if (localFolder.isEmpty()) {
        return false;
    }

    // ---- 物理目录 ----

    if (!QDir().mkpath(localFolder)) {
        return false;
    }

    const QString atomicFileName =
        sourceInfo.fileName();

    const QString destinationPath =
        QDir(localFolder).filePath(atomicFileName);

    const QString absoluteSource =
        sourceInfo.absoluteFilePath();

    const QString absoluteDestination =
        QFileInfo(destinationPath).absoluteFilePath();

    if (absoluteSource != absoluteDestination) {

        if (!copyLocalFile(
                absoluteSource,
                absoluteDestination)) {

            removeLocalPath(localFolder);
            return false;
        }
    }

    // ---- 元数据 ----

    const QFileInfo destinationInfo(
        absoluteDestination);

    if (!destinationInfo.exists()
        || !destinationInfo.isFile()) {

        removeLocalPath(localFolder);
        return false;
    }

    MetadataStore store;

    if (!openStore(store)) {

        removeLocalPath(localFolder);
        return false;
    }

    FileItem folder;

    folder.id =
        QUuid::createUuid().toString(
            QUuid::WithoutBraces);

    folder.parentId = parentId;
    folder.name = atomicFolderName;
    folder.objectType = FileObjectType::AtomicFolder;
    folder.space = fileSpace;
    folder.sourceId = sourceId;
    folder.groupId = effectiveGroupId;

    folder.localPath = localFolder;
    folder.remotePath = remoteFolder;

    folder.modifiedTime = QDateTime::currentDateTime();
    folder.category = static_cast<FileCategory>(category);
    folder.tags = tags;
    folder.syncState = SyncState::LocalOnly;

    FileItem file;

    file.id =
        QUuid::createUuid().toString(
            QUuid::WithoutBraces);

    file.parentId = folder.id;
    file.name = atomicFileName;
    file.objectType = FileObjectType::AtomicFile;
    file.space = fileSpace;
    file.sourceId = sourceId;
    file.groupId = effectiveGroupId;

    file.localPath = absoluteDestination;
    file.remotePath =
        QStringLiteral("%1/%2")
            .arg(remoteFolder, atomicFileName);

    file.extension =
        destinationInfo.suffix().toLower();

    file.size = destinationInfo.size();
    file.modifiedTime = destinationInfo.lastModified();
    file.category = static_cast<FileCategory>(category);
    file.tags = tags;
    file.syncState = SyncState::LocalOnly;

    if (!store.addFile(folder)) {

        removeLocalPath(localFolder);
        return false;
    }

    if (!store.addFile(file)) {

        store.removeFile(folder.id);
        removeLocalPath(localFolder);
        return false;
    }

    emit fileAdded(folder.id);
    emit fileAdded(file.id);

    return true;
}

// ============================================================================
// 纯逻辑文件夹
// ============================================================================

bool FileService::createFolder(
    const QString &name,
    int space,
    const QString &parentId,
    const QString &sourceId,
    const QString &groupId)
{
    if (!isValidSpace(space)) {
        return false;
    }

    const QString trimmed = name.trimmed();

    if (trimmed.isEmpty()) {
        return false;
    }

    const FileSpace fileSpace =
        toFileSpace(space);

    if (fileSpace == FileSpace::GroupUs) {
        return false;
    }

    const QString effectiveGroupId =
        fileSpace == FileSpace::GroupMe
            ? (groupId.trimmed().isEmpty()
                   ? layout::groupMeDirectoryName(0)
                   : groupId.trimmed())
            : QString();

    // ---- 父对象必须存在且是文件夹 ----

    if (!parentId.isEmpty()) {

        FileItem parent;

        if (!loadItem(parentId, parent)) {
            return false;
        }

        if (!parent.isFolder()) {
            return false;
        }

        if (parent.space != fileSpace
            || parent.sourceId != sourceId
            || parent.groupId != effectiveGroupId) {

            return false;
        }
    }

    MetadataStore store;

    if (!openStore(store)) {
        return false;
    }

    FileItem folder;

    folder.id =
        QUuid::createUuid().toString(
            QUuid::WithoutBraces);

    folder.parentId = parentId;
    folder.name =
        uniqueName(
            trimmed,
            parentId,
            fileSpace,
            sourceId,
            effectiveGroupId);

    folder.objectType = FileObjectType::Folder;
    folder.space = fileSpace;
    folder.sourceId = sourceId;
    folder.groupId = effectiveGroupId;

    /*
     * 纯逻辑对象：
     *
     *     没有本地目录
     *     没有远端目录
     */
    folder.localPath = QString();
    folder.remotePath = QString();

    folder.modifiedTime = QDateTime::currentDateTime();
    folder.syncState = SyncState::Synced;

    if (!store.addFile(folder)) {
        return false;
    }

    emit fileAdded(folder.id);

    return true;
}

// ============================================================================
// 重命名
// ============================================================================

bool FileService::renameItem(
    const QString &fileId,
    const QString &newName)
{
    const QString trimmed = newName.trimmed();

    if (trimmed.isEmpty()) {
        return false;
    }

    FileItem item;

    if (!loadItem(fileId, item)) {
        return false;
    }

    if (item.isReadOnly()) {
        return false;
    }

    MetadataStore store;

    if (!openStore(store)) {
        return false;
    }

    // ---- 纯逻辑文件夹 ----

    if (item.isFolder()) {

        item.name =
            uniqueName(
                trimmed,
                item.parentId,
                item.space,
                item.sourceId,
                item.groupId);

        if (!store.updateFile(item)) {
            return false;
        }

        emit fileUpdated(item.id);

        return true;
    }

    // ---- 原子文件夹 ----

    if (item.isAtomicFolder()) {

        const QString oldLocal = item.localPath;
        const QString oldRemote = item.remotePath;

        const QString newRemote =
            remoteFolderPath(
                item.space,
                item.groupId,
                trimmed);

        if (newRemote.isEmpty()) {
            return false;
        }

        const QString newLocal =
            mirrorFolderPath(
                newRemote,
                item.sourceId);

        if (newLocal.isEmpty()) {
            return false;
        }

        if (!oldLocal.isEmpty()
            && QFileInfo::exists(oldLocal)
            && QDir::cleanPath(oldLocal)
                != QDir::cleanPath(newLocal)) {

            if (!moveLocalPathStatic(
                    oldLocal,
                    newLocal)) {

                return false;
            }
        }

        item.name = trimmed;
        item.localPath = newLocal;
        item.remotePath = newRemote;

        /*
         * 远端需要重命名，
         * 交给下一次同步处理。
         */
        item.syncState =
            item.localPath.isEmpty() || !QFileInfo::exists(item.localPath)
                ? SyncState::Synced
                : SyncState::Uploading;

        if (!store.updateFile(item)) {
            return false;
        }

        /*
         * 旧远端路径留下墓碑，
         * 下一次同步会把它删掉，
         * 然后上传新名称。
         */
        if (!oldRemote.isEmpty()
            && oldRemote != newRemote) {

            store.addPendingDelete(
                oldRemote,
                item.sourceId);
        }

        // ---- 子原子文件 ----

        const QVector<FileItem> children =
            descendants(
                item.id,
                item.space,
                item.sourceId,
                item.groupId);

        for (FileItem child : children) {

            if (!child.isAtomicFile()) {
                continue;
            }

            if (!oldRemote.isEmpty()
                && !child.remotePath.isEmpty()) {

                store.addPendingDelete(
                    child.remotePath,
                    item.sourceId);
            }

            child.localPath =
                QDir(newLocal).filePath(child.name);

            child.remotePath =
                QStringLiteral("%1/%2")
                    .arg(newRemote, child.name);

            child.syncState = item.syncState;

            store.updateFile(child);

            emit fileUpdated(child.id);
        }

        emit fileUpdated(item.id);

        return true;
    }

    // ---- 原子文件 ----

    if (item.isAtomicFile()) {

        FileItem parent;

        if (!loadItem(item.parentId, parent)
            || !parent.isAtomicFolder()) {

            return false;
        }

        const QString oldRemote = item.remotePath;

        const QString newRemote =
            QStringLiteral("%1/%2")
                .arg(parent.remotePath, trimmed);

        if (!oldRemote.isEmpty()
            && oldRemote != newRemote) {

            store.addPendingDelete(
                oldRemote,
                item.sourceId);
        }

        QString newLocal;

        if (!parent.localPath.isEmpty()) {

            newLocal =
                QDir(parent.localPath)
                    .filePath(trimmed);
        }

        if (!item.localPath.isEmpty()
            && QFileInfo::exists(item.localPath)
            && !newLocal.isEmpty()
            && QDir::cleanPath(item.localPath)
                != QDir::cleanPath(newLocal)) {

            if (!moveLocalPathStatic(
                    item.localPath,
                    newLocal)) {

                return false;
            }
        }

        item.name = trimmed;
        item.extension =
            QFileInfo(trimmed).suffix().toLower();

        if (!newLocal.isEmpty()) {
            item.localPath = newLocal;
        }

        item.remotePath = newRemote;
        item.syncState = SyncState::Uploading;

        if (!store.updateFile(item)) {
            return false;
        }

        emit fileUpdated(item.id);

        return true;
    }

    return false;
}

// ============================================================================
// 移动（纯逻辑）
// ============================================================================

bool FileService::moveItemTo(
    const QString &fileId,
    const QString &destinationParentId)
{
    FileItem item;

    if (!loadItem(fileId, item)) {
        return false;
    }

    if (item.isReadOnly()) {
        return false;
    }

    /*
     * 原子文件必须留在它所属的原子文件夹内，
     * 否则远端结构会被破坏。
     */
    if (item.isAtomicFile()) {
        return false;
    }

    if (fileId == destinationParentId) {
        return false;
    }

    // ---- 目标必须是同一空间内的普通文件夹 ----

    if (!destinationParentId.isEmpty()) {

        FileItem destination;

        if (!loadItem(destinationParentId, destination)) {
            return false;
        }

        if (!destination.isFolder()) {
            return false;
        }

        if (destination.space != item.space
            || destination.sourceId != item.sourceId
            || destination.groupId != item.groupId) {

            return false;
        }

        /*
         * 不允许把文件夹移动到自己的子树里。
         */
        if (isDescendantOf(
                destinationParentId,
                fileId)) {

            return false;
        }
    }

    const QString oldParentId = item.parentId;

    if (oldParentId == destinationParentId) {
        return true;
    }

    /*
     * ========================================================
     * 只改这一个 parentId。
     * ========================================================
     *
     * 子树通过 parentId 隐式跟随，
     * 父节点完全不受影响。
     */

    item.parentId = destinationParentId;

    if (!saveItem(item)) {
        return false;
    }

    emit fileMoved(
        item.id,
        oldParentId,
        destinationParentId);

    emit fileUpdated(item.id);

    return true;
}

bool FileService::isDescendantOf(
    const QString &candidateId,
    const QString &ancestorId) const
{
    if (candidateId.isEmpty()
        || ancestorId.isEmpty()) {

        return false;
    }

    if (candidateId == ancestorId) {
        return true;
    }

    FileItem cursor;

    if (!loadItem(candidateId, cursor)) {
        return false;
    }

    QSet<QString> guard;

    while (!cursor.parentId.isEmpty()) {

        if (cursor.parentId == ancestorId) {
            return true;
        }

        if (guard.contains(cursor.parentId)) {
            return false;
        }

        guard.insert(cursor.parentId);

        FileItem parent;

        if (!loadItem(cursor.parentId, parent)) {
            return false;
        }

        cursor = parent;
    }

    return false;
}

// ============================================================================
// 复制
// ============================================================================

bool FileService::copyItemRecursive(
    const FileItem &source,
    const QString &destinationParentId,
    QString *newRootId)
{
    MetadataStore store;

    if (!openStore(store)) {
        return false;
    }

    // ---- 纯逻辑文件夹 ----

    if (source.isFolder()) {

        FileItem copy = source;

        copy.id =
            QUuid::createUuid().toString(
                QUuid::WithoutBraces);

        copy.parentId = destinationParentId;

        copy.name =
            uniqueCopyName(
                source.name,
                destinationParentId,
                source.space,
                source.sourceId,
                source.groupId);

        copy.syncState = SyncState::Synced;

        if (!store.addFile(copy)) {
            return false;
        }

        const QVector<FileItem> children =
            store.childrenOf(source.id, source.space);

        for (const FileItem &child : children) {

            if (child.sourceId != source.sourceId) {
                continue;
            }

            if (source.space == FileSpace::GroupMe
                && child.groupId != source.groupId) {
                continue;
            }

            QString ignored;

            if (!copyItemRecursive(
                    child,
                    copy.id,
                    &ignored)) {

                return false;
            }
        }

        if (newRootId) {
            *newRootId = copy.id;
        }

        return true;
    }

    // ---- 原子文件 ----

    if (source.isAtomicFile()) {

        FileItem parent;

        if (!loadItem(source.parentId, parent)) {
            return false;
        }

        FileItem copy = source;

        copy.id =
            QUuid::createUuid().toString(
                QUuid::WithoutBraces);

        copy.parentId = destinationParentId;

        copy.name =
            uniqueCopyName(
                source.name,
                destinationParentId,
                source.space,
                source.sourceId,
                source.groupId);

        copy.remotePath =
            parent.remotePath.isEmpty()
                ? source.remotePath
                : QStringLiteral("%1/%2")
                      .arg(parent.remotePath, copy.name);

        if (!parent.localPath.isEmpty()) {

            const QString destination =
                QDir(parent.localPath).filePath(copy.name);

            if (QFileInfo::exists(source.localPath)
                && !copyLocalFile(
                    source.localPath,
                    destination)) {

                return false;
            }

            copy.localPath = destination;
        }

        copy.syncState = SyncState::LocalOnly;

        if (!store.addFile(copy)) {
            return false;
        }

        if (newRootId) {
            *newRootId = copy.id;
        }

        return true;
    }

    // ---- 原子文件夹 ----

    if (source.isAtomicFolder()) {

        const QString newFolderName =
            uniqueCopyName(
                source.name,
                destinationParentId,
                source.space,
                source.sourceId,
                source.groupId);

        const QString newRemote =
            remoteFolderPath(
                source.space,
                source.groupId,
                newFolderName);

        if (newRemote.isEmpty()) {
            return false;
        }

        const QString newLocal =
            mirrorFolderPath(
                newRemote,
                source.sourceId);

        // ---- 物理复制 ----

        if (!source.localPath.isEmpty()
            && QFileInfo::exists(source.localPath)) {

            if (!QDir().mkpath(
                    QFileInfo(newLocal).absolutePath())) {

                return false;
            }

            if (!QDir(source.localPath).exists()) {
                return false;
            }

            const QStringList entries =
                QDir(source.localPath).entryList(
                    QDir::Files | QDir::NoDotAndDotDot,
                    QDir::Name);

            QDir().mkpath(newLocal);

            for (const QString &entry : entries) {

                if (!copyLocalFile(
                        QDir(source.localPath).filePath(entry),
                        QDir(newLocal).filePath(entry))) {

                    return false;
                }
            }
        }

        FileItem newFolder = source;

        newFolder.id =
            QUuid::createUuid().toString(
                QUuid::WithoutBraces);

        newFolder.parentId = destinationParentId;
        newFolder.name = newFolderName;
        newFolder.remotePath = newRemote;
        newFolder.localPath = newLocal;
        newFolder.syncState = SyncState::LocalOnly;

        if (!store.addFile(newFolder)) {
            return false;
        }

        const QVector<FileItem> files =
            store.childrenOf(source.id, source.space);

        for (const FileItem &file : files) {

            if (file.objectType != FileObjectType::AtomicFile) {
                continue;
            }

            QString ignored;

            if (!copyItemRecursive(
                    file,
                    newFolder.id,
                    &ignored)) {

                return false;
            }
        }

        if (newRootId) {
            *newRootId = newFolder.id;
        }

        return true;
    }

    return false;
}

bool FileService::copyItem(
    const QString &fileId,
    const QString &destinationParentId)
{
    FileItem source;

    if (!loadItem(fileId, source)) {
        return false;
    }

    if (source.isReadOnly()) {
        return false;
    }

    if (isDescendantOf(
            destinationParentId,
            fileId)) {

        return false;
    }

    QString newRootId;

    if (!copyItemRecursive(
            source,
            destinationParentId,
            &newRootId)) {

        return false;
    }

    emit fileCopied(newRootId, fileId);

    return true;
}

// ============================================================================
// 删除
// ============================================================================

bool FileService::removeFile(
    const QString &fileId)
{
    return permanentlyDelete(fileId);
}

bool FileService::permanentlyDelete(
    const QString &fileId)
{
    FileItem item;

    if (!loadItem(fileId, item)) {
        return false;
    }

    MetadataStore store;

    if (!openStore(store)) {
        return false;
    }

    QVector<FileItem> tree =
        descendants(
            item.id,
            item.space,
            item.sourceId,
            item.groupId);

    /*
     * 先删子对象，再删自己。
     */
    std::reverse(tree.begin(), tree.end());

    for (const FileItem &child : tree) {

        if (!child.localPath.isEmpty()) {
            removeLocalPath(child.localPath);
        }

        if (!child.isReadOnly()
            && !child.remotePath.isEmpty()) {

            store.addPendingDelete(
                child.remotePath,
                child.sourceId,
                child.objectType
                    != FileObjectType::AtomicFile);
        }

        store.permanentlyDelete(child.id);

        emit fileRemoved(child.id);
    }

    if (item.isAtomicFolder()
        && !item.localPath.isEmpty()) {

        removeLocalPath(item.localPath);
    }

    if (!item.isReadOnly()
        && !item.remotePath.isEmpty()) {

        store.addPendingDelete(
            item.remotePath,
            item.sourceId,
            item.objectType
                != FileObjectType::AtomicFile);
    }

    store.permanentlyDelete(item.id);

    emit fileRemoved(item.id);

    return true;
}

// ============================================================================
// 回收站
// ============================================================================

bool FileService::moveToTrash(
    const QString &fileId)
{
    FileItem item;

    if (!loadItem(fileId, item)) {
        return false;
    }

    MetadataStore store;

    if (!openStore(store)) {
        return false;
    }

    const QString trashRoot =
        QDir(layout::localLibraryRoot())
            .filePath(kTrashDirectoryName);

    if (!QDir().mkpath(trashRoot)) {
        return false;
    }

    QVector<FileItem> tree =
        descendants(
            item.id,
            item.space,
            item.sourceId,
            item.groupId);

    tree.prepend(item);

    // ---- 顶部对象的物理搬迁 ----

    const QString originPath = item.localPath;

    QString trashPath;

    if (!originPath.isEmpty()
        && QFileInfo::exists(originPath)) {

        trashPath =
            QDir(trashRoot).filePath(
                QStringLiteral("%1_%2")
                    .arg(
                        item.id,
                        QFileInfo(originPath).fileName()));

        if (!moveLocalPathStatic(
                originPath,
                trashPath)) {

            return false;
        }
    }

    // ---- 元数据 ----

    for (FileItem node : tree) {

        QString nodeTrashPath;

        if (!node.localPath.isEmpty()
            && !originPath.isEmpty()) {

            const QString relative =
                QDir(originPath)
                    .relativeFilePath(node.localPath);

            nodeTrashPath =
                relative == QStringLiteral(".")
                    ? trashPath
                    : QDir(trashPath).filePath(relative);
        }

        store.moveToTrash(
            node.id,
            nodeTrashPath,
            node.localPath);
    }

    emit fileMovedToTrash(item.id);

    return true;
}

bool FileService::restoreFromTrash(
    const QString &fileId)
{
    FileItem item;

    if (!loadItem(fileId, item)) {
        return false;
    }

    MetadataStore store;

    if (!openStore(store)) {
        return false;
    }

    const QString originalPath =
        store.originalLocalPath(fileId);

    const QString trashPath = item.localPath;

    QString restoredPath;

    if (!originalPath.isEmpty()
        && !trashPath.isEmpty()
        && QFileInfo::exists(trashPath)) {

        if (QFileInfo::exists(originalPath)) {
            return false;
        }

        if (!moveLocalPathStatic(
                trashPath,
                originalPath)) {

            return false;
        }

        restoredPath = originalPath;
    }

    if (!store.restoreFromTrash(
            fileId,
            restoredPath)) {

        return false;
    }

    // ---- 子树一并恢复 ----

    QVector<FileItem> tree =
        descendants(
            item.id,
            item.space,
            item.sourceId,
            item.groupId);

    for (FileItem node : tree) {

        node.localPath =
            restoredPath.isEmpty()
                ? QString()
                : QDir(restoredPath).filePath(
                      QDir(trashPath)
                          .relativeFilePath(node.localPath));

        store.restoreFromTrash(
            node.id,
            node.localPath);
    }

    emit fileRestored(fileId);

    return true;
}

// ============================================================================
// 查询
// ============================================================================

static QVariantMap itemToMap(const FileItem &item)
{
    QVariantMap map;

    map.insert(QStringLiteral("fileId"), item.id);
    map.insert(QStringLiteral("fileName"), item.name);
    map.insert(QStringLiteral("parentId"), item.parentId);
    map.insert(QStringLiteral("space"), static_cast<int>(item.space));
    map.insert(QStringLiteral("sourceId"), item.sourceId);
    map.insert(QStringLiteral("groupId"), item.groupId);
    map.insert(QStringLiteral("objectType"), static_cast<int>(item.objectType));
    map.insert(QStringLiteral("localPath"), item.localPath);
    map.insert(QStringLiteral("remotePath"), item.remotePath);
    map.insert(QStringLiteral("extension"), item.extension);
    map.insert(QStringLiteral("size"), item.size);
    map.insert(QStringLiteral("modifiedTime"), item.modifiedTime);
    map.insert(QStringLiteral("category"), static_cast<int>(item.category));
    map.insert(QStringLiteral("categoryText"), fileCategoryToString(item.category));
    map.insert(QStringLiteral("tags"), item.tags);
    map.insert(QStringLiteral("annotationsPath"), item.annotationsPath);
    map.insert(QStringLiteral("syncState"), static_cast<int>(item.syncState));
    map.insert(QStringLiteral("syncError"), item.syncError);

    map.insert(QStringLiteral("isFolder"), item.isFolder());
    map.insert(QStringLiteral("isAtomicFolder"), item.isAtomicFolder());
    map.insert(QStringLiteral("isAtomicFile"), item.isAtomicFile());
    map.insert(QStringLiteral("isReadOnly"), item.isReadOnly());
    map.insert(QStringLiteral("isCloud"), item.localPath.isEmpty());

    return map;
}

QVariantMap FileService::fileInfo(
    const QString &fileId) const
{
    FileItem item;

    if (!loadItem(fileId, item)) {
        return {};
    }

    return itemToMap(item);
}

static QVector<FileItem> filteredChildren(
    MetadataStore &store,
    const QString &parentId,
    FileSpace space,
    const QString &sourceId,
    const QString &groupId)
{
    QVector<FileItem> result;

    const QVector<FileItem> raw =
        parentId.isEmpty()
            ? store.rootFiles(space)
            : store.childrenOf(parentId, space);

    for (const FileItem &item : raw) {

        if (item.sourceId != sourceId) {
            continue;
        }

        if (space == FileSpace::GroupMe
            && item.groupId != groupId) {
            continue;
        }

        result.append(item);
    }

    return result;
}

QVariantList FileService::children(
    const QString &parentId,
    int space,
    const QString &sourceId,
    const QString &groupId) const
{
    QVariantList result;

    if (!isValidSpace(space)) {
        return result;
    }

    MetadataStore store;

    if (!openStore(store)) {
        return result;
    }

    const FileSpace fileSpace =
        toFileSpace(space);

    const QString effectiveGroupId =
        fileSpace == FileSpace::GroupMe
            && groupId.trimmed().isEmpty()
            ? layout::groupMeDirectoryName(0)
            : groupId;

    const QVector<FileItem> items =
        filteredChildren(
            store,
            parentId,
            fileSpace,
            sourceId,
            effectiveGroupId);

    for (const FileItem &item : items) {
        result.append(itemToMap(item));
    }

    return result;
}

QVariantList FileService::rootItems(
    int space,
    const QString &sourceId,
    const QString &groupId) const
{
    return children(QString(), space, sourceId, groupId);
}

QVariantList FileService::trashItems(
    int space,
    const QString &sourceId,
    const QString &groupId) const
{
    QVariantList result;

    if (!isValidSpace(space)) {
        return result;
    }

    MetadataStore store;

    if (!openStore(store)) {
        return result;
    }

    const QVector<FileItem> items =
        store.trashFiles(toFileSpace(space));

    for (const FileItem &item : items) {

        if (item.sourceId != sourceId) {
            continue;
        }

        if (groupId.isEmpty()
            || item.groupId == groupId) {

            result.append(itemToMap(item));
        }
    }

    return result;
}

QVariantList FileService::folderChoices(
    int space,
    const QString &sourceId,
    const QString &groupId,
    const QString &excludeSubtreeOf) const
{
    QVariantList result;

    if (!isValidSpace(space)) {
        return result;
    }

    MetadataStore store;

    if (!openStore(store)) {
        return result;
    }

    const FileSpace fileSpace =
        toFileSpace(space);

    const QString effectiveGroupId =
        fileSpace == FileSpace::GroupMe
            && groupId.trimmed().isEmpty()
            ? layout::groupMeDirectoryName(0)
            : groupId;

    // ---- 递归展开 ----

    struct Walker {
        FileService *self;
        MetadataStore *store;
        FileSpace space;
        QString sourceId;
        QString groupId;
        QString exclude;
        QVariantList *out;

        void visit(const QString &parentId,
                   int depth,
                   const QString &pathPrefix)
        {
            const QVector<FileItem> items =
                filteredChildren(
                    *store,
                    parentId,
                    space,
                    sourceId,
                    groupId);

            for (const FileItem &item : items) {

                if (!item.isFolder()) {
                    continue;
                }

                if (!exclude.isEmpty()
                    && self->isDescendantOf(
                        item.id,
                        exclude)) {

                    continue;
                }

                const QString path =
                    pathPrefix.isEmpty()
                        ? item.name
                        : pathPrefix
                              + QStringLiteral(" / ")
                              + item.name;

                QVariantMap map;

                map.insert(QStringLiteral("fileId"), item.id);
                map.insert(QStringLiteral("name"), item.name);
                map.insert(QStringLiteral("depth"), depth);
                map.insert(QStringLiteral("path"), path);

                out->append(map);

                visit(item.id, depth + 1, path);
            }
        }
    };

    Walker walker {
        const_cast<FileService *>(this),
        &store,
        fileSpace,
        sourceId,
        effectiveGroupId,
        excludeSubtreeOf,
        &result
    };

    walker.visit(QString(), 1, QString());

    return result;
}

QString FileService::remoteRootOf(
    const QString &fileId) const
{
    FileItem item;

    if (!loadItem(fileId, item)) {
        return QString();
    }

    return remoteRootFor(item.space, item.groupId);
}

QString FileService::localMirrorPathOf(
    const QString &fileId) const
{
    FileItem item;

    if (!loadItem(fileId, item)) {
        return QString();
    }

    if (!item.localPath.isEmpty()) {
        return item.localPath;
    }

    if (item.remotePath.isEmpty()) {
        return QString();
    }

    return layout::localPathForRemote(
        item.remotePath,
        item.sourceId);
}

bool FileService::existsLocally(
    const QString &fileId) const
{
    FileItem item;

    if (!loadItem(fileId, item)) {
        return false;
    }

    if (!item.localPath.isEmpty()) {
        return QFileInfo::exists(item.localPath);
    }

    return false;
}

bool FileService::isFolder(
    const QString &fileId) const
{
    FileItem item;
    return loadItem(fileId, item) && item.isFolder();
}

bool FileService::isAtomicFolder(
    const QString &fileId) const
{
    FileItem item;
    return loadItem(fileId, item) && item.isAtomicFolder();
}

bool FileService::isAtomicFile(
    const QString &fileId) const
{
    FileItem item;
    return loadItem(fileId, item) && item.isAtomicFile();
}

int FileService::spaceOf(
    const QString &fileId) const
{
    FileItem item;

    if (!loadItem(fileId, item)) {
        return -1;
    }

    return static_cast<int>(item.space);
}

QString FileService::sourceIdOf(
    const QString &fileId) const
{
    FileItem item;

    if (!loadItem(fileId, item)) {
        return QString();
    }

    return item.sourceId;
}

QString FileService::groupIdOf(
    const QString &fileId) const
{
    FileItem item;

    if (!loadItem(fileId, item)) {
        return QString();
    }

    return item.groupId;
}

} // namespace litapp
