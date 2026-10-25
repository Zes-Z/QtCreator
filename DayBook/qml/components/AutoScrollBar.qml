import QtQuick
import DayBook

Item {
    id: root

    property Flickable flickable: null
    property bool revealing: false

    width: 5
    opacity: revealing ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

    readonly property real span: flickable ? Math.max(0, flickable.contentHeight - flickable.height) : 0

    Timer {
        id: hideTimer
        interval: 950
        repeat: false
        onTriggered: {
            if (barMouse.containsMouse || barMouse.pressed)
                hideTimer.restart()
            else
                root.revealing = false
        }
    }

    function reveal() {
        if (!flickable || root.span <= 0)
            return
        root.revealing = true
        hideTimer.restart()
    }

    Rectangle {
        id: thumb
        width: 4
        x: (root.width - width) / 2
        radius: 2
        color: Theme.withAlpha(Theme.ink, barMouse.pressed ? 0.38 : 0.26)
        height: root.flickable && root.flickable.contentHeight > 0
                ? Math.max(26, root.height * Math.min(1, root.flickable.height / root.flickable.contentHeight))
                : 0
        y: root.span > 0
           ? Math.min(root.height - height, (root.flickable.contentY / root.span) * Math.max(0, root.height - height))
           : 0
    }

    MouseArea {
        id: barMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        property real pressY: 0
        property real pressContentY: 0
        onPressed: {
            pressY = mouse.y
            pressContentY = root.flickable ? root.flickable.contentY : 0
            root.reveal()
        }
        onPositionChanged: {
            if (!pressed || !root.flickable || root.span <= 0)
                return
            const usable = Math.max(1, root.height - thumb.height)
            const next = pressContentY + (mouse.y - pressY) / usable * root.span
            root.flickable.contentY = Math.max(0, Math.min(root.span, next))
        }
    }

    Connections {
        target: root.flickable
        function onContentYChanged() { root.reveal() }
        function onContentHeightChanged() { if (root.revealing) root.reveal() }
        function onMovingChanged() {
            if (root.flickable && root.flickable.moving)
                root.reveal()
        }
    }
}
