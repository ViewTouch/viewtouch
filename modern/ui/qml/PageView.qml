import QtQuick

// Renders the controller's current page on its logical canvas, scaled
// uniformly (letterboxed) to fill this item.
Item {
    id: view

    required property LayoutController controller
    // Set while editing; enables the edit overlay.
    property EditorController editor: null
    readonly property bool editing: editor !== null
    property string selectedZoneId: ""

    readonly property size canvas: controller.canvasSize
    readonly property real scaleFactor: Math.min(width / canvas.width, height / canvas.height)
    readonly property var bg: controller.background

    Connections {
        target: view.controller
        function onPageChanged() { view.selectedZoneId = "" }
    }

    // The page at canvas size (the editor's Preview shows it on other screens).
    readonly property Item surfaceItem: surface

    Item {
        id: surface
        objectName: "pageSurface"   // tests map canvas coordinates through this
        width: view.canvas.width
        height: view.canvas.height
        scale: view.scaleFactor
        transformOrigin: Item.TopLeft
        x: (view.width - width * scale) / 2
        y: (view.height - height * scale) / 2
        clip: true

        Rectangle {
            anchors.fill: parent
            color: view.bg.fill ?? "#20232a"
        }

        Image {
            anchors.fill: parent
            visible: !!view.bg.texture
            source: view.bg.texture ? "qrc:/textures/" + view.bg.texture + ".xpm" : ""
            fillMode: Image.Tile
        }

        // A picture behind the page (Page / Theme -> Background -> Picture).
        Image {
            objectName: "pagePicture"
            anchors.fill: parent
            readonly property var pos: view.controller.pos
            visible: !!view.bg.image && status === Image.Ready
            source: !view.bg.image ? "" : pos ? (pos.imageRevision < 0 ? undefined : pos.imageUrl(view.bg.image)) : view.bg.image
            asynchronous: true
            fillMode: ({ fit: Image.PreserveAspectFit, stretch: Image.Stretch, tile: Image.Tile,
                         center: Image.Pad })[view.bg.imageFit] ?? Image.PreserveAspectCrop
        }

        Repeater {
            model: view.controller.zones
            delegate: ZoneItem {
                controller: view.controller
                pos: view.controller.pos
                screenScale: view.scaleFactor
                selectedZoneId: view.selectedZoneId
                editing: view.editing
                editSelected: view.editing && editLayer.sel.includes(zoneId)
                dragDX: editLayer.dragDX
                dragDY: editLayer.dragDY
                previewRect: editLayer.resizeId === zoneId ? editLayer.previewRect : null
                previewRotation: editLayer.rotateId === zoneId ? editLayer.previewRotation : null
                onSelectRequested: view.selectedZoneId = zoneId
                onActivated: view.controller.activate(zoneId)
                onExplainRequested: view.controller.explain(zoneId)
            }
        }

        EditLayer {
            id: editLayer
            anchors.fill: parent
            active: view.editing
            editor: view.editor
            controller: view.controller
            scaleFactor: view.scaleFactor
        }
    }

    // Canvas boundary while editing.
    Rectangle {
        visible: view.editing
        x: surface.x - 1
        y: surface.y - 1
        width: surface.width * surface.scale + 2
        height: surface.height * surface.scale + 2
        color: "transparent"
        border.color: EditorStyle.border
    }
}
