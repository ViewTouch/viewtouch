import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// An item changed (or a new one added) right from the order screen, by a
// manager: its name, price or sizes, its choices and what's on it, the
// kitchen's name for it, sold out and color. All Settings… opens it in the
// Menu Builder.
Popup {
    id: card
    objectName: "quickItemCard"
    property PosService pos
    property var controller
    // The item (from pos.menuItems), or null: a new one in `family`.
    property var item: null
    property string family: ""
    property string name: ""
    property string price: ""
    property bool soldOut: false
    property string color: ""
    property var sizes: []        // [{name, price}]; none: one size, at `price`
    property var groups: []       // the choice groups it asks for (not its own)
    property string onIt: ""      // "lettuce, tomato, onion"
    property string kitchenName: ""
    // As they were: only what changed is saved (What's on it rebuilt from
    // names would lose its extras' prices).
    property string was: "{}"
    function now() {
        return { sizes: namedSizes, groups: groups, onIt: onIt.split(",").map(x => x.trim()).filter(x => x !== ""),
                 kitchenName: kitchenName.trim() }
    }
    readonly property var namedSizes: sizes.filter(z => z.name.trim() !== "" || z.price.trim() !== "")
    readonly property var sharedGroups: pos ? pos.choiceGroups.filter(g => !g.own) : []
    // A new one, once saved: the item it became (All Settings… opens it).
    property string waitingFor: ""

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 680, 680)
    height: Math.min(parent ? parent.height - 32 : 900, contentItem.implicitHeight + 2 * padding)
    modal: true
    padding: 18

    function edit(i) {
        item = i
        family = i.family
        name = i.name
        price = i.priceValue.toFixed(2)
        soldOut = !i.availableSet
        color = i.buttonColor ?? ""
        sizes = (i.sizes ?? []).map(z => ({ name: z.name, price: z.price }))
        groups = (i.groups ?? []).slice()
        onIt = (i.onIt ?? []).join(", ")
        kitchenName = i.kitchenName ?? ""
        was = JSON.stringify(now())
        open()
    }
    function add(f) {
        item = null
        family = f
        name = ""
        price = ""
        soldOut = false
        color = ""
        sizes = []
        groups = []
        onIt = ""
        kitchenName = ""
        was = JSON.stringify(now())
        open()
        nameField.forceActiveFocus()
    }
    function priced(t) { return /^\s*\$?\s*\d+([.,]\d{1,2})?\s*$/.test(t) }
    readonly property bool ready: name.trim() !== ""
        && (namedSizes.length > 0 ? namedSizes.length >= 2 && namedSizes.every(z => z.name.trim() !== "" && priced(z.price))
                                   : priced(price))
    function setSize(i, field, value) {
        const list = sizes.slice()
        list[i] = Object.assign({}, list[i], { [field]: value })
        sizes = list
    }
    function save(thenOpenAll) {
        const c = { name: name.trim(), family: family, available: !soldOut, buttonColor: color }
        if (namedSizes.length === 0)
            c.price = price.trim()
        const before = JSON.parse(was)
        const after = now()
        for (const k of ["sizes", "groups", "onIt", "kitchenName"])
            if (JSON.stringify(after[k]) !== JSON.stringify(before[k]))
                c[k] = after[k]
        if (item) c.id = item.id
        if (thenOpenAll && controller)
            controller.keep("menuBuilderOpen", item ? item.id : name.trim())
        pos.saveMenuItemCard(c)
        close()
        if (thenOpenAll && controller)
            controller.jumpTo("menu-builder")
    }

    component Big: Button {
        implicitHeight: 56
        font.pixelSize: 17
    }

    contentItem: ColumnLayout {
        spacing: 10
        Label {
            text: card.item ? qsTr("Change %1").arg(card.item.name) : qsTr("New item")
            font.pixelSize: 22
            font.bold: true
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Flickable {
            id: fields
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: body.implicitHeight
            clip: true
            contentHeight: body.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { }
            ColumnLayout {
                id: body
                width: card.availableWidth - 12
                spacing: 10
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 8
                    Label { text: qsTr("Name"); font.pixelSize: 16 }
                    TextField {
                        id: nameField
                        objectName: "quickName"
                        Layout.fillWidth: true
                        implicitHeight: 52
                        font.pixelSize: 18
                        text: card.name
                        placeholderText: qsTr("e.g. Fish Tacos")
                        onTextEdited: card.name = text
                    }
                    Label { text: qsTr("Price"); font.pixelSize: 16 }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        TextField {
                            objectName: "quickPrice"
                            visible: card.sizes.length === 0
                            Layout.fillWidth: true
                            implicitHeight: 52
                            font.pixelSize: 18
                            text: card.price
                            placeholderText: "0.00"
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            onTextEdited: card.price = text
                        }
                        Label {
                            visible: card.sizes.length > 0
                            Layout.fillWidth: true
                            text: qsTr("By size, below")
                            opacity: 0.7
                            font.pixelSize: 16
                        }
                        Big {
                            objectName: "quickAddSize"
                            text: card.sizes.length === 0 ? qsTr("Sizes…") : qsTr("+ Size")
                            onClicked: card.sizes = card.sizes.length === 0
                                       ? [{ name: qsTr("Small"), price: card.price }, { name: qsTr("Large"), price: "" }]
                                       : card.sizes.concat([{ name: "", price: "" }])
                        }
                    }
                    // Each size and its whole price.
                    Item { visible: card.sizes.length > 0; width: 1; height: 1 }
                    ColumnLayout {
                        visible: card.sizes.length > 0
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: card.sizes.length    // a count: typing doesn't rebuild the rows
                            delegate: RowLayout {
                                required property int index
                                Layout.fillWidth: true
                                spacing: 6
                                TextField {
                                    objectName: "quickSizeName-" + index
                                    Layout.fillWidth: true
                                    implicitHeight: 48
                                    font.pixelSize: 17
                                    placeholderText: qsTr("e.g. Medium")
                                    text: card.sizes[index] ? card.sizes[index].name : ""
                                    onTextEdited: card.setSize(index, "name", text)
                                }
                                TextField {
                                    objectName: "quickSizePrice-" + index
                                    Layout.preferredWidth: 120
                                    implicitHeight: 48
                                    font.pixelSize: 17
                                    placeholderText: "0.00"
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    text: card.sizes[index] ? card.sizes[index].price : ""
                                    onTextEdited: card.setSize(index, "price", text)
                                }
                                Button {
                                    implicitWidth: 48
                                    implicitHeight: 48
                                    text: "✕"
                                    onClicked: {
                                        const list = card.sizes.slice()
                                        list.splice(index, 1)
                                        // One left: one size again, at its price.
                                        if (list.length === 1) {
                                            card.price = list[0].price
                                            card.sizes = []
                                        } else {
                                            card.sizes = list
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Label { text: qsTr("What's on it"); font.pixelSize: 16 }
                    TextField {
                        objectName: "quickOnIt"
                        Layout.fillWidth: true
                        implicitHeight: 52
                        font.pixelSize: 17
                        text: card.onIt
                        placeholderText: qsTr("lettuce, tomato, onion")
                        onTextEdited: card.onIt = text
                    }
                    Label { text: qsTr("Kitchen name"); font.pixelSize: 16 }
                    TextField {
                        objectName: "quickKitchenName"
                        Layout.fillWidth: true
                        implicitHeight: 52
                        font.pixelSize: 17
                        text: card.kitchenName
                        placeholderText: qsTr("What the kitchen sees (optional)")
                        onTextEdited: card.kitchenName = text
                    }
                    Item { width: 1; height: 1 }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Big {
                            objectName: "quickForSale"
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            checkable: true
                            checked: !card.soldOut
                            highlighted: checked
                            text: qsTr("For sale")
                            onClicked: card.soldOut = false
                        }
                        Big {
                            objectName: "quickSoldOut"
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            checkable: true
                            checked: card.soldOut
                            highlighted: checked
                            palette.button: checked ? "#7a2e2e" : undefined
                            text: qsTr("Sold out")
                            onClicked: card.soldOut = true
                        }
                    }
                }
                Label {
                    visible: card.sharedGroups.length > 0
                    text: qsTr("Choices it asks for")
                    font.pixelSize: 16
                }
                Flow {
                    visible: card.sharedGroups.length > 0
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: card.sharedGroups
                        delegate: Button {
                            required property var modelData
                            readonly property bool on: card.groups.includes(modelData.id)
                            objectName: "quickGroup-" + modelData.id
                            implicitHeight: 48
                            font.pixelSize: 15
                            highlighted: on
                            text: (on ? "✓ " : "") + modelData.name
                            onClicked: card.groups = on ? card.groups.filter(g => g !== modelData.id)
                                                        : card.groups.concat([modelData.id])
                        }
                    }
                }
                Label { text: qsTr("Color"); font.pixelSize: 16 }
                ColorPicker {
                    objectName: "quickColor"
                    Layout.fillWidth: true
                    pos: card.pos
                    noneText: qsTr("Category's")
                    color: card.color
                    onPicked: c => card.color = c
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 8
            Big {
                objectName: "quickSave"
                Layout.fillWidth: true
                highlighted: true
                enabled: card.ready
                text: card.item ? qsTr("Save") : qsTr("Add to the Menu")
                onClicked: card.save(false)
            }
            Big {
                objectName: "quickAll"
                enabled: card.ready
                text: qsTr("All Settings…")
                onClicked: card.save(true)
            }
            Big {
                text: qsTr("Cancel")
                onClicked: card.close()
            }
        }
    }
}
