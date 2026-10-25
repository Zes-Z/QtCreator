import QtQuick
import DayBook

Item {
    id: root

    property string icon: "receipt_long"
    property string title: "还没有记录"
    property string description: "点击下方按钮，记下今天的第一笔。"
    property string actionText: ""
    property string actionIcon: "add"
    signal actionClicked()

    implicitHeight: column.implicitHeight

    Column {
        id: column
        width: parent.width
        spacing: 10
        anchors.verticalCenter: parent.verticalCenter

        Rectangle {
            width: 64
            height: 64
            radius: 22
            color: Theme.accentWash
            border.width: 1
            border.color: Theme.withAlpha(Theme.accent, 0.15)
            anchors.horizontalCenter: parent.horizontalCenter
            Icon {
                anchors.centerIn: parent
                name: root.icon
                size: 30
                iconColor: Theme.accent
            }
        }
        Text {
            width: parent.width
            text: root.title
            color: Theme.ink
            font.family: Theme.fontFamily
            font.pixelSize: 15
            font.weight: 600
            horizontalAlignment: Text.AlignHCenter
        }
        Text {
            width: Math.min(300, parent.width)
            text: root.description
            color: Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: 12
            lineHeight: 1.45
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Item {
            width: 1
            height: 6
            visible: root.actionText !== ""
        }
        AppButton {
            visible: root.actionText !== ""
            text: root.actionText
            icon: root.actionIcon
            compact: true
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: root.actionClicked()
        }
    }
}
