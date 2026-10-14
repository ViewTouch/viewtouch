import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Deliveries: every open delivery, the oldest promise first, by where it is:
// not sent, cooking, ready, out with a driver, or back. Touch the ready ones
// going together, touch the driver, Send Out. When the driver is back,
// Delivered; Open Check to take their payment.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property color ink: zone.st.textColor ?? "white"
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real unit: narrow ? Math.max(14, Math.min(w.width / 20, 44))
                                        : Math.max(14, Math.min(w.width / 60, w.height / 30))

    readonly property var orders: pos ? pos.deliveries : []
    readonly property var drivers: pos ? pos.drivers : []
    property var picked: []        // check ids
    property string driverId: ""
    readonly property var pickedOrders: orders.filter(o => picked.indexOf(o.id) >= 0)
    readonly property bool canSend: driverId !== "" && pickedOrders.length > 0
                                    && pickedOrders.every(o => o.state === "ready" || o.state === "cooking")
    readonly property bool canBack: pickedOrders.length > 0 && pickedOrders.every(o => o.state === "out")
    // Picks that closed (paid) or went away drop off.
    onOrdersChanged: picked = picked.filter(id => orders.some(o => o.id === id))

    function toggle(id) {
        const p = picked.slice()
        const i = p.indexOf(id)
        if (i >= 0) p.splice(i, 1); else p.push(id)
        picked = p
    }
    function colorFor(state) {
        return state === "new" ? "#3a4250"
             : state === "cooking" ? zone.statusColor("deliveryCooking", "#a86a12")
             : state === "ready" ? zone.statusColor("deliveryReady", "#1f7a4a")
             : state === "out" ? zone.statusColor("deliveryOut", "#2f5fb0")
             : zone.statusColor("deliveryBack", "#1f6f78")
    }
    function stateText(o) {
        return o.state === "new" ? qsTr("Not sent to the kitchen")
             : o.state === "cooking" ? qsTr("Cooking")
             : o.state === "ready" ? qsTr("Ready to go")
             : o.state === "out" ? qsTr("Out with %1 · %2 min").arg(o.driver).arg(o.outMinutes)
             : qsTr("Back (%1 min): take the payment").arg(o.outMinutes)
    }

    component Key: Button {
        property color tint: "#2d3440"
        Layout.fillWidth: true
        Layout.preferredHeight: w.unit * 2.6
        font.family: w.face
        font.pixelSize: w.unit * 0.9
        font.bold: true
        palette.button: tint
        palette.buttonText: w.ink
        opacity: enabled ? 1 : 0.4
    }

    GridLayout {
        anchors.fill: parent
        columns: w.narrow ? 1 : 2
        rowSpacing: w.unit * 0.6
        columnSpacing: w.unit * 0.8

        // --- the orders ---
        ListView {
            id: list
            objectName: "deliveryList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: w.narrow ? -1 : w.width * 0.62
            clip: true
            spacing: w.unit * 0.3
            model: w.orders
            boundsBehavior: Flickable.StopAtBounds
            delegate: Rectangle {
                id: card
                required property var modelData
                readonly property bool isPicked: w.picked.indexOf(modelData.id) >= 0
                objectName: "delivery-" + modelData.id
                width: list.width
                height: info.implicitHeight + w.unit
                radius: w.unit * 0.4
                color: cardTap.pressed ? Qt.lighter(w.colorFor(modelData.state), 1.3) : w.colorFor(modelData.state)
                border.color: isPicked ? "white" : "transparent"
                border.width: isPicked ? Math.max(3, w.unit * 0.2) : 0
                ColumnLayout {
                    id: info
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: w.unit * 0.5
                    spacing: w.unit * 0.1
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: (card.isPicked ? "✓ " : "") + (card.modelData.name || card.modelData.label)
                            color: w.ink
                            font.family: w.face
                            font.pixelSize: w.unit * 1.0
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: card.modelData.paid ? qsTr("Paid") : card.modelData.total
                            color: w.ink
                            font.family: w.face
                            font.pixelSize: w.unit * 0.9
                            font.bold: true
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: text !== ""
                        text: card.modelData.address
                        color: w.ink
                        font.family: w.face
                        font.pixelSize: w.unit * 0.8
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: w.stateText(card.modelData)
                            color: w.ink
                            font.family: w.face
                            font.pixelSize: w.unit * 0.75
                            elide: Text.ElideRight
                        }
                        Text {
                            visible: !!card.modelData.promised
                            text: qsTr("Promised %1").arg(card.modelData.promised ?? "")
                            color: card.modelData.late ? "#ffb3b6" : w.ink
                            font.family: w.face
                            font.pixelSize: w.unit * 0.75
                            font.bold: !!card.modelData.late
                        }
                    }
                }
                TapHandler { id: cardTap; onTapped: { if (TouchGuard.covered(point.scenePressPosition)) return; w.toggle(card.modelData.id) } }
            }
            Text {
                anchors.centerIn: parent
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: list.count === 0
                text: qsTr("No deliveries open.")
                color: "#8a94a6"
                font.family: w.face
                font.pixelSize: w.unit * 0.9
            }
        }

        // --- drivers, and what to do ---
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: !w.narrow
            spacing: w.unit * 0.4
            Text {
                text: qsTr("Driver")
                color: "#8a94a6"
                font.family: w.face
                font.pixelSize: w.unit * 0.8
                font.bold: true
            }
            Flow {
                Layout.fillWidth: true
                spacing: w.unit * 0.3
                Repeater {
                    model: w.drivers
                    delegate: Button {
                        required property var modelData
                        objectName: "driver-" + modelData.name
                        width: (parent.width - w.unit * 0.3) / 2
                        height: w.unit * 2.6
                        font.family: w.face
                        font.pixelSize: w.unit * 0.8
                        font.bold: true
                        text: modelData.name + (modelData.out > 0 ? "  ·  " + qsTr("%1 out").arg(modelData.out)
                                                : !modelData.clockedIn ? "  ·  " + qsTr("off") : "")
                        palette.button: w.driverId === modelData.id ? "#2f6fd6" : "#2d3440"
                        palette.buttonText: w.ink
                        opacity: modelData.clockedIn ? 1 : 0.6
                        onClicked: w.driverId = w.driverId === modelData.id ? "" : modelData.id
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                visible: w.drivers.length === 0
                wrapMode: Text.WordWrap
                text: qsTr("No drivers yet: give someone the Delivery driver role (Manager → Employees).")
                color: "#8a94a6"
                font.family: w.face
                font.pixelSize: w.unit * 0.75
            }
            Item { Layout.fillHeight: true; visible: !w.narrow }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: w.pickedOrders.length === 0 ? qsTr("Touch the orders going out together, then the driver.")
                    : qsTr("%n order(s) picked", "", w.pickedOrders.length)
                color: "#8a94a6"
                font.family: w.face
                font.pixelSize: w.unit * 0.75
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: w.unit * 0.3
                columnSpacing: w.unit * 0.3
                Key {
                    objectName: "deliverySendOut"
                    Layout.columnSpan: 2
                    text: qsTr("Send Out")
                    tint: "#2f5fb0"
                    enabled: w.canSend
                    onClicked: { w.pos.sendOut(w.picked, w.driverId); w.picked = [] }
                }
                Key {
                    objectName: "deliveryBack"
                    text: qsTr("Delivered")
                    tint: "#1f6f78"
                    enabled: w.canBack
                    onClicked: { for (const id of w.picked) w.pos.deliveryBack(id); w.picked = [] }
                }
                Key {
                    objectName: "deliveryOpen"
                    text: qsTr("Open Check")
                    enabled: w.pickedOrders.length === 1 && w.pickedOrders[0].busyOn === ""
                    onClicked: w.zone.controller.openCheck(w.pickedOrders[0].id)
                }
            }
        }
    }
}
