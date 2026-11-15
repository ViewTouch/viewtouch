import QtQuick
import QtQuick.Layouts

// The menu board: a TV facing the guests (Manager -> Terminals: Screen
// layout -> Menu board). The menu by category with today's prices; what's
// sold out is crossed off as soon as it's 86'd, and a prep count running low
// says "Only 4 left!". A long menu turns its pages. With Store Settings ->
// Menu board shows ready orders too, the orders that are ready run down the
// right side (one TV for both). Holding the top-left corner shows the login
// page for a minute, like the Now Serving board.
Rectangle {
    id: b
    property PosService pos
    readonly property var board: pos ? (pos.menuBoard ?? ({})) : ({})
    readonly property var prompt: pos ? pos.customerPrompt : ({})
    readonly property color accent: prompt.accent || Ui.accent
    readonly property string logo: !prompt.logo || !pos ? "" : (pos.imageRevision < 0 ? "" : pos.imageUrl(prompt.logo))
    readonly property real unit: Math.max(14, Math.min(width, height * 1.6) / 40)
    readonly property bool showOrders: board.showOrders === true
    readonly property var ready: !showOrders || !pos ? [] : (pos.truckOrders ?? []).filter(o => o.ready && o.readyMinutes < 15)
    // How many are left of the items counted (prep counts): {item: left}.
    readonly property var leftOf: {
        const out = {}
        for (const c of (pos ? pos.prepCounts ?? [] : []))
            out[c.item] = c.left
        return out
    }
    signal peek()

    color: "#0b0e12"

    // Categories with their items, in the menu's order.
    readonly property var blocks: {
        const items = board.items ?? []
        return (board.families ?? []).map(f => ({ title: f, items: items.filter(i => i.family === f) }))
                                     .filter(x => x.items.length > 0)
    }
    // Laid out in columns, a page at a time: each block goes in the first
    // column with room (a block taller than a column gets one to itself).
    readonly property real rowH: unit * 1.7
    readonly property real headH: unit * 2.3
    readonly property int columns: Math.max(1, Math.min(3, Math.floor(menuArea.width / (unit * 12))))
    readonly property var pages: {
        const H = menuArea.height
        if (H <= 0)
            return []
        const out = []
        let page = [], col = 0, used = 0
        for (const blk of blocks) {
            const h = headH + blk.items.reduce((s, i) => s + rowH + (i.description ? unit * 0.9 : 0), 0)
            if (used > 0 && used + h > H) {
                col += 1
                used = 0
            }
            if (col >= columns) {
                out.push(page)
                page = []
                col = 0
            }
            page.push({ title: blk.title, items: blk.items, column: col })
            used += h + unit * 0.6
        }
        if (page.length)
            out.push(page)
        return out
    }
    property int page: 0
    onPagesChanged: if (page >= pages.length) page = 0
    Timer {
        interval: 12000
        running: b.visible && b.pages.length > 1
        repeat: true
        onTriggered: b.page = (b.page + 1) % b.pages.length
    }

    // --- the store, and the time ---
    Rectangle {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: b.unit * 3.6
        color: b.accent
        Image {
            visible: b.logo !== ""
            anchors { left: parent.left; leftMargin: b.unit; verticalCenter: parent.verticalCenter }
            height: parent.height * 0.75
            fillMode: Image.PreserveAspectFit
            source: b.logo
        }
        Text {
            textFormat: Text.PlainText
            visible: b.logo === ""
            anchors { left: parent.left; leftMargin: b.unit; verticalCenter: parent.verticalCenter }
            text: b.pos ? b.pos.storeName : ""
            color: "white"
            font.pixelSize: b.unit * 1.7
            font.bold: true
        }
        Text {
            textFormat: Text.PlainText
            objectName: "menuBoardPages"
            anchors { horizontalCenter: parent.horizontalCenter; verticalCenter: parent.verticalCenter }
            visible: b.pages.length > 1
            text: qsTr("%1 of %2").arg(b.page + 1).arg(b.pages.length)
            color: "#e0ffffff"
            font.pixelSize: b.unit * 1.0
        }
        Text {
            textFormat: Text.PlainText
            id: clock
            anchors { right: parent.right; rightMargin: b.unit; verticalCenter: parent.verticalCenter }
            color: "white"
            font.pixelSize: b.unit * 1.5
            font.bold: true
            function tick() { text = Qt.locale().toString(new Date(), "h:mm AP") }
            Component.onCompleted: tick()
            Timer { interval: 5000; running: true; repeat: true; onTriggered: clock.tick() }
        }
    }

    RowLayout {
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; margins: b.unit }
        spacing: b.unit

        Item {
            id: menuArea
            objectName: "menuBoardMenu"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 3
            Repeater {
                model: b.columns
                delegate: Column {
                    id: col
                    required property int index
                    x: index * (menuArea.width + b.unit) / b.columns
                    width: (menuArea.width + b.unit) / b.columns - b.unit
                    spacing: b.unit * 0.6
                    Repeater {
                        model: (b.pages[b.page] ?? []).filter(blk => blk.column === col.index)
                        delegate: Column {
                            id: block
                            required property var modelData
                            width: col.width
                            Text {
                                textFormat: Text.PlainText
                                width: parent.width
                                height: b.headH
                                verticalAlignment: Text.AlignVCenter
                                text: block.modelData.title
                                color: b.accent.hslLightness < 0.45 ? Qt.lighter(b.accent, 1.6) : b.accent
                                font.pixelSize: b.unit * 1.4
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Repeater {
                                model: block.modelData.items
                                delegate: Item {
                                    id: item
                                    required property var modelData
                                    objectName: "board-" + modelData.id
                                    readonly property bool out: !modelData.available
                                    readonly property int itemsLeft: b.leftOf[modelData.id] ?? -1
                                    readonly property bool low: !out && itemsLeft > 0 && itemsLeft <= (b.board.lowAt ?? 0)
                                    width: block.width
                                    height: b.rowH + (modelData.description ? b.unit * 0.9 : 0)
                                    Text {
                                        id: nameText
                                        textFormat: Text.PlainText
                                        anchors { left: parent.left; top: parent.top; right: priceText.left; rightMargin: b.unit * 0.5 }
                                        height: b.rowH
                                        verticalAlignment: Text.AlignVCenter
                                        text: item.modelData.name + (item.out ? "  " + qsTr("Sold out")
                                                                     : item.low ? "  " + qsTr("Only %1 left!").arg(item.itemsLeft) : "")
                                        color: item.out ? "#6b7380" : "white"
                                        font.pixelSize: b.unit * 0.95
                                        font.bold: true
                                        font.strikeout: item.out
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        id: priceText
                                        textFormat: Text.PlainText
                                        anchors { right: parent.right; top: parent.top }
                                        height: b.rowH
                                        verticalAlignment: Text.AlignVCenter
                                        text: item.modelData.byWeight ? qsTr("%1 / %2").arg(item.modelData.price).arg(item.modelData.unit)
                                                                      : item.modelData.price
                                        color: item.out ? "#6b7380" : item.low ? Ui.warn : "white"
                                        font.pixelSize: b.unit * 0.95
                                        font.bold: true
                                    }
                                    Text {
                                        textFormat: Text.PlainText
                                        visible: !!item.modelData.description
                                        anchors { left: parent.left; right: parent.right; top: nameText.bottom; topMargin: -b.unit * 0.2 }
                                        text: item.modelData.description ?? ""
                                        color: "#8a94a6"
                                        font.pixelSize: b.unit * 0.75
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                visible: b.blocks.length === 0
                text: qsTr("The menu shows here.")
                color: "#7d8796"
                font.pixelSize: b.unit * 1.5
            }
        }

        // Ready orders, down the side (Store Settings: Menu board shows ready orders too).
        ColumnLayout {
            objectName: "menuBoardReady"
            visible: b.showOrders
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.maximumWidth: b.unit * 12
            spacing: b.unit * 0.5
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Ready")
                color: "#3fbf74"
                font.pixelSize: b.unit * 1.8
                font.bold: true
            }
            Rectangle { Layout.fillWidth: true; height: 4; radius: 2; color: "#3fbf74"; opacity: 0.6 }
            Repeater {
                model: b.ready.slice(0, 8)
                delegate: Rectangle {
                    id: card
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: b.unit * 3
                    radius: b.unit * 0.4
                    color: "#173b27"
                    border.color: "#3fbf74"
                    Text {
                        textFormat: Text.PlainText
                        anchors { fill: parent; leftMargin: b.unit * 0.4; rightMargin: b.unit * 0.4 }
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        text: card.modelData.name ? "#" + card.modelData.number + "  " + card.modelData.name : "#" + card.modelData.number
                        color: "white"
                        font.pixelSize: b.unit * 1.3
                        font.bold: true
                    }
                }
            }
            Item { Layout.fillHeight: true }
        }
    }

    // Held for three seconds: the login page underneath, for a minute.
    MouseArea {
        anchors { left: parent.left; top: parent.top }
        width: b.unit * 4
        height: b.unit * 4
        pressAndHoldInterval: 3000
        onPressAndHold: b.peek()
    }
}
