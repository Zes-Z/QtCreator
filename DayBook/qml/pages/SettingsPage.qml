import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Dialogs
import DayBook

Item {
    id: page

    property var accentChoices: [
        { "key": "coral", "color": "#EE6C50", "name": "珊瑚" },
        { "key": "sage", "color": "#2FAE83", "name": "鼠尾草" },
        { "key": "azure", "color": "#4F8EF7", "name": "晴空" },
        { "key": "amber", "color": "#E5A13C", "name": "琥珀" },
        { "key": "plum", "color": "#A47BE8", "name": "梅紫" },
        { "key": "pollen", "color": "#F2A7BD", "name": "花粉" },
        { "key": "custom", "color": "#E85D75", "name": "调色盘", "isPalette": true }
    ]
    property int themeIndex: App.themeMode === "light" ? 0 : App.themeMode === "dark" ? 1 : 2
    property var accentDialog: null
    Component.onCompleted: {
        if (App.startupModal === "palette")
            Qt.callLater(openAccentDialog)
        else if (App.startupModal === "clear")
            Qt.callLater(function() { clearDialog.open() })
    }
    property var currencyChoices: [
        { "code": "CNY", "name": "人民币", "symbol": "¥" },
        { "code": "USD", "name": "美元", "symbol": "$" },
        { "code": "EUR", "name": "欧元", "symbol": "€" },
        { "code": "JPY", "name": "日元", "symbol": "¥" },
        { "code": "GBP", "name": "英镑", "symbol": "£" },
        { "code": "HKD", "name": "港币", "symbol": "HK$" }
    ]

    function openAccentDialog() {
        if (accentDialog) {
            accentDialog.destroy()
            accentDialog = null
        }
        accentDialog = accentDialogComponent.createObject(page)
        if (accentDialog) {
            accentDialog.selectedColor = App.customAccentColor
            accentDialog.open()
        }
    }

    function currencyName(code) {
        for (let i = 0; i < currencyChoices.length; ++i) {
            if (currencyChoices[i].code === code)
                return currencyChoices[i].name
        }
        return "人民币"
    }

    function fontLabel(key) {
        const options = App.fontOptions || []
        for (let i = 0; i < options.length; ++i) {
            if (options[i].key === key)
                return options[i].label
        }
        return "系统"
    }

    function openUrl(url) {
        Qt.openUrlExternally(url)
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: height
        clip: true
        interactive: false
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: content
            width: flick.width
            y: 0
            spacing: 15

            Item {
                width: parent.width
                height: 58
                Column {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    Text {
                        text: "设置"
                        color: Theme.ink
                        font.family: Theme.fontFamily
                        font.pixelSize: 25
                        font.weight: 600
                        font.letterSpacing: -0.3
                    }
                    Text {
                        text: "让这方小账本，更像你自己的样子。"
                        color: Theme.muted
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                    }
                }
                Tag {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: "v" + App.version
                    icon: "verified_user"
                    tagColor: Theme.muted
                    bgColor: Theme.surfaceSunken
                }
            }

            Row {
                width: parent.width
                height: 234
                spacing: 15

                GlassCard {
                    width: parent.width - 380
                    height: parent.height
                    radius: 20
                    shadow: false

                    Item {
                        anchors.fill: parent
                        anchors.margins: 20

                        Text {
                            text: "外观"
                            color: Theme.ink
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            font.weight: 600
                        }
                        Text {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.topMargin: 22
                            text: "选择你喜欢的明暗与颜色"
                            color: Theme.faint
                            font.family: Theme.fontFamily
                            font.pixelSize: 9
                        }

                        Rectangle {
                            id: fontButton
                            anchors.top: parent.top
                            anchors.right: parent.right
                            width: 112
                            height: 28
                            radius: 9
                            color: fontMouse.containsMouse ? Theme.surfaceRaised : Theme.surfaceSunken
                            border.width: 1
                            border.color: fontMouse.containsMouse ? Theme.withAlpha(Theme.accent, 0.24) : Theme.hairline
                            Behavior on color { ColorAnimation { duration: 120 } }

                            Row {
                                anchors.centerIn: parent
                                spacing: 5
                                Text {
                                    text: "Aa"
                                    color: Theme.muted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 11
                                    font.weight: 600
                                }
                                Text {
                                    text: page.fontLabel(App.fontChoice)
                                    color: Theme.inkSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 9
                                }
                                Icon { name: "arrow_downward"; size: 10; iconColor: Theme.faint }
                            }

                            MouseArea {
                                id: fontMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: fontPopup.open()
                            }

                            Popup {
                                id: fontPopup
                                parent: fontButton
                                width: 150
                                padding: 6
                                modal: false
                                dim: false
                                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                                x: fontButton.width - width
                                y: fontButton.height + 6

                                background: Rectangle {
                                    radius: 13
                                    color: Theme.surfaceSolid
                                    border.width: 1
                                    border.color: Theme.cardBorder
                                }

                                contentItem: Column {
                                    width: fontPopup.width - 12
                                    spacing: 2
                                    Repeater {
                                        model: App.fontOptions
                                        Item {
                                            width: parent.width
                                            height: 32
                                            Rectangle {
                                                anchors.fill: parent
                                                radius: 8
                                                color: fontOptionMouse.containsMouse ? Theme.surfaceSunken : "transparent"
                                            }
                                            Text {
                                                anchors.left: parent.left
                                                anchors.leftMargin: 9
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: modelData.label
                                                color: App.fontChoice === modelData.key ? Theme.ink : Theme.inkSecondary
                                                font.family: modelData.family
                                                font.pixelSize: 10
                                                font.weight: App.fontChoice === modelData.key ? 600 : 500
                                            }
                                            Text {
                                                anchors.right: parent.right
                                                anchors.rightMargin: 9
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: "Aa"
                                                color: Theme.faint
                                                font.family: modelData.family
                                                font.pixelSize: 9
                                            }
                                            MouseArea {
                                                id: fontOptionMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    App.setFontChoice(modelData.key)
                                                    fontPopup.close()
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Row {
                            anchors.top: parent.top
                            anchors.topMargin: 52
                            width: parent.width
                            spacing: 12
                            Column {
                                width: parent.width
                                spacing: 7
                                Text {
                                    text: "主题"
                                    color: Theme.inkSecondary
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 10
                                    font.weight: 600
                                }
                                Item {
                                    width: parent.width
                                    height: 34
                                    SegmentedControl {
                                        width: implicitWidth
                                        height: 34
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        items: ["浅色", "深色", "跟随系统"]
                                        currentIndex: page.themeIndex
                                        onActivated: function(index) {
                                            App.setThemeMode(index === 0 ? "light" : index === 1 ? "dark" : "system")
                                        }
                                    }
                                }
                            }

                        }

                        Column {
                            anchors.left: parent.left
                            anchors.bottom: parent.bottom
                            spacing: 7
                            Text {
                                text: "强调色"
                                color: Theme.inkSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                font.weight: 600
                            }
                            Row {
                                spacing: 10
                                Repeater {
                                    model: page.accentChoices
                                    Item {
                                        width: 34
                                        height: 42
                                        Rectangle {
                                            visible: !modelData.isPalette
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            y: 0
                                            width: 26
                                            height: 26
                                            radius: 13
                                            color: modelData.color
                                            border.width: App.accentName === modelData.key ? 2 : 0
                                            border.color: Theme.surfaceSolid
                                        }
                                        Rectangle {
                                            visible: modelData.isPalette === true
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            y: 0
                                            width: 26
                                            height: 26
                                            radius: 13
                                            gradient: Gradient {
                                                GradientStop { position: 0.0; color: "#EE6C50" }
                                                GradientStop { position: 0.25; color: "#E8B64C" }
                                                GradientStop { position: 0.5; color: "#2FAE83" }
                                                GradientStop { position: 0.75; color: "#4F8EF7" }
                                                GradientStop { position: 1.0; color: "#A47BE8" }
                                            }
                                            border.width: App.accentName === modelData.key ? 2 : 0
                                            border.color: Theme.surfaceSolid
                                            Icon {
                                                anchors.centerIn: parent
                                                name: "palette"
                                                size: 13
                                                iconColor: "#FFFFFF"
                                            }
                                        }
                                        Rectangle {
                                            visible: App.accentName === modelData.key
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            y: -4
                                            width: 34
                                            height: 34
                                            radius: 17
                                            color: "transparent"
                                            border.width: 1.5
                                            border.color: modelData.isPalette === true ? App.customAccentColor : modelData.color
                                        }
                                        Text {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            anchors.bottom: parent.bottom
                                            text: modelData.name
                                            color: App.accentName === modelData.key ? Theme.ink : Theme.faint
                                            font.family: Theme.fontFamily
                                            font.pixelSize: 8
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (modelData.isPalette === true) {
                                                    page.openAccentDialog()
                                                } else {
                                                    App.setAccentName(modelData.key)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                GlassCard {
                    width: 365
                    height: parent.height
                    radius: 20
                    shadow: false

                    Item {
                        anchors.fill: parent
                        anchors.margins: 20

                        Text {
                            text: "数字与货币"
                            color: Theme.ink
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            font.weight: 600
                        }
                        Text {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.topMargin: 22
                            text: "金额显示与流通货币"
                            color: Theme.faint
                            font.family: Theme.fontFamily
                            font.pixelSize: 9
                        }

                        Column {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.topMargin: 52
                            spacing: 7
                            Text {
                                text: "货币"
                                color: Theme.inkSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                font.weight: 600
                            }
                            Rectangle {
                                id: currencyButton
                                width: 150
                                height: 36
                                radius: 11
                                color: currencyMouse.containsMouse ? Theme.surfaceRaised : Theme.surfaceSunken
                                border.width: 1
                                border.color: currencyMouse.containsMouse ? Theme.withAlpha(Theme.accent, 0.28) : Theme.hairline
                                Behavior on color { ColorAnimation { duration: 120 } }

                                Row {
                                    anchors.centerIn: parent
                                    spacing: 6
                                    Text {
                                        text: page.currencyName(App.currencyCode) + "  " + App.currencySymbol
                                        color: Theme.inkSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: 10
                                    }
                                    Icon { name: "arrow_downward"; size: 11; iconColor: Theme.faint }
                                }

                                MouseArea {
                                    id: currencyMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: currencyPopup.open()
                                }

                                Popup {
                                    id: currencyPopup
                                    parent: currencyButton
                                    width: 150
                                    padding: 6
                                    modal: false
                                    dim: false
                                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                                    x: currencyButton.width - width
                                    y: currencyButton.height + 6

                                    background: Rectangle {
                                        radius: 13
                                        color: Theme.surfaceSolid
                                        border.width: 1
                                        border.color: Theme.cardBorder
                                    }

                                    contentItem: Column {
                                        width: currencyPopup.width - 12
                                        spacing: 2
                                        Repeater {
                                            model: page.currencyChoices
                                            Item {
                                                width: parent.width
                                                height: 32
                                                Rectangle {
                                                    anchors.fill: parent
                                                    radius: 8
                                                    color: optionMouse.containsMouse ? Theme.surfaceSunken : "transparent"
                                                }
                                                Text {
                                                    anchors.left: parent.left
                                                    anchors.leftMargin: 9
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: modelData.name
                                                    color: App.currencyCode === modelData.code ? Theme.ink : Theme.inkSecondary
                                                    font.family: Theme.fontFamily
                                                    font.pixelSize: 10
                                                    font.weight: App.currencyCode === modelData.code ? 600 : 500
                                                }
                                                Text {
                                                    anchors.right: parent.right
                                                    anchors.rightMargin: 9
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    text: modelData.symbol
                                                    color: Theme.faint
                                                    font.family: Theme.numberFamily
                                                    font.pixelSize: 9
                                                }
                                                MouseArea {
                                                    id: optionMouse
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        App.setCurrencyCode(modelData.code)
                                                        currencyPopup.close()
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            GlassCard {
                width: parent.width
                height: 300
                radius: 20
                shadow: false

                Item {
                    anchors.fill: parent
                    anchors.margins: 20

                    Text {
                        text: "数据与存储"
                        color: Theme.ink
                        font.family: Theme.fontFamily
                        font.pixelSize: 13
                        font.weight: 600
                    }
                    Tag {
                        visible: App.isDemoData
                        anchors.right: parent.right
                        anchors.top: parent.top
                        text: "示例数据"
                        icon: "info"
                        tagColor: Theme.warning
                        bgColor: Theme.withAlpha(Theme.warning, 0.12)
                    }
                    Text {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.topMargin: 22
                        text: "账单存储在本机 SQLite 数据库中，随时可以带走。"
                        color: Theme.faint
                        font.family: Theme.fontFamily
                        font.pixelSize: 9
                    }

                    Column {
                        width: parent.width
                        anchors.top: parent.top
                        anchors.topMargin: 46
                        spacing: 0

                        SettingRow {
                            icon: "download"
                            title: "导出账单"
                            description: "导出全部账单为 CSV，可用 Excel 或 Numbers 打开"
                            actionText: "导出"
                            onClicked: exportDialog.open()
                        }
                        SettingRow {
                            icon: "folder_open"
                            title: "打开数据目录"
                            description: App.dataPath
                            actionText: "打开"
                            onClicked: App.openDataFolder()
                        }
                        SettingRow {
                            icon: "autorenew"
                            title: "载入示例数据"
                            description: "用一份示例账单替换当前数据，方便重新体验"
                            actionText: "载入"
                            onClicked: demoDialog.open()
                        }
                        SettingRow {
                            icon: "delete_forever"
                            title: "清空所有账单"
                            description: "删除全部账单与预算，账户和分类会保留"
                            actionText: "清空"
                            danger: true
                            onClicked: clearDialog.open()
                        }
                    }
                }
            }
        }
    }

    component SettingRow: Item {
        id: row
        property string icon: "receipt_long"
        property string title: ""
        property string description: ""
        property string actionText: ""
        property bool danger: false
        signal clicked()

        width: parent.width
        height: 52

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: -8
            anchors.rightMargin: -8
            radius: 10
            color: rowMouse.containsMouse ? Theme.surfaceSunken : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }
        }

        Rectangle {
            width: 28
            height: 28
            radius: 9
            color: row.danger ? Theme.dangerSoft : Theme.categoryTint(Theme.accent)
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            Icon {
                anchors.centerIn: parent
                name: row.icon
                size: 14
                iconColor: row.danger ? Theme.danger : Theme.accent
            }
        }

        Column {
            anchors.left: parent.left
            anchors.leftMargin: 40
            anchors.right: actionLabel.left
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                width: parent.width
                text: row.title
                color: row.danger ? Theme.danger : Theme.ink
                font.family: Theme.fontFamily
                font.pixelSize: 11
                font.weight: 600
            }
            Text {
                width: parent.width
                text: row.description
                color: Theme.faint
                font.family: Theme.fontFamily
                font.pixelSize: 8
                elide: Text.ElideMiddle
            }
        }

        Text {
            id: actionLabel
            anchors.right: chevron.left
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            text: row.actionText
            color: row.danger ? Theme.danger : Theme.accent
            font.family: Theme.fontFamily
            font.pixelSize: 9
            font.weight: 500
        }
        Icon {
            id: chevron
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            name: "chevron_right"
            size: 13
            iconColor: row.danger ? Theme.withAlpha(Theme.danger, 0.5) : Theme.faint
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.hairline
            opacity: 0.6
        }

        MouseArea {
            id: rowMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: row.clicked()
        }
    }

    Component {
        id: accentDialogComponent
        ColorDialog {
            title: "选择强调色"
            selectedColor: App.customAccentColor
            onAccepted: {
                App.setCustomAccent(selectedColor)
                Qt.callLater(function() {
                    if (page.accentDialog) {
                        page.accentDialog.destroy()
                        page.accentDialog = null
                    }
                })
            }
        }
    }

    FileDialog {
        id: exportDialog
        title: "导出账单"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV 文件 (*.csv)"]
        defaultSuffix: "csv"
        onAccepted: {
            const path = App.exportCsvTo(selectedFile.toString())
            if (path !== "")
                UiBus.toast("账单已导出", "success")
            else
                UiBus.toast(App.lastError, "error")
        }
    }

    ConfirmDialog {
        id: demoDialog
        title: "载入示例数据？"
        message: "当前的账单和预算会被示例数据替换，账户分类会保留。此操作无法撤销。"
        confirmText: "载入"
        danger: false
        onConfirmed: {
            if (App.loadDemoData())
                UiBus.toast("示例数据已载入", "success")
            else
                UiBus.toast(App.lastError, "error")
        }
    }

    ConfirmDialog {
        id: clearDialog
        title: "清空所有账单？"
        message: "所有账单和预算都会被删除，账户与分类会保留。\n建议先导出。"
        confirmText: "清空"
        danger: true
        onConfirmed: {
            if (App.clearAllData())
                UiBus.toast("账单已清空", "success")
            else
                UiBus.toast(App.lastError, "error")
        }
    }
}
