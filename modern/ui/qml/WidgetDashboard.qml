import QtQuick
import QtQuick.Layouts

// Manager -> Dashboard: today so far. Sales against the same day last week
// (by this time), labor as a share of sales, open checks, kitchen times,
// who's on the clock, the best sellers and what's running low.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var d: pos ? pos.dashboard : ({})
    readonly property var sales: d.sales ?? ({})
    readonly property var labor: d.labor ?? ({})
    readonly property var kitchen: d.kitchen ?? ({})
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property color ink: zone.st.textColor ?? "white"
    // Upright (a phone): the numbers two a row, the lists one a row, scrolled.
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real unit: narrow ? Math.max(12, Math.min(w.width / 22, 40))
                                        : Math.max(12, Math.min(w.width / 60, w.height / 34))
    readonly property color muted: "#8a94a6"
    readonly property color good: "#5fd08a"
    readonly property color warn: "#f5b940"
    readonly property color bad: "#ff8a8f"

    // Labor keeps adding up while nothing else happens: once a minute.
    Timer {
        interval: 60000
        repeat: true
        running: w.visible && w.pos !== null
        onTriggered: w.pos.refreshDay()
    }

    component Card: Rectangle {
        default property alias content: inner.data
        property string title
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: 12
        color: "#1d2128"
        ColumnLayout {
            id: inner
            anchors.fill: parent
            anchors.margins: w.unit * 0.8
            spacing: w.unit * 0.25
            Text {
                text: parent.parent.title
                color: w.muted
                font.family: w.face
                font.pixelSize: w.unit * 0.8
                font.bold: true
            }
        }
    }
    component Big: Text {
        color: w.ink
        font.family: w.face
        font.pixelSize: w.unit * 2.2
        font.bold: true
        Layout.fillWidth: true
        elide: Text.ElideRight
    }
    component Small: Text {
        color: w.muted
        font.family: w.face
        font.pixelSize: w.unit * 0.8
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }
    component Row2: RowLayout {
        property string label
        property string value
        property color tint: w.ink
        Layout.fillWidth: true
        Text {
            Layout.fillWidth: true
            text: parent.label
            color: parent.tint
            font.family: w.face
            font.pixelSize: w.unit * 0.9
            elide: Text.ElideRight
        }
        Text {
            text: parent.value
            color: parent.tint
            font.family: w.face
            font.pixelSize: w.unit * 0.9
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: col.height
        interactive: w.narrow
        clip: true
    ColumnLayout {
        id: col
        width: parent.width
        height: w.narrow ? implicitHeight : w.height
        spacing: w.unit * 0.6

        // The numbers
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.preferredHeight: w.narrow ? 3 * w.unit * 6.5 : w.height * 0.32
            columns: w.narrow ? 2 : 5
            rowSpacing: w.unit * 0.6
            columnSpacing: w.unit * 0.6
            Card {
                objectName: "dashSales"
                title: qsTr("Net sales today")
                Big { text: w.sales.net ?? "" }
                Small {
                    visible: w.sales.lastWeek !== undefined
                    text: qsTr("%1 by this time on %2").arg(w.sales.lastWeek ?? "").arg(w.sales.lastWeekDay ?? "")
                }
                Text {
                    visible: w.sales.change !== undefined
                    text: (w.sales.change > 0 ? "▲ +" : w.sales.change < 0 ? "▼ " : "") + (w.sales.change ?? 0) + "%"
                    color: (w.sales.change ?? 0) >= 0 ? w.good : w.bad
                    font.family: w.face
                    font.pixelSize: w.unit * 1.1
                    font.bold: true
                }
                Item { Layout.fillHeight: true }
            }
            Card {
                title: qsTr("Checks")
                Big { text: w.sales.checks ?? 0 }
                Small { text: qsTr("%n guest(s)", "", w.sales.guests ?? 0) + "  ·  " + qsTr("average %1").arg(w.sales.average ?? "") }
                Item { Layout.fillHeight: true }
            }
            Card {
                objectName: "dashLabor"
                title: qsTr("Labor")
                Big {
                    text: w.labor.percent !== undefined ? w.labor.percent + "%" : (w.labor.cost ?? "")
                    color: (w.labor.percent ?? 0) > 35 ? w.bad : (w.labor.percent ?? 0) > 28 ? w.warn : w.ink
                }
                Small { text: w.labor.percent !== undefined ? qsTr("%1 of sales").arg(w.labor.cost ?? "") : qsTr("so far today") }
                Item { Layout.fillHeight: true }
            }
            Card {
                title: qsTr("Open checks")
                Big { text: w.d.open ? w.d.open.count : 0 }
                Small { text: qsTr("%1 still due").arg(w.d.open ? w.d.open.due : "") }
                Item { Layout.fillHeight: true }
            }
            Card {
                title: qsTr("Kitchen")
                Big {
                    text: w.kitchen.averageMinutes !== undefined ? qsTr("%1 min").arg(w.kitchen.averageMinutes) : "–"
                }
                Small {
                    text: (w.kitchen.waiting ?? 0) > 0
                          ? qsTr("%1 waiting, oldest %2 min").arg(w.kitchen.waiting).arg(w.kitchen.oldestMinutes)
                          : qsTr("average from sent to made; nothing waiting")
                    color: (w.kitchen.oldestMinutes ?? 0) >= 20 ? w.bad : w.muted
                }
                Item { Layout.fillHeight: true }
            }
        }

        // The lists
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: !w.narrow
            Layout.preferredHeight: w.narrow ? 3 * w.unit * 10 : -1
            columns: w.narrow ? 1 : 3
            rowSpacing: w.unit * 0.6
            columnSpacing: w.unit * 0.6
            Card {
                title: qsTr("On the clock (%1)").arg((w.labor.onClock ?? []).length)
                Repeater {
                    model: w.labor.onClock ?? []
                    delegate: Row2 {
                        required property var modelData
                        label: modelData.name + "  ·  " + modelData.job
                               + (modelData.overtime === "over" ? "  ·  " + qsTr("overtime")
                                  : modelData.overtime === "soon" ? "  ·  " + qsTr("OT in %1 h").arg(modelData.overtimeLeft) : "")
                        value: modelData.onBreak ? qsTr("on break") : qsTr("since %1").arg(modelData.since)
                        tint: modelData.overTwelve || modelData.overtime === "over" ? w.bad
                            : modelData.onBreak || modelData.overtime === "soon" ? w.warn : w.ink
                    }
                }
                Small { visible: (w.labor.onClock ?? []).length === 0; text: qsTr("Nobody is clocked in.") }
                Item { Layout.fillHeight: true }
            }
            Card {
                objectName: "dashTop"
                title: qsTr("Best sellers today")
                Repeater {
                    model: w.d.top ?? []
                    delegate: Row2 {
                        required property var modelData
                        required property int index
                        label: (index + 1) + ".  " + modelData.name
                        value: modelData.count
                    }
                }
                Small { visible: (w.d.top ?? []).length === 0; text: qsTr("No sales yet.") }
                Item { Layout.fillHeight: true }
            }
            Card {
                objectName: "dashLow"
                title: qsTr("Running low")
                Repeater {
                    model: w.d.low ?? []
                    delegate: Row2 {
                        required property var modelData
                        label: modelData.name
                        value: modelData.out ? qsTr("OUT") : modelData.left
                        tint: modelData.out ? w.bad : w.warn
                    }
                }
                Small {
                    visible: (w.d.soldOut ?? []).length > 0
                    text: qsTr("Sold out: %1").arg((w.d.soldOut ?? []).join(", "))
                    color: w.bad
                }
                Small {
                    visible: (w.d.low ?? []).length === 0 && (w.d.soldOut ?? []).length === 0
                    text: qsTr("Nothing is running low.")
                }
                Item { Layout.fillHeight: true }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            Small {
                text: qsTr("As of %1. It updates as checks close and orders go out.").arg(w.d.at ?? "")
            }
            Text {
                objectName: "dashRequests"
                visible: (w.d.requestsWaiting ?? 0) > 0
                text: qsTr("%n request(s) waiting: Schedule → Requests", "", w.d.requestsWaiting ?? 0)
                color: w.warn
                font.family: w.face
                font.pixelSize: w.unit * 0.9
                font.bold: true
            }
        }
    }
    }
}
