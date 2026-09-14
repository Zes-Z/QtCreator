# 文献阅读 / 办公应用

从零开始构建的一款跨平台桌面文献阅读与办公应用。

## 技术栈

- Qt 6 + **Qt Quick / QML**
- C++17 / CMake
- SQLite（本地元数据）
- QNetworkAccessManager（WebDAV）
- Qt PDF 模块（可选，未安装时自动降级）

## 两套结构：远端是仓库，应用是视图

这是整个项目最重要的设计前提。

### 远端（WebDAV，扁平）

我的账号：

```
zesbks/
├── 原子文件夹1/原子文件1.pdf        <- my doc
├── 原子文件夹2/原子文件2.md
└── group/
    ├── group me/                    <- group me（第 1 个群组）
    │   └── 原子文件夹3/原子文件3.pdf
    └── group me 2/                  <- 未来扩展：平级多个群组目录
        └── ...
```

- 原子文件夹在各自根目录下**平铺**，永不嵌套
- 远端**没有分类文件夹层级**
- `group us` 的内容来自**他人账号**，只读取，永不上传到自己的远端

### 应用内（逻辑树）

```
my doc     -> 读远端 zesbks/*
group me   -> 读远端 zesbks/group/group me*
group us   -> 读他人账号的群组目录（只读）
```

三个根下都可以建**任意层子分类文件夹**。

子分类文件夹是**纯逻辑**的：只存在于 SQLite 的 `parent_id`，
没有本地目录，也没有远端目录。

### 由此推出的两条铁律

1. **移动不碰远端**。移动只修改 `parent_id`：
   子树通过 `parent_id` 隐式跟随，父节点完全不受影响。
   移动不产生任何文件系统或 WebDAV 操作。

2. **本地是镜像缓存**。远端是事实来源：
   - 列表阶段只物化远端元信息（`localPath` 为空 = 云端条目）
   - 双击才按镜像路径下载
   - 本地文件被修改后回推远端

## 目录结构

```
.
├── CMakeLists.txt
├── qml
│   ├── Main.qml
│   ├── FilePage.qml            三栏主界面
│   ├── PdfPreviewPane.qml
│   └── PdfViewPage.qml
└── src
    ├── main.cpp
    ├── core
    │   └── AppLayout.{h,cpp}   ← 路径规则的唯一来源
    ├── models
    │   ├── FileCategory.h
    │   ├── FileItem.{h,cpp}
    │   └── FileListModel.{h,cpp}
    ├── services
    │   ├── AuthService.{h,cpp}
    │   ├── FileService.{h,cpp}   逻辑文件系统
    │   ├── SyncService.{h,cpp}   远端发现 / 上传 / 下载 / 删除
    │   └── WebDavClient.{h,cpp}
    ├── storage
    │   └── MetadataStore.{h,cpp}
    └── parsers
        └── DocumentParser.{h,cpp}
```

`AppLayout` 是路径规则的唯一来源。远端路径、本地镜像路径
都必须经过它，不允许在别处硬编码。

## 数据模型

| 概念 | 说明 |
|---|---|
| `FileSpace` | `MyDoc` / `GroupMe` / `GroupUs` |
| `FileObjectType` | `Folder`（纯逻辑）/ `AtomicFolder` / `AtomicFile` |
| `groupId` | 群组目录名，例如 `group me`、`group me 2` |
| `sourceId` | `GroupUs` 的数据源标识（他人账号） |
| `localPath` | 为空表示云端条目，尚未下载 |
| `remotePath` | 远端绝对路径，由 `AppLayout` 推导 |

### 数据库表

- `files` —— 逻辑对象（含 `group_id`）
- `tags` —— 文件标签
- `sources` —— `group us` 数据源（地址 / 账号 / 密码 / 远端群组根，只读）
- `pending_deletes` —— **墓碑**

墓碑用于保证删除与重命名的一致性：本地删除或重命名一个远端对象后，
旧远端路径会被记入墓碑，由下一轮同步真正删除。
若没有墓碑，下一次同步会把旧路径当成「远端新增」重新拉回应用。

### 远端群组布局自动适配

兼容两种远端布局，无需手工配置：

```
zesbks/group/group me/   （默认）
zesbks/group me/
```

扫描 `zesbks` 时记录其真实子目录，据此选择群组根。
两者都不存在时按默认布局创建。

### 远端根目录自动补建

自己的账号在扫描前若读不到远端根目录，会先把中间层级补齐再重扫一次：

```
SyncService: 尝试补建远端根目录
  QList("/zesbks", "/zesbks/group", "/zesbks/group/group me")
```

因此首次连接、换了群组布局都能自愈，不需要手工建目录。

### 两条安全约束

1. **响应必须真的是 WebDAV multistatus**。
   root 元素不是 `multistatus` 时直接判为失败，
   即使正文是合法 XML（例如 HTML 登录页）。
   否则「解析成功但没有任何条目」会被误判成「远端为空」。

2. **扫描不可信时绝不清理本地元数据**。
   只要本轮扫描出现任何错误，reconcile 就只新增/更新，不删除。
   否则服务器返回登录页时会静默清空用户整个库。

失败信息会带上 HTTP 状态码与响应正文片段，例如：

```
远端根目录 /zesbks 读取失败：
  响应不是 WebDAV multistatus（HTTP 200）：<html><body>Please sign in…
```

## 同步流程

```
1. PROPFIND 远端根
2. 逐个 PROPFIND 原子文件夹（Depth: 1）
3. 清理墓碑指向的远端残留
4. reconcile：远端 -> SQLite
     · 远端有 / 本地无 -> 新增（localPath 为空，云端条目）
     · 远端已消失      -> 删除本地元数据
     · 已存在的行保持 parentId 不变（用户移动过的位置不会被重置）
5. prepareUploads：本地 -> 远端
     · 比较 mtime，远端不旧就不上传
6. MKCOL 缺失目录 -> PUT 文件
```

`GroupUs` 的 Job 只执行 1/2/4，永不执行 5/6。

## 界面

### 主题

固定浅色，**不跟随系统配色**。

Qt 6 默认跟随系统深浅色，因此在 `main.cpp` 里于第一个窗口显示前
调用 `QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light)`。
应用不提供深色模式。

### 左栏：资源管理器

仿 VS Code 的工作区视图：

- 顶部标题栏显示当前位置，右侧为 新建文件夹 / 全部折叠 / 刷新
- 24px 行高、缩进参考线、折叠箭头、悬停与选中高亮
- 单击文件夹即选中并展开，双击文件夹切换展开
- **双击 my doc / group me / group us 可整棵子树展开或折叠**
- 每一行都是独立的拖放目标，悬停时该行高亮并显示落点

### 阅读区

- 中间预览：默认**适应宽度**，随窗口大小自动重新适配；页面小于视口时自动居中
- 中间预览保留上一页 / 下一页按钮
- 双击文件进入全屏阅读：**上下连续滚动**（`PdfMultiPageView`），不提供翻页按钮

## 已实现

- 三栏界面：资源管理器 / 内容与 PDF 预览 / 工具条
- 三个空间 + 任意层纯逻辑子分类文件夹，右键新建 / 重命名 / 移动 / 复制 / 回收站
- 远端 -> 应用内自动发现（my doc / group me）
- `group us` 多来源只读挂载，每个来源独立配置地址与凭据
- 首次 WebDAV 验证通过后自动幂等创建 `zesbks/` 与 `zesbks/group/group me/`
- 本地导入（COPY 语义，不动用户原文件）
- 按需下载（双击打开）
- PDF 阅读：分页导航、缩放、加载失败时显示真实原因
- 远端原子文件夹删除（带路径合法性保护）
- 分类筛选、标签
- WebDAV：PROPFIND / PUT / GET / MKCOL / DELETE，Basic 或 Bearer

## 待实现

1. **PDF 阅读增强**：目录（大纲）、文本选择、搜索、批注
2. **Markdown 阅读 / 编辑**
3. **远端重命名改为 MOVE**：当前实现是「删旧 + 传新」，会多一次上传
4. **冲突处理**：目前以远端 mtime 为准，没有三方合并
5. **标注系统**：`annotationsPath` 已预留但未使用
6. **登录 / 注册接入真实 API**：`AuthService` 目前是占位实现
7. **凭据加密**：WebDAV 密码目前明文存于 QSettings 与 SQLite `sources` 表
8. `DocumentParser` 的 PDF 文本提取与 Markdown 渲染

## 构建

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=D:/Qt/6.11.1/mingw_64
cmake --build build
./build/LitApp
```

> 如果已经通过 Qt Maintenance Tool 安装了 Qt PDF 模块，
> CMake 会自动检测并启用 PDF 支持。

数据库位置：`QStandardPaths::AppDataLocation` 下的 `litapp_metadata.db`。

本地镜像位置：`<Documents>/LitApp/cache/`
（我的账号）与 `<Documents>/LitApp/cache/sources/<sourceId>/`（他人账号）。
