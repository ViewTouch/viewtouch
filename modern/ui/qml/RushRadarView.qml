import QtQuick
import QtQuick.Layouts

// Rush Radar: each kitchen station's next half hour, at a glance. A row a
// station: its wait now, bars for the next 30 minutes (every 5), and the
// worst of it ("18 min at 7:40"), yellow when tickets will run long, red
// when they'll be late. compact: the headline only (a kitchen screen's).
Item {
    id: r
    property PosService pos
    property real unit: 16
    property string face: Ui.font
    property bool compact: false
    // Read again every half minute: what's coming moves on with the clock.
    property int tick: 0
    readonly property var radar: pos && tick >= 0 ? pos.rushRadar : ({})
    // One station only (a station's own kitchen screen); "": all of them.
    property string station: ""
    readonly property var mine: station !== "" ? (radar.stations ?? []).find(s => s.id === station) : undefined
    // A screen for a whole kitchen (a printer's: several stations), not one: all of them.
    readonly property bool all: station === "" || mine === undefined
    // The warning: this station's, or the worst one's.
    readonly property string headline: all ? (radar.headline ?? "")
        : !mine || mine.level === "ok" ? ""
        : mine.peakMinute > 0 ? qsTr("%1 backs up to %2 min at %3.").arg(mine.name).arg(mine.peakWait).arg(mine.peakAt)
        : qsTr("%1 is at %2 min now.").arg(mine.name).arg(mine.peakWait)
    readonly property var stations: (radar.stations ?? []).filter(s => (all || s.id === station)
                                                                     && (s.queue > 0 || s.coming > 0 || s.peakWait > 0))
    readonly property color muted: Ui.muted
    function tint(level) { return level === "late" ? "#ff8a8f" : level === "busy" ? "#f5b940" : "#5fd08a" }
    implicitHeight: compact ? head.implicitHeight : col.implicitHeight

    // A handheld or kitchen screen gets it only while it's asked for (its
    // page is up: not only while this shows, a warning strip shows when there's one).
    property bool active: true
    property bool asked: false
    function ask(on) {
        if (!pos || on === asked) return
        asked = on
        pos.want("rushRadar", on)
    }
    onActiveChanged: ask(active)
    Component.onCompleted: ask(active)
    Component.onDestruction: ask(false)
    Timer { interval: 30000; repeat: true; running: r.active && !!r.pos; onTriggered: r.tick++ }

    ColumnLayout {
        id: col
        anchors.fill: parent
        spacing: r.unit * 0.35
        Text {
            id: head
            objectName: "rushHeadline"
            Layout.fillWidth: true
            text: r.headline !== "" ? "⚠ " + r.headline
                  : r.stations.length ? qsTr("The kitchen keeps up for the next %1 minutes.").arg(r.radar.horizon ?? 30)
                  : qsTr("Nothing on the line, nothing coming.")
            color: r.headline !== "" ? "#f5b940" : r.muted
            font.family: r.face
            font.pixelSize: r.unit * (r.compact ? 0.9 : 1.0)
            font.bold: r.headline !== ""
            elide: Text.ElideRight
        }
        Repeater {
            model: r.compact ? [] : r.stations
            delegate: RowLayout {
                id: row
                required property var modelData
                objectName: "rush-" + modelData.id
                Layout.fillWidth: true
                spacing: r.unit * 0.5
                Text {
                    Layout.preferredWidth: r.unit * 6
                    text: row.modelData.name
                    color: "white"
                    font.family: r.face
                    font.pixelSize: r.unit * 0.9
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    Layout.preferredWidth: r.unit * 10
                    text: qsTr("%1 min now · %2 ahead").arg(row.modelData.waitNow).arg(row.modelData.queue)
                    color: r.muted
                    font.family: r.face
                    font.pixelSize: r.unit * 0.75
                    fontSizeMode: Text.HorizontalFit   // smaller before "…"
                    minimumPixelSize: Math.round(font.pixelSize * 0.7)
                    elide: Text.ElideRight
                }
                // The next half hour: a bar every 5 minutes, as tall as the wait.
                Row {
                    Layout.fillWidth: true
                    Layout.preferredHeight: r.unit * 2
                    spacing: r.unit * 0.15
                    readonly property real most: Math.max(20, ...row.modelData.ahead.map(p => p.wait))
                    Repeater {
                        model: row.modelData.ahead
                        delegate: Item {
                            required property var modelData
                            width: (parent.width - parent.spacing * 6) / 7
                            height: parent.height
                            Rectangle {
                                id: bar
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: Math.max(2, parent.height * modelData.wait / parent.parent.most)
                                radius: 2
                                color: r.tint(modelData.wait >= (r.radar.late ?? 15) ? "late" : modelData.wait >= (r.radar.warn ?? 8) ? "busy" : "ok")
                                opacity: 0.85
                            }
                            // Its wait, and when (now, then every 5 minutes).
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 1
                                text: modelData.minute === 0 ? qsTr("now %1").arg(modelData.wait) : modelData.wait
                                color: Ui.deep
                                font.family: r.face
                                font.pixelSize: r.unit * 0.65
                                font.bold: true
                                visible: bar.height >= r.unit * 0.8
                            }
                            // Short bars: the number above them.
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: bar.top
                                visible: bar.height < r.unit * 0.8
                                text: modelData.minute === 0 ? qsTr("now %1").arg(modelData.wait) : modelData.wait
                                color: r.muted
                                font.family: r.face
                                font.pixelSize: r.unit * 0.65
                            }
                        }
                    }
                }
                Text {
                    Layout.preferredWidth: r.unit * 6.5
                    horizontalAlignment: Text.AlignRight
                    text: row.modelData.peakMinute > 0 ? qsTr("%1 min at %2").arg(row.modelData.peakWait).arg(row.modelData.peakAt)
                                                       : qsTr("%1 min").arg(row.modelData.peakWait)
                    color: r.tint(row.modelData.level)
                    font.family: r.face
                    font.pixelSize: r.unit * 0.9
                    font.bold: row.modelData.level !== "ok"
                }
            }
        }
        Text {
            visible: !r.compact && !(r.radar.history ?? true)
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: qsTr("It learns this store's usual nights from its sales: until a few weeks are in, only the tables already here count.")
            color: r.muted
            font.family: r.face
            font.pixelSize: r.unit * 0.7
        }
    }
}
