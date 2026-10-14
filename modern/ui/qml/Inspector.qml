import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Right-hand panel: properties of the selected zones, the page, or the theme.
// Everything shown comes from the field schema, so new properties need no
// changes here.
Rectangle {
    id: inspector

    required property LayoutController controller
    required property EditorController editor

    // "zone" | "page" | "theme"
    property string mode: editor.selection.length > 0 ? "zone" : "page"

    color: EditorStyle.panel

    Connections {
        target: inspector.editor
        function onSelectionChanged() {
            if (inspector.editor.selection.length > 0)
                inspector.mode = "zone"
            else if (inspector.mode === "zone")
                inspector.mode = "page"
        }
    }

    readonly property string zoneKind: {
        if (editor.revision < 0 || editor.selection.length === 0) return ""   // (compared, not just read: an unused read is compiled away and nothing refreshes)
        return editor.selectionKind !== "" ? editor.selectionKind
             : (editor.fieldInfo("zone", "kind").value ?? "button")
    }
    // As text: unchanged after an edit, so the rows below aren't rebuilt
    // (a field being typed in keeps its place).
    readonly property string fieldsJson: {
        if (editor.revision < 0) return "[]"
        if (mode === "zone") return JSON.stringify(editor.selection.length ? editor.zoneFields(zoneKind) : [])
        if (mode === "theme") return JSON.stringify(editor.themeFields())
        return JSON.stringify(editor.pageFields())
    }
    // [{ name, fields }] in schema order.
    readonly property var groups: {
        const out = []
        for (const f of JSON.parse(fieldsJson)) {
            let g = out.find(x => x.name === f.group)
            if (!g) { g = { name: f.group, key: f.groupKey ?? f.group, fields: [] }; out.push(g) }
            g.fields.push(f)
        }
        return out
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabs
            Layout.fillWidth: true
            currentIndex: ["zone", "page", "theme"].indexOf(inspector.mode)
            TabButton {
                text: inspector.editor.selection.length > 1
                      ? qsTr("%1 zones").arg(inspector.editor.selection.length) : qsTr("Zone")
                enabled: inspector.editor.selection.length > 0
                onClicked: inspector.mode = "zone"
            }
            TabButton { text: qsTr("Page"); onClicked: inspector.mode = "page" }
            TabButton { text: qsTr("Theme"); onClicked: inspector.mode = "theme" }
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: scroll.availableWidth
                spacing: 10

                Item { implicitHeight: 4 }

                // Kind switcher for zones
                ColumnLayout {
                    visible: inspector.mode === "zone"
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    Label { text: qsTr("Type"); color: EditorStyle.muted; font.pixelSize: 12 }
                    ComboBox {
                        Layout.fillWidth: true
                        readonly property var kinds: inspector.editor.basicKinds.concat(inspector.editor.widgetKinds)
                        model: kinds.map(k => inspector.editor.kindName(k))
                        currentIndex: inspector.editor.selectionKind === "" ? -1 : kinds.indexOf(inspector.editor.selectionKind)
                        displayText: inspector.editor.selectionKind === "" ? qsTr("(mixed)") : currentText
                        onActivated: index => inspector.editor.setField("zone", "kind", kinds[index])
                    }
                }

                // Show on: both kinds of screen, or one ("Show only when" -> Screen).
                RowLayout {
                    id: showOn
                    objectName: "showOn"
                    readonly property var info: inspector.editor.revision >= 0 && inspector.mode === "zone"
                                                ? inspector.editor.fieldInfo("zone", "showWhen.screen") : ({})
                    readonly property string value: info.mixed ? "mixed" : (info.value ?? "")
                    visible: inspector.mode === "zone" && inspector.editor.screens.canPhone === true
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 0
                    Label { text: qsTr("Show on"); color: EditorStyle.muted; font.pixelSize: 12; Layout.rightMargin: 8 }
                    Repeater {
                        model: [{ v: "", t: qsTr("Both") }, { v: "standard", t: qsTr("Standard") }, { v: "phone", t: qsTr("Phone") }]
                        delegate: Button {
                            required property var modelData
                            objectName: "showOn-" + (modelData.v || "both")
                            Layout.fillWidth: true
                            implicitHeight: 32
                            text: modelData.t
                            flat: showOn.value !== modelData.v
                            highlighted: showOn.value === modelData.v
                            onClicked: modelData.v === "" ? inspector.editor.clearField("zone", "showWhen.screen")
                                                          : inspector.editor.setField("zone", "showWhen.screen", modelData.v)
                        }
                    }
                }

                // On a phone version: the page's own buttons, shared.
                Rectangle {
                    id: shareBox
                    objectName: "phoneShareBox"
                    readonly property var share: inspector.editor.revision >= 0 && inspector.mode === "zone"
                                                 ? inspector.editor.phoneShare() : ({ shared: 0 })
                    readonly property var own: share.own ?? []
                    visible: share.shared > 0
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    implicitHeight: shareCol.implicitHeight + 16
                    radius: 4
                    color: EditorStyle.panelRaised
                    border.color: EditorStyle.accent
                    ColumnLayout {
                        id: shareCol
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: 12
                            text: qsTr("Shared with the standard page: words, colors and actions changed here change there too. Where it sits and its size are the phone's.")
                        }
                        CheckBox {
                            objectName: "ownLabel"
                            text: qsTr("Own words on phones")
                            checked: shareBox.own.includes("label")
                            onToggled: inspector.editor.setOwnOnPhone("label", checked)
                        }
                        CheckBox {
                            objectName: "ownFontSize"
                            text: qsTr("Own text size on phones")
                            checked: shareBox.own.includes("style.normal.fontSize")
                            onToggled: inspector.editor.setOwnOnPhone("style.normal.fontSize", checked)
                        }
                    }
                }

                Label {
                    visible: inspector.mode === "page"
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    text: qsTr("Nothing selected — these are the settings of this page.")
                    color: EditorStyle.muted
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                }

                Label {
                    visible: inspector.mode === "theme"
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    text: qsTr("The theme is the default look for every page. Pages and zones override it.")
                    color: EditorStyle.muted
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                }

                // Ready-made looks: one touch recolors the theme (Undo takes it back).
                Label {
                    visible: inspector.mode === "theme"
                    Layout.leftMargin: 12
                    text: qsTr("Looks")
                    font.bold: true
                    font.pixelSize: 14
                }
                Flow {
                    visible: inspector.mode === "theme"
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 8
                    Repeater {
                        model: inspector.mode === "theme" ? inspector.controller.looks() : []
                        delegate: Rectangle {
                            id: lookCard
                            required property var modelData
                            objectName: "look-" + modelData.id
                            width: 140
                            height: 74
                            radius: 8
                            color: modelData.colors[0]
                            border.color: lookTap.pressed ? modelData.colors[4] : EditorStyle.muted
                            border.width: lookTap.pressed ? 3 : 1
                            Row {
                                x: 8; y: 8
                                spacing: 4
                                Repeater {
                                    model: [1, 2, 4]
                                    delegate: Rectangle {
                                        required property int modelData
                                        width: 36; height: 24; radius: 4
                                        color: lookCard.modelData.colors[modelData]
                                        border.color: Qt.rgba(0.5, 0.5, 0.5, 0.5)
                                    }
                                }
                            }
                            Text {
                                x: 8
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 8
                                width: parent.width - 16
                                elide: Text.ElideRight
                                text: qsTranslate("Looks", lookCard.modelData.name)
                                color: lookCard.modelData.colors[3]
                                font.pixelSize: 12
                                font.bold: true
                            }
                            TapHandler { id: lookTap; onTapped: inspector.controller.applyLook(lookCard.modelData.id) }
                        }
                    }
                }

                Repeater {
                    model: inspector.groups

                    delegate: ColumnLayout {
                        id: group
                        required property var modelData
                        required property int index
                        // Secondary style states start collapsed.
                        property bool open: !modelData.key.startsWith("Look when")

                        Layout.fillWidth: true
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        spacing: 6

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: EditorStyle.border
                        }
                        ItemDelegate {
                            Layout.fillWidth: true
                            implicitHeight: 30
                            padding: 0
                            text: (group.open ? "▾  " : "▸  ") + group.modelData.name
                            font.bold: true
                            onClicked: group.open = !group.open
                        }

                        Repeater {
                            model: group.open ? group.modelData.fields : []
                            delegate: Loader {
                                id: row
                                required property var modelData
                                Layout.fillWidth: true
                                sourceComponent: modelData.type === "actions" ? actionsComponent : fieldComponent

                                Component {
                                    id: fieldComponent
                                    FieldEditor {
                                        readonly property var info: {
                                            if (inspector.editor.revision < 0) return ({})
                                            return inspector.editor.fieldInfo(inspector.mode, row.modelData.path)
                                        }
                                        field: row.modelData
                                        editor: inspector.editor
                                        pos: inspector.controller.pos
                                        value: info.value
                                        mixed: info.mixed ?? false
                                        isSet: info.isSet ?? false
                                        resolved: info.resolved
                                        onCommit: v => inspector.editor.setField(inspector.mode, row.modelData.path, v)
                                        onReset: inspector.editor.clearField(inspector.mode, row.modelData.path)
                                    }
                                }
                                Component {
                                    id: actionsComponent
                                    Loader {
                                        sourceComponent: inspector.editor.selection.length === 1 ? actionList : multiNote
                                        Component {
                                            id: actionList
                                            ActionListEditor { editor: inspector.editor }
                                        }
                                        Component {
                                            id: multiNote
                                            Label {
                                                text: qsTr("Select a single zone to edit what it does.")
                                                color: EditorStyle.muted
                                                wrapMode: Text.WordWrap
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Page: who points here, and layout problems
                ColumnLayout {
                    visible: inspector.mode === "page"
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 4

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: EditorStyle.border }
                    Label { text: qsTr("Used by"); font.bold: true }
                    Label {
                        visible: inspector.editor.references.length === 0
                        text: qsTr("No buttons lead here.")
                        color: EditorStyle.muted
                        font.italic: true
                    }
                    Repeater {
                        model: inspector.editor.references
                        delegate: Label {
                            required property string modelData
                            text: "• " + modelData
                            color: EditorStyle.muted
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }

                ColumnLayout {
                    visible: inspector.editor.issues.length > 0
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 4

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: EditorStyle.border }
                    Label {
                        text: qsTr("Problems (%1)").arg(inspector.editor.issues.length)
                        font.bold: true
                        color: EditorStyle.warning
                    }
                    Repeater {
                        model: inspector.editor.issues
                        delegate: Label {
                            required property string modelData
                            text: "• " + modelData
                            color: EditorStyle.warning
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            font.pixelSize: 12
                        }
                    }
                }

                Item { implicitHeight: 16 }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: EditorStyle.border
    }
}
