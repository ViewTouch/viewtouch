import QtQuick
import QtQuick.Layouts

// Counting a drawer or bank bill by bill: how many of each of the store's
// bills and coins (Store Settings -> Cash), typed on the keypad, adding up
// as you go. Blind counts (Store Settings or the person) never show what
// should be there. Covers the whole window while open.
Rectangle {
    id: sheet
    property PosService pos
    property var denominations: []
    property real drawerId: 0       // 0: mine
    property string title: ""
    property bool bank: false
    property var counts: []
    property int row: 0

    function start(id, name, pieces, isBank) {
        drawerId = id
        title = name
        bank = isBank
        denominations = pieces
        counts = pieces.map(() => "")
        row = 0
        visible = true
    }
    function cents() {
        let total = 0
        for (let i = 0; i < denominations.length; ++i)
            total += denominations[i].cents * (parseInt(counts[i] || "0") || 0)
        return total
    }
    function money(c) {
        return (pos ? pos.currencySymbol : "$") + (c / 100).toFixed(2)
    }
    function key(k) {
        const c = counts.slice()
        if (k === "⌫") c[row] = c[row].slice(0, -1)
        else if (k === "next") { row = (row + 1) % denominations.length; return }
        else if (c[row].length < 5) c[row] = (c[row] === "0" ? "" : c[row]) + k
        counts = c
    }
    function done() {
        pos.countDrawerPieces(drawerId, counts.map(n => parseInt(n || "0") || 0))
        visible = false
    }

    visible: false
    color: "#e60f1318"
    // The page behind is off while counting (its keypad would take the touches too).
    onVisibleChanged: TouchGuard.sheetOpen = visible
    Component.onDestruction: if (visible) TouchGuard.sheetOpen = false
    readonly property bool wide: width > height

    // Touches stop here, not on the page behind.
    MouseArea { anchors.fill: parent; hoverEnabled: true }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 24, sheet.wide ? 900 : 520)
        height: Math.min(parent.height - 24, sheet.wide ? 640 : 1100)
        radius: 14
        color: Ui.panel
        border.color: Ui.go
        border.width: 2

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: qsTr("Count %1").arg(sheet.title)
                color: "white"
                font.pixelSize: 24
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Touch a row, type how many, then Next.")
                color: Ui.muted
                font.pixelSize: 15
                wrapMode: Text.WordWrap
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: sheet.wide ? 2 : 1
                columnSpacing: 16
                rowSpacing: 10

                // The bills and coins.
                Flickable {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 160
                    contentHeight: rows.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ColumnLayout {
                        id: rows
                        width: list.width
                        spacing: 4
                        Repeater {
                            model: sheet.denominations
                            delegate: Rectangle {
                                id: line
                                required property var modelData
                                required property int index
                                readonly property bool current: sheet.row === index
                                readonly property int n: parseInt(sheet.counts[index] || "0") || 0
                                objectName: "countRow-" + modelData.cents
                                Layout.fillWidth: true
                                Layout.preferredHeight: 52
                                radius: 8
                                color: current ? Ui.accent : Ui.deep
                                border.color: current ? "#9cc0ff" : "transparent"
                                onCurrentChanged: if (current) list.contentY = Math.max(0, Math.min(y - list.height / 2 + height / 2, list.contentHeight - list.height))
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 14
                                    Text {
                                        Layout.preferredWidth: 110
                                        text: line.modelData.text
                                        color: "white"
                                        font.pixelSize: 22
                                        font.bold: true
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: "× " + (sheet.counts[line.index] || (line.current ? "_" : "0"))
                                        color: "white"
                                        font.pixelSize: 22
                                    }
                                    Text {
                                        text: sheet.money(line.n * line.modelData.cents)
                                        color: line.n > 0 ? "white" : "#5d6676"
                                        font.pixelSize: 20
                                    }
                                }
                                TapHandler { onTapped: sheet.row = line.index }
                            }
                        }
                    }
                }

                // The keypad.
                GridLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: sheet.wide
                    Layout.preferredHeight: sheet.wide ? -1 : 300
                    Layout.maximumWidth: sheet.wide ? 340 : 100000
                    columns: 3
                    rowSpacing: 6
                    columnSpacing: 6
                    Repeater {
                        model: ["7", "8", "9", "4", "5", "6", "1", "2", "3", "⌫", "0", "next"]
                        delegate: WidgetKey {
                            required property string modelData
                            objectName: "countKey-" + modelData
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 48
                            text: modelData === "next" ? qsTr("Next ↓") : modelData
                            fontScale: modelData === "next" ? 0.3 : 0.42
                            accent: modelData === "next"
                            onClicked: sheet.key(modelData)
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Counted")
                    color: "white"
                    font.pixelSize: 22
                }
                Text {
                    objectName: "countTotal"
                    text: sheet.money(sheet.cents())
                    color: "white"
                    font.pixelSize: 28
                    font.bold: true
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                WidgetKey {
                    objectName: "countCancel"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    text: qsTr("Cancel")
                    fontScale: 0.3
                    onClicked: sheet.visible = false
                }
                WidgetKey {
                    objectName: "countDone"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    text: sheet.bank ? qsTr("Check Out") : qsTr("Count Drawer")
                    fontScale: 0.3
                    baseColor: Ui.goDark
                    onClicked: sheet.done()
                }
            }
        }
    }
}
