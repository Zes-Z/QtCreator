import QtQuick
import DayBook

GlassCard {
    id: root

    property string title: ""
    property string icon: "receipt_long"
    property color iconColor: Theme.accent
    property double value: 0
    property bool moneyValue: true
    property string valueText: ""
    property string footer: ""
    property string deltaText: ""
    property string deltaTone: "none"
    property string deltaArrow: ""
    property bool footerBelowValue: false

    implicitHeight: 98
    radius: Theme.cardRadius
    sheenStrength: 0.4

    Item {
        anchors.fill: parent
        anchors.margins: 16

        Text {
            anchors.left: parent.left
            anchors.top: parent.top
            text: root.title
            color: Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: 10
            font.weight: 500
            font.letterSpacing: 0.3
        }

        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            width: 28
            height: 28
            radius: 9
            color: Theme.categoryTint(root.iconColor)
            Icon {
                anchors.centerIn: parent
                name: root.icon
                size: 15
                iconColor: root.iconColor
            }
        }

        AmountText {
            visible: root.moneyValue
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.topMargin: 24
            value: root.value
            amountSize: 23
            amountWeight: 600
            amountColor: Theme.ink
        }

        Text {
            visible: !root.moneyValue
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.topMargin: 24
            text: root.valueText
            color: Theme.ink
            font.family: Theme.numberFamily
            font.pixelSize: 23
            font.weight: 600
        }

        Text {
            visible: !root.footerBelowValue
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 29
            width: parent.width * 0.48
            text: root.footer
            color: Theme.faint
            font.family: Theme.fontFamily
            font.pixelSize: 8
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
        }

        Text {
            visible: root.footerBelowValue
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.topMargin: 54
            width: parent.width - (delta.visible ? delta.width + 10 : 0)
            text: root.footer
            color: Theme.faint
            font.family: Theme.fontFamily
            font.pixelSize: 9
            elide: Text.ElideRight
        }

        Rectangle {
            id: delta
            visible: root.deltaText !== ""
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 20
            width: deltaRow.implicitWidth + 14
            radius: 10
            color: root.deltaTone === "bad" ? Theme.expenseSoft
                 : root.deltaTone === "good" ? Theme.incomeSoft
                 : Theme.surfaceSunken

            Row {
                id: deltaRow
                anchors.centerIn: parent
                spacing: 3
                Icon {
                    name: root.deltaArrow !== "" ? root.deltaArrow
                          : root.deltaTone === "bad" ? "arrow_upward" : root.deltaTone === "good" ? "arrow_downward" : "remove"
                    size: 10
                    weight: 500
                    iconColor: root.deltaTone === "bad" ? Theme.expense
                             : root.deltaTone === "good" ? Theme.income
                             : Theme.muted
                }
                Text {
                    text: root.deltaText
                    color: root.deltaTone === "bad" ? Theme.expense
                         : root.deltaTone === "good" ? Theme.income
                         : Theme.muted
                    font.family: Theme.numberFamily
                    font.pixelSize: 9
                    font.weight: 600
                }
            }
        }
    }
}
