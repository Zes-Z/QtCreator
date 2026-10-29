import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Effects
import DayBook

Window {
    id: win

    width: 1200
    height: 800
    minimumWidth: 840
    minimumHeight: 600
    visible: true
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
    title: "DayBook · 日光账本"

    property bool maximized: visibility === Window.Maximized || visibility === Window.FullScreen
    property int frame: 0
    readonly property int shellTop: 58
    readonly property int shellBottom: 8
    property int chromeRadius: maximized ? 0 : 26
    property string currentPage: App.startupPage

    readonly property var navMetaData: ({
        "overview": { "label": "概览", "icon": "home" },
        "transactions": { "label": "明细", "icon": "receipt_long" },
        "insights": { "label": "统计", "icon": "insights" },
        "accounts": { "label": "账户", "icon": "account_balance_wallet" },
        "budgets": { "label": "预算", "icon": "pie_chart" }
    })

    function navMeta(key) {
        return navMetaData[key] !== undefined
               ? navMetaData[key]
               : { "label": key, "icon": "circle" }
    }

    function moveNav(index, delta) {
        const target = index + delta
        if (target >= 0 && target < App.navOrder.length)
            App.moveNavItem(index, target)
    }

    function moveSummary(index, delta) {
        const target = index + delta
        if (target >= 0 && target < App.summaryOrder.length)
            App.moveSummaryItem(index, target)
    }

    function summaryHeight(key) {
        return 68
    }

    function summaryTarget(index, offset) {
        const order = App.summaryOrder
        if (order.length <= 1)
            return index
        const centers = []
        let y = 0
        for (let i = 0; i < order.length; ++i) {
            const h = summaryHeight(order[i])
            centers.push(y + h / 2)
            y += h + 6
        }
        const dragged = centers[index] + offset
        let best = index
        let distance = 1e9
        for (let i = 0; i < centers.length; ++i) {
            const d = Math.abs(centers[i] - dragged)
            if (d < distance) {
                distance = d
                best = i
            }
        }
        return best
    }

    function setPage(page) {
        if (page === "" || page === undefined)
            return
        currentPage = page
    }

    function toggleMaximize() {
        if (maximized)
            visibility = Window.Windowed
        else
            visibility = Window.Maximized
    }

    function toggleFullScreen() {
        if (visibility === Window.FullScreen)
            visibility = Window.Windowed
        else
            visibility = Window.FullScreen
    }

    opacity: 0
    Component.onCompleted: {
        opacity = 1
        if (App.startupModal === "account")
            currentPage = "accounts"
        else if (App.startupModal === "budget")
            currentPage = "budgets"
        else if (App.startupModal === "filter" || App.startupModal === "filtertype")
            currentPage = "transactions"
        if (App.startupModal === "add" || App.startupModal === "transfer") {
            Qt.callLater(function() { txDialog.openNew() })
        } else if (App.startupModal === "date") {
            Qt.callLater(function() {
                txDialog.openNew()
                Qt.callLater(function() { txDialog.openCalendar() })
            })
        } else if (App.startupModal === "about") {
            Qt.callLater(function() { aboutDialog.open() })
        } else if (App.startupModal === "month") {
            currentPage = "overview"
        }
    }
    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

    Shortcut { sequences: [StandardKey.Quit]; onActivated: Qt.quit() }
    Shortcut { sequences: [StandardKey.Close]; onActivated: win.close() }
    Shortcut { sequence: "Ctrl+N"; onActivated: txDialog.openNew() }
    Shortcut { sequence: "Ctrl+1"; onActivated: win.setPage("overview") }
    Shortcut { sequence: "Ctrl+2"; onActivated: win.setPage("transactions") }
    Shortcut { sequence: "Ctrl+3"; onActivated: win.setPage("insights") }
    Shortcut { sequence: "Ctrl+4"; onActivated: win.setPage("accounts") }
    Shortcut { sequence: "Ctrl+5"; onActivated: win.setPage("budgets") }
    Shortcut { sequence: "Ctrl+,"; onActivated: win.setPage("settings") }

    Item {
        id: rootItem
        anchors.fill: parent

        Item {
            id: chrome
            anchors.fill: parent
            anchors.margins: win.frame

            Canvas {
                id: backgroundCanvas
                anchors.fill: parent
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const w = width
                    const h = height
                    const r = win.chromeRadius
                    const dark = Theme.dark

                    ctx.clearRect(0, 0, w, h)
                    ctx.save()
                    ctx.beginPath()
                    if (r <= 0) {
                        ctx.rect(0, 0, w, h)
                    } else {
                        ctx.moveTo(r, 0)
                        ctx.lineTo(w - r, 0)
                        ctx.quadraticCurveTo(w, 0, w, r)
                        ctx.lineTo(w, h - r)
                        ctx.quadraticCurveTo(w, h, w - r, h)
                        ctx.lineTo(r, h)
                        ctx.quadraticCurveTo(0, h, 0, h - r)
                        ctx.lineTo(0, r)
                        ctx.quadraticCurveTo(0, 0, r, 0)
                        ctx.closePath()
                    }
                    ctx.clip()

                    const base = ctx.createLinearGradient(0, 0, w * 0.55, h)
                    if (dark) {
                        base.addColorStop(0, "#151C24")
                        base.addColorStop(0.55, "#0F151B")
                        base.addColorStop(1, "#0A0E12")
                    } else {
                        base.addColorStop(0, "#F7FCF8")
                        base.addColorStop(0.34, "#F1F8FC")
                        base.addColorStop(0.68, "#FBF2F5")
                        base.addColorStop(1, "#FCF4EA")
                    }
                    ctx.fillStyle = base
                    ctx.fillRect(0, 0, w, h)

                    function glow(cx, cy, radius, color) {
                        const g = ctx.createRadialGradient(cx, cy, 0, cx, cy, radius)
                        g.addColorStop(0, color)
                        g.addColorStop(0.55, color.replace(/[\d.]+\)$/, "0.10)"))
                        g.addColorStop(1, "rgba(0,0,0,0)")
                        ctx.fillStyle = g
                        ctx.fillRect(cx - radius, cy - radius, radius * 2, radius * 2)
                    }

                    if (dark) {
                        glow(w * 0.18, h * 0.10, w * 0.55, "rgba(79,142,247,0.16)")
                        glow(w * 0.92, h * 0.42, w * 0.45, "rgba(47,174,131,0.11)")
                        glow(w * 0.42, h * 1.05, w * 0.5, "rgba(164,123,232,0.10)")
                        const shade = ctx.createLinearGradient(0, 0, 0, h)
                        shade.addColorStop(0, "rgba(255,255,255,0.035)")
                        shade.addColorStop(0.3, "rgba(255,255,255,0)")
                        ctx.fillStyle = shade
                        ctx.fillRect(0, 0, w, h)
                    } else {
                        glow(w * 0.12, h * 0.08, w * 0.50, "rgba(158,214,184,0.34)")
                        glow(w * 0.90, h * 0.12, w * 0.46, "rgba(165,207,245,0.30)")
                        glow(w * 0.26, h * 0.56, w * 0.44, "rgba(246,187,207,0.26)")
                        glow(w * 0.94, h * 0.56, w * 0.44, "rgba(246,169,160,0.20)")
                        glow(w * 0.48, h * 1.05, w * 0.52, "rgba(250,205,156,0.30)")
                        glow(w * 0.74, h * 0.34, w * 0.34, "rgba(246,226,170,0.24)")
                        const shade = ctx.createLinearGradient(0, 0, 0, h)
                        shade.addColorStop(0, "rgba(255,255,255,0.24)")
                        shade.addColorStop(0.3, "rgba(255,255,255,0)")
                        ctx.fillStyle = shade
                        ctx.fillRect(0, 0, w, h)
                    }
                    ctx.restore()
                }
            }

            Connections {
                target: Theme
                function onDarkChanged() { backgroundCanvas.requestPaint() }
            }
            Connections {
                target: win
                function onMaximizedChanged() { backgroundCanvas.requestPaint() }
            }
            onWidthChanged: backgroundCanvas.requestPaint()
            onHeightChanged: backgroundCanvas.requestPaint()

            Image {
                anchors.fill: parent
                source: "qrc:/assets/noise.png"
                fillMode: Image.Tile
                opacity: Theme.dark ? 0.045 : 0.032
            }

            // ------------------------------------------------------------- top bar
            Item {
                id: topBar
                width: parent.width
                height: 58

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onPressed: {
                        if (!win.maximized)
                            win.startSystemMove()
                    }
                    onDoubleClicked: win.toggleMaximize()
                }

                TrafficLights {
                    x: 18
                    y: 24
                    windowActive: win.active
                    onCloseClicked: win.close()
                    onMinimizeClicked: win.showMinimized()
                    onMaximizeClicked: win.toggleFullScreen()
                }

                Item {
                    id: logoMark
                    x: 85
                    y: 16
                    width: 32
                    height: 32
                    Image {
                        anchors.fill: parent
                        source: "qrc:/assets/appicon.png"
                        sourceSize.width: 64
                        sourceSize.height: 64
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: aboutDialog.open()
                    }
                }

                Text {
                    x: 124
                    anchors.verticalCenter: logoMark.verticalCenter
                    text: "DayBook"
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: 15
                    font.weight: 700
                    font.letterSpacing: 0.3
                }
                Text {
                    x: 200
                    anchors.verticalCenter: logoMark.verticalCenter
                    text: "日光账本"
                    color: Theme.faint
                    font.family: Theme.fontFamily
                    font.pixelSize: 10
                    font.letterSpacing: 0.6
                    anchors.verticalCenterOffset: 1
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10

                    Text {
                        text: App.todayText()
                        color: Theme.faint
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    IconButton {
                        icon: App.isDark ? "light_mode" : "dark_mode"
                        size: 32
                        iconSize: 17
                        hoverColor: Theme.surfaceSunken
                        anchors.verticalCenter: parent.verticalCenter
                        onClicked: App.setThemeMode(App.isDark ? "light" : "dark")
                    }

                    AppButton {
                        text: "记一笔"
                        icon: "add"
                        compact: true
                        width: 102
                        anchors.verticalCenter: parent.verticalCenter
                        onClicked: txDialog.openNew()
                    }
                }
            }

            // -------------------------------------------------------------- sidebar
            GlassCard {
                id: sidebar
                x: 12
                y: win.shellTop
                width: 204
                height: chrome.height - win.shellTop - win.shellBottom
                radius: 22
                tint: Theme.sidebarSurface
                shadow: true
                shadowBlur: 30
                shadowOffsetY: 10
                sheenStrength: 0.65
                noise: true

                Item {
                    anchors.fill: parent
                    anchors.margins: 12

                    Column {
                        id: navColumn
                        width: parent.width
                        spacing: 3
                        move: Transition {
                            NumberAnimation {
                                properties: "y"
                                duration: 230
                                easing.type: Easing.OutCubic
                            }
                        }

                        Text {
                            height: 21
                            leftPadding: 12
                            text: "账本"
                            color: Theme.muted
                            font.family: Theme.fontFamily
                            font.pixelSize: 10
                            font.weight: 600
                            font.letterSpacing: 1.4
                            verticalAlignment: Text.AlignVCenter
                        }

                        Repeater {
                            model: App.navOrder
                            Item {
                                id: navRow
                                property real dragOffset: 0
                                width: parent.width
                                height: 42
                                z: navDrag.active ? 50 : 0
                                scale: navDrag.active ? 1.02 : 1.0
                                opacity: navDrag.active ? 0.96 : 1.0
                                transform: Translate { y: navRow.dragOffset }
                                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                Behavior on dragOffset {
                                    enabled: !navDrag.active
                                    NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
                                }

                                NavItem {
                                    anchors.fill: parent
                                    label: win.navMeta(modelData).label
                                    icon: win.navMeta(modelData).icon
                                    active: win.currentPage === modelData
                                    onClicked: win.setPage(modelData)
                                }

                                DragHandler {
                                    id: navDrag
                                    target: null
                                    acceptedButtons: Qt.LeftButton
                                    dragThreshold: 7
                                    xAxis.enabled: false
                                    yAxis.enabled: true
                                    onTranslationChanged: if (active) navRow.dragOffset = activeTranslation.y
                                    onActiveChanged: {
                                        if (active) {
                                            navRow.dragOffset = 0
                                        } else {
                                            const offset = navRow.dragOffset
                                            navRow.dragOffset = 0
                                            const target = Math.max(0, Math.min(App.navOrder.length - 1,
                                                                              Math.round(index + offset / 45)))
                                            if (target !== index)
                                                App.moveNavItem(index, target)
                                        }
                                    }
                                }

                                DragGrip {
                                    id: navGrip
                                    anchors.right: parent.right
                                    anchors.rightMargin: 2
                                    anchors.verticalCenter: parent.verticalCenter
                                    z: 10
                                }
                            }
                        }

                        Item { width: 1; height: 10 }
                        Rectangle { width: parent.width; height: 1; color: Theme.hairline }
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: navColumn.bottom
                        anchors.topMargin: 5
                        spacing: 6
                        move: Transition {
                            NumberAnimation {
                                properties: "y"
                                duration: 230
                                easing.type: Easing.OutCubic
                            }
                        }

                        Text {
                            height: 16
                            leftPadding: 12
                            text: "本月速览"
                            color: Theme.muted
                            font.family: Theme.fontFamily
                            font.pixelSize: 9
                            font.weight: 600
                            font.letterSpacing: 1.1
                            verticalAlignment: Text.AlignVCenter
                        }

                        Repeater {
                            model: App.summaryOrder
                            Item {
                                id: summaryRow
                                property real dragOffset: 0
                                visible: index < 2 || chrome.height >= 720
                                width: parent.width
                                height: summaryCard.implicitHeight
                                z: summaryDrag.active ? 50 : 0
                                scale: summaryDrag.active ? 1.02 : 1.0
                                opacity: summaryDrag.active ? 0.96 : 1.0
                                transform: Translate { y: summaryRow.dragOffset }
                                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                Behavior on dragOffset {
                                    enabled: !summaryDrag.active
                                    NumberAnimation { duration: 190; easing.type: Easing.OutCubic }
                                }

                                SidebarSummaryCard {
                                    id: summaryCard
                                    anchors.fill: parent
                                    kind: modelData
                                    onClicked: {
                                        if (modelData === "cashflow")
                                            win.setPage("transactions")
                                        else if (modelData === "budget")
                                            win.setPage("budgets")
                                    }
                                }

                                DragHandler {
                                    id: summaryDrag
                                    target: null
                                    acceptedButtons: Qt.LeftButton
                                    dragThreshold: 7
                                    xAxis.enabled: false
                                    yAxis.enabled: true
                                    onTranslationChanged: if (active) summaryRow.dragOffset = activeTranslation.y
                                    onActiveChanged: {
                                        if (active) {
                                            summaryRow.dragOffset = 0
                                        } else {
                                            const offset = summaryRow.dragOffset
                                            summaryRow.dragOffset = 0
                                            const target = win.summaryTarget(index, offset)
                                            if (target !== index)
                                                App.moveSummaryItem(index, target)
                                        }
                                    }
                                }

                                DragGrip {
                                    id: summaryGrip
                                    anchors.right: parent.right
                                    anchors.rightMargin: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    z: 10
                                }
                            }
                        }

                    }

                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        spacing: 6

                        NavItem {
                            width: parent.width
                            label: "设置"
                            icon: "settings"
                            active: win.currentPage === "settings"
                            onClicked: win.setPage("settings")
                        }

                        Row {
                            width: parent.width
                            height: 20
                            spacing: 5
                            Icon {
                                name: "lock"
                                size: 11
                                iconColor: Theme.faint
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: "本地存储 · 隐私安全"
                                color: Theme.faint
                                font.family: Theme.fontFamily
                                font.pixelSize: 9
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }

            // --------------------------------------------------------------- content
            Loader {
                id: pageLoader
                x: 226
                y: win.shellTop
                width: chrome.width - 238
                height: chrome.height - win.shellTop - win.shellBottom
                sourceComponent: {
                    if (win.currentPage === "transactions")
                        return transactionsPage
                    if (win.currentPage === "insights")
                        return insightsPage
                    if (win.currentPage === "accounts")
                        return accountsPage
                    if (win.currentPage === "budgets")
                        return budgetsPage
                    if (win.currentPage === "settings")
                        return settingsPage
                    return overviewPage
                }
                onLoaded: {
                    opacity = 0
                    pageFade.restart()
                    item.forceActiveFocus()
                }
                opacity: 1
            }

            NumberAnimation {
                id: pageFade
                target: pageLoader
                property: "opacity"
                from: 0
                to: 1
                duration: 210
                easing.type: Easing.OutCubic
            }

            Toast {
                id: toast
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 24
            }
        }

        // Resize handles. Corners are intentionally generous, especially the
        // bottom-right one, because the old 14px hit area was hard to grab.
        MouseArea {
            visible: !win.maximized
            width: 18
            height: 18
            anchors.left: parent.left
            anchors.top: parent.top
            cursorShape: Qt.SizeFDiagCursor
            acceptedButtons: Qt.LeftButton
            z: 1000
            onPressed: win.startSystemResize(Qt.LeftEdge | Qt.TopEdge)
        }
        MouseArea {
            visible: !win.maximized
            width: 18
            height: 18
            anchors.right: parent.right
            anchors.top: parent.top
            cursorShape: Qt.SizeBDiagCursor
            acceptedButtons: Qt.LeftButton
            z: 1000
            onPressed: win.startSystemResize(Qt.RightEdge | Qt.TopEdge)
        }
        MouseArea {
            visible: !win.maximized
            width: 30
            height: 30
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            cursorShape: Qt.SizeBDiagCursor
            acceptedButtons: Qt.LeftButton
            z: 1000
            onPressed: win.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
        }
        MouseArea {
            visible: !win.maximized
            width: 30
            height: 30
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            cursorShape: Qt.SizeFDiagCursor
            acceptedButtons: Qt.LeftButton
            z: 1000
            onPressed: win.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
        }
        MouseArea {
            visible: !win.maximized
            width: 10
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            cursorShape: Qt.SizeHorCursor
            acceptedButtons: Qt.LeftButton
            z: 1000
            onPressed: win.startSystemResize(Qt.LeftEdge)
        }
        MouseArea {
            visible: !win.maximized
            width: 10
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            cursorShape: Qt.SizeHorCursor
            acceptedButtons: Qt.LeftButton
            z: 1000
            onPressed: win.startSystemResize(Qt.RightEdge)
        }
        MouseArea {
            visible: !win.maximized
            height: 10
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            cursorShape: Qt.SizeVerCursor
            acceptedButtons: Qt.LeftButton
            z: 1000
            onPressed: win.startSystemResize(Qt.TopEdge)
        }
        MouseArea {
            visible: !win.maximized
            height: 10
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            cursorShape: Qt.SizeVerCursor
            acceptedButtons: Qt.LeftButton
            z: 1000
            onPressed: win.startSystemResize(Qt.BottomEdge)
        }
    }

    TransactionDialog {
        id: txDialog
        onSaved: {}
        onDeleted: {}
    }

    AboutDialog {
        id: aboutDialog
    }

    Connections {
        target: UiBus
        function onToastRequested(message, kind) { toast.show(message, kind) }
        function onOpenTransactionRequested(id) {
            if (id > 0)
                txDialog.openFor(id)
            else
                txDialog.openNew()
        }
        function onNavigateRequested(page) { win.setPage(page) }
    }

    Component { id: overviewPage; OverviewPage {} }
    Component { id: transactionsPage; TransactionsPage {} }
    Component { id: insightsPage; InsightsPage {} }
    Component { id: accountsPage; AccountsPage {} }
    Component { id: budgetsPage; BudgetsPage {} }
    Component { id: settingsPage; SettingsPage {} }
}
