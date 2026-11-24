import QtQuick
import QtQuick.Effects
import QtQuick.Shapes

// Draws a zone's body for any shape: drop shadow, bevel frame, and face
// (solid color and/or tiled texture). `st` is a fully resolved state style.
Item {
    id: root

    property string shape: "rect"
    property var st: ({})
    // (Undefined for a moment while a page's zones are taken down.)
    readonly property var style: st ?? ({})

    readonly property color fill: style.fill ?? "#808080"
    readonly property string texture: style.texture ?? ""
    readonly property string frame: style.frame ?? "raised"
    readonly property bool bevel: frame === "raised" || frame === "inset"
    readonly property real frameWidth: (frame === "none" || frame === "flat") ? 0 : (style.frameWidth ?? 3)
    readonly property real shadow: style.shadow ?? 0
    readonly property real radius: shape === "rounded" ? (style.radius || Math.min(width, height) * 0.2)
                                                        : (style.radius ?? 0)
    // A rectangle or rounded one: Qt's own path (no outline worked out point
    // by point: 114,000 points a tour of a store's screens, before).
    readonly property bool boxy: shape !== "circle" && shape !== "diamond" && shape !== "hexagon" && shape !== "octagon"
    readonly property bool round: shape === "circle"   // (an ellipse: Qt's own arc)
    readonly property bool polygon: !boxy && !round
    function corner(r, w, h) { return Math.max(0, Math.min(r, w / 2, h / 2)) }
    readonly property color light: Qt.lighter(fill, 1.6)
    readonly property color dark: Qt.darker(fill, 2.0)

    // Closed polygon for the shape inside the box (x, y, w, h).
    function outline(x, y, w, h, r) {
        const pts = []
        const P = (px, py) => pts.push(Qt.point(px, py))
        switch (shape) {
        case "circle": {
            const n = 64, cx = x + w / 2, cy = y + h / 2
            for (let i = 0; i <= n; ++i) {
                const a = 2 * Math.PI * i / n
                P(cx + Math.cos(a) * w / 2, cy + Math.sin(a) * h / 2)
            }
            return pts
        }
        case "diamond":
            P(x + w / 2, y); P(x + w, y + h / 2); P(x + w / 2, y + h); P(x, y + h / 2); P(x + w / 2, y)
            return pts
        case "hexagon": {
            const c = Math.min(w / 4, h / 2)
            P(x + c, y); P(x + w - c, y); P(x + w, y + h / 2); P(x + w - c, y + h)
            P(x + c, y + h); P(x, y + h / 2); P(x + c, y)
            return pts
        }
        case "octagon": {
            const c = Math.min(w, h) * 0.29
            P(x + c, y); P(x + w - c, y); P(x + w, y + c); P(x + w, y + h - c)
            P(x + w - c, y + h); P(x + c, y + h); P(x, y + h - c); P(x, y + c); P(x + c, y)
            return pts
        }
        default: {
            r = Math.max(0, Math.min(r, w / 2, h / 2))
            if (r < 0.5) {
                P(x, y); P(x + w, y); P(x + w, y + h); P(x, y + h); P(x, y)
                return pts
            }
            const seg = 8
            const corner = (cx, cy, start) => {
                for (let i = 0; i <= seg; ++i) {
                    const a = start + (Math.PI / 2) * i / seg
                    P(cx + Math.cos(a) * r, cy + Math.sin(a) * r)
                }
            }
            corner(x + w - r, y + r, -Math.PI / 2)
            corner(x + w - r, y + h - r, 0)
            corner(x + r, y + h - r, Math.PI / 2)
            corner(x + r, y + r, Math.PI)
            pts.push(pts[0])
            return pts
        }
        }
    }

    // Textures: on GPU backends the tiled image is rendered into a texture
    // the size of the zone and used as the face fill, so every shape gets it.
    // The software renderer cannot texture-fill shapes; there the tiled image
    // is drawn directly over rectangular faces and other shapes stay flat.
    readonly property bool softwareRenderer: GraphicsInfo.api === GraphicsInfo.Software
    readonly property bool textured: texture !== "" && textureImage.status === Image.Ready

    Image {
        id: textureImage
        x: root.frameWidth
        y: root.frameWidth
        width: root.width - 2 * root.frameWidth
        height: root.height - 2 * root.frameWidth
        z: 1
        visible: root.softwareRenderer && root.textured && (root.shape === "rect" || root.shape === "rounded")
        fillMode: Image.Tile
        source: root.texture ? "qrc:/textures/" + root.texture + ".xpm" : ""
    }

    Loader {
        id: textureSource
        active: root.textured && !root.softwareRenderer
        visible: false
        sourceComponent: ShaderEffectSource {
            visible: false
            width: root.width
            height: root.height
            sourceItem: textureImage
            hideSource: true
        }
    }

    // A rectangle's or rounded rectangle's shadow, soft: Qt 6.9's
    // RectangularShadow, one quick pass on the GPU. Other shapes, and
    // computers drawing without a GPU, keep the hard one below.
    readonly property bool softShadow: shadow > 0 && (shape === "rect" || shape === "rounded") && !softwareRenderer
    Loader {
        active: root.softShadow
        anchors.fill: parent
        sourceComponent: RectangularShadow {
            id: soft
            radius: root.radius
            offset: Qt.vector2d(root.shadow * 0.5, root.shadow)
            blur: root.shadow * 2.5
            color: Qt.rgba(0, 0, 0, 0.45)
            ShadowOwner { shadow: soft }   // (Qt's own leaks its effect: see shadowowner.hh)
        }
    }

    // A plain or rounded rectangle, flat or with a border, no texture: two
    // Rectangles (the Modern look's every key and table). A floor plan's
    // sixty zones each made a three-path Shape before: a quarter second on a
    // CM5 every time a server came back to the tables.
    readonly property bool plain: (shape === "rect" || shape === "rounded") && !bevel && texture === ""
    Rectangle {
        visible: root.plain && root.shadow > 0 && !root.softShadow
        x: root.shadow
        y: root.shadow
        width: root.width
        height: root.height
        radius: Math.max(0, Math.min(root.radius, width / 2, height / 2))
        color: Qt.rgba(0, 0, 0, 0.35)
    }
    Rectangle {
        visible: root.plain
        anchors.fill: parent
        radius: Math.max(0, Math.min(root.radius, width / 2, height / 2))
        color: root.fill
        border.width: root.frame === "border" ? root.frameWidth : 0
        border.color: root.style.borderColor ?? root.dark
    }

    Loader {
        active: !root.plain
        anchors.fill: parent
        sourceComponent: Shape {
        preferredRendererType: Shape.CurveRenderer

        // Drop shadow (hard)
        ShapePath {
            strokeWidth: -1
            fillColor: root.shadow > 0 && !root.softShadow ? Qt.rgba(0, 0, 0, 0.35) : "transparent"
            PathRectangle {
                x: root.shadow; y: root.shadow
                width: root.boxy ? root.width : 0; height: root.boxy ? root.height : 0
                radius: root.corner(root.radius, root.width, root.height)
            }
            PathAngleArc {
                moveToStart: true
                centerX: root.shadow + root.width / 2; centerY: root.shadow + root.height / 2
                radiusX: root.round ? root.width / 2 : 0; radiusY: root.round ? root.height / 2 : 0
                startAngle: 0; sweepAngle: 360
            }
            PathPolyline { path: !root.polygon || root.shadow <= 0 ? [] : root.outline(root.shadow, root.shadow, root.width, root.height, root.radius) }
        }

        // Frame: bevel gradient, solid border, or nothing
        ShapePath {
            strokeWidth: -1
            fillColor: root.frame === "border" ? (root.style.borderColor ?? root.dark) : "transparent"
            fillGradient: root.bevel ? bevelGradient : null
            PathRectangle {
                width: root.boxy && root.frameWidth > 0 ? root.width : 0; height: root.boxy && root.frameWidth > 0 ? root.height : 0
                radius: root.corner(root.radius, root.width, root.height)
            }
            PathAngleArc {
                moveToStart: true
                centerX: root.width / 2; centerY: root.height / 2
                radiusX: root.round && root.frameWidth > 0 ? root.width / 2 : 0
                radiusY: root.round && root.frameWidth > 0 ? root.height / 2 : 0
                startAngle: 0; sweepAngle: 360
            }
            PathPolyline { path: root.polygon && root.frameWidth > 0 ? root.outline(0, 0, root.width, root.height, root.radius) : [] }
        }

        // Face
        ShapePath {
            strokeWidth: -1
            fillColor: root.fill
            fillItem: root.textured && !root.softwareRenderer ? textureSource.item : null
            PathRectangle {
                x: root.frameWidth; y: root.frameWidth
                width: root.boxy ? root.width - 2 * root.frameWidth : 0
                height: root.boxy ? root.height - 2 * root.frameWidth : 0
                radius: root.corner(root.radius - root.frameWidth, root.width - 2 * root.frameWidth, root.height - 2 * root.frameWidth)
            }
            PathAngleArc {
                moveToStart: true
                centerX: root.width / 2; centerY: root.height / 2
                radiusX: root.round ? root.width / 2 - root.frameWidth : 0
                radiusY: root.round ? root.height / 2 - root.frameWidth : 0
                startAngle: 0; sweepAngle: 360
            }
            PathPolyline {
                path: !root.polygon ? [] : root.outline(root.frameWidth, root.frameWidth,
                                                    root.width - 2 * root.frameWidth, root.height - 2 * root.frameWidth,
                                                    root.radius - root.frameWidth)
            }
        }
    }
    }

    LinearGradient {
        id: bevelGradient
        x1: 0; y1: 0
        x2: root.width; y2: root.height
        GradientStop { position: 0.0; color: root.frame === "inset" ? root.dark : root.light }
        GradientStop { position: 1.0; color: root.frame === "inset" ? root.light : root.dark }
    }
}
