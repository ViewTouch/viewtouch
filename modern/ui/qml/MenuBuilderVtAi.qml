import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Menu Builder: VT Ai (the small AI on the store's computer) reads a menu
// typed or pasted (a paper menu's words, a supplier's price list) and makes
// items of it. Nothing is added here: what it read opens in Bring a Menu In's
// preview, where the manager ticks what comes in.
Dialog {
    id: vtaiDialog
    property Item menu          // the WidgetMenuBuilder
    property var importDialog  // its MenuBuilderImport (a Dialog)
    objectName: "builderVtAiDialog"
    readonly property var vtai: menu && menu.pos ? (menu.pos.vtai ?? {}) : ({})
    readonly property bool busy: vtai.task === "menu" && vtai.busy === true
    property bool asked: false
    title: qsTr("Describe Your Menu (VT Ai)")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent.width - 32, 760)
    height: Math.min(parent.height - 32, 620)
    modal: true
    closePolicy: busy ? Popup.NoAutoClose : Popup.CloseOnEscape
    onOpened: { asked = false; if (menu.pos) menu.pos.vtaiClear() }

    // Its answer: into the preview.
    onVtaiChanged: {
        if (!asked || vtai.task !== "menu" || vtai.busy)
            return
        if (vtai.result) {
            asked = false
            importDialog.read = vtai.result
            close()
            importDialog.open()
            Qt.callLater(() => menu.pos.vtaiClear())   // (not while its change is being told)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("Type or paste a menu: its headings, items and prices, the way it's written. VT Ai makes items, sizes and choices of it here on this computer; you see them all before anything is added.")
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            TextArea {
                id: words
                objectName: "vtaiMenuText"
                enabled: !vtaiDialog.busy
                wrapMode: TextEdit.Wrap
                placeholderText: qsTr("TACOS\nCarne Asada 3.50\nAl Pastor 3.25\n\nDRINKS\nHorchata small 2.50 / large 3.50")
                font.pixelSize: Ui.px(16)
            }
        }
        Label {
            objectName: "vtaiStatus"
            textFormat: Text.PlainText
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: vtaiDialog.vtai.error ? Ui.warn : palette.windowText
            text: vtaiDialog.busy ? (vtaiDialog.vtai.progress || qsTr("Reading the menu…"))
                                    + " " + qsTr("This takes a minute or two for each part; the register works as usual meanwhile.")
                : vtaiDialog.vtai.error ? vtaiDialog.vtai.error
                : !vtaiDialog.vtai.available ? qsTr("VT Ai isn't running on this computer (Store Settings → VT Ai).")
                : qsTr("%L1 of 30,000 letters").arg(words.text.length)
        }
        BusyIndicator { visible: vtaiDialog.busy; running: visible; Layout.alignment: Qt.AlignHCenter }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            MenuTouchButton {
                objectName: "vtaiCancel"
                text: vtaiDialog.busy ? qsTr("Stop") : qsTr("Close")
                onClicked: {
                    if (vtaiDialog.busy) {
                        vtaiDialog.asked = false
                        vtaiDialog.menu.pos.vtaiCancel()
                    } else {
                        vtaiDialog.close()
                    }
                }
            }
            MenuTouchButton {
                objectName: "vtaiRead"
                highlighted: true
                enabled: !vtaiDialog.busy && vtaiDialog.vtai.available === true
                         && words.text.trim() !== "" && words.text.length <= 30000
                text: qsTr("Read It")
                onClicked: {
                    vtaiDialog.asked = true
                    vtaiDialog.menu.pos.vtaiMenuFromText(words.text)
                }
            }
        }
    }
}
