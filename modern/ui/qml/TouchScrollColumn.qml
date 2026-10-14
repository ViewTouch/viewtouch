import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// A column of controls that scrolls when it is taller than its space, with
// a finger-wide bar. Forms use it so their controls can stay big enough to
// touch on small screens instead of being squeezed to fit.
Item {
    id: pane
    default property alias content: column.data
    property alias spacing: column.spacing
    readonly property bool scrolls: flick.contentHeight > flick.height + 1

    implicitHeight: column.implicitHeight
    implicitWidth: column.implicitWidth

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: column.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        interactive: pane.scrolls
        ScrollBar.vertical: TouchScrollBar { id: bar; needed: pane.scrolls }

        ColumnLayout {
            id: column
            width: flick.width - bar.room
        }
    }
}
