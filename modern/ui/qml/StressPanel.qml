import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// The stress test's numbers, over the screen while it runs (vtmodern
// --stress): how busy the pretend dinner is, and how the screen, the
// handhelds, the printers and the database keep up. It moves through the
// floor, host stand, kitchen, expo, dashboard and deliveries on its own
// (Pages: Stay keeps the one shown). At the end: passed or not, and the
// report.
Rectangle {
    id: panel
    objectName: "stressPanel"

    property QtObject stress: null
    property var controller: null
    readonly property var st: stress ? stress.stats : ({})
    readonly property bool done: st.finished ?? false
    property bool open: true
    property bool autoPages: true

    function clock(s) { s = Math.max(0, Math.round(s ?? 0)); return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0") }
    function ms(v) { return (v ?? 0) < 10 ? Number(v ?? 0).toFixed(1) : Math.round(v ?? 0) }

    anchors { top: parent.top; right: parent.right; margins: 8 }
    width: open ? Math.min(done ? 640 : 430, parent.width - 16) : pill.implicitWidth + 24
    height: open ? body.implicitHeight + 24 : 44
    z: 95
    radius: 10
    color: "#e6141820"
    border.color: done ? (st.passed ? "#2fa35a" : "#d64545") : "#3a4250"
    border.width: done ? 3 : 1

    // Around the restaurant: a page every 15 seconds. (Main.qml builds this
    // panel only during a stress test: a store's screen is never toured.)
    Timer {
        objectName: "stressPageTour"
        readonly property var pages: ["tables", "seating", "kitchen", "expo", "dashboard", "deliveries"]
        property int at: 0
        interval: 15000
        repeat: true
        running: panel.autoPages && !panel.done && panel.controller !== null
        onTriggered: { at = (at + 1) % pages.length; panel.controller.jumpTo(pages[at]) }
    }

    MouseArea { anchors.fill: parent }   // touches stop here

    RowLayout {
        id: pill
        visible: !panel.open
        anchors.centerIn: parent
        Label {
            text: panel.done ? (panel.st.passed ? qsTr("Stress test passed") : qsTr("Stress test: problems"))
                             : qsTr("Stress test %1").arg(panel.clock(panel.st.elapsed))
            color: "white"
            font.pixelSize: 16
            font.bold: true
        }
        Button { text: qsTr("Show"); onClicked: panel.open = true }
    }

    ColumnLayout {
        id: body
        visible: panel.open
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("Stress test") + " · " + (panel.st.phase ?? "")
                color: "white"
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideRight
            }
            Label {
                text: panel.clock(panel.st.elapsed) + " / " + panel.clock(panel.st.total)
                color: "#aab3c2"
                font.pixelSize: 15
            }
        }
        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, panel.st.total ?? 1)
            value: Math.min(panel.st.elapsed ?? 0, panel.st.total ?? 0)
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("%1 pretend staff, %2 handhelds · %3× real time · busy %4%")
                  .arg(panel.st.people ?? 0).arg(panel.st.handhelds ?? 0).arg(panel.st.speed ?? 0)
                  .arg(Math.round((panel.st.intensity ?? 0) * 100))
            color: "#aab3c2"
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        // The numbers, two columns.
        GridLayout {
            Layout.fillWidth: true
            visible: !panel.done
            columns: 2
            columnSpacing: 14
            rowSpacing: 3
            component Name: Label { color: "#aab3c2"; font.pixelSize: 14 }
            component Value: Label { color: "white"; font.pixelSize: 14; font.bold: true; Layout.fillWidth: true; horizontalAlignment: Text.AlignRight }

            Name { text: qsTr("Open checks · guests") }
            Value { text: (panel.st.open ?? 0) + " · " + (panel.st.guests ?? 0) }
            Name { text: qsTr("Waiting for a table") }
            Value { text: panel.st.waiting ?? 0 }
            Name { text: qsTr("Closed checks · sales") }
            Value { text: (panel.st.closed ?? 0) + " · $" + (panel.st.sales ?? "0.00") }
            Name { text: qsTr("Touches a minute") }
            Value { text: panel.st.actionsPerMinute ?? 0 }
            Name { text: qsTr("Screen delay: typical / 1 in 100") }
            Value {
                text: (panel.st.lagP50 ?? 0) + " / " + (panel.st.lagP99 ?? 0) + " ms"
                color: (panel.st.lagP99 ?? 0) >= 150 ? "#ff9a9e" : (panel.st.lagP99 ?? 0) >= 60 ? "#ffd479" : "white"
            }
            Name { text: qsTr("Worst delay · freezes over ¼ s") }
            Value {
                text: (panel.st.lagWorst ?? 0) + " ms · " + (panel.st.stalls ?? 0)
                color: (panel.st.stalls ?? 0) > 0 ? "#ff9a9e" : "white"
            }
            Name { text: qsTr("Frames drawn a second") }
            Value { text: panel.st.fps ?? 0 }
            Name { text: qsTr("Handheld touch: typical / 1 in 20") }
            Value { text: panel.ms((panel.st.remote ?? {}).p50) + " / " + panel.ms((panel.st.remote ?? {}).p95) + " ms" }
            Name { text: qsTr("This screen's commands: 1 in 20") }
            Value { text: panel.ms((panel.st.local ?? {}).p95) + " ms" }
            Name { text: qsTr("Tickets printed") }
            Value { text: panel.st.printed ?? 0 }
            Name { text: qsTr("Waiting to be saved") }
            Value { text: (panel.st.db ?? 0) + " (" + qsTr("most %1").arg(panel.st.dbWorst ?? 0) + ")" }
            Name { text: qsTr("Memory · CPU") }
            Value { text: (panel.st.memory ?? 0) + " MB · " + (panel.st.cpu ?? 0) + "%" }
            Name { text: qsTr("Refused (shouldn't have been)") }
            Value {
                text: (panel.st.refused ?? 0) + " (" + (panel.st.unexpected ?? 0) + ")"
                color: (panel.st.unexpected ?? 0) > 0 ? "#ff9a9e" : "white"
            }
        }

        // The end: the verdict and the report.
        Label {
            visible: panel.done
            Layout.fillWidth: true
            text: panel.st.passed ? qsTr("PASSED: every check balanced and saved; the screen kept up.")
                                  : qsTr("PROBLEMS FOUND: see the report.")
            color: panel.st.passed ? "#7be39b" : "#ff9a9e"
            font.pixelSize: 17
            font.bold: true
            wrapMode: Text.WordWrap
        }
        ScrollView {
            visible: panel.done
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(560, panel.parent ? panel.parent.height - 200 : 560)
            clip: true
            TextArea {
                readOnly: true
                text: panel.stress ? panel.stress.report : ""
                font.family: "DejaVu Sans Mono"
                font.pixelSize: 12
                color: "white"
                wrapMode: TextEdit.Wrap
                background: Rectangle { color: "#0d1015"; radius: 6 }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button {
                text: qsTr("Hide")
                implicitHeight: 40
                onClicked: panel.open = false
            }
            Button {
                visible: !panel.done
                text: panel.autoPages ? qsTr("Pages: Auto") : qsTr("Pages: Stay")
                implicitHeight: 40
                onClicked: panel.autoPages = !panel.autoPages
            }
            Item { Layout.fillWidth: true }
            Button {
                visible: panel.done
                text: qsTr("Close ViewTouch")
                implicitHeight: 40
                onClicked: Qt.quit()
            }
        }
    }
}
