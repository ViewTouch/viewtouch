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
    // A phone page longer than the screen (a long Manager page, a 19.5:9
    // phone's page on a 16:9 Stripe reader): as wide as the screen, and it
    // scrolls, instead of shrinking under the finger with bands at the sides.
    readonly property bool scrolls: !editing && controller.formFactor === "phone" && canvas.width > 0
                                    && canvas.height * width / canvas.width > height + 1
    readonly property real scaleFactor: scrolls ? width / canvas.width
                                                : Math.min(width / canvas.width, height / canvas.height)
    readonly property var bg: controller.background

    Connections {
        target: view.controller
        function onPageChanged() {
            view.selectedZoneId = ""
            if (view.controller.pageId !== flick.lastPage) {   // another page: from its top
                flick.lastPage = view.controller.pageId
                flick.contentY = 0
            }
        }
    }

    // The page at canvas size (the editor's Preview shows it on other screens).
    readonly property Item surfaceItem: surface

    Flickable {
        id: flick
        objectName: "pageScroll"
        property string lastPage
        anchors.fill: parent
        interactive: view.scrolls
        contentWidth: width
        contentHeight: view.scrolls ? view.canvas.height * view.scaleFactor : height
        boundsBehavior: Flickable.StopAtBounds
        clip: view.scrolls

    Item {
        id: surface
        objectName: "pageSurface"   // tests map canvas coordinates through this
        width: view.canvas.width
        height: view.canvas.height
        scale: view.scaleFactor
        transformOrigin: Item.TopLeft
        x: (view.width - width * scale) / 2
        y: view.scrolls ? 0 : (view.height - height * scale) / 2
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
    }

    // Where it is on a page that scrolls.
    Rectangle {
        visible: view.scrolls
        anchors.right: parent.right
        anchors.rightMargin: 3
        width: 5
        radius: 2.5
        color: "#80ffffff"
        y: flick.visibleArea.yPosition * view.height
        height: Math.max(24, flick.visibleArea.heightRatio * view.height)
        opacity: flick.moving ? 0.9 : 0.45
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
