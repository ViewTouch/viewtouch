import QtQuick
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

    ShaderEffectSource {
        id: textureSource
        visible: false
        width: root.width
        height: root.height
        sourceItem: root.textured && !root.softwareRenderer ? textureImage : null
        hideSource: true
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        // Drop shadow
        ShapePath {
            strokeWidth: -1
            fillColor: root.shadow > 0 ? Qt.rgba(0, 0, 0, 0.35) : "transparent"
            PathPolyline { path: root.outline(root.shadow, root.shadow, root.width, root.height, root.radius) }
        }

        // Frame: bevel gradient, solid border, or nothing
        ShapePath {
            strokeWidth: -1
            fillColor: root.frame === "border" ? (root.style.borderColor ?? root.dark) : "transparent"
            fillGradient: root.bevel ? bevelGradient : null
            PathPolyline { path: root.frameWidth > 0 ? root.outline(0, 0, root.width, root.height, root.radius) : [] }
        }

        // Face
        ShapePath {
            strokeWidth: -1
            fillColor: root.fill
            fillItem: root.textured && !root.softwareRenderer ? textureSource : null
            PathPolyline {
                path: root.outline(root.frameWidth, root.frameWidth,
                                   root.width - 2 * root.frameWidth, root.height - 2 * root.frameWidth,
                                   root.radius - root.frameWidth)
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
