import QtQuick
import DayBook

Item {
    id: root

    property string label: ""
    property string icon: "home"
    property bool active: false
    property string badge: ""
    signal clicked()

    height: 42
    width: parent ? parent.width : 200

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: 12
        color: root.active ? Theme.accentWash : (mouse.containsMouse ? Theme.surfaceSunken : "transparent")
        border.width: root.active ? 1 : 0
        border.color: Theme.withAlpha(Theme.accent, 0.12)
        Behavior on color { ColorAnimation { duration: 130 } }
    }

    Rectangle {
        width: 3
        height: 16
        radius: 1.5
        x: -7
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.accent
        opacity: root.active ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 130 } }
    }

    Icon {
        id: iconItem
        anchors.left: parent.left
        anchors.leftMargin: 13
        anchors.verticalCenter: parent.verticalCenter
        name: root.icon
        size: 19
        iconColor: root.active ? Theme.accent : Theme.muted
        weight: root.active ? 500 : 400
        Behavior on color { ColorAnimation { duration: 130 } }
    }

    Text {
        anchors.left: iconItem.right
        anchors.leftMargin: 11
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: root.label
        color: root.active ? Theme.ink : Theme.inkSecondary
        font.family: Theme.fontFamily
        font.pixelSize: 13
        font.weight: root.active ? 600 : 500
        elide: Text.ElideRight
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
        Accessible.role: Accessible.Button
        Accessible.name: root.label
    }
}
