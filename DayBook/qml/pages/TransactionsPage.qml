import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import DayBook

Item {
    id: page

    property int pendingDeleteId: 0
    property string pendingDeleteName: ""
    property int kindIndex: App.transactions.kindFilter === "income" ? 2
                         : App.transactions.kindFilter === "expense" ? 1 : 0
    property var filterCategoryItems: App.transactions.kindFilter === "all"
                                      ? App.allCategoryOptions
                                      : App.categoryOptions(App.transactions.kindFilter)
    property bool accountView: false
    property int selectedAccountId: 0
    property real accountFlipScale: 1
    property var accountItems: App.accountOptions()

    function accountName(id) {
        for (let i = 0; i < accountItems.length; ++i) {
            if (accountItems[i].id === id)
                return accountItems[i].name
        }
        return "账户"
    }

    function ensureAccountSelection() {
        accountItems = App.accountOptions()
        if (accountItems.length === 0) {
            selectedAccountId = 0
            return
        }
        let found = false
        for (let i = 0; i < accountItems.length; ++i) {
            if (accountItems[i].id === selectedAccountId)
                found = true
        }
        if (!found)
            selectedAccountId = accountItems[0].id
        App.setAccountBillsAccount(selectedAccountId)
    }

    function toggleAccountView() {
        if (accountFlipScale < 0.99)
            return
        accountFlip.restart()
    }

    SequentialAnimation {
        id: accountFlip
        NumberAnimation { target: page; property: "accountFlipScale"; from: 1; to: 0; duration: 150; easing.type: Easing.InCubic }
        ScriptAction {
            script: {
                page.accountView = !page.accountView
                if (page.accountView)
                    page.ensureAccountSelection()
            }
        }
        NumberAnimation { target: page; property: "accountFlipScale"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
    }

    Component.onCompleted: {
        if (App.startupModal === "filter") {
            Qt.callLater(function() { page.openFilters() })
        } else if (App.startupModal === "filtertype") {
            Qt.callLater(function() { typeControl.activated(2) })
        } else if (App.startupModal === "accountbills") {
            Qt.callLater(function() {
                page.accountView = true
                page.accountItems = App.accountOptions()
                for (let i = 0; i < App.accounts.count; ++i) {
                    const row = App.accounts.get(i)
                    if (row.monthExpense > 0 || row.monthIncome > 0) {
                        page.selectedAccountId = row.id
                        break
                    }
                }
                page.ensureAccountSelection()
            })
        }
    }

    Shortcut {
        sequence: "Ctrl+F"
        onActivated: searchField.field.forceActiveFocus()
    }

    function openFilters() {
        filterPopup.open()
    }

    Column {
        anchors.fill: parent
        spacing: 14

        Item {
            width: parent.width
            height: 56

            Column {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Text {
                    text: "明细"
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: 25
                    font.weight: 600
                    font.letterSpacing: -0.3
                }
                Text {
                    text: App.transactions.hasActiveFilters
                          ? "筛选后 " + App.transactions.transactionCount + " 笔账单"
                          : "这个月，每一笔都有迹可循"
                    color: Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: 11
                }
            }

            MonthSwitcher {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Item {
            width: parent.width
            height: 42

            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 9

                AppTextField {
                    id: searchField
                    width: 236
                    height: 38
                    icon: "search"
                    placeholderText: "搜索分类、账户或备注"
                    onTextEdited: searchTimer.restart()
                    onCleared: searchTimer.restart()
                }

                SegmentedControl {
                    id: typeControl
                    width: implicitWidth
                    height: 38
                    controlHeight: 38
                    items: ["全部", "支出", "收入"]
                    currentIndex: page.kindIndex
                    onActivated: function(index) {
                        App.transactions.setKindFilter(index === 0 ? "all" : index === 1 ? "expense" : "income")
                    }
                }

                Item {
                    width: 38
                    height: 38
                    Rectangle {
                        anchors.fill: parent
                        radius: 11
                        color: filterMouse.containsMouse ? Theme.surfaceSunken : Theme.surfaceRaised
                        border.width: 1
                        border.color: Theme.cardBorder
                    }
                    Icon {
                        anchors.centerIn: parent
                        name: "tune"
                        size: 17
                        iconColor: (App.transactions.categoryFilter > 0 || App.transactions.accountFilter > 0) ? Theme.accent : Theme.inkSecondary
                    }
                    Rectangle {
                        visible: App.transactions.categoryFilter > 0 || App.transactions.accountFilter > 0
                        width: 6
                        height: 6
                        radius: 3
                        color: Theme.accent
                        anchors.right: parent.right
                        anchors.rightMargin: 7
                        anchors.top: parent.top
                        anchors.topMargin: 7
                    }
                    MouseArea {
                        id: filterMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: page.openFilters()
                    }
                }
            }

            Column {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Row {
                    spacing: 5
                    Rectangle { width: 5; height: 5; radius: 3; color: Theme.income; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        text: "收入 " + App.money(App.monthIncome)
                        color: Theme.inkSecondary
                        font.family: Theme.numberFamily
                        font.pixelSize: 10
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                Row {
                    spacing: 5
                    Rectangle { width: 5; height: 5; radius: 3; color: Theme.expense; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        text: "支出 " + App.money(App.monthExpense)
                        color: Theme.inkSecondary
                        font.family: Theme.numberFamily
                        font.pixelSize: 10
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }

        GlassCard {
            width: parent.width
            height: parent.height - 56 - 42 - 28
            radius: 20

            Item {
                anchors.fill: parent
                anchors.margins: 10
                transform: Scale {
                    origin.x: width / 2
                    xScale: page.accountFlipScale
                }

                Item {
                    id: listHeader
                    width: parent.width
                    height: 38
                    x: 8

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: page.accountView
                              ? "账户账单"
                              : (App.transactions.hasActiveFilters ? "筛选结果" : "本月账单")
                        color: Theme.ink
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        font.weight: 600
                    }
                    FlipToggle {
                        anchors.right: parent.right
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        label: page.accountView ? "全部" : "按账户"
                        onClicked: page.toggleAccountView()
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 82
                        anchors.verticalCenter: parent.verticalCenter
                        text: (page.accountView ? App.accountBills.transactionCount
                                                : App.transactions.transactionCount) + " 笔账单"
                        color: Theme.faint
                        font.family: Theme.numberFamily
                        font.pixelSize: 9
                    }
                }

                Flow {
                    id: accountSelector
                    visible: page.accountView
                    x: 8
                    y: 40
                    width: parent.width - 16
                    spacing: 6

                    Repeater {
                        model: page.accountItems
                        Rectangle {
                            height: 28
                            width: accountLabel.implicitWidth + 26
                            radius: 9
                            color: page.selectedAccountId === modelData.id ? Theme.accentSoft : Theme.surfaceSunken
                            border.width: page.selectedAccountId === modelData.id ? 1 : 0
                            border.color: Theme.withAlpha(Theme.accent, 0.4)
                            Row {
                                id: accountLabel
                                anchors.centerIn: parent
                                spacing: 5
                                Icon {
                                    name: modelData.icon
                                    size: 12
                                    iconColor: modelData.color
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: modelData.name
                                    color: page.selectedAccountId === modelData.id ? Theme.ink : Theme.inkSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 9
                                    font.weight: 500
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    page.selectedAccountId = modelData.id
                                    App.setAccountBillsAccount(modelData.id)
                                }
                            }
                        }
                    }
                }

                ListView {
                    id: list
                    anchors.fill: parent
                    anchors.topMargin: page.accountView
                                        ? 42 + accountSelector.implicitHeight + 8 : 42
                    clip: true
                    model: page.accountView ? App.accountBills : App.transactions
                    spacing: 0
                    cacheBuffer: 500
                    boundsBehavior: Flickable.OvershootBounds
                    property real monthPull: 0

                    onMovementStarted: monthPull = 0
                    onVerticalOvershootChanged: {
                        if (moving)
                            monthPull = Math.max(monthPull, Math.abs(verticalOvershoot))
                    }
                    onMovementEnded: {
                        if (monthPull > 34 && contentHeight > height + 4) {
                            if (atYEnd) {
                                App.shiftMonth(-1)
                                Qt.callLater(function() { list.positionViewAtEnd() })
                            } else if (atYBeginning) {
                                App.shiftMonth(1)
                                Qt.callLater(function() { list.positionViewAtBeginning() })
                            }
                        }
                        monthPull = 0
                    }

                    delegate: Item {
                        width: list.width
                        height: model.isHeader ? 38 : 58

                        Item {
                            visible: model.isHeader
                            width: parent.width
                            height: 38

                            Row {
                                x: 10
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 7
                                Text {
                                    text: model.dayLabel
                                    color: Theme.inkSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 11
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

                            Row {
                                anchors.right: parent.right
                                anchors.rightMargin: 14
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 10
                                Text {
                                    visible: model.dayIncome > 0
                                    text: "收入 " + App.money(model.dayIncome)
                                    color: Theme.income
                                    font.family: Theme.numberFamily
                                    font.pixelSize: 9
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    visible: model.dayExpense > 0
                                    text: "支出 " + App.money(model.dayExpense)
                                    color: Theme.muted
                                    font.family: Theme.numberFamily
                                    font.pixelSize: 9
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width - 16
                                x: 8
                                height: 1
                                color: Theme.hairline
                                opacity: 0.6
                            }
                        }

                        TransactionRow {
                            visible: !model.isHeader
                            width: parent.width
                            height: 58
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
                            onEditRequested: UiBus.openTransaction(model.txId)
                            onDeleteRequested: {
                                page.pendingDeleteId = model.txId
                                page.pendingDeleteName = model.categoryName
                                deleteDialog.open()
                            }
                        }
                    }
                }

                EmptyState {
                    visible: (page.accountView ? App.accountBills.transactionCount
                                               : App.transactions.transactionCount) === 0
                    anchors.centerIn: parent
                    width: parent.width
                    icon: page.accountView ? "receipt_long"
                                          : (App.transactions.hasActiveFilters ? "search" : "receipt_long")
                    title: page.accountView ? "该账户本月还没有账单"
                                           : (App.transactions.hasActiveFilters ? "没有找到相关账单" : "这个月还没有账单")
                    description: page.accountView
                                 ? "换一个账户看看，或者先记一笔。"
                                 : (App.transactions.hasActiveFilters
                                    ? "换一个关键词，或者把筛选条件清空看看。"
                                    : "记下第一笔，让这个月开始有迹可循。")
                    actionText: !page.accountView && App.transactions.hasActiveFilters ? "清除筛选" : "记一笔"
                    actionIcon: !page.accountView && App.transactions.hasActiveFilters ? "refresh" : "add"
                    onActionClicked: {
                        if (!page.accountView && App.transactions.hasActiveFilters) {
                            App.transactions.clearFilters()
                            searchField.text = ""
                        } else {
                            UiBus.openTransaction(0)
                        }
                    }
                }
            }
        }
    }

    Timer {
        id: searchTimer
        interval: 180
        onTriggered: App.transactions.setSearch(searchField.text)
    }

    Popup {
        id: filterPopup
        parent: page
        width: 396
        padding: 18
        modal: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        x: Math.max(10, page.width - width - 16)
        y: 118

        background: Item {
            Rectangle {
                id: panel
                anchors.fill: parent
                radius: 18
                color: Theme.surfaceSolid
                border.width: 1
                border.color: Theme.cardBorder
            }
        }

        contentItem: Column {
            width: filterPopup.width - 36
            spacing: 10

            Row {
                width: parent.width
                height: 22
                Text {
                    text: "筛选"
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: 14
                    font.weight: 600
                }
                Item { width: parent.width - 100; height: 1 }
                Text {
                    text: "清除全部"
                    color: Theme.accent
                    font.family: Theme.fontFamily
                    font.pixelSize: 10
                    font.weight: 500
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            App.transactions.clearFilters()
                            searchField.text = ""
                            filterPopup.close()
                        }
                    }
                }
            }

            Text {
                text: "分类"
                color: Theme.faint
                font.family: Theme.fontFamily
                font.pixelSize: 9
                font.weight: 600
            }

            Flow {
                width: parent.width
                spacing: 6
                Repeater {
                    model: page.filterCategoryItems
                    Rectangle {
                        height: 28
                        width: chipRow.implicitWidth + 18
                        radius: 9
                        color: App.transactions.categoryFilter === modelData.id ? Theme.categoryTint(modelData.color)
                             : Theme.surfaceSunken
                        border.width: App.transactions.categoryFilter === modelData.id ? 1 : 0
                        border.color: Theme.withAlpha(modelData.color, 0.42)

                        Row {
                            id: chipRow
                            anchors.centerIn: parent
                            spacing: 5
                            Icon {
                                name: modelData.icon
                                size: 13
                                iconColor: modelData.color
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: modelData.name
                                color: Theme.inkSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                App.transactions.setCategoryFilter(App.transactions.categoryFilter === modelData.id ? 0 : modelData.id)
                            }
                        }
                    }
                }
            }

            Rectangle { width: parent.width; height: 1; color: Theme.hairline }

            Text {
                text: "账户"
                color: Theme.faint
                font.family: Theme.fontFamily
                font.pixelSize: 9
                font.weight: 600
            }

            Flow {
                width: parent.width
                spacing: 6
                Repeater {
                    model: App.accountOptions()
                    Rectangle {
                        height: 28
                        width: accountChipRow.implicitWidth + 18
                        radius: 9
                        color: App.transactions.accountFilter === modelData.id ? Theme.accentSoft : Theme.surfaceSunken
                        border.width: App.transactions.accountFilter === modelData.id ? 1 : 0
                        border.color: Theme.withAlpha(Theme.accent, 0.4)
                        Row {
                            id: accountChipRow
                            anchors.centerIn: parent
                            spacing: 5
                            Icon {
                                name: modelData.icon
                                size: 13
                                iconColor: modelData.color
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: modelData.name
                                color: Theme.inkSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                App.transactions.setAccountFilter(App.transactions.accountFilter === modelData.id ? 0 : modelData.id)
                            }
                        }
                    }
                }
            }
        }
    }

    ConfirmDialog {
        id: deleteDialog
        title: "删除这笔账单？"
        message: "“" + page.pendingDeleteName + "”这笔记录会被永久删除，统计也会同步更新。"
        confirmText: "删除"
        danger: true
        onConfirmed: {
            if (App.deleteTransaction(page.pendingDeleteId))
                UiBus.toast("账单已删除", "success")
            else
                UiBus.toast(App.lastError, "error")
        }
    }
}
