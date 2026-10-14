import QtQuick
import QtQuick.Layouts

// Clocking in with more than one job: "Which job today?" (each job has its
// own pay, and tip pools go by the job worked).
Rectangle {
    id: chooser
    property PosService pos
    readonly property var info: pos ? pos.clockInJobs : ({})
    visible: (info.who ?? "") !== ""
    color: "#cc0f1318"

    // Touches stop here, not on the page behind.
    MouseArea { anchors.fill: parent; hoverEnabled: true }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.9, 460)
        height: column.implicitHeight + 40
        radius: 14
        color: "#232933"
        border.color: "#2f6fd6"
        border.width: 2

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: 20
            spacing: 10
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Which job today, %1?").arg(chooser.info.who ?? "")
                color: "white"
                font.pixelSize: 24
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Repeater {
                model: chooser.info.jobs ?? []
                delegate: WidgetKey {
                    required property var modelData
                    objectName: "jobKey-" + modelData.role
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    text: modelData.name
                    fontScale: 0.32
                    baseColor: "#2f6fd6"
                    onClicked: chooser.pos.clockInAs(modelData.role)
                }
            }
            WidgetKey {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                text: qsTr("Cancel")
                fontScale: 0.3
                baseColor: "#6b2a2a"
                onClicked: chooser.pos.cancelClockIn()
            }
        }
    }
}
