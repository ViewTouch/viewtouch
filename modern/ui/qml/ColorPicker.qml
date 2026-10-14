import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// A color by touch: the store's colors (ten, dark to light), the colors the
// store mixed itself, or Mix… for any other. picked(c) on a touch; with
// noneText, a button for no color of its own (picked("")).
Item {
    id: cp
    // As wide as it's given (ten small swatches at least), as tall as it needs.
    implicitWidth: 10 * 28 + 9 * gap
    implicitHeight: body.implicitHeight
    property string color: ""
    property string noneText: ""
    property PosService pos
    // Each swatch: as big as fits ten across, up to a finger's size.
    readonly property real size: Math.max(28, Math.min(52, (width - 9 * gap) / 10))
    readonly property real gap: 5
    readonly property var own: pos ? pos.customColors : []
    signal picked(string color)

    component Swatch: Rectangle {
        required property string swatch
        width: cp.size
        height: cp.size
        radius: 6
        color: swatch
        border.color: StoreColors.same(cp.color, swatch) ? "white" : Qt.darker(swatch, 1.4)
        border.width: StoreColors.same(cp.color, swatch) ? 3 : 1
        Text {
            anchors.centerIn: parent
            visible: StoreColors.same(cp.color, parent.swatch)
            text: "✓"
            color: StoreColors.ink(parent.swatch)
            font.pixelSize: cp.size * 0.45
            font.bold: true
        }
        MouseArea { anchors.fill: parent; onClicked: cp.picked(parent.swatch) }
    }

    ColumnLayout {
        id: body
        width: cp.width
        spacing: 8
        Grid {
            objectName: "colorPalette"
            columns: 10
            spacing: cp.gap
            Repeater {
                model: StoreColors.all
                delegate: Swatch {
                    required property string modelData
                    required property int index
                    objectName: "color-" + index
                    swatch: modelData
                }
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: cp.gap
            Button {
                objectName: "colorNone"
                visible: cp.noneText !== ""
                implicitHeight: cp.size
                font.pixelSize: 15
                checkable: true
                checked: cp.color === ""
                highlighted: checked
                text: cp.noneText
                onClicked: cp.picked("")
            }
            // The store's own, newest first.
            Repeater {
                model: cp.own
                delegate: Swatch {
                    required property string modelData
                    swatch: modelData
                }
            }
            // The color now, when it's none of these (typed, or from before).
            Swatch {
                visible: StoreColors.valid(cp.color) && !StoreColors.all.some(c => StoreColors.same(c, cp.color))
                         && !cp.own.some(c => StoreColors.same(c, cp.color))
                swatch: StoreColors.valid(cp.color) ? cp.color : "#000000"
            }
            Button {
                objectName: "colorMix"
                implicitHeight: cp.size
                font.pixelSize: 15
                text: qsTr("Mix…")
                onClicked: mixer.openWith(cp.color)
            }
        }

    }

    // Any color: its hue, how light, how strong; the store keeps it.
    Popup {
        id: mixer
        objectName: "colorMixer"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(parent ? parent.width - 32 : 520, 520)
        modal: true
        padding: 18
        property real hue: 0
        property real light: 0.45
        property real strength: 0.6
        readonly property color mixed: Qt.hsla(hue, strength, light, 1)
        readonly property string hex: mixed.toString()
        function openWith(c) {
            if (StoreColors.valid(c)) {
                const k = Qt.color(c)
                hue = Math.max(0, k.hslHue)
                light = k.hslLightness
                strength = k.hslSaturation
            }
            open()
        }
        function take(text) {
            if (!StoreColors.valid(text)) return
            const k = Qt.color(text.trim())
            hue = Math.max(0, k.hslHue)
            light = k.hslLightness
            strength = k.hslSaturation
        }

        // A strip to touch or drag along: its colors under it.
        component Strip: Item {
            id: strip
            property real value: 0
            property Gradient fill
            signal moved(real value)
            Layout.fillWidth: true
            implicitHeight: 52
            Rectangle {
                anchors.fill: parent
                anchors.margins: 6
                radius: 8
                border.color: "#5a6270"
                gradient: strip.fill
            }
            Rectangle {
                x: Math.max(0, Math.min(strip.width - width, strip.value * strip.width - width / 2))
                width: 14
                height: strip.height
                radius: 5
                color: "transparent"
                border.color: "white"
                border.width: 3
            }
            MouseArea {
                anchors.fill: parent
                preventStealing: true
                function at(m) { strip.moved(Math.max(0, Math.min(1, m.x / width))) }
                onPressed: m => at(m)
                onPositionChanged: m => at(m)
            }
        }

        contentItem: ColumnLayout {
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Rectangle {
                    objectName: "colorMixed"
                    Layout.fillWidth: true
                    implicitHeight: 72
                    radius: 10
                    color: mixer.mixed
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Tacos")
                        color: StoreColors.ink(mixer.hex)
                        font.pixelSize: 24
                        font.bold: true
                    }
                }
                TextField {
                    objectName: "colorHex"
                    Layout.preferredWidth: 130
                    implicitHeight: 52
                    font.pixelSize: 18
                    text: mixer.hex
                    onEditingFinished: mixer.take(text)
                }
            }
            Label { text: qsTr("Color"); opacity: 0.75 }
            Strip {
                objectName: "colorHue"
                value: mixer.hue
                fill: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.hsla(0.0, 0.75, 0.5, 1) }
                    GradientStop { position: 0.17; color: Qt.hsla(0.17, 0.75, 0.5, 1) }
                    GradientStop { position: 0.33; color: Qt.hsla(0.33, 0.75, 0.5, 1) }
                    GradientStop { position: 0.5; color: Qt.hsla(0.5, 0.75, 0.5, 1) }
                    GradientStop { position: 0.67; color: Qt.hsla(0.67, 0.75, 0.5, 1) }
                    GradientStop { position: 0.83; color: Qt.hsla(0.83, 0.75, 0.5, 1) }
                    GradientStop { position: 1.0; color: Qt.hsla(1.0, 0.75, 0.5, 1) }
                }
                onMoved: v => mixer.hue = Math.min(v, 0.999)
            }
            Label { text: qsTr("Dark or light"); opacity: 0.75 }
            Strip {
                objectName: "colorLight"
                value: (mixer.light - 0.1) / 0.8
                fill: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.hsla(mixer.hue, mixer.strength, 0.1, 1) }
                    GradientStop { position: 0.5; color: Qt.hsla(mixer.hue, mixer.strength, 0.5, 1) }
                    GradientStop { position: 1.0; color: Qt.hsla(mixer.hue, mixer.strength, 0.9, 1) }
                }
                onMoved: v => mixer.light = 0.1 + v * 0.8
            }
            Label { text: qsTr("Soft or strong"); opacity: 0.75 }
            Strip {
                objectName: "colorStrength"
                value: mixer.strength
                fill: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Qt.hsla(mixer.hue, 0, mixer.light, 1) }
                    GradientStop { position: 1.0; color: Qt.hsla(mixer.hue, 1, mixer.light, 1) }
                }
                onMoved: v => mixer.strength = v
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 6
                spacing: 8
                Button {
                    objectName: "colorUse"
                    Layout.fillWidth: true
                    implicitHeight: 52
                    font.pixelSize: 16
                    highlighted: true
                    text: qsTr("Use This Color")
                    onClicked: {
                        const c = mixer.hex
                        if (cp.pos && !StoreColors.all.some(x => StoreColors.same(x, c)))
                            cp.pos.addCustomColor(c)
                        mixer.close()
                        cp.picked(c)
                    }
                }
                Button {
                    Layout.preferredWidth: 140
                    implicitHeight: 52
                    font.pixelSize: 16
                    text: qsTr("Cancel")
                    onClicked: mixer.close()
                }
            }
        }
    }
}
