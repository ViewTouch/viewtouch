import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Dialogs
import QtQuick.Layouts

// Menu Builder: A ready-made picture for an item.
Popup {
    id: pictureDialog
    property Item menu   // the WidgetMenuBuilder (not by its type: that would be a cycle)
    objectName: "builderPictureDialog"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 32 : 640, 640)
    modal: true
    padding: 16
    readonly property var pictures: ["🌮", "🌯", "🫔", "🥙", "🍔", "🌭", "🍕", "🥪", "🥗", "🍟", "🍗", "🍖",
                                     "🥩", "🥓", "🍳", "🥞", "🧇", "🥐", "🍞", "🥯", "🧀", "🍝", "🍜", "🍲",
                                     "🍛", "🍣", "🍱", "🍤", "🐟", "🦀", "🍚", "🥟", "🌶️", "🥑", "🌽", "🥔",
                                     "🥕", "🍅", "🍰", "🎂", "🧁", "🍩", "🍪", "🍦", "🍨", "🥧", "🍫", "🍎",
                                     "🍓", "🍌", "🍉", "🥤", "🧃", "🧋", "☕", "🍵", "🥛", "🍺", "🍷", "🍸",
                                     "🍹", "🥃", "🍾", "💧"]
    contentItem: ColumnLayout {
        spacing: 10
        Label { text: qsTr("A picture for %1").arg(menu.draft.name || qsTr("it")); font.pixelSize: 20; font.bold: true }
        Grid {
            columns: 8
            spacing: 6
            Layout.alignment: Qt.AlignHCenter
            Repeater {
                model: pictureDialog.pictures
                delegate: Rectangle {
                    required property string modelData
                    required property int index
                    objectName: "builderPicture-" + index
                    width: 64
                    height: 64
                    radius: 10
                    color: "#232933"
                    border.color: "#3a424f"
                    Text {
                        anchors.centerIn: parent
                        text: parent.modelData
                        font.family: "Noto Color Emoji"
                        font.pixelSize: 38
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            const e = parent.modelData, b = menu
                            pictureDialog.close()
                            const ref = b.zone.controller.emojiPicture(e)
                            if (ref !== "") b.set("image", ref)
                        }
                    }
                }
            }
        }
        MenuTouchButton { Layout.fillWidth: true; text: qsTr("Cancel"); onClicked: pictureDialog.close() }
    }
}

// Select -> Move to…: the categories.
