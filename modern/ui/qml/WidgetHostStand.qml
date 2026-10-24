import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// The host stand: every table colored by what it is (available, seated,
// needs bussing, held for a party), the waitlist and reservations beside it.
// Touch a party (or Walk-in), touch one table or several pushed together,
// then Seat or Hold. Touch tables alone to mark them clean or dirty.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string face: zone.st.font ?? Ui.font
    readonly property color ink: zone.st.textColor ?? "white"
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real unit: (narrow ? Math.max(14, Math.min(w.width / 20, 44))
                                         : Math.max(14, Math.min(w.width / 60, w.height / 30))) * zone.textScale

    readonly property var floor: pos ? pos.floor : ({})
    readonly property var tables: zone && zone.controller ? zone.controller.tables() : []

    readonly property var info: pos ? pos.waitlist : ({})
    readonly property var waiting: info.waiting ?? []
    readonly property var booked: info.booked ?? []
    readonly property var parties: showBooked ? booked : waiting
    property bool showBooked: false

    // What the host has picked: a party (or walk-ins), and tables.
    property var partyId: null      // a party's id, -1 for walk-ins, null for none
    property int walkInSize: 2
    property var picked: []         // table names
    property string serverId: ""    // chosen by the host ("": whoever is next up)
    // Who gets the next table: fewest guests now, then seated longest ago.
    readonly property var nextUp: floor._next ?? null
    readonly property string seatServer: serverId !== "" ? serverId : (nextUp ? nextUp.id : "")
    readonly property var party: {
        for (const p of waiting.concat(booked))
            if (p.id === partyId) return p
        return null
    }
    readonly property int partySize: partyId === -1 ? walkInSize : party ? party.size : 0
    readonly property int pickedSeats: {
        let n = 0
        for (const t of tables)
            if (picked.indexOf(t.name) >= 0) n += t.seats
        return n
    }
    // A party's tables, or one they hold, are theirs to pick.
    function stateOf(name) { return (floor[name] ?? {}).state ?? "available" }
    function counts() {
        const c = { available: 0, seated: 0, dirty: 0, reserved: 0 }
        for (const t of tables) c[stateOf(t.name)] = (c[stateOf(t.name)] ?? 0) + 1
        return c
    }
    readonly property var tally: { if (!floor) return ({}); return counts() }   // floor: compared, so it refreshes
    function colorFor(state) {
        return state === "seated" ? zone.statusColor("tableOpen", "#a86a12")
             : state === "dirty" ? zone.statusColor("tableDirty", "#7a3b3b")
             : state === "reserved" ? zone.statusColor("tableReserved", "#5b4aa8")
             : zone.statusColor("tableFree", "#1f7a4a")
    }
    function toggle(name) {
        const p = picked.slice()
        const i = p.indexOf(name)
        if (i >= 0) p.splice(i, 1); else p.push(name)
        picked = p
    }
    function pickParty(id) {
        partyId = partyId === id ? null : id
        // Tables they already hold come picked.
        if (party) {
            const held = []
            for (const t of tables)
                if (stateOf(t.name) === "reserved" && floor[t.name].partyId === party.id) held.push(t.name)
            if (held.length) picked = held
        }
    }
    function done() { partyId = null; picked = []; serverId = "" }
    readonly property bool pickedFree: {
        for (const n of picked) if (stateOf(n) === "seated") return false
        return picked.length > 0
    }

    // Servers on the clock to give the table to (first: whoever is logged in).
    readonly property var servers: {
        const out = []
        for (const s of (pos ? pos.staff : []))
            if (s.clockedIn || s.me) out.push(s)
        out.sort((a, b) => (b.me ? 1 : 0) - (a.me ? 1 : 0))
        return out
    }

    component Key: PosButton {
        id: key
        property color tint: Ui.button
        Layout.fillWidth: true
        Layout.preferredHeight: w.unit * 2.6
        font.family: w.face
        font.pixelSize: w.unit * 0.9
        font.bold: true
        palette.button: tint
        palette.buttonText: w.ink
        opacity: enabled ? 1 : 0.4
    }
    component Small: Text {
        color: Ui.muted
        font.family: w.face
        font.pixelSize: w.unit * 0.75
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }

    GridLayout {
        anchors.fill: parent
        columns: w.narrow ? 1 : 2
        rowSpacing: w.unit * 0.6
        columnSpacing: w.unit * 0.8

        // --- the floor ---
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: w.narrow ? -1 : w.width * 0.6
            Layout.preferredHeight: w.narrow ? w.height * 0.5 : -1
            spacing: w.unit * 0.4
            Flow {
                Layout.fillWidth: true
                spacing: w.unit * 0.8
                Repeater {
                    model: [["available", qsTr("Available")], ["seated", qsTr("Seated")],
                            ["dirty", qsTr("Dirty")], ["reserved", qsTr("Held")]]
                    delegate: Row {
                        required property var modelData
                        spacing: w.unit * 0.3
                        Rectangle {
                            width: w.unit * 0.8; height: width; radius: 4
                            anchors.verticalCenter: parent.verticalCenter
                            color: w.colorFor(modelData[0])
                        }
                        Text {
                            text: modelData[1] + " " + (w.tally[modelData[0]] ?? 0)
                            color: w.ink
                            font.family: w.face
                            font.pixelSize: w.unit * 0.8
                        }
                    }
                }
            }
            // The floor plan as drawn, the tables colored by what they are.
            FloorPlanView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                controller: w.zone ? w.zone.controller : null
                unit: w.unit
                face: w.face
                ink: w.ink
                readable: (text, fill) => w.zone.readable(text, fill)
                textScale: w.zone.textScale
                stateKey: "hostPlan"
                prefix: "host-"
                tabPrefix: "hostPlan-"
                floorName: "hostFloor"
                tint: name => w.colorFor(w.stateOf(name))
                picked: name => w.picked.indexOf(name) >= 0
                lines: (name, seats) => {
                    const f = w.floor[name] ?? {}
                    const state = f.state ?? "available"
                    return state === "seated"
                        ? [f.with ? qsTr("with %1").arg(f.with) : qsTr("%n guest(s)", "", f.guests ?? 0) + " · " + (f.minutes ?? 0) + "m"]
                          .concat(f.with ? [] : [f.server ?? ""])
                          .concat(f.with || f.freeIn === undefined ? []
                                  : [f.stage === "paying" ? qsTr("Paying: free soon")
                                     : qsTr("Free ~%1m").arg(f.freeIn)])
                        : state === "dirty" ? [qsTr("Dirty %1m").arg(f.minutes ?? 0)]
                        : state === "reserved" ? [(f.party ?? "") + (f.time ? " " + f.time : "")]
                        : [qsTr("%n seat(s)", "", seats)]
                }
                onTableTouched: name => w.toggle(name)
            }
        }

        // --- the parties, and what to do ---
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: w.unit * 0.4
            RowLayout {
                Layout.fillWidth: true
                spacing: w.unit * 0.3
                Key {
                    objectName: "hostWaiting"
                    text: qsTr("Waiting (%1)").arg(w.waiting.length)
                    tint: !w.showBooked ? Ui.accent : Ui.button
                    onClicked: w.showBooked = false
                }
                Key {
                    objectName: "hostBooked"
                    text: qsTr("Reservations (%1)").arg(w.booked.length)
                    tint: w.showBooked ? Ui.accent : Ui.button
                    onClicked: w.showBooked = true
                }
            }
            ListView {
                id: partyList
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: w.unit * 4
                clip: true
                spacing: w.unit * 0.2
                model: w.parties
                boundsBehavior: Flickable.StopAtBounds
                delegate: Rectangle {
                    id: row
                    required property var modelData
                    objectName: "hostParty-" + modelData.name
                    width: partyList.width
                    height: w.unit * 2.6
                    radius: 8
                    color: w.partyId === modelData.id ? Ui.accent : partyTap.pressed ? Ui.line : "#262b33"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: w.unit * 0.6
                        anchors.rightMargin: w.unit * 0.6
                        Text {
                            Layout.fillWidth: true
                            // A regular: ★ after their name, and their note (allergies...).
                            text: row.modelData.name + ((row.modelData.visits ?? 0) >= 2 ? " ★" : "")
                                  + "  ·  " + row.modelData.size
                                  + (row.modelData.note ? "  ·  " + row.modelData.note : "")
                                  + (row.modelData.regularNote ? "  ·  " + row.modelData.regularNote : "")
                            color: w.ink
                            font.family: w.face
                            font.pixelSize: w.unit * 0.9
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: row.modelData.reservation
                                  ? row.modelData.time + (row.modelData.table ? "  ·  " + row.modelData.table : "")
                                  : qsTr("%1 min").arg(row.modelData.waited)
                            color: row.modelData.late || row.modelData.overdue ? "#ff8a8f" : "#c8cfda"
                            font.family: w.face
                            font.pixelSize: w.unit * 0.8
                        }
                    }
                    TapHandler { id: partyTap; onTapped: { if (TouchGuard.covered(point.scenePressPosition)) return; w.pickParty(row.modelData.id) } }
                }
                Small {
                    anchors.centerIn: parent
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    visible: partyList.count === 0
                    text: w.showBooked ? qsTr("No reservations.") : qsTr("No one is waiting.")
                }
            }
            // Walk-ins: no name needed, just how many.
            RowLayout {
                Layout.fillWidth: true
                spacing: w.unit * 0.3
                Key {
                    objectName: "hostWalkIn"
                    text: qsTr("Walk-in")
                    tint: w.partyId === -1 ? Ui.accent : Ui.button
                    onClicked: w.partyId = w.partyId === -1 ? null : -1
                }
                Key {
                    text: "−"
                    Layout.preferredWidth: w.unit * 2.6
                    Layout.fillWidth: false
                    enabled: w.walkInSize > 1
                    onClicked: { w.walkInSize--; w.partyId = -1 }
                }
                Text {
                    text: qsTr("%n guest(s)", "", w.walkInSize)
                    color: w.ink
                    font.family: w.face
                    font.pixelSize: w.unit * 0.9
                    Layout.preferredWidth: w.unit * 5
                    horizontalAlignment: Text.AlignHCenter
                }
                Key {
                    text: "+"
                    Layout.preferredWidth: w.unit * 2.6
                    Layout.fillWidth: false
                    onClicked: { w.walkInSize++; w.partyId = -1 }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: w.servers.length > 1
                Text {
                    text: qsTr("Server")
                    color: w.ink
                    font.family: w.face
                    font.pixelSize: w.unit * 0.8
                }
                ComboBox {
                    id: serverBox
                    objectName: "hostServer"
                    Layout.fillWidth: true
                    Layout.preferredHeight: w.unit * 2.2
                    font.pixelSize: w.unit * 0.85
                    model: w.servers.map(s => w.nextUp && s.id === w.nextUp.id ? qsTr("%1 (next up)").arg(s.name) : s.name)
                    currentIndex: Math.max(0, w.servers.findIndex(s => s.id === w.seatServer))
                    onActivated: i => w.serverId = w.servers[i].id
                }
            }
            // What the picks add up to, and what can be done with them.
            Small {
                objectName: "hostHint"
                text: w.partySize > 0 && w.picked.length > 0
                      ? qsTr("%1 for %n guest(s)", "", w.partySize).arg(w.picked.join(" + "))
                        + "  ·  " + qsTr("%n seat(s)", "", w.pickedSeats)
                        + (w.pickedSeats > 0 && w.pickedSeats < w.partySize ? "  ·  " + qsTr("not enough seats") : "")
                      : w.partySize > 0 ? qsTr("Now touch a table, or several to push together.")
                      : w.picked.length > 0 ? qsTr("%1 picked. Touch a party to seat or hold, or mark them.").arg(w.picked.join(", "))
                      : qsTr("Touch a party or Walk-in, then their table.")
                color: w.pickedSeats > 0 && w.pickedSeats < w.partySize ? "#f5b940" : Ui.muted
            }
            // Rush Radar: the kitchen about to back up. Seating more now makes
            // it worse; a few minutes more at the bar or the door helps.
            Rectangle {
                objectName: "hostRush"
                visible: hostRush.headline !== ""
                Layout.fillWidth: true
                Layout.preferredHeight: hostRushCol.implicitHeight + w.unit * 0.6
                radius: 8
                color: "#3a2e10"
                border.color: "#f5b940"
                ColumnLayout {
                    id: hostRushCol
                    anchors.fill: parent
                    anchors.margins: w.unit * 0.3
                    spacing: 2
                    RushRadarView {
                        id: hostRush
                        Layout.fillWidth: true
                        compact: true
                        active: w.visible
                        pos: w.pos
                        unit: w.unit * 0.85
                        face: w.face
                    }
                    Small {
                        text: qsTr("If you can, seat the next party in a few minutes: the kitchen catches up first.")
                        color: "#f5d48a"
                    }
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: w.unit * 0.3
                columnSpacing: w.unit * 0.3
                Key {
                    objectName: "hostSeat"
                    text: qsTr("Seat")
                    tint: Ui.go
                    enabled: w.partySize > 0 && w.pickedFree
                    onClicked: {
                        const server = w.seatServer
                        if (w.partyId === -1) w.pos.seatWalkIn(w.walkInSize, w.picked, server)
                        else w.pos.seatPartyAt(w.partyId, w.picked, server)
                        w.done()
                    }
                }
                Key {
                    objectName: "hostHold"
                    text: qsTr("Hold for Them")
                    tint: "#5b4aa8"
                    enabled: w.party !== null && w.pickedFree
                    onClicked: { w.pos.reserveTables(w.partyId, w.picked); w.done() }
                }
                Key {
                    objectName: "hostClean"
                    text: qsTr("Mark Available")
                    enabled: w.pickedFree
                    onClicked: { for (const t of w.picked) w.pos.setTableState(t, "clean"); w.picked = [] }
                }
                Key {
                    objectName: "hostDirty"
                    text: qsTr("Mark Dirty")
                    enabled: w.pickedFree
                    onClicked: { for (const t of w.picked) w.pos.setTableState(t, "dirty"); w.picked = [] }
                }
                // The server is busy: the host takes the table's payment (Store Settings).
                Key {
                    objectName: "hostPay"
                    Layout.columnSpan: 2
                    visible: w.pos !== null && w.pos.can("check.settle")
                    text: qsTr("Pay This Table")
                    tint: "#2a62b8"
                    enabled: w.partySize === 0 && w.picked.length === 1 && w.stateOf(w.picked[0]) === "seated"
                    onClicked: { const t = w.picked[0]; w.done(); w.zone.controller.payTable(t) }
                }
            }
        }
    }
}
