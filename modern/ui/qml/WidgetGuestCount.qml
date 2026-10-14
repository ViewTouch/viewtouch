import QtQuick
import QtQuick.Layouts

// Guest count with − / + (the number pad on the page types it directly).
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null

    RowLayout {
        anchors.fill: parent
        anchors.margins: w.height * 0.1
        spacing: w.width * 0.04

        WidgetKey {
            Layout.preferredWidth: w.height * 0.8
            Layout.fillHeight: true
            visible: w.zone.keyShown("fewer")
            text: w.zone.keyText("fewer", "−")
            textColor: w.zone.st.textColor ?? "white"
            onClicked: w.pos.adjustGuests(-1)
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: w.pos ? w.pos.entryGuests : 1
                color: w.zone.st.textColor ?? "white"
                font.family: w.zone.st.font ?? "DejaVu Sans"
                font.pixelSize: w.height * 0.45
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: w.pos && w.pos.pendingTable !== "" ? qsTr("guests at %1").arg(w.pos.pendingTable) : qsTr("guests")
                color: "#8a94a6"
                font.family: w.zone.st.font ?? "DejaVu Sans"
                font.pixelSize: w.height * 0.13
            }
        }
        WidgetKey {
            Layout.preferredWidth: w.height * 0.8
            Layout.fillHeight: true
            visible: w.zone.keyShown("more")
            text: w.zone.keyText("more", "+")
            textColor: w.zone.st.textColor ?? "white"
            onClicked: w.pos.adjustGuests(1)
        }
    }
}
