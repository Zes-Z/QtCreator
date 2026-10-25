import QtQuick
import QtQuick.Controls
import DayBook

Item {
    id: root

    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias echoMode: field.echoMode
    property alias validator: field.validator
    property alias inputMask: field.inputMask
    property alias inputMethodHints: field.inputMethodHints
    property alias readOnly: field.readOnly
    property alias field: field
    property alias horizontalAlignment: field.horizontalAlignment
    property string icon: "search"
    property bool clearable: true
    property int textSize: 13
    property bool focused: field.activeFocus
    property bool showIcon: icon !== ""
    signal accepted()
    signal textEdited(string value)
    signal cleared()

    implicitHeight: 40
    implicitWidth: 220

    Rectangle {
        anchors.fill: parent
        radius: Theme.controlRadius
        color: Theme.dark ? Theme.withAlpha(Theme.ink, 0.055) : Theme.withAlpha("#FFFFFF", 0.58)
        border.width: 1
        border.color: field.activeFocus ? Theme.withAlpha(Theme.accent, 0.62) : Theme.hairline
        Behavior on border.color { ColorAnimation { duration: 140 } }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: Theme.controlRadius + 2
            color: "transparent"
            border.width: 2
            border.color: Theme.withAlpha(Theme.accent, 0.12)
            visible: field.activeFocus
        }
    }

    Icon {
        visible: root.showIcon
        name: root.icon
        size: 17
        iconColor: field.activeFocus ? Theme.accent : Theme.muted
        anchors.left: parent.left
        anchors.leftMargin: 13
        anchors.verticalCenter: parent.verticalCenter
    }

    TextField {
        id: field
        anchors.fill: parent
        leftPadding: root.showIcon ? 38 : 14
        rightPadding: root.clearable && field.text.length > 0 ? 36 : 14
        color: Theme.ink
        placeholderTextColor: Theme.faint
        selectionColor: Theme.withAlpha(Theme.accent, 0.28)
        selectedTextColor: Theme.ink
        font.family: Theme.fontFamily
        font.pixelSize: root.textSize
        verticalAlignment: TextInput.AlignVCenter
        background: null
        selectByMouse: true
        onAccepted: root.accepted()
        onTextEdited: root.textEdited(field.text)
    }

    IconButton {
        visible: root.clearable && field.text.length > 0
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.verticalCenter: parent.verticalCenter
        icon: "close"
        iconSize: 13
        size: 26
        iconColor: Theme.muted
        onClicked: {
            field.text = ""
            field.forceActiveFocus()
            root.cleared()
        }
    }
}
