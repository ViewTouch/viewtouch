import QtQuick
import QtQuick.Layouts

// Tables to clean, the longest waiting first: touch Start Cleaning, then
// Ready when it's set. A busser's screen (and the host stand's Tables to
// Clean). Orange past the store's minutes (Store Settings).
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string face: zone.st.font ?? Ui.font
    readonly property color ink: zone.st.textColor ?? "white"
    // On a phone, text at least 15 on the screen (ZoneItem.minText).
    readonly property real unit: Math.max(baseUnit, zone ? zone.minText(15) : 0)
    readonly property real baseUnit: Math.max(16, Math.min(w.width / 30, 40)) * (zone ? zone.textScale : 1)

    readonly property var floor: pos ? pos.floor : ({})
    // [{name, state, minutes, late, by}], the longest waiting first; being cleaned after.
    readonly property var tables: {
        const out = []
        for (const name in floor) {
            if (name.startsWith("_"))
                continue
            const f = floor[name]
            if (f.state === "dirty" || f.state === "cleaning")
                out.push({ name: name, state: f.state, minutes: f.minutes ?? 0, late: f.late ?? false, by: f.by ?? "" })
        }
        out.sort((a, b) => (a.state === "cleaning") - (b.state === "cleaning") || b.minutes - a.minutes)
        return out
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: w.unit * 0.4

        ListView {
            id: list
            objectName: "busList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: w.unit * 0.3
            boundsBehavior: Flickable.StopAtBounds
            model: w.tables
            delegate: Rectangle {
                id: row
                required property var modelData
                objectName: "bus-" + modelData.name
                width: list.width
                height: Math.max(w.unit * 3, w.zone ? w.zone.touch(64) : 0)
                radius: 10
                color: modelData.state === "cleaning" ? w.zone.statusColor("tableCleaning", "#3b6f7a")
                     : modelData.late ? "#8a4b12" : w.zone.statusColor("tableDirty", "#7a3b3b")
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: w.unit * 0.8
                    anchors.rightMargin: w.unit * 0.4
                    spacing: w.unit * 0.6
                    Text {
                        textFormat: Text.PlainText
                        text: row.modelData.name
                        color: w.ink
                        font.family: w.face
                        font.pixelSize: w.unit * 1.3
                        font.bold: true
                        Layout.preferredWidth: w.unit * 5
                        elide: Text.ElideRight
                    }
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: row.modelData.state === "cleaning"
                              ? qsTr("Cleaning · %1 · %2m").arg(row.modelData.by).arg(row.modelData.minutes)
                              : (row.modelData.late ? "⚠ " : "") + qsTr("Dirty %1m").arg(row.modelData.minutes)
                        color: w.ink
                        font.family: w.face
                        font.pixelSize: w.unit * 0.9
                        elide: Text.ElideRight
                    }
                    WidgetKey {
                        objectName: "bus-" + row.modelData.name + "-key"
                        Layout.preferredWidth: w.unit * 8
                        Layout.fillHeight: true
                        Layout.topMargin: w.unit * 0.3
                        Layout.bottomMargin: w.unit * 0.3
                        text: row.modelData.state === "cleaning" ? qsTr("Ready ✓") : qsTr("Start Cleaning")
                        baseColor: row.modelData.state === "cleaning" ? Ui.go : Ui.key
                        fontScale: 0.4
                        onClicked: w.pos.setTableState(row.modelData.name, row.modelData.state === "cleaning" ? "clean" : "cleaning")
                    }
                }
            }
            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("All clean ✓")
                color: Ui.muted
                font.family: w.face
                font.pixelSize: w.unit * 1.4
            }
        }
    }
}
