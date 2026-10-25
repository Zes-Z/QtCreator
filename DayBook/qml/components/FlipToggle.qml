import QtQuick
import DayBook

Rectangle {
    id: root

    property string label: ""
    signal clicked()

    implicitWidth: toggleRow.implicitWidth + 18
    implicitHeight: 26
    radius: 9
    color: toggleMouse.containsMouse ? Theme.surfaceRaised : Theme.surfaceSunken
    border.width: 1
    border.color: toggleMouse.containsMouse ? Theme.withAlpha(Theme.accent, 0.28) : Theme.hairline
    Behavior on color { ColorAnimation { duration: 120 } }

    Row {
        id: toggleRow
        anchors.centerIn: parent
        spacing: 4
        Icon {
            name: "sync"
            size: 12
            iconColor: toggleMouse.containsMouse ? Theme.accent : Theme.muted
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.label
            color: toggleMouse.containsMouse ? Theme.ink : Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: 9
            font.weight: 500
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: toggleMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
