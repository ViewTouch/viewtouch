import QtQuick
import QtQuick.Controls.Fusion

// Who is working, as big buttons (clocked-in people first): touching one
// transfers the check you are on to them, then goes back.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property var people: {
        const list = pos ? pos.staff.filter(p => !p.me) : []
        return list.sort((a, b) => (b.clockedIn - a.clockedIn) || a.name.localeCompare(b.name))
    }

    GridView {
        id: grid
        anchors.fill: parent
        anchors.margins: 8
        clip: true
        readonly property int columns: Math.max(1, Math.floor((width - gridBar.width - 4) / 360))
        cellWidth: (width - gridBar.room) / columns
        cellHeight: 150
        model: w.people
        ScrollBar.vertical: TouchScrollBar { id: gridBar; needed: Math.ceil(grid.count / grid.columns) * (150) > grid.height }
        delegate: Item {
            id: cell
            required property var modelData
            width: grid.cellWidth
            height: grid.cellHeight
            ZoneShape {
                anchors.fill: parent
                anchors.margins: 8
                shape: "rounded"
                st: ({ fill: tap.pressed ? "#3b4556" : (cell.modelData.clockedIn ? "#1f5f3a" : "#2d3440"),
                       frame: "raised", frameWidth: 3, radius: 14, shadow: 4 })
            }
            Column {
                anchors.centerIn: parent
                width: parent.width - 32
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: cell.modelData.name
                    color: "white"
                    font.family: w.face
                    font.pixelSize: 30
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: (w.pos ? w.pos.roleName(cell.modelData.role) : cell.modelData.role) + (cell.modelData.clockedIn ? qsTr(" · on the clock") : "")
                    color: "#b8c0cc"
                    font.family: w.face
                    font.pixelSize: 20
                }
            }
            TapHandler {
                id: tap
                onTapped: {
                    if (TouchGuard.covered(point.scenePressPosition)) return   // the keyboard's touch
                    w.pos.transferCheck(cell.modelData.id)
                    w.zone.controller.goBack()
                }
            }
        }
        Text {
            anchors.centerIn: parent
            visible: grid.count === 0
            text: qsTr("Nobody else is set up")
            color: "#8a94a6"
            font.family: w.face
            font.pixelSize: 28
        }
    }
}
