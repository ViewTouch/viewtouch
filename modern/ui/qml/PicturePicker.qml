import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Dialogs
import QtQuick.Layouts

// Choosing a picture by touch: the pictures on this computer as thumbnails,
// newest first (Pictures, Documents, Downloads, the Desktop, and on a
// ViewTouch computer each person's too), and USB sticks. Browse… opens the
// ordinary file dialog for anywhere else.
Popup {
    id: picker
    objectName: "picturePicker"

    property var pos: null
    property string title: qsTr("Choose a picture")
    property var files: []
    signal picked(string path)

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 900, 1100)
    height: Math.min(parent ? parent.height - 32 : 700, 760)
    modal: true
    padding: 20

    function refresh() { files = pos ? pos.pictureFiles() : [] }
    // What the picture buttons call: on Android, Android's own photo picker
    // (it sees the phone's photos without asking for access to them all).
    function choose() {
        if (Qt.platform.os === "android")
            browse.open()
        else
            open()
    }
    onOpened: refresh()

    background: Rectangle { color: "#1c2128"; radius: 12; border.color: "#3a4250" }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        RowLayout {
            spacing: 10
            Label {
                text: picker.title
                font.pixelSize: 22
                font.bold: true
                color: "white"
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Button {
                objectName: "pictureUsb"
                text: qsTr("USB Stick")
                implicitHeight: 52
                font.pixelSize: 16
                onClicked: { if (picker.pos) picker.pos.openUsbSticks(); picker.refresh() }
            }
            Button {
                text: qsTr("Browse…")
                implicitHeight: 52
                font.pixelSize: 16
                onClicked: browse.open()
            }
            Button {
                text: qsTr("Cancel")
                implicitHeight: 52
                font.pixelSize: 16
                onClicked: picker.close()
            }
        }
        Label {
            visible: picker.files.length === 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#aab3c2"
            font.pixelSize: 16
            text: qsTr("No pictures found. Put them in Pictures, Documents or Downloads on this computer, or on a USB stick (touch USB Stick).")
        }
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            cellWidth: Math.floor(width / Math.max(2, Math.floor(width / 190)))
            cellHeight: cellWidth + 34
            model: picker.files
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 14 }
            delegate: Item {
                required property var modelData
                width: grid.cellWidth
                height: grid.cellHeight
                Rectangle {
                    anchors { fill: parent; margins: 6 }
                    radius: 8
                    color: tap.pressed ? "#2f6fd6" : "#262c35"
                    Image {
                        id: thumb
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
                        height: width
                        source: "file://" + modelData.path
                        sourceSize.width: 256
                        sourceSize.height: 256
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }
                    Label {
                        anchors { left: parent.left; right: parent.right; top: thumb.bottom; margins: 6 }
                        text: modelData.name
                        elide: Text.ElideMiddle
                        horizontalAlignment: Text.AlignHCenter
                        color: "white"
                        font.pixelSize: 13
                    }
                    Label {
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 4 }
                        text: modelData.folder
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                        color: "#8a94a6"
                        font.pixelSize: 11
                    }
                    TapHandler {
                        id: tap
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onTapped: {
                            picker.close()
                            picker.picked(modelData.path)
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: browse
        title: picker.title
        nameFilters: [qsTr("Pictures (*.png *.jpg *.jpeg *.webp *.gif *.bmp *.svg)")]
        onAccepted: {
            picker.close()
            picker.picked(selectedFile.toString())
        }
    }
}
