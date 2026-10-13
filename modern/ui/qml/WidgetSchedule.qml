import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// The staff schedule, a week at a time: each day's shifts (green when the
// person is on the clock), adding and removing shifts, and clocking someone
// in when the store only lets staff clock in on their schedule.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var info: pos ? pos.schedule : ({})
    readonly property var days: info.days ?? []
    property var chosen: null   // a shift touched

    property string who: ""
    // The day a new shift goes on: today until one is touched.
    property int pickedDay: -1
    readonly property int dayIndex: pickedDay >= 0 ? pickedDay : Math.max(0, days.findIndex(d => d.today))
    property string start: "4:00 PM"
    property string end: "10:00 PM"
    property string note: ""

    function add() {
        const day = days[dayIndex]
        if (!day)
            return
        pos.addShift({ employeeId: who, start: day.date + " " + start, end: day.date + " " + end, note: note })
        note = ""
    }

    // Upright (a phone): the week as rows, and adding a shift over three lines.
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real zoom: zone ? zone.formZoom(narrow ? 380 : 680) : 1
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Button { text: "‹"; font.pixelSize: 22; implicitWidth: 56; onClicked: { w.pickedDay = 0; w.pos.setScheduleWeek((w.info.week ?? 0) - 1) } }
                Label {
                    text: w.info.title ?? ""
                    font.pixelSize: 22
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    Layout.preferredWidth: 220
                }
                Button { text: "›"; font.pixelSize: 22; implicitWidth: 56; onClicked: { w.pickedDay = 0; w.pos.setScheduleWeek((w.info.week ?? 0) + 1) } }
                Item { Layout.fillWidth: true }
                Label {
                    visible: w.info.required ?? false
                    text: qsTr("Staff clock in only on their schedule")
                    opacity: 0.7
                }
            }

            // The week.
            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: w.narrow ? 1 : 7
                rowSpacing: 6
                columnSpacing: 6
                Repeater {
                    model: w.days
                    delegate: Rectangle {
                        id: dayBox
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        radius: 8
                        color: modelData.today ? "#26324a" : "#1d222b"
                        border.color: w.dayIndex === index ? "#4a7bd8" : "transparent"
                        border.width: 2
                        MouseArea { anchors.fill: parent; onClicked: { w.pickedDay = dayBox.index; w.chosen = null } }
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 4
                            Label {
                                text: dayBox.modelData.label
                                font.bold: true
                                font.pixelSize: 15
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Repeater {
                                model: dayBox.modelData.shifts
                                delegate: Rectangle {
                                    id: chip
                                    required property var modelData
                                    Layout.fillWidth: true
                                    implicitHeight: chipText.implicitHeight + 10
                                    radius: 6
                                    color: modelData.onClock ? "#1f6b40" : w.chosen && w.chosen.id === modelData.id ? "#3d5a99" : "#2e3644"
                                    Column {
                                        id: chipText
                                        anchors.fill: parent
                                        anchors.margins: 5
                                        Label { text: chip.modelData.name; font.bold: true; width: parent.width; elide: Text.ElideRight }
                                        Label { text: chip.modelData.time; font.pixelSize: 12; width: parent.width; elide: Text.ElideRight }
                                        Label {
                                            visible: chip.modelData.note !== ""
                                            text: chip.modelData.note
                                            font.pixelSize: 11
                                            opacity: 0.7
                                            width: parent.width
                                            elide: Text.ElideRight
                                        }
                                    }
                                    MouseArea { anchors.fill: parent; onClicked: w.chosen = chip.modelData }
                                }
                            }
                            Item { Layout.fillHeight: true }
                        }
                    }
                }
            }

            // A shift touched: remove it, or clock them in.
            RowLayout {
                visible: w.chosen !== null
                Layout.fillWidth: true
                Label {
                    text: w.chosen ? w.chosen.name + "  ·  " + w.chosen.time : ""
                    font.pixelSize: 17
                    font.bold: true
                    Layout.fillWidth: true
                }
                Button {
                    visible: w.chosen !== null && w.chosen.now && !w.chosen.onClock
                    text: qsTr("Clock Them In")
                    implicitHeight: 48
                    onClicked: { w.pos.clockInEmployee(w.chosen.employeeId); w.chosen = null }
                }
                Button {
                    text: qsTr("Remove Shift")
                    implicitHeight: 48
                    onClicked: { w.pos.removeShift(w.chosen.id); w.chosen = null }
                }
                Button { text: qsTr("‹ Done"); implicitHeight: 48; onClicked: w.chosen = null }
            }

            // Add a shift to the day picked above.
            GridLayout {
                visible: w.chosen === null
                Layout.fillWidth: true
                columns: w.narrow ? 4 : 7
                rowSpacing: 8
                columnSpacing: 8
                ComboBox {
                    id: staffBox
                    Layout.columnSpan: w.narrow ? 2 : 1
                    Layout.fillWidth: w.narrow
                    Layout.preferredWidth: 170   // names are short: the note gets the room
                    textRole: "name"
                    valueRole: "id"
                    model: w.info.staff ?? []
                    onActivated: w.who = currentValue
                    Component.onCompleted: w.who = currentValue ?? ""
                    onModelChanged: if (w.who === "") w.who = currentValue ?? ""
                }
                Label { Layout.columnSpan: w.narrow ? 2 : 1; text: w.days[w.dayIndex] ? w.days[w.dayIndex].label : "" ; font.bold: true }
                TextField {
                    implicitWidth: 110
                    Layout.preferredWidth: 110
                    Layout.fillWidth: false
                    text: w.start
                    placeholderText: "4:00 PM"
                    inputMethodHints: Qt.ImhPreferNumbers
                    onTextEdited: w.start = text
                }
                Label { text: "–" }
                TextField {
                    implicitWidth: 110
                    Layout.preferredWidth: 110
                    Layout.fillWidth: false
                    text: w.end
                    placeholderText: "10:00 PM"
                    inputMethodHints: Qt.ImhPreferNumbers
                    onTextEdited: w.end = text
                }
                TextField {
                    Layout.fillWidth: true
                    placeholderText: qsTr("note (patio, close…)")
                    text: w.note
                    onTextEdited: w.note = text
                }
                Button {
                    Layout.columnSpan: w.narrow ? 4 : 1
                    Layout.fillWidth: w.narrow
                    text: qsTr("Add Shift")
                    highlighted: true
                    implicitHeight: 48
                    onClicked: w.add()
                }
            }

            // Hours this week (red over the weekly overtime line).
            Flow {
                Layout.fillWidth: true
                spacing: 14
                Repeater {
                    model: w.info.totals ?? []
                    delegate: Label {
                        required property var modelData
                        text: modelData.name + " " + Number(modelData.hours).toFixed(1) + "h"
                        color: modelData.over ? "#ff9a9e" : palette.text
                    }
                }
            }
        }
    }
}
