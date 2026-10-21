import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Dialogs
import QtQuick.Layouts

// Menu Builder: Importing a menu (a spreadsheet or another store's): what comes in, and where.
Dialog {
    id: importDialog
    property Item menu   // the WidgetMenuBuilder (not by its type: that would be a cycle)
    objectName: "builderImportDialog"
    property var read: ({})
    readonly property var items: read.items ?? []
    readonly property var categoriesIn: [...new Set(items.map(i => i.category || (menu.category ? menu.category.name : "")))]
    title: read.error ? qsTr("Can't import that") : qsTr("Import %n item(s)", "", items.length)
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent.width - 40, 820)
    height: Math.min(parent.height - 40, 640)
    modal: true
    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: importDialog.read.error
                  ? importDialog.read.error
                  : importDialog.read.menuFile
                    ? qsTr("%1's menu, with its choices. Items already on this menu are left as they are.").arg(importDialog.read.from || "?")
                    : (importDialog.read.from ? qsTr("From %1's menu export.").arg(importDialog.read.from) + " " : "")
                      + qsTr("Into: %1. Rows without a category go in %2.").arg(importDialog.categoriesIn.join(", "))
                                                                          .arg(menu.category ? menu.category.name : "?")
                      + ((importDialog.read.groups ?? []).length > 0
                         ? " " + qsTr("Choice groups: %1.").arg(importDialog.read.groups.map(g => g.name).join(", ")) : "")
        }
        Label {
            visible: (importDialog.read.problems ?? []).length > 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#f5b940"
            text: qsTr("Left out: %1").arg((importDialog.read.problems ?? []).join("; "))
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: importDialog.items
            delegate: RowLayout {
                required property var modelData
                width: ListView.view.width
                Label { Layout.fillWidth: true; text: (modelData.photoData || modelData.photo ? "📷 " : "") + modelData.name; elide: Text.ElideRight }
                Label {
                    opacity: 0.7
                    visible: text !== ""
                    text: modelData.sizes ? qsTr("%n size(s)", "", modelData.sizes.length)
                                          : modelData.groups ? qsTr("%n choice group(s)", "", modelData.groups.length) : ""
                }
                Label { text: (modelData.sizes ? qsTr("from") + " " : "") + menu.pos.currencySymbol + Number(modelData.price).toFixed(2) }
                Label { Layout.preferredWidth: 180; text: modelData.category; opacity: 0.7; elide: Text.ElideRight }
            }
        }
        RowLayout {
            visible: !importDialog.read.menuFile
            Layout.fillWidth: true
            Switch { id: updatePrices; objectName: "builderImportPrices" }
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Items already on the menu get the file's price (otherwise they're left as they are)") }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            MenuTouchButton {
                objectName: "builderImportGo"
                Layout.fillWidth: true
                highlighted: true
                enabled: importDialog.items.length > 0
                text: qsTr("Import")
                onClicked: {
                    if (importDialog.read.menuFile)
                        menu.pos.importMenuFile(importDialog.read.menuFile)
                    else
                        menu.pos.importMenuRows(importDialog.items, menu.categoryId, updatePrices.checked, importDialog.read.groups ?? [])
                    importDialog.close()
                }
            }
            MenuTouchButton { Layout.preferredWidth: 160; text: qsTr("Cancel"); onClicked: importDialog.close() }
        }
    }
}

// The menu checked: touch a problem to fix it.
