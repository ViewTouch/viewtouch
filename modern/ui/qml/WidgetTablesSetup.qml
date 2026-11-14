import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// The store's tables, a question at a time: which rooms, how many tables of
// each size in each (and seats at a bar's counter), how they're named, then
// a look at the floor plan before it's made (LayoutController::buildFloor).
// The page editor (Edit Pages) is for moving them about afterwards.
Item {
    id: w
    property ZoneItem zone
    readonly property LayoutController controller: zone ? zone.controller : null
    readonly property color ink: zone && zone.st.textColor ? zone.st.textColor : Ui.text
    readonly property bool small: zone ? zone.narrow : false

    property int at: 0   // 0 rooms | 1 how many | 2 names | 3 a look | 4 made
    onAtChanged: flick.contentY = 0
    property var rooms: ({ dining: { on: true, two: 2, four: 6, six: 1, seats: 0 },
                           bar: { on: false, two: 2, four: 0, six: 0, seats: 6 },
                           patio: { on: false, two: 0, four: 4, six: 0, seats: 0 } })
    property bool byRoom: false
    property int made: 0
    readonly property var roomList: [
        { id: "dining", name: qsTr("Dining Room"), hint: qsTr("Tables inside") },
        { id: "bar", name: qsTr("Bar"), hint: qsTr("A counter with seats, and tables") },
        { id: "patio", name: qsTr("Patio"), hint: qsTr("Tables outside") }]
    readonly property var chosen: roomList.filter(r => rooms[r.id].on)
    function setRoom(id, key, value) {
        const r = JSON.parse(JSON.stringify(rooms))
        r[id][key] = value
        rooms = r
    }
    readonly property var answers: ({
        byRoom: byRoom,
        rooms: chosen.map(r => ({ id: r.id, two: rooms[r.id].two, four: rooms[r.id].four,
                                  six: rooms[r.id].six, seats: r.id === "bar" ? rooms[r.id].seats : 0 }))
    })
    readonly property var preview: controller ? controller.previewFloor(answers) : ({ count: 0, names: [], zones: [] })
    readonly property string missing: at === 0 && chosen.length === 0 ? qsTr("Touch at least one room.")
                                    : at === 1 && preview.count === 0 ? qsTr("Say how many tables there are.") : ""
    function next() {
        if (missing !== "") return
        if (at < 3) { at += 1; return }
        if (controller.buildFloor(answers)) {
            made = preview.count
            at = 4
        }
    }

    readonly property real zoom: zone ? zone.formZoom(small ? 380 : 1100) : 1
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        ColumnLayout {
            anchors { fill: parent; margins: w.small ? 10 : 20 }
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                visible: w.at < 4
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    color: w.ink
                    opacity: 0.7
                    font.pixelSize: 15
                    text: qsTr("Your tables") + "  ·  " + qsTr("%1 of 4").arg(w.at + 1)
                }
                Row {
                    spacing: 6
                    Repeater {
                        model: 4
                        delegate: Rectangle {
                            required property int index
                            width: 12; height: 12; radius: 6
                            color: index <= w.at ? Ui.accent : Qt.rgba(0.5, 0.5, 0.5, 0.4)
                        }
                    }
                }
            }

            Flickable {
                id: flick
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentHeight: body.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: TouchScrollBar {}

                ColumnLayout {
                    id: body
                    width: flick.width - 14
                    spacing: 12

                    // --- rooms ---
                    ColumnLayout {
                        visible: w.at === 0
                        Layout.fillWidth: true
                        spacing: 12
                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: w.ink
                            font.pixelSize: w.small ? 22 : 28
                            font.bold: true
                            text: qsTr("Where do guests sit?")
                        }
                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: w.ink
                            opacity: 0.75
                            font.pixelSize: 15
                            text: qsTr("Touch each one your place has. Each gets its own plan, and Main shows them all.")
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 10
                            Repeater {
                                model: w.roomList
                                delegate: PosButton {
                                    required property var modelData
                                    objectName: "tablesRoom-" + modelData.id
                                    implicitHeight: 84
                                    implicitWidth: w.small ? body.width : 300
                                    lines: 2
                                    checkable: true
                                    checked: w.rooms[modelData.id].on
                                    text: (checked ? "✓ " : "") + modelData.name + "\n" + modelData.hint
                                    onClicked: w.setRoom(modelData.id, "on", !w.rooms[modelData.id].on)
                                }
                            }
                        }
                    }

                    // --- how many ---
                    ColumnLayout {
                        visible: w.at === 1
                        Layout.fillWidth: true
                        spacing: 10
                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: w.ink
                            font.pixelSize: w.small ? 22 : 28
                            font.bold: true
                            text: qsTr("How many tables?")
                        }
                        Repeater {
                            model: w.chosen
                            delegate: ColumnLayout {
                                id: roomBox
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 6
                                Label {
                                    textFormat: Text.PlainText
                                    Layout.topMargin: 8
                                    color: w.ink
                                    font.pixelSize: 20
                                    font.bold: true
                                    text: roomBox.modelData.name
                                }
                                Repeater {
                                    model: (roomBox.modelData.id === "bar"
                                            ? [{ key: "seats", text: qsTr("Seats at the counter") }] : [])
                                           .concat([{ key: "two", text: qsTr("Tables for 2") },
                                                    { key: "four", text: qsTr("Tables for 4") },
                                                    { key: "six", text: qsTr("Tables for 6 or more") }])
                                    delegate: RowLayout {
                                        id: countRow
                                        required property var modelData
                                        spacing: 8
                                        readonly property int value: w.rooms[roomBox.modelData.id][modelData.key]
                                        Label {
                                            textFormat: Text.PlainText
                                            Layout.preferredWidth: w.small ? 150 : 260
                                            wrapMode: Text.WordWrap
                                            color: w.ink
                                            font.pixelSize: 17
                                            text: countRow.modelData.text
                                        }
                                        PosButton {
                                            objectName: "tablesLess-" + roomBox.modelData.id + "-" + countRow.modelData.key
                                            implicitWidth: 64
                                            implicitHeight: 56
                                            enabled: countRow.value > 0
                                            text: "−"
                                            font.pixelSize: 26
                                            onClicked: w.setRoom(roomBox.modelData.id, countRow.modelData.key, countRow.value - 1)
                                        }
                                        Label {
                                            textFormat: Text.PlainText
                                            objectName: "tablesCount-" + roomBox.modelData.id + "-" + countRow.modelData.key
                                            Layout.preferredWidth: 56
                                            horizontalAlignment: Text.AlignHCenter
                                            color: w.ink
                                            font.pixelSize: 24
                                            font.bold: true
                                            text: String(countRow.value)
                                        }
                                        PosButton {
                                            objectName: "tablesMore-" + roomBox.modelData.id + "-" + countRow.modelData.key
                                            implicitWidth: 64
                                            implicitHeight: 56
                                            enabled: countRow.value < 60
                                            text: "+"
                                            font.pixelSize: 26
                                            onClicked: w.setRoom(roomBox.modelData.id, countRow.modelData.key, countRow.value + 1)
                                        }
                                    }
                                }
                            }
                        }
                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            wrapMode: Text.WordWrap
                            color: w.ink
                            opacity: 0.8
                            font.pixelSize: 16
                            text: qsTr("%n table(s) in all.", "", w.preview.count)
                        }
                    }

                    // --- names ---
                    ColumnLayout {
                        visible: w.at === 2
                        Layout.fillWidth: true
                        spacing: 12
                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: w.ink
                            font.pixelSize: w.small ? 22 : 28
                            font.bold: true
                            text: qsTr("How are they numbered?")
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 10
                            PosButton {
                                objectName: "tablesNumbers-plain"
                                implicitHeight: 84
                                implicitWidth: w.small ? body.width : 360
                                lines: 2
                                checkable: true
                                checked: !w.byRoom
                                text: qsTr("One count for all") + "\nT1, T2, T3…"
                                onClicked: w.byRoom = false
                            }
                            PosButton {
                                objectName: "tablesNumbers-room"
                                visible: w.chosen.length > 1
                                implicitHeight: 84
                                implicitWidth: w.small ? body.width : 360
                                lines: 2
                                checkable: true
                                checked: w.byRoom
                                text: qsTr("A letter for each room") + "\n" + qsTr("D1… inside, B1… at the bar, P1… on the patio")
                                onClicked: w.byRoom = true
                            }
                        }
                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: w.ink
                            opacity: 0.75
                            font.pixelSize: 15
                            text: qsTr("Seats at a bar's counter are Bar 1, Bar 2… Any table can be renamed later in Edit Pages.")
                        }
                    }

                    // --- a look ---
                    ColumnLayout {
                        visible: w.at === 3
                        Layout.fillWidth: true
                        spacing: 10
                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: w.ink
                            font.pixelSize: w.small ? 22 : 28
                            font.bold: true
                            text: qsTr("Is this right?")
                        }
                        // The plan as it will be (Main: every room).
                        Rectangle {
                            id: plan
                            objectName: "tablesPreview"
                            Layout.fillWidth: true
                            // What's left of the screen under the question, the words below it in sight.
                            // (No taller than the plan itself needs at this width.)
                            Layout.preferredHeight: Math.max(160, Math.min(w.small ? 320 : 460, flick.height - (w.small ? 260 : 190),
                                                                           w.preview.w > 0 ? body.width * w.preview.h / w.preview.w + 16 : 460))
                            Layout.minimumHeight: Layout.preferredHeight
                            Layout.maximumHeight: Layout.preferredHeight
                            color: "#3b2a1a"
                            radius: 6
                            clip: true
                            readonly property real s: w.preview.w > 0
                                ? Math.min((width - 16) / w.preview.w, (height - 16) / w.preview.h, 0.9) : 1
                            Item {
                                x: (plan.width - w.preview.w * plan.s) / 2
                                y: (plan.height - w.preview.h * plan.s) / 2
                                Repeater {
                                    model: w.preview.zones
                                    delegate: Rectangle {
                                        id: shape
                                        required property var modelData
                                        readonly property bool isTable: modelData.kind === "table"
                                        readonly property bool isArea: !isTable && modelData.props.decor === "area"
                                        x: modelData.x * plan.s
                                        y: modelData.y * plan.s
                                        width: modelData.w * plan.s
                                        height: modelData.h * plan.s
                                        radius: modelData.shape === "circle" ? width / 2 : 4
                                        color: isTable ? "#2a313d" : isArea ? (modelData.props.color || "#4a3826") : "#6b4a2b"
                                        border.color: isTable ? "#c9d1dc" : "transparent"
                                        border.width: isTable ? 1 : 0
                                        Text {
                                            textFormat: Text.PlainText
                                            anchors.centerIn: shape.isTable ? parent : undefined
                                            x: shape.isTable ? 0 : 8
                                            y: shape.isTable ? 0 : 4
                                            width: shape.isTable ? shape.width - 4 : implicitWidth
                                            horizontalAlignment: Text.AlignHCenter
                                            fontSizeMode: shape.isTable ? Text.HorizontalFit : Text.FixedSize
                                            minimumPixelSize: 7
                                            text: shape.modelData.label
                                            color: "white"
                                            opacity: shape.isArea ? 0.75 : 1
                                            font.pixelSize: shape.isTable ? Math.max(9, Math.min(18, shape.height * 0.35)) : 14
                                            font.bold: shape.isTable
                                        }
                                    }
                                }
                            }
                        }
                        Label {
                            textFormat: Text.PlainText
                            objectName: "tablesSummary"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: w.ink
                            font.pixelSize: 16
                            text: qsTr("%n table(s): %1.", "", w.preview.count).arg(
                                      w.preview.names.length > 8 ? w.preview.names.slice(0, 4).join(", ") + " … " + w.preview.names.slice(-2).join(", ")
                                                                 : w.preview.names.join(", "))
                        }
                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: w.ink
                            opacity: 0.75
                            font.pixelSize: 15
                            text: qsTr("These take the place of the tables on your floor plans; the buttons around them stay. A backup is made first, and you can put the old tables back.")
                        }
                    }

                    // --- made ---
                    ColumnLayout {
                        visible: w.at === 4
                        Layout.fillWidth: true
                        spacing: 14
                        Label {
                            textFormat: Text.PlainText
                            objectName: "tablesMade"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: "#7bd88f"
                            font.pixelSize: w.small ? 22 : 28
                            font.bold: true
                            text: qsTr("✓ %n table(s) are on the floor plan.", "", w.made)
                        }
                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: w.ink
                            font.pixelSize: 16
                            text: qsTr("To move one, make it round or rename it: Manager → Edit Pages, on the Tables page.")
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 10
                            PosButton {
                                objectName: "tablesSee"
                                implicitHeight: 64
                                highlighted: true
                                text: qsTr("See the Tables")
                                onClicked: w.controller.showPage("tables")
                            }
                            PosButton {
                                objectName: "tablesUndo"
                                visible: w.controller ? w.controller.canUndoFloor : false
                                implicitHeight: 64
                                text: qsTr("Put the Old Tables Back")
                                onClicked: if (w.controller.undoFloor()) w.at = 0
                            }
                        }
                    }
                }
            }

            Label {
                textFormat: Text.PlainText
                objectName: "tablesMissing"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignRight
                visible: text !== "" && w.at < 4
                color: "#f5b940"
                font.pixelSize: 15
                text: w.missing
            }
            RowLayout {
                Layout.fillWidth: true
                visible: w.at < 4
                spacing: 8
                PosButton {
                    objectName: "tablesBack"
                    visible: w.at > 0
                    implicitHeight: 60
                    Layout.preferredWidth: w.small ? 110 : 160
                    text: qsTr("‹ Back")
                    onClicked: w.at -= 1
                }
                Item { Layout.fillWidth: true }
                PosButton {
                    objectName: "tablesNext"
                    implicitHeight: 60
                    Layout.preferredWidth: w.small ? 200 : 280
                    highlighted: true
                    enabled: w.missing === ""
                    text: w.at === 3 ? qsTr("Use These Tables") : qsTr("Next ›")
                    onClicked: w.next()
                }
            }
        }
    }
}
