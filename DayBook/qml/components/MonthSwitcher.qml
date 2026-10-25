import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import DayBook

Item {
    id: root

    implicitWidth: 188
    implicitHeight: 34
    property int pickerYear: new Date().getFullYear()

    function monthKey(year, month) {
        return year + "-" + (month < 10 ? "0" + month : "" + month)
    }

    function openPicker() {
        pickerYear = Number(App.currentMonth.split("-")[0])
        picker.open()
    }

    Rectangle {
        anchors.fill: parent
        radius: 11
        color: Theme.surfaceRaised
        border.width: 1
        border.color: Theme.cardBorder
    }

    Row {
        anchors.fill: parent
        spacing: 0

        Item {
            width: 32
            height: parent.height
            Icon {
                anchors.centerIn: parent
                name: "chevron_left"
                size: 18
                iconColor: prevMouse.containsMouse ? Theme.ink : Theme.muted
            }
            MouseArea {
                id: prevMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: App.shiftMonth(-1)
            }
        }

        Item {
            width: 124
            height: parent.height
            Text {
                anchors.centerIn: parent
                text: App.monthLabel
                color: mouse.containsMouse ? Theme.ink : Theme.inkSecondary
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.weight: 600
            }
            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.openPicker()
            }
        }

        Item {
            width: 32
            height: parent.height
            Icon {
                anchors.centerIn: parent
                name: "chevron_right"
                size: 18
                iconColor: nextMouse.containsMouse ? Theme.ink : Theme.muted
            }
            MouseArea {
                id: nextMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: App.shiftMonth(1)
            }
        }
    }

    Popup {
        id: picker
        width: 288
        padding: 16
        modal: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        parent: root
        x: (root.width - width) / 2
        y: root.height + 8

        background: Item {
            Rectangle {
                id: panel
                anchors.fill: parent
                radius: 18
                color: Theme.surfaceSolid
                border.width: 1
                border.color: Theme.cardBorder
            }
        }

        contentItem: Column {
            width: picker.width - 32
            spacing: 12
            Row {
                width: parent.width
                height: 28
                IconButton {
                    icon: "chevron_left"
                    size: 26
                    iconSize: 16
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: root.pickerYear--
                }
                Item { width: parent.width - 96; height: 1 }
                Text {
                    text: root.pickerYear + "年"
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: 600
                    anchors.verticalCenter: parent.verticalCenter
                }
                Item { width: 4; height: 1 }
                IconButton {
                    icon: "chevron_right"
                    size: 26
                    iconSize: 16
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: root.pickerYear++
                }
            }
            Grid {
                width: parent.width
                columns: 4
                spacing: 6
                Repeater {
                    model: 12
                    Item {
                        width: (picker.width - 32 - 18) / 4
                        height: 34
                        Rectangle {
                            anchors.fill: parent
                            radius: 9
                            color: {
                                const current = App.currentMonth === root.monthKey(root.pickerYear, index + 1)
                                if (current) return Theme.accent
                                if (mouse.containsMouse) return Theme.surfaceSunken
                                return "transparent"
                            }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: index + 1 + "月"
                            color: App.currentMonth === root.monthKey(root.pickerYear, index + 1)
                                   ? Theme.accentContrast : Theme.inkSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: 12
                            font.weight: 500
                        }
                        MouseArea {
                            id: mouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                App.setCurrentMonth(root.monthKey(root.pickerYear, index + 1))
                                picker.close()
                            }
                        }
                    }
                }
            }
            Rectangle {
                width: parent.width
                height: 1
                color: Theme.hairline
            }
            Item {
                width: parent.width
                height: 30
                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "回到本月"
                    color: Theme.accent
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    font.weight: 600
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        App.goToToday()
                        picker.close()
                    }
                }
            }
        }
    }
}
