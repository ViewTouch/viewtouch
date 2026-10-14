import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Dialogs
import QtQuick.Layouts

// Menu Builder: Before service: what the menu is missing.
Dialog {
    id: checkDialog
    property Item menu   // the WidgetMenuBuilder (not by its type: that would be a cycle)
    objectName: "builderCheckDialog"
    readonly property var problems: menu.pos ? menu.pos.menuProblems : []
    title: problems.length ? qsTr("Before service") : qsTr("Ready to go")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent.width - 40, 760)
    height: Math.min(parent.height - 40, 600)
    modal: true
    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        Label {
            visible: checkDialog.problems.length === 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Every item has a price and a category, every choice it asks for is there, and its kitchen ticket goes to a printer that's set up.")
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: checkDialog.problems
            delegate: MenuTouchButton {
                required property var modelData
                width: ListView.view.width
                text: (modelData.serious ? "⚠ " : "• ") + modelData.text
                onClicked: {
                    checkDialog.close()
                    const problem = modelData, b = menu
                    b.leave(() => b.openProblem(problem))
                }
            }
        }
        MenuTouchButton { Layout.alignment: Qt.AlignRight; text: qsTr("Close"); onClicked: checkDialog.close() }
    }
}

// Starter menus: a kind of place's categories, choices and items.
