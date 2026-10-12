import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Dialogs
import QtQuick.Layouts

// Top bar in edit mode: save/exit, undo/redo, add zones, arrange, page files.
Rectangle {
    id: bar

    required property LayoutController controller
    required property EditorController editor
    property Item pageSurface: null   // the page as drawn, for Preview

    signal exitRequested()

    color: EditorStyle.panel
    implicitHeight: EditorStyle.toolbarHeight

    readonly property bool hasSelection: editor.selection.length > 0

    component Sep: Rectangle {
        implicitWidth: 1
        implicitHeight: 28
        color: EditorStyle.border
    }

    component Tool: ToolButton {
        property string tip
        ToolTip.visible: hovered && tip !== ""
        ToolTip.text: tip
        ToolTip.delay: 500
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 2

        Label {
            text: qsTr("Editing")
            color: EditorStyle.accent
            font.bold: true
            Layout.rightMargin: 4
        }
        Label {
            text: qsTranslate("Page", bar.controller.pageName)
            font.bold: true
            elide: Text.ElideRight
            Layout.maximumWidth: 180
            Layout.rightMargin: 8
        }

        // The tools scroll sideways on a narrow screen; Save and Done stay put.
        Flickable {
            id: toolsScroll
            objectName: "editorTools"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: tools.implicitWidth
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentWidth > width
            clip: true
            ScrollIndicator.horizontal: ScrollIndicator {}
            RowLayout {
                id: tools
                height: toolsScroll.height
                spacing: 2
                Sep {}
                Tool { text: qsTr("Undo"); enabled: bar.editor.canUndo; tip: bar.editor.undoText; onClicked: bar.editor.undo() }
                Tool { text: qsTr("Redo"); enabled: bar.editor.canRedo; tip: bar.editor.redoText; onClicked: bar.editor.redo() }

                Sep {}
                Tool { text: qsTr("+ Button"); tip: qsTr("Add a button"); onClicked: bar.editor.addZone("button") }
                Tool { text: qsTr("+ Text"); tip: qsTr("Add a text label"); onClicked: bar.editor.addZone("label") }
                Tool { text: qsTr("+ Image"); tip: qsTr("Add an image button"); onClicked: bar.editor.addZone("image") }
                Tool { text: qsTr("+ Note"); tip: qsTr("Add a note only visible while editing"); onClicked: bar.editor.addZone("comment") }
                // Floor plans: tables, and walls, doors and the rest around them.
                Tool {
                    objectName: "addFloorPiece"
                    text: qsTr("+ Floor Plan ▾")
                    tip: qsTr("Add a table, a wall, a door, a bar counter… to draw the room")
                    onClicked: floorMenu.popup(0, height)
                    Menu {
                        id: floorMenu
                        MenuItem { text: qsTr("Table"); onTriggered: bar.editor.addZone("table") }
                        MenuSeparator {}
                        Repeater {
                            model: [
                                { decor: "wall", text: qsTr("Wall") }, { decor: "window", text: qsTr("Window") },
                                { decor: "door", text: qsTr("Door") }, { decor: "counter", text: qsTr("Bar counter") },
                                { decor: "booth", text: qsTr("Booth bench") }, { decor: "plant", text: qsTr("Plant") },
                                { decor: "pillar", text: qsTr("Pillar") }, { decor: "fence", text: qsTr("Fence or rail") },
                                { decor: "entrance", text: qsTr("Entrance") }, { decor: "area", text: qsTr("Area (a room's floor)") },
                            ]
                            delegate: MenuItem {
                                required property var modelData
                                objectName: "addDecor-" + modelData.decor
                                text: modelData.text
                                onTriggered: bar.editor.addZone("decor:" + modelData.decor)
                            }
                        }
                    }
                }
                Tool {
                    text: qsTr("+ Panel ▾")
                    tip: qsTr("Add a working panel: order list, keypad, table map…")
                    onClicked: widgetMenu.popup(0, height)   // under the button (a touch has no pointer)
                    Menu {
                        id: widgetMenu
                        Repeater {
                            model: bar.editor.widgetKinds
                            delegate: MenuItem {
                                required property string modelData
                                text: bar.editor.kindName(modelData)
                                onTriggered: bar.editor.addZone(modelData)
                            }
                        }
                    }
                }

                Sep {}
                Tool { text: qsTr("Copy"); enabled: bar.hasSelection; tip: "Ctrl+C"; onClicked: bar.editor.copy() }
                Tool { text: qsTr("Paste"); enabled: bar.editor.hasClipboard; tip: qsTr("Ctrl+V — works across pages"); onClicked: bar.editor.paste() }
                Tool { text: qsTr("Duplicate"); enabled: bar.hasSelection; tip: "Ctrl+D"; onClicked: bar.editor.duplicateSelection() }
                Tool { text: qsTr("Delete"); enabled: bar.hasSelection; tip: "Del"; onClicked: bar.editor.deleteSelection() }

                Sep {}
                Tool {
                    text: qsTr("Arrange ▾")
                    enabled: bar.hasSelection
                    onClicked: arrangeMenu.popup(0, height)   // under the button (a touch has no pointer)
                    Menu {
                        id: arrangeMenu
                        MenuItem { text: qsTr("Turn 90° ⟳"); onTriggered: bar.editor.rotateSelection(90) }
                        MenuItem { text: qsTr("Turn 15° ⟳"); onTriggered: bar.editor.rotateSelection(15) }
                        MenuItem { text: qsTr("Turn 15° ⟲"); onTriggered: bar.editor.rotateSelection(-15) }
                        MenuItem { text: qsTr("Straighten"); onTriggered: bar.editor.setRotation(0) }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Bring to front"); onTriggered: bar.editor.bringToFront() }
                        MenuItem { text: qsTr("Send to back"); onTriggered: bar.editor.sendToBack() }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Align left edges"); onTriggered: bar.editor.align("left") }
                        MenuItem { text: qsTr("Align centers"); onTriggered: bar.editor.align("hcenter") }
                        MenuItem { text: qsTr("Align right edges"); onTriggered: bar.editor.align("right") }
                        MenuItem { text: qsTr("Align top edges"); onTriggered: bar.editor.align("top") }
                        MenuItem { text: qsTr("Align middles"); onTriggered: bar.editor.align("vcenter") }
                        MenuItem { text: qsTr("Align bottom edges"); onTriggered: bar.editor.align("bottom") }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Space evenly across"); onTriggered: bar.editor.distribute("horizontal") }
                        MenuItem { text: qsTr("Space evenly down"); onTriggered: bar.editor.distribute("vertical") }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Same width as first"); onTriggered: bar.editor.matchSize("width") }
                        MenuItem { text: qsTr("Same height as first"); onTriggered: bar.editor.matchSize("height") }
                        MenuItem { text: qsTr("Same size as first"); onTriggered: bar.editor.matchSize("both") }
                    }
                }
                Tool {
                    objectName: "previewButton"
                    text: qsTr("Preview…")
                    tip: qsTr("This page on a phone, a tablet, a terminal and a kiosk")
                    onClicked: preview.open()
                }
                Tool {
                    objectName: "layoutsButton"
                    text: qsTr("Layouts…")
                    tip: qsTr("Ready-made layouts for this page, and page files")
                    onClicked: gallery.open()
                }
                Tool {
                    text: qsTr("File ▾")
                    onClicked: fileMenu.popup(0, height)   // under the button (a touch has no pointer)
                    Menu {
                        id: fileMenu
                        MenuItem { text: qsTr("Export this page…"); onTriggered: fileDialog.run("exportPage") }
                        MenuItem { text: qsTr("Import a page…"); onTriggered: fileDialog.run("importPage") }
                        MenuItem { text: qsTr("Use a page file for this page…"); onTriggered: fileDialog.run("importPageHere") }
                        MenuSeparator {}
                        MenuItem { text: qsTr("Export all pages…"); onTriggered: fileDialog.run("exportLayout") }
                        MenuItem { text: qsTr("Replace all pages from file…"); onTriggered: fileDialog.run("importLayout") }
                    }
                }
            }
        }

        Label {
            visible: bar.editor.dirty
            text: bar.width < 1200 ? qsTr("Unsaved") : qsTr("Unsaved changes")
            color: EditorStyle.warning
            Layout.rightMargin: 6
            Layout.leftMargin: 8
        }
        Button {
            text: qsTr("Save")
            enabled: bar.editor.dirty
            highlighted: true
            onClicked: bar.controller.saveEdits()
        }
        Button {
            text: qsTr("Done")
            onClicked: bar.exitRequested()
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: EditorStyle.border
    }

    PagePreview {
        id: preview
        objectName: "pagePreview"
        editor: bar.editor
        source: bar.pageSurface
    }
    LayoutGallery {
        id: gallery
        objectName: "layoutGallery"
        editor: bar.editor
    }

    FileDialog {
        id: fileDialog
        property string action
        function run(what) {
            action = what
            const saving = what.startsWith("export")
            fileMode = saving ? FileDialog.SaveFile : FileDialog.OpenFile
            const page = what.endsWith("Page") || what === "importPageHere"
            nameFilters = page ? [qsTr("ViewTouch page (*.vtpage.json)"), qsTr("JSON (*.json)")]
                               : [qsTr("ViewTouch layout (*.vtlayout.json)"), qsTr("JSON (*.json)")]
            defaultSuffix = page ? "vtpage.json" : "vtlayout.json"
            selectedFile = saving ? (page ? bar.editor.pageId + ".vtpage.json" : "layout.vtlayout.json") : ""
            open()
        }
        onAccepted: bar.editor[action](selectedFile)
    }
}
