import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Send a message to other screens: everyone, the kitchen screens, the
// floor, or one person - a ready-made one or typed - and the last ones sent.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    property string to: "all"
    // How long it shows: 0 = once (the last hour); else until that day's end.
    property int keepDays: 0
    function until() {
        if (keepDays === 0) return 0
        const d = new Date()
        d.setDate(d.getDate() + keepDays - 1)
        d.setHours(23, 59, 0, 0)
        return d.getTime()
    }
    readonly property var presets: [qsTr("Need a runner"), qsTr("Order up"), qsTr("86: "), qsTr("Manager please"),
                                    qsTr("Help at the host stand"), qsTr("Table needs bussing"), qsTr("Allergy: please check")]

    // Upright (a phone): the message, then the recent ones below.
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real zoom: zone ? zone.formZoom(narrow ? 380 : 1100) : 1
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        GridLayout {
            anchors.fill: parent
            anchors.margins: 10
            columns: w.narrow ? 1 : 3   // the message | a separator | recent ones
            rowSpacing: 14
            columnSpacing: 14

            // Scrolls when many people are clocked in.
            TouchScrollColumn {
                Layout.preferredWidth: w.narrow ? parent.width : parent.width * 0.58
                Layout.fillWidth: w.narrow
                Layout.fillHeight: true
                spacing: 8
                Label { text: qsTr("To"); font.bold: true; font.pixelSize: 17 }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: [{ id: "all", name: qsTr("Everyone") }, { id: "kitchen", name: qsTr("Kitchen screens") },
                                { id: "floor", name: qsTr("Floor") }]
                                .concat((w.pos ? w.pos.staff : []).filter(p => p.clockedIn).map(p => ({ id: p.name, name: p.name })))
                        delegate: Button {
                            required property var modelData
                            text: modelData.name
                            checkable: true
                            checked: w.to === modelData.id
                            implicitHeight: 48
                            font.pixelSize: 16
                            onClicked: w.to = modelData.id
                        }
                    }
                }
                Label { text: qsTr("Message"); font.bold: true; font.pixelSize: 17; Layout.topMargin: 8 }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: w.presets
                        delegate: Button {
                            required property string modelData
                            text: modelData
                            implicitHeight: 48
                            font.pixelSize: 16
                            onClicked: { message.text = modelData; message.forceActiveFocus(); message.cursorPosition = message.length }
                        }
                    }
                }
                TextField {
                    id: message
                    Layout.fillWidth: true
                    implicitHeight: 52
                    font.pixelSize: 20
                    placeholderText: qsTr("Type a message…")
                    onAccepted: send.clicked()
                }
                Label { text: qsTr("Show it"); font.bold: true; font.pixelSize: 17; Layout.topMargin: 8 }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: [{ days: 0, name: qsTr("Once") }, { days: 1, name: qsTr("Until tonight") },
                                { days: 2, name: qsTr("Until tomorrow night") }, { days: 7, name: qsTr("For a week") }]
                        delegate: Button {
                            required property var modelData
                            objectName: "keep-" + modelData.days
                            text: modelData.name
                            checkable: true
                            checked: w.keepDays === modelData.days
                            implicitHeight: 48
                            font.pixelSize: 16
                            onClicked: w.keepDays = modelData.days
                        }
                    }
                }
                Button {
                    id: send
                    Layout.fillWidth: true
                    implicitHeight: 60
                    highlighted: true
                    font.pixelSize: 19
                    text: qsTr("Send")
                    enabled: message.text.trim().length > 0
                    onClicked: {
                        if (w.keepDays > 0)
                            w.pos.postMessage(w.to, message.text, w.until())
                        else
                            w.pos.sendMessage(w.to, message.text)
                        message.text = ""
                        w.keepDays = 0
                    }
                }
            }

            ToolSeparator { Layout.fillHeight: true; visible: !w.narrow }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Label { text: qsTr("Posted, and the last hour"); font.bold: true; font.pixelSize: 17 }
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: w.pos ? w.pos.messages : []
                    ScrollBar.vertical: TouchScrollBar { id: listBar }
                    delegate: Column {
                        required property var modelData
                        width: ListView.view.width - listBar.room
                        Label {
                            text: (modelData.posted ? qsTr("until %1").arg(modelData.until) : modelData.time)
                                  + "  ·  " + modelData.from + " → "
                                  + (({ all: qsTr("everyone"), kitchen: qsTr("kitchen"), floor: qsTr("floor") })[modelData.to] ?? modelData.to)
                            opacity: 0.7
                        }
                        RowLayout {
                            width: parent.width
                            Label { text: modelData.text; font.pixelSize: 17; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            Button {
                                visible: modelData.posted === true
                                objectName: "takeDown-" + modelData.id
                                text: qsTr("Take Down")
                                implicitHeight: 40
                                onClicked: w.pos.removeMessage(modelData.id)
                            }
                        }
                    }
                }
            }
        }
    }
}
