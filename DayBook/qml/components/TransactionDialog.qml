import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import DayBook

Popup {
    id: root

    property int editingId: 0
    property string kind: "expense"
    property var categoryItems: []
    property var accountItems: []
    property int categoryId: 0
    property int accountId: 0
    property int transferAccountId: 0
    property string amountText: ""
    property string noteText: ""
    property string dateText: ""
    property string timeText: ""
    property string errorText: ""
    property date selectedDate: new Date()
    property int calYear: new Date().getFullYear()
    property int calMonth: new Date().getMonth()

    signal saved()
    signal deleted()

    width: 580
    modal: true
    dim: false
    padding: 24
    focus: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay
    Overlay.modal: Rectangle { color: "transparent" }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.965; to: 1; duration: 200; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
    }

    background: Item {
        Rectangle {
            id: dialogPanel
            anchors.fill: parent
            radius: 26
            color: Theme.surfaceSolid
            border.width: 1
            border.color: Theme.cardBorder
        }
    }

    function todayString() {
        return Qt.formatDate(new Date(), "yyyy-MM-dd")
    }

    function nowString() {
        return Qt.formatTime(new Date(), "HH:mm")
    }

    function reloadCategories() {
        categoryItems = App.categoryOptions(kind)
        if (categoryItems.length > 0) {
            let found = false
            for (let i = 0; i < categoryItems.length; ++i) {
                if (categoryItems[i].id === categoryId)
                    found = true
            }
            if (!found)
                categoryId = categoryItems[0].id
        } else {
            categoryId = 0
        }
    }

    function openNew() {
        editingId = 0
        kind = "expense"
        categoryId = 0
        accountId = 0
        transferAccountId = 0
        amountText = ""
        noteText = ""
        selectedDate = new Date()
        dateText = todayString()
        timeText = nowString()
        errorText = ""
        accountItems = App.accountOptions()
        if (accountItems.length > 0)
            accountId = accountItems[0].id
        if (App.startupModal === "transfer" && accountItems.length > 1) {
            kind = "transfer"
            for (let i = 0; i < accountItems.length; ++i) {
                if (accountItems[i].id !== accountId) {
                    transferAccountId = accountItems[i].id
                    break
                }
            }
        }
        reloadCategories()
        open()
        Qt.callLater(function() {
            amountField.text = root.amountText
            noteField.text = root.noteText
            timeField.text = root.timeText
            amountField.forceActiveFocus()
        })
    }

    function openFor(id) {
        const data = App.transactionById(id)
        if (data.id === undefined || data.id === 0)
            return
        editingId = id
        kind = data.kind
        categoryId = data.categoryId
        accountId = data.accountId
        transferAccountId = data.transferAccountId || 0
        amountText = App.formatAmountInput(Number(data.amount).toFixed(2))
        noteText = data.note || ""
        const parts = String(data.date).split("-")
        selectedDate = new Date(Number(parts[0]), Number(parts[1]) - 1, Number(parts[2]))
        dateText = data.date
        timeText = data.time
        errorText = ""
        accountItems = App.accountOptions()
        reloadCategories()
        open()
        Qt.callLater(function() {
            amountField.text = root.amountText
            noteField.text = root.noteText
            timeField.text = root.timeText
            amountField.forceActiveFocus()
        })
    }

    function openCalendar() {
        const parts = String(dateText).split("-")
        if (parts.length === 3) {
            calYear = Number(parts[0])
            calMonth = Number(parts[1]) - 1
        }
        calendar.open()
    }

    function save() {
        const amount = parseFloat(String(amountText).replace(/,/g, ""))
        if (!isFinite(amount) || amount <= 0) {
            errorText = "请输入大于 0 的金额"
            amountField.forceActiveFocus()
            return
        }
        if (kind === "transfer") {
            if (transferAccountId <= 0) {
                errorText = "请选择转入账户"
                return
            }
            if (transferAccountId === accountId) {
                errorText = "转出和转入账户不能相同"
                return
            }
        } else if (categoryId <= 0) {
            errorText = "请选择一个分类"
            return
        }
        if (accountId <= 0) {
            errorText = "请选择一个账户"
            return
        }
        const result = App.saveTransaction({
            "id": editingId,
            "kind": kind,
            "amount": amount,
            "categoryId": categoryId,
            "accountId": accountId,
            "transferAccountId": kind === "transfer" ? transferAccountId : 0,
            "note": noteText,
            "date": dateText,
            "time": timeText.length === 5 ? timeText : nowString()
        })
        if (result < 0) {
            errorText = App.lastError
            return
        }
        UiBus.toast(editingId > 0 ? "账单已更新"
                                  : (kind === "transfer" ? "已记录转账" : "已记下这笔"), "success")
        close()
        saved()
    }

    function deleteCurrent() {
        if (editingId <= 0)
            return
        if (App.deleteTransaction(editingId)) {
            UiBus.toast("账单已删除", "success")
            close()
            deleted()
        } else {
            errorText = App.lastError
        }
    }

    function monthName(m) {
        return (m + 1) + "月"
    }

    function daysInMonth(year, month) {
        return new Date(year, month + 1, 0).getDate()
    }

    function firstWeekday(year, month) {
        return (new Date(year, month, 1).getDay() + 6) % 7
    }

    contentItem: Column {
        width: root.width - 48
        spacing: 0

        Item {
            width: parent.width
            height: 38
            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8
                Text {
                    text: root.editingId > 0 ? "编辑账单" : "记一笔"
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: 21
                    font.weight: 600
                    anchors.verticalCenter: parent.verticalCenter
                }
                Rectangle {
                    visible: root.editingId > 0
                    width: 48
                    height: 20
                    radius: 10
                    color: Theme.surfaceSunken
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        anchors.centerIn: parent
                        text: "#" + root.editingId
                        color: Theme.faint
                        font.family: Theme.numberFamily
                        font.pixelSize: 9
                    }
                }
            }
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10
                SegmentedControl {
                    width: implicitWidth
                    height: 38
                    controlHeight: 38
                    anchors.verticalCenter: parent.verticalCenter
                    items: ["支出", "收入", "转移支付"]
                    currentIndex: root.kind === "expense" ? 0
                                : root.kind === "income" ? 1 : 2
                    onActivated: function(index) {
                        root.kind = index === 0 ? "expense"
                                  : index === 1 ? "income" : "transfer"
                        root.categoryId = 0
                        root.transferAccountId = 0
                        if (root.kind === "transfer") {
                            for (let i = 0; i < root.accountItems.length; ++i) {
                                if (root.accountItems[i].id !== root.accountId) {
                                    root.transferAccountId = root.accountItems[i].id
                                    break
                                }
                            }
                        }
                        root.reloadCategories()
                    }
                }
                IconButton {
                    icon: "close"
                    size: 32
                    iconSize: 17
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: root.close()
                }
            }
        }

        Item { width: 1; height: 18 }

        Rectangle {
            width: parent.width
            height: 108
            radius: 18
            color: Theme.surfaceSunken
            border.width: 1
            border.color: root.errorText !== "" ? Theme.withAlpha(Theme.danger, 0.4) : Theme.hairline

            Text {
                x: 20
                y: 13
                text: "金额"
                color: Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: 11
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.right: parent.right
                anchors.rightMargin: 20
                y: 37
                spacing: 8
                Text {
                    text: App.currencySymbol
                    color: root.kind === "income" ? Theme.income : Theme.muted
                    font.family: Theme.numberFamily
                    font.pixelSize: 26
                    font.weight: 500
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 5
                }
                TextField {
                    id: amountField
                    width: parent.width - 46
                    height: 54
                    placeholderText: "0.00"
                    color: Theme.ink
                    placeholderTextColor: Theme.faint
                    selectionColor: Theme.withAlpha(Theme.accent, 0.25)
                    font.family: Theme.numberFamily
                    font.pixelSize: 36
                    font.weight: 600
                    font.features: ({ "tnum": 1 })
                    verticalAlignment: TextInput.AlignVCenter
                    background: null
                    leftPadding: 0
                    selectByMouse: true
                    onTextEdited: {
                        const formatted = App.formatAmountInput(text)
                        if (formatted !== text)
                            text = formatted
                        root.amountText = formatted
                        if (root.errorText !== "")
                            root.errorText = ""
                    }
                    onAccepted: root.save()
                    onActiveFocusChanged: {
                        if (!activeFocus && text.length > 0) {
                            const normal = root.normalizeAmount(text)
                            root.amountText = App.formatAmountInput(normal)
                            text = root.amountText
                        }
                    }
                }
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 13
                text: root.errorText
                color: Theme.danger
                font.family: Theme.fontFamily
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }
        }

        Item { width: 1; height: 16 }

        Text {
            visible: root.kind !== "transfer"
            text: "分类"
            color: Theme.inkSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 12
            font.weight: 600
        }
        Item { width: 1; height: 8; visible: root.kind !== "transfer" }

        Grid {
            visible: root.kind !== "transfer"
            width: parent.width
            columns: 5
            spacing: 7
            Repeater {
                model: root.categoryItems
                Item {
                    width: (root.width - 48 - 28) / 5
                    height: 66
                    Rectangle {
                        anchors.fill: parent
                        radius: 13
                        color: root.categoryId === modelData.id ? Theme.categoryTint(modelData.color) : "transparent"
                        border.width: root.categoryId === modelData.id ? 1 : 0
                        border.color: Theme.withAlpha(modelData.color, 0.42)
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                    Column {
                        anchors.centerIn: parent
                        spacing: 5
                        CategoryIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            icon: modelData.icon
                            accentColor: modelData.color
                            size: 32
                            iconSize: 17
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.name
                            color: root.categoryId === modelData.id ? Theme.ink : Theme.inkSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            font.weight: root.categoryId === modelData.id ? 600 : 500
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.categoryId = modelData.id
                            if (root.errorText !== "")
                                root.errorText = ""
                        }
                    }
                }
            }
        }

        Item { width: 1; height: 14 }

        Text {
            text: root.kind === "transfer" ? "转出账户" : "账户"
            color: Theme.inkSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 12
            font.weight: 600
        }
        Item { width: 1; height: 8 }

        Flow {
            width: parent.width
            spacing: 7
            Repeater {
                model: root.accountItems
                Rectangle {
                    height: 32
                    width: accountRow.implicitWidth + 22
                    radius: 10
                    color: root.accountId === modelData.id ? Theme.accentSoft : Theme.surfaceSunken
                    border.width: root.accountId === modelData.id ? 1 : 0
                    border.color: Theme.withAlpha(Theme.accent, 0.45)
                    Row {
                        id: accountRow
                        anchors.centerIn: parent
                        spacing: 6
                        Icon {
                            name: modelData.icon
                            size: 14
                            iconColor: modelData.color
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: modelData.name
                            color: root.accountId === modelData.id ? Theme.ink : Theme.inkSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            font.weight: 500
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.accountId = modelData.id
                    }
                }
            }
        }

        Item { width: 1; height: 12; visible: root.kind === "transfer" }

        Text {
            visible: root.kind === "transfer"
            text: "转入账户"
            color: Theme.inkSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 12
            font.weight: 600
        }
        Item { width: 1; height: 8; visible: root.kind === "transfer" }

        Flow {
            visible: root.kind === "transfer"
            width: parent.width
            spacing: 7
            Repeater {
                model: root.accountItems
                Rectangle {
                    height: 32
                    width: transferAccountRow.implicitWidth + 22
                    radius: 10
                    color: root.transferAccountId === modelData.id ? Theme.accentSoft : Theme.surfaceSunken
                    border.width: root.transferAccountId === modelData.id ? 1 : 0
                    border.color: Theme.withAlpha(Theme.accent, 0.45)
                    Row {
                        id: transferAccountRow
                        anchors.centerIn: parent
                        spacing: 6
                        Icon {
                            name: modelData.icon
                            size: 14
                            iconColor: modelData.color
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: modelData.name
                            color: root.transferAccountId === modelData.id ? Theme.ink : Theme.inkSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            font.weight: 500
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.transferAccountId = modelData.id
                    }
                }
            }
        }

        Item { width: 1; height: 14 }

        Row {
            width: parent.width
            spacing: 10
            Rectangle {
                id: dateButton
                width: 205
                height: 42
                radius: 11
                color: Theme.surfaceSunken
                border.width: 1
                border.color: dateMouse.containsMouse ? Theme.withAlpha(Theme.accent, 0.35) : Theme.hairline
                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    Icon { name: "calendar_month"; size: 16; iconColor: Theme.muted; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        text: root.dateText
                        color: Theme.ink
                        font.family: Theme.numberFamily
                        font.pixelSize: 12
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                MouseArea {
                    id: dateMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        const parts = root.dateText.split("-")
                        root.calYear = Number(parts[0])
                        root.calMonth = Number(parts[1]) - 1
                        calendar.open()
                    }
                }
            }

            Rectangle {
                width: 82
                height: 42
                radius: 11
                color: Theme.surfaceSunken
                border.width: 1
                border.color: Theme.hairline
                Row {
                    anchors.centerIn: parent
                    spacing: 5
                    Icon { name: "schedule"; size: 15; iconColor: Theme.muted; anchors.verticalCenter: parent.verticalCenter }
                    TextField {
                        id: timeField
                        width: 42
                        height: 28
                        color: Theme.ink
                        font.family: Theme.numberFamily
                        font.pixelSize: 11
                        background: null
                        padding: 0
                        horizontalAlignment: TextInput.AlignHCenter
                        inputMask: "99:99"
                        onTextEdited: root.timeText = text
                        onAccepted: root.save()
                    }
                }
            }

            AppTextField {
                id: noteField
                width: parent.width - 205 - 82 - 20
                height: 42
                icon: "receipt_long"
                placeholderText: "写点什么…（可选）"
                onTextEdited: root.noteText = text
                onAccepted: root.save()
            }
        }

        Item { width: 1; height: 20 }

        Item {
            width: parent.width
            height: 38
            AppButton {
                visible: root.editingId > 0
                text: "删除"
                icon: "delete"
                kind: "danger"
                compact: true
                width: 82
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                onClicked: confirmDelete.open()
            }
            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 9
                AppButton {
                    text: "取消"
                    kind: "secondary"
                    compact: true
                    width: 76
                    onClicked: root.close()
                }
                AppButton {
                    text: "保存"
                    icon: "check"
                    compact: false
                    width: 96
                    onClicked: root.save()
                }
            }
        }
    }

    function normalizeAmount(value) {
        const number = parseFloat(String(value).replace(/,/g, ""))
        if (!isFinite(number))
            return ""
        return String(Math.round(number * 100) / 100)
    }

    Popup {
        id: calendar
        parent: dateButton
        width: 294
        x: 0
        y: -calendar.height - 8
        padding: 14
        modal: false
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Item {
            Rectangle {
                id: calPanel
                anchors.fill: parent
                radius: 16
                color: Theme.surfaceSolid
                border.width: 1
                border.color: Theme.cardBorder
            }
        }

        contentItem: Column {
            width: calendar.width - 28
            spacing: 8
            Row {
                width: parent.width
                height: 26
                IconButton { icon: "chevron_left"; size: 26; iconSize: 16; onClicked: {
                    root.calMonth--
                    if (root.calMonth < 0) { root.calMonth = 11; root.calYear-- }
                }}
                Text {
                    width: parent.width - 52
                    text: root.calYear + "年 " + root.monthName(root.calMonth)
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    font.weight: 600
                    horizontalAlignment: Text.AlignHCenter
                    anchors.verticalCenter: parent.verticalCenter
                }
                IconButton { icon: "chevron_right"; size: 26; iconSize: 16; onClicked: {
                    root.calMonth++
                    if (root.calMonth > 11) { root.calMonth = 0; root.calYear++ }
                }}
            }
            Row {
                width: parent.width
                Repeater {
                    model: ["一", "二", "三", "四", "五", "六", "日"]
                    Text {
                        width: parent.width / 7
                        text: modelData
                        color: Theme.faint
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
            Grid {
                width: parent.width
                columns: 7
                spacing: 2
                Repeater {
                    model: 42
                    Item {
                        property int dayNumber: index - root.firstWeekday(root.calYear, root.calMonth) + 1
                        property bool valid: dayNumber >= 1 && dayNumber <= root.daysInMonth(root.calYear, root.calMonth)
                        property bool isSelected: {
                            if (!valid) return false
                            const d = new Date(root.calYear, root.calMonth, dayNumber)
                            return d.getFullYear() === root.selectedDate.getFullYear()
                                && d.getMonth() === root.selectedDate.getMonth()
                                && d.getDate() === root.selectedDate.getDate()
                        }
                        property bool isToday: {
                            if (!valid) return false
                            const now = new Date()
                            return now.getFullYear() === root.calYear && now.getMonth() === root.calMonth && now.getDate() === dayNumber
                        }
                        width: (calendar.width - 28 - 12) / 7
                        height: 30
                        Rectangle {
                            anchors.centerIn: parent
                            width: 27
                            height: 27
                            radius: 9
                            color: parent.isSelected ? Theme.accent : (calMouse.containsMouse && parent.valid ? Theme.surfaceSunken : "transparent")
                            border.width: parent.isToday && !parent.isSelected ? 1 : 0
                            border.color: Theme.withAlpha(Theme.accent, 0.5)
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: parent.valid
                            text: parent.dayNumber
                            color: parent.isSelected ? Theme.accentContrast : (parent.isToday ? Theme.accent : Theme.inkSecondary)
                            font.family: Theme.numberFamily
                            font.pixelSize: 11
                            font.weight: parent.isSelected || parent.isToday ? 600 : 400
                        }
                        MouseArea {
                            id: calMouse
                            anchors.fill: parent
                            enabled: parent.valid
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                const d = new Date(root.calYear, root.calMonth, parent.dayNumber)
                                root.selectedDate = d
                                root.dateText = Qt.formatDate(d, "yyyy-MM-dd")
                                calendar.close()
                            }
                        }
                    }
                }
            }
            Rectangle { width: parent.width; height: 1; color: Theme.hairline }
            Item {
                width: parent.width
                height: 24
                Text {
                    text: "今天"
                    color: Theme.accent
                    font.family: Theme.fontFamily
                    font.pixelSize: 11
                    font.weight: 600
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: Qt.formatDate(new Date(), "yyyy-MM-dd")
                    color: Theme.faint
                    font.family: Theme.numberFamily
                    font.pixelSize: 10
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        const now = new Date()
                        root.selectedDate = now
                        root.dateText = Qt.formatDate(now, "yyyy-MM-dd")
                        root.calYear = now.getFullYear()
                        root.calMonth = now.getMonth()
                        calendar.close()
                    }
                }
            }
        }
    }

    ConfirmDialog {
        id: confirmDelete
        title: "删除这笔账单？"
        message: "删除后无法恢复，本月统计也会同步更新。"
        confirmText: "删除"
        danger: true
        onConfirmed: root.deleteCurrent()
    }
}
