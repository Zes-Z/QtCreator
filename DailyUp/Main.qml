import QtQuick
import QtQuick.Window
import QtQuick.Layouts

Window {
    id: root
    width: 320
    height: 600
    minimumWidth: 292
    maximumWidth: 648
    minimumHeight: 265
    visible: true
    title: qsTr("DailyUp")
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"

    Rectangle {
        id: mainPanel
        anchors.fill: parent
        anchors.margins: 0
        color: "#F0F0F5"
        radius: 24
        clip: true

        // 独立定位在右上角外侧圆弧处的关闭按钮
        Rectangle {
            width: 26
            height: 26
            radius: 13
            color: closeArea.containsMouse ? "#3A80D2" : "#4A90E2"
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 0
            anchors.rightMargin: 0
            z: 100

            Behavior on color { ColorAnimation { duration: 150 } }

            Text {
                anchors.centerIn: parent
                text: "×"
                font.pixelSize: 16
                font.bold: true
                font.family: "Segoe UI"
                color: "#FFFFFF"
                anchors.verticalCenterOffset: -1
            }

            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: Qt.quit()
            }
        }

        // 独立定位在左下角外侧圆弧处的最小化按钮
        Rectangle {
            width: 26
            height: 26
            radius: 13
            color: minArea.containsMouse ? "#E0A800" : "#FFC107"
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.bottomMargin: 0
            anchors.leftMargin: 0
            z: 100

            Behavior on color { ColorAnimation { duration: 150 } }

            Text {
                anchors.centerIn: parent
                text: "-"
                font.pixelSize: 18
                font.bold: true
                font.family: "Segoe UI"
                color: "#FFFFFF"
                anchors.verticalCenterOffset: -1
            }

            MouseArea {
                id: minArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.showMinimized()
            }
        }

        // 自定义窗口拖拽与边界回弹逻辑
        MouseArea {
            id: dragMouseArea
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            property point startMousePos

            onPressed: (mouse) => {
                startMousePos = Qt.point(mouse.x, mouse.y)
            }

            onPositionChanged: (mouse) => {
                if (pressed) {
                    var dx = mouse.x - startMousePos.x
                    var dy = mouse.y - startMousePos.y
                    root.x += dx
                    root.y += dy
                }
            }

            onReleased: {
                var screenW = Screen.desktopAvailableWidth
                var screenH = Screen.desktopAvailableHeight

                if (root.x < 0) {
                    bounceX.to = 0
                    bounceX.start()
                } else if (root.x + root.width > screenW) {
                    bounceX.to = screenW - root.width
                    bounceX.start()
                }

                if (root.y < 0) {
                    bounceY.to = 0
                    bounceY.start()
                } else if (root.y + root.height > screenH) {
                    bounceY.to = screenH - root.height
                    bounceY.start()
                }
            }
        }

        NumberAnimation {
            id: bounceX
            target: root
            property: "x"
            duration: 250
            easing.type: Easing.OutBack
            easing.overshoot: 1.2
        }

        NumberAnimation {
            id: bounceY
            target: root
            property: "y"
            duration: 250
            easing.type: Easing.OutBack
            easing.overshoot: 1.2
        }

        // 垂直三区块布局
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            // 区块 1：头部 - 最近 7 天时间线滑动选择器（含滚轮平滑滚动，已移除回弹）
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 75
                color: "#FFFFFF"
                radius: 16

                WheelHandler {
                    target: timelineView
                    onWheel: (event) => {
                        var rawDelta = event.pixelDelta.x !== 0 ? event.pixelDelta.x : (event.angleDelta.x !== 0 ? event.angleDelta.x : event.angleDelta.y)
                        var step = event.pixelDelta.x !== 0 ? rawDelta : (rawDelta / 120.0 * 45.0)

                        var minX = 0
                        var maxX = Math.max(0, timelineView.contentWidth - timelineView.width)
                        var currentX = timelineView.contentX
                        var rawDestX = currentX - step
                        var destX = Math.max(minX, Math.min(maxX, rawDestX))

                        smoothScrollX.from = currentX
                        smoothScrollX.to = destX
                        smoothScrollX.restart()
                    }
                }

                NumberAnimation {
                    id: smoothScrollX
                    target: timelineView
                    property: "contentX"
                    duration: 100
                    easing.type: Easing.OutCubic
                }

                ListView {
                    id: timelineView
                    anchors.fill: parent
                    anchors.margins: 8
                    orientation: ListView.Horizontal
                    clip: true
                    model: 7
                    spacing: 8
                    currentIndex: 6
                    interactive: true

                    onWidthChanged: {
                        positionViewAtIndex(currentIndex, ListView.Contain)
                    }
                    onCurrentIndexChanged: {
                        positionViewAtIndex(currentIndex, ListView.Contain)
                    }

                    delegate: Item {
                        width: 76
                        height: 55

                        property bool isToday: (index === 6)
                        property bool isSelected: (timelineView.currentIndex === index)

                        property var targetDate: {
                            var d = new Date()
                            d.setDate(d.getDate() - (6 - index))
                            return d
                        }
                        property string dayStr: ["周日", "周一", "周二", "周三", "周四", "周五", "周六"][targetDate.getDay()]
                        property string dateStr: (targetDate.getMonth() + 1) + "/" + targetDate.getDate()

                        Rectangle {
                            anchors.fill: parent
                            radius: 12
                            color: isSelected ? "#FFF176" : (isToday ? "#E1F5FE" : "#F7F7FA")

                            Behavior on color { ColorAnimation { duration: 200 } }

                            Column {
                                anchors.centerIn: parent
                                spacing: 4

                                Text {
                                    text: dayStr
                                    font.pixelSize: 15
                                    font.bold: true
                                    font.family: "Segoe UI"
                                    color: isSelected ? "#7A5810" : (isToday ? "#00BCD4" : "#999999")
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }

                                Text {
                                    text: dateStr
                                    font.pixelSize: 13
                                    font.bold: true
                                    font.family: "Segoe UI"
                                    color: isSelected ? "#1A1A1A" : (isToday ? "#00BCD4" : "#333333")
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    timelineView.currentIndex = index
                                }
                            }
                        }
                    }
                }
            }

            // 区块 2：主体 - 交互式每日任务列表区域（含滚轮平滑滚动，已移除回弹）
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 50
                color: "#FFFFFF"
                radius: 16

                WheelHandler {
                    target: taskListView
                    onWheel: (event) => {
                        var rawDelta = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y
                        var step = event.pixelDelta.y !== 0 ? rawDelta : (rawDelta / 120.0 * 45.0)

                        var minY = 0
                        var maxY = Math.max(0, taskListView.contentHeight - taskListView.height)
                        var currentY = taskListView.contentY
                        var rawDestY = currentY - step
                        var destY = Math.max(minY, Math.min(maxY, rawDestY))

                        smoothScrollY.from = currentY
                        smoothScrollY.to = destY
                        smoothScrollY.restart()
                    }
                }

                NumberAnimation {
                    id: smoothScrollY
                    target: taskListView
                    property: "contentY"
                    duration: 100
                    easing.type: Easing.OutCubic
                }

                ListView {
                    id: taskListView
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true
                    spacing: 6
                    interactive: true

                    model: ListModel {
                        id: taskModel
                        ListElement { taskText: "复习日语复合动词语义合成性论文"; isCompleted: true }
                        ListElement { taskText: "编写 DailyUp 的 QML 界面逻辑"; isCompleted: true }
                        ListElement { taskText: "接入 C++ 数据库持久化存储模块"; isCompleted: false }
                        ListElement { taskText: "调整窗口拖拽边界与缩放约束"; isCompleted: false }
                    }

                    delegate: Item {
                        width: taskListView.width
                        height: 38

                        required property int index
                        required property string taskText
                        required property bool isCompleted

                        // 右滑露出：置顶 + 编辑
                        Row {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            height: 38
                            spacing: 0

                            Rectangle {
                                width: 56; height: 38; radius: 8
                                color: "#4A90E2"
                                Text {
                                    anchors.centerIn: parent
                                    text: "置顶"
                                    color: "#FFFFFF"
                                    font.pixelSize: 12
                                    font.family: "宋体"
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        taskModel.move(index, 0, 1)
                                        contentItem.x = 0
                                    }
                                }
                            }

                            Rectangle {
                                width: 56; height: 38; radius: 8
                                color: "#FFF176"
                                Text {
                                    anchors.centerIn: parent
                                    text: "编辑"
                                    color: "#1A1A1A"
                                    font.pixelSize: 12
                                    font.family: "宋体"
                                    font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        editInput.text = taskText
                                        editIndex.value = index
                                        editPopup.visible = true
                                        contentItem.x = 0
                                    }
                                }
                            }
                        }

                        // 左滑露出：删除
                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            height: 38

                            Rectangle {
                                width: 56; height: 38; radius: 8
                                color: "#FF5252"
                                Text {
                                    anchors.centerIn: parent
                                    text: "删除"
                                    color: "#FFFFFF"
                                    font.pixelSize: 12
                                    font.family: "宋体"
                                    font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        taskModel.remove(index)
                                    }
                                }
                            }
                        }

                        Rectangle {
                            id: contentItem
                            width: parent.width
                            height: 38
                            color: isCompleted ? "#FAFAFA" : "#F9F9FB"
                            radius: 8
                            x: 0

                            Behavior on x {
                                enabled: !dragHandler.active
                                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 18
                                    radius: 4
                                    color: isCompleted ? "#4A90E2" : "transparent"
                                    border.color: isCompleted ? "#4A90E2" : "#CCCCCC"
                                    border.width: 1.5

                                    Behavior on color { ColorAnimation { duration: 150 } }

                                    Text {
                                        anchors.centerIn: parent
                                        text: "✓"
                                        font.pixelSize: 11
                                        font.bold: true
                                        color: "#FFFFFF"
                                        opacity: isCompleted ? 1 : 0
                                        Behavior on opacity { NumberAnimation { duration: 150 } }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: taskText
                                    font.pixelSize: 13
                                    font.family: "Segoe UI"
                                    color: isCompleted ? "#AAAAAA" : "#333333"
                                    font.strikeout: isCompleted
                                    elide: Text.ElideRight
                                }
                            }

                            DragHandler {
                                id: dragHandler
                                target: contentItem
                                yAxis.enabled: false
                                xAxis.minimum: -56
                                xAxis.maximum: 112

                                onActiveChanged: {
                                    if (!active) {
                                        if (contentItem.x < -28) {
                                            contentItem.x = -56
                                        } else if (contentItem.x > 84) {
                                            contentItem.x = 112
                                        } else if (contentItem.x > 28) {
                                            contentItem.x = 56
                                        } else {
                                            contentItem.x = 0
                                        }
                                    }
                                }
                            }

                            TapHandler {
                                onTapped: {
                                    if (contentItem.x === 0) {
                                        taskModel.setProperty(index, "isCompleted", !isCompleted)
                                    } else {
                                        contentItem.x = 0
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 区块 3：底部 - 添加任务操作栏
            Rectangle {
                id: bottomTaskBlock
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: "#FFFFFF"
                radius: 16

                Text {
                    anchors.centerIn: parent
                    text: "+ Add New Task"
                    font.pixelSize: 15
                    font.family: "Segoe UI"
                    color: "#4A90E2"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        addPopup.visible = true
                        newTaskInput.text = ""
                        newTaskInput.forceActiveFocus()
                    }
                }
            }
        }

        // 弹窗 1：新增任务面板
        Rectangle {
            id: addPopup
            visible: false
            width: Math.min(270, root.width - 32)
            height: addLayout.implicitHeight + 32
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 25
            color: "#FFFFFF"
            radius: 20
            border.color: "#E1E1E6"
            border.width: 1
            z: 100

            ColumnLayout {
                id: addLayout
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Text {
                    text: "添加新任务"
                    font.bold: true
                    font.pixelSize: 14
                    color: "#333333"
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(54, addFlick.contentHeight + 16)
                    Layout.maximumHeight: 120
                    color: "#F7F7FA"
                    radius: 10
                    border.color: "#E1E1E6"

                    Flickable {
                        id: addFlick
                        anchors.fill: parent
                        anchors.margins: 4
                        contentWidth: width
                        contentHeight: newTaskInput.contentHeight
                        clip: true

                        TextEdit {
                            id: newTaskInput
                            width: addFlick.width
                            font.pixelSize: 13
                            font.family: "Segoe UI"
                            color: "#333333"
                            focus: true
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            topPadding: 8
                            bottomPadding: 8
                            leftPadding: 8
                            rightPadding: 8
                        }
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: 8

                    Rectangle {
                        width: 60; height: 28
                        radius: 8
                        color: "#EFEFF4"
                        Text {
                            anchors.centerIn: parent
                            text: "取消"
                            font.pixelSize: 12
                            font.family: "宋体"
                            color: "#666666"
                        }
                        MouseArea { anchors.fill: parent; onClicked: addPopup.visible = false }
                    }

                    Rectangle {
                        width: 60; height: 28
                        radius: 8
                        color: "#4A90E2"
                        Text {
                            anchors.centerIn: parent
                            text: "确定"
                            font.pixelSize: 12
                            font.family: "宋体"
                            color: "#FFFFFF"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                var trimmedText = newTaskInput.text.trim()
                                if (trimmedText !== "") {
                                    taskModel.append({"taskText": trimmedText, "isCompleted": false})
                                    addPopup.visible = false
                                }
                            }
                        }
                    }
                }
            }
        }

        // 弹窗 2：修改任务面板
        QtObject {
            id: editIndex
            property int value: 0
        }

        Rectangle {
            id: editPopup
            visible: false
            width: Math.min(270, root.width - 32)
            height: editLayout.implicitHeight + 32
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 25
            color: "#FFFFFF"
            radius: 20
            border.color: "#E1E1E6"
            border.width: 1
            z: 100

            ColumnLayout {
                id: editLayout
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Text {
                    text: "修改任务"
                    font.bold: true
                    font.pixelSize: 14
                    color: "#333333"
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(54, editFlick.contentHeight + 16)
                    Layout.maximumHeight: 120
                    color: "#F7F7FA"
                    radius: 10
                    border.color: "#E1E1E6"

                    Flickable {
                        id: editFlick
                        anchors.fill: parent
                        anchors.margins: 4
                        contentWidth: width
                        contentHeight: editInput.contentHeight
                        clip: true

                        TextEdit {
                            id: editInput
                            width: editFlick.width
                            font.pixelSize: 13
                            font.family: "Segoe UI"
                            color: "#333333"
                            focus: true
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            topPadding: 8
                            bottomPadding: 8
                            leftPadding: 8
                            rightPadding: 8
                        }
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: 8

                    Rectangle {
                        width: 60; height: 28
                        radius: 8
                        color: "#EFEFF4"
                        Text {
                            anchors.centerIn: parent
                            text: "取消"
                            font.pixelSize: 12
                            font.family: "宋体"
                            color: "#666666"
                        }
                        MouseArea { anchors.fill: parent; onClicked: editPopup.visible = false }
                    }

                    Rectangle {
                        width: 60; height: 28
                        radius: 8
                        color: "#4A90E2"
                        Text {
                            anchors.centerIn: parent
                            text: "确定"
                            font.pixelSize: 12
                            font.family: "宋体"
                            color: "#FFFFFF"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                var trimmedText = editInput.text.trim()
                                if (trimmedText !== "") {
                                    taskModel.setProperty(editIndex.value, "taskText", trimmedText)
                                    editPopup.visible = false
                                }
                            }
                        }
                    }
                }
            }
        }

        // 右下角无感缩放把手
        MouseArea {
            width: 20
            height: 20
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            cursorShape: Qt.SizeFDiagCursor
            onPressed: (mouse) => {
                root.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
            }
        }
    }
}