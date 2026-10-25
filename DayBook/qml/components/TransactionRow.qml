import QtQuick
import DayBook

Item {
    id: root

    property int txId: 0
    property string kind: "expense"
    property double amount: 0
    property string categoryName: ""
    property string categoryIcon: "category"
    property color categoryColor: Theme.accent
    property string accountName: ""
    property string transferAccountName: ""
    property bool transferMode: kind === "transfer"
    property bool actionsVisible: true
    property string note: ""
    property string time: ""

    signal editRequested()
    signal deleteRequested()

    implicitHeight: 62
    height: 62

    HoverHandler { id: rowHover }

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        radius: 13
        color: rowHover.hovered ? Theme.surfaceSunken : "transparent"
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    CategoryIcon {
        id: icon
        x: 14
        anchors.verticalCenter: parent.verticalCenter
        icon: root.categoryIcon
        accentColor: root.categoryColor
        size: 38
        iconSize: 19
    }

    Column {
        anchors.left: icon.right
        anchors.leftMargin: 12
        anchors.right: amountText.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 3

        Text {
            width: parent.width
            text: root.categoryName
            color: Theme.ink
            font.family: Theme.fontFamily
            font.pixelSize: 13
            font.weight: 600
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: {
                let detail = root.transferMode
                        ? (root.accountName + " → " + root.transferAccountName)
                        : (root.note !== "" ? root.note : root.accountName)
                if (!root.transferMode && root.note !== "" && root.accountName !== "")
                    detail += " · " + root.accountName
                if (root.transferMode && root.note !== "")
                    detail += " · " + root.note
                if (root.time !== "")
                    detail += " · " + root.time
                return detail
            }
            color: Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }

    AmountText {
        id: amountText
        anchors.right: parent.right
        anchors.rightMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        value: root.amount
        signedAmount: !root.transferMode
        income: root.kind === "income"
        amountSize: 13.5
        amountWeight: 600
        amountColor: root.transferMode ? Theme.muted
                                      : (root.kind === "income" ? Theme.income : Theme.ink)
        opacity: rowHover.hovered && root.actionsVisible ? 0 : 1
        Behavior on opacity { NumberAnimation { duration: 100 } }
    }

    Row {
        id: actionRow
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        z: 5
        opacity: root.actionsVisible && rowHover.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 120 } }

        IconButton {
            icon: "edit"
            size: 28
            iconSize: 15
            hoverColor: Theme.surfaceRaised
            onClicked: root.editRequested()
        }
        IconButton {
            icon: "delete"
            size: 28
            iconSize: 15
            iconColor: Theme.muted
            hoverColor: Theme.dangerSoft
            onClicked: root.deleteRequested()
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 0
        acceptedButtons: Qt.LeftButton
        cursorShape: Qt.PointingHandCursor
        onClicked: root.editRequested()
    }
}
