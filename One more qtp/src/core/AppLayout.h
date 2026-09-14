#pragma once

#include <QString>

/*
 * ================================================================
 * AppLayout
 * ================================================================
 *
 * 本文件是整个应用「路径规则」的唯一来源。
 *
 * 在此之前，远端路径规则同时散落在：
 *
 *     FileService   -> 硬编码 "/zesbks/%1"
 *     SyncService   -> remoteSpaceRoot(space)
 *
 * 两者互不一致，导致入库的 remotePath
 * 与同步实际使用的路径对不上。
 *
 * 现在所有路径都必须经过这里。
 *
 * ================================================================
 * 远端布局（WebDAV）
 * ================================================================
 *
 * 我自己的账号：
 *
 *     zesbks/
 *     ├── 原子文件夹1/原子文件1.pdf      <- FileSpace::MyDoc
 *     ├── 原子文件夹2/原子文件2.md
 *     └── group/
 *         ├── group me/                  <- FileSpace::GroupMe 第 1 个群组
 *         │   └── 原子文件夹3/原子文件3.pdf
 *         └── group me 2/                <- 未来扩展：平级多个群组目录
 *             └── ...
 *
 * 要点：
 *
 * 1. 原子文件夹在各自根目录下「平铺」，
 *    永远不嵌套。
 *
 * 2. 应用内的子分类文件夹是纯逻辑概念，
 *    不产生任何远端目录。
 *
 * 3. FileSpace::GroupUs 指向「他人账号」，
 *    结构与他人的群组目录同构，
 *    但仅读取，永不写入。
 *
 * ================================================================
 * 本地布局
 * ================================================================
 *
 * 本地是远端的镜像缓存：
 *
 *     <Documents>/LitApp/cache/zesbks/...
 *                                    <- 我的账号镜像
 *
 *     <Documents>/LitApp/cache/sources/<sourceId>/zesbks/...
 *                                    <- 他人账号镜像
 *
 * 因此：
 *
 *     双击远端条目 -> 按镜像路径下载
 *     本地文件被修改 -> 按镜像路径回推远端
 *
 * 两条路径互为反函数，不需要额外映射表。
 */

namespace litapp {
namespace layout {

// ============================================================
// 远端：我的账号
// ============================================================

// 共享根目录：/zesbks
QString remoteShareRoot();

// 群组容器目录：/zesbks/group
QString remoteGroupRoot();

// 第 index 个自建群组目录名。
//
//     0 -> "group me"
//     1 -> "group me 2"
//     2 -> "group me 3"
QString groupMeDirectoryName(int groupIndex);

// 第 index 个自建群组的绝对远端路径。
QString remoteGroupMeRoot(int groupIndex);

/*
 * /zesbks 下的「保留目录名」。
 *
 * 这些是群组容器，不是原子文件夹，
 * 扫描 my doc 时必须跳过。
 *
 * 覆盖两种布局：
 *
 *     zesbks/group/group me/   （默认）
 *     zesbks/group me/
 */
bool isReservedShareEntry(const QString &name);

// ============================================================
// 远端：原子对象
// ============================================================

// remoteRoot + "/" + atomicFolderName
QString remoteAtomicFolderPath(
    const QString &remoteRoot,
    const QString &atomicFolderName);

// remoteRoot + "/" + atomicFolderName + "/" + atomicFileName
QString remoteAtomicFilePath(
    const QString &remoteRoot,
    const QString &atomicFolderName,
    const QString &atomicFileName);

// ============================================================
// 远端：路径工具
// ============================================================

// 统一成以 '/' 开头、无重复斜杠、无结尾斜杠的绝对路径。
QString normalizeRemotePath(const QString &path);

// 父目录。根目录的父目录仍然是根目录。
QString remoteParentPath(const QString &remotePath);

// remotePath 是否位于 root 之下（不含 root 本身）。
bool isRemotePathInside(
    const QString &remotePath,
    const QString &root);

// 由 remotePath 得到相对于 root 的第一级目录名。
//
//     root     = /zesbks
//     remotePath = /zesbks/A/B
//     -> "A"
QString firstLevelNameUnder(
    const QString &remotePath,
    const QString &root);

// ============================================================
// 本地：镜像缓存
// ============================================================

// <Documents>
QString localBaseRoot();

// <Documents>/LitApp
QString localLibraryRoot();

// <Documents>/LitApp/cache
QString localCacheRoot();

// <Documents>/LitApp/cache/zesbks
QString localOwnMirrorRoot();

// <Documents>/LitApp/cache/sources/<sourceId>/zesbks
QString localSourceMirrorRoot(
    const QString &sourceId);

// 按 sourceId 选择镜像根。
// sourceId 为空表示「我自己的账号」。
QString localMirrorRoot(
    const QString &sourceId = QString());

// 远端绝对路径 -> 本地镜像路径。
QString localPathForRemote(
    const QString &remotePath,
    const QString &sourceId = QString());

// 本地镜像路径 -> 远端绝对路径。
//
// 若 localPath 不在镜像根内，返回空字符串。
QString remotePathForLocal(
    const QString &localPath,
    const QString &sourceId = QString());

// ============================================================
// 工具
// ============================================================

// 把任意字符串变成可以安全用作目录名的形式。
QString sanitizeFileSystemComponent(
    const QString &value);

} // namespace layout
} // namespace litapp
