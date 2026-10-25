import QtQuick
import DayBook

Rectangle {
    id: root

    property string icon: "category"
    property color accentColor: Theme.accent
    property int size: 38
    property int iconSize: 19
    property bool solid: false
    property bool bordered: false

    implicitWidth: size
    implicitHeight: size
    width: size
    height: size
    radius: size * 0.34
    color: solid ? accentColor : Theme.categoryTint(accentColor)
    border.width: bordered ? 1 : 0
    border.color: Theme.withAlpha(accentColor, 0.22)

    Rectangle {
        visible: root.solid
        anchors.fill: parent
        radius: parent.radius
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.withAlpha("#FFFFFF", 0.18) }
            GradientStop { position: 1; color: "transparent" }
        }
    }

    Icon {
        anchors.centerIn: parent
        name: root.icon
        size: root.iconSize
        iconColor: root.solid ? "#FFFFFF" : root.accentColor
        weight: 500
    }
}
