import QtQuick

// The floor plans in one panel, for phones and anywhere a whole page of
// tables doesn't fit: a tab for each plan, the tables where they stand,
// colored by whose check is on them (yours, someone else's, the one you're
// on); two fingers zoom. Touching a table opens it as on the floor plan.
// props.plan: the plan to start on (empty: the one last picked here, else
// the floor's own); props.tabs false hides the tabs; props.action "move":
// touching a table moves the check you are on there, then goes back.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var props: zone && zone.props ? zone.props : ({})

    // Again when the checks change (compared, not just read: an unused read is compiled away).
    function status(name) { return !w.pos || w.pos.checksRevision < 0 ? ({ open: false }) : w.pos.tableStatus(name) }
    function minutesText(m) { return m < 60 ? m + "m" : Math.floor(m / 60) + "h " + (m % 60) + "m" }
    property real now: Date.now()
    Timer { interval: 30000; running: true; repeat: true; onTriggered: w.now = Date.now() }

    FloorPlanView {
        anchors.fill: parent
        controller: w.zone ? w.zone.controller : null
        unit: Math.max(14, Math.min(w.width / 22, 44)) * w.zone.textScale
        face: w.zone.st.font ?? Ui.font
        ink: w.zone.st.textColor ?? "white"
        readable: (text, fill) => w.zone.readable(text, fill)
        textScale: w.zone.textScale
        tabs: w.props.tabs !== false
        startPlan: w.props.plan ?? ""
        prefix: "floor-"
        tabPrefix: "floorPlan-"
        floorName: "floorPlanArea"
        // A phone: Plan / List (big tiles: yours first, the longest since a Send on top).
        listToggle: w.props.list === true || (w.zone && w.zone.controller && w.zone.controller.formFactor === "phone")
        // The style may start it as the list (Modern: "tablesList").
        listFirst: !!w.zone && w.zone.st.tablesList === true
        sortKey: name => {
            const s = w.status(name)
            if (!s.open) return 3e6
            const waited = s.lastSentAt ? Math.floor((w.now - s.lastSentAt) / 60000)
                                        : s.since ? Math.floor((w.now - s.since) / 60000) : 0
            return (s.mine ? 0 : 1e6) + (s.unsent > 0 ? 0 : 5e5) - Math.min(waited, 4e5)
        }
        tileLines: (name, seats) => {
            const s = w.status(name)
            if (!s.open)
                return lines(name, seats)
            const ago = s.lastSentAt ? Math.max(0, Math.floor((w.now - s.lastSentAt) / 60000)) : -1
            return [((s.checks ?? 1) > 1 ? qsTr("%1 checks").arg(s.checks) : (s.server ?? "")) + "  ·  " + (s.total ?? ""),
                    s.unsent > 0 ? "⚠ " + qsTr("%n not sent", "", s.unsent)
                                 : ago === 0 ? qsTr("Sent just now")
                                 : ago > 0 ? qsTr("Sent %1 ago").arg(w.minutesText(ago)) : qsTr("Nothing sent yet")]
        }
        tint: name => {
            const s = w.status(name)
            const floor = s.floor ?? ""
            return floor === "dirty" ? w.zone.statusColor("tableDirty", "#7a3b3b")
                 : floor === "reserved" ? w.zone.statusColor("tableReserved", "#5b4aa8")
                 : floor === "joined" ? w.zone.statusColor("tableOpen", "#a86a12")
                 : !s.open ? (w.zone.st.fill ?? Ui.button)
                 : s.current ? w.zone.statusColor("tableCurrent", Ui.accent)
                 : s.mine ? w.zone.statusColor("tableMine", Ui.go)
                 : w.zone.statusColor("tableOpen", "#a86a12")
        }
        lines: (name, seats) => {
            const s = w.status(name)
            const floor = s.floor ?? ""
            if (s.open) {
                const minutes = s.since ? Math.max(0, Math.floor((w.now - s.since) / 60000)) : 0
                return [(s.checks ?? 1) > 1 ? qsTr("%1 checks").arg(s.checks) : (s.server ?? ""),
                        (s.total ?? "") + "  ·  " + w.minutesText(minutes)]
            }
            if (floor === "dirty") return [qsTr("Needs bussing")]
            if (floor === "joined") return [qsTr("With %1").arg(s.with ?? "")]
            if (floor === "reserved") return [qsTr("Held: %1").arg(s.party ?? "") + (s.time ? "  ·  " + s.time : "")]
            return seats > 0 ? [seats === 1 ? qsTr("1 seat") : qsTr("%1 seats").arg(seats)] : []
        }
        onTableTouched: name => {
            if ((w.props.action ?? "") === "move") {
                w.pos.moveCheck(name)
                w.zone.controller.goBack()
            } else {
                w.zone.controller.selectTable(name)
            }
        }
    }
}
