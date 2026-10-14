import QtQuick
import QtQuick.Controls.Fusion

// Edit-mode overlay, placed inside the scaled page surface so every
// coordinate here is in logical canvas units. Handles selection, rubber-band,
// move, resize, grid snapping and alignment guides. Nothing is committed
// until the mouse is released (one undo step per gesture); while dragging,
// ZoneItems follow dragDX/dragDY/previewRect for live feedback.
Item {
    id: overlay

    property EditorController editor
    property LayoutController controller
    property real scaleFactor: 1
    property bool active: false

    enabled: active
    visible: active

    // Live gesture state, read by ZoneItems.
    property real dragDX: 0
    property real dragDY: 0
    property string resizeId: ""
    property var previewRect: null
    property string rotateId: ""
    property var previewRotation: null

    readonly property int grid: controller ? controller.pageGrid : 8
    readonly property var geo: editor ? editor.geometry : []
    readonly property var sel: editor ? editor.selection : []
    readonly property real px: 1 / Math.max(scaleFactor, 0.01)   // one screen pixel
    readonly property real snapDistance: 8 * px
    readonly property real handleSize: 14 * px
    // How far a finger may wander before a touch is a drag (not a tap).
    readonly property real dragSlop: Math.max(3, Qt.styleHints.startDragDistance) * px
    readonly property var selectedGeo: geo.filter(g => sel.includes(g.id))
    readonly property var single: selectedGeo.length === 1 ? selectedGeo[0] : null

    property string mode: ""        // "move" | "resize" | "rotate" | "band" | "pending"
    property point pressPoint
    property var startBounds: null
    property var startRect: null
    property string handle: ""
    property var guides: []         // [{ vertical, pos }]
    property var band: null         // { x, y, w, h }
    property var hovered: null

    function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }
    function snapGrid(v) { return Math.round(v / grid) * grid }

    // A turned zone: a point in its own (unturned) frame, about its middle.
    function turn(px, py, cx, cy, deg) {
        const a = deg * Math.PI / 180, dx = px - cx, dy = py - cy
        return Qt.point(cx + dx * Math.cos(a) - dy * Math.sin(a), cy + dx * Math.sin(a) + dy * Math.cos(a))
    }
    function local(g, x, y) { return g.rot ? turn(x, y, g.x + g.w / 2, g.y + g.h / 2, -g.rot) : Qt.point(x, y) }

    function zoneAt(x, y) {
        for (let i = geo.length - 1; i >= 0; --i) {
            const g = geo[i]
            if (g.kind === "comment" && !active) continue
            const p = local(g, x, y)
            if (p.x >= g.x && p.x < g.x + g.w && p.y >= g.y && p.y < g.y + g.h)
                return g
        }
        return null
    }

    // The round handle that turns a single zone: above its top, turned with it.
    readonly property real rotateReach: 34 * px
    // Below it when its top is at the top of the page (no room above).
    function rotateBelow(g) { return g.y - rotateReach - handleSize < 0 }
    function rotateSpot(g) {
        const y = rotateBelow(g) ? g.y + g.h + rotateReach : g.y - rotateReach
        return turn(g.x + g.w / 2, y, g.x + g.w / 2, g.y + g.h / 2, g.rot || 0)
    }
    function onRotateHandle(x, y) {
        if (!single || single.inherited) return false
        const s = rotateSpot(single)
        const reach = Math.max(handleSize, 22 * px)
        return Math.abs(x - s.x) <= reach && Math.abs(y - s.y) <= reach
    }

    function boundsOf(list) {
        if (list.length === 0) return null
        let x1 = Infinity, y1 = Infinity, x2 = -Infinity, y2 = -Infinity
        for (const g of list) {
            x1 = Math.min(x1, g.x); y1 = Math.min(y1, g.y)
            x2 = Math.max(x2, g.x + g.w); y2 = Math.max(y2, g.y + g.h)
        }
        return { x: x1, y: y1, w: x2 - x1, h: y2 - y1 }
    }

    // Candidate lines to snap to: canvas edges/center and every other zone's
    // edges/center (template zones included, so pages line up with them).
    function targets(vertical, exclude) {
        const size = vertical ? controller.canvasSize.width : controller.canvasSize.height
        const t = [0, size / 2, size]
        for (const g of geo) {
            if (exclude.includes(g.id)) continue
            if (vertical) t.push(g.x, g.x + g.w / 2, g.x + g.w)
            else t.push(g.y, g.y + g.h / 2, g.y + g.h)
        }
        return t
    }

    // Best (smallest) snap of any candidate to any target within reach.
    function snapTo(candidates, lines) {
        let best = null
        for (const c of candidates) {
            for (const t of lines) {
                const d = t - c
                if (Math.abs(d) <= snapDistance && (!best || Math.abs(d) < Math.abs(best.delta)))
                    best = { delta: d, at: t }
            }
        }
        return best
    }

    function handleRects(g) {
        const s = handleSize, h = s / 2
        const xs = { l: g.x - h, c: g.x + g.w / 2 - h, r: g.x + g.w - h }
        const ys = { t: g.y - h, m: g.y + g.h / 2 - h, b: g.y + g.h - h }
        return [
            { id: "tl", x: xs.l, y: ys.t, cursor: Qt.SizeFDiagCursor },
            { id: "t",  x: xs.c, y: ys.t, cursor: Qt.SizeVerCursor },
            { id: "tr", x: xs.r, y: ys.t, cursor: Qt.SizeBDiagCursor },
            { id: "r",  x: xs.r, y: ys.m, cursor: Qt.SizeHorCursor },
            { id: "br", x: xs.r, y: ys.b, cursor: Qt.SizeFDiagCursor },
            { id: "b",  x: xs.c, y: ys.b, cursor: Qt.SizeVerCursor },
            { id: "bl", x: xs.l, y: ys.b, cursor: Qt.SizeBDiagCursor },
            { id: "l",  x: xs.l, y: ys.m, cursor: Qt.SizeHorCursor },
        ]
    }

    function handleAt(x, y) {
        if (!single) return null
        const lp = local(single, x, y)   // a turned zone's handles turn with it
        x = lp.x
        y = lp.y
        const s = handleSize
        // A finger's reach (handles are small on screen), less on small zones
        // so the middle of one still moves it.
        const pad = Math.max(4 * px, Math.min(16 * px, Math.min(single.w, single.h) / 4 - s / 2))
        for (const hr of handleRects(single)) {
            if (x >= hr.x - pad && x <= hr.x + s + pad && y >= hr.y - pad && y <= hr.y + s + pad)
                return hr
        }
        return null
    }

    function moveDelta(rawDX, rawDY) {
        const b = startBounds
        let nx = b.x + rawDX, ny = b.y + rawDY
        const gs = []
        const sx = snapTo([nx, nx + b.w / 2, nx + b.w], targets(true, sel))
        if (sx) { nx += sx.delta; gs.push({ vertical: true, pos: sx.at }) }
        else nx = snapGrid(nx)
        const sy = snapTo([ny, ny + b.h / 2, ny + b.h], targets(false, sel))
        if (sy) { ny += sy.delta; gs.push({ vertical: false, pos: sy.at }) }
        else ny = snapGrid(ny)
        nx = clamp(nx, 0, controller.canvasSize.width - b.w)
        ny = clamp(ny, 0, controller.canvasSize.height - b.h)
        guides = gs
        return Qt.point(nx - b.x, ny - b.y)
    }

    function snapEdge(v, vertical, gs) {
        const s = snapTo([v], targets(vertical, sel))
        if (s) { gs.push({ vertical: vertical, pos: s.at }); return s.at }
        return snapGrid(v)
    }

    function resizeRect(p) {
        const r = startRect
        let x1 = r.x, y1 = r.y, x2 = r.x + r.w, y2 = r.y + r.h
        const gs = []
        const minSize = 16
        if (handle.includes("l")) x1 = Math.min(snapEdge(clamp(p.x, 0, x2 - minSize), true, gs), x2 - minSize)
        if (handle.includes("r")) x2 = Math.max(snapEdge(clamp(p.x, x1 + minSize, controller.canvasSize.width), true, gs), x1 + minSize)
        if (handle.includes("t")) y1 = Math.min(snapEdge(clamp(p.y, 0, y2 - minSize), false, gs), y2 - minSize)
        if (handle.includes("b")) y2 = Math.max(snapEdge(clamp(p.y, y1 + minSize, controller.canvasSize.height), false, gs), y1 + minSize)
        guides = gs
        return { x: x1, y: y1, w: x2 - x1, h: y2 - y1 }
    }

    // Resizing a turned zone: the edge follows the finger in the zone's own
    // frame, and the opposite edge stays where it is on the page.
    function resizeTurned(p) {
        const r = startRect, deg = r.rot
        const cx = r.x + r.w / 2, cy = r.y + r.h / 2
        const lp = turn(p.x, p.y, cx, cy, -deg)
        let x1 = r.x, y1 = r.y, x2 = r.x + r.w, y2 = r.y + r.h
        const minSize = 16
        if (handle.includes("l")) x1 = Math.min(snapGrid(lp.x), x2 - minSize)
        if (handle.includes("r")) x2 = Math.max(snapGrid(lp.x), x1 + minSize)
        if (handle.includes("t")) y1 = Math.min(snapGrid(lp.y), y2 - minSize)
        if (handle.includes("b")) y2 = Math.max(snapGrid(lp.y), y1 + minSize)
        // Its new middle, turned back onto the page.
        const c = turn((x1 + x2) / 2, (y1 + y2) / 2, cx, cy, deg)
        const w = x2 - x1, h = y2 - y1
        guides = []
        return { x: c.x - w / 2, y: c.y - h / 2, w: w, h: h }
    }

    function finishGesture() {
        mode = ""
        dragDX = 0
        dragDY = 0
        resizeId = ""
        previewRect = null
        rotateId = ""
        previewRotation = null
        guides = []
        band = null
    }

    // Faint grid so the snap spacing is visible.
    Canvas {
        id: gridCanvas
        anchors.fill: parent
        opacity: 0.18
        readonly property int step: Math.max(overlay.grid * 8, 32)
        onStepChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = "#ffffff"
            ctx.lineWidth = overlay.px
            ctx.beginPath()
            for (let x = step; x < width; x += step) { ctx.moveTo(x, 0); ctx.lineTo(x, height) }
            for (let y = step; y < height; y += step) { ctx.moveTo(0, y); ctx.lineTo(width, y) }
            ctx.stroke()
        }
    }

    // Outline under the pointer.
    Rectangle {
        visible: !!overlay.hovered && overlay.mode === "" && !overlay.sel.includes(overlay.hovered.id)
        x: overlay.hovered ? overlay.hovered.x : 0
        y: overlay.hovered ? overlay.hovered.y : 0
        width: overlay.hovered ? overlay.hovered.w : 0
        height: overlay.hovered ? overlay.hovered.h : 0
        rotation: overlay.hovered ? overlay.hovered.rot || 0 : 0
        color: "transparent"
        border.width: 2 * overlay.px
        border.color: overlay.hovered && overlay.hovered.inherited ? EditorStyle.muted : Qt.rgba(1, 1, 1, 0.6)
    }

    // Selection outlines, following the drag.
    Repeater {
        model: overlay.selectedGeo
        delegate: Rectangle {
            required property var modelData
            readonly property bool resizing: overlay.resizeId === modelData.id && overlay.previewRect
            x: resizing ? overlay.previewRect.x : modelData.x + overlay.dragDX
            y: resizing ? overlay.previewRect.y : modelData.y + overlay.dragDY
            width: resizing ? overlay.previewRect.w : modelData.w
            height: resizing ? overlay.previewRect.h : modelData.h
            rotation: overlay.rotateId === modelData.id && overlay.previewRotation !== null ? overlay.previewRotation : modelData.rot || 0
            color: Qt.rgba(0.3, 0.55, 1, 0.08)
            border.width: 2 * overlay.px
            border.color: EditorStyle.accent
        }
    }

    // Resize handles for a single selection (turned with it).
    Repeater {
        id: handles
        readonly property var g: !overlay.single ? null
                                 : overlay.resizeId && overlay.previewRect ? Object.assign({}, overlay.previewRect, { rot: overlay.single.rot })
                                 : overlay.single
        model: overlay.single && overlay.mode !== "move" && overlay.mode !== "rotate" ? overlay.handleRects(g) : []
        delegate: Rectangle {
            required property var modelData
            readonly property point at: handles.g && handles.g.rot
                ? overlay.turn(modelData.x + overlay.handleSize / 2, modelData.y + overlay.handleSize / 2,
                               handles.g.x + handles.g.w / 2, handles.g.y + handles.g.h / 2, handles.g.rot)
                : Qt.point(modelData.x + overlay.handleSize / 2, modelData.y + overlay.handleSize / 2)
            x: at.x - overlay.handleSize / 2
            y: at.y - overlay.handleSize / 2
            rotation: handles.g ? handles.g.rot || 0 : 0
            width: overlay.handleSize
            height: overlay.handleSize
            color: "white"
            border.width: 2 * overlay.px
            border.color: EditorStyle.accent
        }
    }

    // The turn handle: a round knob above a single selection, on a stalk.
    Item {
        visible: !!overlay.single && !overlay.single.inherited && overlay.mode !== "move" && overlay.mode !== "resize"
        readonly property var g: overlay.single
        readonly property real deg: overlay.mode === "rotate" && overlay.previewRotation !== null ? overlay.previewRotation
                                                                                                 : (g ? g.rot || 0 : 0)
        x: g ? g.x : 0
        y: g ? g.y : 0
        width: g ? g.w : 0
        height: g ? g.h : 0
        rotation: deg
        readonly property bool below: g ? overlay.rotateBelow(g) : false
        Rectangle {
            x: parent.width / 2 - overlay.px
            y: parent.below ? parent.height : -overlay.rotateReach
            width: 2 * overlay.px
            height: overlay.rotateReach
            color: EditorStyle.accent
        }
        Rectangle {
            objectName: "rotateHandle"
            readonly property real d: overlay.handleSize * 1.4
            x: parent.width / 2 - d / 2
            y: (parent.below ? parent.height + overlay.rotateReach : -overlay.rotateReach) - d / 2
            width: d
            height: d
            radius: d / 2
            color: "white"
            border.width: 2 * overlay.px
            border.color: EditorStyle.accent
            Text {
                anchors.centerIn: parent
                text: "⟳"
                font.pixelSize: parent.d * 0.7
                color: EditorStyle.accent
            }
        }
    }

    // Alignment guides.
    Repeater {
        model: overlay.guides
        delegate: Rectangle {
            required property var modelData
            x: modelData.vertical ? modelData.pos - overlay.px : 0
            y: modelData.vertical ? 0 : modelData.pos - overlay.px
            width: modelData.vertical ? 2 * overlay.px : overlay.width
            height: modelData.vertical ? overlay.height : 2 * overlay.px
            color: EditorStyle.guide
        }
    }

    // Rubber band.
    Rectangle {
        visible: !!overlay.band
        x: overlay.band ? overlay.band.x : 0
        y: overlay.band ? overlay.band.y : 0
        width: overlay.band ? overlay.band.w : 0
        height: overlay.band ? overlay.band.h : 0
        color: Qt.rgba(0.3, 0.55, 1, 0.15)
        border.width: overlay.px
        border.color: EditorStyle.accent
    }

    // Size / position readout while dragging.
    Rectangle {
        readonly property var r: overlay.mode === "rotate" && overlay.single ? overlay.single
                                 : overlay.previewRect
                                 ? overlay.previewRect
                                 : (overlay.mode === "move" && overlay.startBounds
                                    ? { x: overlay.startBounds.x + overlay.dragDX, y: overlay.startBounds.y + overlay.dragDY,
                                        w: overlay.startBounds.w, h: overlay.startBounds.h } : null)
        visible: !!r
        x: r ? r.x : 0
        y: r ? Math.max(0, r.y - height - 6 * overlay.px) : 0
        width: readout.implicitWidth + 12 * overlay.px
        height: readout.implicitHeight + 6 * overlay.px
        radius: 4 * overlay.px
        color: "#e0101418"
        Text {
            id: readout
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 13 * overlay.px
            text: overlay.mode === "rotate" && overlay.previewRotation !== null ? overlay.previewRotation + "°"
                : parent.r ? `${Math.round(parent.r.x)}, ${Math.round(parent.r.y)}   ${Math.round(parent.r.w)} × ${Math.round(parent.r.h)}` : ""
        }
    }

    // Hint for zones that belong to a template.
    Rectangle {
        visible: !!overlay.hovered && overlay.hovered.inherited && overlay.mode === ""
        x: overlay.hovered ? Math.min(overlay.hovered.x, overlay.width - width) : 0
        y: overlay.hovered ? Math.max(0, overlay.hovered.y - height - 4 * overlay.px) : 0
        width: hint.implicitWidth + 16 * overlay.px
        height: hint.implicitHeight + 8 * overlay.px
        radius: 4 * overlay.px
        color: "#e0101418"
        Text {
            id: hint
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 13 * overlay.px
            text: overlay.hovered ? qsTr("From template “%1” — double-click to edit it").arg(overlay.hovered.ownerName) : ""
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        preventStealing: true

        // Set imperatively as the gesture changes.
        cursorShape: Qt.ArrowCursor

        onPositionChanged: m => {
            const p = Qt.point(m.x, m.y)
            if (overlay.mode === "") {
                if (overlay.onRotateHandle(p.x, p.y)) {
                    overlay.hovered = null
                    cursorShape = Qt.PointingHandCursor
                    return
                }
                const h = overlay.handleAt(p.x, p.y)
                overlay.hovered = h ? null : overlay.zoneAt(p.x, p.y)
                cursorShape = h ? h.cursor : (overlay.hovered && !overlay.hovered.inherited ? Qt.OpenHandCursor : Qt.ArrowCursor)
                return
            }
            if (overlay.mode === "pending") {
                // A tap (a finger wanders a little) never nudges a zone.
                if (Math.abs(p.x - overlay.pressPoint.x) < overlay.dragSlop && Math.abs(p.y - overlay.pressPoint.y) < overlay.dragSlop)
                    return
                overlay.mode = "move"
                cursorShape = Qt.ClosedHandCursor
            }
            if (overlay.mode === "rotate") {
                // From its middle to the finger, 0° straight up; 15° steps (Shift: free).
                const g = overlay.single
                const a = Math.atan2(p.y - (g.y + g.h / 2), p.x - (g.x + g.w / 2)) * 180 / Math.PI + 90
                          - (overlay.rotateBelow(g) ? 180 : 0)
                const free = (m.modifiers & Qt.ShiftModifier) !== 0
                overlay.previewRotation = ((Math.round(free ? a : Math.round(a / 15) * 15) % 360) + 360) % 360
                return
            }
            if (overlay.mode === "move") {
                const d = overlay.moveDelta(p.x - overlay.pressPoint.x, p.y - overlay.pressPoint.y)
                overlay.dragDX = d.x
                overlay.dragDY = d.y
            } else if (overlay.mode === "resize") {
                overlay.previewRect = overlay.startRect.rot ? overlay.resizeTurned(p) : overlay.resizeRect(p)
            } else if (overlay.mode === "band") {
                const x1 = Math.min(p.x, overlay.pressPoint.x), y1 = Math.min(p.y, overlay.pressPoint.y)
                overlay.band = { x: x1, y: y1, w: Math.abs(p.x - overlay.pressPoint.x), h: Math.abs(p.y - overlay.pressPoint.y) }
            }
        }

        onPressed: m => {
            overlay.forceActiveFocus()   // commit any inspector text field
            const p = Qt.point(m.x, m.y)
            overlay.pressPoint = p
            const additive = (m.modifiers & (Qt.ShiftModifier | Qt.ControlModifier)) !== 0

            if (m.button === Qt.LeftButton && overlay.onRotateHandle(p.x, p.y)) {
                overlay.mode = "rotate"
                overlay.rotateId = overlay.single.id
                overlay.previewRotation = overlay.single.rot || 0
                return
            }
            const h = m.button === Qt.LeftButton ? overlay.handleAt(p.x, p.y) : null
            if (h) {
                overlay.mode = "resize"
                overlay.handle = h.id
                overlay.startRect = { x: overlay.single.x, y: overlay.single.y, w: overlay.single.w, h: overlay.single.h,
                                      rot: overlay.single.rot || 0 }
                overlay.resizeId = overlay.single.id
                overlay.previewRect = overlay.startRect
                return
            }

            const z = overlay.zoneAt(p.x, p.y)
            if (z && z.inherited) {
                if (!additive) overlay.editor.clearSelection()
                overlay.editor.setNotice(qsTr("“%1” comes from the template “%2”. Double-click it to edit the template.")
                                       .arg(z.label || z.id).arg(z.ownerName))
                overlay.mode = ""
                return
            }
            if (z) {
                if (additive) {
                    overlay.editor.select(z.id, true)
                    overlay.mode = ""
                    return
                }
                if (!overlay.sel.includes(z.id))
                    overlay.editor.select(z.id, false)
                if (m.button === Qt.RightButton) {
                    const at = mouse.mapToItem(contextMenu.parent, m.x, m.y)
                    contextMenu.popup(at.x, at.y)
                    overlay.mode = ""
                    return
                }
                overlay.startBounds = overlay.boundsOf(overlay.geo.filter(g => overlay.editor.selection.includes(g.id)))
                overlay.mode = "pending"
                return
            }
            if (!additive)
                overlay.editor.clearSelection()
            if (m.button === Qt.RightButton) {
                overlay.mode = ""
                return
            }
            overlay.mode = "band"
            overlay.band = { x: p.x, y: p.y, w: 0, h: 0 }
        }

        onReleased: m => {
            const additive = (m.modifiers & (Qt.ShiftModifier | Qt.ControlModifier)) !== 0
            if (overlay.mode === "move" && (overlay.dragDX !== 0 || overlay.dragDY !== 0)) {
                const rects = {}
                for (const g of overlay.selectedGeo)
                    rects[g.id] = { x: g.x + overlay.dragDX, y: g.y + overlay.dragDY, w: g.w, h: g.h }
                const commit = rects
                overlay.finishGesture()
                overlay.editor.commitRects(commit)
                return
            }
            if (overlay.mode === "rotate") {
                const deg = overlay.previewRotation
                const was = overlay.single ? overlay.single.rot || 0 : 0
                overlay.finishGesture()
                if (deg !== null && deg !== was)
                    overlay.editor.setRotation(deg)
                return
            }
            if (overlay.mode === "resize" && overlay.previewRect) {
                const rects = {}
                rects[overlay.resizeId] = overlay.previewRect
                overlay.finishGesture()
                overlay.editor.commitRects(rects)
                return
            }
            if (overlay.mode === "band" && overlay.band && (overlay.band.w > overlay.dragSlop || overlay.band.h > overlay.dragSlop))
                overlay.editor.selectInRect(overlay.band.x, overlay.band.y, overlay.band.w, overlay.band.h, additive)
            overlay.finishGesture()
        }

        onCanceled: overlay.finishGesture()

        // A touchscreen has no right button: holding a zone opens its menu.
        onPressAndHold: m => {
            if (overlay.mode !== "pending")
                return   // dragging, resizing or drawing a box
            overlay.finishGesture()
            const at = mouse.mapToItem(contextMenu.parent, m.x, m.y)
            contextMenu.popup(at.x, at.y)
        }

        onDoubleClicked: m => {
            const z = overlay.zoneAt(m.x, m.y)
            if (z && z.inherited)
                overlay.controller.showPage(z.owner)
        }

        onExited: overlay.hovered = null
    }

    Menu {
        id: contextMenu
        parent: Overlay.overlay   // not scaled with the canvas
        MenuItem { text: qsTr("Copy"); onTriggered: overlay.editor.copy() }
        MenuItem { text: qsTr("Cut"); onTriggered: overlay.editor.cut() }
        MenuItem { text: qsTr("Duplicate"); onTriggered: overlay.editor.duplicateSelection() }
        MenuItem { text: qsTr("Delete"); onTriggered: overlay.editor.deleteSelection() }
        MenuSeparator {}
        MenuItem { text: qsTr("Bring to front"); onTriggered: overlay.editor.bringToFront() }
        MenuItem { text: qsTr("Send to back"); onTriggered: overlay.editor.sendToBack() }
        MenuSeparator {}
        MenuItem { text: qsTr("Turn 90° ⟳"); onTriggered: overlay.editor.rotateSelection(90) }
        MenuItem { text: qsTr("Turn 15° ⟳"); onTriggered: overlay.editor.rotateSelection(15) }
        MenuItem { text: qsTr("Turn 15° ⟲"); onTriggered: overlay.editor.rotateSelection(-15) }
        MenuItem { text: qsTr("Straighten"); onTriggered: overlay.editor.setRotation(0) }
    }
}
