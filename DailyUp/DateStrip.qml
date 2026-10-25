import QtQuick
import QtQuick.Layouts
import DailyUp

// 最近七天。今天永远在最右侧，选中的一天用强调色填充。
Item {
    id: strip

    property date today: new Date()
    property string selectedDate: ""

    signal dateSelected(string dateStr)

    readonly property int gap: 6
    readonly property real pillWidth: Math.max(30, Math.min(64, (width - gap * 6) / 7))

    readonly property var days: {
        const result = []
        for (let i = 0; i < 7; ++i) {
            const day = new Date(strip.today)
            day.setHours(0, 0, 0, 0)
            day.setDate(day.getDate() - (6 - i))
            const key = Qt.formatDate(day, "yyyy-MM-dd")
            result.push({
                "key": key,
                "weekday": ["周日", "周一", "周二", "周三", "周四", "周五", "周六"][day.getDay()],
                "label": day.getDate(),
                "isToday": key === Qt.formatDate(strip.today, "yyyy-MM-dd")
            })
        }
        return result
    }

    Row {
        anchors.centerIn: parent
        spacing: strip.gap

        Repeater {
            model: strip.days

            delegate: Rectangle {
                id: pill

                width: strip.pillWidth
                height: strip.height
                radius: 14

                readonly property bool selected: modelData.key === strip.selectedDate

                color: selected
                       ? Theme.accent
                       : (modelData.isToday
                          ? Theme.accentSoft
                          : (pillHover.hovered ? Theme.chip : Theme.alpha(Theme.chip, 0)))

                border.width: (!selected && modelData.isToday) ? 1 : 0
                border.color: Theme.alpha(Theme.accent, 0.30)
                Behavior on color { ColorAnimation { duration: 160 } }

                Column {
                    anchors.centerIn: parent
                    spacing: 3

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.weekday
                        font.pixelSize: 10
                        font.weight: Font.Medium
                        color: pill.selected ? Theme.accentContrast : Theme.faint
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.label
                        font.pixelSize: 18
                        font.weight: pill.selected ? Font.DemiBold : Font.Medium
                        color: pill.selected
                               ? Theme.accentContrast
                               : (modelData.isToday ? Theme.accent : Theme.ink)
                    }
                }

                Rectangle {
                    visible: modelData.isToday && !pill.selected
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 5
                    width: 4
                    height: 4
                    radius: 2
                    color: Theme.accent
                }

                HoverHandler {
                    id: pillHover
                    cursorShape: Qt.PointingHandCursor
                }

                TapHandler {
                    onTapped: strip.dateSelected(modelData.key)
                }
            }
        }
    }
}