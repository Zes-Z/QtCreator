import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore

Page {
    id: root
    background: Rectangle {
        color: root.contentBg
    }
    signal logoutRequested()
    signal openPdfFullscreen(string path)

    property string selectedLocalName: ""
    property string selectedLocalPath: ""
    property string selectedLocalId: ""
    property string selectedRemotePath: ""

    property string pdfPreviewSource: ""
    property string pendingDownloadId: ""
    property string pendingDownloadRemote: ""
    property string pendingDownloadName: ""
    property bool pendingOpenFullscreen: false
    property string activeSettingsPopup: ""
    property int pendingMkcolCount: 0

    property string localDir: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)

    property int themeIndex: 0
    property bool myDocSortAscending: true
    property bool groupDocSortAscending: true
    property color panelColor: themeIndex === 1 ? "#2b2b2b" : "#f4f6f8"
    property color panelBorder: themeIndex === 1 ? "#444444" : "#d8dee6"
    property color contentBg: themeIndex === 1 ? "#1e1e1e" : "#ffffff"
    property color textColor: themeIndex === 1 ? "#eeeeee" : "#222222"
    property color subtleText: themeIndex === 1 ? "#aaaaaa" : "#666666"
    property color previewBg: themeIndex === 1 ? "#252525" : "#fafafa"

    onPdfPreviewSourceChanged: {
        if (pdfLoader.item) {
            pdfLoader.item.source = pdfPreviewSource
        }
    }

    Component.onCompleted: {
        webDavClient.restoreSettings()
        serverField.text = webDavClient.serverUrl()
        usernameField.text = webDavClient.username()
        passwordField.text = webDavClient.password()

        refreshFiles()
        syncService.setActiveZone(0)
        syncService.startAutoSync(180000)
    }

    ListModel {
        id: treeModel
    }

    ListModel {
        id: folderFilesModel
    }

    property string currentFolder: ""
    property int currentFolderZone: -1

    function refreshFiles() {
        fileListModel.reloadAllFromStore("litapp_metadata.db")
        rebuildTree()
    }

    function rebuildTree() {
        treeModel.clear()
        const files = fileListModel.toVariantList()

        appendZoneGroup(qsTr("My Doc"), 0, files)
        treeModel.append({ type: "separator", depth: 0, visible: true })
        appendZoneGroup(qsTr("群组 Doc"), 1, files)
    }

    function appendZoneGroup(title, zone, files) {
        treeModel.append({
            type: "group",
            name: title,
            depth: 0,
            zone: zone,
            expanded: true,
            visible: true
        })

        const folders = {}
        for (let i = 0; i < files.length; ++i) {
            const f = files[i]
            if (f.zone !== zone) {
                continue
            }
            const key = f.folderName.length > 0 ? f.folderName : f.fileName
            if (!folders[key]) {
                folders[key] = []
            }
            folders[key].push(f)
        }

        const ascending = zone === 0 ? root.myDocSortAscending : root.groupDocSortAscending;
        const keys = Object.keys(folders).sort()
        if (!ascending) {
            keys.reverse()
        }

        for (const key of keys) {
            treeModel.append({
                type: "folder",
                name: key,
                depth: 1,
                expanded: false,
                visible: true,
                folder: key,
                zone: zone
            })
            const children = folders[key].slice()
            children.sort(function(a, b) {
                const aMain = a.fileName.replace(/\.[^/.]+$/, "").toLowerCase() === key.toLowerCase()
                const bMain = b.fileName.replace(/\.[^/.]+$/, "").toLowerCase() === key.toLowerCase()
                if (aMain !== bMain) {
                    return aMain ? -1 : 1
                }
                return a.fileName.localeCompare(b.fileName)
            })
            for (let j = 0; j < children.length; ++j) {
                const f = children[j]
                treeModel.append({
                    type: "file",
                    name: f.fileName,
                    depth: 2,
                    visible: false,
                    folder: key,
                    zone: zone,
                    fileId: f.fileId,
                    localPath: f.localPath,
                    remotePath: f.remotePath,
                    isCloud: f.isCloud,
                    categoryText: f.categoryText,
                    annotationsPath: f.annotationsPath
                })
            }
        }
    }

    function toggleFolder(index) {
        const item = treeModel.get(index)
        if (item.type !== "folder") {
            return
        }
        item.expanded = !item.expanded
        for (let i = index + 1; i < treeModel.count; ++i) {
            const child = treeModel.get(i)
            if (child.type !== "file") {
                break
            }
            child.visible = item.expanded
        }
        treeModel.set(index, item)
    }

    function showFolder(zone, folderName) {
        currentFolder = folderName
        currentFolderZone = zone
        folderFilesModel.clear()
        const files = fileListModel.toVariantList()
        for (let i = 0; i < files.length; ++i) {
            const f = files[i]
            if (f.zone === zone && f.folderName === folderName) {
                folderFilesModel.append({
                    name: f.fileName,
                    localPath: f.localPath,
                    remotePath: f.remotePath,
                    fileId: f.fileId,
                    isCloud: f.isCloud,
                    categoryText: f.categoryText
                })
            }
        }
    }

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

    function ensureZesbksAndList() {
        pendingMkcolCount = 3
        webDavClient.createDirectory("/zesbks")
        webDavClient.createDirectory("/zesbks/my doc")
        webDavClient.createDirectory("/zesbks/group doc")
    }

    function toggleExpandGroup(zone) {
        let anyCollapsed = false
        for (let i = 0; i < treeModel.count; ++i) {
            const item = treeModel.get(i)
            if (item.type === "folder" && item.zone === zone && !item.expanded) {
                anyCollapsed = true
                break
            }
        }
        const expand = anyCollapsed
        for (let i = 0; i < treeModel.count; ++i) {
            const item = treeModel.get(i)
            if (item.type === "folder" && item.zone === zone) {
                item.expanded = expand
                for (let j = i + 1; j < treeModel.count; ++j) {
                    const child = treeModel.get(j)
                    if (child.type !== "file") {
                        break
                    }
                    child.visible = expand
                }
                treeModel.set(i, item)
            }
        }
    }

    function toggleSortOrder(zone) {
        if (zone === 0) {
            root.myDocSortAscending = !root.myDocSortAscending
        } else {
            root.groupDocSortAscending = !root.groupDocSortAscending
        }
        rebuildTree()
    }




    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

    SplitView {
        id: mainSplit
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 12
        orientation: Qt.Horizontal

        handle: Rectangle {
            implicitWidth: mainSplit.orientation === Qt.Horizontal ? 4 : mainSplit.width
            implicitHeight: mainSplit.orientation === Qt.Horizontal ? mainSplit.height : 4
            color: "transparent"
        }

        // ============ 左：文件目录区 ============
        Rectangle {
            SplitView.preferredWidth: 280
            SplitView.minimumWidth: 160
            SplitView.fillHeight: true
            radius: 14
            color: root.panelColor
            border.color: root.panelBorder
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Label { text: qsTr("分类:"); color: root.textColor }
                    ComboBox {
                        id: categoryFilter
                        model: [qsTr("全部"), qsTr("书籍"), qsTr("期刊论文"), qsTr("原子知识"), qsTr("未分类")]
                        Layout.fillWidth: true
                        background: Rectangle {
                            color: "#ffffff"
                            border.color: "#cccccc"
                            border.width: 1
                            radius: 6
                        }
                    }
                }

                Button {
                    text: qsTr("添加本地文件")
                    Layout.fillWidth: true
                    background: Rectangle {
                        radius: 8
                        color: "#e8e8e8"
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#222222"
                        font.weight: Font.Normal
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: fileDialog.open()
                }

                ListView {
                    id: localList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 0
                    model: treeModel

                    delegate: Rectangle {
                        width: parent ? parent.width : 0
                        height: !model.visible ? 0
                             : (model.type === "separator" ? 16
                             : (model.type === "group" ? 32
                             : (model.type === "folder" ? 32 : 40)))
                        radius: 6
                        color: (model.type !== "separator" && localList.currentIndex === index) ? "#d6e6ff" : "transparent"
                        visible: model.visible

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            height: 1
                            visible: model.type === "separator"
                            gradient: Gradient {
                                GradientStop { position: 0.0; color: "transparent" }
                                GradientStop { position: 0.5; color: "#ccd5e0" }
                                GradientStop { position: 1.0; color: "transparent" }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            enabled: model.type !== "separator"
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: (mouse) => {
                                if (mouse.button === Qt.RightButton) {
                                    treeMenu.itemZone = model.zone
                                    treeMenu.itemName = model.name
                                    treeMenu.itemType = model.type
                                    treeMenu.itemId = model.fileId
                                    treeMenu.popup()
                                    return
                                }
                                localList.currentIndex = index
                                if (model.type === "group") {
                                    root.currentFolder = ""
                                    folderFilesModel.clear()
                                } else if (model.type === "folder") {
                                    root.toggleFolder(index)
                                    root.showFolder(model.zone, model.folder)
                                } else if (model.type === "file") {
                                    root.selectedLocalName = model.name
                                    root.selectedLocalPath = model.localPath
                                    root.selectedLocalId = model.fileId
                                    root.selectedRemotePath = model.remotePath
                                    if (model.name.toLowerCase().endsWith(".pdf")) {
                                        if (model.localPath.length > 0) {
                                            root.pdfPreviewSource = model.localPath
                                        } else if (hasPdfSupport && model.remotePath.length > 0) {
                                            root.pendingDownloadId = model.fileId
                                            root.pendingDownloadName = model.name
                                            root.pendingDownloadRemote = model.remotePath
                                            root.pendingOpenFullscreen = false
                                            const target = root.localDir + "/" + model.name
                                            webDavClient.downloadFile(model.remotePath, target)
                                            statusLabel.color = "blue"
                                            statusLabel.text = qsTr("正在下载: ") + model.name
                                        }
                                    }
                                }
                            }
                            onDoubleClicked: {
                                if (model.type !== "file") {
                                    return
                                }
                                localList.currentIndex = index
                                root.selectedLocalName = model.name
                                root.selectedLocalPath = model.localPath
                                root.selectedLocalId = model.fileId
                                root.selectedRemotePath = model.remotePath

                                if (!model.name.toLowerCase().endsWith(".pdf")) {
                                    statusLabel.color = "orange"
                                    statusLabel.text = qsTr("当前仅支持 PDF 预览")
                                    return
                                }

                                if (model.localPath.length > 0) {
                                    root.pdfPreviewSource = model.localPath
                                    root.openPdfFullscreen(model.localPath)
                                    statusLabel.color = "green"
                                    statusLabel.text = qsTr("已打开: ") + model.name
                                } else if (hasPdfSupport) {
                                    // 远程元信息文件：按需下载完整文件
                                    root.pendingDownloadId = model.fileId
                                    root.pendingDownloadName = model.name
                                    root.pendingDownloadRemote = model.remotePath
                                    root.pendingOpenFullscreen = true
                                    const target = root.localDir + "/" + model.name
                                    webDavClient.downloadFile(model.remotePath, target)
                                    statusLabel.color = "blue"
                                    statusLabel.text = qsTr("正在下载: ") + model.name
                                } else {
                                    statusLabel.color = "red"
                                    statusLabel.text = qsTr("请先安装 Qt PDF 模块")
                                }
                            }
                        }

                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 8 + model.depth * 16
                            anchors.rightMargin: 8
                            spacing: 4
                            visible: model.type !== "separator"

                            Text {
                                text: model.type === "group"
                                      ? (model.zone === 0 ? "📚 " : "🗂️ ")
                                      : (model.type === "folder"
                                         ? (model.expanded ? "📂 " : "📁 ")
                                         : "📄 ")
                                font.pixelSize: 14
                                color: root.textColor
                            }
                            Text {
                                text: model.name + (model.type === "file" && model.isCloud ? "  ☁️" : "")
                                elide: Text.ElideMiddle
                                font.bold: model.type === "group" || model.type === "folder" || model.isCloud === true
                                color: root.textColor
                                Layout.fillWidth: true
                            }
                            ToolButton {
                                visible: model.type === "group"
                                text: "◀"
                                implicitWidth: 22
                                implicitHeight: 22
                                padding: 0
                                onClicked: {
                                    groupMenu.zone = model.zone
                                    groupMenu.popup()
                                }
                            }
                        }
                    }
                }
            }
        }

        // ============ 中：PDF 预览/阅读区 ============
        Rectangle {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 300
            SplitView.fillHeight: true
            radius: 14
            color: root.contentBg
            border.color: root.panelBorder
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.currentFolder.length > 0 ? 120 : 0
                    visible: root.currentFolder.length > 0
                    color: "transparent"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 4

                        Label {
                            text: "📁 " + (root.currentFolderZone === 0 ? qsTr("My Doc") : qsTr("群组 Doc")) + " / " + root.currentFolder
                            font.bold: true
                            color: root.textColor
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: folderFilesModel
                            delegate: Rectangle {
                                width: parent ? parent.width : 0
                                height: 26
                                radius: 4
                                color: "transparent"

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        if (model.localPath.length > 0) {
                                            root.pdfPreviewSource = model.localPath
                                        }
                                    }
                                    onDoubleClicked: {
                                        if (model.localPath.length > 0) {
                                            root.openPdfFullscreen(model.localPath)
                                        }
                                    }
                                }

                                Text {
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideMiddle
                                    color: root.textColor
                                    text: (model.isCloud ? "☁️ " : "📄 ") + model.name
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    visible: root.currentFolder.length > 0
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 0.5; color: "#ccd5e0" }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "transparent"

                    Loader {
                        id: pdfLoader
                        anchors.fill: parent
                        active: root.pdfPreviewSource.length > 0 && hasPdfSupport
                        source: active ? "qrc:/qt/qml/LitApp/qml/PdfPreviewPane.qml" : ""
                        onLoaded: item.source = root.pdfPreviewSource
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: !pdfLoader.active
                        text: root.pdfPreviewSource.length === 0
                              ? qsTr("请选择左侧 PDF 文件进行预览")
                              : qsTr("PDF 预览暂不可用（未安装 Qt PDF）")
                        color: "#999999"
                    }
                }
            }
        }

    }

        // ============ 右：固定设置图标栏 ============
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

                Item {
                    Layout.fillHeight: true
                }

                Button {
                    id: themeBtn
                    text: root.themeIndex === 0 ? "🌙" : "☀️"
                    implicitWidth: 46
                    implicitHeight: 46
                    Layout.alignment: Qt.AlignHCenter
                    hoverEnabled: true
                    flat: true
                    focusPolicy: Qt.NoFocus
                    padding: 0
                    background: Rectangle {
                        radius: 10
                        color: themeBtn.hovered ? (root.themeIndex === 1 ? "#3a3a3a" : "#e8e8e8") : "transparent"
                        border.color: "transparent"
                        border.width: 0
                    }
                    onClicked: root.themeIndex = root.themeIndex === 0 ? 1 : 0
                }

                Button {
                    id: trashBtn
                    text: "🗑️"
                    implicitWidth: 46
                    implicitHeight: 46
                    Layout.alignment: Qt.AlignHCenter
                    hoverEnabled: true
                    flat: true
                    focusPolicy: Qt.NoFocus
                    padding: 0
                    background: Rectangle {
                        radius: 10
                        color: trashBtn.hovered ? "#e8e8e8" : "transparent"
                        border.color: "transparent"
                        border.width: 0
                    }
                    onClicked: root.openSettings("recycle")
                }


                Button {
                    id: syncBtn
                    text: "🔄"
                    implicitWidth: 46
                    implicitHeight: 46
                    Layout.alignment: Qt.AlignHCenter
                    hoverEnabled: true
                    flat: true
                    focusPolicy: Qt.NoFocus
                    padding: 0
                    background: Rectangle {
                        radius: 10
                        color: syncBtn.hovered ? "#e8e8e8" : "transparent"
                        border.color: "transparent"
                        border.width: 0
                    }
                    onHoveredChanged: {
                        if (hovered && root.activeSettingsPopup.length === 0) {
                            root.previewSettings("sync")
                        } else if (!hovered && settingsPopup.previewMode) {
                            settingsPopup.close()
                        }
                    }
                    onClicked: root.openSettings("sync")
                }

                Button {
                    id: accountBtn
                    text: "🔑"
                    implicitWidth: 46
                    implicitHeight: 46
                    Layout.alignment: Qt.AlignHCenter
                    hoverEnabled: true
                    flat: true
                    focusPolicy: Qt.NoFocus
                    padding: 0
                    background: Rectangle {
                        radius: 10
                        color: accountBtn.hovered ? "#e8e8e8" : "transparent"
                        border.color: "transparent"
                        border.width: 0
                    }
                    onHoveredChanged: {
                        if (hovered && root.activeSettingsPopup.length === 0) {
                            root.previewSettings("account")
                        } else if (!hovered && settingsPopup.previewMode) {
                            settingsPopup.close()
                        }
                    }
                    onClicked: root.openSettings("account")
                }
            }
        }
    }

    Popup {
        id: settingsPopup
        x: root.width - width - 80
        y: root.height - height - 170
        width: 280
        padding: 0
        background: null
        property string panelMode: "account"
        property bool previewMode: false

        onOpened: {
            if (!previewMode) {
                root.activeSettingsPopup = panelMode
            }
        }
        onClosed: root.activeSettingsPopup = ""

        contentItem: Rectangle {
            radius: 10
            color: root.panelColor
            border.color: root.panelBorder
            border.width: 1
            width: 280
            height: settingsContentColumn.implicitHeight + 32

            ColumnLayout {
                id: settingsContentColumn
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 16
                spacing: 12

                Label {
                    text: settingsPopup.panelMode === "account" ? qsTr("账号")
                        : settingsPopup.panelMode === "sync" ? qsTr("数据同步")
                        : qsTr("回收站")
                    font.bold: true
                    font.pixelSize: 17
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                    color: root.textColor
                }

                StackLayout {
                    id: settingsStack
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: settingsPopup.panelMode === "account" ? 0
                                : settingsPopup.panelMode === "sync" ? 1
                                : 2

                    // ---------- 账号 ----------
                    ColumnLayout {
                        id: accountPanel
                        spacing: 8

                        TextField {
                            id: accountEmailField
                            placeholderText: qsTr("邮箱")
                            Layout.fillWidth: true
                            background: Rectangle { color: "#ffffff"; border.color: "#cccccc"; border.width: 1; radius: 6 }
                        }
                        TextField {
                            id: accountPasswordField
                            placeholderText: qsTr("密码")
                            echoMode: TextInput.Password
                            Layout.fillWidth: true
                            background: Rectangle { color: "#ffffff"; border.color: "#cccccc"; border.width: 1; radius: 6 }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Button {
                                text: qsTr("登录")
                                Layout.fillWidth: true
                                background: Rectangle { radius: 8; color: "#e8e8e8" }
                                contentItem: Text { text: parent.text; color: "#222222"; font.weight: Font.Normal; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                onClicked: authService.login(accountEmailField.text.trim(), accountPasswordField.text)
                            }
                            Button {
                                text: qsTr("注册")
                                Layout.fillWidth: true
                                background: Rectangle { radius: 8; color: "#e8e8e8" }
                                contentItem: Text { text: parent.text; color: "#222222"; font.weight: Font.Normal; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                onClicked: authService.registerAccount(accountEmailField.text.trim(), accountPasswordField.text)
                            }
                        }
                        Label {
                            text: authService.email().length > 0 ? authService.email() : qsTr("未登录")
                            color: root.subtleText
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                        Button {
                            text: qsTr("退出登录")
                            Layout.fillWidth: true
                            visible: authService.isLoggedIn()
                            background: Rectangle { radius: 8; color: "#e8e8e8" }
                            contentItem: Text { text: parent.text; color: "#222222"; font.weight: Font.Normal; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            onClicked: {
                                authService.logout()
                                syncService.stopAutoSync()
                                root.logoutRequested()
                            }
                        }
                    }

                    // ---------- 数据同步 ----------
                    ColumnLayout {
                        id: syncPanel
                        spacing: 8

                        TextField {
                            id: serverField
                            placeholderText: "https://dav.example.com/"
                            Layout.fillWidth: true
                            background: Rectangle { color: "#ffffff"; border.color: "#cccccc"; border.width: 1; radius: 6 }
                        }
                        TextField {
                            id: usernameField
                            placeholderText: qsTr("用户名")
                            Layout.fillWidth: true
                            background: Rectangle { color: "#ffffff"; border.color: "#cccccc"; border.width: 1; radius: 6 }
                        }
                        TextField {
                            id: passwordField
                            placeholderText: qsTr("密码")
                            echoMode: TextInput.Password
                            Layout.fillWidth: true
                            background: Rectangle { color: "#ffffff"; border.color: "#cccccc"; border.width: 1; radius: 6 }
                        }

                        Button {
                            text: qsTr("保存并连接")
                            Layout.fillWidth: true
                            background: Rectangle { radius: 8; color: "#e8e8e8" }
                            contentItem: Text {
                                text: parent.text
                                color: "#222222"
                                font.weight: Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                webDavClient.setServer(serverField.text.trim(),
                                                       usernameField.text.trim(),
                                                       passwordField.text)
                                statusLabel.color = "green"
                                statusLabel.text = qsTr("正在连接...")
                                root.ensureZesbksAndList()
                            }
                        }

                        Label {
                            id: statusLabel
                            color: "green"
                            wrapMode: Text.Wrap
                            visible: text.length > 0
                            Layout.fillWidth: true
                        }

                        Label {
                            text: qsTr("每 3 分钟自动同步元信息")
                            color: root.subtleText
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                    }

                    // ---------- 回收站 ----------
                    ColumnLayout {
                        id: recyclePanel
                        spacing: 8

                        Label {
                            text: qsTr("回收站为空")
                            color: root.subtleText
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                    }

                }
            }
        }
    }

    Menu {
        id: groupMenu
        property int zone: 0
        background: Rectangle {
            color: "#ffffff"
            border.color: "#cccccc"
            border.width: 1
            radius: 8
        }
        MenuItem {
            text: qsTr("展开/聚合全部")
            background: Rectangle { color: parent.hovered ? "#e8e8e8" : "#ffffff" }
            onClicked: root.toggleExpandGroup(groupMenu.zone)
        }
        MenuItem {
            text: qsTr("正序/倒序排序")
            background: Rectangle { color: parent.hovered ? "#e8e8e8" : "#ffffff" }
            onClicked: root.toggleSortOrder(groupMenu.zone)
        }
    }

    Menu {
        id: treeMenu
        property int itemZone: 0
        property string itemName: ""
        property string itemType: ""
        property string itemId: ""
        background: Rectangle {
            color: "#ffffff"
            border.color: "#cccccc"
            border.width: 1
            radius: 8
        }

        MenuItem {
            text: qsTr("移到回收站")
            background: Rectangle { color: parent.hovered ? "#e8e8e8" : "#ffffff" }
            onClicked: {
                fileService.moveToTrash(treeMenu.itemId)
                refreshFiles()
                statusLabel.color = "orange"
                statusLabel.text = qsTr("已移到回收站: ") + treeMenu.itemName
            }
        }
        MenuItem {
            text: qsTr("复制 (Ctrl)")
            background: Rectangle { color: parent.hovered ? "#e8e8e8" : "#ffffff" }
            onClicked: {
                fileService.copyItem(treeMenu.itemId)
                refreshFiles()
                statusLabel.text = qsTr("已复制: ") + treeMenu.itemName
            }
        }
        MenuItem {
            text: qsTr("移动 (Shift)")
            background: Rectangle { color: parent.hovered ? "#e8e8e8" : "#ffffff" }
            onClicked: {
                const dest = treeMenu.itemZone === 0 ? 1 : 0
                fileService.moveItem(treeMenu.itemId, dest)
                refreshFiles()
                statusLabel.text = qsTr("已移动: ") + treeMenu.itemName
            }
        }
        MenuItem {
            text: qsTr("重命名")
            background: Rectangle { color: parent.hovered ? "#e8e8e8" : "#ffffff" }
            onClicked: {
                renameDialog.targetId = treeMenu.itemId
                renameDialog.oldName = treeMenu.itemName
                renameDialog.open()
            }
        }
    }




    FileDialog {
        id: fileDialog
        title: qsTr("选择要添加的文献文件")
        nameFilters: ["文献文件 (*.pdf *.md *.markdown)", "所有文件 (*)"]
        onAccepted: {
            metadataDialog.pendingPath = selectedFile.toString()
            metadataDialog.open()
        }
    }

    Dialog {
        id: metadataDialog
        title: qsTr("设置分类与标签")
        modal: true
        width: 380
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        background: Rectangle {
            radius: 10
            color: root.panelColor
            border.color: root.panelBorder
            border.width: 1
        }
        property string pendingPath: ""

        contentItem: ColumnLayout {
            spacing: 12

            Label { text: qsTr("原分类（必选，单选）") }

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

            Label { text: qsTr("自定义标签（逗号分隔，可留空）") }

            TextField {
                id: tagsField
                placeholderText: qsTr("例如: 量子计算, 2025, 精读")
                Layout.fillWidth: true
            }
        }

        footer: DialogButtonBox {
            alignment: Qt.AlignHCenter

            Button {
                text: qsTr("跳过")
                DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole
                onClicked: {
                    categoryCombo.currentIndex = 3
                    tagsField.text = ""
                    metadataDialog.accept()
                }
            }
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
            const ok = fileService.addLocalFile(
                metadataDialog.pendingPath,
                0,
                categoryCombo.currentIndex,
                tagsField.text.trim())
            if (ok) {
                refreshFiles()
                statusLabel.color = "green"
                statusLabel.text = qsTr("已添加本地文件")
            } else {
                statusLabel.color = "red"
                statusLabel.text = qsTr("添加失败，请检查文件是否有效")
            }
        }
    }

    Dialog {
        id: renameDialog
        title: qsTr("重命名")
        modal: true
        width: 320
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        background: Rectangle {
            radius: 10
            color: root.panelColor
            border.color: root.panelBorder
            border.width: 1
        }
        property string targetId: ""
        property string oldName: ""

        contentItem: ColumnLayout {
            spacing: 8
            TextField {
                id: renameField
                Layout.fillWidth: true
                text: renameDialog.oldName
                selectByMouse: true
                background: Rectangle { color: "#ffffff"; border.color: "#cccccc"; border.width: 1; radius: 6 }
            }
        }

        footer: DialogButtonBox {
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
            fileService.renameItem(renameDialog.targetId, renameField.text.trim())
            refreshFiles()
            statusLabel.text = qsTr("已重命名: ") + renameField.text.trim()
        }
    }


    Connections {
        target: syncService
        function onSyncFinished(ok, message) {
            statusLabel.color = ok ? "green" : "red"
            statusLabel.text = message
            refreshFiles()
        }
    }

    Connections {
        target: webDavClient
        function onDirectoryListed(path, entries, error) {
            if (error.length > 0) {
                statusLabel.color = "red"
                statusLabel.text = error
                return
            }
            statusLabel.color = "green"
            statusLabel.text = qsTr("已连接，远程 %1 个文件").arg(entries.length)
        }
        function onDirectoryCreated(remotePath, ok, error) {
            if (!ok) {
                statusLabel.color = "red"
                statusLabel.text = qsTr("创建目录失败: ") + error
                root.pendingMkcolCount = 0
                return
            }
            root.pendingMkcolCount--
            if (root.pendingMkcolCount === 0) {
                webDavClient.listDirectory("/zesbks/")
            }
        }
        function onUploadFinished(remotePath, ok, error) {
            statusLabel.color = ok ? "green" : "red"
            statusLabel.text = ok ? qsTr("上传成功: ") + remotePath : error
        }
        function onDownloadFinished(remotePath, localPath, ok, error) {
            if (root.pendingDownloadRemote === remotePath) {
                if (ok) {
                    fileService.markDownloaded(root.pendingDownloadId, localPath)
                    refreshFiles()
                    root.pdfPreviewSource = localPath
                    if (root.pendingOpenFullscreen) {
                        root.openPdfFullscreen(localPath)
                    }
                    statusLabel.color = "green"
                    statusLabel.text = qsTr("下载完成并打开: ") + root.pendingDownloadName
                } else {
                    statusLabel.color = "red"
                    statusLabel.text = error
                }
                root.pendingDownloadId = ""
                root.pendingDownloadRemote = ""
                root.pendingDownloadName = ""
                root.pendingOpenFullscreen = false
            }
        }
    }
}

