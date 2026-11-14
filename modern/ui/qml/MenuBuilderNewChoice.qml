import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Menu Builder: a new choice (Salsa: Mild, Medium, Hot) made in three
// questions while adding an item: what they choose, how many, the options
// and which of them cost extra. The choice group's card has the rest.
Item {
    id: nc
    property var menu   // the WidgetMenuBuilder (not by its type: that would be a cycle)
    objectName: "newChoice"
    // For which item ("Fish Tacos"), said in the questions.
    property string itemName: ""
    signal made(string id)
    signal cancelled()

    property int at: 0   // 0 what | 1 how many | 2 options
    property string name: ""
    property string kind: "one"   // one | any | upTo
    property int upTo: 2
    property bool required: true
    property var options: []
    property string waiting: ""
    onAtChanged: flick.contentY = 0   // each question from its top

    readonly property bool small: menu ? menu.narrow : false
    readonly property var examples: [
        { name: qsTr("Size"), kind: "one", required: true, options: [qsTr("Small"), qsTr("Medium"), qsTr("Large")] },
        { name: qsTr("Salsa"), kind: "one", required: true, options: [qsTr("Mild"), qsTr("Medium"), qsTr("Hot")] },
        { name: qsTr("Toppings"), kind: "any", required: false, options: [qsTr("Cheese"), qsTr("Bacon"), qsTr("Avocado")] },
        { name: qsTr("Side"), kind: "one", required: true, options: [qsTr("Fries"), qsTr("Salad"), qsTr("Rice")] },
        { name: qsTr("How cooked"), kind: "one", required: true, options: [qsTr("Rare"), qsTr("Medium"), qsTr("Well done")] }]

    function begin() {
        at = 0
        name = ""
        kind = "one"
        upTo = 2
        required = true
        options = [{ name: "", price: "" }, { name: "", price: "" }]
        waiting = ""
        nameField.text = ""
        nameField.forceActiveFocus()
    }
    function useExample(e) {
        name = e.name
        nameField.text = e.name
        kind = e.kind
        required = e.required
        options = e.options.map(n => ({ name: n, price: "" }))
    }
    function setOption(k, key, value) {
        const o = JSON.parse(JSON.stringify(options))
        o[k][key] = value
        options = o
    }
    readonly property var named: options.filter(o => (o.name ?? "").trim() !== "")
    readonly property bool taken: menu ? menu.allGroups.some(g => g.name.toLowerCase() === name.trim().toLowerCase()) : false
    function priceOk(p) { return String(p ?? "").trim() === "" || /^\d+([.,]\d{0,2})?$/.test(String(p).trim()) }
    // Why Next can't be touched yet; empty: it can.
    readonly property string missing: {
        if (at === 0)
            return name.trim() === "" ? qsTr("Give it a name first.")
                 : taken ? qsTr("There's already a choice called %1.").arg(name.trim()) : ""
        if (at === 2) {
            if (named.length < (kind === "one" ? 2 : 1))
                return kind === "one" ? qsTr("Name at least two options.") : qsTr("Name at least one option.")
            if (!named.every(o => priceOk(o.price)))
                return qsTr("An extra charge is a number, like 0.50.")
        }
        return ""
    }
    function next() {
        if (missing !== "") return
        if (at < 2) { at += 1; return }
        const max = kind === "one" ? 1 : kind === "upTo" ? Math.max(2, upTo) : 0
        waiting = name.trim()
        giveUp.restart()
        menu.pos.saveChoiceGroup({ id: "", name: name.trim(), min: required ? 1 : 0, max: max, askHow: false,
                                   options: named.map(o => ({ name: o.name.trim(), price: String(o.price ?? "").trim().replace(",", "."),
                                                              included: false })) })
    }
    // A choice the store refused (it says why): Add This Choice can be touched again.
    Timer { id: giveUp; interval: 10000; onTriggered: nc.waiting = "" }
    Connections {
        target: nc.menu
        function onAllGroupsChanged() {
            if (nc.waiting === "") return
            const g = nc.menu.allGroups.find(x => x.name.toLowerCase() === nc.waiting.toLowerCase())
            if (g) {
                nc.waiting = ""
                nc.made(g.id)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.7
            font.pixelSize: 15
            text: qsTr("A new choice · %1 of 3").arg(nc.at + 1)
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

                // --- what ---
                ColumnLayout {
                    visible: nc.at === 0
                    Layout.fillWidth: true
                    spacing: 12
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: nc.small ? 22 : 28
                        font.bold: true
                        text: qsTr("What do they choose?")
                    }
                    TextField {
                        id: nameField
                        objectName: "newChoiceName"
                        Layout.fillWidth: true
                        Layout.maximumWidth: 560
                        implicitHeight: 60
                        font.pixelSize: 22
                        placeholderText: qsTr("e.g. Salsa")
                        onTextEdited: nc.name = text
                        onAccepted: nc.next()
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        opacity: 0.75
                        font.pixelSize: 15
                        text: qsTr("Or start from one of these and change it:")
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: nc.examples
                            delegate: MenuTouchButton {
                                required property var modelData
                                required property int index
                                objectName: "newChoiceExample-" + index
                                implicitHeight: 56
                                text: modelData.name + ":  " + modelData.options.join(", ")
                                onClicked: nc.useExample(modelData)
                            }
                        }
                    }
                }

                // --- how many ---
                ColumnLayout {
                    visible: nc.at === 1
                    Layout.fillWidth: true
                    spacing: 12
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: nc.small ? 22 : 28
                        font.bold: true
                        text: qsTr("How many can they pick for %1?").arg(nc.name.trim())
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: [{ kind: "one", text: qsTr("Just one"), hint: qsTr("Mild or Hot") },
                                    { kind: "any", text: qsTr("As many as they like"), hint: qsTr("Cheese and Bacon and…") },
                                    { kind: "upTo", text: qsTr("Up to a number"), hint: qsTr("Any two sides") }]
                            delegate: MenuTouchButton {
                                required property var modelData
                                objectName: "newChoiceKind-" + modelData.kind
                                implicitHeight: 76
                                implicitWidth: nc.small ? body.width : 240
                                lines: 2
                                checkable: true
                                checked: nc.kind === modelData.kind
                                text: modelData.text + "\n" + modelData.hint
                                onClicked: nc.kind = modelData.kind
                            }
                        }
                    }
                    RowLayout {
                        visible: nc.kind === "upTo"
                        spacing: 8
                        MenuTouchButton { implicitWidth: 64; text: "−"; enabled: nc.upTo > 2; onClicked: nc.upTo -= 1 }
                        Label {
                            textFormat: Text.PlainText
                            objectName: "newChoiceUpTo"
                            font.pixelSize: 22
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            Layout.preferredWidth: 110
                            text: qsTr("Up to %1").arg(nc.upTo)
                        }
                        MenuTouchButton { implicitWidth: 64; text: "+"; enabled: nc.upTo < 20; onClicked: nc.upTo += 1 }
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        wrapMode: Text.WordWrap
                        font.pixelSize: nc.small ? 18 : 22
                        font.bold: true
                        text: qsTr("Do they have to choose?")
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        MenuTouchButton {
                            objectName: "newChoiceRequired"
                            implicitHeight: 76
                            implicitWidth: nc.small ? body.width : 300
                            lines: 2
                            checkable: true
                            checked: nc.required
                            text: qsTr("Yes") + "\n" + qsTr("It's always asked, and can't be skipped")
                            onClicked: nc.required = true
                        }
                        MenuTouchButton {
                            objectName: "newChoiceOptional"
                            implicitHeight: 76
                            implicitWidth: nc.small ? body.width : 300
                            lines: 2
                            checkable: true
                            checked: !nc.required
                            text: qsTr("No") + "\n" + qsTr("They can go on without choosing")
                            onClicked: nc.required = false
                        }
                    }
                }

                // --- the options ---
                ColumnLayout {
                    visible: nc.at === 2
                    Layout.fillWidth: true
                    spacing: 10
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: nc.small ? 22 : 28
                        font.bold: true
                        text: qsTr("What are the options for %1?").arg(nc.name.trim())
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        opacity: 0.75
                        font.pixelSize: 15
                        text: qsTr("One on each line. If one costs more, type how much it adds (like 0.50); leave it empty when it's free.")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 640
                        spacing: 8
                        Label { textFormat: Text.PlainText; Layout.fillWidth: true; opacity: 0.7; font.pixelSize: 14; text: qsTr("Option") }
                        Label { textFormat: Text.PlainText; Layout.preferredWidth: nc.small ? 96 : 150; opacity: 0.7; font.pixelSize: 14; text: qsTr("Costs extra") }
                        Item { Layout.preferredWidth: 56 }
                    }
                    // Counted: typing changes the rows, not how many (the fields stay).
                    Repeater {
                        model: nc.options.length
                        delegate: RowLayout {
                            id: optionRow
                            required property int index
                            readonly property var modelData: nc.options[index] ?? ({})
                            Layout.fillWidth: true
                            Layout.maximumWidth: 640
                            spacing: 8
                            TextField {
                                objectName: "newChoiceOption-" + optionRow.index
                                Layout.fillWidth: true
                                implicitHeight: 56
                                font.pixelSize: 18
                                text: optionRow.modelData.name ?? ""
                                placeholderText: optionRow.index === 0 ? qsTr("e.g. Mild") : optionRow.index === 1 ? qsTr("e.g. Hot") : ""
                                onTextEdited: nc.setOption(optionRow.index, "name", text)
                            }
                            TextField {
                                objectName: "newChoiceExtra-" + optionRow.index
                                Layout.preferredWidth: nc.small ? 96 : 150
                                implicitHeight: 56
                                font.pixelSize: 18
                                text: optionRow.modelData.price ?? ""
                                placeholderText: qsTr("Free")
                                inputMethodHints: Qt.ImhDecimalNumbersOnly
                                color: nc.priceOk(text) ? palette.text : "#ff8a80"
                                onTextEdited: nc.setOption(optionRow.index, "price", text)
                            }
                            MenuTouchButton {
                                Layout.preferredWidth: 56
                                implicitHeight: 56
                                enabled: nc.options.length > 1
                                text: "✕"
                                onClicked: nc.options = nc.options.filter((o, k) => k !== optionRow.index)
                            }
                        }
                    }
                    MenuTouchButton {
                        objectName: "newChoiceAddOption"
                        implicitHeight: 56
                        text: qsTr("+ Another Option")
                        onClicked: nc.options = nc.options.concat([{ name: "", price: "" }])
                    }
                }
            }
        }

        Label {
            textFormat: Text.PlainText
            objectName: "newChoiceMissing"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignRight
            visible: text !== ""
            color: "#f5b940"
            font.pixelSize: 15
            text: nc.missing
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            MenuTouchButton {
                objectName: "newChoiceBack"
                implicitHeight: 60
                Layout.preferredWidth: nc.small ? 100 : 160
                text: nc.at === 0 ? qsTr("Cancel") : qsTr("‹ Back")
                onClicked: {
                    if (nc.at === 0) nc.cancelled()
                    else nc.at -= 1
                }
            }
            Item { Layout.fillWidth: true }
            MenuTouchButton {
                objectName: "newChoiceNext"
                implicitHeight: 60
                Layout.preferredWidth: nc.small ? 170 : 260
                highlighted: true
                enabled: nc.missing === "" && nc.waiting === ""
                text: nc.at < 2 ? qsTr("Next ›") : qsTr("Add This Choice")
                onClicked: nc.next()
            }
        }
    }
}
