import QtQuick
import DayBook

Item {
    id: root

    width: 16
    height: 30

    HoverHandler { id: hover; cursorShape: Qt.SizeVerCursor }

    Grid {
        anchors.centerIn: parent
        columns: 2
        rows: 3
        columnSpacing: 4
        rowSpacing: 4
        Repeater {
            model: 6
            Rectangle {
                width: 3
                height: 3
                radius: 1.5
                color: hover.hovered ? Theme.muted : Theme.withAlpha(Theme.ink, 0.24)
            }
        }
    }
}
