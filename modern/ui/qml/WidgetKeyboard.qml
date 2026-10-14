import QtQuick
import QtQuick.Layouts

// On-screen keyboard for notes; types into the POS text entry.
// props.placeholder: hint shown while nothing is typed.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    property bool shifted: true   // first letter capitalized

    readonly property var rows: [
        ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"],
        ["q", "w", "e", "r", "t", "y", "u", "i", "o", "p"],
        ["a", "s", "d", "f", "g", "h", "j", "k", "l", "'"],
        ["shift", "z", "x", "c", "v", "b", "n", "m", ",", "back"],
        ["clear", "space", "."],
    ]

    function press(k) {
        if (k === "shift") { shifted = !shifted; return }
        if (k === "back" || k === "clear" || k === "space") {
            pos.textKey(k)
        } else {
            pos.textKey(shifted ? k.toUpperCase() : k)
            shifted = false
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: w.height * 0.03
        spacing: w.height * 0.02

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: w.height * 0.14
            radius: 8
            color: "#10141a"
            Text {
                anchors.fill: parent
                anchors.leftMargin: 16
                verticalAlignment: Text.AlignVCenter
                text: w.pos && w.pos.textEntry !== "" ? w.pos.textEntry + "▏" : (w.zone && w.zone.props && w.zone.props.placeholder ? qsTranslate("Page", w.zone.props.placeholder) : qsTr("Type a note…"))
                color: w.pos && w.pos.textEntry !== "" ? "white" : "#8a94a6"
                font.family: w.zone.st.font ?? "DejaVu Sans"
                font.pixelSize: parent.height * 0.45
                elide: Text.ElideLeft
            }
        }

        Repeater {
            model: w.rows
            delegate: RowLayout {
                id: row
                required property var modelData
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: w.height * 0.015
                Repeater {
                    model: row.modelData
                    delegate: WidgetKey {
                        required property string modelData
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: modelData === "space" ? 6 : (modelData.length > 1 ? 1.5 : 1)
                        text: modelData === "space" ? qsTr("space") : modelData === "back" ? "⌫"
                            : modelData === "shift" ? "⇧" : modelData === "clear" ? qsTr("Clear")
                            : (w.shifted ? modelData.toUpperCase() : modelData)
                        accent: modelData === "shift" && w.shifted
                        fontScale: modelData.length > 1 ? 0.3 : 0.42
                        onClicked: w.press(modelData)
                    }
                }
            }
        }
    }
}
