import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Log Waste: what was thrown away (a menu item, which takes its recipe off
// the shelf, or an ingredient), how much and why. The Stock report tells it
// apart from what goes missing without a word.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var info: pos ? pos.wasteLog : ({})
    property bool byIngredient: false
    property string chosen: ""
    property string reason: ""
    property string search: ""
    readonly property var choices: (byIngredient ? (info.ingredients ?? []) : (info.items ?? []))
                                   .filter(c => search === "" || c.name.toLowerCase().includes(search.toLowerCase()))
    readonly property var chosenOne: (byIngredient ? (info.ingredients ?? []) : (info.items ?? [])).find(c => c.id === chosen)
    readonly property int loggedCount: (info.today ?? []).length
    onLoggedCountChanged: { chosen = ""; reason = ""; qty.text = "1" }
    onByIngredientChanged: chosen = ""

    function log() {
        const r = { qty: qty.text, reason: reason }
        r[byIngredient ? "ingredient" : "item"] = chosen
        pos.logWaste(r)
    }

    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real zoom: zone ? zone.formZoom(narrow ? 460 : 900) : 1.4
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    PosButton {
                        objectName: "wasteItems"
                        text: qsTr("Menu item")
                        checkable: true
                        checked: !w.byIngredient
                        highlighted: checked
                        implicitHeight: 46
                        font.pixelSize: 15
                        onClicked: w.byIngredient = false
                    }
                    PosButton {
                        objectName: "wasteIngredients"
                        text: qsTr("Ingredient")
                        checkable: true
                        checked: w.byIngredient
                        highlighted: checked
                        implicitHeight: 46
                        font.pixelSize: 15
                        onClicked: w.byIngredient = true
                    }
                    TextField {
                        objectName: "wasteSearch"
                        Layout.fillWidth: true
                        implicitHeight: 46
                        font.pixelSize: 15
                        placeholderText: qsTr("Find…")
                        onTextEdited: w.search = text
                    }
                }
                GridView {
                    id: grid
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 100
                    clip: true
                    readonly property int columns: Math.max(2, Math.floor(width / 170))
                    cellWidth: (width - gridBar.room) / columns
                    cellHeight: 58
                    model: w.choices
                    ScrollBar.vertical: TouchScrollBar { id: gridBar; needed: grid.contentHeight > grid.height + 1 }
                    delegate: PosButton {
                        required property var modelData
                        objectName: "waste-" + modelData.id
                        width: grid.cellWidth - 6
                        height: grid.cellHeight - 6
                        text: modelData.name
                        checkable: true
                        checked: w.chosen === modelData.id
                        highlighted: checked
                        font.pixelSize: 14
                        onClicked: w.chosen = modelData.id
                    }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: w.info.reasons ?? []
                        delegate: PosButton {
                            required property string modelData
                            required property int index
                            objectName: "wasteReason-" + index
                            text: modelData
                            checkable: true
                            checked: w.reason === modelData
                            highlighted: checked
                            implicitHeight: 44
                            font.pixelSize: 14
                            onClicked: w.reason = modelData
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Label { text: qsTr("How much"); font.pixelSize: 15 }
                    PosButton {
                        objectName: "wasteLess"
                        text: "−"
                        implicitWidth: 52
                        implicitHeight: 48
                        font.pixelSize: 20
                        onClicked: qty.text = String(Math.max(1, (Number(qty.text) || 1) - 1))
                    }
                    TextField {
                        id: qty
                        objectName: "wasteQty"
                        text: "1"
                        implicitWidth: 100
                        implicitHeight: 48
                        font.pixelSize: 18
                        horizontalAlignment: Text.AlignHCenter
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                    }
                    PosButton {
                        objectName: "wasteMore"
                        text: "+"
                        implicitWidth: 52
                        implicitHeight: 48
                        font.pixelSize: 20
                        onClicked: qty.text = String((Number(qty.text) || 0) + 1)
                    }
                    Label { text: w.byIngredient && w.chosenOne ? w.chosenOne.unit : ""; font.pixelSize: 15 }
                    PosButton {
                        objectName: "logWaste"
                        Layout.fillWidth: true
                        Layout.leftMargin: 12
                        text: w.chosenOne ? qsTr("Log Waste: %1 %2").arg(qty.text).arg(w.chosenOne.name) : qsTr("Log Waste")
                        highlighted: true
                        enabled: w.chosen !== "" && w.reason !== "" && Number(qty.text) > 0
                        implicitHeight: 48
                        font.pixelSize: 17
                        onClicked: w.log()
                    }
                }
            }

            ToolSeparator { Layout.fillHeight: true; visible: !w.narrow }

            ColumnLayout {
                visible: !w.narrow
                Layout.preferredWidth: parent.width * 0.32
                Layout.fillWidth: false
                Layout.fillHeight: true
                spacing: 6
                Label { text: qsTr("Thrown away today"); font.bold: true; font.pixelSize: 17 }
                ListView {
                    id: today
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: w.info.today ?? []
                    ScrollBar.vertical: TouchScrollBar { id: todayBar; needed: today.contentHeight > today.height + 1 }
                    delegate: ColumnLayout {
                        required property var modelData
                        width: ListView.view.width - todayBar.room
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: modelData.qty + " " + modelData.what + "  ·  " + modelData.value
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.when + "  ·  " + modelData.reason + "  ·  " + modelData.by
                            opacity: 0.6
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }
                }
                Label {
                    visible: (w.info.today ?? []).length === 0
                    text: qsTr("Nothing thrown away today.")
                    opacity: 0.6
                }
            }
        }
    }
}
