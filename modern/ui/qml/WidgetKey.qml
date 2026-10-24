import QtQuick

// A key on a widget keypad. Its look comes from the zone it is in: the
// "Built-in button look" (style keyFill, keyTextColor, keyLitFill, keyFont,
// keyRadius), inherited from the page and the theme like any style.
Rectangle {
    id: key

    // The style of the zone this key is in (the nearest ZoneItem up the tree).
    readonly property var keySt: {
        for (let p = key.parent; p; p = p.parent)
            if (p.styleNormal !== undefined && p.st !== undefined)
                return p.st
        return ({})
    }
    property string text
    property color baseColor: keySt.keyFill ?? Ui.key
    property color textColor: keySt.keyTextColor ?? "white"
    property string fontFamily: Ui.font
    property real fontScale: 0.42
    // A size for its text shared with the keys beside it (> 0): one size for
    // the row, not each shrunk on its own (a short name looked bigger).
    property real pixelSize: 0
    property bool accent: false
    // Held down (half a second): held() instead of clicked(), for keys that say so.
    property bool holdable: false
    // Takes the whole touch (on a sheet over other keys): nothing behind it
    // sees the finger, not even the click a touch screen makes of it after.
    property bool takesTouch: false

    signal clicked()
    signal held()

    radius: keySt.keyRadius !== undefined ? keySt.keyRadius : Math.min(width, height) * 0.14
    color: tap.pressed || accent ? (keySt.keyLitFill ?? (tap.pressed ? "#4c8dff" : Ui.go)) : baseColor
    // The style's own edge for keys (Modern: a thin gray one), else a darker shade.
    border.color: !(tap.pressed || accent) && keySt.keyBorderColor ? keySt.keyBorderColor : Qt.darker(color, 1.5)
    border.width: 2

    Text {
        anchors.centerIn: parent
        width: parent.width - 8
        height: parent.height   // a name shrunk to fit its width stays in the middle
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: key.text
        color: key.textColor
        font.family: key.keySt.keyFont ?? key.fontFamily
        // The zone's "Button text size %" (Built-in button look) on top.
        font.pixelSize: (key.pixelSize > 0 ? key.pixelSize : Math.max(10, Math.min(key.height, key.width * 1.2) * key.fontScale))
                        * (key.keySt.keyTextSize ?? 100) / 100
        font.bold: true
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: 10
    }

    TapHandler {
        id: tap
        longPressThreshold: key.holdable ? 0.5 : 0.8
        gesturePolicy: key.takesTouch ? TapHandler.ReleaseWithinBounds : TapHandler.DragThreshold
        onTapped: { if (TouchGuard.covered(point.scenePressPosition)) return; key.clicked() }
        onLongPressed: { if (TouchGuard.covered(point.scenePressPosition)) return; if (key.holdable) key.held() }
    }
}
