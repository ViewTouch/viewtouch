import QtQuick
import QtQuick.Layouts

// PIN entry: masked dots and a keypad. Enter logs in.
// props.keys false: the dots only (Legacy's login, whose keys are buttons of their own).
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property bool keys: zone?.props?.keys ?? true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: w.height * 0.04
        spacing: w.height * 0.03

        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            Layout.alignment: w.keys ? 0 : Qt.AlignBottom
            horizontalAlignment: Text.AlignHCenter
            text: w.zone.label || qsTr("Enter your PIN")
            color: w.zone.st.textColor ?? "white"
            font.family: w.zone.st.font ?? Ui.font
            font.pixelSize: w.height * (w.keys ? 0.05 : 0.16)
        }

        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: w.height * 0.025
            Repeater {
                model: Math.max(4, w.pos ? w.pos.pinLength : 0)
                delegate: Rectangle {
                    required property int index
                    width: w.height * (w.keys ? 0.045 : 0.14)
                    height: width
                    radius: width / 2
                    color: w.pos && index < w.pos.pinLength ? (w.zone.st.textColor ?? "white") : "transparent"
                    border.color: w.zone.st.textColor ?? "white"
                    border.width: 2
                }
            }
        }

        Item {   // (no keys: the dots at the middle)
            visible: !w.keys
            Layout.fillHeight: true
        }

        GridLayout {
            visible: w.keys
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
                    fontFamily: w.zone.st.font ?? Ui.font
                    onClicked: modelData === "enter" ? w.zone.controller.login() : w.pos.pinKey(modelData)
                }
            }
        }
    }
}
