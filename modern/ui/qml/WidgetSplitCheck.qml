import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Split check: touch an item on the left, then the check to move it to on
// the right (another check at this table, or a new one): one at a time, or
// all of a line. An item can be shared (a bottle of wine for three: even
// pieces, one to each check), or the whole check split evenly.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property color ink: zone.st.textColor ?? "white"
    // Upright (a phone): the items, then the checks below, at a phone's size.
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real unit: narrow ? Math.max(14, Math.min(44, w.width * 0.045))
                                        : Math.max(14, Math.min(28, w.width * 0.022))
    readonly property var targets: {
        // Compared, not just read: an unused read is compiled away (no refresh).
        if (!pos || !pos.openChecks || !pos.lines) return []
        return pos.splitTargets()
    }
    readonly property var chosen: pos && pos.lines ? (pos.lines.find(l => l.selected) ?? null) : null
    // A line of 2 or more: move one of them, or all.
    property bool moveAll: false
    onChosenChanged: if (!chosen || chosen.quantity < 2) moveAll = false

    GridLayout {
        anchors.fill: parent
        anchors.margins: w.unit * 0.6
        columns: w.narrow ? 1 : 2
        rowSpacing: w.unit
        columnSpacing: w.unit

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Text {
                text: w.pos && w.pos.hasCheck ? qsTr("%1 · check #%2").arg(w.pos.check.label).arg(w.pos.check.id)
                                               : qsTr("No check open")
                color: w.ink
                font.family: w.face
                font.pixelSize: w.unit * 1.2
                font.bold: true
            }
            Text {
                text: qsTr("1. Touch an item")
                color: "#8a94a6"
                font.family: w.face
                font.pixelSize: w.unit * 0.8
            }
            ListView {
                id: lines1
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.vertical: TouchScrollBar { id: linesBar }
                spacing: 4
                model: w.pos ? w.pos.lines : []
                delegate: Rectangle {
                    id: line
                    required property var modelData
                    width: ListView.view.width - linesBar.room
                    height: w.unit * 2.2
                    radius: 8
                    color: modelData.selected ? "#2f6fd6" : "#2d3440"
                    TapHandler { onTapped: { if (TouchGuard.covered(point.scenePressPosition)) return; w.pos.selectedLine = line.modelData.id } }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: w.unit * 0.5
                        anchors.rightMargin: w.unit * 0.5
                        Text {
                            Layout.fillWidth: true
                            text: (line.modelData.quantity > 1 ? line.modelData.quantity + " × " : "") + line.modelData.name
                                  + (line.modelData.modifiers.length ? "  (" + line.modelData.modifiers.map(m => m.name).join(", ") + ")" : "")
                            color: w.ink
                            font.family: w.face
                            font.pixelSize: w.unit
                            elide: Text.ElideRight
                        }
                        Text {
                            text: line.modelData.price
                            color: w.ink
                            font.family: w.face
                            font.pixelSize: w.unit
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: false    // nested layouts fill by default
            Layout.preferredWidth: parent.width * 0.4
            Layout.fillHeight: true
            Text {
                text: qsTr("2. Move it to")
                color: "#8a94a6"
                font.family: w.face
                font.pixelSize: w.unit * 0.8
            }
            RowLayout {
                objectName: "splitHowMany"
                visible: !!w.chosen && w.chosen.quantity > 1
                Layout.fillWidth: true
                spacing: w.unit * 0.4
                WidgetKey {
                    objectName: "splitOne"
                    Layout.fillWidth: true
                    Layout.preferredHeight: w.unit * 2.4
                    text: qsTr("One")
                    accent: !w.moveAll
                    fontScale: 0.35
                    onClicked: w.moveAll = false
                }
                WidgetKey {
                    objectName: "splitAll"
                    Layout.fillWidth: true
                    Layout.preferredHeight: w.unit * 2.4
                    text: w.chosen ? qsTr("All %1").arg(w.chosen.quantity) : ""
                    accent: w.moveAll
                    fontScale: 0.35
                    onClicked: w.moveAll = true
                }
            }
            Repeater {
                model: w.targets
                delegate: WidgetKey {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: w.unit * 3
                    text: modelData.id === 0 ? "+ " + modelData.label : modelData.label + "   " + modelData.total
                    accent: modelData.id === 0
                    fontScale: 0.3
                    onClicked: w.pos.splitLine(modelData.id, w.moveAll)
                }
            }
            Text {
                Layout.topMargin: w.unit * 0.5
                text: qsTr("Or")
                color: "#8a94a6"
                font.family: w.face
                font.pixelSize: w.unit * 0.8
            }
            WidgetKey {
                objectName: w.chosen && w.chosen.shared ? "splitPutBack" : "splitShare"
                Layout.fillWidth: true
                Layout.preferredHeight: w.unit * 2.6
                enabled: !!w.chosen && !w.chosen.comment && !w.chosen.voided
                opacity: enabled ? 1 : 0.45
                text: w.chosen && w.chosen.shared ? qsTr("Put Back Together") : qsTr("Share This Item…")
                fontScale: 0.3
                onClicked: {
                    if (w.chosen.shared)
                        w.pos.unshareLine()
                    else
                        ways.ask(false)
                }
            }
            WidgetKey {
                objectName: "splitEvenly"
                Layout.fillWidth: true
                Layout.preferredHeight: w.unit * 2.6
                enabled: !!w.pos && !!w.pos.lines && w.pos.lines.some(l => !l.comment && !l.voided)
                opacity: enabled ? 1 : 0.45
                text: qsTr("Split Evenly…")
                fontScale: 0.3
                onClicked: ways.ask(true)
            }
            Item { Layout.fillHeight: true }
        }
    }

    // How many ways: to share the item, or split the check.
    Popup {
        id: ways
        objectName: "splitWays"
        property bool evenly: false
        function ask(e) { evenly = e; open() }
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(parent ? parent.width - 32 : 560, 560)
        modal: true
        padding: 18
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: ways.evenly ? qsTr("Split this check evenly: how many checks?")
                                  : (w.chosen ? qsTr("Share %1: how many ways?").arg(w.chosen.name) : "")
                font.pixelSize: 20
                font.bold: true
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: ways.evenly ? qsTr("Each check gets the same: whole items while they go around, the rest in even pieces.")
                                  : qsTr("It's cut in even pieces on this check; then move each piece to its check.")
                opacity: 0.75
                wrapMode: Text.Wrap
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 8
                rowSpacing: 8
                Repeater {
                    model: 8
                    delegate: Button {
                        required property int index
                        objectName: "splitWays-" + (index + 2)
                        Layout.fillWidth: true
                        implicitHeight: 64
                        font.pixelSize: 24
                        text: index + 2
                        onClicked: {
                            const n = index + 2
                            const evenly = ways.evenly
                            ways.close()
                            if (evenly)
                                w.pos.splitEvenly(n)
                            else
                                w.pos.shareLine(n)
                        }
                    }
                }
            }
            Button {
                Layout.fillWidth: true
                implicitHeight: 52
                font.pixelSize: 16
                text: qsTr("Cancel")
                onClicked: ways.close()
            }
        }
    }
}
