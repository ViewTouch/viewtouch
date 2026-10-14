import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Dialogs
import QtQuick.Layouts

// Menu Builder: Starting from a starter menu.
Dialog {
    id: templateDialog
    property Item menu   // the WidgetMenuBuilder (not by its type: that would be a cycle)
    objectName: "builderTemplateDialog"
    title: qsTr("Start from a starter menu")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent.width - 40, 760)
    modal: true
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.8
            text: qsTr("Its categories, choices and items, with typical prices, are added to your menu; nothing already on it changes. Then change anything you like.")
        }
        Repeater {
            model: menu.pos ? menu.pos.menuTemplates : []
            delegate: MenuTouchButton {
                required property var modelData
                objectName: "builderTemplate-" + modelData.id
                Layout.fillWidth: true
                implicitHeight: 72
                text: modelData.name + "  ·  " + qsTr("%1 categories, %2 items").arg(modelData.categories).arg(modelData.items)
                      + "\n" + modelData.description
                onClicked: {
                    menu.pos.applyMenuTemplate(modelData.id)
                    templateDialog.close()
                }
            }
        }
        MenuTouchButton { Layout.alignment: Qt.AlignRight; text: qsTr("Cancel"); onClicked: templateDialog.close() }
    }
}

// What's being dragged, under the finger.
