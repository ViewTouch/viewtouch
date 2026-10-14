import QtQuick
import QtQuick.Layouts

// The Time Clock: someone types their PIN (they don't log in), sees whether
// they're clocked in, today's hours and their shifts for the next two weeks,
// and clocks in, out, or on / off break. It forgets them after a little while.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var info: pos ? pos.timeClock : ({})
    readonly property bool someone: !!info.name
    readonly property var ot: info.overtime ?? ({})
    readonly property var grabs: info.requests ? info.requests.upForGrabs : []
    readonly property var mine: info.requests ? info.requests.mine : []
    property bool pickingOff: false
    property string offDay: ""
    onSomeoneChanged: pickingOff = false
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property color ink: zone.st.textColor ?? "white"
    // Upright (a phone): the keypad alone; then the person above their schedule.
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real unit: narrow ? Math.max(14, Math.min(w.width / 20, w.height / 34))
                                        : Math.max(14, Math.min(w.width / 46, w.height / 26))
    property string pin: ""

    // Back to the keypad when nobody's touched it for a while.
    Timer {
        id: idle
        interval: 20000
        running: w.someone
        onTriggered: w.pos.timeClockDone()
    }
    onInfoChanged: idle.restart()
    function touched() { idle.restart() }

    GridLayout {
        anchors.fill: parent
        columns: w.narrow ? 1 : 2
        rowSpacing: w.unit
        columnSpacing: w.unit

        // --- left: the keypad, or the person and their buttons ---
        Rectangle {
            Layout.preferredWidth: w.narrow ? parent.width : parent.width * 0.42
            Layout.fillWidth: w.narrow
            Layout.fillHeight: !w.narrow || !w.someone
            Layout.preferredHeight: w.narrow && w.someone ? w.unit * 18 : -1
            radius: 12
            color: "#1d2128"

            // The keypad
            ColumnLayout {
                visible: !w.someone
                anchors.fill: parent
                anchors.margins: w.unit
                spacing: w.unit * 0.5
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: w.pin.length ? "●".repeat(w.pin.length) : qsTr("Type your PIN")
                    color: w.pin.length ? w.ink : "#8a94a6"
                    font.family: w.face
                    font.pixelSize: w.unit * 1.5
                }
                GridLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    columns: 3
                    rowSpacing: w.unit * 0.4
                    columnSpacing: w.unit * 0.4
                    Repeater {
                        model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "Clear", "0", "OK"]
                        delegate: WidgetKey {
                            required property string modelData
                            objectName: "clockKey-" + modelData
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: modelData === "Clear" ? qsTr("Clear") : modelData === "OK" ? qsTr("OK") : modelData
                            fontScale: modelData.length > 1 ? 0.3 : 0.45
                            baseColor: modelData === "OK" ? "#1f6b40" : "#343c49"
                            onClicked: {
                                if (modelData === "Clear") {
                                    w.pin = ""
                                } else if (modelData === "OK") {
                                    const p = w.pin
                                    w.pin = ""
                                    if (p !== "")
                                        w.pos.timeClockStart(p)
                                } else if (w.pin.length < 8) {
                                    w.pin += modelData
                                }
                            }
                        }
                    }
                }
            }

            // The person
            ColumnLayout {
                visible: w.someone
                anchors.fill: parent
                anchors.margins: w.unit
                spacing: w.unit * 0.5
                Text {
                    Layout.fillWidth: true
                    text: w.info.name ?? ""
                    color: w.ink
                    font.family: w.face
                    font.pixelSize: w.unit * 1.8
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    objectName: "clockStatus"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: w.info.status === "in" ? qsTr("Clocked in since %1").arg(w.info.since)
                                                   + (w.info.job ? "  ·  " + w.info.job : "")
                        : w.info.status === "break" ? qsTr("On break since %1").arg(w.info.breakSince)
                        : qsTr("Not clocked in")
                    color: w.info.status === "in" ? "#5fd08a" : w.info.status === "break" ? "#f5b940" : "#8a94a6"
                    font.family: w.face
                    font.pixelSize: w.unit
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Today: %1 hours").arg(w.info.todayHours ?? "0")
                          + "  ·  " + qsTr("This week: %1 hours").arg(w.ot.weekHours ?? "0")
                    color: "#b8c0cc"
                    font.family: w.face
                    font.pixelSize: w.unit * 0.8
                    wrapMode: Text.WordWrap
                }
                // Close to overtime (or in it).
                Text {
                    objectName: "clockOvertime"
                    visible: w.ot.state === "soon" || w.ot.state === "over"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: w.ot.state === "over" ? qsTr("You're in overtime. Check with a manager before staying on.")
                                                : qsTr("%1 h until overtime.").arg(w.ot.left)
                    color: w.ot.state === "over" ? "#ff8a8f" : "#f5b940"
                    font.family: w.face
                    font.pixelSize: w.unit * 0.9
                    font.bold: true
                }
                Item { Layout.fillHeight: true }
                WidgetKey {
                    objectName: "clockIn"
                    visible: w.info.status === "out"
                    Layout.fillWidth: true
                    Layout.preferredHeight: w.unit * 3.2
                    text: qsTr("Clock In")
                    baseColor: "#1f6b40"
                    fontScale: 0.32
                    onClicked: { w.touched(); w.pos.timeClockAct("in") }
                }
                WidgetKey {
                    objectName: "clockBreak"
                    visible: w.info.status !== "out"
                    Layout.fillWidth: true
                    Layout.preferredHeight: w.unit * 2.6
                    text: w.info.status === "break" ? qsTr("End Break") : qsTr("Start Break")
                    baseColor: "#a86a12"
                    fontScale: 0.32
                    onClicked: { w.touched(); w.pos.timeClockAct("break") }
                }
                WidgetKey {
                    objectName: "clockOut"
                    visible: w.info.status !== "out"
                    Layout.fillWidth: true
                    Layout.preferredHeight: w.unit * 3.2
                    text: qsTr("Clock Out")
                    baseColor: "#8a2c2c"
                    fontScale: 0.32
                    onClicked: { w.touched(); w.pos.timeClockAct("out") }
                }
                WidgetKey {
                    objectName: "timeOff"
                    Layout.fillWidth: true
                    Layout.preferredHeight: w.unit * 2.2
                    text: qsTr("Ask for Time Off…")
                    fontScale: 0.32
                    onClicked: { w.touched(); w.offDay = ""; w.pickingOff = true }
                }
                WidgetKey {
                    objectName: "clockDone"
                    Layout.fillWidth: true
                    Layout.preferredHeight: w.unit * 2.2
                    text: qsTr("Done")
                    fontScale: 0.32
                    onClicked: w.pos.timeClockDone()
                }
            }
        }

        // --- right: their schedule ---
        Rectangle {
            visible: !w.narrow || w.someone
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 12
            color: "#1d2128"

            Text {
                visible: !w.someone
                anchors.centerIn: parent
                width: parent.width * 0.8
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("Type your PIN to clock in or out, take a break, and see your schedule.")
                color: "#8a94a6"
                font.family: w.face
                font.pixelSize: w.unit * 1.1
            }

            // Asking for a day off: the day, a reason, Ask.
            Rectangle {
                objectName: "offPicker"
                visible: w.someone && w.pickingOff
                z: 5
                anchors.fill: parent
                radius: 12
                color: "#1d2128"
                MouseArea { anchors.fill: parent }
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: w.unit
                    spacing: w.unit * 0.5
                    Text {
                        text: qsTr("Which day off?")
                        color: w.ink
                        font.family: w.face
                        font.pixelSize: w.unit * 1.3
                        font.bold: true
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 7
                        rowSpacing: w.unit * 0.25
                        columnSpacing: w.unit * 0.25
                        Repeater {
                            model: 28
                            delegate: WidgetKey {
                                required property int index
                                readonly property date day: new Date(new Date().getFullYear(), new Date().getMonth(),
                                                                     new Date().getDate() + index)
                                readonly property string iso: Qt.formatDate(day, "yyyy-MM-dd")
                                objectName: "offDay-" + index
                                Layout.fillWidth: true
                                Layout.preferredHeight: w.unit * 2.2
                                text: day.toLocaleDateString(Qt.locale(), "ddd") + "\n" + day.toLocaleDateString(Qt.locale(), "MMM d")
                                accent: w.offDay === iso
                                fontScale: 0.28
                                onClicked: { w.touched(); w.offDay = iso }
                            }
                        }
                    }
                    Text {
                        text: qsTr("Why? (optional)")
                        color: "#8a94a6"
                        font.family: w.face
                        font.pixelSize: w.unit * 0.85
                    }
                    RowLayout {
                        id: reasons
                        property string chosen: ""
                        Layout.fillWidth: true
                        spacing: w.unit * 0.3
                        Repeater {
                            model: [QT_TR_NOOP("Personal"), QT_TR_NOOP("Sick"), QT_TR_NOOP("Vacation"), QT_TR_NOOP("School")]
                            delegate: WidgetKey {
                                required property string modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: w.unit * 1.8
                                text: qsTr(modelData)
                                accent: reasons.chosen === modelData
                                fontScale: 0.34
                                onClicked: reasons.chosen = reasons.chosen === modelData ? "" : modelData
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: w.unit * 0.4
                        WidgetKey {
                            Layout.fillWidth: true
                            Layout.preferredHeight: w.unit * 2.4
                            text: qsTr("Cancel")
                            fontScale: 0.32
                            onClicked: { w.touched(); w.pickingOff = false }
                        }
                        WidgetKey {
                            objectName: "askOff"
                            Layout.fillWidth: true
                            Layout.preferredHeight: w.unit * 2.4
                            enabled: w.offDay !== ""
                            opacity: enabled ? 1 : 0.4
                            text: qsTr("Ask")
                            baseColor: "#1f6b40"
                            fontScale: 0.32
                            onClicked: {
                                w.touched()
                                w.pos.timeClockRequestOff(w.offDay, reasons.chosen)
                                w.pickingOff = false
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                visible: w.someone
                anchors.fill: parent
                anchors.margins: w.unit
                spacing: w.unit * 0.4
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("My schedule")
                        color: w.ink
                        font.family: w.face
                        font.pixelSize: w.unit * 1.3
                        font.bold: true
                    }
                    Text {
                        text: qsTr("%1 hours this week").arg(w.info.weekHours ?? "0")
                        color: "#8a94a6"
                        font.family: w.face
                        font.pixelSize: w.unit * 0.8
                    }
                }
                Text {
                    visible: (w.info.shifts ?? []).length === 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("No shifts on the schedule for the next two weeks.")
                    color: "#8a94a6"
                    font.family: w.face
                    font.pixelSize: w.unit
                }
                ListView {
                    id: shiftList
                    objectName: "clockShifts"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: w.unit * 0.3
                    model: w.info.shifts ?? []
                    delegate: Rectangle {
                        required property var modelData
                        width: ListView.view.width
                        height: w.unit * 2.4
                        radius: 8
                        color: modelData.now ? "#1f4f35" : "#262c36"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: w.unit * 0.6
                            anchors.rightMargin: w.unit * 0.6
                            Text {
                                Layout.preferredWidth: w.unit * 7
                                text: modelData.day
                                color: w.ink
                                font.family: w.face
                                font.pixelSize: w.unit * 0.95
                                font.bold: true
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.hours + (modelData.note ? "  ·  " + modelData.note : "")
                                color: w.ink
                                font.family: w.face
                                font.pixelSize: w.unit * 0.95
                                elide: Text.ElideRight
                            }
                            Text {
                                text: modelData.now ? qsTr("now") : qsTr("%1 h").arg(modelData.length)
                                color: modelData.now ? "#5fd08a" : "#8a94a6"
                                font.family: w.face
                                font.pixelSize: w.unit * 0.8
                                font.bold: modelData.now
                            }
                            WidgetKey {
                                objectName: "giveAway-" + modelData.id
                                visible: modelData.future && !modelData.givingAway
                                Layout.preferredWidth: w.unit * 5.2
                                Layout.preferredHeight: w.unit * 1.7
                                text: qsTr("Give Away")
                                fontScale: 0.36
                                onClicked: { w.touched(); w.pos.timeClockGiveAway(modelData.id) }
                            }
                            Text {
                                visible: modelData.givingAway
                                text: qsTr("up for grabs")
                                color: "#f5b940"
                                font.family: w.face
                                font.pixelSize: w.unit * 0.75
                            }
                        }
                    }
                }
                // Others' shifts given away: take one (a manager approves).
                Text {
                    visible: w.grabs.length > 0
                    text: qsTr("Up for grabs")
                    color: "#f5b940"
                    font.family: w.face
                    font.pixelSize: w.unit
                    font.bold: true
                }
                Repeater {
                    model: w.grabs
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: modelData.day + "  " + modelData.hours + "  ·  " + modelData.who
                            color: w.ink
                            font.family: w.face
                            font.pixelSize: w.unit * 0.9
                            elide: Text.ElideRight
                        }
                        WidgetKey {
                            objectName: "take-" + modelData.id
                            Layout.preferredWidth: w.unit * 5
                            Layout.preferredHeight: w.unit * 1.7
                            text: qsTr("Take It")
                            baseColor: "#1f6b40"
                            fontScale: 0.36
                            onClicked: { w.touched(); w.pos.timeClockTake(modelData.id) }
                        }
                    }
                }
                // My requests and where they stand.
                Text {
                    visible: w.mine.length > 0
                    text: qsTr("My requests")
                    color: w.ink
                    font.family: w.face
                    font.pixelSize: w.unit
                    font.bold: true
                }
                Repeater {
                    model: w.mine
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            text: modelData.text
                            color: w.ink
                            font.family: w.face
                            font.pixelSize: w.unit * 0.85
                            elide: Text.ElideRight
                        }
                        Text {
                            objectName: "requestStatus-" + modelData.id
                            text: modelData.statusText
                            color: modelData.status === "approved" ? "#5fd08a"
                                 : modelData.status === "denied" ? "#ff8a8f" : "#f5b940"
                            font.family: w.face
                            font.pixelSize: w.unit * 0.8
                        }
                        WidgetKey {
                            visible: modelData.canCancel
                            Layout.preferredWidth: w.unit * 4
                            Layout.preferredHeight: w.unit * 1.5
                            text: modelData.taking ? qsTr("Drop") : qsTr("Cancel")
                            fontScale: 0.36
                            onClicked: { w.touched(); w.pos.timeClockCancelRequest(modelData.id) }
                        }
                    }
                }
            }
        }
    }
}
