import QtQuick

// One table, placed like any button. The zone's label is the table's name;
// props.seats shows while it is free. Its own style colors a free table;
// an open one shows whose it is: yours, someone else's, or the check you
// are on. Touching it opens the table as on the floor plan.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string name: zone ? zone.label.trim() : ""
    // Again when the checks change (compared, not just read: an unused read is compiled away).
    readonly property var status: !w.pos || !w.name || !w.pos.openChecks ? ({ open: false }) : w.pos.tableStatus(w.name)
    readonly property string floorState: status.floor ?? ""
    readonly property color tint: floorState === "dirty" ? zone.statusColor("tableDirty", "#7a3b3b")
                                  : floorState === "reserved" ? zone.statusColor("tableReserved", "#5b4aa8")
                                  : floorState === "joined" ? zone.statusColor("tableOpen", "#a86a12")
                                  : !status.open ? (zone.st.fill ?? "#2d3440")
                                  : status.current ? zone.statusColor("tableCurrent", "#2f6fd6")
                                  : status.mine ? zone.statusColor("tableMine", "#1f8a4c")
                                  : zone.statusColor("tableOpen", "#a86a12")
    readonly property color ink: zone.readable(zone.st.textColor ?? "white", tint)
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property int seats: zone && zone.props ? (zone.props.seats ?? 0) : 0
    // How long the table has been seated, kept current.
    property real now: Date.now()
    Timer { interval: 30000; running: w.status.open ?? false; repeat: true; onTriggered: w.now = Date.now() }
    readonly property int seatedMinutes: (w.status.open ?? false) && w.status.since
                                         ? Math.max(0, Math.floor((w.now - w.status.since) / 60000)) : 0
    readonly property bool seatedLong: (w.status.open ?? false) && seatedMinutes >= (w.status.longAfter ?? 90)
    function minutesText(m) { return m < 60 ? m + "m" : Math.floor(m / 60) + "h " + (m % 60) + "m" }

    // props.chairs: a chair for each seat around the table, the top inside them.
    readonly property bool chairs: zone && zone.props ? zone.props.chairs === true && seats > 0 : false
    readonly property real chairDepth: chairs ? Math.min(width, height) * 0.17 : 0
    readonly property bool round: ["circle", "octagon", "hexagon", "diamond"].includes(zone ? zone.shape : "")
    // Where each chair goes: {x, y, a} (its middle, and which way it faces).
    readonly property var chairSpots: {
        if (!chairs) return []
        const out = [], n = seats, c = chairDepth, cx = width / 2, cy = height / 2
        if (round || n === 1) {
            for (let i = 0; i < n; ++i) {
                const a = -Math.PI / 2 + 2 * Math.PI * i / n + (n === 1 ? Math.PI : 0)
                out.push({ x: cx + Math.cos(a) * (width / 2 - c / 2), y: cy + Math.sin(a) * (height / 2 - c / 2),
                           a: a * 180 / Math.PI + 90 })
            }
            return out
        }
        // Long sides, the rest at the ends when a side is full.
        const along = width >= height
        const side = along ? width - 2 * c : height - 2 * c
        const perSide = Math.max(1, Math.floor(side / (c * 1.6)))
        const first = Math.min(perSide, Math.ceil(n / 2)), second = Math.min(perSide, n - first)
        const ends = n - first - second
        const place = (count, t) => {
            for (let i = 0; i < count; ++i) {
                const f = c + side * (i + 0.5) / count
                if (along) out.push(t === 0 ? { x: f, y: c / 2, a: 0 } : { x: f, y: height - c / 2, a: 180 })
                else out.push(t === 0 ? { x: width - c / 2, y: f, a: 90 } : { x: c / 2, y: f, a: 270 })
            }
        }
        place(first, 0)
        place(second, 1)
        for (let i = 0; i < Math.min(ends, 2); ++i)
            out.push(along ? { x: i === 0 ? c / 2 : width - c / 2, y: cy, a: i === 0 ? 270 : 90 }
                           : { x: cx, y: i === 0 ? c / 2 : height - c / 2, a: i === 0 ? 0 : 180 })
        return out
    }
    Repeater {
        model: w.chairSpots
        Rectangle {
            required property var modelData
            objectName: "chair"
            width: w.chairDepth * 0.95
            height: w.chairDepth * 0.75
            x: modelData.x - width / 2
            y: modelData.y - height / 2
            rotation: modelData.a
            radius: height * 0.3
            color: w.zone.statusColor("chair", "#8b7d6b")
            border.color: Qt.darker(color, 1.7)
            border.width: 2
            // Its back, away from the table.
            Rectangle {
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: parent.height * 0.3
                radius: height / 2
                color: Qt.darker(parent.color, 1.35)
            }
        }
    }

    ZoneShape {
        anchors.fill: parent
        anchors.margins: w.chairDepth
        shape: w.zone.shape
        // Seated past the store's mark: a red frame.
        st: Object.assign({}, w.zone.st, { fill: tap.pressed ? Qt.lighter(w.tint, 1.3) : w.tint },
                          w.seatedLong ? { frame: "border", borderColor: w.zone.statusColor("tableLong", "#ff4d4d"), frameWidth: 6 } : {})
    }

    // Upright however the table is turned; as wide as it looks.
    Column {
        anchors.centerIn: parent
        readonly property real turned: ((w.zone.rotation % 180) + 180) % 180
        readonly property real across: turned > 45 && turned < 135 ? parent.height : parent.width
        width: across - 16 - 2 * w.chairDepth
        rotation: -w.zone.rotation
        spacing: 2
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: w.name !== "" ? w.name : qsTr("(no name)")
            color: w.ink
            font.family: w.face
            font.pixelSize: Math.min((Math.min(w.width, w.height) - 2 * w.chairDepth) * 0.28, w.zone.st.fontSize ?? 40)
            font.bold: w.zone.st.bold ?? true
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            visible: w.status.open ?? false
            text: ((w.status.checks ?? 1) > 1 ? qsTr("%1 checks").arg(w.status.checks) : (w.status.server ?? ""))
                  + "  ·  " + (w.status.total ?? "") + "  ·  " + w.minutesText(w.seatedMinutes)
            color: w.ink
            font.family: w.face
            font.pixelSize: Math.min((Math.min(w.width, w.height) - 2 * w.chairDepth) * 0.13, 20)
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            visible: w.floorState !== ""
            text: w.floorState === "dirty" ? qsTr("Needs bussing")
                : w.floorState === "joined" ? qsTr("With %1").arg(w.status.with ?? "")
                : qsTr("Held: %1").arg(w.status.party ?? "") + (w.status.time ? "  ·  " + w.status.time : "")
            color: w.ink
            font.family: w.face
            font.pixelSize: Math.min((Math.min(w.width, w.height) - 2 * w.chairDepth) * 0.13, 20)
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            visible: !(w.status.open ?? false) && w.floorState === "" && w.seats > 0
            text: w.seats === 1 ? qsTr("1 seat") : qsTr("%1 seats").arg(w.seats)
            color: Qt.darker(w.ink, 1.4)
            font.family: w.face
            font.pixelSize: Math.min((Math.min(w.width, w.height) - 2 * w.chairDepth) * 0.12, 18)
        }
    }

    TapHandler {
        id: tap
        enabled: w.name !== ""
        onTapped: { if (TouchGuard.covered(point.scenePressPosition)) return; w.zone.controller.selectTable(w.name) }
    }
}
