import QtQuick
import QtQuick.Effects
import DayBook

Item {
    id: root

    property real radius: Theme.cardRadius
    property color tint: Theme.surface
    property color borderColor: Theme.cardBorder
    property bool shadow: true
    property real shadowBlur: 28
    property color shadowColor: Theme.cardShadow
    property real shadowOffsetY: 9
    property real sheenStrength: 0.5
    property bool noise: true
    property bool interactive: false
    property bool hovered: false

    default property alias content: contentItem.data

    RectangularShadow {
        anchors.fill: card
        radius: root.radius
        blur: root.shadow ? root.shadowBlur : 0
        color: root.shadowColor
        offset: Qt.vector2d(0, root.shadowOffsetY)
        spread: 0
        visible: root.shadow
    }

    Rectangle {
        id: card
        anchors.fill: parent
        radius: root.radius
        color: root.tint
        border.width: 1
        border.color: root.borderColor
        Behavior on color { ColorAnimation { duration: 160 } }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.dark ? Qt.rgba(1, 1, 1, 0.055) : Qt.rgba(1, 1, 1, 0.20) }
            GradientStop { position: 0.42; color: "transparent" }
        }
        opacity: root.sheenStrength
    }

    Image {
        anchors.fill: parent
        source: "qrc:/assets/noise.png"
        fillMode: Image.Tile
        opacity: root.noise ? 0.040 : 0
        visible: root.noise
    }

    Item {
        id: contentItem
        anchors.fill: parent
    }
}
