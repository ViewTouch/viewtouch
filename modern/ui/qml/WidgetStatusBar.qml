import QtQuick

// Last status message (the same text the toast shows).
Item {
    id: w
    property ZoneItem zone

    Text {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        verticalAlignment: Text.AlignVCenter
        text: w.zone.controller ? w.zone.controller.statusText : ""
        color: w.zone.st.textColor ?? "white"
        font.family: w.zone.st.font ?? "DejaVu Sans"
        font.pixelSize: w.height * 0.4
        elide: Text.ElideRight
    }
}
