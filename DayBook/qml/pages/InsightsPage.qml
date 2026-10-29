import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import DayBook

Item {
    id: page

    property int donutHover: -1
    property int incomeHover: -1
    property bool compositionFlipped: false
    property bool budgetFlipped: true
    property bool periodFlipped: false
    property real compositionScale: 1
    property real budgetScale: 1
    property real periodScale: 1

    readonly property var compositionSegments: compositionFlipped ? incomeSegments : donutSegments
    readonly property real compositionTotal: compositionFlipped ? App.monthIncome : App.monthExpense
    readonly property int compositionHoverIndex: compositionFlipped ? incomeHover : donutHover

    function compositionLabel() {
        return compositionFlipped ? "本月收入" : "本月支出"
    }

    function compositionCount() {
        return compositionFlipped ? incomeCount() : expenseCount()
    }

    function setCompositionHover(index) {
        if (compositionFlipped)
            incomeHover = index
        else
            donutHover = index
    }

    function toggleComposition() {
        if (compositionScale < 0.99)
            return
        compositionFlip.restart()
    }

    function toggleBudget() {
        if (budgetScale < 0.99)
            return
        budgetFlip.restart()
    }

    function togglePeriod() {
        if (periodScale < 0.99)
            return
        periodFlip.restart()
    }

    SequentialAnimation {
        id: compositionFlip
        NumberAnimation { target: page; property: "compositionScale"; from: 1; to: 0; duration: 150; easing.type: Easing.InCubic }
        ScriptAction { script: page.compositionFlipped = !page.compositionFlipped }
        NumberAnimation { target: page; property: "compositionScale"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
    }

    SequentialAnimation {
        id: budgetFlip
        NumberAnimation { target: page; property: "budgetScale"; from: 1; to: 0; duration: 150; easing.type: Easing.InCubic }
        ScriptAction { script: page.budgetFlipped = !page.budgetFlipped }
        NumberAnimation { target: page; property: "budgetScale"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
    }

    SequentialAnimation {
        id: periodFlip
        NumberAnimation { target: page; property: "periodScale"; from: 1; to: 0; duration: 150; easing.type: Easing.InCubic }
        ScriptAction { script: page.periodFlipped = !page.periodFlipped }
        NumberAnimation { target: page; property: "periodScale"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
    }
    property int trendRange: 1
    property var donutSegments: buildDonut(App.categoryBreakdown)
    property var incomeSegments: buildDonut(App.incomeBreakdown)

    Component.onCompleted: {
        flick.contentY = 0
        if (App.startupModal === "quarter")
            trendRange = 0
        else if (App.startupModal === "seven")
            trendRange = 2
        else if (App.startupModal === "income")
            compositionFlipped = true
        else if (App.startupModal === "budgetflip")
            budgetFlipped = true
        else if (App.startupModal === "period")
            periodFlipped = true

    }

    function trendMonths() {
        return (App.monthlyTrend || []).slice(-3)
    }

    function trendSeries() {
        if (trendRange === 0)
            return App.quarterSeries || []
        let series = App.dailySeries || []
        if (trendRange === 2)
            series = series.slice(Math.max(0, series.length - 7))
        return series
    }

    function trendValues() {
        return trendSeries().map(function(item) { return item.amount })
    }

    function trendLabels() {
        return trendSeries().map(function(item) { return item.label })
    }

    function trendSubtitle() {
        if (trendRange === 0)
            return "本季度每日支出"
        if (trendRange === 2)
            return "近 7 天每日支出"
        return "本月每日支出"
    }

    function expenseCount() {
        let n = 0
        for (let i = 0; i < App.categoryBreakdown.length; ++i)
            n += Number(App.categoryBreakdown[i].count) || 0
        return n
    }

    function incomeCount() {
        let n = 0
        for (let i = 0; i < App.incomeBreakdown.length; ++i)
            n += Number(App.incomeBreakdown[i].count) || 0
        return n
    }

    function buildDonut(list) {
        const result = []
        let rest = 0
        for (let i = 0; i < list.length; ++i) {
            if (i < 4) result.push(list[i])
            else rest += Number(list[i].amount) || 0
        }
        if (rest > 0.005)
            result.push({ "name": "其他", "amount": rest, "color": "#B7AEA1", "icon": "more_horiz" })
        return result
    }

    function savingsRate() {
        if (App.monthIncome <= 0)
            return 0
        return Math.round(App.monthBalance / App.monthIncome * 100)
    }

    function weekdayMax() {
        const list = App.weekdayStats || []
        let best = null
        for (let i = 0; i < list.length; ++i) {
            if (best === null || list[i].amount > best.amount)
                best = list[i]
        }
        return best
    }

    function weekdayAverage() {
        const list = App.weekdayStats || []
        let sum = 0
        for (let i = 0; i < list.length; ++i)
            sum += Number(list[i].amount) || 0
        return sum / 7
    }

    function topCategory() {
        return App.categoryBreakdown.length > 0 ? App.categoryBreakdown[0] : null
    }

    function changeSentence() {
        if (App.lastMonthExpense <= 0)
            return "从这个月开始，慢慢看见自己的消费节奏。"
        const diff = Math.abs(App.monthExpense - App.lastMonthExpense)
        if (App.monthExpense >= App.lastMonthExpense)
            return "本月支出较上月增加 " + App.money(diff) + "，留意一下节奏。"
        return "本月支出较上月减少 " + App.money(diff) + "，保持得很好。"
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(flick.height, content.implicitHeight)
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: content
            width: flick.width
            y: 0
            spacing: 12

            Item {
                width: parent.width
                height: 50
                Column {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    Text {
                        text: "统计"
                        color: Theme.ink
                        font.family: Theme.fontFamily
                        font.pixelSize: 25
                        font.weight: 600
                        font.letterSpacing: -0.3
                    }
                    Text {
                        text: page.changeSentence()
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

            MonthlyStatRow {
                width: parent.width
                cardHeight: 104
            }

            Row {
                width: parent.width
                height: 318
                spacing: 12

                GlassCard {
                    width: parent.width - 312
                    height: parent.height
                    radius: 20
                    shadow: false

                    Item {
                        anchors.fill: parent
                        anchors.margins: 18

                        Item {
                            width: parent.width
                            height: 38

                            Row {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 8
                                Text {
                                    text: "支出走势"
                                    color: Theme.ink
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 13
                                    font.weight: 600
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            SegmentedControl {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                width: implicitWidth
                                height: 38
                                controlHeight: 38
                                hoverHighlight: false
                                items: ["本季度", "本月", "近七天"]
                                currentIndex: page.trendRange
                                onActivated: function(index) { page.trendRange = index }
                            }
                        }

                        AreaChart {
                            width: parent.width
                            height: parent.height - 46
                            y: 46
                            values: page.trendValues()
                            labels: page.trendLabels()
                            labelEvery: page.trendRange === 0 ? 10 : (page.trendRange === 2 ? 1 : 0)
                            lineColor: Theme.expense
                        }
                    }
                }

                GlassCard {
                    width: 300
                    height: parent.height
                    radius: 20
                    shadow: false

                    Item {
                        anchors.fill: parent
                        anchors.margins: 18
                        transform: Scale {
                            origin.x: width / 2
                            xScale: page.compositionScale
                        }

                        Text {
                            text: "收支构成"
                            color: Theme.ink
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            font.weight: 600
                        }
                        FlipToggle {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            label: page.compositionFlipped ? "支出" : "收入"
                            onClicked: page.toggleComposition()
                        }

                        DonutChart {
                            visible: page.compositionSegments.length > 0
                            width: 176
                            height: 176
                            anchors.centerIn: parent
                            thickness: 18
                            segments: page.compositionSegments
                            centerLabel: page.compositionHoverIndex >= 0 && page.compositionHoverIndex < page.compositionSegments.length
                                          ? page.compositionSegments[page.compositionHoverIndex].name : page.compositionLabel()
                            centerValue: page.compositionHoverIndex >= 0 && page.compositionHoverIndex < page.compositionSegments.length
                                          ? App.money(page.compositionSegments[page.compositionHoverIndex].amount) : App.money(page.compositionTotal)
                            centerSub: page.compositionHoverIndex >= 0 && page.compositionHoverIndex < page.compositionSegments.length
                                          ? ((page.compositionTotal > 0 ? Math.round(page.compositionSegments[page.compositionHoverIndex].amount / page.compositionTotal * 100) : 0) + "%")
                                          : (page.compositionCount() + " 笔")
                            onSegmentHovered: function(index) { page.setCompositionHover(index) }
                        }

                        EmptyState {
                            visible: page.compositionSegments.length === 0
                            anchors.centerIn: parent
                            width: parent.width
                            icon: "donut_large"
                            title: page.compositionFlipped ? "暂无收入" : "暂无支出"
                            description: page.compositionFlipped ? "记一笔收入后，这里会显示收入构成。" : "记一笔后，这里会形成你的消费画像。"
                            actionText: "记一笔"
                            onActionClicked: UiBus.openTransaction(0)
                        }
                    }
                }
            }

            Row {
                width: parent.width
                height: 292
                spacing: 12

                GlassCard {
                    width: (parent.width - 12) / 2
                    height: parent.height
                    radius: 20
                    shadow: false

                    Item {
                        anchors.fill: parent
                        anchors.margins: 18
                        transform: Scale {
                            origin.x: width / 2
                            xScale: page.budgetScale
                        }

                        Text {
                            text: "预算与分类"
                            color: Theme.ink
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            font.weight: 600
                        }
                        FlipToggle {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            label: page.budgetFlipped ? "排行" : "预算"
                            onClicked: page.toggleBudget()
                        }

                        Column {
                            visible: !page.budgetFlipped
                            width: parent.width
                            anchors.top: parent.top
                            anchors.topMargin: 32
                            spacing: 2
                            Repeater {
                                model: App.categoryBreakdown.slice(0, 5)
                                Item {
                                    width: parent.width
                                    height: 45

                                    CategoryIcon {
                                        id: rankIcon
                                        x: 0
                                        anchors.verticalCenter: parent.verticalCenter
                                        icon: modelData.icon
                                        accentColor: modelData.color
                                        size: 30
                                        iconSize: 15
                                    }
                                    Column {
                                        anchors.left: rankIcon.right
                                        anchors.leftMargin: 10
                                        anchors.right: rankAmount.left
                                        anchors.rightMargin: 10
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 4
                                        Row {
                                            width: parent.width
                                            Text {
                                                text: modelData.name
                                                color: Theme.inkSecondary
                                                font.family: Theme.fontFamily
                                                font.pixelSize: 10
                                                font.weight: 500
                                            }
                                            Text {
                                                text: " · " + modelData.count + " 笔"
                                                color: Theme.faint
                                                font.family: Theme.fontFamily
                                                font.pixelSize: 9
                                            }
                                        }
                                        ProgressBar {
                                            width: parent.width
                                            height: 4
                                            value: App.monthExpense > 0 ? modelData.amount / App.monthExpense : 0
                                            fillColor: modelData.color
                                            barHeight: 4
                                        }
                                    }
                                    Column {
                                        id: rankAmount
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 2
                                        AmountText {
                                            anchors.right: parent.right
                                            value: modelData.amount
                                            amountSize: 10
                                            amountWeight: 600
                                        }
                                        Text {
                                            anchors.right: parent.right
                                            text: (App.monthExpense > 0 ? Math.round(modelData.amount / App.monthExpense * 100) : 0) + "%"
                                            color: Theme.faint
                                            font.family: Theme.numberFamily
                                            font.pixelSize: 8
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            visible: !page.budgetFlipped && App.categoryBreakdown.length === 0
                            anchors.centerIn: parent
                            text: "还没有支出数据"
                            color: Theme.faint
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                        }

                        Item {
                            visible: page.budgetFlipped
                            anchors.top: parent.top
                            anchors.topMargin: 32
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom

                            Text {
                                y: 0
                                text: App.budgets.totalPercent + "%"
                                color: App.budgets.totalSpent > App.budgets.totalBudget
                                       ? Theme.danger : Theme.ink
                                font.family: Theme.numberFamily
                                font.pixelSize: 25
                                font.weight: 600
                            }
                            Text {
                                x: 82
                                y: 8
                                text: "已用 " + App.money(App.budgets.totalSpent)
                                      + " / " + App.money(App.budgets.totalBudget)
                                color: Theme.muted
                                font.family: Theme.numberFamily
                                font.pixelSize: 9
                            }
                            ProgressBar {
                                width: parent.width
                                y: 40
                                value: App.budgets.totalBudget > 0
                                       ? Math.min(1, App.budgets.totalSpent / App.budgets.totalBudget) : 0
                                fillColor: App.budgets.totalSpent > App.budgets.totalBudget
                                           ? Theme.danger : Theme.accent
                                over: App.budgets.totalSpent > App.budgets.totalBudget
                                barHeight: 6
                            }

                            Flickable {
                                id: insightBudgetScroll
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.topMargin: 56
                                anchors.bottom: parent.bottom
                                clip: true
                                contentWidth: width
                                contentHeight: budgetList.height
                                boundsBehavior: Flickable.StopAtBounds

                                Column {
                                    id: budgetList
                                    width: insightBudgetScroll.width
                                    spacing: 2
                                    Repeater {
                                        model: App.budgets
                                        Item {
                                            width: parent.width
                                            height: 42
                                            property var budgetRow: App.budgets.get(index)

                                            CategoryIcon {
                                                id: flipBudgetIcon
                                                x: 0
                                                anchors.verticalCenter: parent.verticalCenter
                                                icon: budgetRow.icon
                                                accentColor: budgetRow.color
                                                size: 26
                                                iconSize: 13
                                            }
                                            Column {
                                                anchors.left: flipBudgetIcon.right
                                                anchors.leftMargin: 9
                                                anchors.right: flipBudgetPct.left
                                                anchors.rightMargin: 10
                                                anchors.verticalCenter: parent.verticalCenter
                                                spacing: 4
                                                Text {
                                                    text: budgetRow.name
                                                    color: Theme.inkSecondary
                                                    font.family: Theme.fontFamily
                                                    font.pixelSize: 10
                                                    font.weight: 500
                                                }
                                                ProgressBar {
                                                    width: parent.width
                                                    height: 4
                                                    value: budgetRow.percent / 100
                                                    fillColor: budgetRow.over ? Theme.danger : budgetRow.color
                                                    over: budgetRow.over
                                                    barHeight: 4
                                                }
                                            }
                                            Text {
                                                id: flipBudgetPct
                                                anchors.right: parent.right
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: budgetRow.percent + "%"
                                                color: budgetRow.over ? Theme.danger : Theme.muted
                                                font.family: Theme.numberFamily
                                                font.pixelSize: 9
                                                font.weight: 600
                                            }
                                        }
                                    }
                                }
                            }

                            Text {
                                visible: App.budgets.count === 0
                                anchors.centerIn: parent
                                text: "还没有设置预算"
                                color: Theme.faint
                                font.family: Theme.fontFamily
                                font.pixelSize: 11
                            }
                        }
                    }
                }

                GlassCard {
                    width: (parent.width - 12) / 2
                    height: parent.height
                    radius: 20
                    shadow: false

                    Item {
                        anchors.fill: parent
                        anchors.margins: 18
                        transform: Scale {
                            origin.x: width / 2
                            xScale: page.periodScale
                        }

                        Text {
                            text: "收支"
                            color: Theme.ink
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            font.weight: 600
                        }
                        FlipToggle {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            label: page.periodFlipped ? "月份" : "星期"
                            onClicked: page.togglePeriod()
                        }

                        Item {
                            visible: !page.periodFlipped
                            anchors.top: parent.top
                            anchors.topMargin: 32
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom

                            Row {
                                spacing: 10
                                Row {
                                    spacing: 5
                                    Rectangle { width: 6; height: 6; radius: 3; color: Theme.income; anchors.verticalCenter: parent.verticalCenter }
                                    Text { text: "收入"; color: Theme.muted; font.family: Theme.fontFamily; font.pixelSize: 9; anchors.verticalCenter: parent.verticalCenter }
                                }
                                Row {
                                    spacing: 5
                                    Rectangle { width: 6; height: 6; radius: 3; color: Theme.expense; anchors.verticalCenter: parent.verticalCenter }
                                    Text { text: "支出"; color: Theme.muted; font.family: Theme.fontFamily; font.pixelSize: 9; anchors.verticalCenter: parent.verticalCenter }
                                }
                            }

                            BarChart {
                                width: parent.width
                                height: parent.height - 24
                                y: 24
                                chartData: App.monthlyTrend.slice(-6)
                                series: [
                                    { "key": "income", "color": Theme.income, "label": "收入" },
                                    { "key": "expense", "color": Theme.expense, "label": "支出" }
                                ]
                                showAxis: true
                            }
                        }

                        Item {
                            visible: page.periodFlipped
                            anchors.top: parent.top
                            anchors.topMargin: 32
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom

                            Text {
                                anchors.right: parent.right
                                text: page.weekdayMax() !== null ? "平均 " + App.money(page.weekdayAverage()) : ""
                                color: Theme.faint
                                font.family: Theme.numberFamily
                                font.pixelSize: 9
                            }

                            BarChart {
                                width: parent.width
                                height: parent.height - 56
                                y: 12
                                chartData: App.weekdayStats
                                series: [{ "key": "amount", "color": Theme.expense, "label": "支出" }]
                                showAxis: true
                            }

                            Rectangle {
                                width: parent.width
                                height: 36
                                anchors.bottom: parent.bottom
                                radius: 10
                                color: Theme.accentWash
                                Row {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 6
                                    Icon { name: "auto_awesome"; size: 13; iconColor: Theme.accent }
                                    Text {
                                        text: page.weekdayMax() !== null
                                              ? "「" + page.weekdayMax().label + "」是本月花钱最多的一天"
                                              : "记几笔之后，会看到一周的消费节奏"
                                        color: Theme.inkSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: 9
                                        anchors.verticalCenter: parent.verticalCenter
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
