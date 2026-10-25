import QtQuick
import DayBook

Item {
    id: root

    property double value: 0
    property color ringColor: Theme.accent
    property color trackColor: Theme.dark ? Theme.withAlpha(Theme.ink, 0.08) : Theme.withAlpha(Theme.ink, 0.07)
    property real lineWidth: 8
    property double animatedValue: 0

    implicitWidth: 96
    implicitHeight: 96

    Behavior on animatedValue { NumberAnimation { duration: 900; easing.type: Easing.OutCubic } }
    Component.onCompleted: animatedValue = Math.max(0, Math.min(1, value))
    onValueChanged: animatedValue = Math.max(0, Math.min(1, value))
    onAnimatedValueChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
    onRingColorChanged: canvas.requestPaint()
    onTrackColorChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        renderStrategy: Canvas.Cooperative

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const lw = root.lineWidth
            const cx = width / 2
            const cy = height / 2
            const r = Math.max(1, Math.min(width, height) / 2 - lw / 2 - 1)

            ctx.lineWidth = lw
            ctx.lineCap = "round"
            ctx.strokeStyle = root.trackColor
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI * 2)
            ctx.stroke()

            if (root.animatedValue > 0.001) {
                ctx.strokeStyle = root.ringColor
                ctx.beginPath()
                ctx.arc(cx, cy, r, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * root.animatedValue)
                ctx.stroke()
            }
        }
    }
}
