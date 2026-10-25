import QtQuick
import DayBook

Item {
    id: root

    property var values: []
    property color lineColor: Theme.accent
    property real lineWidth: 1.8
    property bool fill: true
    property bool showEndDot: true

    onValuesChanged: canvas.requestPaint()
    onLineColorChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const vals = root.values || []
            if (vals.length < 2)
                return

            const pad = 3
            const w = width - pad * 2
            const h = height - pad * 2
            let min = Math.min.apply(null, vals)
            let max = Math.max.apply(null, vals)
            if (max - min < 0.001) {
                max = min + 1
            }
            const range = max - min
            const pts = []
            for (let i = 0; i < vals.length; ++i) {
                pts.push({
                    x: pad + (i / (vals.length - 1)) * w,
                    y: pad + (1 - (vals[i] - min) / range) * h
                })
            }

            function smoothPath(context, points) {
                context.moveTo(points[0].x, points[0].y)
                for (let i = 0; i < points.length - 1; ++i) {
                    const p0 = points[i]
                    const p1 = points[i + 1]
                    const cx = (p0.x + p1.x) / 2
                    context.bezierCurveTo(cx, p0.y, cx, p1.y, p1.x, p1.y)
                }
            }

            if (root.fill) {
                const grad = ctx.createLinearGradient(0, 0, 0, height)
                grad.addColorStop(0, Theme.withAlpha(root.lineColor, 0.28))
                grad.addColorStop(1, Theme.withAlpha(root.lineColor, 0.0))
                ctx.beginPath()
                smoothPath(ctx, pts)
                ctx.lineTo(pts[pts.length - 1].x, height)
                ctx.lineTo(pts[0].x, height)
                ctx.closePath()
                ctx.fillStyle = grad
                ctx.fill()
            }

            ctx.beginPath()
            smoothPath(ctx, pts)
            ctx.strokeStyle = root.lineColor
            ctx.lineWidth = root.lineWidth
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            ctx.stroke()

            if (root.showEndDot) {
                const end = pts[pts.length - 1]
                ctx.beginPath()
                ctx.arc(end.x, end.y, 3.2, 0, Math.PI * 2)
                ctx.fillStyle = root.lineColor
                ctx.fill()
                ctx.beginPath()
                ctx.arc(end.x, end.y, 5.5, 0, Math.PI * 2)
                ctx.fillStyle = Theme.withAlpha(root.lineColor, 0.18)
                ctx.fill()
            }
        }
    }
}
