import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Find any check, today's or from any day before (back to the first one):
// "#123", "17.62", a name, a phone number, a table, a server, an item or a
// gift card - or Show All for every check in the range, a page at a time.
// Touch one to see it: its items, payments, refunds and its whole history
// (the audit trail). Refund… gives money back (a manager's); Reprint
// Receipt prints a copy.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var info: pos ? pos.checkSearch : ({})
    readonly property var found: info.results ?? []
    readonly property var check: info.selected ?? null
    // How far back: days (0: every check ever).
    property int days: 365
    property string asked: ""          // the search ("" with Show All: every check)
    property bool searched: false
    property var offsets: [0]         // where each page began (for ‹ Newer)
    function run(query, offset) {
        asked = query
        searched = true
        w.pos.searchChecks(query, days, offset)
    }
    function fresh(query) { offsets = [0]; run(query, 0) }
    function older() {
        offsets = offsets.concat([info.nextOffset])
        run(asked, info.nextOffset)
    }
    function newer() {
        offsets = offsets.slice(0, -1)
        run(asked, offsets[offsets.length - 1])
    }

    // Refund…: how much (all that's left, or part) and why.
    property var refunding: null      // the payment
    function askRefund(payment) {
        refunding = payment
        refundAmount.text = (payment.leftCents / 100).toFixed(2)
        refundReason.text = ""
    }

    // Upright (a phone): the results, then a check on its own, with ‹ Results.
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real zoom: zone ? zone.formZoom(narrow ? 360 : 1150) : 1.4
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                spacing: 8
                TextField {
                    id: query
                    objectName: "searchQuery"
                    Layout.fillWidth: true
                    implicitHeight: 48
                    font.pixelSize: 18
                    placeholderText: qsTr("#123, 17.62, a name, phone, table, server, item…")
                    onAccepted: if (text.trim().length > 1) w.fresh(text.trim())
                }
                Button {
                    objectName: "searchGo"
                    text: qsTr("Search")
                    highlighted: true
                    implicitHeight: 48
                    implicitWidth: 120
                    font.pixelSize: 17
                    enabled: query.text.trim().length > 1
                    onClicked: w.fresh(query.text.trim())
                }
                Button {
                    objectName: "searchAll"
                    text: qsTr("Show All")
                    implicitHeight: 48
                    implicitWidth: 120
                    font.pixelSize: 17
                    onClicked: { query.text = ""; w.fresh("") }
                }
            }
            // How far back.
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: [[1, qsTr("Today")], [7, qsTr("Week")], [31, qsTr("Month")], [365, qsTr("Year")],
                            [730, qsTr("2 Years")], [1826, qsTr("5 Years")], [18263, qsTr("50 Years")], [0, qsTr("All")]]
                    delegate: Button {
                        required property var modelData
                        objectName: "range-" + modelData[0]
                        text: modelData[1]
                        implicitHeight: 38
                        font.pixelSize: 14
                        highlighted: w.days === modelData[0]
                        onClicked: {
                            w.days = modelData[0]
                            if (w.searched)
                                w.fresh(w.asked)
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: w.searched
                Label {
                    Layout.fillWidth: true
                    text: w.info.loading ? qsTr("Looking…")
                        : w.found.length === 0 ? (w.info.query ? qsTr("No checks match \"%1\" on this page.").arg(w.info.query)
                                                               : qsTr("No closed checks in this range."))
                        : w.info.query ? qsTr("%1 found for \"%2\", newest first.").arg(w.found.length).arg(w.info.query)
                                       : qsTr("%1 checks, newest first.").arg(w.found.length)
                    opacity: 0.7
                    elide: Text.ElideRight
                }
                Button {
                    objectName: "pageNewer"
                    visible: w.offsets.length > 1
                    text: qsTr("‹ Newer")
                    onClicked: w.newer()
                }
                Button {
                    objectName: "pageOlder"
                    visible: !!w.info.more && !w.info.loading
                    text: qsTr("Older ›")
                    onClicked: w.older()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                ListView {
                    id: list
                    objectName: "searchResults"
                    visible: !w.narrow || !w.check
                    Layout.preferredWidth: (w.width / w.zoom - 32) * (w.narrow ? 1 : 0.45)   // not the row's width: that loops
                    Layout.fillWidth: w.narrow
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: w.found
                    ScrollBar.vertical: TouchScrollBar { id: listBar; needed: list.contentHeight > list.height + 1 }
                    delegate: Rectangle {
                        id: row
                        required property var modelData
                        objectName: "found-" + modelData.id
                        width: ListView.view.width - listBar.room
                        height: 56
                        radius: 6
                        color: w.check && w.check.id === modelData.id ? "#2f4f86" : rowArea.pressed ? "#323b49" : "#232a35"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 0
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "#" + row.modelData.id + "  " + row.modelData.label; font.bold: true; font.pixelSize: 15; Layout.fillWidth: true; elide: Text.ElideRight }
                                Label { text: row.modelData.total; font.bold: true; font.pixelSize: 15 }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: row.modelData.when + "  ·  " + row.modelData.server
                                      + (row.modelData.customer ? "  ·  " + row.modelData.customer : "")
                                      + (row.modelData.status !== "closed" ? "  ·  " + row.modelData.status : "")
                                opacity: 0.65
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                        }
                        MouseArea { id: rowArea; anchors.fill: parent; onClicked: w.pos.selectFoundCheck(row.modelData.id) }
                    }
                }

                Rectangle {
                    visible: !w.narrow || !!w.check
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#1d232c"
                    Label {
                        anchors.centerIn: parent
                        visible: !w.check
                        text: w.found.length ? qsTr("Touch a check to see it.") : ""
                        opacity: 0.6
                    }
                    ColumnLayout {
                        visible: !!w.check
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4
                        Button {
                            visible: w.narrow
                            text: qsTr("‹ Results")
                            onClicked: w.pos.selectFoundCheck(0)
                        }
                        Label { text: w.check ? "#" + w.check.id + "  " + w.check.label : ""; font.bold: true; font.pixelSize: 20 }
                        Label {
                            Layout.fillWidth: true
                            text: w.check ? w.check.server + "  ·  " + qsTr("%n guest(s)", "", w.check.guests ?? 1)
                                            + (w.check.customer ? "  ·  " + w.check.customer : "") : ""
                            opacity: 0.7
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: w.check ? qsTr("Opened %1").arg(w.check.opened) + (w.check.closed ? "  ·  " + qsTr("closed %1").arg(w.check.closed) : "") : ""
                            opacity: 0.6
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                        ListView {
                            id: lines
                            objectName: "checkDetail"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.topMargin: 6
                            clip: true
                            // The items, the payments (refund them), refunds made, and everything
                            // that happened to the check: its audit trail.
                            model: {
                                if (!w.check)
                                    return []
                                const rows = w.check.lines.map(l => Object.assign({ type: "line" }, l))
                                rows.push({ type: "header", name: qsTr("Payments") })
                                for (const p of w.check.payments)
                                    rows.push(Object.assign({ type: "payment" }, p))
                                if ((w.check.refunds ?? []).length) {
                                    rows.push({ type: "header", name: qsTr("Refunds") })
                                    for (const r of w.check.refunds)
                                        rows.push(Object.assign({ type: "refund" }, r))
                                }
                                if ((w.check.events ?? []).length) {
                                    rows.push({ type: "header", name: qsTr("History") })
                                    for (const e of w.check.events)
                                        rows.push(Object.assign({ type: "event" }, e))
                                }
                                return rows
                            }
                            ScrollBar.vertical: TouchScrollBar { id: linesBar; needed: lines.contentHeight > lines.height + 1 }
                            delegate: ColumnLayout {
                                id: detailRow
                                required property var modelData
                                width: ListView.view.width - linesBar.room
                                spacing: 0
                                readonly property string type: modelData.type
                                Label {
                                    visible: detailRow.type === "header"
                                    Layout.topMargin: 8
                                    text: detailRow.modelData.name ?? ""
                                    font.bold: true
                                    opacity: 0.8
                                }
                                RowLayout {
                                    visible: detailRow.type === "line" || detailRow.type === "payment" || detailRow.type === "refund"
                                    Layout.fillWidth: true
                                    Label {
                                        Layout.fillWidth: true
                                        text: detailRow.type === "refund"
                                              ? qsTr("%1 · %2 · %3").arg(detailRow.modelData.when).arg(detailRow.modelData.tender).arg(detailRow.modelData.reason)
                                              : (detailRow.modelData.quantity > 1 ? detailRow.modelData.quantity + " × " : "") + (detailRow.modelData.name ?? "")
                                        font.strikeout: detailRow.modelData.voided ?? false
                                        color: detailRow.type === "payment" ? "#7ee2a8" : detailRow.type === "refund" ? "#ffb3b6" : "white"
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        text: detailRow.type === "line" ? (detailRow.modelData.price ?? "")
                                            : detailRow.type === "refund" ? "−" + detailRow.modelData.amount
                                            : (detailRow.modelData.amount ?? "")
                                        color: detailRow.type === "payment" ? "#7ee2a8" : detailRow.type === "refund" ? "#ffb3b6" : "white"
                                    }
                                    Button {
                                        objectName: "refund-" + (detailRow.modelData.id ?? "")
                                        visible: detailRow.type === "payment" && !!detailRow.modelData.refundable
                                        text: qsTr("Refund…")
                                        implicitHeight: 34
                                        font.pixelSize: 13
                                        onClicked: w.askRefund(detailRow.modelData)
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    visible: text !== ""
                                    text: detailRow.type === "line" ? (detailRow.modelData.modifiers ?? "")
                                        : detailRow.type === "payment"
                                          ? [detailRow.modelData.tip ? qsTr("tip %1").arg(detailRow.modelData.tip) : "",
                                             detailRow.modelData.change ? qsTr("change given %1").arg(detailRow.modelData.change) : "",
                                             detailRow.modelData.refunded ? qsTr("%1 refunded").arg(detailRow.modelData.refunded) : ""]
                                                .filter(x => x).join("  ·  ")
                                        : detailRow.type === "refund" ? qsTr("by %1").arg(detailRow.modelData.by)
                                                                         + (detailRow.modelData.reference ? "  ·  " + detailRow.modelData.reference : "")
                                        : ""
                                    opacity: 0.6
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                                // The audit trail: when, who, what.
                                RowLayout {
                                    visible: detailRow.type === "event"
                                    Layout.fillWidth: true
                                    Label {
                                        text: detailRow.modelData.when ?? ""
                                        opacity: 0.55
                                        font.pixelSize: 12
                                        Layout.preferredWidth: 150
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: (detailRow.modelData.who ? detailRow.modelData.who + ": " : "") + (detailRow.modelData.what ?? "")
                                        font.pixelSize: 13
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("Total"); font.bold: true; font.pixelSize: 18; Layout.fillWidth: true }
                            Label { text: w.check ? w.check.total : ""; font.bold: true; font.pixelSize: 18 }
                        }
                        Button {
                            objectName: "reprint"
                            Layout.fillWidth: true
                            implicitHeight: 50
                            font.pixelSize: 16
                            text: qsTr("Reprint Receipt")
                            onClicked: w.pos.reprintCheck(w.check.id)
                        }
                    }
                }
            }
        }
    }

    // Refund…: the amount (all that's left, or part), the reason, then a
    // manager's PIN if it isn't a manager here.
    Rectangle {
        id: refundSheet
        objectName: "refundSheet"
        anchors.fill: parent
        z: 5
        visible: w.refunding !== null
        color: Qt.rgba(0.05, 0.06, 0.08, 0.94)
        MouseArea { anchors.fill: parent }
        ColumnLayout {
            anchors.centerIn: parent
            // The screen's size, like the rest of the page.
            scale: w.zoom
            width: Math.min(parent.width / w.zoom - 40, 520)
            spacing: 10
            Label {
                text: w.refunding ? qsTr("Refund %1").arg(w.refunding.name) : ""
                font.bold: true
                font.pixelSize: 20
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            Label {
                text: !w.refunding ? ""
                    : w.refunding.how === "stripe" ? qsTr("Back to the guest's card through Stripe (5–10 business days to show).")
                    : w.refunding.how === "cash" ? qsTr("Cash out of this drawer (or your bank), as a refund.")
                    : qsTr("Recorded here: give it back on the card machine it was taken on.")
                opacity: 0.75
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            Label {
                text: w.refunding ? qsTr("Amount (up to %1)").arg(w.refunding.left)
                                    + (w.refunding.tip ? "  ·  " + qsTr("the %1 tip stays with the staff").arg(w.refunding.tip) : "")
                                  : ""
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
            TextField {
                id: refundAmount
                objectName: "refundAmount"
                Layout.fillWidth: true
                implicitHeight: 48
                font.pixelSize: 20
                inputMethodHints: Qt.ImhFormattedNumbersOnly
            }
            Label { text: qsTr("Why") }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: [qsTr("Wrong item"), qsTr("Food quality"), qsTr("Charged twice"), qsTr("Order canceled"), qsTr("Guest complaint")]
                    delegate: Button {
                        required property string modelData
                        text: modelData
                        font.pixelSize: 13
                        onClicked: refundReason.text = modelData
                    }
                }
            }
            TextField {
                id: refundReason
                objectName: "refundReason"
                Layout.fillWidth: true
                implicitHeight: 44
                placeholderText: qsTr("The reason (goes in the check's history)")
            }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: qsTr("Cancel")
                    Layout.fillWidth: true
                    implicitHeight: 52
                    onClicked: w.refunding = null
                }
                Button {
                    objectName: "refundGo"
                    Layout.fillWidth: true
                    implicitHeight: 52
                    highlighted: true
                    readonly property int cents: Math.round(Number(refundAmount.text) * 100)
                    enabled: cents > 0 && w.refunding !== null && cents <= w.refunding.leftCents && refundReason.text.trim() !== ""
                    text: enabled ? qsTr("Refund %1").arg((w.pos ? w.pos.currencySymbol : "") + (cents / 100).toFixed(2))
                                  : qsTr("Refund")
                    onClicked: {
                        w.pos.refundPayment(w.check.id, w.refunding.id, cents, refundReason.text.trim())
                        w.refunding = null
                    }
                }
            }
        }
    }
}
