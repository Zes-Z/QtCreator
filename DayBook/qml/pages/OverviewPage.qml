import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import DayBook

Item {
    id: page

    property int hoveredDonut: -1
    property var donutSegments: buildDonutSegments(App.categoryBreakdown)
    property real donutSize: Math.min(220, Math.max(120, bottomRow.height - 70))

    Component.onCompleted: {
        if (App.startupModal === "month")
            Qt.callLater(function() { monthSwitcher.openPicker() })
    }

    function greetingText() {
        const h = new Date().getHours()
        if (h < 5) return "夜深了"
        if (h < 9) return "早上好"
        if (h < 12) return "上午好"
        if (h < 14) return "中午好"
        if (h < 18) return "下午好"
        return "晚上好"
    }

    function buildDonutSegments(list) {
        const result = []
        let rest = 0.0
        for (let i = 0; i < list.length; ++i) {
            if (i < 4) {
                result.push(list[i])
            } else {
                rest += Number(list[i].amount) || 0
            }
        }
        if (rest > 0.005) {
            result.push({ "name": "其他", "amount": rest, "color": "#B7AEA1", "icon": "more_horiz", "percent": 0 })
        }
        return result
    }

    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 50

        Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            Text {
                text: page.greetingText()
                color: Theme.ink
                font.family: Theme.fontFamily
                font.pixelSize: 24
                font.weight: 600
                font.letterSpacing: -0.3
            }
            Text {
                text: "把每一笔日常，都过成喜欢的样子。"
                color: Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: 10
            }
        }

        MonthSwitcher {
            id: monthSwitcher
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Row {
        id: heroRow
        anchors.top: header.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        height: 150
        spacing: 14

        Item {
            id: hero
            width: parent.width
            height: parent.height

            Rectangle {
                id: heroPanel
                anchors.fill: parent
                radius: 20
                gradient: Gradient {
                    GradientStop { position: 0; color: Theme.heroA }
                    GradientStop { position: 1; color: Theme.heroB }
                }
                border.width: 1
                border.color: Theme.heroHairline
            }

            Canvas {
                anchors.fill: parent
                opacity: 0.9
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = "rgba(255,255,255,0.055)"
                    ctx.lineWidth = 1
                    for (let i = 0; i < 3; ++i) {
                        ctx.beginPath()
                        ctx.arc(width * 0.86, height * 0.18, 66 + i * 48, 0, Math.PI * 2)
                        ctx.stroke()
                    }
                    const glow = ctx.createRadialGradient(width * 0.84, height * 0.16, 0, width * 0.84, height * 0.16, 180)
                    glow.addColorStop(0, "rgba(255,171,133,0.20)")
                    glow.addColorStop(1, "rgba(255,171,133,0)")
                    ctx.fillStyle = glow
                    ctx.fillRect(width * 0.55, 0, width * 0.45, height)
                }
            }

            Text {
                x: 20
                y: 16
                text: "本月结余"
                color: Theme.heroMuted
                font.family: Theme.fontFamily
                font.pixelSize: 10
                font.letterSpacing: 1.2
            }

            AmountText {
                x: 18
                y: 36
                value: App.monthBalance
                amountSize: 36
                amountWeight: 600
                amountColor: Theme.heroInk
            }

            Row {
                x: 20
                y: 94
                spacing: 22

                Row {
                    spacing: 7
                    Rectangle { width: 6; height: 6; radius: 3; color: "#62D6A7"; anchors.verticalCenter: parent.verticalCenter }
                    Column {
                        spacing: 1
                        Text { text: "本月收入"; color: Theme.heroMuted; font.family: Theme.fontFamily; font.pixelSize: 9 }
                        AmountText { value: App.monthIncome; amountSize: 13; amountWeight: 600; amountColor: Theme.heroInk }
                    }
                }
                Rectangle { width: 1; height: 26; color: Theme.heroHairline; anchors.verticalCenter: parent.verticalCenter }
                Row {
                    spacing: 7
                    Rectangle { width: 6; height: 6; radius: 3; color: "#FF9B80"; anchors.verticalCenter: parent.verticalCenter }
                    Column {
                        spacing: 1
                        Text { text: "本月支出"; color: Theme.heroMuted; font.family: Theme.fontFamily; font.pixelSize: 9 }
                        AmountText { value: App.monthExpense; amountSize: 13; amountWeight: 600; amountColor: Theme.heroInk }
                    }
                }
            }

            Sparkline {
                visible: hero.width > 500
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 14
                width: Math.min(196, parent.width * 0.36)
                height: 42
                values: App.dailySeries.map(function(item) { return item.cumulative })
                lineColor: Qt.rgba(1, 1, 1, 0.55)
                showEndDot: true
            }

            Rectangle {
                visible: App.lastMonthExpense > 0
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.top: parent.top
                anchors.topMargin: 14
                height: 24
                width: changeRow.implicitWidth + 18
                radius: 12
                color: Qt.rgba(1, 1, 1, 0.08)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.08)

                Row {
                    id: changeRow
                    anchors.centerIn: parent
                    spacing: 4
                    Icon {
                        name: App.expenseChange > 0 ? "arrow_upward" : "arrow_downward"
                        size: 11
                        weight: 500
                        iconColor: Qt.rgba(1, 1, 1, 0.65)
                    }
                    Text {
                        text: "支出较上月 " + App.percentLabel(App.expenseChange)
                        color: Qt.rgba(1, 1, 1, 0.68)
                        font.family: Theme.numberFamily
                        font.pixelSize: 9
                    }
                }
            }
        }

    }

    MonthlyStatRow {
        id: statsRow
        anchors.top: heroRow.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        width: parent.width
        cardHeight: 104
    }

    Row {
        id: bottomRow
        anchors.top: statsRow.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 14

        GlassCard {
            id: compositionCard
            width: Math.max(250, parent.width * 0.42)
            height: parent.height
            radius: 20

            Item {
                anchors.fill: parent
                anchors.margins: 16

                Text {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    text: "支出构成"
                    color: Theme.ink
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: 600
                }
                Text {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: 2
                    text: App.monthShortLabel
                    color: Theme.faint
                    font.family: Theme.fontFamily
                    font.pixelSize: 9
                }

                DonutChart {
                    id: donut
                    visible: page.donutSegments.length > 0
                    width: page.donutSize
                    height: page.donutSize
                    anchors.centerIn: parent
                    thickness: Math.max(12, page.donutSize * 0.11)
                    segments: page.donutSegments
                    centerLabel: page.hoveredDonut >= 0 && page.hoveredDonut < page.donutSegments.length
                                  ? page.donutSegments[page.hoveredDonut].name : "本月支出"
                    centerValue: page.hoveredDonut >= 0 && page.hoveredDonut < page.donutSegments.length
                                  ? App.money(page.donutSegments[page.hoveredDonut].amount) : App.money(App.monthExpense)
                    centerSub: page.hoveredDonut >= 0 && page.hoveredDonut < page.donutSegments.length
                                  ? ((App.monthExpense > 0 ? Math.round(page.donutSegments[page.hoveredDonut].amount / App.monthExpense * 100) : 0) + "%") : (App.monthTransactionCount + " 笔")
                    onSegmentHovered: function(index) { page.hoveredDonut = index }
                }

                EmptyState {
                    visible: page.donutSegments.length === 0
                    anchors.centerIn: parent
                    width: parent.width
                    contentOffset: 8
                    contentSpacing: 7
                    titleSize: 13
                    icon: "donut_large"
                    title: "还没有支出"
                    description: "记下第一笔，构成图就会出现在这里。"
                    actionText: "记一笔"
                    onActionClicked: UiBus.openTransaction(0)
                }
            }
        }

        BudgetCategoryListCard {
            width: parent.width - compositionCard.width - 14
            height: parent.height
            radius: 20
            shadow: false
            compact: true
            showActions: false
            showAddRow: false
            onRowClicked: UiBus.navigate("budgets")
            onAddRequested: UiBus.navigate("budgets")
        }
    }
}
