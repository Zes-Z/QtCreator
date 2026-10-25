import QtQuick
import DayBook

Item {
    id: root

    property string kind: "cashflow"
    signal clicked()

    implicitHeight: 68
    height: implicitHeight

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: Theme.surfaceSunken
        border.width: 1
        border.color: cardMouse.containsMouse ? Theme.withAlpha(Theme.accent, 0.24) : Theme.hairline
        Behavior on border.color { ColorAnimation { duration: 130 } }
    }

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: Theme.withAlpha(Theme.accent, 0.05)
        opacity: cardMouse.containsMouse ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 130 } }
    }

    Item {
        anchors.fill: parent
        anchors.margins: 11
        anchors.rightMargin: 23
        visible: root.kind === "cashflow"

        Text {
            text: "收入与支出"
            color: Theme.inkSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 10
            font.weight: 600
        }

        Row {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 18
            spacing: 8

            Column {
                width: (parent.width - 8) / 2
                spacing: 2
                Text { text: "支出"; color: Theme.faint; font.family: Theme.fontFamily; font.pixelSize: 8 }
                AmountText { value: App.monthExpense; amountSize: 12; amountWeight: 600; amountColor: Theme.expense }
            }
            Column {
                width: (parent.width - 8) / 2
                spacing: 2
                Text { text: "收入"; color: Theme.faint; font.family: Theme.fontFamily; font.pixelSize: 8 }
                AmountText { value: App.monthIncome; amountSize: 12; amountWeight: 600; amountColor: Theme.income }
            }
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: 11
        anchors.rightMargin: 23
        visible: root.kind === "budget"

        Text {
            id: budgetTitle
            text: "本月预算"
            color: Theme.inkSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 10
            font.weight: 600
        }
        Text {
            anchors.right: parent.right
            text: App.monthBudget > 0
                  ? Math.max(0, Math.round(100 - App.monthBudgetPercent)) + "%"
                  : "未设置"
            color: App.monthBudget > 0 && App.monthBudgetSpent > App.monthBudget ? Theme.danger : Theme.muted
            font.family: Theme.numberFamily
            font.pixelSize: 9
            font.weight: 600
        }

        ProgressBar {
            width: parent.width
            y: 22
            value: App.monthBudget > 0
                   ? Math.max(0, Math.min(1, (App.monthBudget - App.monthBudgetSpent) / App.monthBudget))
                   : 0
            fillColor: App.monthBudget > 0 && App.monthBudgetSpent > App.monthBudget ? Theme.danger : Theme.accent
            over: App.monthBudget > 0 && App.monthBudgetSpent > App.monthBudget
            barHeight: 4
        }

        Row {
            width: parent.width
            y: 33
            Text {
                text: App.monthBudget > 0 ? "已用 " + App.money(App.monthBudgetSpent) : "给这个月设个上限"
                color: Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: 8
            }
            Item { width: parent.width - 128; height: 1 }
            Text {
                text: App.monthBudget > 0 ? "共 " + App.money(App.monthBudget) : "去设置 →"
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: 8
                font.weight: 500
            }
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: 11
        anchors.rightMargin: 23
        visible: root.kind === "reserve"

        Text {
            text: "待定"
            color: Theme.inkSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 10
            font.weight: 600
        }
        Icon {
            anchors.right: parent.right
            name: "more_horiz"
            size: 14
            iconColor: Theme.faint
        }
        Text {
            anchors.top: parent.top
            anchors.topMargin: 17
            text: "留一个位置给下一块"
            color: Theme.faint
            font.family: Theme.fontFamily
            font.pixelSize: 8
        }
    }

    MouseArea {
        id: cardMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.kind !== "reserve" ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
