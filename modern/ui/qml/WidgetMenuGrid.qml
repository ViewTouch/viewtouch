import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// The menu, laid out by itself: a button for every item of a family (or of
// every family, with a chip for each across the top). New items, prices and
// sold-out items show up with no page editing. props.family: one family, or
// empty for all; props.columns; props.photos: show item photos;
// props.search: the items whose name has what's typed (the Find page);
// props.popular: today's best sellers, most sold first (the Popular page).
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string fixedFamily: zone && zone.props && zone.props.family ? zone.props.family : ""
    // Columns: set by the page (0: as many as fit; see MenuButtons).
    readonly property int setColumns: zone && zone.props && zone.props.columns > 0 ? zone.props.columns : 0
    readonly property bool pagePhotos: zone && zone.props && zone.props.photos === true
    readonly property bool search: zone && zone.props && zone.props.search === true
    readonly property bool popular: zone && zone.props && zone.props.popular === true
    readonly property string typed: pos && search ? pos.textEntry.trim().toLowerCase() : ""
    readonly property string face: zone.st.font ?? "DejaVu Sans"

    readonly property var items: pos ? pos.menuItems.filter(i => !i.modifier) : []
    // The menu's categories with items, in their order (Menu Builder).
    readonly property var categories: pos ? pos.menuCategories.filter(c => c.count > 0) : []
    // The guest's allergies (the check's): items that contain one are marked.
    readonly property var allergies: pos && pos.hasCheck ? (pos.check.allergies ?? []) : []
    // ★ Favorites first: the items marked so, then today's best sellers (12 in all).
    readonly property var favorites: {
        const out = items.filter(i => i.favorite)
        for (const id of (pos ? pos.popularItems : []))
            if (out.length < 12 && !out.some(i => i.id === id)) {
                const i = items.find(x => x.id === id)
                if (i) out.push(i)
            }
        return out.slice(0, Math.max(12, items.filter(i => i.favorite).length))
    }
    // ♥ Mine: what the person logged in orders most (their regulars' usual
    // drinks, the specials they push); once there are a few.
    readonly property var mine: (pos ? pos.myUsuals : []).map(id => items.find(i => i.id === id)).filter(i => i !== undefined)
    readonly property var families: (favorites.length ? ["★"] : []).concat(mine.length >= 4 ? ["♥"] : [])
                                    .concat(categories.map(c => c.id))
    readonly property bool special: family === "★" || family === "♥"
    // The category being shown: the one touched on the menu screen.
    readonly property string chosen: zone && zone.controller ? zone.controller.menuCategory : ""
    readonly property string family: fixedFamily !== "" ? fixedFamily
                                    : families.includes(chosen) ? chosen
                                    : ((categories.find(c => c.now) ?? categories[0] ?? {}).id ?? "")
    function categoryOf(id) { return categories.find(c => c.id === id) ?? ({}) }
    // The category shown: its own button size, photos, prices (Menu Builder).
    readonly property var shownCategory: !search && !popular && !special ? categoryOf(family) : ({})
    // Searching: every family, best matches first (the name starts with it, then a word does).
    readonly property var shown: {
        if (popular)
            return (pos ? pos.popularItems : []).map(id => items.find(i => i.id === id)).filter(i => i !== undefined)
        if (!search)
            return family === "★" ? favorites : family === "♥" ? mine : items.filter(i => i.family === family)
        if (typed === "")
            return []
        const digits = /^[0-9]+$/.test(typed)
        const rank = i => {
            if (digits && i.number && i.number.startsWith(typed))
                return i.number === typed ? 0 : 1            // its number first
            const n = i.name.toLowerCase()
            return n.startsWith(typed) ? 2 : n.split(/[^a-z0-9]+/).some(word => word.startsWith(typed)) ? 3
                 : n.includes(typed) ? 4 : -1
        }
        return items.map(i => ({ item: i, rank: rank(i) })).filter(x => x.rank >= 0)
                    .sort((a, b) => a.rank - b.rank || a.item.name.localeCompare(b.item.name)).map(x => x.item)
    }
    function title(f) {
        if (f === "★")
            return qsTr("★ Favorites")
        if (f === "♥")
            return qsTr("♥ Mine")
        const name = categoryOf(f).name ?? ""
        return f === "" ? qsTr("Other") : qsTranslate("Page", name !== "" ? name : f.charAt(0).toUpperCase() + f.slice(1))
    }

    readonly property real gap: 12
    // Arranging (managers): touch an item, then move it or color it.
    // Kept by the controller: a menu change rebuilds the page.
    property bool arranging: zone && zone.controller ? zone.controller.widgetState(zone.zoneId + ".arranging") === true : false
    property string picked: zone && zone.controller ? (zone.controller.widgetState(zone.zoneId + ".picked") ?? "") : ""
    function setArranging(on) {
        arranging = on
        picked = ""
        zone.controller.setWidgetState(zone.zoneId + ".arranging", on)
        zone.controller.setWidgetState(zone.zoneId + ".picked", "")
    }
    function pick(id) {
        picked = id
        zone.controller.setWidgetState(zone.zoneId + ".picked", id)
    }
    readonly property bool mayArrange: pos !== null && pos.loggedIn && pos.can("manager") && !search && !popular

    // Rush Radar: which stations are about to back up, and which aren't
    // (a server can steer a table to what comes out faster).
    RushRadarView {
        id: radar
        visible: false
        active: w.visible && w.pos !== null && w.pos.loggedIn
        pos: w.pos
    }
    readonly property var busy: {
        const out = {}
        for (const s of (radar.radar.stations ?? []))
            if (s.level !== "ok")
                out[s.id] = { wait: s.peakWait, level: s.level, name: s.name }
        return out
    }
    readonly property string busyHint: {
        const all = radar.radar.stations ?? []
        const slow = all.filter(s => s.level !== "ok")
        if (!slow.length)
            return ""
        const quick = all.filter(s => s.level === "ok").map(s => qsTr("%1 %2 min").arg(s.name).arg(s.peakWait))
        return slow.map(s => qsTr("%1 busy: %2 min").arg(s.name).arg(s.peakWait)).join(" · ")
               + (quick.length ? "  ·  " + qsTr("faster: %1").arg(quick.join(", ")) : "")
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: w.gap

        Text {
            objectName: "menuBusyHint"
            visible: w.busyHint !== "" && !w.arranging
            Layout.fillWidth: true
            text: "⚠ " + w.busyHint
            color: "#f5b940"
            font.family: w.face
            font.bold: true
            font.pixelSize: Math.max(15, w.zone ? w.zone.touch(17) : 20)
            elide: Text.ElideRight
        }

        // Families across the top (only when showing them all). A phone:
        // three to a row; all with one text size, the largest at which each
        // name fits (each shrinking on its own left "Breakfast Plates" tiny
        // beside "Burgers").
        Flow {
            id: tabs
            Layout.fillWidth: true
            visible: !w.search && !w.popular && w.fixedFamily === "" && w.families.length > 1
            spacing: w.gap * 0.6
            // As many as fit at 120 pixels on the screen (a phone: three).
            readonly property real screenWidth: w.width * (w.zone ? w.zone.screenScale : 1)
            readonly property int perRow: Math.max(2, Math.min(6, Math.floor(screenWidth / 120)))
            readonly property real keyW: perRow < 6 ? (w.width - spacing * (perRow - 1)) / perRow
                                                    : Math.max(160, (w.width - spacing * 5) / 6)
            readonly property real keyH: Math.max(56, w.zone ? w.zone.touch(52) : 56)
            FontMetrics { id: tabFont; font.pixelSize: 100; font.bold: true; font.family: w.zone ? (w.zone.st.keyFont ?? "DejaVu Sans") : "DejaVu Sans" }
            readonly property real textPx: {
                let px = keyH * 0.38
                for (const f of w.families.map(x => w.title(x)).concat([qsTr("Arrange…")]))
                    px = Math.min(px, (keyW - 16) * 100 / Math.max(1, tabFont.advanceWidth(f)))
                return Math.max(10, px)
            }
            Repeater {
                model: w.families
                delegate: WidgetKey {
                    required property string modelData
                    objectName: "menuFamily-" + modelData
                    width: tabs.keyW
                    height: tabs.keyH
                    text: w.title(modelData)
                    accent: modelData === w.family
                    pixelSize: tabs.textPx
                    onClicked: w.zone.controller.menuCategory = modelData
                }
            }
            WidgetKey {
                objectName: "menuArrange"
                visible: w.mayArrange && !w.arranging
                width: tabs.keyW
                height: tabs.keyH
                text: qsTr("Arrange…")
                pixelSize: tabs.textPx
                onClicked: w.setArranging(true)
            }
        }

        MenuButtons {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            items: w.shown
            category: w.shownCategory
            categories: w.categories
            pos: w.pos
            textScale: w.zone ? w.zone.textScale : 1
            busy: w.busy
            setColumns: w.setColumns
            pagePhotos: w.pagePhotos
            // Headings and spaces: one category, as arranged in the Menu Builder.
            sectioned: !w.search && !w.popular && !w.special
            st: w.zone ? w.zone.st : ({})
            face: w.face
            gap: w.gap
            arranging: w.arranging
            picked: w.picked
            managing: w.mayArrange
            allergies: w.allergies
            onItemTapped: item => {
                if (w.arranging) w.pick(item.id)
                else if (item.available) w.zone.controller.orderItem(item.id, w.search)
            }
            // A manager holds one: changed right here.
            onItemHeld: item => quickCard.edit(item)
        }
        // Arranging: move the picked item, color it, Done.
        RowLayout {
            objectName: "arrangeBar"
            visible: w.arranging
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: w.gap * 0.5
            readonly property real keyH: Math.max(56, w.zone ? w.zone.touch(52) : 56)
            WidgetKey {
                objectName: "arrangeEarlier"
                enabled: w.picked !== ""
                opacity: enabled ? 1 : 0.4
                Layout.preferredWidth: 150; Layout.preferredHeight: parent.keyH
                text: qsTr("◀ Earlier")
                fontScale: 0.34
                onClicked: w.pos.moveMenuItem(w.picked, -1)
            }
            WidgetKey {
                objectName: "arrangeLater"
                enabled: w.picked !== ""
                opacity: enabled ? 1 : 0.4
                Layout.preferredWidth: 150; Layout.preferredHeight: parent.keyH
                text: qsTr("Later ▶")
                fontScale: 0.34
                onClicked: w.pos.moveMenuItem(w.picked, 1)
            }
            // Its color: the store's colors, its own, or one mixed.
            WidgetKey {
                objectName: "arrangeColor"
                enabled: w.picked !== ""
                opacity: enabled ? 1 : 0.4
                Layout.preferredWidth: 170; Layout.preferredHeight: parent.keyH
                readonly property var item: w.pos ? w.pos.menuItems.find(i => i.id === w.picked) : null
                text: "🎨 " + qsTr("Color…")
                baseColor: item && item.buttonColor ? item.buttonColor : "#343c49"
                fontScale: 0.32
                onClicked: colorPopup.open()
            }
            WidgetKey {
                objectName: "arrangeEdit"
                enabled: w.picked !== ""
                opacity: enabled ? 1 : 0.4
                Layout.preferredWidth: 140; Layout.preferredHeight: parent.keyH
                text: qsTr("Edit…")
                fontScale: 0.32
                onClicked: {
                    const i = w.items.find(x => x.id === w.picked)
                    if (i) quickCard.edit(i)
                }
            }
            // A new one, in the category shown.
            WidgetKey {
                objectName: "arrangeAdd"
                visible: w.family !== "" && !w.special
                Layout.preferredWidth: 170; Layout.preferredHeight: parent.keyH
                text: qsTr("+ New Item")
                fontScale: 0.32
                onClicked: quickCard.add(w.family)
            }
            Item { Layout.fillWidth: true }
            WidgetKey {
                objectName: "arrangeDone"
                Layout.preferredWidth: 150; Layout.preferredHeight: parent.keyH
                text: qsTr("Done")
                baseColor: "#1f6b40"
                fontScale: 0.34
                onClicked: w.setArranging(false)
            }
        }
        Text {
            visible: w.shown.length === 0
            Layout.alignment: Qt.AlignHCenter
            text: !w.pos || !w.pos.loggedIn ? ""
                : w.popular ? qsTr("Today's best sellers show up here as orders come in.")
                : w.search ? (w.typed === "" ? qsTr("Type part of a name: \"cob\" finds Cobb.")
                                              : qsTr("Nothing on the menu has \"%1\".").arg(w.typed))
                : qsTr("Nothing on the menu here yet (Manager -> Menu).")
            color: "#8a94a6"
            font.pixelSize: 24
        }
    }

    Popup {
        id: colorPopup
        objectName: "arrangeColorPopup"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(parent ? parent.width - 32 : 600, 600)
        modal: true
        padding: 16
        contentItem: ColorPicker {
            pos: w.pos
            noneText: qsTr("No color")
            color: {
                const i = w.pos ? w.pos.menuItems.find(x => x.id === w.picked) : null
                return i ? i.buttonColor : ""
            }
            onPicked: c => {
                w.pos.setMenuItemColor(w.picked, c)
                colorPopup.close()
            }
        }
    }

    QuickItemCard {
        id: quickCard
        pos: w.pos
        controller: w.zone ? w.zone.controller : null
    }
}
