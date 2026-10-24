import QtQuick
import QtQuick.Layouts

// Voiding an item already sent, or a discount or comp: why? One touch on a
// reason from the store's list (Store Settings -> Voids and comps) lets it
// go ahead; Cancel doesn't. The Exceptions report adds the reasons up.
Rectangle {
    id: sheet
    property PosService pos
    readonly property var info: pos ? pos.reasonAsk : ({})
    readonly property var reasons: info.reasons ?? []
    visible: (info.kind ?? "") !== ""
    color: "#cc0f1318"

    // Touches stop here, not on the page behind.
    MouseArea { anchors.fill: parent; hoverEnabled: true }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 24, 560)
        height: Math.min(parent.height - 24, column.implicitHeight + 40)
        radius: 14
        color: Ui.panel
        border.color: sheet.info.kind === "void" ? "#c0392b" : "#b7791f"
        border.width: 2

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: 20
            spacing: 10
            Text {
                Layout.fillWidth: true
                text: sheet.info.kind === "void" ? qsTr("Why take it off?") : qsTr("Why the discount?")
                color: "white"
                font.pixelSize: 24
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                text: sheet.info.what ?? ""
                color: Ui.textSoft
                font.pixelSize: 18
                wrapMode: Text.WordWrap
            }
            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: grid.implicitHeight
                contentHeight: grid.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                GridLayout {
                    id: grid
                    width: parent.width
                    columns: sheet.width > 480 ? 2 : 1
                    rowSpacing: 8
                    columnSpacing: 8
                    Repeater {
                        model: sheet.reasons
                        delegate: WidgetKey {
                            required property string modelData
                            required property int index
                            objectName: "reason-" + index
                            Layout.fillWidth: true
                            Layout.preferredHeight: 64
                            text: modelData
                            fontScale: 0.28
                            onClicked: sheet.pos.giveReason(modelData)
                        }
                    }
                }
            }
            WidgetKey {
                objectName: "reasonCancel"
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                text: qsTr("Cancel")
                fontScale: 0.3
                onClicked: sheet.pos.giveReason("")
            }
        }
    }
}
