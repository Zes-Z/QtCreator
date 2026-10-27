import QtQuick
import QtQuick.Layouts
import DailyUp

// 单条任务。
// 左滑露出「删除」，右滑露出「置顶 / 编辑」，与旧版手势保持一致。
Item {
    id: row

    width: ListView.view ? ListView.view.width : 0
    height: 45

    required property int taskId
    required property string taskContent
    required property bool isCompleted
    required property bool isPinned
    required property int index

    property var manager

    signal editRequested(int index, string content)

    HoverHandler {
        id: rowHover
        cursorShape: Qt.PointingHandCursor
    }

    // 滑动时露出的操作层
    Row {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6
        visible: contentItem.x > 0.5

        Rectangle {
            width: 62
            height: 38
            radius: 11
            color: Theme.accentSoft
            border.width: 1
            border.color: Theme.alpha(Theme.accent, 0.18)

            Text {
                anchors.centerIn: parent
                text: row.isPinned ? "取消置顶" : "置顶"
                font.pixelSize: 11
                font.weight: Font.Medium
                color: Theme.accent
            }

            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler {
                onTapped: {
                    contentItem.x = 0
                    if (row.manager)
                        row.manager.toggleTaskPin(row.index)
                }
            }
        }

        Rectangle {
            width: 62
            height: 38
            radius: 11
            color: Theme.warningSoft

            Text {
                anchors.centerIn: parent
                text: "编辑"
                font.pixelSize: 11
                font.weight: Font.Medium
                color: Theme.warning
            }

            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler {
                onTapped: {
                    contentItem.x = 0
                    row.editRequested(row.index, row.taskContent)
                }
            }
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 62
        height: 38
        radius: 11
        color: Theme.dangerSoft
        visible: contentItem.x < -0.5

        Text {
            anchors.centerIn: parent
            text: "删除"
            font.pixelSize: 11
            font.weight: Font.Medium
            color: Theme.danger
        }

        HoverHandler { cursorShape: Qt.PointingHandCursor }
        TapHandler {
            onTapped: {
                if (row.manager)
                    row.manager.deleteTask(row.index)
            }
        }
    }

    Rectangle {
        id: contentItem

        width: row.width
        height: row.height
        radius: 12

        color: row.isCompleted
               ? Theme.doneTint
               : (rowHover.hovered ? Theme.chip : Theme.alpha(Theme.chip, 0))

        Behavior on color { ColorAnimation { duration: 140 } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 12
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                Layout.alignment: Qt.AlignVCenter
                radius: 10

                color: row.isCompleted ? Theme.success : Theme.alpha(Theme.success, 0)
                border.width: row.isCompleted ? 0 : 1.5
                border.color: row.isCompleted ? "transparent" : Theme.alpha(Theme.ink, 0.22)
                Behavior on color { ColorAnimation { duration: 150 } }

                Text {
                    anchors.centerIn: parent
                    text: "✓"
                    font.pixelSize: 11
                    font.bold: true
                    color: "#FFFFFF"
                    opacity: row.isCompleted ? 1 : 0
                    scale: row.isCompleted ? 1 : 0.55
                    Behavior on opacity { NumberAnimation { duration: 130 } }
                    Behavior on scale { NumberAnimation { duration: 170; easing.type: Easing.OutBack } }
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: row.taskContent
                font.pixelSize: 14
                font.weight: row.isPinned ? Font.Bold : Font.Medium
                color: row.isCompleted ? Theme.faint : Theme.ink
                font.strikeout: row.isCompleted
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            Rectangle {
                visible: row.isPinned
                Layout.preferredWidth: 3
                Layout.preferredHeight: 14
                Layout.alignment: Qt.AlignVCenter
                radius: 1.5
                color: Theme.accent
            }
        }

        DragHandler {
            target: contentItem
            xAxis.enabled: true
            yAxis.enabled: false
            xAxis.minimum: -62
            xAxis.maximum: 130

            onActiveChanged: {
                if (active)
                    return
                const x = contentItem.x
                let destination = 0
                if (x > 65)
                    destination = 130
                else if (x < -31)
                    destination = -62
                snapX.stop()
                snapX.from = x
                snapX.to = destination
                snapX.start()
            }
        }

        NumberAnimation {
            id: snapX
            target: contentItem
            property: "x"
            duration: 190
            easing.type: Easing.OutCubic
        }

        TapHandler {
            onTapped: {
                if (contentItem.x !== 0) {
                    snapX.stop()
                    snapX.from = contentItem.x
                    snapX.to = 0
                    snapX.start()
                } else if (row.manager) {
                    row.manager.toggleTaskStatus(row.index)
                }
            }
        }
    }
}