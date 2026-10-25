import QtQuick
import DayBook

Item {
    id: root

    property var values: []
    property var labels: []
    property color lineColor: Theme.chartExpense
    property bool showAxis: true
    property bool showGrid: true
    property string valueSuffix: "元"
    property real reveal: 0
    property int hoveredIndex: -1
    property int labelEvery: 0
    property real axisMax: 100

    implicitHeight: 210

    readonly property real plotLeft: showAxis ? 38 : 4
    readonly property real plotRight: 8
    readonly property real plotTop: 8
    readonly property real plotBottom: showAxis ? 26 : 8
    readonly property real plotWidth: Math.max(1, width - plotLeft - plotRight)
    readonly property real plotHeight: Math.max(1, height - plotTop - plotBottom)

    onValuesChanged: {
        recompute()
        root.reveal = 0
        revealAnim.restart()
        seriesCanvas.requestPaint()
        gridCanvas.requestPaint()
    }
    onWidthChanged: {
        seriesCanvas.requestPaint()
        gridCanvas.requestPaint()
    }
    onHeightChanged: {
        seriesCanvas.requestPaint()
        gridCanvas.requestPaint()
    }
    onLineColorChanged: seriesCanvas.requestPaint()
    Component.onCompleted: {
        recompute()
        revealAnim.restart()
    }

    NumberAnimation {
        id: revealAnim
        target: root
        property: "reveal"
        from: 0
        to: 1
        duration: 760
        easing.type: Easing.OutCubic
    }

    function recompute() {
        const vals = root.values || []
        let max = 0
        for (let i = 0; i < vals.length; ++i)
            max = Math.max(max, Number(vals[i]) || 0)
        if (max <= 0)
            max = 100
        root.axisMax = max * 1.16
    }

    function pointFor(index, value, w, h) {
        const vals = root.values || []
        const n = Math.max(2, vals.length)
        const x = root.plotLeft + (vals.length <= 1 ? w / 2 : (index / (vals.length - 1)) * w)
        const y = root.plotTop + (1 - (Number(value) || 0) / root.axisMax) * h
        return { "x": x, "y": y }
    }

    Canvas {
        id: gridCanvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const w = root.plotWidth
            const h = root.plotHeight
            const left = root.plotLeft
            const top = root.plotTop

            if (root.showGrid) {
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
        width: root.plotWidth * Math.max(0, Math.min(1, root.reveal))
        height: root.plotHeight
        x: root.plotLeft
        y: root.plotTop
        clip: true

        Canvas {
            id: seriesCanvas
            width: root.plotWidth
            height: root.plotHeight
            antialiasing: true
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const vals = root.values || []
                const w = width
                const h = height
                if (vals.length === 0)
                    return

                const pts = []
                for (let i = 0; i < vals.length; ++i) {
                    const px = vals.length <= 1 ? w / 2 : (i / (vals.length - 1)) * w
                    const py = (1 - (Number(vals[i]) || 0) / root.axisMax) * h
                    pts.push({ "x": px, "y": py })
                }

                function smooth(context, points) {
                    context.moveTo(points[0].x, points[0].y)
                    for (let i = 0; i < points.length - 1; ++i) {
                        const p0 = points[i]
                        const p1 = points[i + 1]
                        const cx = (p0.x + p1.x) / 2
                        context.bezierCurveTo(cx, p0.y, cx, p1.y, p1.x, p1.y)
                    }
                }

                const grad = ctx.createLinearGradient(0, 0, 0, h)
                grad.addColorStop(0, Theme.withAlpha(root.lineColor, 0.32))
                grad.addColorStop(0.7, Theme.withAlpha(root.lineColor, 0.06))
                grad.addColorStop(1, Theme.withAlpha(root.lineColor, 0.0))
                ctx.beginPath()
                smooth(ctx, pts)
                ctx.lineTo(pts[pts.length - 1].x, h)
                ctx.lineTo(pts[0].x, h)
                ctx.closePath()
                ctx.fillStyle = grad
                ctx.fill()

                ctx.beginPath()
                smooth(ctx, pts)
                ctx.strokeStyle = root.lineColor
                ctx.lineWidth = 2.2
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.shadowColor = Theme.withAlpha(root.lineColor, 0.28)
                ctx.shadowBlur = 8
                ctx.stroke()
                ctx.shadowBlur = 0

                if (pts.length > 0) {
                    const last = pts[pts.length - 1]
                    ctx.beginPath()
                    ctx.arc(last.x, last.y, 3.4, 0, Math.PI * 2)
                    ctx.fillStyle = root.lineColor
                    ctx.fill()
                    ctx.beginPath()
                    ctx.arc(last.x, last.y, 6, 0, Math.PI * 2)
                    ctx.fillStyle = Theme.withAlpha(root.lineColor, 0.16)
                    ctx.fill()
                }
            }
        }
    }

    Item {
        id: xLabels
        visible: root.showAxis
        x: root.plotLeft
        y: root.plotTop + root.plotHeight + 6
        width: root.plotWidth
        height: 14

        Repeater {
            model: root.labels
            Text {
                property int count: (root.labels || []).length
                property int step: root.labelEvery > 0
                                   ? root.labelEvery
                                   : Math.max(1, Math.ceil((count - 1) / 5))
                visible: root.labelEvery > 0
                         ? (index % root.labelEvery === 0 || index === count - 1)
                         : index === 0 || index === count - 1 || index % step === 0
                x: Math.min(Math.max((count <= 1 ? 0.5 : index / (count - 1)) * xLabels.width - width / 2, -4), xLabels.width - width + 4)
                width: 30
                text: modelData
                color: Theme.faint
                font.family: Theme.numberFamily
                font.pixelSize: 9
                horizontalAlignment: Text.AlignHCenter
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
            const n = (root.values || []).length
            if (n <= 0)
                return
            const idx = Math.max(0, Math.min(n - 1, Math.round(mouse.x / Math.max(1, width) * (n - 1))))
            root.hoveredIndex = idx
        }
        onExited: root.hoveredIndex = -1
    }

    Item {
        visible: root.hoveredIndex >= 0 && (root.values || []).length > 1
        z: 20
        x: root.plotLeft
        y: root.plotTop
        width: root.plotWidth
        height: root.plotHeight

        readonly property var vals: root.values || []
        readonly property real px: vals.length <= 1 ? width / 2 : (root.hoveredIndex / (vals.length - 1)) * width

        Rectangle {
            x: parent.px
            y: 0
            width: 1
            height: parent.height
            color: Theme.withAlpha(Theme.ink, 0.18)
        }

        Rectangle {
            width: 7
            height: 7
            radius: 4
            color: root.lineColor
            border.width: 2
            border.color: Theme.surfaceSolid
            x: parent.px - 3.5
            y: (1 - (Number(parent.vals[root.hoveredIndex]) || 0) / root.axisMax) * parent.height - 3.5
        }

        Rectangle {
            id: tooltip
            property string labelText: (root.hoveredIndex >= 0 && root.labels && root.labels.length > root.hoveredIndex)
                                      ? String(root.labels[root.hoveredIndex]) : ""
            width: tooltipRow.implicitWidth + 20
            height: 30
            radius: 9
            color: Theme.tooltipBg
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.08)
            x: Math.min(Math.max(parent.px - width / 2, -root.plotLeft + 2), parent.width - width + root.plotLeft - 2)
            y: -36
            Row {
                id: tooltipRow
                anchors.centerIn: parent
                spacing: 7
                Text {
                    text: tooltip.labelText
                    color: Theme.withAlpha(Theme.tooltipText, 0.65)
                    font.family: Theme.fontFamily
                    font.pixelSize: 9
                }
                Text {
                    text: App.money(Number((root.values || [])[root.hoveredIndex]) || 0)
                    color: Theme.tooltipText
                    font.family: Theme.numberFamily
                    font.pixelSize: 10
                    font.weight: 600
                }
            }
        }
    }
}
