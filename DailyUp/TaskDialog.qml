import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DailyUp

// 新建 / 编辑共用对话框。macOS 上使用轻遮罩，不弹独立窗口。
Popup {
    id: dialog

    property string heading: "新建任务"
    property string confirmLabel: "添加"
    property string presetText: ""

    signal accepted(string text)

    popupType: Popup.Item
    modal: true
    dim: false
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    anchors.centerIn: parent

    width: Math.min(304, (parent ? parent.width : 360) - 44)
    height: contentItem.implicitHeight

    function openWith(text) {
        presetText = text || ""
        open()
    }

    function submit() {
        const value = input.text.trim()
        if (value.length === 0)
            return
        accepted(value)
        close()
    }

    onOpened: {
        input.text = presetText
        input.forceActiveFocus()
        input.cursorPosition = input.text.length
    }

    onClosed: input.text = ""


    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: 150
            easing.type: Easing.OutCubic
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: 110
        }
    }

    background: Item {
        Rectangle {
            id: card
            anchors.fill: parent
            radius: Theme.cardRadius
            color: Theme.surface
        }

    }

    contentItem: Item {
        implicitWidth: dialog.width
        implicitHeight: contentColumn.implicitHeight + 36

        ColumnLayout {
            id: contentColumn

            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Text {
                Layout.fillWidth: true
                text: dialog.heading
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: Theme.ink
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                radius: 11

                color: Theme.chip
                border.width: 1
                border.color: input.activeFocus ? Theme.alpha(Theme.accent, 0.55) : Theme.hairline
                Behavior on border.color { ColorAnimation { duration: 120 } }

                TextField {
                    id: input

                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    padding: 0
                    leftPadding: 0
                    rightPadding: 0
                    topPadding: 0
                    bottomPadding: 0

                    background: null
                    color: Theme.ink
                    placeholderText: "写下要做的事…"
                    placeholderTextColor: Theme.faint
                    font.pixelSize: 13
                    verticalAlignment: TextInput.AlignVCenter
                    selectionColor: Theme.accentSoft
                    selectedTextColor: Theme.ink
                    clip: true

                    onAccepted: dialog.submit()
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8

                Item {
                    width: 72
                    height: 32

                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: cancelHover.hovered ? Theme.chip : Theme.alpha(Theme.chip, 0)
                        border.width: 1
                        border.color: Theme.hairline
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "取消"
                        font.pixelSize: 12
                        color: Theme.inkSecondary
                    }

                    HoverHandler {
                        id: cancelHover
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        onTapped: dialog.close()
                    }
                }

                Item {
                    id: confirmButton

                    width: 82
                    height: 32

                    readonly property bool canSubmit: input.text.trim().length > 0

                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Theme.accent }
                            GradientStop { position: 1.0; color: Theme.accentDeep }
                        }
                        opacity: confirmButton.canSubmit ? 1 : 0.45
                        Behavior on opacity { NumberAnimation { duration: 120 } }

                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            color: "#FFFFFF"
                            opacity: confirmHover.hovered ? 0.08 : 0
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: dialog.confirmLabel
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: Theme.accentContrast
                        opacity: confirmButton.canSubmit ? 1 : 0.55
                    }

                    HoverHandler {
                        id: confirmHover
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        enabled: confirmButton.canSubmit
                        onTapped: dialog.submit()
                    }
                }
            }
        }
    }
}