import QtQuick
import QtQuick.Layouts

// Settlement summary: totals, payments applied (touch to select for Undo
// Payment), amount entered, and the balance or change.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property color ink: zone.st.textColor ?? "white"
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property real unit: Math.max(12, Math.min(zone.st.fontSize ?? 28, w.width * 0.05))
    readonly property var totals: pos ? pos.totals : ({})
    readonly property bool owing: (totals.balanceCents ?? 0) > 0

    component Line: RowLayout {
        property string name
        property string value
        property real size: w.unit
        property bool strong: false
        property color tint: w.ink
        Layout.fillWidth: true
        Text {
            Layout.fillWidth: true
            text: parent.name
            color: parent.tint
            font.family: w.face
            font.pixelSize: parent.size
            font.bold: parent.strong
            elide: Text.ElideRight
        }
        Text {
            text: parent.value
            color: parent.tint
            font.family: w.face
            font.pixelSize: parent.size
            font.bold: parent.strong
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: w.unit * 0.8
        spacing: w.unit * 0.35

        Text {
            text: w.pos && w.pos.hasCheck ? w.pos.check.label + "  #" + w.pos.check.id : qsTr("No check open")
            color: w.ink
            font.family: w.face
            font.pixelSize: w.unit * 1.3
            font.bold: true
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        ColumnLayout {
            visible: w.pos && w.pos.hasCheck
            Layout.fillWidth: true
            spacing: w.unit * 0.2

            Line { name: qsTr("Items"); value: w.totals.items ?? "" }
            Line { name: qsTr("Discount"); value: w.totals.discounts ?? ""; visible: w.totals.hasDiscount ?? false }
            Repeater {
                model: w.totals.taxLines ?? []
                delegate: Line {
                    required property var modelData
                    name: modelData.name
                    value: modelData.amount
                    size: w.unit * 0.8
                    tint: "#b8c0cc"
                }
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#3a4250" }
            Line {
                visible: w.totals.hasGratuity ?? false
                name: qsTr("Gratuity %1%").arg(w.totals.gratuityPercent ?? 0)
                value: w.totals.gratuity ?? ""
            }
            Line { name: qsTr("Total"); value: w.totals.total ?? ""; strong: true; size: w.unit * 1.2 }
            Line { name: qsTr("Cash rounding"); value: w.totals.rounding ?? ""; visible: w.totals.hasRounding ?? false }
            Line {
                visible: w.totals.hasTips ?? false
                name: qsTr("Tips")
                value: w.totals.tips ?? ""
                tint: "#7ee2a8"
            }
        }

        Text {
            visible: w.pos && w.pos.payments.length > 0
            text: qsTr("Payments")
            color: "#8a94a6"
            font.family: w.face
            font.pixelSize: w.unit * 0.75
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: w.pos ? w.pos.payments : []
            delegate: Rectangle {
                id: pay
                required property var modelData
                width: ListView.view.width
                height: w.unit * 1.8
                radius: 6
                color: modelData.selected ? "#2f6fd6" : "transparent"
                TapHandler { onTapped: { if (TouchGuard.covered(point.scenePressPosition)) return; w.pos.selectedPayment = pay.modelData.id } }
                Line {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    name: pay.modelData.name + (pay.modelData.tip ? qsTr("  + tip %1").arg(pay.modelData.tip) : "")
                    value: pay.modelData.amount
                }
            }
        }

        // Tips go on card payments: percentages, or the keypad amount.
        RowLayout {
            visible: (w.totals.hasCard ?? false) && w.zone.keyShown("tips")
            Layout.fillWidth: true
            Layout.preferredHeight: w.unit * 2.4
            Layout.fillHeight: false   // nested layouts fill by default
            spacing: w.unit * 0.3
            Text {
                text: w.zone.keyText("tips", qsTr("Tip"))
                color: w.ink
                font.family: w.face
                font.pixelSize: w.unit
            }
            Repeater {
                model: [15, 18, 20]
                delegate: WidgetKey {
                    required property int modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: modelData + "%"
                    fontScale: 0.4
                    onClicked: w.pos.addTip(modelData)
                }
            }
            WidgetKey {
                Layout.fillWidth: true
                Layout.fillHeight: true
                text: w.pos && w.pos.entry !== "" ? w.pos.entryAmount : qsTr("Amount")
                fontScale: 0.34
                onClicked: w.pos.addTip(0)
            }
        }
        WidgetKey {
            visible: w.pos && w.pos.hasCheck && w.zone.keyShown("gratuity")
            Layout.fillWidth: true
            Layout.preferredHeight: w.unit * 2
            text: (w.totals.hasGratuity ?? false) ? qsTr("Remove gratuity")
                : w.zone.keyText("gratuity", qsTr("Add %1% gratuity").arg(w.totals.storeGratuityPercent ?? 18))
            fontScale: 0.4
            onClicked: w.pos.setGratuity((w.totals.hasGratuity ?? false) ? 0 : (w.totals.storeGratuityPercent ?? 18))
        }

        // A bar tab's card on file: held, or one to hold.
        RowLayout {
            readonly property var check: w.pos && w.pos.hasCheck ? w.pos.check : ({})
            readonly property var hold: check.hold ?? ({})
            visible: !!hold.card || (!!check.tab && !!check.tabHold)
            Layout.fillWidth: true
            Layout.preferredHeight: w.unit * 2
            Layout.fillHeight: false
            spacing: w.unit * 0.3
            Text {
                Layout.fillWidth: true
                text: parent.hold.card ? qsTr("Card held: %1 · %2").arg(parent.hold.card).arg(parent.hold.amount)
                                       : qsTr("No card held for this tab")
                color: parent.hold.card ? "#7ee2a8" : "#f5b940"
                font.family: w.face
                font.pixelSize: w.unit * 0.8
                elide: Text.ElideRight
            }
            WidgetKey {
                objectName: "cardOnFileKey"
                Layout.preferredWidth: w.unit * 9
                Layout.fillHeight: true
                text: qsTr("Card on File…")
                fontScale: 0.38
                onClicked: w.zone.controller.command("cardOnFile")
            }
        }

        Line {
            visible: w.pos && w.pos.entry !== ""
            name: qsTr("Amount entered")
            value: w.pos ? w.pos.entryAmount : ""
            tint: "#f5b940"
        }

        Rectangle {
            visible: w.pos && w.pos.hasCheck
            Layout.fillWidth: true
            Layout.preferredHeight: w.unit * 3
            radius: 10
            color: w.owing ? "#3a2020" : "#173a26"
            Line {
                anchors.fill: parent
                anchors.margins: w.unit * 0.6
                name: w.owing ? qsTr("Balance due") : (w.totals.hasChange ? qsTr("Change") : qsTr("Paid in full"))
                value: w.owing ? (w.totals.balance ?? "") : (w.totals.hasChange ? w.totals.change : "")
                strong: true
                size: w.unit * 1.4
                tint: w.owing ? "#ff9a9e" : "#7ee2a8"
            }
        }
    }
}
