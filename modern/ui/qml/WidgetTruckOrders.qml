import QtQuick
import QtQuick.Controls.Fusion

// A food truck's window: the orders not yet picked up, a card each (#27
// Maria, pager 12, what's in it, how long). Ready calls it (the screens say
// which pager to buzz), again to call again; Picked Up takes it off, once it's
// paid. One not paid yet (a call-in) opens to change it, or Pay. Those put
// away before they went to the kitchen (New Order, ready later) are here
// too, on top. Without a kitchen screen this is where orders are called;
// with one, bumping calls them too.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string face: zone.st.font ?? Ui.font
    readonly property var orders: pos ? pos.truckOrders : []

    Text {
        textFormat: Text.PlainText
        anchors.centerIn: parent
        visible: w.orders.length === 0
        text: qsTr("No orders waiting.")
        color: Ui.muted
        font.family: w.face
        font.pixelSize: 28
    }

    GridView {
        id: grid
        anchors.fill: parent
        anchors.margins: 8
        clip: true
        // Upright (a phone): one a row, bigger.
        readonly property bool narrow: w.zone ? w.zone.narrow : false
        readonly property real big: narrow ? 1.5 : 1
        readonly property int columns: narrow ? 1 : Math.max(1, Math.floor((width - gridBar.width - 4) / 440))
        cellWidth: (width - gridBar.room) / columns
        cellHeight: 230 * big
        model: w.orders
        ScrollBar.vertical: TouchScrollBar { id: gridBar; needed: Math.ceil(grid.count / grid.columns) * grid.cellHeight > grid.height }
        delegate: Item {
            id: card
            required property var modelData
            width: grid.cellWidth
            height: grid.cellHeight
            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                radius: 12
                color: card.modelData.ready ? "#1f5f3a" : Ui.button
                border.color: card.modelData.ready ? "#3fbf74" : Qt.darker(color, 1.5)
                border.width: 3
            }
            Column {
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 18 }
                spacing: 4
                Row {
                    width: parent.width
                    spacing: 12
                    Text {
                        textFormat: Text.PlainText
                        objectName: "truckOrderLabel"
                        width: parent.width - pagerTag.width - 12
                        text: card.modelData.label
                        color: "white"
                        font.family: w.face
                        font.pixelSize: 34 * grid.big
                        font.bold: true
                        fontSizeMode: Text.HorizontalFit   // a long name: smaller before "…"
                        minimumPixelSize: Math.round(font.pixelSize * 0.6)
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        id: pagerTag
                        visible: card.modelData.pager > 0
                        width: visible ? pagerText.implicitWidth + 20 : 0
                        height: pagerText.implicitHeight + 8
                        radius: height / 2
                        color: "#ffd479"
                        Text {
                            textFormat: Text.PlainText
                            id: pagerText
                            anchors.centerIn: parent
                            text: qsTr("Pager %1").arg(card.modelData.pager)
                            color: "#2a2000"
                            font.family: w.face
                            font.pixelSize: 22 * grid.big
                            font.bold: true
                        }
                    }
                }
                Text {
                    textFormat: Text.PlainText
                    width: parent.width
                    text: card.modelData.items
                    color: "#d8dee8"
                    font.family: w.face
                    font.pixelSize: 20 * grid.big
                    elide: Text.ElideRight
                }
                Text {
                    textFormat: Text.PlainText
                    objectName: "truckOrderStatus"
                    width: parent.width
                    text: (card.modelData.later !== "" ? qsTr("For %1").arg(card.modelData.later)
                           : !card.modelData.sent ? qsTr("Not sent yet")
                           : (card.modelData.ready ? qsTr("Ready") : qsTr("Making it"))
                             + "  ·  " + qsTr("%n min", "", card.modelData.minutes))
                          + (card.modelData.paid ? "" : "  ·  " + qsTr("NOT PAID"))
                          + (card.modelData.openOn !== "" ? "  ·  " + qsTr("open on %1").arg(card.modelData.openOn) : "")
                    color: card.modelData.paid ? Ui.muted : "#ffb4a8"
                    font.family: w.face
                    font.pixelSize: 18 * grid.big
                    elide: Text.ElideRight
                }
            }
            // Sent: Ready (Call Again). Not paid: Open and Pay; paid: Picked Up.
            Row {
                id: keys
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 18 }
                spacing: 12
                readonly property var shown: (card.modelData.sent ? ["ready"] : [])
                                             .concat(card.modelData.paid ? ["pickedUp"] : ["open", "pay"])
                Repeater {
                    model: keys.shown
                    delegate: WidgetKey {
                        required property string modelData
                        objectName: "truckOrder" + modelData.charAt(0).toUpperCase() + modelData.slice(1)
                        width: (keys.width - 12 * (keys.shown.length - 1)) / keys.shown.length
                        height: 64 * grid.big
                        fontScale: 0.4
                        text: modelData === "ready" ? (card.modelData.ready ? qsTr("Call Again") : qsTr("Ready"))
                            : modelData === "open" ? qsTr("Open")
                            : modelData === "pay" ? qsTr("Pay") : qsTr("Picked Up")
                        accent: (modelData === "ready" && !card.modelData.ready) || modelData === "pay"
                        onClicked: {
                            const id = card.modelData.id
                            if (modelData === "ready") w.pos.orderReady(id)
                            else if (modelData === "pickedUp") w.pos.orderPickedUp(id)
                            else w.zone.controller.openOrder(id, modelData === "pay")
                        }
                    }
                }
            }
        }
    }
}
