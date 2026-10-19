import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// End of Day: shows what is still in the way (open checks, uncounted
// drawer), then closes the business day and starts the next one.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var day: pos ? pos.day : ({})
    readonly property var backup: day.backup ?? ({})
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property color ink: zone.st.textColor ?? "white"
    readonly property real unit: Math.max(14, Math.min(30, w.width * 0.04))
    property bool armed: false

    Timer { id: disarm; interval: 3000; onTriggered: w.armed = false }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: w.unit
        spacing: w.unit * 0.5

        Text {
            text: qsTr("Business day #%1").arg(w.day.id ?? "")
            color: w.ink
            font.family: w.face
            font.pixelSize: w.unit * 1.5
            font.bold: true
        }
        Text {
            text: qsTr("Opened %1").arg(w.day.opened ?? "")
            color: "#8a94a6"
            font.family: w.face
            font.pixelSize: w.unit * 0.85
        }
        Text {
            text: qsTr("%1 checks closed · net sales %2").arg(w.day.closedChecks ?? 0).arg(w.day.netSales ?? "")
            color: w.ink
            font.family: w.face
            font.pixelSize: w.unit
        }

        Repeater {
            model: w.day.blockers ?? []
            delegate: Text {
                required property string modelData
                Layout.fillWidth: true
                text: "✗  " + modelData
                color: "#ff9a9e"
                font.family: w.face
                font.pixelSize: w.unit
                wrapMode: Text.WordWrap
            }
        }
        // Still on the clock: clock them out now (or fix it later in Time Punches).
        Text {
            visible: (w.day.clockedIn ?? []).length > 0
            Layout.topMargin: w.unit * 0.4
            text: qsTr("Still clocked in")
            color: "#f5b940"
            font.family: w.face
            font.pixelSize: w.unit
            font.bold: true
        }
        Repeater {
            model: w.day.clockedIn ?? []
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: w.unit * 0.5
                Text {
                    Layout.fillWidth: true
                    text: modelData.name + "  ·  " + qsTr("since %1").arg(modelData.since)
                          + (modelData.onBreak ? "  ·  " + qsTr("on break") : "")
                          + (modelData.overTwelve ? "  ·  " + qsTr("forgot to clock out?") : "")
                    color: modelData.overTwelve ? "#ff9a9e" : w.ink
                    font.family: w.face
                    font.pixelSize: w.unit * 0.85
                    elide: Text.ElideRight
                }
                WidgetKey {
                    objectName: "eodClockOut-" + modelData.id
                    Layout.preferredWidth: w.unit * 7
                    Layout.preferredHeight: Math.max(w.unit * 1.8, w.zone ? w.zone.touch(46) : 0)
                    text: qsTr("Clock Out Now")
                    fontScale: 0.32
                    onClicked: w.pos.clockOutPunch(modelData.id)
                }
            }
        }
        Text {
            visible: (w.day.clockedIn ?? []).some(p => p.overTwelve)
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Forgot hours ago? Clock them out, then fix the time in Schedule → Time Punches.")
            color: "#8a94a6"
            font.family: w.face
            font.pixelSize: w.unit * 0.7
        }
        // The closing checklist: what's left (not a blocker).
        RowLayout {
            visible: (w.day.closingLeft ?? 0) > 0
            Layout.fillWidth: true
            spacing: w.unit * 0.5
            Text {
                Layout.fillWidth: true
                text: qsTr("%n closing task(s) not done yet", "", w.day.closingLeft ?? 0)
                color: "#f5b940"
                font.family: w.face
                font.pixelSize: w.unit * 0.9
                wrapMode: Text.WordWrap
            }
            WidgetKey {
                objectName: "eodChecklist"
                Layout.preferredWidth: w.unit * 9
                Layout.preferredHeight: Math.max(w.unit * 2.6, w.zone ? w.zone.touch(52) : 0)
                text: qsTr("Checklists…")
                fontScale: 0.36
                onClicked: w.zone.controller.jumpTo("checklists")
            }
        }
        Text {
            visible: w.day.ready ?? false
            text: "✓  " + qsTr("Everything is settled and counted.")
            color: "#7ee2a8"
            font.family: w.face
            font.pixelSize: w.unit
        }

        Item { Layout.fillHeight: true }

        // The last backup, and its second copy when one is set up; under it,
        // a note for the next shift and backing up now, full width.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: w.unit * 0.5
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                readonly property var b: w.backup
                text: !b.at ? qsTr("No backup yet this session.")
                     : b.ok ? qsTr("Backed up at %1.").arg(b.at) + (b.copy ? "  " + b.copy : "")
                            : qsTr("Backup failed at %1: %2").arg(b.at).arg(b.error)
                color: !b.at ? "#8a94a6" : (b.ok && b.copyOk !== false) ? "#7ee2a8" : "#ff9a9e"
                font.family: w.face
                font.pixelSize: w.unit * 0.75
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: w.unit * 0.5
                WidgetKey {
                    objectName: "handoffNote"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(w.unit * 2.6, w.zone ? w.zone.touch(52) : 0)
                    visible: w.zone.keyShown("handoff")
                    text: w.zone.keyText("handoff", qsTr("Note for the Next Shift…"))
                    fontScale: 0.36
                    onClicked: handoffWrite.open()
                }
                WidgetKey {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(w.unit * 2.6, w.zone ? w.zone.touch(52) : 0)
                    visible: w.zone.keyShown("backup")
                    text: w.zone.keyText("backup", qsTr("Back Up Now"))
                    fontScale: 0.36
                    onClicked: w.pos.backupNow()
                }
            }
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Closing the day saves its reports (see Reports) and starts a new day. Checks cannot be reopened afterwards.")
            color: "#8a94a6"
            font.family: w.face
            font.pixelSize: w.unit * 0.75
        }
        WidgetKey {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(w.unit * 3.2, w.zone ? w.zone.touch(46) : 0)
            text: w.armed ? qsTr("Tap again to close the day") : qsTr("Close the Day")
            baseColor: (w.day.ready ?? false) ? (w.armed ? "#b83232" : "#1f5f3a") : "#3a3f48"
            // Not ready: dimmed (a touch still says what's missing).
            textColor: (w.day.ready ?? false) ? "white" : "#9aa3b5"
            fontScale: 0.3
            onClicked: {
                if (!(w.day.ready ?? false)) {
                    w.pos.endOfDay()   // explains what is missing
                    return
                }
                if (!w.armed) {
                    w.armed = true
                    disarm.restart()
                    return
                }
                w.armed = false
                w.pos.endOfDay()
            }
        }
    }

    // A note for the next shift's manager: they see it when they log in.
    Popup {
        id: handoffWrite
        objectName: "handoffWrite"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(parent ? parent.width - 24 : 600, 640)
        modal: true
        padding: 20
        onOpened: { noteText.text = ""; noteText.forceActiveFocus() }
        background: Rectangle { color: "#232933"; radius: 14; border.color: "#2f6fd6"; border.width: 2 }
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Text {
                Layout.fillWidth: true
                text: qsTr("A note for the next shift's manager")
                color: "white"
                font.pixelSize: 22
                font.bold: true
                wrapMode: Text.WordWrap
            }
            TextArea {
                id: noteText
                objectName: "handoffText"
                Layout.fillWidth: true
                Layout.preferredHeight: 160
                wrapMode: TextEdit.Wrap
                font.pixelSize: 20
                placeholderText: qsTr("Walk-in door sticks · 86 brisket tomorrow · Sam owes $20")
            }
            RowLayout {
                spacing: 10
                WidgetKey {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    text: qsTr("Cancel")
                    fontScale: 0.34
                    takesTouch: true
                    onClicked: handoffWrite.close()
                }
                WidgetKey {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    text: qsTr("Leave Note")
                    accent: true
                    fontScale: 0.34
                    takesTouch: true
                    onClicked: { w.pos.leaveHandoffNote(noteText.text); handoffWrite.close() }
                }
            }
        }
    }
}
