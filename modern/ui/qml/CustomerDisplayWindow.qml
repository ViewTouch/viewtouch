import QtQuick

// The customer display as a window of its own, full screen on the second
// monitor (desktop sessions; a kiosk splits its one window instead).
Window {
    id: win
    required property PosService pos
    width: 1024
    height: 600
    visible: true
    color: "#0f1318"
    title: qsTr("ViewTouch — Customer Display")
    // Manager -> Terminals: the Now Serving board instead of their order.
    Loader {
        // Inside the safe area (a guest-facing tablet's notch or rounded
        // corners); the window's color still fills it all.
        anchors {
            fill: parent
            topMargin: parent.SafeArea.margins.top
            bottomMargin: parent.SafeArea.margins.bottom
            leftMargin: parent.SafeArea.margins.left
            rightMargin: parent.SafeArea.margins.right
        }
        sourceComponent: (win.pos?.terminalGuestScreen ?? "") === "board" ? guestBoard : guestOrder
        Component { id: guestOrder; CustomerDisplay { pos: win.pos } }
        Component { id: guestBoard; NowServing { pos: win.pos } }
    }
}
