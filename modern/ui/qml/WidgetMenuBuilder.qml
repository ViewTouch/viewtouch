import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Dialogs
import QtQuick.Layouts

// Manager -> Menu Builder: the whole menu on one screen. Categories (their
// order, color, meal periods, and what new items in them start with); a
// category's items as tiles, + Add Item; an item's card: name, price,
// category, photo, its choices picked from a list, and what's on it (No,
// Light, Extra, On the side then work everywhere). On a phone, one at a
// time: categories, items, the card.
Item {
    id: w
    // Every button a finger's size.
    component TouchButton: MenuTouchButton {}
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real zoom: zone ? zone.formZoom(narrow ? 340 : 1180) : 1

    readonly property var categories: pos ? pos.menuCategories : []
    readonly property var allItems: pos ? pos.menuItems.filter(i => !i.modifier) : []
    readonly property var groups: pos ? pos.choiceGroups.filter(g => !g.own) : []
    readonly property var periods: pos ? pos.mealPeriods : []
    readonly property var stations: pos ? pos.kitchenStations.filter(s => !s.printer) : []
    readonly property var printers: pos ? pos.kitchenStations.filter(s => s.printer) : []
    readonly property var taxes: [{ id: "food", name: qsTr("Food") }, { id: "alcohol", name: qsTr("Alcohol") },
                                  { id: "merchandise", name: qsTr("Merchandise") }, { id: "room", name: qsTr("Room") }]

    // What's chosen, and the card being edited.
    property string categoryId: ""
    readonly property var category: categories.find(c => c.id === categoryId) ?? null
    readonly property var items: allItems.filter(i => i.family === categoryId)
    property string itemId: ""        // "" + editingItem: a new one
    property bool editingItem: false
    property bool editingCategory: false
    property bool editingGroup: false
    property string mode: "menu"          // menu | choices (Choice Groups)
    readonly property var editedGroup: editingGroup && draft.id ? (pos ? pos.choiceGroups.find(g => g.id === draft.id) : null) : null
    property var draft: ({})
    property string stage: "categories"   // a phone: categories | items | card
    // Dragging an item tile or a category (hold, then drag): which, and where to.
    property string dragId: ""
    property int dropIndex: -1
    // An item dragged onto a category on the left: moved there.
    property string dropCategory: ""
    property int listScroll: 0
    Timer {
        interval: 30
        repeat: true
        running: w.dragId !== "" && w.listScroll !== 0
        onTriggered: categoryList.contentY = Math.max(0, Math.min(categoryList.contentHeight - categoryList.height,
                                                                  categoryList.contentY + 14 * w.listScroll))
    }
    // Select: several items, then one change for all of them.
    property bool selecting: false
    property bool preview: false
    property var selected: []
    function toggleSelected(id) {
        selected = selected.includes(id) ? selected.filter(x => x !== id) : selected.concat([id])
    }
    function changeSelected(changes) {
        pos.changeMenuItems(selected, changes)
    }

    Component.onCompleted: {
        if (categories.length) categoryId = categories[0].id
        // Sent from the order screen's quick card (All Settings…): that item.
        const open = zone && zone.controller ? zone.controller.take("menuBuilderOpen") : undefined
        if (open) {
            const i = allItems.find(x => x.id === open || x.name.toLowerCase() === String(open).toLowerCase())
            if (i) {
                categoryId = i.family
                editItem(i)
            } else {
                waitingFor = String(open)   // not here yet: opened when it is
            }
            return
        }
        // A card left unsaved when this page was left: back as it was.
        const k = zone && zone.controller ? zone.controller.take("menuBuilder") : undefined
        if (k) {
            mode = k.mode
            if (k.categoryId) categoryId = k.categoryId
            draft = k.draft
            itemId = k.draft.id ?? ""
            returnToItem = k.returnToItem ?? null
            editingItem = k.editingItem
            editingCategory = k.editingCategory
            editingGroup = k.editingGroup
            stage = "card"
        }
    }
    Component.onDestruction: {
        if (zone && zone.controller)
            zone.controller.keep("menuBuilder", unsaved()
                ? { mode: mode, categoryId: categoryId, draft: draft, returnToItem: returnToItem,
                    editingItem: editingItem, editingCategory: editingCategory, editingGroup: editingGroup }
                : undefined)
    }
    // A new category, once saved, is the one shown.
    property string waitingForCategory: ""
    onCategoriesChanged: {
        const added = waitingForCategory === "" ? null
                    : categories.find(c => c.name.toLowerCase() === waitingForCategory.toLowerCase())
        if (added) {
            waitingForCategory = ""
            pickCategory(added.id)
            Qt.callLater(() => categoryList.positionViewAtIndex(categories.findIndex(c => c.id === added.id), ListView.Contain))
        } else if (!category && categories.length) {
            categoryId = categories[0].id
        }
    }

    function copy(o) { return JSON.parse(JSON.stringify(o)) }
    // The open card's item as saved.
    readonly property var savedItem: editingItem && draft.id ? (allItems.find(i => i.id === draft.id) ?? null) : null
    function pickCategory(id) {
        categoryId = id   // chosen items stay chosen (Select works across categories)
        editingItem = false
        editingCategory = false
        stage = "items"
    }
    function editCategory(c) {
        returnToItem = null
        draft = categoryDraft(c)
        editingCategory = true
        editingItem = false
        stage = "card"
    }
    // Text size choices (percent; 0: the item's category's, or the usual).
    function textSizes(usual) {
        return [{ pct: 0, name: usual }, { pct: 80, name: qsTr("Small") }, { pct: 100, name: qsTr("Normal") },
                { pct: 125, name: qsTr("Large") }, { pct: 150, name: qsTr("Extra large") }]
    }
    function itemDraft(i) {
        return i ? { id: i.id, name: i.name, price: i.priceValue.toFixed(2), family: i.family, image: i.image,
                     groups: i.groups.slice(), onIt: i.onIt.join(", "), available: i.availableSet,
                     kioskHide: i.kioskHide, description: i.description, favorite: i.favorite,
                     allergens: (i.allergens ?? []).slice(), kitchenName: i.kitchenName, number: i.number, barcodes: i.barcodes ?? "",
                     buttonColor: i.buttonColor, textSize: i.textSize ?? 0, prepMinutes: i.prepMinutes ? String(i.prepMinutes) : "",
                     takeoutPrice: i.takeoutPrice, periodPrices: Object.assign({}, i.periodPrices),
                     taxClass: i.taxClass, printer: i.printer, station: i.station,
                     section: i.section ?? "", breakBefore: i.breakBefore ?? "" }
                 : { id: "", name: "", price: "", family: categoryId, image: "", groups: [], onIt: "",
                     available: true, kioskHide: false, description: "", favorite: false, allergens: [],
                     kitchenName: "", number: "", barcodes: "", buttonColor: "", textSize: 0, prepMinutes: "", takeoutPrice: "", periodPrices: {},
                     section: "", breakBefore: "",
                     // what its category's items start with
                     taxClass: category ? (category.taxClass || "food") : "food",
                     printer: category ? (category.printer || "kitchen") : "kitchen", station: category ? category.station : "" }
    }
    function categoryDraft(c) {
        return c ? { id: c.id, name: c.name, color: c.color, periods: c.periods.slice(), printer: c.printer,
                     station: c.station, taxClass: c.taxClass, buttonSize: c.buttonSize ?? "", photos: c.photos === true,
                     hidePrice: c.hidePrice === true, shades: c.shades === true, textSize: c.textSize ?? 0 }
                 : { id: "", name: "", color: StoreColors.starters[categories.length % StoreColors.starters.length], periods: [],
                     printer: "kitchen", station: "", taxClass: "food", buttonSize: "", photos: false, hidePrice: false, shades: false, textSize: 0 }
    }
    function groupDraft(g) {
        const kind = !g ? "one" : g.max === 1 ? "one" : g.max === 0 ? "any" : "upTo"
        return g ? { id: g.id, name: g.name, kind: kind, upTo: g.max > 1 ? g.max : 3, required: g.min > 0,
                     atLeast: Math.max(1, g.min), askHow: g.askHow, options: g.options.map(o => ({ name: o.name,
                     price: o.price ? o.price.toFixed(2) : "", included: o.included, kitchenName: o.kitchenName,
                     sizePrices: Object.assign({}, o.sizePrices ?? {}) })) }
                 : { id: "", name: "", kind: "one", upTo: 3, required: true, atLeast: 1, askHow: false,
                     options: [{ name: "", price: "", included: false }, { name: "", price: "", included: false }] }
    }
    // Unsaved changes on the open card? Compared with what's saved, the way
    // the store reads it ("3.5" is 3.50; groups and periods in any order).
    function same(a, b) {
        const norm = v => {
            if (Array.isArray(v)) {
                const n = v.map(norm)
                return n.every(x => typeof x === "string") ? n.slice().sort() : n
            }
            if (v && typeof v === "object") {
                const o = {}
                for (const k of Object.keys(v).sort()) {
                    const n = norm(v[k])
                    if (n !== "") o[k] = n
                }
                return o
            }
            if (typeof v === "string") {
                const t = v.trim()
                if (t !== "" && !isNaN(Number(t.replace(",", ".")))) return String(Number(t.replace(",", ".")))
                return t.split(",").map(x => x.trim()).filter(x => x).join(",")
            }
            return v === undefined || v === null ? "" : v
        }
        return JSON.stringify(norm(a)) === JSON.stringify(norm(b))
    }
    function unsaved() {
        // A new one already saved: it's there by its name now.
        const named = list => !draft.id && (draft.name ?? "").trim() !== ""
                              && list.some(x => x.name.toLowerCase() === draft.name.trim().toLowerCase())
        if ((editingItem && named(allItems)) || (editingCategory && named(categories)) || (editingGroup && named(allGroups)))
            return false
        if (editingItem) {
            const saved = draft.id ? allItems.find(i => i.id === draft.id) : null
            if (draft.id && !saved) return false   // removed meanwhile
            const fresh = itemDraft(saved)
            if (!draft.id)
                for (const k of ["family", "taxClass", "printer", "station"]) fresh[k] = draft[k]
            return !same(fresh, draft)
        }
        if (editingCategory) {
            const saved = draft.id ? categories.find(c => c.id === draft.id) : null
            if (draft.id && !saved) return false
            const fresh = categoryDraft(saved)
            if (!draft.id) fresh.color = draft.color
            return !same(fresh, draft)
        }
        if (editingGroup) {
            const saved = draft.id ? allGroups.find(g => g.id === draft.id) : null
            if (draft.id && !saved) return false
            return !same(groupDraft(saved), draft)
        }
        return false
    }
    // Before leaving the open card for another: keep or drop its changes.
    function leave(then) {
        if (unsaved()) {
            leaveDialog.then = then
            leaveDialog.open()
        } else {
            then()
        }
    }
    // A problem from the menu check: straight to what to fix.
    function openProblem(p) {
        if (p.item) {
            const i = w.allItems.find(x => x.id === p.item)
            if (i) { w.mode = "menu"; w.categoryId = i.family; w.editItem(i) }
        } else if (p.group) {
            const g = w.allGroups.find(x => x.id === p.group)
            if (g) w.editGroup(g)
        } else if (p.category) {
            const c = w.categories.find(x => x.id === p.category)
            if (c) { w.mode = "menu"; w.categoryId = c.id; w.editCategory(c) }
        }
    }
    function saveCard() {
        if (editingItem) saveItem()
        else if (editingCategory) saveCategoryCard()
        else if (editingGroup) saveGroupCard()
    }
    function saveCategoryCard() {
        const c = copy(draft)
        if (!c.id)
            waitingForCategory = c.name.trim()
        pos.saveCategory(c)
        if (!c.id)
            editingCategory = false
    }
    function saveGroupCard() {
        const g = groupRecord()
        if (!g.id)
            waitingForGroup = g.name.trim()
        pos.saveChoiceGroup(g)
    }
    // + New Choice Group on an item's card: back to the item afterwards.
    property var returnToItem: null
    function backToItem(groupId) {
        const d = returnToItem
        returnToItem = null
        if (!d) return
        if (groupId && !d.groups.includes(groupId))
            d.groups.push(groupId)
        mode = "menu"
        if (d.family) categoryId = d.family
        draft = d
        itemId = d.id
        editingGroup = false
        editingCategory = false
        editingItem = true
        stage = "card"
    }
    function editItem(i) {
        returnToItem = null
        draft = itemDraft(i)
        itemId = i ? i.id : ""
        editingItem = true
        editingCategory = false
        stage = "card"
    }
    // A choice group: its rule in words (how many; required), its options as rows.
    function editGroup(g) {
        draft = groupDraft(g)
        mode = "choices"
        editingGroup = true
        editingItem = false
        editingCategory = false
        stage = "card"
    }
    function setOption(i, key, value) {
        const d = copy(draft)
        d.options[i][key] = value
        draft = d
    }
    // The sizes the store's items come in (Small, Large…), each once.
    readonly property var sizeNames: {
        const out = []
        for (const i of allItems)
            for (const z of (i.sizes ?? []))
                if (!out.some(n => n.toLowerCase() === z.name.toLowerCase())) out.push(z.name)
        return out
    }
    function setSizePrice(i, size, value) {
        const d = copy(draft)
        d.options[i].sizePrices = Object.assign({}, d.options[i].sizePrices ?? {})
        d.options[i].sizePrices[size.toLowerCase()] = value
        draft = d
    }
    function moveOption(i, by) {
        const d = copy(draft)
        const j = i + by
        if (j < 0 || j >= d.options.length) return
        const t = d.options[i]; d.options[i] = d.options[j]; d.options[j] = t
        draft = d
    }
    function groupRecord() {
        const d = draft
        const max = d.kind === "one" ? 1 : d.kind === "upTo" ? Math.max(2, Number(d.upTo) || 2) : 0
        const min = !d.required ? 0 : d.kind === "any" ? Math.max(1, Number(d.atLeast) || 1) : 1
        return { id: d.id, name: d.name, min: min, max: max, askHow: d.askHow, options: d.options }
    }
    property string waitingForGroup: ""
    readonly property var allGroups: pos ? pos.choiceGroups : []
    onAllGroupsChanged: {
        if (waitingForGroup === "")
            return
        const added = allGroups.find(g => g.name.toLowerCase() === waitingForGroup.toLowerCase())
        if (added) {
            waitingForGroup = ""
            if (returnToItem) {
                backToItem(added.id)
                return
            }
            editGroup(added)
            Qt.callLater(() => groupList.positionViewAtIndex(groups.findIndex(g => g.id === added.id), ListView.Contain))
        }
    }
    function setPeriodPrice(period, value) {
        const p = Object.assign({}, draft.periodPrices ?? {})
        p[period] = value
        set("periodPrices", p)
    }
    // The card's More settings, open or not (stays so from item to item).
    property bool moreOpen: false
    // Find an item anywhere on the menu (its name, or its number).
    property string search: ""
    readonly property var found: {
        const t = search.trim().toLowerCase()
        if (t === "") return []
        return allItems.filter(i => i.name.toLowerCase().includes(t) || (i.number !== "" && i.number === t)).slice(0, 30)
    }
    // Like…: another item's choices, what's on it, allergens, kitchen and
    // tax, kitchen time and color (not its name, price, number or photo).
    function copyFrom(i) {
        const d = copy(draft)
        Object.assign(d, { groups: i.groups.slice(), onIt: i.onIt.join(", "), allergens: (i.allergens ?? []).slice(),
                           printer: i.printer, station: i.station, taxClass: i.taxClass,
                           prepMinutes: i.prepMinutes ? String(i.prepMinutes) : "", buttonColor: i.buttonColor })
        draft = d
    }
    function set(key, value) {
        const d = copy(draft)
        d[key] = value
        draft = d
    }
    function toggleIn(key, value) {
        const list = (draft[key] ?? []).slice()
        const at = list.indexOf(value)
        if (at >= 0) list.splice(at, 1); else list.push(value)
        set(key, list)
    }
    // A new item, once saved, is the one shown.
    property string waitingFor: ""
    // Duplicate: the item that wasn't there before is the one shown.
    property var idsBefore: null
    property string syncPrice: ""
    onAllItemsChanged: {
        if (syncPrice !== "" && editingItem && draft.id === syncPrice) {
            const i = allItems.find(x => x.id === syncPrice)
            if (i) set("price", i.priceValue.toFixed(2))
            syncPrice = ""
        }
        if (idsBefore) {
            const copy = allItems.find(i => !idsBefore.includes(i.id))
            if (copy) {
                idsBefore = null
                categoryId = copy.family
                editItem(copy)
            }
            return
        }
        if (waitingFor === "")
            return
        const added = allItems.find(i => i.name.toLowerCase() === waitingFor.toLowerCase())
        if (added) {
            waitingFor = ""
            categoryId = added.family
            editItem(added)
        }
    }
    function saveItem() {
        const card = copy(draft)
        if (card.id === "")
            waitingFor = card.name.trim()
        pos.saveMenuItemCard(card)
    }

    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        // Older stores: meal pages with category buttons placed by hand.
        Rectangle {
            id: handBuilt
            objectName: "builderHandBuilt"
            visible: w.zone && w.zone.controller ? w.zone.controller.menuScreensHandBuilt : false
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
            height: visible ? handBuiltRow.implicitHeight + 20 : 0
            radius: 8
            color: "#4a3a14"
            RowLayout {
                id: handBuiltRow
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 15
                    text: qsTr("Your menu pages have buttons placed by hand, so new items and categories don't show up on them. Switch them to fill themselves from the menu? You'll see them in the page editor first, to Save or Undo.")
                }
                TouchButton {
                    objectName: "builderSwitchScreens"
                    highlighted: true
                    text: qsTr("Switch to Self-Filling")
                    onClicked: w.zone.controller.switchToSelfFillingMenu()
                }
            }
        }

        RowLayout {
            anchors { left: parent.left; right: parent.right; top: handBuilt.bottom; bottom: parent.bottom; margins: 8 }
            spacing: 12

            // --- categories ---
            ColumnLayout {
                visible: !w.narrow || w.stage === "categories"
                Layout.preferredWidth: w.narrow ? -1 : 270
                Layout.fillWidth: w.narrow
                Layout.fillHeight: true
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    TouchButton {
                        objectName: "builderModeMenu"
                        Layout.fillWidth: true
                        text: qsTr("Menu")
                        highlighted: w.mode === "menu"
                        onClicked: w.leave(() => { w.returnToItem = null; w.mode = "menu"; w.editingGroup = false })
                    }
                    TouchButton {
                        objectName: "builderModeChoices"
                        Layout.fillWidth: true
                        text: qsTr("Choice Groups")
                        highlighted: w.mode === "choices"
                        onClicked: w.leave(() => { w.mode = "choices"; w.editingItem = false; w.editingCategory = false; w.selecting = false; w.selected = [] })
                    }
                }
                // Ready to go? What would trip up service. And the last change, taken back.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    TouchButton {
                        objectName: "builderCheck"
                        readonly property var problems: w.pos ? w.pos.menuProblems : []
                        readonly property int serious: problems.filter(p => p.serious).length
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        // Half the width with Undo beside it: shorter.
                        text: serious ? qsTr("⚠ %n to fix", "", serious)
                                      : undoButton.visible ? qsTr("✓ Ready")
                                      : problems.length ? qsTr("✓ Ready (%n note(s))", "", problems.length) : qsTr("✓ Ready to go")
                        palette.button: serious ? "#7a2e2e" : "#1f5f3a"
                        onClicked: checkDialog.open()
                    }
                    // (The card open closes: what it shows may be gone.)
                    TouchButton {
                        id: undoButton
                        objectName: "builderUndo"
                        visible: !!w.pos && w.pos.menuUndoText !== ""
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        font.pixelSize: 14
                        text: qsTr("↶ Undo") + "\n" + (w.pos ? w.pos.menuUndoText : "")
                        onClicked: {
                            w.editingItem = false
                            w.editingCategory = false
                            w.editingGroup = false
                            w.returnToItem = null
                            w.stage = w.mode === "choices" ? "categories" : "items"
                            w.pos.undoMenuChange()
                        }
                    }
                }
                // Anywhere on the menu: its name or number.
                TextField {
                    objectName: "builderSearch"
                    visible: w.mode === "menu"
                    Layout.fillWidth: true
                    implicitHeight: 48
                    font.pixelSize: 16
                    placeholderText: qsTr("🔍 Find an item (name or number)")
                    text: w.search
                    onTextEdited: w.search = text
                    inputMethodHints: Qt.ImhNoPredictiveText
                }
                ListView {
                    id: foundList
                    objectName: "builderFound"
                    visible: w.mode === "menu" && w.search.trim() !== ""
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: w.found
                    delegate: Rectangle {
                        required property var modelData
                        id: foundRow
                        objectName: "builderFound-" + modelData.id
                        width: foundList.width
                        height: 64
                        radius: 8
                        color: w.editingItem && w.itemId === modelData.id ? "#2b3a52" : "#232933"
                        readonly property var cat: w.categories.find(c => c.id === modelData.family)
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10
                            Rectangle { width: 18; height: 40; radius: 4; color: foundRow.cat ? foundRow.cat.color || "#4a5260" : "#4a5260" }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Label { text: modelData.name; font.pixelSize: 18; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                                Label {
                                    text: (foundRow.cat ? foundRow.cat.name + "  ·  " : "")
                                          + (modelData.availableSet ? modelData.price : qsTr("sold out"))
                                          + (modelData.number !== "" ? "  ·  #" + modelData.number : "")
                                    font.pixelSize: 13
                                    opacity: 0.7
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                const item = modelData, b = w
                                b.leave(() => { b.categoryId = item.family; b.editItem(item) })
                            }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: w.found.length === 0
                        text: qsTr("Nothing on the menu by that name.")
                        opacity: 0.6
                    }
                }
                // Choice groups: every one, its rule and who uses it.
                ListView {
                    id: groupList
                    objectName: "builderGroups"
                    visible: w.mode === "choices"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: w.groups
                    delegate: Rectangle {
                        required property var modelData
                        objectName: "builderGroupRow-" + modelData.id
                        width: groupList.width
                        height: 64
                        radius: 8
                        color: w.editingGroup && w.draft.id === modelData.id ? "#2b3a52" : "#232933"
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 0
                            Label { text: modelData.name; font.pixelSize: 18; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                            Label {
                                text: modelData.rule + "  ·  " + qsTr("%n option(s)", "", modelData.options.length)
                                      + "  ·  " + qsTr("used by %n", "", modelData.usedBy.length)
                                font.pixelSize: 13
                                opacity: 0.7
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                const group = modelData, b = w
                                b.leave(() => b.editGroup(group))
                            }
                        }
                    }
                }
                TouchButton {
                    objectName: "builderAddGroup"
                    visible: w.mode === "choices"
                    Layout.fillWidth: true
                    text: qsTr("+ Choice Group")
                    onClicked: w.leave(() => { w.returnToItem = null; w.editGroup(null) })
                }
                ListView {
                    id: categoryList
                    visible: w.mode === "menu" && w.search.trim() === ""
                    interactive: w.dragId === ""
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: w.categories
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        objectName: "builderCategory-" + modelData.id
                        width: categoryList.width
                        height: 64
                        radius: 8
                        color: modelData.id === w.categoryId ? "#2b3a52" : "#232933"
                        border.color: modelData.id === w.categoryId ? "#4c8dff" : "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10
                            Rectangle { width: 18; height: 40; radius: 4; color: modelData.color || "#4a5260" }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Label { text: modelData.name; font.pixelSize: 18; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                                Label {
                                    text: qsTr("%n item(s)", "", modelData.count) + "  ·  "
                                          + (modelData.periods.length === 0 ? qsTr("all day")
                                             : modelData.periods.map(p => (w.periods.find(x => x.id === p) ?? { name: p }).name).join(", "))
                                    font.pixelSize: 13
                                    opacity: 0.7
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                        // An item dragged onto it.
                        Rectangle {
                            objectName: "builderDropOn-" + modelData.id
                            anchors.fill: parent
                            visible: w.dropCategory === modelData.id
                            color: "#33f5b940"
                            radius: parent.radius
                            border.color: "#f5b940"
                            border.width: 4
                        }
                        Rectangle {
                            anchors.fill: parent
                            visible: w.dragId !== "" && w.dropIndex === index && w.dragId !== modelData.id && w.mode === "menu"
                            color: "transparent"
                            radius: parent.radius
                            border.color: "#f5b940"
                            border.width: 4
                        }
                        MouseArea {
                            anchors.fill: parent
                            pressAndHoldInterval: 350
                            property bool dragging: false
                            preventStealing: dragging
                            onClicked: {
                                const id = modelData.id, b = w
                                b.leave(() => b.pickCategory(id))
                            }
                            onPressAndHold: m => {
                                dragging = true
                                w.dragId = modelData.id
                                ghost.text = modelData.name
                                ghost.color = modelData.color || "#4a5260"
                                moved(m)
                            }
                            function moved(m) {
                                const p = mapToItem(categoryList, m.x, m.y)
                                w.dropIndex = categoryList.indexAt(p.x + categoryList.contentX, p.y + categoryList.contentY)
                                const g = mapToItem(ghost.parent, m.x, m.y)
                                ghost.x = g.x - ghost.width / 2
                                ghost.y = g.y - ghost.height / 2
                            }
                            onPositionChanged: m => { if (dragging) moved(m) }
                            onReleased: {
                                if (!dragging) return
                                dragging = false
                                const to = w.dropIndex
                                const id = w.dragId
                                w.dragId = ""
                                w.dropIndex = -1
                                if (to >= 0 && w.categories[to] && w.categories[to].id !== id)
                                    w.pos.moveCategoryTo(id, to)
                            }
                            onCanceled: { dragging = false; w.dragId = ""; w.dropIndex = -1 }
                        }
                    }
                }
                RowLayout {
                    visible: w.mode === "menu"
                    Layout.fillWidth: true
                    spacing: 6
                    TouchButton {
                        objectName: "builderAddCategory"
                        Layout.fillWidth: true
                        text: qsTr("+ Category")
                        onClicked: w.leave(() => w.editCategory(null))
                    }
                    TouchButton {
                        objectName: "builderThemes"
                        Layout.preferredWidth: 44
                        text: "🎨"
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Colors for every category at once")
                        onClicked: themeDialog.open()
                    }
                    TouchButton { Layout.preferredWidth: 44; text: "▲"; enabled: !!w.category; onClicked: w.pos.moveCategory(w.categoryId, -1) }
                    TouchButton { Layout.preferredWidth: 44; text: "▼"; enabled: !!w.category; onClicked: w.pos.moveCategory(w.categoryId, 1) }
                }
                // Many at once: from a spreadsheet, or a starter menu; the
                // printed menu. One row, or two when the words are longer
                // (Spanish) than the keys.
                GridLayout {
                    visible: w.mode === "menu"
                    Layout.fillWidth: true
                    columnSpacing: 6
                    rowSpacing: 6
                    // (Each label and its padding: a key's own minimum is wider than needed.)
                    function room(k) { return k.implicitContentWidth + k.leftPadding + k.rightPadding }
                    columns: room(importKey) + room(exportKey) + room(startersKey) + room(printKey)
                             + 3 * columnSpacing <= width ? 4 : 2
                    TouchButton {
                        id: importKey
                        objectName: "builderImport"
                        font.pixelSize: 13
                        font.bold: false   // four in a row: plain, as they fit
                        leftPadding: 2
                        rightPadding: 2
                        Layout.fillWidth: true
                        Layout.preferredWidth: implicitWidth
                        text: qsTr("Import…")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("A spreadsheet (Excel or CSV), a menu from Square, Clover or Toast, or another store's")
                        onClicked: importFile.open()
                    }
                    TouchButton {
                        id: exportKey
                        objectName: "builderExport"
                        font.pixelSize: 13
                        font.bold: false   // four in a row: plain, as they fit
                        leftPadding: 2
                        rightPadding: 2
                        Layout.fillWidth: true
                        Layout.preferredWidth: implicitWidth
                        text: qsTr("Export")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("The whole menu as a file, for Import… at another store")
                        onClicked: w.zone.controller.exportMenu()
                    }
                    TouchButton {
                        id: startersKey
                        objectName: "builderTemplates"
                        font.pixelSize: 13
                        font.bold: false   // four in a row: plain, as they fit
                        leftPadding: 2
                        rightPadding: 2
                        Layout.fillWidth: true
                        Layout.preferredWidth: implicitWidth
                        text: qsTr("Starter…")
                        onClicked: templateDialog.open()
                    }
                    TouchButton {
                        id: printKey
                        objectName: "builderPrintMenu"
                        font.pixelSize: 13
                        font.bold: false   // four in a row: plain, as they fit
                        leftPadding: 2
                        rightPadding: 2
                        Layout.fillWidth: true
                        Layout.preferredWidth: implicitWidth
                        text: qsTr("Print…")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("The menu with today's prices as a PDF, to print or hand out")
                        onClicked: w.leave(() => printDialog.open())
                    }
                }
            }

            // --- the category's items ---
            ColumnLayout {
                visible: (!w.narrow || w.stage === "items") && w.mode === "menu"
                Layout.preferredWidth: w.narrow ? -1 : 430
                Layout.fillWidth: w.narrow
                Layout.fillHeight: true
                spacing: 8
                // The category's name: beside its keys, or above them when
                // it's too long to read there.
                TextMetrics {
                    id: categoryName
                    font.pixelSize: 22
                    font.bold: true
                    text: w.category ? w.category.name : ""
                }
                Label {
                    objectName: "builderCategoryNameAbove"
                    visible: !categoryHeader.nameFits && text !== ""
                    Layout.fillWidth: true
                    text: categoryName.text
                    font: categoryName.font
                    elide: Text.ElideRight
                }
                RowLayout {
                    id: categoryHeader
                    readonly property bool nameFits: categoryName.advanceWidth + 12
                        <= width - (backKey.visible ? backKey.implicitWidth + spacing : 0)
                           - pricesKey.implicitWidth - severalKey.implicitWidth - editCategoryKey.implicitWidth - 3 * spacing
                    Layout.fillWidth: true
                    TouchButton { id: backKey; visible: w.narrow; text: qsTr("‹ Categories"); onClicked: w.stage = "categories" }
                    Label {
                        Layout.fillWidth: true
                        text: categoryHeader.nameFits ? categoryName.text : ""
                        font.pixelSize: 22
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    TouchButton {
                        id: pricesKey
                        objectName: "builderPrices"
                        visible: !!w.category
                        text: qsTr("Prices…")
                        onClicked: w.leave(() => pricesDialog.open())
                    }
                    TouchButton {
                        id: severalKey
                        objectName: "builderAddSeveral"
                        visible: !!w.category
                        text: qsTr("Add Several…")
                        onClicked: severalDialog.open()
                    }
                    TouchButton {
                        id: editCategoryKey
                        objectName: "builderEditCategory"
                        visible: !!w.category
                        text: qsTr("Edit Category")
                        onClicked: w.leave(() => w.editCategory(w.category))
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: w.selecting ? qsTr("Touch the items to change together.")
                            : w.items.length > 1 ? (w.narrow ? qsTr("Hold one and drag it to move it (the order screen follows).")
                                                             : qsTr("Hold one and drag it to move it, or onto a category."))
                            : ""
                        opacity: 0.6
                        font.pixelSize: 13
                    }
                    // The order screen's buttons, as they'll look.
                    TouchButton {
                        objectName: "builderPreview"
                        visible: !!w.category && w.items.length > 0 && !w.selecting
                        implicitHeight: 44
                        font.pixelSize: 14
                        checkable: true
                        checked: w.preview
                        highlighted: checked
                        text: qsTr("👁 Preview")
                        onClicked: w.preview = checked
                    }
                    TouchButton {
                        objectName: "builderSelect"
                        visible: !!w.category && w.items.length > 0
                        implicitHeight: 44
                        font.pixelSize: 14
                        highlighted: w.selecting
                        text: w.selecting ? qsTr("Done") : qsTr("Select…")
                        onClicked: {
                            if (w.selecting) {
                                w.selecting = false
                                w.selected = []
                            } else {
                                const b = w
                                b.leave(() => { b.editingItem = false; b.editingCategory = false; b.selecting = true; b.selected = [] })
                            }
                        }
                    }
                }
                // Prices set to change later.
                PosButton {
                    objectName: "builderPriceChanges"
                    readonly property var list: w.pos ? w.pos.priceChanges : []
                    visible: list.length > 0
                    Layout.fillWidth: true
                    implicitHeight: 40
                    font.pixelSize: 14
                    text: list.length === 1 ? "⏰ " + list[0].label + "  ·  " + list[0].when
                                            : "⏰ " + qsTr("%n price changes set for later", "", list.length)
                    onClicked: w.leave(() => pricesDialog.open())
                }
                // What to do with the chosen ones.
                Flow {
                    objectName: "builderSelectBar"
                    visible: w.selecting
                    Layout.fillWidth: true
                    spacing: 6
                    Label {
                        height: 44
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("%n chosen", "", w.selected.length)
                        font.bold: true
                        font.pixelSize: 15
                    }
                    TouchButton {
                        objectName: "builderSelectAll"
                        implicitHeight: 44
                        font.pixelSize: 14
                        readonly property bool all: w.items.length > 0 && w.items.every(i => w.selected.includes(i.id))
                        text: all ? qsTr("None") : qsTr("All")
                        onClicked: {
                            const here = w.items.map(i => i.id)
                            w.selected = all ? w.selected.filter(id => !here.includes(id))
                                             : w.selected.concat(here.filter(id => !w.selected.includes(id)))
                        }
                    }
                    TouchButton {
                        objectName: "builderSelectMove"
                        implicitHeight: 44
                        font.pixelSize: 14
                        enabled: w.selected.length > 0
                        text: qsTr("Move to…")
                        onClicked: moveSeveral.open()
                    }
                    TouchButton {
                        objectName: "builderSelectColor"
                        implicitHeight: 44
                        font.pixelSize: 14
                        enabled: w.selected.length > 0
                        text: qsTr("Color…")
                        onClicked: colorSeveral.open()
                    }
                    TouchButton {
                        objectName: "builderSelectSoldOut"
                        implicitHeight: 44
                        font.pixelSize: 14
                        enabled: w.selected.length > 0
                        text: qsTr("Sold Out")
                        onClicked: w.changeSelected({ available: false })
                    }
                    TouchButton {
                        objectName: "builderSelectForSale"
                        implicitHeight: 44
                        font.pixelSize: 14
                        enabled: w.selected.length > 0
                        text: qsTr("For Sale")
                        onClicked: w.changeSelected({ available: true })
                    }
                    TouchButton {
                        objectName: "builderSelectRemove"
                        implicitHeight: 44
                        font.pixelSize: 14
                        enabled: w.selected.length > 0
                        text: qsTr("Remove…")
                        onClicked: removeSeveral.open()
                    }
                }
                // Preview: this category on the order screen (its menu area, scaled down).
                Rectangle {
                    objectName: "builderPreviewPane"
                    visible: w.preview && !w.selecting
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#14171c"
                    radius: 8
                    clip: true
                    readonly property real design: 1300
                    MenuButtons {
                        width: parent.design
                        height: parent.height * parent.design / Math.max(1, parent.width)
                        scale: parent.width / parent.design
                        transformOrigin: Item.TopLeft
                        items: w.items
                        category: w.category ?? ({})
                        categories: w.categories
                        pos: w.pos
                        onItemTapped: item => {
                            const b = w
                            b.leave(() => b.editItem(item))
                        }
                    }
                }
                GridView {
                    id: itemGrid
                    visible: !w.preview || w.selecting
                    interactive: w.dragId === ""
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    cellWidth: width / 2
                    cellHeight: 96
                    model: [{ add: true }].concat(w.items)
                    delegate: Item {
                        required property var modelData
                        required property int index
                        width: itemGrid.cellWidth
                        height: itemGrid.cellHeight
                        Rectangle {
                            objectName: modelData.add ? "builderAddItem" : "builderItem-" + modelData.id
                            anchors.fill: parent
                            anchors.margins: 4
                            radius: 10
                            readonly property string tint: modelData.add ? "#1f5f3a"
                                                          : (modelData.buttonColor || (w.category ? w.category.color : "") || "#343c49")
                            color: tint
                            opacity: modelData.add || modelData.availableSet ? 1 : 0.5
                            readonly property bool marked: !modelData.add && (w.selecting ? w.selected.includes(modelData.id)
                                                                                           : w.editingItem && w.itemId === modelData.id)
                            border.color: marked ? "#f5b940" : Qt.darker(tint, 1.3)
                            border.width: marked ? 4 : 1
                            Column {
                                anchors.centerIn: parent
                                width: parent.width - 16
                                Label {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    text: modelData.add ? qsTr("+ Add Item") : modelData.name
                                    color: StoreColors.ink(parent.parent.tint)
                                    font.pixelSize: 18
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Label {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    visible: !modelData.add
                                    text: modelData.add ? "" : (modelData.section ? modelData.section + "  ·  " : "")
                                                               + (modelData.availableSet ? modelData.price : qsTr("sold out"))
                                    color: StoreColors.ink(parent.parent.tint)
                                    opacity: 0.8
                                    font.pixelSize: 15
                                }
                            }
                            // Outlined: where a dragged item will go.
                            Rectangle {
                                anchors.fill: parent
                                visible: w.dragId !== "" && !modelData.add && w.dropIndex === index && w.dragId !== modelData.id
                                color: "transparent"
                                radius: parent.radius
                                border.color: "#f5b940"
                                border.width: 4
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: !!w.category
                                pressAndHoldInterval: 350
                                property bool dragging: false
                                preventStealing: dragging
                                onClicked: {
                                    if (w.selecting) {
                                        if (!modelData.add) w.toggleSelected(modelData.id)
                                        return
                                    }
                                    // Taken now: after a Save in the prompt this tile (and
                                    // what it can see) may be gone.
                                    const item = modelData.add ? null : modelData, b = w
                                    b.leave(() => b.editItem(item))
                                }
                                // Held: dragged to another place in the category, or onto another.
                                onPressAndHold: m => {
                                    if (modelData.add || w.selecting) return
                                    dragging = true
                                    w.dragId = modelData.id
                                    ghost.text = modelData.name
                                    ghost.color = parent.tint
                                    moved(m)
                                }
                                function moved(m) {
                                    const p = mapToItem(itemGrid, m.x, m.y)
                                    w.dropIndex = itemGrid.indexAt(p.x + itemGrid.contentX, p.y + itemGrid.contentY)
                                    // Over the categories: onto one.
                                    const c = mapToItem(categoryList, m.x, m.y)
                                    const onList = categoryList.visible && c.x >= 0 && c.y >= 0 && c.x < categoryList.width && c.y < categoryList.height
                                    // Near its top or bottom: it scrolls, for categories out of view.
                                    const overColumn = categoryList.visible && c.x >= 0 && c.x < categoryList.width
                                    w.listScroll = !overColumn ? 0 : c.y < 48 ? -1 : c.y > categoryList.height - 48 ? 1 : 0
                                    const at = onList ? categoryList.indexAt(c.x + categoryList.contentX, c.y + categoryList.contentY) : -1
                                    w.dropCategory = at >= 0 && w.categories[at].id !== w.categoryId ? w.categories[at].id : ""
                                    if (w.dropCategory !== "") w.dropIndex = -1
                                    const g = mapToItem(ghost.parent, m.x, m.y)
                                    ghost.x = g.x - ghost.width / 2
                                    ghost.y = g.y - ghost.height / 2
                                }
                                onPositionChanged: m => { if (dragging) moved(m) }
                                onReleased: {
                                    if (!dragging) return
                                    dragging = false
                                    // Cleared first: the move rebuilds the tiles (this one too).
                                    const to = w.dropIndex - 1   // the first tile is + Add Item
                                    const id = w.dragId, onto = w.dropCategory, name = modelData.name
                                    w.dragId = ""
                                    w.dropIndex = -1
                                    w.dropCategory = ""
                                    w.listScroll = 0
                                    if (onto !== "")
                                        w.pos.saveMenuItemCard({ id: id, name: name, family: onto })
                                    else if (to >= 0 && to < w.items.length && w.items[to].id !== id)
                                        w.pos.moveMenuItemTo(id, to)
                                }
                                onCanceled: { dragging = false; w.dragId = ""; w.dropIndex = -1; w.dropCategory = ""; w.listScroll = 0 }
                            }
                            // Chosen (Select).
                            Rectangle {
                                visible: w.selecting && !modelData.add && w.selected.includes(modelData.id)
                                anchors.top: parent.top
                                anchors.right: parent.right
                                anchors.margins: 6
                                width: 30; height: 30; radius: 15
                                color: "#f5b940"
                                Label { anchors.centerIn: parent; text: "✓"; color: "#14171c"; font.bold: true; font.pixelSize: 18 }
                            }
                        }
                    }
                }
            }

            // --- the card: an item, or a category ---
            Rectangle {
                visible: !w.narrow || w.stage === "card"
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 10
                color: "#1c2129"
                Label {
                    anchors.centerIn: parent
                    visible: !w.editingItem && !w.editingCategory && !w.editingGroup
                    width: parent.width - 40
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    text: w.mode === "choices" ? qsTr("Touch a choice group to change it, or + Choice Group.")
                        : w.selecting ? qsTr("Touch items to choose them (in any category), then move, color, sell out or remove them together.")
                        : qsTr("Touch an item to change it, or + Add Item.")
                    opacity: 0.6
                    font.pixelSize: 18
                }

                Flickable {
                    id: cardFlick
                    objectName: "builderCard"
                    Timer {
                        id: moreScroll
                        property real top: 0
                        interval: 30
                        onTriggered: cardFlick.contentY = Math.max(0, Math.min(cardFlick.contentHeight - cardFlick.height, top))
                    }
                    anchors { left: parent.left; right: parent.right; top: parent.top; bottom: cardButtons.top; margins: 14 }
                    visible: w.editingItem || w.editingCategory || w.editingGroup
                    clip: true
                    contentHeight: cardColumn.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: TouchScrollBar { id: cardBar }

                    ColumnLayout {
                        id: cardColumn
                        width: cardFlick.width - cardBar.room
                        spacing: 10

                        TouchButton { visible: w.narrow; text: qsTr("‹ Back"); onClicked: w.stage = w.mode === "choices" ? "categories" : "items" }

                        // --- an item ---
                        ColumnLayout {
                            visible: w.editingItem
                            Layout.fillWidth: true
                            spacing: 10
                            RowLayout {
                                Layout.fillWidth: true
                                Label { Layout.fillWidth: true; text: w.draft.id ? qsTr("Item") : qsTr("New item"); font.pixelSize: 22; font.bold: true }
                                // Most new tacos are like the other tacos.
                                TouchButton {
                                    objectName: "builderLike"
                                    implicitHeight: 44
                                    font.pixelSize: 14
                                    text: qsTr("Like…")
                                    onClicked: likeDialog.open()
                                }
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 10
                                rowSpacing: 8
                                Label { text: qsTr("Name") }
                                TextField {
                                    objectName: "builderName"
                                    Layout.fillWidth: true
                                    text: w.draft.name ?? ""
                                    placeholderText: qsTr("e.g. Fish Tacos")
                                    onTextEdited: w.set("name", text)
                                }
                                Label { text: qsTr("Price") }
                                TextField {
                                    objectName: "builderPrice"
                                    Layout.fillWidth: true
                                    text: w.draft.price ?? ""
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    placeholderText: "0.00"
                                    onTextEdited: w.set("price", text)
                                }
                                // Sizes (Small, Large…): asked first when it's ordered.
                                Label { text: qsTr("Sizes") }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    readonly property var sizes: w.savedItem ? (w.savedItem.sizes ?? []) : []
                                    Label {
                                        objectName: "builderSizesText"
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        opacity: parent.sizes.length ? 1 : 0.6
                                        text: parent.sizes.length ? parent.sizes.map(z => z.name + " " + z.price).join("  ·  ")
                                            : w.draft.id ? qsTr("One size") : qsTr("One size (add it first for more)")
                                    }
                                    TouchButton {
                                        objectName: "builderSizes"
                                        implicitHeight: 44
                                        font.pixelSize: 14
                                        enabled: !!w.draft.id
                                        text: qsTr("Sizes…")
                                        onClicked: {
                                            const b = w
                                            b.leave(() => sizesDialog.openFor(b.savedItem))
                                        }
                                    }
                                }
                                Label { text: qsTr("Category") }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: w.categories.map(c => c.name)
                                    currentIndex: w.categories.findIndex(c => c.id === w.draft.family)
                                    onActivated: i => {
                                        const c = w.categories[i]
                                        // A new one: made and taxed the way that category's items are.
                                        if (!w.draft.id) {
                                            const d = w.copy(w.draft)
                                            Object.assign(d, { taxClass: c.taxClass || "food", printer: c.printer || "kitchen", station: c.station })
                                            w.draft = d
                                        }
                                        w.set("family", c.id)
                                    }
                                }
                                // Under a heading on the order screen ("Tacos", "Burritos").
                                Label { text: qsTr("Section") }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    readonly property var known: [...new Set(w.allItems.filter(i => i.family === w.draft.family && i.section)
                                                                                     .map(i => i.section))]
                                    TextField {
                                        objectName: "builderSection"
                                        Layout.fillWidth: true
                                        text: w.draft.section ?? ""
                                        placeholderText: qsTr("None, or a heading like Tacos")
                                        onTextEdited: w.set("section", text)
                                    }
                                    Flow {
                                        visible: parent.known.length > 0
                                        Layout.fillWidth: true
                                        spacing: 4
                                        Repeater {
                                            model: parent.parent.known
                                            delegate: TouchButton {
                                                required property string modelData
                                                implicitHeight: 40
                                                font.pixelSize: 14
                                                checkable: true
                                                checked: (w.draft.section ?? "") === modelData
                                                highlighted: checked
                                                text: modelData
                                                onClicked: w.set("section", checked ? modelData : "")
                                            }
                                        }
                                    }
                                }
                                Label { text: qsTr("Photo") }
                                FieldEditor {
                                    Layout.fillWidth: true
                                    field: ({ path: "image", label: "", type: "image" })
                                    pos: w.pos
                                    value: w.draft.image || undefined
                                    onCommit: v => w.set("image", v ?? "")
                                    onReset: w.set("image", "")
                                }
                                // No photo of it? A ready-made picture.
                                Item { width: 1; height: 1 }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    TouchButton {
                                        objectName: "builderReadyPicture"
                                        implicitHeight: 44
                                        font.pixelSize: 14
                                        text: qsTr("🌮 Ready-Made Picture…")
                                        onClicked: pictureDialog.open()
                                    }
                                    Label {
                                        visible: !!w.draft.image && !!w.category && w.category.photos !== true
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        opacity: 0.7
                                        font.pixelSize: 13
                                        text: qsTr("Turn on Photos for %1 (Edit Category) to show pictures on the order screen; the kiosk shows them always.")
                                              .arg(w.category ? w.category.name : "")
                                    }
                                }
                                Label { text: qsTr("Sold out") }
                                Switch {
                                    checked: !(w.draft.available ?? true)
                                    onToggled: w.set("available", !checked)
                                }
                                Label { text: qsTr("★ Favorite") }
                                Switch {
                                    objectName: "builderFavorite"
                                    checked: w.draft.favorite ?? false
                                    onToggled: w.set("favorite", checked)
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Always in the order screen's ★ Favorites, with today's best sellers.")
                                }
                                Label { text: qsTr("On the kiosk") }
                                Switch {
                                    checked: !(w.draft.kioskHide ?? false)
                                    onToggled: w.set("kioskHide", !checked)
                                }
                            }

                            // What's on it.
                            Label { text: qsTr("What's on it"); font.pixelSize: 18; font.bold: true }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                opacity: 0.7
                                font.pixelSize: 14
                                text: qsTr("The ingredients it comes with, separated by commas. Guests and staff can have each No, Light, Extra or On the side.")
                            }
                            TextField {
                                objectName: "builderOnIt"
                                Layout.fillWidth: true
                                text: w.draft.onIt ?? ""
                                placeholderText: qsTr("e.g. lettuce, tomato, onion, mayo")
                                onTextEdited: w.set("onIt", text)
                            }

                            // Allergens: guests and the kitchen are warned.
                            Label { text: qsTr("Contains"); font.pixelSize: 18; font.bold: true }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 6
                                Repeater {
                                    model: w.pos ? w.pos.allergenList() : []
                                    delegate: TouchButton {
                                        required property var modelData
                                        objectName: "builderAllergen-" + modelData.id
                                        checkable: true
                                        checked: (w.draft.allergens ?? []).includes(modelData.id)
                                        highlighted: checked
                                        text: modelData.name
                                        onClicked: w.toggleIn("allergens", modelData.id)
                                    }
                                }
                            }

                            // Also on a page of buttons placed by hand (a Happy Hour page).
                            RowLayout {
                                readonly property var pages: w.draft.id && w.zone && w.zone.controller && w.pos
                                                             ? (w.pos.adminRevision >= 0 && w.zone.controller.menuScreensHandBuilt !== undefined   // again when pages change
                                                                ? w.zone.controller.handBuiltPages(w.draft.id) : []) : []
                                visible: pages.length > 0
                                Layout.fillWidth: true
                                spacing: 8
                                Label { text: qsTr("Also a button on") }
                                ComboBox {
                                    id: alsoOn
                                    objectName: "builderAlsoOn"
                                    Layout.fillWidth: true
                                    model: parent.pages.map(p => p.name + (p.has ? "  ✓" : ""))
                                }
                                TouchButton {
                                    objectName: "builderAlsoOnAdd"
                                    enabled: alsoOn.currentIndex >= 0 && !(parent.pages[alsoOn.currentIndex] ?? {}).has
                                    text: qsTr("Add Button")
                                    onClicked: w.zone.controller.addItemButton(parent.pages[alsoOn.currentIndex].id, w.draft.id, w.draft.name)
                                }
                            }

                            // Its choices, from the list.
                            Label { text: qsTr("Choices it asks for"); font.pixelSize: 18; font.bold: true }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 6
                                Repeater {
                                    model: w.groups
                                    delegate: TouchButton {
                                        required property var modelData
                                        objectName: "builderGroup-" + modelData.id
                                        checkable: true
                                        checked: (w.draft.groups ?? []).includes(modelData.id)
                                        highlighted: checked
                                        text: modelData.name + "  ·  " + modelData.rule
                                        onClicked: w.toggleIn("groups", modelData.id)
                                    }
                                }
                                TouchButton {
                                    objectName: "builderNewGroup"
                                    text: qsTr("+ New Choice Group…")
                                    onClicked: {
                                        const d = w.copy(w.draft)
                                        w.editGroup(null)
                                        w.returnToItem = d
                                    }
                                }
                            }

                            // The rest most kitchens set: folded away until wanted.
                            TouchButton {
                                objectName: "builderMore"
                                Layout.fillWidth: true
                                flat: true
                                text: (w.moreOpen ? "▾ " : "▸ ") + qsTr("More: kiosk, kitchen, number, other prices")
                                onClicked: {
                                    w.moreOpen = !w.moreOpen
                                    // Opened: scrolled up into view, once laid out.
                                    if (w.moreOpen) {
                                        moreScroll.top = y - 8
                                        moreScroll.restart()
                                    }
                                }
                            }
                            GridLayout {
                                visible: w.moreOpen
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 10
                                rowSpacing: 8
                                Label { text: qsTr("On the kiosk"); Layout.preferredWidth: 110 }
                                TextField {
                                    objectName: "builderDescription"
                                    Layout.fillWidth: true
                                    text: w.draft.description ?? ""
                                    placeholderText: qsTr("A line guests read, e.g. Grilled steak, onion, cilantro")
                                    onTextEdited: w.set("description", text)
                                }
                                Label { text: qsTr("Kitchen name"); Layout.preferredWidth: 110 }
                                TextField {
                                    objectName: "builderKitchenName"
                                    Layout.fillWidth: true
                                    text: w.draft.kitchenName ?? ""
                                    placeholderText: qsTr("Shorter, on tickets: e.g. ASADA")
                                    onTextEdited: w.set("kitchenName", text)
                                }
                                Label { text: qsTr("Number"); Layout.preferredWidth: 110 }
                                TextField {
                                    objectName: "builderNumber"
                                    Layout.fillWidth: true
                                    text: w.draft.number ?? ""
                                    placeholderText: qsTr("To ring it in by number, e.g. 104")
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    onTextEdited: w.set("number", text)
                                }
                                Label { text: qsTr("Barcodes"); Layout.preferredWidth: 110 }
                                TextField {
                                    objectName: "builderBarcodes"
                                    Layout.fillWidth: true
                                    text: w.draft.barcodes ?? ""
                                    placeholderText: qsTr("Scan the package here")
                                    inputMethodHints: Qt.ImhNoPredictiveText
                                    onTextEdited: w.set("barcodes", text)
                                    // A scanner ends with Enter: ready for another (commas between).
                                    onAccepted: if (text.trim() !== "" && !text.trim().endsWith(",")) { text = text.trim() + ", "; w.set("barcodes", text) }
                                }
                                Label { text: qsTr("Kitchen time"); Layout.preferredWidth: 110 }
                                TextField {
                                    objectName: "builderPrepMinutes"
                                    Layout.fillWidth: true
                                    text: w.draft.prepMinutes ?? ""
                                    placeholderText: qsTr("Minutes; empty: learned from the kitchen")
                                    inputMethodHints: Qt.ImhDigitsOnly
                                    onTextEdited: w.set("prepMinutes", text)
                                }
                                Label { text: qsTr("Takeout price"); Layout.preferredWidth: 110 }
                                TextField {
                                    objectName: "builderTakeoutPrice"
                                    Layout.fillWidth: true
                                    text: w.draft.takeoutPrice ?? ""
                                    placeholderText: qsTr("Empty: the same")
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    onTextEdited: w.set("takeoutPrice", text)
                                }
                                Repeater {
                                    model: w.periods
                                    delegate: RowLayout {
                                        required property var modelData
                                        Layout.columnSpan: 2
                                        Layout.fillWidth: true
                                        spacing: 10
                                        Label { text: qsTr("At %1").arg(modelData.name); Layout.preferredWidth: 110; elide: Text.ElideRight }
                                        TextField {
                                            objectName: "builderPeriodPrice-" + modelData.id
                                            Layout.fillWidth: true
                                            text: (w.draft.periodPrices ?? {})[modelData.id] ?? ""
                                            placeholderText: qsTr("Empty: the same")
                                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                                            onTextEdited: w.setPeriodPrice(modelData.id, text)
                                        }
                                    }
                                }
                                Label { text: qsTr("Before it"); Layout.preferredWidth: 110 }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Repeater {
                                        model: [{ id: "", name: qsTr("Nothing") }, { id: "space", name: qsTr("A space") },
                                                { id: "row", name: qsTr("A new row") }]
                                        delegate: TouchButton {
                                            required property var modelData
                                            objectName: "builderBreak-" + (modelData.id || "none")
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: 1
                                            font.pixelSize: 14
                                            checkable: true
                                            checked: (w.draft.breakBefore ?? "") === modelData.id
                                            highlighted: checked
                                            text: modelData.name
                                            onClicked: w.set("breakBefore", modelData.id)
                                        }
                                    }
                                }
                                Label { text: qsTr("Button color"); Layout.columnSpan: 2 }
                                ColorPicker {
                                    objectName: "builderButtonColor"
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    pos: w.pos
                                    noneText: qsTr("Category's")
                                    color: w.draft.buttonColor ?? ""
                                    onPicked: c => w.set("buttonColor", c)
                                }
                                // Its name's size on the order screen.
                                Label { text: qsTr("Text size"); Layout.columnSpan: 2 }
                                Flow {
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Repeater {
                                        model: w.textSizes(qsTr("Category's"))
                                        delegate: TouchButton {
                                            required property var modelData
                                            objectName: "builderTextSize-" + modelData.pct
                                            font.pixelSize: 14
                                            checkable: true
                                            checked: (w.draft.textSize ?? 0) === modelData.pct
                                            highlighted: checked
                                            text: modelData.name
                                            onClicked: w.set("textSize", modelData.pct)
                                        }
                                    }
                                }
                                Label { text: qsTr("Kitchen ticket"); Layout.preferredWidth: 110 }
                                ComboBox {
                                    objectName: "builderItemPrinter"
                                    Layout.fillWidth: true
                                    model: w.printers.map(p => p.name)
                                    currentIndex: w.printers.findIndex(p => p.id === w.draft.printer)
                                    onActivated: i => w.set("printer", w.printers[i].id)
                                }
                                Label { text: qsTr("Made at"); Layout.preferredWidth: 110 }
                                ComboBox {
                                    Layout.fillWidth: true
                                    readonly property var opts: [{ id: "", name: qsTr("(anywhere)") }].concat(w.stations)
                                    model: opts.map(s => s.name)
                                    currentIndex: Math.max(0, opts.findIndex(s => s.id === (w.draft.station ?? "")))
                                    onActivated: i => w.set("station", opts[i].id)
                                }
                                Label { text: qsTr("Tax"); Layout.preferredWidth: 110 }
                                ComboBox {
                                    objectName: "builderItemTax"
                                    Layout.fillWidth: true
                                    model: w.taxes.map(t => t.name)
                                    currentIndex: Math.max(0, w.taxes.findIndex(t => t.id === (w.draft.taxClass || "food")))
                                    onActivated: i => w.set("taxClass", w.taxes[i].id)
                                }
                            }

                        }

                        // --- a choice group ---
                        ColumnLayout {
                            visible: w.editingGroup
                            Layout.fillWidth: true
                            spacing: 10
                            Label { text: w.draft.id ? qsTr("Choice group") : qsTr("New choice group"); font.pixelSize: 22; font.bold: true }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 10
                                rowSpacing: 8
                                Label { text: qsTr("Name") }
                                TextField {
                                    objectName: "builderGroupName"
                                    Layout.fillWidth: true
                                    text: w.draft.name ?? ""
                                    placeholderText: qsTr("e.g. Salsa, Size, Toppings")
                                    onTextEdited: w.set("name", text)
                                }
                                Label { text: qsTr("Guests pick") }
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Repeater {
                                        model: [{ kind: "one", text: qsTr("One") }, { kind: "upTo", text: qsTr("Up to…") },
                                                { kind: "any", text: qsTr("Any number") }]
                                        delegate: TouchButton {
                                            required property var modelData
                                            objectName: "builderKind-" + modelData.kind
                                            text: modelData.text
                                            highlighted: w.draft.kind === modelData.kind
                                            onClicked: w.set("kind", modelData.kind)
                                        }
                                    }
                                    SpinBox {
                                        visible: w.draft.kind === "upTo"
                                        from: 2; to: 20
                                        value: Number(w.draft.upTo) || 3
                                        onValueModified: w.set("upTo", value)
                                    }
                                }
                                Label { text: qsTr("Required") }
                                Switch {
                                    objectName: "builderRequired"
                                    checked: w.draft.required ?? false
                                    onToggled: w.set("required", checked)
                                }
                                Label { text: qsTr("Light, Extra, On the side") }
                                Switch {
                                    checked: w.draft.askHow ?? false
                                    onToggled: w.set("askHow", checked)
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                opacity: 0.7
                                font.pixelSize: 14
                                text: qsTr("Required: the order can't go without a choice. Light, Extra, On the side: guests can ask for a choice that way (dressing on the side), not for temperatures or sizes.")
                            }
                            Label { text: qsTr("Options"); font.pixelSize: 18; font.bold: true }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Label { Layout.fillWidth: true; text: qsTr("Name"); opacity: 0.7; font.pixelSize: 13 }
                                Label { Layout.preferredWidth: 90; text: qsTr("Adds"); opacity: 0.7; font.pixelSize: 13 }
                                Label { Layout.preferredWidth: 110; text: qsTr("Comes on it"); opacity: 0.7; font.pixelSize: 13 }
                                Item { Layout.preferredWidth: 3 * 52 + 12 }
                            }
                            // Counted: typing changes the draft, not the rows.
                            Repeater {
                                model: (w.draft.options ?? []).length
                                delegate: RowLayout {
                                    id: optionRow
                                    required property int index
                                    readonly property var option: (w.draft.options ?? [])[index] ?? ({})
                                    Layout.fillWidth: true
                                    spacing: 6
                                    TextField {
                                        objectName: "builderOption-" + optionRow.index
                                        Layout.fillWidth: true
                                        text: optionRow.option.name ?? ""
                                        placeholderText: qsTr("e.g. Pico de gallo")
                                        onTextEdited: w.setOption(optionRow.index, "name", text)
                                    }
                                    TextField {
                                        objectName: "builderOptionPrice-" + optionRow.index
                                        Layout.preferredWidth: 90
                                        text: optionRow.option.price ?? ""
                                        placeholderText: "0.00"
                                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                                        onTextEdited: w.setOption(optionRow.index, "price", text)
                                    }
                                    Switch {
                                        Layout.preferredWidth: 110
                                        checked: optionRow.option.included ?? false
                                        onToggled: w.setOption(optionRow.index, "included", checked)
                                    }
                                    TouchButton { Layout.preferredWidth: 52; text: "▲"; enabled: optionRow.index > 0; onClicked: w.moveOption(optionRow.index, -1) }
                                    TouchButton {
                                        Layout.preferredWidth: 52
                                        text: "▼"
                                        enabled: optionRow.index < (w.draft.options ?? []).length - 1
                                        onClicked: w.moveOption(optionRow.index, 1)
                                    }
                                    TouchButton {
                                        Layout.preferredWidth: 52
                                        text: "✕"
                                        onClicked: {
                                            const d = w.copy(w.draft)
                                            d.options.splice(optionRow.index, 1)
                                            w.draft = d
                                        }
                                    }
                                }
                            }
                            TouchButton {
                                objectName: "builderAddOption"
                                text: qsTr("+ Option")
                                onClicked: {
                                    const d = w.copy(w.draft)
                                    d.options.push({ name: "", price: "", included: false })
                                    w.draft = d
                                }
                            }
                            // An option can cost more on a bigger size (extra cheese on a Large).
                            ColumnLayout {
                                visible: w.sizeNames.length > 0 && (w.draft.options ?? []).some(o => (o.name ?? "").trim() !== "")
                                Layout.fillWidth: true
                                spacing: 6
                                Label { text: qsTr("Prices by size"); font.pixelSize: 18; font.bold: true }
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                    opacity: 0.7
                                    font.pixelSize: 14
                                    text: qsTr("On an item with sizes. Empty: the price above.")
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Item { Layout.fillWidth: true }
                                    Repeater {
                                        model: w.sizeNames
                                        delegate: Label {
                                            required property string modelData
                                            Layout.preferredWidth: 84
                                            text: modelData
                                            opacity: 0.7
                                            font.pixelSize: 13
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                                // Counted: typing changes the draft, not the rows.
                                Repeater {
                                    model: (w.draft.options ?? []).length
                                    delegate: RowLayout {
                                        id: sizedRow
                                        required property int index
                                        readonly property var option: (w.draft.options ?? [])[index] ?? ({})
                                        visible: (option.name ?? "").trim() !== "" && !option.included
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Label { Layout.fillWidth: true; text: sizedRow.option.name ?? ""; elide: Text.ElideRight }
                                        Repeater {
                                            model: w.sizeNames
                                            delegate: TextField {
                                                required property string modelData
                                                objectName: "builderSizePrice-" + sizedRow.index + "-" + modelData
                                                Layout.preferredWidth: 84
                                                text: (sizedRow.option.sizePrices ?? {})[modelData.toLowerCase()] ?? ""
                                                placeholderText: sizedRow.option.price || "0.00"
                                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                                onTextEdited: w.setSizePrice(sizedRow.index, modelData, text)
                                            }
                                        }
                                    }
                                }
                            }
                            Label {
                                visible: !!w.editedGroup
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                opacity: 0.7
                                font.pixelSize: 14
                                text: w.editedGroup ? (w.editedGroup.usedBy.length
                                                       ? qsTr("Asked for by: %1").arg(w.editedGroup.usedBy.join(", "))
                                                       : qsTr("No item asks for it yet: choose it on an item's card."))
                                                    : ""
                            }
                        }

                        // --- a category ---
                        ColumnLayout {
                            visible: w.editingCategory
                            Layout.fillWidth: true
                            spacing: 10
                            Label { text: w.draft.id ? qsTr("Category") : qsTr("New category"); font.pixelSize: 22; font.bold: true }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 10
                                rowSpacing: 8
                                Label { text: qsTr("Name") }
                                TextField {
                                    objectName: "builderCategoryName"
                                    Layout.fillWidth: true
                                    text: w.draft.name ?? ""
                                    placeholderText: qsTr("e.g. Tacos")
                                    onTextEdited: w.set("name", text)
                                }
                                Label { text: qsTr("Color"); Layout.columnSpan: 2 }
                                ColorPicker {
                                    objectName: "builderCategoryColor"
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    pos: w.pos
                                    color: w.draft.color ?? ""
                                    onPicked: c => w.set("color", c)
                                }
                                Label { text: qsTr("On the menu") }
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    TouchButton {
                                        checkable: true
                                        checked: (w.draft.periods ?? []).length === 0
                                        highlighted: checked
                                        text: qsTr("All day")
                                        onClicked: w.set("periods", [])
                                    }
                                    Repeater {
                                        model: w.periods
                                        delegate: TouchButton {
                                            required property var modelData
                                            checkable: true
                                            checked: (w.draft.periods ?? []).includes(modelData.id)
                                            highlighted: checked
                                            text: modelData.name
                                            onClicked: w.toggleIn("periods", modelData.id)
                                        }
                                    }
                                }
                            }
                            // How its items look on the order screen.
                            Label { text: qsTr("Its buttons on the order screen"); font.pixelSize: 16; font.bold: true }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 10
                                rowSpacing: 8
                                Label { text: qsTr("Size") }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Repeater {
                                        model: [{ id: "", name: qsTr("Fit") }, { id: "small", name: qsTr("Small") },
                                                { id: "medium", name: qsTr("Medium") }, { id: "large", name: qsTr("Large") }]
                                        delegate: TouchButton {
                                            required property var modelData
                                            objectName: "builderButtonSize-" + (modelData.id || "fit")
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: 1
                                            font.pixelSize: 15
                                            checkable: true
                                            checked: (w.draft.buttonSize ?? "") === modelData.id
                                            highlighted: checked
                                            text: modelData.name
                                            onClicked: w.set("buttonSize", modelData.id)
                                        }
                                    }
                                }
                                Label {
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                    opacity: 0.7
                                    font.pixelSize: 13
                                    text: (w.draft.buttonSize ?? "") === "" ? qsTr("Fit: as big as they can be with all of them showing.")
                                        : qsTr("Many small ones for drinks, a few large ones for plates; more scroll.")
                                }
                                // Its items' names' size (an item can have its own).
                                Label { text: qsTr("Text size"); Layout.columnSpan: 2 }
                                Flow {
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Repeater {
                                        model: w.textSizes(qsTr("Normal")).filter(o => o.pct !== 100)
                                        delegate: TouchButton {
                                            required property var modelData
                                            objectName: "builderCategoryTextSize-" + modelData.pct
                                            font.pixelSize: 14
                                            checkable: true
                                            checked: (w.draft.textSize ?? 0) === modelData.pct
                                            highlighted: checked
                                            text: modelData.name
                                            onClicked: w.set("textSize", modelData.pct)
                                        }
                                    }
                                }
                                Label { text: qsTr("Photos") }
                                Switch {
                                    objectName: "builderCategoryPhotos"
                                    checked: w.draft.photos === true
                                    onToggled: w.set("photos", checked)
                                }
                                Label { text: qsTr("Shades") }
                                Switch {
                                    objectName: "builderCategoryShades"
                                    checked: w.draft.shades === true
                                    onToggled: w.set("shades", checked)
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Its items take shades of its color, each a little different (an item's own color still wins).")
                                }
                                Label { text: qsTr("Prices") }
                                Switch {
                                    objectName: "builderCategoryPrices"
                                    checked: w.draft.hidePrice !== true
                                    onToggled: w.set("hidePrice", !checked)
                                }
                            }
                            Label { text: qsTr("New items in it start with"); font.pixelSize: 16; font.bold: true }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 10
                                rowSpacing: 8
                                Label { text: qsTr("Kitchen ticket") }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: w.printers.map(p => p.name)
                                    currentIndex: w.printers.findIndex(p => p.id === w.draft.printer)
                                    onActivated: i => w.set("printer", w.printers[i].id)
                                }
                                Label { text: qsTr("Made at") }
                                ComboBox {
                                    Layout.fillWidth: true
                                    readonly property var opts: [{ id: "", name: qsTr("(anywhere)") }].concat(w.stations)
                                    model: opts.map(s => s.name)
                                    currentIndex: Math.max(0, opts.findIndex(s => s.id === (w.draft.station ?? "")))
                                    onActivated: i => w.set("station", opts[i].id)
                                }
                                Label { text: qsTr("Tax") }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: w.taxes.map(t => t.name)
                                    currentIndex: Math.max(0, w.taxes.findIndex(t => t.id === (w.draft.taxClass || "food")))
                                    onActivated: i => w.set("taxClass", w.taxes[i].id)
                                }
                            }

                        }
                    }
                }
                // Save and the rest: always in view, under the card.
                ColumnLayout {
                    id: cardButtons
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 14 }
                    visible: w.editingItem || w.editingCategory || w.editingGroup
                    height: visible ? implicitHeight : 0
                    Label {
                        objectName: "builderNotSaved"
                        visible: w.unsaved()
                        text: qsTr("● Not saved yet")
                        color: "#f5b940"
                        font.pixelSize: 14
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: w.editingGroup
                        spacing: 8
                        TouchButton {
                            objectName: "builderSaveGroup"
                            Layout.fillWidth: true
                            highlighted: true
                            text: w.draft.id ? qsTr("Save") : qsTr("Add Choice Group")
                            enabled: (w.draft.name ?? "").trim() !== ""
                            onClicked: w.saveGroupCard()
                        }
                        TouchButton {
                            objectName: "builderBackToItem"
                            visible: !!w.returnToItem
                            text: qsTr("‹ %1").arg(w.returnToItem ? (w.returnToItem.name || qsTr("New item")) : "")
                            onClicked: w.backToItem("")
                        }
                        TouchButton {
                            visible: !!w.draft.id
                            text: qsTr("Remove…")
                            onClicked: removeGroupDialog.open()
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: w.editingItem
                        spacing: 8
                        TouchButton {
                            objectName: "builderSave"
                            Layout.fillWidth: true
                            highlighted: true
                            text: w.draft.id ? qsTr("Save") : qsTr("Add to the Menu")
                            enabled: (w.draft.name ?? "").trim() !== ""
                            onClicked: w.saveItem()
                        }
                        TouchButton {
                            objectName: "builderDuplicate"
                            visible: !!w.draft.id
                            text: qsTr("Duplicate")
                            onClicked: {
                                w.idsBefore = w.allItems.map(i => i.id)
                                w.pos.duplicateMenuItem(w.draft.id)
                            }
                        }
                        TouchButton {
                            objectName: "builderRemove"
                            visible: !!w.draft.id
                            text: qsTr("Remove…")
                            onClicked: removeDialog.open()
                        }
                        TouchButton {
                            visible: !!w.draft.id
                            text: qsTr("More…")
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Every setting: delivery price, recipe, by weight, discounts…")
                            onClicked: {
                                const b = w
                                b.leave(() => b.zone.controller.jumpTo("admin-menu"))
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: w.editingCategory
                        spacing: 8
                        TouchButton {
                            objectName: "builderSaveCategory"
                            Layout.fillWidth: true
                            highlighted: true
                            text: w.draft.id ? qsTr("Save") : qsTr("Add Category")
                            enabled: (w.draft.name ?? "").trim() !== ""
                            onClicked: w.saveCategoryCard()
                        }
                        TouchButton {
                            visible: !!w.draft.id
                            text: qsTr("Remove")
                            enabled: !!w.category && w.category.count === 0
                            ToolTip.visible: hovered && !enabled
                            ToolTip.text: qsTr("Move or remove its items first.")
                            onClicked: {
                                w.pos.deleteCategory(w.draft.id)
                                w.editingCategory = false
                            }
                        }
                    }
                }
            }
        }
    }

    // Many at once: "Carne Asada 3.50" a line, or "Tacos: Carne Asada 3.50, Al Pastor 3.25".
    // A spreadsheet (CSV or Excel), or Square's, Clover's or Toast's menu export
    // (a USB stick, this computer): read here, shown, then added.
    FileDialog {
        id: importFile
        title: qsTr("A menu or a spreadsheet")
        nameFilters: [qsTr("Menus and spreadsheets (*.xlsx *.csv *.tsv *.txt *.json)"), qsTr("All files (*)")]
        onAccepted: {
            importDialog.read = w.zone.controller.readMenuFile(selectedFile)
            importDialog.open()
        }
    }
    MenuBuilderImport {
        id: importDialog
        menu: w
    }
    MenuBuilderCheck {
        id: checkDialog
        menu: w
    }
    MenuBuilderStarter {
        id: templateDialog
        menu: w
    }
    Rectangle {
        id: ghost
        objectName: "builderGhost"
        property string text
        visible: w.dragId !== ""
        z: 100
        width: 200 * w.zoom
        height: 72 * w.zoom
        radius: 10
        opacity: 0.9
        border.color: "#f5b940"
        border.width: 3
        Label {
            anchors.centerIn: parent
            width: parent.width - 12
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: ghost.text
            color: StoreColors.ink(ghost.color)
            font.pixelSize: 17 * w.zoom
            font.bold: true
        }
    }

    MenuBuilderSeveral {
        id: severalDialog
        menu: w
    }
    MenuBuilderPrices {
        id: pricesDialog
        menu: w
    }
    MenuBuilderLike {
        id: likeDialog
        menu: w
    }
    MenuBuilderPrint {
        id: printDialog
        menu: w
    }
    MenuBuilderTheme {
        id: themeDialog
        menu: w
    }
    MenuBuilderSizes {
        id: sizesDialog
        menu: w
    }
    MenuBuilderPicture {
        id: pictureDialog
        menu: w
    }
    Popup {
        id: moveSeveral
        objectName: "builderMoveSeveral"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(parent ? parent.width - 32 : 520, 520)
        modal: true
        padding: 16
        contentItem: ColumnLayout {
            spacing: 6
            Label { text: qsTr("Move %n item(s) to", "", w.selected.length); font.pixelSize: 20; font.bold: true }
            Repeater {
                model: w.categories
                delegate: TouchButton {
                    required property var modelData
                    objectName: "builderMoveTo-" + modelData.id
                    Layout.fillWidth: true
                    text: modelData.name
                    palette.button: modelData.color || "#343c49"
                    palette.buttonText: StoreColors.ink(modelData.color || "#343c49")
                    onClicked: {
                        // Closed first: the move rebuilds these buttons (this one too).
                        const id = modelData.id, b = w
                        moveSeveral.close()
                        b.changeSelected({ family: id })
                    }
                }
            }
            TouchButton { Layout.fillWidth: true; text: qsTr("Cancel"); onClicked: moveSeveral.close() }
        }
    }
    Popup {
        id: colorSeveral
        objectName: "builderColorSeveral"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(parent ? parent.width - 32 : 600, 600)
        modal: true
        padding: 16
        contentItem: ColorPicker {
            pos: w.pos
            noneText: qsTr("Category's")
            onPicked: c => {
                colorSeveral.close()
                w.changeSelected({ buttonColor: c })
            }
        }
    }
    Dialog {
        id: removeSeveral
        objectName: "builderRemoveSeveral"
        title: qsTr("Remove %n item(s) from the menu?", "", w.selected.length)
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAboutToShow: DialogWords.apply(this)
        Label { text: qsTr("They come off every menu screen. Checks they're already on keep them.") }
        onAccepted: {
            const ids = w.selected
            w.pos.removeMenuItems(ids)
            for (const id of ids)
                w.zone.controller.removeItemButtons(id)
            w.selected = []
        }
    }

    // Leaving a card with changes: save them, drop them, or stay.
    Dialog {
        id: leaveDialog
        objectName: "builderLeaveDialog"
        property var then: null
        padding: 20
        font.pixelSize: 18
        title: qsTr("Save the changes to %1?").arg((w.draft.name ?? "").trim() || qsTr("this card"))
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        RowLayout {
            spacing: 8
            TouchButton {
                objectName: "builderLeaveSave"
                highlighted: true
                text: qsTr("Save")
                onClicked: {
                    const then = leaveDialog.then
                    leaveDialog.close()
                    w.saveCard()
                    w.waitingFor = ""; w.waitingForCategory = ""; w.waitingForGroup = ""
                    if (!w.unsaved() && then) then()   // not if the save was refused
                }
            }
            TouchButton {
                objectName: "builderLeaveDiscard"
                text: qsTr("Don't Save")
                onClicked: {
                    const then = leaveDialog.then
                    leaveDialog.close()
                    if (then) then()
                }
            }
            TouchButton { text: qsTr("Keep Editing"); onClicked: leaveDialog.close() }
        }
    }

    Dialog {
        id: removeGroupDialog
        title: qsTr("Remove %1?").arg(w.draft.name ?? "")
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAboutToShow: DialogWords.apply(this)
        Label {
            text: w.editedGroup && w.editedGroup.usedBy.length
                  ? qsTr("%1 stop asking for it.").arg(w.editedGroup.usedBy.join(", ")) : qsTr("No item asks for it.")
            wrapMode: Text.WordWrap
            width: 420
        }
        onAccepted: {
            w.pos.deleteChoiceGroup(w.draft.id)
            w.editingGroup = false
        }
    }

    Dialog {
        id: removeDialog
        title: qsTr("Remove %1 from the menu?").arg(w.draft.name ?? "")
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAboutToShow: DialogWords.apply(this)
        Label { text: qsTr("It comes off every menu screen. Checks it's already on keep it.") }
        onAccepted: {
            w.pos.deleteMenuItemCard(w.draft.id)
            w.zone.controller.removeItemButtons(w.draft.id)
            w.editingItem = false
            w.stage = "items"
        }
    }
}
