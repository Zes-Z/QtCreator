import QtQuick
import DayBook

Item {
    id: root

    property double value: 0
    property color fillColor: Theme.accent
    property color trackColor: Theme.dark ? Theme.withAlpha(Theme.ink, 0.08) : Theme.withAlpha(Theme.ink, 0.065)
    property real barHeight: 6
    property bool over: false

    implicitHeight: barHeight
    height: barHeight

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.trackColor
    }

    Rectangle {
        width: Math.max(0, Math.min(1, root.value) * parent.width)
        height: parent.height
        radius: height / 2
        color: root.over ? Theme.danger : root.fillColor
        Behavior on width { NumberAnimation { duration: 520; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: 200 } }
    }
}
