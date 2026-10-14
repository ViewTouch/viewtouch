import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// In the editor's top bar: which screen's version of the page is edited.
// Standard: the page as designed. Phone: its phone version, whose buttons are
// the page's (same words, colors and actions; only where they sit is the
// phone's). A page without one is laid out on phones by itself; "Make phone
// version" starts one from that. On the phone version: the page's buttons not
// on it yet (to put there, or leave off), where it differs from the page,
// and going back to automatic.
RowLayout {
    id: ps

    required property EditorController editor

    readonly property var screens: editor.revision >= 0 ? editor.screens : ({})
    readonly property bool isPhone: screens.isPhone === true
    readonly property bool hasPhone: (screens.phone ?? "") !== ""
    readonly property var missing: editor.revision >= 0 && isPhone ? editor.notOnPhone() : []
    readonly property int newThere: missing.filter(m => !m.hidden).length
    readonly property var differences: editor.revision >= 0 && isPhone ? editor.phoneDifferences() : []

    objectName: "phoneScreens"
    visible: screens.canPhone === true

    function closeTray() { tray.close() }
    spacing: 0

    component Segment: Button {
        required property bool on
        checkable: false
        flat: !on
        highlighted: on
        font.bold: on
        implicitHeight: 32
    }

    Segment {
        objectName: "screenStandard"
        text: qsTr("Standard")
        on: !ps.isPhone
        onClicked: ps.editor.showScreen("standard")
    }
    Segment {
        objectName: "screenPhone"
        text: qsTr("Phone")
        on: ps.isPhone
        onClicked: ps.hasPhone ? ps.editor.showScreen("phone") : makeAsk.open()
    }
    ToolButton {
        objectName: "phoneMenuButton"
        visible: ps.isPhone
        text: ps.newThere > 0 ? qsTr("⚠ %n not on phone", "", ps.newThere) : qsTr("Phone version ▾")
        palette.buttonText: ps.newThere > 0 ? EditorStyle.warning : EditorStyle.text
        onClicked: tray.open()
    }

    // No phone version yet.
    Popup {
        id: makeAsk
        objectName: "makePhonePopup"
        y: ps.height
        width: 360
        padding: 16
        ColumnLayout {
            width: parent.width
            spacing: 12
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Phones lay this page out by themselves, in rows of big buttons. Make a phone version to place its buttons yourself: their words, colors and actions stay shared with this page.")
            }
            Button {
                objectName: "makePhoneVersion"
                Layout.alignment: Qt.AlignRight
                highlighted: true
                text: qsTr("Make phone version")
                onClicked: { makeAsk.close(); ps.editor.makePhoneVersion() }
            }
        }
    }

    // On the phone version: the page's buttons not here; back to automatic.
    Popup {
        id: tray
        objectName: "phoneTray"
        y: ps.height
        width: 520
        height: Math.min(trayCol.implicitHeight + 24, (Overlay.overlay ? Overlay.overlay.height : 800) - 120)
        padding: 12
        ScrollView {
            anchors.fill: parent
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                id: trayCol
                width: parent.width
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: EditorStyle.muted
                    text: ps.missing.length === 0
                          ? qsTr("Every button of the page is on the phone.")
                          : qsTr("On the page, not on the phone:")
                }
                Repeater {
                    model: ps.missing
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: (modelData.label !== "" ? modelData.label : modelData.id) + "  ·  " + modelData.kind
                        }
                        Label {
                            visible: !modelData.hidden
                            text: qsTr("new")
                            color: EditorStyle.warning
                        }
                        Button {
                            objectName: "placeOnPhone-" + modelData.id
                            text: qsTr("Put on phone")
                            onClicked: ps.editor.placeOnPhone([modelData.id])
                        }
                        Button {
                            objectName: "leaveOff-" + modelData.id
                            visible: !modelData.hidden
                            text: qsTr("Leave off")
                            onClicked: ps.editor.hideOnPhone([modelData.id])
                        }
                    }
                }
                // Where the phone has drifted from the page: its own words and
                // sizes, and buttons only it has.
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: EditorStyle.border; visible: ps.differences.length > 0 }
                Label {
                    visible: ps.differences.length > 0
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: EditorStyle.muted
                    text: qsTr("Different on the phone:")
                }
                Repeater {
                    model: ps.differences
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: modelData.phoneOnly
                                  ? qsTr("%1  ·  only on the phone (%2)").arg(modelData.label).arg(modelData.what)
                                  : modelData.same
                                    ? qsTr("%1  ·  own %2, the same as the page's for now").arg(modelData.label).arg(modelData.what)
                                    : qsTr("%1  ·  own %2: “%3” here, “%4” on the page").arg(modelData.label).arg(modelData.what)
                                          .arg(modelData.phone).arg(modelData.standard)
                        }
                        Button {
                            objectName: "showDifference-" + modelData.id
                            text: qsTr("Show")
                            onClicked: ps.editor.selectOnly([modelData.id])
                        }
                        Button {
                            objectName: "followPage-" + modelData.id
                            visible: !modelData.phoneOnly
                            text: qsTr("Same as page")
                            onClicked: ps.editor.followPage(modelData.id, modelData.path)
                        }
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: EditorStyle.border }
                Button {
                    objectName: "removePhoneVersion"
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Back to automatic…")
                    onClicked: confirmRemove.open()
                }
            }
        }
    }

    Dialog {
        id: confirmRemove
        objectName: "confirmRemovePhone"
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("Back to automatic?")
        standardButtons: Dialog.Yes | Dialog.Cancel
        Label {
            width: 360
            wrapMode: Text.WordWrap
            text: qsTr("The phone version goes, and phones lay the page out by themselves again. The page itself doesn't change. (Undo brings it back.)")
        }
        onAccepted: { tray.close(); ps.editor.removePhoneVersion() }
    }
}
