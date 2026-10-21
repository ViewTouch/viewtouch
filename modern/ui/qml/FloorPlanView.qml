import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// A floor plan as drawn: every table where it stands, turned as it is, with
// the walls, doors and bar around it; a tab for each plan (Main, Dining Area,
// Bar, Patio...); two fingers zoom in (up to 3 times) and drag it around.
// Upright in a tall space (a phone), a wide plan is turned a quarter to fill
// it, when that makes it much bigger. What a table looks like and what a touch
// does are the owner's: the host stand's states, or whose check it is.
ColumnLayout {
    id: v

    property LayoutController controller
    property real unit: 20
    property string face: "DejaVu Sans"
    property color ink: "white"
    // Text that reads on a table's color (ZoneItem.readable), and the
    // person's text size.
    property var readable: (text, fill) => text
    property real textScale: 1
    property bool tabs: true
    // The plan to start on ("": the one last picked here, else the floor's own),
    // and where that pick is kept (LayoutController::widgetState).
    property string startPlan: ""
    property string stateKey: "floorPlan"
    // Names for tests and the editor: <prefix><table>, <tabPrefix><plan id>.
    property string prefix: "plan-"
    property string tabPrefix: "planTab-"
    property string floorName: "floorPlan"
    // A table's look: tint(name), lines(name, seats) -> [line, ...],
    // picked(name) -> bool. Touches come back as tableTouched(name).
    property var tint: name => "#2d3440"
    property var lines: (name, seats) => []
    property var picked: name => false
    signal tableTouched(string name)
    // A list of big tiles instead of the plan (a phone: tables you can hit
    // with a thumb), with a Plan / List key by the tabs. The tiles' order
    // (sortKey(name), lowest first) and words (tileLines) are the owner's.
    property bool listToggle: false
    property var sortKey: name => 0
    property var tileLines: (name, seats) => v.lines(name, seats)
    property bool asList: listToggle && controller !== null && controller.widgetState(stateKey + "List") === true
    function showList(on) {
        asList = on
        if (controller) controller.setWidgetState(stateKey + "List", on)
    }

    readonly property var plans: controller ? controller.floorPlans() : []
    property string planId: startPlan !== "" ? startPlan : controller ? (controller.widgetState(stateKey) ?? "") : ""
    readonly property string planShown: plans.find(p => p.id === planId) ? planId : (plans.length ? plans[0].id : "")
    readonly property var plan: controller && planShown !== "" ? controller.floorPlan(planShown)
                                                                : ({ x: 0, y: 0, w: 0, h: 0, zones: [] })
    // A room's own page reuses the panel of the page before: its room again.
    onStartPlanChanged: if (startPlan !== "") planId = startPlan
    function choose(id) {
        planId = id
        if (controller) controller.setWidgetState(stateKey, id)
    }

    spacing: unit * 0.4

    RowLayout {
        Layout.fillWidth: true
        visible: (v.tabs && v.plans.length > 1) || v.listToggle
        spacing: v.unit * 0.3
        Repeater {
            model: v.tabs ? v.plans : []
            delegate: PosButton {
                required property var modelData
                objectName: v.tabPrefix + modelData.id
                text: qsTranslate("Page", modelData.name)
                Layout.fillWidth: true
                Layout.preferredWidth: 1   // all the same width
                Layout.preferredHeight: v.unit * 2.6
                font.family: v.face
                font.pixelSize: v.unit * 0.8
                font.bold: true
                palette.button: modelData.id === v.planShown ? "#2f6fd6" : "#2d3440"
                palette.buttonText: v.ink
                onClicked: v.choose(modelData.id)
            }
        }
        Item { Layout.fillWidth: true; visible: v.listToggle && !(v.tabs && v.plans.length > 1) }
        PosButton {
            objectName: v.floorName + "ListKey"
            visible: v.listToggle
            // Beside the room tabs: the sign only (they need the width).
            readonly property bool roomy: !(v.tabs && v.plans.length > 1)
            text: roomy ? (v.asList ? qsTr("▦ Plan") : qsTr("☰ List")) : (v.asList ? "▦" : "☰")
            Accessible.name: v.asList ? qsTr("▦ Plan") : qsTr("☰ List")
            Layout.preferredWidth: roomy ? v.unit * 5.5 : v.unit * 2.6
            Layout.preferredHeight: v.unit * 2.6
            font.family: v.face
            font.pixelSize: roomy ? v.unit * 0.8 : v.unit * 1.2
            font.bold: true
            palette.button: "#343c49"
            palette.buttonText: v.ink
            onClicked: v.showList(!v.asList)
        }
    }

    // The tables as tiles, two or three across.
    GridView {
        id: tableList
        objectName: v.floorName + "List"
        visible: v.asList
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        readonly property int perRow: width > v.unit * 40 ? 3 : 2
        cellWidth: Math.floor(width / perRow)
        cellHeight: Math.max(v.unit * 5.5, cellWidth * 0.6)
        model: v.asList ? v.plan.zones.filter(z => z.kind === "table")
                              .map(z => ({ name: z.label.trim(), seats: z.props.seats ?? 0 }))
                              .sort((a, b) => (v.sortKey(a.name) - v.sortKey(b.name))
                                              || a.name.localeCompare(b.name, undefined, { numeric: true }))
                        : []
        delegate: Item {
            id: tile
            required property var modelData
            width: tableList.cellWidth
            height: tableList.cellHeight
            Rectangle {
                id: face
                objectName: v.prefix + tile.modelData.name
                anchors.fill: parent
                anchors.margins: v.unit * 0.25
                radius: v.unit * 0.5
                readonly property color tint: v.tint(tile.modelData.name)
                color: tileTap.pressed ? Qt.lighter(tint, 1.3) : tint
                border.color: v.picked(tile.modelData.name) ? "white" : Qt.darker(tint, 1.4)
                border.width: v.picked(tile.modelData.name) ? 4 : 2
                Column {
                    anchors.centerIn: parent
                    width: parent.width - v.unit
                    spacing: v.unit * 0.1
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: (v.picked(tile.modelData.name) ? "✓ " : "") + tile.modelData.name
                        color: v.readable(v.ink, face.tint)
                        font.family: v.face
                        font.pixelSize: v.unit * 1.25
                        font.bold: true
                        fontSizeMode: Text.HorizontalFit
                        minimumPixelSize: 10
                    }
                    Repeater {
                        model: v.tileLines(tile.modelData.name, tile.modelData.seats)
                        Text {
                            required property var modelData
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: modelData
                            color: v.readable(String(modelData).startsWith("⚠") ? "#ffd27a" : v.ink, face.tint)
                            font.family: v.face
                            font.pixelSize: v.unit * 0.72
                            font.bold: String(modelData).startsWith("⚠")
                            elide: Text.ElideRight
                        }
                    }
                }
                TapHandler {
                    id: tileTap
                    onTapped: { if (TouchGuard.covered(point.scenePressPosition)) return; v.tableTouched(tile.modelData.name) }
                }
            }
        }
    }

    Flickable {
        id: floorArea
        objectName: v.floorName
        visible: !v.asList
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        // A wide plan in a tall space: turned a quarter, when it's then much bigger.
        readonly property real fitFlat: v.plan.w > 0 && v.plan.h > 0 ? Math.min(width / v.plan.w, height / v.plan.h) : 1
        readonly property real fitTurned: v.plan.w > 0 && v.plan.h > 0 ? Math.min(width / v.plan.h, height / v.plan.w) : 1
        readonly property bool turned: fitTurned > fitFlat * 1.25
        readonly property real fit: turned ? fitTurned : fitFlat
        readonly property real planW: (turned ? v.plan.h : v.plan.w) * fit * zoom
        readonly property real planH: (turned ? v.plan.w : v.plan.h) * fit * zoom
        property real zoom: 1
        property real zoomFrom: 1
        contentWidth: Math.max(width, planW)
        contentHeight: Math.max(height, planH)
        interactive: zoom > 1.01
        boundsBehavior: Flickable.StopAtBounds
        Connections {
            target: v
            function onPlanShownChanged() { floorArea.zoom = 1; floorArea.contentX = 0; floorArea.contentY = 0 }
        }
        PinchHandler {
            target: null
            onActiveChanged: if (active) floorArea.zoomFrom = floorArea.zoom
            onActiveScaleChanged: {
                const z = Math.max(1, Math.min(3, floorArea.zoomFrom * activeScale))
                // About the middle of what's shown.
                const cx = floorArea.contentX + floorArea.width / 2, cy = floorArea.contentY + floorArea.height / 2
                const k = z / floorArea.zoom
                floorArea.zoom = z
                floorArea.contentX = Math.max(0, Math.min(floorArea.contentWidth - floorArea.width, cx * k - floorArea.width / 2))
                floorArea.contentY = Math.max(0, Math.min(floorArea.contentHeight - floorArea.height, cy * k - floorArea.height / 2))
            }
        }
        // The plan at its own size, scaled (and turned) into the space.
        Item {
            id: plane
            width: v.plan.w
            height: v.plan.h
            scale: floorArea.fit * floorArea.zoom
            rotation: floorArea.turned ? 90 : 0
            transformOrigin: Item.Center
            x: floorArea.contentWidth / 2 - width / 2
            y: floorArea.contentHeight / 2 - height / 2
            Repeater {
                model: v.plan.zones
                delegate: Item {
                    id: piece
                    required property var modelData
                    readonly property bool isTable: modelData.kind === "table"
                    readonly property string name: isTable ? modelData.label.trim() : ""
                    objectName: isTable ? v.prefix + name : ""
                    x: modelData.x - v.plan.x
                    y: modelData.y - v.plan.y
                    width: modelData.w
                    height: modelData.h
                    rotation: modelData.rot
                    // How it's turned on the screen: words are kept upright against it.
                    readonly property real onScreen: modelData.rot + plane.rotation
                    // Walls, doors, the bar...
                    readonly property bool isArea: !isTable && modelData.props.decor === "area"
                    Loader {
                        anchors.fill: parent
                        active: !piece.isTable
                        sourceComponent: WidgetDecor {
                            // An area's name goes over the tables (below), not under them.
                            zone: ({ props: piece.modelData.props, label: piece.isArea ? "" : piece.modelData.label,
                                     shape: piece.modelData.shape, rotation: piece.onScreen, st: {} })
                        }
                    }
                    // A table: the owner's color; picked, a white rim and a check.
                    readonly property bool isPicked: isTable && v.picked(name)
                    readonly property color color: isTable ? v.tint(name) : "transparent"
                    readonly property real side: Math.min(width, height)
                    ZoneShape {
                        visible: piece.isTable
                        anchors.fill: parent
                        shape: piece.modelData.shape
                        st: piece.isPicked ? { fill: piece.color, frame: "border", borderColor: "white",
                                               frameWidth: Math.max(6, piece.side * 0.06) }
                                           : { fill: tableTap.pressed ? Qt.lighter(piece.color, 1.3) : piece.color,
                                               frame: "raised" }
                    }
                    Column {
                        visible: piece.isTable
                        anchors.centerIn: parent
                        readonly property real turn: ((piece.onScreen % 180) + 180) % 180
                        width: (turn > 45 && turn < 135 ? piece.height : piece.width) - piece.side * 0.12
                        rotation: -piece.onScreen
                        Text {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: (piece.isPicked ? "✓ " : "") + piece.name
                            color: v.readable(v.ink, piece.color)
                            font.family: v.face
                            font.pixelSize: Math.min(piece.side * 0.3, 48) * v.textScale
                            font.bold: true
                            fontSizeMode: Text.HorizontalFit   // a stool's "Bar 1": smaller, not cut
                            minimumPixelSize: 8
                        }
                        Repeater {
                            model: piece.isTable ? v.lines(piece.name, piece.modelData.props.seats ?? 0) : []
                            Text {
                                required property var modelData
                                required property int index
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                text: modelData
                                color: v.readable(v.ink, piece.color)
                                font.family: v.face
                                font.pixelSize: Math.min(piece.side * (index === 0 ? 0.15 : 0.13), index === 0 ? 24 : 20) * v.textScale
                                fontSizeMode: Text.HorizontalFit
                                minimumPixelSize: 8
                                elide: Text.ElideRight
                            }
                        }
                    }
                    TapHandler {
                        id: tableTap
                        enabled: piece.isTable
                        onTapped: { if (TouchGuard.covered(point.scenePressPosition)) return; v.tableTouched(piece.name) }
                    }
                }
            }
            // The rooms' names, over everything (a table may stand on the corner).
            Repeater {
                model: v.plan.zones.filter(z => z.kind === "decor" && z.props.decor === "area" && z.label)
                delegate: WidgetDecor {
                    required property var modelData
                    x: modelData.x - v.plan.x
                    y: modelData.y - v.plan.y
                    width: modelData.w
                    height: modelData.h
                    rotation: modelData.rot
                    zone: ({ props: Object.assign({}, modelData.props, { wordsOnly: true }), label: modelData.label,
                             shape: modelData.shape, rotation: modelData.rot + plane.rotation, st: {} })
                }
            }
        }
    }
}
