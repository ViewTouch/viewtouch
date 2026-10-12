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
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property color ink: zone.st.textColor ?? "white"
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real unit: narrow ? Math.max(14, Math.min(w.width / 20, 44))
                                        : Math.max(14, Math.min(w.width / 60, w.height / 30))

    readonly property var floor: pos ? pos.floor : ({})
    readonly property var tables: zone && zone.controller ? zone.controller.tables() : []
    // The plan shown: the one the host last picked, else the floor's own.
    readonly property var plans: zone && zone.controller ? zone.controller.floorPlans() : []
    property string planId: zone && zone.controller ? (zone.controller.widgetState("hostPlan") ?? "") : ""
    readonly property string planShown: plans.find(p => p.id === planId) ? planId : (plans.length ? plans[0].id : "")
    readonly property var plan: zone && zone.controller && planShown !== "" ? zone.controller.floorPlan(planShown)
                                                                            : ({ x: 0, y: 0, w: 0, h: 0, zones: [] })
    function choosePlan(id) {
        planId = id
        if (zone && zone.controller) zone.controller.setWidgetState("hostPlan", id)
    }
    readonly property var info: pos ? pos.waitlist : ({})
    readonly property var waiting: info.waiting ?? []
    readonly property var booked: info.booked ?? []
    readonly property var parties: showBooked ? booked : waiting
    property bool showBooked: false

    // What the host has picked: a party (or walk-ins), and tables.
    property var partyId: null      // a party's id, -1 for walk-ins, null for none
    property int walkInSize: 2
    property var picked: []         // table names
    property string serverId: ""
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
    function done() { partyId = null; picked = [] }
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

    component Key: Button {
        id: key
        property color tint: "#2d3440"
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
        color: "#8a94a6"
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
            // The floor plans: Main, Dining Area, Bar, Patio...
            RowLayout {
                Layout.fillWidth: true
                visible: w.plans.length > 1
                spacing: w.unit * 0.3
                Repeater {
                    model: w.plans
                    delegate: Key {
                        required property var modelData
                        objectName: "hostPlan-" + modelData.id
                        text: qsTranslate("Page", modelData.name)
                        tint: modelData.id === w.planShown ? "#2f6fd6" : "#2d3440"
                        Layout.preferredWidth: 1   // all the same width
                        font.pixelSize: w.unit * 0.8
                        onClicked: w.choosePlan(modelData.id)
                    }
                }
            }
            // The plan as drawn, every table where it stands, colored by
            // what it is; touch tables to pick them.
            // Two fingers zoom in (up to 3 times) and drag it around.
            Flickable {
                id: floorArea
                objectName: "hostFloor"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                readonly property real fit: w.plan.w > 0 && w.plan.h > 0 ? Math.min(width / w.plan.w, height / w.plan.h) : 1
                property real zoom: 1
                property real zoomFrom: 1
                contentWidth: Math.max(width, w.plan.w * fit * zoom)
                contentHeight: Math.max(height, w.plan.h * fit * zoom)
                interactive: zoom > 1.01
                boundsBehavior: Flickable.StopAtBounds
                Connections {
                    target: w
                    function onPlanShownChanged() { floorArea.zoom = 1; floorArea.contentX = 0; floorArea.contentY = 0 }
                }
                PinchHandler {
                    target: null
                    onActiveChanged: if (active) floorArea.zoomFrom = floorArea.zoom
                    onActiveScaleChanged: {
                        const z = Math.max(1, Math.min(3, floorArea.zoomFrom * activeScale))
                        // Zoom about the middle of what's shown.
                        const cx = floorArea.contentX + floorArea.width / 2, cy = floorArea.contentY + floorArea.height / 2
                        const k = z / floorArea.zoom
                        floorArea.zoom = z
                        floorArea.contentX = Math.max(0, Math.min(floorArea.contentWidth - floorArea.width, cx * k - floorArea.width / 2))
                        floorArea.contentY = Math.max(0, Math.min(floorArea.contentHeight - floorArea.height, cy * k - floorArea.height / 2))
                    }
                }
                Item {
                    width: w.plan.w
                    height: w.plan.h
                    scale: floorArea.fit * floorArea.zoom
                    transformOrigin: Item.TopLeft
                    x: (floorArea.contentWidth - width * scale) / 2
                    y: (floorArea.contentHeight - height * scale) / 2
                    Repeater {
                        model: w.plan.zones
                        delegate: Item {
                            id: piece
                            required property var modelData
                            readonly property bool isTable: modelData.kind === "table"
                            readonly property string name: isTable ? modelData.label.trim() : ""
                            objectName: isTable ? "host-" + name : ""
                            x: modelData.x - w.plan.x
                            y: modelData.y - w.plan.y
                            width: modelData.w
                            height: modelData.h
                            rotation: modelData.rot
                            // Walls, doors, the bar...
                            Loader {
                                anchors.fill: parent
                                active: !piece.isTable
                                sourceComponent: WidgetDecor {
                                    zone: ({ props: piece.modelData.props, label: piece.modelData.label,
                                             shape: piece.modelData.shape, rotation: piece.modelData.rot, st: {} })
                                }
                            }
                            // A table: its state's color; picked, a white rim and a check.
                            readonly property var f: isTable ? (w.floor[name] ?? {}) : ({})
                            readonly property string state: f.state ?? "available"
                            readonly property bool isPicked: isTable && w.picked.indexOf(name) >= 0
                            readonly property real side: Math.min(width, height)
                            ZoneShape {
                                visible: piece.isTable
                                anchors.fill: parent
                                shape: piece.modelData.shape
                                st: piece.isPicked ? { fill: w.colorFor(piece.state), frame: "border", borderColor: "white",
                                                       frameWidth: Math.max(6, piece.side * 0.06) }
                                                   : { fill: tableTap.pressed ? Qt.lighter(w.colorFor(piece.state), 1.3)
                                                                             : w.colorFor(piece.state), frame: "raised" }
                            }
                            Column {
                                visible: piece.isTable
                                anchors.centerIn: parent
                                readonly property real turned: ((piece.rotation % 180) + 180) % 180
                                width: (turned > 45 && turned < 135 ? piece.height : piece.width) - piece.side * 0.12
                                rotation: -piece.rotation   // upright however the table is turned
                                Text {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    text: (piece.isPicked ? "✓ " : "") + piece.name
                                    color: w.ink
                                    font.family: w.face
                                    font.pixelSize: Math.min(piece.side * 0.3, 48)
                                    font.bold: true
                                    fontSizeMode: Text.HorizontalFit   // a stool's "Bar 1": smaller, not cut
                                    minimumPixelSize: 8
                                }
                                Text {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    text: piece.state === "seated"
                                          ? (piece.f.with ? qsTr("with %1").arg(piece.f.with)
                                                          : qsTr("%n guest(s)", "", piece.f.guests ?? 0) + " · " + (piece.f.minutes ?? 0) + "m")
                                        : piece.state === "dirty" ? qsTr("Dirty %1m").arg(piece.f.minutes ?? 0)
                                        : piece.state === "reserved" ? (piece.f.party ?? "") + (piece.f.time ? " " + piece.f.time : "")
                                        : qsTr("%n seat(s)", "", piece.modelData.props.seats ?? 0)
                                    color: w.ink
                                    font.family: w.face
                                    font.pixelSize: Math.min(piece.side * 0.15, 24)
                                    elide: Text.ElideRight
                                }
                                Text {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    visible: piece.state === "seated" && !piece.f.with
                                    text: piece.f.server ?? ""
                                    color: w.ink
                                    font.family: w.face
                                    font.pixelSize: Math.min(piece.side * 0.13, 20)
                                    elide: Text.ElideRight
                                }
                            }
                            TapHandler {
                                id: tableTap
                                enabled: piece.isTable
                                onTapped: { if (TouchGuard.covered(point.scenePressPosition)) return; w.toggle(piece.name) }
                            }
                        }
                    }
                }
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
                    tint: !w.showBooked ? "#2f6fd6" : "#2d3440"
                    onClicked: w.showBooked = false
                }
                Key {
                    objectName: "hostBooked"
                    text: qsTr("Reservations (%1)").arg(w.booked.length)
                    tint: w.showBooked ? "#2f6fd6" : "#2d3440"
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
                    color: w.partyId === modelData.id ? "#2f6fd6" : partyTap.pressed ? "#3a4250" : "#262b33"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: w.unit * 0.6
                        anchors.rightMargin: w.unit * 0.6
                        Text {
                            Layout.fillWidth: true
                            text: row.modelData.name + "  ·  " + row.modelData.size
                                  + (row.modelData.note ? "  ·  " + row.modelData.note : "")
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
                    tint: w.partyId === -1 ? "#2f6fd6" : "#2d3440"
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
                    model: w.servers.map(s => s.name)
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
                color: w.pickedSeats > 0 && w.pickedSeats < w.partySize ? "#f5b940" : "#8a94a6"
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: w.unit * 0.3
                columnSpacing: w.unit * 0.3
                Key {
                    objectName: "hostSeat"
                    text: qsTr("Seat")
                    tint: "#1f8a4c"
                    enabled: w.partySize > 0 && w.pickedFree
                    onClicked: {
                        const server = w.serverId
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
            }
        }
    }
}
