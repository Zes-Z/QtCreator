import QtQuick
import DayBook

Item {
    id: root

    property var chartData: []
    property var series: [{ "key": "expense", "color": Theme.chartExpense, "label": "支出" }]
    property bool showAxis: true
    property real axisMax: 100
    property real reveal: 0
    property int hoveredIndex: -1

    implicitHeight: 220

    readonly property real plotLeft: showAxis ? 38 : 4
    readonly property real plotRight: 8
    readonly property real plotTop: 10
    readonly property real plotBottom: showAxis ? 26 : 8
    readonly property real plotWidth: Math.max(1, width - plotLeft - plotRight)
    readonly property real plotHeight: Math.max(1, height - plotTop - plotBottom)

    onChartDataChanged: {
        recompute()
        root.reveal = 0
        revealAnim.restart()
        canvas.requestPaint()
    }
    onSeriesChanged: {
        recompute()
        canvas.requestPaint()
    }
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
    onHoveredIndexChanged: canvas.requestPaint()
    onRevealChanged: canvas.requestPaint()
    Component.onCompleted: {
        recompute()
        revealAnim.restart()
    }

    function recompute() {
        let max = 0
        for (let i = 0; i < chartData.length; ++i) {
            for (let s = 0; s < series.length; ++s) {
                const key = series[s].key
                max = Math.max(max, Number(chartData[i][key]) || 0)
            }
        }
        if (max <= 0)
            max = 100
        root.axisMax = max * 1.18
    }

    NumberAnimation {
        id: revealAnim
        target: root
        property: "reveal"
        from: 0
        to: 1
        duration: 700
        easing.type: Easing.OutCubic
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const w = root.plotWidth
            const h = root.plotHeight
            const left = root.plotLeft
            const top = root.plotTop

            ctx.strokeStyle = Theme.chartGrid
            ctx.lineWidth = 1
            ctx.setLineDash([3, 5])
            for (let i = 0; i <= 3; ++i) {
                const y = top + (i / 3) * h
                ctx.beginPath()
                ctx.moveTo(left, y)
                ctx.lineTo(left + w, y)
                ctx.stroke()
            }
            ctx.setLineDash([])

            const n = chartData.length
            if (n <= 0)
                return
            const group = w / n
            const count = Math.max(1, series.length)
            const barW = Math.min(series.length > 1 ? 12 : 22, (group * 0.52) / count)
            const padding = 3

            for (let i = 0; i < n; ++i) {
                if (i === root.hoveredIndex) {
                    ctx.fillStyle = Theme.dark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0.137, 0.122, 0.102, 0.04)
                    const gx = left + i * group + 2
                    ctx.beginPath()
                    ctx.moveTo(gx + 6, top)
                    ctx.lineTo(gx + group - 4 - 6, top)
                    ctx.quadraticCurveTo(gx + group - 4, top, gx + group - 4, top + 6)
                    ctx.lineTo(gx + group - 4, top + h)
                    ctx.lineTo(gx, top + h)
                    ctx.lineTo(gx, top + 6)
                    ctx.quadraticCurveTo(gx, top, gx + 6, top)
                    ctx.fill()
                }
                for (let s = 0; s < series.length; ++s) {
                    const key = series[s].key
                    const value = Number(chartData[i][key]) || 0
                    const bh = Math.max(value > 0 ? 2 : 0, (value / root.axisMax) * h * root.reveal)
                    if (bh <= 0)
                        continue
                    const total = barW * count + padding * (count - 1)
                    const bx = left + i * group + (group - total) / 2 + s * (barW + padding)
                    const by = top + h - bh
                    const rr = Math.min(4, barW / 2, bh)
                    ctx.beginPath()
                    ctx.moveTo(bx, by + bh)
                    ctx.lineTo(bx, by + rr)
                    ctx.quadraticCurveTo(bx, by, bx + rr, by)
                    ctx.lineTo(bx + barW - rr, by)
                    ctx.quadraticCurveTo(bx + barW, by, bx + barW, by + rr)
                    ctx.lineTo(bx + barW, by + bh)
                    ctx.closePath()
                    ctx.fillStyle = series[s].color
                    ctx.fill()
                }
            }
        }
    }

    Item {
        id: yLabels
        visible: root.showAxis
        x: 0
        y: root.plotTop - 7
        width: root.plotLeft - 8
        height: root.plotHeight
        Repeater {
            model: 3
            Text {
                width: yLabels.width
                y: (index / 2) * (yLabels.height - 12) - 2
                text: App.compactNumber(root.axisMax * (1 - index / 2))
                color: Theme.faint
                font.family: Theme.numberFamily
                font.pixelSize: 9
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    Item {
        id: xLabels
        visible: root.showAxis
        x: root.plotLeft
        y: root.plotTop + root.plotHeight + 7
        width: root.plotWidth
        height: 14
        Repeater {
            model: root.chartData
            Text {
                property int count: (root.chartData || []).length
                x: (index + 0.5) / Math.max(1, count) * xLabels.width - width / 2
                width: Math.min(44, Math.max(24, xLabels.width / Math.max(1, count)))
                text: modelData.label
                color: index === root.hoveredIndex ? Theme.inkSecondary : Theme.faint
                font.family: Theme.numberFamily
                font.pixelSize: 9
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }
    }

    MouseArea {
        id: hoverArea
        x: root.plotLeft
        y: root.plotTop
        width: root.plotWidth
        height: root.plotHeight
        hoverEnabled: true
        onPositionChanged: function(mouse) {
            const n = root.chartData.length
            if (n <= 0)
                return
            root.hoveredIndex = Math.max(0, Math.min(n - 1, Math.floor(mouse.x / Math.max(1, width) * n)))
        }
        onExited: root.hoveredIndex = -1
    }

    Rectangle {
        id: tooltip
        visible: root.hoveredIndex >= 0 && root.chartData.length > 0
        z: 20
        width: tooltipColumn.implicitWidth + 20
        height: tooltipColumn.implicitHeight + 14
        radius: 10
        color: Theme.tooltipBg
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.08)
        x: Math.min(Math.max(root.plotLeft + (root.hoveredIndex + 0.5) / Math.max(1, root.chartData.length) * root.plotWidth - width / 2, 2), root.width - width - 2)
        y: Math.max(0, root.plotTop - 2)

        Column {
            id: tooltipColumn
            anchors.centerIn: parent
            spacing: 3
            Text {
                text: root.hoveredIndex >= 0 && root.chartData.length > root.hoveredIndex ? root.chartData[root.hoveredIndex].label : ""
                color: Theme.withAlpha(Theme.tooltipText, 0.62)
                font.family: Theme.fontFamily
                font.pixelSize: 9
            }
            Repeater {
                model: root.series
                Row {
                    spacing: 5
                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: modelData.color
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: modelData.label + " " + App.money(Number((root.chartData[root.hoveredIndex] || {})[modelData.key]) || 0)
                        color: Theme.tooltipText
                        font.family: Theme.numberFamily
                        font.pixelSize: 10
                    }
                }
            }
        }
    }
}
