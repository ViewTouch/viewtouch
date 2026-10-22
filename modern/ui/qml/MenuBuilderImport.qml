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
    readonly property var categoriesIn: [...new Set(items.map(i => i.category || into.editText))]
    // A spreadsheet's rows can be ticked, a few at a time into one category
    // and then the next (Toast's item list has no categories); a ViewTouch menu
    // file comes in whole.
    readonly property bool picking: !read.menuFile && items.length > 0
    readonly property var menuNames: {
        const names = {}
        for (const i of menu.allItems) names[i.name.toLowerCase()] = true
        return names
    }
    function isOn(name) { return menuNames[name.toLowerCase()] === true }
    property var ticks: ({})          // row index -> ticked, where changed by hand
    property string search: ""
    property var sent: ({})           // row index -> imported (before the menu shows it)
    property string added: ""         // what the last import did, the dialog still open
    function ticked(i) { return sent[i] !== true && (ticks[i] ?? !isOn(items[i].name)) }
    function tick(list, on) {
        const t = Object.assign({}, ticks)
        for (const i of list) t[i] = on
        ticks = t
    }
    readonly property var shown: items.map((it, i) => i).filter(i => {
        const it = items[i]
        if (sent[i] === true || (!showAll.checked && isOn(it.name) && ticks[i] === undefined))
            return false
        const q = search.trim().toLowerCase()
        return q === "" || it.name.toLowerCase().includes(q) || (it.category || "").toLowerCase().includes(q)
    })
    readonly property var chosen: picking ? items.filter((it, i) => ticked(i)) : items
    onReadChanged: { ticks = {}; sent = {}; search = ""; added = ""; into.editText = menu.category ? menu.category.name : "" }
    title: read.error ? qsTr("Can't import that") : qsTr("Import %n item(s)", "", chosen.length)
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
                      + (importDialog.items.some(i => !i.category) ? ""
                         : qsTr("Into: %1.").arg(importDialog.categoriesIn.join(", ")))
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
        Label {
            visible: importDialog.added !== ""
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#7bd88f"
            text: importDialog.added
        }
        // Where the rows without a category go: one of the menu's, or a new one by name.
        RowLayout {
            visible: importDialog.picking && importDialog.items.some(i => !i.category)
            Layout.fillWidth: true
            Label { text: qsTr("Rows without a category go in") }
            ComboBox {
                id: into
                objectName: "builderImportInto"
                Layout.fillWidth: true
                implicitHeight: 44
                editable: true
                model: menu.categories.map(c => c.name)
            }
        }
        RowLayout {
            visible: importDialog.picking
            Layout.fillWidth: true
            TextField {
                objectName: "builderImportSearch"
                Layout.fillWidth: true
                implicitHeight: 44
                placeholderText: qsTr("🔍 Find (name or category)")
                text: importDialog.search
                onTextEdited: importDialog.search = text
                inputMethodHints: Qt.ImhNoPredictiveText
            }
            MenuTouchButton {
                objectName: "builderImportAll"
                text: qsTr("Tick these")
                onClicked: importDialog.tick(importDialog.shown, true)
            }
            MenuTouchButton {
                objectName: "builderImportNone"
                text: qsTr("Untick these")
                onClicked: importDialog.tick(importDialog.shown, false)
            }
        }
        CheckBox {
            id: showAll
            objectName: "builderImportShowAll"
            visible: importDialog.picking && importDialog.items.some(i => importDialog.isOn(i.name))
            text: qsTr("Show the ones already on the menu")
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: importDialog.picking ? importDialog.shown : importDialog.items
            delegate: RowLayout {
                required property var modelData
                readonly property var row: importDialog.picking ? importDialog.items[modelData] : modelData
                width: ListView.view.width
                CheckBox {
                    visible: importDialog.picking
                    checked: importDialog.picking && importDialog.ticked(parent.modelData)
                    onToggled: importDialog.tick([parent.modelData], checked)
                }
                Label {
                    Layout.fillWidth: true
                    text: (row.photoData || row.photo ? "📷 " : "") + row.name
                          + (importDialog.isOn(row.name) ? "  ·  " + qsTr("on the menu") : "")
                    elide: Text.ElideRight
                }
                Label {
                    opacity: 0.7
                    visible: text !== ""
                    text: row.sizes ? qsTr("%n size(s)", "", row.sizes.length)
                                    : row.groups ? qsTr("%n choice group(s)", "", row.groups.length) : ""
                }
                Label { text: (row.sizes ? qsTr("from") + " " : "") + menu.pos.currencySymbol + Number(row.price).toFixed(2) }
                Label { Layout.preferredWidth: 180; text: row.category || into.editText; opacity: 0.7; elide: Text.ElideRight }
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
                enabled: importDialog.chosen.length > 0 && (!importDialog.picking || into.editText.trim() !== ""
                                                            || importDialog.chosen.every(i => i.category))
                text: importDialog.picking ? qsTr("Import %n", "", importDialog.chosen.length) : qsTr("Import")
                onClicked: {
                    if (importDialog.read.menuFile) {
                        menu.pos.importMenuFile(importDialog.read.menuFile)
                        importDialog.close()
                        return
                    }
                    // Rows without a category: into the one chosen (made if it's a new name).
                    const name = into.editText.trim()
                    const rows = importDialog.chosen.map(i => i.category ? i : Object.assign({}, i, { category: name }))
                    const picked = importDialog.items.map((it, i) => i).filter(i => importDialog.ticked(i))
                    const left = importDialog.items.filter((it, i) => !picked.includes(i) && importDialog.sent[i] !== true
                                                                       && !importDialog.isOn(it.name))
                    menu.pos.importMenuRows(rows, menu.categoryId, updatePrices.checked, importDialog.read.groups ?? [])
                    // Some left: the dialog stays, for the next category (what went in
                    // is hidden; the store says if something didn't).
                    if (left.length > 0 && importDialog.picking) {
                        const sent = Object.assign({}, importDialog.sent)
                        const t = Object.assign({}, importDialog.ticks)
                        for (const i of picked) { sent[i] = true; delete t[i] }
                        importDialog.sent = sent
                        importDialog.ticks = t
                        importDialog.added = name ? qsTr("%n item(s) imported into %1. Tick the next ones.", "", picked.length).arg(name)
                                                  : qsTr("%n item(s) imported. Tick the next ones.", "", picked.length)
                        importDialog.search = ""
                    } else {
                        importDialog.close()
                    }
                }
            }
            MenuTouchButton {
                objectName: "builderImportClose"
                Layout.preferredWidth: 160
                text: importDialog.added ? qsTr("Done") : qsTr("Cancel")
                onClicked: importDialog.close()
            }
        }
    }
}

// The menu checked: touch a problem to fix it.
