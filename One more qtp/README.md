# 文献阅读 / 办公应用

从零开始构建的一款跨平台桌面文献阅读与办公应用。

## 主要特性

- 支持文件类型：
  - PDF
  - Markdown（.md）
  - 后续可扩展 EPUB、DOCX、TXT 等
- 文件分区：
  - 我的文件
  - 群组文件
  - 两个分区均支持云同步
- 云同步：
  - 第一阶段：WebDAV（已实现基础客户端）
  - 后续阶段：自建服务器存储
- 元数据分类与标签：
  - 原分类（必选、单选）：书籍 / 期刊论文 / Markdown 原子知识 / 其他
  - 自定义标签：一个文件可拥有多个标签
- 账号体系：
  - 使用邮箱 + 密码登录/注册
  - 账号数据与文件元数据同步

## 技术栈

- Qt 6 + **Qt Quick / QML**（UI 形态已确定）
- C++17
- CMake
- SQLite（本地元数据缓存）
- QNetworkAccessManager（WebDAV 与 API 请求）
- Qt PDF 模块（安装后启用 PDF 阅读，当前尚未安装）

## 目录结构

```
.
├── CMakeLists.txt
├── README.md
├── qml
│   ├── Main.qml
│   ├── LoginPage.qml
│   └── FilePage.qml
└── src
    ├── main.cpp
    ├── models
    │   ├── FileCategory.h
    │   ├── FileItem.h
    │   ├── FileItem.cpp
    │   ├── FileListModel.h
    │   └── FileListModel.cpp
    ├── services
    │   ├── AuthService.h/.cpp
    │   ├── FileService.h/.cpp
    │   ├── SyncService.h/.cpp
    │   └── WebDavClient.h/.cpp
    ├── storage
    │   ├── MetadataStore.h
    │   └── MetadataStore.cpp
    └── parsers
        ├── DocumentParser.h
        └── DocumentParser.cpp
```

## 已实现

- QML 登录页（邮箱 + 密码）
- 三栏圆角面板 UI：左侧文件目录、中间 PDF 预览/阅读、右侧设置
- WebDAV 配置自动保存（服务器地址 / 用户名 / 密码）
- 本地文件导入：选择文件后设置分类与标签，写入 SQLite 并刷新列表
- PDF 预览：中间区域直接嵌入 `PdfPreviewPane`
- 远程文件仅同步元信息，打开时才按需下载完整文件
- `WebDavClient`：
  - PROPFIND 列出远程目录
  - PUT 上传文件
  - GET 下载文件
  - MKCOL 创建目录
  - DELETE 删除文件
  - Basic Auth / Bearer Token
- `SyncService`：每 3 分钟自动同步，自动上传缺失本地文件、同步远程元信息
- `FileListModel`：将本地文件元数据暴露给 QML
- SQLite 本地元数据存储

## 待实现

1. **PDF 阅读增强**：缩放、页码、目录、翻页
2. **Markdown 阅读/编辑增强**
3. **完整同步流程**：冲突处理、增量更新
4. **群组文件与协作**

## 构建

```bash
# 使用你的 Qt 安装路径
cmake -S . -B build -DCMAKE_PREFIX_PATH=D:/Qt/6.11.1/mingw_64
cmake --build build
./build/LitApp
```

> 如果已经通过 Qt Maintenance Tool 安装了 Qt PDF 模块，CMake 会自动检测并启用 PDF 支持。
