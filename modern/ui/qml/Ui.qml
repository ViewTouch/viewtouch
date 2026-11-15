pragma Singleton
import QtQuick

// The colors and font the built-in panels draw with (the check, keypads,
// reports, sheets): the store's style's (Store Settings -> Screens -> Style;
// its theme's "palette"), each with the Classic color when the theme has
// none. Signal colors (amber for a warning, red for an allergy) stay the
// same in every style; color-safe screens get their own.
//
// Sizes: the type scale (fontXs..fontXxl) and px(n) for a size of its own,
// both times the text size of whoever is here (Employees, Terminals, Aa).
// textStyle: the text effect (None, Embossed, Outline) for keys and labels.
QtObject {
    // The style's palette, set by the window (LayoutController::stylePalette).
    property var theme: ({})

    readonly property color text: theme.text ?? "#e6e9ef"            // words
    readonly property color textSoft: theme.textSoft ?? "#b8c0cc"    // secondary words
    readonly property color muted: theme.muted ?? "#8a94a6"          // hints, labels
    readonly property color key: theme.key ?? "#343c49"              // a panel's keys
    readonly property color button: theme.button ?? "#2d3440"        // plain buttons
    readonly property color panel: theme.panel ?? "#232933"          // a panel's face
    readonly property color sunken: theme.sunken ?? "#1d2128"        // a field, a well
    readonly property color deep: theme.deep ?? "#14171c"            // behind it all
    readonly property color line: theme.line ?? "#3a4250"            // edges, dividers
    readonly property color accent: theme.accent ?? "#2f6fd6"        // chosen, selected
    readonly property color go: theme.go ?? "#1f8a4c"                // OK, done, start
    readonly property color goDark: theme.goDark ?? "#1f6b40"        // the same, quieter
    readonly property string font: theme.font ?? "DejaVu Sans"

    // Whoever is here: their text size (1 = 100%) and text effect, and color-safe colors.
    property real textScale: 1
    property string textEffect: ""
    property bool colorSafe: false
    readonly property int textStyle: textEffect === "embossed" ? Text.Raised : textEffect === "outline" ? Text.Outline : Text.Normal
    // The effect's second color, for text of `color`: a darker shade on light
    // text, a lighter one on dark text (the raised edge shows either way).
    function effectColor(c) {
        const k = Qt.color(c)
        return k.hslLightness > 0.55 ? Qt.darker(k, 3.2) : Qt.lighter(k, 2.4)
    }

    // A size of its own (pixels at 100%), times the text size.
    function px(n) { return Math.round(n * textScale) }
    readonly property int fontXs: px(14)    // fine print, a key's hint
    readonly property int fontSm: px(16)    // secondary lines
    readonly property int fontMd: px(20)    // body, list rows
    readonly property int fontLg: px(26)    // headings, amounts
    readonly property int fontXl: px(34)    // a page's title, the total
    readonly property int fontXxl: px(48)   // the one number that matters (balance due)
    // Spacing steps and a row tall enough for a finger and its words.
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space6: 24
    readonly property int rowH: Math.max(48, Math.round(fontMd * 2.2))

    // Signal colors: what they mean, the same in every style (color-safe: Okabe and Ito's, as ZoneItem's).
    readonly property color warn: colorSafe ? "#f0e442" : "#f5b940"      // running low, waiting, look at this
    readonly property color danger: colorSafe ? "#d55e00" : "#ff6369"    // late, allergy, void
    readonly property color ok: colorSafe ? "#56b4e9" : "#7ee2a8"        // done, ready, paid
    readonly property color info: "#8ab4ff"                               // a note, a hint that matters
    readonly property color returning: colorSafe ? "#e69f00" : "#ff8a80" // coming back, owed back

    // The device asks for less motion (its accessibility settings, Qt 6.12):
    // what only decorates stands still (flashes, slides, glides); what says
    // something (a timer, a progress bar) still moves.
    readonly property bool reduceMotion: Application.styleHints.accessibility.motionPreference === Qt.ReducedMotion
}
