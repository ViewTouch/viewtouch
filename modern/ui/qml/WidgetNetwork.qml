import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Manager -> Network: is everything on the store's network working? This
// computer's role, the standby server (the live copy that takes over if
// the main one stops), the screens connected, and how each printer did.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var net: pos ? pos.network : ({})
    readonly property bool serving: net.role === "main"
    readonly property var standby: net.standby ?? null

    function time(ms) { return ms ? Qt.locale().toString(new Date(ms), "h:mm AP") : "" }

    component Dot: Rectangle {
        property string state: "unknown"
        implicitWidth: 22
        implicitHeight: 22
        radius: 11
        color: state === "ok" ? "#2fbf71" : state === "failed" ? "#e04848" : "#7b8494"
    }
    component Heading: Label {
        font.pixelSize: 22
        font.bold: true
        Layout.topMargin: 14
    }

    readonly property real zoom: zone ? zone.formZoom(zone.narrow ? 380 : 688) : 1
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        Flickable {
            id: flick
            anchors.fill: parent
            anchors.margins: 16
            contentHeight: column.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: TouchScrollBar { id: bar }

            ColumnLayout {
                id: column
                width: flick.width - bar.room
                spacing: 8

                Heading { text: qsTr("This computer"); Layout.topMargin: 0 }
                RowLayout {
                    spacing: 12
                    Dot { state: "ok" }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 18
                        text: w.serving
                              ? qsTr("%1 is the main server: it keeps the store's data and serves the other screens.").arg(w.net.machine ?? "")
                              : qsTr("This computer runs the store on its own (it doesn't serve other screens).")
                    }
                }
                Label {
                    objectName: "storeBuild"
                    visible: !!w.net.build
                    Layout.fillWidth: true
                    opacity: 0.7
                    font.pixelSize: 16
                    text: qsTr("This computer runs ViewTouch %1.").arg(w.net.build ?? "")
                }
                Label {
                    visible: (w.net.term ?? 0) > 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    opacity: 0.7
                    font.pixelSize: 16
                    text: qsTr("It took over from another main server %n time(s).", "", w.net.term ?? 0)
                }

                Heading { text: qsTr("Standby server"); visible: w.serving }
                RowLayout {
                    visible: w.serving
                    spacing: 12
                    Dot { objectName: "standbyDot"; state: w.standby ? "ok" : "failed" }
                    Label {
                        objectName: "standbyText"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 18
                        text: w.standby
                              ? qsTr("In sync: %1 has a live copy of everything (since %2). If this computer stops, it takes over within about 20 seconds.")
                                    .arg(w.standby.address).arg(w.time(w.standby.since))
                              : qsTr("No standby. If this computer stops, the store stops until it is back. "
                                     + "To keep running, start a second computer with:  vtmodern --standby auto --pair <code from Terminals>")
                    }
                }

                Heading { text: qsTr("Screens connected (%1)").arg((w.net.terminals ?? []).length); visible: w.serving }
                Label {
                    visible: w.serving && (w.net.terminals ?? []).length === 0
                    text: qsTr("None right now.")
                    opacity: 0.6
                    font.pixelSize: 17
                }
                Repeater {
                    model: w.serving ? (w.net.terminals ?? []) : []
                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 54
                        radius: 8
                        color: "#232933"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 14
                            Dot { state: "ok" }
                            Label { text: modelData.name; font.pixelSize: 18; font.bold: true; Layout.preferredWidth: 260; elide: Text.ElideRight }
                            Label { text: modelData.address; font.pixelSize: 16; opacity: 0.7; Layout.preferredWidth: 180 }
                            Label {
                                Layout.fillWidth: true
                                font.pixelSize: 16
                                elide: Text.ElideRight
                                text: modelData.user ? qsTr("%1 is logged in").arg(modelData.user) : qsTr("Nobody logged in")
                            }
                            // Behind the store: it needs the update (a manager there can get it).
                            Label {
                                objectName: "screenBuild-" + modelData.name
                                font.pixelSize: 16
                                color: modelData.behind || modelData.ahead ? "#f5b940" : "white"
                                opacity: modelData.behind || modelData.ahead ? 1 : 0.6
                                text: !modelData.build ? qsTr("older version")
                                    : modelData.behind ? (modelData.hasUpdate ? qsTr("build %1 · update ready").arg(modelData.build)
                                                                              : qsTr("build %1 · needs an update").arg(modelData.build))
                                    : modelData.ahead ? qsTr("build %1 · newer than this computer").arg(modelData.build)
                                    : qsTr("build %1").arg(modelData.build)
                            }
                            Label { text: qsTr("since %1").arg(w.time(modelData.since)); font.pixelSize: 16; opacity: 0.6 }
                        }
                    }
                }

                // Updates for the screens: the tablet app and Linux packages, put in a folder here.
                Heading { text: qsTr("Updates for the screens"); visible: w.serving }
                Label {
                    objectName: "updatesFolder"
                    visible: w.serving
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 16
                    opacity: 0.8
                    text: (w.net.updates ?? []).length
                          ? qsTr("Ready: %1. A manager on a screen that's behind gets the update there.")
                                .arg((w.net.updates ?? []).map(u => u.name).join(", "))
                          : qsTr("To update the tablets, copy the new app (ViewTouch-b<build>-arm64-v8a.apk) into %1 on this computer. Screens that are behind then offer it to a manager.")
                                .arg(w.net.updatesDir ?? "")
                }

                Heading { text: qsTr("Printers") }
                Label {
                    visible: (w.net.printers ?? []).length === 0
                    text: qsTr("No printers set up (Manager → Printers).")
                    opacity: 0.6
                    font.pixelSize: 17
                }
                Repeater {
                    model: w.net.printers ?? []
                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: printerRow.implicitHeight + 20
                        radius: 8
                        color: "#232933"
                        RowLayout {
                            id: printerRow
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 14
                            Dot { state: modelData.status }
                            Label { text: modelData.name; font.pixelSize: 18; font.bold: true; Layout.preferredWidth: 260; elide: Text.ElideRight }
                            Label { text: modelData.where || modelData.type; font.pixelSize: 16; opacity: 0.7; Layout.preferredWidth: 180; elide: Text.ElideMiddle }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                font.pixelSize: 16
                                color: modelData.status === "failed" || modelData.problem ? "#ff9a9e" : "white"
                                // What the printer itself says first (out of paper, cover open...).
                                text: modelData.problem === "paperOut" ? qsTr("Out of paper")
                                    : modelData.problem === "coverOpen" ? qsTr("Cover open")
                                    : modelData.problem === "paperLow" ? qsTr("Paper running low")
                                    : modelData.problem === "offline" ? qsTr("Not answering")
                                    : modelData.problem === "offlinePaper" ? qsTr("Not answering: probably out of paper")
                                    : modelData.problem === "silent" ? qsTr("Not responding: cover open or out of paper?")
                                    : modelData.problem === "error" ? qsTr("Printer error (paper jam or cutter?)")
                                    : modelData.status === "ok" ? qsTr("Printed at %1").arg(w.time(modelData.at))
                                    : modelData.status === "failed" ? qsTr("Failed at %1: %2").arg(w.time(modelData.at)).arg(modelData.error)
                                    : qsTr("Nothing printed yet since start-up")
                            }
                        }
                    }
                }
            }
        }
    }
}
