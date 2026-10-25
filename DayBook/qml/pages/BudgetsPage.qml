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

    GlassCard {
        id: listCard
        anchors.top: heroCard.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        radius: 20

        Item {
            anchors.fill: parent
            anchors.margins: 16

            Item {
                id: listHeader
                width: parent.width
                height: 34
                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "分类预算"
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: 600
                }
                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: App.budgets.count + " 个分类 · 每月独立"
                    color: Theme.faint
                    font.family: Theme.fontFamily
                    font.pixelSize: 9
                }
            }

            Flickable {
                id: budgetScroll
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: listHeader.bottom
                anchors.bottom: parent.bottom
                clip: true
                contentWidth: width
                contentHeight: rowsColumn.height
                boundsBehavior: Flickable.StopAtBounds
                interactive: true

                Column {
                id: rowsColumn
                width: budgetScroll.width
                spacing: 0

                Repeater {
                    model: App.budgets
                    Item {
                        width: rowsColumn.width
                        height: 60

                        CategoryIcon {
                            id: budgetIcon
                            x: 4
                            anchors.verticalCenter: parent.verticalCenter
                            icon: model.icon
                            accentColor: model.color
                            size: 34
                            iconSize: 17
                        }

                        Column {
                            anchors.left: budgetIcon.right
                            anchors.leftMargin: 11
                            anchors.right: budgetActions.left
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 5
                            Row {
                                width: parent.width
                                Text {
                                    text: model.name
                                    color: Theme.ink
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 11
                                    font.weight: 600
                                }
                                Text {
                                    text: "  " + App.money(model.spent) + " / " + App.money(model.budget)
                                    color: Theme.muted
                                    font.family: Theme.numberFamily
                                    font.pixelSize: 9
                                }
                            }
                            ProgressBar {
                                width: parent.width
                                height: 5
                                value: model.percent / 100
                                fillColor: model.over ? Theme.danger : model.color
                                over: model.over
                                barHeight: 5
                            }
                        }

                        HoverHandler { id: rowHover }

                        Column {
                            id: budgetActions
                            anchors.right: parent.right
                            anchors.rightMargin: 58
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Text {
                                anchors.right: parent.right
                                text: (model.over ? "超出 " : "剩余 ") + App.money(Math.abs(model.remaining))
                                color: model.over ? Theme.danger : Theme.ink
                                font.family: Theme.numberFamily
                                font.pixelSize: 10
                                font.weight: 600
                            }
                            Text {
                                anchors.right: parent.right
                                text: "已用 " + model.percent + "%"
                                color: model.over ? Theme.danger : Theme.faint
                                font.family: Theme.fontFamily
                                font.pixelSize: 8
                            }
                        }

                        Row {
                            anchors.right: parent.right
                            anchors.rightMargin: 0
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 0
                            z: 5
                            opacity: rowHover.hovered ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                            IconButton { icon: "edit"; size: 26; iconSize: 14; onClicked: budgetDialog.openFor(model.categoryId) }
                            IconButton {
                                icon: "delete"
                                size: 26
                                iconSize: 14
                                iconColor: Theme.muted
                                hoverColor: Theme.dangerSoft
                                onClicked: {
                                    page.pendingCategoryId = model.categoryId
                                    page.pendingCategoryName = model.name
                                    removeDialog.open()
                                }
                            }
                        }

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width - 8
                            x: 4
                            height: 1
                            color: Theme.hairline
                            opacity: index === App.budgets.count - 1 ? 0 : 0.65
                        }

                        MouseArea {
                            anchors.fill: parent
                            z: 0
                            cursorShape: Qt.PointingHandCursor
                            onClicked: budgetDialog.openFor(model.categoryId)
                        }
                    }
                }

                Item {
                    id: addBudgetRow
                    visible: App.budgets.count > 0
                    width: rowsColumn.width
                    height: 48

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 2
                        radius: 10
                        color: addBudgetMouse.containsMouse ? Theme.withAlpha(Theme.accent, 0.055) : "transparent"
                        border.width: 1
                        border.color: addBudgetMouse.containsMouse ? Theme.withAlpha(Theme.accent, 0.36) : Theme.hairline
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    Row {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 10
                        Rectangle {
                            width: 28
                            height: 28
                            radius: 9
                            color: Theme.accentWash
                            Icon { anchors.centerIn: parent; name: "add"; size: 15; iconColor: Theme.accent }
                        }
                        Text {
                            text: "添加分类预算"
                            color: Theme.accent
                            font.family: Theme.fontFamily
                            font.pixelSize: 10
                            font.weight: 500
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    MouseArea {
                        id: addBudgetMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: budgetDialog.openNew()
                    }
                }
                }
            }

            EmptyState {
                visible: App.budgets.count === 0
                anchors.centerIn: parent
                width: parent.width
                icon: "pie_chart"
                title: "还没有分类预算"
                description: "从最容易超支的分类开始，给这个月设个上限。"
                actionText: "设置预算"
                onActionClicked: budgetDialog.openNew()
            }
        }
    }

    BudgetDialog {
        id: budgetDialog
        onSaved: Qt.callLater(function() {
            budgetScroll.contentY = Math.max(0, budgetScroll.contentHeight - budgetScroll.height)
        })
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
