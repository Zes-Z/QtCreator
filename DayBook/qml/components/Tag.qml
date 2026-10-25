import QtQuick
import DayBook

Rectangle {
    id: root

    property string text: ""
    property string icon: ""
    property color tagColor: Theme.muted
    property color bgColor: Theme.withAlpha(Theme.ink, 0.05)
    property int fontSize: 10

    implicitWidth: row.implicitWidth + 16
    implicitHeight: 22
    radius: 11
    color: bgColor
    border.width: 0

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 4
        Icon {
            visible: root.icon !== ""
            name: root.icon
            size: 12
            iconColor: root.tagColor
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.text
            color: root.tagColor
            font.family: Theme.fontFamily
            font.pixelSize: root.fontSize
            font.weight: 600
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
