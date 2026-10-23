import QtQuick
import QtQuick.Layouts

// A message from another screen ("86 salmon", "need a runner"), across the
// top of the screens it's for until someone here touches OK. A phone
// (compact): one line, the page moved down under it rather than covered.
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

    visible: shown !== null
    height: visible ? (compact ? 48 : 74) : 0
    color: "#2f6fd6"
    border.color: "white"
    border.width: 2

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 18
        anchors.rightMargin: 10
        spacing: 14
        Text {
            visible: banner.compact
            Layout.fillWidth: true
            function plain(t) { return String(t).replace(/&/g, "&amp;").replace(/</g, "&lt;") }   // what they typed, as typed
            text: banner.shown ? "<b>" + plain(banner.shown.from) + ":</b> " + plain(banner.shown.text) : ""
            textFormat: Text.StyledText
            color: "white"
            font.pixelSize: 17
            elide: Text.ElideRight
        }
        Column {
            visible: !banner.compact
            Layout.fillWidth: true
            Text {
                text: !banner.shown ? ""
                    : banner.shown.posted ? qsTr("From %1 · until %2").arg(banner.shown.from).arg(banner.shown.until)
                    : qsTr("From %1 · %2").arg(banner.shown.from).arg(banner.shown.time)
                color: "#d8e4ff"
                font.pixelSize: 14
            }
            Text {
                width: parent.width
                text: banner.shown ? banner.shown.text : ""
                color: "white"
                font.pixelSize: 26
                font.bold: true
                elide: Text.ElideRight
            }
        }
        Text {
            visible: banner.waiting.length > 1
            text: qsTr("+%1 more").arg(banner.waiting.length - 1)
            color: "white"
            font.pixelSize: 16
        }
        WidgetKey {
            objectName: "messageOk"
            Layout.preferredWidth: banner.compact ? 72 : 120
            Layout.fillHeight: true
            Layout.margins: banner.compact ? 4 : 8
            text: qsTr("OK")
            baseColor: "#1f6b40"
            fontScale: 0.4
            onClicked: {
                const s = Object.assign({}, banner.seen)
                s[banner.shown.posted ? banner.shown.id + "|" + banner.pos.userName : banner.shown.id] = true
                banner.seen = s
            }
        }
    }
}
