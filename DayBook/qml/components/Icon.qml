import QtQuick
import DayBook

Text {
    id: root

    property string name: "circle"
    property int size: 20
    property color iconColor: Theme.ink
    property bool filled: false
    property int weight: 400

    text: Icons.glyph(name)
    color: iconColor
    font.family: Icons.family
    font.pixelSize: size
    font.variableAxes: ({ "FILL": filled ? 1 : 0, "wght": weight, "opsz": 24, "GRAD": 0 })
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    renderType: Text.QtRendering
}
