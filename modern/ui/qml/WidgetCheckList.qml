import QtQuick
import QtQuick.Controls.Fusion

// Every open check as a card; touch one to work on it. props.mode:
//   (none)  open it
//   merge   merge it into the check you are on, then go back
//   closed  checks closed today (managers): reopen one and go to Settle
//   tabs    the bar's open tabs: open one
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property string mode: zone && zone.props && zone.props.mode ? zone.props.mode : ""
    readonly property var checks: {
        if (!pos) return []
        if (mode === "closed") return pos.closedChecks
        if (mode === "merge") return pos.openChecks.filter(c => !c.current)
        if (mode === "tabs") return pos.openChecks.filter(c => c.type === "tab")
        if (filter !== "") return pos.openChecks.filter(c => c.label === filter)   // a table's: all of them
        return showAll ? pos.openChecks : pos.openChecks.filter(c => c.forMe)
    }
    // The closed checks come to a handheld only while a list shows them.
    readonly property bool wantsClosed: pos !== null && mode === "closed"
    property bool asked: false
    function ask(on) {
        if (on === asked || !pos) return
        asked = on
        pos.want("closedChecks", on)
    }
    onWantsClosedChanged: ask(wantsClosed)
    Component.onCompleted: ask(wantsClosed)
    Component.onDestruction: ask(false)

    // Open checks: yours (and the counter's kiosk orders), or everyone's to see.
    property bool showAll: false
    readonly property bool choosesWhose: mode === "" && filter === ""
    // A table with several checks (after a split) shows only its checks.
    readonly property string filter: pos ? pos.checkFilter : ""

    // My Checks | All Checks
    Row {
        id: whose
        visible: w.choosesWhose
        anchors { left: parent.left; top: parent.top; margins: 16 }
        height: visible ? 112 : 0   // a finger's size on a phone too
        spacing: 10
        Repeater {
            model: [{ all: false, text: qsTr("My Checks") }, { all: true, text: qsTr("All Checks") }]
            delegate: WidgetKey {
                required property var modelData
                objectName: modelData.all ? "allChecks" : "myChecks"
                width: 280
                height: whose.height
                fontScale: 0.36
                text: modelData.text
                accent: w.showAll === modelData.all
                onClicked: w.showAll = modelData.all
            }
        }
    }

    Rectangle {
        id: banner
        visible: w.filter !== ""
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        height: visible ? 64 : 0
        radius: 10
        color: "#2b62b0"
        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 20
            text: qsTr("Checks at %1 — touch here to show all").arg(w.filter)
            color: "white"
            font.family: w.face
            font.pixelSize: 26
        }
        TapHandler { onTapped: { if (TouchGuard.covered(point.scenePressPosition)) return; w.pos.checkFilter = "" } }
    }

    GridView {
        id: grid
        ScrollBar.vertical: TouchScrollBar { id: gridBar; needed: Math.ceil(grid.count / grid.columns) * (170) > grid.height }
        anchors.fill: parent
        anchors.margins: 16
        anchors.topMargin: whose.visible ? 144 : banner.visible ? 96 : 16
        clip: true
        readonly property int columns: Math.max(1, Math.floor((width - gridBar.width - 4) / 320))
        cellWidth: Math.max(280, (width - gridBar.room) / columns)
        cellHeight: 170
        model: w.checks

        delegate: Item {
            id: card
            required property var modelData
            width: grid.cellWidth
            height: grid.cellHeight

            ZoneShape {
                anchors.fill: parent
                anchors.margins: 8
                st: ({ fill: tap.pressed ? "#3b4556" : (card.modelData.mine ? "#1f5f3a" : "#2d3440"),
                       frame: "raised", frameWidth: 3, radius: 14, shadow: 4 })
                // Someone else's you may not open (a manager's PIN can).
                opacity: card.locked ? 0.55 : 1
            }
            readonly property bool locked: w.mode === "" && card.modelData.mayOpen === false
            Text {
                visible: card.locked
                anchors { right: parent.right; top: parent.top; margins: 22 }
                text: "🔒"
                font.pixelSize: 24
            }
            Column {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 4
                Text {
                    width: parent.width
                    text: card.modelData.label
                    color: "white"
                    font.family: w.face
                    font.pixelSize: 30
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    text: card.modelData.total
                    color: "white"
                    font.family: w.face
                    font.pixelSize: 26
                }
                Text {
                    width: parent.width
                    text: (card.modelData.customer && card.modelData.customer !== card.modelData.label
                           ? card.modelData.customer + " · " : "")
                          + (w.mode === "closed"
                             ? qsTr("%1 · closed %2 · #%3").arg(card.modelData.server).arg(card.modelData.closed).arg(card.modelData.id)
                             : card.modelData.due ? qsTr("%1 · ready %2").arg(card.modelData.server).arg(card.modelData.due)
                             : qsTr("%1 · %2 min").arg(card.modelData.server).arg(card.modelData.minutes))
                          + (card.modelData.busyOn ? " · " + qsTr("on %1").arg(card.modelData.busyOn) : "")
                    color: "#b8c0cc"
                    font.family: w.face
                    font.pixelSize: 20
                    elide: Text.ElideRight
                }
            }
            TapHandler {
                id: tap
                onTapped: {
                    if (TouchGuard.covered(point.scenePressPosition)) return   // the keyboard's touch
                    if (w.mode === "merge") {
                        w.pos.mergeCheck(card.modelData.id)
                        w.zone.controller.goBack()
                    } else if (w.mode === "closed") {
                        w.pos.reopenCheck(card.modelData.id)
                        w.zone.controller.jumpTo("settle")
                    } else {
                        w.zone.controller.openCheck(card.modelData.id)
                    }
                }
            }
        }

        Text {
            anchors.centerIn: parent
            visible: grid.count === 0
            text: w.choosesWhose && !w.showAll ? qsTr("You have no open checks")
                 : w.mode === "closed" ? qsTr("No checks closed today")
                 : w.mode === "tabs" ? qsTr("No tabs open. Touch New Tab… to start one.")
                 : w.mode === "merge" ? qsTr("No other open checks") : qsTr("No open checks")
            color: "#8a94a6"
            font.family: w.face
            font.pixelSize: 32
        }
    }
}
