import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Dialogs
import QtQuick.Layouts

// Edit mode -> Layouts…: ready-made arrangements of the page being edited
// (or of the order screen around a menu page), and page files: one page,
// or every page of the restaurant.
Popup {
    id: gal
    objectName: "layoutGallery"
    property EditorController editor
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    // Sized from the window (it's declared in the toolbar).
    width: Overlay.overlay ? Math.min(Overlay.overlay.width - 40, 1200) : 1200
    height: Overlay.overlay ? Math.min(Overlay.overlay.height - 40, 820) : 820
    padding: 20
    background: Rectangle {
        color: EditorStyle.chrome
        border.color: EditorStyle.border
        radius: 10
    }

    readonly property var choices: visible && editor && editor.revision >= 0 ? editor.arrangements() : []   // compared: a comma's left side is compiled away

    // A small drawing of a page: its zones as boxes in their colors.
    component Preview: Rectangle {
        property var zones: []
        property var bg: ({})
        readonly property real k: width / 1920
        color: bg.fill ?? "#20232a"
        clip: true
        Image {
            anchors.fill: parent
            visible: !!parent.bg.texture
            source: parent.bg.texture ? "qrc:/textures/" + parent.bg.texture + ".xpm" : ""
            fillMode: Image.Tile
            opacity: 0.9
        }
        Repeater {
            model: parent.zones
            delegate: Rectangle {
                required property var modelData
                readonly property var r: modelData.rect ?? ({})
                readonly property string kind: modelData.kind ?? "button"
                readonly property var st: modelData.style && modelData.style.normal ? modelData.style.normal : ({})
                visible: kind !== "comment"
                x: (r.x ?? 0) * parent.k
                y: (r.y ?? 0) * parent.k
                width: (r.w ?? 0) * parent.k
                height: (r.h ?? 0) * parent.k
                radius: kind === "table" && modelData.shape === "circle" ? width / 2 : 3
                color: kind === "label" || kind === "image" ? "transparent"
                     : st.fill ?? (["button", "table"].includes(kind) ? "#4a5263" : "#2a303b")
                border.color: kind === "label" ? "transparent" : Qt.rgba(1, 1, 1, 0.15)
                Text {
                    anchors.fill: parent
                    anchors.margins: 2
                    visible: kind !== "image"
                    text: modelData.label ? modelData.label.replace("{store.name}", "Store") : ""
                    color: "white"
                    font.pixelSize: Math.max(6, Math.min(parent.height * 0.4, 11))
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            Label {
                Layout.fillWidth: true
                text: gal.choices.length ? qsTr("Layouts for %1").arg(gal.choices[0].pageName) : qsTr("Layouts")
                font.pixelSize: 22
                font.bold: true
            }
            Button { text: qsTr("Close"); onClicked: gal.close() }
        }
        Label {
            Layout.fillWidth: true
            visible: gal.choices.length === 0
            wrapMode: Text.WordWrap
            opacity: 0.75
            text: qsTr("There are no ready-made layouts for this page. Ready-made layouts come with the login, tables, order, pay and kitchen screens.")
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: gal.choices.length > 0
            clip: true
            Flow {
                width: gal.availableWidth
                spacing: 14
                Repeater {
                    model: gal.choices
                    delegate: Rectangle {
                        id: card
                        required property var modelData
                        objectName: "layout-" + modelData.id
                        width: 360
                        height: 290
                        radius: 10
                        color: EditorStyle.panel
                        border.color: pick.containsMouse ? EditorStyle.accent : EditorStyle.border
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6
                            Preview {
                                Layout.preferredWidth: 340
                                Layout.preferredHeight: 191
                                zones: card.modelData.zones
                                bg: card.modelData.background ?? ({})
                            }
                            Label { text: qsTranslate("Layouts", card.modelData.name); font.bold: true; font.pixelSize: 16 }
                            Label {
                                Layout.fillWidth: true
                                text: qsTranslate("Layouts", card.modelData.description)
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                                opacity: 0.75
                                font.pixelSize: 13
                            }
                        }
                        MouseArea {
                            id: pick
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: gal.editor.useArrangement(card.modelData.id)
                        }
                    }
                }
            }
        }

        // Page files: someone else's design, or a copy of yours.
        Label { text: qsTr("This page"); font.bold: true; font.pixelSize: 15 }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: 8
            Button {
                objectName: "useFileHere"
                text: qsTr("Use a Page File for This Page…")
                onClicked: files.run("importPageHere")
            }
            Button { text: qsTr("Add a Page from a File…"); onClicked: files.run("importPage") }
            Button { text: qsTr("Export This Page…"); onClicked: files.run("exportPage") }
        }
        Label { text: qsTr("The whole restaurant (every page)"); font.bold: true; font.pixelSize: 15 }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: 8
            Button { text: qsTr("Export Every Page…"); onClicked: files.run("exportLayout") }
            Button {
                objectName: "replaceAll"
                text: qsTr("Replace Every Page from a File…")
                onClicked: confirm.open()
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.7
                font.pixelSize: 12
                text: qsTr("Changes are a draft: Undo takes them back, Save keeps them.")
            }
        }
    }

    Dialog {
        id: confirm
        anchors.centerIn: parent
        modal: true
        title: qsTr("Replace every page?")
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAboutToShow: DialogWords.apply(this)
        Label {
            width: 420
            wrapMode: Text.WordWrap
            text: qsTr("All of this restaurant's pages are replaced by the ones in the file. Until you Save, Undo brings them back.")
        }
        onAccepted: files.run("importLayout")
    }

    FileDialog {
        id: files
        property string action
        function run(what) {
            action = what
            const saving = what.startsWith("export")
            const page = what !== "exportLayout" && what !== "importLayout"
            fileMode = saving ? FileDialog.SaveFile : FileDialog.OpenFile
            nameFilters = page ? [qsTr("ViewTouch page (*.vtpage.json)"), qsTr("JSON (*.json)")]
                               : [qsTr("ViewTouch layout (*.vtlayout.json)"), qsTr("JSON (*.json)")]
            defaultSuffix = page ? "vtpage.json" : "vtlayout.json"
            selectedFile = saving ? (page ? gal.editor.pageId + ".vtpage.json" : "restaurant.vtlayout.json") : ""
            open()
        }
        onAccepted: gal.editor[action](selectedFile)
    }
}
