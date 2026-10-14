import QtQuick
import QtQuick.Layouts

// Notes from the last shift's manager ("walk-in door sticks, 86 brisket
// tomorrow"): shown to each manager when they log in, until they touch Got It.
// They're left on the End of Day screen (Note for the Next Shift…).
Rectangle {
    id: sheet
    property PosService pos
    readonly property var notes: pos && pos.loggedIn ? pos.handoff : []
    visible: notes.length > 0
    color: "#cc0f1318"

    // Touches stop here, not on the page behind.
    MouseArea { anchors.fill: parent; hoverEnabled: true }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 24, 640)
        height: Math.min(parent.height - 24, column.implicitHeight + 40)
        radius: 14
        color: "#232933"
        border.color: "#2f6fd6"
        border.width: 2

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12
            Text {
                Layout.fillWidth: true
                text: qsTr("Notes from the last shift")
                color: "white"
                font.pixelSize: 24
                font.bold: true
            }
            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: list.implicitHeight
                contentHeight: list.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ColumnLayout {
                    id: list
                    width: parent.width
                    spacing: 12
                    Repeater {
                        model: sheet.notes
                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                Layout.fillWidth: true
                                text: modelData.from + "  ·  " + modelData.when
                                color: "#8a94a6"
                                font.pixelSize: 15
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.text
                                color: "white"
                                font.pixelSize: 20
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
            WidgetKey {
                objectName: "handoffRead"
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                text: qsTr("Got It")
                accent: true
                fontScale: 0.36
                takesTouch: true
                onClicked: sheet.pos.readHandoffNotes()
            }
        }
    }
}
