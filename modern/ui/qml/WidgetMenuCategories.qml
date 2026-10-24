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
    // ♥ Mine first, once the person logged in has a few usuals.
    readonly property var shown: ((pos && pos.myUsuals.length >= 4) ? [{ id: "♥", name: qsTr("♥ Mine"), color: "#7a2f55", mine: true }] : [])
        .concat((pos ? pos.menuCategories : []).filter(c =>
        c.count > 0 && (period !== "" ? (c.periods.length === 0 || c.periods.includes(period)) : c.now)))
    readonly property bool phone: zone && zone.controller ? zone.controller.formFactor === "phone" : false
    readonly property real gap: phone ? 8 : 16
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
                objectName: modelData.mine ? "category-mine" : "category-" + modelData.id
                width: (w.width - w.gap * (w.columns - 1)) / w.columns
                // As tall as fits, up to a comfortable size (a phone: about 56 on the screen).
                height: Math.min(w.phone ? w.zone.touch(56) : 260, (w.height - w.gap * (w.rows - 1)) / w.rows)
                text: modelData.mine ? modelData.name : qsTranslate("Page", modelData.name)
                readonly property bool mark: w.zone.st.menuColors === "mark"   // Modern: a strip of its color
                baseColor: (!mark && modelData.color) || (w.zone.st.keyFill ?? "#343c49")
                textColor: modelData.color && !mark ? w.inkOn(modelData.color) : (w.zone.st.keyTextColor ?? "white")
                Rectangle {
                    visible: parent.mark && !!parent.modelData.color
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.topMargin: 4
                    width: parent.width - parent.radius * 2
                    height: Math.max(3, parent.height * 0.06)
                    radius: height / 2
                    color: parent.modelData.color || "transparent"
                }
                fontScale: w.phone ? 0.3 : 0.2
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
