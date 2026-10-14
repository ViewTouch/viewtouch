import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// The self-order kiosk: guests order on their own, then pay at the counter.
//   Attract (photos, "Touch to order") -> For Here / To Go -> the menu and
//   their order -> a name -> the order number.
// Landscape: categories | dishes | their order. Portrait (a tall kiosk such
// as a 21.5" floor stand): big photo cards, with the categories and the order
// bar at the bottom, in reach; "Easy Reach" brings everything into the lower
// part of the screen for guests in a wheelchair, and children.
// No way to the staff pages from here: a manager holds the top-left corner
// for three seconds and types their PIN (`managerExit`).
Rectangle {
    id: k
    property PosService pos
    signal managerExit()

    readonly property var info: pos ? pos.selfOrder : ({})
    readonly property var menu: pos ? pos.kioskMenu : ({})
    readonly property var brand: pos ? pos.customerPrompt : ({})
    readonly property color accent: brand.accent || "#2f6fd6"
    // Pictures by ref ("store:logo.png", a path...): this screen's copy.
    function img(ref) { return pos ? (pos.imageRevision < 0 ? undefined : pos.imageUrl(ref)) : ref }
    readonly property string logo: !brand.logo ? "" : img(brand.logo)
    readonly property bool portrait: height > width * 1.15
    // One unit: about 6 mm on a 21.5" portrait kiosk (26 px of 1080), 20 px
    // on a 1280x800 landscape screen.
    readonly property real u: look.size * (portrait ? Math.max(14, width / 42) : Math.max(12, Math.min(width / 64, height / 40)))

    // The store's look for its kiosk (Store Settings -> Self-order kiosk).
    readonly property var lookSet: info.look ?? ({})
    readonly property var look: ({ size: lookSet.size || 1 })
    readonly property color bg: lookSet.background || "#14181f"
    readonly property color card: lookSet.card || "#2a313d"
    readonly property color panel: lookSet.card ? Qt.darker(card, 1.25) : "#1d232c"
    readonly property color go: lookSet.go || "#1f8a4c"
    readonly property color ink: lookSet.text || "white"
    readonly property color muted: Qt.rgba(ink.r, ink.g, ink.b, 0.62)
    readonly property color soft: Qt.rgba(ink.r, ink.g, ink.b, 0.8)
    readonly property string face: lookSet.font || Qt.application.font.family
    // Readable text on any button color: dark on light ones, white on dark ones.
    function textOn(c) { return 0.299 * c.r + 0.587 * c.g + 0.114 * c.b > 0.6 ? "#1b1b1b" : "white" }

    readonly property var lastOrder: info.lastOrder ?? ({})
    readonly property bool done: (lastOrder.number ?? 0) > 0
    readonly property bool ordering: info.ordering ?? false
    property bool reviewing: false
    property bool choosingType: false   // touched the attract screen: For Here / To Go
    property bool orderOpen: false      // portrait: the order sheet
    property bool easyReach: false
    property string family: ""
    readonly property var families: menu.families ?? []
    readonly property string shownFamily: families.includes(family) ? family : (families[0] ?? "")
    // What the guest is avoiding (Allergies): those items aren't shown.
    property var avoid: []
    readonly property var items: (menu.items ?? []).filter(i => i.family === shownFamily
                                                            && !(i.allergens ?? []).some(a => avoid.includes(a)))
    readonly property var lines: pos ? pos.lines.filter(l => !l.voided && !l.comment) : []
    readonly property var choosing: pos ? pos.choosing : ({})
    readonly property string total: pos ? (pos.totals.total ?? "") : ""

    color: k.bg
    onOrderingChanged: {
        if (ordering) {
            choosingType = false
            return
        }
        reviewing = false
        orderOpen = false
        family = ""
        if (!done)   // cancelled: the next guest starts with the full screen
            easyReach = false
    }
    onDoneChanged: if (!done) easyReach = false   // the number shown, then the next guest

    // --- idle: an order nobody is finishing is cleared ---------------------------------
    property int idleLeft: 0
    function touched() { idle.restart(); typeIdle.restart(); stillThere.visible = false }
    PointHandler { onActiveChanged: if (active) k.touched() }   // any touch, without taking it
    // Touches stop at the kiosk: the store's page is still behind it, and a
    // touch that fell through pressed its buttons (Log Out cancelled the order).
    MouseArea { anchors.fill: parent; z: -1 }
    Timer {
        id: idle
        interval: (k.info.idleSeconds ?? 90) * 1000
        running: k.ordering
        onTriggered: { k.idleLeft = 15; stillThere.visible = true; countdown.start() }
    }
    Timer {
        id: countdown
        interval: 1000
        repeat: true
        running: stillThere.visible
        onTriggered: if (--k.idleLeft <= 0) { stillThere.visible = false; k.pos.kioskCancel() }
    }
    // For Here / To Go left untouched: back to the pictures.
    Timer {
        id: typeIdle
        interval: 30000
        running: k.choosingType
        onTriggered: k.choosingType = false
    }
    // Allergies: what to leave off the menu for this guest.
    Rectangle {
        id: avoidSheet
        objectName: "kioskAvoidSheet"
        visible: false
        anchors.fill: parent
        z: 50
        color: Qt.rgba(0, 0, 0, 0.85)
        MouseArea { anchors.fill: parent }   // nothing under it is touched
        Column {
            anchors.centerIn: parent
            width: Math.min(parent.width - 2 * k.u, 44 * k.u)
            spacing: k.u
            Text {
                font.family: k.face
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("Anything to avoid? Dishes with it won't be shown.")
                color: "white"
                font.pixelSize: k.u * 1.4
                font.bold: true
            }
            Grid {
                anchors.horizontalCenter: parent.horizontalCenter
                columns: 3
                spacing: k.u * 0.5
                Repeater {
                    model: k.pos ? k.pos.allergenList() : []
                    delegate: Big {
                        required property var modelData
                        readonly property bool on: k.avoid.includes(modelData.id)
                        objectName: "kioskAvoid-" + modelData.id
                        width: k.u * 13
                        height: k.u * 3.6
                        text: (on ? "✓ " : "") + modelData.name
                        size: 1
                        base: on ? "#c62828" : k.card
                        onClicked: {
                            const list = k.avoid.slice()
                            const at = list.indexOf(modelData.id)
                            if (at >= 0) list.splice(at, 1); else list.push(modelData.id)
                            k.avoid = list
                        }
                    }
                }
            }
            Text {
                font.family: k.face
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("For a serious allergy, please also tell our staff.")
                color: "#ffcdd2"
                font.pixelSize: k.u * 0.85
            }
            Big {
                objectName: "kioskAvoidDone"
                anchors.horizontalCenter: parent.horizontalCenter
                width: k.u * 14
                height: k.u * 3.6
                text: qsTr("Done")
                base: k.go
                onClicked: avoidSheet.visible = false
            }
        }
    }

    // The confirmation goes back to the attract screen by itself.
    Timer {
        interval: 20000
        running: k.done && !k.ordering
        onTriggered: k.pos.kioskCancel()
    }

    component Big: Rectangle {
        id: big
        property string text
        property color base: k.card
        property real size: 1.4
        signal clicked()
        radius: k.u * 0.6
        color: !enabled ? "#20252e" : tap.pressed ? Qt.lighter(base, 1.3) : base
        implicitHeight: k.u * 3.4
        Text {
            font.family: k.face
            anchors.fill: parent
            anchors.margins: k.u * 0.4
            text: big.text
            color: big.enabled ? k.textOn(big.color) : "#6b7383"
            font.pixelSize: k.u * big.size
            font.bold: true
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            fontSizeMode: Text.Fit
            minimumPixelSize: k.u * 0.7
        }
        // A MouseArea: these sit in scrolling lists too (a TapHandler there
        // loses the press to the list).
        MouseArea { id: tap; anchors.fill: parent; onClicked: big.clicked() }
    }

    // A dish: its photo (or its initial), name, description and price.
    component Card: Item {
        id: card
        required property var modelData
        Rectangle {
            anchors.fill: parent
            anchors.margins: k.u * 0.35
            radius: k.u * 0.6
            color: cardTap.pressed ? "#323b49" : k.panel
            clip: true
            Rectangle {
                id: photo
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: parent.height * (k.portrait ? 0.58 : 0.5)
                color: Qt.darker(k.accent, 2.2)
                Text {
                    font.family: k.face
                    anchors.centerIn: parent
                    visible: !card.modelData.image || picture.status !== Image.Ready
                    text: card.modelData.name.charAt(0)
                    color: Qt.lighter(k.accent, 1.6)
                    font.pixelSize: k.u * 3
                    font.bold: true
                }
                Image {
                    id: picture
                    anchors.fill: parent
                    visible: !!card.modelData.image
                    source: k.img(card.modelData.image)
                    // A ready-made picture whole; a photo filling the space.
                    readonly property bool icon: (card.modelData.image ?? "").startsWith("store:emoji-")
                    anchors.margins: icon ? parent.height * 0.12 : 0
                    fillMode: icon ? Image.PreserveAspectFit : Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize.width: 600
                }
            }
            ColumnLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: photo.bottom
                anchors.bottom: parent.bottom
                anchors.margins: k.u * 0.5
                spacing: k.u * 0.15
                Text {
                    font.family: k.face
                    Layout.fillWidth: true
                    text: card.modelData.name
                    color: k.ink
                    font.pixelSize: k.u * 1.05
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    font.family: k.face
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: card.modelData.description
                    color: k.muted
                    font.pixelSize: k.u * 0.75
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                    maximumLineCount: 2
                }
                Text {
                    visible: (card.modelData.contains ?? []).length > 0
                    font.family: k.face
                    Layout.fillWidth: true
                    text: qsTr("Contains: %1").arg((card.modelData.contains ?? []).join(", "))
                    color: "#ff8a80"
                    font.pixelSize: k.u * 0.65
                    elide: Text.ElideRight
                }
                Text {
                    font.family: k.face
                    text: card.modelData.price + ((card.modelData.left ?? -1) >= 0 ? "  ·  " + qsTr("%1 left").arg(card.modelData.left) : "")
                    color: Qt.lighter(k.accent, 1.5)
                    font.pixelSize: k.u * 1.05
                    font.bold: true
                }
            }
            // Sold out: shown, but can't be ordered.
            Rectangle {
                anchors.fill: parent
                visible: !card.modelData.available
                color: "#c0141820"
                Text {
                    font.family: k.face
                    anchors.centerIn: parent
                    text: qsTr("SOLD OUT")
                    color: "#ff9a9e"
                    font.pixelSize: k.u * 1.4
                    font.bold: true
                    rotation: -12
                }
            }
        }
        // A MouseArea: it works inside the scrolling grid (a drag still scrolls).
        MouseArea {
            id: cardTap
            anchors.fill: parent
            enabled: card.modelData.available
            onClicked: k.pos.kioskAdd(card.modelData.id)
        }
    }

    // Their order, one line each: what, choices, price, and × to take it off.
    component OrderList: ListView {
        id: cart
        objectName: "kioskCart"
        clip: true
        spacing: k.u * 0.3
        model: k.lines
        ScrollBar.vertical: TouchScrollBar { id: cartBar; needed: cart.contentHeight > cart.height + 1 }
        delegate: RowLayout {
            id: row
            required property var modelData
            width: ListView.view.width - cartBar.room
            spacing: k.u * 0.4
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Text {
                    font.family: k.face
                    Layout.fillWidth: true
                    text: row.modelData.name
                    color: k.ink
                    font.pixelSize: k.u * 0.95
                    wrapMode: Text.WordWrap
                }
                Text {
                    font.family: k.face
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: (row.modelData.modifiers ?? []).map(m => m.name).join(", ")
                    color: k.muted
                    font.pixelSize: k.u * 0.75
                    wrapMode: Text.WordWrap
                }
            }
            Text { font.family: k.face; text: row.modelData.price; color: k.ink; font.pixelSize: k.u * 0.95 }
            Big {
                objectName: "kioskRemove"
                Layout.preferredWidth: k.u * 2.6
                Layout.preferredHeight: k.u * 2.6
                text: "×"
                size: 1.4
                base: "#3a2a2e"
                onClicked: k.pos.kioskRemove(row.modelData.id)
            }
        }
    }

    // --- attract: pictures and messages until someone touches -------------------------------
    Item {
        id: attract
        objectName: "kioskAttract"
        anchors.fill: parent
        visible: !k.ordering && !k.done && !k.choosingType
        onVisibleChanged: if (visible) k.avoid = []   // the next guest starts afresh
        // The store's slides (Store Settings → Customer display), then dishes with photos.
        readonly property var slides: (k.brand.slides ?? []).concat(
            (k.menu.items ?? []).filter(i => !!i.image && !i.image.startsWith("store:emoji-") && i.available).map(i => ({ image: i.image, text: i.name, price: i.price })))
        property int index: 0
        readonly property var current: slides.length ? slides[index % slides.length] : null
        readonly property string picture: !current ? ""
            : typeof current === "string" ? (current.startsWith("image:") ? k.img(current.slice(6)) : "")
            : k.img(current.image)
        readonly property string caption: !current ? "" : typeof current === "string" ? (current.startsWith("image:") ? "" : current)
                                                     : current.text + "  ·  " + current.price
        Timer {
            interval: 6000
            running: attract.visible && attract.slides.length > 1
            repeat: true
            onTriggered: fade.restart()
        }
        SequentialAnimation {
            id: fade
            NumberAnimation { target: slide; property: "opacity"; to: 0; duration: 400 }
            ScriptAction { script: attract.index = (attract.index + 1) % Math.max(1, attract.slides.length) }
            NumberAnimation { target: slide; property: "opacity"; to: 1; duration: 400 }
        }
        Item {
            id: slide
            anchors.fill: parent
            Image {
                anchors.fill: parent
                visible: attract.picture !== ""
                source: attract.picture
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
            }
            Rectangle {
                visible: attract.picture !== ""   // shade only a photo   // the text stays readable on a photo
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#80000000" }
                    GradientStop { position: 0.45; color: "#00000000" }
                    GradientStop { position: 0.7; color: "#30000000" }
                    GradientStop { position: 1.0; color: "#e0000000" }
                }
            }
        }
        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: k.u * 2
            spacing: k.u
            Image {
                visible: k.logo !== ""
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: k.u * 7
                Layout.preferredWidth: k.u * 22
                source: k.logo
                fillMode: Image.PreserveAspectFit
            }
            Text {
                font.family: k.face
                visible: k.logo === ""
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: k.pos ? k.pos.storeName : ""
                color: attract.picture !== "" ? "white" : k.ink
                font.pixelSize: k.u * 3
                font.bold: true
                wrapMode: Text.WordWrap
            }
        }
        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: k.u * 2
            spacing: k.u * 1.5
            Text {
                font.family: k.face
                Layout.fillWidth: true
                visible: attract.caption !== ""
                horizontalAlignment: Text.AlignHCenter
                text: attract.caption
                color: attract.picture !== "" ? "white" : k.ink
                font.pixelSize: k.u * 2
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.min(parent.width, k.u * 24)
                Layout.preferredHeight: k.u * 5.5
                radius: height / 2
                color: k.accent
                SequentialAnimation on scale {   // a gentle pulse: touch me
                    running: attract.visible
                    loops: Animation.Infinite
                    NumberAnimation { from: 1; to: 1.05; duration: 900; easing.type: Easing.InOutQuad }
                    NumberAnimation { from: 1.05; to: 1; duration: 900; easing.type: Easing.InOutQuad }
                }
                Text {
                    font.family: k.face
                    anchors.centerIn: parent
                    text: k.lookSet.welcome || qsTr("Touch to Order")
                    color: k.textOn(k.accent)
                    font.pixelSize: k.u * 2
                    font.bold: true
                }
            }
        }
        // Straight to the menu when the store doesn't ask For Here or To Go.
        MouseArea {
            anchors.fill: parent
            onClicked: k.lookSet.askWhere === false ? k.pos.kioskStart(false) : k.choosingType = true
        }
    }

    // --- the stage: everything a guest touches; Easy Reach puts it in the lower part ----------
    Item {
        id: stage
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: k.easyReach ? parent.height * 0.6 : parent.height
        Behavior on height { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

        // --- For Here / To Go ---
        ColumnLayout {
            visible: k.choosingType && !k.ordering && !k.done
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: k.u * 2
            anchors.bottomMargin: k.portrait ? parent.height * 0.18 : k.u * 4
            spacing: k.u * 1.5
            Text {
                font.family: k.face
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Where will you eat?")
                color: k.ink
                font.pixelSize: k.u * 2.2
                font.bold: true
            }
            GridLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.min(parent.width, k.u * 46)
                columns: 2
                columnSpacing: k.u * 1.5
                Big {
                    objectName: "kioskForHere"
                    Layout.fillWidth: true
                    Layout.preferredHeight: k.u * (k.portrait ? 10 : 8)
                    text: qsTr("For Here")
                    size: 2.2
                    base: k.accent
                    onClicked: k.pos.kioskStart(false)
                }
                Big {
                    objectName: "kioskToGo"
                    Layout.fillWidth: true
                    Layout.preferredHeight: k.u * (k.portrait ? 10 : 8)
                    text: qsTr("To Go")
                    size: 2.2
                    base: k.accent
                    onClicked: k.pos.kioskStart(true)
                }
            }
        }

        // --- landscape: categories | dishes | their order ---
        RowLayout {
            visible: k.ordering && !k.reviewing && !k.portrait
            anchors.fill: parent
            anchors.margins: k.u * 0.8
            spacing: k.u * 0.8

            ListView {
                Layout.preferredWidth: k.u * 12
                Layout.fillHeight: true
                spacing: k.u * 0.5
                clip: true
                model: k.families
                delegate: Big {
                    required property string modelData
                    width: ListView.view.width
                    height: k.u * 3.6
                    // "burgers" -> "Burgers" (and in the guest's language when it has the phrase)
                    text: qsTranslate("Page", modelData.charAt(0).toUpperCase() + modelData.slice(1))
                    size: 1.2
                    base: modelData === k.shownFamily ? k.accent : k.card
                    onClicked: k.family = modelData
                }
            }
            GridView {
                id: grid
                objectName: "kioskItems"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                readonly property int across: Math.max(1, Math.floor(width / (k.u * 13)))
                cellWidth: width / across
                cellHeight: k.u * 15
                model: k.items
                ScrollBar.vertical: TouchScrollBar { needed: grid.contentHeight > grid.height + 1 }
                delegate: Card { width: grid.cellWidth; height: grid.cellHeight }
            }
            Rectangle {
                Layout.preferredWidth: k.u * 19
                Layout.fillHeight: true
                radius: k.u * 0.6
                color: k.panel
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: k.u * 0.7
                    spacing: k.u * 0.4
                    Text {
                        font.family: k.face
                        Layout.fillWidth: true
                        text: k.info.toGo ? qsTr("Your order · to go") : qsTr("Your order · for here")
                        color: k.ink
                        font.pixelSize: k.u * 1.2
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    OrderList { Layout.fillWidth: true; Layout.fillHeight: true }
                    Text {
                        font.family: k.face
                        visible: k.lines.length === 0
                        Layout.fillWidth: true
                        text: qsTr("Touch something on the menu to add it.")
                        color: k.muted
                        font.pixelSize: k.u * 0.9
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { font.family: k.face; text: qsTr("Total"); color: k.ink; font.pixelSize: k.u * 1.2; font.bold: true; Layout.fillWidth: true }
                        Text { font.family: k.face; text: k.total; color: k.ink; font.pixelSize: k.u * 1.4; font.bold: true }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: k.u * 0.5
                        Big {
                            objectName: "kioskStartOver"
                            Layout.preferredWidth: k.u * 6
                            Layout.preferredHeight: k.u * 3.4
                            text: qsTr("Start Over")
                            size: 0.9
                            onClicked: k.pos.kioskCancel()
                        }
                        Big {
                            objectName: "kioskAllergies"
                            Layout.preferredWidth: k.u * 6
                            Layout.preferredHeight: k.u * 3.4
                            text: k.avoid.length ? qsTr("Avoiding %n", "", k.avoid.length) : qsTr("Allergies")
                            size: 0.9
                            base: k.avoid.length ? "#c62828" : k.card
                            onClicked: avoidSheet.visible = true
                        }
                        Big {
                            objectName: "kioskReview"
                            Layout.fillWidth: true
                            Layout.preferredHeight: k.u * 3.4
                            enabled: k.lines.length > 0
                            text: qsTr("Done ›")
                            base: k.go
                            onClicked: k.reviewing = true
                        }
                    }
                }
            }
        }

        // --- portrait: dishes above; categories and the order bar below, in reach ---
        ColumnLayout {
            visible: k.ordering && !k.reviewing && k.portrait
            anchors.fill: parent
            anchors.margins: k.u * 0.6
            spacing: k.u * 0.5

            RowLayout {   // who we are
                Layout.fillWidth: true
                Layout.fillHeight: false   // nested layouts fill by default
                visible: !k.easyReach
                Text {
                    font.family: k.face
                    Layout.fillWidth: true
                    text: k.pos ? k.pos.storeName : ""
                    color: k.ink
                    font.pixelSize: k.u * 1.4
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    font.family: k.face
                    text: k.info.toGo ? qsTr("To Go") : qsTr("For Here")
                    color: k.muted
                    font.pixelSize: k.u
                }
            }
            GridView {
                id: tallGrid
                objectName: "kioskItems"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                readonly property int across: k.easyReach ? 4 : 3
                cellWidth: width / across
                cellHeight: cellWidth * (k.easyReach ? 1.15 : 1.3)
                model: k.items
                ScrollBar.vertical: TouchScrollBar { needed: tallGrid.contentHeight > tallGrid.height + 1 }
                delegate: Card { width: tallGrid.cellWidth; height: tallGrid.cellHeight }
            }
            ListView {   // the categories
                Layout.fillWidth: true
                Layout.preferredHeight: k.u * 3.4
                orientation: ListView.Horizontal
                spacing: k.u * 0.5
                clip: true
                model: k.families
                delegate: Big {
                    required property string modelData
                    width: Math.max(k.u * 9, (ListView.view.width - k.u * 0.5 * (k.families.length - 1)) / Math.max(1, k.families.length))
                    height: k.u * 3.4
                    text: qsTranslate("Page", modelData.charAt(0).toUpperCase() + modelData.slice(1))
                    size: 1.1
                    base: modelData === k.shownFamily ? k.accent : k.card
                    onClicked: k.family = modelData
                }
            }
            RowLayout {   // the order bar
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.preferredHeight: k.u * 4
                spacing: k.u * 0.5
                Big {
                    objectName: "kioskStartOver"
                    Layout.preferredWidth: k.u * 6
                    Layout.fillHeight: true
                    text: qsTr("Start Over")
                    size: 0.85
                    onClicked: k.pos.kioskCancel()
                }
                Big {
                    objectName: "kioskAllergies"
                    Layout.preferredWidth: k.u * 6
                    Layout.fillHeight: true
                    text: k.avoid.length ? qsTr("Avoiding %n", "", k.avoid.length) : qsTr("Allergies")
                    size: 0.85
                    base: k.avoid.length ? "#c62828" : k.card
                    onClicked: avoidSheet.visible = true
                }
                Big {
                    objectName: "kioskEasyReach"
                    visible: k.lookSet.easyReach !== false
                    Layout.preferredWidth: k.u * 6
                    Layout.fillHeight: true
                    text: k.easyReach ? qsTr("Full Screen") : qsTr("Easy Reach")
                    size: 0.85
                    base: k.easyReach ? Qt.darker(k.accent, 1.3) : k.card
                    onClicked: k.easyReach = !k.easyReach
                }
                Big {
                    objectName: "kioskOrderBar"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    enabled: k.lines.length > 0
                    text: k.lines.length === 0 ? qsTr("Touch a dish to add it")
                          : (k.lines.length === 1 ? qsTr("1 item") : qsTr("%1 items").arg(k.lines.length)) + "  ·  " + k.total
                    size: 1
                    base: k.panel
                    onClicked: k.orderOpen = true
                }
                Big {
                    objectName: "kioskReview"
                    Layout.preferredWidth: k.u * 9
                    Layout.fillHeight: true
                    enabled: k.lines.length > 0
                    text: qsTr("Done ›")
                    base: k.go
                    onClicked: k.reviewing = true
                }
            }
        }

        // --- portrait: the order, as a sheet from the bottom ---
        Rectangle {
            visible: k.ordering && !k.reviewing && k.portrait && k.orderOpen
            anchors.fill: parent
            color: "#a0101318"
            MouseArea { anchors.fill: parent; onClicked: k.orderOpen = false }
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: Math.min(parent.height * 0.75, k.u * 8 + Math.max(k.u * 6, k.lines.length * k.u * 3.4) + k.u * 8)
                radius: k.u
                color: k.panel
                MouseArea { anchors.fill: parent }   // touches on the sheet stay here
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: k.u
                    spacing: k.u * 0.5
                    Text {
                        font.family: k.face
                        text: k.info.toGo ? qsTr("Your order · to go") : qsTr("Your order · for here")
                        color: k.ink
                        font.pixelSize: k.u * 1.4
                        font.bold: true
                    }
                    OrderList { Layout.fillWidth: true; Layout.fillHeight: true }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { font.family: k.face; text: qsTr("Total"); color: k.ink; font.pixelSize: k.u * 1.3; font.bold: true; Layout.fillWidth: true }
                        Text { font.family: k.face; text: k.total; color: k.ink; font.pixelSize: k.u * 1.5; font.bold: true }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: k.u * 0.6
                        Big {
                            Layout.preferredWidth: k.u * 12
                            Layout.preferredHeight: k.u * 3.8
                            text: qsTr("Keep Ordering")
                            size: 1
                            onClicked: k.orderOpen = false
                        }
                        Big {
                            Layout.fillWidth: true
                            Layout.preferredHeight: k.u * 3.8
                            enabled: k.lines.length > 0
                            text: qsTr("Done ›")
                            base: k.go
                            onClicked: { k.orderOpen = false; k.reviewing = true }
                        }
                    }
                }
            }
        }

        // --- review: a name to call the order by ---
        ColumnLayout {
            visible: k.ordering && k.reviewing
            anchors.fill: parent
            anchors.margins: k.u
            spacing: k.u * 0.8
            Item { Layout.fillHeight: true; visible: k.portrait && !k.easyReach }   // in reach: at the bottom
            Text {
                font.family: k.face
                text: qsTr("Almost done: %1").arg(k.total)
                color: k.ink
                font.pixelSize: k.u * 2
                font.bold: true
            }
            Text {
                font.family: k.face
                Layout.fillWidth: true
                text: k.lines.map(l => l.name).join(" · ")
                color: k.muted
                font.pixelSize: k.u
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
            Text { font.family: k.face; visible: k.lookSet.askName !== false; text: qsTr("Your name, so we can call your order"); color: k.ink; font.pixelSize: k.u * 1.2 }
            TextField {
                id: nameField
                objectName: "kioskName"
                visible: k.lookSet.askName !== false
                Layout.fillWidth: true
                implicitHeight: k.u * 3.4
                font.pixelSize: k.u * 1.6
                maximumLength: 40
                placeholderText: qsTr("Name")
                onVisibleChanged: if (visible) { text = ""; forceActiveFocus() }
                onAccepted: place.clicked()
            }
            TouchKeyboard {
                visible: k.lookSet.askName !== false
                Layout.fillWidth: true
                Layout.fillHeight: !k.portrait || k.easyReach
                Layout.preferredHeight: k.portrait && !k.easyReach ? k.u * 16 : -1
                target: nameField
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: k.u * 0.6
                Big {
                    Layout.preferredWidth: k.u * 12
                    Layout.preferredHeight: k.u * 3.6
                    text: qsTr("‹ Back to the Menu")
                    size: 1
                    onClicked: k.reviewing = false
                }
                Big {
                    id: place
                    objectName: "kioskPlace"
                    Layout.fillWidth: true
                    Layout.preferredHeight: k.u * 3.6
                    enabled: k.lookSet.askName === false || nameField.text.trim().length > 0
                    text: qsTr("Place My Order")
                    base: k.go
                    onClicked: k.pos.kioskFinish({ name: nameField.text, avoid: k.avoid })
                }
            }
        }

        // --- the order number ---
        ColumnLayout {
            visible: k.done && !k.ordering
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.85, k.u * 50)
            spacing: k.u
            Text {
                font.family: k.face
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Thank you, %1!").arg(k.lastOrder.name ?? "")
                color: k.ink
                font.pixelSize: k.u * 2.4
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Text { font.family: k.face; Layout.alignment: Qt.AlignHCenter; text: qsTr("Your order number"); color: k.soft; font.pixelSize: k.u * 1.4 }
            Text {
                font.family: k.face
                objectName: "kioskNumber"
                Layout.alignment: Qt.AlignHCenter
                text: String(k.lastOrder.number ?? "")
                color: Qt.lighter(k.accent, 1.5)
                font.pixelSize: k.u * 8
                font.bold: true
            }
            Text {
                font.family: k.face
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: k.lastOrder.sent ? qsTr("We're making it now. Please pay at the counter.")
                                       : qsTr("Please pay at the counter (%1), and we'll start making it.").arg(k.lastOrder.total ?? "")
                color: k.ink
                font.pixelSize: k.u * 1.4
                wrapMode: Text.WordWrap
            }
            Text {
                objectName: "kioskTakeSlip"
                font.family: k.face
                visible: k.lastOrder.slip ?? false
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Take your slip below and show it at the counter.")
                color: k.ink
                font.pixelSize: k.u * 1.2
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Big {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: k.u * 16
                Layout.topMargin: k.u
                text: qsTr("New Order")
                base: k.accent
                onClicked: k.pos.kioskCancel()
            }
        }

        // --- choices (temperature, sides...) for what was just added: a sheet from the bottom ---
        Rectangle {
            visible: k.ordering && (k.choosing.active ?? false)
            anchors.fill: parent
            color: "#d0101318"
            MouseArea { anchors.fill: parent }   // touches stop here
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: k.portrait ? 0 : (parent.height - height) / 2
                width: k.portrait ? parent.width : Math.min(parent.width * 0.92, k.u * 46)
                height: Math.min(parent.height * (k.portrait ? 0.75 : 0.9), choicesColumn.implicitHeight + k.u * 2)
                radius: k.u * 0.8
                color: k.panel
                ColumnLayout {
                    id: choicesColumn
                    anchors.fill: parent
                    anchors.margins: k.u
                    spacing: k.u * 0.6
                    Text {
                        font.family: k.face
                        text: k.choosing.item ?? ""
                        color: k.ink
                        font.pixelSize: k.u * 1.6
                        font.bold: true
                    }
                    TouchScrollColumn {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredHeight: implicitHeight
                        spacing: k.u * 0.5
                        // Counted, not the lists themselves: a choice updates the
                        // buttons in place, so the sheet stays where it was scrolled
                        // (a new list rebuilt them and jumped back to the top).
                        Repeater {
                            model: (k.choosing.groups ?? []).length
                            delegate: ColumnLayout {
                                id: group
                                required property int index
                                readonly property var modelData: (k.choosing.groups ?? [])[index] ?? ({})
                                readonly property int optionCount: (modelData.options ?? []).length
                                Layout.fillWidth: true
                                spacing: k.u * 0.3
                                Text {
                                    font.family: k.face
                                    text: group.modelData.onlyIncluded ? group.modelData.name
                                                                       : group.modelData.name + "  ·  " + group.modelData.rule
                                    color: group.modelData.done ? k.soft : "#f5b940"
                                    font.pixelSize: k.u * 1.1
                                    font.bold: true
                                }
                                // What comes on it: each No, Light, as it comes, Extra or on the side.
                                Text {
                                    visible: group.modelData.included && !group.modelData.onlyIncluded
                                    font.family: k.face
                                    text: qsTr("What's on it")
                                    color: k.soft
                                    font.pixelSize: k.u * 0.95
                                }
                                Repeater {
                                    model: group.optionCount
                                    delegate: RowLayout {
                                        id: onIt
                                        required property int index
                                        readonly property var modelData: (group.modelData.options ?? [])[index] ?? ({})
                                        visible: modelData.included ?? false
                                        Layout.fillWidth: true
                                        spacing: k.u * 0.3
                                        Text {
                                            font.family: k.face
                                            Layout.preferredWidth: k.u * 7
                                            text: onIt.modelData.name
                                            color: onIt.modelData.how === "no" ? k.soft : k.ink
                                            font.strikeout: onIt.modelData.how === "no"
                                            font.pixelSize: k.u * 1.05
                                            elide: Text.ElideRight
                                        }
                                        Repeater {
                                            model: [{ how: "no", text: qsTr("No") }, { how: "lite", text: qsTr("Light") },
                                                    { how: "", text: qsTr("Regular") }, { how: "extra", text: qsTr("Extra") },
                                                    { how: "side", text: qsTr("On the side") }]
                                            delegate: Big {
                                                required property var modelData
                                                objectName: "kioskHow-" + onIt.modelData.name + "-" + (modelData.how || "regular")
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: k.u * 2.4
                                                text: modelData.text
                                                size: 0.75
                                                base: (onIt.modelData.how ?? "") === modelData.how ? k.accent : k.card
                                                onClicked: k.pos.setChoice(group.modelData.id, onIt.modelData.index,
                                                                           modelData.how === "" ? "off" : modelData.how)
                                            }
                                        }
                                    }
                                }
                                Text {
                                    visible: group.modelData.included && !group.modelData.onlyIncluded
                                    font.family: k.face
                                    text: qsTr("Add")
                                    color: k.soft
                                    font.pixelSize: k.u * 0.95
                                }
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: k.u * 0.4
                                    Repeater {
                                        model: group.optionCount
                                        delegate: Big {
                                            required property int index
                                            readonly property var modelData: (group.modelData.options ?? [])[index] ?? ({})
                                            visible: !(modelData.included ?? false)
                                            width: k.u * 10
                                            height: k.u * 3.2
                                            text: modelData.name + (modelData.soldOut ? "\n" + qsTr("sold out")
                                                                    : modelData.price ? "\n+" + modelData.price : "")
                                            enabled: !modelData.soldOut || modelData.chosen
                                            opacity: enabled ? 1 : 0.4
                                            size: 0.95
                                            base: modelData.chosen ? k.accent : k.card
                                            onClicked: k.pos.chooseOption(group.modelData.id, modelData.index)
                                        }
                                    }
                                }
                                // A choice made, had Light, Extra or on the side (dressing on the side).
                                Repeater {
                                    model: group.optionCount
                                    delegate: RowLayout {
                                        id: added
                                        required property int index
                                        readonly property var modelData: (group.modelData.options ?? [])[index] ?? ({})
                                        visible: (group.modelData.askHow ?? false) && !(modelData.included ?? false)
                                                 && (modelData.chosen ?? false)
                                        Layout.fillWidth: true
                                        spacing: k.u * 0.3
                                        Text {
                                            font.family: k.face
                                            Layout.preferredWidth: k.u * 7
                                            text: added.modelData.name
                                            color: k.ink
                                            font.pixelSize: k.u * 1.05
                                            elide: Text.ElideRight
                                        }
                                        Repeater {
                                            model: [{ how: "", text: qsTr("Regular") }, { how: "lite", text: qsTr("Light") },
                                                    { how: "extra", text: qsTr("Extra") }, { how: "side", text: qsTr("On the side") }]
                                            delegate: Big {
                                                required property var modelData
                                                objectName: "kioskHow-" + added.modelData.name + "-" + (modelData.how || "regular")
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: k.u * 2.4
                                                text: modelData.text
                                                size: 0.75
                                                base: (added.modelData.how ?? "") === modelData.how ? k.accent : k.card
                                                onClicked: k.pos.setChoice(group.modelData.id, added.modelData.index, modelData.how)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: k.u * 0.6
                        Big {
                            Layout.preferredWidth: k.u * 10
                            Layout.preferredHeight: k.u * 3.4
                            text: qsTr("Remove It")
                            size: 1
                            base: "#3a2a2e"
                            onClicked: k.pos.kioskRemove(k.choosing.lineId)
                        }
                        Big {
                            objectName: "kioskChoicesDone"
                            Layout.fillWidth: true
                            Layout.preferredHeight: k.u * 3.4
                            enabled: (k.choosing.groups ?? []).every(g => g.done)
                            text: qsTr("Add to My Order")
                            base: k.go
                            onClicked: k.pos.finishChoosing()
                        }
                    }
                }
            }
        }

        // --- still there? ---
        Rectangle {
            id: stillThere
            visible: false
            anchors.fill: parent
            color: "#e0101318"
            ColumnLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: k.u * 2
                anchors.bottomMargin: parent.height * 0.2
                spacing: k.u
                Text {
                    font.family: k.face
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Are you still there?")
                    color: k.ink
                    font.pixelSize: k.u * 2.4
                    font.bold: true
                }
                Text {
                    font.family: k.face
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Your order will be cleared in %1 seconds.").arg(k.idleLeft)
                    color: k.soft
                    font.pixelSize: k.u * 1.3
                    wrapMode: Text.WordWrap
                }
                Big {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.min(parent.width, k.u * 30)
                    Layout.preferredHeight: k.u * 4.4
                    text: qsTr("Yes, Keep Ordering")
                    base: k.accent
                    onClicked: k.touched()
                }
            }
        }
    }

    // Easy Reach: the top of the screen just shows who we are.
    Text {
        font.family: k.face
        visible: k.easyReach && k.ordering
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.top
        anchors.verticalCenterOffset: (parent.height - stage.height) / 2
        text: k.pos ? k.pos.storeName : ""
        color: "#4a5263"
        font.pixelSize: k.u * 2.4
        font.bold: true
    }

    // --- staff only: hold the top-left corner for 3 seconds -------------------------------
    Item {
        width: k.u * 4
        height: k.u * 4
        z: 10
        TapHandler {
            longPressThreshold: 3
            onLongPressed: k.managerExit()
        }
    }
}
