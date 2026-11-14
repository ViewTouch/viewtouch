import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Find My Printers: the printers on the store's network (no addresses to
// type), each with a page that prints its address (to tell which is which)
// and a button for what it's for: receipts, the kitchen or the bar.
ColumnLayout {
    id: f
    property PosService pos
    property color ink: "white"
    property bool small: false
    readonly property var search: pos ? pos.printerSearch : ({})
    readonly property bool busy: search.busy === true
    readonly property var found: search.found ?? []
    spacing: 8

    PosButton {
        objectName: "findPrinters"
        Layout.fillWidth: f.small
        implicitHeight: 56
        highlighted: !f.search.done
        enabled: !f.busy
        font.pixelSize: 17
        text: f.busy ? qsTr("Looking…") : f.search.done ? qsTr("Look Again") : qsTr("Find My Printers")
        onClicked: f.pos.findPrinters()
    }
    Label {
        textFormat: Text.PlainText
        objectName: "findPrintersNone"
        visible: f.search.done === true && f.found.length === 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: "#f5b940"
        font.pixelSize: 15
        text: qsTr("None found. Is the printer on, and plugged into the same network (router or switch) as this computer? A USB printer isn't found this way: Manager → Printers.")
    }
    Label {
        textFormat: Text.PlainText
        visible: f.found.length > 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: f.ink
        opacity: 0.8
        font.pixelSize: 15
        text: qsTr("Touch Print Its Address on each: the one that prints is that one. Then say what it's for.")
    }
    Repeater {
        model: f.found
        delegate: Rectangle {
            id: row
            required property var modelData
            objectName: "foundPrinter-" + modelData.host
            Layout.fillWidth: true
            implicitHeight: rowGrid.implicitHeight + 16
            radius: 6
            color: Qt.rgba(1, 1, 1, 0.07)
            border.color: Qt.rgba(1, 1, 1, 0.18)
            GridLayout {
                id: rowGrid
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 8 }
                columns: f.small ? 2 : 5
                columnSpacing: 8
                rowSpacing: 6
                ColumnLayout {
                    Layout.columnSpan: f.small ? 2 : 1
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        textFormat: Text.PlainText
                        color: f.ink
                        font.pixelSize: 18
                        font.bold: true
                        text: row.modelData.host
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: row.modelData.printer ? "#7ee2a8" : f.ink
                        opacity: row.modelData.printer ? 1 : 0.75
                        font.pixelSize: 14
                        text: row.modelData.printer ? qsTr("✓ Your %1 printer").arg(row.modelData.printer)
                            : row.modelData.answered ? qsTr("A receipt or kitchen printer")
                            : qsTr("Takes print jobs (may be a printer)")
                    }
                }
                PosButton {
                    objectName: "foundPrinterTest-" + row.modelData.host
                    Layout.columnSpan: f.small ? 2 : 1
                    Layout.fillWidth: f.small
                    implicitHeight: 52
                    font.pixelSize: 15
                    text: qsTr("Print Its Address")
                    onClicked: f.pos.printWhoAmI(row.modelData.host)
                }
                Repeater {
                    model: [{ kind: "receipt", text: qsTr("Receipts") }, { kind: "kitchen", text: qsTr("Kitchen") },
                            { kind: "bar", text: qsTr("Bar") }]
                    delegate: PosButton {
                        required property var modelData
                        objectName: "foundPrinterUse-" + modelData.kind + "-" + row.modelData.host
                        Layout.fillWidth: f.small
                        implicitHeight: 52
                        font.pixelSize: 15
                        text: modelData.text
                        onClicked: f.pos.setupAddPrinter(modelData.kind,
                                                         row.modelData.port === 9100 ? row.modelData.host
                                                                                     : row.modelData.host + ":" + row.modelData.port)
                    }
                }
            }
        }
    }
}
