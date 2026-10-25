import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Effects
import DayBook

Popup {
    id: root

    property int editingId: 0
    property int typeIndex: 0
    property string selectedIcon: "wallet"
    property string selectedColor: "#E8A94E"
    property string errorText: ""
    property var accountColorDialog: null
    property var iconChoices: ["wallet", "account_balance", "credit_card", "phone_iphone", "savings", "payments", "storefront", "credit_score", "travel_explore"]
    property var colorChoices: ["#E8A94E", "#F26D50", "#3FBF7F", "#4F8EF7", "#8B7CF6", "#EC7FA9", "#D4AF37", "#B9C0C8"]

    signal saved()

    width: 486
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
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 110 } }

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

    function typeKey(index) {
        return ["cash", "wallet", "bank", "credit"][index] || "cash"
    }

    function typeIndexForKey(key) {
        const index = ["cash", "wallet", "bank", "credit"].indexOf(key)
        return index < 0 ? 0 : index
    }

    function openColorDialog() {
        if (accountColorDialog) {
            accountColorDialog.destroy()
            accountColorDialog = null
        }
        accountColorDialog = accountColorComponent.createObject(Overlay.overlay)
        if (accountColorDialog) {
            accountColorDialog.selectedColor = root.selectedColor
            accountColorDialog.open()
        }
    }

    function openNew() {
        editingId = 0
        typeIndex = 0
        selectedIcon = "wallet"
        selectedColor = "#E8A94E"
        errorText = ""
        open()
        Qt.callLater(function() {
            if (!nameField)
                return
            nameField.text = ""
            initialField.text = ""
            noteField.text = ""
            nameField.field.forceActiveFocus()
        })
    }

    function openFor(id) {
        const accounts = App.accountOptions()
        let account = null
        for (let i = 0; i < accounts.length; ++i) {
            if (accounts[i].id === id)
                account = accounts[i]
        }
        if (account === null)
            return
        editingId = id
        typeIndex = typeIndexForKey(account.type)
        selectedIcon = account.icon || "wallet"
        selectedColor = account.color || "#E8A94E"
        errorText = ""
        open()
        Qt.callLater(function() {
            if (!nameField)
                return
            nameField.text = account.name
            initialField.text = Math.abs(Number(account.initial)) < 0.001
                                ? ""
                                : App.formatAmountInput(Number(account.initial).toFixed(2))
            noteField.text = account.note || ""
            nameField.field.forceActiveFocus()
        })
    }

    function save() {
        if (String(nameField.text).trim() === "") {
            errorText = "给账户取个名字吧"
            nameField.field.forceActiveFocus()
            return
        }
        let initial = parseFloat(String(initialField.text).replace(/,/g, ""))
        if (!isFinite(initial))
            initial = 0
        const id = App.saveAccount({
            "id": editingId,
            "name": String(nameField.text).trim(),
            "type": typeKey(typeIndex),
            "icon": selectedIcon,
            "color": selectedColor,
            "initial": initial,
            "note": noteField.text
        })
        if (id < 0) {
            errorText = App.lastError
            return
        }
        UiBus.toast(editingId > 0 ? "账户已更新" : "账户已添加", "success")
        close()
        saved()
    }

    contentItem: Column {
        width: root.width - 48
        spacing: 0
        topPadding: 0

        Item {
            width: parent.width
            height: 32
            Text {
                anchors.left: parent.left
                anchors.right: closeButton.left
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: root.editingId > 0 ? "编辑账户" : "添加账户"
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

        AppTextField {
            id: nameField
            width: parent.width
            icon: "wallet"
            placeholderText: "账户名称，例如：招商银行"
            clearable: false
            onTextEdited: {
                if (root.errorText !== "")
                    root.errorText = ""
            }
        }

        Item { width: 1; height: 14 }

        Row {
            width: parent.width
            spacing: 12
            Column {
                id: typeColumn
                width: typeControl.implicitWidth
                spacing: 7
                Text {
                    text: "账户类型"
                    color: Theme.inkSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 11
                    font.weight: 600
                }
                SegmentedControl {
                    id: typeControl
                    width: implicitWidth
                    items: ["现金", "钱包", "储蓄卡", "信用卡"]
                    currentIndex: root.typeIndex
                    onActivated: function(index) { root.typeIndex = index }
                }
            }
            Column {
                width: parent.width - typeColumn.width - 12
                spacing: 7
                Text {
                    text: "初始余额"
                    color: Theme.inkSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 11
                    font.weight: 600
                }
                AppTextField {
                    id: initialField
                    width: parent.width
                    height: 34
                    icon: "currency_yuan"
                    placeholderText: "0.00"
                    textSize: 12
                    onTextEdited: function(value) {
                        const formatted = App.formatAmountInput(value)
                        if (formatted !== value)
                            initialField.text = formatted
                    }
                }
            }
        }

        Item { width: 1; height: 16 }

        Text {
            text: "图标"
            color: Theme.inkSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 11
            font.weight: 600
        }
        Item { width: 1; height: 8 }
        Row {
            width: parent.width
            spacing: 6
            Repeater {
                model: root.iconChoices
                Rectangle {
                    width: (parent.width - (root.iconChoices.length - 1) * 6) / root.iconChoices.length
                    height: 38
                    radius: 12
                    color: root.selectedIcon === modelData ? Theme.categoryTint(root.selectedColor) : Theme.surfaceSunken
                    border.width: root.selectedIcon === modelData ? 1 : 0
                    border.color: Theme.withAlpha(root.selectedColor, 0.5)
                    Icon {
                        anchors.centerIn: parent
                        name: modelData
                        size: 18
                        iconColor: root.selectedIcon === modelData ? root.selectedColor : Theme.muted
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.selectedIcon = modelData
                    }
                }
            }
        }

        Item { width: 1; height: 14 }

        Text {
            text: "颜色"
            color: Theme.inkSecondary
            font.family: Theme.fontFamily
            font.pixelSize: 11
            font.weight: 600
        }
        Item { width: 1; height: 8 }
        Row {
            spacing: 10
            Repeater {
                model: root.colorChoices
                Item {
                    width: 24
                    height: 24
                    Rectangle {
                        anchors.centerIn: parent
                        width: 22
                        height: 22
                        radius: 11
                        color: modelData
                        border.width: root.selectedColor === modelData ? 2 : 0
                        border.color: Theme.surfaceSolid
                    }
                    Rectangle {
                        visible: root.selectedColor === modelData
                        anchors.centerIn: parent
                        width: 30
                        height: 30
                        radius: 15
                        color: "transparent"
                        border.width: 1.5
                        border.color: modelData
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.selectedColor = modelData
                    }
                }
            }
            Item {
                width: 24
                height: 24
                Rectangle {
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    radius: 11
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#EE6C50" }
                        GradientStop { position: 0.25; color: "#E8B64C" }
                        GradientStop { position: 0.5; color: "#2FAE83" }
                        GradientStop { position: 0.75; color: "#4F8EF7" }
                        GradientStop { position: 1.0; color: "#A47BE8" }
                    }
                    Icon {
                        anchors.centerIn: parent
                        name: "palette"
                        size: 12
                        iconColor: "#FFFFFF"
                    }
                }
                Rectangle {
                    visible: root.colorChoices.indexOf(root.selectedColor) < 0
                    anchors.centerIn: parent
                    width: 30
                    height: 30
                    radius: 15
                    color: "transparent"
                    border.width: 1.5
                    border.color: root.selectedColor
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.openColorDialog()
                }
            }
        }

        Item { width: 1; height: 14 }

        AppTextField {
            id: noteField
            width: parent.width
            icon: "receipt_long"
            placeholderText: "备注（可选）"
        }

        Text {
            width: parent.width
            text: root.errorText
            color: Theme.danger
            font.family: Theme.fontFamily
            font.pixelSize: 11
            horizontalAlignment: Text.AlignRight
            visible: root.errorText !== ""
        }

        Item { width: 1; height: 18 }

        Item {
            width: parent.width
            height: 38
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
                    onClicked: root.save()
                }
            }
        }
    }

    Component {
        id: accountColorComponent
        ColorDialog {
            title: "选择账户颜色"
            selectedColor: root.selectedColor
            onAccepted: {
                root.selectedColor = App.colorHex(selectedColor)
                Qt.callLater(function() {
                    if (root.accountColorDialog) {
                        root.accountColorDialog.destroy()
                        root.accountColorDialog = null
                    }
                })
            }
        }
    }
}
