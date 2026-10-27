import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import DayBook

Popup {
    id: root

    width: 480
    height: 320
    modal: true
    dim: false
    padding: 24
    focus: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay
    Overlay.modal: Rectangle { color: "transparent" }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 150; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.965; to: 1; duration: 190; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 110 } }

    background: Item {
        Rectangle {
            anchors.fill: parent
            radius: 24
            color: Theme.surfaceSolid
            border.width: 1
            border.color: Theme.cardBorder
        }
    }

    contentItem: Item {
        width: root.width - 48
        height: root.height - 48

        IconButton {
            anchors.right: parent.right
            anchors.top: parent.top
            icon: "close"
            size: 30
            iconSize: 15
            onClicked: root.close()
        }

        Row {
            spacing: 12
            Image {
                width: 48
                height: 48
                source: "qrc:/assets/appicon.png"
                sourceSize.width: 96
                sourceSize.height: 96
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                Text {
                    text: "DayBook"
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: 18
                    font.weight: 700
                }
                Text {
                    text: "日光账本 · v" + App.version
                    color: Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: 10
                }
            }
        }

        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 72
            text: "一个安静的本地记账应用。没有账号，没有云端，只有你和你的日子。"
            color: Theme.inkSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 11
            lineHeight: 1.6
            wrapMode: Text.WordWrap
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 128
            height: 1
            color: Theme.hairline
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 146
            spacing: 10
            Row {
                spacing: 7
                Icon { name: "lock"; size: 14; iconColor: Theme.income; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    text: "数据只保存在这台电脑"
                    color: Theme.inkSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 10
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Row {
                spacing: 7
                Icon { name: "code"; size: 14; iconColor: Theme.faint; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    text: "Qt " + App.qtVersion + " · Material Symbols"
                    color: Theme.faint
                    font.family: Theme.fontFamily
                    font.pixelSize: 10
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Row {
                spacing: 7
                Icon { name: "open_in_new"; size: 14; iconColor: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    text: "开源项目与设计参考"
                    color: Theme.accent
                    font.family: Theme.fontFamily
                    font.pixelSize: 10
                    anchors.verticalCenter: parent.verticalCenter
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Qt.openUrlExternally("https://github.com/actualbudget/actual")
                    }
                }
            }
        }

        Text {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            text: "本地 SQLite · 无账号 · 无云同步"
            color: Theme.faint
            font.family: Theme.fontFamily
            font.pixelSize: 9
        }
    }
}
