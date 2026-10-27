import QtQuick
import DayBook

GlassCard {
    id: root

    property bool compact: false
    property bool showActions: false
    property bool showAddRow: false
    property int rowHeight: compact ? 46 : 60

    signal editRequested(int categoryId)
    signal deleteRequested(int categoryId, string name)
    signal addRequested()
    signal rowClicked(int categoryId)

    function scrollToBottom() {
        listScroll.contentY = Math.max(0, listScroll.contentHeight - listScroll.height)
    }

    Item {
        anchors.fill: parent
        anchors.margins: root.compact ? 14 : 16

        Item {
            id: listHeader
            width: parent.width
            height: root.compact ? 30 : 34

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
            id: listScroll
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: listHeader.bottom
            anchors.bottom: parent.bottom
            clip: true
            contentWidth: width
            contentHeight: rowsColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            interactive: true

            Column {
                id: rowsColumn
                width: listScroll.width
                spacing: 0

                Repeater {
                    model: App.budgets
                    BudgetCategoryRow {
                        width: rowsColumn.width
                        rowHeight: root.rowHeight
                        categoryId: model.categoryId
                        name: model.name
                        iconName: model.icon
                        accentColor: model.color
                        spent: model.spent
                        budget: model.budget
                        remaining: model.remaining
                        percent: model.percent
                        over: model.over
                        showActions: root.showActions
                        showDivider: index !== App.budgets.count - 1
                        onEditRequested: root.editRequested(model.categoryId)
                        onDeleteRequested: root.deleteRequested(model.categoryId, model.name)
                        onClicked: root.rowClicked(model.categoryId)
                    }
                }

                Item {
                    id: addBudgetRow
                    visible: root.showAddRow && App.budgets.count > 0
                    width: rowsColumn.width
                    height: root.showAddRow ? 48 : 0

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
                        onClicked: root.addRequested()
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
            onActionClicked: root.addRequested()
        }
    }
}
