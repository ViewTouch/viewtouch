import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Factory reset: what goes, that a backup is taken first, and typing RESET
// to be sure. ViewTouch then restarts with the starter set.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null

    readonly property real zoom: zone ? zone.formZoom(zone.narrow ? 380 : 1100) : 1
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 40, 760)
            spacing: 14

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Start over as a fresh install")
                font.pixelSize: 28
                font.bold: true
                color: "#ff9a9e"
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 17
                text: qsTr("This deletes every sale and past day, customers, gift cards, house accounts, the waitlist, "
                           + "the schedule, staff, the menu, inventory, settings, paired devices and your page changes. "
                           + "ViewTouch then restarts with the starter pages, menu, staff and settings.")
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 17
                color: "#7ee2a8"
                text: qsTr("A backup is taken first (backups/…-before-reset.db), so it can be undone with --restore. "
                           + "Earlier backups and saved report files are kept.")
            }
            Label { text: qsTr("Type RESET to confirm"); font.pixelSize: 17; Layout.topMargin: 10 }
            TextField {
                implicitHeight: 56
                id: confirm
                Layout.fillWidth: true
                font.pixelSize: 24
                placeholderText: "RESET"
                inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
            }
            Button {
                Layout.fillWidth: true
                implicitHeight: 70
                font.pixelSize: 20
                enabled: confirm.text.trim() === "RESET"
                text: qsTr("Back Up and Reset Everything")
                palette.button: enabled ? "#a42828" : "#3a3f48"
                palette.buttonText: "white"
                onClicked: w.pos.factoryReset(confirm.text)
            }
        }
    }
}
