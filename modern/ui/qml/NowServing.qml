import QtQuick
import QtQuick.Layouts

// The Now Serving board: a TV facing the guests (Manager -> Terminals:
// Screen layout -> Now Serving board). Orders called by number: those being
// made, and those ready, big, until they're picked up (or a while after).
// Nobody works on it; holding the top-left corner shows the login page for
// a minute (a manager: Manager -> Close ViewTouch, say).
Rectangle {
    id: b
    property PosService pos
    // Ready orders leave the board once picked up, or this long after.
    readonly property int readyStays: 15
    readonly property var orders: pos ? (pos.truckOrders ?? []) : []
    readonly property var ready: orders.filter(o => o.ready && o.readyMinutes < readyStays)
    readonly property var making: orders.filter(o => !o.ready)
    readonly property var prompt: pos ? pos.customerPrompt : ({})
    readonly property color accent: prompt.accent || Ui.accent
    readonly property string logo: !prompt.logo || !pos ? "" : (pos.imageRevision < 0 ? "" : pos.imageUrl(prompt.logo))
    readonly property real unit: Math.max(14, Math.min(width, height * 1.6) / 40)
    signal peek()
    signal chime()   // an order just called: a sound, where the TV has one

    color: "#0b0e12"

    // Just called: flashes for a few seconds.
    property var seen: ({})
    property var fresh: ({})
    onReadyChanged: {
        const now = {}, newly = {}
        for (const o of ready) {
            now[o.id] = true
            if (!seen[o.id] && o.readyMinutes < 1)   // called just now, not already up when the board started
                newly[o.id] = true
        }
        seen = now
        if (Object.keys(newly).length > 0) {
            fresh = Object.assign({}, fresh, newly)
            freshTimer.restart()
            chime()
        }
    }
    Timer { id: freshTimer; interval: 8000; onTriggered: b.fresh = ({}) }

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
            visible: b.logo === ""
            anchors { left: parent.left; leftMargin: b.unit; verticalCenter: parent.verticalCenter }
            text: b.pos ? b.pos.storeName : ""
            color: "white"
            font.pixelSize: b.unit * 1.7
            font.bold: true
        }
        Text {
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
        Repeater {
            // Preparing, then Ready (the wider side: it's what guests look for).
            // Two columns that stay: only their orders change (rebuilt with each
            // order, a page turned back to the first and the flashing restarted).
            model: 2
            delegate: ColumnLayout {
                id: column
                required property int index
                readonly property var modelData: index === 0
                    ? {title: qsTr("Preparing"), list: b.making, color: "#c7cedb", big: false, share: 2}
                    : {title: qsTr("Ready"), list: b.ready, color: "#3fbf74", big: true, share: 3}
                objectName: modelData.big ? "boardReady" : "boardPreparing"
                readonly property int count: modelData.list.length
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: modelData.share
                spacing: b.unit * 0.6
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: column.modelData.title
                    color: column.modelData.color
                    font.pixelSize: b.unit * 2.3
                    font.bold: true
                }
                Rectangle { Layout.fillWidth: true; height: 4; radius: 2; color: column.modelData.color; opacity: 0.6 }
                // Which page of them (more than fit at once).
                Text {
                    objectName: column.modelData.big ? "boardReadyPages" : "boardPreparingPages"
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    visible: flow.pages > 1
                    text: qsTr("%1 of %2").arg(flow.page + 1).arg(flow.pages)
                    color: "#7d8796"
                    font.pixelSize: b.unit * 0.9
                }
                Flow {
                    id: flow
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: b.unit * 0.6
                    clip: true
                    readonly property real cardH: b.unit * (column.modelData.big ? 4.4 : 3)
                    readonly property int across: column.modelData.big ? 2 : 1
                    // More than fit: a page at a time, turned every few seconds.
                    readonly property int perPage: Math.max(1, Math.floor((height + spacing) / (cardH + spacing)) * across)
                    readonly property int pages: Math.max(1, Math.ceil(column.count / perPage))
                    property int page: 0
                    onPagesChanged: if (page >= pages) page = 0
                    Timer {
                        interval: 6000
                        running: flow.pages > 1
                        repeat: true
                        onTriggered: flow.page = (flow.page + 1) % flow.pages
                    }
                    Repeater {
                        model: column.modelData.list.slice(flow.page * flow.perPage, (flow.page + 1) * flow.perPage)
                        delegate: Rectangle {
                            id: card
                            required property var modelData
                            readonly property bool justCalled: column.modelData.big && b.fresh[modelData.id] === true
                            width: (flow.width - flow.spacing * (flow.across - 1)) / flow.across
                            height: flow.cardH
                            radius: b.unit * 0.4
                            color: column.modelData.big ? "#173b27" : "#1a1f27"
                            border.color: column.modelData.big ? "#3fbf74" : "#2a313c"
                            border.width: justCalled ? 4 : 1
                            Text {
                                anchors { fill: parent; leftMargin: b.unit * 0.6; rightMargin: b.unit * 0.6 }
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                                // #27 Maria
                                text: card.modelData.name ? "#" + card.modelData.number + "  " + card.modelData.name
                                                          : "#" + card.modelData.number
                                color: column.modelData.big ? "white" : "#c7cedb"
                                font.pixelSize: flow.cardH * (column.modelData.big ? 0.48 : 0.45)
                                font.bold: true
                            }
                            SequentialAnimation on opacity {
                                running: card.justCalled
                                loops: 5
                                NumberAnimation { to: 0.35; duration: 350 }
                                NumberAnimation { to: 1; duration: 350 }
                                onStopped: card.opacity = 1
                            }
                        }
                    }
                }
            }
        }
    }

    // Nothing waiting: the store's pictures and messages in turn (Store
    // Settings, as on the customer display), and what the board is for.
    Item {
        id: idle
        objectName: "boardIdle"
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; margins: b.unit * 2 }
        visible: b.orders.length === 0
        readonly property var slides: b.prompt.slides ?? []
        property int index: 0
        Timer {
            interval: 8000
            running: idle.visible && idle.slides.length > 1
            repeat: true
            onTriggered: idle.index = (idle.index + 1) % Math.max(1, idle.slides.length)
        }
        readonly property string current: slides.length ? slides[index % slides.length] : ""
        readonly property bool picture: current.startsWith("image:")
        Image {
            anchors { fill: parent; bottomMargin: b.unit * 3 }
            visible: idle.picture
            fillMode: Image.PreserveAspectFit
            source: !idle.picture || !b.pos ? "" : (b.pos.imageRevision < 0 ? "" : b.pos.imageUrl(idle.current.slice(6)))
        }
        Text {
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; verticalCenterOffset: -b.unit }
            visible: !idle.picture && idle.current !== ""
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: idle.current
            color: "white"
            font.pixelSize: b.unit * 2.2
            font.bold: true
        }
        Text {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Your order number shows here while it's made, and when it's ready.")
            color: "#7d8796"
            font.pixelSize: b.unit * 1.3
        }
    }

    // An hour with nothing to show (closed, say): dark, to spare the TV; the
    // next order lights it again.
    property double lastBusy: Date.now()
    property bool dark: false
    onOrdersChanged: if (orders.length > 0) { lastBusy = Date.now(); dark = false }
    Timer {
        interval: 60000
        running: true
        repeat: true
        onTriggered: b.dark = b.orders.length === 0 && Date.now() - b.lastBusy > b.darkAfterMs
    }
    property int darkAfterMs: 3600000
    Rectangle {
        objectName: "boardDark"
        anchors.fill: parent
        visible: b.dark
        color: "black"
        z: 5
        TapHandler { onTapped: { b.lastBusy = Date.now(); b.dark = false } }
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
