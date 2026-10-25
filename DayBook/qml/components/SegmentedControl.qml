import QtQuick
import QtQuick.Effects
import DayBook

Item {
    id: root

    property var items: []
    property int currentIndex: 0
    property int controlHeight: 34
    property int itemPadding: 28
    property bool hoverHighlight: true
    property int itemVersion: 0
    signal activated(int index)

    implicitHeight: controlHeight
    implicitWidth: row.implicitWidth + 8

    Rectangle {
        anchors.fill: parent
        radius: 11
        color: Theme.dark ? Qt.rgba(0, 0, 0, 0.28) : Qt.rgba(0.137, 0.122, 0.102, 0.10)
        border.width: 1
        border.color: Theme.dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0.137, 0.122, 0.102, 0.09)
    }

    RectangularShadow {
        anchors.fill: thumb
        radius: thumb.radius
        color: Theme.dark ? Qt.rgba(0, 0, 0, 0.48) : Qt.rgba(0.298, 0.216, 0.125, 0.22)
        blur: 10
        offset: Qt.vector2d(0, 2)
        spread: -1
    }

    Rectangle {
        id: thumb
        height: parent.height - 6
        y: 3
        radius: 8
        color: Theme.dark ? Qt.rgba(1, 1, 1, 0.14) : "#FFFFFF"
        border.width: 1
        border.color: Theme.dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0.137, 0.122, 0.102, 0.07)
        width: {
            root.itemVersion
            const item = repeater.itemAt(root.currentIndex)
            return item ? item.width - 4 : 0
        }
        x: {
            root.itemVersion
            const item = repeater.itemAt(root.currentIndex)
            return item ? row.x + item.x + 2 : row.x + 2
        }
        Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
    }

    Row {
        id: row
        x: Math.max(4, (root.width - row.implicitWidth) / 2)
        y: 4
        spacing: 0
        Repeater {
            id: repeater
            model: root.items
            onItemAdded: root.itemVersion++
            onItemRemoved: root.itemVersion++
            Item {
                width: label.implicitWidth + root.itemPadding
                height: root.controlHeight - 8
                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: root.hoverHighlight && mouse.containsMouse && index !== root.currentIndex
                           ? Theme.withAlpha(Theme.ink, 0.035) : "transparent"
                }
                Text {
                    id: label
                    anchors.centerIn: parent
                    text: modelData
                    color: index === root.currentIndex ? Theme.ink : Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    font.weight: index === root.currentIndex ? 600 : 500
                    Behavior on color { ColorAnimation { duration: 140 } }
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.currentIndex === index)
                            return
                        // The control is controlled from the outside: emit only.
                        // The parent updates currentIndex through its binding.
                        root.activated(index)
                    }
                }
            }
        }
    }
}
