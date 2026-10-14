import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// "When touched" list for the selected zone: ordered action cards whose
// fields come from the action schema. Every change rewrites the whole list
// (one undo step).
ColumnLayout {
    id: ale

    required property EditorController editor

    readonly property var actions: {
        if (editor.revision < 0) return []   // (compared, not just read: an unused read is compiled away and nothing refreshes)
        return editor.actions()
    }
    readonly property var types: editor.actionTypes()

    spacing: 8
    Layout.fillWidth: true

    function typeDef(type) {
        return types.find(t => t.type === type) ?? { type: type, label: type, fields: [] }
    }

    function copyActions() {
        return JSON.parse(JSON.stringify(actions))
    }

    function setKey(i, key, value) {
        const list = copyActions()
        if (value === undefined || value === "" || (Array.isArray(value) && value.length === 0))
            delete list[i][key]
        else
            list[i][key] = value
        editor.setActions(list)
    }

    function defaultsFor(type) {
        switch (type) {
        case "jump": return { type: type, mode: "push" }
        case "qualifier": return { type: type, qualifier: "no" }
        case "command": return { type: type, name: "sendOrder" }
        default: return { type: type }
        }
    }

    function setType(i, type) {
        const list = copyActions()
        list[i] = defaultsFor(type)
        editor.setActions(list)
    }

    function move(i, delta) {
        const list = copyActions()
        const j = i + delta
        if (j < 0 || j >= list.length) return
        const t = list[i]; list[i] = list[j]; list[j] = t
        editor.setActions(list)
    }

    function remove(i) {
        const list = copyActions()
        list.splice(i, 1)
        editor.setActions(list)
    }

    function add(type) {
        const list = copyActions()
        list.push(defaultsFor(type))
        editor.setActions(list)
    }

    Label {
        visible: ale.actions.length === 0
        text: qsTr("Nothing happens when this is touched.")
        color: EditorStyle.muted
        font.italic: true
    }

    Repeater {
        model: ale.actions.length

        delegate: Frame {
            id: card
            required property int index
            readonly property var action: ale.actions[index] ?? ({})
            readonly property var def: ale.typeDef(action.type)

            Layout.fillWidth: true
            padding: 8
            background: Rectangle {
                color: EditorStyle.panelRaised
                border.color: EditorStyle.border
                radius: 6
            }

            ColumnLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: (card.index + 1) + "."
                        color: EditorStyle.muted
                    }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ale.types
                        textRole: "label"
                        valueRole: "type"
                        currentIndex: Math.max(0, ale.types.findIndex(t => t.type === card.action.type))
                        onActivated: index => ale.setType(card.index, ale.types[index].type)
                    }
                    ToolButton { text: "▲"; enabled: card.index > 0; onClicked: ale.move(card.index, -1) }
                    ToolButton { text: "▼"; enabled: card.index < ale.actions.length - 1; onClicked: ale.move(card.index, 1) }
                    ToolButton {
                        text: "✕"
                        onClicked: ale.remove(card.index)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remove this action")
                    }
                }

                Repeater {
                    model: card.def.fields
                    delegate: FieldEditor {
                        required property var modelData
                        field: modelData
                        editor: ale.editor
                        value: card.action[modelData.path]
                        onCommit: v => ale.setKey(card.index, modelData.path, v)
                        onReset: ale.setKey(card.index, modelData.path, undefined)
                    }
                }
            }
        }
    }

    Button {
        text: qsTr("+ Add action")
        onClicked: addMenu.popup(0, height)   // under the button (a touch has no pointer)
        Menu {
            id: addMenu
            Repeater {
                model: ale.types
                delegate: MenuItem {
                    required property var modelData
                    text: modelData.label
                    onTriggered: ale.add(modelData.type)
                }
            }
        }
    }
}
