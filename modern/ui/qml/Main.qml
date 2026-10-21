import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

ApplicationWindow {
    id: root

    required property LayoutController controller
    readonly property bool editing: controller.editing
    readonly property EditorController editor: controller.editor
    // Kiosk screens stay full screen; elsewhere F11 switches to a window.
    property bool kiosk: false
    // A stress test running (vtmodern --stress): its numbers over the screen.
    property QtObject stress: null
    // The customer display in the second monitor's part of this window (a
    // kiosk's one window spans both monitors). With one screen (trying it
    // out), the right third.
    property bool customerDisplay: false
    // Where the first monitor ends (set from the screens; 0: one screen).
    property real customerDisplayAt: 0
    readonly property real posWidth: !customerDisplay ? width
                                     : customerDisplayAt > 0 ? Math.min(width, customerDisplayAt)
                                     : Math.round(width * 0.66)
    // An on-screen keyboard for text fields: most touch screens have no
    // keyboard. --touch-keyboard yes|no, else the terminal's setting (Manager
    // -> Terminals), else on (Android: the device's own keyboard instead).
    property string keyboardFlag: ""
    readonly property string keyboardSetting: controller.pos ? ((controller.pos as PosService).terminalKeyboard ?? "") : ""
    property bool touchKeyboard: keyboardFlag !== "" ? keyboardFlag === "on"
                               : keyboardSetting !== "" ? keyboardSetting === "on"
                               : Qt.platform.os !== "android"
    // What a barcode scanner is typing (see the page's Keys).
    property string scanBuffer: ""
    property double scanLast: 0
    // The text field being typed in, if any.
    readonly property Item typingIn: {
        const line = activeFocusItem as TextInput
        if (line && !line.readOnly)
            return line
        const area = activeFocusItem as TextEdit
        return area && !area.readOnly ? area : null
    }
    // The keyboard pops up over the screen (the page keeps its size). When it
    // would cover the field being typed in, the page (or the setup guide)
    // slides up just enough to keep the field in view, and back down after.
    // The device's own keyboard (Android): it pops up over the window (the
    // manifest's adjustNothing), and its top is where ours would be.
    readonly property rect deviceKeys: Qt.inputMethod.keyboardRectangle
    readonly property bool deviceKeysUp: !touchKeys.visible && Qt.inputMethod.visible && deviceKeys.height > 0
    readonly property bool keyboardUp: touchKeys.visible || deviceKeysUp
    readonly property real keysHeight: touchKeys.visible ? touchKeys.height
                                     : deviceKeysUp ? Math.min(root.height, deviceKeys.height) : 0
    readonly property real keysTop: touchKeys.visible ? root.height - touchKeys.height
                                  : deviceKeysUp ? (deviceKeys.y > 0 ? Math.min(root.height, deviceKeys.y)
                                                                     : root.height - keysHeight)
                                  : root.height
    property real lift: 0
    Behavior on lift { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
    function inside(item, container) {
        for (let p = item; p; p = p.parent)
            if (p === container) return true
        return false
    }
    function updateLift() {
        const field = typingIn
        if (!keyboardUp || !field || !(inside(field, pageColumn) || inside(field, setupLoader))) {
            lift = 0
            return
        }
        // Where the field's bottom is with the page where it belongs.
        const bottom = field.mapToItem(null, 0, field.height).y + lift
        lift = Math.max(0, Math.min(bottom + 16 - keysTop, keysHeight))
    }
    onTypingInChanged: Qt.callLater(updateLift)
    onKeysTopChanged: Qt.callLater(updateLift)

    // Phones get phone pages (the controller decides; see formFactor).
    onWidthChanged: controller.windowResized(width, height)
    onHeightChanged: controller.windowResized(width, height)
    Component.onCompleted: controller.windowResized(width, height)

    width: 1280
    height: 720
    visible: true
    title: (editing ? qsTr("Editing — ") : "") + qsTr("ViewTouch — %1").arg(controller.pageName)
    color: editing ? EditorStyle.chrome : "black"

    palette {
        window: EditorStyle.panel
        windowText: EditorStyle.text
        base: EditorStyle.chrome
        alternateBase: EditorStyle.panelRaised
        text: EditorStyle.text
        button: EditorStyle.panelRaised
        buttonText: EditorStyle.text
        highlight: EditorStyle.accent
        highlightedText: "white"
        placeholderText: EditorStyle.muted
        toolTipBase: EditorStyle.panelRaised
        toolTipText: EditorStyle.text
        mid: EditorStyle.border
        dark: EditorStyle.chrome
        light: EditorStyle.panelRaised
    }

    function toast(text) {
        toastText.text = text
        toastBox.opacity = 1
        toastTimer.restart()
    }

    // Leave edit mode, asking first when there are unsaved changes.
    function requestLeaveEdit() {
        if (!editing)
            return
        if (editor.dirty)
            leaveDialog.open()
        else
            controller.leaveEditMode(false)
    }

    onEditingChanged: if (!editing) pageView.forceActiveFocus()

    // Something over the whole screen (see the page's `enabled`).
    readonly property bool pageCovered: selfOrder.visible || root.controller.asleep || cardWait.visible
                                        || receiptSheet.visible || setupLoader.active || jobChooser.visible || cashTipsPrompt.visible
                                        || approvalPad.visible || allergySheet.visible || couponSheet.visible || holdSheet.visible || exemptSheet.visible
                                        || TouchGuard.sheetOpen || reasonSheet.visible || handoffSheet.visible

    Loader {
        active: root.customerDisplay
        x: root.posWidth
        width: root.width - root.posWidth
        height: root.height
        sourceComponent: CustomerDisplay { pos: root.controller.pos as PosService }
    }

    ColumnLayout {
        id: pageColumn
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.posWidth
        spacing: 0
        transform: Translate { y: -root.lift }

        Loader {
            Layout.fillWidth: true
            active: root.editing
            visible: active
            sourceComponent: EditorToolbar {
                controller: root.controller
                editor: root.editor
                pageSurface: pageView.surfaceItem
                onExitRequested: root.requestLeaveEdit()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Loader {
                Layout.fillHeight: true
                Layout.preferredWidth: EditorStyle.pagesWidth
                active: root.editing
                visible: active
                sourceComponent: PagePanel {
                    controller: root.controller
                    editor: root.editor
                }
            }

            PageView {
                id: pageView
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: root.editing ? 12 : 0
                // Off while something covers the whole screen: Qt still offers a
                // touch to the tap handlers underneath whatever took it, so the
                // page's buttons were pressed through the kiosk, a sheet or the
                // PIN pad, and the touch that woke the screen saver pressed one
                // (the kiosk's space bar put the order away by touching a table
                // behind it). Under the pop-up keyboard: see TouchGuard.
                enabled: !root.pageCovered
                controller: root.controller
                editor: root.editing ? root.editor : null
                focus: true

                Keys.onPressed: event => {
                    if (root.editing)
                        return
                    // A barcode scanner types its code faster than anyone,
                    // then Enter.
                    const now = Date.now()
                    const burst = now - root.scanLast < 60
                    root.scanLast = now
                    if (event.text.length === 1 && event.text > " ") {
                        root.scanBuffer = burst ? root.scanBuffer + event.text : event.text
                    } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && burst
                               && root.scanBuffer.length >= 4) {
                        const code = root.scanBuffer
                        root.scanBuffer = ""
                        root.controller.scanned(code)
                        event.accepted = true
                        return
                    } else {
                        root.scanBuffer = ""
                    }
                    if (event.key === Qt.Key_Escape && root.controller.numberKey("escape")) {
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        root.controller.goBack()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Home) {
                        root.controller.goHome()
                        event.accepted = true
                    } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                               && root.controller.numberKey("enter")) {
                        event.accepted = true      // the item with the number typed
                    } else if (event.key === Qt.Key_Backspace && root.controller.numberKey("back")) {
                        event.accepted = true
                    } else if (event.text.length === 1 && event.text >= "0" && event.text <= "9"
                               && root.controller.numberKey(event.text)) {
                        event.accepted = true
                    } else if (event.text !== "" && !burst && root.controller.triggerHotkey(event.text)) {
                        event.accepted = true
                    }
                }
            }

            Loader {
                Layout.fillHeight: true
                Layout.preferredWidth: EditorStyle.panelWidth
                active: root.editing
                visible: active
                sourceComponent: Inspector {
                    controller: root.controller
                    editor: root.editor
                }
            }
        }
    }

    // The page's tap handlers ignore presses on the keyboard (TouchGuard).
    Binding {
        target: TouchGuard
        property: "keyboardTop"
        value: touchKeys.visible ? root.height - touchKeys.height : -1
    }

    // The on-screen keyboard: over the screen, above every dialog, and it
    // never takes the focus from the field.
    Popup {
        id: touchKeys
        objectName: "touchKeys"
        parent: Overlay.overlay
        visible: root.touchKeyboard && root.typingIn !== null && !selfOrder.visible
        modal: false
        focus: false
        closePolicy: Popup.NoAutoClose
        z: 1000
        padding: 0
        x: 0
        width: root.posWidth
        height: Math.min(root.height * 0.42, 420)
        y: root.height - height
        background: null
        contentItem: TouchKeyboard {
            id: keyboard
            target: touchKeys.visible ? root.typingIn : null
            onDismissed: pageView.forceActiveFocus()
        }
    }

    // --- keyboard --------------------------------------------------------------
    // Text fields take their own editing keys first (Qt shortcut override), so
    // Delete/arrows in the inspector edit text instead of zones.

    Shortcut {
        // Ctrl+E works on keyboards whose F-keys need Fn (Macs, many laptops).
        sequences: ["F1", "Ctrl+E"]
        // Entering checks the layout.edit permission (a manager must be logged in).
        onActivated: root.editing ? root.requestLeaveEdit() : root.controller.requestEditMode()
    }
    // Android's Back button goes back a page instead of closing the app.
    Shortcut {
        enabled: !root.editing
        sequence: "Back"
        onActivated: root.controller.goBack()
    }
    Shortcut {
        enabled: !root.kiosk
        sequences: [StandardKey.FullScreen, "F11", "Ctrl+Shift+F"]
        onActivated: root.visibility === Window.FullScreen ? root.showNormal() : root.showFullScreen()
    }
    Shortcut { enabled: root.editing; sequences: [StandardKey.Save]; onActivated: root.controller.saveEdits() }
    Shortcut { enabled: root.editing; sequences: [StandardKey.Undo]; onActivated: root.editor.undo() }
    Shortcut { enabled: root.editing; sequences: [StandardKey.Redo, "Ctrl+Y"]; onActivated: root.editor.redo() }
    Shortcut { enabled: root.editing; sequences: [StandardKey.Copy]; onActivated: root.editor.copy() }
    Shortcut { enabled: root.editing; sequences: [StandardKey.Cut]; onActivated: root.editor.cut() }
    Shortcut { enabled: root.editing; sequences: [StandardKey.Paste]; onActivated: root.editor.paste() }
    Shortcut { enabled: root.editing; sequences: [StandardKey.SelectAll]; onActivated: root.editor.selectAll() }
    Shortcut { enabled: root.editing; sequence: "Ctrl+D"; onActivated: root.editor.duplicateSelection() }
    Shortcut { enabled: root.editing; sequences: [StandardKey.Delete, "Backspace"]; onActivated: root.editor.deleteSelection() }
    Shortcut { enabled: root.editing; sequence: "Esc"; onActivated: root.editor.clearSelection() }
    Shortcut { enabled: root.editing; sequence: "Ctrl+]"; onActivated: root.editor.bringToFront() }
    Shortcut { enabled: root.editing; sequence: "Ctrl+["; onActivated: root.editor.sendToBack() }

    // Arrows move by one grid step; Shift+arrows by one unit.
    Shortcut { enabled: root.editing; sequence: "Left"; onActivated: root.editor.nudge(-root.controller.pageGrid, 0) }
    Shortcut { enabled: root.editing; sequence: "Right"; onActivated: root.editor.nudge(root.controller.pageGrid, 0) }
    Shortcut { enabled: root.editing; sequence: "Up"; onActivated: root.editor.nudge(0, -root.controller.pageGrid) }
    Shortcut { enabled: root.editing; sequence: "Down"; onActivated: root.editor.nudge(0, root.controller.pageGrid) }
    Shortcut { enabled: root.editing; sequence: "Shift+Left"; onActivated: root.editor.nudge(-1, 0) }
    Shortcut { enabled: root.editing; sequence: "Shift+Right"; onActivated: root.editor.nudge(1, 0) }
    Shortcut { enabled: root.editing; sequence: "Shift+Up"; onActivated: root.editor.nudge(0, -1) }
    Shortcut { enabled: root.editing; sequence: "Shift+Down"; onActivated: root.editor.nudge(0, 1) }

    // Page Up / Page Down walk through the page list.
    function stepPage(delta) {
        const pages = editor.pages
        const i = pages.findIndex(p => p.id === editor.pageId)
        const next = pages[(i + delta + pages.length) % pages.length]
        if (next) controller.showPage(next.id)
    }
    Shortcut { enabled: root.editing; sequence: "PgUp"; onActivated: root.stepPage(-1) }
    Shortcut { enabled: root.editing; sequence: "PgDown"; onActivated: root.stepPage(1) }

    // --- dialogs and feedback ------------------------------------------------------

    Dialog {
        id: leaveDialog
        title: qsTr("Save your changes?")
        anchors.centerIn: parent
        modal: true
        width: 420
        standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel
        onAboutToShow: DialogWords.apply(this)

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("You changed pages in this session. Save them before leaving edit mode?")
        }
        onAccepted: root.controller.leaveEditMode(true)
        onDiscarded: {
            root.controller.leaveEditMode(false)
            close()
        }
    }

    // A terminal that lost its server says so until it is back.
    // With the store's standby ready, it takes over by itself shortly; a
    // manager can make it take over now. Without one, it can work offline:
    // new checks, paid in cash, sent to the store once it's back.
    Rectangle {
        id: offline
        readonly property bool standby: !!root.controller.pos && (root.controller.pos as PosService).standbyReady
        readonly property int held: root.controller.pos ? root.controller.pos.heldCount : 0
        readonly property bool working: !!root.controller.pos && root.controller.pos.offlineMode
        readonly property bool canWork: !!root.controller.pos && root.controller.pos.offlineReady && !standby
        readonly property int made: root.controller.pos ? root.controller.pos.offlinePending : 0
        objectName: "offlineBar"
        visible: !!root.controller.pos && (!root.controller.pos.online || working)
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 12
        width: offlineRow.implicitWidth + 40
        height: offlineRow.implicitHeight + 20
        radius: Math.min(height / 2, 30)
        color: working ? "#e08a5a00" : "#e0b83232"
        z: 10
        RowLayout {
            id: offlineRow
            anchors.centerIn: parent
            spacing: 16
            Text {
                color: "white"
                font.pixelSize: 18
                font.bold: true
                text: offline.working && root.controller.pos.online
                      ? qsTr("The store's computer is back: finish this check, and this screen goes back to it.")
                      : offline.working
                      ? qsTr("Working offline: new checks, paid in cash.") + "\n"
                        + (offline.made > 0 ? qsTr("%n check(s) made offline go to the store's computer once it's back.", "", offline.made)
                                            : qsTr("They go to the store's computer once it's back."))
                      : (offline.standby ? qsTr("The main server isn't answering.\nThe standby computer takes over in a few seconds.")
                                         : qsTr("No connection. Reconnecting…"))
                        + (offline.held > 0 ? "\n" + qsTr("%n tap(s) waiting: they go in when it's back.", "", offline.held) : "")
            }
            WidgetKey {
                objectName: "workOffline"
                visible: offline.canWork && !offline.working
                Layout.preferredWidth: 190
                Layout.preferredHeight: 56
                text: qsTr("Work Offline")
                fontScale: 0.3
                baseColor: "#343c49"
                onClicked: root.controller.pos.workOffline()
            }
            WidgetKey {
                objectName: "takeOverNow"
                visible: offline.standby
                Layout.preferredWidth: 190
                Layout.preferredHeight: 56
                text: qsTr("Take Over Now")
                fontScale: 0.3
                baseColor: "#343c49"
                onClicked: approvalPad.takingOver = true
            }
        }
    }

    // This screen behind the store (or ahead of it): said, with the update
    // for a manager to get. Not on the kiosk; Later hides it till the next connection.
    Rectangle {
        id: updateStrip
        objectName: "updateStrip"
        readonly property var info: root.controller.pos ? root.controller.pos.updateInfo : ({})
        readonly property bool manager: !!root.controller.pos && root.controller.pos.loggedIn && root.controller.pos.can("manager")
        property bool later: false
        Connections {
            target: root.controller.pos
            function onOnlineChanged() { updateStrip.later = false }
        }
        visible: !later && (info.behind === true || info.ahead === true) && !selfOrder.visible && !root.pageCovered
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 12
        width: Math.min(parent.width - 24, updateRow.implicitWidth + 32)
        height: updateRow.implicitHeight + 16
        radius: 10
        color: "#e0302a14"
        border.color: "#f5b940"
        border.width: 2
        z: 9
        RowLayout {
            id: updateRow
            anchors.centerIn: parent
            spacing: 12
            Text {
                Layout.maximumWidth: root.width * 0.55
                wrapMode: Text.WordWrap
                color: "white"
                font.pixelSize: 16
                text: {
                    const i = updateStrip.info
                    if (i.ahead)
                        return qsTr("This screen is newer than the store's computer (%1). Update the store's computer.").arg(i.storeText ?? "")
                    if (i.ready)
                        return qsTr("The update is here: %1").arg(i.ready)
                    if (i.downloading !== undefined)
                        return qsTr("Getting the update… %1%").arg(i.downloading)
                    return qsTr("This screen runs an older ViewTouch (%1) than the store (%2).").arg(i.text ?? "").arg(i.storeText ?? "")
                         + (i.update ? (updateStrip.manager ? "" : " " + qsTr("A manager can update it here."))
                                     : " " + qsTr("Put the new version on the store's computer (Manager → Network)."))
                }
            }
            WidgetKey {
                objectName: "updateNow"
                visible: !!updateStrip.info.behind && !!updateStrip.info.update && updateStrip.manager
                         && updateStrip.info.downloading === undefined && (!updateStrip.info.ready || Qt.platform.os === "android")
                Layout.preferredWidth: 170
                Layout.preferredHeight: 48
                text: updateStrip.info.ready ? qsTr("Install") : qsTr("Update Now")
                fontScale: 0.3
                baseColor: "#1f6f3a"
                onClicked: root.controller.pos.getUpdate()
            }
            WidgetKey {
                Layout.preferredWidth: 110
                Layout.preferredHeight: 48
                text: qsTr("Later")
                fontScale: 0.3
                baseColor: "#343c49"
                onClicked: updateStrip.later = true
            }
        }
    }

    // The guest's allergies (Check Options -> Allergy...): touch each one.
    Rectangle {
        id: allergySheet
        objectName: "allergySheet"
        property bool open: false
        // Whose: the whole table (0) or one guest's seat; at first the seat
        // being ordered for.
        property int seat: 0
        readonly property var check: root.controller.pos && root.controller.pos.hasCheck ? root.controller.pos.check : ({})
        readonly property var chosen: seat > 0 ? ((check.seatAllergies ?? {})[String(seat)] ?? []) : (check.allergies ?? [])
        readonly property int seats: {
            let n = Math.max(check.guests ?? 0, check.seat ?? 0)
            for (const k in (check.seatAllergies ?? {})) n = Math.max(n, Number(k))
            for (const l of (root.controller.pos ? root.controller.pos.lines : [])) n = Math.max(n, l.seat ?? 0)
            return n
        }
        readonly property real u: Math.max(14, Math.min(root.width, root.height) / 30)
        anchors.fill: parent
        z: 74
        visible: open && !!root.controller.pos && root.controller.pos.hasCheck
        color: Qt.rgba(0.04, 0.05, 0.07, 0.92)
        Connections {
            target: root.controller
            function onWidgetCommand(name, args) {
                if (name !== "allergies")
                    return
                allergySheet.seat = args && args.seat !== undefined ? args.seat : (allergySheet.check.seat ?? 0)
                allergySheet.open = true
            }
        }
        MouseArea { anchors.fill: parent }
        Column {
            anchors.centerIn: parent
            width: Math.min(parent.width - 2 * allergySheet.u, 46 * allergySheet.u)
            spacing: allergySheet.u
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: allergySheet.seat > 0 ? qsTr("What is seat %1 allergic to?").arg(allergySheet.seat)
                                            : qsTr("What is the guest allergic to?")
                color: "white"
                font.pixelSize: 1.6 * allergySheet.u
                font.bold: true
            }
            // Whose: the whole table, or a seat (a red mark: has some).
            Flow {
                visible: allergySheet.seats > 0
                width: parent.width
                spacing: allergySheet.u / 3
                Repeater {
                    model: allergySheet.seats + 1
                    delegate: WidgetKey {
                        required property int index
                        readonly property bool has: index > 0 ? ((allergySheet.check.seatAllergies ?? {})[String(index)] ?? []).length > 0
                                                              : (allergySheet.check.allergies ?? []).length > 0
                        objectName: "allergySeat" + index
                        width: index === 0 ? 9 * allergySheet.u : 3.6 * allergySheet.u
                        height: 3 * allergySheet.u
                        text: (has ? "⚠ " : "") + (index === 0 ? qsTr("Whole table") : index)
                        fontScale: index === 0 ? 0.3 : 0.4
                        accent: index === allergySheet.seat
                        onClicked: allergySheet.seat = index
                    }
                }
            }
            Grid {
                id: allergenGrid
                anchors.horizontalCenter: parent.horizontalCenter
                // Three across, as wide as fits (a phone: narrower keys, not cut off).
                columns: 3
                spacing: allergySheet.u / 2
                readonly property real keyW: Math.min(14 * allergySheet.u, (parent.width - spacing * (columns - 1)) / columns)
                Repeater {
                    model: root.controller.pos ? root.controller.pos.allergenList() : []
                    delegate: WidgetKey {
                        required property var modelData
                        readonly property bool on: allergySheet.chosen.includes(modelData.id)
                        objectName: "allergy-" + modelData.id
                        width: allergenGrid.keyW
                        height: 4 * allergySheet.u
                        text: (on ? "✓ " : "") + modelData.name
                        fontScale: 0.3
                        baseColor: on ? "#c62828" : "#343c49"
                        onClicked: {
                            const list = allergySheet.chosen.slice()
                            const at = list.indexOf(modelData.id)
                            if (at >= 0) list.splice(at, 1); else list.push(modelData.id)
                            root.controller.pos.setAllergies(list, allergySheet.seat)
                        }
                    }
                }
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("The kitchen sees it on the ticket and the kitchen screen, and items with it are marked in red. For anything else, add a note.")
                color: "#c8ced8"
                font.pixelSize: 0.9 * allergySheet.u
            }
            WidgetKey {
                objectName: "allergyDone"
                anchors.horizontalCenter: parent.horizontalCenter
                width: 16 * allergySheet.u
                height: 4 * allergySheet.u
                text: qsTr("Done")
                fontScale: 0.3
                baseColor: "#1f6f3a"
                onClicked: allergySheet.open = false
            }
        }
    }

    // A coupon's code (Pay -> Coupon…): typed, or scanned (a scanner types
    // it and presses Enter). The ones on the check, to take off.
    Rectangle {
        id: couponSheet
        objectName: "couponSheet"
        property bool open: false
        readonly property var coupons: root.controller.pos && root.controller.pos.hasCheck
                                       ? (root.controller.pos.check.coupons ?? []) : []
        readonly property real u: Math.max(14, Math.min(root.width, root.height) / 30)
        function apply() {
            if (couponCode.text.trim() === "")
                return
            root.controller.pos.applyCoupon(couponCode.text)
            couponCode.text = ""
        }
        anchors.fill: parent
        z: 74
        visible: open && !!root.controller.pos && root.controller.pos.hasCheck
        color: Qt.rgba(0.04, 0.05, 0.07, 0.92)
        onVisibleChanged: if (visible) couponCode.forceActiveFocus()
        Connections {
            target: root.controller
            function onWidgetCommand(name, args) { if (name === "coupon") couponSheet.open = true }
        }
        MouseArea { anchors.fill: parent }
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.max(couponSheet.u, parent.height * 0.06)
            width: Math.min(parent.width - 2 * couponSheet.u, 34 * couponSheet.u)
            spacing: couponSheet.u * 0.8
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("Type or scan the coupon")
                color: "white"
                font.pixelSize: 1.6 * couponSheet.u
                font.bold: true
            }
            RowLayout {
                width: parent.width
                spacing: couponSheet.u / 2
                TextField {
                    id: couponCode
                    objectName: "couponCode"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 3.4 * couponSheet.u
                    font.pixelSize: 1.4 * couponSheet.u
                    font.capitalization: Font.AllUppercase
                    inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
                    placeholderText: qsTr("Code")
                    onAccepted: couponSheet.apply()
                }
                WidgetKey {
                    objectName: "couponApply"
                    Layout.preferredWidth: 9 * couponSheet.u
                    Layout.preferredHeight: 3.4 * couponSheet.u
                    text: qsTr("Apply")
                    fontScale: 0.3
                    baseColor: "#2f6fd6"
                    onClicked: couponSheet.apply()
                }
            }
            Repeater {
                model: couponSheet.coupons
                delegate: RowLayout {
                    required property var modelData
                    width: parent.width
                    spacing: couponSheet.u / 2
                    Text {
                        Layout.fillWidth: true
                        text: modelData.name + "  ·  " + (modelData.working ? qsTr("%1 off").arg(modelData.off)
                                                                            : qsTr("takes nothing off now"))
                        color: modelData.working ? "#7ee2a8" : "#f5b940"
                        font.pixelSize: 1.1 * couponSheet.u
                        wrapMode: Text.WordWrap
                    }
                    WidgetKey {
                        objectName: "couponRemove-" + modelData.id
                        Layout.preferredWidth: 9 * couponSheet.u
                        Layout.preferredHeight: 2.8 * couponSheet.u
                        text: qsTr("Remove")
                        fontScale: 0.3
                        baseColor: "#5a2a2a"
                        onClicked: root.controller.pos.removeCoupon(modelData.id)
                    }
                }
            }
            WidgetKey {
                objectName: "couponDone"
                anchors.horizontalCenter: parent.horizontalCenter
                width: 14 * couponSheet.u
                height: 3.4 * couponSheet.u
                text: qsTr("Done")
                fontScale: 0.3
                baseColor: "#1f6f3a"
                onClicked: { couponSheet.open = false; couponCode.text = ""; pageView.forceActiveFocus() }
            }
        }
    }

    // Tax exempt (Check… -> Tax Exempt…): the buyer's exemption certificate,
    // its number and who, kept with the check; the tax comes off. Managers.
    Rectangle {
        id: exemptSheet
        objectName: "exemptSheet"
        property bool open: false
        readonly property string current: root.controller.pos && root.controller.pos.hasCheck
                                          ? (root.controller.pos.check.taxExempt ?? "") : ""
        readonly property real u: Math.max(14, Math.min(root.width, root.height) / 30)
        anchors.fill: parent
        z: 74
        visible: open && !!root.controller.pos && root.controller.pos.hasCheck
        color: Qt.rgba(0.04, 0.05, 0.07, 0.92)
        onVisibleChanged: if (visible) { exemptCert.text = current; exemptCert.forceActiveFocus() }
        Connections {
            target: root.controller
            function onWidgetCommand(name, args) { if (name === "taxExempt") exemptSheet.open = true }
        }
        MouseArea { anchors.fill: parent }
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.max(exemptSheet.u, parent.height * 0.06)
            width: Math.min(parent.width - 2 * exemptSheet.u, 34 * exemptSheet.u)
            spacing: exemptSheet.u * 0.8
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("Tax exempt")
                color: "white"
                font.pixelSize: 1.6 * exemptSheet.u
                font.bold: true
            }
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: qsTr("The buyer's exemption certificate: its number and who it's for (12-3456789, St. Mary's School). It's kept with the check and printed on the receipt; keep a copy of the certificate.")
                color: "#b8c0cc"
                font.pixelSize: 1.0 * exemptSheet.u
            }
            TextField {
                id: exemptCert
                objectName: "exemptCert"
                width: parent.width
                height: 3.4 * exemptSheet.u
                font.pixelSize: 1.3 * exemptSheet.u
                placeholderText: qsTr("Certificate number and who")
                onAccepted: if (text.trim() !== "") { root.controller.pos.setTaxExempt(text); exemptSheet.open = false }
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: exemptSheet.u
                WidgetKey {
                    objectName: "exemptApply"
                    width: 12 * exemptSheet.u
                    height: 3.4 * exemptSheet.u
                    text: qsTr("Tax Exempt")
                    fontScale: 0.3
                    baseColor: "#2f6fd6"
                    enabled: exemptCert.text.trim() !== ""
                    onClicked: { root.controller.pos.setTaxExempt(exemptCert.text); exemptSheet.open = false }
                }
                WidgetKey {
                    objectName: "exemptRemove"
                    visible: exemptSheet.current !== ""
                    width: 12 * exemptSheet.u
                    height: 3.4 * exemptSheet.u
                    text: qsTr("Tax It Again")
                    fontScale: 0.3
                    baseColor: "#5a2a2a"
                    onClicked: { root.controller.pos.setTaxExempt(""); exemptSheet.open = false }
                }
                WidgetKey {
                    objectName: "exemptCancel"
                    width: 10 * exemptSheet.u
                    height: 3.4 * exemptSheet.u
                    text: qsTr("Cancel")
                    fontScale: 0.3
                    onClicked: { exemptSheet.open = false; pageView.forceActiveFocus() }
                }
            }
        }
    }

    // A bar tab's card on file (Pay -> Card on File…): hold one; charge it
    // for the tab with a tip; or let it go.
    Rectangle {
        id: holdSheet
        objectName: "holdSheet"
        property bool open: false
        readonly property var check: root.controller.pos && root.controller.pos.hasCheck ? root.controller.pos.check : ({})
        readonly property var hold: check.hold ?? ({})
        readonly property bool held: !!hold.card
        readonly property var totals: root.controller.pos ? root.controller.pos.totals : ({})
        readonly property real u: Math.max(14, Math.min(root.width, root.height) / 30)
        function charge(percent, cents) {
            root.controller.pos.chargeHold(percent, cents)
            holdSheet.open = false
        }
        anchors.fill: parent
        z: 74
        visible: open && !!root.controller.pos && root.controller.pos.hasCheck
        color: Qt.rgba(0.04, 0.05, 0.07, 0.92)
        Connections {
            target: root.controller
            function onWidgetCommand(name, args) { if (name === "cardOnFile") holdSheet.open = true }
        }
        MouseArea { anchors.fill: parent }
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.max(holdSheet.u, parent.height * 0.05)
            width: Math.min(parent.width - 2 * holdSheet.u, 36 * holdSheet.u)
            spacing: holdSheet.u * 0.7
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: holdSheet.held ? holdSheet.hold.card : qsTr("Hold the guest's card")
                color: "white"
                font.pixelSize: 1.6 * holdSheet.u
                font.bold: true
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: holdSheet.held
                      ? qsTr("%1 set aside since %2, not charged. Owed now: %3.").arg(holdSheet.hold.amount)
                            .arg(holdSheet.hold.since).arg(holdSheet.totals.balance ?? "")
                      : holdSheet.check.tabHold
                        ? qsTr("The bank sets %1 aside on the card; nothing is charged. When the tab closes, the card is charged for the tab and the tip.").arg(holdSheet.check.tabHold)
                        : qsTr("Holding cards is off (Manager -> Store Settings).")
                color: "#c8ced8"
                font.pixelSize: 1.0 * holdSheet.u
            }
            WidgetKey {
                objectName: "holdStart"
                visible: !holdSheet.held && !!holdSheet.check.tabHold
                width: parent.width
                height: 3.6 * holdSheet.u
                text: qsTr("Hold Card")
                fontScale: 0.3
                baseColor: "#2f6fd6"
                onClicked: { holdSheet.open = false; root.controller.command("holdCard") }
            }
            Text {
                visible: holdSheet.held
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Charge it, with a tip of")
                color: "white"
                font.pixelSize: 1.1 * holdSheet.u
            }
            Row {
                visible: holdSheet.held
                width: parent.width
                spacing: holdSheet.u / 2
                Repeater {
                    model: [0, 15, 18, 20]
                    delegate: WidgetKey {
                        required property int modelData
                        objectName: "holdTip-" + modelData
                        width: (parent.width - 3 * parent.spacing) / 4
                        height: 3.6 * holdSheet.u
                        text: modelData === 0 ? qsTr("No Tip") : modelData + "%"
                        fontScale: 0.3
                        baseColor: "#1f6f3a"
                        onClicked: holdSheet.charge(modelData * 100, 0)
                    }
                }
            }
            RowLayout {
                visible: holdSheet.held
                width: parent.width
                spacing: holdSheet.u / 2
                TextField {
                    id: holdTip
                    objectName: "holdTipAmount"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 3.2 * holdSheet.u
                    font.pixelSize: 1.2 * holdSheet.u
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    placeholderText: qsTr("Tip amount, e.g. 8.00")
                }
                WidgetKey {
                    objectName: "holdTipCharge"
                    Layout.preferredWidth: 14 * holdSheet.u
                    Layout.preferredHeight: 3.2 * holdSheet.u
                    text: qsTr("Charge with This Tip")
                    fontScale: 0.28
                    baseColor: "#1f6f3a"
                    enabled: holdTip.text.trim() !== "" && !isNaN(Number(holdTip.text.replace(",", ".")))
                    onClicked: {
                        const cents = Math.round(Number(holdTip.text.replace(",", ".")) * 100)
                        holdTip.text = ""
                        holdSheet.charge(0, cents)
                    }
                }
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: holdSheet.u / 2
                WidgetKey {
                    objectName: "holdRelease"
                    visible: holdSheet.held
                    width: 16 * holdSheet.u
                    height: 3.2 * holdSheet.u
                    text: qsTr("Let the Card Go")
                    fontScale: 0.28
                    baseColor: "#5a2a2a"
                    onClicked: { root.controller.pos.releaseHold(); holdSheet.open = false }
                }
                WidgetKey {
                    objectName: "holdClose"
                    width: 12 * holdSheet.u
                    height: 3.2 * holdSheet.u
                    text: qsTr("Close")
                    fontScale: 0.28
                    baseColor: "#343c49"
                    onClicked: { holdSheet.open = false; holdTip.text = ""; pageView.forceActiveFocus() }
                }
            }
        }
    }

    // A card taken offline that the bank declined once sent: managers see it
    // until they've dealt with it (OK).
    Column {
        id: cardAlerts
        objectName: "cardAlerts"
        readonly property var alerts: root.controller.pos ? root.controller.pos.cardAlerts : []
        visible: alerts.length > 0 && !selfOrder.visible && !root.pageCovered
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 76
        spacing: 6
        z: 9
        Repeater {
            model: cardAlerts.alerts
            delegate: Rectangle {
                required property var modelData
                width: Math.min(root.width - 24, alertRow.implicitWidth + 28)
                height: alertRow.implicitHeight + 14
                radius: 10
                color: "#e0b83232"
                RowLayout {
                    id: alertRow
                    anchors.centerIn: parent
                    spacing: 12
                    Text {
                        Layout.maximumWidth: root.width * 0.6
                        wrapMode: Text.WordWrap
                        color: "white"
                        font.pixelSize: 16
                        font.bold: true
                        text: modelData.text
                    }
                    WidgetKey {
                        Layout.preferredWidth: 90
                        Layout.preferredHeight: 44
                        text: qsTr("OK")
                        fontScale: 0.3
                        baseColor: "#343c49"
                        onClicked: root.controller.pos.seeCardAlert(modelData.ref)
                    }
                }
            }
        }
    }

    // A printer that needs someone (out of paper, cover open, not answering):
    // on every staff screen until it's fixed. Not on the kiosk (guests).
    Column {
        id: printerAlerts
        objectName: "printerAlerts"
        readonly property var alerts: root.controller.pos ? (root.controller.pos as PosService).printerAlerts : []
        visible: alerts.length > 0 && !selfOrder.visible
        anchors.top: parent.top
        anchors.topMargin: offline.visible ? offline.height + 20 : 12
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6
        z: 11
        Repeater {
            model: printerAlerts.alerts
            delegate: Rectangle {
                required property var modelData
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(alertText.implicitWidth + 64, root.width - 32)
                height: alertText.implicitHeight + 18
                radius: Math.min(height / 2, 24)
                color: modelData.urgent ? "#e8b83232" : "#e8a86a12"
                Text {
                    id: alertText
                    objectName: "printerAlert-" + parent.modelData.id
                    anchors.centerIn: parent
                    width: Math.min(implicitWidth, root.width - 80)
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    text: "🖨  " + parent.modelData.text
                    color: "white"
                    font.pixelSize: 18
                    font.bold: true
                }
            }
        }
    }

    // Practice (training): everyone can see nothing here is real.
    Rectangle {
        visible: !root.editing && root.controller.pos !== null && (root.controller.pos as PosService).training
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: practiceText.implicitWidth + 40
        height: practiceText.implicitHeight + 10
        radius: 6
        color: "#f5b940"
        z: 50
        Text {
            id: practiceText
            anchors.centerIn: parent
            text: qsTr("PRACTICE - nothing here is a real sale")
            font.bold: true
            font.pixelSize: 16
            color: "#1b1b1b"
        }
    }

    // Messages from other screens, across the top.
    MessageBanner {
        anchors.top: parent.top
        anchors.left: parent.left
        width: root.posWidth
        z: 55
        pos: root.controller.pos as PosService
        kitchenScreen: root.controller.pageKind === "kitchen"
    }

    // A manager's PIN for something this person may not do on their own.
    // A self-order kiosk: guests order on their own; nothing else shows.
    SelfOrder {
        id: selfOrder
        objectName: "selfOrder"
        anchors.fill: parent
        z: 55
        visible: !root.editing && root.controller.pos !== null && ((root.controller.pos as PosService).selfOrder.on ?? false)
        pos: root.controller.pos as PosService
        onManagerExit: approvalPad.leavingKiosk = true
    }

    // Screen saver: dimmed after no touches; the first touch only wakes it.
    Rectangle {
        id: screenSaver
        objectName: "screenSaver"
        anchors.fill: parent
        z: 90
        visible: root.controller.asleep
        color: "#f2050608"
        Column {
            anchors.centerIn: parent
            spacing: 12
            opacity: 0.55
            // The store's logo (Store Settings), when there is one.
            Image {
                objectName: "screenSaverLogo"
                readonly property var pos: root.controller.pos
                anchors.horizontalCenter: parent.horizontalCenter
                visible: status === Image.Ready
                source: pos && screenSaver.visible ? (pos.imageRevision < 0 ? undefined : pos.imageUrl("logo:")) : ""
                sourceSize.height: Math.min(root.width, root.height) * 0.3
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.controller.pos ? root.controller.pos.storeName : ""
                color: "white"
                font.pixelSize: Math.min(root.width, root.height) * 0.06
                font.bold: true
            }
            Text {
                id: saverClock
                anchors.horizontalCenter: parent.horizontalCenter
                color: "#c9d1de"
                font.pixelSize: Math.min(root.width, root.height) * 0.1
                Timer {
                    interval: 1000
                    repeat: true
                    triggeredOnStart: true
                    running: screenSaver.visible
                    onTriggered: saverClock.text = Qt.locale().toString(new Date(), "h:mm AP")
                }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Touch to wake")
                color: "#8a94a6"
                font.pixelSize: Math.min(root.width, root.height) * 0.03
            }
        }
        // Wakes when the finger lifts: the page is still off during the press.
        MouseArea { anchors.fill: parent; onReleased: root.controller.wake() }
    }

    // Taking a card on this screen's reader: the amount and Cancel, over
    // everything. (On a Stripe reader, Stripe's own screen covers this.)
    Rectangle {
        id: cardWait
        objectName: "cardWait"
        readonly property CardReader reader: root.controller.cardReader
        anchors.fill: parent
        z: 75
        visible: reader.busy
        color: Qt.rgba(0.04, 0.05, 0.07, 0.92)
        readonly property real u: Math.max(14, Math.min(root.width, root.height) / 28)
        MouseArea { anchors.fill: parent }   // nothing behind it while the card is taken
        Column {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.86, cardWait.u * 30)
            spacing: cardWait.u
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: "💳"
                font.pixelSize: cardWait.u * 4
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: cardWait.reader.amount
                color: "white"
                font.pixelSize: cardWait.u * 2.6
                font.bold: true
            }
            Text {
                objectName: "cardKinds"
                visible: text !== ""
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: cardWait.reader.cardKinds
                color: "#f5b940"
                font.pixelSize: cardWait.u * 0.9
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: cardWait.reader.status
                color: "#c8cfda"
                font.pixelSize: cardWait.u
            }
            // Practice (the simulated reader), or Stripe test mode on a reader
            // beside the screen: "tap" a test card. A debit card pays no surcharge.
            Row {
                visible: cardWait.reader.testCards
                width: parent.width
                spacing: cardWait.u * 0.5
                readonly property real keyW: (width - spacing * 2) / 3
                WidgetKey {
                    objectName: "testCardApprove"
                    width: parent.keyW
                    height: cardWait.u * 2.6
                    fontScale: 0.28
                    baseColor: "#1f8a4c"
                    text: qsTr("Tap Credit Card")
                    onClicked: cardWait.reader.presentTestCard("credit")
                }
                WidgetKey {
                    objectName: "testCardDebit"
                    width: parent.keyW
                    height: cardWait.u * 2.6
                    fontScale: 0.28
                    baseColor: "#1f6f78"
                    text: qsTr("Tap Debit Card")
                    onClicked: cardWait.reader.presentTestCard("debit")
                }
                WidgetKey {
                    objectName: "testCardDecline"
                    width: parent.keyW
                    height: cardWait.u * 2.6
                    fontScale: 0.28
                    baseColor: "#8a2c30"
                    text: qsTr("Tap Declined Card")
                    onClicked: cardWait.reader.presentTestCard("declined")
                }
            }
            WidgetKey {
                objectName: "cardCancel"
                width: parent.width
                height: cardWait.u * 3
                fontScale: 0.36
                text: qsTr("Cancel")
                onClicked: cardWait.reader.cancel()
            }
        }
    }

    // The guest's receipt: where to print it, an email (a Stripe card), or none.
    // Near the top, so the keyboard below leaves the email box in view.
    Rectangle {
        id: receiptSheet
        objectName: "receiptSheet"
        readonly property var offer: root.controller.pos ? (root.controller.pos as PosService).receiptOffer : ({})
        readonly property real u: Math.max(14, Math.min(root.width, root.height) / 30)
        anchors.fill: parent
        z: 74
        visible: !!offer.checkId
        color: Qt.rgba(0.04, 0.05, 0.07, 0.9)
        onVisibleChanged: if (visible) receiptEmail.text = offer.email ?? ""
        MouseArea { anchors.fill: parent }
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.max(receiptSheet.u, parent.height * 0.08)
            width: Math.min(parent.width * 0.9, receiptSheet.u * 30)
            spacing: receiptSheet.u * 0.6
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("Receipt · %1 · %2").arg(receiptSheet.offer.label ?? "").arg(receiptSheet.offer.total ?? "")
                color: "white"
                font.pixelSize: receiptSheet.u * 1.3
                font.bold: true
            }
            Text {
                visible: (receiptSheet.offer.printers ?? []).length > 0
                text: qsTr("Print on")
                color: "#c8cfda"
                font.pixelSize: receiptSheet.u * 0.9
            }
            Flow {
                width: parent.width
                spacing: receiptSheet.u * 0.4
                Repeater {
                    model: receiptSheet.offer.printers ?? []
                    delegate: WidgetKey {
                        required property var modelData
                        objectName: "receiptOn-" + modelData.id
                        width: (parent.width - receiptSheet.u * 0.4) / 2
                        height: receiptSheet.u * 2.8
                        fontScale: 0.34
                        text: modelData.name
                        baseColor: "#2f5fb0"
                        onClicked: (root.controller.pos as PosService).printReceiptOn(receiptSheet.offer.checkId, modelData.id)
                    }
                }
            }
            Text {
                visible: !(receiptSheet.offer.printers ?? []).length
                width: parent.width
                wrapMode: Text.WordWrap
                text: qsTr("No printer prints receipts yet (Manager → Printers → Prints receipts).")
                color: "#c8cfda"
                font.pixelSize: receiptSheet.u * 0.8
            }
            // A card from a Stripe reader: Stripe emails its receipt.
            Text {
                visible: !!receiptSheet.offer.canEmail
                text: qsTr("Or email it")
                color: "#c8cfda"
                font.pixelSize: receiptSheet.u * 0.9
            }
            Row {
                visible: !!receiptSheet.offer.canEmail
                width: parent.width
                spacing: receiptSheet.u * 0.4
                TextField {
                    id: receiptEmail
                    objectName: "receiptEmail"
                    width: parent.width - emailSend.width - parent.spacing
                    height: receiptSheet.u * 2.8
                    font.pixelSize: receiptSheet.u
                    inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                    placeholderText: qsTr("guest@example.com")
                    onAccepted: emailSend.clicked()
                }
                WidgetKey {
                    id: emailSend
                    objectName: "receiptEmailSend"
                    width: receiptSheet.u * 6
                    height: receiptSheet.u * 2.8
                    fontScale: 0.34
                    text: qsTr("Email")
                    baseColor: "#1f8a4c"
                    onClicked: (root.controller.pos as PosService).emailReceipt(receiptSheet.offer.checkId, receiptEmail.text)
                }
            }
            WidgetKey {
                objectName: "noReceipt"
                width: parent.width
                height: receiptSheet.u * 2.6
                fontScale: 0.34
                text: qsTr("No Receipt")
                onClicked: (root.controller.pos as PosService).noReceipt()
            }
        }
    }

    // A new store's setup guide (managers; Manager -> Setup Guide…).
    Loader {
        id: setupLoader
        anchors.fill: parent
        transform: Translate { y: -root.lift }
        z: 70
        active: root.controller.setupOpen
        sourceComponent: SetupGuide { controller: root.controller }
    }

    JobChooser {
        id: jobChooser
        anchors.fill: parent
        z: 59
        pos: root.controller.pos as PosService
    }

    CashTipsPrompt {
        id: cashTipsPrompt
        anchors.fill: parent
        z: 59
        pos: root.controller.pos as PosService
    }

    ReasonSheet {
        id: reasonSheet
        anchors.fill: parent
        z: 59
        pos: root.controller.pos as PosService
    }

    HandoffSheet {
        id: handoffSheet
        anchors.fill: parent
        z: 58
        pos: root.controller.pos as PosService
    }

    ApprovalPad {
        id: approvalPad
        anchors.fill: parent
        z: 60
        pos: root.controller.pos as PosService
    }

    Rectangle {
        id: toastBox
        objectName: "toast"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        // Above the row of buttons most pages have at the bottom (not over
        // their labels); at the top while the keyboard is up (not over the
        // field being typed in), and on a phone (its pages end in rows of
        // buttons of their own height: it covered Save and Add).
        readonly property bool atTop: root.keyboardUp || (root.controller && root.controller.formFactor === "phone")
        anchors.bottomMargin: atTop ? root.height - height - 24 : Math.max(24, root.height * 0.135)
        width: Math.min(toastText.implicitWidth + 40, root.width - 40)
        height: toastText.implicitHeight + 20
        radius: 18
        color: "#e0101418"
        opacity: 0
        visible: opacity > 0

        Text {
            id: toastText
            anchors.centerIn: parent
            width: parent.width - 40
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: "white"
            font.pixelSize: 16
        }

        Behavior on opacity { NumberAnimation { duration: 150 } }
        Timer { id: toastTimer; interval: 2600; onTriggered: toastBox.opacity = 0 }
    }

    // What was just added, for a moment ("+ Caesar · Seat 2"): seen from the
    // corner of the eye while looking at the guest. Touches go through it.
    Rectangle {
        id: addedFlash
        objectName: "addedFlash"
        property string text: ""
        property bool waiting: false
        Connections {
            target: root.controller
            function onAddedFlash(text, waiting) {
                addedFlash.text = text
                addedFlash.waiting = waiting
                addedFlash.opacity = 1
                addedTimer.restart()
            }
        }
        z: 49
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.round(root.height * 0.36)
        width: Math.min(addedText.implicitWidth + 48, root.width - 32)
        height: addedText.implicitHeight + 22
        radius: height / 2
        color: waiting ? "#e0805a10" : "#e0186b3a"
        border.color: waiting ? "#f5b940" : "#3ddc84"
        border.width: 2
        opacity: 0
        visible: opacity > 0
        Text {
            id: addedText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, root.width - 80)
            elide: Text.ElideRight
            color: "white"
            font.pixelSize: 22
            font.bold: true
            text: addedFlash.text + (addedFlash.waiting ? "  " + qsTr("(waiting)") : "")
        }
        Behavior on opacity { NumberAnimation { duration: 180 } }
        Timer { id: addedTimer; interval: 900; onTriggered: addedFlash.opacity = 0 }
    }

    // A held button: what it does (touch to close; gone after a while).
    Rectangle {
        id: explainCard
        objectName: "explainCard"
        readonly property var info: root.controller.explanation
        visible: !!info.text
        z: 50
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 24
        width: Math.min(root.width - 40, 640)
        height: explainCol.implicitHeight + 32
        radius: 14
        color: "#f0101418"
        border.color: "#4c8dff"
        border.width: 2
        onInfoChanged: if (info.text) explainTimer.restart()
        Timer { id: explainTimer; interval: 9000; onTriggered: root.controller.clearExplanation() }
        Column {
            id: explainCol
            x: 20; y: 16
            width: parent.width - 40
            spacing: 6
            Text {
                width: parent.width
                text: explainCard.info.title ?? ""
                color: "white"
                font.pixelSize: 22
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Text {
                width: parent.width
                text: explainCard.info.text ?? ""
                color: "#d7dde8"
                font.pixelSize: 18
                wrapMode: Text.WordWrap
            }
            Text {
                width: parent.width
                text: qsTr("Hold any button to see what it does. Touch here to close.")
                color: "#8a94a6"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
        }
        MouseArea { anchors.fill: parent; onClicked: root.controller.clearExplanation() }
    }

    Connections {
        target: root.controller
        function onStatusChanged() { root.toast(root.controller.statusText) }
    }
    Connections {
        target: root.editor
        ignoreUnknownSignals: true
        function onNoticeChanged() { root.toast(root.editor.notice) }
    }

    // The stress test's numbers (vtmodern --stress). Only built then: a
    // store's window has no panel, and no tour of the pages.
    Loader {
        anchors.fill: parent
        z: 95
        active: root.stress !== null
        sourceComponent: Item {
            StressPanel { stress: root.stress; controller: root.controller }
        }
    }
}
