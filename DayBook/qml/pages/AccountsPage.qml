import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import DayBook

Item {
    id: page

    property int pendingAccountId: 0
    property string pendingAccountName: ""
    property bool pendingArchives: false
    readonly property real distributionTotal: Math.max(1, App.totalAssets + App.totalLiabilities)

    Component.onCompleted: {
        if (App.startupModal === "account")
            Qt.callLater(function() { accountDialog.openNew() })
        else if (App.startupModal === "accountedit" && App.accounts.count > 0) {
            Qt.callLater(function() { accountDialog.openFor(App.accounts.get(0).id) })
        }
        else if (App.startupModal === "accountbills" && App.accounts.count > 0) {
            Qt.callLater(function() {
                let a = App.accounts.get(0)
                for (let i = 0; i < App.accounts.count; ++i) {
                    const row = App.accounts.get(i)
                    if (row.monthExpense > 0 || row.monthIncome > 0) {
                        a = row
                        break
                    }
                }
                accountBillsDialog.openFor(a.id, a.name, a.color)
            })
        }
    }

    function requestDelete(id, name) {
        pendingAccountId = id
        pendingAccountName = name
        pendingArchives = App.accountInUse(id)
        deleteDialog.open()
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.height + 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: content
            width: flick.width
            y: 2
            spacing: 15

            Item {
                width: parent.width
                height: 58
                Column {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    Text {
                        text: "账户"
                        color: Theme.ink
                        font.family: Theme.fontFamily
                        font.pixelSize: 25
                        font.weight: 600
                        font.letterSpacing: -0.3
                    }
                    Text {
                        text: "每一份资产，都有自己的来处。"
                        color: Theme.muted
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                    }
                }
                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 9
                    AppButton {
                        text: "添加账户"
                        icon: "add"
                        compact: true
                        width: 104
                        onClicked: accountDialog.openNew()
                    }
                }
            }

            GlassCard {
                width: parent.width
                height: 150
                radius: 20
                shadow: false
                sheenStrength: 0.8

                Rectangle {
                    anchors.fill: parent
                    radius: 20
                    gradient: Gradient {
                        GradientStop { position: 0; color: Theme.dark ? Qt.rgba(0.18, 0.28, 0.24, 0.26) : Qt.rgba(0.88, 0.95, 0.91, 0.55) }
                        GradientStop { position: 1; color: "transparent" }
                    }
                }

                Item {
                    anchors.fill: parent
                    anchors.margins: 20

                    Text {
                        x: 0
                        y: 0
                        text: "净资产"
                        color: Theme.muted
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                        font.letterSpacing: 1.2
                    }
                    AmountText {
                        x: 0
                        y: 20
                        value: App.netWorth
                        amountSize: 34
                        amountWeight: 600
                        amountColor: Theme.ink
                    }
                    Row {
                        x: 0
                        anchors.bottom: parent.bottom
                        spacing: 20
                        Row {
                            spacing: 6
                            Rectangle { width: 5; height: 5; radius: 3; color: Theme.income; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: "总资产 " + App.money(App.totalAssets); color: Theme.muted; font.family: Theme.numberFamily; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                        }
                        Row {
                            spacing: 6
                            Rectangle { width: 5; height: 5; radius: 3; color: Theme.danger; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: "总负债 " + App.money(App.totalLiabilities); color: Theme.muted; font.family: Theme.numberFamily; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }

                    Item {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(380, parent.width * 0.42)
                        height: 76

                        Text {
                            anchors.right: parent.right
                            text: App.totalLiabilities > 0 ? "资产 / 负债分布" : "资产分布"
                            color: Theme.faint
                            font.family: Theme.fontFamily
                            font.pixelSize: 9
                        }

                        Row {
                            id: assetBar
                            width: parent.width
                            height: 10
                            y: 20
                            spacing: 3
                            Repeater {
                                model: App.accounts
                                Rectangle {
                                    visible: model.balance > 0
                                    height: 10
                                    radius: 5
                                    color: model.color
                                    width: Math.max(0, (model.balance / distributionTotal) * (assetBar.width - 15))
                                }
                            }
                            Rectangle {
                                visible: App.totalLiabilities > 0
                                height: 10
                                radius: 5
                                color: Theme.danger
                                width: Math.max(0, (App.totalLiabilities / distributionTotal) * (assetBar.width - 15))
                            }
                        }

                        Row {
                            width: parent.width
                            y: 40
                            spacing: 10
                            Repeater {
                                model: App.accounts
                                Row {
                                    visible: model.balance > 0
                                    spacing: 4
                                    Rectangle { width: 5; height: 5; radius: 3; color: model.color; anchors.verticalCenter: parent.verticalCenter }
                                    Text { text: model.name; color: Theme.muted; font.family: Theme.fontFamily; font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }
                                }
                            }
                            Row {
                                visible: App.totalLiabilities > 0
                                spacing: 4
                                Rectangle { width: 5; height: 5; radius: 3; color: Theme.danger; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: "负债"; color: Theme.muted; font.family: Theme.fontFamily; font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }
                            }
                        }

                        Text {
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            text: App.accounts.count + " 个账户 · 数据只保存在本机"
                            color: Theme.faint
                            font.family: Theme.fontFamily
                            font.pixelSize: 8
                        }
                    }
                }
            }

            Grid {
                id: grid
                width: parent.width
                columns: 3
                spacing: 15

                Repeater {
                    model: App.accounts
                    GlassCard {
                        id: accountCard
                        width: (grid.width - 30) / 3
                        height: 154
                        radius: 18
                        shadow: false
                        interactive: true

                        Rectangle {
                            anchors.fill: parent
                            radius: 18
                            gradient: Gradient {
                                GradientStop { position: 0; color: Theme.withAlpha(model.color, Theme.dark ? 0.16 : 0.12) }
                                GradientStop { position: 0.6; color: "transparent" }
                            }
                        }

                        HoverHandler { id: cardHover }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: accountBillsDialog.openFor(model.accountId, model.name, model.color)
                        }

                        Item {
                            anchors.fill: parent
                            anchors.margins: 15

                            CategoryIcon {
                                id: accountIcon
                                x: 0
                                y: 0
                                icon: model.icon
                                accentColor: model.color
                                size: 34
                                iconSize: 17
                            }

                            Column {
                                anchors.left: accountIcon.right
                                anchors.leftMargin: 9
                                anchors.top: parent.top
                                spacing: 2
                                Text {
                                    text: model.name
                                    color: Theme.ink
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 12
                                    font.weight: 600
                                }
                                Text {
                                    text: model.typeLabel
                                    color: Theme.faint
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 9
                                }
                            }

                            Row {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                spacing: 0
                                opacity: cardHover.hovered ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: 120 } }
                                IconButton { icon: "edit"; size: 26; iconSize: 14; hoverColor: Theme.surfaceRaised; onClicked: accountDialog.openFor(model.accountId) }
                                IconButton { icon: "delete"; size: 26; iconSize: 14; iconColor: Theme.muted; hoverColor: Theme.dangerSoft; onClicked: page.requestDelete(model.accountId, model.name) }
                            }

                            AmountText {
                                x: 0
                                y: 52
                                value: model.balance
                                amountSize: 23
                                amountWeight: 600
                                amountColor: model.balance < 0 ? Theme.danger : Theme.ink
                            }

                            Row {
                                anchors.left: parent.left
                                anchors.bottom: parent.bottom
                                spacing: 5
                                Text {
                                    text: "本月支出"
                                    color: Theme.faint
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 9
                                }
                                Text {
                                    text: App.money(model.monthExpense)
                                    color: Theme.muted
                                    font.family: Theme.numberFamily
                                    font.pixelSize: 9
                                }
                            }

                        }
                    }
                }

                GlassCard {
                    width: (grid.width - 30) / 3
                    height: 154
                    radius: 18
                    shadow: false
                    tint: Theme.surfaceSunken
                    borderColor: addMouse.containsMouse ? Theme.withAlpha(Theme.accent, 0.42) : Theme.hairlineStrong

                    Rectangle {
                        anchors.fill: parent
                        radius: 18
                        color: Theme.withAlpha(Theme.accent, 0.055)
                        opacity: addMouse.containsMouse ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 130 } }
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: 44
                        height: 44
                        radius: 15
                        color: Theme.accentWash
                        Icon { anchors.centerIn: parent; name: "add"; size: 22; iconColor: Theme.accent }
                    }

                    MouseArea {
                        id: addMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: accountDialog.openNew()
                    }
                }
            }
        }
    }

    AccountDialog {
        id: accountDialog
        onSaved: {}
    }

    AccountBillsDialog {
        id: accountBillsDialog
        onEditRequested: function(txId) {
            UiBus.openTransaction(txId)
        }
    }

    ConfirmDialog {
        id: deleteDialog
        title: page.pendingArchives ? "归档这个账户？" : "删除这个账户？"
        message: page.pendingArchives
                 ? "“" + page.pendingAccountName + "”下已有账单记录，账户会被归档，历史数据仍会保留。"
                 : "“" + page.pendingAccountName + "”还没有账单记录，可以直接删除。"
        confirmText: page.pendingArchives ? "归档" : "删除"
        danger: true
        onConfirmed: {
            if (App.deleteAccount(page.pendingAccountId))
                UiBus.toast(page.pendingArchives ? "账户已归档" : "账户已删除", "success")
            else
                UiBus.toast(App.lastError, "error")
        }
    }
}
