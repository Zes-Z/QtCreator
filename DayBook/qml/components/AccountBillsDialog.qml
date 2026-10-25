import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import DayBook

Popup {
    id: root

    property int accountId: 0
    property string accountName: ""
    property color accountColor: Theme.accent

    signal editRequested(int txId)

    width: 660
    height: 640
    modal: true
    dim: false
    padding: 22
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

    function openFor(id, name, color) {
        accountId = id
        accountName = name
        accountColor = color
        App.setAccountBillsAccount(id)
        open()
    }

    contentItem: Item {
        width: root.width - 44
        height: root.height - 44

        Item {
            width: parent.width
            height: 34
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: root.accountColor
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: root.accountName + " · 本月流水"
                color: Theme.ink
                font.family: Theme.fontFamily
                font.pixelSize: 19
                font.weight: 600
            }
            IconButton {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                icon: "close"
                size: 30
                iconSize: 15
                onClicked: root.close()
            }
        }

        Row {
            width: parent.width
            anchors.top: parent.top
            anchors.topMargin: 44
            spacing: 18
            Text {
                text: App.accountBills.transactionCount + " 笔账单"
                color: Theme.faint
                font.family: Theme.numberFamily
                font.pixelSize: 10
            }
            Text {
                text: "支出 " + App.money(App.accountBills.totalExpense)
                color: Theme.expense
                font.family: Theme.numberFamily
                font.pixelSize: 10
            }
            Text {
                text: "收入 " + App.money(App.accountBills.totalIncome)
                color: Theme.income
                font.family: Theme.numberFamily
                font.pixelSize: 10
            }
        }

        Rectangle {
            anchors.top: parent.top
            anchors.topMargin: 68
            width: parent.width
            height: 1
            color: Theme.hairline
        }

        ListView {
            id: billsList
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 76
            anchors.bottom: parent.bottom
            clip: true
            model: App.accountBills
            spacing: 0
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 500

            delegate: Item {
                width: billsList.width
                height: model.isHeader ? 34 : 56

                Item {
                    visible: model.isHeader
                    width: parent.width
                    height: 34
                    Row {
                        x: 6
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 7
                        Text {
                            text: model.dayLabel
                            color: Theme.inkSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: 10
                            font.weight: 600
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: "· " + model.dayWeek
                            color: Theme.faint
                            font.family: Theme.fontFamily
                            font.pixelSize: 9
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: model.dayCount + " 笔"
                            color: Theme.faint
                            font.family: Theme.numberFamily
                            font.pixelSize: 9
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width - 12
                        x: 6
                        height: 1
                        color: Theme.hairline
                        opacity: 0.6
                    }
                }

                TransactionRow {
                    visible: !model.isHeader
                    width: parent.width
                    height: 56
                    actionsVisible: false
                    txId: model.txId
                    kind: model.kind
                    amount: model.amount
                    categoryName: model.categoryName
                    categoryIcon: model.categoryIcon
                    categoryColor: model.categoryColor
                    accountName: model.accountName
                    transferAccountName: model.transferAccountName
                    note: model.note
                    time: model.time
                    onEditRequested: {
                        root.close()
                        root.editRequested(model.txId)
                    }
                }
            }
        }

        AutoScrollBar {
            flickable: billsList
            anchors.right: parent.right
            anchors.top: billsList.top
            anchors.bottom: billsList.bottom
        }

        EmptyState {
            visible: App.accountBills.transactionCount === 0
            anchors.centerIn: parent
            width: parent.width
            icon: "receipt_long"
            title: "这个月还没有流水"
            description: "该账户在本月还没有账单记录。"
        }
    }
}
