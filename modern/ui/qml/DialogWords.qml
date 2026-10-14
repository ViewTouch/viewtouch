pragma Singleton
import QtQuick
import QtQuick.Controls.Fusion

// A Dialog's standard buttons (OK, Cancel, Save...) in the screen's
// language: Qt words them once, when the dialog is made, so a dialog calls
// DialogWords.apply(this) as it opens.
QtObject {
    function apply(dialog) {
        const words = [[Dialog.Ok, qsTr("OK")], [Dialog.Cancel, qsTr("Cancel")], [Dialog.Save, qsTr("Save")],
                       [Dialog.Discard, qsTr("Discard")], [Dialog.Yes, qsTr("Yes")], [Dialog.No, qsTr("No")],
                       [Dialog.Close, qsTr("Close")]]
        for (const [which, text] of words) {
            const b = dialog.standardButton(which)
            if (b)
                b.text = text
        }
    }
}
