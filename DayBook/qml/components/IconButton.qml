import QtQuick
import DayBook

Item {
    id: root

    property string icon: "more_horiz"
    property int iconSize: 18
    property int size: 34
    property bool soft: false
    property color iconColor: Theme.ink
    property color hoverColor: Theme.surfaceSunken
    signal clicked()

    implicitWidth: size
    implicitHeight: size
    opacity: enabled ? 1 : 0.4

    Rectangle {
        anchors.fill: parent
        radius: Math.min(width, height) / 2
        color: root.soft ? Theme.surfaceRaised : (mouse.containsMouse ? root.hoverColor : "transparent")
        border.width: root.soft ? 1 : 0
        border.color: Theme.cardBorder
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    Icon {
        anchors.centerIn: parent
        name: root.icon
        size: root.iconSize
        iconColor: root.iconColor
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
        Accessible.role: Accessible.Button
    }
}
