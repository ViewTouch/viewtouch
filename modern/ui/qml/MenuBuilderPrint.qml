import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Dialogs
import QtQuick.Layouts

// Menu Builder: The printed menu (PDF): what's on it and how it looks.
Popup {
    id: printDialog
    property Item menu   // the WidgetMenuBuilder (not by its type: that would be a cycle)
    objectName: "builderPrintDialog"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 1100, 1100)
    height: Math.min(parent ? parent.height - 32 : 760, 760)
    modal: true
    padding: 16
    property string title: ""
    property string subtitle: ""
    property int columns: 2
    property bool pictures: true
    property bool descriptions: true
    property bool soldOut: false
    property string paper: "letter"
    property var leftOut: []      // category ids not on it
    property string preview: ""
    property int pages: 0
    property string saved: ""
    readonly property var options: ({
        title: title, subtitle: subtitle, columns: columns, pictures: pictures, descriptions: descriptions,
        soldOut: soldOut, paper: paper,
        categories: menu.categories.map(c => c.id).filter(id => !leftOut.includes(id))
    })
    onOpened: {
        title = menu.pos.storeName
        saved = ""
        redraw.restart()
    }
    onOptionsChanged: if (opened) redraw.restart()
    Timer {
        id: redraw
        interval: 200
        onTriggered: {
            printDialog.preview = menu.zone.controller.menuPreview(printDialog.options, 620)
            printDialog.pages = menu.zone.controller.menuPages(printDialog.options)
        }
    }
    component Choice: MenuTouchButton {
        property string label
        property bool on: false
        implicitHeight: 44
        font.pixelSize: 15
        highlighted: on
        text: (on ? "✓ " : "") + label
    }
    contentItem: RowLayout {
        spacing: 16
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8
            Label { text: qsTr("Printed menu"); font.pixelSize: 20; font.bold: true }
            Label { text: qsTr("Title"); opacity: 0.75 }
            TextField {
                objectName: "printTitle"
                Layout.fillWidth: true
                implicitHeight: 48
                font.pixelSize: 17
                text: printDialog.title
                onTextEdited: printDialog.title = text
            }
            Label { text: qsTr("Under it"); opacity: 0.75 }
            TextField {
                objectName: "printSubtitle"
                Layout.fillWidth: true
                implicitHeight: 48
                font.pixelSize: 17
                placeholderText: qsTr("Address, phone, hours")
                text: printDialog.subtitle
                onTextEdited: printDialog.subtitle = text
            }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Choice { objectName: "printOneColumn"; label: qsTr("1 column"); on: printDialog.columns === 1; onClicked: printDialog.columns = 1 }
                Choice { objectName: "printTwoColumns"; label: qsTr("2 columns"); on: printDialog.columns === 2; onClicked: printDialog.columns = 2 }
                Choice { label: qsTr("Letter"); on: printDialog.paper === "letter"; onClicked: printDialog.paper = "letter" }
                Choice { label: "A4"; on: printDialog.paper === "a4"; onClicked: printDialog.paper = "a4" }
            }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                Choice { objectName: "printPictures"; label: qsTr("Pictures"); on: printDialog.pictures; onClicked: printDialog.pictures = !on }
                Choice { label: qsTr("Descriptions"); on: printDialog.descriptions; onClicked: printDialog.descriptions = !on }
                Choice { label: qsTr("Sold-out items"); on: printDialog.soldOut; onClicked: printDialog.soldOut = !on }
            }
            Label { text: qsTr("Categories on it"); opacity: 0.75 }
            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 60
                clip: true
                contentHeight: categoryFlow.implicitHeight
                Flow {
                    id: categoryFlow
                    width: parent.width
                    spacing: 6
                    Repeater {
                        model: menu.categories
                        delegate: Choice {
                            required property var modelData
                            objectName: "printCategory-" + modelData.id
                            label: modelData.name
                            on: !printDialog.leftOut.includes(modelData.id)
                            onClicked: printDialog.leftOut = on
                                       ? printDialog.leftOut.concat([modelData.id])
                                       : printDialog.leftOut.filter(id => id !== modelData.id)
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                visible: printDialog.saved !== ""
                text: qsTr("Saved to %1").arg(printDialog.saved)
                wrapMode: Text.WrapAnywhere
                color: "#6cc58a"
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                MenuTouchButton {
                    objectName: "printSave"
                    Layout.fillWidth: true
                    highlighted: true
                    enabled: printDialog.pages > 0 && printDialog.options.categories.length > 0
                    text: qsTr("Save as PDF")
                    onClicked: printDialog.saved = menu.zone.controller.printMenu(printDialog.options)
                }
                MenuTouchButton { text: qsTr("Close"); onClicked: printDialog.close() }
            }
        }
        // The first page, as it will print.
        ColumnLayout {
            Layout.preferredWidth: 440
            Layout.fillHeight: true
            spacing: 6
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#2b3038"
                radius: 6
                Image {
                    objectName: "printPreview"
                    anchors.fill: parent
                    anchors.margins: 8
                    fillMode: Image.PreserveAspectFit
                    source: printDialog.preview
                    cache: false
                    asynchronous: true
                    smooth: true
                    mipmap: true
                }
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: printDialog.pages > 1 ? qsTr("Page 1 of %1").arg(printDialog.pages)
                                            : printDialog.pages === 1 ? qsTr("1 page") : qsTr("Nothing to print")
                opacity: 0.75
            }
        }
    }
}

