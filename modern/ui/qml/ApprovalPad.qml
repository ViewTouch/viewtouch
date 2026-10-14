import QtQuick
import QtQuick.Layouts

// "Manager approval": a void, a discount or a manager action someone may
// not do on their own. A manager types their PIN here and it goes through
// once (and the check's history says who approved it).
// The same pad asks for a manager's PIN to let the standby server take over
// (takingOver), when a screen has lost the main server.
Rectangle {
    id: pad
    property PosService pos
    readonly property var info: pos ? pos.approval : ({})
    property string pin: ""
    property bool takingOver: false
    property bool leavingKiosk: false   // a manager ends self-order mode on this screen
    readonly property bool asking: takingOver || leavingKiosk
    visible: asking || (info.needed ?? false)
    onVisibleChanged: pin = ""
    color: "#cc0f1318"

    // Touches stop here, not on the page behind (a TapHandler alone lets
    // them through to the buttons underneath).
    MouseArea { anchors.fill: parent; hoverEnabled: true }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.9, 420)
        height: column.implicitHeight + 40
        radius: 14
        color: "#232933"
        border.color: "#f5b940"
        border.width: 2

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: 20
            spacing: 10
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: pad.takingOver ? qsTr("Take over the store")
                    : pad.leavingKiosk ? qsTr("Staff only") : qsTr("Manager approval")
                color: "#f5b940"
                font.pixelSize: 24
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: pad.takingOver
                      ? qsTr("The main server isn't answering. The standby computer has a copy of everything and serves the store from now on.")
                      : pad.leavingKiosk ? qsTr("A manager's PIN ends self-order mode on this screen.")
                      : qsTr("%1 for %2").arg(pad.info.action ?? "").arg(pad.info.who ?? "")
                color: "white"
                font.pixelSize: 16
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: pad.pin.length ? "●".repeat(pad.pin.length) : qsTr("Manager PIN")
                color: pad.pin.length ? "white" : "#8a94a6"
                font.pixelSize: 26
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 3
                rowSpacing: 8
                columnSpacing: 8
                Repeater {
                    model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "Cancel", "0", "OK"]
                    delegate: WidgetKey {
                        required property string modelData
                        objectName: "approvalKey-" + modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 64
                        text: modelData === "Cancel" ? qsTr("Cancel") : modelData === "OK" ? qsTr("OK") : modelData
                        fontScale: modelData.length > 1 ? 0.3 : 0.45
                        baseColor: modelData === "OK" ? "#1f6b40" : modelData === "Cancel" ? "#6b2a2a" : "#343c49"
                        onClicked: {
                            if (modelData === "Cancel") {
                                if (pad.asking) {
                                    pad.takingOver = false
                                    pad.leavingKiosk = false
                                } else {
                                    pad.pos.cancelApproval()
                                }
                            } else if (modelData === "OK") {
                                const p = pad.pin
                                pad.pin = ""
                                if (pad.takingOver) {
                                    pad.takingOver = false
                                    pad.pos.takeOver(p)
                                } else if (pad.leavingKiosk) {
                                    pad.leavingKiosk = false
                                    pad.pos.leaveSelfOrder(p)
                                } else {
                                    pad.pos.approve(p)
                                }
                            } else if (pad.pin.length < 8) {
                                pad.pin += modelData
                            }
                        }
                    }
                }
            }
        }
    }
}
