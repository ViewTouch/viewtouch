import QtQuick
import QtQuick.Controls.Fusion
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
    // Today's loss alerts (big voids, comps, refunds...), newest first.
    readonly property var alerts: pos ? pos.lossAlerts : []
    // An alert's check, in Find a Check (its items, payments and history).
    function openAlert(a) {
        if (!a.checkId) return
        zone.controller.keep("findCheck", { query: "#" + a.checkId, id: a.checkId })
        zone.controller.showPage("find-check")
    }
    // The profit coach: ways to earn more, from the last 4 weeks.
    readonly property var coach: pos ? pos.coach : ({})
    readonly property var tips: coach.tips ?? []
    // A tip's button: what it says, and what it does.
    function tipAction(t) {
        switch (t.kind) {
        case "raise": return qsTr("Change the Price…")
        case "gem": return qsTr("Make It a Favorite")
        case "weak": case "never": return qsTr("Open It")
        case "quiet": return qsTr("Set Up a Happy Hour…")
        case "discounts": return qsTr("See Who Gave Them")
        default: return ""
        }
    }
    function doTip(t) {
        const c = zone.controller
        switch (t.kind) {
        case "raise":
            c.keep("menuBuilderOpen", t.item)
            c.keep("menuBuilderPrice", t.price)   // filled in; nothing changes until Save
            c.showPage("menu-builder")
            break
        case "gem":
            w.pos.changeMenuItems([t.item], { favorite: true })
            break
        case "weak": case "never":
            c.keep("menuBuilderOpen", t.item)
            c.showPage("menu-builder")
            break
        case "quiet":
            c.showPage("admin-meal-periods")
            break
        case "discounts":
            c.keep("reportOpen", "exceptions")
            c.showPage("reports")
            break
        }
    }
    // Seen once the Dashboard shows them (the Manager page's count goes).
    Timer {
        interval: 1500
        running: w.visible && w.alerts.some(a => a.new)
        onTriggered: w.pos.alertsSeen()
    }
    readonly property string face: zone.st.font ?? Ui.font
    readonly property color ink: zone.st.textColor ?? "white"
    // Upright (a phone): the numbers two a row, the lists one a row, scrolled.
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real unit: narrow ? Math.max(12, Math.min(w.width / 22, 40))
                                        : Math.max(12, Math.min(w.width / 60, w.height / 34))
    readonly property color muted: Ui.muted
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
        color: Ui.sunken
        ColumnLayout {
            id: inner
            anchors.fill: parent
            anchors.margins: w.unit * 0.8
            spacing: w.unit * 0.25
            Text {
                textFormat: Text.PlainText
                text: parent.parent.title
                color: w.muted
                font.family: w.face
                font.pixelSize: w.unit * 0.8
                font.bold: true
            }
        }
    }
    component Big: Text {
        textFormat: Text.PlainText
        color: w.ink
        font.family: w.face
        font.pixelSize: w.unit * 2.2
        font.bold: true
        Layout.fillWidth: true
        elide: Text.ElideRight
    }
    component Small: Text {
        textFormat: Text.PlainText
        color: w.muted
        font.family: w.face
        font.pixelSize: w.unit * 0.8
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }
    // A card's list: scrolls inside the card when it's longer (a busy day's
    // staff), with a bar on the side saying there's more.
    component List: ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: w.unit * 1.4
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        spacing: w.unit * 0.25
        ScrollIndicator.vertical: ScrollIndicator { active: true }
    }
    component Row2: RowLayout {
        property string label
        property string value
        property color tint: w.ink
        Layout.fillWidth: true
        Text {
            textFormat: Text.PlainText
            Layout.fillWidth: true
            text: parent.label
            color: parent.tint
            font.family: w.face
            font.pixelSize: w.unit * 0.9
            elide: Text.ElideRight
        }
        Text {
            textFormat: Text.PlainText
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

        // Worth a look: today's alerts, the newest in full.
        Rectangle {
            objectName: "dashAlerts"
            visible: w.alerts.length > 0
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.preferredHeight: visible ? alertRow.implicitHeight + w.unit * 0.8 : 0
            radius: 10
            color: Qt.rgba(0.8, 0.2, 0.2, 0.22)
            border.color: w.bad
            RowLayout {
                id: alertRow
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: w.unit * 0.5 }
                spacing: w.unit * 0.5
                Text {
                    textFormat: Text.PlainText
                    text: "⚠"
                    color: w.bad
                    font.pixelSize: w.unit * 1.2
                }
                Text {
                    textFormat: Text.PlainText
                    objectName: "dashAlertText"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    text: w.alerts.length ? w.alerts[0].time + "  " + w.alerts[0].text : ""
                    color: w.ink
                    font.family: w.face
                    font.pixelSize: w.unit * 0.9
                    font.bold: true
                    MouseArea { anchors.fill: parent; onClicked: w.openAlert(w.alerts[0]) }
                }
                PosButton {
                    objectName: "dashAlertsAll"
                    text: qsTr("See All (%1)").arg(w.alerts.length)
                    font.pixelSize: w.unit * 0.8
                    onClicked: alertList.open()
                }
            }
        }

        // Ways to earn more: the best tip, the rest a touch away.
        Rectangle {
            objectName: "dashCoach"
            visible: w.tips.length > 0
            Layout.fillWidth: true
            Layout.fillHeight: false
            Layout.preferredHeight: visible ? coachRow.implicitHeight + w.unit * 0.8 : 0
            radius: 10
            color: Qt.rgba(0.2, 0.7, 0.4, 0.18)
            border.color: w.good
            RowLayout {
                id: coachRow
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: w.unit * 0.5 }
                spacing: w.unit * 0.5
                Text {
                    textFormat: Text.PlainText
                    text: "$"
                    color: w.good
                    font.pixelSize: w.unit * 1.2
                    font.bold: true
                }
                Text {
                    textFormat: Text.PlainText
                    objectName: "dashCoachText"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    text: w.tips.length ? w.tips[0].title + (w.tips[0].worthText ? "  ·  " + w.tips[0].worthText : "") : ""
                    color: w.ink
                    font.family: w.face
                    font.pixelSize: w.unit * 0.9
                    font.bold: true
                    MouseArea { anchors.fill: parent; onClicked: tipList.open() }
                }
                PosButton {
                    objectName: "dashCoachAll"
                    text: qsTr("Ways to Earn More (%1)").arg(w.tips.length)
                    font.pixelSize: w.unit * 0.8
                    onClicked: tipList.open()
                }
            }
        }

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
                    textFormat: Text.PlainText
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

        // Rush Radar: the kitchen's next half hour.
        Card {
            objectName: "dashRush"
            title: qsTr("Rush Radar: the next %1 minutes").arg(rush.radar.horizon ?? 30)
            Layout.fillHeight: false
            Layout.preferredHeight: rush.implicitHeight + w.unit * 4
            RushRadarView {
                id: rush
                active: w.visible
                Layout.fillWidth: true
                Layout.fillHeight: true
                pos: w.pos
                unit: w.unit
                face: w.face
            }
            // Labor in the moment: the next hour against who's on.
            Text {
                textFormat: Text.PlainText
                objectName: "dashStaffing"
                readonly property var staffing: w.labor.staffing ?? ({})
                visible: staffing.known === true
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: (staffing.action === "cut" ? "✂ " : staffing.action === "call" ? "☎ " : "✓ ") + (staffing.text ?? "")
                color: staffing.action === "call" ? w.bad : staffing.action === "cut" ? w.warn : w.good
                font.family: w.face
                font.pixelSize: w.unit * 0.9
                font.bold: staffing.action !== "ok"
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
                List {
                    objectName: "dashOnClock"
                    visible: count > 0
                    model: w.labor.onClock ?? []
                    delegate: Row2 {
                        width: ListView.view.width - w.unit * 0.6   // room for the scroll bar
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
                Item { Layout.fillHeight: true; visible: (w.labor.onClock ?? []).length === 0 }
            }
            Card {
                objectName: "dashTop"
                title: qsTr("Best sellers today")
                List {
                    visible: count > 0
                    model: w.d.top ?? []
                    delegate: Row2 {
                        width: ListView.view.width - w.unit * 0.6   // room for the scroll bar
                        required property var modelData
                        required property int index
                        label: (index + 1) + ".  " + modelData.name
                        value: modelData.count
                    }
                }
                Small { visible: (w.d.top ?? []).length === 0; text: qsTr("No sales yet.") }
                Item { Layout.fillHeight: true; visible: (w.d.top ?? []).length === 0 }
            }
            Card {
                objectName: "dashLow"
                title: qsTr("Running low")
                List {
                    visible: count > 0
                    model: w.d.low ?? []
                    delegate: Row2 {
                        width: ListView.view.width - w.unit * 0.6   // room for the scroll bar
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
                Item { Layout.fillHeight: true; visible: (w.d.low ?? []).length === 0 }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            Small {
                text: qsTr("As of %1. It updates as checks close and orders go out.").arg(w.d.at ?? "")
            }
            Text {
                textFormat: Text.PlainText
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

    Popup {
        id: alertList
        objectName: "dashAlertList"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(parent ? parent.width - 32 : 700, 760)
        height: Math.min(parent ? parent.height - 48 : 600, 640)
        modal: true
        padding: 16
        contentItem: ColumnLayout {
            spacing: 8
            Label { textFormat: Text.PlainText; text: qsTr("Worth a look today"); font.pixelSize: 20; font.bold: true }
            Label {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.75
                text: qsTr("Often there's a good reason. Touch one to see its check and its history. What counts: Store Settings → Voids and comps.")
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6
                model: w.alerts
                delegate: PosButton {
                    required property var modelData
                    width: ListView.view.width
                    lines: 2
                    font.bold: false
                    text: modelData.time + "   " + modelData.text
                    onClicked: {
                        alertList.close()
                        w.openAlert(modelData)
                    }
                }
            }
            PosButton { Layout.alignment: Qt.AlignRight; Layout.preferredWidth: 140; text: qsTr("Close"); onClicked: alertList.close() }
        }
    }

    Popup {
        id: tipList
        objectName: "dashTipList"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(parent ? parent.width - 32 : 760, 820)
        height: Math.min(parent ? parent.height - 48 : 640, 700)
        modal: true
        padding: 16
        contentItem: ColumnLayout {
            spacing: 8
            Label { textFormat: Text.PlainText; text: qsTr("Ways to earn more"); font.pixelSize: 20; font.bold: true }
            Label {
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                opacity: 0.75
                text: qsTr("From your own last 4 weeks of sales and costs. The money is an estimate. Not Now hides a tip for a month.")
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 10
                model: w.tips
                delegate: Rectangle {
                    id: tipCard
                    required property var modelData
                    width: ListView.view.width
                    implicitHeight: tipBody.implicitHeight + 20
                    radius: 8
                    color: Qt.rgba(1, 1, 1, 0.06)
                    border.color: Qt.rgba(1, 1, 1, 0.16)
                    ColumnLayout {
                        id: tipBody
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                textFormat: Text.PlainText
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.pixelSize: 17
                                font.bold: true
                                text: tipCard.modelData.title
                            }
                            Label {
                                textFormat: Text.PlainText
                                visible: text !== ""
                                color: w.good
                                font.pixelSize: 16
                                font.bold: true
                                text: tipCard.modelData.worthText ?? ""
                            }
                        }
                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            opacity: 0.85
                            font.pixelSize: 15
                            text: tipCard.modelData.why
                        }
                        RowLayout {
                            spacing: 8
                            PosButton {
                                objectName: "tipDo-" + tipCard.modelData.id
                                visible: text !== ""
                                highlighted: true
                                text: w.tipAction(tipCard.modelData)
                                onClicked: {
                                    const t = tipCard.modelData
                                    tipList.close()
                                    w.doTip(t)
                                }
                            }
                            PosButton {
                                objectName: "tipNotNow-" + tipCard.modelData.id
                                text: qsTr("Not Now")
                                onClicked: w.pos.coachNotNow(tipCard.modelData.id)
                            }
                        }
                    }
                }
            }
            Label {
                textFormat: Text.PlainText
                visible: w.tips.length === 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: w.coach.waiting ?? qsTr("Nothing to change: the menu, its prices and the week look healthy.")
            }
            PosButton { Layout.alignment: Qt.AlignRight; Layout.preferredWidth: 140; text: qsTr("Close"); onClicked: tipList.close() }
        }
    }
}
