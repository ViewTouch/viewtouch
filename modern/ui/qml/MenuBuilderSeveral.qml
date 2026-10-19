import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Dialogs
import QtQuick.Layouts

// Menu Builder: Adding several items at once, with their prices.
Dialog {
    id: severalDialog
    property Item menu   // the WidgetMenuBuilder (not by its type: that would be a cycle)
    objectName: "builderSeveral"
    title: qsTr("Add several to %1").arg(menu.category ? menu.category.name : "")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent.width - 40, 760)
    modal: true
    onOpened: severalText.text = ""
    // What will be added (the store reads it the same way). Callbacks, not
    // loops that change their own variables: Qt 6.10's compiler ran out of
    // memory on those (the Ubuntu 26.04 package build was killed).
    function parse(text, start) {
        const out = []
        let category = start
        text.split("\n").map(l => l.trim()).filter(l => l !== "").forEach(l => {
            const colon = l.indexOf(":")
            const named = colon > 0 && !/\d/.test(l.slice(0, colon))
            if (named)
                category = l.slice(0, colon).trim()
            const cat = category
            const rest = named ? l.slice(colon + 1) : l
            rest.split(/,(?!\d{1,2}\s*(?:,|$))/).map(p => p.trim()).filter(p => p !== "").forEach(p => {
                const m = p.match(/^(.*?)[\s\-–:]*\$?\s*(\d+(?:[.,]\d{1,2})?)\s*$/)
                out.push(m && m[1].trim() ? { name: m[1].trim(), price: Number(m[2].replace(",", ".")).toFixed(2), category: cat }
                                          : { name: p, price: "", category: cat })
            })
        })
        return out
    }
    readonly property var preview: parse(severalText.text, menu.category ? menu.category.name : "")
    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.8
            text: qsTr("One per line, or separated by commas, each with its price: \"Carne Asada 3.50\". Start a line with another category's name and a colon to put what follows there (made if it's new): \"Drinks: Horchata 2.75\".")
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            TextArea {
                id: severalText
                objectName: "builderSeveralText"
                placeholderText: qsTr("Carne Asada 3.50\nAl Pastor 3.25\nDrinks: Horchata 2.75")
                wrapMode: TextEdit.Wrap
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            visible: severalDialog.preview.length > 0
            text: severalDialog.preview.map(e => e.price ? e.name + " " + menu.pos.currencySymbol + e.price + (e.category !== (menu.category ? menu.category.name : "") ? " (" + e.category + ")" : "")
                                                         : "⚠ " + e.name + " " + qsTr("(no price)")).join("  ·  ")
            font.pixelSize: 14
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            MenuTouchButton {
                objectName: "builderSeveralAdd"
                Layout.fillWidth: true
                highlighted: true
                enabled: severalDialog.preview.length > 0 && severalDialog.preview.every(e => e.price !== "")
                text: qsTr("Add %n item(s)", "", severalDialog.preview.length)
                onClicked: {
                    menu.pos.addMenuItemsFromText(menu.categoryId, severalText.text)
                    severalDialog.close()
                }
            }
            MenuTouchButton {
                Layout.preferredWidth: 160
                text: qsTr("Cancel")
                onClicked: severalDialog.close()
            }
        }
    }
}

// Prices up or down, a category's or the whole menu's: shown first, then
// changed (one Undo).
