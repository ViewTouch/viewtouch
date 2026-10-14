import QtQuick
import QtQuick.Layouts

// Just clocked out: "Cash tips you kept today?" (PosSettings::declareCashTips).
// They count with card tips in the Tips report and the tip pool.
Rectangle {
    id: prompt
    property PosService pos
    readonly property var info: pos ? pos.cashTipsAsk : ({})
    visible: (info.who ?? "") !== ""
    color: "#cc0f1318"
    property string typed: ""
    onVisibleChanged: typed = ""

    function key(k) {
        if (k === "⌫") typed = typed.slice(0, -1)
        else if (k === "." && typed.includes(".")) return
        else if (typed.includes(".") && typed.split(".")[1].length >= 2) return
        else if (typed.length < 8) typed += k
    }

    // Touches stop here, not on the page behind.
    MouseArea { anchors.fill: parent; hoverEnabled: true }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.9, 420)
        height: column.implicitHeight + 40
        radius: 14
        color: "#232933"
        border.color: "#1f8a4c"
        border.width: 2

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: 20
            spacing: 10
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Cash tips you kept today, %1?").arg(prompt.info.who ?? "")
                color: "white"
                font.pixelSize: 22
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                radius: 8
                color: "#14171c"
                Text {
                    objectName: "cashTipsAmount"
                    anchors.centerIn: parent
                    text: (prompt.pos ? prompt.pos.currencySymbol : "$") + (prompt.typed || "0")
                    color: "white"
                    font.pixelSize: 30
                    font.bold: true
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 3
                rowSpacing: 6
                columnSpacing: 6
                Repeater {
                    model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "⌫"]
                    delegate: WidgetKey {
                        required property string modelData
                        objectName: "cashTipsKey-" + modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 60
                        text: modelData
                        fontScale: 0.4
                        onClicked: prompt.key(modelData)
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                WidgetKey {
                    objectName: "cashTipsNone"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    text: qsTr("None")
                    fontScale: 0.3
                    onClicked: prompt.pos.declareCashTips("0")
                }
                WidgetKey {
                    objectName: "cashTipsSave"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    enabled: prompt.typed !== ""
                    opacity: enabled ? 1 : 0.5
                    text: qsTr("Save")
                    fontScale: 0.3
                    baseColor: "#1f6b40"
                    onClicked: prompt.pos.declareCashTips(prompt.typed)
                }
            }
        }
    }
}
