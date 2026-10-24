import QtQuick
import QtQuick.Layouts

// One of the day's checklists (props.list: "opening" or "closing"): touch a
// task when it's done (again to undo); each says who did it and when.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string list: zone && zone.props && zone.props.list ? zone.props.list : "closing"
    readonly property var tasks: pos ? (pos.checklists[list] ?? []) : []
    readonly property int done: pos ? (pos.checklists[list + "Done"] ?? 0) : 0
    readonly property string face: zone.st.font ?? Ui.font
    readonly property color ink: zone.st.textColor ?? "white"
    readonly property real unit: Math.max(14, Math.min(w.width / 26, w.height / 20))

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: Ui.sunken
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: w.unit
        spacing: w.unit * 0.4
        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: w.list === "opening" ? qsTr("Opening") : qsTr("Closing")
                color: w.ink
                font.family: w.face
                font.pixelSize: w.unit * 1.4
                font.bold: true
            }
            Text {
                text: qsTr("%1 of %2 done").arg(w.done).arg(w.tasks.length)
                color: w.done === w.tasks.length && w.tasks.length > 0 ? "#5fd08a" : Ui.muted
                font.family: w.face
                font.pixelSize: w.unit * 0.9
            }
        }
        Text {
            visible: w.tasks.length === 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("No tasks yet: Manager → Settings, Opening / Closing checklist.")
            color: Ui.muted
            font.family: w.face
            font.pixelSize: w.unit * 0.9
        }
        Repeater {
            model: w.tasks
            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                objectName: "task-" + w.list + "-" + index
                Layout.fillWidth: true
                Layout.preferredHeight: w.unit * 2.4
                radius: 8
                color: tap.pressed ? "#2b3340" : modelData.done ? "#1f3a2b" : "#262c36"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: w.unit * 0.5
                    anchors.rightMargin: w.unit * 0.5
                    spacing: w.unit * 0.5
                    Rectangle {
                        Layout.preferredWidth: w.unit * 1.3
                        Layout.preferredHeight: w.unit * 1.3
                        radius: 6
                        color: row.modelData.done ? Ui.go : "transparent"
                        border.color: row.modelData.done ? Ui.go : Ui.muted
                        border.width: 2
                        Text {
                            anchors.centerIn: parent
                            visible: row.modelData.done
                            text: "✓"
                            color: "white"
                            font.pixelSize: w.unit
                            font.bold: true
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: row.modelData.task
                        color: row.modelData.done ? Ui.textSoft : w.ink
                        font.family: w.face
                        font.pixelSize: w.unit
                        font.strikeout: row.modelData.done
                        elide: Text.ElideRight
                    }
                    Text {
                        visible: row.modelData.done
                        text: row.modelData.by + "  ·  " + row.modelData.at
                        color: Ui.muted
                        font.family: w.face
                        font.pixelSize: w.unit * 0.75
                    }
                }
                TapHandler { id: tap; onTapped: { if (TouchGuard.covered(point.scenePressPosition)) return; w.pos.tickChecklist(w.list, row.index) } }
            }
        }
        Item { Layout.fillHeight: true }
    }
}
