import QtQuick

// Menu items as buttons, laid out by themselves: the order screen's menu, and
// the Menu Builder's preview of it. Under their section headings ("Tacos"),
// with a space or a new row before an item when it asks for one; sized by
// the category (small, medium, large) or to fit them all without scrolling.
// itemTapped(item) and itemHeld(item) are for the owner to act on.
Flickable {
    id: mb
    objectName: "menuButtons"
    // The items, in order (pos.menuItems' entries).
    property var items: []
    // The category shown ({} for Favorites, a search, best sellers): its
    // button size, photos, prices, shades.
    property var category: ({})
    // Every category (an item's color comes from its own).
    property var categories: []
    property PosService pos
    property int setColumns: 0        // the page's columns (0: fit)
    property bool pagePhotos: false
    property bool sectioned: true     // headings and spaces (one category shown)
    property var st: ({})             // the page's key style
    property string face: "DejaVu Sans"
    property real gap: 12
    property bool arranging: false
    property string picked: ""
    property bool managing: false     // held: changed right there (managers)
    property var allergies: []
    signal itemTapped(var item)
    signal itemHeld(var item)

    readonly property bool photos: pagePhotos || category.photos === true
    readonly property bool hidePrice: category.hidePrice === true
    readonly property string buttonSize: category.buttonSize ?? ""
    readonly property real sizeWidth: ({ small: 190, medium: 270, large: 400 })[buttonSize] ?? 0
    readonly property real headerHeight: 44

    // Sections in the order their first item comes; the rest under none.
    readonly property var blocks: {
        if (!sectioned)
            return [{ title: "", items: items }]
        const out = []
        for (const i of items) {
            const t = i.section ?? ""
            let b = out.find(x => x.title === t)
            if (!b) { b = { title: t, items: [] }; out.push(b) }
            b.items.push(i)
        }
        // Items with no section first, the rest as they come.
        return out.filter(b => b.title === "").concat(out.filter(b => b.title !== ""))
    }
    readonly property int headings: blocks.filter(b => b.title !== "").length
    // Each block's cells for `columns` across: items, and gaps for a space or
    // a new row before one.
    function cellsOf(block, columns) {
        const out = []
        for (const i of block.items) {
            if (sectioned && out.length > 0) {
                if (i.breakBefore === "space")
                    out.push({ gap: true })
                else if (i.breakBefore === "row")
                    while (out.length % columns !== 0) out.push({ gap: true })
            }
            out.push(i)
        }
        return out
    }
    function rowsFor(columns) {
        let n = 0
        for (const b of blocks) n += Math.max(1, Math.ceil(cellsOf(b, columns).length / columns))
        return n
    }

    // Columns: by the category's size; the page's; or as many as fit all of
    // them without scrolling, as big as they can be.
    readonly property int fitColumns: {
        const W = width, H = height - headings * headerHeight
        if (W <= 0 || H <= 0)
            return 4
        let best = 3, bestHeight = 0
        for (let c = 2; c <= 7; ++c) {
            const h = Math.min(W / c * (photos ? 0.9 : 0.62), H / rowsFor(c))
            if (h > bestHeight + 0.5) { best = c; bestHeight = h }
        }
        return best
    }
    readonly property int columns: sizeWidth > 0 ? Math.max(1, Math.round(width / sizeWidth))
                                 : setColumns > 0 ? setColumns : fitColumns
    readonly property real cellWidth: width / columns
    readonly property real cellHeight: sizeWidth > 0 ? cellWidth * (photos ? 0.95 : ({ small: 0.62, medium: 0.68, large: 0.72 })[buttonSize])
                                     : setColumns > 0 ? Math.min(cellWidth * (photos ? 0.9 : 0.6), Math.max(110, height / 3))
                                     : Math.max(90, Math.min(cellWidth * (photos ? 0.9 : 0.75),
                                                             (height - headings * headerHeight) / rowsFor(columns)))

    function categoryOf(id) { return categories.find(c => c.id === id) ?? ({}) }
    function img(ref) { return pos && ref ? (pos.imageRevision < 0 ? undefined : pos.imageUrl(ref)) : "" }
    readonly property var allergenNames: pos ? pos.allergenList() : []
    function allergenName(id) { return (allergenNames.find(a => a.id === id) ?? { name: id }).name }
    function allergyHits(item) { return (item.allergens ?? []).filter(a => allergies.includes(a)) }
    // Its own color; else its category's, or a shade of it (each a little different).
    function colorOf(item, index) {
        if (item.buttonColor) return item.buttonColor
        const c = categoryOf(item.family)
        if (!c.color) return ""
        if (!c.shades) return c.color
        const k = [1, 1.22, 0.82, 1.1, 0.9][index % 5]
        return String(k >= 1 ? Qt.lighter(c.color, k) : Qt.darker(c.color, 1 / k))
    }

    contentWidth: width
    contentHeight: column.height
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    interactive: contentHeight > height + 1

    Column {
        id: column
        width: mb.width
        Repeater {
            model: mb.blocks
            delegate: Column {
                id: block
                required property var modelData
                width: mb.width
                Text {
                    visible: block.modelData.title !== ""
                    objectName: "menuSection-" + block.modelData.title
                    width: parent.width
                    height: visible ? mb.headerHeight : 0
                    leftPadding: mb.gap / 2
                    verticalAlignment: Text.AlignBottom
                    bottomPadding: 6
                    text: qsTranslate("Page", block.modelData.title)
                    color: mb.st.textColor ?? "#c8d0dc"
                    font.family: mb.face
                    font.pixelSize: 24
                    font.bold: true
                }
                Grid {
                    columns: mb.columns
                    Repeater {
                        model: mb.cellsOf(block.modelData, mb.columns)
                        delegate: Item {
                            id: cell
                            required property var modelData
                            required property int index
                            width: mb.cellWidth
                            height: mb.cellHeight
                            readonly property bool isGap: cell.modelData.gap === true
                            readonly property string itemColor: isGap ? "" : mb.colorOf(cell.modelData, index)
                            Rectangle {
                                id: card
                                visible: !cell.isGap
                                objectName: cell.isGap ? "" : "menuItem-" + cell.modelData.id
                                anchors.fill: parent
                                anchors.margins: mb.gap / 2
                                readonly property var st: mb.st
                                radius: st.keyRadius !== undefined ? st.keyRadius : 14
                                color: !cell.modelData.available ? "#3a3f48"
                                     : press.pressed ? (st.keyLitFill ?? "#4c8dff") : (cell.itemColor || (st.keyFill ?? "#343c49"))
                                readonly property bool isPicked: mb.arranging && mb.picked === cell.modelData.id
                                readonly property var hits: cell.isGap ? [] : mb.allergyHits(cell.modelData)
                                border.color: isPicked ? "#f5b940" : hits.length ? "#ff3b3b" : Qt.darker(color, 1.4)
                                border.width: isPicked ? 6 : hits.length ? 6 : 2
                                // Has something the guest is allergic to.
                                Rectangle {
                                    objectName: "allergyMark-" + cell.modelData.id
                                    visible: parent.hits.length > 0
                                    z: 3
                                    anchors.bottom: parent.bottom
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.bottomMargin: 4
                                    width: Math.min(parent.width - 8, hitText.implicitWidth + 14)
                                    height: hitText.implicitHeight + 4
                                    radius: height / 2
                                    color: "#c62828"
                                    Text {
                                        id: hitText
                                        anchors.centerIn: parent
                                        width: Math.min(implicitWidth, parent.parent.width - 22)
                                        elide: Text.ElideRight
                                        text: "⚠ " + parent.parent.hits.map(a => mb.allergenName(a)).join(", ")
                                        color: "white"
                                        font.family: mb.face
                                        font.pixelSize: Math.max(15, Math.min(cell.height * 0.09, 22))
                                        font.bold: true
                                    }
                                }
                                opacity: cell.modelData.available ? 1 : 0.55
                                // Running low: "5 left".
                                readonly property int itemsLeft: mb.pos && !cell.isGap ? (mb.pos.stockLeft[cell.modelData.id] ?? -1) : -1
                                Rectangle {
                                    visible: parent.itemsLeft > 0 && cell.modelData.available
                                    z: 2
                                    anchors.top: parent.top
                                    anchors.right: parent.right
                                    anchors.margins: 6
                                    width: leftLabel.implicitWidth + 12
                                    height: leftLabel.implicitHeight + 4
                                    radius: height / 2
                                    color: "#e0161a20"
                                    border.color: "#f5b940"
                                    border.width: 2
                                    Text {
                                        id: leftLabel
                                        anchors.centerIn: parent
                                        text: qsTr("%1 left").arg(parent.parent.itemsLeft)
                                        color: "#f5b940"
                                        font.family: mb.face
                                        font.pixelSize: 14
                                        font.bold: true
                                    }
                                }
                                Text {
                                    visible: !!cell.modelData.number
                                    z: 2
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.margins: 8
                                    text: cell.modelData.number ?? ""
                                    color: "#8a94a6"
                                    font.family: mb.face
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                                Image {
                                    id: photo
                                    visible: mb.photos && status === Image.Ready
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.margins: 6
                                    height: parent.height * 0.55
                                    // A ready-made picture whole; a photo filling the space.
                                    fillMode: !cell.isGap && (cell.modelData.image ?? "").startsWith("store:emoji-")
                                              ? Image.PreserveAspectFit : Image.PreserveAspectCrop
                                    asynchronous: true
                                    source: mb.photos && !cell.isGap ? mb.img(cell.modelData.image) : ""
                                }
                                Column {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.top: photo.visible ? photo.bottom : parent.top
                                    anchors.margins: 8
                                    spacing: 2
                                    Item { width: 1; height: Math.max(0, (parent.height - name.height - (price.visible ? price.height : 0)) / 2 - 2) }
                                    Text {
                                        id: name
                                        width: parent.width
                                        horizontalAlignment: Text.AlignHCenter
                                        wrapMode: Text.WordWrap
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                        text: cell.isGap ? "" : qsTranslate("Page", cell.modelData.name)
                                        color: cell.itemColor ? StoreColors.ink(cell.itemColor) : (card.st.keyTextColor ?? "white")
                                        font.family: card.st.keyFont ?? mb.face
                                        // Smaller for a long word, so it never breaks mid-word.
                                        readonly property int longest: cell.isGap ? 4 : Math.max(4, ...cell.modelData.name.split(/\s+/).map(x => x.length))
                                        font.pixelSize: Math.max(13, Math.min(cell.height * 0.16, cell.width * 0.11, cell.width * 1.6 / longest))
                                        font.bold: true
                                    }
                                    Text {
                                        id: price
                                        objectName: cell.isGap ? "" : "menuPrice-" + cell.modelData.id
                                        visible: !mb.hidePrice || !cell.modelData.available
                                        width: parent.width
                                        horizontalAlignment: Text.AlignHCenter
                                        text: cell.isGap ? "" : !cell.modelData.available ? qsTr("SOLD OUT")
                                            : cell.modelData.price + (cell.modelData.byWeight ? " / " + cell.modelData.unit : "")
                                        color: !cell.modelData.available ? "#ff6b6b" : name.color
                                        opacity: cell.modelData.available ? 0.8 : 1
                                        font.family: card.st.keyFont ?? mb.face
                                        font.pixelSize: Math.max(12, name.font.pixelSize * 0.8)
                                    }
                                }
                                // A MouseArea: inside a Flickable a TapHandler loses the press.
                                MouseArea {
                                    id: press
                                    anchors.fill: parent
                                    enabled: !cell.isGap && (cell.modelData.available || mb.arranging || mb.managing)
                                    pressAndHoldInterval: 700
                                    onPressAndHold: if (mb.managing) mb.itemHeld(cell.modelData)
                                    onClicked: mb.itemTapped(cell.modelData)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
