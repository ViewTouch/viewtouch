import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// An order for later: the day, the hour and the minutes it should be ready.
// It goes to the kitchen by itself a little before (Store Settings).
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var check: pos ? pos.check : ({})
    readonly property real dueAt: check.dueAt ?? 0

    // The choice being made; the hour picks the time (minutes default to :00).
    property int day: 0
    property int hour: -1
    property int minute: 0
    readonly property date today: { const d = new Date(); d.setHours(0, 0, 0, 0); return d }

    function dateFor(dayOffset, h, m) {
        const d = new Date(w.today)
        d.setDate(d.getDate() + dayOffset)
        d.setHours(h, m, 0, 0)
        return d
    }
    function apply() {
        if (w.hour >= 0)
            w.pos.setDueAt(w.dateFor(w.day, w.hour, w.minute).getTime())
    }
    function passed(dayOffset, h, m) { return w.dateFor(dayOffset, h, m).getTime() <= Date.now() }

    Component.onCompleted: {
        if (w.dueAt > 0) {   // what it is now
            const d = new Date(w.dueAt)
            const start = new Date(d); start.setHours(0, 0, 0, 0)
            w.day = Math.max(0, Math.round((start - w.today) / 86400000))
            w.hour = d.getHours()
            w.minute = d.getMinutes() - d.getMinutes() % 15
        }
    }

    // A choice: solid blue when it's the one picked.
    component Pick: Button {
        palette.button: highlighted ? "#2f6fd6" : "#343c49"
        palette.buttonText: "white"
    }

    // Upright (a phone): fewer days and hours a row, bigger.
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real zoom: zone ? zone.formZoom(narrow ? 380 : 900, narrow ? 0 : 560) : 1.4
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Label {
                objectName: "laterDue"
                Layout.fillWidth: true
                text: w.dueAt > 0 ? qsTr("Ready %1").arg(w.check.due ?? "") : qsTr("As soon as possible")
                font.pixelSize: 26
                font.bold: true
                color: w.dueAt > 0 ? "#7ec8ff" : "#e6e9ef"
            }

            Label { text: qsTr("Day"); opacity: 0.7; font.pixelSize: 16 }
            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                columns: w.narrow ? 4 : 7
                rowSpacing: 6
                columnSpacing: 6
                Repeater {
                    model: 7
                    Pick {
                        required property int index
                        objectName: "laterDay-" + index
                        Layout.fillWidth: true
                        implicitHeight: zone ? zone.touch(56) : 56
                        font.pixelSize: 16
                        highlighted: w.day === index
                        text: index === 0 ? qsTr("Today") : index === 1 ? qsTr("Tomorrow")
                             : Qt.locale().toString(w.dateFor(index, 12, 0), "ddd d")
                        onClicked: { w.day = index; w.apply() }
                    }
                }
            }

            Label { text: qsTr("Hour"); opacity: 0.7; font.pixelSize: 16 }
            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                columns: w.narrow ? 6 : 9
                rowSpacing: 6
                columnSpacing: 6
                Repeater {
                    model: 18   // 6 AM to 11 PM
                    Pick {
                        required property int index
                        readonly property int h: 6 + index
                        objectName: "laterHour-" + h
                        Layout.fillWidth: true
                        implicitHeight: zone ? zone.touch(56) : 56
                        font.pixelSize: 16
                        enabled: !w.passed(w.day, h, 45)
                        highlighted: w.hour === h
                        text: Qt.locale().toString(w.dateFor(0, h, 0), "h AP")
                        onClicked: {
                            w.hour = h
                            if (w.passed(w.day, h, w.minute))   // later in this hour
                                w.minute = [0, 15, 30, 45].find(m => !w.passed(w.day, h, m))
                            w.apply()
                        }
                    }
                }
            }

            Label { text: qsTr("Minutes"); opacity: 0.7; font.pixelSize: 16 }
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                spacing: 6
                Repeater {
                    model: [0, 15, 30, 45]
                    Pick {
                        required property int modelData
                        objectName: "laterMinute-" + modelData
                        Layout.fillWidth: true
                        implicitHeight: zone ? zone.touch(56) : 56
                        font.pixelSize: 18
                        enabled: w.hour >= 0 && !w.passed(w.day, w.hour, modelData)
                        highlighted: w.hour >= 0 && w.minute === modelData
                        text: ":" + (modelData < 10 ? "0" : "") + modelData
                        onClicked: { w.minute = modelData; w.apply() }
                    }
                }
            }

            Item { Layout.fillHeight: true }

            Button {
                objectName: "laterAsap"
                Layout.fillWidth: true
                implicitHeight: zone ? zone.touch(56) : 56
                font.pixelSize: 17
                enabled: w.dueAt > 0
                text: qsTr("As Soon as Possible")
                onClicked: { w.hour = -1; w.pos.setDueAt(0) }
            }
        }
    }
}
