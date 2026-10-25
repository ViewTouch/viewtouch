import QtQuick

// What the tables' colors mean, in the colors the tables use (the store's
// status colors, or the ones for color blindness): beside a floor plan.
// props.long: false leaves out "Seated long".
Item {
    id: w
    property ZoneItem zone
    readonly property string face: zone ? (zone.st.font ?? Ui.font) : Ui.font
    readonly property color ink: zone ? (zone.st.textColor ?? Ui.textSoft) : Ui.textSoft
    readonly property var keys: [
        { name: qsTr("Free"), color: zone ? (zone.st.keyFill ?? Ui.button) : Ui.button, edge: Ui.muted },
        { name: qsTr("Yours"), color: zone ? zone.statusColor("tableMine", Ui.go) : Ui.go },
        { name: qsTr("Someone else's"), color: zone ? zone.statusColor("tableOpen", "#a86a12") : "#a86a12" },
        { name: qsTr("The one you're on"), color: zone ? zone.statusColor("tableCurrent", Ui.accent) : Ui.accent },
        { name: qsTr("Reserved"), color: zone ? zone.statusColor("tableReserved", "#5b4aa8") : "#5b4aa8" },
        { name: qsTr("Needs bussing"), color: zone ? zone.statusColor("tableDirty", "#7a3b3b") : "#7a3b3b" }
    ].concat(zone && zone.props && zone.props.long === false ? []
             : [{ name: qsTr("Seated long"), color: "transparent", edge: zone ? zone.statusColor("tableLong", "#ff4d4d") : "#ff4d4d" }])
    // The words' size: one row on a wide screen, two on a phone's.
    readonly property int rows: width < 1200 ? 2 : 1
    TextMetrics { id: names; font.family: w.face; font.pixelSize: 100; text: w.keys.map(k => k.name).join("") }
    // ...and no bigger than lets every one fit in those rows (a swatch and the gaps are about 2.6 of it each).
    readonly property real fits: width * rows * 0.92 / (names.advanceWidth / 100 + keys.length * 2.6)
    readonly property real px: Math.max(10, Math.min(height / (rows * 1.9), 34, fits))

    Flow {
        anchors.fill: parent
        anchors.margins: w.px * 0.4
        spacing: w.px * 0.9
        Repeater {
            model: w.keys
            delegate: Row {
                required property var modelData
                spacing: w.px * 0.35
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: w.px * 1.1
                    height: w.px * 1.1
                    radius: w.px * 0.25
                    color: modelData.color
                    border.color: modelData.edge ?? Qt.lighter(modelData.color, 1.4)
                    border.width: modelData.edge ? Math.max(2, w.px * (modelData.color === "transparent" ? 0.18 : 0.08)) : 1
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.name
                    color: w.ink
                    font.family: w.face
                    font.pixelSize: w.px
                }
            }
        }
    }
}
