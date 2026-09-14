import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore

/*
 * ================================================================
 * FilePage
 * ================================================================
 *
 * 三栏结构：
 *
 *     左：文件树
 *     中：当前文件夹内容 / PDF 预览
 *     右：工具条（主题 / 回收站 / 同步 / 账号 / 群组来源）
 *
 * ================================================================
 * 文件树结构
 * ================================================================
 *
 *     my doc
 *         ├── 普通文件夹（纯逻辑）
 *         │     └── 原子文件夹
 *         │            └── 原子文件
 *         └── 原子文件夹
 *
 *     group me
 *         └── group me            （群组目录，未来可扩展多个）
 *
 *     group us
 *         └── <来源>              （他人账号，只读）
 *
 * 注意：
 *
 *     只有原子文件夹 / 原子文件对应远端内容。
 *
 *     普通文件夹是纯逻辑容器，
 *     移动它不会产生任何远端操作。
 */
Page {
    id: root

    signal logoutRequested()
    signal openPdfFullscreen(string path)

    // ============================================================
    // 常量
    // ============================================================

    readonly property int spaceMyDoc: 0
    readonly property int spaceGroupMe: 1
    readonly property int spaceGroupUs: 2

    readonly property int objectFolder: 0
    readonly property int objectAtomicFolder: 1
    readonly property int objectAtomicFile: 2

    readonly property string databasePath: appDatabasePath

    // ============================================================
    // 主题
    // ============================================================

    /*
     * 固定浅色主题。
     *
     * 应用不跟随系统配色，也不提供深色模式。
     */
    readonly property color panelColor: "#f4f6f8"
    readonly property color panelBorder: "#d9dfe7"
    readonly property color contentBg: "#ffffff"
    readonly property color textColor: "#222222"
    readonly property color subtleText: "#666666"
    readonly property color previewBg: "#fafafa"
    readonly property color fieldBg: "#ffffff"

    readonly property color accent: "#3080d0"

    // 列表行
    readonly property color rowSelected: "#d6e6ff"
    readonly property color rowHover: "#eaeff6"

    // 拖放目标高亮
    readonly property color rowDropHover: "#cfe4f7"
    readonly property color indentGuide: "#e4e9f0"

    background: Rectangle { color: root.contentBg }

    // ============================================================
    // 当前位置
    // ============================================================

    property int currentSpace: spaceMyDoc
    property string currentSourceId: ""
    property string currentGroupId: ""
    property string currentFolderId: ""
    property string currentFolderName: ""

    // ============================================================
    // 当前选择
    // ============================================================

    property string selectedId: ""
    property string selectedName: ""
    property int selectedObjectType: -1
    property int selectedSpace: -1
    property string selectedSourceId: ""
    property string selectedGroupId: ""
    property string selectedParentId: ""
    property string selectedLocalPath: ""
    property string selectedRemotePath: ""
    property bool selectedReadOnly: false

    // ============================================================
    // PDF
    // ============================================================

    property string pdfPreviewSource: ""
    property string pendingDownloadId: ""
    property bool pendingOpenFullscreen: false

    // ============================================================
    // 模型
    // ============================================================

    ListModel { id: treeModel }
    ListModel { id: folderFilesModel }
    ListModel { id: moveTargetModel }
    ListModel { id: sourceModel }

    // 展开状态：key -> bool
    property var expandedState: ({})

    // 拖拽时高亮的目标行（-1 = 无）
    property int dropHoverIndex: -1

    // 右键菜单当前的动态动作
    property var contextActions: []



    // ============================================================
    // 启动
    // ============================================================

    Component.onCompleted: {
        webDavClient.restoreSettings()

        serverField.text = webDavClient.serverUrl()
        usernameField.text = webDavClient.username()
        passwordField.text = webDavClient.password()

        reloadSources()
        refreshAll()

        // 每 3 分钟自动同步一次。
        syncService.startAutoSync(180000)
        // TEMP-VERIFY
        var probeItems = fileListModel.toVariantList()
        for (var pi = 0; pi < probeItems.length; ++pi) {
            if (Number(probeItems[pi].objectType) === 1) {
                console.log("DELTEST -> " + probeItems[pi].fileName + " @ " + probeItems[pi].remotePath)
                syncService.deleteRemoteAtomicFolder(probeItems[pi].fileId)
                break
            }
        }


    }

    onPdfPreviewSourceChanged: root.syncPreviewSource()

    /*
     * 只在来源真的变化时重新加载文档。
     *
     * 否则每次单击文件都会重新赋值，
     * 触发 PDF 文档重建与 link model 重载。
     */
    function syncPreviewSource() {
        const pane = pdfLoader.item

        // Loader.item 在切换期间可能是别的类型或已销毁。
        if (!pane || pane.source === undefined) {
            return
        }

        const next = root.pdfPreviewSource

        if (pane.source.toString() === next) {
            return
        }

        pane.source = next
    }

    // ============================================================
    // 小工具
    // ============================================================

    function spaceTitle(space) {
        switch (space) {
        case root.spaceMyDoc:   return qsTr("my doc")
        case root.spaceGroupMe: return qsTr("group me")
        case root.spaceGroupUs: return qsTr("group us")
        }
        return qsTr("未知")
    }

    function objectTypeName(type) {
        switch (type) {
        case root.objectFolder:       return qsTr("分类文件夹")
        case root.objectAtomicFolder: return qsTr("原子文件夹")
        case root.objectAtomicFile:   return qsTr("原子文件")
        }
        return qsTr("对象")
    }

    function nodeKey(type, id) {
        return type + "|" + String(id)
    }

    function isExpanded(type, id, fallback) {
        const key = nodeKey(type, id)
        if (root.expandedState[key] === undefined) {
            return fallback
        }
        return root.expandedState[key] === true
    }

    function setExpanded(type, id, value) {
        const state = root.expandedState
        state[nodeKey(type, id)] = value
        root.expandedState = state
    }

    function setStatus(text, color) {
        statusLabel.text = text
        statusLabel.color = color === undefined ? root.subtleText : color
    }

    function fileSizeText(bytes) {
        if (!bytes || bytes <= 0) {
            return ""
        }
        if (bytes < 1024) {
            return bytes + " B"
        }
        if (bytes < 1024 * 1024) {
            return (bytes / 1024).toFixed(1) + " KB"
        }
        return (bytes / 1024 / 1024).toFixed(1) + " MB"
    }

    // ============================================================
    // 数据刷新
    // ============================================================

    function refreshAll() {
        fileListModel.reloadAllFromStore(root.databasePath)
        rebuildTree()
        refreshCurrentFolder()
    }

    function allItems() {
        return fileListModel.toVariantList()
    }


    function refreshCurrentFolder() {
        folderFilesModel.clear()

        if (root.currentFolderId.length === 0) {
            // 根：显示该空间下的一级对象。
            appendFolderRows("")
            return
        }

        appendFolderRows(root.currentFolderId)
    }

    function appendFolderRows(parentId) {
        const files = allItems()

        for (let i = 0; i < files.length; ++i) {
            const f = files[i]

            if (Number(f.space) !== root.currentSpace) continue
            if ((f.sourceId || "") !== (root.currentSourceId || "")) continue

            if (root.currentSpace === root.spaceGroupMe
                && (f.groupId || "") !== (root.currentGroupId || "")) {
                continue
            }

            if ((f.parentId || "") !== parentId) continue

            folderFilesModel.append({
                fileId: f.fileId,
                name: f.fileName,
                parentId: f.parentId || "",
                space: Number(f.space),
                sourceId: f.sourceId || "",
                groupId: f.groupId || "",
                objectType: Number(f.objectType),
                localPath: f.localPath || "",
                remotePath: f.remotePath || "",
                size: f.size || 0,
                categoryText: f.categoryText || "",
                isReadOnly: f.isReadOnly === true
            })
        }
    }

    // ============================================================
    // 树构建
    // ============================================================

    function rebuildTree() {
        treeModel.clear()

        appendSpaceNode(qsTr("my doc"), root.spaceMyDoc, "", "")
        appendSpaceNode(qsTr("group me"), root.spaceGroupMe, "", "")
        appendSpaceNode(qsTr("group us"), root.spaceGroupUs, "", "")

        refreshVisibility()
    }

    function appendSpaceNode(title, space, sourceId, groupId) {
        const expanded = isExpanded("space", space, true)

        treeModel.append({
            type: "space",
            name: title,
            depth: 0,
            visible: true,
            expanded: expanded,
            space: space,
            sourceId: sourceId,
            groupId: groupId,
            fileId: "",
            objectType: -1
        })

        if (space === root.spaceGroupUs) {
            appendGroupUsNodes()
            return
        }

        if (space === root.spaceGroupMe) {
            appendGroupMeNodes()
            return
        }

        appendChildren("", space, sourceId, groupId, 1)
    }

    /*
     * group me 的群组目录列表。
     *
     * 永远包含默认的 "group me"，
     * 这样即使本地还没有任何 group me 内容，
     * 用户也能选中它并把文件加进去。
     */
    function groupMeIds() {
        const ids = ["group me"]
        const files = allItems()

        for (let i = 0; i < files.length; ++i) {
            const f = files[i]
            if (Number(f.space) !== root.spaceGroupMe) continue
            const g = f.groupId || ""
            if (g.length === 0) continue
            if (ids.indexOf(g) < 0) ids.push(g)
        }

        ids.sort(function(a, b) {
            if (a === "group me") return -1
            if (b === "group me") return 1
            return a.localeCompare(b)
        })

        return ids
    }

    function appendGroupMeNodes() {
        const groups = groupMeIds()

        if (groups.length === 0) {
            return
        }

        for (let i = 0; i < groups.length; ++i) {
            const gid = groups[i]
            const expanded = isExpanded("group", gid, true)

            treeModel.append({
                type: "group",
                name: gid,
                depth: 1,
                visible: true,
                expanded: expanded,
                space: root.spaceGroupMe,
                sourceId: "",
                groupId: gid,
                fileId: "",
                objectType: -1
            })

            appendChildren("", root.spaceGroupMe, "", gid, 2)
        }
    }

    function appendGroupUsNodes() {
        for (let i = 0; i < sourceModel.count; ++i) {
            const source = sourceModel.get(i)
            const expanded = isExpanded("group", source.sourceId, true)

            treeModel.append({
                type: "group",
                name: source.displayName,
                depth: 1,
                visible: true,
                expanded: expanded,
                space: root.spaceGroupUs,
                sourceId: source.sourceId,
                groupId: "",
                fileId: "",
                objectType: -1,
                readOnly: true
            })

            appendChildren("", root.spaceGroupUs, source.sourceId, "", 2)
        }
    }

    function appendChildren(parentId, space, sourceId, groupId, depth) {
        const files = allItems()

        const folders = []
        const atomicFolders = []
        const atomicFiles = []

        for (let i = 0; i < files.length; ++i) {
            const f = files[i]

            if (Number(f.space) !== Number(space)) continue
            if ((f.sourceId || "") !== (sourceId || "")) continue

            if (Number(space) === root.spaceGroupMe
                && (f.groupId || "") !== (groupId || "")) {
                continue
            }

            if ((f.parentId || "") !== (parentId || "")) continue

            const type = Number(f.objectType)

            if (type === root.objectFolder) {
                folders.push(f)
            } else if (type === root.objectAtomicFolder) {
                atomicFolders.push(f)
            } else if (type === root.objectAtomicFile) {
                atomicFiles.push(f)
            }
        }

        const byName = function(a, b) {
            return String(a.fileName).localeCompare(String(b.fileName))
        }

        folders.sort(byName)
        atomicFolders.sort(byName)
        atomicFiles.sort(byName)

        for (let i = 0; i < folders.length; ++i) {
            const f = folders[i]
            const expanded = isExpanded("folder", f.fileId, false)

            treeModel.append({
                type: "folder",
                name: f.fileName,
                depth: depth,
                visible: true,
                expanded: expanded,
                space: Number(f.space),
                sourceId: f.sourceId || "",
                groupId: f.groupId || "",
                fileId: f.fileId,
                parentId: f.parentId || "",
                objectType: root.objectFolder,
                localPath: f.localPath || "",
                remotePath: f.remotePath || ""
            })

            appendChildren(f.fileId, space, sourceId, groupId, depth + 1)
        }

        for (let i = 0; i < atomicFolders.length; ++i) {
            const f = atomicFolders[i]
            const expanded = isExpanded("folder", f.fileId, false)

            treeModel.append({
                type: "folder",
                name: f.fileName,
                depth: depth,
                visible: true,
                expanded: expanded,
                space: Number(f.space),
                sourceId: f.sourceId || "",
                groupId: f.groupId || "",
                fileId: f.fileId,
                parentId: f.parentId || "",
                objectType: root.objectAtomicFolder,
                localPath: f.localPath || "",
                remotePath: f.remotePath || "",
                isReadOnly: f.isReadOnly === true
            })

            appendChildren(f.fileId, space, sourceId, groupId, depth + 1)
        }

        for (let i = 0; i < atomicFiles.length; ++i) {
            const f = atomicFiles[i]

            treeModel.append({
                type: "file",
                name: f.fileName,
                depth: depth,
                visible: true,
                expanded: false,
                space: Number(f.space),
                sourceId: f.sourceId || "",
                groupId: f.groupId || "",
                fileId: f.fileId,
                parentId: f.parentId || "",
                objectType: root.objectAtomicFile,
                localPath: f.localPath || "",
                remotePath: f.remotePath || "",
                size: f.size || 0,
                isCloud: f.isCloud === true,
                isReadOnly: f.isReadOnly === true
            })
        }
    }

    // ============================================================
    // 可见性
    // ============================================================

    function refreshVisibility() {
        const items = []
        for (let i = 0; i < treeModel.count; ++i) {
            items.push(treeModel.get(i))
        }

        const stack = []

        for (let i = 0; i < items.length; ++i) {
            const item = items[i]

            if (item.type === "space") {
                item.visible = true
                stack.length = 0
                stack.push({ depth: 0, expanded: item.expanded })
                treeModel.set(i, item)
                continue
            }

            let visible = true

            for (let d = 0; d < stack.length; ++d) {
                if (item.depth > stack[d].depth
                    && stack[d].expanded === false) {
                    visible = false
                    break
                }
            }

            item.visible = visible
            treeModel.set(i, item)

            while (stack.length > 0
                   && stack[stack.length - 1].depth >= item.depth) {
                stack.pop()
            }

            if (item.type === "folder" || item.type === "group") {
                stack.push({ depth: item.depth, expanded: item.expanded })
            }
        }
    }

    /*
     * ========================================================
     * 树节点快照
     * ========================================================
     *
     * 委托里的 model 是视图对象，模型一旦变动就会失效。
     *
     * 之前点击文件夹时先 toggleNode() -> rebuildTree()，
     * 模型被清空后 model.fileId 变成 undefined，
     * 于是 openFolder() 拿到空 id，导入目标永远是根目录。
     *
     * 现在所有交互都先取一份纯 JS 快照再操作。
     */
    function snapshotOf(index) {
        if (index === undefined || index === null) return null
        if (index < 0 || index >= treeModel.count) return null

        const it = treeModel.get(index)

        return {
            type: it.type,
            fileId: it.fileId || "",
            name: it.name || "",
            parentId: it.parentId || "",
            space: Number(it.space),
            sourceId: it.sourceId || "",
            groupId: it.groupId || "",
            objectType: Number(it.objectType),
            localPath: it.localPath || "",
            remotePath: it.remotePath || "",
            isReadOnly: it.isReadOnly === true
        }
    }

    // 中间列表行来自于 folderFilesModel。
    function snapshotOfRow(row) {
        if (!row) return null

        return {
            type: Number(row.objectType) === root.objectFolder
                  ? "folder" : "file",
            fileId: row.fileId || "",
            name: row.name || "",
            parentId: row.parentId || "",
            space: Number(row.space),
            sourceId: row.sourceId || "",
            groupId: row.groupId || "",
            objectType: Number(row.objectType),
            localPath: row.localPath || "",
            remotePath: row.remotePath || "",
            isReadOnly: row.isReadOnly === true
        }
    }

    function iconForNode(node) {
        if (node.type === "space") {
            if (node.space === root.spaceMyDoc) return "📖"
            if (node.space === root.spaceGroupMe) return "🫆"
            return "🤝"
        }
        if (node.type === "group") return "✉"
        if (node.type === "folder") return "🗂"
        return "📄"
    }

    // ---- 展开状态 ----

    function nodeKeyOf(node) {
        if (node.type === "group") {
            return "group|" + (node.groupId || node.sourceId || "")
        }
        if (node.type === "folder") {
            return "folder|" + node.fileId
        }
        return node.type + "|" + String(node.space)
    }

    function nodeExpanded(node) {
        const v = root.expandedState[root.nodeKeyOf(node)]
        if (v === undefined) return node.type !== "folder"
        return v === true
    }

    /*
     * 只更新展开标志与可见性，不重建模型。
     *
     * 重建会销毁委托、打断点击处理，
     * 也会丢滚动位置。
     */
    function containerKeyOf(it) {
        if (it.type === "group") {
            return "group|" + (it.groupId || it.sourceId || "")
        }
        if (it.type === "folder") {
            return "folder|" + it.fileId
        }
        if (it.type === "space") {
            return "space|" + String(it.space)
        }
        return ""
    }

    function applyExpansion() {
        for (let i = 0; i < treeModel.count; ++i) {
            const it = treeModel.get(i)

            if (it.type !== "folder"
                && it.type !== "group"
                && it.type !== "space") {
                continue
            }

            const key = root.containerKeyOf(it)
            const v = root.expandedState[key]

            it.expanded = (v === undefined)
                          ? (it.type !== "folder")
                          : (v === true)

            treeModel.set(i, it)
        }

        refreshVisibility()
    }

    /*
     * 双击根节点：把它下面所有容器一起展开或折叠。
     *
     * treeModel 是前序遍历（父节点在前，depth 递增），
     * 所以从当前行往后扫到 depth 不大于自己即可。
     */
    function toggleSubtreeAt(index) {
        const node = root.snapshotOf(index)
        if (!node) return

        const target = !root.nodeExpanded(node)
        const state = root.expandedState

        state[root.nodeKeyOf(node)] = target

        for (let i = index + 1; i < treeModel.count; ++i) {
            const it = treeModel.get(i)

            if (it.depth <= node.depth) break

            const key = root.containerKeyOf(it)

            if (key.length > 0) {
                state[key] = target
            }
        }

        root.expandedState = state
        root.applyExpansion()

        root.setStatus(
            (target ? qsTr("已展开：") : qsTr("已折叠：")) + node.name,
            root.subtleText)
    }

    function setNodeExpanded(node, value) {
        const state = root.expandedState
        state[root.nodeKeyOf(node)] = value
        root.expandedState = state
        root.applyExpansion()
    }

    function collapseAll() {
        const files = allItems()
        const state = {}

        for (let i = 0; i < files.length; ++i) {
            if (Number(files[i].objectType) === root.objectFolder) {
                state["folder|" + files[i].fileId] = false
            }
        }

        // 三个空间根也跟着收起，只保留它们自己可见。
        state["space|" + String(root.spaceMyDoc)] = false
        state["space|" + String(root.spaceGroupMe)] = false
        state["space|" + String(root.spaceGroupUs)] = false

        root.expandedState = state
        rebuildTree()
    }

    // ---- 位置描述 ----

    function locationLabel() {
        var t = root.spaceTitle(root.currentSpace)

        if (root.currentSourceId.length > 0) {
            t += " › " + root.currentSourceId
        } else if (root.currentGroupId.length > 0
                   && root.currentSpace === root.spaceGroupMe) {
            t += " › " + root.currentGroupId
        }

        if (root.currentFolderName.length > 0) {
            t += " › " + root.currentFolderName
        }

        return t
    }

    function explorerTitle() {
        return root.locationLabel()
    }

    function labelForParentOf(node) {
        if (!node.parentId || node.parentId.length === 0) {
            return root.spaceTitle(node.space)
        }

        const files = allItems()

        for (let i = 0; i < files.length; ++i) {
            if (files[i].fileId === node.parentId) {
                return root.spaceTitle(node.space) + " › " + files[i].fileName
            }
        }

        return root.spaceTitle(node.space)
    }

    /*
     * 一个树节点对应哪个导入目标。
     *
     * 分类文件夹       -> 放进这个文件夹
     * 原子文件夹/文件   -> 放进它所在的那一层
     * 空间 / 群组根     -> 放进该空间的根
     * group us         -> null（只读，不接受导入）
     */
    function importTargetFor(node) {
        if (!node) return null
        if (node.space === root.spaceGroupUs) return null
        if (node.isReadOnly === true) return null

        if (node.type === "folder" && node.objectType === root.objectFolder) {
            return {
                space: node.space,
                sourceId: node.sourceId,
                groupId: node.groupId,
                parentId: node.fileId,
                label: root.spaceTitle(node.space) + " › " + node.name
            }
        }

        if (node.type === "folder" || node.type === "file") {
            return {
                space: node.space,
                sourceId: node.sourceId,
                groupId: node.groupId,
                parentId: node.parentId,
                label: root.labelForParentOf(node)
            }
        }

        return {
            space: node.space,
            sourceId: node.sourceId,
            groupId: node.groupId,
            parentId: "",
            label: root.spaceTitle(node.space)
        }
    }

    // ============================================================
    // 选择 / 导航
    // ============================================================

    function selectItem(item) {
        root.selectedId = item.fileId || ""
        root.selectedName = item.name || ""
        root.selectedObjectType = Number(item.objectType)
        root.selectedSpace = Number(item.space)
        root.selectedSourceId = item.sourceId || ""
        root.selectedGroupId = item.groupId || ""
        root.selectedParentId = item.parentId || ""
        root.selectedLocalPath = item.localPath || ""
        root.selectedRemotePath = item.remotePath || ""
        root.selectedReadOnly = item.isReadOnly === true
    }

    function openSpace(item) {
        root.currentSpace = Number(item.space)
        root.currentSourceId = item.sourceId || ""
        root.currentGroupId = item.groupId || ""

        /*
         * 点击空间根节点时，落到第一个具体群组 / 来源。
         *
         * 否则 currentGroupId 会停留在空字符串，
         * 导致中心列表把 group me 的内容全部过滤掉，
         * 看起来就像「加不进 group me」。
         */
        if (root.currentSpace === root.spaceGroupMe
            && root.currentGroupId.length === 0) {

            const ids = root.groupMeIds()

            if (ids.length > 0) {
                root.currentGroupId = ids[0]
            }
        }

        if (root.currentSpace === root.spaceGroupUs
            && root.currentSourceId.length === 0
            && sourceModel.count > 0) {

            root.currentSourceId = sourceModel.get(0).sourceId
        }

        root.currentFolderId = ""
        root.currentFolderName = item.type === "space" || item.type === "group"
                                 ? ""
                                 : (item.name || "")
        refreshCurrentFolder()
    }

    function openFolder(item) {
        root.currentSpace = Number(item.space)
        root.currentSourceId = item.sourceId || ""
        root.currentGroupId = item.groupId || ""
        root.currentFolderId = item.fileId || ""
        root.currentFolderName = item.name || ""
        refreshCurrentFolder()
    }

    /*
     * 单击树节点后的跳转。
     *
     * 同时把当前位置告诉用户，
     * 因为「📑 添加文件」与拖放都会加入这里。
     */
    function navigateTo(node) {
        if (node.type === "space" || node.type === "group") {
            root.openSpace(node)
        } else if (node.type === "folder") {
            root.openFolder(node)
        }

        root.setStatus(
            qsTr("加入目标：") + root.locationLabel(),
            root.subtleText)
    }

    // ============================================================
    // 打开文件
    // ============================================================

    /*
     * 把本地文件系统路径转成真正的 file:/// URL。
     *
     * PdfDocument.source 是 url 类型。
     *
     * 如果直接传 "C:/x/a.pdf"，
     * QML 会把它解析成 scheme 为 "c" 的非法 URL，
     * 文档无法加载，pageCount 恒为 0。
     */
    function toFileUrl(path) {
        if (!path) {
            return ""
        }

        var p = String(path)

        if (p.indexOf("file:") === 0) {
            return p
        }

        p = p.replace(/\\/g, "/")

        if (p.charAt(0) !== "/") {
            p = "/" + p
        }

        return "file://" + p
    }

    function openFile(item, fullscreen) {
        selectItem(item)

        const name = item.name || ""
        const id = item.fileId || ""

        if (!name.toLowerCase().endsWith(".pdf")) {
            setStatus(qsTr("当前仅支持 PDF 预览：") + name, "orange")
            return
        }

        // ---- 本地已经存在（刚导入，或已经下载过） ----

        if (id.length > 0 && fileService.existsLocally(id)) {

            const url = root.toFileUrl(item.localPath)

            root.pdfPreviewSource = url

            if (fullscreen) {
                root.openPdfFullscreen(url)
            }

            setStatus(qsTr("已打开：") + name, "green")
            return
        }

        if (!hasPdfSupport) {
            setStatus(qsTr("未安装 Qt PDF 模块"), "red")
            return
        }

        // ---- 云端条目：按需下载 ----

        if ((item.remotePath || "").length === 0) {
            setStatus(
                qsTr("该对象在本地和远端都不存在：") + name,
                "red")
            return
        }

        root.pendingDownloadId = id
        root.pendingOpenFullscreen = fullscreen

        setStatus(qsTr("正在下载：") + name, "blue")

        if (!syncService.downloadItem(id)) {
            setStatus(qsTr("无法开始下载：") + name, "red")
            root.pendingDownloadId = ""
        }
    }

    // ============================================================
    // 导入
    // ============================================================

    /*
     * 导入一个或多个本地文件。
     *
     * 单个文件：弹出对话框让用户选择分类与标签。
     * 多个文件：按「未分类」直接导入。
     */
    function importPaths(paths) {
        if (!paths || paths.length === 0) {
            return
        }

        if (root.currentSpace === root.spaceGroupUs) {
            setStatus(qsTr("group us 来自他人账号，不可导入"), "orange")
            return
        }

        // 没有指定行时，目标是当前位置。
        root.importPathsInto(paths, {
            space: root.currentSpace,
            sourceId: root.currentSourceId,
            groupId: root.currentGroupId,
            parentId: root.currentFolderId,
            label: root.locationLabel()
        })
    }

    function importPathsInto(paths, target) {
        if (!paths || paths.length === 0 || !target) {
            return
        }

        // 单个文件：让用户设置分类与标签。
        if (paths.length === 1) {
            metadataDialog.pendingPath = paths[0]
            metadataDialog.targetFromNode = true
            metadataDialog.targetSpace = target.space
            metadataDialog.targetSourceId = target.sourceId
            metadataDialog.targetGroupId = target.groupId
            metadataDialog.targetParentId = target.parentId
            metadataDialog.open()
            return
        }

        var imported = 0

        for (var i = 0; i < paths.length; ++i) {
            if (fileService.addLocalFile(
                    paths[i],
                    target.space,
                    target.parentId,
                    target.sourceId,
                    target.groupId,
                    3,
                    "")) {

                imported++
            }
        }

        refreshAll()

        setStatus(
            qsTr("已加入 ") + imported + qsTr(" / ") + paths.length
                + qsTr(" 个文件 → ") + target.label,
            imported > 0 ? "green" : "red")
    }

    // ============================================================
    // 群组来源
    // ============================================================

    function reloadSources() {
        sourceModel.clear()

        const sources = syncService.groupSources()

        for (let i = 0; i < sources.length; ++i) {
            const s = sources[i]
            sourceModel.append({
                sourceId: s.sourceId,
                displayName: s.displayName || s.sourceId,
                serverUrl: s.serverUrl || "",
                username: s.username || "",
                password: s.password || "",
                remoteRoot: s.remoteRoot || ""
            })
        }
    }

    // ============================================================
    // 布局
    // ============================================================

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 12
        // 底部留出状态栏的位置，避免面板被压住
        anchors.bottomMargin: 34
        spacing: 8

        // --------------------------------------------------------
        // 左：文件树
        // --------------------------------------------------------

        Rectangle {
            id: explorerPanel

            Layout.preferredWidth: 280
            Layout.minimumWidth: 150
            Layout.fillHeight: true
            radius: 14
            color: root.panelColor
            border.color: root.panelBorder
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                // ---- 标题栏（类似 VS Code 资源管理器） ----
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 6
                        text: root.explorerTitle()
                        font.bold: true
                        font.pixelSize: 11
                        color: root.subtleText
                        elide: Text.ElideMiddle
                    }

                    Button {
                        text: "🗂"
                        font.pixelSize: 12
                        implicitWidth: 26
                        implicitHeight: 22
                        flat: true
                        padding: 0
                        enabled: root.currentSpace !== root.spaceGroupUs
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("在当前位置新建分类文件夹")
                        onClicked: createFolderDialog.open()
                    }

                    Button {
                        text: "⇕"
                        font.pixelSize: 12
                        implicitWidth: 26
                        implicitHeight: 22
                        flat: true
                        padding: 0
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("全部折叠")
                        onClicked: root.collapseAll()
                    }

                    Button {
                        text: "⟳"
                        font.pixelSize: 12
                        implicitWidth: 26
                        implicitHeight: 22
                        flat: true
                        padding: 0
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("刷新")
                        onClicked: root.refreshAll()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 6
                    Layout.rightMargin: 6
                    height: 1
                    color: root.panelBorder
                }

                ListView {
                    id: treeList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 0
                    model: treeModel
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        id: treeRow

                        width: ListView.view ? ListView.view.width : 0
                        height: model.visible ? 24 : 0
                        visible: model.visible
                        radius: 4

                        /*
                         * Repeater 自己的 model 属性会遮蔽委托的 model，
                         * 所以先把 depth 取出来存到行属性上。
                         */
                        property int indentLevel: Number(model.depth)


                        color: {
                            if (root.dropHoverIndex === index) return root.rowDropHover
                            if (treeList.currentIndex === index) return root.rowSelected
                            if (rowMouse.hovered) return root.rowHover
                            return "transparent"
                        }

                        // ---- 缩进参考线 ----
                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 0

                            Repeater {
                                model: treeRow.indentLevel

                                delegate: Item {
                                    width: 14
                                    height: treeRow.height

                                    Rectangle {
                                        width: 1
                                        height: parent.height
                                        x: 6
                                        color: root.indentGuide
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: rowMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton

                            onClicked: function(mouse) {
                                // 快照必须在任何模型变动之前取，
                                // 否则委托里的 model 会失效。
                                const node = root.snapshotOf(index)
                                if (!node) return

                                treeList.currentIndex = index
                                root.selectItem(node)

                                if (mouse.button === Qt.RightButton) {
                                    const p = rowMouse.mapToItem(root, mouse.x, mouse.y)
                                    root.openContextMenu(p.x, p.y)
                                    return
                                }

                                if (node.type === "file") {
                                    root.openFile(node, false)
                                    return
                                }

                                // 单击文件夹即选中并展开
                                if (node.type === "folder") {
                                    root.setNodeExpanded(node, true)
                                }

                                root.navigateTo(node)
                            }

                            onDoubleClicked: {
                                const node = root.snapshotOf(index)
                                if (!node) return

                                if (node.type === "file") {
                                    root.openFile(node, true)
                                    return
                                }

                                /*
                                 * my doc / group me / group us 这些根节点双击时
                                 * 整棵子树一起展开或折叠。
                                 *
                                 * 普通文件夹只切换自己。
                                 */
                                if (node.type === "space" || node.type === "group") {
                                    root.toggleSubtreeAt(index)
                                } else {
                                    root.setNodeExpanded(node, !root.nodeExpanded(node))
                                }
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 4 + treeRow.indentLevel * 14
                            anchors.rightMargin: 8
                            spacing: 4

                            // 折叠箭头
                            Item {
                                Layout.preferredWidth: 14
                                Layout.fillHeight: true

                                Text {
                                    anchors.centerIn: parent
                                    visible: model.type === "folder" || model.type === "group"
                                    text: model.expanded ? "⌄" : "›"
                                    color: root.subtleText
                                    font.pixelSize: 13
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton

                                    onClicked: {
                                        const node = root.snapshotOf(index)
                                        if (!node) return
                                        if (node.type !== "folder" && node.type !== "group") return
                                        root.setNodeExpanded(node, !root.nodeExpanded(node))
                                    }
                                }
                            }

                            Text {
                                text: root.iconForNode(model)
                                font.pixelSize: 13
                                Layout.preferredWidth: 18
                            }

                            Text {
                                Layout.fillWidth: true
                                text: model.name || ""
                                color: root.textColor
                                font.pixelSize: 12
                                font.bold: model.type === "space"
                                elide: Text.ElideMiddle
                            }

                            Text {
                                visible: model.isCloud === true
                                text: "☁"
                                color: root.subtleText
                                font.pixelSize: 10
                            }

                            Text {
                                visible: model.isReadOnly === true
                                text: "🔒"
                                color: root.subtleText
                                font.pixelSize: 10
                            }
                        }

                        // ---- 每一行都是独立的投放目标 ----
                        DropArea {
                            anchors.fill: parent
                            keys: ["text/uri-list"]

                            onEntered: function(drag) {
                                const node = root.snapshotOf(index)
                                const target = node ? root.importTargetFor(node) : null

                                if (!target) {
                                    drag.accepted = false
                                    return
                                }

                                drag.accept(Qt.CopyAction)
                                root.dropHoverIndex = index
                                root.setStatus(
                                    qsTr("松开即可加入：") + target.label,
                                    root.accent)
                            }

                            onExited: {
                                if (root.dropHoverIndex === index) {
                                    root.dropHoverIndex = -1
                                }
                            }

                            onDropped: function(drop) {
                                const node = root.snapshotOf(index)
                                const target = node ? root.importTargetFor(node) : null

                                root.dropHoverIndex = -1

                                if (!target || !drop.hasUrls) {
                                    drop.accepted = false
                                    return
                                }

                                var paths = []
                                for (var i = 0; i < drop.urls.length; ++i) {
                                    paths.push(drop.urls[i].toString())
                                }

                                drop.accept(Qt.CopyAction)
                                root.importPathsInto(paths, target)
                            }
                        }
                    }
                }
            }
        }

        // --------------------------------------------------------
        // 中：内容
        // --------------------------------------------------------

        Rectangle {
            Layout.fillWidth: true
            Layout.minimumWidth: 160
            Layout.fillHeight: true
            radius: 14
            color: root.contentBg
            border.color: root.panelBorder
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: "📁 " + root.spaceTitle(root.currentSpace)
                          + (root.currentFolderName.length > 0
                             ? " / " + root.currentFolderName
                             : "")
                    color: root.textColor
                    font.bold: true
                    elide: Text.ElideMiddle
                }

                ListView {
                    Layout.fillWidth: true
                    // 有预览时只占一小条，把高度让给阅读区。
                    Layout.fillHeight: root.pdfPreviewSource.length === 0
                    Layout.minimumHeight: 72
                    Layout.preferredHeight: 132
                    Layout.maximumHeight: root.pdfPreviewSource.length === 0
                                         ? 100000
                                         : 220
                    clip: true
                    spacing: 1
                    model: folderFilesModel

                    delegate: Rectangle {
                        width: ListView.view ? ListView.view.width : 0
                        height: 30
                        radius: 6
                        color: mouseArea.containsMouse
                            ? root.rowSelected
                            : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8

                            Text {
                                text: model.objectType === root.objectAtomicFolder
                                      ? "🗃️"
                                      : (model.objectType === root.objectFolder
                                         ? "🗂" : "📄")
                                font.pixelSize: 15
                                Layout.preferredWidth: 22
                            }

                            Text {
                                text: model.name
                                color: root.textColor
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            Text {
                                text: root.fileSizeText(model.size)
                                color: root.subtleText
                                font.pixelSize: 11
                            }

                            Text {
                                text: model.categoryText
                                color: root.subtleText
                                font.pixelSize: 11
                            }
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton

                            onClicked: function(mouse) {
                                const node = root.snapshotOfRow(model)
                                if (!node) return

                                root.selectItem(node)

                                if (mouse.button === Qt.RightButton) {
                                    if (node.objectType !== root.objectFolder) {
                                        const p = mouseArea.mapToItem(root, mouse.x, mouse.y)
                                        root.openContextMenu(p.x, p.y)
                                    }
                                    return
                                }

                                if (model.objectType === root.objectFolder
                                    || model.objectType === root.objectAtomicFolder) {
                                    root.openFolder(model)
                                } else {
                                    root.openFile(model, false)
                                }
                            }

                            onDoubleClicked: {
                                if (model.objectType === root.objectAtomicFile) {
                                    root.openFile(model, true)
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    id: previewFrame

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 260
                    Layout.preferredHeight: 700

                    color: root.previewBg
                    radius: 10
                    clip: true

                    visible: root.pdfPreviewSource.length > 0

                    Loader {
                        id: pdfLoader

                        anchors.fill: parent

                        active: root.pdfPreviewSource.length > 0 && hasPdfSupport
                        source: active
                            ? "qrc:/qt/qml/LitApp/qml/PdfPreviewPane.qml"
                            : ""

                        onLoaded: {
                            item.paletteText = root.textColor
                            root.syncPreviewSource()
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: !pdfLoader.active
                        color: root.subtleText
                        text: qsTr("未安装 Qt PDF 模块，无法预览")
                    }
                }
            }
        }

        // --------------------------------------------------------
        // 右：工具条
        // --------------------------------------------------------

        Rectangle {
            Layout.preferredWidth: 60
            Layout.fillHeight: true
            radius: 14
            color: root.panelColor
            border.color: root.panelBorder
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8

                Item { Layout.fillHeight: true }

                Button {
                    id: addFileButton

                    text: "📑"
                    implicitWidth: 46
                    implicitHeight: 46
                    flat: true
                    padding: 0
                    Layout.alignment: Qt.AlignHCenter

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("添加文件（也可以直接拖拽到窗口里）")

                    onClicked: fileDialog.open()
                }


                Button {
                    text: "🗑️"
                    implicitWidth: 46
                    implicitHeight: 46
                    flat: true
                    padding: 0
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: {
                        root.reloadTrash()
                        settingsPopup.panelMode = "recycle"
                        settingsPopup.open()
                    }
                }

                Button {
                    text: "🔄"
                    implicitWidth: 46
                    implicitHeight: 46
                    flat: true
                    padding: 0
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: {
                        settingsPopup.panelMode = "sync"
                        settingsPopup.open()
                    }
                }

                Button {
                    text: "🤝"
                    implicitWidth: 46
                    implicitHeight: 46
                    flat: true
                    padding: 0
                    Layout.alignment: Qt.AlignHCenter

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("群组来源")

                    onClicked: {
                        root.reloadSources()
                        settingsPopup.panelMode = "sources"
                        settingsPopup.open()
                    }
                }

                Button {
                    text: "🔑"
                    implicitWidth: 46
                    implicitHeight: 46
                    flat: true
                    padding: 0
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: {
                        settingsPopup.panelMode = "account"
                        settingsPopup.open()
                    }
                }
            }
        }
    }

    // ============================================================
    // 状态栏
    // ============================================================

    Label {
        id: statusLabel

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 14
        height: 18

        color: root.subtleText
        font.pixelSize: 12
        elide: Text.ElideRight
        text: qsTr("就绪")
    }

    // ============================================================
    // 右键菜单
    // ============================================================
    Menu {
        id: contextMenu

        /*
         * 菜单项是按上下文动态生成的。
         *
         * 之前用一堆 visible: 绑定来隐藏菜单项，
         * 导致 Menu 的隐式高度与内容不同步，
         * 出现「边框超高、上方留白」。
         *
         * 现在每次打开前重建动作列表，
         * 菜单只包含真正要显示的项。
         */
        Instantiator {
            model: root.contextActions

            delegate: MenuItem {
                required property var modelData

                text: modelData.text || ""
                enabled: modelData.enabled !== false

                onTriggered: {
                    if (modelData.run) {
                        modelData.run()
                    }
                }

                background: Rectangle {
                    implicitWidth: 210
                    implicitHeight: 28
                    radius: 4

                    color: parent.highlighted
                           ? root.rowSelected
                           : "transparent"

                    // 分组分隔线
                    Rectangle {
                        visible: modelData.groupStart === true
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        height: 1
                        color: root.panelBorder
                    }
                }
            }

            onObjectAdded: function(index, object) {
                contextMenu.insertItem(index, object)
            }

            onObjectRemoved: function(index, object) {
                contextMenu.removeItem(object)
            }
        }
    }

    /*
     * 根据当前选择构建右键菜单，并在鼠标位置稍偏移处弹出。
     *
     * 偏移是为了不紧贴窗口左边缘。
     */
    function openContextMenu(x, y) {
        root.contextActions = root.buildContextActions()

        if (root.contextActions.length === 0) {
            return
        }

        contextMenu.popup(root,
                          Math.max(8, Math.round(x) + 6),
                          Math.max(8, Math.round(y) + 6))
    }

    function buildContextActions() {
        var actions = []

        const hasSelection = root.selectedId.length > 0
        const isFile = root.selectedObjectType === root.objectAtomicFile
        const isAtomicFolder = root.selectedObjectType === root.objectAtomicFolder
        const readOnly = root.selectedReadOnly === true
        const inGroupUs = root.currentSpace === root.spaceGroupUs

        if (isFile) {
            actions.push({
                text: qsTr("打开"),
                enabled: true,
                run: function() { root.openSelectedFile(false) }
            })
            actions.push({
                text: qsTr("全屏阅读"),
                enabled: true,
                run: function() { root.openSelectedFile(true) }
            })
        }

        if (hasSelection) {
            actions.push({
                text: qsTr("新建分类文件夹"),
                enabled: !inGroupUs,
                groupStart: isFile,
                run: function() { createFolderDialog.open() }
            })

            actions.push({
                text: qsTr("重命名"),
                enabled: !readOnly,
                run: function() {
                    renameDialog.oldName = root.selectedName
                    renameDialog.open()
                }
            })

            if (!isFile) {
                actions.push({
                    text: qsTr("移动到…"),
                    enabled: !readOnly,
                    run: function() {
                        root.buildMoveTargets()
                        moveDialog.open()
                    }
                })
            }

            actions.push({
                text: qsTr("复制到当前文件夹"),
                enabled: !readOnly,
                run: function() {
                    const ok = fileService.copyItem(
                        root.selectedId, root.currentFolderId)
                    refreshAll()
                    root.setStatus(
                        ok ? qsTr("已复制：") + root.selectedName
                           : qsTr("复制失败"),
                        ok ? "green" : "red")
                }
            })

            actions.push({
                text: qsTr("移到回收站"),
                enabled: !readOnly,
                groupStart: true,
                run: function() {
                    const ok = fileService.moveToTrash(root.selectedId)
                    refreshAll()
                    root.setStatus(
                        ok ? qsTr("已移到回收站：") + root.selectedName
                           : qsTr("移到回收站失败"),
                        ok ? "orange" : "red")
                }
            })

            if (isAtomicFolder && !readOnly) {
                actions.push({
                    text: qsTr("从远端永久删除"),
                    enabled: true,
                    run: function() {
                        deleteRemoteDialog.targetName = root.selectedName
                        deleteRemoteDialog.open()
                    }
                })
            }
        }

        return actions
    }

    function openSelectedFile(fullscreen) {
        const files = allItems()
        for (let i = 0; i < files.length; ++i) {
            if (files[i].fileId === root.selectedId) {
                root.openFile(files[i], fullscreen)
                return
            }
        }
    }

    function buildMoveTargets() {
        moveTargetModel.clear()

        moveTargetModel.append({
            fileId: "",
            path: qsTr("<空间根目录>"),
            depth: 0
        })

        const choices = fileService.folderChoices(
            root.selectedSpace,
            root.selectedSourceId,
            root.selectedGroupId,
            root.selectedId)

        for (let i = 0; i < choices.length; ++i) {
            moveTargetModel.append({
                fileId: choices[i].fileId,
                path: choices[i].path,
                depth: choices[i].depth
            })
        }
    }

    // ============================================================
    // 添加文件
    // ============================================================

    FileDialog {
        id: fileDialog
        title: qsTr("选择要加入的文件")
        nameFilters: [
            qsTr("文献文件 (*.pdf *.md *.markdown *.doc *.docx *.txt)"),
            qsTr("所有文件 (*)")
        ]
        onAccepted: root.importPaths([selectedFile.toString()])
    }

    // ============================================================
    // 导入元数据
    // ============================================================

    Dialog {
        id: metadataDialog

        title: qsTr("设置分类与标签")
        modal: true
        width: 400
        anchors.centerIn: Overlay.overlay

        property string pendingPath: ""

        // 目标位置：来自拖拽所在的行，或当前位置。
        property bool targetFromNode: false
        property int targetSpace: -1
        property string targetSourceId: ""
        property string targetGroupId: ""
        property string targetParentId: ""

        function resolvedTarget() {
            if (metadataDialog.targetFromNode) {
                return {
                    space: metadataDialog.targetSpace,
                    sourceId: metadataDialog.targetSourceId,
                    groupId: metadataDialog.targetGroupId,
                    parentId: metadataDialog.targetParentId
                }
            }
            return {
                space: root.currentSpace,
                sourceId: root.currentSourceId,
                groupId: root.currentGroupId,
                parentId: root.currentFolderId
            }
        }

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 11
                color: root.accent
                text: qsTr("加入位置：") + root.locationLabel()
            }

            Label {
                text: qsTr("原分类")
                color: root.textColor
            }

            ComboBox {
                id: categoryCombo
                Layout.fillWidth: true
                model: [
                    qsTr("书籍"),
                    qsTr("期刊论文"),
                    qsTr("Markdown 原子知识"),
                    qsTr("未分类")
                ]
            }

            Label {
                text: qsTr("自定义标签（逗号分隔）")
                color: root.textColor
            }

            TextField {
                id: tagsField
                Layout.fillWidth: true
                placeholderText: qsTr("例如：量子计算, 2025, 精读")
            }

            Label {
                Layout.fillWidth: true
                color: root.subtleText
                font.pixelSize: 11
                wrapMode: Text.Wrap
                text: qsTr("文件会被复制到本地镜像，并在下次同步时上传到远端。")
            }
        }

        footer: DialogButtonBox {
            alignment: Qt.AlignHCenter

            Button {
                text: qsTr("取消")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("确定")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }

        onAccepted: {
            if (metadataDialog.pendingPath.length === 0) {
                root.setStatus(qsTr("没有选择有效文件"), "red")
                return
            }

            const target = metadataDialog.resolvedTarget()

            const ok = fileService.addLocalFile(
                metadataDialog.pendingPath,
                target.space,
                target.parentId,
                target.sourceId,
                target.groupId,
                categoryCombo.currentIndex,
                tagsField.text.trim())

            refreshAll()

            root.setStatus(
                ok ? qsTr("已加入文件，创建原子文件夹")
                   : qsTr("加入文件失败"),
                ok ? "green" : "red")

            metadataDialog.pendingPath = ""
            metadataDialog.targetFromNode = false
            tagsField.text = ""
            categoryCombo.currentIndex = 0
        }
    }

    // ============================================================
    // 新建分类文件夹
    // ============================================================

    Dialog {
        id: createFolderDialog

        title: qsTr("新建分类文件夹")
        modal: true
        width: 360
        anchors.centerIn: Overlay.overlay

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                text: qsTr("文件夹名称")
                color: root.textColor
            }

            TextField {
                id: newFolderNameField
                Layout.fillWidth: true
                placeholderText: qsTr("例如：机器学习")
                selectByMouse: true
            }

            Label {
                Layout.fillWidth: true
                color: root.subtleText
                font.pixelSize: 11
                wrapMode: Text.Wrap
                text: qsTr("分类文件夹只是应用内的逻辑层级，不会在远端创建目录。")
            }
        }

        footer: DialogButtonBox {
            alignment: Qt.AlignHCenter

            Button {
                text: qsTr("取消")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("确定")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }

        onAccepted: {
            const ok = fileService.createFolder(
                newFolderNameField.text.trim(),
                root.currentSpace,
                root.currentFolderId,
                root.currentSourceId,
                root.currentGroupId)

            refreshAll()

            root.setStatus(
                ok ? qsTr("已创建文件夹")
                   : qsTr("创建文件夹失败"),
                ok ? "green" : "red")

            newFolderNameField.text = ""
        }
    }

    // ============================================================
    // 重命名
    // ============================================================

    Dialog {
        id: renameDialog

        title: qsTr("重命名")
        modal: true
        width: 360
        anchors.centerIn: Overlay.overlay

        property string oldName: ""

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                text: qsTr("新名称")
                color: root.textColor
            }

            TextField {
                id: renameField
                Layout.fillWidth: true
                selectByMouse: true
            }
        }

        footer: DialogButtonBox {
            alignment: Qt.AlignHCenter

            Button {
                text: qsTr("取消")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("确定")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }

        onOpened: {
            renameField.text = renameDialog.oldName
            renameField.selectAll()
            renameField.forceActiveFocus()
        }

        onAccepted: {
            const ok = fileService.renameItem(
                root.selectedId,
                renameField.text.trim())

            refreshAll()

            root.setStatus(
                ok ? qsTr("已重命名")
                   : qsTr("重命名失败"),
                ok ? "green" : "red")
        }
    }

    // ============================================================
    // 移动
    // ============================================================

    Dialog {
        id: moveDialog

        title: qsTr("移动到…")
        modal: true
        width: 420
        anchors.centerIn: Overlay.overlay

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: root.textColor
                text: qsTr("移动对象：") + root.selectedName
            }

            Label {
                text: qsTr("目标文件夹")
                color: root.textColor
            }

            ComboBox {
                id: moveTargetCombo
                Layout.fillWidth: true
                model: moveTargetModel
                textRole: "path"
                valueRole: "fileId"
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 11
                color: root.subtleText
                text: qsTr("移动只改变应用内的逻辑层级，不影响远端结构，也不会影响上级对象。")
            }
        }

        footer: DialogButtonBox {
            alignment: Qt.AlignHCenter

            Button {
                text: qsTr("取消")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("确定")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }

        onAccepted: {
            const ok = fileService.moveItemTo(
                root.selectedId,
                moveTargetCombo.currentValue)

            refreshAll()

            root.setStatus(
                ok ? qsTr("已移动：") + root.selectedName
                   : qsTr("移动失败"),
                ok ? "green" : "red")
        }
    }

    // ============================================================
    // 远端删除确认
    // ============================================================

    Dialog {
        id: deleteRemoteDialog

        title: qsTr("从远端删除")
        modal: true
        width: 420
        anchors.centerIn: Overlay.overlay

        property string targetName: ""

        contentItem: Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: root.textColor
            text: qsTr("将删除远端原子文件夹及其全部内容：")
                  + deleteRemoteDialog.targetName
        }

        footer: DialogButtonBox {
            alignment: Qt.AlignHCenter

            Button {
                text: qsTr("取消")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("删除")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }

        onAccepted: {
            syncService.deleteRemoteAtomicFolder(root.selectedId)
        }
    }

    // ============================================================
    // 群组来源编辑
    // ============================================================

    Dialog {
        id: sourceDialog

        title: qsTr("群组来源")
        modal: true
        width: 460
        anchors.centerIn: Overlay.overlay

        property string editingId: ""

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 11
                color: root.subtleText
                text: qsTr("他人账号只读：本应用只读取对方群组目录，永不写入。")
            }

            TextField {
                id: sourceIdField
                Layout.fillWidth: true
                placeholderText: qsTr("来源标识（英文，例如 alice）")
                enabled: sourceDialog.editingId.length === 0
            }

            TextField {
                id: sourceNameField
                Layout.fillWidth: true
                placeholderText: qsTr("显示名称")
            }

            TextField {
                id: sourceUrlField
                Layout.fillWidth: true
                placeholderText: qsTr("服务器地址 https://dav.example.com/")
            }

            TextField {
                id: sourceUserField
                Layout.fillWidth: true
                placeholderText: qsTr("对方用户名")
            }

            TextField {
                id: sourcePassField
                Layout.fillWidth: true
                placeholderText: qsTr("对方密码")
                echoMode: TextInput.Password
            }

            TextField {
                id: sourceRootField
                Layout.fillWidth: true
                placeholderText: qsTr("对方群组目录 /zesbks/group/group me")
            }
        }

        footer: DialogButtonBox {
            alignment: Qt.AlignHCenter

            Button {
                text: qsTr("取消")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }

            Button {
                text: qsTr("保存")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }

        onAccepted: {
            const ok = syncService.saveGroupSource(
                sourceIdField.text.trim(),
                sourceNameField.text.trim(),
                sourceUrlField.text.trim(),
                sourceUserField.text.trim(),
                sourcePassField.text,
                sourceRootField.text.trim())

            reloadSources()
            rebuildTree()

            root.setStatus(
                ok ? qsTr("来源已保存")
                   : qsTr("来源保存失败（需要标识与地址）"),
                ok ? "green" : "red")

            if (ok) {
                syncService.syncGroupUs(sourceIdField.text.trim())
            }
        }
    }

    // ============================================================
    // 设置面板
    // ============================================================

    Popup {
        id: settingsPopup

        // 窗口变窄时贴左边界，不允许跑出窗口。
        x: Math.max(8, root.width - width - 80)
        y: 40

        width: Math.max(260, Math.min(340, root.width - 24))
        padding: 16

        property string panelMode: "account"

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                Layout.fillWidth: true
                font.bold: true
                font.pixelSize: 17
                color: root.textColor
                horizontalAlignment: Text.AlignHCenter
                text: {
                    if (settingsPopup.panelMode === "account") return qsTr("账号")
                    if (settingsPopup.panelMode === "sync") return qsTr("数据同步")
                    if (settingsPopup.panelMode === "sources") return qsTr("群组来源")
                    return qsTr("回收站")
                }
            }

            StackLayout {
                Layout.fillWidth: true
                currentIndex: {
                    if (settingsPopup.panelMode === "account") return 0
                    if (settingsPopup.panelMode === "sync") return 1
                    if (settingsPopup.panelMode === "sources") return 2
                    return 3
                }

                // ---- 账号 ----
                ColumnLayout {
                    spacing: 8

                    TextField {
                        id: accountEmailField
                        Layout.fillWidth: true
                        placeholderText: qsTr("邮箱")
                    }

                    TextField {
                        id: accountPasswordField
                        Layout.fillWidth: true
                        placeholderText: qsTr("密码")
                        echoMode: TextInput.Password
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            Layout.fillWidth: true
                            text: qsTr("登录")
                            onClicked: authService.login(
                                accountEmailField.text.trim(),
                                accountPasswordField.text)
                        }

                        Button {
                            Layout.fillWidth: true
                            text: qsTr("注册")
                            onClicked: authService.registerAccount(
                                accountEmailField.text.trim(),
                                accountPasswordField.text)
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        color: root.subtleText
                        elide: Text.ElideMiddle
                        text: authService.email().length > 0
                              ? authService.email()
                              : qsTr("未登录")
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("退出登录")
                        visible: authService.isLoggedIn()
                        onClicked: {
                            authService.logout()
                            syncService.stopAutoSync()
                            root.logoutRequested()
                        }
                    }
                }

                // ---- 同步 ----
                ColumnLayout {
                    spacing: 8

                    TextField {
                        id: serverField
                        Layout.fillWidth: true
                        placeholderText: qsTr("服务器地址 https://dav.example.com/")
                    }

                    TextField {
                        id: usernameField
                        Layout.fillWidth: true
                        placeholderText: qsTr("用户名")
                    }

                    TextField {
                        id: passwordField
                        Layout.fillWidth: true
                        placeholderText: qsTr("密码")
                        echoMode: TextInput.Password
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("保存并连接")
                        onClicked: {
                            webDavClient.setServer(
                                serverField.text.trim(),
                                usernameField.text.trim(),
                                passwordField.text)

                            root.setStatus(qsTr("正在验证并创建远端目录…"), "blue")

                            syncService.ensureRemoteLayout()
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("立即同步 my doc")
                        onClicked: {
                            syncService.syncMyDoc()
                            root.setStatus(qsTr("正在同步…"), "blue")
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        font.pixelSize: 11
                        color: root.subtleText
                        text: qsTr("同步根目录：/zesbks/")
                              + "\n"
                              + qsTr("群组根目录：/zesbks/group/group me/")
                    }
                }

                // ---- 群组来源 ----
                ColumnLayout {
                    spacing: 8

                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180
                        clip: true
                        model: sourceModel

                        delegate: RowLayout {
                            width: ListView.view ? ListView.view.width : 0
                            spacing: 6

                            Label {
                                Layout.fillWidth: true
                                elide: Text.ElideMiddle
                                color: root.textColor
                                text: model.displayName
                            }

                            Button {
                                text: qsTr("同步")
                                onClicked: {
                                    syncService.syncGroupUs(model.sourceId)
                                    root.setStatus(
                                        qsTr("正在读取来源…"), "blue")
                                }
                            }

                            Button {
                                text: qsTr("删除")
                                onClicked: {
                                    syncService.removeGroupSource(model.sourceId)
                                    root.reloadSources()
                                    root.refreshAll()
                                }
                            }
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("新增来源")
                        onClicked: {
                            sourceDialog.editingId = ""
                            sourceIdField.text = ""
                            sourceNameField.text = ""
                            sourceUrlField.text = ""
                            sourceUserField.text = ""
                            sourcePassField.text = ""
                            sourceRootField.text = "/zesbks/group/group me"
                            sourceDialog.open()
                        }
                    }
                }

                // ---- 回收站 ----
                ColumnLayout {
                    spacing: 8

                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 200
                        clip: true
                        model: trashModel

                        delegate: RowLayout {
                            width: ListView.view ? ListView.view.width : 0
                            spacing: 6

                            Label {
                                Layout.fillWidth: true
                                elide: Text.ElideMiddle
                                color: root.textColor
                                text: model.fileName
                            }

                            Button {
                                text: qsTr("恢复")
                                onClicked: {
                                    fileService.restoreFromTrash(model.fileId)
                                    root.reloadTrash()
                                    root.refreshAll()
                                }
                            }

                            Button {
                                text: qsTr("彻底删除")
                                onClicked: {
                                    fileService.permanentlyDelete(model.fileId)
                                    root.reloadTrash()
                                    root.refreshAll()
                                }
                            }
                        }
                    }

                    Button {
                        Layout.fillWidth: true
                        text: qsTr("刷新")
                        onClicked: root.reloadTrash()
                    }
                }
            }
        }
    }

    ListModel { id: trashModel }

    function reloadTrash() {
        trashModel.clear()

        const items = fileService.trashItems(
            root.currentSpace,
            root.currentSourceId,
            root.currentGroupId)

        for (let i = 0; i < items.length; ++i) {
            trashModel.append({
                fileId: items[i].fileId,
                fileName: items[i].fileName
            })
        }
    }

    // ============================================================
    // 与后端联动
    // ============================================================

    Connections {
        target: syncService

        function onSyncFinished(ok, message) {
            root.setStatus(message, ok ? "green" : "red")
            if (ok) {
                root.refreshAll()
            }
        }

        function onItemDownloaded(fileId, remotePath, localPath, ok, error) {
            if (fileId !== root.pendingDownloadId) {
                return
            }

            if (ok) {

                const url = root.toFileUrl(localPath)

                root.pdfPreviewSource = url

                if (root.pendingOpenFullscreen) {
                    root.openPdfFullscreen(url)
                }

                refreshAll()
                root.setStatus(qsTr("已下载并打开"), "green")
            } else {
                root.setStatus(qsTr("下载失败：") + error, "red")
            }

            root.pendingDownloadId = ""
            root.pendingOpenFullscreen = false
        }

        function onLibraryChanged() {
            refreshAll()
        }

        function onStatusMessage(message) {
            root.setStatus(message, "green")
        }

        function onRemoteDeletionFinished(ok, message) {
            root.setStatus(message, ok ? "green" : "red")
            refreshAll()
        }
    }

    Connections {
        target: authService

        function onLoginFinished(ok, email, error) {
            root.setStatus(ok ? qsTr("已登录：") + email : error,
                           ok ? "green" : "red")
        }

        function onRegisterFinished(ok, email, error) {
            root.setStatus(ok ? qsTr("已注册：") + email : error,
                           ok ? "green" : "red")
        }
    }

    // ============================================================
    // 拖拽导入
    // ============================================================

    /*
     * 把文件从资源管理器拖进窗口即可加入。
     *
     * 目标位置就是当前选中的空间 / 群组 / 分类文件夹，
     * 因此先点一下 group me 再拖拽，就会加进 group me。
     */
    DropArea {
        id: dropArea

        anchors.fill: parent
        keys: ["text/uri-list"]

        onEntered: function(drag) {
            drag.accept(Qt.CopyAction)
            root.setStatus(
                qsTr("松开即可加入：") + root.dropTargetLabel(),
                "#3080d0")
        }

        onExited: root.setStatus(qsTr("就绪"), root.subtleText)

        onDropped: function(drop) {
            if (!drop.hasUrls) {
                drop.accepted = false
                return
            }

            var paths = []

            for (var i = 0; i < drop.urls.length; ++i) {
                paths.push(drop.urls[i].toString())
            }

            drop.accept(Qt.CopyAction)
            root.importPaths(paths)
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 6
        radius: 14
        color: "transparent"
        border.width: 2
        border.color: "#3080d0"
        visible: dropArea.containsDrag
        z: 100

        Label {
            anchors.centerIn: parent
            color: "#3080d0"
            font.pixelSize: 16
            font.bold: true
            text: qsTr("松开以加入：") + root.dropTargetLabel()
        }
    }

    function dropTargetLabel() {
        var label = root.spaceTitle(root.currentSpace)

        if (root.currentSourceId.length > 0) {
            label += " / " + root.currentSourceId
        } else if (root.currentGroupId.length > 0) {
            label += " / " + root.currentGroupId
        }

        if (root.currentFolderName.length > 0) {
            label += " / " + root.currentFolderName
        }

        return label
    }
}
