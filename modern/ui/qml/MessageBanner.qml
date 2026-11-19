import QtQuick
import QtQuick.Layouts

// A message from another screen ("86 salmon", "need a runner"), across the
// top of the screens it's for until someone here touches OK. A phone
// (compact): up to two lines, the page moved down under it rather than covered.
Rectangle {
    id: banner
    property PosService pos
    property bool kitchenScreen: false   // kitchen, bar and expo screens
    property var seen: ({})              // message ids already OK'd here
    property bool compact: false

    // Newest first: the ones for this screen not yet OK'd.
    readonly property var waiting: {
        if (!pos)
            return []
        const me = pos.userName
        // A posted message comes back for each person who logs in, until it expires.
        return pos.messages.filter(m => !seen[m.posted ? m.id + "|" + me : m.id]
            && (m.to === "all" || (m.to === "kitchen" && kitchenScreen) || (m.to === "floor" && !kitchenScreen)
                || (me !== "" && m.to === me)))
    }
    readonly property var shown: waiting.length ? waiting[waiting.length - 1] : null   // the oldest first
    // "T5: food is up": green, with a chime when it comes.
    readonly property bool ready: shown !== null && shown.kind === "ready"
    signal chime()
    property var chimed: ({})
    onWaitingChanged: {
        let fresh = false
        const c = Object.assign({}, chimed)
        for (const m of waiting)
            if (m.kind === "ready" && !c[m.id]) {
                c[m.id] = true
                fresh = true
            }
        if (fresh) {
            chimed = c
            chime()
        }
    }

    visible: shown !== null
    height: visible ? (compact ? Math.max(48, compactText.implicitHeight + 14) : 74) : 0
    color: ready ? Ui.go : Ui.accent
    border.color: "white"
    border.width: 2

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 10
        spacing: 14
        Text {
            id: compactText
            visible: banner.compact
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            maximumLineCount: 2
            // What they typed, as typed (escaped), after who sent it in bold.
            text: banner.shown ? "<b>" + Qt.escapeHtml(String(banner.shown.from)) + ":</b> "
                                 + Qt.escapeHtml(String(banner.shown.text)) : ""
            textFormat: Text.StyledText
            color: "white"
            font.pixelSize: Ui.px(17)
            elide: Text.ElideRight
        }
        Column {
            visible: !banner.compact
            Layout.fillWidth: true
            Text {
                textFormat: Text.PlainText
                text: !banner.shown ? ""
                    : banner.shown.posted ? qsTr("From %1 · until %2").arg(banner.shown.from).arg(banner.shown.until)
                    : qsTr("From %1 · %2").arg(banner.shown.from).arg(banner.shown.time)
                color: "#d8e4ff"
                font.pixelSize: Ui.px(14)
            }
            Text {
                textFormat: Text.PlainText
                width: parent.width
                text: banner.shown ? banner.shown.text : ""
                color: "white"
                font.pixelSize: Ui.px(26)
                font.bold: true
                elide: Text.ElideRight
            }
        }
        Text {
            textFormat: Text.PlainText
            visible: banner.waiting.length > 1
            text: qsTr("+%1 more").arg(banner.waiting.length - 1)
            color: "white"
            font.pixelSize: Ui.px(16)
        }
        WidgetKey {
            objectName: "messageOk"
            Layout.preferredWidth: banner.compact ? 72 : 120
            Layout.fillHeight: true
            Layout.margins: banner.compact ? 4 : 8
            text: banner.ready ? qsTr("Got It") : qsTr("OK")
            baseColor: Ui.goDark
            fontScale: 0.4
            onClicked: {
                const s = Object.assign({}, banner.seen)
                s[banner.shown.posted ? banner.shown.id + "|" + banner.pos.userName : banner.shown.id] = true
                banner.seen = s
            }
        }
    }
}
