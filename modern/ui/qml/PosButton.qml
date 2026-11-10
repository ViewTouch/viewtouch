import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// A button on the app's own screens, in the look of its keys: flat, bold,
// a solid color (palette.button; blue when highlighted or checked, lit while
// pressed, dimmed when it can't be used). Everything else as a Button.
Button {
    id: b
    font.bold: true
    // How many lines its words may take (a tile: "Combo de Hamburguesa" in two).
    property int lines: 1
    // A finger, 48 pixels on the screen, in its own units: the page and the
    // forms on it are scaled (a 1024x600 screen shows the page at half size),
    // so a button 48 tall where it's made can be 25 on the screen.
    readonly property real shownScale: {
        let f = 1
        for (let p = b.parent; p; p = p.parent)
            f *= p.scale
        return f > 0 ? f : 1
    }
    readonly property real finger: Math.ceil(48 / shownScale)
    Layout.minimumHeight: finger
    Layout.minimumWidth: finger
    readonly property color fill: !b.enabled ? Qt.darker(b.palette.button, 1.35)
                                  : b.down ? Qt.lighter(b.highlighted || b.checked ? Ui.accent : b.palette.button, 1.35)
                                  : b.highlighted || b.checked ? Ui.accent : b.palette.button
    contentItem: Text {
        textFormat: Text.PlainText
        text: b.text
        font: b.font
        color: b.enabled ? (b.highlighted || b.checked ? "white" : b.palette.buttonText) : Qt.rgba(1, 1, 1, 0.4)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        wrapMode: b.lines > 1 ? Text.WordWrap : Text.NoWrap
        maximumLineCount: b.lines > 1 ? b.lines : 999   // (one: its own lines, "\n", all shown)
        fontSizeMode: b.lines > 1 ? Text.Fit : Text.HorizontalFit
        // (A font sized in points shrinks by minimumPointSize: without it, it didn't.)
        minimumPixelSize: Math.max(9, b.font.pixelSize * 0.6)
        minimumPointSize: Math.max(7, b.font.pointSize * 0.6)
    }
    background: Rectangle {
        implicitWidth: Math.max(64, b.finger)
        implicitHeight: b.finger
        radius: 6
        color: b.fill
        border.color: b.visualFocus ? "#4c8dff" : Qt.darker(b.fill, 1.45)
        border.width: b.visualFocus ? 2 : 1
    }
}
