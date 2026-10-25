#pragma once

#include "FileCategory.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace litapp {

/*
 * ================================================================
 * FileSpace
 * ================================================================
 *
 * 应用中的逻辑空间。
 *
 * 注意：
 *
 * FileSpace 只描述“应用看到的逻辑位置”，
 * 不等同于 WebDAV 的物理目录结构。
 *
 * MyDoc:
 *     当前用户自己的文献。
 *
 * GroupMe:
 *     当前用户自己创建/管理的群组文献。
 *
 * GroupUs:
 *     来自其他用户群组数据源的文献。
 */
enum class FileSpace {
    MyDoc = 0,
    GroupMe = 1,
    GroupUs = 2
};


/*
 * ================================================================
 * FileObjectType
 * ================================================================
 *
 * 应用中的对象类型。
 *
 * Folder:
 *     普通文件夹。
 *
 *     可以包含：
 *         Folder
 *         AtomicFolder
 *
 *     例如：
 *
 *         机器学习/
 *         ├── 深度学习/
 *         └── Transformer/
 *
 *
 * AtomicFolder:
 *     一个原子对象的容器。
 *
 *     例如：
 *
 *         test1/
 *             test1.pdf
 *
 *     AtomicFolder 和 AtomicFile 的名称彼此独立。
 *
 *
 * AtomicFile:
 *     AtomicFolder 内部实际承载的文件。
 */
enum class FileObjectType {
    Folder = 0,
    AtomicFolder = 1,
    AtomicFile = 2
};


/*
 * ================================================================
 * SyncState
 * ================================================================
 *
 * 文件当前的同步状态。
 */
enum class SyncState {
    LocalOnly = 0,
    Synced = 1,
    Uploading = 2,
    Downloading = 3,
    Conflict = 4,
    Error = 5
};


/*
 * ================================================================
 * FileItem
 * ================================================================
 *
 * 应用文件系统中的一个逻辑对象。
 *
 * 重要：
 *
 * FileItem 同时承载：
 *
 *     1. 应用逻辑树信息
 *     2. 本地文件信息
 *     3. WebDAV 映射信息
 *     4. 文献元数据
 *     5. 同步状态
 *
 * 但是：
 *
 *     FileItem::parentId
 *
 * 决定的是“应用逻辑树中的父对象”。
 *
 * 它不能用于直接推导 WebDAV 物理路径。
 *
 * WebDAV 路径由 SyncService 根据：
 *
 *     FileSpace
 *     sourceId
 *     AtomicFolder
 *     AtomicFile
 *
 * 等信息单独计算。
 */
struct FileItem {

    // ============================================================
    // 对象身份
    // ============================================================

    /*
     * 对象唯一 ID。
     *
     * 对 Folder / AtomicFolder / AtomicFile
     * 都应该唯一。
     */
    QString id;


    /*
     * 父对象 ID。
     *
     * 空字符串：
     *     表示该对象位于对应 FileSpace 的根部。
     *
     * 非空：
     *     表示该对象属于某个 Folder。
     *
     * 例如：
     *
     *     my doc
     *     └── AI
     *         └── test1
     *
     *     AI.id    = "folder-A"
     *     test1.parentId = "folder-A"
     *
     * 注意：
     *
     * parentId 只表示应用逻辑层关系。
     */
    QString parentId;


    /*
     * 对象自身名称。
     *
     * Folder:
     *     文件夹名称
     *
     * AtomicFolder:
     *     原子文件夹名称
     *
     * AtomicFile:
     *     原子文件名称
     *
     * 三者名称彼此独立。
     *
     * 因此：
     *
     *     AtomicFolder:
     *         test1
     *
     *     AtomicFile:
     *         paper-final.pdf
     *
     * 是完全合法的。
     *
     * 重命名 AtomicFolder 不应该自动重命名 AtomicFile。
     */
    QString name;


    /*
     * 对象类型。
     */
    FileObjectType objectType = FileObjectType::AtomicFile;


    /*
     * 所属逻辑空间。
     *
     *     MyDoc
     *     GroupMe
     *     GroupUs
     */
    FileSpace space = FileSpace::MyDoc;


    // ============================================================
    // Group Us 数据源
    // ============================================================

    /*
     * 数据源 ID。
     *
     * 对：
     *
     *     MyDoc
     *     GroupMe
     *
     * 通常为空。
     *
     * 对：
     *
     *     GroupUs
     *
     * 用来标识该对象来自哪个远程群组数据源。
     *
     * 例如：
     *
     *     GroupUs
     *         sourceId = "user-A"
     *
     *     GroupUs
     *         sourceId = "user-B"
     *
     * 两者可以同时存在。
     *
     * 注意：
     *
     * server URL / username / password 等认证信息
     * 不应该复制到每一个 FileItem。
     *
     * 它们应该由独立的数据源配置系统管理。
     */
    QString sourceId;


    /*
     * 群组目录名。
     *
     * 对 MyDoc：
     *     恒为空。
     *
     * 对 GroupMe：
     *     表示用户自建的第几个群组目录。
     *
     *         第 1 个 -> "group me"
     *         第 2 个 -> "group me 2"
     *
     *     对应远端：
     *
     *         /zesbks/group/<groupId>/
     *
     * 对 GroupUs：
     *     远端群组根由 GroupSource 配置决定，
     *     此字段通常为空。
     */
    QString groupId;


    // ============================================================
    // 本地 / 远程路径
    // ============================================================

    /*
     * 本地实际路径。
     *
     * 对 AtomicFile：
     *
     *     /Documents/LitApp/test1.pdf
     *
     * 对 Folder：
     *
     *     /Documents/LitApp/AI
     *
     * 对 AtomicFolder：
     *
     *     /Documents/LitApp/test1
     *
     * 具体路径组织由 FileService / SyncService 管理。
     */
    QString localPath;


    /*
     * 对应的 WebDAV 路径。
     *
     * 例如：
     *
     *     /zesbks/test1/test1.pdf
     *
     * 注意：
     *
     * remotePath 是同步映射结果。
     *
     * 它不是应用逻辑树的唯一依据。
     *
     * 不应该通过：
     *
     *     remotePath
     *
     * 反过来决定：
     *
     *     parentId
     *     space
     *
     * 等逻辑属性。
     */
    QString remotePath;


    // ============================================================
    // 文件元数据
    // ============================================================

    /*
     * 文件扩展名。
     *
     * Folder / AtomicFolder：
     *     通常为空。
     *
     * AtomicFile：
     *     例如：
     *
     *         pdf
     *         md
     *         docx
     */
    QString extension;


    /*
     * 注释 / 标注数据文件路径。
     *
     * 不要求与 AtomicFile 位于同一个 WebDAV 物理目录。
     *
     * 具体同步策略由后续 Annotation 系统决定。
     */
    QString annotationsPath;


    /*
     * 文件大小。
     *
     * 对 Folder / AtomicFolder：
     *     可以为 0，
     *     也可以在未来扩展为递归大小。
     */
    qint64 size = 0;


    /*
     * 最后修改时间。
     */
    QDateTime modifiedTime;


    /*
     * 文献分类。
     *
     * 分类属于应用元数据。
     *
     * 注意：
     *
     * category 不应该决定 WebDAV 物理目录。
     */
    FileCategory category = FileCategory::Other;


    /*
     * 用户标签。
     */
    QStringList tags;


    // ============================================================
    // 同步状态
    // ============================================================

    /*
     * 当前同步状态。
     */
    SyncState syncState = SyncState::LocalOnly;


    /*
     * 最近一次同步错误。
     *
     * 如果 syncState != Error，
     * 通常为空。
     */
    QString syncError;


    // ============================================================
    // 基本判断
    // ============================================================

    /*
     * 判断 FileItem 是否具有基本有效性。
     *
     * 当前最基本要求：
     *
     *     id 非空
     *     name 非空
     */
    bool isValid() const
    {
        return !id.isEmpty()
        && !name.isEmpty();
    }


    /*
     * 是否为普通 Folder。
     */
    bool isFolder() const
    {
        return objectType == FileObjectType::Folder;
    }


    /*
     * 是否为 AtomicFolder。
     */
    bool isAtomicFolder() const
    {
        return objectType == FileObjectType::AtomicFolder;
    }


    /*
     * 是否为 AtomicFile。
     */
    bool isAtomicFile() const
    {
        return objectType == FileObjectType::AtomicFile;
    }


    /*
     * 是否为只读对象。
     *
     * GroupUs 的内容来自他人账号，
     * 本应用只读取，永远不回写。
     */
    bool isReadOnly() const
    {
        return space == FileSpace::GroupUs;
    }
};

} // namespace litapp


/*
 * ================================================================
 * Qt Meta Type
 * ================================================================
 *
 * 允许 FileItem 以及相关 enum 在：
 *
 *     QVariant
 *     queued signal / slot
 *     Qt 元对象系统
 *
 * 中使用。
 */
Q_DECLARE_METATYPE(litapp::FileItem)
Q_DECLARE_METATYPE(litapp::FileSpace)
Q_DECLARE_METATYPE(litapp::FileObjectType)
Q_DECLARE_METATYPE(litapp::SyncState)