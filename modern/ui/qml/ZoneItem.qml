import QtQuick
import QtQuick.Shapes

// One zone on a page. Roles come from ZoneModel; styles are pre-resolved.
Item {
    id: zone

    required property string zoneId
    required property string kind
    required property string label
    required property int zoneX
    required property int zoneY
    required property int zoneW
    required property int zoneH
    required property string shape
    required property string behavior
    required property bool zoneEnabled
    required property bool inherited
    required property bool current
    required property string imagePath
    required property var styleNormal
    required property var styleSelected
    required property var styleDisabled
    required property var props
    required property bool soldOut   // its item is 86'd: shown, not orderable
    required property var showWhen   // show/hide rules (LayoutController::ruleShows does the same)
    required property string itemId  // the item it orders (empty: it doesn't)
    required property int zoneRotation   // turned about its middle, degrees clockwise
    // Running low: how many can still be made (-1: plenty, or not an item).
    readonly property int stockLeft: {
        if (!itemId || !pos || editing)
            return -1
        const left = pos.stockLeft
        return left[itemId] ?? left[itemId.toLowerCase()] ?? -1
    }

    // POS session and controller, for widgets.
    // Screen pixels per canvas unit (the page is scaled to fit the screen).
    property real screenScale: 1
    // Form widgets lay out desktop-sized controls (25 px) and scale them by
    // this: big enough for a finger (46 screen px), unless the content would
    // then be narrower than `minWidth` (or shorter than `minHeight`) canvas units.
    // Taller than wide (a phone held upright): widgets stack their parts.
    readonly property bool narrow: width < height * 0.9
    function formZoom(minWidth, minHeight) {
        const touch = 46 / (25 * Math.max(0.1, screenScale))
        return Math.max(1, Math.min(touch, width / minWidth, minHeight ? height / minHeight : touch))
    }
    // Canvas units for `px` screen pixels: keys at least a finger tall.
    function touch(px) { return px / Math.max(0.1, screenScale) }
    property LayoutController controller
    property PosService pos
    // The logged-in person's text size (Employees: Text size); never while editing.
    readonly property real textScale: pos && !editing && pos.userPrefs.textSize ? pos.userPrefs.textSize / 100 : 1

    // A widget's own buttons, hidden or renamed in the editor
    // (props.hideButtons, props.buttons.<id>.hide / .label).
    function keyShown(id) {
        if (!props) return true
        if (props.hideButtons === true) return false
        const b = props.buttons ? props.buttons[id] : undefined
        return !(b && b.hide === true)
    }
    // Where built-in button `id` goes among `ids` (their usual order):
    // props.buttons.<id>.order (1 = first) moves it ahead of any button in
    // that place; the others keep their usual order.
    function keyOrder(id, ids) {
        const set = x => {
            const b = props && props.buttons ? props.buttons[x] : undefined
            return b && b.order ? b.order : 0
        }
        const at = x => set(x) || ids.indexOf(x) + 1
        const sorted = ids.slice().sort((a, b) => at(a) - at(b) || (set(b) ? 1 : 0) - (set(a) ? 1 : 0)
                                                  || ids.indexOf(a) - ids.indexOf(b))
        return sorted.indexOf(id)
    }
    // Text that reads on its fill: the style's text color, unless it's too close
    // to the fill (a theme's dark text on a green button): then dark or white.
    function readable(text, fill) {
        const t = Qt.color(text), f = Qt.color(fill || "transparent")
        if (f.a < 0.5)
            return t
        const lum = c => {
            const ch = v => v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4)
            return 0.2126 * ch(c.r) + 0.7152 * ch(c.g) + 0.0722 * ch(c.b)
        }
        const a = lum(t), b = lum(f)
        const ratio = (Math.max(a, b) + 0.05) / (Math.min(a, b) + 0.05)
        return ratio >= 4.5 ? t : (b > 0.35 ? Qt.color("#1b1b1b") : Qt.color("white"))   // WCAG AA
    }
    readonly property color ink: readable(st.textColor ?? "white", st.fill)

    // A status color from the theme (Theme -> Status colors), else the usual one.
    // Colors for color blindness (the person's or the screen's): Okabe and Ito's,
    // told apart by how light they are as well as their hue: blue for mine and
    // new, yellow for soon, vermilion for late (with LATE on it).
    readonly property bool colorSafe: !!(pos && !editing && pos.userPrefs.colorSafe)
    readonly property var safeColors: ({
        tableMine: "#0072b2", tableOpen: "#e69f00", tableCurrent: "#56b4e9", tableLong: "#f0e442",
        tableReserved: "#cc79a7", tableDirty: "#6b5d52", tableFree: "#0072b2",
        kitchenNew: "#0072b2", kitchenWarn: "#f0e442", kitchenLate: "#d55e00", kitchenReady: "#009e73",
        soldOut: "#d55e00", low: "#f0e442"
    })
    function statusColor(name, usual) {
        if (colorSafe && safeColors[name])
            return safeColors[name]
        const c = controller ? controller.statusColors : null
        return c && c[name] ? c[name] : usual
    }
    function keyText(id, usual) {
        const b = props && props.buttons ? props.buttons[id] : undefined
        return b && b.label ? qsTranslate("Page", b.label) : usual
    }

    // Set by PageView for behavior "select" (one lit zone per page).
    property string selectedZoneId: ""

    // Edit mode: live drag/resize feedback from EditLayer.
    property bool editing: false
    property bool editSelected: false
    property real dragDX: 0
    property real dragDY: 0
    property var previewRect: null
    property var previewRotation: null   // turning it: the angle so far

    signal activated()
    signal explainRequested()   // held: what it does, instead of doing it
    signal selectRequested()

    readonly property bool isWidget: !["button", "label", "image", "comment"].includes(kind)

    // Live text in a label: {check.total}, {user.name}, {time}... filled in
    // and kept up to date. Unknown names stay as typed.
    property date now: new Date()
    Timer {
        interval: 15000
        repeat: true
        running: zone.label.indexOf("{time}") >= 0 || zone.label.indexOf("{date}") >= 0
        onTriggered: zone.now = new Date()
    }
    function fill(text) {
        if (text.indexOf("{") < 0)
            return text
        const p = pos
        const c = p ? p.check : ({})
        const t = p && p.hasCheck ? p.totals : ({})
        const values = {
            "store.name": p ? p.storeName : "",
            "user.name": p ? p.userName : "",
            "user.role": p ? p.userRole : "",
            "terminal": p ? p.terminalName : "",
            "check.label": c.label ?? "",
            "check.number": c.id ?? "",
            "check.server": c.server ?? "",
            "check.guests": c.guests ?? "",
            "check.customer": c.customer ? c.customer.name : "",
            "check.due": c.due ?? "",
            "check.items": p && p.hasCheck ? p.lines.length : "",
            "check.subtotal": t.subtotal ?? "",
            "check.tax": t.tax ?? "",
            "check.total": t.total ?? "",
            "check.balance": t.balance ?? "",
            "entry": p ? p.entryAmount : "",
            "typed": p ? p.textEntry : "",
            "time": Qt.locale().toString(zone.now, "h:mm AP"),
            "date": Qt.locale().toString(zone.now, "ddd MMM d"),
            "mealPeriod": controller ? controller.mealPeriod : "",
        }
        // New problems for the managers (Store Check): "⚠ 2", or nothing.
        if (text.indexOf("{problems}") >= 0) {
            const n = p && p.storeCheck ? (p.storeCheck.unseen ?? 0) : 0
            values["problems"] = n > 0 ? "⚠ " + n : ""
        }
        return text.replace(/\{([a-zA-Z.]+)\}/g, (m, k) => k in values ? String(values[k]) : m)
    }

    // The screen it is on: the one being edited (Standard | Phone) in the editor.
    readonly property string shownScreen: !controller ? ""
        : controller.editing && controller.editor
          ? (controller.editor.revision >= 0 && controller.editor.screens.isPhone ? "phone" : "standard")
          : controller.formFactor
    // Its "Show only when" rules hold now.
    readonly property bool ruleShows: {
        const r = showWhen
        if (!r || Object.keys(r).length === 0) return true
        const p = pos
        const loggedIn = !!p && p.loggedIn
        if (r.login === "loggedIn" && !loggedIn) return false
        if (r.login === "loggedOut" && loggedIn) return false
        if (r.login === "manager" && !(p && p.permissions.includes("manager"))) return false
        const open = !!p && p.hasCheck
        if (r.check === "open" && !open) return false
        if (r.check === "none" && open) return false
        if (r.checkType && (!open || p.check.type !== r.checkType)) return false
        if (r.mealPeriod && (!controller || controller.mealPeriod !== r.mealPeriod)) return false
        if (r.screen && (!controller || shownScreen !== r.screen)) return false
        if (r.service && !r.service.split(",").includes((p && p.service && p.service.mode) || "restaurant")) return false
        return true
    }
    // Widgets with a working implementation (Widget<Kind>.qml); the rest
    // show a placeholder until their milestone.
    readonly property var builtWidgets: ["table", "tableGrid", "staffPicker", "checkHistory", "modifierPicker",
        "soldOutList", "orderList", "loginPad", "guestCount", "numPad",
        "paymentPanel", "checkList", "keyboard", "clock", "logoutPanel", "statusBar",
        "adminPanel", "reportView", "drawerPanel", "endOfDay", "splitCheck", "kitchenDisplay", "customerInfo",
        "customerLookup", "giftCard", "waitlist", "schedule", "factoryReset", "messageComposer", "network", "receiveDelivery", "stockCount", "wasteLog", "storeCheck", "prepLabels", "checkSearch", "orderLater", "menuGrid", "menuCategories", "menuBuilder", "timeClock", "dashboard", "checklist", "hostStand", "deliveryBoard", "decor", "floorPlan", "tableKey"]
    readonly property bool hasWidget: isWidget && builtWidgets.includes(kind)
    readonly property bool interactive: zoneEnabled && behavior !== "passthrough"
                                        && (kind === "button" || kind === "image")

    property bool toggled: false
    property bool armed: false      // "double": first touch
    property bool flashing: false   // "blink": brief highlight after touch

    readonly property bool lit: current || toggled || armed || flashing
                                || (behavior === "select" && selectedZoneId === zoneId)
                                || (tap.pressed && behavior !== "none" && !editing)
    readonly property var st: !zoneEnabled ? styleDisabled : lit ? styleSelected : styleNormal

    x: previewRect ? previewRect.x : zoneX + (editSelected ? dragDX : 0)
    y: previewRect ? previewRect.y : zoneY + (editSelected ? dragDY : 0)
    width: previewRect ? previewRect.w : zoneW
    height: previewRect ? previewRect.h : zoneH
    rotation: previewRotation !== null ? previewRotation : zoneRotation
    visible: (kind !== "comment" || editing) && (ruleShows || editing) && (!emptyLogo || editing)   // notes: editor only
    // Template zones are dimmed while editing: they belong to another page;
    // so are zones whose rules hide them right now.
    opacity: (st.opacity ?? 1) * (editing && (inherited || !ruleShows) ? 0.45 : 1)

    ZoneShape {
        visible: zone.kind !== "decor"   // floor plan pieces draw themselves
        anchors.fill: parent
        shape: zone.shape
        st: zone.st
    }

    // Its picture: the store's ("store:logo.png"), the store logo ("logo:"),
    // or a resource; each screen shows its own copy.
    readonly property string pictureUrl: !imagePath ? "" : pos ? (pos.imageRevision < 0 ? undefined : pos.imageUrl(imagePath)) : imagePath
    // A logo zone with no logo set: nothing to show (the editor still shows it).
    readonly property bool emptyLogo: imagePath === "logo:" && pictureUrl === "" && kind === "image"

    Image {
        id: picture
        visible: zone.pictureUrl !== ""
        source: zone.pictureUrl
        anchors.fill: parent
        anchors.margins: (zone.st.frameWidth ?? 3) + 6
        anchors.bottomMargin: caption.visible ? parent.height * 0.3 : anchors.margins
        fillMode: Image.PreserveAspectFit
        asynchronous: true
    }

    Text {
        id: caption
        visible: text !== "" && !zone.hasWidget
        // Page text is translated too (i18n/<lang>.json, or the store's own phrases).
        text: zone.isWidget ? zone.kind + (zone.label ? "\n" + zone.label : "")
            : zone.editing ? qsTranslate("Page", zone.label)   // the editor shows {check.total} as typed
            : zone.fill(qsTranslate("Page", zone.label))
        anchors.fill: parent
        anchors.margins: (zone.st.frameWidth ?? 3) + 8
        anchors.topMargin: picture.visible ? parent.height * 0.7 : anchors.margins
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
        fontSizeMode: Text.Fit
        minimumPixelSize: 10
        color: zone.ink
        font.family: zone.st.font ?? Ui.font
        font.pixelSize: (zone.st.fontSize ?? 28) * zone.textScale
        font.bold: zone.st.bold ?? true
        style: zone.st.textStyle === "embossed" ? Text.Raised
             : zone.st.textStyle === "outline" ? Text.Outline : Text.Normal
        styleColor: Qt.darker(color, 3)
    }

    // Running low: "5 left", so the server can warn the guest.
    Rectangle {
        objectName: "stockLeft"
        visible: zone.stockLeft > 0 && !zone.soldOut
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 6
        width: leftText.implicitWidth + 14
        height: leftText.implicitHeight + 6
        radius: height / 2
        color: "#e0161a20"
        border.color: zone.statusColor("low", "#f5b940")
        border.width: 2
        Text {
            id: leftText
            anchors.centerIn: parent
            text: qsTr("%1 left").arg(zone.stockLeft)
            color: zone.statusColor("low", "#f5b940")
            font.family: zone.st.font ?? Ui.font
            font.pixelSize: Math.max(11, Math.min(zone.height * 0.16, 22))
            font.bold: true
        }
    }

    // 86'd: the item can't be ordered right now.
    Rectangle {
        visible: zone.soldOut
        anchors.fill: parent
        radius: zone.st.radius ?? 14
        color: "#a0000000"
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.9, soldOutText.implicitWidth + 24)
            height: soldOutText.implicitHeight + 10
            radius: 6
            rotation: -8
            color: zone.statusColor("soldOut", "#b83232")
            Text {
                id: soldOutText
                anchors.centerIn: parent
                text: qsTr("SOLD OUT")
                color: "white"
                font.family: zone.st.font ?? Ui.font
                font.pixelSize: Math.max(12, Math.min(zone.height * 0.22, 34))
                font.bold: true
            }
        }
    }

    // The window's own palette, for what the zone's style leaves unset.
    Item { id: plain; visible: false }
    Loader {
        id: widget
        anchors.fill: parent
        active: zone.hasWidget
        // Its form buttons take the built-in button look (style keyFill...).
        palette.button: zone.st.keyFill ?? plain.palette.button
        palette.buttonText: zone.st.keyTextColor ?? plain.palette.buttonText
        palette.highlight: zone.st.keyLitFill ?? plain.palette.highlight
        enabled: !zone.editing   // arranging a page must not ring up sales
        onActiveChanged: load()
        Component.onCompleted: load()
        function load() {
            if (active)
                setSource("Widget" + zone.kind[0].toUpperCase() + zone.kind.slice(1) + ".qml", { zone: zone })
        }
        Connections {
            target: zone
            function onKindChanged() { widget.load() }
        }
    }

    // Placeholder for widgets not built yet.
    Shape {
        visible: zone.isWidget && !zone.hasWidget
        anchors.fill: parent
        ShapePath {
            strokeColor: "#80ffffff"
            strokeWidth: 2
            strokeStyle: ShapePath.DashLine
            dashPattern: [6, 4]
            fillColor: "transparent"
            startX: 4; startY: 4
            PathLine { x: zone.width - 4; y: 4 }
            PathLine { x: zone.width - 4; y: zone.height - 4 }
            PathLine { x: 4; y: zone.height - 4 }
            PathLine { x: 4; y: 4 }
        }
    }

    Text {
        visible: zone.armed
        text: qsTr("tap again")
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 6 }
        color: caption.color
        font.pixelSize: 16
    }

    TapHandler {
        id: tap
        enabled: zone.interactive && !zone.editing
        longPressThreshold: 0.7
        onLongPressed: { if (TouchGuard.covered(point.scenePressPosition)) return; zone.explainRequested() }
        onTapped: {
            if (TouchGuard.covered(point.scenePressPosition)) return   // the keyboard's touch
            switch (zone.behavior) {
            case "toggle":
                zone.toggled = !zone.toggled
                break
            case "select":
                zone.selectRequested()
                break
            case "double":
                if (!zone.armed) {
                    zone.armed = true
                    armTimer.restart()
                    return
                }
                zone.armed = false
                break
            case "blink":
                zone.flashing = true
                flashTimer.restart()
                break
            }
            zone.activated()
        }
    }

    Timer { id: flashTimer; interval: 180; onTriggered: zone.flashing = false }
    Timer { id: armTimer; interval: 1500; onTriggered: zone.armed = false }
}
