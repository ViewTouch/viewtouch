import QtQuick
import QtQuick.Controls.Fusion

// What has been done to the check you are on: transfers, moves, merges,
// reopening, voids, discounts - with who and when.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property color ink: zone.st.textColor ?? "#e6e9ef"

    Text {
        id: heading
        x: 16
        y: 12
        text: w.pos && w.pos.hasCheck ? qsTr("History of %1").arg(w.pos.check.label) : qsTr("No check open")
        color: w.ink
        font.family: w.face
        font.pixelSize: 28
        font.bold: true
    }
    ListView {
        id: history
        ScrollBar.vertical: TouchScrollBar { id: historyBar }
        anchors { left: parent.left; right: parent.right; top: heading.bottom; bottom: parent.bottom; margins: 16 }
        clip: true
        spacing: 8
        model: w.pos ? w.pos.checkHistory : []
        delegate: Column {
            id: row
            required property var modelData
            width: ListView.view.width - historyBar.room
            Text {
                width: parent.width
                text: row.modelData.what
                color: w.ink
                font.family: w.face
                font.pixelSize: 22
                wrapMode: Text.WordWrap
            }
            Text {
                text: row.modelData.time + (row.modelData.who ? " · " + row.modelData.who : "")
                color: "#8a94a6"
                font.family: w.face
                font.pixelSize: 18
            }
        }
        Text {
            visible: history.count === 0
            text: qsTr("Nothing yet.")
            color: "#8a94a6"
            font.family: w.face
            font.pixelSize: 22
        }
    }
}
