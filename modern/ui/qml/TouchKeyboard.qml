import QtQuick
import QtQuick.Layouts

// The on-screen keyboard for text fields (customer names, Manager forms...)
// on touch screens without a keyboard. It types into `target` - the focused
// TextField / TextArea - without taking the focus, and shows a number pad
// for number fields.
Rectangle {
    id: kb
    // A TextInput (TextField) or a TextEdit (TextArea).
    property var target: null
    signal dismissed()

    readonly property bool numeric: target !== null && (target.inputMethodHints
        & (Qt.ImhDigitsOnly | Qt.ImhFormattedNumbersOnly | Qt.ImhDialableCharactersOnly)) !== 0
    property bool shifted: false
    property bool symbols: false

    readonly property var letters: [
        ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"],
        ["q", "w", "e", "r", "t", "y", "u", "i", "o", "p"],
        ["a", "s", "d", "f", "g", "h", "j", "k", "l", "'"],
        ["shift", "z", "x", "c", "v", "b", "n", "m", "back"],
        ["?123", "@", "space", ".", "-", "enter", "hide"],
    ]
    readonly property var marks: [
        ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"],
        ["!", "#", "$", "%", "&", "*", "(", ")", "/", "+"],
        [":", ";", "\"", ",", "?", "_", "=", "<", ">", "'"],
        ["abc", "~", "[", "]", "{", "}", "\\", "|", "back"],
        ["abc", "@", "space", ".", "-", "enter", "hide"],
    ]
    readonly property var digits: [
        ["1", "2", "3"], ["4", "5", "6"], ["7", "8", "9"], [".", "0", "back"], ["-", "enter", "hide"],
    ]
    // A phone held upright: a bottom row with room for fingers (@ and - are
    // on the symbols page).
    readonly property bool narrow: width < 600
    readonly property var narrowLetters: letters.slice(0, 4).concat([["?123", ",", "space", ".", "enter", "hide"]])
    readonly property var narrowMarks: [
        ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"],
        ["@", "#", "$", "%", "&", "*", "-", "+", "(", ")"],
        ["!", "\"", "'", ":", ";", "/", "?", "_", "="],
        ["~", "[", "]", "{", "}", "<", ">", "|", "back"],
        ["abc", ".", "space", "enter", "hide"],
    ]
    readonly property var rows: numeric ? digits : symbols ? (narrow ? narrowMarks : marks)
                                                           : (narrow ? narrowLetters : letters)

    color: "#12161c"
    implicitHeight: Math.min(parent ? parent.height * 0.42 : 360, 420)

    function type(text) {
        if (!target)
            return
        if (target.selectedText && target.selectedText.length > 0)
            target.remove(target.selectionStart, target.selectionEnd)
        target.insert(target.cursorPosition, text)
    }

    function press(k) {
        if (!target)
            return
        switch (k) {
        case "shift": shifted = !shifted; return
        case "?123": symbols = true; return
        case "abc": symbols = false; return
        case "hide": dismissed(); return
        case "space": type(" "); return
        case "back":
            if (target.selectedText && target.selectedText.length > 0)
                target.remove(target.selectionStart, target.selectionEnd)
            else if (target.cursorPosition > 0)
                target.remove(target.cursorPosition - 1, target.cursorPosition)
            return
        case "enter":
            // A one-line field is done (and saves, like Enter on a keyboard);
            // a TextArea gets a new line.
            if (target.hasOwnProperty("wrapMode") && target.hasOwnProperty("textDocument"))
                type("\n")
            else {
                if (target.accepted)
                    target.accepted()
                if (target.editingFinished)
                    target.editingFinished()
                dismissed()
            }
            return
        }
        type(shifted ? k.toUpperCase() : k)
        shifted = false
    }

    // Capital letter at the start of a field and after ". "
    onTargetChanged: {
        symbols = false
        shifted = target !== null && !numeric && (target.text ?? "").length === 0
    }

    // Touches here are the keyboard's: this takes them from MouseAreas
    // behind it (tap handlers there check TouchGuard). The keys take a tap
    // without taking the focus from the field.
    MouseArea { anchors.fill: parent }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        Repeater {
            model: kb.rows
            delegate: RowLayout {
                id: row
                required property var modelData
                Layout.fillWidth: !kb.numeric
                Layout.preferredWidth: kb.numeric ? Math.min(kb.width - 16, 560) : -1
                Layout.alignment: Qt.AlignHCenter
                Layout.fillHeight: true
                spacing: 6
                Repeater {
                    model: row.modelData
                    delegate: Rectangle {
                        id: key
                        required property string modelData
                        readonly property bool wide: modelData === "space"
                        readonly property bool special: ["shift", "back", "enter", "hide", "?123", "abc"].includes(modelData)
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.horizontalStretchFactor: wide ? 5 : special && !kb.numeric ? 2 : 1
                        // Widths in that proportion (the space bar widest).
                        Layout.preferredWidth: Layout.horizontalStretchFactor * 10
                        radius: 8
                        color: tap.pressed ? "#4a7bd8"
                             : modelData === "enter" ? "#1f5f3a"
                             : modelData === "shift" && kb.shifted ? "#4a5672"
                             : special ? "#2a313d" : "#343c49"
                        Text {
                            anchors.centerIn: parent
                            width: key.width - 6
                            horizontalAlignment: Text.AlignHCenter
                            text: ({ shift: "⇧", back: "⌫", enter: kb.numeric ? qsTr("Done") : "⏎", hide: "⌨▾",
                                     space: qsTr("space") })[key.modelData]
                                  ?? (kb.shifted ? key.modelData.toUpperCase() : key.modelData)
                            color: "white"
                            font.pixelSize: Math.max(14, Math.min(key.height * 0.42, 30))
                            // Never wider than its key ("?123" on a phone).
                            fontSizeMode: Text.HorizontalFit
                            minimumPixelSize: 9
                        }
                        // A TapHandler never takes the focus (the field keeps it), and
                        // every tap counts: "ll" is two letters, not a double-click.
                        TapHandler {
                            id: tap
                            longPressThreshold: 0.4
                            onTapped: kb.press(key.modelData)
                            onLongPressed: if (key.modelData === "back") repeat.start()
                            onPressedChanged: if (!pressed) repeat.stop()
                        }
                        Timer { id: repeat; interval: 70; repeat: true; onTriggered: kb.press("back") }
                    }
                }
            }
        }
    }
}
