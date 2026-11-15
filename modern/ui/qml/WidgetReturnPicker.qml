import QtQuick
import QtQuick.Layouts

// Returns: the sale items come back from. Scan the receipt's barcode (or
// type its sale number, or Find a Check → Return Items…), then how many of
// each come back and why; Damaged keeps them off the shelf. No Receipt…: an
// item at today's price (a manager's). They go on the check being rung, below
// zero: Pay gives the money back (cash, the card, or store credit), and new
// items on the same check make it an exchange.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string face: zone.st.font ?? Ui.font
    readonly property color ink: zone.st.textColor ?? "white"
    readonly property real unit: Math.max(16, Math.min(w.width / 30, 40)) * (zone ? zone.textScale : 1)
    readonly property real tapH: w.zone ? w.zone.touch(64) : 48

    readonly property var sale: pos ? pos.returnSale : ({})
    readonly property bool haveSale: !!sale.id && !sale.loading
    // How many of each line come back ({lineId: n}), why, and damaged.
    property var picked: ({})
    property string reason: ""
    property bool damaged: false
    property bool noReceipt: false
    property string typed: ""
    // Asked for (Bring Back, an item with no receipt): back on the sale once it's there.
    property int linesBefore: -1
    onSaleChanged: if (!haveSale) picked = ({})

    readonly property var reasons: [qsTr("Changed mind"), qsTr("Wrong size"), qsTr("Damaged"), qsTr("Didn't work"), qsTr("Gift")]
    readonly property int pickedCount: Object.values(picked).reduce((a, b) => a + b, 0)

    function setPicked(id, n) {
        const p = Object.assign({}, picked)
        p[id] = n
        picked = p
    }
    function bringBack() {
        const picks = []
        for (const id in picked)
            if (picked[id] > 0)
                picks.push({ line: Number(id), qty: picked[id], restock: !w.damaged })
        w.linesBefore = w.returnCount()
        w.pos.returnLines(picks, w.reason)
    }
    function returnCount() { return (w.pos && w.pos.hasCheck ? w.pos.lines : []).filter(l => l.quantity < 0).length }
    // Back on the sale once the items are on it (a manager's OK may come first).
    Connections {
        target: w.pos
        function onCheckChanged() {
            if (w.linesBefore >= 0 && w.returnCount() > w.linesBefore) {
                w.linesBefore = -1
                w.picked = ({})
                w.noReceipt = false
                w.reason = ""
                w.damaged = false
                w.zone.controller.goToOrder()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: w.unit * 0.4
        spacing: w.unit * 0.4

        // --- which sale ---------------------------------------------------------------
        Text {
            textFormat: Text.PlainText
            objectName: "returnTitle"
            Layout.fillWidth: true
            text: w.haveSale ? qsTr("Sale %1 · %2 · %3").arg(w.sale.label || ("#" + w.sale.id)).arg(w.sale.when).arg(w.sale.total)
                 : w.sale.loading ? qsTr("Looking for sale #%1…").arg(w.sale.id)
                 : w.noReceipt ? qsTr("No receipt: touch the item coming back")
                 : qsTr("Scan the receipt's barcode, or type the sale number")
            color: w.ink
            font.family: w.face
            font.pixelSize: w.unit * 1.1
            font.bold: true
            elide: Text.ElideRight
        }
        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            visible: w.haveSale && w.sale.tooOld === true
            text: qsTr("Sold %1 days ago: past the store's days for returns, so a manager's.").arg(w.sale.days)
            color: "#f5b940"
            font.family: w.face
            font.pixelSize: w.unit * 0.8
            wrapMode: Text.WordWrap
        }

        // The sale number, typed (no barcode to scan).
        RowLayout {
            visible: !w.haveSale && !w.noReceipt && !w.sale.loading
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: w.unit * 0.6
            GridLayout {
                Layout.preferredWidth: Math.min(w.width * 0.5, w.unit * 16)
                Layout.fillHeight: true
                columns: 3
                rowSpacing: 6
                columnSpacing: 6
                Text {
                    textFormat: Text.PlainText
                    objectName: "returnTyped"
                    Layout.columnSpan: 3
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignRight
                    text: w.typed === "" ? qsTr("Sale #") : "#" + w.typed
                    color: w.typed === "" ? Ui.muted : w.ink
                    font.family: w.face
                    font.pixelSize: w.unit * 1.5
                    font.bold: true
                }
                Repeater {
                    model: ["7", "8", "9", "4", "5", "6", "1", "2", "3", "C", "0", "OK"]
                    WidgetKey {
                        required property string modelData
                        objectName: "returnPad-" + modelData
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: w.tapH
                        text: modelData === "OK" ? qsTr("Find") : modelData
                        baseColor: modelData === "OK" ? Ui.go : Ui.key
                        fontScale: 0.4
                        onClicked: {
                            if (modelData === "C")
                                w.typed = ""
                            else if (modelData === "OK") {
                                if (w.typed !== "")
                                    w.pos.startReturn(Number(w.typed))
                                w.typed = ""
                            } else if (w.typed.length < 12)
                                w.typed += modelData
                        }
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: w.unit * 0.4
                WidgetKey {
                    objectName: "returnFindCheck"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(w.unit * 2.6, w.tapH)
                    text: qsTr("Find a Check…")
                    fontScale: 0.4
                    onClicked: w.zone.controller.jumpTo("find-check")
                }
                WidgetKey {
                    objectName: "returnNoReceipt"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(w.unit * 2.6, w.tapH)
                    text: qsTr("No Receipt…")
                    fontScale: 0.4
                    onClicked: { w.noReceipt = true; w.reason = "" }
                }
                Text {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: qsTr("Receipts have a barcode at the bottom: scan it here.")
                    color: Ui.muted
                    font.family: w.face
                    font.pixelSize: w.unit * 0.8
                    wrapMode: Text.WordWrap
                }
            }
        }

        // --- the sale's items: how many come back -------------------------------------
        ListView {
            id: list
            objectName: "returnLines"
            visible: w.haveSale
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: w.unit * 0.25
            boundsBehavior: Flickable.StopAtBounds
            model: w.haveSale ? w.sale.lines : []
            delegate: Rectangle {
                id: row
                required property var modelData
                readonly property int n: w.picked[modelData.id] ?? 0
                objectName: "returnLine-" + modelData.id
                width: list.width
                height: Math.max(w.unit * 2.6, w.tapH)
                radius: 8
                color: row.n > 0 ? "#3a2a2a" : Ui.panel
                opacity: modelData.left > 0 ? 1 : 0.5
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: w.unit * 0.6
                    anchors.rightMargin: w.unit * 0.3
                    spacing: w.unit * 0.4
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: row.modelData.name
                        color: w.ink
                        font.family: w.face
                        font.pixelSize: w.unit * 0.95
                        elide: Text.ElideRight
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: row.modelData.left > 0 ? qsTr("%1 each · %2 can come back").arg(row.modelData.price).arg(row.modelData.left)
                                                      : qsTr("all back already")
                        color: Ui.muted
                        font.family: w.face
                        font.pixelSize: w.unit * 0.75
                    }
                    WidgetKey {
                        objectName: "returnLess-" + row.modelData.id
                        visible: row.modelData.left > 0
                        Layout.preferredWidth: Math.max(w.unit * 2.4, w.tapH)
                        Layout.fillHeight: true
                        Layout.topMargin: 4
                        Layout.bottomMargin: 4
                        text: "−"
                        fontScale: 0.5
                        onClicked: w.setPicked(row.modelData.id, Math.max(0, row.n - 1))
                    }
                    Text {
                        textFormat: Text.PlainText
                        objectName: "returnCount-" + row.modelData.id
                        visible: row.modelData.left > 0
                        text: row.n
                        color: row.n > 0 ? "#ff8a80" : w.ink
                        font.family: w.face
                        font.pixelSize: w.unit * 1.2
                        font.bold: true
                        Layout.preferredWidth: w.unit * 1.6
                        horizontalAlignment: Text.AlignHCenter
                    }
                    WidgetKey {
                        objectName: "returnMore-" + row.modelData.id
                        visible: row.modelData.left > 0
                        Layout.preferredWidth: Math.max(w.unit * 2.4, w.tapH)
                        Layout.fillHeight: true
                        Layout.topMargin: 4
                        Layout.bottomMargin: 4
                        text: "+"
                        fontScale: 0.5
                        onClicked: w.setPicked(row.modelData.id, Math.min(row.modelData.left, row.n + 1))
                    }
                }
            }
        }

        // --- no receipt: the item -----------------------------------------------------
        GridView {
            id: items
            objectName: "returnItems"
            visible: w.noReceipt
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            cellWidth: width / Math.max(1, Math.floor(width / (w.unit * 8)))
            cellHeight: Math.max(w.unit * 2.6, w.tapH + 8)
            model: w.noReceipt && w.pos ? w.pos.menuItems.filter(m => !m.modifier && !m.byWeight) : []
            delegate: Item {
                id: cell
                required property var modelData
                width: items.cellWidth
                height: items.cellHeight
                WidgetKey {
                    objectName: "returnItem-" + cell.modelData.id
                    anchors.fill: parent
                    anchors.margins: 4
                    enabled: w.reason !== ""
                    opacity: enabled ? 1 : 0.5
                    text: cell.modelData.name + "\n" + cell.modelData.price
                    fontScale: 0.3
                    onClicked: { w.linesBefore = w.returnCount(); w.pos.returnItem(cell.modelData.id, 0, w.reason, !w.damaged) }
                }
            }
        }

        // --- why ----------------------------------------------------------------------
        RowLayout {
            visible: w.haveSale || w.noReceipt
            Layout.fillWidth: true
            spacing: w.unit * 0.3
            Repeater {
                model: w.reasons
                WidgetKey {
                    required property string modelData
                    required property int index
                    objectName: "returnReason-" + index
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.max(w.unit * 2.2, w.tapH)
                    text: modelData
                    baseColor: w.reason === modelData ? Ui.accent : Ui.key
                    fontScale: 0.32
                    onClicked: { w.reason = modelData; if (index === 2) w.damaged = true }
                }
            }
            WidgetKey {
                objectName: "returnDamaged"
                Layout.preferredWidth: w.unit * 8
                Layout.preferredHeight: Math.max(w.unit * 2.2, w.tapH)
                text: w.damaged ? qsTr("☑ Damaged: not back in stock") : qsTr("☐ Damaged: not back in stock")
                baseColor: w.damaged ? "#8a4b12" : Ui.key
                fontScale: 0.28
                onClicked: w.damaged = !w.damaged
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: w.unit * 0.4
            WidgetKey {
                objectName: "returnCancel"
                Layout.preferredWidth: w.unit * 8
                Layout.preferredHeight: Math.max(w.unit * 2.6, w.tapH)
                text: w.haveSale || w.noReceipt || w.sale.loading ? qsTr("Start Over") : qsTr("‹ Back")
                fontScale: 0.4
                onClicked: {
                    if (w.haveSale || w.noReceipt || w.sale.loading) {
                        w.pos.cancelReturn()
                        w.noReceipt = false
                        w.picked = ({})
                        w.reason = ""
                        w.damaged = false
                    } else {
                        w.zone.controller.goBack()
                    }
                }
            }
            Item { Layout.fillWidth: true }
            WidgetKey {
                objectName: "returnBringBack"
                visible: w.haveSale
                enabled: w.pickedCount > 0 && w.reason !== ""
                opacity: enabled ? 1 : 0.5
                Layout.preferredWidth: w.unit * 12
                Layout.preferredHeight: Math.max(w.unit * 2.6, w.tapH)
                text: w.pickedCount > 0 ? qsTr("Bring Back %1").arg(w.pickedCount) : qsTr("Bring Back")
                baseColor: Ui.go
                fontScale: 0.4
                onClicked: w.bringBack()
            }
        }
    }
}
