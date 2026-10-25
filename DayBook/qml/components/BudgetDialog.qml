import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import DayBook

Popup {
    id: root

    property int editingCategoryId: 0
    property int categoryId: 0
    property string errorText: ""
    property var options: []
    property var selectedCategory: null
    property bool customMode: false
    property string customName: ""

    signal saved()
    signal deleted()

    width: 466
    modal: true
    dim: false
    padding: 24
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
    exit: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 110 } }

    background: Item {
        Rectangle {
            id: panel
            anchors.fill: parent
            radius: 24
            color: Theme.surfaceSolid
            border.width: 1
            border.color: Theme.cardBorder
        }
    }

    function refreshOptions() {
        const all = App.budgetCategoryOptions()
        const list = []
        for (let i = 0; i < all.length; ++i) {
            if (editingCategoryId > 0 || !all[i].hasBudget)
                list.push(all[i])
        }
        options = list
        selectedCategory = null
        for (let i = 0; i < options.length; ++i) {
            if (options[i].id === categoryId) {
                selectedCategory = options[i]
                break
            }
        }
        if (selectedCategory === null && options.length > 0) {
            selectedCategory = options[0]
            categoryId = options[0].id
        }
    }

    function openNew() {
        editingCategoryId = 0
        categoryId = 0
        customMode = false
        customName = ""
        errorText = ""
        refreshOptions()
        open()
        Qt.callLater(function() {
            if (!amountField)
                return
            if (customNameField)
                customNameField.text = ""
            amountField.text = ""
            amountField.forceActiveFocus()
        })
    }

    function openFor(categoryIdValue) {
        editingCategoryId = categoryIdValue
        categoryId = categoryIdValue
        customMode = false
        customName = ""
        refreshOptions()
        errorText = ""
        open()
        Qt.callLater(function() {
            if (!amountField)
                return
            const category = selectedCategory
            amountField.text = category !== null && Number(category.budget) > 0
                               ? App.formatAmountInput(String(Math.round(Number(category.budget) * 100) / 100))
                               : ""
            amountField.forceActiveFocus()
        })
    }

    function save() {
        const typedName = customName !== ""
                          ? customName
                          : (customNameField ? String(customNameField.text) : "")
        const customNameValue = typedName.trim()
        const useCustom = editingCategoryId === 0 && customNameValue !== ""
        if (!useCustom && categoryId <= 0) {
            errorText = "请选择一个分类，或输入自定义对象"
            return
        }
        const amount = parseFloat(String(amountField.text).replace(/,/g, ""))
        if (!isFinite(amount) || amount <= 0) {
            errorText = "请输入大于 0 的预算金额"
            amountField.forceActiveFocus()
            return
        }
        const ok = useCustom
                   ? App.saveCustomBudget(customNameValue, amount, "more_horiz", "#918A81")
                   : App.saveBudget(categoryId, amount)
        if (!ok) {
            errorText = App.lastError
            return
        }
        UiBus.toast("预算已保存", "success")
        close()
        saved()
    }

    function removeBudget() {
        if (editingCategoryId <= 0)
            return
        if (App.deleteBudget(editingCategoryId)) {
            UiBus.toast("预算已移除", "success")
            close()
            deleted()
        } else {
            errorText = App.lastError
        }
    }

    contentItem: Column {
        width: root.width - 48
        spacing: 0

        Item {
            width: parent.width
            height: 32
            Text {
                anchors.left: parent.left
                anchors.right: closeButton.left
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: root.editingCategoryId > 0 ? "编辑预算" : "添加预算"
                color: Theme.ink
                font.family: Theme.fontFamily
                font.pixelSize: 20
                font.weight: 600
                elide: Text.ElideRight
            }
            IconButton {
                id: closeButton
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                icon: "close"
                size: 32
                iconSize: 17
                onClicked: root.close()
            }
        }

        Item { width: 1; height: 18 }

        Text {
            visible: root.editingCategoryId === 0
            text: "选择分类"
            color: Theme.inkSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 11
            font.weight: 600
        }
        Item { width: 1; height: 8; visible: root.editingCategoryId === 0 }

        Grid {
            visible: root.editingCategoryId === 0
            width: parent.width
            columns: 4
            spacing: 7
            Repeater {
                model: root.options
                Item {
                    width: (root.width - 48 - 21) / 4
                    height: 52
                    Rectangle {
                        anchors.fill: parent
                        radius: 12
                        color: root.categoryId === modelData.id ? Theme.categoryTint(modelData.color) : Theme.surfaceSunken
                        border.width: root.categoryId === modelData.id ? 1 : 0
                        border.color: Theme.withAlpha(modelData.color, 0.42)
                    }
                    Row {
                        anchors.centerIn: parent
                        spacing: 6
                        CategoryIcon {
                            icon: modelData.icon
                            accentColor: modelData.color
                            size: 26
                            iconSize: 14
                        }
                        Text {
                            text: modelData.name
                            color: root.categoryId === modelData.id ? Theme.ink : Theme.inkSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            font.weight: root.categoryId === modelData.id ? 600 : 500
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.categoryId = modelData.id
                            root.customMode = false
                            root.customName = ""
                            if (customNameField)
                                customNameField.text = ""
                        }
                    }
                }
            }
        }

        Item { width: 1; height: 12; visible: root.editingCategoryId === 0 }

        Text {
            visible: root.editingCategoryId === 0
            text: "或自定义对象"
            color: Theme.inkSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 11
            font.weight: 600
        }
        Item { width: 1; height: 7; visible: root.editingCategoryId === 0 }

        AppTextField {
            id: customNameField
            visible: root.editingCategoryId === 0
            width: parent.width
            height: 40
            icon: "edit"
            placeholderText: "输入自定义预算对象名称，例如：宠物、旅行"
            onTextEdited: {
                const value = String(customNameField.text).trim()
                root.customName = value
                root.customMode = value !== ""
                if (root.customMode) {
                    root.categoryId = 0
                    root.selectedCategory = null
                } else if (root.options.length > 0) {
                    root.selectedCategory = root.options[0]
                    root.categoryId = root.options[0].id
                }
                if (root.errorText !== "")
                    root.errorText = ""
            }
            onCleared: {
                root.customName = ""
                root.customMode = false
                if (root.options.length > 0) {
                    root.selectedCategory = root.options[0]
                    root.categoryId = root.options[0].id
                }
                if (root.errorText !== "")
                    root.errorText = ""
            }
        }

        Rectangle {
            visible: root.editingCategoryId > 0 && root.selectedCategory !== null
            width: parent.width
            height: 56
            radius: 13
            color: Theme.surfaceSunken
            Row {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10
                CategoryIcon {
                    icon: root.selectedCategory !== null ? root.selectedCategory.icon : "category"
                    accentColor: root.selectedCategory !== null ? root.selectedCategory.color : Theme.accent
                    size: 32
                    iconSize: 17
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        text: root.selectedCategory !== null ? root.selectedCategory.name : ""
                        color: Theme.ink
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        font.weight: 600
                    }
                    Text {
                        text: "本月已支出 " + App.money(root.selectedCategory !== null ? root.selectedCategory.spent : 0)
                        color: Theme.muted
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                    }
                }
            }
        }

        Text {
            visible: root.editingCategoryId === 0 && root.options.length === 0
            width: parent.width
            text: "所有支出分类都已经设置了预算。去明细里控制一下开销吧。"
            color: Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        Item { width: 1; height: 16 }

        Text {
            text: "每月预算"
            color: Theme.inkSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 11
            font.weight: 600
        }
        Item { width: 1; height: 8 }

        Rectangle {
            width: parent.width
            height: 78
            radius: 15
            color: Theme.surfaceSunken
            border.width: 1
            border.color: root.errorText !== "" ? Theme.withAlpha(Theme.danger, 0.4) : Theme.hairline
            Text {
                id: currencySymbol
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                text: "¥"
                color: Theme.muted
                font.family: Theme.numberFamily
                font.pixelSize: 22
            }
            TextField {
                id: amountField
                anchors.left: currencySymbol.right
                anchors.leftMargin: 8
                anchors.right: parent.right
                anchors.rightMargin: 112
                anchors.verticalCenter: parent.verticalCenter
                height: 44
                placeholderText: "0.00"
                color: Theme.ink
                placeholderTextColor: Theme.faint
                font.family: Theme.numberFamily
                font.pixelSize: 27
                font.weight: 600
                background: null
                padding: 0
                selectByMouse: true
                onTextEdited: {
                    const formatted = App.formatAmountInput(text)
                    if (formatted !== text)
                        text = formatted
                    if (root.errorText !== "")
                        root.errorText = ""
                }
                onAccepted: root.save()
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: {
                    if (root.selectedCategory === null)
                        return ""
                    const spent = Number(root.selectedCategory.spent) || 0
                    const entered = parseFloat(amountField.text)
                    if (!isFinite(entered) || entered <= 0)
                        return "已花 " + App.money(spent)
                    const left = entered - spent
                    return left >= 0 ? "还剩 " + App.money(left) : "已超 " + App.money(-left)
                }
                color: {
                    const entered = parseFloat(amountField.text)
                    if (isFinite(entered) && entered > 0 && entered - (Number(root.selectedCategory ? root.selectedCategory.spent : 0)) < 0)
                        return Theme.danger
                    return Theme.muted
                }
                font.family: Theme.fontFamily
                font.pixelSize: 10
            }
        }

        Text {
            width: parent.width
            text: root.errorText
            color: Theme.danger
            font.family: Theme.fontFamily
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
            visible: root.errorText !== ""
            topPadding: 6
        }

        Item { width: 1; height: 18 }

        Item {
            width: parent.width
            height: 38
            AppButton {
                visible: root.editingCategoryId > 0
                text: "移除"
                icon: "delete"
                kind: "danger"
                compact: true
                width: 82
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                onClicked: confirmRemove.open()
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
                    width: 96
                    enabled: root.categoryId > 0
                    onClicked: root.save()
                }
            }
        }
    }

    ConfirmDialog {
        id: confirmRemove
        title: "移除这个预算？"
        message: "移除后本月不再追踪该分类的预算，账单记录不会受影响。"
        confirmText: "移除"
        danger: true
        onConfirmed: root.removeBudget()
    }
}
