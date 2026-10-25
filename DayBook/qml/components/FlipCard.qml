import QtQuick
import DayBook

Item {
    id: root

    property bool flipped: false
    property alias frontContent: frontItem.data
    property alias backContent: backItem.data
    property int flipDuration: 420
    property real angle: 0

    clip: true
    implicitWidth: 200
    implicitHeight: 200

    onFlippedChanged: angle = flipped ? 180 : 0
    Behavior on angle {
        NumberAnimation { duration: root.flipDuration; easing.type: Easing.InOutCubic }
    }

    Item {
        id: frontItem
        anchors.fill: parent
        visible: root.angle < 90
        transform: Rotation {
            origin.x: frontItem.width / 2
            origin.y: frontItem.height / 2
            axis { x: 0; y: 1; z: 0 }
            angle: root.angle
        }
    }

    Item {
        id: backItem
        anchors.fill: parent
        visible: root.angle >= 90
        transform: Rotation {
            origin.x: backItem.width / 2
            origin.y: backItem.height / 2
            axis { x: 0; y: 1; z: 0 }
            angle: root.angle - 180
        }
    }
}
