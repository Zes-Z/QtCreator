import QtQuick
import DayBook

Row {
    id: root

    property real cardHeight: 104
    readonly property real gap: 14
    readonly property real cardWidth: (width - gap * 2) / 3
    width: parent ? parent.width : implicitWidth
    height: cardHeight
    spacing: gap

    StatCard {
        width: root.cardWidth
        height: root.cardHeight
        footerBelowValue: true
        title: "本月支出"
        icon: "receipt_long"
        iconColor: Theme.expense
        value: App.monthExpense
        footer: App.monthTransactionCount + " 笔记录"
        deltaText: App.lastMonthExpense > 0 ? App.percentLabel(App.expenseChange) + " 较上月" : ""
        deltaTone: App.expenseChange > 0 ? "bad" : "good"
        deltaArrow: App.expenseChange > 0 ? "arrow_upward" : "arrow_downward"
        clickable: true
        onClicked: {
            App.transactions.setKindFilter("expense")
            UiBus.navigate("transactions")
        }
    }
    StatCard {
        width: root.cardWidth
        height: root.cardHeight
        footerBelowValue: true
        title: "本月收入"
        icon: "payments"
        iconColor: Theme.income
        value: App.monthIncome
        footer: "工资与副业"
        deltaText: App.lastMonthIncome > 0 ? App.percentLabel(App.incomeChange) + " 较上月" : ""
        deltaTone: App.incomeChange >= 0 ? "good" : "bad"
        deltaArrow: App.incomeChange >= 0 ? "arrow_upward" : "arrow_downward"
        clickable: true
        onClicked: {
            App.transactions.setKindFilter("income")
            UiBus.navigate("transactions")
        }
    }
    StatCard {
        width: root.cardWidth
        height: root.cardHeight
        footerBelowValue: true
        title: "日均支出"
        icon: "schedule"
        iconColor: Theme.accent
        value: App.dailyAverage
        footer: "每笔平均 " + App.money(App.monthTransactionCount > 0 ? App.monthExpense / App.monthTransactionCount : 0)
    }
}
