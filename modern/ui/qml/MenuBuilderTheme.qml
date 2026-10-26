import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Menu Builder: Colors for every category at once.
Popup {
    id: themeDialog
    property Item menu   // the WidgetMenuBuilder (not by its type: that would be a cycle)
    objectName: "builderThemeDialog"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 720, 720)
    height: Math.min(parent ? parent.height - 32 : 640, 640)
    modal: true
    padding: 16
    property string theme: ""
    onOpened: theme = ""
    readonly property var chosen: StoreColors.themes.find(t => t.id === theme) ?? null
    function colorFor(index) { return chosen ? chosen.colors[index % chosen.colors.length] : menu.categories[index].color }
    contentItem: ColumnLayout {
        spacing: 10
        Label { text: qsTr("Colors for every category"); font.pixelSize: 20; font.bold: true }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: StoreColors.themes
                delegate: Rectangle {
                    required property var modelData
                    objectName: "builderTheme-" + modelData.id
                    width: 150
                    height: 76
                    radius: 8
                    color: "#232933"
                    border.color: themeDialog.theme === modelData.id ? "#f5b940" : "#3a424f"
                    border.width: themeDialog.theme === modelData.id ? 3 : 1
                    Column {
                        anchors.centerIn: parent
                        spacing: 6
                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.name; font.pixelSize: 15; font.bold: true }
                        Row {
                            spacing: 3
                            Repeater {
                                model: modelData.colors.slice(0, 6)
                                delegate: Rectangle { required property string modelData; width: 18; height: 18; radius: 3; color: modelData }
                            }
                        }
                    }
                    MouseArea { anchors.fill: parent; onClicked: themeDialog.theme = modelData.id }
                }
            }
        }
        // The categories as they'd be.
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: menu.categories
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 40
                radius: 6
                color: themeDialog.colorFor(index) || "#4a5260"
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    x: 12
                    text: modelData.name
                    color: StoreColors.ink(parent.color.toString())
                    font.pixelSize: 16
                    font.bold: true
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            MenuTouchButton {
                objectName: "builderThemeUse"
                Layout.fillWidth: true
                highlighted: true
                enabled: !!themeDialog.chosen
                text: qsTr("Use These Colors")
                onClicked: {
                    const colors = {}
                    menu.categories.forEach((c, i) => colors[c.id] = themeDialog.colorFor(i))
                    themeDialog.close()
                    menu.pos.setCategoryColors(colors)
                }
            }
            MenuTouchButton { Layout.preferredWidth: 160; text: qsTr("Cancel"); onClicked: themeDialog.close() }
        }
    }
}

// Sizes…: a name and a whole price each; the item's price becomes the smallest.
