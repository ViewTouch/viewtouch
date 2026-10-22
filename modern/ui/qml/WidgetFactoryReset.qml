import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Factory reset: what goes, that a backup is taken first, and typing RESET
// to be sure. ViewTouch then restarts with the starter set, or an empty
// store to set up from nothing.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    property bool empty: false

    readonly property real zoom: zone ? zone.formZoom(zone.narrow ? 380 : 1100) : 1
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 40, 760)
            spacing: 10

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Start over as a fresh install")
                font.pixelSize: 24
                font.bold: true
                color: "#ff9a9e"
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 16
                text: qsTr("This deletes every sale and past day, customers, gift cards, house accounts, the waitlist, "
                           + "the schedule, staff, the menu, inventory, settings, paired devices and your page changes. "
                           + "Then ViewTouch restarts with:")
            }
            GridLayout {
                Layout.fillWidth: true
                columns: w.zone && w.zone.narrow ? 1 : 2
                columnSpacing: 10
                rowSpacing: 10
                Repeater {
                    model: [
                        { empty: false, name: "resetStarter", title: qsTr("The sample café"),
                          text: qsTr("Its menu, tables, staff and settings, to try things out on.") },
                        { empty: true, name: "resetEmpty", title: qsTr("An empty store"),
                          text: qsTr("No menu, tables or sample staff. Log in with 1234: the setup guide has you add yourself.") }
                    ]
                    delegate: PosButton {
                        id: choice
                        required property var modelData
                        objectName: modelData.name
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        implicitHeight: Math.max(96, contentItem.implicitHeight + 24)
                        readonly property bool chosen: w.empty === modelData.empty
                        palette.button: chosen ? "#2b3a52" : "#2a303b"
                        palette.buttonText: "white"
                        onClicked: w.empty = modelData.empty
                        contentItem: ColumnLayout {
                            spacing: 4
                            Label {
                                Layout.fillWidth: true
                                text: (choice.chosen ? "✓ " : "") + modelData.title
                                font.pixelSize: 18
                                font.bold: true
                                color: "white"
                                wrapMode: Text.WordWrap
                            }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.text
                                font.pixelSize: 14
                                font.bold: false
                                color: "#c9d1dc"
                                wrapMode: Text.WordWrap
                            }
                        }
                        background: Rectangle {
                            radius: 8
                            color: choice.palette.button
                            border.width: 2
                            border.color: choice.chosen ? "#4c8dff" : "#3a414d"
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 16
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
            // Erasing the store is the maintainer's (the code set on its computer).
            Label {
                objectName: "resetWhose"
                visible: !(w.zone && w.zone.pos && w.zone.pos.can("maintain"))
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#f5b940"
                font.pixelSize: 18
                text: qsTr("Only the store's maintainer can erase it: log in with the maintainer's code.")
            }
            PosButton {
                Layout.fillWidth: true
                implicitHeight: 70
                font.pixelSize: 20
                enabled: confirm.text.trim() === "RESET" && !!w.zone && !!w.zone.pos && w.zone.pos.can("maintain")
                objectName: "resetGo"
                text: w.empty ? qsTr("Back Up and Start Empty") : qsTr("Back Up and Reset Everything")
                palette.button: enabled ? "#a42828" : "#3a3f48"
                palette.buttonText: "white"
                onClicked: w.pos.factoryReset(confirm.text, w.empty)
            }
        }
    }
}
