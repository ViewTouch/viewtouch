import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Store Check: are we ready to open (printers, the backup, disk space, the
// standby, the clock, the drawers, the opening checklist), and what went
// wrong lately, kept so a manager sees it after the rush.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property var info: pos ? pos.storeCheck : ({})
    readonly property var checks: info.checks ?? []
    readonly property int toLook: checks.filter(c => !c.ok && !c.info).length

    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real zoom: zone ? zone.formZoom(narrow ? 460 : 900) : 1.4

    component Row: RowLayout {
        id: row
        required property var modelData
        spacing: 10
        Label {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 28
            text: row.modelData.info ? "ℹ" : row.modelData.ok ? "✓" : "⚠"
            color: row.modelData.info ? "#8ab4ff" : row.modelData.ok ? "#7ee2a8" : "#f5b940"
            font.pixelSize: 22
            font.bold: true
        }
        ColumnLayout {
            Layout.fillWidth: true
            // The row's width, not the text's: long details wrap (they ran
            // under the scrollbar).
            Layout.preferredWidth: Math.max(0, row.width - 38)
            Layout.maximumWidth: Math.max(0, row.width - 38)
            spacing: 0
            Label {
                Layout.fillWidth: true
                text: row.modelData.title
                font.pixelSize: 16
                font.bold: true
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                Layout.maximumWidth: Math.max(0, row.width - 38)
                text: row.modelData.detail
                font.pixelSize: 14
                opacity: 0.75
                wrapMode: Text.WordWrap
            }
        }
    }

    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        GridLayout {
            anchors.fill: parent
            anchors.margins: 10
            columns: w.narrow ? 1 : 2
            columnSpacing: 18
            rowSpacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                spacing: 8
                Label {
                    objectName: "readyToOpen"
                    Layout.fillWidth: true
                    text: w.toLook === 0 ? qsTr("✓ Ready to open") : qsTr("⚠ %n to look at", "", w.toLook)
                    color: w.toLook === 0 ? "#7ee2a8" : "#f5b940"
                    font.pixelSize: 24
                    font.bold: true
                }
                ListView {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 10
                    model: w.checks
                    ScrollBar.vertical: TouchScrollBar { id: listBar; needed: list.contentHeight > list.height + 1 }
                    delegate: Row { width: ListView.view.width - listBar.room }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Problems, the last 7 days")
                        font.pixelSize: 20
                        font.bold: true
                    }
                    PosButton {
                        objectName: "problemsSeen"
                        visible: (w.info.unseen ?? 0) > 0
                        text: qsTr("Mark as seen")
                        implicitHeight: 46
                        font.pixelSize: 15
                        onClicked: w.pos.problemsSeen()
                    }
                }
                ListView {
                    id: problems
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 8
                    model: w.info.problems ?? []
                    ScrollBar.vertical: TouchScrollBar { id: problemsBar; needed: problems.contentHeight > problems.height + 1 }
                    delegate: ColumnLayout {
                        required property var modelData
                        width: ListView.view.width - problemsBar.room
                        spacing: 0
                        Label {
                            Layout.fillWidth: true
                            text: (modelData.new ? "● " : "") + modelData.when
                            color: modelData.new ? "#f5b940" : "white"
                            opacity: modelData.new ? 1 : 0.6
                            font.pixelSize: 13
                            font.bold: modelData.new
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.text
                            font.pixelSize: 15
                            wrapMode: Text.WordWrap
                        }
                    }
                }
                Label {
                    visible: (w.info.problems ?? []).length === 0
                    text: qsTr("Nothing went wrong.")
                    opacity: 0.6
                }
            }
        }
    }
}
