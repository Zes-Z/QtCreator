import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import DayBook

Popup {
    id: root

    property string title: "确认操作"
    property string message: ""
    property string confirmText: "确认"
    property string cancelText: "取消"
    property bool danger: false
    signal confirmed()

    width: 390
    modal: true
    dim: false
    padding: 0
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay
    Overlay.modal: Rectangle { color: "transparent" }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 150; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 180; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
    }

    background: Item {
        Rectangle {
            id: panel
            anchors.fill: parent
            radius: 22
            color: Theme.surfaceSolid
            border.width: 1
            border.color: Theme.cardBorder
        }
    }

    contentItem: Column {
        width: root.width
        spacing: 10
        topPadding: 24
        leftPadding: 24
        rightPadding: 24
        bottomPadding: 20

        Text {
            width: parent.width - 48
            text: root.title
            color: Theme.ink
            font.family: Theme.fontFamily
            font.pixelSize: 17
            font.weight: 600
        }
        Text {
            width: parent.width - 48
            text: root.message
            color: Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: 13
            lineHeight: 1.5
            wrapMode: Text.WordWrap
        }
        Item { width: 1; height: 8 }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 24
            spacing: 10
            AppButton {
                text: root.cancelText
                kind: "secondary"
                compact: true
                onClicked: root.close()
            }
            AppButton {
                text: root.confirmText
                kind: root.danger ? "danger" : "primary"
                compact: true
                onClicked: {
                    root.close()
                    root.confirmed()
                }
            }
        }
    }
}
