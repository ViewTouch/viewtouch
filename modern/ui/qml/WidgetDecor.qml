import QtQuick
import QtQuick.Shapes

// A floor plan piece (props.decor): a wall, window, door, bar counter, booth,
// plant, pillar, fence, entrance or a named area. Drawn to its size and
// turned with its zone; words on it stay upright. Touching it does nothing.
Item {
    id: w
    property ZoneItem zone
    readonly property string decor: zone && zone.props ? (zone.props.decor ?? "wall") : "wall"
    readonly property string words: zone ? zone.label : ""
    // props.color: the piece's own color; else the usual for its sort.
    function tone(usual) { return zone && zone.props && zone.props.color ? zone.props.color : usual }
    readonly property real thin: Math.max(2, Math.min(width, height) * 0.08)
    readonly property real upright: zone ? -zone.rotation : 0

    // Wall: plaster, edged.
    Rectangle {
        visible: w.decor === "wall"
        anchors.fill: parent
        color: w.tone("#d9d3c7")
        border.color: Qt.darker(color, 1.6)
        border.width: 2
    }

    // Window: glass in the wall, a frame line down its length.
    Rectangle {
        visible: w.decor === "window"
        anchors.fill: parent
        color: w.tone("#9fd3f0")
        opacity: 0.85
        border.color: "#d9d3c7"
        border.width: 3
        Rectangle {
            anchors.centerIn: parent
            width: parent.width >= parent.height ? parent.width - 6 : 2
            height: parent.width >= parent.height ? 2 : parent.height - 6
            color: "#d9d3c7"
        }
    }

    // Door: the leaf open along its left edge, and the arc it swings through.
    Shape {
        id: door
        visible: w.decor === "door"
        anchors.fill: parent
        readonly property real r: Math.min(w.width, w.height)
        ShapePath {
            strokeColor: w.tone("#d9d3c7")
            strokeWidth: 4
            fillColor: "transparent"
            startX: 0; startY: w.height
            PathLine { x: 0; y: w.height - door.r }
        }
        ShapePath {
            strokeColor: w.tone("#d9d3c7")
            strokeWidth: 2
            strokeStyle: ShapePath.DashLine
            dashPattern: [4, 3]
            fillColor: Qt.rgba(1, 1, 1, 0.06)
            startX: 0; startY: w.height - door.r
            PathArc { x: door.r; y: w.height; radiusX: door.r; radiusY: door.r }
            PathLine { x: 0; y: w.height }
        }
    }

    // Bar counter: wood, a lighter top edge where drinks go.
    Rectangle {
        visible: w.decor === "counter"
        anchors.fill: parent
        radius: Math.min(width, height) * 0.25
        color: w.tone("#6b4a2b")
        border.color: Qt.darker(color, 1.8)
        border.width: 3
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: parent.height * 0.12 }
            height: parent.height * 0.22
            radius: height / 2
            color: Qt.lighter(parent.color, 1.35)
        }
    }

    // Booth bench: a seat with its back along the top.
    Rectangle {
        visible: w.decor === "booth"
        anchors.fill: parent
        radius: Math.min(width, height) * 0.18
        color: w.tone("#7a2e2e")
        border.color: Qt.darker(color, 1.8)
        border.width: 2
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 4 }
            height: parent.height * 0.35
            radius: parent.radius
            color: Qt.darker(parent.color, 1.3)
        }
    }

    // Plant: leaves around a darker middle.
    Item {
        visible: w.decor === "plant"
        anchors.fill: parent
        readonly property real d: Math.min(width, height)
        Repeater {
            model: 7
            Rectangle {
                required property int index
                readonly property real a: index * 2 * Math.PI / 7
                width: parent.d * 0.42
                height: width
                radius: width / 2
                x: parent.width / 2 + Math.cos(a) * parent.d * 0.27 - width / 2
                y: parent.height / 2 + Math.sin(a) * parent.d * 0.27 - height / 2
                color: w.tone("#3f8f4a")
                border.color: Qt.darker(color, 1.5)
            }
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.d * 0.4
            height: width
            radius: width / 2
            color: Qt.darker(w.tone("#3f8f4a"), 1.4)
        }
    }

    // Pillar: stone, square or round (its shape).
    Rectangle {
        visible: w.decor === "pillar"
        anchors.fill: parent
        radius: w.zone && w.zone.shape === "circle" ? Math.min(width, height) / 2 : 4
        color: w.tone("#9a948a")
        border.color: Qt.darker(color, 1.7)
        border.width: 3
    }

    // Fence or rail: a rail with posts along it.
    Item {
        visible: w.decor === "fence"
        anchors.fill: parent
        readonly property bool across: width >= height
        Rectangle {
            anchors.centerIn: parent
            width: parent.across ? parent.width : parent.width * 0.4
            height: parent.across ? parent.height * 0.4 : parent.height
            color: w.tone("#8b6b47")
        }
        Repeater {
            model: Math.max(2, Math.floor((parent.across ? parent.width : parent.height) / 48) + 1)
            Rectangle {
                required property int index
                readonly property int n: Math.max(2, Math.floor((parent.across ? parent.width : parent.height) / 48) + 1)
                readonly property real s: Math.min(parent.width, parent.height)
                width: s
                height: s
                x: parent.across ? index * (parent.width - s) / (n - 1) : 0
                y: parent.across ? 0 : index * (parent.height - s) / (n - 1)
                color: Qt.darker(w.tone("#8b6b47"), 1.3)
            }
        }
    }

    // Entrance: an arrow pointing in (turn it to point the way), its words upright.
    Item {
        visible: w.decor === "entrance"
        anchors.fill: parent
        Shape {
            id: arrow
            anchors.fill: parent
            readonly property real a: Math.min(w.width, w.height) * 0.3
            ShapePath {
                strokeColor: "transparent"
                fillColor: w.tone("#f5b940")
                startX: w.width / 2 - arrow.a * 0.45; startY: 0
                PathLine { x: w.width / 2 + arrow.a * 0.45; y: 0 }
                PathLine { x: w.width / 2 + arrow.a * 0.45; y: w.height - arrow.a * 1.2 }
                PathLine { x: w.width / 2 + arrow.a; y: w.height - arrow.a * 1.2 }
                PathLine { x: w.width / 2; y: w.height }
                PathLine { x: w.width / 2 - arrow.a; y: w.height - arrow.a * 1.2 }
                PathLine { x: w.width / 2 - arrow.a * 0.45; y: w.height - arrow.a * 1.2 }
                PathLine { x: w.width / 2 - arrow.a * 0.45; y: 0 }
            }
        }
    }

    // Area: a room's floor, outlined, its name in the corner.
    Shape {
        visible: w.decor === "area"
        anchors.fill: parent
        ShapePath {
            strokeColor: Qt.rgba(1, 1, 1, 0.45)
            strokeWidth: 3
            strokeStyle: ShapePath.DashLine
            dashPattern: [6, 4]
            fillColor: w.zone && w.zone.props && w.zone.props.color ? Qt.alpha(w.zone.props.color, 0.35) : Qt.rgba(1, 1, 1, 0.05)
            startX: 2; startY: 2
            PathLine { x: w.width - 2; y: 2 }
            PathLine { x: w.width - 2; y: w.height - 2 }
            PathLine { x: 2; y: w.height - 2 }
            PathLine { x: 2; y: 2 }
        }
    }

    // Its words (an area's name, Entrance...), upright however it's turned.
    Text {
        objectName: "decorWords"
        visible: w.words !== ""
        anchors.centerIn: w.decor === "area" ? undefined : parent
        x: w.decor === "area" ? 16 : 0
        y: w.decor === "area" ? 10 : 0
        rotation: w.decor === "area" ? 0 : w.upright
        text: qsTranslate("Page", w.words)
        color: "white"
        opacity: w.decor === "area" ? 0.7 : 1
        font.family: w.zone ? (w.zone.st.font ?? "DejaVu Sans") : "DejaVu Sans"
        font.pixelSize: w.decor === "area" ? Math.min(40, Math.max(16, w.height * 0.12))
                                           : Math.max(12, Math.min(28, Math.min(w.width, w.height) * 0.3))
        font.bold: true
        style: Text.Raised
        styleColor: "#80000000"
    }
}
