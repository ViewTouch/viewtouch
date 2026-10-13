import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Prep labels: what it is, when it was made and when to use it by, on the
// label printer (or as slips on a receipt printer). The use-by comes from the
// ingredient (Inventory: Keeps for), or the time chosen here.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var info: pos ? pos.prepLabels : ({})
    property string chosen: ""
    property string search: ""
    property int hours: 0
    property int count: 1
    readonly property var chosenOne: (info.ingredients ?? []).find(i => i.id === chosen)
    readonly property string name: other.text.trim() !== "" ? other.text.trim() : (chosenOne ? chosenOne.name : "")
    readonly property var choices: (info.ingredients ?? [])
                                   .filter(i => search === "" || i.name.toLowerCase().includes(search.toLowerCase()))
    readonly property var keeps: [{ h: 0, t: qsTr("No date") }, { h: 24, t: qsTr("1 day") }, { h: 48, t: qsTr("2 days") },
                                  { h: 72, t: qsTr("3 days") }, { h: 120, t: qsTr("5 days") }, { h: 168, t: qsTr("7 days") }]

    function choose(item) {
        chosen = item.id
        other.text = ""
        if (item.hours > 0)
            hours = item.hours
    }
    function fmt(d) { return Qt.locale().toString(d, "ddd M/d h:mm AP") }

    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real zoom: zone ? zone.formZoom(narrow ? 460 : 900) : 1.4
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        GridLayout {
            anchors.fill: parent
            anchors.margins: 10
            columns: w.narrow ? 1 : 2
            columnSpacing: 16
            rowSpacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    TextField {
                        objectName: "labelSearch"
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        implicitHeight: 46
                        font.pixelSize: 15
                        placeholderText: qsTr("Find…")
                        onTextEdited: w.search = text
                    }
                    TextField {
                        id: other
                        objectName: "labelOther"
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        implicitHeight: 46
                        font.pixelSize: 15
                        placeholderText: qsTr("Or type what it is")
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
                    delegate: Button {
                        required property var modelData
                        objectName: "label-" + modelData.id
                        width: grid.cellWidth - 6
                        height: grid.cellHeight - 6
                        text: modelData.name
                        checkable: true
                        checked: w.chosen === modelData.id && other.text.trim() === ""
                        highlighted: checked
                        font.pixelSize: 14
                        onClicked: w.choose(modelData)
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.maximumWidth: w.narrow ? 100000 : 420
                spacing: 8
                // How it will look.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: preview.implicitHeight + 16
                    radius: 6
                    color: "white"
                    ColumnLayout {
                        id: preview
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 0
                        Text {
                            Layout.fillWidth: true
                            text: w.name !== "" ? w.name : qsTr("(choose what it is)")
                            color: "black"
                            font.pixelSize: 21
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text { text: qsTr("Made %1").arg(w.fmt(new Date())); color: "black"; font.pixelSize: 15 }
                        Text {
                            visible: w.hours > 0
                            text: qsTr("Use by %1").arg(w.fmt(new Date(Date.now() + w.hours * 3600000)))
                            color: "black"
                            font.pixelSize: 15
                            font.bold: true
                        }
                        Text {
                            text: (note.text.trim() !== "" ? note.text.trim() + "  ·  " : "") + (w.pos ? w.pos.userName.split(" ")[0] : "")
                            color: "black"
                            font.pixelSize: 13
                        }
                    }
                }
                Label { text: qsTr("Keeps for"); font.pixelSize: 13; opacity: 0.75 }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    rowSpacing: 6
                    columnSpacing: 6
                    Repeater {
                        model: w.keeps
                        delegate: Button {
                            required property var modelData
                            objectName: "keeps-" + modelData.h
                            text: modelData.t
                            checkable: true
                            checked: w.hours === modelData.h
                            highlighted: checked
                            Layout.fillWidth: true
                            implicitHeight: 42
                            font.pixelSize: 14
                            onClicked: w.hours = modelData.h
                        }
                    }
                }
                TextField {
                    id: note
                    objectName: "labelNote"
                    Layout.fillWidth: true
                    implicitHeight: 42
                    font.pixelSize: 14
                    placeholderText: qsTr("Note (batch, amount…)")
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Button {
                        objectName: "labelsLess"
                        text: "−"
                        implicitWidth: 52
                        implicitHeight: 48
                        font.pixelSize: 20
                        onClicked: w.count = Math.max(1, w.count - 1)
                    }
                    Label {
                        objectName: "labelCount"
                        text: String(w.count)
                        Layout.preferredWidth: 44
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: 20
                        font.bold: true
                    }
                    Button {
                        objectName: "labelsMore"
                        text: "+"
                        implicitWidth: 52
                        implicitHeight: 48
                        font.pixelSize: 20
                        onClicked: w.count = Math.min(50, w.count + 1)
                    }
                    Button {
                        objectName: "printLabels"
                        Layout.fillWidth: true
                        text: qsTr("Print %n label(s)", "", w.count)
                        highlighted: true
                        enabled: w.name !== "" && (w.info.printer ?? "") !== ""
                        implicitHeight: 48
                        font.pixelSize: 16
                        onClicked: w.pos.printPrepLabels({ ingredient: other.text.trim() === "" ? w.chosen : "",
                                                           name: other.text.trim(), hours: w.hours, count: w.count,
                                                           note: note.text })
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: (w.info.printer ?? "") !== "" ? qsTr("Prints on %1").arg(w.info.printer)
                                                       : qsTr("No label printer: set one up in Manager → Printers.")
                    color: (w.info.printer ?? "") !== "" ? "white" : "#f5b940"
                    opacity: 0.75
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
                Item { Layout.fillHeight: true }
            }
        }
    }
}
