import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore

Page {
    id: root

    signal logoutRequested()
    signal openPdfFullscreen(string path)

    background: Rectangle {
        color: root.contentBg
    }

    // ============================================================
    // Selection
    // ============================================================

    property string selectedId: ""
    property string selectedName: ""
    property string selectedLocalPath: ""
    property string selectedRemotePath: ""
    property int selectedSpace: -1
    property int selectedObjectType: -1
    property string selectedSourceId: ""

    // ============================================================
    // PDF
    // ============================================================

    property string pdfPreviewSource: ""
    property string pendingDownloadId: ""
    property string pendingDownloadRemote: ""
    property string pendingDownloadName: ""
    property bool pendingOpenFullscreen: false

    // ============================================================
    // Settings
    // ============================================================

    property string activeSettingsPopup: ""

    property int themeIndex: 0

    property color panelColor:
        themeIndex === 1 ? "#2b2b2b" : "#f4f6f8"

    property color panelBorder:
        themeIndex === 1 ? "#444444" : "#d8dee6"

    property color contentBg:
        themeIndex === 1 ? "#1e1e1e" : "#ffffff"

    property color textColor:
        themeIndex === 1 ? "#eeeeee" : "#222222"

    property color subtleText:
        themeIndex === 1 ? "#aaaaaa" : "#666666"

    property color previewBg:
        themeIndex === 1 ? "#252525" : "#fafafa"

    // ============================================================
    // Current logical location
    // ============================================================

    property int currentSpace: 0
    property string currentSourceId: ""
    property string currentFolderId: ""
    property string currentFolderName: ""

    property string databasePath: "litapp_metadata.db"

    property string localDocumentsRoot:
        StandardPaths.writableLocation(
            StandardPaths.DocumentsLocation)

    // ============================================================
    // Models
    // ============================================================

    ListModel {
        id: treeModel
    }

    ListModel {
        id: folderFilesModel
    }

    // ============================================================
    // Constants
    // ============================================================

    readonly property int spaceMyDoc: 0
    readonly property int spaceGroupMe: 1
    readonly property int spaceGroupUs: 2

    readonly property int objectFolder: 0
    readonly property int objectAtomicFolder: 1
    readonly property int objectAtomicFile: 2

    // ============================================================
    // Startup
    // ============================================================

    Component.onCompleted: {
        webDavClient.restoreSettings()

        serverField.text = webDavClient.serverUrl()
        usernameField.text = webDavClient.username()
        passwordField.text = webDavClient.password()

        refreshFiles()

        if (syncService.setActiveSpace !== undefined) {
            syncService.setActiveSpace(
                root.spaceMyDoc,
                "")
        }

        syncService.startAutoSync(180000)
    }

    // ============================================================
    // PDF preview
    // ============================================================

    onPdfPreviewSourceChanged: {
        if (pdfLoader.item) {
            pdfLoader.item.source =
                root.pdfPreviewSource
        }
    }

    // ============================================================
    // Basic helpers
    // ============================================================

    function spaceTitle(space) {
        switch (space) {
        case root.spaceMyDoc:
            return qsTr("my doc")

        case root.spaceGroupMe:
            return qsTr("group me")

        case root.spaceGroupUs:
            return qsTr("group us")

        default:
            return qsTr("未知")
        }
    }

    function objectTypeName(type) {
        switch (type) {
        case root.objectFolder:
            return qsTr("文件夹")

        case root.objectAtomicFolder:
            return qsTr("原子文件夹")

        case root.objectAtomicFile:
            return qsTr("原子文件")

        default:
            return qsTr("对象")
        }
    }

    function isFolderType(type) {
        return type === root.objectFolder
            || type === root.objectAtomicFolder
    }

    function fileNameWithoutExtension(name) {
        const dot = name.lastIndexOf(".")

        if (dot <= 0) {
            return name
        }

        return name.substring(0, dot)
    }

    function iconForItem(item) {
        if (item.type === "space") {
            if (item.space === root.spaceMyDoc) {
                return "📚"
            }

            if (item.space === root.spaceGroupMe) {
                return "👤"
            }

            return "👥"
        }

        if (item.type === "folder") {
            return item.expanded ? "📂" : "📁"
        }

        if (item.objectType === root.objectAtomicFolder) {
            return "🗃️"
        }

        return "📄"
    }

    // ============================================================
    // Refresh
    // ============================================================

    function refreshFiles() {
        fileListModel.reloadAllFromStore(
            root.databasePath)

        rebuildTree()

        /*
         * 如果当前正在打开某个逻辑文件夹，
         * 刷新后同步更新中央区域。
         */
        if (root.currentFolderId.length > 0) {
            refreshCurrentFolder()
        } else {
            folderFilesModel.clear()
        }
    }

    function refreshCurrentFolder() {
        folderFilesModel.clear()

        if (root.currentFolderId.length === 0) {
            return
        }

        const files =
            fileListModel.toVariantList()

        for (let i = 0; i < files.length; ++i) {
            const f = files[i]

            if (Number(f.space) !==
                    root.currentSpace) {
                continue
            }

            if ((f.sourceId || "") !==
                    (root.currentSourceId || "")) {
                continue
            }

            if ((f.parentId || "") !==
                    root.currentFolderId) {
                continue
            }

            folderFilesModel.append({
                fileId: f.fileId,
                name: f.fileName,
                localPath: f.localPath || "",
                remotePath: f.remotePath || "",
                objectType: Number(f.objectType),
                isCloud: f.isCloud === true,
                categoryText: f.categoryText || ""
            })
        }
    }

    // ============================================================
    // Tree construction
    // ============================================================

    function rebuildTree() {
        treeModel.clear()

        const files =
            fileListModel.toVariantList()

        appendSpace(
            qsTr("my doc"),
            root.spaceMyDoc,
            "",
            files)

        appendSpace(
            qsTr("group me"),
            root.spaceGroupMe,
            "",
            files)

        appendSpace(
            qsTr("group us"),
            root.spaceGroupUs,
            "",
            files)

        rebuildTreeVisibility()
    }

    function appendSpace(
            title,
            space,
            sourceId,
            files) {

        treeModel.append({
            type: "space",
            name: title,
            depth: 0,
            visible: true,
            expanded: true,
            space: space,
            sourceId: sourceId,
            fileId: "",
            objectType: -1,
            parentId: ""
        })

        appendChildren(
            "",
            space,
            sourceId,
            1,
            files)
    }

    function appendChildren(
            parentId,
            space,
            sourceId,
            depth,
            files) {

        const folders = []
        const atomicFolders = []
        const atomicFiles = []

        for (let i = 0; i < files.length; ++i) {
            const f = files[i]

            if (Number(f.space) !==
                    Number(space)) {
                continue
            }

            if ((f.sourceId || "") !==
                    (sourceId || "")) {
                continue
            }

            if ((f.parentId || "") !==
                    (parentId || "")) {
                continue
            }

            const type =
                Number(f.objectType)

            if (type === root.objectFolder) {
                folders.push(f)
            } else if (type === root.objectAtomicFolder) {
                atomicFolders.push(f)
            } else if (type === root.objectAtomicFile) {
                atomicFiles.push(f)
            }
        }

        folders.sort(function(a, b) {
            return String(a.fileName)
                .localeCompare(
                    String(b.fileName))
        })

        atomicFolders.sort(function(a, b) {
            return String(a.fileName)
                .localeCompare(
                    String(b.fileName))
        })

        atomicFiles.sort(function(a, b) {
            return String(a.fileName)
                .localeCompare(
                    String(b.fileName))
        })

        for (let i = 0;
             i < folders.length;
             ++i) {

            const f = folders[i]

            treeModel.append({
                type: "folder",
                name: f.fileName,
                depth: depth,
                visible: true,
                expanded: false,
                space: space,
                sourceId: sourceId,
                fileId: f.fileId,
                parentId: f.parentId || "",
                objectType: root.objectFolder,
                localPath: f.localPath || "",
                remotePath: f.remotePath || "",
                isCloud: f.isCloud === true
            })
        }

        for (let i = 0;
             i < atomicFolders.length;
             ++i) {

            const f = atomicFolders[i]

            treeModel.append({
                type: "folder",
                name: f.fileName,
                depth: depth,
                visible: true,
                expanded: false,
                space: space,
                sourceId: sourceId,
                fileId: f.fileId,
                parentId: f.parentId || "",
                objectType: root.objectAtomicFolder,
                localPath: f.localPath || "",
                remotePath: f.remotePath || "",
                isCloud: f.isCloud === true
            })
        }

        for (let i = 0;
             i < atomicFiles.length;
             ++i) {

            const f = atomicFiles[i]

            treeModel.append({
                type: "file",
                name: f.fileName,
                depth: depth,
                visible: true,
                expanded: false,
                space: space,
                sourceId: sourceId,
                fileId: f.fileId,
                parentId: f.parentId || "",
                objectType: root.objectAtomicFile,
                localPath: f.localPath || "",
                remotePath: f.remotePath || "",
                isCloud: f.isCloud === true,
                categoryText: f.categoryText || "",
                annotationsPath:
                    f.annotationsPath || ""
            })
        }
    }

    // ============================================================
    // Tree visibility
    // ============================================================

    function rebuildTreeVisibility() {
        const items = []

        for (let i = 0;
             i < treeModel.count;
             ++i) {

            items.push(
                treeModel.get(i))
        }

        const visibleStack = []

        for (let i = 0;
             i < items.length;
             ++i) {

            const item = items[i]

            if (item.type === "space") {
                item.visible = true

                visibleStack.length = 0

                visibleStack.push({
                    depth: 0,
                    expanded: item.expanded
                })

                treeModel.set(i, item)

                continue
            }

            let visible = true

            for (let d = 0;
                 d < visibleStack.length;
                 ++d) {

                const parent =
                    visibleStack[d]

                if (item.depth > parent.depth &&
                    parent.expanded === false) {

                    visible = false
                    break
                }
            }

            item.visible = visible

            treeModel.set(i, item)

            while (
                visibleStack.length > 0
                && visibleStack[
                    visibleStack.length - 1
                ].depth >= item.depth
            ) {
                visibleStack.pop()
            }

            if (item.type === "folder") {
                visibleStack.push({
                    depth: item.depth,
                    expanded: item.expanded
                })
            }
        }
    }

    function toggleTreeFolder(index) {
        if (index < 0 ||
            index >= treeModel.count) {
            return
        }

        const item =
            treeModel.get(index)

        if (item.type !== "folder") {
            return
        }

        item.expanded =
            !item.expanded

        treeModel.set(index, item)

        rebuildTreeVisibility()
    }

    // ============================================================
    // Select item
    // ============================================================

    function selectItem(item) {
        root.selectedId =
            item.fileId || ""

        root.selectedName =
            item.name || ""

        root.selectedLocalPath =
            item.localPath || ""

        root.selectedRemotePath =
            item.remotePath || ""

        root.selectedSpace =
            Number(item.space)

        root.selectedObjectType =
            Number(item.objectType)

        root.selectedSourceId =
            item.sourceId || ""
    }

    // ============================================================
    // Open folder
    // ============================================================

    function openFolder(item) {
        root.currentSpace =
            Number(item.space)

        root.currentSourceId =
            item.sourceId || ""

        root.currentFolderId =
            item.fileId || ""

        root.currentFolderName =
            item.name || ""

        refreshCurrentFolder()
    }

    function openSpace(item) {
        root.currentSpace =
            Number(item.space)

        root.currentSourceId =
            item.sourceId || ""

        root.currentFolderId = ""
        root.currentFolderName = ""

        folderFilesModel.clear()
    }

    // ============================================================
    // Open file / preview
    // ============================================================

    function openFile(item, fullscreen) {
        selectItem(item)

        if (!item.name) {
            return
        }

        if (!item.name
                .toLowerCase()
                .endsWith(".pdf")) {

            statusLabel.color = "orange"

            statusLabel.text =
                qsTr("当前仅支持 PDF 预览")

            return
        }

        if ((item.localPath || "").length > 0) {
            root.pdfPreviewSource =
                item.localPath

            if (fullscreen) {
                root.openPdfFullscreen(
                    item.localPath)
            }

            statusLabel.color = "green"

            statusLabel.text =
                fullscreen
                ? qsTr("已打开: ")
                    + item.name
                : qsTr("已选择: ")
                    + item.name

            return
        }

        if (hasPdfSupport &&
            (item.remotePath || "").length > 0) {

            root.pendingDownloadId =
                item.fileId

            root.pendingDownloadName =
                item.name

            root.pendingDownloadRemote =
                item.remotePath

            root.pendingOpenFullscreen =
                fullscreen

            const target =
                root.localDocumentsRoot
                + "/LitApp/"
                + item.name

            webDavClient.downloadFile(
                item.remotePath,
                target)

            statusLabel.color = "blue"

            statusLabel.text =
                qsTr("正在下载: ")
                + item.name

            return
        }

        statusLabel.color = "red"

        statusLabel.text =
            qsTr("请先安装 Qt PDF 模块")
    }

    // ============================================================
    // Settings
    // ============================================================

    function openSettings(mode) {
        settingsPopup.panelMode = mode
        settingsPopup.previewMode = false
        settingsPopup.open()
    }

    function previewSettings(mode) {
        if (root.activeSettingsPopup.length > 0) {
            return
        }

        settingsPopup.panelMode = mode
        settingsPopup.previewMode = true
        settingsPopup.open()
    }

    // ============================================================
    // Right-click actions
    // ============================================================

    function openContextMenu(item) {
        selectItem(item)

        treeMenu.itemId =
            item.fileId || ""

        treeMenu.itemName =
            item.name || ""

        treeMenu.itemType =
            Number(item.objectType)

        treeMenu.itemSpace =
            Number(item.space)

        treeMenu.itemSourceId =
            item.sourceId || ""

        treeMenu.itemParentId =
            item.parentId || ""

        treeMenu.open()
    }

    // ============================================================
    // Add file
    // ============================================================

    function addSelectedFile(path) {
        if (!path ||
            path.length === 0) {
            return
        }

        metadataDialog.pendingPath =
            path

        metadataDialog.open()
    }

    // ============================================================
    // Add file to current location
    // ============================================================

    function currentParentIdForImport() {
        /*
         * 当前位于普通 Folder 时，
         * 导入对象的逻辑 parentId 就是该 Folder。
         *
         * 如果当前没有打开文件夹，
         * 则 parentId 为空，表示当前 space 根目录。
         */
        return root.currentFolderId || ""
    }

    // ============================================================
    // Layout
    // ============================================================

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // ========================================================
        // LEFT
        // ========================================================

        Rectangle {
            id: leftPanel

            Layout.preferredWidth: 300
            Layout.minimumWidth: 200
            Layout.fillHeight: true

            radius: 14

            color: root.panelColor

            border.color:
                root.panelBorder

            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("分类:")
                        color: root.textColor
                    }

                    ComboBox {
                        id: categoryFilter

                        model: [
                            qsTr("全部"),
                            qsTr("书籍"),
                            qsTr("期刊论文"),
                            qsTr("原子知识"),
                            qsTr("未分类")
                        ]

                        Layout.fillWidth: true

                        background: Rectangle {
                            color:
                                root.themeIndex === 1
                                ? "#333333"
                                : "#ffffff"

                            border.color:
                                root.panelBorder

                            border.width: 1

                            radius: 6
                        }

                        contentItem: Text {
                            text:
                                categoryFilter.displayText

                            color:
                                root.textColor

                            verticalAlignment:
                                Text.AlignVCenter

                            leftPadding: 10
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Button {
                        text: qsTr("加入文件")

                        Layout.fillWidth: true

                        onClicked: {
                            fileDialog.open()
                        }
                    }

                    Button {
                        text: qsTr("新建文件夹")

                        Layout.fillWidth: true

                        onClicked: {
                            createFolderDialog.open()
                        }
                    }
                }

                ListView {
                    id: localList

                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    clip: true
                    spacing: 1

                    model: treeModel

                    delegate: Rectangle {
                        width: localList.width

                        height:
                            model.visible
                            ? (
                                model.type === "space"
                                ? 38
                                : 34
                            )
                            : 0

                        visible:
                            model.visible

                        radius: 6

                        color:
                            localList.currentIndex === index
                            ? "#d6e6ff"
                            : "transparent"

                        MouseArea {
                            anchors.fill: parent

                            acceptedButtons:
                                Qt.LeftButton
                                | Qt.RightButton

                            onClicked:
                                function(mouse) {

                                localList.currentIndex =
                                    index

                                if (model.type === "space") {

                                    root.openSpace(
                                        model)

                                } else {

                                    root.selectItem(
                                        model)

                                    if (mouse.button ===
                                            Qt.RightButton) {

                                        root.openContextMenu(
                                            model)

                                        return
                                    }

                                    if (model.type ===
                                            "folder") {

                                        root.toggleTreeFolder(
                                            index)

                                        root.openFolder(
                                            model)

                                    } else if (
                                        model.type ===
                                            "file") {

                                        root.openFile(
                                            model,
                                            false)
                                    }
                                }

                                if (mouse.button ===
                                        Qt.RightButton) {

                                    root.openContextMenu(
                                        model)
                                }
                            }

                            onDoubleClicked: {

                                if (model.type ===
                                        "space") {
                                    return
                                }

                                if (model.type ===
                                        "folder") {

                                    root.toggleTreeFolder(
                                        index)

                                    root.openFolder(
                                        model)

                                    return
                                }

                                if (model.type ===
                                        "file") {

                                    root.openFile(
                                        model,
                                        true)
                                }
                            }
                        }

                        RowLayout {
                            anchors.fill: parent

                            anchors.leftMargin:
                                8 + model.depth * 18

                            anchors.rightMargin: 6

                            spacing: 6

                            Text {
                                text:
                                    model.type ===
                                        "folder"
                                    ? (
                                        model.expanded
                                        ? "▾"
                                        : "▸"
                                    )
                                    : ""

                                color:
                                    root.subtleText

                                font.pixelSize: 13

                                Layout.preferredWidth: 14
                            }

                            Text {
                                text:
                                    root.iconForItem(
                                        model)

                                font.pixelSize: 15

                                Layout.preferredWidth: 22
                            }

                            Text {
                                text:
                                    model.name || ""

                                color:
                                    root.textColor

                                font.bold:
                                    model.type ===
                                        "space"
                                    || model.type ===
                                        "folder"

                                elide:
                                    Text.ElideMiddle

                                Layout.fillWidth: true
                            }

                            Text {
                                visible:
                                    model.type ===
                                        "file"
                                    && model.isCloud ===
                                        true

                                text: "☁"

                                color:
                                    root.subtleText
                            }
                        }
                    }
                }
            }
        }

        // ========================================================
        // CENTER
        // ========================================================

        Rectangle {
            Layout.fillWidth: true
            Layout.minimumWidth: 360
            Layout.fillHeight: true

            radius: 14

            color:
                root.contentBg

            border.color:
                root.panelBorder

            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true

                    Layout.preferredHeight:
                        root.currentFolderId.length > 0
                        ? 110
                        : 0

                    visible:
                        root.currentFolderId.length > 0

                    color: "transparent"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 6

                        Label {
                            text:
                                "📁 "
                                + root.spaceTitle(
                                    root.currentSpace)
                                + " / "
                                + root.currentFolderName

                            color:
                                root.textColor

                            font.bold: true

                            Layout.fillWidth: true
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            clip: true

                            model:
                                folderFilesModel

                            delegate: Rectangle {
                                width: parent.width
                                height: 28

                                color: "transparent"

                                Text {
                                    anchors.fill:
                                        parent

                                    anchors.leftMargin:
                                        6

                                    verticalAlignment:
                                        Text.AlignVCenter

                                    text:
                                        (
                                            model.objectType
                                            === root.objectAtomicFolder
                                            ? "🗃️ "
                                            : "📄 "
                                        )
                                        + model.name

                                    color:
                                        root.textColor

                                    elide:
                                        Text.ElideMiddle
                                }

                                MouseArea {
                                    anchors.fill:
                                        parent

                                    onClicked: {
                                        if (
                                            model.objectType
                                            === root.objectAtomicFile
                                        ) {
                                            root.openFile(
                                                model,
                                                false)
                                        }
                                    }

                                    onDoubleClicked: {
                                        if (
                                            model.objectType
                                            === root.objectAtomicFile
                                        ) {
                                            root.openFile(
                                                model,
                                                true)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1

                    visible:
                        root.currentFolderId.length > 0

                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: "transparent"
                        }

                        GradientStop {
                            position: 0.5
                            color: "#ccd5e0"
                        }

                        GradientStop {
                            position: 1.0
                            color: "transparent"
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    color:
                        root.previewBg

                    Loader {
                        id: pdfLoader

                        anchors.fill: parent

                        active:
                            root.pdfPreviewSource.length > 0
                            && hasPdfSupport

                        source:
                            active
                            ? "qrc:/qt/qml/LitApp/qml/PdfPreviewPane.qml"
                            : ""

                        onLoaded: {
                            item.source =
                                root.pdfPreviewSource
                        }
                    }

                    Label {
                        anchors.centerIn:
                            parent

                        visible:
                            !pdfLoader.active

                        text:
                            root.pdfPreviewSource.length === 0
                            ? qsTr(
                                "请选择左侧 PDF 文件进行预览")
                            : qsTr(
                                "PDF 预览暂不可用（未安装 Qt PDF）")

                        color:
                            root.subtleText
                    }
                }
            }
        }

        // ========================================================
        // RIGHT SETTINGS BAR
        // ========================================================

        Rectangle {
            Layout.preferredWidth: 60
            Layout.fillHeight: true

            radius: 14

            color:
                root.panelColor

            border.color:
                root.panelBorder

            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 8

                Item {
                    Layout.fillHeight: true
                }

                Button {
                    id: themeBtn

                    text:
                        root.themeIndex === 0
                        ? "🌙"
                        : "☀️"

                    implicitWidth: 46
                    implicitHeight: 46

                    Layout.alignment:
                        Qt.AlignHCenter

                    flat: true
                    padding: 0

                    onClicked: {
                        root.themeIndex =
                            root.themeIndex === 0
                            ? 1
                            : 0
                    }
                }

                Button {
                    text: "🗑️"

                    implicitWidth: 46
                    implicitHeight: 46

                    Layout.alignment:
                        Qt.AlignHCenter

                    flat: true
                    padding: 0

                    onClicked: {
                        root.openSettings(
                            "recycle")
                    }
                }

                Button {
                    id: syncBtn

                    text: "🔄"

                    implicitWidth: 46
                    implicitHeight: 46

                    Layout.alignment:
                        Qt.AlignHCenter

                    flat: true
                    padding: 0

                    onClicked: {
                        root.openSettings(
                            "sync")
                    }
                }

                Button {
                    id: accountBtn

                    text: "🔑"

                    implicitWidth: 46
                    implicitHeight: 46

                    Layout.alignment:
                        Qt.AlignHCenter

                    flat: true
                    padding: 0

                    onClicked: {
                        root.openSettings(
                            "account")
                    }
                }
            }
        }
    }

    // ============================================================
    // Context menu
    // ============================================================

    Menu {
        id: treeMenu

        property string itemId: ""
        property string itemName: ""
        property string itemParentId: ""
        property string itemSourceId: ""

        property int itemType: -1
        property int itemSpace: -1

        width: 190

        background: Rectangle {
            color:
                root.themeIndex === 1
                ? "#2b2b2b"
                : "#ffffff"

            border.color:
                root.panelBorder

            border.width: 1

            radius: 8
        }

        MenuItem {
            text: qsTr("打开")

            visible:
                treeMenu.itemType !==
                root.objectFolder

            onTriggered: {
                const items =
                    fileListModel.toVariantList()

                for (let i = 0;
                     i < items.length;
                     ++i) {

                    if (items[i].fileId ===
                            treeMenu.itemId) {

                        root.openFile(
                            items[i],
                            false)

                        break
                    }
                }
            }
        }

        MenuItem {
            text: qsTr("展开 / 折叠")

            visible:
                treeMenu.itemType ===
                    root.objectFolder
                || treeMenu.itemType ===
                    root.objectAtomicFolder

            onTriggered: {
                for (let i = 0;
                     i < treeModel.count;
                     ++i) {

                    const item =
                        treeModel.get(i)

                    if (item.fileId ===
                            treeMenu.itemId) {

                        root.toggleTreeFolder(
                            i)

                        break
                    }
                }
            }
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("重命名")

            visible:
                treeMenu.itemType !== -1

            onTriggered: {
                renameDialog.targetId =
                    treeMenu.itemId

                renameDialog.oldName =
                    treeMenu.itemName

                renameDialog.targetType =
                    treeMenu.itemType

                renameDialog.open()
            }
        }

        MenuItem {
            text: qsTr("复制")

            visible:
                treeMenu.itemType !== -1

            onTriggered: {
                if (fileService.copyItem !==
                        undefined) {

                    const ok =
                        fileService.copyItem(
                            treeMenu.itemId)

                    refreshFiles()

                    statusLabel.color =
                        ok ? "green" : "red"

                    statusLabel.text =
                        ok
                        ? qsTr("已复制: ")
                            + treeMenu.itemName
                        : qsTr("复制失败")
                }
            }
        }

        MenuItem {
            text: qsTr("移动")

            visible:
                treeMenu.itemType !== -1

            onTriggered: {
                moveDialog.targetId =
                    treeMenu.itemId

                moveDialog.targetName =
                    treeMenu.itemName

                moveDialog.targetSpace =
                    treeMenu.itemSpace

                moveDialog.open()
            }
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("移到回收站")

            visible:
                treeMenu.itemType !== -1

            onTriggered: {
                const ok =
                    fileService.moveToTrash(
                        treeMenu.itemId)

                refreshFiles()

                statusLabel.color =
                    ok ? "orange" : "red"

                statusLabel.text =
                    ok
                    ? qsTr("已移到回收站: ")
                        + treeMenu.itemName
                    : qsTr("移到回收站失败")
            }
        }
    }

    // ============================================================
    // Add file dialog
    // ============================================================

    FileDialog {
        id: fileDialog

        title:
            qsTr("选择要加入的文件")

        nameFilters: [
            qsTr(
                "文献文件 (*.pdf *.md *.markdown)"),
            qsTr("所有文件 (*)")
        ]

        onAccepted: {
            /*
             * selectedFile 是 QUrl。
             *
             * 这里统一转成 string，
             * 后续 FileService.addLocalFile()
             * 会负责处理 file:// URL。
             */
            metadataDialog.pendingPath =
                selectedFile.toString()

            metadataDialog.open()
        }
    }

    // ============================================================
    // Metadata dialog
    // ============================================================

    Dialog {
        id: metadataDialog

        title:
            qsTr("设置分类与标签")

        modal: true

        width: 380

        x:
            (root.width - width) / 2

        y:
            (root.height - height) / 2

        property string pendingPath: ""

        background: Rectangle {
            radius: 10

            color:
                root.panelColor

            border.color:
                root.panelBorder

            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                text:
                    qsTr("原分类")

                color:
                    root.textColor
            }

            ComboBox {
                id: categoryCombo

                model: [
                    qsTr("书籍"),
                    qsTr("期刊论文"),
                    qsTr("Markdown 原子知识"),
                    qsTr("未分类")
                ]

                Layout.fillWidth: true
            }

            Label {
                text:
                    qsTr(
                        "自定义标签（逗号分隔，可留空）")

                color:
                    root.textColor
            }

            TextField {
                id: tagsField

                placeholderText:
                    qsTr(
                        "例如: 量子计算, 2025, 精读")

                Layout.fillWidth: true
            }
        }

        footer: DialogButtonBox {
            alignment:
                Qt.AlignHCenter

            Button {
                text:
                    qsTr("取消")

                DialogButtonBox.buttonRole:
                    DialogButtonBox.RejectRole
            }

            Button {
                text:
                    qsTr("确定")

                DialogButtonBox.buttonRole:
                    DialogButtonBox.AcceptRole
            }
        }

        onAccepted: {

            if (metadataDialog.pendingPath
                    .length === 0) {

                statusLabel.color =
                    "red"

                statusLabel.text =
                    qsTr("没有选择有效文件")

                return
            }

            /*
             * =====================================================
             * 新架构下的逻辑位置
             * =====================================================
             *
             * space
             * parentId
             * sourceId
             *
             * 三者共同决定文件在应用中的逻辑位置。
             *
             * FileService 会进一步创建：
             *
             *     AtomicFolder
             *          └── AtomicFile
             *
             * =====================================================
             */

            const space =
                root.currentSpace

            const parentId =
                root.currentParentIdForImport()

            const sourceId =
                root.currentSourceId

            const category =
                categoryCombo.currentIndex

            const tagsCsv =
                tagsField.text.trim()

            /*
             * =====================================================
             * 重要：
             *
             * FileService.h 中真实接口为：
             *
             * addLocalFile(
             *     filePath,
             *     space,
             *     parentId,
             *     sourceId,
             *     category,
             *     tagsCsv)
             *
             * 这里必须严格按照这个顺序调用。
             *
             * 原先代码把 category / tags 放到了
             * parentId / sourceId 的位置，这是错误的。
             * =====================================================
             */

            const ok =
                fileService.addLocalFile(
                    metadataDialog.pendingPath,
                    space,
                    parentId,
                    sourceId,
                    category,
                    tagsCsv)

            if (ok) {

                /*
                 * C++ FileService 完成：
                 *
                 * 1. 创建 AtomicFolder
                 * 2. 创建 AtomicFile
                 * 3. 写入 MetadataStore
                 *
                 * 这里必须重新从数据库读取，
                 * 让左侧文件树立即出现新对象。
                 */
                refreshFiles()

                statusLabel.color =
                    "green"

                statusLabel.text =
                    qsTr(
                        "已加入文件，并创建原子文件夹")

                /*
                 * 如果当前是在某个 Folder 内加入，
                 * refreshFiles() 已经会同步刷新
                 * folderFilesModel。
                 */

            } else {

                statusLabel.color =
                    "red"

                statusLabel.text =
                    qsTr(
                        "加入文件失败，请检查文件是否有效")
            }

            /*
             * 清理输入状态，
             * 防止下一次打开对话框残留。
             */
            metadataDialog.pendingPath = ""

            tagsField.text = ""

            categoryCombo.currentIndex = 0
        }
    }

    // ============================================================
    // New folder dialog
    // ============================================================

    Dialog {
        id: createFolderDialog

        title:
            qsTr("新建文件夹")

        modal: true

        width: 340

        x:
            (root.width - width) / 2

        y:
            (root.height - height) / 2

        background: Rectangle {
            radius: 10

            color:
                root.panelColor

            border.color:
                root.panelBorder

            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                text:
                    qsTr("文件夹名称")

                color:
                    root.textColor
            }

            TextField {
                id: newFolderNameField

                Layout.fillWidth: true

                placeholderText:
                    qsTr("例如：机器学习")

                selectByMouse: true
            }
        }

        footer: DialogButtonBox {
            alignment:
                Qt.AlignHCenter

            Button {
                text:
                    qsTr("取消")

                DialogButtonBox.buttonRole:
                    DialogButtonBox.RejectRole
            }

            Button {
                text:
                    qsTr("确定")

                DialogButtonBox.buttonRole:
                    DialogButtonBox.AcceptRole
            }
        }

        onAccepted: {

            const name =
                newFolderNameField.text.trim()

            if (name.length === 0) {
                return
            }

            if (fileService.createFolder !==
                    undefined) {

                const ok =
                    fileService.createFolder(
                        name,
                        root.currentSpace,
                        root.currentFolderId,
                        root.currentSourceId)

                if (ok) {

                    refreshFiles()

                    statusLabel.color =
                        "green"

                    statusLabel.text =
                        qsTr("已创建文件夹: ")
                        + name

                } else {

                    statusLabel.color =
                        "red"

                    statusLabel.text =
                        qsTr("创建文件夹失败")
                }

            } else {

                statusLabel.color =
                    "orange"

                statusLabel.text =
                    qsTr(
                        "当前 FileService 尚未提供新建文件夹接口")
            }

            newFolderNameField.text = ""
        }
    }

    // ============================================================
    // Rename dialog
    // ============================================================

    Dialog {
        id: renameDialog

        title:
            qsTr("重命名")

        modal: true

        width: 340

        x:
            (root.width - width) / 2

        y:
            (root.height - height) / 2

        property string targetId: ""
        property string oldName: ""
        property int targetType: -1

        background: Rectangle {
            radius: 10

            color:
                root.panelColor

            border.color:
                root.panelBorder

            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                text:
                    qsTr("新名称")

                color:
                    root.textColor
            }

            TextField {
                id: renameField

                Layout.fillWidth: true

                text:
                    renameDialog.oldName

                selectByMouse: true
            }
        }

        footer: DialogButtonBox {
            alignment:
                Qt.AlignHCenter

            Button {
                text:
                    qsTr("取消")

                DialogButtonBox.buttonRole:
                    DialogButtonBox.RejectRole
            }

            Button {
                text:
                    qsTr("确定")

                DialogButtonBox.buttonRole:
                    DialogButtonBox.AcceptRole
            }
        }

        onOpened: {

            renameField.text =
                renameDialog.oldName

            renameField.selectAll()
            renameField.forceActiveFocus()
        }

        onAccepted: {

            const name =
                renameField.text.trim()

            if (name.length === 0) {
                return
            }

            let ok = false

            if (fileService.renameItem !==
                    undefined) {

                ok =
                    fileService.renameItem(
                        renameDialog.targetId,
                        name)
            }

            refreshFiles()

            statusLabel.color =
                ok ? "green" : "red"

            statusLabel.text =
                ok
                ? qsTr("已重命名: ")
                    + name
                : qsTr("重命名失败")
        }
    }

    // ============================================================
    // Move dialog
    // ============================================================

    Dialog {
        id: moveDialog

        title:
            qsTr("移动")

        modal: true

        width: 360

        x:
            (root.width - width) / 2

        y:
            (root.height - height) / 2

        property string targetId: ""
        property string targetName: ""
        property int targetSpace: -1

        background: Rectangle {
            radius: 10

            color:
                root.panelColor

            border.color:
                root.panelBorder

            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 10

            Label {
                text:
                    qsTr("移动对象：")
                    + moveDialog.targetName

                color:
                    root.textColor

                wrapMode:
                    Text.Wrap

                Layout.fillWidth: true
            }

            Label {
                text:
                    qsTr("目标空间")

                color:
                    root.textColor
            }

            ComboBox {
                id: moveSpaceCombo

                model: [
                    qsTr("my doc"),
                    qsTr("group me"),
                    qsTr("group us")
                ]

                currentIndex:
                    moveDialog.targetSpace >= 0
                    ? moveDialog.targetSpace
                    : 0

                Layout.fillWidth: true
            }

            Label {
                text:
                    qsTr(
                        "说明：移动操作不会改变原子文件名称。")

                color:
                    root.subtleText

                wrapMode:
                    Text.Wrap

                Layout.fillWidth: true
            }
        }

        footer: DialogButtonBox {
            alignment:
                Qt.AlignHCenter

            Button {
                text:
                    qsTr("取消")

                DialogButtonBox.buttonRole:
                    DialogButtonBox.RejectRole
            }

            Button {
                text:
                    qsTr("确定")

                DialogButtonBox.buttonRole:
                    DialogButtonBox.AcceptRole
            }
        }

        onAccepted: {

            const destSpace =
                moveSpaceCombo.currentIndex

            let ok = false

            if (fileService.moveItemTo !==
                    undefined) {

                ok =
                    fileService.moveItemTo(
                        moveDialog.targetId,
                        destSpace,
                        "",
                        "")

            } else if (
                fileService.moveItem !==
                undefined
            ) {

                ok =
                    fileService.moveItem(
                        moveDialog.targetId,
                        destSpace)
            }

            refreshFiles()

            statusLabel.color =
                ok ? "green" : "red"

            statusLabel.text =
                ok
                ? qsTr("已移动: ")
                    + moveDialog.targetName
                : qsTr("移动失败")
        }
    }

    // ============================================================
    // Settings popup
    // ============================================================

    Popup {
        id: settingsPopup

        x:
            root.width - width - 80

        y:
            root.height - height - 30

        width: 290

        padding: 0

        background: null

        property string panelMode:
            "account"

        property bool previewMode:
            false

        onOpened: {

            if (!previewMode) {

                root.activeSettingsPopup =
                    panelMode
            }
        }

        onClosed: {

            root.activeSettingsPopup =
                ""
        }

        contentItem: Rectangle {
            width: 290

            height:
                settingsContentColumn.implicitHeight
                + 32

            radius: 10

            color:
                root.panelColor

            border.color:
                root.panelBorder

            border.width: 1

            ColumnLayout {
                id: settingsContentColumn

                anchors.top:
                    parent.top

                anchors.left:
                    parent.left

                anchors.right:
                    parent.right

                anchors.margins: 16

                spacing: 12

                Label {
                    text:
                        settingsPopup.panelMode ===
                            "account"
                        ? qsTr("账号")
                        : settingsPopup.panelMode ===
                            "sync"
                          ? qsTr("数据同步")
                          : qsTr("回收站")

                    font.bold: true
                    font.pixelSize: 17

                    horizontalAlignment:
                        Text.AlignHCenter

                    color:
                        root.textColor

                    Layout.fillWidth: true
                }

                StackLayout {
                    id: settingsStack

                    Layout.fillWidth: true

                    currentIndex:
                        settingsPopup.panelMode ===
                            "account"
                        ? 0
                        : settingsPopup.panelMode ===
                            "sync"
                          ? 1
                          : 2

                    // =============================================
                    // Account
                    // =============================================

                    ColumnLayout {
                        spacing: 8

                        TextField {
                            id: accountEmailField

                            placeholderText:
                                qsTr("邮箱")

                            Layout.fillWidth: true
                        }

                        TextField {
                            id: accountPasswordField

                            placeholderText:
                                qsTr("密码")

                            echoMode:
                                TextInput.Password

                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Button {
                                text:
                                    qsTr("登录")

                                Layout.fillWidth: true

                                onClicked: {

                                    authService.login(
                                        accountEmailField.text.trim(),
                                        accountPasswordField.text)
                                }
                            }

                            Button {
                                text:
                                    qsTr("注册")

                                Layout.fillWidth: true

                                onClicked: {

                                    authService.registerAccount(
                                        accountEmailField.text.trim(),
                                        accountPasswordField.text)
                                }
                            }
                        }

                        Label {
                            text:
                                authService.email().length > 0
                                ? authService.email()
                                : qsTr("未登录")

                            color:
                                root.subtleText

                            elide:
                                Text.ElideMiddle

                            Layout.fillWidth: true
                        }

                        Button {
                            text:
                                qsTr("退出登录")

                            visible:
                                authService.isLoggedIn()

                            Layout.fillWidth: true

                            onClicked: {

                                authService.logout()

                                syncService.stopAutoSync()

                                root.logoutRequested()
                            }
                        }
                    }

                    // =============================================
                    // Sync
                    // =============================================

                    ColumnLayout {
                        spacing: 8

                        TextField {
                            id: serverField

                            placeholderText:
                                "https://dav.example.com/"

                            Layout.fillWidth: true
                        }

                        TextField {
                            id: usernameField

                            placeholderText:
                                qsTr("用户名")

                            Layout.fillWidth: true
                        }

                        TextField {
                            id: passwordField

                            placeholderText:
                                qsTr("密码")

                            echoMode:
                                TextInput.Password

                            Layout.fillWidth: true
                        }

                        Button {
                            text:
                                qsTr("保存并连接")

                            Layout.fillWidth: true

                            onClicked: {

                                webDavClient.setServer(
                                    serverField.text.trim(),
                                    usernameField.text.trim(),
                                    passwordField.text)

                                statusLabel.color =
                                    "green"

                                statusLabel.text =
                                    qsTr("正在连接...")

                                webDavClient.listDirectory(
                                    "/zesbks/")
                            }
                        }

                        Label {
                            id: statusLabel

                            color:
                                root.subtleText

                            wrapMode:
                                Text.Wrap

                            Layout.fillWidth: true
                        }

                        Label {
                            text:
                                qsTr(
                                    "同步根目录：/zesbks/")

                            color:
                                root.subtleText

                            font.pixelSize: 12

                            Layout.fillWidth: true

                            wrapMode:
                                Text.Wrap
                        }
                    }

                    // =============================================
                    // Recycle
                    // =============================================

                    ColumnLayout {
                        spacing: 8

                        Label {
                            text:
                                qsTr("回收站")

                            color:
                                root.textColor

                            font.bold: true
                        }

                        Label {
                            text:
                                qsTr(
                                    "删除对象会先进入本地回收站。")

                            color:
                                root.subtleText

                            wrapMode:
                                Text.Wrap

                            Layout.fillWidth: true
                        }

                        Button {
                            text:
                                qsTr("刷新")

                            Layout.fillWidth: true

                            onClicked: {

                                statusLabel.color =
                                    root.subtleText

                                statusLabel.text =
                                    qsTr(
                                        "回收站功能将在下一阶段完善")
                            }
                        }
                    }
                }
            }
        }
    }

    // ============================================================
    // Sync signals
    // ============================================================

    Connections {
        target:
            syncService

        function onSyncFinished(
                ok,
                message) {

            statusLabel.color =
                ok ? "green" : "red"

            statusLabel.text =
                message

            refreshFiles()
        }
    }

    // ============================================================
    // WebDAV signals
    // ============================================================

    Connections {
        target:
            webDavClient

        function onDirectoryListed(
                path,
                entries,
                error) {

            if (error.length > 0) {

                statusLabel.color =
                    "red"

                statusLabel.text =
                    error

                return
            }

            statusLabel.color =
                "green"

            statusLabel.text =
                qsTr("已连接：")
                + path
                + qsTr("，")
                + entries.length
                + qsTr(" 项")
        }

        function onUploadFinished(
                remotePath,
                ok,
                error) {

            statusLabel.color =
                ok ? "green" : "red"

            statusLabel.text =
                ok
                ? qsTr("上传成功: ")
                    + remotePath
                : error
        }

        function onDownloadFinished(
                remotePath,
                localPath,
                ok,
                error) {

            if (root.pendingDownloadRemote !==
                    remotePath) {
                return
            }

            if (ok) {

                fileService.markDownloaded(
                    root.pendingDownloadId,
                    localPath)

                refreshFiles()

                root.pdfPreviewSource =
                    localPath

                if (root.pendingOpenFullscreen) {

                    root.openPdfFullscreen(
                        localPath)
                }

                statusLabel.color =
                    "green"

                statusLabel.text =
                    qsTr(
                        "下载完成并打开: ")
                    + root.pendingDownloadName

            } else {

                statusLabel.color =
                    "red"

                statusLabel.text =
                    error
            }

            root.pendingDownloadId = ""
            root.pendingDownloadRemote = ""
            root.pendingDownloadName = ""
            root.pendingOpenFullscreen = false
        }
    }
}