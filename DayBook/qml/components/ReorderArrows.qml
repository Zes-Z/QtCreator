import QtQuick
import DayBook

Column {
    id: root

    property bool canUp: true
    property bool canDown: true
    signal moveUp()
    signal moveDown()

    width: 18
    spacing: 0

    Item {
        width: root.width
        height: 14
        opacity: root.canUp ? 0.72 : 0.18

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: upMouse.containsMouse ? Theme.surfaceRaised : "transparent"
        }
        Icon { anchors.centerIn: parent; name: "arrow_upward"; size: 11; iconColor: Theme.muted }
        MouseArea {
            id: upMouse
            anchors.fill: parent
            enabled: root.canUp
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.moveUp()
        }
    }

    Item {
        width: root.width
        height: 14
        opacity: root.canDown ? 0.72 : 0.18

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: downMouse.containsMouse ? Theme.surfaceRaised : "transparent"
        }
        Icon { anchors.centerIn: parent; name: "arrow_downward"; size: 11; iconColor: Theme.muted }
        MouseArea {
            id: downMouse
            anchors.fill: parent
            enabled: root.canDown
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.moveDown()
        }
    }
}
