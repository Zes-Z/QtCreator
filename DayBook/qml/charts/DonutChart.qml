import QtQuick
import QtQuick.Shapes
import DayBook

Item {
    id: root

    property var segments: []
    property real thickness: 18
    property real reveal: 0
    property int hoveredIndex: -1
    property string centerLabel: "本月支出"
    property string centerValue: ""
    property string centerSub: ""
    signal segmentHovered(int index)

    implicitWidth: 170
    implicitHeight: 170

    readonly property real total: {
        let sum = 0
        for (let i = 0; i < segments.length; ++i)
            sum += Number(segments[i].amount) || 0
        return sum
    }
    readonly property real ringRadius: Math.max(4, Math.min(width, height) / 2 - thickness / 2 - 4)
    readonly property real gapDegrees: segments.length > 1 ? 2.0 : 0.0

    onSegmentsChanged: {
        root.reveal = 0
        revealAnim.restart()
    }

    NumberAnimation {
        id: revealAnim
        target: root
        property: "reveal"
        from: 0
        to: 1
        duration: 780
        easing.type: Easing.OutCubic
    }

    Shape {
        anchors.fill: parent
        layer.enabled: true
        layer.samples: 4
        ShapePath {
            fillColor: "transparent"
            strokeColor: Theme.dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(35 / 255, 31 / 255, 26 / 255, 0.065)
            strokeWidth: root.thickness
            capStyle: ShapePath.FlatCap
            PathAngleArc {
                centerX: root.width / 2
                centerY: root.height / 2
                radiusX: root.ringRadius
                radiusY: root.ringRadius
                startAngle: 0
                sweepAngle: 359.99
            }
        }
    }

    Repeater {
        model: root.segments
        Shape {
            anchors.fill: parent
            layer.enabled: true
            layer.samples: 4
            visible: root.total > 0 && Number(modelData.amount) > 0

            ShapePath {
                fillColor: "transparent"
                strokeColor: modelData.color
                strokeWidth: root.thickness + (index === root.hoveredIndex ? 4 : 0)
                capStyle: ShapePath.FlatCap

                PathAngleArc {
                    centerX: root.width / 2
                    centerY: root.height / 2
                    radiusX: root.ringRadius
                    radiusY: root.ringRadius
                    startAngle: {
                        let previous = 0
                        for (let i = 0; i < index; ++i)
                            previous += Number(root.segments[i].amount) || 0
                        return -90 + (previous / root.total) * 360 * root.reveal + root.gapDegrees / 2
                    }
                    sweepAngle: Math.max(0.6,
                                         (Number(modelData.amount) / root.total) * 360 * root.reveal
                                         - root.gapDegrees)
                }
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 2
        Text {
            width: root.width * 0.62
            text: root.centerLabel
            color: Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
        }
        Text {
            width: root.width * 0.82
            text: root.centerValue
            color: Theme.ink
            font.family: Theme.numberFamily
            font.pixelSize: Math.max(14, Math.round(root.width * 0.145))
            font.weight: 600
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
        Text {
            width: root.width * 0.62
            text: root.centerSub
            color: Theme.faint
            font.family: Theme.fontFamily
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onPositionChanged: function(mouse) {
            const cx = width / 2
            const cy = height / 2
            const dx = mouse.x - cx
            const dy = mouse.y - cy
            const dist = Math.sqrt(dx * dx + dy * dy)
            const r = Math.min(width, height) / 2
            if (dist > r + 4 || dist < r - root.thickness - 8) {
                if (root.hoveredIndex !== -1) {
                    root.hoveredIndex = -1
                    root.segmentHovered(-1)
                }
                return
            }
            let angle = Math.atan2(dy, dx) + Math.PI / 2
            if (angle < 0)
                angle += Math.PI * 2
            let acc = 0
            let found = -1
            for (let i = 0; i < root.segments.length; ++i) {
                const share = (Number(root.segments[i].amount) || 0) / Math.max(1, root.total) * Math.PI * 2
                if (angle >= acc && angle < acc + share) {
                    found = i
                    break
                }
                acc += share
            }
            if (found !== root.hoveredIndex) {
                root.hoveredIndex = found
                root.segmentHovered(found)
            }
        }
        onExited: {
            root.hoveredIndex = -1
            root.segmentHovered(-1)
        }
    }
}
