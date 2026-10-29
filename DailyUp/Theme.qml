pragma Singleton
import QtQuick
import DailyUp

// DailyUp 的唯一配色来源。
// 五套预设 + 调色盘自定义色共用同一套排版与间距，只替换颜色。
QtObject {
    id: theme

    readonly property string key: AppSettings.theme
    readonly property var presetKeys: ["mist", "mint", "sunset", "pink", "gold"]
    readonly property color customAccentColor: AppSettings.customAccent

    readonly property var palettes: ({
        "mist": {
            "label": "晨雾蓝",
            "accent": "#3D7BF7",
            "accentDeep": "#2B5FD4",
            "accentSoft": "#E8F1FE",
            "accentWash": "#F4F8FF",
            "bgTop": "#FCFDFF",
            "bgBottom": "#E9EEF7",
            "success": "#1EA97C",
            "doneTint": "#EFF8F4",
            "chip": "#F1F4FA",
            "contrast": "#FFFFFF"
        },
        "mint": {
            "label": "薄荷绿",
            "accent": "#1EA97F",
            "accentDeep": "#0E8763",
            "accentSoft": "#DFF5EC",
            "accentWash": "#F2FCF7",
            "bgTop": "#FBFEFC",
            "bgBottom": "#E6F1EA",
            "success": "#12916A",
            "doneTint": "#E9F7F0",
            "chip": "#EFF7F2",
            "contrast": "#FFFFFF"
        },
        "sunset": {
            "label": "日落橙",
            "accent": "#E8763A",
            "accentDeep": "#C25822",
            "accentSoft": "#FCEADF",
            "accentWash": "#FFF8F3",
            "bgTop": "#FFFDFB",
            "bgBottom": "#F8E9DD",
            "success": "#2C9E7A",
            "doneTint": "#F9F0E7",
            "chip": "#FBF1EA",
            "contrast": "#FFFFFF"
        },
        "pink": {
            "label": "樱粉",
            "accent": "#D95C82",
            "accentDeep": "#B84467",
            "accentSoft": "#FBE7ED",
            "accentWash": "#FFF6F9",
            "bgTop": "#FFFBFC",
            "bgBottom": "#F7E9EF",
            "success": "#1EA97C",
            "doneTint": "#F8EFF3",
            "chip": "#FAEFF3",
            "contrast": "#FFFFFF"
        },
        "gold": {
            "label": "鎏金",
            "accent": "#f6f33e",
            "accentDeep": "#eae323",
            "accentSoft": "#FCF1D5",
            "accentWash": "#FFFCF4",
            "bgTop": "#FFFDF9",
            "bgBottom": "#F9EFD8",
            "success": "#2F9E7B",
            "doneTint": "#FBF3E2",
            "chip": "#FBF3E0",
            "contrast": "#2B2114"
        }
    })

    readonly property var p: key === "custom"
                                ? customPalette(customAccentColor)
                                : (palettes[key] !== undefined ? palettes[key] : palettes["mist"])

    // 强调色 / 背景
    readonly property color accent: p.accent
    readonly property color accentDeep: p.accentDeep
    readonly property color accentSoft: p.accentSoft
    readonly property color accentWash: p.accentWash
    readonly property color bgTop: p.bgTop
    readonly property color bgBottom: p.bgBottom

    // 语义色
    readonly property color success: p.success
    readonly property color doneTint: p.doneTint
    readonly property color danger: "#E5484D"
    readonly property color dangerSoft: "#FDEBEC"
    readonly property color warning: "#D99100"
    readonly property color warningSoft: "#FBF0D8"

    // 文本
    readonly property color ink: "#1D1D21"
    readonly property color inkSecondary: "#6B6B73"
    readonly property color faint: "#A3A3AB"
    readonly property color accentContrast: p.contrast

    // 表面
    readonly property color surface: "#FFFFFF"
    readonly property color card: Qt.rgba(1, 1, 1, 0.70)
    readonly property color cardBorder: Qt.rgba(1, 1, 1, 0.85)
    readonly property color chip: p.chip
    readonly property color track: Qt.rgba(0.11, 0.11, 0.13, 0.07)
    readonly property color hairline: Qt.rgba(0.11, 0.11, 0.13, 0.075)

    // 几何
    readonly property int windowRadius: 22
    readonly property int cardRadius: 18

    function alpha(c, a) {
        const col = (typeof c === "string") ? Qt.color(c) : c
        return Qt.rgba(col.r, col.g, col.b, a)
    }

    function mixColor(a, b, t) {
        const ca = Qt.color(a)
        const cb = Qt.color(b)
        return Qt.rgba(ca.r + (cb.r - ca.r) * t,
                       ca.g + (cb.g - ca.g) * t,
                       ca.b + (cb.b - ca.b) * t,
                       1)
    }

    function luminance(c) {
        const col = Qt.color(c)
        return 0.2126 * col.r + 0.7152 * col.g + 0.0722 * col.b
    }

    function hueOf(c) {
        const col = Qt.color(c)
        const r = col.r
        const g = col.g
        const b = col.b
        const max = Math.max(r, g, b)
        const min = Math.min(r, g, b)
        const delta = max - min
        if (delta === 0)
            return 0
        let hue
        if (max === r)
            hue = ((g - b) / delta) % 6
        else if (max === g)
            hue = (b - r) / delta + 2
        else
            hue = (r - g) / delta + 4
        hue /= 6
        return hue < 0 ? hue + 1 : hue
    }


    // 调色盘：从用户选的颜色推导整套浅色主题。
    function customPalette(accent) {
        const base = Qt.color(accent)
        const contrast = luminance(base) > 0.5 ? "#241F1A" : "#FFFFFF"
        return {
            "label": "自定义",
            "accent": base,
            "accentDeep": Qt.darker(base, 1.28),
            "accentSoft": mixColor("#FFFFFF", base, 0.16),
            "accentWash": mixColor("#FFFFFF", base, 0.05),
            "bgTop": mixColor("#FFFFFF", base, 0.02),
            "bgBottom": mixColor("#F1F2F6", base, 0.15),
            "success": "#1EA97C",
            "doneTint": mixColor("#FFFFFF", base, 0.06),
            "chip": mixColor("#FFFFFF", base, 0.08),
            "contrast": contrast
        }
    }
}