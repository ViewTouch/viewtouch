pragma Singleton
import QtQuick

// The colors offered for categories, item buttons and page buttons: one list
// for the Menu Builder, the order screen's Arrange and the page editor. Ten
// colors, each dark, middle and light (a row each, in that order); the middle
// row holds the eight colors offered before, so stores' colors still match.
QtObject {
    readonly property var dark:   ["#7a1f1f", "#a8521a", "#7a5410", "#1f5f3a", "#145257", "#1d4580", "#4a2f8f", "#8a1f5c", "#5c3b1d", "#2b3038"]
    readonly property var middle: ["#b83232", "#d9732b", "#a86a12", "#1f8a4c", "#1f6f73", "#2b62b0", "#6b46c1", "#c2408a", "#8a5a2b", "#4a5260"]
    readonly property var light:  ["#e06666", "#f2a65a", "#f2d16b", "#6cc58a", "#4fb3b0", "#6c9ce0", "#a58be0", "#ec8fc0", "#c49a6c", "#9aa3b0"]
    readonly property var all: dark.concat(middle).concat(light)
    // New categories take these in turn.
    readonly property var starters: ["#a86a12", "#b83232", "#1f8a4c", "#1f6f73", "#2b62b0", "#6b46c1", "#8a5a2b", "#4a5260",
                                     "#d9732b", "#c2408a", "#145257", "#7a1f1f"]

    // Themes: every category recolored at once, in their order (Colors…).
    readonly property var themes: [
        { id: "classic", name: qsTr("Classic"), colors: starters },
        { id: "warm", name: qsTr("Warm"), colors: ["#b83232", "#d9732b", "#a86a12", "#c2408a", "#8a5a2b", "#7a1f1f",
                                                   "#a8521a", "#8a1f5c", "#5c3b1d", "#7a5410", "#e06666", "#f2a65a"] },
        { id: "cool", name: qsTr("Cool"), colors: ["#2b62b0", "#1f6f73", "#6b46c1", "#1f8a4c", "#1d4580", "#145257",
                                                   "#4a2f8f", "#1f5f3a", "#4a5260", "#6c9ce0", "#4fb3b0", "#a58be0"] },
        { id: "earth", name: qsTr("Earth"), colors: ["#8a5a2b", "#1f5f3a", "#a86a12", "#5c3b1d", "#7a5410", "#145257",
                                                     "#a8521a", "#4a5260", "#7a1f1f", "#c49a6c", "#1f6f73", "#2b3038"] },
        { id: "bright", name: qsTr("Bright"), colors: ["#e53935", "#fb8c00", "#f2c200", "#43a047", "#00acc1", "#1e88e5",
                                                       "#8e24aa", "#d81b60", "#6d4c41", "#00897b", "#3949ab", "#c0ca33"] },
        { id: "soft", name: qsTr("Soft"), colors: light.concat(["#f7c6c6", "#cfe8d5"]) },
        { id: "contrast", name: qsTr("High contrast"), colors: ["#b71c1c", "#1b5e20", "#0d47a1", "#f9a825", "#4a148c", "#e65100",
                                                                "#006064", "#880e4f", "#3e2723", "#263238", "#33691e", "#01579b"] }
    ]

    // Text that reads on it: dark on light colors, white on the rest.
    function ink(c) {
        if (!c) return "white"
        const k = Qt.color(c)
        return 0.299 * k.r + 0.587 * k.g + 0.114 * k.b > 0.6 ? "#14171c" : "white"
    }
    function same(a, b) {
        return !!a && !!b && String(a).toLowerCase() === String(b).toLowerCase()
    }
    function valid(c) {
        return /^#[0-9a-fA-F]{6}$/.test(String(c ?? "").trim())
    }
}
