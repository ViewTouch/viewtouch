import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Employees: someone new, one question at a time: their name, what they do,
// and the PIN they log in with. Everything else is on their form (All
// Settings), which works on the same draft.
Rectangle {
    id: ne
    property var panel   // the WidgetAdminPanel (not by its type: that would be a cycle)
    objectName: "newEmployee"
    color: palette.window

    property int at: 0         // 0 name | 1 job | 2 PIN | 3 added
    property bool saving: false
    property string addedName: ""
    property string addedPin: ""
    onAtChanged: flick.contentY = 0   // each question from its top

    readonly property bool small: panel ? panel.narrow : false
    readonly property var draft: panel ? panel.draft : ({})
    readonly property string name: String(draft.name ?? "").trim()
    readonly property string role: String(draft.role ?? "")
    readonly property string pin: String(draft.pin ?? "")
    // Someone on the staff by that name already: said (not refused).
    readonly property bool twin: panel !== undefined && panel !== null && name !== ""
        && panel.records.some(r => String(r.name ?? "").trim().toLowerCase() === name.toLowerCase())
    readonly property bool boss: role === "manager" || role === "owner" || role === "admin"
    readonly property int shortest: boss ? 6 : 4
    readonly property var roles: {
        const f = panel ? panel.fields.find(x => x.path === "role") : null
        return f ? (f.options ?? []) : []
    }
    // What each job can do, in a line.
    function roleText(r) {
        switch (r) {
        case "server": return qsTr("Takes orders at tables and is paid tips")
        case "bartender": return qsTr("Takes orders and tabs at the bar")
        case "cashier": return qsTr("Orders and payments at the counter")
        case "host": return qsTr("Seats guests and keeps the waitlist")
        case "busser": return qsTr("Clocks in and out")
        case "driver": return qsTr("Takes deliveries out")
        case "manager": return qsTr("Everything, and reports, staff and settings")
        case "owner": return qsTr("Everything, and the store's own settings")
        default: return ""
        }
    }
    readonly property string missing: {
        if (at === 0 && name === "") return qsTr("Type their name first.")
        if (at === 1 && role === "") return qsTr("Touch what they do.")
        if (at === 2) {
            if (!/^[0-9]*$/.test(pin)) return qsTr("A PIN is only numbers.")
            if (pin.length < shortest || pin.length > 8)
                return boss ? qsTr("A manager's PIN is 6 to 8 numbers.") : qsTr("A PIN is 4 to 8 numbers.")
        }
        return ""
    }

    function set(key, value) {
        const d = panel.copy(panel.draft)
        d[key] = value
        panel.dirty = true
        panel.draft = d
    }
    function begin() {
        at = 0
        saving = false
        nameField.text = draft.name ?? ""
        pinField.text = ""
        nameField.forceActiveFocus()
    }
    // A PIN nobody has to think of (the store says so if it's taken).
    function makeUp() {
        let p = ""
        for (let k = 0; k < shortest; ++k)
            p += String(Math.floor(Math.random() * 10))
        pinField.text = p
        set("pin", p)
    }
    function next() {
        if (missing !== "") return
        if (at < 2) {
            at += 1
            if (at === 2) pinField.forceActiveFocus()
            else ne.forceActiveFocus()
            return
        }
        addedName = name
        addedPin = pin
        saving = true
        giveUp.restart()
        panel.save()
    }
    // Saved: the panel opens the new one's form.
    Connections {
        target: ne.panel
        function onIndexChanged() {
            if (ne.saving && ne.panel.index >= 0) {
                ne.saving = false
                giveUp.stop()
                ne.at = 3
            }
        }
    }
    // Refused (the store says why: a PIN someone has): try again.
    Timer { id: giveUp; interval: 10000; onTriggered: ne.saving = false }
    Connections {
        target: ne.panel ? ne.panel.pos : null
        function onNotice() { if (ne.saving && ne.panel.pending === "") ne.saving = false }
    }

    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: w => w.accepted = true }

    ColumnLayout {
        anchors { fill: parent; margins: ne.small ? 10 : 20 }
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            visible: ne.at < 3
            Label {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                elide: Text.ElideRight
                opacity: 0.7
                font.pixelSize: 15
                text: (ne.at > 0 && ne.name !== "" ? ne.name : qsTr("Someone new")) + "  ·  " + qsTr("%1 of 3").arg(ne.at + 1)
            }
            Row {
                spacing: 6
                Repeater {
                    model: 3
                    delegate: Rectangle {
                        required property int index
                        width: 12; height: 12; radius: 6
                        color: index <= ne.at ? Ui.accent : Qt.rgba(1, 1, 1, 0.25)
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

                // --- name ---
                ColumnLayout {
                    visible: ne.at === 0
                    Layout.fillWidth: true
                    spacing: 12
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: ne.small ? 22 : 26
                        font.bold: true
                        text: qsTr("What's their name?")
                    }
                    TextField {
                        id: nameField
                        objectName: "newEmployeeName"
                        property string enterText: qsTr("Next ›")
                        EnterKey.type: Qt.EnterKeyNext
                        Layout.fillWidth: true
                        Layout.maximumWidth: 520
                        implicitHeight: 56
                        font.pixelSize: 20
                        placeholderText: qsTr("e.g. Ana")
                        onTextEdited: ne.set("name", text)
                        onAccepted: ne.next()
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        opacity: 0.75
                        font.pixelSize: 15
                        text: qsTr("As it shows on checks and reports: a first name is enough.")
                    }
                    Label {
                        textFormat: Text.PlainText
                        objectName: "newEmployeeTwin"
                        visible: ne.twin
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "#f5b940"
                        font.pixelSize: 15
                        text: qsTr("There's already a %1 on the staff. Add a last initial (%1 M.) so checks and reports tell them apart.").arg(ne.name)
                    }
                }

                // --- job ---
                ColumnLayout {
                    visible: ne.at === 1
                    Layout.fillWidth: true
                    spacing: 10
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: ne.small ? 22 : 26
                        font.bold: true
                        text: qsTr("What does %1 do?").arg(ne.name)
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: ne.roles
                            delegate: PosButton {
                                required property var modelData
                                objectName: "newEmployeeRole-" + modelData.value
                                implicitHeight: 76
                                implicitWidth: ne.small ? body.width : Math.min(380, (body.width - 8) / 2)
                                font.pixelSize: 15
                                lines: 2
                                checkable: true
                                checked: ne.role === modelData.value
                                text: modelData.text + "\n" + ne.roleText(modelData.value)
                                onClicked: ne.set("role", modelData.value)
                            }
                        }
                    }
                }

                // --- PIN ---
                ColumnLayout {
                    visible: ne.at === 2
                    Layout.fillWidth: true
                    spacing: 12
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: ne.small ? 22 : 26
                        font.bold: true
                        text: qsTr("Which PIN will %1 log in with?").arg(ne.name)
                    }
                    RowLayout {
                        spacing: 10
                        TextField {
                            id: pinField
                            objectName: "newEmployeePin"
                            property string enterText: qsTr("Add")
                            EnterKey.type: Qt.EnterKeyNext
                            Layout.preferredWidth: 220
                            implicitHeight: 64
                            font.pixelSize: 28
                            font.letterSpacing: 4
                            maximumLength: 8
                            placeholderText: ne.boss ? "000000" : "0000"
                            inputMethodHints: Qt.ImhDigitsOnly
                            onTextEdited: ne.set("pin", text)
                            onAccepted: ne.next()
                        }
                        PosButton {
                            objectName: "newEmployeeMakeUp"
                            implicitHeight: 64
                            text: qsTr("Make One Up")
                            onClicked: ne.makeUp()
                        }
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        opacity: 0.75
                        font.pixelSize: 15
                        text: ne.boss ? qsTr("6 to 8 numbers: a manager can do the most, so theirs is harder to guess. Only they should know it.")
                                      : qsTr("4 to 8 numbers. Only they should know it; a manager can give them a new one any time.")
                    }
                }

                // --- added ---
                ColumnLayout {
                    visible: ne.at === 3
                    Layout.fillWidth: true
                    spacing: 14
                    Label {
                        textFormat: Text.PlainText
                        objectName: "newEmployeeAdded"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: ne.small ? 22 : 26
                        font.bold: true
                        color: "#7bd88f"
                        text: qsTr("✓ %1 can log in now.").arg(ne.addedName)
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 18
                        text: qsTr("Their PIN is %1. Tell them, then it's theirs to keep.").arg(ne.addedPin)
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 10
                        PosButton {
                            objectName: "newEmployeeAnother"
                            implicitHeight: 64
                            highlighted: true
                            text: qsTr("Add Someone Else")
                            onClicked: ne.panel.addEmployee()
                        }
                        PosButton {
                            objectName: "newEmployeeMore"
                            implicitHeight: 64
                            text: qsTr("Pay Rate and More…")
                            onClicked: ne.panel.asking = false
                        }
                    }
                }
            }
        }

        Label {
            textFormat: Text.PlainText
            objectName: "newEmployeeMissing"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignRight
            visible: text !== "" && ne.at < 3
            color: "#f5b940"
            font.pixelSize: 15
            text: ne.missing
        }
        GridLayout {
            Layout.fillWidth: true
            columns: ne.small ? 2 : 4
            columnSpacing: 8
            rowSpacing: 8
            PosButton {
                objectName: "newEmployeeBack"
                visible: ne.at < 3
                implicitHeight: 60
                Layout.row: ne.small ? 1 : 0
                Layout.column: 0
                Layout.preferredWidth: ne.small ? 110 : 150
                text: ne.at === 0 ? qsTr("Cancel") : qsTr("‹ Back")
                onClicked: {
                    if (ne.at > 0) { ne.at -= 1; return }
                    ne.panel.asking = false
                    ne.panel.index = -2
                    ne.panel.dirty = false
                }
            }
            // Every setting, on their form, with what's typed so far.
            PosButton {
                objectName: "newEmployeeAll"
                visible: ne.at < 3
                implicitHeight: ne.small ? 48 : 60
                Layout.row: 0
                Layout.column: ne.small ? 0 : 1
                Layout.columnSpan: ne.small ? 2 : 1
                Layout.alignment: Qt.AlignLeft
                flat: true
                font.pixelSize: 14
                text: qsTr("All Settings")
                onClicked: ne.panel.asking = false
            }
            Item { visible: !ne.small; Layout.row: 0; Layout.column: 2; Layout.fillWidth: true }
            PosButton {
                objectName: "newEmployeeNext"
                implicitHeight: 60
                Layout.row: ne.small ? 1 : 0
                Layout.column: ne.small ? 1 : 3
                Layout.fillWidth: ne.small
                Layout.preferredWidth: ne.small ? 150 : 240
                highlighted: true
                enabled: ne.at === 3 || (ne.missing === "" && !ne.saving)
                text: ne.at === 3 ? qsTr("Done") : ne.at === 2 ? qsTr("Add %1").arg(ne.name) : qsTr("Next ›")
                onClicked: {
                    if (ne.at === 3) ne.panel.asking = false
                    else ne.next()
                }
            }
        }
    }
}
