import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Menu Builder: an item's sizes and colors (a shop's T-shirt). Up to two
// lists, then each one sold: its barcode, SKU, a price of its own (empty:
// the item's), whether it's sold at all, and (Track stock on) how many are
// here. Ringing the item in shows them as a grid; scanning one's barcode
// rings that one in.
Popup {
    id: dialog
    property PosService pos
    objectName: "builderVariantsDialog"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 900, 900)
    height: Math.min(parent ? parent.height - 32 : 760, 760)
    modal: true
    padding: 16
    property var item: null
    property string firstName: ""
    property string firstValues: ""
    property string secondName: ""
    property string secondValues: ""
    property var rows: []   // [{key, label, barcode, sku, price, hidden, stock, stockWas}]
    readonly property bool tracked: !!item && !!item.stockTrack

    function split(text) { return text.split(/[,\n]+/).map(s => s.trim().replace(/\|/g, "")).filter(s => s !== "") }
    function openFor(i) {
        item = i
        const axes = i.variantAxes ?? []
        firstName = axes.length > 0 ? axes[0].name : qsTr("Size")
        firstValues = axes.length > 0 ? axes[0].values.join(", ") : ""
        secondName = axes.length > 1 ? axes[1].name : qsTr("Color")
        secondValues = axes.length > 1 ? axes[1].values.join(", ") : ""
        rows = (i.variants ?? []).map(c => ({ key: c.key, label: c.label, barcode: c.barcode, sku: c.sku, price: c.priceValue,
                                              hidden: c.hidden, stock: c.stock === undefined || c.stock === null ? "" : String(c.stock),
                                              stockWas: c.stock === undefined || c.stock === null ? "" : String(c.stock) }))
        open()
    }
    // Every size with every color, keeping what was typed for each.
    function makeRows() {
        const a = split(firstValues), b = split(secondValues)
        const keys = []
        for (const x of a) {
            if (b.length === 0)
                keys.push(x)
            for (const y of b)
                keys.push(x + "|" + y)
        }
        rows = keys.map(k => rows.find(r => r.key === k)
                             ?? { key: k, label: k.split("|").join(", "), barcode: "", sku: "", price: "", hidden: false, stock: "", stockWas: "" })
    }
    function setRow(k, key, value) {
        const r = JSON.parse(JSON.stringify(rows))
        r[k][key] = value
        rows = r
    }
    function save() {
        const axes = []
        if (split(firstValues).length)
            axes.push({ name: firstName, values: split(firstValues) })
        if (split(secondValues).length)
            axes.push({ name: secondName, values: split(secondValues) })
        const id = item.id
        pos.setItemVariants(id, axes, rows.map(r => ({ key: r.key, barcode: r.barcode, sku: r.sku, price: r.price, hidden: r.hidden })))
        // How many of each are here (counted: Track stock).
        if (tracked)
            for (const r of rows)
                if (r.stock !== r.stockWas && r.stock !== "" && !isNaN(Number(r.stock)))
                    pos.setVariantStock(id, r.key, Number(r.stock))
        close()
    }

    contentItem: ColumnLayout {
        spacing: 8
        Label {
            textFormat: Text.PlainText
            text: qsTr("Sizes and colors of %1").arg(dialog.item ? dialog.item.name : "")
            font.pixelSize: 20
            font.bold: true
        }
        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.75
            text: qsTr("Type the sizes, then the colors (commas between), and Make the List. Each can have its own barcode and price; ringing it in shows them as a grid.")
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: 6
            rowSpacing: 6
            TextField {
                objectName: "builderAxis0Name"
                Layout.preferredWidth: 140
                implicitHeight: 48
                text: dialog.firstName
                onTextEdited: dialog.firstName = text
            }
            TextField {
                objectName: "builderAxis0Values"
                Layout.fillWidth: true
                implicitHeight: 48
                text: dialog.firstValues
                placeholderText: qsTr("e.g. S, M, L, XL")
                onTextEdited: dialog.firstValues = text
            }
            MenuTouchButton {
                implicitHeight: 48
                font.pixelSize: 14
                text: qsTr("S M L XL")
                onClicked: dialog.firstValues = "S, M, L, XL"
            }
            TextField {
                objectName: "builderAxis1Name"
                Layout.preferredWidth: 140
                implicitHeight: 48
                text: dialog.secondName
                onTextEdited: dialog.secondName = text
            }
            TextField {
                objectName: "builderAxis1Values"
                Layout.fillWidth: true
                implicitHeight: 48
                text: dialog.secondValues
                placeholderText: qsTr("e.g. Black, White, Red (or empty)")
                onTextEdited: dialog.secondValues = text
            }
            MenuTouchButton {
                objectName: "builderMakeVariants"
                implicitHeight: 48
                font.pixelSize: 14
                highlighted: true
                text: qsTr("Make the List")
                onClicked: dialog.makeRows()
            }
        }
        // Each one: sold or not, barcode, SKU, price, on hand.
        RowLayout {
            Layout.fillWidth: true
            visible: dialog.rows.length > 0
            spacing: 6
            Label { textFormat: Text.PlainText; text: qsTr("Sold"); Layout.preferredWidth: 56; opacity: 0.7 }
            Label { textFormat: Text.PlainText; text: ""; Layout.preferredWidth: 120 }
            Label { textFormat: Text.PlainText; text: qsTr("Barcode"); Layout.fillWidth: true; opacity: 0.7 }
            Label { textFormat: Text.PlainText; text: qsTr("SKU"); Layout.preferredWidth: 110; opacity: 0.7 }
            Label { textFormat: Text.PlainText; text: qsTr("Price"); Layout.preferredWidth: 90; opacity: 0.7 }
            Label { textFormat: Text.PlainText; text: qsTr("On hand"); Layout.preferredWidth: 80; opacity: 0.7; visible: dialog.tracked }
        }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: dialog.rows.length
            delegate: RowLayout {
                id: row
                required property int index
                readonly property var modelData: dialog.rows[index] ?? ({})
                width: list.width
                spacing: 6
                Switch {
                    objectName: "builderVariantSold-" + row.index
                    Layout.preferredWidth: 56
                    checked: !row.modelData.hidden
                    onToggled: dialog.setRow(row.index, "hidden", !checked)
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.preferredWidth: 120
                    text: row.modelData.label ?? ""
                    font.bold: true
                    elide: Text.ElideRight
                }
                TextField {
                    objectName: "builderVariantBarcode-" + row.index
                    Layout.fillWidth: true
                    implicitHeight: 44
                    text: row.modelData.barcode ?? ""
                    placeholderText: qsTr("Scan or type")
                    onTextEdited: dialog.setRow(row.index, "barcode", text)
                }
                TextField {
                    Layout.preferredWidth: 110
                    implicitHeight: 44
                    text: row.modelData.sku ?? ""
                    onTextEdited: dialog.setRow(row.index, "sku", text)
                }
                TextField {
                    objectName: "builderVariantPrice-" + row.index
                    Layout.preferredWidth: 90
                    implicitHeight: 44
                    text: row.modelData.price ?? ""
                    placeholderText: dialog.item ? Number(dialog.item.priceValue).toFixed(2) : ""
                    inputMethodHints: Qt.ImhDecimalNumbersOnly
                    onTextEdited: dialog.setRow(row.index, "price", text)
                }
                TextField {
                    objectName: "builderVariantStock-" + row.index
                    visible: dialog.tracked
                    Layout.preferredWidth: 80
                    implicitHeight: 44
                    text: row.modelData.stock ?? ""
                    inputMethodHints: Qt.ImhDigitsOnly
                    onTextEdited: dialog.setRow(row.index, "stock", text)
                }
            }
        }
        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            visible: !dialog.tracked && dialog.rows.length > 0
            wrapMode: Text.WordWrap
            opacity: 0.7
            text: qsTr("Turn on Track stock for the item to count each one.")
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            MenuTouchButton {
                objectName: "builderVariantsSave"
                Layout.fillWidth: true
                highlighted: true
                text: dialog.rows.length ? qsTr("Save Sizes and Colors") : qsTr("No Sizes or Colors")
                onClicked: dialog.save()
            }
            MenuTouchButton { Layout.preferredWidth: 140; text: qsTr("Cancel"); onClicked: dialog.close() }
        }
    }
}
