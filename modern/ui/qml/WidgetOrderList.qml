import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// The current check: header, seat / course controls, order lines with
// modifiers, totals. Touch a line to select it (modifiers, Void, seat and
// course apply to it). props.controls: false hides the seat / course row.
// A table's checks (one per guest paying alone): 1 2 3 + across the top.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property color ink: zone.st.textColor ?? "white"
    readonly property string face: zone.st.font ?? Ui.font
    // At least 15 pixels on the screen (its smallest text, 0.6 of it, 9): a
    // phone draws the page smaller than it was made, and the seat, course and
    // Again keys were 6 or 7 pixels there.
    readonly property real unit: Math.max(12, zone.touch(15) * zone.textScale,
                                          Math.min((zone.st.fontSize ?? 28) * zone.textScale, w.width * 0.05 * zone.textScale))
    // A phone: the check's own keys (Choose, the course, − + Again) a finger
    // tall, 40 pixels; elsewhere as tall as their text needs.
    readonly property real tapH: zone && zone.controller && zone.controller.formFactor === "phone" ? zone.touch(40) : 0
    readonly property var check: pos ? pos.check : ({})
    // A swiped line: from here, since selecting it rebuilds the lines (and the row).
    function swipeOff(lineId) {
        pos.selectedLine = lineId
        pos.voidItem()
    }
    function swipeMore(lineId, sent) {
        if (sent) pos.repeatLine(lineId)
        else pos.lineMore(lineId)
    }
    readonly property var totals: pos ? pos.totals : ({})
    readonly property bool paid: pos !== null && pos.payments.length > 0
    readonly property bool controls: !(zone && zone.props && zone.props.controls === false)
    // The course Fire would send next (the lowest one on hold).
    readonly property int nextCourse: {
        let next = 0
        for (const l of (pos ? pos.lines : []))
            if (l.held && (next === 0 || l.course < next)) next = l.course
        return next
    }

    // A small panel (a phone's: under 420 pixels on the screen): one line on
    // top and the total, so several items show while ordering; Whole Check
    // opens all of it over the menu (to the rows of buttons at the bottom).
    readonly property bool small: zone ? w.height * zone.screenScale < 420 : false
    property bool whole: false
    readonly property bool folded: small && !whole
    property real wholeHeight: height
    function openWhole() {
        // Down to the buttons along the bottom of the page (Send, Pay...).
        const page = zone.parent
        let bottom = page.height
        for (const c of page.children)
            if (c !== zone && c.visible && c.width > 0 && c.y >= page.height * 0.75 && c.y > zone.y + zone.height)
                bottom = Math.min(bottom, c.y)
        wholeHeight = Math.max(height, bottom - 12 - zone.y)
        whole = true
    }
    // Folded again when the check goes (closed, put away, another opened).
    Connections {
        target: w.pos
        function onCheckClosed() { w.whole = false }
    }
    onVisibleChanged: if (!visible) whole = false

    Rectangle {
        id: wholeArea
        objectName: "wholeCheck"
        visible: w.whole && w.pos !== null && w.pos.hasCheck
        parent: w.zone ? w.zone.parent : w
        x: w.zone ? w.zone.x : 0
        y: w.zone ? w.zone.y : 0
        z: 1000
        width: w.width
        height: w.wholeHeight
        radius: 8
        color: w.zone && w.zone.st.fill ? w.zone.st.fill : Ui.panel
        border.color: "#4c8dff"
        border.width: 2
        MouseArea { anchors.fill: parent }   // the menu under it isn't touched
    }

    ColumnLayout {
        id: body
        parent: w.whole && wholeArea.visible ? wholeArea : w
        anchors.fill: parent
        anchors.margins: w.unit * 0.6
        spacing: w.unit * 0.3

        // Header
        RowLayout {
            Layout.fillWidth: true
            visible: w.pos && w.pos.hasCheck
            Text {
                readonly property var mine: (w.pos ? w.pos.tableChecks : []).find(c => c.current)
                text: (w.check.label ?? "") + (mine && w.pos.tableChecks.length > 1 ? "  ·  " + qsTr("Check %1").arg(mine.number) : "")
                color: w.ink
                font.family: w.face
                font.pixelSize: w.unit * 1.2
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            // The drinks sent last, again (tabs and tables).
            WidgetKey {
                objectName: "anotherRound"
                visible: (w.check.roundSize ?? 0) > 0 && w.zone.keyShown("round")
                Layout.preferredWidth: w.unit * 6.4
                Layout.preferredHeight: Math.max(w.unit * 1.5, w.tapH)
                text: w.zone.keyText("round", qsTr("Another Round"))
                baseColor: "#1f6f78"
                fontScale: 0.5
                onClicked: w.pos.anotherRound()
            }
            WidgetKey {
                objectName: "wholeCheckKey"
                visible: w.small && w.zone.keyShown("whole")
                Layout.preferredWidth: w.unit * 6.4
                Layout.preferredHeight: Math.max(w.unit * 1.5, w.tapH)
                text: w.whole ? qsTr("Close ▴") : qsTr("Whole Check ▾")
                baseColor: w.whole ? Ui.accent : Ui.key
                fontScale: 0.5
                onClicked: w.whole ? (w.whole = false) : w.openWhole()
            }
            Text {
                // (A food truck's order is called by its own number, in the label: not this one too.)
                visible: !w.folded && !((w.check.orderNumber ?? 0) > 0)
                text: qsTr("#%1").arg(w.check.id ?? "")
                color: Ui.muted
                font.family: w.face
                font.pixelSize: w.unit * 0.8
            }
        }
        Text {
            visible: w.pos && w.pos.hasCheck && !w.folded
            text: (w.check.server ?? "") + "  ·  "
                  + ((w.check.guests ?? 1) === 1 ? qsTr("1 guest") : qsTr("%1 guests").arg(w.check.guests))
                  + "  ·  " + (w.check.due ? qsTr("ready %1").arg(w.check.due) : (w.check.opened ?? ""))
            color: Ui.muted
            font.family: w.face
            font.pixelSize: w.unit * 0.7
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        // The table's checks: touch one to switch to it here; + opens another.
        // One line however many there are: what doesn't fit is a page away
        // (‹ ›), and the open check is always on the page shown.
        Item {
            id: tableChecks
            readonly property var checks: w.pos ? w.pos.tableChecks : []
            visible: w.pos && w.pos.hasCheck && checks.length > 0 && w.zone.keyShown("tableChecks") && !w.folded
            Layout.fillWidth: true
            Layout.preferredHeight: key
            Layout.fillHeight: false
            readonly property real key: w.unit * 1.8
            readonly property real gap: w.unit * 0.25
            // Three or more: just the numbers.
            readonly property bool compact: checks.length >= 3
            readonly property real slot: key * (compact ? 1.25 : 2.6)
            readonly property real arrow: key * 0.9
            readonly property real more: key * 1.25
            // Room for the numbers beside ⋯ and +, with and without the arrows.
            readonly property real room: width - slot - more - 2 * gap
            readonly property int fitAll: Math.max(1, Math.floor((room + gap) / (slot + gap)))
            readonly property bool paged: checks.length > fitAll
            readonly property int perPage: paged ? Math.max(1, Math.floor((room - 2 * (arrow + gap)) / (slot + gap))) : checks.length
            readonly property int current: Math.max(0, checks.findIndex(c => c.current))
            property int first: 0
            // The open check stays on the page shown.
            function follow() {
                if (current < first || current >= first + perPage)
                    first = Math.max(0, Math.min(current - perPage + 1, checks.length - perPage))
                first = Math.max(0, Math.min(first, checks.length - perPage))
            }
            onCurrentChanged: follow()
            onPerPageChanged: follow()
            onChecksChanged: follow()

            Row {
                spacing: tableChecks.gap
                height: tableChecks.key
                WidgetKey {
                    objectName: "tableCheck-prev"
                    visible: tableChecks.paged
                    enabled: tableChecks.first > 0
                    opacity: enabled ? 1 : 0.35
                    width: tableChecks.arrow; height: tableChecks.key
                    fontScale: 0.6
                    text: "‹"
                    onClicked: tableChecks.first = Math.max(0, tableChecks.first - tableChecks.perPage)
                }
                Repeater {
                    model: tableChecks.checks.slice(tableChecks.first, tableChecks.first + tableChecks.perPage)
                    delegate: WidgetKey {
                        required property var modelData
                        objectName: "tableCheck-" + modelData.number
                        width: tableChecks.slot; height: tableChecks.key
                        fontScale: tableChecks.compact ? 0.6 : 0.48
                        text: tableChecks.compact ? String(modelData.number)
                                                  : w.zone.keyText("tableChecks", qsTr("Check")) + " " + modelData.number
                        accent: modelData.current
                        enabled: modelData.current || modelData.busyOn === ""
                        onClicked: if (!modelData.current) w.pos.switchCheck(modelData.id)
                    }
                }
                WidgetKey {
                    objectName: "tableCheck-next"
                    visible: tableChecks.paged
                    enabled: tableChecks.first + tableChecks.perPage < tableChecks.checks.length
                    opacity: enabled ? 1 : 0.35
                    width: tableChecks.arrow; height: tableChecks.key
                    fontScale: 0.6
                    text: "›"
                    onClicked: tableChecks.first = Math.min(tableChecks.checks.length - tableChecks.perPage,
                                                            tableChecks.first + tableChecks.perPage)
                }
            }
            // The table's other tools: split by seat, print all, back together.
            WidgetKey {
                objectName: "tableCheck-more"
                anchors.right: plusKey.left
                anchors.rightMargin: tableChecks.gap
                width: tableChecks.more; height: tableChecks.key
                fontScale: 0.6
                text: "⋯"
                onClicked: sheet.mode = "table"
            }
            // Always in the same place, at the right end.
            WidgetKey {
                id: plusKey
                objectName: "tableCheck-new"
                anchors.right: parent.right
                width: tableChecks.slot; height: tableChecks.key
                fontScale: tableChecks.compact ? 0.6 : 0.48
                text: tableChecks.compact ? "+" : qsTr("+ Check")
                onClicked: w.pos.newTableCheck()
            }
        }
        // Phone orders: who it's for, right on the check. Touch to type it in
        // (no page of its own); a delivery's address too. Missing: in amber.
        // A food truck's order (called by its number): the name only.
        Row {
            id: who
            readonly property var customer: w.check.customer ?? ({})
            readonly property bool delivery: w.check.type === "delivery"
            readonly property bool truck: (w.check.orderNumber ?? 0) > 0
            visible: w.controls && w.pos && w.pos.hasCheck && (delivery || w.check.type === "takeout" || truck) && !w.folded
            Layout.fillWidth: true
            Layout.preferredHeight: w.unit * 1.9 * Math.min(sheet.big, 1.5)
            spacing: w.unit * 0.25
            // Name, phone, (address,) and when it's wanted.
            readonly property int keys: truck ? 2 : delivery ? 4 : 3
            readonly property real keyW: (width - spacing * (keys - 1)) / keys
            Repeater {
                model: who.truck ? ["name", "pager"] : who.delivery ? ["name", "phone", "address"] : ["name", "phone"]
                delegate: WidgetKey {
                    required property string modelData
                    objectName: "who-" + modelData
                    width: who.keyW; height: who.height
                    fontScale: 0.42
                    readonly property string value: modelData === "pager" ? ((w.check.pager ?? 0) > 0 ? qsTr("Pager %1").arg(w.check.pager) : "")
                                                                          : who.customer[modelData] ?? ""
                    readonly property bool needed: value === "" && ((modelData === "name" && (!who.truck || (w.pos.service?.needsName ?? false)))
                                                                    || (modelData === "address" && who.delivery))
                    text: value !== "" ? value
                        : modelData === "name" ? qsTr("+ Name") : modelData === "phone" ? qsTr("+ Phone")
                        : modelData === "pager" ? qsTr("+ Pager") : qsTr("+ Address")
                    baseColor: needed ? "#a86a12" : (keySt.keyFill ?? Ui.key)
                    onClicked: sheet.ask(modelData)
                }
            }
            WidgetKey {
                objectName: "who-later"
                visible: !who.truck
                width: who.keyW; height: who.height
                fontScale: 0.42
                text: w.check.due ? w.check.due : qsTr("Ready Later…")
                baseColor: w.check.due ? "#1f6f78" : (keySt.keyFill ?? Ui.key)
                onClicked: w.zone.controller.jumpTo("order-later")
            }
        }
        // When it'll be ready: quoted from how busy the kitchen is, or what they were told.
        Text {
            objectName: "readyQuote"
            visible: who.visible && !w.check.due && ((w.check.quote ?? 0) > 0 || !!w.check.promised)
            text: w.check.promised
                  ? (who.delivery ? qsTr("Promised by %1") : qsTr("Promised for %1")).arg(w.check.promised)
                  : (who.delivery ? qsTr("Arrives in about %1 min") : qsTr("Ready in about %1 min")).arg(w.check.quote ?? 0)
            color: "#7ec8ff"
            font.family: w.face
            font.pixelSize: w.unit * 0.7
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        // A regular: how often they come, and their note (allergies...).
        Text {
            objectName: "regularInfo"
            visible: w.pos && w.pos.hasCheck && !!w.check.regular && !w.folded
            text: "★ " + (w.check.regular ?? "")
            color: "#ffd479"
            font.family: w.face
            font.pixelSize: w.unit * 0.75
            font.bold: true
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
        // A regular: their last order again, in one touch (before anything else is on).
        WidgetKey {
            objectName: "sameAsLastTime"
            visible: w.controls && w.pos && w.pos.hasCheck && !!w.check.lastOrder
                     && !(w.pos.lines ?? []).some(l => !l.comment && !l.fee)
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(w.unit * 1.9, w.tapH)
            fontScale: 0.4
            text: qsTr("Same as Last Time: %1").arg(w.check.lastOrder ?? "")
            baseColor: "#1f6f78"
            onClicked: w.pos.sameAsLastTime()
        }
        // Often added with what's on the check (a hint: touching one adds it).
        RowLayout {
            objectName: "goesWith"
            visible: w.controls && w.pos && w.pos.hasCheck && (w.check.goesWith ?? []).length > 0 && !w.folded
                     && (w.pos.lines ?? []).some(l => !l.comment && !l.fee)
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(w.unit * 1.6, w.tapH)
            spacing: w.unit * 0.25
            Text {
                text: qsTr("Often added:")
                color: "#aab3c2"
                font.family: w.face
                font.pixelSize: w.unit * 0.6
            }
            Repeater {
                model: w.check.goesWith ?? []
                WidgetKey {
                    required property var modelData
                    objectName: "goesWith-" + modelData.id
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(w.unit * 1.5, w.tapH)
                    fontScale: 0.4
                    text: "+ " + modelData.name
                    baseColor: "#2b3a4a"
                    onClicked: w.zone.controller.orderItem(modelData.id)
                }
            }
        }
        Text {
            readonly property var customer: w.check.customer ?? ({})
            visible: w.pos && w.pos.hasCheck && !who.visible && !!(customer.name || customer.phone) && !w.folded
            text: (customer.name ?? "") + (customer.phone ? "  ·  " + customer.phone : "")
            color: "#7ec8ff"
            font.family: w.face
            font.pixelSize: w.unit * 0.75
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        // Seats, around the table: touch one and what's added goes to it (a
        // touched line moves to it). Each shows how many items it has; +
        // adds one more. Shown folded too: on a phone it's how seats are set.
        ListView {
            id: seats
            objectName: "seatChips"
            readonly property int current: w.check.seat ?? 0
            // Items on each seat (0: shared), and the most seats there are.
            readonly property var counts: {
                const out = {}
                for (const l of (w.pos ? w.pos.lines : []))
                    if (!l.voided && !l.comment && !l.fee)
                        out[l.seat ?? 0] = (out[l.seat ?? 0] ?? 0) + l.quantity
                return out
            }
            readonly property int highest: {
                let m = 0
                for (const k in counts) m = Math.max(m, Number(k))
                return m
            }
            readonly property int guests: w.check.guests ?? 0
            readonly property int seatCount: Math.max(guests, highest, current)   // (not "count": a list's own)
            visible: w.controls && w.pos !== null && w.pos.hasCheck && w.zone.keyShown("seat")
                     && (guests > 0 || current > 0 || highest > 0)
            Layout.fillWidth: true
            Layout.preferredHeight: chip
            readonly property real chip: Math.max(w.unit * 1.5, w.zone.touch(44))
            // Wide enough for its count, as many as fit; more scroll sideways.
            readonly property real chipWidth: Math.max(chip, Math.min(chip * 1.5, (width - spacing * (seatCount + 2)) / (seatCount + 2)))
            orientation: ListView.Horizontal
            spacing: w.unit * 0.2
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: seatCount + 2   // Shared, 1..seatCount, +
            delegate: WidgetKey {
                required property int index
                readonly property bool more: index === seats.seatCount + 1
                readonly property int seat: more ? seats.seatCount + 1 : index
                readonly property int items: seats.counts[seat] ?? 0
                readonly property bool allergic: !more && seat > 0 && ((w.check.seatAllergies ?? {})[String(seat)] ?? []).length > 0
                objectName: more ? "seatAdd" : "seat" + seat
                width: index === 0 ? seats.chipWidth * 1.5 : seats.chipWidth
                height: seats.chip
                accent: !more && seat === seats.current
                baseColor: more ? "#2a3140" : allergic ? "#7a2226" : Ui.key
                fontScale: index === 0 ? 0.36 : 0.45
                text: more ? "+" : (allergic ? "⚠" : "") + (index === 0 ? w.zone.keyText("seat", qsTr("Shared")) : seat)
                      + (items > 0 && !more ? " ·" + items : "")
                onClicked: w.pos.setSeat(seat)
                // Held: this guest's allergies.
                holdable: !more && seat > 0
                onHeld: w.zone.controller.commandWith("allergies", { seat: seat })
            }
            // The touched seat in view (seat 9 of 12).
            onCurrentChanged: positionViewAtIndex(Math.min(current, seatCount + 1), ListView.Contain)
        }

        // Seat and course for new items (or a touched line); Fire. Sized from
        // the panel's width so it never pushes the panel wider.
        Item {
            id: bar
            visible: w.controls && w.pos && w.pos.hasCheck && !w.folded
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(w.unit * 1.6, w.tapH)
            Layout.fillHeight: false
            readonly property real key: Math.min(w.unit * 1.5, width / 13)
            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                height: parent.height
                spacing: bar.key * 0.15
                Text {
                    visible: w.zone.keyShown("course")
                    anchors.verticalCenter: parent.verticalCenter
                    text: w.zone.keyText("course", qsTr("Course"))
                    color: Ui.muted
                    font.family: w.face
                    font.pixelSize: bar.key * 0.45
                }
                Repeater {
                    model: [1, 2, 3]
                    delegate: WidgetKey {
                        required property int modelData
                        objectName: "course" + modelData
                        visible: w.zone.keyShown("course")
                        width: Math.max(bar.key, w.tapH); height: parent.height
                        text: modelData
                        accent: (w.check.course ?? 1) === modelData
                        onClicked: w.pos.setCourse(modelData)
                    }
                }
            }
            // Fire it later: in 5, 10, 15 or 20 minutes (or not after all).
            WidgetKey {
                objectName: "fireLater"
                visible: fireKey.visible
                anchors.right: fireKey.left
                anchors.rightMargin: bar.key * 0.15
                width: bar.key * 1.05
                height: parent.height
                text: "⏱"
                fontScale: 0.5
                accent: !!w.check.firesAt
                onClicked: sheet.mode = "fire"
            }
            WidgetKey {
                id: fireKey
                visible: w.nextCourse > 0 && w.zone.keyShown("fire")
                anchors.right: parent.right
                width: bar.key * 2.25
                height: parent.height
                text: w.check.firesAt ? qsTr("Course %1 at %2").arg(w.nextCourse).arg(w.check.firesAt)
                                      : w.zone.keyText("fire", qsTr("Fire Course %1").arg(w.nextCourse))
                baseColor: "#a86a12"
                fontScale: 0.28
                objectName: "fireCourse"
                onClicked: w.pos.fireCourse()
            }
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Ui.line; visible: w.pos && w.pos.hasCheck }

        ListView {
            id: list
            ScrollBar.vertical: TouchScrollBar { id: listBar }
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            model: w.pos ? w.pos.lines : []
            onCountChanged: positionViewAtEnd()

            delegate: Rectangle {
                id: row
                required property var modelData
                width: ListView.view.width - listBar.room
                height: Math.max(col.implicitHeight + w.unit * 0.4, w.tapH)   // a phone: a finger tall
                radius: 6
                color: modelData.selected ? Ui.accent : "transparent"
                // Tapped while the server was out of reach: goes in when it's back.
                readonly property bool waiting: modelData.waiting === true
                objectName: waiting ? "waitingLine" : ""
                opacity: waiting ? 0.55 : 1

                TapHandler { enabled: !row.waiting; onTapped: { if (TouchGuard.covered(point.scenePressPosition)) return; w.pos.selectedLine = row.modelData.id } }
                // Swipe left: off the check (a void once sent). Swipe right: one more.
                readonly property real swipeAt: width * 0.28
                DragHandler {
                    id: swipe
                    objectName: "lineSwipe"
                    target: null
                    yAxis.enabled: false
                    enabled: w.controls && !row.modelData.voided && !w.paid && !row.waiting
                    onActiveChanged: {
                        if (active)
                            return
                        const dx = row.slide
                        const line = row.modelData
                        row.slide = 0
                        if (dx <= -row.swipeAt)
                            w.swipeOff(line.id)
                        else if (dx >= row.swipeAt && !line.comment && !line.fee)
                            w.swipeMore(line.id, line.sent)
                    }
                    onTranslationChanged: if (active) row.slide = translation.x
                }
                property real slide: 0
                // What letting go does, under the line as it slides.
                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    visible: row.slide !== 0
                    color: row.slide < 0 ? (row.slide <= -row.swipeAt ? "#c0393f" : "#5a2a2d")
                                         : (row.slide >= row.swipeAt ? Ui.go : "#22402f")
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: row.slide < 0 ? parent.right : undefined
                        anchors.left: row.slide > 0 ? parent.left : undefined
                        anchors.margins: w.unit * 0.5
                        text: row.slide < 0 ? (row.modelData.sent ? qsTr("Void") : qsTr("Remove")) : qsTr("+1")
                        color: "white"
                        font.family: w.face
                        font.pixelSize: w.unit * 0.8
                        font.bold: true
                    }
                }

                ColumnLayout {
                    id: col
                    transform: Translate { x: row.slide }
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: w.unit * 0.3
                    anchors.rightMargin: w.unit * 0.3
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            // ● not yet sent to the kitchen; ‖ held for a later course
                            text: row.modelData.held ? "‖" : row.modelData.sent ? " " : "●"
                            color: "#f5b940"
                            font.pixelSize: row.modelData.held ? w.unit * 0.8 : w.unit * 0.5
                            font.bold: true
                            Layout.preferredWidth: w.unit * 0.8
                        }
                        Text {
                            // S2 / C2: seat and (later) course
                            visible: text !== ""
                            text: (row.modelData.seat > 0 ? "S" + row.modelData.seat : "")
                                  + (row.modelData.course > 1 ? (row.modelData.seat > 0 ? " " : "") + "C" + row.modelData.course : "")
                                  + (row.modelData.held ? " " + qsTr("HOLD") : "")
                            color: row.modelData.held ? "#f5b940" : "#7ec8ff"
                            font.family: w.face
                            font.pixelSize: w.unit * 0.65
                            font.bold: true
                        }
                        Text {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1   // what's left after Choose and the price (big text)
                            text: (row.modelData.quantity > 1 ? row.modelData.quantity + " × " : "") + row.modelData.name
                            color: row.modelData.voided ? Ui.muted : w.ink
                            font.family: w.face
                            font.pixelSize: w.unit
                            font.italic: row.modelData.comment
                            font.strikeout: row.modelData.voided
                            elide: Text.ElideRight
                        }
                        // Choices for an unsent item: amber while a required one is missing.
                        // A phone: the key on the touched line (a finger tall), the others
                        // just say so (so every line stays one line).
                        Text {
                            visible: row.modelData.choices && w.tapH > 0 && !row.modelData.selected && row.modelData.needsChoice
                            text: qsTr("Choose ›")
                            color: "#f5b940"
                            font.family: w.face
                            font.pixelSize: w.unit * 0.8
                            font.bold: true
                        }
                        WidgetKey {
                            visible: row.modelData.choices && (w.tapH === 0 || row.modelData.selected)
                            Layout.preferredWidth: w.unit * 3.4
                            Layout.preferredHeight: Math.max(w.unit * 1.4, w.tapH)
                            text: qsTr("Choose")
                            baseColor: row.modelData.needsChoice ? "#a86a12" : Ui.key
                            fontScale: 0.5
                            onClicked: w.zone.controller.chooseLine(row.modelData.id)
                        }
                        Text {
                            text: row.modelData.voided ? qsTr("VOID") : row.waiting ? qsTr("waiting…") : row.modelData.price
                            color: row.modelData.voided ? "#ff6369" : row.waiting ? "#f5b940" : w.ink
                            font.family: w.face
                            font.pixelSize: w.unit
                        }
                    }
                    // Has what its guest is allergic to.
                    Text {
                        visible: (row.modelData.allergy ?? []).length > 0
                        Layout.fillWidth: true
                        Layout.leftMargin: w.unit * 1.6
                        text: "⚠ " + qsTr("Contains %1").arg((row.modelData.allergy ?? []).join(", "))
                        color: "#ff6369"
                        font.family: w.face
                        font.pixelSize: w.unit * 0.8
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Repeater {
                        model: row.modelData.modifiers
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.leftMargin: w.unit * 1.6
                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                color: Ui.textSoft
                                font.family: w.face
                                font.pixelSize: w.unit * 0.8
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.price
                                color: Ui.textSoft
                                font.family: w.face
                                font.pixelSize: w.unit * 0.8
                            }
                        }
                    }
                    // The touched line: − 2 + to change how many, Again for one
                    // more the same way (a sent line: only more, as a new line).
                    Row {
                        id: qtyBar
                        visible: row.modelData.selected && row.modelData.countable && w.zone.keyShown("quantity")
                        Layout.topMargin: w.unit * 0.25
                        Layout.leftMargin: w.unit * 0.8
                        spacing: w.unit * 0.25
                        // Keys this wide fit the line (big text on a narrow check): − 1 +, Again, Move…
                        readonly property real units: (row.modelData.sent ? 0 : 3.7) + 3
                                                      + (tableChecks.checks.length > 0 && !w.paid ? 3 : 0)
                        readonly property real key: Math.min(w.unit * 1.6, (col.width - w.unit * 2) / units)
                        readonly property real keyH: Math.max(key, w.tapH)
                        WidgetKey {
                            objectName: "lineLess"
                            visible: !row.modelData.sent
                            width: Math.max(qtyBar.key * 1.3, w.tapH); height: qtyBar.keyH
                            fontScale: 0.6
                            text: "−"
                            onClicked: w.pos.lineLess(row.modelData.id)
                        }
                        Text {
                            visible: !row.modelData.sent
                            width: qtyBar.key * 1.1
                            height: qtyBar.keyH
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: row.modelData.quantity
                            color: "white"
                            font.family: w.face
                            font.pixelSize: qtyBar.key * 0.6
                            font.bold: true
                        }
                        WidgetKey {
                            objectName: "lineMore"
                            visible: !row.modelData.sent
                            width: Math.max(qtyBar.key * 1.3, w.tapH); height: qtyBar.keyH
                            fontScale: 0.6
                            text: "+"
                            onClicked: w.pos.lineMore(row.modelData.id)
                        }
                        WidgetKey {
                            objectName: "lineAgain"
                            width: qtyBar.key * 3; height: qtyBar.keyH
                            fontScale: 0.45
                            text: w.zone.keyText("quantity", qsTr("Again"))
                            onClicked: w.pos.repeatLine(row.modelData.id)
                        }
                        // At a table: to another of its checks (or a new one).
                        WidgetKey {
                            objectName: "lineMove"
                            visible: tableChecks.checks.length > 0 && !w.paid
                            width: qtyBar.key * 3; height: qtyBar.keyH
                            fontScale: 0.45
                            text: qsTr("Move…")
                            onClicked: sheet.mode = "move"
                        }
                    }
                }
            }

            // "Removed Cobb  Undo": for a few seconds after an item comes off.
            Rectangle {
                id: undoBar
                objectName: "undoBar"
                readonly property string text: w.pos ? w.pos.undoText : ""
                property bool timedOut: false
                onTextChanged: { timedOut = false; undoTimer.restart() }
                Timer { id: undoTimer; interval: 8000; onTriggered: undoBar.timedOut = true }
                visible: text !== "" && !timedOut && w.zone.keyShown("undo")
                z: 2
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: w.unit * 2
                radius: 6
                color: Ui.sunken
                border.color: "#f5b940"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: w.unit * 0.5
                    anchors.rightMargin: w.unit * 0.25
                    Text {
                        Layout.fillWidth: true
                        text: undoBar.text
                        color: "white"
                        font.family: w.face
                        font.pixelSize: w.unit * 0.75
                        elide: Text.ElideRight
                    }
                    WidgetKey {
                        objectName: "undoLast"
                        Layout.preferredWidth: w.unit * 3.6
                        Layout.preferredHeight: Math.max(w.unit * 1.5, w.tapH)
                        fontScale: 0.5
                        baseColor: "#a86a12"
                        text: w.zone.keyText("undo", qsTr("Undo"))
                        onClicked: w.pos.undoLast()
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width * 0.8
                visible: list.count === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: w.pos && w.pos.hasCheck ? qsTr("Touch menu items to add them.")
                                             : (w.pos?.service?.mode ?? "") === "retail" ? qsTr("Scan or touch an item to start a sale.")
                                             : qsTr("No check open.\nTouch an item to start a quick check.")
                color: Ui.muted
                font.family: w.face
                font.pixelSize: w.unit * 0.8
            }
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Ui.line; visible: w.pos && w.pos.hasCheck }

        GridLayout {
            Layout.fillWidth: true
            visible: w.pos && w.pos.hasCheck
            columns: 2
            rowSpacing: 0

            component Cell: Text {
                property bool strong: false
                color: w.ink
                font.family: w.face
                font.pixelSize: strong ? w.unit * 1.1 : w.unit * 0.8
                font.bold: strong
            }

            // Folded: the total only (Whole Check shows the rest).
            Cell { text: qsTr("Subtotal"); visible: !w.folded; Layout.fillWidth: true }
            Cell { text: w.totals.subtotal ?? ""; visible: !w.folded }
            Cell { text: qsTr("Discount"); visible: (w.totals.hasDiscount ?? false) && !w.folded; Layout.fillWidth: true }
            Cell { text: w.totals.discounts ?? ""; visible: (w.totals.hasDiscount ?? false) && !w.folded }
            Cell { text: qsTr("Tax"); visible: !w.folded; Layout.fillWidth: true }
            Cell { text: w.totals.tax ?? ""; visible: !w.folded }
            Cell { text: qsTr("Gratuity %1%").arg(w.totals.gratuityPercent ?? 0); visible: (w.totals.hasGratuity ?? false) && !w.folded; Layout.fillWidth: true }
            Cell { text: w.totals.gratuity ?? ""; visible: (w.totals.hasGratuity ?? false) && !w.folded }
            Cell { text: qsTr("Total"); strong: true; Layout.fillWidth: true }
            Cell { text: w.totals.total ?? ""; strong: true }
            Cell { text: qsTr("Paid"); visible: w.paid; Layout.fillWidth: true }
            Cell { text: w.totals.paid ?? ""; visible: w.paid }
            Cell { text: qsTr("Balance due"); visible: w.paid; Layout.fillWidth: true }
            Cell { text: w.totals.balance ?? ""; visible: w.paid }
        }
    }

    // Choices over the panel: the table's tools, or where to move the touched line.
    Rectangle {
        id: sheet
        property string mode: ""   // "" | "table" | "move"
        objectName: "orderSheet"
        parent: body.parent
        z: 1
        anchors.fill: parent
        visible: mode !== "" && w.pos && w.pos.hasCheck
        color: Qt.rgba(0.06, 0.07, 0.09, 0.94)
        radius: 8
        MouseArea { anchors.fill: parent; onClicked: sheet.mode = "" }   // outside the keys: close
        // On a phone the check's text is small: its choices are made bigger.
        readonly property real big: w.zone && w.zone.controller && w.zone.controller.formFactor === "phone" ? 1.8 : 1
        readonly property real key: w.unit * 2.2 * big
        readonly property var others: tableChecks.checks.filter(c => !c.current)
        // Close, then act: from here, since the key touched may go away with either.
        function run(action) {
            mode = ""
            action()
        }
        function fireIn(minutes) {   // 0: now; -1: not on a timer after all
            mode = ""
            w.pos.fireCourseIn(minutes)
        }
        // 2 × Cheeseburger: move one of them, or all.
        property bool moveAll: false
        readonly property int moveCount: (w.pos?.lines.find(l => l.selected)?.quantity) ?? 1
        onModeChanged: moveAll = false
        function moveTo(checkId) {   // 0: a new check
            const all = moveAll
            mode = ""
            w.pos.splitLine(checkId, all)
        }
        // Who the order is for: "name" | "phone" | "address".
        readonly property bool asking: mode === "name" || mode === "phone" || mode === "address" || mode === "pager"
        function ask(field) {
            mode = field
            whoField.text = field === "pager" ? ((w.check.pager ?? 0) > 0 ? String(w.check.pager) : "")
                                              : (w.check.customer ?? {})[field] ?? ""
            whoField.selectAll()
            whoField.forceActiveFocus()
        }
        function saveWho() {
            if (mode === "pager") {   // a food truck's: the guest's pager, by its number
                mode = ""
                w.pos.setPager(parseInt(whoField.text, 10) || 0)
                return
            }
            const c = Object.assign({}, w.check.customer ?? {})
            c[mode] = whoField.text
            mode = ""
            w.pos.setCustomer(c)
        }
        function useRegular(id) {
            mode = ""
            w.pos.useCustomer(id)
        }

        Column {
            anchors.centerIn: parent
            width: parent.width * (sheet.big > 1 ? 0.94 : 0.85)
            spacing: w.unit * 0.4
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: sheet.mode === "name" ? qsTr("Name for the order")
                    : sheet.mode === "phone" ? qsTr("Their phone number")
                    : sheet.mode === "address" ? qsTr("Where to deliver it")
                    : sheet.mode === "pager" ? qsTr("The pager's number (0: none)")
                    : sheet.mode === "move" ? qsTr("Move %1 to…").arg(w.pos.lines.find(l => l.selected)?.name ?? "")
                    : sheet.mode === "fire" ? qsTr("Fire course %1").arg(w.nextCourse)
                    : (w.check.label ?? "")
                color: "white"
                font.family: w.face
                font.pixelSize: w.unit * sheet.big
                font.bold: true
            }
            // Who it's for: type it, or touch a regular that matches.
            TextField {
                id: whoField
                objectName: "whoField"
                visible: sheet.asking
                width: parent.width
                height: sheet.key
                font.family: w.face
                font.pixelSize: w.unit * 0.9 * sheet.big
                inputMethodHints: sheet.mode === "phone" ? Qt.ImhDialableCharactersOnly : sheet.mode === "pager" ? Qt.ImhDigitsOnly : Qt.ImhNone
                placeholderText: sheet.mode === "name" ? qsTr("Name") : sheet.mode === "phone" ? qsTr("Phone") : sheet.mode === "pager" ? qsTr("Number on the pager")
                                                                       : qsTr("Street, apartment, city")
                onTextEdited: if (sheet.mode !== "address" && sheet.mode !== "pager" && text.trim().length >= 3) w.pos.findCustomers(text.trim())
                onAccepted: sheet.saveWho()
            }
            Repeater {
                model: sheet.asking && sheet.mode !== "address" && sheet.mode !== "pager" && whoField.text.trim().length >= 3
                       ? (w.pos.customers ?? []).slice(0, 3) : []
                delegate: WidgetKey {
                    required property var modelData
                    objectName: "regular-" + modelData.id
                    width: parent.width; height: sheet.key * 0.8
                    fontScale: 0.38
                    text: modelData.name + (modelData.phone ? "  ·  " + modelData.phone : "")
                    onClicked: sheet.useRegular(modelData.id)
                }
            }
            WidgetKey {
                objectName: "whoSave"
                visible: sheet.asking
                width: parent.width; height: sheet.key
                fontScale: 0.42
                text: qsTr("Save")
                baseColor: Ui.go
                onClicked: sheet.saveWho()
            }
            // Table tools
            WidgetKey {
                objectName: "splitBySeat"
                visible: sheet.mode === "table"
                width: parent.width; height: sheet.key
                fontScale: 0.38
                text: qsTr("One Check per Seat")
                onClicked: sheet.run(() => w.pos.splitBySeat())
            }
            WidgetKey {
                objectName: "printTableChecks"
                visible: sheet.mode === "table"
                width: parent.width; height: sheet.key
                fontScale: 0.38
                text: tableChecks.checks.length > 1 ? qsTr("Print Every Check (%1)").arg(tableChecks.checks.length)
                                                    : qsTr("Print the Check")
                onClicked: sheet.run(() => w.pos.printTableChecks())
            }
            WidgetKey {
                objectName: "combineTableChecks"
                visible: sheet.mode === "table" && tableChecks.checks.length > 1
                width: parent.width; height: sheet.key
                fontScale: 0.38
                text: qsTr("Put Them Back Together")
                onClicked: sheet.run(() => w.pos.combineTableChecks())
            }
            // Fire the next course: now, or paced.
            Flow {
                visible: sheet.mode === "fire"
                width: parent.width
                spacing: w.unit * 0.3
                Repeater {
                    model: sheet.mode === "fire" ? [0, 5, 10, 15, 20] : []
                    delegate: WidgetKey {
                        required property int modelData
                        objectName: "fireIn-" + modelData
                        width: modelData === 0 ? parent.width : (parent.width - w.unit * 0.9) / 4
                        height: sheet.key
                        fontScale: 0.4
                        text: modelData === 0 ? qsTr("Now") : qsTr("In %1 min").arg(modelData)
                        baseColor: modelData === 0 ? "#a86a12" : Ui.key
                        onClicked: sheet.fireIn(modelData)
                    }
                }
                WidgetKey {
                    objectName: "fireCancel"
                    visible: !!w.check.firesAt
                    width: parent.width
                    height: sheet.key * 0.8
                    fontScale: 0.4
                    text: qsTr("Don't fire it at %1").arg(w.check.firesAt ?? "")
                    onClicked: sheet.fireIn(-1)
                }
            }
            // Move the touched line
            Flow {
                visible: sheet.mode === "move"
                width: parent.width
                spacing: w.unit * 0.3
                WidgetKey {
                    objectName: "moveOne"
                    visible: sheet.moveCount > 1
                    width: sheet.key * 2.4; height: sheet.key
                    fontScale: 0.42
                    text: qsTr("One")
                    accent: !sheet.moveAll
                    onClicked: sheet.moveAll = false
                }
                WidgetKey {
                    objectName: "moveAll"
                    visible: sheet.moveCount > 1
                    width: sheet.key * 2.4; height: sheet.key
                    fontScale: 0.42
                    text: qsTr("All %1").arg(sheet.moveCount)
                    accent: sheet.moveAll
                    onClicked: sheet.moveAll = true
                }
                Item { visible: sheet.moveCount > 1; width: parent.width; height: 1 }   // then the checks, below
                Repeater {
                    model: sheet.mode === "move" ? sheet.others : []
                    delegate: WidgetKey {
                        required property var modelData
                        objectName: "moveTo-" + modelData.number
                        width: sheet.key * 2.4; height: sheet.key
                        fontScale: 0.42
                        text: qsTr("Check %1").arg(modelData.number)
                        enabled: modelData.busyOn === ""
                        onClicked: sheet.moveTo(modelData.id)
                    }
                }
                WidgetKey {
                    objectName: "moveTo-new"
                    width: sheet.key * 3.6; height: sheet.key
                    fontScale: 0.42
                    text: qsTr("A New Check")
                    onClicked: sheet.moveTo(0)
                }
            }
            WidgetKey {
                objectName: "sheetCancel"
                width: parent.width; height: sheet.key * 0.8
                fontScale: 0.4
                text: qsTr("Cancel")
                onClicked: sheet.mode = ""
            }
        }
        Connections {   // another check, or none: nothing to choose for
            target: w.pos
            function onCheckChanged() { if (!w.pos.hasCheck) sheet.mode = "" }
        }
    }
}
