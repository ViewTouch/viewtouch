import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Edit mode -> Preview…: the page being edited on four screens - a phone, a
// 10" tablet, a 15.6" terminal and a 21.5" portrait kiosk - fitted as they
// would show it, with how big its smallest button really comes out there.
Popup {
    id: pv
    property EditorController editor
    property Item source: null   // the page as drawn, at canvas size
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Overlay.overlay ? Math.min(Overlay.overlay.width - 40, 1280) : 1280
    height: Overlay.overlay ? Math.min(Overlay.overlay.height - 40, 760) : 760
    padding: 20
    background: Rectangle {
        color: EditorStyle.chrome
        border.color: EditorStyle.border
        radius: 10
    }

    readonly property var info: visible && editor && editor.revision >= 0 ? editor.previewInfo() : ({})
    readonly property real canvasW: info.canvasW ?? 1920
    readonly property real canvasH: info.canvasH ?? 1080
    // Screens: the viewing area in millimeters (as held: phones and tablets
    // sideways for landscape pages).
    readonly property var screens: [
        { id: "phone", name: qsTr("Phone (6.1\")"), w: 141, h: 65 },
        { id: "tablet", name: qsTr("Tablet (10.2\")"), w: 207, h: 155 },
        { id: "terminal", name: qsTr("Terminal (15.6\")"), w: 345, h: 194 },
        { id: "kiosk", name: qsTr("Kiosk, upright (21.5\")"), w: 268, h: 476 }
    ]
    // A finger needs about 9 mm.
    function verdict(mm) {
        return mm >= 9 ? { text: qsTr("good for fingers"), color: "#5fd08a" }
             : mm >= 7 ? { text: qsTr("small"), color: "#f5b940" }
             : { text: qsTr("too small to touch reliably"), color: "#ff8a8f" }
    }

    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("Preview")
                font.pixelSize: 22
                font.bold: true
            }
            Button {
                text: qsTr("Close")
                onClicked: pv.close()
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: EditorStyle.muted
            text: pv.info.isPhone ? qsTr("This is a phone page: phones show it, other screens the page it's a version of.")
                : pv.info.phone ? qsTr("Phones set to phone pages show its phone version instead: %1.").arg(pv.info.phone)
                : qsTr("Pages keep their shape on every screen: where the screen is a different shape, the rest is left empty.")
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16
            Repeater {
                model: pv.screens
                delegate: ColumnLayout {
                    id: col
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 6
                    // Millimeters per canvas unit on this screen, and the smallest button there.
                    readonly property real mmPerUnit: Math.min(modelData.w / pv.canvasW, modelData.h / pv.canvasH)
                    readonly property real smallestMm: (pv.info.smallest ?? 0) * mmPerUnit
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        // The screen, as big as fits here.
                        Rectangle {
                            id: screen
                            objectName: "preview-" + col.modelData.id
                            readonly property real k: Math.min(parent.width / col.modelData.w, parent.height / col.modelData.h)
                            width: col.modelData.w * k
                            height: col.modelData.h * k
                            anchors.centerIn: parent
                            color: "black"
                            radius: 8
                            border.color: "#555"
                            border.width: 4
                            ShaderEffectSource {
                                anchors.centerIn: parent
                                readonly property real f: Math.min((parent.width - 8) / pv.canvasW, (parent.height - 8) / pv.canvasH)
                                width: pv.canvasW * f
                                height: pv.canvasH * f
                                sourceItem: pv.visible ? pv.source : null
                                live: pv.visible
                                hideSource: false
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: col.modelData.name
                        font.bold: true
                    }
                    Label {
                        objectName: "previewSize-" + col.modelData.id
                        visible: (pv.info.smallest ?? 0) > 0
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: qsTr("Smallest button: %1 mm, %2").arg(col.smallestMm.toFixed(1)).arg(pv.verdict(col.smallestMm).text)
                        color: pv.verdict(col.smallestMm).color
                        font.pixelSize: 13
                    }
                }
            }
        }
    }
}
