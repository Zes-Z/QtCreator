import QtQuick
import QtQuick.Effects
import DayBook

Item {
    id: root

    property string text
    property string icon
    property string kind: "primary"
    property bool compact: false
    property real buttonRadius: Theme.buttonRadius
    signal clicked()

    implicitHeight: compact ? 32 : 38
    implicitWidth: row.implicitWidth + (compact ? 24 : 34)
    opacity: enabled ? 1 : 0.45
    scale: area.pressed && enabled ? 0.975 : 1
    Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }

    RectangularShadow {
        anchors.fill: bg
        radius: root.buttonRadius
        visible: root.kind === "primary" && root.enabled
        color: Theme.withAlpha(Theme.accent, 0.32)
        blur: 18
        offset: Qt.vector2d(0, 7)
        spread: -1
    }

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: root.buttonRadius
        color: {
            if (root.kind === "primary") return Theme.accentDeep
            if (root.kind === "secondary") return Theme.surfaceRaised
            if (root.kind === "danger") return Theme.dangerSoft
            if (root.kind === "warning") return Theme.withAlpha(Theme.warning, 0.14)
            return "transparent"
        }
        border.width: root.kind === "ghost" ? 0 : 1
        border.color: root.kind === "primary" ? Theme.withAlpha("#FFFFFF", 0.14)
                                            : root.kind === "danger" ? Theme.withAlpha(Theme.danger, 0.22)
                                            : Theme.cardBorder
        Behavior on color { ColorAnimation { duration: 140 } }
    }

    Rectangle {
        visible: root.kind === "primary"
        anchors.fill: parent
        radius: root.buttonRadius
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.accent }
            GradientStop { position: 1; color: Theme.withAlpha(Theme.accent, 0.0) }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.buttonRadius
        color: Theme.withAlpha("#FFFFFF", 0.08)
        opacity: area.containsMouse && root.enabled ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 120 } }
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6
        Icon {
            visible: root.icon !== ""
            name: root.icon
            size: root.compact ? 15 : 17
            iconColor: {
                if (root.kind === "primary") return Theme.accentContrast
                if (root.kind === "danger") return Theme.danger
                if (root.kind === "warning") return Theme.warning
                return Theme.ink
            }
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: root.text
            visible: root.text !== ""
            color: {
                if (root.kind === "primary") return Theme.accentContrast
                if (root.kind === "danger") return Theme.danger
                if (root.kind === "warning") return Theme.warning
                return Theme.ink
            }
            font.family: Theme.fontFamily
            font.pixelSize: root.compact ? 12 : 13
            font.weight: 600
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
        Accessible.role: Accessible.Button
        Accessible.name: root.text
    }
}
