import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Left-hand panel: every page, with create / duplicate / delete.
Rectangle {
    id: panel

    required property LayoutController controller
    required property EditorController editor

    color: EditorStyle.panel

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Label {
            text: qsTr("Pages")
            font.bold: true
            font.pixelSize: 15
            Layout.margins: 12
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            // A page's phone version is reached by the Standard | Phone switch.
            readonly property var listed: panel.editor.pages.filter(p => !p.variantOf)
            readonly property string shownPage: panel.editor.revision >= 0 ? (panel.editor.screens.page ?? panel.editor.pageId) : ""
            model: listed
            currentIndex: listed.findIndex(p => p.id === shownPage)
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: row
                required property var modelData
                required property int index
                width: ListView.view.width
                highlighted: ListView.isCurrentItem
                onClicked: panel.controller.showPage(modelData.id)

                contentItem: ColumnLayout {
                    spacing: 1
                    Label {
                        text: row.modelData.name ? qsTranslate("Page", row.modelData.name) : row.modelData.id
                        font.bold: row.highlighted
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Label {
                        text: panel.editor.pageKindName(row.modelData.kind)
                              + (row.modelData.role ? "  ·  " + qsTr("role: %1").arg(panel.editor.pageRoleName(row.modelData.role)) : "")
                              + (row.modelData.templateId ? "  ·  ⧉ " + row.modelData.templateId : "")
                              + (row.modelData.hasPhone ? "  ·  " + qsTr("+ phone") : "")
                        color: row.highlighted ? Qt.rgba(1, 1, 1, 0.8) : EditorStyle.muted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: EditorStyle.border }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            spacing: 4
            Button {
                text: qsTr("New…")
                Layout.fillWidth: true
                onClicked: newPageDialog.open()
            }
            Button {
                text: qsTr("Copy")
                Layout.fillWidth: true
                onClicked: panel.editor.duplicatePage()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Duplicate this page")
            }
            Button {
                text: qsTr("Delete")
                Layout.fillWidth: true
                onClicked: deleteDialog.open()
            }
        }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: EditorStyle.border
    }

    Dialog {
        id: newPageDialog
        title: qsTr("New page")
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAboutToShow: DialogWords.apply(this)
        width: 380

        onOpened: {
            nameField.text = ""
            kindBox.currentIndex = 0
            templateBox.currentIndex = 0
            nameField.forceActiveFocus()
        }
        onAccepted: panel.editor.newPage(nameField.text, kindBox.currentText, templateBox.currentValue)

        ColumnLayout {
            anchors.fill: parent
            spacing: 6
            Label { text: qsTr("Name") }
            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: qsTr("e.g. Happy Hour")
                onAccepted: newPageDialog.accept()
            }
            Label { text: qsTr("Type") }
            ComboBox {
                id: kindBox
                Layout.fillWidth: true
                model: panel.editor.pageKinds
                onActivated: {
                    // Menu pages normally share the order template.
                    const menuKinds = ["index", "items", "modifier"]
                    const idx = templateBox.model.findIndex(o => o.value === "order-template")
                    if (menuKinds.includes(currentText) && idx >= 0 && templateBox.currentIndex === 0)
                        templateBox.currentIndex = idx
                }
            }
            Label { text: qsTr("Template (zones shown behind this page)") }
            ComboBox {
                id: templateBox
                Layout.fillWidth: true
                model: panel.editor.revision >= 0 ? panel.editor.pageOptions() : []   // pages made since show too
                textRole: "text"
                valueRole: "value"
            }
        }
    }

    Dialog {
        id: deleteDialog
        title: qsTr("Delete page?")
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAboutToShow: DialogWords.apply(this)
        width: 420
        onAccepted: panel.editor.deletePage()

        ColumnLayout {
            anchors.fill: parent
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: panel.editor.references.length === 0
                      ? qsTr("Delete this page? No buttons lead here. You can undo this.")
                      : qsTr("These buttons lead here and will stop working:\n• %1\n\nYou can undo this.")
                        .arg(panel.editor.references.join("\n• "))
            }
        }
    }
}
