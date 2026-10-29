import QtQuick
import QtQuick.Controls.Fusion

// A food truck's window: the orders sent and not yet picked up, a card each
// (#27 Maria, pager 12, what's in it, how long). Ready calls it (the screens
// say which pager to buzz), again to call again; Picked Up takes it off. Without a kitchen
// screen this is where orders are called; with one, bumping calls them too.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string face: zone.st.font ?? Ui.font
    readonly property var orders: pos ? pos.truckOrders : []

    Text {
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
                        objectName: "truckOrderLabel"
                        width: parent.width - pagerTag.width - 12
                        text: card.modelData.label
                        color: "white"
                        font.family: w.face
                        font.pixelSize: 34 * grid.big
                        font.bold: true
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
                    width: parent.width
                    text: card.modelData.items
                    color: "#d8dee8"
                    font.family: w.face
                    font.pixelSize: 20 * grid.big
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: (card.modelData.ready ? qsTr("Ready") : qsTr("Making it"))
                          + "  ·  " + qsTr("%n min", "", card.modelData.minutes)
                          + (card.modelData.paid ? "" : "  ·  " + qsTr("NOT PAID"))
                    color: card.modelData.paid ? Ui.muted : "#ffb4a8"
                    font.family: w.face
                    font.pixelSize: 18 * grid.big
                    elide: Text.ElideRight
                }
            }
            Row {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 18 }
                spacing: 12
                WidgetKey {
                    objectName: "truckOrderReady"
                    width: (parent.width - 12) / 2
                    height: 64 * grid.big
                    fontScale: 0.4
                    text: card.modelData.ready ? qsTr("Call Again") : qsTr("Ready")
                    accent: !card.modelData.ready
                    onClicked: w.pos.orderReady(card.modelData.id)
                }
                WidgetKey {
                    objectName: "truckOrderPickedUp"
                    width: (parent.width - 12) / 2
                    height: 64 * grid.big
                    fontScale: 0.4
                    text: qsTr("Picked Up")
                    onClicked: w.pos.orderPickedUp(card.modelData.id)
                }
            }
        }
    }
}
