import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Dialogs
import QtQuick.Layouts

// Menu Builder: Changing prices: by how much, now or later; each new price shown first.
Dialog {
    id: pricesDialog
    property Item menu   // the WidgetMenuBuilder (not by its type: that would be a cycle)
    objectName: "builderPricesDialog"
    title: qsTr("Change prices")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent.width - 40, 760)
    height: Math.min(parent.height - 40, 720)
    modal: true
    property bool wholeMenu: false
    property bool lower: false
    property bool percent: true
    property int roundTo: 5          // cents: 1 (exact), 5, 25, 50
    property string amount: ""
    // Now, or later: a day from today (0 today, 1 tomorrow…) at a time.
    property bool later: false
    property int dayOffset: 1
    property string time: "06:00"
    onOpened: { amount = ""; wholeMenu = false; lower = false; later = false; dayOffset = 1; time = "06:00" }
    readonly property bool timeOk: /^([01]?\d|2[0-3]):[0-5]\d$/.test(time.trim())
    readonly property var when: {
        if (!timeOk) return null
        const d = new Date()
        d.setDate(d.getDate() + dayOffset)
        const hm = time.trim().split(":")
        d.setHours(Number(hm[0]), Number(hm[1]), 0, 0)
        return d
    }
    readonly property string whenText: !when ? ""
        : (dayOffset === 0 ? qsTr("today") : dayOffset === 1 ? qsTr("tomorrow")
           : Qt.locale().dayName(when.getDay(), Locale.LongFormat)) + " " + Qt.locale().toString(when, "h:mm AP")
    // What it's called on the list and in Undo: "Burgers +10%".
    readonly property string label: (wholeMenu ? qsTr("Whole menu") : (menu.category ? menu.category.name : ""))
                                     + " " + (lower ? "−" : "+") + (percent ? amount.trim() + "%"
                                                                      : (menu.pos ? menu.pos.currencySymbol : "$") + amount.trim())
    // Each item's new prices: its regular one, and its meal, takeout and
    // delivery prices the same way.
    readonly property var changes: {
        const by = Number(amount.trim().replace(",", "."))
        if (amount.trim() === "" || isNaN(by) || by <= 0) return []
        const adjust = text => {
            const was = Math.round(Number(text) * 100)
            if (!(was > 0)) return text   // free (or priced when rung in): left alone
            const now = percent ? was * (lower ? 1 - by / 100 : 1 + by / 100)
                                : was + (lower ? -1 : 1) * Math.round(by * 100)
            return (Math.max(0, Math.round(now / roundTo) * roundTo) / 100).toFixed(2)
        }
        const out = []
        for (const i of (wholeMenu ? menu.allItems : menu.items)) {
            const was = i.priceValue.toFixed(2)
            const c = { id: i.id, name: i.name, was: was, price: adjust(was), periodPrices: {} }
            let changed = c.price !== was
            for (const k of ["takeoutPrice", "deliveryPrice"])
                if (i[k]) { c[k] = adjust(i[k]); changed = changed || c[k] !== i[k] }
            for (const p of Object.keys(i.periodPrices ?? {})) {
                c.periodPrices[p] = adjust(i.periodPrices[p])
                changed = changed || c.periodPrices[p] !== i.periodPrices[p]
            }
            c.others = Object.keys(i.periodPrices ?? {}).length + (i.takeoutPrice ? 1 : 0) + (i.deliveryPrice ? 1 : 0)
            if (changed) out.push(c)
        }
        return out
    }
    component Pick: Button {
        implicitHeight: 52
        font.pixelSize: 16
        checkable: true
        highlighted: checked
        Layout.fillWidth: true
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Pick { text: menu.category ? menu.category.name : ""; checked: !pricesDialog.wholeMenu; onClicked: pricesDialog.wholeMenu = false }
            Pick { objectName: "builderPricesWhole"; text: qsTr("The whole menu"); checked: pricesDialog.wholeMenu; onClicked: pricesDialog.wholeMenu = true }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Pick { text: qsTr("Raise"); checked: !pricesDialog.lower; onClicked: pricesDialog.lower = false }
            Pick { objectName: "builderPricesLower"; text: qsTr("Lower"); checked: pricesDialog.lower; onClicked: pricesDialog.lower = true }
            TextField {
                objectName: "builderPricesAmount"
                Layout.preferredWidth: 120
                implicitHeight: 52
                font.pixelSize: 18
                text: pricesDialog.amount
                placeholderText: pricesDialog.percent ? "5" : "0.50"
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                onTextEdited: pricesDialog.amount = text
            }
            Pick { text: "%"; checked: pricesDialog.percent; onClicked: pricesDialog.percent = true }
            Pick { objectName: "builderPricesMoney"; text: menu.pos ? menu.pos.currencySymbol : "$"; checked: !pricesDialog.percent; onClicked: pricesDialog.percent = false }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Label { text: qsTr("Round to") }
            Repeater {
                model: [{ c: 1, t: qsTr("Exact") }, { c: 5, t: "0.05" }, { c: 25, t: "0.25" }, { c: 50, t: "0.50" }]
                delegate: Pick {
                    required property var modelData
                    text: modelData.t
                    checked: pricesDialog.roundTo === modelData.c
                    onClicked: pricesDialog.roundTo = modelData.c
                }
            }
        }
        // When: now, or a day and time to come (they change by themselves then).
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Pick { text: qsTr("Now"); checked: !pricesDialog.later; onClicked: pricesDialog.later = false }
            Pick { objectName: "builderPricesLater"; text: qsTr("Later…"); checked: pricesDialog.later; onClicked: pricesDialog.later = true }
        }
        Flow {
            visible: pricesDialog.later
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: 7
                delegate: Button {
                    required property int index
                    objectName: "builderPricesDay-" + index
                    implicitHeight: 48
                    font.pixelSize: 15
                    checkable: true
                    checked: pricesDialog.dayOffset === index
                    highlighted: checked
                    text: index === 0 ? qsTr("Today") : index === 1 ? qsTr("Tomorrow")
                        : Qt.locale().dayName(new Date(Date.now() + index * 86400000).getDay(), Locale.LongFormat)
                    onClicked: pricesDialog.dayOffset = index
                }
            }
            TextField {
                objectName: "builderPricesTime"
                width: 110
                implicitHeight: 48
                font.pixelSize: 18
                text: pricesDialog.time
                placeholderText: "06:00"
                inputMethodHints: Qt.ImhPreferNumbers
                onTextEdited: pricesDialog.time = text
            }
        }
        // Set for later already: each can be canceled.
        Repeater {
            model: menu.pos ? menu.pos.priceChanges : []
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 6
                Label {
                    Layout.fillWidth: true
                    text: "⏰ " + modelData.when + "  ·  " + modelData.label + "  ·  " + qsTr("%n price(s)", "", modelData.count)
                    elide: Text.ElideRight
                    font.pixelSize: 15
                }
                Button {
                    objectName: "builderPriceChangeCancel-" + modelData.id
                    implicitHeight: 44
                    text: qsTr("Cancel It")
                    onClicked: menu.pos.cancelPriceChange(modelData.id)
                }
            }
        }
        ListView {
            objectName: "builderPricesPreview"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: pricesDialog.changes
            delegate: RowLayout {
                required property var modelData
                width: ListView.view.width
                height: 34
                Label {
                    Layout.fillWidth: true
                    text: modelData.name + (modelData.others ? "  " + qsTr("(+%n other price(s))", "", modelData.others) : "")
                    elide: Text.ElideRight
                    font.pixelSize: 16
                }
                Label { text: modelData.was; opacity: 0.6; font.pixelSize: 16 }
                Label { text: "→"; opacity: 0.6; font.pixelSize: 16 }
                Label { Layout.preferredWidth: 80; horizontalAlignment: Text.AlignRight; text: modelData.price; font.bold: true; font.pixelSize: 16 }
            }
            Label {
                anchors.centerIn: parent
                width: parent.width - 40
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                visible: pricesDialog.changes.length === 0
                opacity: 0.6
                text: qsTr("Type how much; each new price shows here before anything changes. Items priced 0 are left alone.")
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            MenuTouchButton {
                objectName: "builderPricesApply"
                Layout.fillWidth: true
                highlighted: true
                enabled: pricesDialog.changes.length > 0 && (!pricesDialog.later || !!pricesDialog.when && pricesDialog.when > new Date())
                text: pricesDialog.later ? qsTr("Change %n price(s) %1", "", pricesDialog.changes.length).arg(pricesDialog.whenText)
                                         : qsTr("Change %n price(s)", "", pricesDialog.changes.length)
                onClicked: {
                    // Taken first: the list follows the prices, so it changes as they do.
                    const changes = pricesDialog.changes.map(c => {
                        const out = { id: c.id, price: c.price, periodPrices: c.periodPrices }
                        for (const k of ["takeoutPrice", "deliveryPrice"]) if (c[k] !== undefined) out[k] = c[k]
                        return out
                    })
                    if (pricesDialog.later) {
                        menu.pos.schedulePrices(changes, pricesDialog.when.getTime(), pricesDialog.label)
                        pricesDialog.close()
                        return
                    }
                    const open = menu.editingItem && menu.draft.id ? changes.find(x => x.id === menu.draft.id) : null
                    menu.pos.setMenuPrices(changes)
                    // The open card shows the new prices.
                    if (open) {
                        const d = menu.copy(menu.draft)
                        d.price = open.price
                        if (open.takeoutPrice !== undefined) d.takeoutPrice = open.takeoutPrice
                        d.periodPrices = Object.assign({}, d.periodPrices ?? {}, open.periodPrices)
                        menu.draft = d
                    }
                    pricesDialog.close()
                }
            }
            MenuTouchButton {
                Layout.preferredWidth: 160
                text: qsTr("Cancel")
                onClicked: pricesDialog.close()
            }
        }
    }
}

// Like…: the item to copy from; this category's first.
