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
    CustomerDisplay { anchors.fill: parent; pos: win.pos }
}
