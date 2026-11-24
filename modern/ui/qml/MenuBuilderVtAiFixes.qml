import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Menu Builder: what VT Ai proposes for the menu (a price for an item with
// none, a category for one in none, descriptions, kitchen names), each with a
// tick: only the ticked ones are made, as one step Undo takes back.
Dialog {
    id: fixDialog
    property Item menu   // the WidgetMenuBuilder
    objectName: "builderVtAiFixes"
    property string task: ""      // fix | describe
    property string what: ""      // describe: description | kitchenName
    readonly property var vtai: menu && menu.pos ? (menu.pos.vtai ?? {}) : ({})
    readonly property bool mine: vtai.task === task
    readonly property bool busy: mine && vtai.busy === true
    readonly property var fixes: mine && !busy ? (vtai.fixes ?? []) : []
    property var off: ({})        // index -> unticked
    readonly property var chosen: fixes.filter((f, i) => off[i] !== true)

    // Asks VT Ai, then shows what it proposes.
    function check() { task = "fix"; what = ""; off = {}; open(); menu.pos.vtaiFixMenu() }
    function describe(ids, kind) { task = "describe"; what = kind; off = {}; open(); menu.pos.vtaiDescribe(ids, kind) }
    // The menu in another language: which one first.
    property bool picking: false
    function suggest(ids, kind) { task = "suggest"; what = kind; off = {}; picking = false; open(); menu.pos.vtaiSuggest(ids, kind) }
    function translate() { task = "translate"; what = ""; off = {}; picking = true; if (menu.pos) menu.pos.vtaiClear(); open() }
    function translateInto(lang) { what = lang; picking = false; menu.pos.vtaiTranslate(lang) }

    title: task === "suggest" ? (what === "allergens" ? qsTr("Allergens (VT Ai)") : qsTr("Choices for Items (VT Ai)"))
         : task === "translate" ? qsTr("Translate the Menu (VT Ai)")
         : task === "fix" ? qsTr("Check the Menu (VT Ai)")
         : what === "kitchenName" ? qsTr("Kitchen Names (VT Ai)") : qsTr("Descriptions (VT Ai)")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent.width - 32, 820)
    height: Math.min(parent.height - 32, 620)
    modal: true
    closePolicy: busy ? Popup.NoAutoClose : Popup.CloseOnEscape
    onClosed: { picking = false; if (menu.pos && !busy && mine) menu.pos.vtaiClear() }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8
        Label {
            objectName: "vtaiFixStatus"
            textFormat: Text.PlainText
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: fixDialog.mine && fixDialog.vtai.error ? Ui.warn : palette.windowText
            text: fixDialog.picking ? qsTr("Into which language? Each person's screen shows the menu in their own language; the kitchen's tickets and receipts in the store's.")
                : !fixDialog.mine ? ""
                : fixDialog.busy ? (fixDialog.vtai.progress || qsTr("Thinking…")) + " " + qsTr("This takes a minute or two; the register works as usual meanwhile.")
                : fixDialog.vtai.error ? fixDialog.vtai.error
                : fixDialog.fixes.length === 0 ? (fixDialog.vtai.answer ?? "")
                : qsTr("Untick any you don't want, then use the rest. Undo takes them all back.")
        }
        BusyIndicator { visible: fixDialog.busy; running: visible; Layout.alignment: Qt.AlignHCenter }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            visible: fixDialog.picking
            Repeater {
                model: [{ code: "es", name: "Español" }, { code: "fr", name: "Français" },
                        { code: "pt", name: "Português" }, { code: "zh", name: "中文" }]
                delegate: MenuTouchButton {
                    required property var modelData
                    objectName: "vtaiLang_" + modelData.code
                    text: modelData.name
                    onClicked: fixDialog.translateInto(modelData.code)
                }
            }
        }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: fixDialog.fixes
            delegate: RowLayout {
                id: row
                required property var modelData
                required property int index
                width: list.width
                spacing: 8
                CheckBox {
                    objectName: "vtaiFix" + row.index
                    checked: fixDialog.off[row.index] !== true
                    onToggled: {
                        const o = Object.assign({}, fixDialog.off)
                        o[row.index] = !checked
                        fixDialog.off = o
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        font.bold: true
                        font.pixelSize: Ui.px(16)
                        text: String(row.modelData.field).startsWith("word:") ? qsTr("%1 → %2").arg(row.modelData.name).arg(row.modelData.shown)
                            : row.modelData.field === "price" ? qsTr("%1: %2").arg(row.modelData.name).arg(row.modelData.shown)
                            : row.modelData.field === "family" ? qsTr("%1 into %2").arg(row.modelData.name).arg(row.modelData.shown)
                            : row.modelData.name
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        opacity: 0.8
                        font.pixelSize: Ui.px(14)
                        visible: text !== ""
                        text: String(row.modelData.field).startsWith("word:") ? ""
                            : row.modelData.field === "allergens" || row.modelData.field === "groups"
                              ? "+ " + row.modelData.shown + (row.modelData.why ? "  ·  " + row.modelData.why : "")
                            : row.modelData.field === "description" || row.modelData.field === "kitchenName"
                              ? row.modelData.shown : (row.modelData.why ?? "")
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            MenuTouchButton {
                text: fixDialog.busy ? qsTr("Stop") : qsTr("Close")
                onClicked: {
                    if (fixDialog.busy) fixDialog.menu.pos.vtaiCancel()
                    fixDialog.close()
                }
            }
            MenuTouchButton {
                objectName: "vtaiApply"
                highlighted: true
                visible: fixDialog.fixes.length > 0
                enabled: fixDialog.chosen.length > 0
                text: qsTr("Use %n", "", fixDialog.chosen.length)
                onClicked: {
                    fixDialog.menu.pos.vtaiApply(fixDialog.chosen)
                    fixDialog.close()
                }
            }
        }
    }
}
