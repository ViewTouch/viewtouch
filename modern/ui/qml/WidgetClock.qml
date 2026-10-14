import QtQuick

// Current time and date.
Item {
    id: w
    property ZoneItem zone
    property date now: new Date()

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: w.now = new Date()
    }

    // The whole date when it fits at a good size, else a short one
    // ("sábado, 10 de octubre de 2026" -> "sáb 10 oct").
    readonly property string time: Qt.locale().toString(now, "h:mm AP")
    readonly property string longText: time + "   ·   " + now.toLocaleDateString(Qt.locale(), Locale.LongFormat)
    TextMetrics {
        id: longSize
        font.family: w.zone.st.font ?? "DejaVu Sans"
        font.pixelSize: w.height * 0.4
        text: w.longText
    }

    Text {
        anchors.centerIn: parent
        width: parent.width - 16
        horizontalAlignment: Text.AlignHCenter
        text: longSize.advanceWidth * 0.85 <= width ? w.longText
              : w.time + "   ·   " + w.now.toLocaleDateString(Qt.locale(), Qt.locale().name.startsWith("en") ? "ddd MMM d" : "ddd d MMM")
        color: w.zone.st.textColor ?? "white"
        font.family: w.zone.st.font ?? "DejaVu Sans"
        font.pixelSize: w.height * 0.4
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: 10
    }
}
