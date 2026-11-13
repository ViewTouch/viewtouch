import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// A legacy ViewTouch store's menu and sales: its dat folder (found on this
// computer or a USB stick), what they hold and will become, which to bring,
// then bringing them (a backup is taken first).
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var state: pos ? pos.legacyImport : ({})
    readonly property var preview: state.preview ?? null
    readonly property var done: state.done ?? []
    readonly property string busy: state.busy ?? ""
    property var folders: []
    property string typed: ""
    // The parts to bring; history only into a store with no sales yet.
    property var parts: ({ menu: true, history: true })

    function look() { folders = pos ? pos.legacyFolders() : [] }
    Component.onCompleted: { look(); if (pos) pos.legacyClear() }
    Connections {
        target: w.pos
        function onUsbSticksOpened() { w.look() }
    }
    function money(cents) { return (w.pos ? w.pos.currencySymbol : "$") + (cents / 100).toLocaleString(Qt.locale(), "f", 2) }
    function setPart(key, on) { const p = Object.assign({}, parts); p[key] = on; parts = p }
    // Words in the page's own color (dark on Legacy's sand, light in Modern).
    readonly property color ink: zone && zone.st.textColor ? zone.st.textColor : Ui.text

    readonly property real zoom: zone ? zone.formZoom(zone.narrow ? 380 : 1100) : 1
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        Flickable {
            anchors.fill: parent
            anchors.margins: 12
            contentHeight: column.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ColumnLayout {
                id: column
                width: Math.min(parent.width, 900)
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 12

                // Reading a year of days takes a few seconds: said, not a frozen screen.
                Label {
                    textFormat: Text.PlainText
                    objectName: "legacyBusy"
                    Layout.fillWidth: true
                    visible: w.busy !== ""
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 22
                    font.bold: true
                    color: w.ink
                    text: w.busy
                }

                // --- 1. where its files are ---
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    visible: !w.preview && w.done.length === 0 && w.busy === ""
                    wrapMode: Text.WordWrap
                    font.pixelSize: 16
                    color: w.ink
                    opacity: 0.8
                    text: qsTr("Legacy ViewTouch kept its menu and every closed day's sales in its dat folder "
                               + "(/usr/viewtouch/dat, or a copy of it on a USB stick). Choose it: you'll see what "
                               + "comes over before anything changes. The menu goes on this ViewTouch's own menu screens.")
                }
                Repeater {
                    model: !w.preview && w.done.length === 0 && w.busy === "" ? w.folders : []
                    delegate: PosButton {
                        required property var modelData
                        objectName: "legacyFolder-" + index
                        required property int index
                        Layout.fillWidth: true
                        implicitHeight: 72
                        text: modelData.path + "   ·   " + modelData.label
                        onClicked: w.pos.legacyPreview(modelData.path)
                    }
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    visible: !w.preview && w.done.length === 0 && w.busy === "" && w.folders.length === 0
                    wrapMode: Text.WordWrap
                    font.pixelSize: 16
                    color: "#f0b429"
                    text: qsTr("No legacy dat folder found on this computer. Put in the USB stick with it, then Look on "
                               + "USB Sticks, or type where it is.")
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: !w.preview && w.done.length === 0 && w.busy === ""
                    spacing: 10
                    TextField {
                        id: path
                        objectName: "legacyPath"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 60
                        font.pixelSize: 18
                        placeholderText: "/media/usb/dat"
                        onAccepted: if (text !== "") w.pos.legacyPreview(text)
                    }
                    PosButton {
                        objectName: "legacyOpenPath"
                        Layout.preferredWidth: 160
                        implicitHeight: 60
                        enabled: path.text !== ""
                        text: qsTr("Open")
                        onClicked: w.pos.legacyPreview(path.text)
                    }
                    PosButton {
                        objectName: "legacyUsb"
                        Layout.preferredWidth: 240
                        implicitHeight: 60
                        text: qsTr("Look on USB Sticks")
                        onClicked: { if (!w.pos.openUsbSticks()) w.look() }
                    }
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    visible: (w.state.error ?? "") !== "" && w.done.length === 0
                    wrapMode: Text.WordWrap
                    font.pixelSize: 16
                    color: "#d14343"
                    text: w.state.error ?? ""
                }

                // --- 2. what it holds, and what comes over ---
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    visible: !!w.preview && w.done.length === 0
                    font.pixelSize: 24
                    font.bold: true
                    color: w.ink
                    wrapMode: Text.WordWrap
                    text: w.preview ? (w.preview.storeName || qsTr("A legacy store")) : ""
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    visible: !!w.preview && w.done.length === 0
                    font.pixelSize: 14
                    color: w.ink
                    opacity: 0.6
                    wrapMode: Text.WrapAnywhere
                    text: w.state.folder ?? ""
                }
                Repeater {
                    model: w.preview && w.done.length === 0 ? [
                        { key: "menu", title: qsTr("Its menu"),
                          text: qsTr("%n item(s) in %1 families and %2 choice groups (its modifier pages), with their "
                                     + "prices, taxes and printers, in place of this store's menu, on its menu screens", "",
                                     w.preview.items).arg(w.preview.categories).arg(w.preview.choices) },
                        { key: "history", title: qsTr("Its sales history"),
                          text: w.preview.hasSales
                                ? qsTr("Not into a store that has sales already: only into a new one (or after a factory reset)")
                                : qsTr("%n closed day(s), %1 to %2: %3 checks, %4, with their reports", "", w.preview.days)
                                      .arg(w.preview.from ?? "").arg(w.preview.to ?? "").arg(w.preview.checks).arg(w.money(w.preview.sales)) }
                    ] : []
                    delegate: PosButton {
                        id: part
                        required property var modelData
                        objectName: "legacyPart-" + modelData.key
                        Layout.fillWidth: true
                        implicitHeight: Math.max(72, contentItem.implicitHeight + 20)
                        readonly property bool possible: modelData.key !== "history" || !w.preview.hasSales
                        readonly property bool chosen: possible && w.parts[modelData.key]
                        enabled: possible
                        palette.button: chosen ? "#2b3a52" : "#2a303b"
                        onClicked: w.setPart(modelData.key, !chosen)
                        contentItem: ColumnLayout {
                            spacing: 2
                            Label {
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                                text: (part.chosen ? "✓ " : "   ") + part.modelData.title
                                font.pixelSize: 18
                                font.bold: true
                                color: part.possible ? "white" : Ui.muted
                            }
                            Label {
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                                text: part.modelData.text
                                font.pixelSize: 14
                                color: Ui.textSoft
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    visible: !!w.preview && w.done.length === 0 && (w.preview.differ ?? []).length > 0
                    wrapMode: Text.WordWrap
                    font.pixelSize: 14
                    color: "#f0b429"
                    text: w.preview ? qsTr("%n check(s) come to a little more or less here than legacy took (legacy "
                                           + "worked some discounts out its own way); each closes for what legacy took, the difference a \"Legacy difference\" discount:", "",
                                           (w.preview.differ ?? []).length)
                                      + "\n" + (w.preview.differ ?? []).slice(0, 8).join("\n")
                                      + ((w.preview.differ ?? []).length > 8 ? "\n…" : "") : ""
                }
                Repeater {
                    model: w.preview && w.done.length === 0 ? (w.preview.notes ?? []).concat(w.preview.errors ?? []) : []
                    delegate: Label {
                        required property string modelData
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 14
                        color: (w.preview.errors ?? []).includes(modelData) ? "#d14343" : w.ink
                        text: "• " + modelData
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: !!w.preview && w.done.length === 0
                    spacing: 10
                    PosButton {
                        objectName: "legacyOther"
                        Layout.fillWidth: true
                        implicitHeight: 72
                        text: qsTr("Another Folder")
                        onClicked: w.pos.legacyClear()
                    }
                    PosButton {
                        objectName: "legacyBring"
                        Layout.fillWidth: true
                        implicitHeight: 72
                        palette.button: Ui.goDark
                        text: qsTr("Bring It Over")
                        enabled: !!w.preview && w.busy === "" && (w.preview.errors ?? []).length === 0
                                 && (w.parts.menu || (w.parts.history && !w.preview.hasSales))
                        onClicked: w.pos.legacyImport(w.state.folder,
                                                      Object.assign({}, w.parts, { history: w.parts.history && !w.preview.hasSales }))
                    }
                }

                // --- 3. done ---
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    visible: w.done.length > 0
                    font.pixelSize: 24
                    font.bold: true
                    color: w.ink
                    wrapMode: Text.WordWrap
                    text: qsTr("Brought over")
                }
                Repeater {
                    model: w.done
                    delegate: Label {
                        required property string modelData
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 16
                        color: w.ink
                        text: "✓ " + modelData
                    }
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    visible: w.done.length > 0
                    wrapMode: Text.WordWrap
                    font.pixelSize: 14
                    color: w.ink
                    opacity: 0.8
                    text: qsTr("The store as it was before is kept in a backup: %1.").arg(w.state.backup ?? "")
                }
            }
        }
    }
}
