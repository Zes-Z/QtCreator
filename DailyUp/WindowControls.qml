import QtQuick
import QtQuick.Window
import DailyUp

// macOS 风格红黄绿窗口按钮。
Row {
    id: controls

    property var win

    spacing: 8

    HoverHandler {
        id: rowHover
        cursorShape: Qt.PointingHandCursor
    }

    Repeater {
        model: 3

        delegate: Item {
            width: 12
            height: 12

            Rectangle {
                anchors.fill: parent
                radius: 6
                color: index === 0 ? "#FF5F57" : (index === 1 ? "#FEBC2E" : "#28C840")
                opacity: controls.win && !controls.win.active ? 0.45 : 1
                Behavior on opacity { NumberAnimation { duration: 140 } }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 0.5
                    radius: 5.5
                    color: "transparent"
                    border.width: 0.5
                    border.color: Qt.rgba(0, 0, 0, 0.10)
                }
            }

            Text {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -0.5
                text: index === 0 ? "×" : (index === 1 ? "−" : "+")
                font.pixelSize: 9
                font.bold: true
                color: index === 0 ? "#8E1B12" : (index === 1 ? "#8A5A00" : "#0B5A18")
                opacity: rowHover.hovered ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 110 } }
            }

            TapHandler {
                onTapped: {
                    if (!controls.win)
                        return
                    if (index === 0)
                        Qt.quit()
                    else if (index === 1)
                        controls.win.showMinimized()
                    else if (controls.win.visibility === Window.Maximized)
                        controls.win.showNormal()
                    else
                        controls.win.showMaximized()
                }
            }
        }
    }
}
