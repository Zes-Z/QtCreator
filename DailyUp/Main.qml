import QtQuick
import QtQuick.Window
import QtQuick.Controls
import DailyUp 1.0

Window {
    id: root

    width: 360
    height: 640
    minimumWidth: 320
    minimumHeight: 360
    maximumWidth: 560
    maximumHeight: 900

    visible: true
    title: "DailyUp"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
    color: "transparent"

    property string currentDate: ""
    property date todayDate: new Date()
    property int editingIndex: -1
    property bool ready: false

    readonly property string todayKey: root.dateKey(root.todayDate)
    readonly property bool viewingToday: root.currentDate === root.todayKey
    readonly property real completionRatio: taskManager.totalCount > 0
                                             ? taskManager.completedCount / taskManager.totalCount
                                             : 0
    readonly property string dateHeading: root.headingFor(root.currentDate)
    readonly property string dateWeekday: root.weekdayFor(root.currentDate)

    function two(value) {
        return String(value).padStart(2, "0")
    }

    function dateKey(value) {
        return value.getFullYear() + "-" + root.two(value.getMonth() + 1) + "-" + root.two(value.getDate())
    }

    function parsed(value) {
        const result = new Date(value + "T00:00:00")
        return isNaN(result.getTime()) ? new Date() : result
    }

    function headingFor(value) {
        const day = root.parsed(value)
        return (day.getMonth() + 1) + "月" + day.getDate() + "日"
    }

    function weekdayFor(value) {
        const day = root.parsed(value)
        return ["周日", "周一", "周二", "周三", "周四", "周五", "周六"][day.getDay()]
    }

    function scheduleGeometrySave() {
        if (!root.ready || root.visibility !== Window.Windowed)
            return
        saveGeometry.restart()
    }

    TaskManager {
        id: taskManager
    }

    Shortcut {
        sequence: "Ctrl+N"
        onActivated: addDialog.openWith("")
    }

    Shortcut {
        sequence: "Ctrl+W"
        onActivated: Qt.quit()
    }

    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }

    Shortcut {
        sequence: "Ctrl+,"
        onActivated: themePicker.open()
    }

    Connections {
        target: taskManager

        function onDateRollover() {
            root.todayDate = new Date()
            root.currentDate = root.todayKey
        }
    }

    function showThemePicker() {
        themePicker.open()
    }

    function showAddDialog() {
        addDialog.openWith("")
    }

    function showEditDialog() {
        if (taskManager.totalCount === 0)
            return
        root.editingIndex = 0
        editDialog.openWith(taskManager.contentAt(0))
    }

    Component.onCompleted: {
        const now = new Date()
        root.todayDate = now
        root.currentDate = root.dateKey(now)
        taskManager.loadTasks(root.currentDate)

        const geometry = AppSettings.windowGeometry
        if (geometry && geometry.width) {
            root.width = Math.max(root.minimumWidth, Math.min(root.maximumWidth, geometry.width))
            root.height = Math.max(root.minimumHeight, Math.min(root.maximumHeight, geometry.height))
        }
        root.ready = true
    }

    onWidthChanged: root.scheduleGeometrySave()
    onHeightChanged: root.scheduleGeometrySave()
    onClosing: AppSettings.saveWindowGeometry(root.width, root.height)

    Timer {
        id: saveGeometry
        interval: 600
        onTriggered: AppSettings.saveWindowGeometry(root.width, root.height)
    }

    Rectangle {
        id: panel

        anchors.fill: parent
        radius: Theme.windowRadius
        clip: true
        opacity: 0

        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.bgTop }
            GradientStop { position: 1.0; color: Theme.bgBottom }
        }

        Behavior on opacity { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

        Component.onCompleted: panel.opacity = 1

        // 顶部一层极淡的强调色，让背景不至于太平
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 220
            radius: Theme.windowRadius
            color: "transparent"

            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.alpha(Theme.accent, 0.09) }
                GradientStop { position: 1.0; color: Theme.alpha(Theme.accent, 0.0) }
            }
        }

        // 顶部 1px 高光，模拟 macOS 玻璃边缘
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.windowRadius
            anchors.rightMargin: Theme.windowRadius
            height: 1
            color: Qt.rgba(1, 1, 1, 0.65)
        }

        // 空白标题栏可拖动窗口
        MouseArea {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 40
            acceptedButtons: Qt.LeftButton
            onPressed: root.startSystemMove()
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 15
            text: "DailyUp"
            font.pixelSize: 11
            font.weight: Font.Medium
            font.letterSpacing: 0.6
            color: Theme.faint
        }

        WindowControls {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 16
            anchors.topMargin: 15
            win: root
        }

        Row {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: 12
            anchors.topMargin: 10
            spacing: 2

            Item {
                width: 28
                height: 28

                Rectangle {
                    anchors.fill: parent
                    radius: 14
                    color: themeButtonHover.hovered ? Theme.alpha(Theme.accent, 0.12) : Theme.alpha(Theme.accent, 0)
                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    anchors.centerIn: parent
                    color: Theme.accent
                    border.width: 1.5
                    border.color: Qt.rgba(1, 1, 1, 0.75)
                }

                HoverHandler {
                    id: themeButtonHover
                    cursorShape: Qt.PointingHandCursor
                }

                ToolTip.visible: themeButtonHover.hovered
                ToolTip.delay: 500
                ToolTip.text: "切换主题"

                TapHandler {
                    onTapped: themePicker.opened ? themePicker.close() : themePicker.open()
                }
            }

            Item {
                width: 28
                height: 28

                Rectangle {
                    anchors.fill: parent
                    radius: 14
                    color: folderButtonHover.hovered ? Theme.alpha(Theme.ink, 0.07) : Theme.alpha(Theme.ink, 0)
                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                Item {
                    width: 15
                    height: 13
                    anchors.centerIn: parent

                    Rectangle {
                        x: 0
                        y: 3
                        width: 15
                        height: 10
                        radius: 2.5
                        color: folderButtonHover.hovered ? Theme.accent : Theme.alpha(Theme.ink, 0.48)
                    }

                    Rectangle {
                        x: 1
                        y: 0
                        width: 7
                        height: 5
                        radius: 1.5
                        color: folderButtonHover.hovered ? Theme.accent : Theme.alpha(Theme.ink, 0.48)
                    }
                }

                HoverHandler {
                    id: folderButtonHover
                    cursorShape: Qt.PointingHandCursor
                }

                ToolTip.visible: folderButtonHover.hovered
                ToolTip.delay: 500
                ToolTip.text: "打开数据目录"

                TapHandler {
                    onTapped: taskManager.openDatabaseFolder()
                }
            }
        }

        Text {
            id: dateLabel

            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 12
            anchors.topMargin: 36

            text: root.dateHeading
            font.pixelSize: 21
            font.weight: Font.DemiBold
            color: Theme.ink
        }

        Text {
            id: weekdayLabel

            anchors.left: dateLabel.right
            anchors.leftMargin: 8
            anchors.baseline: dateLabel.baseline

            text: root.dateWeekday
            font.pixelSize: 12
            color: Theme.inkSecondary
        }

        Rectangle {
            visible: root.viewingToday

            anchors.left: weekdayLabel.right
            anchors.leftMargin: 8
            anchors.verticalCenter: weekdayLabel.verticalCenter

            width: 33
            height: 18
            radius: 9
            color: Theme.accentSoft

            Text {
                anchors.centerIn: parent
                text: "今天"
                font.pixelSize: 10
                font.weight: Font.Medium
                color: Theme.accent
            }
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.baseline: dateLabel.baseline

            visible: taskManager.totalCount > 0
            text: taskManager.completedCount + " / " + taskManager.totalCount
            font.pixelSize: 12
            font.weight: Font.Medium
            color: Theme.inkSecondary
        }

        Rectangle {
            id: progressTrack

            anchors.top: dateLabel.bottom
            anchors.topMargin: 6
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 12
            anchors.rightMargin: 12

            height: 3
            radius: 1.5
            color: Theme.track

            Rectangle {
                width: parent.width * root.completionRatio
                height: parent.height
                radius: 1.5
                color: Theme.accent
                Behavior on width { NumberAnimation { duration: 320; easing.type: Easing.OutCubic } }
            }
        }

        DateStrip {
            id: dateStrip

            anchors.top: progressTrack.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            height: 48

            today: root.todayDate
            selectedDate: root.currentDate

            onDateSelected: (dateStr) => {
                root.currentDate = dateStr
                taskManager.loadTasks(dateStr)
            }
        }

        Rectangle {
            id: taskCard

            anchors.top: dateStrip.bottom
            anchors.topMargin: 6
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottom: addButton.top
            anchors.bottomMargin: 9   // 卡片底 → 按钮顶，与按钮底边 9 对称

            radius: Theme.cardRadius
            color: Theme.card
            border.width: 1
            border.color: Theme.cardBorder


            ListView {
                id: taskList

                anchors.fill: parent

                anchors.margins: 9
                anchors.bottomMargin: taskCard.height > 270 ? 32 : 9
                clip: true
                spacing: 6  // 任务条间距

                model: taskManager

                delegate: TaskRow {
                    width: taskList.width
                    manager: taskManager

                    onEditRequested: (index, content) => {
                        root.editingIndex = index
                        editDialog.openWith(content)
                    }
                }

                add: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180 }
                }

                displaced: Transition {
                    NumberAnimation { property: "y"; duration: 180; easing.type: Easing.OutCubic }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 9
                visible: taskManager.totalCount > 0 && taskCard.height > 270
                text: "左滑删除 · 右滑置顶 / 编辑"
                font.pixelSize: 10
                color: Theme.faint
            }

            Column {
                visible: taskManager.totalCount === 0
                anchors.centerIn: parent
                spacing: 9

                Rectangle {
                    width: 52
                    height: 52
                    radius: 26
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Theme.alpha(Theme.accent, 0.10)

                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        font.pixelSize: 22
                        font.bold: true
                        color: Theme.accent
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "今天还没有任务"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: Theme.inkSecondary
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "写下今天的第一件小事"
                    font.pixelSize: 12
                    color: Theme.faint
                }
            }
        }

        Rectangle {
            id: addShadow

            anchors.left: addButton.left
            anchors.right: addButton.right
            anchors.top: addButton.top
            anchors.topMargin: 4
            height: addButton.height

            radius: addButton.radius
            color: Theme.alpha(Theme.accentDeep, 0.18)
        }

        Rectangle {
            id: addButton

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 9    // 距窗口底边

            height: 45
            radius: 14

            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.accent }
                GradientStop { position: 1.0; color: Theme.accentDeep }
            }

            scale: addMouse.pressed ? 0.985 : 1
            Behavior on scale { NumberAnimation { duration: 90 } }

            Rectangle {
                anchors.fill: parent
                radius: 14
                color: "#FFFFFF"
                opacity: addMouse.containsMouse ? 0.10 : 0
                Behavior on opacity { NumberAnimation { duration: 120 } }
            }

            Row {
                anchors.centerIn: parent
                spacing: 7

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "+"
                    font.pixelSize: 18
                    font.weight: Font.Medium
                    color: Theme.accentContrast
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "新建任务"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    color: Theme.accentContrast
                }
            }

            MouseArea {
                id: addMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: addDialog.openWith("")
            }
        }

        // 边缘 / 角落拖拽调整大小（无边框窗口在 macOS 上没有系统缩放边）
        MouseArea {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 42
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            width: 5
            cursorShape: Qt.SizeHorCursor

            property real startWidth: 0
            property real startSceneX: 0

            onPressed: (mouse) => {
                startWidth = root.width
                startSceneX = mouse.scenePosition.x
            }

            onPositionChanged: (mouse) => {
                if (!pressed)
                    return
                root.width = Math.max(root.minimumWidth,
                                      Math.min(root.maximumWidth,
                                               startWidth + mouse.scenePosition.x - startSceneX))
            }
        }

        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: 18
            anchors.bottom: parent.bottom
            height: 5
            cursorShape: Qt.SizeVerCursor

            property real startHeight: 0
            property real startSceneY: 0

            onPressed: (mouse) => {
                startHeight = root.height
                startSceneY = mouse.scenePosition.y
            }

            onPositionChanged: (mouse) => {
                if (!pressed)
                    return
                root.height = Math.max(root.minimumHeight,
                                       Math.min(root.maximumHeight,
                                                startHeight + mouse.scenePosition.y - startSceneY))
            }
        }

        MouseArea {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: 18
            height: 18
            cursorShape: Qt.SizeFDiagCursor

            property real startWidth: 0
            property real startHeight: 0
            property real startSceneX: 0
            property real startSceneY: 0

            onPressed: (mouse) => {
                startWidth = root.width
                startHeight = root.height
                startSceneX = mouse.scenePosition.x
                startSceneY = mouse.scenePosition.y
            }

            onPositionChanged: (mouse) => {
                if (!pressed)
                    return
                root.width = Math.max(root.minimumWidth,
                                      Math.min(root.maximumWidth,
                                               startWidth + mouse.scenePosition.x - startSceneX))
                root.height = Math.max(root.minimumHeight,
                                       Math.min(root.maximumHeight,
                                                startHeight + mouse.scenePosition.y - startSceneY))
            }
        }

        // 弹窗打开时的透明拦截层：点击/滚轮不会再落到日期条或任务列表
        MouseArea {
            id: modalBlocker

            anchors.fill: parent
            // 位于内容之上、弹窗之下；靠声明顺序而不是 z 值
            visible: addDialog.opened || editDialog.opened || themePicker.opened
            enabled: visible
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            preventStealing: true
            cursorShape: Qt.ArrowCursor

            onPressed: (mouse) => {
                if (addDialog.opened)
                    addDialog.close()
                else if (editDialog.opened)
                    editDialog.close()
                else if (themePicker.opened)
                    themePicker.close()
                mouse.accepted = true
            }

            onWheel: (wheel) => {
                wheel.accepted = true
            }
        }

        Rectangle {
            id: dialogDim

            anchors.fill: parent
            radius: Theme.windowRadius
            color: Qt.rgba(0.06, 0.08, 0.12, 0.12)
            opacity: (addDialog.opened || editDialog.opened) ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 170 } }
        }

        Rectangle {
            id: toast

            property string message: ""

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: addButton.top
            anchors.bottomMargin: 14
            width: Math.min(310, parent.width - 48)
            height: 32
            radius: 16
            color: Qt.rgba(0.12, 0.12, 0.14, 0.92)
            opacity: 0
            visible: opacity > 0
            z: 180

            Behavior on opacity { NumberAnimation { duration: 160 } }

            Text {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                text: toast.message
                color: "#FFFFFF"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Timer {
                id: toastTimer
                interval: 2600
                onTriggered: toast.opacity = 0
            }

            function show(text) {
                message = text
                opacity = 1
                toastTimer.restart()
            }
        }

        Connections {
            target: taskManager

            function onErrorOccurred(message) {
                toast.show(message)
            }
        }

        TaskDialog {
            id: addDialog
            heading: "新建任务"
            confirmLabel: "添加"
            onAccepted: (text) => taskManager.addTask(root.currentDate, text)
        }

        TaskDialog {
            id: editDialog
            heading: "编辑任务"
            confirmLabel: "保存"
            onAccepted: (text) => taskManager.updateTaskContent(root.editingIndex, text)
        }

        ThemePicker {
            id: themePicker
        }
    }
}