import QtQuick
import DayBook

Item {
    id: root

    property int categoryId: 0
    property string name: ""
    property string iconName: "category"
    property color accentColor: Theme.accent
    property double spent: 0
    property double budget: 0
    property double remaining: 0
    property int percent: 0
    property bool over: false
    property bool showActions: false
    property bool showDivider: true
    property real rowHeight: 60
    property bool compact: rowHeight < 48

    readonly property real iconSize: compact ? 24 : 34
    readonly property real iconGlyph: compact ? 13 : 17
    readonly property real titleSize: compact ? 10 : 11
    readonly property real amountSize: compact ? 8 : 9
    readonly property real valueSize: compact ? 9 : 10
    readonly property real percentSize: 8
    readonly property real barHeight: compact ? 4 : 5

    signal editRequested()
    signal deleteRequested()
    signal clicked()

    width: parent ? parent.width : implicitWidth
    height: rowHeight

    CategoryIcon {
        id: categoryIcon
        x: 4
        anchors.verticalCenter: parent.verticalCenter
        icon: root.iconName
        accentColor: root.accentColor
        size: root.iconSize
        iconSize: root.iconGlyph
    }

    Column {
        anchors.left: categoryIcon.right
        anchors.leftMargin: root.compact ? 8 : 11
        anchors.right: valueColumn.left
        anchors.rightMargin: root.compact ? 8 : 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.compact ? 3 : 5

        Row {
            width: parent.width
            spacing: 0
            Text {
                text: root.name
                color: Theme.ink
                font.family: Theme.fontFamily
                font.pixelSize: root.titleSize
                font.weight: 600
            }
            Text {
                text: "  " + App.money(root.spent) + " / " + App.money(root.budget)
                color: Theme.muted
                font.family: Theme.numberFamily
                font.pixelSize: root.amountSize
                elide: Text.ElideRight
            }
        }

        ProgressBar {
            width: parent.width
            height: root.barHeight
            value: Math.min(1, root.percent / 100)
            fillColor: root.over ? Theme.danger : root.accentColor
            over: root.over
            barHeight: root.barHeight
        }
    }

    Column {
        id: valueColumn
        anchors.right: parent.right
        anchors.rightMargin: root.showActions ? 58 : 0
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Text {
            anchors.right: parent.right
            text: (root.over ? "超出 " : "剩余 ") + App.money(Math.abs(root.remaining))
            color: root.over ? Theme.danger : Theme.ink
            font.family: Theme.numberFamily
            font.pixelSize: root.valueSize
            font.weight: 600
        }
        Text {
            anchors.right: parent.right
            text: "已用 " + root.percent + "%"
            color: root.over ? Theme.danger : Theme.faint
            font.family: Theme.fontFamily
            font.pixelSize: root.percentSize
        }
    }

    HoverHandler { id: rowHover }

    Row {
        visible: root.showActions
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0
        z: 5
        opacity: rowHover.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 120 } }
        IconButton {
            icon: "edit"
            size: 26
            iconSize: 14
            onClicked: root.editRequested()
        }
        IconButton {
            icon: "delete"
            size: 26
            iconSize: 14
            iconColor: Theme.muted
            hoverColor: Theme.dangerSoft
            onClicked: root.deleteRequested()
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width - 8
        x: 4
        height: 1
        color: Theme.hairline
        opacity: root.showDivider ? 0.65 : 0
    }

    MouseArea {
        anchors.fill: parent
        z: 0
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
