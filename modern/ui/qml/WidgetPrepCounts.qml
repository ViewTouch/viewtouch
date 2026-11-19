import QtQuick
import QtQuick.Layouts

// Prep counts (a food truck's "made 60 birria tacos"): how many are left of
// each counted item, going down as orders are sent. −/+ and +10 when more
// are made; the number to type how many are left; Stop to not count it any
// more. Add Item… picks one from the menu. At none left it is sold out until
// more are made (End of Day clears them all).
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string face: zone.st.font ?? Ui.font
    readonly property color ink: zone.st.textColor ?? "white"
    // On a phone, text at least 15 on the screen (ZoneItem.minText).
    readonly property real unit: Math.max(baseUnit, zone ? zone.minText(15) : 0)
    readonly property real baseUnit: Math.max(16, Math.min(w.width / 30, 40)) * (zone ? zone.textScale : 1)
    readonly property real tapH: w.zone ? w.zone.touch(64) : 48

    readonly property var counts: pos ? pos.prepCounts : []
    // The item the pad is for ({item, name}), or null; and what's typed.
    property var padFor: null
    property string typed: ""
    property bool picking: false

    function openPad(item, name) { w.typed = ""; w.padFor = { item: item, name: name }; w.picking = false }
    function digit(d) { if (w.typed.length < 4) w.typed = (w.typed === "0" ? "" : w.typed) + d }
    function done() {
        if (w.padFor && w.typed !== "")
            w.pos.setPrepCount(w.padFor.item, parseInt(w.typed))
        w.padFor = null
    }

    ColumnLayout {
        id: body
        anchors.fill: parent
        spacing: w.unit * 0.4
        enabled: w.padFor === null && !w.picking

        ListView {
            id: list
            objectName: "prepList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: w.unit * 0.3
            boundsBehavior: Flickable.StopAtBounds
            model: w.counts
            delegate: Rectangle {
                id: row
                required property var modelData
                objectName: "prep-" + modelData.item
                width: list.width
                height: Math.max(w.unit * 3, w.tapH)
                radius: 10
                color: modelData.left === 0 ? "#5a2a2a" : modelData.low ? "#6b4a14" : Ui.panel
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: w.unit * 0.8
                    anchors.rightMargin: w.unit * 0.4
                    spacing: w.unit * 0.4
                    Text {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        text: row.modelData.name
                        color: w.ink
                        font.family: w.face
                        font.pixelSize: w.unit * 1.1
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        textFormat: Text.PlainText
                        text: row.modelData.left === 0 ? qsTr("Sold out") : qsTr("%1 left").arg(row.modelData.left)
                        color: row.modelData.left === 0 ? Ui.returning : row.modelData.low ? Ui.warn : w.ink
                        font.family: w.face
                        font.pixelSize: w.unit * 1.1
                        font.bold: true
                        Layout.preferredWidth: w.unit * 6
                        horizontalAlignment: Text.AlignRight
                    }
                    Repeater {
                        model: [{ id: "less", text: "−1", n: -1 }, { id: "more", text: "+1", n: 1 }, { id: "more10", text: "+10", n: 10 }]
                        WidgetKey {
                            required property var modelData
                            objectName: "prep-" + row.modelData.item + "-" + modelData.id
                            Layout.preferredWidth: Math.max(w.unit * 3, w.tapH)
                            Layout.fillHeight: true
                            Layout.topMargin: w.unit * 0.3
                            Layout.bottomMargin: w.unit * 0.3
                            text: modelData.text
                            fontScale: 0.45
                            onClicked: w.pos.addPrepped(row.modelData.item, modelData.n)
                        }
                    }
                    WidgetKey {
                        objectName: "prep-" + row.modelData.item + "-set"
                        Layout.preferredWidth: w.unit * 4
                        Layout.fillHeight: true
                        Layout.topMargin: w.unit * 0.3
                        Layout.bottomMargin: w.unit * 0.3
                        text: qsTr("Set…")
                        fontScale: 0.4
                        onClicked: w.openPad(row.modelData.item, row.modelData.name)
                    }
                    WidgetKey {
                        objectName: "prep-" + row.modelData.item + "-stop"
                        Layout.preferredWidth: w.unit * 4
                        Layout.fillHeight: true
                        Layout.topMargin: w.unit * 0.3
                        Layout.bottomMargin: w.unit * 0.3
                        text: qsTr("Stop")
                        baseColor: Ui.muted
                        fontScale: 0.4
                        onClicked: w.pos.setPrepCount(row.modelData.item, -1)
                    }
                }
            }
            Text {
                textFormat: Text.PlainText
                anchors.centerIn: parent
                width: parent.width * 0.8
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: list.count === 0
                text: qsTr("Nothing counted yet. Add Item… and say how many you made: the screens count down as orders go out.")
                color: Ui.muted
                font.family: w.face
                font.pixelSize: w.unit * 1.1
            }
        }
        WidgetKey {
            objectName: "prepAdd"
            Layout.alignment: Qt.AlignLeft
            Layout.preferredWidth: w.unit * 10
            Layout.preferredHeight: Math.max(w.unit * 2.6, w.tapH)
            text: qsTr("Add Item…")
            baseColor: Ui.go
            fontScale: 0.4
            onClicked: w.picking = true
        }
    }

    // Add Item…: the menu's items (not counted yet), touch one.
    Rectangle {
        id: picker
        objectName: "prepPicker"
        anchors.fill: parent
        visible: w.picking
        color: Ui.deep
        radius: 10
        readonly property var items: {
            if (!w.pos || !w.picking)
                return []
            const counted = w.counts.map(c => c.item)
            return w.pos.menuItems.filter(m => !m.modifier && !counted.includes(m.id))
        }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: w.unit * 0.4
            spacing: w.unit * 0.4
            Text {
                textFormat: Text.PlainText
                text: qsTr("Which item did you prep?")
                color: w.ink
                font.family: w.face
                font.pixelSize: w.unit * 1.2
                font.bold: true
            }
            GridView {
                id: grid
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                cellWidth: width / Math.max(1, Math.floor(width / (w.unit * 9)))
                cellHeight: Math.max(w.unit * 2.8, w.tapH + 8)
                model: picker.items
                delegate: Item {
                    id: cell
                    required property var modelData
                    width: grid.cellWidth
                    height: grid.cellHeight
                    WidgetKey {
                        objectName: "prepPick-" + cell.modelData.id
                        anchors.fill: parent
                        anchors.margins: 4
                        text: cell.modelData.name
                        fontScale: 0.32
                        takesTouch: true
                        onClicked: w.openPad(cell.modelData.id, cell.modelData.name)
                    }
                }
            }
            WidgetKey {
                objectName: "prepPickCancel"
                Layout.preferredWidth: w.unit * 8
                Layout.preferredHeight: Math.max(w.unit * 2.6, w.tapH)
                text: qsTr("Cancel")
                fontScale: 0.4
                takesTouch: true
                onClicked: w.picking = false
            }
        }
    }

    // How many are left (made): a number pad.
    Rectangle {
        id: pad
        objectName: "prepPad"
        visible: w.padFor !== null
        anchors.centerIn: parent
        width: Math.min(parent.width, w.unit * 16)
        height: Math.min(parent.height, w.unit * 22)
        color: Ui.panel
        radius: 12
        border.color: Ui.accent
        border.width: 2
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: w.unit * 0.5
            spacing: w.unit * 0.3
            Text {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                text: w.padFor ? qsTr("%1: how many now?").arg(w.padFor.name) : ""
                color: w.ink
                font.family: w.face
                font.pixelSize: w.unit * 0.9
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                textFormat: Text.PlainText
                objectName: "prepPadShown"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                text: w.typed === "" ? "0" : w.typed
                color: w.ink
                font.family: w.face
                font.pixelSize: w.unit * 1.8
                font.bold: true
            }
            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: 3
                rowSpacing: 6
                columnSpacing: 6
                Repeater {
                    model: ["7", "8", "9", "4", "5", "6", "1", "2", "3", "C", "0", "OK"]
                    WidgetKey {
                        required property string modelData
                        objectName: "prepPad-" + modelData
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: modelData === "OK" ? qsTr("OK") : modelData
                        baseColor: modelData === "OK" ? Ui.go : Ui.key
                        fontScale: 0.45
                        takesTouch: true
                        onClicked: modelData === "OK" ? w.done() : modelData === "C" ? w.typed = "" : w.digit(modelData)
                    }
                }
            }
            WidgetKey {
                objectName: "prepPadCancel"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(w.unit * 2.2, w.tapH)
                text: qsTr("Cancel")
                fontScale: 0.4
                takesTouch: true
                onClicked: w.padFor = null
            }
        }
    }
}
