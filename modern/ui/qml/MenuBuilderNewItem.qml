import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Menu Builder: a new item, one question at a time: its name and where on
// the menu, its price, the choices it asks for (or a new one), where its
// order goes, then a look before it's saved. It works on the builder's own
// draft, so All Settings opens the same item on the full card.
Rectangle {
    id: wiz
    property var menu   // the WidgetMenuBuilder (not by its type: that would be a cycle)
    objectName: "newItem"
    color: palette.window

    property int at: 0            // 0 name | 1 price | 2 choices | 3 where | 4 look | 5 saved
    property bool choosing: false  // making a new choice (MenuBuilderNewChoice)
    property bool free: false
    property bool sameNameOk: false
    property bool addingCategory: false
    property string waitingCategory: ""

    readonly property bool small: menu ? menu.narrow : false
    readonly property var draft: menu ? menu.draft : ({})
    readonly property string sym: menu && menu.pos ? (menu.pos.currencySymbol || "$") : "$"
    readonly property var category: menu ? (menu.categories.find(c => c.id === draft.family) ?? null) : null
    readonly property string name: String(draft.name ?? "").trim()
    readonly property var twin: !menu || name === "" ? null
        : (menu.allItems.find(i => i.name.toLowerCase() === name.toLowerCase()) ?? null)
    readonly property bool priceOk: /^\d+([.,]\d{0,2})?$/.test(String(draft.price ?? "").trim())
    readonly property real priceValue: priceOk ? Number(String(draft.price).trim().replace(",", ".")) : 0
    readonly property var chosen: menu ? (draft.groups ?? []).map(id => menu.groups.find(g => g.id === id)).filter(g => g) : []
    readonly property var printer: menu ? (menu.printers.find(p => p.id === draft.printer) ?? null) : null
    readonly property var station: menu ? (menu.stations.find(s => s.id === (draft.station ?? "")) ?? null) : null
    readonly property var saved: menu ? menu.wizardSaved : null
    onSavedChanged: if (saved) at = 5
    onAtChanged: flick.contentY = 0   // each question from its top

    function money(v) { return sym + Number(v).toFixed(2) }
    function optionsOf(g) { return menu.optionsOf(g) }
    readonly property string whereText: {
        const p = printer ? qsTranslate("Data", String(printer.name ?? "")) : ""
        const st = station ? qsTranslate("Data", String(station.name ?? "")) : ""
        return p !== "" && st !== "" ? qsTr("Its ticket prints on %1, and the %2 screen shows it.").arg(p).arg(st)
             : p !== "" ? qsTr("Its ticket prints on %1, and the %1 screen shows it.").arg(p)
             : st !== "" ? qsTr("The %1 screen shows it.").arg(st) : qsTr("Every kitchen screen shows it.")
    }

    function begin() {
        at = 0
        choosing = false
        free = false
        sameNameOk = false
        addingCategory = menu.categories.length === 0
        waitingCategory = ""
        nameField.text = draft.name ?? ""
        priceField.text = draft.price ?? ""
        newCategoryField.text = ""
        nameField.forceActiveFocus()
    }
    function pickCategory(c) {
        const d = menu.copy(menu.draft)
        d.family = c.id
        // what its category's items start with
        d.taxClass = c.taxClass || "food"
        d.printer = c.printer || "kitchen"
        d.station = c.station ?? ""
        menu.draft = d
        menu.categoryId = c.id
    }
    function addCategory() {
        const n = newCategoryField.text.trim()
        if (n === "") return
        const there = menu.categories.find(c => c.name.toLowerCase() === n.toLowerCase())
        if (there) {
            pickCategory(there)
        } else {
            const c = menu.categoryDraft(null)
            c.name = n
            waitingCategory = n
            menu.pos.saveCategory(c)
            categoryGiveUp.restart()
        }
        addingCategory = false
        newCategoryField.text = ""
    }
    // A category the store refused (it says why): Next can be touched again.
    Timer { id: categoryGiveUp; interval: 10000; onTriggered: wiz.waitingCategory = "" }
    Connections {
        target: wiz.menu
        function onCategoriesChanged() {
            if (wiz.waitingCategory === "") return
            const c = wiz.menu.categories.find(x => x.name.toLowerCase() === wiz.waitingCategory.toLowerCase())
            if (c) {
                wiz.waitingCategory = ""
                wiz.pickCategory(c)
            }
        }
    }
    function moveGroup(id, by) {
        const list = (draft.groups ?? []).slice()
        const i = list.indexOf(id), j = i + by
        if (i < 0 || j < 0 || j >= list.length) return
        list[i] = list[j]
        list[j] = id
        menu.set("groups", list)
    }
    // Why Next can't be touched yet; empty: it can.
    readonly property string missing: {
        if (at === 0) {
            if (name === "") return qsTr("Type its name first.")
            if (!category) return qsTr("Choose where on the menu it goes.")
            if (twin && !sameNameOk) return qsTr("There's already a %1 on the menu.").arg(twin.name)
        }
        if (at === 1) {
            if (free) return ""
            if (!priceOk) return qsTr("Type its price, like 4.50.")
            if (priceValue === 0) return qsTr("For something with no price, touch It's Free.")
        }
        return ""
    }
    function next() {
        if (missing !== "") return
        if (at < 4) {
            at += 1
            if (at === 1) priceField.forceActiveFocus()
            else wiz.forceActiveFocus()
            return
        }
        menu.saveWizard()
    }
    function back() {
        if (at > 0) { at -= 1; return }
        const m = menu
        m.leave(() => m.closeWizard(true))
    }

    // (Nothing under it is touched.)
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: w => w.accepted = true }

    MenuBuilderNewChoice {
        id: newChoice
        anchors { fill: parent; margins: wiz.small ? 10 : 24 }
        visible: wiz.choosing
        menu: wiz.menu
        itemName: wiz.name
        onMade: id => {
            if (!(wiz.draft.groups ?? []).includes(id))
                wiz.menu.set("groups", (wiz.draft.groups ?? []).concat([id]))
            wiz.choosing = false
        }
        onCancelled: wiz.choosing = false
    }

    ColumnLayout {
        anchors { fill: parent; margins: wiz.small ? 10 : 24 }
        visible: !wiz.choosing
        spacing: 12

        // Where they are: five dots, and the item's name once it has one.
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                elide: Text.ElideRight
                opacity: 0.7
                font.pixelSize: 15
                text: wiz.at === 5 ? "" : (wiz.at > 0 && wiz.name !== "" ? wiz.name + "  ·  " : qsTr("A new item") + "  ·  ")
                                          + qsTr("%1 of 5").arg(wiz.at + 1)
            }
            Row {
                visible: wiz.at < 5
                spacing: 6
                Repeater {
                    model: 5
                    delegate: Rectangle {
                        required property int index
                        width: 12; height: 12; radius: 6
                        color: index <= wiz.at ? Ui.accent : Qt.rgba(1, 1, 1, 0.25)
                    }
                }
            }
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: body.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: TouchScrollBar {}

            ColumnLayout {
                id: body
                width: flick.width - 14
                spacing: 12

                // --- 1: its name, and where on the menu ---
                ColumnLayout {
                    visible: wiz.at === 0
                    Layout.fillWidth: true
                    spacing: 12
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: wiz.small ? 22 : 28
                        font.bold: true
                        text: qsTr("What is it called?")
                    }
                    TextField {
                        id: nameField
                        objectName: "newItemName"
                        property string enterText: qsTr("Next ›")
                        EnterKey.type: Qt.EnterKeyNext
                        Layout.fillWidth: true
                        Layout.maximumWidth: 560
                        implicitHeight: 60
                        font.pixelSize: 22
                        placeholderText: qsTr("e.g. Fish Tacos")
                        onTextEdited: {
                            wiz.sameNameOk = false
                            wiz.menu.set("name", text)
                        }
                        onAccepted: wiz.next()
                    }
                    // The same name twice is allowed (a lunch and a dinner one), but said.
                    RowLayout {
                        visible: wiz.twin !== null && !wiz.sameNameOk
                        Layout.fillWidth: true
                        spacing: 8
                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: "#f5b940"
                            font.pixelSize: 15
                            text: wiz.twin ? qsTr("There's already a %1 on the menu. Change that one, or have two with the same name?").arg(wiz.twin.name) : ""
                        }
                        MenuTouchButton {
                            objectName: "newItemOpenTwin"
                            text: qsTr("Open It")
                            onClicked: {
                                const i = wiz.twin, m = wiz.menu
                                m.closeWizard(true)
                                m.categoryId = i.family
                                m.editItem(i)
                            }
                        }
                        MenuTouchButton {
                            objectName: "newItemSameName"
                            text: qsTr("Have Two")
                            onClicked: wiz.sameNameOk = true
                        }
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        wrapMode: Text.WordWrap
                        font.pixelSize: wiz.small ? 18 : 22
                        font.bold: true
                        text: qsTr("Where on the menu does it go?")
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: wiz.menu ? wiz.menu.categories : []
                            delegate: Rectangle {
                                id: chip
                                required property var modelData
                                objectName: "newItemCategory-" + modelData.id
                                readonly property bool on: wiz.draft.family === modelData.id
                                width: Math.max(140, chipText.implicitWidth + 36)
                                height: 60
                                radius: 6
                                color: modelData.color || "#3a4658"
                                border.color: on ? "#f5b940" : Qt.darker(color, 1.4)
                                border.width: on ? 4 : 1
                                Text {
                                    id: chipText
                                    textFormat: Text.PlainText
                                    anchors.centerIn: parent
                                    text: (chip.on ? "✓ " : "") + qsTranslate("Data", String(chip.modelData.name ?? ""))
                                    color: StoreColors.ink(chip.color)
                                    font.pixelSize: 18
                                    font.bold: true
                                }
                                MouseArea { anchors.fill: parent; onClicked: wiz.pickCategory(chip.modelData) }
                            }
                        }
                        MenuTouchButton {
                            objectName: "newItemAddCategory"
                            visible: !wiz.addingCategory
                            implicitHeight: 60
                            text: qsTr("+ New Category")
                            onClicked: {
                                wiz.addingCategory = true
                                newCategoryField.forceActiveFocus()
                            }
                        }
                    }
                    RowLayout {
                        visible: wiz.addingCategory
                        Layout.fillWidth: true
                        Layout.maximumWidth: 560
                        spacing: 8
                        TextField {
                            id: newCategoryField
                            objectName: "newItemCategoryName"
                            property string enterText: qsTr("Add")
                            EnterKey.type: Qt.EnterKeyNext
                            Layout.fillWidth: true
                            implicitHeight: 56
                            font.pixelSize: 18
                            placeholderText: qsTr("A new category, e.g. Tacos or Drinks")
                            onAccepted: wiz.addCategory()
                        }
                        MenuTouchButton {
                            objectName: "newItemCategoryAdd"
                            implicitHeight: 56
                            highlighted: true
                            enabled: newCategoryField.text.trim() !== ""
                            text: qsTr("Add")
                            onClicked: wiz.addCategory()
                        }
                    }
                }

                // --- 2: its price ---
                ColumnLayout {
                    visible: wiz.at === 1
                    Layout.fillWidth: true
                    spacing: 12
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: wiz.small ? 22 : 28
                        font.bold: true
                        text: qsTr("What does %1 cost?").arg(wiz.name)
                    }
                    RowLayout {
                        spacing: 10
                        Label { textFormat: Text.PlainText; text: wiz.sym; font.pixelSize: 30; font.bold: true; opacity: wiz.free ? 0.4 : 1 }
                        TextField {
                            id: priceField
                            objectName: "newItemPrice"
                            property string enterText: qsTr("Next ›")
                            EnterKey.type: Qt.EnterKeyNext
                            Layout.preferredWidth: 220
                            implicitHeight: 68
                            font.pixelSize: 30
                            enabled: !wiz.free
                            placeholderText: "0.00"
                            inputMethodHints: Qt.ImhDecimalNumbersOnly
                            onTextEdited: wiz.menu.set("price", text)
                            onAccepted: wiz.next()
                        }
                        MenuTouchButton {
                            objectName: "newItemFree"
                            implicitHeight: 68
                            checkable: true
                            checked: wiz.free
                            text: qsTr("It's Free")
                            onClicked: {
                                wiz.free = !wiz.free
                                if (wiz.free) {
                                    priceField.text = "0.00"
                                    wiz.menu.set("price", "0.00")
                                } else {
                                    priceField.text = ""
                                    wiz.menu.set("price", "")
                                    priceField.forceActiveFocus()
                                }
                            }
                        }
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        opacity: 0.75
                        font.pixelSize: 15
                        text: qsTr("Before tax. If it comes in sizes (Small, Large), type the smallest one's price: the sizes are added after it's saved.")
                    }
                }

                // --- 3: the choices it asks for ---
                ColumnLayout {
                    visible: wiz.at === 2
                    Layout.fillWidth: true
                    spacing: 10
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: wiz.small ? 22 : 28
                        font.bold: true
                        text: qsTr("Is there anything to choose with %1?").arg(wiz.name)
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        opacity: 0.75
                        font.pixelSize: 15
                        text: wiz.chosen.length === 0
                              ? qsTr("Like which salsa, what size, or extra cheese. Touch the ones it asks for; with none, it's rung in with one touch.")
                              : qsTr("It asks for these when it's ordered, in this order.")
                    }
                    MenuTouchButton {
                        objectName: "newItemNewChoice"
                        implicitHeight: 60
                        Layout.preferredWidth: wiz.small ? body.width : 320
                        text: qsTr("+ Make a New Choice")
                        onClicked: {
                            wiz.choosing = true
                            newChoice.begin()
                        }
                    }
                    Repeater {
                        model: wiz.menu ? wiz.menu.groups : []
                        delegate: Rectangle {
                            id: groupRow
                            required property var modelData
                            objectName: "newItemGroup-" + modelData.id
                            readonly property int place: (wiz.draft.groups ?? []).indexOf(modelData.id)
                            readonly property bool on: place >= 0
                            Layout.fillWidth: true
                            Layout.maximumWidth: 820
                            implicitHeight: Math.max(64, groupWords.implicitHeight + 20)
                            radius: 6
                            color: on ? Qt.rgba(Ui.accent.r, Ui.accent.g, Ui.accent.b, 0.35) : Qt.rgba(1, 1, 1, 0.06)
                            border.color: on ? Ui.accent : Qt.rgba(1, 1, 1, 0.18)
                            border.width: on ? 2 : 1
                            MouseArea { anchors.fill: parent; onClicked: wiz.menu.toggleIn("groups", groupRow.modelData.id) }
                            RowLayout {
                                anchors { fill: parent; leftMargin: 12; rightMargin: 8 }
                                spacing: 10
                                Label {
                                    textFormat: Text.PlainText
                                    Layout.preferredWidth: 30
                                    horizontalAlignment: Text.AlignHCenter
                                    font.pixelSize: 22
                                    font.bold: true
                                    text: groupRow.on ? "✓" : ""
                                }
                                Column {
                                    id: groupWords
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Label {
                                        textFormat: Text.PlainText
                                        width: parent.width
                                        elide: Text.ElideRight
                                        font.pixelSize: 18
                                        font.bold: true
                                        text: qsTranslate("Data", String(groupRow.modelData.name ?? "")) + "  ·  " + groupRow.modelData.rule
                                    }
                                    Label {
                                        textFormat: Text.PlainText
                                        width: parent.width
                                        elide: Text.ElideRight
                                        maximumLineCount: 2
                                        wrapMode: Text.WordWrap
                                        opacity: 0.8
                                        font.pixelSize: 14
                                        text: wiz.optionsOf(groupRow.modelData)
                                    }
                                }
                                // Asked earlier: one place up.
                                MenuTouchButton {
                                    objectName: "newItemGroupUp-" + groupRow.modelData.id
                                    visible: groupRow.place > 0
                                    Layout.preferredWidth: 56
                                    implicitHeight: 48
                                    text: "▲"
                                    onClicked: wiz.moveGroup(groupRow.modelData.id, -1)
                                }
                                Label {
                                    textFormat: Text.PlainText
                                    visible: groupRow.on && wiz.chosen.length > 1
                                    Layout.preferredWidth: 34
                                    horizontalAlignment: Text.AlignHCenter
                                    opacity: 0.8
                                    font.pixelSize: 15
                                    text: qsTr("#%1").arg(groupRow.place + 1)
                                }
                            }
                        }
                    }
                }

                // --- 4: where its order goes ---
                ColumnLayout {
                    visible: wiz.at === 3
                    Layout.fillWidth: true
                    spacing: 12
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: wiz.small ? 22 : 28
                        font.bold: true
                        text: qsTr("Where does the order for %1 go?").arg(wiz.name)
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: wiz.small ? 16 : 19
                        font.bold: true
                        text: qsTr("Its ticket prints on")
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            // The kitchen's and the bar's first; the receipt printer (at the
                            // counter) last: a small place may print everything there.
                            model: wiz.menu ? wiz.menu.printers.filter(p => p.id !== "receipt")
                                                  .concat(wiz.menu.printers.filter(p => p.id === "receipt")) : []
                            delegate: MenuTouchButton {
                                required property var modelData
                                objectName: "newItemPrinter-" + modelData.id
                                readonly property bool usual: wiz.category !== null && (wiz.category.printer || "kitchen") === modelData.id
                                implicitHeight: 76
                                implicitWidth: wiz.small ? body.width : 220
                                lines: 2
                                checkable: true
                                checked: wiz.draft.printer === modelData.id
                                text: qsTranslate("Data", String(modelData.name ?? ""))
                                      + (usual ? "\n" + qsTr("usual for %1").arg(qsTranslate("Data", String(wiz.category.name ?? "")))
                                         : modelData.id === "receipt" ? "\n" + qsTr("the one at the counter") : "")
                                onClicked: wiz.menu.set("printer", modelData.id)
                            }
                        }
                    }
                    Label {
                        textFormat: Text.PlainText
                        visible: stationChips.visible
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        wrapMode: Text.WordWrap
                        font.pixelSize: wiz.small ? 16 : 19
                        font.bold: true
                        text: qsTr("Who makes it? (their kitchen screen shows it)")
                    }
                    Flow {
                        id: stationChips
                        visible: wiz.menu ? wiz.menu.stations.length > 0 : false
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: wiz.menu ? [{ id: "", name: qsTr("Anyone") }].concat(wiz.menu.stations) : []
                            delegate: MenuTouchButton {
                                required property var modelData
                                objectName: "newItemStation-" + (modelData.id || "any")
                                readonly property bool usual: wiz.category !== null && modelData.id !== ""
                                                              && wiz.category.station === modelData.id
                                implicitHeight: 76
                                implicitWidth: wiz.small ? body.width : 220
                                lines: 2
                                checkable: true
                                checked: (wiz.draft.station ?? "") === modelData.id
                                text: (modelData.id === "" ? modelData.name : qsTranslate("Data", String(modelData.name ?? "")))
                                      + (usual ? "\n" + qsTr("usual for %1").arg(qsTranslate("Data", String(wiz.category.name ?? ""))) : "")
                                onClicked: wiz.menu.set("station", modelData.id)
                            }
                        }
                    }
                    Label {
                        textFormat: Text.PlainText
                        objectName: "newItemWhere"
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        wrapMode: Text.WordWrap
                        opacity: 0.85
                        font.pixelSize: 16
                        text: wiz.whereText
                    }
                }

                // --- 5: a look before it's saved ---
                ColumnLayout {
                    visible: wiz.at === 4
                    Layout.fillWidth: true
                    spacing: 12
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: wiz.small ? 22 : 28
                        font.bold: true
                        text: qsTr("Is this right?")
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: wiz.small ? 1 : 2
                        columnSpacing: 24
                        rowSpacing: 12
                        // Its button, as on the order screen.
                        Rectangle {
                            id: lookTile
                            Layout.preferredWidth: 220
                            Layout.preferredHeight: 130
                            Layout.alignment: Qt.AlignTop
                            radius: 8
                            color: wiz.category ? (wiz.category.color || "#3a4658") : "#3a4658"
                            border.color: Qt.darker(color, 1.4)
                            Column {
                                anchors.centerIn: parent
                                width: parent.width - 16
                                spacing: 4
                                Text {
                                    textFormat: Text.PlainText
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                    text: wiz.name
                                    color: StoreColors.ink(lookTile.color)
                                    font.pixelSize: 20
                                    font.bold: true
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    text: wiz.free || wiz.priceValue === 0 ? qsTr("Free") : wiz.money(wiz.priceValue)
                                    color: StoreColors.ink(lookTile.color)
                                    opacity: 0.9
                                    font.pixelSize: 17
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: [
                                    { at: 0, what: qsTr("Name"), text: wiz.name + (wiz.category ? "  ·  " + qsTr("in %1").arg(qsTranslate("Data", String(wiz.category.name ?? ""))) : "") },
                                    { at: 1, what: qsTr("Price"), text: wiz.free || wiz.priceValue === 0 ? qsTr("Free") : wiz.money(wiz.priceValue) },
                                    { at: 2, what: qsTr("Choices"), text: wiz.chosen.length ? wiz.chosen.map(g => qsTranslate("Data", String(g.name ?? ""))).join(", ")
                                                                                              : qsTr("None: one touch rings it in") },
                                    { at: 3, what: qsTr("Order"), text: wiz.whereText }]
                                delegate: Rectangle {
                                    id: lookRow
                                    required property var modelData
                                    objectName: "newItemLook-" + modelData.at
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: 640
                                    implicitHeight: Math.max(56, lookWords.implicitHeight + 16)
                                    radius: 6
                                    color: Qt.rgba(1, 1, 1, 0.06)
                                    border.color: Qt.rgba(1, 1, 1, 0.18)
                                    MouseArea { anchors.fill: parent; onClicked: wiz.at = lookRow.modelData.at }
                                    RowLayout {
                                        id: lookWords
                                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 12; rightMargin: 12 }
                                        spacing: 10
                                        Label { textFormat: Text.PlainText; Layout.preferredWidth: 78; opacity: 0.7; font.pixelSize: 15; text: lookRow.modelData.what }
                                        Label {
                                            textFormat: Text.PlainText
                                            Layout.fillWidth: true
                                            wrapMode: Text.WordWrap
                                            font.pixelSize: 17
                                            font.bold: true
                                            text: lookRow.modelData.text
                                        }
                                        Label { textFormat: Text.PlainText; opacity: 0.7; font.pixelSize: 15; text: qsTr("Change ›") }
                                    }
                                }
                            }
                        }
                    }
                }

                // --- saved ---
                ColumnLayout {
                    visible: wiz.at === 5
                    Layout.fillWidth: true
                    spacing: 14
                    Label {
                        textFormat: Text.PlainText
                        objectName: "newItemSaved"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: wiz.small ? 22 : 28
                        font.bold: true
                        color: "#7bd88f"
                        text: wiz.saved ? qsTr("✓ %1 is on the menu.").arg(wiz.saved.name) : ""
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        opacity: 0.8
                        font.pixelSize: 16
                        text: qsTr("Its button is on the order screen now. What next?")
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 10
                        MenuTouchButton {
                            objectName: "newItemAnother"
                            implicitHeight: 76
                            implicitWidth: wiz.small ? body.width : 260
                            lines: 2
                            highlighted: true
                            text: qsTr("Add Another") + "\n" + (wiz.category ? qsTr("in %1").arg(qsTranslate("Data", String(wiz.category.name ?? ""))) : "")
                            onClicked: wiz.menu.startWizard("where")
                        }
                        MenuTouchButton {
                            objectName: "newItemAnotherLike"
                            visible: wiz.saved ? (wiz.saved.groups ?? []).length > 0 : false
                            implicitHeight: 76
                            implicitWidth: wiz.small ? body.width : 260
                            lines: 2
                            text: qsTr("Add Another Like It") + "\n" + qsTr("with the same choices")
                            onClicked: wiz.menu.startWizard("choices")
                        }
                        MenuTouchButton {
                            objectName: "newItemSizes"
                            implicitHeight: 76
                            implicitWidth: wiz.small ? body.width : 260
                            lines: 2
                            text: qsTr("It Comes in Sizes…") + "\n" + qsTr("Small, Large, each with its price")
                            onClicked: {
                                const i = wiz.menu.allItems.find(x => x.id === wiz.saved.id)
                                wiz.menu.closeWizard(false)
                                if (i) wiz.menu.openSizes(i)
                            }
                        }
                    }
                }
            }
        }

        Label {
            textFormat: Text.PlainText
            objectName: "newItemMissing"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignRight
            visible: text !== "" && wiz.at < 5
            color: "#f5b940"
            font.pixelSize: 15
            text: wiz.missing
        }
        // Back, All Settings and Next in a row; a phone: All Settings above them.
        GridLayout {
            Layout.fillWidth: true
            columns: wiz.small ? 2 : 4
            columnSpacing: 8
            rowSpacing: 8
            MenuTouchButton {
                objectName: "newItemBack"
                visible: wiz.at < 5
                implicitHeight: 60
                Layout.row: wiz.small ? 1 : 0
                Layout.column: 0
                Layout.preferredWidth: wiz.small ? 110 : 160
                text: wiz.at === 0 ? qsTr("Cancel") : qsTr("‹ Back")
                onClicked: wiz.back()
            }
            // Every setting an item has, on its card, with what's typed so far.
            MenuTouchButton {
                objectName: "newItemAll"
                visible: wiz.at < 5
                implicitHeight: wiz.small ? 48 : 60
                Layout.row: 0
                Layout.column: wiz.small ? 0 : 1
                Layout.columnSpan: wiz.small ? 2 : 1
                Layout.alignment: Qt.AlignLeft
                flat: true
                font.pixelSize: 14
                text: qsTr("All Settings")
                onClicked: wiz.menu.closeWizard(false)
            }
            Item { visible: !wiz.small; Layout.row: 0; Layout.column: 2; Layout.fillWidth: true }
            MenuTouchButton {
                objectName: "newItemNext"
                implicitHeight: 60
                Layout.row: wiz.small ? 1 : 0
                Layout.column: wiz.small ? 1 : 3
                Layout.fillWidth: wiz.small
                Layout.preferredWidth: wiz.small ? 150 : 260
                highlighted: true
                enabled: wiz.at === 5 || (wiz.missing === "" && wiz.waitingCategory === "" && !(wiz.menu && wiz.menu.wizardIds))
                text: wiz.at === 5 ? qsTr("Done") : wiz.at === 4 ? qsTr("Save") : wiz.at === 2 && wiz.chosen.length === 0 ? qsTr("No Choices ›") : qsTr("Next ›")
                onClicked: {
                    if (wiz.at === 5) wiz.menu.closeWizard(false)
                    else wiz.next()
                }
            }
        }
    }
}
