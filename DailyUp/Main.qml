import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import DailyUp 1.0

Window {
    id: root

    property string currentDate: ""
    property int dateRefreshKey: 0

    property date todayDate: new Date()
    TaskManager {
        id: taskManager
    }
    // 新增：监听跨日信号
    Connections {
        target: taskManager

        function onDateRollover() {

            root.todayDate = new Date()

            timelineView.positionViewAtIndex(
                6,
                ListView.Contain
            )

        }
    }

    width: 320
    height: 600
    minimumWidth: 292
    maximumWidth: 648
    minimumHeight: 265
    visible: true
    title: qsTr("DailyUp")
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"

    Component.onCompleted: {
        var today = new Date()
        var dateString =
                today.getFullYear() + "-" +
                String(today.getMonth() + 1).padStart(2, "0") + "-" +
                String(today.getDate()).padStart(2, "0")

        root.currentDate = dateString
        taskManager.loadTasks(dateString)
    }

    Rectangle {
        id: mainPanel
        anchors.fill: parent
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
            z: 100

            Text {
                anchors.centerIn: parent
                text: "×"
                font.pixelSize: 16
                font.bold: true
                color: "#FFFFFF"
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
            z: 100

            Text {
                anchors.centerIn: parent
                text: "-"
                font.pixelSize: 18
                font.bold: true
                color: "#FFFFFF"
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
                      startMousePos = Qt.point(mouse.x, mouse.y)             }
             onPositionChanged: (mouse) => {
                  if (pressed) {
                        var dx = mouse.x - startMousePos.x
                       var dy = mouse.y - startMousePos.y
                         root.x += dx
                    root.y += dy                 }
          }
         onReleased: {
                var screenW = Screen.desktopAvailableWidth
                var screenH = Screen.desktopAvailableHeight
                    if (root.x < 0) {
                        bounceX.to = 0
                        bounceX.start()
             }
                    else if (root.x + root.width > screenW) {
                        bounceX.to = screenW - root.width
                        bounceX.start()                 }
        if (root.y < 0) {                     bounceY.to = 0
            bounceY.start()                 }
        else if (root.y + root.height > screenH) {
        bounceY.to = screenH - root.height
        bounceY.start()                 }             }         }
        NumberAnimation {
            id: bounceX
            target: root
            property: "x"
            duration: 250
            easing.type: Easing.OutBack
            easing.overshoot: 1.2         }
        NumberAnimation {
            id: bounceY
            target: root
            property: "y"
            duration: 250
            easing.type: Easing.OutBack
            easing.overshoot: 1.2         }

        // 垂直三区块布局
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 75
                color: "#FFFFFF"
                radius: 16

                ListView {
                    id: timelineView
                    anchors.fill: parent
                    anchors.margins: 8
                    orientation: ListView.Horizontal
                    clip: true
                    model: 7
                    spacing: 8
                    currentIndex: 6

                    onWidthChanged: {
                        positionViewAtIndex(currentIndex, ListView.Contain)
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        onWheel: (wheel) => {
                            var delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                            if (delta > 0) {
                                timelineView.flick(600, 0)
                            } else {
                                timelineView.flick(-600, 0)
                            }
                        }
                    }

                    delegate: Item {
                        width: 76
                        height: 55
                        property bool isToday:
                            databaseDateStr === Qt.formatDate(root.todayDate, "yyyy-MM-dd")
                        property bool isSelected: timelineView.currentIndex === index

                        property var targetDate: {
                            var d = new Date(root.todayDate)
                            d.setDate(
                                d.getDate() - (6 - index)
                            )
                            return d
                        }

                        property string dayStr:
                            [
                                "周日",
                                "周一",
                                "周二",
                                "周三",
                                "周四",
                                "周五",
                                "周六"
                            ][targetDate.getDay()]

                        property string dateStr:
                            (targetDate.getMonth() + 1) + "/" + targetDate.getDate()

                        property string databaseDateStr:
                            targetDate.getFullYear() + "-" +
                            String(targetDate.getMonth() + 1).padStart(2, "0") + "-" +
                            String(targetDate.getDate()).padStart(2, "0")

                        Rectangle {
                            anchors.fill: parent
                            radius: 12

                            color:
                                isSelected ?
                                "#FFF176" :
                                (isToday ? "#E1F5FE" : "#F7F7FA")

                            Column {
                                anchors.centerIn: parent
                                spacing: 4

                                Text {
                                    text: dayStr
                                    font.pixelSize: 15
                                    font.bold: true
                                }

                                Text {
                                    text: dateStr
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                            }

                            MouseArea {
                                anchors.fill: parent

                                onClicked: {
                                    timelineView.currentIndex = index
                                    root.currentDate = databaseDateStr
                                    taskManager.loadTasks(databaseDateStr)
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#FFFFFF"
                radius: 16

                ListView {
                    id: taskListView

                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true
                    spacing: 6

                    model: taskManager

                    delegate: Item {
                        width: taskListView.width
                        height: 38

                        required property int taskId
                        required property string taskContent
                        required property bool isCompleted
                        required property bool isPinned
                        required property int index


                        Row {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            height: 38

                            Rectangle {
                                width: 56
                                height: 38
                                radius: 8
                                color: "#4A90E2"

                                Text {
                                    anchors.centerIn: parent
                                    text: "置顶"
                                    color: "#FFFFFF"
                                    font.pixelSize: 12
                                }

                                MouseArea {
                                    anchors.fill: parent

                                    onClicked: {
                                        taskManager.toggleTaskPin(index)
                                        contentItem.x = 0
                                    }
                                }
                            }

                            Rectangle {
                                width: 56
                                height: 38
                                radius: 8
                                color: "#FFF176"

                                Text {
                                    anchors.centerIn: parent
                                    text: "编辑"
                                    color: "#1A1A1A"
                                    font.pixelSize: 12
                                }

                                MouseArea {
                                    anchors.fill: parent

                                    onClicked: {
                                        editInput.text = taskContent
                                        editIndex.value = index
                                        editPopup.visible = true
                                        editInput.forceActiveFocus()
                                        contentItem.x = 0
                                    }
                                }
                            }
                        }


                        Row {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            height: 38

                            Rectangle {
                                width: 56
                                height: 38
                                radius: 8
                                color: "#FF5252"

                                Text {
                                    anchors.centerIn: parent
                                    text: "删除"
                                    color: "#FFFFFF"
                                    font.pixelSize: 12
                                }

                                MouseArea {
                                    anchors.fill: parent

                                    onClicked: {
                                        taskManager.deleteTask(index)
                                    }
                                }
                            }
                        }


                        Rectangle {
                            id: contentItem

                            width: parent.width
                            height: 38

                            radius: 8

                            color:
                                isCompleted ?
                                "#FAFAFA" :
                                "#F9F9FB"


                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 10


                                Rectangle {
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 18

                                    radius: 4

                                    color:
                                        isCompleted ?
                                        "#4A90E2" :
                                        "transparent"

                                    border.color:
                                        isCompleted ?
                                        "#4A90E2" :
                                        "#CCCCCC"


                                    Text {
                                        anchors.centerIn: parent

                                        text: "✓"

                                        color: "#FFFFFF"

                                        opacity:
                                            isCompleted ? 1 : 0
                                    }
                                }


                                Text {
                                    Layout.fillWidth: true

                                    text: taskContent

                                    font.pixelSize: 13

                                    color:
                                        isCompleted ?
                                        "#AAAAAA" :
                                        "#333333"

                                    font.strikeout:
                                        isCompleted

                                    elide:
                                        Text.ElideRight
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
                                        }
                                        else if (contentItem.x > 84) {
                                            contentItem.x = 112
                                        }
                                        else if (contentItem.x > 28) {
                                            contentItem.x = 56
                                        }
                                        else {
                                            contentItem.x = 0
                                        }
                                    }
                                }
                            }


                            TapHandler {

                                onTapped: {

                                    if (contentItem.x === 0) {
                                        taskManager.toggleTaskStatus(index)
                                    }
                                    else {
                                        contentItem.x = 0
                                    }
                                }
                            }
                        }
                    }
                }
            }


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

            ColumnLayout {
                id: addLayout
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Text {
                    text: "添加新任务"
                    font.bold: true
                    font.pixelSize: 14
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    color: "#F7F7FA"
                    radius: 10

                    TextInput {
                        id: newTaskInput
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        verticalAlignment: TextInput.AlignVCenter
                        font.pixelSize: 13
                        onAccepted: {
                            var text = newTaskInput.text.trim()
                            if (text !== "") {
                                taskManager.addTask(root.currentDate, text)
                                addPopup.visible = false
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: 8
                    Rectangle {
                        width: 60
                        height: 28
                        radius: 8
                        color: "#EFEFF4"

                        Text {
                            anchors.centerIn: parent
                            text: "取消"
                            font.pixelSize: 12
                        }

                        MouseArea {
                            anchors.fill: parent

                            onClicked: {
                                addPopup.visible = false
                            }
                        }
                    }


                    Rectangle {
                        width: 60
                        height: 28
                        radius: 8
                        color: "#4A90E2"
                        Text {
                            anchors.centerIn: parent
                            text: "确定"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                var text =
                                        newTaskInput.text.trim()

                                if (text !== "") {

                                    taskManager.addTask(
                                        root.currentDate,
                                        text
                                    )

                                    addPopup.visible = false
                                }
                            }
                        }
                    }
                }
            }
        }


        QtObject {
            id: editIndex
            property int value: 0
        }


        Rectangle {
            id: editPopup
            visible: false
            width: Math.min(
                       270,
                       root.width - 32
                   )
            height:
                editLayout.implicitHeight + 32
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 25

            color: "#FFFFFF"
            radius: 20
            border.color: "#E1E1E6"
            border.width: 1

            ColumnLayout {
                id: editLayout
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12
                Text {
                    text: "修改任务"
                    font.bold: true
                    font.pixelSize: 14
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    color: "#F7F7FA"
                    radius: 10

                    TextInput {
                        id: editInput
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        verticalAlignment: TextInput.AlignVCenter
                        font.pixelSize: 13
                        onAccepted: {
                            var text = editInput.text.trim()
                            if (text !== "") {
                                taskManager.updateTaskContent(editIndex.value, text)
                                editPopup.visible = false
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.alignment:
                        Qt.AlignRight
                    spacing: 8
                    Rectangle {
                        width: 60
                        height: 28
                        radius: 8
                        color: "#EFEFF4"

                        Text {
                            anchors.centerIn: parent
                            text: "取消"
                            font.pixelSize: 12
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                editPopup.visible = false
                            }
                        }
                    }


                    Rectangle {
                        width: 60
                        height: 28
                        radius: 8
                        color: "#4A90E2"
                        Text {
                            anchors.centerIn: parent
                            text: "确定"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                var text =
                                        editInput.text.trim()
                                if (text !== "") {
                                    taskManager.updateTaskContent(
                                        editIndex.value,
                                        text
                                    )
                                    editPopup.visible = false
                                }
                            }
                        }
                    }
                }
            }
        }


        MouseArea {
            width: 20
            height: 20
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            cursorShape:
                Qt.SizeFDiagCursor

            onPressed: {
                root.startSystemResize(
                    Qt.RightEdge |
                    Qt.BottomEdge
                )
            }
        }
    }
}