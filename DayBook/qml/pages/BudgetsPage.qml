import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import DayBook

Item {
    id: page

    property int pendingCategoryId: 0
    property string pendingCategoryName: ""

    Component.onCompleted: {
        if (App.startupModal === "budget")
            Qt.callLater(function() { budgetDialog.openNew() })
    }

    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 56

        Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            Text {
                text: "预算"
                color: Theme.ink
                font.family: Theme.fontFamily
                font.pixelSize: 25
                font.weight: 600
                font.letterSpacing: -0.3
            }
            Text {
                text: "给开销一个温柔的边界，给生活留一点余地。"
                color: Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: 11
            }
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10
            MonthSwitcher { anchors.verticalCenter: parent.verticalCenter }
            AppButton {
                text: "设置预算"
                icon: "add"
                compact: true
                width: 100
                anchors.verticalCenter: parent.verticalCenter
                onClicked: budgetDialog.openNew()
            }
        }
    }

    GlassCard {
        id: heroCard
        anchors.top: header.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.right: parent.right
        height: 146
        radius: 20
        sheenStrength: 0.7

        Item {
            anchors.fill: parent
            anchors.margins: 20

            Item {
                visible: App.monthBudget > 0
                width: 102
                height: 102
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter

                ProgressRing {
                    anchors.fill: parent
                    value: App.monthBudget > 0 ? Math.min(1, App.monthBudgetSpent / App.monthBudget) : 0
                    ringColor: App.monthBudgetSpent > App.monthBudget ? Theme.danger : Theme.accent
                    lineWidth: 9
                }
                Column {
                    anchors.centerIn: parent
                    spacing: 1
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: App.monthBudgetPercent + "%"
                        color: App.monthBudgetSpent > App.monthBudget ? Theme.danger : Theme.ink
                        font.family: Theme.numberFamily
                        font.pixelSize: 20
                        font.weight: 600
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "已使用"
                        color: Theme.faint
                        font.family: Theme.fontFamily
                        font.pixelSize: 8
                    }
                }
            }

            Rectangle {
                visible: App.monthBudget <= 0
                width: 58
                height: 58
                radius: 18
                color: Theme.accentWash
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                Icon { anchors.centerIn: parent; name: "pie_chart"; size: 27; iconColor: Theme.accent }
            }

            Column {
                visible: App.monthBudget > 0
                anchors.left: parent.left
                anchors.leftMargin: 124
                anchors.verticalCenter: parent.verticalCenter
                spacing: 5
                Text {
                    text: "本月预算"
                    color: Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: 10
                }
                AmountText {
                    value: App.monthBudget
                    amountSize: 28
                    amountWeight: 600
                    amountColor: Theme.ink
                }
                Row {
                    spacing: 8
                    Tag {
                        text: App.monthBudgetSpent > App.monthBudget ? "已超支" : "节奏良好"
                        icon: App.monthBudgetSpent > App.monthBudget ? "error" : "check_circle"
                        tagColor: App.monthBudgetSpent > App.monthBudget ? Theme.danger : Theme.income
                        bgColor: App.monthBudgetSpent > App.monthBudget ? Theme.dangerSoft : Theme.incomeSoft
                    }
                    Text {
                        text: App.monthBudgetSpent > App.monthBudget
                              ? "超出 " + App.money(App.monthBudgetSpent - App.monthBudget)
                              : "还剩 " + App.money(App.monthBudget - App.monthBudgetSpent)
                        color: App.monthBudgetSpent > App.monthBudget ? Theme.danger : Theme.income
                        font.family: Theme.numberFamily
                        font.pixelSize: 9
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            Column {
                visible: App.monthBudget <= 0
                anchors.left: parent.left
                anchors.leftMargin: 76
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                Text {
                    text: "给这个月设一个预算"
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: 16
                    font.weight: 600
                }
                Text {
                    width: parent.width
                    text: "把餐饮、购物、娱乐等分类分别设个上限，超支时会在概览里提醒你。"
                    color: Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }
                AppButton {
                    text: "开始设置"
                    compact: true
                    width: 88
                    onClicked: budgetDialog.openNew()
                }
            }

            Row {
                visible: App.monthBudget > 0
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 30
                Column {
                    spacing: 4
                    Text { text: "已使用"; color: Theme.faint; font.family: Theme.fontFamily; font.pixelSize: 9 }
                    AmountText { value: App.monthBudgetSpent; amountSize: 14; amountWeight: 600; amountColor: Theme.ink }
                }
                Column {
                    spacing: 4
                    Text {
                        text: App.monthBudgetSpent > App.monthBudget ? "超出" : "剩余"
                        color: Theme.faint
                        font.family: Theme.fontFamily
                        font.pixelSize: 9
                    }
                    AmountText {
                        value: Math.abs(App.monthBudget - App.monthBudgetSpent)
                        amountSize: 14
                        amountWeight: 600
                        amountColor: App.monthBudgetSpent > App.monthBudget ? Theme.danger : Theme.income
                    }
                }
                Column {
                    spacing: 4
                    Text { text: "分类"; color: Theme.faint; font.family: Theme.fontFamily; font.pixelSize: 9 }
                    Text {
                        text: App.budgets.count + " 个"
                        color: Theme.ink
                        font.family: Theme.numberFamily
                        font.pixelSize: 14
                        font.weight: 600
                    }
                }
            }
        }
    }

    BudgetCategoryListCard {
        id: listCard
        anchors.top: heroCard.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        radius: 20
        showActions: true
        showAddRow: true
        onEditRequested: function(categoryId) { budgetDialog.openFor(categoryId) }
        onDeleteRequested: function(categoryId, name) {
            page.pendingCategoryId = categoryId
            page.pendingCategoryName = name
            removeDialog.open()
        }
        onAddRequested: budgetDialog.openNew()
        onRowClicked: function(categoryId) { budgetDialog.openFor(categoryId) }
    }

    BudgetDialog {
        id: budgetDialog
        onSaved: Qt.callLater(function() { listCard.scrollToBottom() })
    }

    ConfirmDialog {
        id: removeDialog
        title: "移除这个预算？"
        message: "“" + page.pendingCategoryName + "”的本月预算会被移除，账单记录不会受影响。"
        confirmText: "移除"
        danger: true
        onConfirmed: {
            if (App.deleteBudget(page.pendingCategoryId))
                UiBus.toast("预算已移除", "success")
            else
                UiBus.toast(App.lastError, "error")
        }
    }
}
