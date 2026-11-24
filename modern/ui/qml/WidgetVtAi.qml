import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Ask VT Ai: a manager's question about the store's sales ("What sold best
// this week?"), answered on this computer by VT Ai from one of ViewTouch's
// own reports, which opens a touch away. It reads; it never changes anything.
Item {
    id: w
    function fingers(px) {
        return zone ? Math.max(px, zone.touch(Math.max(px, 48))) : px
    }
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var vtai: pos ? (pos.vtai ?? {}) : ({})
    readonly property bool mine: vtai.task === "ask"
    readonly property bool busy: mine && vtai.busy === true
    readonly property var examples: [qsTr("What sold best this week?"), qsTr("What were today's sales?"),
                                     qsTr("What was our busiest hour yesterday?"), qsTr("Who sold the most last week?"),
                                     qsTr("Which items make the least money?")]
    function ask(q) {
        question.text = q
        if (pos && q.trim() !== "") pos.vtaiAsk(q)
    }
    Component.onCompleted: {
        if (pos && !busy) pos.vtaiClear()
        device.ask("status")
    }

    // VT Ai's service on this computer (the store's): on, off, its model.
    QtObject {
        id: device
        property bool here: false        // this computer can change it
        property bool working: false
        property string state: ""        // NOTINSTALLED, NOMODEL, RUNNING, STOPPED
        property string model: ""
        property string said: ""
        function ask(what) {
            if (!w.pos) return
            working = what !== "status"
            here = w.pos.vtaiDevice(what)
            if (!here) working = false
            else if (what === "model") said = qsTr("Downloading VT Ai's model (about 2.5 GB): this takes a while; the register works as usual meanwhile.")
        }
    }
    Connections {
        target: w.pos
        function onVtaiDeviceSaid(what, said, ok) {
            if (what === "status") {
                const words = said.split(" ")
                device.state = ok ? words[0] : ""
                device.model = ok ? words.slice(1).join(" ") : ""
                return
            }
            device.working = false
            device.said = ok ? "" : said   // (done: the status says so)
            device.ask("status")
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 10
        // On this computer: VT Ai on or off, and its model.
        RowLayout {
            objectName: "vtaiSetup"
            Layout.fillWidth: true
            visible: device.here && device.state !== ""
            spacing: 8
            Label {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: Ui.px(15)
                text: device.working ? (device.said || qsTr("Working…"))
                    : (device.said ? device.said + " " : "")
                      + (device.state === "NOTINSTALLED" ? qsTr("VT Ai isn't installed on this computer: sudo apt install vtmodern-ai")
                        : device.state === "NOMODEL" ? qsTr("VT Ai needs its model (about 2.5 GB): Get the Model.")
                        : device.state === "RUNNING" ? qsTr("VT Ai is on (%1).").arg(device.model)
                        : device.state === "STOPPED" ? qsTr("VT Ai is off.") : "")
            }
            MenuTouchButton {
                objectName: "vtaiOnOff"
                visible: device.state === "RUNNING" || device.state === "STOPPED"
                enabled: !device.working
                text: device.state === "RUNNING" ? qsTr("Turn Off") : qsTr("Turn On")
                onClicked: { device.said = ""; device.ask(device.state === "RUNNING" ? "off" : "on") }
            }
            MenuTouchButton {
                visible: device.state === "NOMODEL" || device.state === "RUNNING" || device.state === "STOPPED"
                enabled: !device.working
                text: device.state === "NOMODEL" ? qsTr("Get the Model") : qsTr("Update the Model")
                onClicked: device.ask("model")
            }
            MenuTouchButton {
                visible: device.state === "NOMODEL"
                enabled: !device.working
                text: qsTr("From a USB Stick")
                onClicked: { device.said = ""; device.ask("modelUsb") }
            }
        }
        Label {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.8
            font.pixelSize: Ui.px(15)
            text: w.vtai.available
                  ? qsTr("Ask about this store's sales, items, staff, tables or customers. VT Ai answers here on this computer from ViewTouch's reports; nothing leaves the store.")
                  : qsTr("VT Ai isn't running on this computer (Manager → VT Ai).")
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TextField {
                id: question
                objectName: "vtaiQuestion"
                Layout.fillWidth: true
                Layout.preferredHeight: w.fingers(56)
                enabled: !w.busy && w.vtai.available === true
                maximumLength: 500
                placeholderText: qsTr("What sold best this week?")
                font.pixelSize: Ui.px(18)
                onAccepted: w.ask(text)
            }
            MenuTouchButton {
                objectName: "vtaiAsk"
                highlighted: true
                Layout.preferredHeight: w.fingers(56)
                enabled: !w.busy && w.vtai.available === true && question.text.trim() !== ""
                text: qsTr("Ask")
                onClicked: w.ask(question.text)
            }
        }
        // Ideas, a touch each.
        Flow {
            Layout.fillWidth: true
            spacing: 8
            visible: !w.busy && !(w.mine && w.vtai.answer)
            Repeater {
                model: w.examples
                delegate: MenuTouchButton {
                    required property string modelData
                    enabled: w.vtai.available === true
                    font.pixelSize: Ui.px(14)
                    text: modelData
                    onClicked: w.ask(modelData)
                }
            }
        }
        // Working on it, or its answer.
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 10
            color: Qt.rgba(0.5, 0.5, 0.5, 0.08)
            border.color: Qt.rgba(0.5, 0.5, 0.5, 0.3)
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12
                BusyIndicator { visible: w.busy; running: visible; Layout.alignment: Qt.AlignHCenter }
                Label {
                    objectName: "vtaiAnswer"
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Ui.px(w.mine && w.vtai.answer ? 22 : 16)
                    color: w.mine && w.vtai.error ? Ui.warn : palette.windowText
                    text: !w.mine ? ""
                        : w.busy ? (w.vtai.progress || qsTr("Thinking…")) + "\n" + qsTr("This takes a minute on the store's computer; the register works as usual meanwhile.")
                        : w.vtai.error ? w.vtai.error
                        : (w.vtai.answer ?? "")
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    visible: w.mine && !w.busy && !!w.vtai.report && !!w.vtai.answer
                    opacity: 0.7
                    font.pixelSize: Ui.px(14)
                    text: !visible ? ""
                        : w.vtai.quick ? qsTr("From the %1 report, %2.").arg(w.vtai.report.title ?? "").arg(w.vtai.report.label ?? "")
                        : qsTr("From the %1 report, %2. VT Ai can be wrong: the report has the numbers.")
                              .arg(w.vtai.report.title ?? "").arg(w.vtai.report.label ?? "")
                }
                Item { Layout.fillHeight: true }
                RowLayout {
                    Layout.fillWidth: true
                    MenuTouchButton {
                        objectName: "vtaiOpenReport"
                        visible: w.mine && !w.busy && !!w.vtai.report
                        text: qsTr("Open the Report")
                        onClicked: {
                            const r = w.vtai.report, c = w.zone.controller
                            c.keep("reportOpen", r.id)
                            c.keep("reportPeriod", { period: r.period, from: r.from, to: r.to })
                            c.showPage("reports")
                        }
                    }
                    Item { Layout.fillWidth: true }
                    MenuTouchButton {
                        visible: w.busy
                        text: qsTr("Stop")
                        onClicked: w.pos.vtaiCancel()
                    }
                }
            }
        }
    }
}
