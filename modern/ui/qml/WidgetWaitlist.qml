import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// The host stand: the waitlist and reservations on the left; on the right,
// adding a party, or what to do with the one chosen (notify, check in,
// seat them at a free table that fits, or take them off the list).
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var info: pos ? pos.waitlist : ({})
    readonly property var waiting: info.waiting ?? []
    readonly property var booked: info.booked ?? []
    readonly property bool showBooked: tabs.currentIndex === 1
    readonly property var rows: showBooked ? booked : waiting
    property var chosenId: null
    readonly property var chosen: {
        for (const p of waiting.concat(booked))
            if (p.id === chosenId) return p
        return null
    }
    property bool seating: false

    // Tables that are free, the smallest that fits first.
    readonly property var freeTables: {
        if (pos && pos.checksRevision < 0) return []   // again when checks change (compared: an unused read is compiled away)
        const size = chosen ? chosen.size : 1
        const all = zone && zone.controller ? zone.controller.tables() : []
        // Not dirty, not pushed into another party's table, not held for someone else.
        const free = all.filter(t => {
            const s = w.pos.tableStatus(t.name)
            return !s.open && s.floor !== "dirty" && s.floor !== "joined"
                   && !(s.floor === "reserved" && (!w.chosen || s.partyId !== w.chosen.id))
        })
        free.sort((a, b) => {
            const fa = a.seats >= size, fb = b.seats >= size
            if (fa !== fb) return fa ? -1 : 1
            return fa ? a.seats - b.seats : b.seats - a.seats
        })
        return free
    }

    // --- the add form ---
    property string newName: ""
    property string newPhone: ""
    property int newSize: 2
    property string newNote: ""
    property bool reservation: false
    property int quote: info.nextQuote ?? 10
    property bool quoteTouched: false
    onInfoChanged: if (!quoteTouched) quote = info.nextQuote ?? 10
    property int dayOffset: 0
    property string time: "7:00 PM"

    function clearForm() {
        newName = ""; newPhone = ""; newSize = 2; newNote = ""; quoteTouched = false
        quote = info.nextQuote ?? 10
    }
    function add() {
        const p = { name: newName, phone: newPhone, size: newSize, note: newNote }
        if (reservation) {
            const d = new Date()
            d.setDate(d.getDate() + dayOffset)
            const day = Qt.formatDate(d, "yyyy-MM-dd")
            p.at = day + " " + time
            pos.addReservation(p)
        } else {
            p.quote = quote
            pos.addToWaitlist(p)
        }
        clearForm()
    }

    // Upright (a phone): the list; + Add a Party or a party, each on its own, with ‹ List.
    readonly property bool narrow: zone ? zone.narrow : false
    property bool adding: false
    readonly property bool listShown: !narrow || (!adding && chosen === null)
    readonly property real zoom: zone ? zone.formZoom(narrow ? 360 : 719, narrow ? 0 : 330) : 1
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 12

            // --- the line and the book ---
            ColumnLayout {
                visible: w.listShown
                Layout.preferredWidth: w.narrow ? parent.width : parent.width * 0.52
                Layout.fillWidth: w.narrow
                Layout.fillHeight: true
                spacing: 6
                TabBar {
                    id: tabs
                    Layout.fillWidth: true
                    TabButton { text: qsTr("Waitlist (%1)").arg(w.waiting.length); font.pixelSize: 17; implicitHeight: 48 }
                    TabButton { text: qsTr("Reservations (%1)").arg(w.booked.length); font.pixelSize: 17; implicitHeight: 48 }
                    onCurrentIndexChanged: { w.chosenId = null; w.seating = false; w.reservation = currentIndex === 1 }
                }
                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: w.rows
                    ScrollBar.vertical: TouchScrollBar { id: listBar }
                    delegate: ItemDelegate {
                        id: row
                        required property var modelData
                        width: ListView.view.width - listBar.room
                        height: 64
                        highlighted: row.modelData.id === w.chosenId
                        onClicked: { w.chosenId = row.modelData.id; w.seating = false }
                        contentItem: RowLayout {
                            spacing: 10
                            Label {
                                text: w.showBooked ? row.modelData.time : row.modelData.position + "."
                                font.pixelSize: 17
                                font.bold: true
                                color: row.modelData.overdue ? "#ff9a9e" : palette.text
                                Layout.preferredWidth: w.showBooked ? 150 : 30
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Label {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    font.pixelSize: 17
                                    font.bold: true
                                    text: row.modelData.name + "  ·  " + (row.modelData.size === 1 ? qsTr("1 person") : qsTr("%1 people").arg(row.modelData.size))
                                          + (row.modelData.reservation && !w.showBooked ? "  ·  " + qsTr("reserved") : "")
                                }
                                Label {
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    opacity: 0.75
                                    text: [row.modelData.regular ? "★ " + row.modelData.regular : "", row.modelData.note, row.modelData.phone,
                                           row.modelData.overdue ? qsTr("late") : ""].filter(s => s).join("  ·  ")
                                }
                            }
                            Label {
                                visible: !w.showBooked
                                font.pixelSize: 16
                                horizontalAlignment: Text.AlignRight
                                color: row.modelData.late ? "#ff9a9e" : palette.text
                                text: qsTr("%1 min").arg(row.modelData.waited)
                                      + (row.modelData.quoted ? " / " + row.modelData.quoted : "")
                                      + (row.modelData.notifiedAgo >= 0 ? "\n" + qsTr("notified %1m ago").arg(row.modelData.notifiedAgo) : "")
                            }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: list.count === 0
                        text: w.showBooked ? qsTr("No reservations.") : qsTr("No one is waiting.")
                        opacity: 0.6
                    }
                }
                Label {
                    Layout.fillWidth: true
                    opacity: 0.7
                    text: qsTr("Seated today %1  ·  average wait %2 min  ·  no-shows %3")
                          .arg(w.info.seatedToday ?? 0).arg(w.info.averageWait ?? 0).arg(w.info.noShows ?? 0)
                }
                Button {
                    visible: w.narrow
                    Layout.fillWidth: true
                    implicitHeight: 52
                    highlighted: true
                    text: qsTr("+ Add a Party")
                    onClicked: w.adding = true
                }
            }

            ToolSeparator { Layout.fillHeight: true; visible: !w.narrow }

            // --- add a party ---
            ColumnLayout {
                visible: w.chosen === null && (!w.narrow || w.adding)
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                Button {
                    visible: w.narrow
                    text: qsTr("‹ List")
                    onClicked: w.adding = false
                }
                Label {
                    text: w.reservation ? qsTr("Book a table") : qsTr("Add to the waitlist")
                    font.pixelSize: 20
                    font.bold: true
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 10
                    Label { text: qsTr("Name") }
                    TextField { implicitHeight: 48; Layout.fillWidth: true; text: w.newName; onTextEdited: w.newName = text }
                    Label { text: qsTr("Phone") }
                    TextField {
                        implicitHeight: 48
                        Layout.fillWidth: true
                        text: w.newPhone
                        inputMethodHints: Qt.ImhDialableCharactersOnly
                        placeholderText: qsTr("for the text when the table is ready")
                        onTextEdited: w.newPhone = text
                    }
                    Label { text: qsTr("Party") }
                    RowLayout {
                        Button { text: "−"; font.pixelSize: 26; implicitWidth: 56; implicitHeight: 48; onClicked: w.newSize = Math.max(1, w.newSize - 1) }
                        Label { text: w.newSize; font.pixelSize: 24; font.bold: true; horizontalAlignment: Text.AlignHCenter; Layout.preferredWidth: 50 }
                        Button { text: "+"; font.pixelSize: 26; implicitWidth: 56; implicitHeight: 48; onClicked: w.newSize = Math.min(99, w.newSize + 1) }
                    }
                    Label { text: w.reservation ? qsTr("When") : qsTr("Quote") }
                    RowLayout {
                        visible: !w.reservation
                        Button { text: "−5"; font.pixelSize: 20; implicitHeight: 48; onClicked: { w.quoteTouched = true; w.quote = Math.max(0, w.quote - 5) } }
                        Label { text: qsTr("%1 min").arg(w.quote); font.pixelSize: 20; Layout.preferredWidth: 80; horizontalAlignment: Text.AlignHCenter }
                        Button { text: "+5"; font.pixelSize: 20; implicitHeight: 48; onClicked: { w.quoteTouched = true; w.quote += 5 } }
                    }
                    ColumnLayout {
                        visible: w.reservation
                        RowLayout {
                            Repeater {
                                model: [qsTr("Today"), qsTr("Tomorrow"), "+2", "+3", "+4", "+5", "+6"]
                                delegate: Button {
                                    required property string modelData
                                    required property int index
                                    text: modelData
                                    checkable: true
                                    checked: w.dayOffset === index
                                    onClicked: w.dayOffset = index
                                }
                            }
                        }
                        TextField {
                            implicitHeight: 48
                            implicitWidth: 120
                            text: w.time
                            placeholderText: "7:30 PM"
                            inputMethodHints: Qt.ImhPreferNumbers
                            onTextEdited: w.time = text
                        }
                    }
                    Label { text: qsTr("Note") }
                    TextField {
                        implicitHeight: 48
                        Layout.fillWidth: true
                        text: w.newNote
                        placeholderText: qsTr("high chair, booth, birthday…")
                        onTextEdited: w.newNote = text
                    }
                }
                Item { Layout.fillHeight: true }
                Button {
                    Layout.fillWidth: true
                    implicitHeight: 64
                    font.pixelSize: 19
                    highlighted: true
                    enabled: w.newName.trim().length > 0
                    text: w.reservation ? qsTr("Book It") : qsTr("Add to the Waitlist")
                    onClicked: { w.add(); w.adding = false }
                }
            }

            // --- the party chosen ---
            ColumnLayout {
                visible: w.chosen !== null
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                Button {
                    visible: w.narrow
                    text: qsTr("‹ List")
                    onClicked: w.chosenId = null
                }
                Label {
                    text: w.chosen ? w.chosen.name + "  ·  " + (w.chosen.size === 1 ? qsTr("1 person") : qsTr("%1 people").arg(w.chosen.size)) : ""
                    font.pixelSize: 22
                    font.bold: true
                }
                Label {
                    opacity: 0.75
                    text: !w.chosen ? "" : w.chosen.status === "booked" ? qsTr("Reserved for %1").arg(w.chosen.time)
                                       : qsTr("Waiting %1 min (quoted %2)").arg(w.chosen.waited).arg(w.chosen.quoted)
                }
                Label { visible: w.chosen && w.chosen.note !== ""; text: w.chosen ? w.chosen.note : "" }

                GridLayout {
                    visible: !w.seating
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 8
                    Button {
                        Layout.fillWidth: true
                        implicitHeight: 64
                        visible: w.chosen && w.chosen.status !== "booked"
                        text: w.chosen && w.chosen.status === "notified" ? qsTr("Notify Again") : qsTr("Table Ready: Notify")
                        onClicked: w.pos.notifyParty(w.chosen.id)
                    }
                    Button {
                        Layout.fillWidth: true
                        implicitHeight: 64
                        visible: w.chosen && w.chosen.status === "booked"
                        text: qsTr("They're Here")
                        onClicked: { w.pos.checkInParty(w.chosen.id); tabs.currentIndex = 0 }
                    }
                    Button {
                        Layout.fillWidth: true
                        implicitHeight: 64
                        highlighted: true
                        text: qsTr("Seat Them…")
                        onClicked: w.seating = true
                    }
                    Button {
                        Layout.fillWidth: true
                        implicitHeight: 56
                        text: w.chosen && w.chosen.status === "booked" ? qsTr("Cancel Booking") : qsTr("Left the Line")
                        onClicked: { w.pos.partyGone(w.chosen.id, false); w.chosenId = null }
                    }
                    Button {
                        Layout.fillWidth: true
                        implicitHeight: 56
                        visible: w.chosen && w.chosen.status === "booked"
                        text: qsTr("No-Show")
                        onClicked: { w.pos.partyGone(w.chosen.id, true); w.chosenId = null }
                    }
                    Button {
                        Layout.fillWidth: true
                        implicitHeight: 56
                        text: qsTr("‹ Done")
                        onClicked: w.chosenId = null
                    }
                }

                // Seating: the server, then a free table (best fit first).
                RowLayout {
                    visible: w.seating
                    Label { text: qsTr("Server") }
                    ComboBox {
                        id: server
                        Layout.fillWidth: true
                        textRole: "name"
                        valueRole: "id"
                        model: w.pos ? w.pos.staff.filter(s => s.clockedIn || s.me) : []
                    }
                }
                GridView {
                    id: tables
                    visible: w.seating
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    cellWidth: (width - tablesBar.room) / 3
                    cellHeight: 84
                    model: w.freeTables
                    ScrollBar.vertical: TouchScrollBar { id: tablesBar; needed: Math.ceil(tables.count / 3) * 84 > tables.height }
                    delegate: Button {
                        required property var modelData
                        required property int index
                        width: tables.cellWidth - 8
                        height: tables.cellHeight - 8
                        readonly property bool fits: !w.chosen || modelData.seats >= w.chosen.size
                        highlighted: index === 0 && fits
                        opacity: fits ? 1 : 0.55
                        text: modelData.name + "\n" + (modelData.seats === 1 ? qsTr("1 seat") : qsTr("%1 seats").arg(modelData.seats))
                        onClicked: {
                            // Seating changes the list (this button among it): everything first.
                            const party = w.chosen.id, table = modelData.name, who = server.currentValue ?? "", pos = w.pos
                            w.seating = false
                            w.chosenId = null
                            pos.seatParty(party, table, who)
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: tables.count === 0
                        text: qsTr("No table is free.")
                        opacity: 0.6
                    }
                }
                Button {
                    visible: w.seating
                    text: qsTr("‹ Back")
                    implicitHeight: 48
                    onClicked: w.seating = false
                }
                Item { Layout.fillHeight: true; visible: !w.seating }
            }
        }
    }
}
