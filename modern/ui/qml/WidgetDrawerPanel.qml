import QtQuick
import QtQuick.Layouts
import QtQuick.Window

// Cash drawer: start it with a bank, see expected cash, count it at the end
// of the shift. With server banks it is the logged-in person's own bank
// (Start Bank / Check Out). Managers also see everyone else's open drawers
// or banks and can count them. Amounts come from the number pad.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var d: pos ? pos.drawer : ({})
    readonly property string face: zone.st.font ?? Ui.font
    readonly property color ink: zone.st.textColor ?? "white"
    readonly property real unit: Math.max(14, Math.min(30, w.width * 0.045))
    readonly property bool bank: (d.mode ?? "") === "serverBank"
    // Drawer users on a terminal set up without a cash drawer (one left
    // open from before can still be counted).
    readonly property bool noDrawer: !bank && !(d.hasDrawer ?? true) && !(d.open ?? false)

    // Count Drawer / Check Out / a manager's Count: the total typed on the
    // keypad, else counted bill by bill.
    function count(id, name) {
        if (pos.entry !== "")
            id === 0 ? pos.countDrawer() : pos.countDrawerById(id)
        else if (id === 0 && (d.countBlocked ?? "") !== "")
            pos.countDrawer()   // says why not
        else
            counter.start(id, name, d.denominations ?? [], bank && id === 0)
    }
    DrawerCount {
        id: counter
        objectName: "drawerCount"
        parent: w.Window.contentItem
        anchors.fill: parent
        z: 60
        pos: w.pos
    }

    component Line: RowLayout {
        property string name
        property string value
        property bool strong: false
        property color tint: w.ink
        Layout.fillWidth: true
        Text { Layout.fillWidth: true; text: parent.name; color: parent.tint; font.family: w.face; font.pixelSize: w.unit; font.bold: parent.strong }
        Text { text: parent.value; color: parent.tint; font.family: w.face; font.pixelSize: w.unit; font.bold: parent.strong }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: w.unit * 0.8
        spacing: w.unit * 0.4

        Text {
            text: w.d.name ?? qsTr("Drawer 1")
            color: w.ink
            font.family: w.face
            font.pixelSize: w.unit * 1.5
            font.bold: true
        }
        Text {
            text: w.noDrawer ? qsTr("This terminal has no cash drawer")
                 : !w.d.exists ? (w.bank ? qsTr("Starts with your first cash sale") : qsTr("Not started today"))
                 : w.d.open ? qsTr("Open since %1 (%2)").arg(w.d.opened).arg(w.d.openedBy)
                            : qsTr("Counted by %1").arg(w.d.closedBy)
            color: w.d.open ? "#7ee2a8" : "#f5b940"
            font.family: w.face
            font.pixelSize: w.unit * 0.85
        }

        ColumnLayout {
            visible: w.d.exists ?? false
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: w.unit * 0.2
            Line { name: qsTr("Starting cash"); value: w.d.startingCash ?? "" }
            Line { visible: !(w.d.blind ?? false); name: qsTr("Cash sales"); value: w.d.cashSales ?? "" }
            Repeater {
                model: w.d.movements ?? []
                delegate: Line {
                    required property var modelData
                    name: modelData.what + "  (" + modelData.time + ")"
                    value: modelData.amount
                    tint: Ui.textSoft
                }
            }
            Line { visible: !(w.d.blind ?? false); name: w.bank ? qsTr("Cash to turn in") : qsTr("Expected in drawer"); value: w.d.expected ?? ""; strong: true }
            Line {
                visible: w.d.blind ?? false
                name: qsTr("Counted blind: what should be there shows after the count")
                tint: Ui.textSoft
            }
            Repeater {
                model: w.d.open ? [] : (w.d.countedPieces ?? [])
                delegate: Line {
                    required property var modelData
                    name: "   " + modelData.text
                    value: modelData.amount
                    tint: Ui.textSoft
                }
            }
            Line { visible: !w.d.open; name: qsTr("Counted"); value: w.d.counted ?? "" }
            Line {
                visible: !w.d.open && !(w.d.hideResult ?? false)
                name: (w.d.overShortCents ?? 0) < 0 ? qsTr("Short") : (w.d.overShortCents ?? 0) > 0 ? qsTr("Over") : qsTr("Balanced")
                value: w.d.overShort ?? ""
                strong: true
                tint: (w.d.overShortCents ?? 0) === 0 ? "#7ee2a8" : "#ff9a9e"
            }
        }

        // Manager: other people's open banks / other terminals' drawers.
        Text {
            visible: (w.d.others ?? []).length > 0
            text: w.bank ? qsTr("Other open banks") : qsTr("Other open drawers")
            color: Ui.muted
            font.family: w.face
            font.pixelSize: w.unit * 0.8
        }
        Repeater {
            model: w.d.others ?? []
            delegate: RowLayout {
                id: other
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredHeight: w.unit * 2.2
                Layout.fillHeight: false
                spacing: w.unit * 0.4
                Text {
                    Layout.fillWidth: true
                    text: other.modelData.name
                    color: w.ink
                    font.family: w.face
                    font.pixelSize: w.unit * 0.9
                    elide: Text.ElideRight
                }
                Text {
                    text: other.modelData.expected
                    color: w.ink
                    font.family: w.face
                    font.pixelSize: w.unit * 0.9
                }
                WidgetKey {
                    Layout.preferredWidth: w.unit * 5
                    Layout.fillHeight: true
                    text: qsTr("Count")
                    fontScale: 0.4
                    onClicked: w.count(other.modelData.id, other.modelData.name)
                }
            }
        }

        Item { Layout.fillHeight: true }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: w.noDrawer ? qsTr("Take cash on a terminal with a drawer, or have a manager give you your own bank (Manager → Employees → Cash handling).")
                 : w.bank ? (w.d.open ? qsTr("At the end of your shift, touch Check Out and count your cash bill by bill (or type the total on the keypad first).")
                                     : qsTr("Cash you take goes in your bank. To start with cash on hand, type it, then Start Bank."))
                         : w.d.open ? qsTr("At the end of the shift, touch Count Drawer and count the cash bill by bill (or type the total on the keypad first).")
                                    : qsTr("Type the starting cash on the keypad, then Start Drawer.")
            color: Ui.muted
            font.family: w.face
            font.pixelSize: w.unit * 0.8
        }
        Text {
            visible: w.pos && w.pos.entry !== ""
            text: qsTr("Entered: %1").arg(w.pos ? w.pos.entryAmount : "")
            color: "#f5b940"
            font.family: w.face
            font.pixelSize: w.unit
        }
        Text {
            visible: (w.d.open ?? false) && w.pos && w.pos.textEntry !== ""
            text: qsTr("Reason: %1").arg(w.pos ? w.pos.textEntry : "")
            color: "#f5b940"
            font.family: w.face
            font.pixelSize: w.unit * 0.8
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        RowLayout {
            visible: !w.noDrawer
            Layout.fillWidth: true
            Layout.preferredHeight: w.unit * 3
            Layout.fillHeight: false   // nested layouts fill by default
            spacing: w.unit * 0.4
            layoutDirection: w.zone.keyOrder("drawer", ["drawer", "noSale"]) === 1 ? Qt.RightToLeft : Qt.LeftToRight
            WidgetKey {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: w.zone.keyShown("drawer")
                text: w.bank ? (w.d.open ? qsTr("Check Out") : qsTr("Start Bank"))
                             : w.d.open ? qsTr("Count Drawer") : qsTr("Start Drawer")
                accent: true
                fontScale: 0.34
                onClicked: w.d.open ? w.count(0, w.d.name ?? "") : w.pos.openDrawerSession()
            }
            WidgetKey {
                visible: !w.bank && w.zone.keyShown("noSale")   // server banks: no drawer to open
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: w.zone.keyText("noSale", qsTr("No Sale"))
                fontScale: 0.34
                onClicked: w.pos.noSale()
            }
        }
        // Manager: what a pay out is for (Store Settings: expense categories).
        Flow {
            visible: (w.d.open ?? false) && w.pos !== null && w.pos.can("manager") && (w.d.categories ?? []).length > 0
            Layout.fillWidth: true
            spacing: w.unit * 0.3
            Repeater {
                model: w.d.categories ?? []
                delegate: WidgetKey {
                    required property var modelData
                    objectName: "expense-" + modelData.name
                    width: w.unit * 6
                    height: Math.max(w.unit * 2.2, w.zone ? w.zone.touch(46) : 0)
                    text: modelData.name
                    fontScale: 0.32
                    baseColor: modelData.chosen ? Ui.accent : Ui.key
                    onClicked: w.pos.setExpenseCategory(modelData.name)
                }
            }
        }
        // Manager: cash taken out (vendor, ice…) or put in (change). The
        // reason comes from the Reason… keyboard page when typed.
        RowLayout {
            visible: w.d.open ?? false
            Layout.fillWidth: true
            Layout.preferredHeight: w.unit * 3
            Layout.fillHeight: false
            spacing: w.unit * 0.4
            layoutDirection: w.zone.keyOrder("payOut", ["payOut", "paidIn"]) === 1 ? Qt.RightToLeft : Qt.LeftToRight
            WidgetKey {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: w.zone.keyShown("payOut")
                text: w.zone.keyText("payOut", qsTr("Pay Out"))
                fontScale: 0.34
                onClicked: w.pos.payout("payout")
            }
            WidgetKey {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: w.zone.keyShown("paidIn")
                text: w.zone.keyText("paidIn", qsTr("Paid In"))
                fontScale: 0.34
                onClicked: w.pos.payout("paidIn")
            }
        }
    }
}
