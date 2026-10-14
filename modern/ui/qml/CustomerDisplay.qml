import QtQuick
import QtQuick.Layouts

// What the guest sees at the counter:
// - between guests: the store's logo and its messages / pictures in turn;
// - while ordering: each item with its choices, the total, their rewards
//   (or a way to join with their phone number);
// - when the cashier asks: tip choices, or a custom amount;
// - after paying: thank you, their change and points, and a receipt by
//   print or text.
Rectangle {
    id: d
    property PosService pos
    readonly property bool hasCheck: pos !== null && pos.hasCheck
    readonly property var lines: hasCheck ? pos.lines : []
    readonly property var totals: hasCheck ? pos.totals : ({})
    readonly property var prompt: pos ? pos.customerPrompt : ({})
    readonly property var loyalty: prompt.loyalty ?? ({})
    readonly property color accent: prompt.accent || "#2f6fd6"
    readonly property real unit: Math.max(14, Math.min(width, height * 1.6) / 40)
    // Pictures by ref ("store:logo.png", a path...): this screen's copy.
    function img(ref) { return pos ? (pos.imageRevision < 0 ? undefined : pos.imageUrl(ref)) : ref }
    readonly property string logo: !prompt.logo ? "" : img(prompt.logo)

    color: "#0f1318"

    // What was showing, for the thank-you once the check is closed.
    property var lastTotals: ({})
    property var lastLoyalty: ({})
    // (Only while they belong to a check: closing empties them, maybe
    // before hasCheck itself changes.)
    onTotalsChanged: if (totals.total !== undefined) lastTotals = totals
    onLoyaltyChanged: if (loyalty.earning !== undefined) lastLoyalty = loyalty
    property bool thanking: false
    property bool receiptDone: false
    Timer { id: thanks; interval: 20000; onTriggered: d.thanking = false }
    Connections {
        target: d.pos
        function onCheckClosed() { d.thanking = true; d.receiptDone = false; thanks.restart() }
    }
    onHasCheckChanged: if (hasCheck) { thanking = false; keypad.close() }

    // --- header: logo or name, and the check ---
    Rectangle {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: d.unit * 3.4
        color: d.accent
        Image {
            visible: d.logo !== ""
            anchors { left: parent.left; leftMargin: d.unit; verticalCenter: parent.verticalCenter }
            height: parent.height * 0.75
            fillMode: Image.PreserveAspectFit
            source: d.logo
        }
        Text {
            visible: d.logo === ""
            anchors { left: parent.left; leftMargin: d.unit; verticalCenter: parent.verticalCenter }
            text: d.pos ? d.pos.storeName : ""
            color: "white"
            font.pixelSize: d.unit * 1.6
            font.bold: true
        }
        Text {
            anchors { right: parent.right; rightMargin: d.unit; verticalCenter: parent.verticalCenter }
            visible: d.hasCheck
            text: (d.prompt.label ?? "") + (d.prompt.checkId ? "  ·  #" + d.prompt.checkId : "")
            color: "white"
            font.pixelSize: d.unit * 1.2
        }
    }

    Item {
        id: body
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom }

        // --- between guests: messages and pictures in turn ---
        Item {
            id: idle
            anchors.fill: parent
            visible: !d.hasCheck && !d.thanking
            readonly property var slides: d.prompt.slides ?? []
            property int index: 0
            Timer {
                interval: 7000
                running: idle.visible && idle.slides.length > 1
                repeat: true
                onTriggered: { fade.restart() }
            }
            SequentialAnimation {
                id: fade
                NumberAnimation { target: slide; property: "opacity"; to: 0; duration: 350 }
                ScriptAction { script: idle.index = (idle.index + 1) % Math.max(1, idle.slides.length) }
                NumberAnimation { target: slide; property: "opacity"; to: 1; duration: 350 }
            }
            Item {
                id: slide
                anchors.fill: parent
                anchors.margins: d.unit * 2
                readonly property string current: idle.slides.length ? idle.slides[idle.index % idle.slides.length] : ""
                readonly property bool picture: current.startsWith("image:")
                Image {
                    anchors.fill: parent
                    visible: slide.picture
                    fillMode: Image.PreserveAspectFit
                    source: !slide.picture ? "" : d.img(slide.current.slice(6))
                }
                ColumnLayout {
                    anchors.centerIn: parent
                    width: parent.width
                    visible: !slide.picture
                    spacing: d.unit
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Welcome!")
                        color: Qt.lighter(d.accent, 1.5)
                        font.pixelSize: d.unit * 1.6
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: slide.current !== "" ? slide.current : (d.pos ? d.pos.storeName : "")
                        color: "white"
                        font.pixelSize: d.unit * 2.4
                        font.bold: true
                    }
                }
            }
        }

        // --- thank you ---
        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width * 0.8
            visible: d.thanking && !d.hasCheck
            spacing: d.unit * 0.8
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Thank you!")
                color: "#7ee2a8"
                font.pixelSize: d.unit * 3.4
                font.bold: true
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                visible: d.lastTotals.hasChange ?? false
                text: qsTr("Your change: %1").arg(d.lastTotals.change ?? "")
                color: "white"
                font.pixelSize: d.unit * 2
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                visible: (d.lastLoyalty.member ?? "") !== "" && (d.lastLoyalty.earning ?? 0) > 0
                text: qsTr("You earned %1 points").arg(d.lastLoyalty.earning ?? 0)
                color: "#f5b940"
                font.pixelSize: d.unit * 1.4
            }
            // A receipt?
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: d.unit
                visible: !d.receiptDone
                spacing: d.unit * 0.6
                component Choice: Rectangle {
                    id: choice
                    property string label
                    signal picked()
                    implicitWidth: d.unit * 9
                    implicitHeight: d.unit * 3.4
                    radius: d.unit * 0.5
                    color: tap.pressed ? Qt.lighter(d.accent, 1.3) : "#2a313d"
                    Text { anchors.centerIn: parent; text: choice.label; color: "white"; font.pixelSize: d.unit * 1.2 }
                    TapHandler { id: tap; onTapped: choice.picked() }
                }
                Choice { label: qsTr("Print receipt"); onPicked: { d.pos.sendReceipt("print"); d.receiptDone = true } }
                Choice {
                    visible: d.prompt.canText ?? false
                    label: qsTr("Text me")
                    onPicked: keypad.ask(qsTr("Your phone number"), false, phone => { d.pos.sendReceipt("text", phone); d.receiptDone = true })
                }
                Choice { label: qsTr("No receipt"); onPicked: d.receiptDone = true }
            }
        }

        // --- the order ---
        RowLayout {
            anchors.fill: parent
            anchors.margins: d.unit * 1.2
            spacing: d.unit * 1.5
            visible: d.hasCheck

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: d.lines
                spacing: d.unit * 0.35
                onCountChanged: positionViewAtEnd()   // the newest item stays in view
                delegate: Column {
                    id: row
                    required property var modelData
                    width: ListView.view.width
                    visible: !modelData.comment
                    RowLayout {
                        width: parent.width
                        Text {
                            Layout.fillWidth: true
                            text: (row.modelData.quantity > 1 ? row.modelData.quantity + " × " : "") + row.modelData.name
                            color: row.modelData.voided ? "#e06c75" : "white"
                            font.strikeout: row.modelData.voided ?? false
                            font.pixelSize: d.unit * 1.25
                            elide: Text.ElideRight
                        }
                        Text {
                            text: row.modelData.voided ? qsTr("void") : row.modelData.price
                            color: row.modelData.voided ? "#e06c75" : "white"
                            font.pixelSize: d.unit * 1.25
                        }
                    }
                    Repeater {
                        model: row.modelData.modifiers ?? []
                        delegate: RowLayout {
                            required property var modelData
                            width: row.width
                            Text {
                                Layout.fillWidth: true
                                leftPadding: d.unit * 1.2
                                text: "+ " + (modelData.name ?? modelData)
                                color: "#9aa4b5"
                                font.pixelSize: d.unit * 0.95
                                elide: Text.ElideRight
                            }
                            // Included in the item's price above.
                            Text { text: modelData.price ? "+" + modelData.price : ""; color: "#9aa4b5"; font.pixelSize: d.unit * 0.95 }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.preferredWidth: parent.width * 0.4
                Layout.fillWidth: false
                Layout.fillHeight: true
                spacing: d.unit * 0.4

                // Rewards: their points, or join.
                Rectangle {
                    visible: d.loyalty.enabled ?? false
                    Layout.fillWidth: true
                    Layout.preferredHeight: d.unit * 4.4
                    radius: d.unit * 0.5
                    color: "#1b222c"
                    border.color: (d.loyalty.member ?? "") !== "" ? "#f5b940" : d.accent
                    border.width: 2
                    Column {
                        anchors.centerIn: parent
                        width: parent.width - d.unit
                        visible: (d.loyalty.member ?? "") !== ""
                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: (d.loyalty.named ?? false) ? qsTr("Hi %1!").arg(d.loyalty.member ?? "")
                                                             : qsTr("Welcome! (%1)").arg(d.loyalty.member ?? "")
                            color: "white"
                            font.pixelSize: d.unit * 1.2
                            fontSizeMode: Text.HorizontalFit
                            minimumPixelSize: d.unit * 0.7
                            font.bold: true
                        }
                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("%1 points · +%2 today").arg(d.loyalty.points ?? 0).arg(d.loyalty.earning ?? 0)
                            color: "#f5b940"
                            font.pixelSize: d.unit
                        }
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: (d.loyalty.member ?? "") === ""
                        text: qsTr("Earn rewards: touch to add your phone")
                        color: "white"
                        font.pixelSize: d.unit
                    }
                    TapHandler {
                        enabled: (d.loyalty.member ?? "") === ""
                        onTapped: keypad.ask(qsTr("Your phone number"), false, phone => d.pos.customerJoin(phone))
                    }
                }

                Item { Layout.fillHeight: true }

                component Line: RowLayout {
                    property alias label: l.text
                    property alias value: v.text
                    property real size: 1.15
                    property color ink: "white"
                    Layout.fillWidth: true
                    Text { id: l; Layout.fillWidth: true; color: "#c7cedb"; font.pixelSize: d.unit * parent.size }
                    Text { id: v; color: parent.ink; font.pixelSize: d.unit * parent.size }
                }
                Line { label: qsTr("Subtotal"); value: d.totals.items ?? "" }
                Line {
                    label: qsTr("Discounts & promotions")
                    value: d.totals.discounts ?? ""
                    visible: d.totals.hasDiscount ?? false
                    ink: "#7ee2a8"
                }
                Line { label: qsTr("Tax"); value: d.totals.tax ?? "" }
                Line { label: qsTr("Gratuity"); value: d.totals.gratuity ?? ""; visible: d.totals.hasGratuity ?? false }
                Line { label: qsTr("Tip"); value: d.prompt.tip || d.totals.tips || ""; visible: (d.prompt.tipChosen ?? false) || (d.totals.hasTips ?? false) }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 2; color: "#2c3442" }
                Line { label: qsTr("Total"); value: d.totals.total ?? ""; size: 2.3 }
                Line { label: qsTr("Paid"); value: d.totals.paid ?? ""; visible: d.hasCheck && d.pos.payments.length > 0 }
                Line { label: qsTr("Change"); value: d.totals.change ?? ""; visible: d.totals.hasChange ?? false; size: 1.7 }
            }
        }
    }

    // --- tip choices, when the cashier asks ---
    Rectangle {
        anchors.fill: parent
        visible: (d.prompt.askingTip ?? false) && !keypad.visible
        color: "#ee0f1318"
        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.88, d.unit * 52)
            spacing: d.unit
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Add a tip?")
                color: "white"
                font.pixelSize: d.unit * 2.6
                font.bold: true
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Total %1").arg(d.totals.total ?? "")
                color: "#9aa4b5"
                font.pixelSize: d.unit * 1.3
            }
            GridLayout {
                Layout.fillWidth: true
                columns: Math.min(4, Math.max(1, (d.prompt.choices ?? []).length))
                rowSpacing: d.unit * 0.6
                columnSpacing: d.unit * 0.6
                Repeater {
                    model: d.prompt.choices ?? []
                    delegate: Rectangle {
                        id: choice
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: d.unit * 6
                        radius: d.unit * 0.6
                        color: hit.pressed ? Qt.lighter(d.accent, 1.3) : d.accent
                        Column {
                            anchors.centerIn: parent
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: choice.modelData.percent + "%"
                                color: "white"
                                font.pixelSize: d.unit * 2.2
                                font.bold: true
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: choice.modelData.amount
                                color: "#e6ecff"
                                font.pixelSize: d.unit * 1.1
                            }
                        }
                        TapHandler { id: hit; onTapped: d.pos.customerTip("percent", choice.modelData.percent * 100) }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: d.unit * 0.6
                Repeater {
                    model: [{ label: qsTr("Custom amount"), custom: true }, { label: qsTr("No tip"), custom: false }]
                    delegate: Rectangle {
                        id: other
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: d.unit * 3.4
                        radius: d.unit * 0.6
                        color: otherTap.pressed ? "#3a414f" : "#2a313d"
                        Text { anchors.centerIn: parent; text: other.modelData.label; color: "white"; font.pixelSize: d.unit * 1.3 }
                        TapHandler {
                            id: otherTap
                            onTapped: other.modelData.custom
                                       ? keypad.ask(qsTr("Tip amount"), true, cents => d.pos.customerTip("amount", cents))
                                       : d.pos.customerTip("none", 0)
                        }
                    }
                }
            }
        }
    }

    // --- a keypad for the guest: a phone number, or an amount ---
    Rectangle {
        id: keypad
        anchors.fill: parent
        visible: false
        color: "#f00f1318"
        property string title
        property bool money: false
        property string entry: ""
        property var done: null
        function ask(t, isMoney, then) { title = t; money = isMoney; entry = ""; done = then; visible = true }
        function close() { visible = false; done = null }
        readonly property string shown: !money ? entry
            : "$" + (Number(entry || "0") / 100).toFixed(2)
        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.6, d.unit * 26)
            spacing: d.unit * 0.5
            Text { Layout.alignment: Qt.AlignHCenter; text: keypad.title; color: "white"; font.pixelSize: d.unit * 1.6; font.bold: true }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: d.unit * 3
                radius: d.unit * 0.4
                color: "#1b222c"
                Text { anchors.centerIn: parent; text: keypad.shown || " "; color: "white"; font.pixelSize: d.unit * 1.8 }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 3
                rowSpacing: d.unit * 0.4
                columnSpacing: d.unit * 0.4
                Repeater {
                    model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "⌫", "0", "OK"]
                    delegate: Rectangle {
                        id: key
                        required property string modelData
                        objectName: "guestKey-" + modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: d.unit * 3.2
                        radius: d.unit * 0.4
                        color: keyTap.pressed ? Qt.lighter(d.accent, 1.3) : modelData === "OK" ? d.accent : "#2a313d"
                        Text { anchors.centerIn: parent; text: key.modelData; color: "white"; font.pixelSize: d.unit * 1.5 }
                        // TapHandler: every tap counts ("555" is not a double-click).
                        TapHandler {
                            id: keyTap
                            onTapped: {
                                if (key.modelData === "⌫")
                                    keypad.entry = keypad.entry.slice(0, -1)
                                else if (key.modelData === "OK") {
                                    const then = keypad.done
                                    const value = keypad.entry
                                    keypad.close()
                                    if (then && value !== "")
                                        then(keypad.money ? Number(value) : value)
                                } else if (keypad.entry.length < 12)
                                    keypad.entry += key.modelData
                            }
                        }
                    }
                }
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Cancel")
                color: "#9aa4b5"
                font.pixelSize: d.unit * 1.1
                TapHandler { onTapped: keypad.close() }
            }
        }
    }
}
