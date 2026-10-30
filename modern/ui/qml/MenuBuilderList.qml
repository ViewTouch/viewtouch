import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// The Menu Builder's List: every item on the menu in one table, found by
// any part of its name, number, barcode, category or kitchen name (the
// builder's Find box), narrowed to a category or to what's sold out or has
// no code, and sorted by touching a heading. A row opens the item's card;
// in Select, a row is chosen.
ColumnLayout {
    id: list
    property Item builder   // the Menu Builder (its items, search, selection)
    spacing: 6

    property string scope: "all"     // all | category
    property string filter: ""       // "" | soldout | nocode | low
    property string sortBy: "name"   // name | category | price | tax | code | stock
    // A Stock column once anything counts its stock (Track stock).
    readonly property bool anyStock: builder ? builder.allItems.some(i => i.stockTrack) : false
    property bool descending: false
    // Narrow (beside an item's card): name, price and number only.
    readonly property bool compact: width < 600

    function matches(i, t) {
        return i.name.toLowerCase().includes(t)
            || (i.number !== "" && i.number.startsWith(t))
            || (i.barcodes ?? "").toLowerCase().split(", ").some(b => b !== "" && b.includes(t))
            || (i.familyName ?? "").toLowerCase().includes(t)
            || (i.kitchenName ?? "").toLowerCase().includes(t)
    }
    function taxName(id) {
        const t = builder.taxes.find(x => x.id === id)
        return t ? t.name : id
    }
    function code(i) { return i.number !== "" ? "#" + i.number : (i.barcodes ?? "").split(", ")[0] }
    readonly property var rows: {
        if (!builder)
            return []
        const t = builder.search.trim().toLowerCase()
        let out = builder.allItems
        if (scope === "category")
            out = out.filter(i => i.family === builder.categoryId)
        if (filter === "soldout")
            out = out.filter(i => !i.availableSet)
        else if (filter === "nocode")
            out = out.filter(i => i.number === "" && (i.barcodes ?? "") === "")
        else if (filter === "low")
            out = out.filter(i => i.stockTrack && i.stockLow)
        if (t !== "")
            out = out.filter(i => matches(i, t))
        const key = {
            name: i => i.name.toLowerCase(),
            category: i => (i.familyName ?? "").toLowerCase() + "\u0001" + i.name.toLowerCase(),
            price: i => i.priceValue,
            tax: i => i.taxClass + "\u0001" + i.name.toLowerCase(),
            code: i => code(i) || "\uffff",
            stock: i => i.stockTrack ? Number(i.stockOnHand) : Infinity,
        }[sortBy]
        const sorted = out.slice().sort((a, b) => {
            const x = key(a), y = key(b)
            return x < y ? -1 : x > y ? 1 : 0
        })
        return descending ? sorted.reverse() : sorted
    }
    function sort(by) {
        if (sortBy === by)
            descending = !descending
        else {
            sortBy = by
            descending = by === "price"   // the dearest first
        }
    }

    // Which items.
    Flow {
        Layout.fillWidth: true
        spacing: 6
        Repeater {
            model: [
                { id: "all", label: qsTr("All Items"), scope: true },
                { id: "category", label: list.builder && list.builder.category ? list.builder.category.name : qsTr("This Category"), scope: true },
                { id: "soldout", label: qsTr("Sold Out") },
                { id: "nocode", label: qsTr("No Number or Barcode") },
            ].concat(list.anyStock ? [{ id: "low", label: qsTr("Low Stock") }] : [])
            delegate: MenuTouchButton {
                required property var modelData
                objectName: "builderListFilter-" + modelData.id
                implicitHeight: 44
                font.pixelSize: 14
                highlighted: modelData.scope ? list.scope === modelData.id : list.filter === modelData.id
                text: modelData.label
                onClicked: {
                    if (modelData.scope)
                        list.scope = modelData.id
                    else
                        list.filter = list.filter === modelData.id ? "" : modelData.id
                }
            }
        }
    }

    // Headings: touched, sorted by that (again: the other way).
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 40
        radius: 6
        color: Ui.sunken
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10 + listBar.room
            spacing: 8
            Repeater {
                model: [
                    { id: "name", label: qsTr("Item"), fill: true },
                    { id: "category", label: qsTr("Category"), w: 130, wide: true },
                    { id: "price", label: qsTr("Price"), w: 80 },
                    { id: "tax", label: qsTr("Tax"), w: 90, wide: true },
                    { id: "code", label: qsTr("Number"), w: 110 },
                ].concat(list.anyStock ? [{ id: "stock", label: qsTr("Stock"), w: 70 }] : [])
                delegate: Label {
                    required property var modelData
                    objectName: "builderListSort-" + modelData.id
                    visible: !list.compact || !modelData.wide
                    Layout.fillWidth: !!modelData.fill
                    Layout.preferredWidth: modelData.w ?? -1
                    Layout.fillHeight: true
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: modelData.id === "price" ? Text.AlignRight : Text.AlignLeft
                    text: modelData.label + (list.sortBy === modelData.id ? (list.descending ? " ▼" : " ▲") : "")
                    font.pixelSize: 14
                    font.bold: list.sortBy === modelData.id
                    opacity: list.sortBy === modelData.id ? 1 : 0.7
                    elide: Text.ElideRight
                    TapHandler { onTapped: { if (TouchGuard.covered(point.scenePressPosition)) return; list.sort(modelData.id) } }
                }
            }
        }
    }

    ListView {
        id: table
        objectName: "builderList"
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        spacing: 4
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: TouchScrollBar { id: listBar }
        model: list.rows
        delegate: Rectangle {
            id: row
            required property var modelData
            objectName: "builderListRow-" + modelData.id
            readonly property bool chosen: list.builder.selecting ? list.builder.selected.includes(modelData.id)
                                                                  : list.builder.editingItem && list.builder.itemId === modelData.id
            readonly property var cat: list.builder.categories.find(c => c.id === modelData.family)
            width: table.width - listBar.room
            height: 56
            radius: 8
            color: chosen ? "#2b3a52" : Ui.panel
            border.color: chosen && list.builder.selecting ? Ui.accent : "transparent"
            border.width: 2
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    text: (list.builder.selecting ? (row.chosen ? "☑  " : "☐  ") : "") + row.modelData.name
                    font.pixelSize: 17
                    font.bold: true
                    font.strikeout: !row.modelData.availableSet
                    elide: Text.ElideRight
                }
                RowLayout {
                    visible: !list.compact
                    Layout.preferredWidth: 130
                    spacing: 6
                    Rectangle { width: 10; height: 28; radius: 3; color: row.cat ? row.cat.color || "#4a5260" : "#4a5260" }
                    Label {
                        Layout.fillWidth: true
                        text: row.modelData.familyName ?? ""
                        font.pixelSize: 14
                        opacity: 0.8
                        elide: Text.ElideRight
                    }
                }
                Label {
                    Layout.preferredWidth: 80
                    horizontalAlignment: Text.AlignRight
                    text: row.modelData.availableSet ? row.modelData.regularPrice : qsTr("sold out")
                    font.pixelSize: 15
                    opacity: row.modelData.availableSet ? 1 : 0.6
                    elide: Text.ElideLeft
                }
                Label {
                    visible: !list.compact
                    Layout.preferredWidth: 90
                    text: list.taxName(row.modelData.taxClass)
                    font.pixelSize: 14
                    opacity: 0.8
                    elide: Text.ElideRight
                }
                Label {
                    Layout.preferredWidth: 110
                    text: list.code(row.modelData)
                    font.pixelSize: 14
                    opacity: 0.8
                    elide: Text.ElideRight
                }
                // How many on hand (Track stock): red when low.
                Label {
                    visible: list.anyStock
                    Layout.preferredWidth: 70
                    horizontalAlignment: Text.AlignRight
                    text: row.modelData.stockTrack ? row.modelData.stockOnHand : ""
                    font.pixelSize: 15
                    font.bold: !!row.modelData.stockLow
                    color: row.modelData.stockLow ? "#ff6b6b" : palette.windowText
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    const item = row.modelData, b = list.builder
                    if (b.selecting)
                        b.toggleSelected(item.id)
                    else
                        b.leave(() => { b.categoryId = item.family; b.editItem(item) })
                }
            }
        }
        Label {
            anchors.centerIn: parent
            visible: list.rows.length === 0
            width: parent.width - 40
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            text: list.builder && list.builder.search.trim() !== "" ? qsTr("Nothing on the menu by that name.")
                                                                   : qsTr("No items here.")
            opacity: 0.6
        }
    }
    Label {
        Layout.fillWidth: true
        text: qsTr("%n item(s)", "", list.rows.length)
        font.pixelSize: 13
        opacity: 0.6
    }
}
