import QtQuick
import QtQuick.Controls.Fusion

// Every table in the store as a big button, for phones: the floor plan's
// tables (wherever they are placed) in a scrolling grid with their status.
// props.columns: buttons per row (default 3). props.action "move": touching
// a table moves the check you are on there, then goes back.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property int columns: zone && zone.props && zone.props.columns ? zone.props.columns : 3
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property var tables: zone && zone.controller ? zone.controller.tables() : []

    GridView {
        id: grid
        anchors.fill: parent
        clip: true
        readonly property int columns: w.columns
        cellWidth: (width - gridBar.room) / columns
        cellHeight: Math.min(cellWidth * 0.8, 240)
        model: w.tables
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: TouchScrollBar { id: gridBar; needed: Math.ceil(grid.count / grid.columns) * (Math.min((width - gridBar.width - 4) / columns * 0.8, 240)) > grid.height }

        delegate: Item {
            id: cell
            required property var modelData
            width: grid.cellWidth
            height: grid.cellHeight
            // Again when the checks change (compared, not just read: an unused read is compiled away).
            readonly property var status: !w.pos || !w.pos.openChecks ? ({ open: false }) : w.pos.tableStatus(modelData.name)
            readonly property color tint: !status.open ? (w.zone.st.fill ?? "#2d3440")
                                          : status.current ? "#2f6fd6"
                                          : status.mine ? "#1f8a4c" : "#a86a12"
            ZoneShape {
                anchors.fill: parent
                anchors.margins: 8
                shape: "rounded"
                st: Object.assign({}, w.zone.st, { fill: tap.pressed ? Qt.lighter(cell.tint, 1.3) : cell.tint })
            }
            Column {
                anchors.centerIn: parent
                width: parent.width - 24
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: cell.modelData.name
                    color: w.zone.st.textColor ?? "white"
                    font.family: w.face
                    font.pixelSize: Math.min(cell.height * 0.3, 56)
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: cell.status.open ? (cell.status.total ?? "")
                                           : cell.modelData.seats === 1 ? qsTr("1 seat")
                                           : cell.modelData.seats > 1 ? qsTr("%1 seats").arg(cell.modelData.seats) : ""
                    color: Qt.darker(w.zone.st.textColor ?? "white", cell.status.open ? 1.0 : 1.4)
                    font.family: w.face
                    font.pixelSize: Math.min(cell.height * 0.15, 28)
                }
            }
            TapHandler {
                id: tap
                onTapped: {
                    if (TouchGuard.covered(point.scenePressPosition)) return   // the keyboard's touch
                    if ((w.zone.props.action ?? "") === "move") {
                        w.pos.moveCheck(cell.modelData.name)
                        w.zone.controller.goBack()
                    } else {
                        w.zone.controller.selectTable(cell.modelData.name)
                    }
                }
            }
        }
    }
}
