import QtQuick
import QtQuick.Controls.Fusion

// A button on the app's own screens, in the look of its keys: flat, bold,
// a solid color (palette.button; blue when highlighted or checked, lit while
// pressed, dimmed when it can't be used). Everything else as a Button.
Button {
    id: b
    font.bold: true
    leftPadding: 12
    rightPadding: 12
    readonly property color fill: !b.enabled ? Qt.darker(b.palette.button, 1.35)
                                  : b.down ? Qt.lighter(b.highlighted || b.checked ? "#2f6fd6" : b.palette.button, 1.35)
                                  : b.highlighted || b.checked ? "#2f6fd6" : b.palette.button
    contentItem: Text {
        text: b.text
        font: b.font
        color: b.enabled ? (b.highlighted || b.checked ? "white" : b.palette.buttonText) : Qt.rgba(1, 1, 1, 0.4)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: Math.max(9, b.font.pixelSize * 0.6)
    }
    background: Rectangle {
        implicitWidth: 64
        implicitHeight: 44
        radius: 6
        color: b.fill
        border.color: b.visualFocus ? "#4c8dff" : Qt.darker(b.fill, 1.45)
        border.width: b.visualFocus ? 2 : 1
    }
}
