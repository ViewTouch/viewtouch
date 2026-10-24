pragma Singleton
import QtQuick

// The colors and font the built-in panels draw with (the check, keypads,
// reports, sheets): the store's style's (Store Settings -> Screens -> Style;
// its theme's "palette"), each with the Classic color when the theme has
// none. Signal colors (amber for a warning, red for an allergy) stay fixed.
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
}
