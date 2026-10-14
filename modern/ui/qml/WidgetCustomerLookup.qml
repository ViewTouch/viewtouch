import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Customers: find one by phone or name, see their visits and house account,
// edit them, put them on the open check, take a payment on their account.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var results: pos ? pos.customers : []
    readonly property var chosen: pos ? pos.customer : ({})
    readonly property bool manager: pos !== null && pos.permissions.indexOf("manager") >= 0
    property var draft: ({})
    property bool creating: false

    function reload() {
        draft = {
            id: chosen.id ?? "", name: chosen.name ?? "", phone: chosen.phone ?? "", email: chosen.email ?? "",
            address: chosen.address ?? "", note: chosen.note ?? "", houseAccount: chosen.houseAccount ?? false,
            limit: chosen.accountLimitCents ? (chosen.accountLimitCents / 100).toFixed(2) : ""
        }
    }
    onChosenChanged: if (!creating || chosen.id) { creating = false; reload() }
    Component.onCompleted: { if (pos) pos.findCustomers(""); reload() }

    function setField(key, value) {
        const d = Object.assign({}, draft)
        d[key] = value
        draft = d
    }
    function cents(text) {
        const v = Number(String(text).replace(/[^0-9.]/g, ""))
        return isNaN(v) ? 0 : Math.round(v * 100)
    }
    function save() {
        const r = { id: draft.id, name: draft.name, phone: draft.phone, email: draft.email,
                    address: draft.address, note: draft.note }
        if (manager) {
            r.houseAccount = draft.houseAccount
            r.accountLimit = cents(draft.limit)
        }
        pos.saveCustomer(r)
        creating = false
    }

    Timer { id: searchTimer; interval: 250; onTriggered: w.pos.findCustomers(search.text) }

    // Upright (a phone): the search, then the customer on their own, with ‹ Search.
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property bool showing: creating || (chosen.id ?? "") !== ""
    readonly property real zoom: zone ? zone.formZoom(narrow ? 340 : 688) : 1
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 14

            // --- find ---
            ColumnLayout {
                visible: !w.narrow || !w.showing
                Layout.preferredWidth: w.narrow ? parent.width : parent.width * 0.4
                Layout.fillWidth: w.narrow
                Layout.fillHeight: true
                spacing: 8
                TextField {
                    id: search
                    Layout.fillWidth: true
                    placeholderText: qsTr("Phone or name…")
                    font.pixelSize: 18
                    onTextEdited: searchTimer.restart()
                }
                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: w.results
                    ScrollBar.vertical: TouchScrollBar { id: listBar }
                    delegate: ItemDelegate {
                        id: row
                        required property var modelData
                        width: ListView.view.width - listBar.room
                        height: 62
                        highlighted: row.modelData.id === (w.chosen.id ?? "")
                        onClicked: w.pos.selectCustomer(row.modelData.id)
                        contentItem: ColumnLayout {
                            spacing: 0
                            Label {
                                text: row.modelData.name || row.modelData.phone
                                font.bold: true
                                font.pixelSize: 17
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                opacity: 0.75
                                text: [row.modelData.phone,
                                       (row.modelData.visits === 1 ? qsTr("1 visit") : qsTr("%1 visits").arg(row.modelData.visits)),
                                       row.modelData.balanceCents > 0 ? qsTr("owes %1").arg(row.modelData.balance) : ""]
                                      .filter(s => s).join("  ·  ")
                            }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: list.count === 0
                        text: search.text ? qsTr("No one matches.") : qsTr("No customers yet.")
                        opacity: 0.6
                    }
                }
                Button {
                    Layout.fillWidth: true
                    implicitHeight: 48
                    text: qsTr("+ New Customer")
                    onClicked: {
                        w.creating = true
                        w.pos.selectCustomer("")
                        // A number typed in the search box is probably theirs.
                        const digits = search.text.replace(/[^0-9]/g, "")
                        w.draft = { id: "", name: digits.length >= 7 ? "" : search.text,
                                    phone: digits.length >= 7 ? search.text : "", email: "", address: "",
                                    note: "", houseAccount: false, limit: "" }
                    }
                }
            }

            ToolSeparator { Layout.fillHeight: true; visible: !w.narrow }

            // --- the customer ---
            Flickable {
                id: form
                visible: !w.narrow || w.showing
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentHeight: details.implicitHeight
                clip: true
                ScrollBar.vertical: TouchScrollBar { id: formBar }

                ColumnLayout {
                    id: details
                    width: form.width - formBar.room
                    spacing: 8
                    visible: w.creating || (w.chosen.id ?? "") !== ""

                    Button {
                        visible: w.narrow
                        text: qsTr("‹ Search")
                        onClicked: { w.creating = false; w.pos.selectCustomer("") }
                    }
                    Label {
                        text: w.creating ? qsTr("New customer") : (w.chosen.name || w.chosen.phone || "")
                        font.bold: true
                        font.pixelSize: 22
                    }
                    // Loyalty: points, and the rewards they can spend on this check.
                    RowLayout {
                        visible: !w.creating && (w.pos ? (w.pos.customerPrompt.loyalty ?? {}).enabled ?? false : false)
                        spacing: 8
                        Label {
                            text: qsTr("%1 points").arg(w.chosen.points ?? 0)
                            font.pixelSize: 18
                            font.bold: true
                            color: "#f5b940"
                        }
                        Repeater {
                            model: w.pos && (w.chosen.onCheck ?? false) ? ((w.pos.customerPrompt.loyalty ?? {}).rewards ?? []) : []
                            delegate: Button {
                                required property var modelData
                                text: qsTr("%1 off (%2 pts)").arg(modelData.value).arg(modelData.points)
                                enabled: modelData.ready
                                onClicked: w.pos.redeemReward(modelData.index)
                            }
                        }
                        Label {
                            visible: !(w.chosen.onCheck ?? false)
                            text: qsTr("Put them on the check to use rewards")
                            opacity: 0.6
                        }
                    }
                    Label {
                        visible: !w.creating
                        opacity: 0.75
                        text: ((w.chosen.visits ?? 0) === 1 ? qsTr("1 visit") : qsTr("%1 visits").arg(w.chosen.visits ?? 0)) + "  ·  " + qsTr("spent %1").arg(w.chosen.spent ?? "")
                              + (w.chosen.lastVisit ? "  ·  " + qsTr("last %1").arg(w.chosen.lastVisit) : "")
                    }

                    component Entry: ColumnLayout {
                        id: entry
                        property alias label: caption.text
                        property string key
                        property int hints: Qt.ImhNone
                        Layout.fillWidth: true
                        spacing: 2
                        Label { id: caption; opacity: 0.8 }
                        TextField {
                            Layout.fillWidth: true
                            text: w.draft[entry.key] ?? ""
                            inputMethodHints: entry.hints
                            onTextEdited: w.setField(entry.key, text)
                        }
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 4
                        Entry { label: qsTr("Name"); key: "name" }
                        Entry { label: qsTr("Phone"); key: "phone"; hints: Qt.ImhDialableCharactersOnly }
                        Entry { label: qsTr("Email"); key: "email"; hints: Qt.ImhEmailCharactersOnly }
                        Entry { label: qsTr("Note (allergies, gate code…)"); key: "note" }
                    }
                    Entry { label: qsTr("Address (delivery)"); key: "address" }

                    // House account: managers open it and set the limit.
                    Frame {
                        Layout.fillWidth: true
                        visible: !w.creating || w.manager
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 6
                            RowLayout {
                                CheckBox {
                                    text: qsTr("House account")
                                    checked: w.draft.houseAccount ?? false
                                    enabled: w.manager
                                    onToggled: w.setField("houseAccount", checked)
                                }
                                Label { text: qsTr("Limit $"); visible: w.draft.houseAccount ?? false }
                                TextField {
                                    visible: w.draft.houseAccount ?? false
                                    enabled: w.manager
                                    implicitWidth: 120
                                    placeholderText: qsTr("none")
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    text: w.draft.limit ?? ""
                                    onTextEdited: w.setField("limit", text)
                                }
                                Item { Layout.fillWidth: true }
                                Label {
                                    visible: w.chosen.houseAccount ?? false
                                    text: qsTr("Owes %1").arg(w.chosen.balance ?? "")
                                    font.bold: true
                                    font.pixelSize: 18
                                    color: (w.chosen.balanceCents ?? 0) > 0 ? "#f5b940" : "#7ee2a8"
                                }
                            }
                            RowLayout {
                                visible: (w.chosen.balanceCents ?? 0) > 0
                                Label { text: qsTr("Payment $") }
                                TextField {
                                    id: payAmount
                                    implicitWidth: 120
                                    placeholderText: qsTr("all")
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                }
                                Button {
                                    text: qsTr("Paid Cash")
                                    onClicked: { w.pos.payOnAccount("cash", w.cents(payAmount.text)); payAmount.text = "" }
                                }
                                Button {
                                    text: qsTr("Paid Card")
                                    onClicked: { w.pos.payOnAccount("card", w.cents(payAmount.text)); payAmount.text = "" }
                                }
                            }
                            Repeater {
                                model: w.chosen.account ?? []
                                delegate: RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Label { text: modelData.at; opacity: 0.6; Layout.preferredWidth: 140 }
                                    Label { text: modelData.what; Layout.fillWidth: true; elide: Text.ElideRight }
                                    Label { text: modelData.amount }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        spacing: 10
                        Button {
                            text: qsTr("Save")
                            implicitHeight: 52
                            implicitWidth: 140
                            highlighted: true
                            onClicked: w.save()
                        }
                        // Privacy laws: a guest may ask to be forgotten (managers; touch twice).
                        Button {
                            id: forget
                            property bool armed: false
                            visible: !w.creating && w.pos !== null && w.pos.can("manager")
                            text: armed ? qsTr("Touch again to forget them") : qsTr("Forget This Customer…")
                            implicitHeight: 52
                            palette.button: armed ? "#8b2c2c" : palette.button
                            onClicked: {
                                if (!armed) { armed = true; disarm.restart(); return }
                                armed = false
                                w.pos.forgetCustomer(w.chosen.id)
                            }
                            Timer { id: disarm; interval: 4000; onTriggered: forget.armed = false }
                        }
                        Button {
                            text: (w.chosen.onCheck ?? false) ? qsTr("On this check ✓") : qsTr("Put on This Check")
                            implicitHeight: 52
                            enabled: w.pos !== null && w.pos.hasCheck && !w.creating && !(w.chosen.onCheck ?? false)
                            onClicked: w.pos.useCustomer(w.chosen.id)
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: !details.visible
                    text: qsTr("Find a customer on the left, or add a new one.")
                    opacity: 0.6
                }
            }
        }
    }
}
