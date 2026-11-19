import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Factory reset: what goes, that a backup is taken first, and typing RESET
// to be sure. ViewTouch then restarts as a kind of store with its starter
// menu (a restaurant's café, a sushi bar, a taco truck, a bookstore...), or
// an empty store to set up from nothing.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    // "" (a restaurant, its café), "empty", or "kind:menu" ("retail:books").
    property string startAs: ""
    readonly property bool empty: startAs === "empty"
    readonly property string kind: empty ? "" : (startAs.split(":")[0] || "restaurant")
    readonly property var sets: pos ? pos.starterSets() : []
    readonly property var packs: (sets.find(k => k.kind === w.kind) ?? {}).packs ?? []
    // The kind's own menu: what a restaurant's card means ("" for its café).
    function first(kind) {
        const k = w.sets.find(x => x.kind === kind)
        const p = k && k.packs.length ? k.packs[0].set : kind
        return p === "restaurant" ? "" : p
    }

    readonly property real zoom: zone ? zone.formZoom(zone.narrow ? 380 : 1100) : 1
    // A phone's bigger text can need more room than the panel has: then it scrolls.
    Flickable {
        id: scroller
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, (form.implicitHeight + 24) * w.zoom)
        interactive: contentHeight > height + 1
        boundsBehavior: Flickable.StopAtBounds
        clip: interactive

        Item {
            width: w.width / w.zoom
            height: scroller.contentHeight / w.zoom
            scale: w.zoom
            transformOrigin: Item.TopLeft

            ColumnLayout {
                id: form
                anchors.horizontalCenter: parent.horizontalCenter
                y: Math.max(12, (parent.height - height) / 2)   // from the top when it scrolls
                width: Math.min(parent.width - 40, 760)
                spacing: 10

                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Start over as a fresh install")
                    font.pixelSize: Ui.px(24)
                    font.bold: true
                    color: "#ff9a9e"
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Ui.px(16)
                    text: qsTr("This deletes every sale and past day, customers, gift cards, house accounts, the waitlist, "
                               + "the schedule, staff, the menu, inventory, settings, paired devices and your page changes. "
                               + "Then ViewTouch restarts with:")
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: w.zone && w.zone.narrow ? 1 : 2   // (four: two rows of two)
                    columnSpacing: 10
                    rowSpacing: 10
                    Repeater {
                        model: [
                            { kind: "restaurant", name: "resetStarter", title: qsTr("A restaurant"),
                              text: qsTr("Its menu, tables, staff and settings, to try things out on.") },
                            { kind: "foodTruck", name: "resetTruck", title: qsTr("A food truck"),
                              text: qsTr("Orders at the window by number, prep counts, a menu board; its menu below.") },
                            { kind: "retail", name: "resetShop", title: qsTr("A shop"),
                              text: qsTr("Products with barcodes, sizes and colors, returns; what it sells below.") },
                            { kind: "", name: "resetEmpty", title: qsTr("An empty store"),
                              text: qsTr("No menu, tables or sample staff. Log in with 1234: the setup guide has you add yourself.") }
                        ]
                        delegate: PosButton {
                            id: choice
                            required property var modelData
                            objectName: modelData.name
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            implicitHeight: Math.max(96, contentItem.implicitHeight + 24)
                            readonly property bool chosen: modelData.kind === "" ? w.empty : w.kind === modelData.kind
                            palette.button: chosen ? "#2b3a52" : "#2a303b"
                            palette.buttonText: "white"
                            onClicked: w.startAs = modelData.kind === "" ? "empty" : w.first(modelData.kind)
                            contentItem: ColumnLayout {
                                spacing: 4
                                Label {
                                    textFormat: Text.PlainText
                                    Layout.fillWidth: true
                                    text: (choice.chosen ? "✓ " : "") + modelData.title
                                    font.pixelSize: Ui.px(18)
                                    font.bold: true
                                    color: "white"
                                    wrapMode: Text.WordWrap
                                }
                                Label {
                                    textFormat: Text.PlainText
                                    Layout.fillWidth: true
                                    text: modelData.text
                                    font.pixelSize: Ui.px(14)
                                    font.bold: false
                                    color: "#c9d1dc"
                                    wrapMode: Text.WordWrap
                                }
                            }
                            background: Rectangle {
                                radius: 8
                                color: choice.palette.button
                                border.width: 2
                                border.color: choice.chosen ? "#4c8dff" : "#3a414d"
                            }
                        }
                    }
                }
                // Its menu: the kind's own first, then every other of its kind
                // (a restaurant's or a truck's food, a shop's goods: never mixed).
                Label {
                    textFormat: Text.PlainText
                    visible: !w.empty && w.packs.length > 1
                    Layout.topMargin: 6
                    text: w.kind === "retail" ? qsTr("What it sells") : qsTr("Its menu")
                    font.pixelSize: Ui.px(17)
                    font.bold: true
                }
                Flow {
                    objectName: "resetPacks"
                    visible: !w.empty && w.packs.length > 1
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: w.empty ? [] : w.packs
                        delegate: PosButton {
                            required property var modelData
                            objectName: "resetPack-" + (modelData.id || "cafe-grill")
                            readonly property string set: modelData.set === "restaurant" ? "" : modelData.set
                            readonly property bool chosen: w.startAs === set
                            text: (chosen ? "✓ " : "") + modelData.name
                            font.pixelSize: Ui.px(15)
                            implicitHeight: 52
                            palette.button: chosen ? "#2b3a52" : "#2a303b"
                            palette.buttonText: "white"
                            onClicked: w.startAs = set
                        }
                    }
                }
                Label {
                    textFormat: Text.PlainText
                    visible: !w.empty
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Ui.px(14)
                    color: "#c9d1dc"
                    text: (w.packs.find(p => (p.set === "restaurant" ? "" : p.set) === w.startAs) ?? {}).description ?? ""
                }
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: Ui.px(16)
                    color: "#7ee2a8"
                    text: qsTr("A backup is taken first (backups/…-before-reset.db), so it can be undone with --restore. "
                               + "Earlier backups and saved report files are kept.")
                }
                Label { textFormat: Text.PlainText; text: qsTr("Type RESET to confirm"); font.pixelSize: Ui.px(17); Layout.topMargin: 10 }
                TextField {
                    implicitHeight: 56
                    id: confirm
                    Layout.fillWidth: true
                    font.pixelSize: Ui.px(24)
                    placeholderText: "RESET"
                    inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                }
                // Erasing the store is the maintainer's (the code set on its computer).
                Label {
                    textFormat: Text.PlainText
                    objectName: "resetWhose"
                    visible: !(w.zone && w.zone.pos && w.zone.pos.can("maintain"))
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Ui.warn
                    font.pixelSize: Ui.px(18)
                    text: qsTr("Only the store's maintainer can erase it: log in with the maintainer's code.")
                }
                PosButton {
                    Layout.fillWidth: true
                    implicitHeight: 70
                    font.pixelSize: Ui.px(20)
                    enabled: confirm.text.trim() === "RESET" && !!w.zone && !!w.zone.pos && w.zone.pos.can("maintain")
                    objectName: "resetGo"
                    text: w.empty ? qsTr("Back Up and Start Empty") : qsTr("Back Up and Reset Everything")
                    palette.button: enabled ? "#a42828" : "#3a3f48"
                    palette.buttonText: "white"
                    onClicked: w.pos.factoryReset(confirm.text, w.startAs)
                }
            }
        }
    }
}
