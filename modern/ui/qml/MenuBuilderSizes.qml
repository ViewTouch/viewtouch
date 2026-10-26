import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Menu Builder: An item's sizes, each with its whole price.
Popup {
    id: sizesDialog
    property Item menu   // the WidgetMenuBuilder (not by its type: that would be a cycle)
    objectName: "builderSizesDialog"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 600, 600)
    modal: true
    padding: 16
    property var item: null
    property var rows: []
    function openFor(i) {
        item = i
        rows = (i.sizes ?? []).length ? i.sizes.map(z => ({ name: z.name, price: z.price })) : []
        if (rows.length === 0) preset([qsTr("Small"), qsTr("Large")])
        open()
    }
    function preset(names) {
        const base = item ? item.priceValue : 0
        rows = names.map((n, k) => ({ name: n, price: (base + k * 1).toFixed(2) }))
    }
    function setRow(k, key, value) {
        const r = JSON.parse(JSON.stringify(rows))
        r[k][key] = value
        rows = r
    }
    contentItem: ColumnLayout {
        spacing: 8
        Label { text: qsTr("Sizes of %1").arg(sizesDialog.item ? sizesDialog.item.name : ""); font.pixelSize: 20; font.bold: true }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.75
            text: qsTr("Each size with its whole price. Ordering it asks for the size; the smallest is its price.")
        }
        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: [[qsTr("Small"), qsTr("Large")], [qsTr("Small"), qsTr("Medium"), qsTr("Large")],
                        [qsTr("Regular"), qsTr("Large")]]
                delegate: MenuTouchButton {
                    required property var modelData
                    implicitHeight: 44
                    font.pixelSize: 14
                    text: modelData.join(" / ")
                    onClicked: sizesDialog.preset(modelData)
                }
            }
        }
        // Counted: typing changes the rows, not how many (the fields stay).
        Repeater {
            model: sizesDialog.rows.length
            delegate: RowLayout {
                id: sizeRow
                required property int index
                readonly property var modelData: sizesDialog.rows[index] ?? ({})
                Layout.fillWidth: true
                spacing: 6
                TextField {
                    objectName: "builderSizeName-" + sizeRow.index
                    Layout.fillWidth: true
                    implicitHeight: 48
                    font.pixelSize: 16
                    text: sizeRow.modelData.name
                    placeholderText: qsTr("e.g. Large")
                    onTextEdited: sizesDialog.setRow(sizeRow.index, "name", text)
                }
                TextField {
                    objectName: "builderSizePrice-" + sizeRow.index
                    Layout.preferredWidth: 110
                    implicitHeight: 48
                    font.pixelSize: 16
                    text: sizeRow.modelData.price
                    placeholderText: "0.00"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    onTextEdited: sizesDialog.setRow(sizeRow.index, "price", text)
                }
                MenuTouchButton {
                    implicitHeight: 48
                    text: "✕"
                    onClicked: sizesDialog.rows = sizesDialog.rows.filter((r, k) => k !== sizeRow.index)
                }
            }
        }
        MenuTouchButton {
            implicitHeight: 44
            font.pixelSize: 14
            text: qsTr("+ Size")
            onClicked: sizesDialog.rows = sizesDialog.rows.concat([{ name: "", price: "" }])
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            MenuTouchButton {
                objectName: "builderSizesSave"
                Layout.fillWidth: true
                highlighted: true
                enabled: sizesDialog.rows.length !== 1
                text: sizesDialog.rows.length ? qsTr("Save Sizes") : qsTr("One Size Only")
                onClicked: {
                    const id = sizesDialog.item.id, rows = sizesDialog.rows
                    sizesDialog.close()
                    menu.syncPrice = id   // its price may change: the open card follows
                    menu.pos.setItemSizes(id, rows)
                }
            }
            MenuTouchButton { Layout.preferredWidth: 140; text: qsTr("Cancel"); onClicked: sizesDialog.close() }
        }
    }
}

// Ready-made pictures: food and drink, drawn for the store.
