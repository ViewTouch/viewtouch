import QtQuick
import QtQuick.Layouts

// PIN entry: masked dots and a keypad. Enter logs in.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: w.height * 0.04
        spacing: w.height * 0.03

        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: w.zone.label || qsTr("Enter your PIN")
            color: w.zone.st.textColor ?? "white"
            font.family: w.zone.st.font ?? "DejaVu Sans"
            font.pixelSize: w.height * 0.05
        }

        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: w.height * 0.025
            Repeater {
                model: Math.max(4, w.pos ? w.pos.pinLength : 0)
                delegate: Rectangle {
                    required property int index
                    width: w.height * 0.045
                    height: width
                    radius: width / 2
                    color: w.pos && index < w.pos.pinLength ? (w.zone.st.textColor ?? "white") : "transparent"
                    border.color: w.zone.st.textColor ?? "white"
                    border.width: 2
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 3
            rowSpacing: w.height * 0.02
            columnSpacing: w.height * 0.02

            Repeater {
                model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "clear", "0", "enter"]
                delegate: WidgetKey {
                    required property string modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: modelData === "clear" ? qsTr("Clear") : modelData === "enter" ? qsTr("Enter") : modelData
                    accent: modelData === "enter"
                    fontScale: modelData.length > 1 ? 0.3 : 0.42
                    textColor: w.zone.st.textColor ?? "white"
                    fontFamily: w.zone.st.font ?? "DejaVu Sans"
                    onClicked: modelData === "enter" ? w.zone.controller.login() : w.pos.pinKey(modelData)
                }
            }
        }
    }
}
