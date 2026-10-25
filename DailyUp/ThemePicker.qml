import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DailyUp

// 主题选择浮层：左侧五套预设，右侧一条竖向调色带。
Popup {
    id: picker

    width: 208
    height: contentItem.implicitHeight
    padding: 0

    popupType: Popup.Item
    modal: true
    dim: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    x: parent ? parent.width - width - 14 : 0
    y: 42

    onOpened: hueArea.hue = Theme.hueOf(AppSettings.customAccent)

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: 130
            easing.type: Easing.OutCubic
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: 100
        }
    }

    background: Item {
        Rectangle {
            id: card
            anchors.fill: parent
            radius: 16
            color: Theme.accentWash
            border.width: 1
            border.color: Theme.alpha(Theme.accent, 0.16)
        }
    }

    contentItem: Item {
        implicitWidth: picker.width
        implicitHeight: contentColumn.implicitHeight + 16

        ColumnLayout {
            id: contentColumn

            anchors.fill: parent
            anchors.margins: 8
            spacing: 4

            Text {
                Layout.fillWidth: true
                Layout.leftMargin: 8
                Layout.bottomMargin: 2
                text: "主题"
                font.pixelSize: 11
                font.weight: Font.Medium
                color: Theme.faint
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Repeater {
                        model: Theme.presetKeys

                        delegate: Rectangle {
                            id: option

                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            radius: 10

                            readonly property var themeColors: Theme.palettes[modelData]
                            readonly property bool current: Theme.key === modelData

                            color: optionHover.hovered ? Theme.chip : Theme.alpha(Theme.chip, 0)
                            Behavior on color { ColorAnimation { duration: 120 } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 16
                                    Layout.preferredHeight: 16
                                    radius: 8
                                    color: option.themeColors.accent
                                    border.width: option.current ? 2 : 1
                                    border.color: option.current ? Qt.rgba(0, 0, 0, 0.16) : Qt.rgba(0, 0, 0, 0.06)
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: option.themeColors.label
                                    font.pixelSize: 13
                                    font.weight: option.current ? Font.DemiBold : Font.Normal
                                    color: Theme.ink
                                }

                                Text {
                                    text: "✓"
                                    visible: option.current
                                    font.pixelSize: 12
                                    font.bold: true
                                    color: Theme.accent
                                }
                            }

                            HoverHandler {
                                id: optionHover
                                cursorShape: Qt.PointingHandCursor
                            }

                            TapHandler {
                                onTapped: {
                                    AppSettings.theme = modelData
                                    picker.close()
                                }
                            }
                        }
                    }
                }

                Item {
                    id: hueArea

                    Layout.preferredWidth: 18
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignTop

                    property real hue: 0.92

                    Rectangle {
                        id: hueBar

                        width: 8
                        height: parent.height
                        anchors.horizontalCenter: parent.horizontalCenter
                        radius: 4

                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: "#FF3B30" }
                            GradientStop { position: 0.17; color: "#FFD60A" }
                            GradientStop { position: 0.33; color: "#34C759" }
                            GradientStop { position: 0.50; color: "#32ADE6" }
                            GradientStop { position: 0.67; color: "#0A84FF" }
                            GradientStop { position: 0.83; color: "#BF5AF2" }
                            GradientStop { position: 1.0; color: "#FF3B30" }
                        }
                    }

                    Rectangle {
                        id: hueKnob
                        width: 16
                        height: 16
                        radius: 8
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: Math.max(0, Math.min(hueArea.height - height,
                                                hueArea.hue * (hueArea.height - height)))
                        color: Qt.hsva(hueArea.hue, 0.62, 0.92, 1)
                        border.width: 2
                        border.color: "#FFFFFF"
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor

                        function applyHue(mouseY) {
                            const usable = Math.max(1, hueArea.height - 16)
                            hueArea.hue = Math.max(0, Math.min(1, (mouseY - 8) / usable))
                            AppSettings.customAccent = Qt.hsva(hueArea.hue, 0.62, 0.92, 1)
                            AppSettings.theme = "custom"
                        }

                        onPressed: (mouse) => applyHue(mouse.y)
                        onPositionChanged: (mouse) => {
                            if (pressed)
                                applyHue(mouse.y)
                        }
                    }
                }
            }
        }
    }
}