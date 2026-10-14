import QtQuick
import QtQuick.Layouts

// Who is logged in, their clock status, and their open checks.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property color ink: zone.st.textColor ?? "white"
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property real unit: Math.min(w.height * 0.14, w.width * 0.07)
    readonly property int myChecks: {
        if (!pos) return 0
        return pos.openChecks.filter(c => c.mine).length
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: w.unit * 0.6
        spacing: w.unit * 0.25

        Text {
            Layout.fillWidth: true
            text: w.pos && w.pos.loggedIn ? w.pos.userName : qsTr("Nobody logged in")
            color: w.ink
            font.family: w.face
            font.pixelSize: w.unit * 1.2
            font.bold: true
            elide: Text.ElideRight
        }
        Text {
            visible: w.pos && w.pos.loggedIn
            Layout.fillWidth: true
            text: w.pos ? w.pos.roleName(w.pos.userRole) : ""
            color: "#8a94a6"
            font.family: w.face
            font.pixelSize: w.unit * 0.8
        }
        Text {
            visible: w.pos && w.pos.loggedIn
            Layout.fillWidth: true
            text: !w.pos || !w.pos.clockedIn ? qsTr("Not clocked in")
                 : w.pos.onBreakSince ? qsTr("On break since %1").arg(w.pos.onBreakSince)
                                      : qsTr("On the clock since %1").arg(w.pos.clockedInSince)
            color: w.pos && w.pos.clockedIn && !w.pos.onBreakSince ? "#7ee2a8" : "#f5b940"
            font.family: w.face
            font.pixelSize: w.unit * 0.8
            wrapMode: Text.WordWrap
        }
        Text {
            visible: w.pos && w.pos.loggedIn
            Layout.fillWidth: true
            text: (w.myChecks === 1 ? qsTr("1 open check") : qsTr("%1 open checks").arg(w.myChecks))
                  + (w.pos && w.pos.tipsOwed ? "  ·  " + qsTr("tips owed %1").arg(w.pos.tipsOwed) : "")
            color: w.ink
            font.family: w.face
            font.pixelSize: w.unit * 0.8
            wrapMode: Text.WordWrap
        }
        Text {
            visible: w.pos !== null && w.pos.loggedIn && w.pos.nextShift !== ""
            Layout.fillWidth: true
            text: qsTr("Next shift: %1").arg(w.pos ? w.pos.nextShift : "")
            color: "#8fb6ff"
            font.family: w.face
            font.pixelSize: w.unit * 0.8
            elide: Text.ElideRight
        }
        // Server banks: the cash this person has to turn in.
        Text {
            readonly property var bank: w.pos ? w.pos.drawer : ({})
            visible: w.pos && w.pos.loggedIn && bank.mode === "serverBank" && (bank.open ?? false)
            Layout.fillWidth: true
            text: qsTr("Cash in your bank: %1").arg(bank.expected ?? "")
            color: "#f5b940"
            font.family: w.face
            font.pixelSize: w.unit * 0.8
        }
        Item { Layout.fillHeight: true }
    }
}
