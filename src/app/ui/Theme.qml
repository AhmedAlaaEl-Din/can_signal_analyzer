pragma Singleton
import QtQuick

// Design tokens for the whole application. See docs/design.md.
//
// Colors are defined twice (light + dark) and every public color is a blend
// of the two, driven by a single animated value `t` (0 = light, 1 = dark).
// One NumberAnimation therefore cross-fades the whole UI on theme switch.
QtObject {
    id: theme

    // ---------------------------------------------------------------- mode
    property bool dark: true
    property real t: 1.0
    readonly property NumberAnimation _themeAnimation: NumberAnimation {
        target: theme
        property: "t"
        duration: theme.motion.slow
        easing.type: Easing.InOutCubic
    }
    onDarkChanged: {
        _themeAnimation.stop()
        _themeAnimation.to = dark ? 1.0 : 0.0
        _themeAnimation.start()
    }
    function toggle() { dark = !dark }

    function blend(light, darkColor) {
        const k = t
        return Qt.rgba(light.r + (darkColor.r - light.r) * k,
                       light.g + (darkColor.g - light.g) * k,
                       light.b + (darkColor.b - light.b) * k,
                       light.a + (darkColor.a - light.a) * k)
    }
    function alpha(c, a) { return Qt.rgba(c.r, c.g, c.b, a) }

    // ------------------------------------------------------------ palettes
    readonly property QtObject light: QtObject {
        readonly property color bg: "#F3F4F7"
        readonly property color surface: "#FFFFFF"
        readonly property color raised: "#F8F9FB"
        readonly property color sunken: "#FBFBFD"
        readonly property color popup: "#FFFFFF"
        readonly property color border: "#E1E4EA"
        readonly property color borderStrong: "#C9CED8"
        readonly property color text: "#151922"
        readonly property color textMuted: "#4E5668"
        readonly property color textFaint: "#646C7E"
        readonly property color accent: "#5A43D6"
        readonly property color accentHover: "#4C36C4"
        readonly property color accentPressed: "#3F2BAE"
        readonly property color accentText: "#5A43D6"
        readonly property color textOnAccent: "#FFFFFF"
        readonly property color ok: "#16794C"
        readonly property color warning: "#9A5800"
        readonly property color error: "#C4342F"
        readonly property color ink: "#0E1014"   // overlay base (hover/pressed/zebra)
        readonly property real zebra: 0.022
        readonly property real hover: 0.045
        readonly property real pressed: 0.08
        readonly property real shadow: 0.10
    }
    readonly property QtObject darkPalette: QtObject {
        readonly property color bg: "#0E1014"
        readonly property color surface: "#15181E"
        readonly property color raised: "#1B1F27"
        readonly property color sunken: "#111318"
        readonly property color popup: "#1E222B"
        readonly property color border: "#242934"
        readonly property color borderStrong: "#343A48"
        readonly property color text: "#E7EAF0"
        readonly property color textMuted: "#A2AABA"
        readonly property color textFaint: "#848D9E"
        readonly property color accent: "#A594FF"
        readonly property color accentHover: "#B5A7FF"
        readonly property color accentPressed: "#9381F5"
        readonly property color accentText: "#B4A6FF"
        readonly property color textOnAccent: "#14102B"
        readonly property color ok: "#4CD08A"
        readonly property color warning: "#F2B54B"
        readonly property color error: "#F47C78"
        readonly property color ink: "#FFFFFF"
        readonly property real zebra: 0.018
        readonly property real hover: 0.045
        readonly property real pressed: 0.08
        readonly property real shadow: 0.45
    }

    // ------------------------------------------------------- public colors
    readonly property color bg: blend(light.bg, darkPalette.bg)
    readonly property color surface: blend(light.surface, darkPalette.surface)
    readonly property color raised: blend(light.raised, darkPalette.raised)
    readonly property color sunken: blend(light.sunken, darkPalette.sunken)
    readonly property color popup: blend(light.popup, darkPalette.popup)
    readonly property color border: blend(light.border, darkPalette.border)
    readonly property color borderStrong: blend(light.borderStrong, darkPalette.borderStrong)
    readonly property color text: blend(light.text, darkPalette.text)
    readonly property color textMuted: blend(light.textMuted, darkPalette.textMuted)
    readonly property color textFaint: blend(light.textFaint, darkPalette.textFaint)
    readonly property color accent: blend(light.accent, darkPalette.accent)
    readonly property color accentHover: blend(light.accentHover, darkPalette.accentHover)
    readonly property color accentPressed: blend(light.accentPressed, darkPalette.accentPressed)
    readonly property color accentText: blend(light.accentText, darkPalette.accentText)
    readonly property color textOnAccent: blend(light.textOnAccent, darkPalette.textOnAccent)
    readonly property color ok: blend(light.ok, darkPalette.ok)
    readonly property color warning: blend(light.warning, darkPalette.warning)
    readonly property color error: blend(light.error, darkPalette.error)

    // Translucent overlays (state layers), derived from the ink color.
    readonly property color _ink: blend(light.ink, darkPalette.ink)
    readonly property real _zebraA: light.zebra + (darkPalette.zebra - light.zebra) * t
    readonly property color zebra: alpha(_ink, _zebraA)
    readonly property color hover: alpha(_ink, light.hover)
    readonly property color pressed: alpha(_ink, light.pressed)
    readonly property color selection: alpha(accent, 0.14 + 0.06 * t)
    readonly property color accentSubtle: alpha(accent, 0.12 + 0.04 * t)
    readonly property color focusRing: alpha(accent, 0.55)
    readonly property color shadow: Qt.rgba(0, 0, 0, light.shadow + (darkPalette.shadow - light.shadow) * t)
    readonly property color highlightChange: alpha(warning, 0.15 + 0.01 * t)

    function toneColor(tone) {
        switch (tone) {
        case "accent": return accentText
        case "ok": return ok
        case "warning": return warning
        case "error": return error
        default: return textMuted
        }
    }
    function toneFill(tone) {
        switch (tone) {
        case "accent": return alpha(accent, 0.12 + 0.03 * t)
        case "ok": return alpha(ok, 0.12 + 0.02 * t)
        case "warning": return alpha(warning, 0.13 + 0.02 * t)
        case "error": return alpha(error, 0.12 + 0.03 * t)
        default: return alpha(_ink, 0.05)
        }
    }

    // ---------------------------------------------------------- typography
    function _pick(candidates) {
        const available = Qt.fontFamilies()
        for (let i = 0; i < candidates.length; ++i) {
            if (available.indexOf(candidates[i]) >= 0)
                return candidates[i]
        }
        return candidates[candidates.length - 1]
    }
    // Sans for UI text; monospace with fixed-width digits for IDs, data and times.
    readonly property string fontSans: _pick(["Segoe UI Variable Text", "Segoe UI Variable", "Segoe UI", "sans-serif"])
    readonly property string fontDisplay: _pick(["Segoe UI Variable Display", "Segoe UI Variable", "Segoe UI", "sans-serif"])
    readonly property string fontMono: _pick(["Cascadia Mono", "Consolas", "Courier New", "monospace"])

    readonly property QtObject type: QtObject {
        readonly property int caption: 11   // column headers, chip labels
        readonly property int small: 12     // secondary text
        readonly property int body: 13      // default UI text
        readonly property int mono: 13      // table data
        readonly property int title: 15     // app name, section titles
        readonly property int headline: 20  // empty states
        readonly property real captionSpacing: 0.6 // letter spacing for UPPERCASE captions
    }

    // ------------------------------------------------- spacing, radius, size
    readonly property QtObject space: QtObject {
        readonly property int xxs: 2
        readonly property int xs: 4
        readonly property int sm: 8
        readonly property int md: 12
        readonly property int lg: 16
        readonly property int xl: 24
        readonly property int xxl: 32
    }
    readonly property QtObject radius: QtObject {
        readonly property int sm: 4
        readonly property int md: 6
        readonly property int lg: 10
        readonly property int pill: 999
    }
    readonly property QtObject size: QtObject {
        readonly property int control: 32
        readonly property int controlSmall: 26
        readonly property int toolbar: 60
        readonly property int statusbar: 32
        readonly property int row: 26
        readonly property int header: 32
        readonly property int icon: 16
    }

    // ------------------------------------------------------------ elevation
    // level 0: flush with the page; 1: cards; 2: floating pills; 3: popups.
    function elevationColor(level) {
        switch (level) {
        case 0: return bg
        case 1: return surface
        case 2: return raised
        default: return popup
        }
    }
    function shadowBlur(level) { return [0, 0.25, 0.45, 0.7][Math.min(level, 3)] }
    function shadowOffset(level) { return [0, 1, 3, 8][Math.min(level, 3)] }

    // --------------------------------------------------------------- motion
    readonly property QtObject motion: QtObject {
        readonly property int fast: 120    // hover, press
        readonly property int normal: 160  // tabs, state changes
        readonly property int slow: 200    // theme switch, page transitions
        readonly property int highlight: 650 // decay of "byte changed" highlight
        readonly property int easing: Easing.OutCubic
    }
}
