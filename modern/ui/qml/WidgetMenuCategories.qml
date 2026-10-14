import QtQuick

// The menu's categories as buttons, laid out by themselves, in the Menu
// Builder's order and colors: touch one for its items (the menu page on that
// category). New categories show up with no page editing. props.period: the
// meal period's categories (empty: this page's, else what's on now);
// props.columns.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property int columns: zone && zone.props && zone.props.columns > 0 ? zone.props.columns : 3
    readonly property string period: zone && zone.props && zone.props.period ? zone.props.period : ""
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    // With items, and on the menu at this meal (or all day).
    readonly property var shown: (pos ? pos.menuCategories : []).filter(c =>
        c.count > 0 && (period !== "" ? (c.periods.length === 0 || c.periods.includes(period)) : c.now))
    readonly property real gap: 16
    readonly property int rows: Math.max(1, Math.ceil(shown.length / columns))
    // Dark text on light buttons.
    function inkOn(c) {
        const k = Qt.color(c)
        return 0.299 * k.r + 0.587 * k.g + 0.114 * k.b > 0.6 ? "#14171c" : "white"
    }

    Grid {
        anchors.fill: parent
        columns: w.columns
        spacing: w.gap
        Repeater {
            model: w.shown
            delegate: WidgetKey {
                required property var modelData
                objectName: "category-" + modelData.id
                width: (w.width - w.gap * (w.columns - 1)) / w.columns
                // As tall as fits, up to a comfortable size.
                height: Math.min(260, (w.height - w.gap * (w.rows - 1)) / w.rows)
                text: qsTranslate("Page", modelData.name)
                baseColor: modelData.color || (w.zone.st.keyFill ?? "#343c49")
                textColor: modelData.color ? w.inkOn(modelData.color) : (w.zone.st.keyTextColor ?? "white")
                fontScale: 0.2
                onClicked: w.zone.controller.openCategory(modelData.id)
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: w.shown.length === 0
        text: qsTr("Nothing on the menu yet: Manager → Menu Builder.")
        color: "#8a94a6"
        font.family: w.face
        font.pixelSize: 28
    }
}
