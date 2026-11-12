import QtQuick
import QtQuick.Layouts

// Brought over from legacy ViewTouch with a PIN too short for their job (or
// none): after logging in once with it, they choose a new one, typed twice.
Rectangle {
    id: prompt
    property PosService pos
    visible: pos ? (pos.userPrefs.newPin ?? false) : false
    color: "#cc0f1318"
    property string typed: ""
    property string first: ""   // the first time it was typed
    readonly property bool manager: pos ? ["manager", "owner", "admin"].includes(pos.userRole ?? "") : false
    readonly property int least: manager ? 6 : 4
    onVisibleChanged: { typed = ""; first = "" }

    function key(k) {
        if (k === "⌫") typed = typed.slice(0, -1)
        else if (typed.length < 8) typed += k
    }
    function next() {
        if (first === "") {
            first = typed
            typed = ""
        } else if (typed !== first) {
            first = ""
            typed = ""
            mismatch.visible = true
        } else {
            pos.chooseNewPin(typed)
            typed = ""
            first = ""
        }
    }

    // Touches stop here, not on the page behind.
    MouseArea { anchors.fill: parent; hoverEnabled: true }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.9, 420)
        height: Math.min(parent.height * 0.96, column.implicitHeight + 40)
        radius: 14
        color: Ui.panel
        border.color: Ui.go
        border.width: 2

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: 20
            spacing: 10
            Text {
                textFormat: Text.PlainText
                objectName: "newPinTitle"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: prompt.first === "" ? qsTr("Choose your new PIN") : qsTr("Type it again")
                color: "white"
                font.pixelSize: 22
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: prompt.manager ? qsTr("6 to 8 digits: managers' PINs are longer, so they're harder to guess.")
                                     : qsTr("4 to 8 digits, only yours.")
                color: Ui.textSoft
                font.pixelSize: 15
                wrapMode: Text.WordWrap
            }
            Text {
                id: mismatch
                textFormat: Text.PlainText
                Layout.fillWidth: true
                visible: false
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("The two didn't match. Choose it again.")
                color: "#f0b429"
                font.pixelSize: 15
                wrapMode: Text.WordWrap
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                radius: 8
                color: Ui.deep
                Text {
                    textFormat: Text.PlainText
                    objectName: "newPinDots"
                    anchors.centerIn: parent
                    text: prompt.typed.length ? "●".repeat(prompt.typed.length) : " "
                    color: "white"
                    font.pixelSize: 30
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 3
                rowSpacing: 6
                columnSpacing: 6
                Repeater {
                    model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "", "0", "⌫"]
                    delegate: WidgetKey {
                        required property string modelData
                        objectName: "newPinKey-" + modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 60
                        visible: modelData !== ""
                        text: modelData
                        fontScale: 0.4
                        onClicked: { mismatch.visible = false; prompt.key(modelData) }
                    }
                }
            }
            WidgetKey {
                objectName: "newPinNext"
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                enabled: prompt.typed.length >= prompt.least
                opacity: enabled ? 1 : 0.5
                text: prompt.first === "" ? qsTr("Next") : qsTr("Save My PIN")
                fontScale: 0.3
                baseColor: Ui.goDark
                onClicked: prompt.next()
            }
        }
    }
}
