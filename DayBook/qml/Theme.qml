pragma Singleton
import QtQuick
import DayBook

QtObject {
    id: t

    readonly property bool dark: App.isDark
    readonly property string accentKey: App.accentName
    readonly property string fontFamily: App.fontFamily
    readonly property string numberFamily: App.numberFamily
    readonly property string iconFamily: App.iconFontFamily

    readonly property var accentData: {
        const map = {
            "coral": { "main": "#EE6C50", "deep": "#D2553A", "softLight": "#FBE5DD", "softDark": "#3B241E", "contrast": "#FFF9F5" },
            "sage":  { "main": "#2FAE83", "deep": "#208A66", "softLight": "#DFF3EA", "softDark": "#16342B", "contrast": "#F7FFFB" },
            "azure": { "main": "#4F8EF7", "deep": "#356FD5", "softLight": "#E3EDFD", "softDark": "#182840", "contrast": "#F8FBFF" },
            "amber": { "main": "#E5A13C", "deep": "#C28122", "softLight": "#F9EDD6", "softDark": "#3A2D18", "contrast": "#FFFBF3" },
            "plum":  { "main": "#A47BE8", "deep": "#8157C8", "softLight": "#EEE6FA", "softDark": "#2B213B", "contrast": "#FCFAFF" },
            "pollen": { "main": "#F2A7BD", "deep": "#D77F98", "softLight": "#FBECF1", "softDark": "#3B2630", "contrast": "#3A1F2A" }
        }
        if (accentKey === "custom") {
            const c = App.customAccentColor
            const luminance = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b
            return {
                "main": c,
                "deep": Qt.darker(c, 1.24),
                "softLight": Qt.lighter(c, 1.72),
                "softDark": Qt.darker(c, 2.2),
                "contrast": luminance > 0.64 ? "#231F1A" : "#FFFFFF"
            }
        }
        return map[accentKey] !== undefined ? map[accentKey] : map["coral"]
    }

    readonly property color accent: accentData.main
    readonly property color accentDeep: accentData.deep
    readonly property color accentContrast: accentData.contrast
    readonly property color accentSoft: dark ? accentData.softDark : accentData.softLight
    readonly property color accentWash: dark ? withAlpha(accent, 0.18) : withAlpha(accent, 0.11)

    // Surfaces
    readonly property color bgTop: dark ? "#121820" : "#F7FCF8"
    readonly property color bgBottom: dark ? "#0A0E12" : "#FBF2F5"
    readonly property color glowPeach: dark ? Qt.rgba(0.31, 0.56, 0.97, 0.10) : Qt.rgba(1.00, 0.69, 0.56, 0.30)
    readonly property color glowSage: dark ? Qt.rgba(0.18, 0.68, 0.51, 0.08) : Qt.rgba(0.66, 0.85, 0.76, 0.26)
    readonly property color glowLilac: dark ? Qt.rgba(0.64, 0.48, 0.91, 0.08) : Qt.rgba(0.86, 0.79, 0.95, 0.24)
    readonly property color windowBorder: dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0.137, 0.122, 0.102, 0.10)
    readonly property color windowShadow: dark ? Qt.rgba(0, 0, 0, 0.62) : Qt.rgba(0.247, 0.180, 0.114, 0.28)

    readonly property color ink: dark ? "#F4F0E7" : "#231F1A"
    readonly property color inkSecondary: dark ? "#CAC2B4" : "#575047"
    readonly property color muted: dark ? "#948D82" : "#8C8377"
    readonly property color faint: dark ? "#655F57" : "#B7AEA1"

    readonly property color surface: dark ? Qt.rgba(1, 1, 1, 0.055) : Qt.rgba(255, 255, 255, 0.54)
    readonly property color surfaceRaised: dark ? Qt.rgba(1, 1, 1, 0.085) : Qt.rgba(255, 255, 255, 0.76)
    readonly property color surfaceSolid: dark ? "#1A2028" : "#FCF9F3"
    readonly property color surfaceSunken: dark ? Qt.rgba(0, 0, 0, 0.18) : Qt.rgba(0.137, 0.122, 0.102, 0.050)
    readonly property color sidebarSurface: dark ? Qt.rgba(255, 255, 255, 0.045) : Qt.rgba(255, 255, 255, 0.40)
    readonly property color cardBorder: dark ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(255, 255, 255, 0.72)
    readonly property color cardBorderStrong: dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(255, 255, 255, 0.92)
    readonly property color hairline: dark ? Qt.rgba(1, 1, 1, 0.075) : Qt.rgba(0.137, 0.122, 0.102, 0.080)
    readonly property color hairlineStrong: dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0.137, 0.122, 0.102, 0.14)
    readonly property color glassSheen: dark ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(255, 255, 255, 0.5)
    readonly property color cardShadow: dark ? Qt.rgba(0, 0, 0, 0.34) : Qt.rgba(0.298, 0.216, 0.125, 0.15)

    // Hero
    readonly property color heroA: dark ? "#1B222A" : "#24201C"
    readonly property color heroB: dark ? "#27313B" : "#3A2E26"
    readonly property color heroInk: "#FBF7EE"
    readonly property color heroMuted: "#BBAF9E"
    readonly property color heroHairline: Qt.rgba(1, 1, 1, 0.10)

    // Semantic
    readonly property color income: dark ? "#48C99A" : "#259E76"
    readonly property color incomeSoft: dark ? Qt.rgba(0.28, 0.79, 0.60, 0.16) : Qt.rgba(0.15, 0.62, 0.46, 0.10)
    readonly property color expense: dark ? "#F58A6E" : "#DD6247"
    readonly property color expenseSoft: dark ? Qt.rgba(0.96, 0.54, 0.43, 0.16) : Qt.rgba(0.87, 0.38, 0.28, 0.10)
    readonly property color danger: dark ? "#F07E6B" : "#D9513C"
    readonly property color dangerSoft: dark ? Qt.rgba(0.94, 0.49, 0.42, 0.16) : Qt.rgba(0.85, 0.32, 0.24, 0.10)
    readonly property color warning: "#E3A23F"
    readonly property color success: income

    // Charts
    readonly property color chartGrid: dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0.137, 0.122, 0.102, 0.10)
    readonly property color chartIncome: dark ? "#52C99B" : "#2FAE83"
    readonly property color chartExpense: accent
    readonly property color tooltipBg: dark ? "#242C35" : "#2A2520"
    readonly property color tooltipText: "#FBF7EE"

    // Geometry
    readonly property int windowRadius: 26
    readonly property int panelRadius: 22
    readonly property int cardRadius: 18
    readonly property int controlRadius: 12
    readonly property int buttonRadius: 13

    readonly property int pagePadding: 22
    readonly property int gap: 16
    readonly property int smallGap: 10

    function toColor(c) {
        if (typeof c === "string") {
            const s = c.trim()
            if (s.charAt(0) === "#" && (s.length === 7 || s.length === 9)) {
                const r = parseInt(s.substr(1, 2), 16) / 255
                const g = parseInt(s.substr(3, 2), 16) / 255
                const b = parseInt(s.substr(5, 2), 16) / 255
                const a = s.length === 9 ? parseInt(s.substr(7, 2), 16) / 255 : 1
                return Qt.rgba(r, g, b, a)
            }
            return Qt.rgba(0.55, 0.55, 0.55, 1)
        }
        return c
    }

    function withAlpha(c, a) {
        const color = toColor(c)
        return Qt.rgba(color.r, color.g, color.b, a)
    }

    function categoryTint(c) {
        return dark ? withAlpha(c, 0.20) : withAlpha(c, 0.13)
    }

    function opacityForHover(base, hovered) {
        return hovered ? Math.min(1, base + 0.05) : base
    }
}
