import QtQuick
import DayBook

Row {
    id: root

    property bool windowActive: true
    signal closeClicked()
    signal minimizeClicked()
    signal maximizeClicked()

    spacing: 8

    Repeater {
        model: 3
        Item {
            width: 12
            height: 12
            Rectangle {
                anchors.fill: parent
                radius: 6
                color: index === 0 ? "#FF5F57" : index === 1 ? "#FEBC2E" : "#28C840"
                opacity: root.windowActive ? 1 : 0.42
                border.width: 1
                border.color: Theme.withAlpha("#000000", root.windowActive ? 0.14 : 0.05)
                Behavior on opacity { NumberAnimation { duration: 140 } }
            }
            Icon {
                anchors.centerIn: parent
                name: index === 0 ? "close" : index === 1 ? "remove" : "fullscreen"
                size: index === 2 ? 6 : 7
                weight: 600
                iconColor: index === 0 ? "#7A0B05" : index === 1 ? "#7A4A00" : "#075A13"
                opacity: mouse.containsMouse && root.windowActive ? 0.9 : 0
                Behavior on opacity { NumberAnimation { duration: 100 } }
            }
            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (index === 0) root.closeClicked()
                    else if (index === 1) root.minimizeClicked()
                    else root.maximizeClicked()
                }
            }
        }
    }
}
