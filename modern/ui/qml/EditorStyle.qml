pragma Singleton
import QtQuick

// Colors and metrics for the editor chrome (not the POS pages themselves,
// which are styled by the layout theme).
QtObject {
    readonly property color chrome: "#15181d"
    readonly property color panel: "#1f232a"
    readonly property color panelRaised: "#272c35"
    readonly property color border: "#343b47"
    readonly property color text: "#e6e9ef"
    readonly property color muted: "#8a94a6"
    readonly property color accent: "#4c8dff"
    readonly property color guide: "#ff4fa3"
    readonly property color danger: "#ff6369"
    readonly property color warning: "#f5b940"
    readonly property int panelWidth: 340
    readonly property int pagesWidth: 240
    readonly property int toolbarHeight: 48
}
