import QtQuick
import QtQuick.Layouts

// Kitchen / bar display: every order sent and not yet made, oldest first.
// Touch a ticket when it is ready (bump); Recall brings the last one back.
// props.station: "kitchen", "bar"... shows only that printer's lines
// (empty = everything). The Station button picks one of the store's
// stations (grill, fryer...) for this screen instead; it stays picked.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string pageStation: zone && zone.props && zone.props.station ? zone.props.station : ""
    // Panels side by side keep the station they were set up with (props.lockStation).
    readonly property bool locked: zone && zone.props && zone.props.lockStation === true
    readonly property string station: !expo && !locked && pos && pos.kitchenStation ? pos.kitchenStation : pageStation
    readonly property var stations: pos ? pos.kitchenStations.filter(s => !s.printer) : []
    function stationName(id) {
        if (id === "") return qsTr("All stations")
        const all = w.pos ? w.pos.kitchenStations : []
        const s = all.find(x => x.id === id)
        return s ? s.name : id.charAt(0).toUpperCase() + id.slice(1)
    }
    // The page's own, then each station, and around again.
    function nextStation() {
        const ids = [w.pageStation].concat(w.stations.map(s => s.id).filter(id => id !== w.pageStation))
        const next = ids[(ids.indexOf(w.station) + 1) % ids.length]
        w.pos.setKitchenStation(next === w.pageStation ? "" : next)
    }
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    property real now: Date.now()
    // Header buttons and title fit a narrow screen (two or three stations side by side).
    readonly property real keyWidth: Math.max(96, Math.min(240, width * 0.12))

    // Tickets with only this station's lines.
    // props.mode "expo": the expediter - every station, what each has made,
    // green when the whole ticket is ready; touching it sends it out.
    readonly property bool expo: zone && zone.props && zone.props.mode === "expo"
    property var armed: null   // expo: a ticket touched once while not ready

    // Kitchen highlight colors (Manager -> Menu), readable on the ticket.
    function inkFor(name) {
        return ({ red: "#c92a2a", orange: "#d9480f", yellow: "#9c6f00", green: "#2b8a3e",
                  blue: "#1864ab", purple: "#7048e8" })[name] ?? "#1b1b1b"
    }

    readonly property var tickets: {
        if (!pos) return []
        if (expo) return pos.expoTickets
        const out = []
        for (const t of pos.kitchenTickets) {
            const lines = station === "" ? t.lines
                        : t.lines.filter(l => l.station === station || l.printer === station || l.comment)
            if (lines.some(l => !l.comment))
                out.push(Object.assign({}, t, { lines: lines }))
        }
        return out
    }

    Timer { id: disarmExpo; interval: 3000; onTriggered: w.armed = null }

    // The same from buttons placed anywhere on the page.
    Connections {
        target: w.zone ? w.zone.controller : null
        function onWidgetCommand(name) {
            if (name === "kitchenAllDay")
                w.showAllDay = !w.showAllDay
            else if (name === "kitchenStation" && !w.expo && !w.locked && w.stations.length > 0)
                w.nextStation()
        }
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: w.now = Date.now()
    }

    function elapsed(sentAt) {
        const s = Math.max(0, Math.floor((now - sentAt) / 1000))
        return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0")
    }
    // Green, then yellow after the store's warn minutes, red when late.
    function ageColor(ticket) {
        const minutes = (now - ticket.sentAt) / 60000
        return minutes < (ticket.warnMinutes ?? 8) ? w.zone.statusColor("kitchenNew", "#1f8a4c")
             : minutes < (ticket.lateMinutes ?? 15) ? w.zone.statusColor("kitchenWarn", "#b7791f")
             : w.zone.statusColor("kitchenLate", "#c53030")
    }

    // "All day": everything still to make here, by item, most first.
    property bool showAllDay: false
    readonly property var allDay: {
        const counts = {}
        for (const t of tickets)
            for (const l of t.lines)
                if (!l.comment)
                    counts[l.name] = (counts[l.name] ?? 0) + l.quantity
        return Object.keys(counts).map(k => ({ name: k, count: counts[k] })).sort((a, b) => b.count - a.count)
    }

    // What's above a ticket's lines: RUSH / VIP / ready time, the expediter's
    // "waiting on", the guest's allergies, the note. A continued card shows
    // only the allergies again.
    component TicketTop: Column {
        id: top
        property var ticket: ({})
        property bool first: true
        spacing: 4
        Text {
            visible: top.first && (top.ticket.rush || top.ticket.vip || !!top.ticket.due)
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: [top.ticket.rush ? qsTr("RUSH") : "", top.ticket.vip ? qsTr("VIP") : "",
                   top.ticket.due ? qsTr("READY %1").arg(top.ticket.due) : ""]
                  .filter(s => s).join("  ·  ")
            color: top.ticket.rush ? "#d62828" : "#9a7b12"
            font.family: w.face
            font.pixelSize: 28
            font.bold: true
        }
        Text {
            visible: top.first && w.expo && !top.ticket.ready
            width: parent.width
            wrapMode: Text.WordWrap
            readonly property bool isArmed: w.armed !== null && w.armed.checkId === top.ticket.checkId
                                            && w.armed.sentAt === top.ticket.sentAt
            text: isArmed ? qsTr("Not all made. Touch again to send it out anyway.")
                          : qsTr("Waiting on: %1").arg((top.ticket.waitingOn ?? []).join(", "))
            color: isArmed ? "#c92a2a" : "#6b6b6b"
            font.family: w.face
            font.pixelSize: 20
            font.bold: isArmed
        }
        // The guest's allergies: big, on red, before anything else is read.
        Rectangle {
            objectName: "kdsAllergy"
            visible: (top.ticket.allergies ?? []).length > 0
            width: parent.width
            height: allergyText.implicitHeight + 12
            radius: 6
            color: "#c62828"
            Text {
                id: allergyText
                anchors.centerIn: parent
                width: parent.width - 16
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("ALLERGY: %1").arg((top.ticket.allergies ?? []).join(", ").toUpperCase())
                color: "white"
                font.family: w.face
                font.pixelSize: 26
                font.bold: true
            }
        }
        Text {
            visible: top.first && !!top.ticket.note
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Note: %1").arg(top.ticket.note ?? "")
            color: "#b83232"
            font.family: w.face
            font.pixelSize: 22
            font.bold: true
        }
    }

    // One item and its choices ("S2 1  Cobb Salad", "› No Onion").
    component TicketLine: Column {
        id: line
        property var line: ({})
        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            // "S2 1  Cobb Salad", course 2+ marked: the kitchen
            // plates by seat and knows what was fired later.
            text: (line.line.seat > 0 ? "S" + line.line.seat + "  " : "")
                  + (line.line.comment ? "** " + line.line.name + " **"
                                       : line.line.quantity + "  " + line.line.name)
                  + (line.line.course > 1 ? "   (course " + line.line.course + ")" : "")
                  + (w.expo ? "   " + (line.line.made ? "✓" : "· " + line.line.station) : "")
            color: line.line.comment ? "#b83232"
                 : w.expo && line.line.made ? "#2b8a3e" : w.inkFor(line.line.color)
            font.family: w.face
            font.pixelSize: 26
            font.bold: true
            font.italic: line.line.comment === true
        }
        Repeater {
            model: line.line.modifiers ?? []
            delegate: Text {
                required property string modelData
                width: line.width
                leftPadding: 28
                wrapMode: Text.WordWrap
                text: "› " + modelData
                color: "#444"
                font.family: w.face
                font.pixelSize: 22
            }
        }
        Text {
            visible: (line.line.contains ?? []).length > 0
            width: line.width
            leftPadding: 28
            wrapMode: Text.WordWrap
            text: qsTr("!! CONTAINS %1").arg((line.line.contains ?? []).join(", ").toUpperCase())
            color: "#c62828"
            font.family: w.face
            font.pixelSize: 22
            font.bold: true
        }
    }

    // Each ticket laid out unseen, to know how tall its parts are.
    readonly property real cardWidth: 360
    Item {
        id: measure
        visible: false
        width: w.cardWidth - 28
        Repeater {
            id: measured
            model: w.tickets
            delegate: Item {
                required property var modelData
                property alias firstTop: firstTop
                property alias moreTop: moreTop
                property alias lineItems: lineItems
                TicketTop { id: firstTop; ticket: parent.modelData; first: true; width: measure.width }
                TicketTop { id: moreTop; ticket: parent.modelData; first: false; width: measure.width }
                Repeater {
                    id: lineItems
                    model: parent.modelData.lines
                    delegate: TicketLine {
                        required property var modelData
                        line: modelData
                        width: measure.width
                    }
                }
                Text { id: continued; visible: false; text: qsTr("Continued ▸"); font.family: w.face; font.pixelSize: 24; font.bold: true }
                property alias continued: continued
            }
        }
    }

    // [[{ticket, part, parts, from, to}, ...], ...]: the columns, left to right.
    property var columns: []
    function relayout() { Qt.callLater(w.layoutColumns) }
    onTicketsChanged: relayout()
    onArmedChanged: relayout()
    function layoutColumns() {
        const room = board.height
        if (room <= 0) return
        const chrome = 72 + 14 + 22 + 8   // the head, the margins, a little to spare
        const out = []
        let column = null, used = 0
        for (let k = 0; k < w.tickets.length; ++k) {
            const m = measured.itemAt(k)
            const t = w.tickets[k]
            const n = t.lines.length
            const heights = []
            for (let i = 0; i < n; ++i) {
                const it = m ? m.lineItems.itemAt(i) : null
                heights.push((it ? it.implicitHeight : 40) + 4)
            }
            const top = (shown, item) => item && item.implicitHeight > 0 ? item.implicitHeight + 4 : 0
            const more = m ? m.continued.implicitHeight + 4 : 40
            // Its parts: as many lines as fit each card (an item stays whole).
            const parts = []
            let i = 0
            while (i < n || parts.length === 0) {
                const firstPart = parts.length === 0
                let space = room - chrome - top(true, m ? (firstPart ? m.firstTop : m.moreTop) : null)
                let rest = 0
                for (let j = i; j < n; ++j) rest += heights[j]
                if (rest <= space) {
                    parts.push({ from: i, to: n, height: chrome + room - chrome - space + rest })
                    break
                }
                space -= more
                let j = i, sum = 0
                while (j < n && sum + heights[j] <= space) sum += heights[j++]
                if (j === i) { sum = heights[j]; ++j }   // an item taller than a card: on its own
                parts.push({ from: i, to: j, height: room })
                i = j
            }
            for (let p = 0; p < parts.length; ++p) {
                const piece = { ticket: t, part: p + 1, parts: parts.length, from: parts[p].from, to: parts[p].to }
                const h = parts[p].height
                if (column && p === 0 && used + 12 + h <= room) {
                    column.push(piece)
                    used += 12 + h
                } else {
                    column = [piece]
                    out.push(column)
                    used = h
                }
            }
        }
        w.columns = out
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // One row; the buttons in the order the editor gives them.
        GridLayout {
            id: header
            readonly property var keys: ["station", "message", "waste", "labels", "allDay", "recall"]
            // A phone: the title on its own row, the buttons three to a row
            // under it (in one row they were cut to "Station…", "Messa…").
            readonly property bool stacked: w.width * (w.zone ? w.zone.screenScale : 1) < 700
            readonly property real keyH: w.zone ? w.zone.touch(48) : 56
            function shown(id) {
                if (!w.zone.keyShown(id)) return false
                if (id === "station") return !w.expo && !w.locked && w.stations.length > 0
                return !(w.expo && (id === "waste" || id === "labels"))
            }
            // Stacked: by place among the buttons shown (no gaps for hidden ones).
            readonly property var order: keys.filter(id => shown(id)).sort((a, b) => w.zone.keyOrder(a, keys) - w.zone.keyOrder(b, keys))
            function col(id) { return stacked ? Math.max(0, order.indexOf(id)) % 3 : 1 + w.zone.keyOrder(id, keys) }
            function row(id) { return stacked ? 1 + Math.floor(Math.max(0, order.indexOf(id)) / 3) : 0 }
            readonly property real keyPx: stacked && w.zone ? w.zone.touch(16) : 0
            columnSpacing: 6
            rowSpacing: 6
            Layout.fillWidth: true
            Layout.preferredHeight: stacked ? (w.zone ? w.zone.touch(34) : 40) + Math.ceil(order.length / 3) * (keyH + 6) : 72
            Layout.fillHeight: false   // nested layouts fill by default
            Text {
                Layout.row: 0
                Layout.column: 0
                Layout.columnSpan: header.stacked ? 3 : 1
                Layout.fillWidth: true
                text: (w.expo ? qsTr("Expo") : w.stationName(w.station))
                      + "  ·  " + (w.tickets.length === 1 ? qsTr("1 order") : qsTr("%1 orders").arg(w.tickets.length))
                color: "white"
                font.family: w.face
                font.pixelSize: header.stacked ? (w.zone ? w.zone.touch(20) : 22) : Math.max(20, Math.min(40, w.width / 32))
                font.bold: true
                elide: Text.ElideRight
            }
            WidgetKey {
                objectName: "kdsStation"
                visible: header.shown("station")
                Layout.column: header.col("station")
                Layout.row: header.row("station")
                Layout.preferredWidth: header.stacked ? 0 : w.keyWidth
                Layout.fillWidth: header.stacked
                Layout.minimumWidth: 56
                Layout.fillHeight: true
                text: w.zone.keyText("station", qsTr("Station…"))
                fontScale: 0.4
                pixelSize: header.keyPx
                onClicked: w.nextStation()
            }
            WidgetKey {
                visible: header.shown("message")
                Layout.column: header.col("message")
                Layout.row: header.row("message")
                Layout.preferredWidth: header.stacked ? 0 : w.keyWidth
                Layout.fillWidth: header.stacked
                Layout.minimumWidth: 56
                Layout.fillHeight: true
                text: w.zone.keyText("message", qsTr("Message…"))
                fontScale: 0.4
                pixelSize: header.keyPx
                onClicked: w.zone.controller.jumpTo("message")
            }
            WidgetKey {
                objectName: "kdsWaste"
                visible: header.shown("waste")
                Layout.column: header.col("waste")
                Layout.row: header.row("waste")
                Layout.preferredWidth: header.stacked ? 0 : w.keyWidth
                Layout.fillWidth: header.stacked
                Layout.minimumWidth: 56
                Layout.fillHeight: true
                text: w.zone.keyText("waste", qsTr("Waste…"))
                fontScale: 0.4
                pixelSize: header.keyPx
                onClicked: w.zone.controller.jumpTo("waste-log")
            }
            WidgetKey {
                objectName: "kdsLabels"
                visible: header.shown("labels")
                Layout.column: header.col("labels")
                Layout.row: header.row("labels")
                Layout.preferredWidth: header.stacked ? 0 : w.keyWidth
                Layout.fillWidth: header.stacked
                Layout.minimumWidth: 56
                Layout.fillHeight: true
                text: w.zone.keyText("labels", qsTr("Labels…"))
                fontScale: 0.4
                pixelSize: header.keyPx
                onClicked: w.zone.controller.jumpTo("prep-labels")
            }
            WidgetKey {
                objectName: "kdsAllDay"
                visible: header.shown("allDay")
                Layout.column: header.col("allDay")
                Layout.row: header.row("allDay")
                Layout.preferredWidth: header.stacked ? 0 : w.keyWidth
                Layout.fillWidth: header.stacked
                Layout.minimumWidth: 56
                Layout.fillHeight: true
                text: w.showAllDay ? qsTr("Hide All Day") : w.zone.keyText("allDay", qsTr("All Day"))
                fontScale: 0.4
                pixelSize: header.keyPx
                onClicked: w.showAllDay = !w.showAllDay
            }
            WidgetKey {
                objectName: "kdsRecall"
                visible: header.shown("recall")
                Layout.column: header.col("recall")
                Layout.row: header.row("recall")
                Layout.preferredWidth: header.stacked ? 0 : w.keyWidth
                Layout.fillWidth: header.stacked
                Layout.minimumWidth: 56
                Layout.fillHeight: true
                text: w.zone.keyText("recall", qsTr("Recall"))
                fontScale: 0.4
                pixelSize: header.keyPx
                onClicked: w.expo ? w.pos.expoRecall() : w.pos.recallTicket()
            }
        }

        // Rush Radar: a strip when this station (or, on the expo, any) is about to back up.
        Rectangle {
            objectName: "kdsRush"
            visible: rushStrip.headline !== ""
            Layout.fillWidth: true
            Layout.preferredHeight: rushStrip.implicitHeight + 16
            Layout.fillHeight: false
            radius: 8
            color: "#3a2e10"
            border.color: "#f5b940"
            RushRadarView {
                id: rushStrip
                anchors.fill: parent
                anchors.margins: 8
                compact: true
                active: w.visible
                pos: w.pos
                station: w.expo ? "" : w.station
                unit: Math.max(16, Math.min(28, w.width / 50))
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

        // The tickets in columns as tall as the screen, oldest first: small
        // ones stack, and one too long for the room left goes on in the
        // next column on a card of its own ("Continued"). More columns than
        // fit: swipe sideways.
        Flickable {
            id: board
            objectName: "kdsBoard"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: columnsRow.implicitWidth
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            onHeightChanged: w.relayout()

            Behavior on contentX { enabled: !board.moving; NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
            Row {
                id: columnsRow
                spacing: 12
                Repeater {
                    model: w.columns
                    delegate: Column {
                        id: ticketColumn
                        required property var modelData
                        width: w.cardWidth
                        spacing: 12
                        Repeater {
                            model: ticketColumn.modelData
                            delegate: Rectangle {
                    id: card
                    required property var modelData
                    readonly property var ticket: modelData.ticket
                    objectName: "kdsCard-" + ticket.checkId + "-" + modelData.part
                    width: w.cardWidth
                    height: Math.min(board.height, body.implicitHeight + 72 + 14 + 22)
                    radius: 10
                    color: "#f4f1ea"
                    // Rush tickets wear a red frame, VIP gold.
                    border.color: tap.pressed ? "#2f6fd6" : card.ticket.rush ? "#ff3b3b"
                                : card.ticket.vip ? "#d4af37" : "transparent"
                    border.width: card.ticket.rush || card.ticket.vip ? 8 : 4
                    clip: true

                    Rectangle {
                        id: head
                        width: parent.width
                        height: 72
                        radius: 10
                        color: w.expo && card.ticket.ready ? w.zone.statusColor("kitchenReady", "#1f6fd6") : w.ageColor(card.ticket)
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 10; color: parent.color }
                        Column {
                            anchors.left: parent.left
                            anchors.right: timeText.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 14
                            anchors.rightMargin: 8
                            Text {
                                width: parent.width
                                elide: Text.ElideRight
                                // A long order's cards: "T5  2/3".
                                text: card.ticket.label + (card.modelData.parts > 1
                                                           ? "  " + card.modelData.part + "/" + card.modelData.parts : "")
                                color: "white"
                                font.family: w.face
                                font.pixelSize: 28
                                font.bold: true
                            }
                            Text {
                                width: parent.width
                                elide: Text.ElideRight
                                text: card.ticket.customer || card.ticket.server
                                color: "white"
                                font.family: w.face
                                font.pixelSize: 18
                            }
                        }
                        // Its time, against what it should take; LATE past that.
                        Column {
                            id: timeText
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.rightMargin: 14
                            readonly property bool late: !w.expo && !(card.ticket.ready ?? false)
                                                         && (w.now - card.ticket.sentAt) / 60000 >= (card.ticket.lateMinutes ?? 15)
                            Text {
                                anchors.right: parent.right
                                visible: timeText.late
                                text: qsTr("LATE")
                                color: "white"
                                font.family: w.face
                                font.pixelSize: 16
                                font.bold: true
                            }
                        Text {
                            anchors.right: parent.right
                            text: w.expo && card.ticket.ready ? qsTr("READY")
                                : w.elapsed(card.ticket.sentAt)
                                  + ((card.ticket.targetMinutes ?? 0) > 0 && !timeText.late
                                     ? " / " + qsTr("%1m").arg(card.ticket.targetMinutes) : "")
                            color: "white"
                            font.family: w.face
                            font.pixelSize: timeText.late ? 24 : 26
                            font.bold: true
                        }
                        }
                    }

                    Column {
                        id: body
                        anchors.top: head.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 14
                        spacing: 4
                        TicketTop {
                            ticket: card.ticket
                            first: card.modelData.part === 1
                            width: parent.width
                        }
                        Repeater {
                            model: card.ticket.lines.slice(card.modelData.from, card.modelData.to)
                            delegate: TicketLine {
                                required property var modelData
                                line: modelData
                                width: body.width
                            }
                        }
                        Text {
                            objectName: "kdsContinued"
                            visible: card.modelData.part < card.modelData.parts
                            width: parent.width
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("Continued ▸")
                            color: "#1864ab"
                            font.family: w.face
                            font.pixelSize: 24
                            font.bold: true
                        }
                    }

                    TapHandler {
                        id: tap
                        onTapped: {
                            if (TouchGuard.covered(point.scenePressPosition)) return   // the keyboard's touch
                            const t = card.ticket
                            if (!w.expo) {
                                w.pos.bumpTicket(t.checkId, t.sentAt, w.station)
                            } else if (t.ready || (w.armed && w.armed.checkId === t.checkId && w.armed.sentAt === t.sentAt)) {
                                w.armed = null
                                w.pos.expoBump(t.checkId, t.sentAt)   // out to the table
                            } else {
                                w.armed = { checkId: t.checkId, sentAt: t.sentAt }   // not ready: touch again to send anyway
                                disarmExpo.restart()
                            }
                        }
                    }
                }
                        }
                    }
                }
            }
        }

        // All day: the totals to make, for batching (twelve burgers on the grill...).
        Rectangle {
            visible: w.showAllDay
            Layout.preferredWidth: 340
            Layout.fillHeight: true
            radius: 10
            color: "#232933"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6
                Text {
                    text: qsTr("All day")
                    color: "white"
                    font.family: w.face
                    font.pixelSize: 30
                    font.bold: true
                }
                Repeater {
                    model: w.allDay
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Text {
                            text: modelData.count
                            color: "#f5b940"
                            font.family: w.face
                            font.pixelSize: 30
                            font.bold: true
                            Layout.preferredWidth: 60
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: "white"
                            font.family: w.face
                            font.pixelSize: 24
                            elide: Text.ElideRight
                        }
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }
        }
    }

    // Orders past the right edge: how many, and a touch goes there.
    readonly property int hiddenOrders: {
        const step = w.cardWidth + 12
        const seen = board.contentX + board.width
        const ids = new Set()
        for (let c = 0; c < w.columns.length; ++c)
            if (c * step + w.cardWidth > seen + 1)
                for (const piece of w.columns[c])
                    if (piece.part === 1) ids.add(piece.ticket.checkId + ":" + piece.ticket.sentAt)
        return ids.size
    }
    Rectangle {
        visible: w.hiddenOrders > 0
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        width: moreText.implicitWidth + 40
        height: 64
        radius: 32
        color: "#1864ab"
        border.color: "white"
        border.width: 2
        Text {
            id: moreText
            objectName: "kdsMore"
            anchors.centerIn: parent
            text: qsTr("%n more ▸", "", w.hiddenOrders)
            color: "white"
            font.family: w.face
            font.pixelSize: 26
            font.bold: true
        }
        // (Its own: the order under it isn't bumped.)
        MouseArea {
            anchors.fill: parent
            onClicked: board.contentX = Math.min(Math.max(0, board.contentWidth - board.width),
                                                 board.contentX + Math.max(w.cardWidth + 12, board.width - w.cardWidth))
        }
    }

    Text {
        anchors.centerIn: parent
        visible: w.tickets.length === 0
        text: qsTr("All caught up")
        color: "#8a94a6"
        font.family: w.face
        font.pixelSize: 48
    }
}
