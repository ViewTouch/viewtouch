import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Who a takeout or delivery order is for: name, phone, address, note.
// Saved to the current check; printed on its receipt and kitchen tickets.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var check: pos ? pos.check : ({})
    readonly property var saved: check.customer ?? ({})
    property var draft: ({})
    property bool dirty: false

    // Details save as you type (and when the page is left), so "Continue"
    // never loses them.
    function commit() {
        saveTimer.stop()
        if (dirty && pos && pos.hasCheck)
            pos.setCustomer(draft)
        dirty = false
    }
    Timer { id: saveTimer; interval: 700; onTriggered: w.commit() }

    // What was last typed in name / phone, to look up regulars.
    property string searching: ""
    function suggest(text) {
        searching = text.trim().length >= 3 ? text.trim() : ""
        if (searching !== "")
            pos.findCustomers(searching)
    }
    Component.onDestruction: commit()

    function reload() {
        draft = { name: saved.name ?? "", phone: saved.phone ?? "", address: saved.address ?? "", note: saved.note ?? "" }
        dirty = false
    }
    Component.onCompleted: reload()
    onSavedChanged: if (!dirty) reload()

    // Controls are laid out at a comfortable size, then scaled with the page.
    readonly property real zoom: zone ? zone.formZoom(zone.narrow ? 380 : 560) : 1.6
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            Label {
                text: w.pos && w.pos.hasCheck ? w.check.label : qsTr("No check open")
                font.bold: true
                font.pixelSize: 20
            }

            component Entry: ColumnLayout {
                id: entry
                property alias label: caption.text
                property string key
                property bool multiline: false
                property int hints: Qt.ImhNone
                signal edited(string text)
                function edit(text) {
                    const d = Object.assign({}, w.draft)
                    d[entry.key] = text
                    w.draft = d
                    w.dirty = true
                    saveTimer.restart()
                }
                Layout.fillWidth: true
                spacing: 2
                Label { id: caption; opacity: 0.8 }
                TextField {
                    visible: !entry.multiline
                    Layout.fillWidth: true
                    text: w.draft[entry.key] ?? ""
                    enabled: w.pos !== null && w.pos.hasCheck
                    inputMethodHints: entry.hints
                    onTextEdited: { entry.edit(text); entry.edited(text) }
                }
                TextArea {
                    visible: entry.multiline
                    Layout.fillWidth: true
                    text: w.draft[entry.key] ?? ""
                    enabled: w.pos !== null && w.pos.hasCheck
                    wrapMode: TextEdit.Wrap
                    onTextChanged: if (activeFocus) entry.edit(text)
                }
            }

            Entry { label: qsTr("Name"); key: "name"; onEdited: text => w.suggest(text) }
            Entry { label: qsTr("Phone"); key: "phone"; hints: Qt.ImhDialableCharactersOnly; onEdited: text => w.suggest(text) }

            // Regulars matching what is typed: one touch fills the rest.
            Flow {
                Layout.fillWidth: true
                spacing: 6
                visible: w.searching !== ""
                Repeater {
                    model: w.searching !== "" && w.pos ? w.pos.customers.slice(0, 4) : []
                    delegate: Button {
                        required property var modelData
                        text: (modelData.name || "") + "  " + (modelData.phone || "")
                              + (modelData.visits ? "  ·  " + (modelData.visits === 1 ? qsTr("1 visit") : qsTr("%1 visits").arg(modelData.visits)) : "")
                        onClicked: {
                            w.searching = ""
                            w.dirty = false
                            saveTimer.stop()
                            w.pos.useCustomer(modelData.id)
                        }
                    }
                }
            }
            Entry { label: qsTr("Address (delivery)"); key: "address"; multiline: true }
            Entry { label: qsTr("Note for the kitchen / driver"); key: "note" }

            Item { Layout.fillHeight: true }

            Label {
                text: w.dirty ? qsTr("Saving…") : qsTr("Saved to the check")
                opacity: 0.7
            }
        }
    }
}
