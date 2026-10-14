import QtQuick
import QtQuick.Controls.Fusion

// The menu by category: touch an item to mark it sold out (86) or back.
// props.modifiers: true lists modifiers too.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property bool withModifiers: zone && zone.props && zone.props.modifiers === true
    readonly property var items: {
        const all = pos ? pos.menuItems.filter(m => w.withModifiers || !m.modifier) : []
        return all.sort((a, b) => (a.family || "~").localeCompare(b.family || "~") || a.name.localeCompare(b.name))
    }

    GridView {
        id: grid
        anchors.fill: parent
        anchors.margins: 8
        clip: true
        // Upright (a phone): two a row, bigger.
        readonly property bool narrow: w.zone ? w.zone.narrow : false
        readonly property real big: narrow ? 1.7 : 1
        readonly property int columns: narrow ? 2 : Math.max(1, Math.floor((width - gridBar.width - 4) / 300))
        cellWidth: (width - gridBar.room) / columns
        cellHeight: 120 * big
        model: w.items
        ScrollBar.vertical: TouchScrollBar { id: gridBar; needed: Math.ceil(grid.count / grid.columns) * grid.cellHeight > grid.height }
        delegate: Item {
            id: cell
            required property var modelData
            width: grid.cellWidth
            height: grid.cellHeight
            ZoneShape {
                anchors.fill: parent
                anchors.margins: 6
                shape: "rounded"
                st: ({ fill: tap.pressed ? "#4c8dff" : (cell.modelData.available ? "#2d3440" : "#7a2323"),
                       frame: "raised", frameWidth: 3, radius: 12, shadow: 3 })
            }
            Column {
                anchors.centerIn: parent
                width: parent.width - 28
                Text {
                    width: parent.width
                    text: cell.modelData.name
                    color: "white"
                    font.family: w.face
                    font.pixelSize: 24 * grid.big
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: (cell.modelData.available ? qsTr("Available") : qsTr("SOLD OUT (86)"))
                          + (cell.modelData.family ? "  ·  " + cell.modelData.family : "")
                    color: cell.modelData.available ? "#8a94a6" : "#ffd0d0"
                    font.family: w.face
                    font.pixelSize: 18 * grid.big
                    elide: Text.ElideRight
                }
            }
            TapHandler {
                id: tap
                onTapped: { if (TouchGuard.covered(point.scenePressPosition)) return; w.pos.setAvailable(cell.modelData.id, !cell.modelData.available) }
            }
        }
    }
}
