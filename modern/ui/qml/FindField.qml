import QtQuick
import QtQuick.Controls.Fusion

// Find…: Qt 6.10's SearchField (its magnifier, and an ✕ that empties it),
// with the ✕ a finger wide (the style's is 22 pixels). `edited` comes for
// what's typed and for the ✕ (text ""), so a screen reacts to both alike.
SearchField {
    id: f
    signal edited(string text)
    // What the phone's keyboard offers (SearchField itself has no such
    // property: it goes to the text inside).
    property int inputMethodHints: Qt.ImhNone

    onTextEdited: f.edited(f.text)
    onClearButtonPressed: {
        f.text = ""
        f.edited("")
    }
    Binding {
        target: f.contentItem
        property: "inputMethodHints"
        value: f.inputMethodHints
        when: f.contentItem !== null
    }
    clearIndicator.indicator: Item {
        objectName: "findClear"
        implicitWidth: 48
        implicitHeight: 48
        x: f.width - width
        height: f.height
        visible: f.text.length > 0
        Text {
            textFormat: Text.PlainText
            anchors.centerIn: parent
            text: "✕"
            font.pixelSize: 20
            color: f.palette.placeholderText
        }
    }
}
