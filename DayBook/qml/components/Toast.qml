import QtQuick
import QtQuick.Effects
import DayBook

Item {
    id: root

    property string message: ""
    property string kind: "success"
    property bool shown: false
    property color iconColor: kind === "error" ? Theme.danger : kind === "info" ? Theme.accent : Theme.success
    property string iconName: kind === "error" ? "error" : kind === "info" ? "info" : "check_circle"

    width: pill.width
    height: 44
    opacity: shown ? 1 : 0
    scale: shown ? 1 : 0.97
    visible: opacity > 0.001
    z: 300

    Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

    Timer {
        id: hideTimer
        interval: 2400
        onTriggered: root.shown = false
    }

    function show(text, type) {
        root.message = text
        root.kind = type === undefined ? "success" : type
        root.shown = true
        hideTimer.restart()
    }

    RectangularShadow {
        anchors.fill: pill
        radius: pill.radius
        color: Qt.rgba(0, 0, 0, 0.30)
        blur: 24
        offset: Qt.vector2d(0, 8)
        spread: -2
    }

    Rectangle {
        id: pill
        anchors.horizontalCenter: parent.horizontalCenter
        height: 44
        width: Math.min(440, row.implicitWidth + 32)
        radius: 22
        color: Theme.dark ? Qt.rgba(0.11, 0.14, 0.17, 0.96) : Qt.rgba(0.16, 0.14, 0.12, 0.94)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.10)

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 9
            Icon {
                name: root.iconName
                size: 18
                iconColor: root.iconColor
            }
            Text {
                text: root.message
                color: "#FCF8F0"
                font.family: Theme.fontFamily
                font.pixelSize: 13
                font.weight: 500
                elide: Text.ElideRight
                width: Math.min(390, implicitWidth)
            }
        }
    }
}
