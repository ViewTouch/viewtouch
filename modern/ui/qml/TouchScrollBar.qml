import QtQuick
import QtQuick.Controls.Fusion

// A scrollbar wide enough to drag with a finger (about 8 mm), shown only when
// the list holds more than fits. Rows leave `room` for it on the right.
ScrollBar {
    id: bar
    readonly property real room: policy === ScrollBar.AlwaysOn ? width + 4 : 0
    // Grids set this from their row count: reading `size` there would loop
    // (the bar's room changes the cells, which change the content height).
    property bool needed: size < 0.999

    policy: needed ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
    // Its size as implicitWidth too: a list it's attached to sizes it by that
    // (Qt 6.8), which narrowed it to its handle.
    // At least 40 pixels on the screen, also on a page drawn smaller than it
    // was made (a 1920-wide page on a smaller screen).
    readonly property real drawnScale: {
        const p = bar.parent
        if (!p)
            return 1
        const r = p.mapToItem(null, 0, 0, 100, 100)
        return r.width > 0 ? Math.max(0.3, r.width / 100) : 1
    }
    readonly property real thickness: Math.round(Math.max(28, 40 / drawnScale, Math.min(44, Screen.pixelDensity * 8)))
    implicitWidth: thickness
    width: thickness
    padding: 3
    minimumSize: 0.12

    background: Rectangle {
        radius: width / 2
        color: "#141820"
        opacity: 0.85
    }
    contentItem: Rectangle {
        implicitWidth: bar.thickness - 6
        radius: width / 2
        color: bar.pressed ? "#7aa7ff" : "#56627a"
    }
}
