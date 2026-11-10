import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// On-screen keyboard for notes; types into the POS text entry.
// props.placeholder: hint shown while nothing is typed.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    property bool shifted: true   // first letter capitalized

    readonly property var digitRow: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"]
    readonly property var letterRows: [
        ["q", "w", "e", "r", "t", "y", "u", "i", "o", "p"],
        ["a", "s", "d", "f", "g", "h", "j", "k", "l", "'"],
        ["shift", "z", "x", "c", "v", "b", "n", "m", ",", "back"],
    ]
    // Five rows each a finger tall (48 screen px) when there's room; else
    // four: the digits behind a 123 key, in the top row's place.
    readonly property bool compact: zone !== null
                                    && (w.height * 0.85 / 5) * zone.screenScale < 48
    property bool digits: false
    readonly property var rows: compact
        ? [digits ? digitRow : letterRows[0], letterRows[1], letterRows[2], [digits ? "abc" : "123", "clear", "space", "."]]
        : [digitRow].concat(letterRows).concat([["clear", "space", "."]])

    // On a phone: the phone's own keyboard (bigger keys, the language its
    // owner types in) into a field, instead of forty keys a few pixels wide.
    readonly property bool phone: zone !== null && zone.shownScreen === "phone"
    readonly property string hint: w.zone && w.zone.props && w.zone.props.placeholder
                                   ? qsTranslate("Page", w.zone.props.placeholder) : qsTr("Type a note…")
    // What the field holds, sent as the keys the store knows: removed
    // letters as back, added ones as typed.
    function typeTo(text) {
        if (!pos)
            return
        const was = pos.textEntry
        let same = 0
        while (same < was.length && same < text.length && was[same] === text[same])
            ++same
        for (let n = was.length - same; n > 0; --n)
            pos.textKey("back")
        for (const ch of text.slice(same))
            pos.textKey(ch === " " ? "space" : ch)
    }

    TextField {
        id: phoneField
        objectName: "noteField"
        visible: w.phone
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
        height: Math.max(48, w.height * 0.25)
        font.pixelSize: 18
        placeholderText: w.hint
        // Qt 6.12: a phone held sideways keeps the page in view, not a
        // full-screen box over it.
        inputMethodHints: Qt.ImhNoFullscreen
        onTextEdited: w.typeTo(text)
        // The store's text while not typing (a handheld waits for the
        // store: its answer mustn't take back what's being typed).
        Binding on text {
            when: !phoneField.activeFocus
            value: w.pos ? w.pos.textEntry : ""
        }
        onVisibleChanged: if (visible) forceActiveFocus()
        Component.onCompleted: if (visible) forceActiveFocus()
    }

    function press(k) {
        if (k === "shift") { shifted = !shifted; return }
        if (k === "123" || k === "abc") { digits = k === "123"; return }
        if (k === "back" || k === "clear" || k === "space") {
            pos.textKey(k)
        } else {
            pos.textKey(shifted ? k.toUpperCase() : k)
            shifted = false
        }
    }

    // The keys get the room: the typed line and the gaps are kept small, so
    // five rows of keys are each a finger tall (48 screen px) on a 1024x600
    // screen too, when the page gives the keyboard its height.
    ColumnLayout {
        visible: !w.phone
        anchors.fill: parent
        anchors.margins: w.height * 0.015
        spacing: w.height * 0.01

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: w.height * 0.11
            radius: 8
            color: "#10141a"
            Text {
                textFormat: Text.PlainText
                anchors.fill: parent
                anchors.leftMargin: 16
                verticalAlignment: Text.AlignVCenter
                text: w.pos && w.pos.textEntry !== "" ? w.pos.textEntry + "▏" : (w.zone && w.zone.props && w.zone.props.placeholder ? qsTranslate("Page", w.zone.props.placeholder) : qsTr("Type a note…"))
                color: w.pos && w.pos.textEntry !== "" ? "white" : Ui.muted
                font.family: w.zone.st.font ?? Ui.font
                font.pixelSize: parent.height * 0.45
                // What's typed: its end shows. The hint: smaller to fit (a
                // phone cut "Reason: price, change…" to "…ce, change…").
                readonly property bool typed: !!w.pos && w.pos.textEntry !== ""
                anchors.rightMargin: 16
                fontSizeMode: typed ? Text.FixedSize : Text.HorizontalFit
                minimumPixelSize: 10
                elide: typed ? Text.ElideLeft : Text.ElideRight
            }
        }

        Repeater {
            model: w.rows
            delegate: RowLayout {
                id: row
                required property var modelData
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Math.min(w.height * 0.012, w.zone ? w.zone.touch(4) : 6)   // (narrow: the keys keep the width)
                Repeater {
                    model: row.modelData
                    delegate: WidgetKey {
                        required property string modelData
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: modelData === "space" ? 6 : (modelData.length > 1 ? 1.5 : 1)
                        objectName: modelData === "123" || modelData === "abc" ? "keyboardDigits" : ""
                        text: modelData === "space" ? qsTr("space") : modelData === "back" ? "⌫"
                            : modelData === "shift" ? "⇧" : modelData === "clear" ? qsTr("Clear")
                            : (w.shifted ? modelData.toUpperCase() : modelData)
                        accent: modelData === "shift" && w.shifted
                        fontScale: modelData.length > 1 ? 0.3 : 0.42
                        onClicked: w.press(modelData)
                    }
                }
            }
        }
    }
}
