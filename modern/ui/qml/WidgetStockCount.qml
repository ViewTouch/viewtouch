import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Counting stock: how many of each item are on the shelves (by vendor, or
// all). Only items typed are counted; Save Count makes what was counted what's
// on hand, and the Stock Count report shows how far off it was from what the
// sales and deliveries say should be there.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var info: pos ? pos.stockCounting : ({})
    readonly property bool showExpected: info.showExpected === true
    property string vendor: ""
    property var counted: ({})   // ingredient id -> how many, as typed
    property int typed: 0        // bumps when it changes
    readonly property var shown: (info.ingredients ?? []).filter(i => vendor === "" || i.vendor === vendor)
    readonly property int howMany: typed >= 0 ? Object.keys(counted).filter(k => String(counted[k]).trim() !== "").length : 0
    readonly property int recentCount: (info.recent ?? []).length
    onRecentCountChanged: clear()   // saved: start over

    function clear() {
        counted = ({})
        ++typed
    }
    function save() {
        const lines = []
        for (const id of Object.keys(counted))
            if (String(counted[id]).trim() !== "")
                lines.push({ ingredient: id, counted: String(counted[id]) })
        pos.saveStockCount({ lines: lines })
    }

    // Upright (a phone): the count only.
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real zoom: zone ? zone.formZoom(narrow ? 460 : 900) : 1.4
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                Flow {   // which shelf: by vendor
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: [{ id: "", name: qsTr("All items") }].concat(w.info.vendors ?? [])
                        delegate: PosButton {
                            required property var modelData
                            objectName: "countVendor-" + modelData.id
                            text: modelData.name
                            checkable: true
                            checked: w.vendor === modelData.id
                            highlighted: checked
                            implicitHeight: 44
                            font.pixelSize: 15
                            onClicked: w.vendor = modelData.id
                        }
                    }
                }
                RowLayout {   // headings
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    Label { text: qsTr("Item"); opacity: 0.6; Layout.fillWidth: true }
                    Label { visible: w.showExpected; text: qsTr("Should be"); opacity: 0.6; Layout.preferredWidth: 110 }
                    Label { text: qsTr("Counted"); opacity: 0.6; Layout.preferredWidth: 140 }
                }
                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: w.shown
                    ScrollBar.vertical: TouchScrollBar { id: listBar; needed: list.contentHeight > list.height + 1 }
                    delegate: RowLayout {
                        id: row
                        required property var modelData
                        width: ListView.view.width - listBar.room
                        height: 52
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label {
                                Layout.fillWidth: true
                                text: row.modelData.name
                                font.pixelSize: 16
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Last counted %1").arg(row.modelData.lastCounted)
                                font.pixelSize: 12
                                opacity: 0.55
                                elide: Text.ElideRight
                            }
                        }
                        Label {
                            visible: w.showExpected
                            Layout.preferredWidth: 110
                            text: (row.modelData.expected ?? "") + " " + row.modelData.unit
                            opacity: 0.8
                        }
                        TextField {
                            objectName: "counted-" + row.modelData.id
                            Layout.preferredWidth: 140
                            implicitHeight: 46
                            font.pixelSize: 17
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            text: w.typed >= 0 ? (w.counted[row.modelData.id] ?? "") : ""
                            placeholderText: row.modelData.unit
                            onTextEdited: { w.counted[row.modelData.id] = text; ++w.typed }
                        }
                    }
                }
                Label {
                    visible: w.shown.length === 0
                    text: (w.info.ingredients ?? []).length === 0 ? qsTr("Add ingredients in Manager → Inventory first.")
                                                                  : qsTr("Nothing comes from this vendor yet (set it on each item in Inventory).")
                    opacity: 0.6
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Only the items you type are counted; leave the rest empty.")
                    opacity: 0.6
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    spacing: 10
                    Label {
                        objectName: "countedHowMany"
                        text: qsTr("%1 of %2 counted").arg(w.howMany).arg((w.info.ingredients ?? []).length)
                        font.pixelSize: 18
                        font.bold: true
                        Layout.fillWidth: true
                    }
                    PosButton {
                        text: qsTr("Clear")
                        implicitHeight: 52
                        font.pixelSize: 16
                        onClicked: w.clear()
                    }
                    PosButton {
                        objectName: "saveStockCount"
                        text: qsTr("Save Count")
                        highlighted: true
                        enabled: w.howMany > 0
                        implicitHeight: 52
                        implicitWidth: 200
                        font.pixelSize: 17
                        onClicked: w.save()
                    }
                }
            }

            ToolSeparator { Layout.fillHeight: true; visible: !w.narrow }

            ColumnLayout {
                visible: !w.narrow
                Layout.preferredWidth: parent.width * 0.3
                Layout.fillWidth: false
                Layout.fillHeight: true
                spacing: 6
                Label { text: qsTr("Counted lately"); font.bold: true; font.pixelSize: 17 }
                ListView {
                    id: recent
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: w.info.recent ?? []
                    ScrollBar.vertical: TouchScrollBar { id: recentBar; needed: recent.contentHeight > recent.height + 1 }
                    delegate: ColumnLayout {
                        required property var modelData
                        width: ListView.view.width - recentBar.room
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: modelData.when + "  ·  " + modelData.value
                            color: modelData.short ? "#ff9a9e" : "white"
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("%n item(s)", "", modelData.items) + "  ·  " + modelData.by
                            opacity: 0.6
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }
                }
                Label {
                    visible: (w.info.recent ?? []).length === 0
                    text: qsTr("Nothing counted yet.")
                    opacity: 0.6
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Reports → Stock Count shows each item against what should be there.")
                    opacity: 0.6
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
