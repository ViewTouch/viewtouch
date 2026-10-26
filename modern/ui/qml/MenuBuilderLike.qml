import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Menu Builder: Like…: copying another item's settings onto the card.
Popup {
    id: likeDialog
    property Item menu   // the WidgetMenuBuilder (not by its type: that would be a cycle)
    objectName: "builderLikeDialog"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 560, 560)
    height: Math.min(parent ? parent.height - 32 : 640, 640)
    modal: true
    padding: 16
    property string find: ""
    onOpened: find = ""
    readonly property var choices: {
        const t = find.trim().toLowerCase()
        const others = menu.allItems.filter(i => i.id !== menu.draft.id && (t === "" || i.name.toLowerCase().includes(t)))
        return others.filter(i => i.family === menu.draft.family).concat(others.filter(i => i.family !== menu.draft.family))
    }
    contentItem: ColumnLayout {
        spacing: 8
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Like which item? Its choices, what's on it, allergens, kitchen, tax and color are copied here; then change anything.")
            font.pixelSize: 15
        }
        TextField {
            objectName: "builderLikeFind"
            Layout.fillWidth: true
            implicitHeight: 48
            font.pixelSize: 16
            placeholderText: qsTr("🔍 Find an item")
            text: likeDialog.find
            onTextEdited: likeDialog.find = text
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: likeDialog.choices
            delegate: MenuTouchButton {
                required property var modelData
                objectName: "builderLike-" + modelData.id
                width: ListView.view.width
                readonly property var cat: menu.categories.find(c => c.id === modelData.family)
                text: modelData.name + (cat ? "  ·  " + cat.name : "")
                      + (modelData.groups.length ? "  ·  " + qsTr("%n choice(s)", "", modelData.groups.length) : "")
                onClicked: {
                    const i = modelData, b = menu
                    likeDialog.close()
                    b.copyFrom(i)
                }
            }
        }
        MenuTouchButton { Layout.fillWidth: true; text: qsTr("Cancel"); onClicked: likeDialog.close() }
    }
}

// 🎨: every category recolored from a theme, shown first.
// A menu to print or hand out: today's prices, laid out on pages.
