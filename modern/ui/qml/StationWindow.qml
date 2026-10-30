import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// The home screen of a computer that is only for ViewTouch: ViewTouch runs
// on top of it, and this shows when ViewTouch is closed (Manager -> Close
// ViewTouch) or stops by itself (then it starts again). Icons for the
// computer (network, time, updates, restart...), and on the main register the
// store's setup screens. The first time: what this computer is.
ApplicationWindow {
    id: root

    required property StationController station
    property bool kiosk: false

    width: 1280
    height: 800
    visible: true
    title: qsTr("ViewTouch")
    color: Ui.deep

    readonly property real u: Math.max(10, Math.min(width / 64, height / 40))
    readonly property bool portrait: height > width
    // Eight tiles to a row on a wide screen, four on a tall one (at 1280 x 800
    // eight fit only if a tile may be 6.5 units wide).
    readonly property real tileWidth: Math.max(root.u * 6,
        (width - root.u * 2.8 - root.u * ((portrait ? 4 : 8) - 1)) / (portrait ? 4 : 8) - 0.5)
    // Narrower tiles are a little shorter, so three rows fit the height.
    readonly property real tileHeight: Math.min(root.u * 7.6, Math.max(root.u * 5.8, tileWidth * 0.88))
    // home | network | time | updates | help | reset | about | setup
    property string panel: station.role === "" ? "setup" : "home"
    // A newer ViewTouch found online (its version), to install.
    property string onlineReady: ""
    Timer { id: restartSoon; interval: 3000; onTriggered: Qt.quit() }

    // Settings opened with the PIN stay open for two minutes.
    property bool unlocked: false
    property var afterPin: null

    readonly property var roleNames: ({
        store: qsTr("Main register"), terminal: qsTr("Register"), kitchen: qsTr("Kitchen screen"),
        selforder: qsTr("Self-order kiosk"), timeclock: qsTr("Time clock")
    })

    palette {
        window: Ui.deep
        windowText: Ui.text
        base: "#0f1216"
        text: Ui.text
        button: Ui.button
        buttonText: Ui.text
        highlight: Ui.accent
        highlightedText: "white"
        placeholderText: "#6b7585"
    }

    Shortcut {
        enabled: !root.kiosk
        sequences: ["F11"]
        onActivated: root.visibility === Window.FullScreen ? root.showNormal() : root.showFullScreen()
    }

    function withPin(action) {
        if (!station.hasPin || unlocked) {
            action()
            return
        }
        afterPin = action
        pinPad.open()
    }

    // "America/New_York" -> "New York"
    function zoneName(zone) {
        return (zone || "").split("/").pop().replace(/_/g, " ")
    }

    function say(text, good) {
        toast.text = text
        toast.good = good
        toast.visible = true
        toastTimer.restart()
    }

    Timer { id: lockTimer; interval: 120000; onTriggered: root.unlocked = false }
    onPanelChanged: if (panel === "home") lockTimer.restart()

    Connections {
        target: root.station
        function onFinished(action, ok, message) {
            const what = {
                wifi: [qsTr("Connected."), qsTr("Couldn't connect")],
                timezone: [qsTr("Time zone changed."), qsTr("Couldn't change the time zone")],
                update: [qsTr("Up to date."), qsTr("The update didn't finish")],
                "update-usb": [qsTr("ViewTouch updated."), qsTr("Nothing installed")],
                "online-check": ["", qsTr("Couldn't look for a new ViewTouch")],
                "online-update": ["", qsTr("Not installed")],
                ssh: [qsTr("Done."), qsTr("Couldn't change remote help")],
                backup: [qsTr("Backed up."), qsTr("The backup failed")],
                erase: [qsTr("The store is erased; ViewTouch starts fresh."), qsTr("Not erased")],
                forget: [qsTr("Choose what this computer is."), qsTr("Couldn't start over")],
                setup: [qsTr("Set up."), qsTr("Not set up")],
                maintainer: ["", ""],
                reboot: ["", qsTr("Couldn't restart")],
                poweroff: ["", qsTr("Couldn't shut down")]
            }[action] ?? [qsTr("Done."), qsTr("That didn't work")]
            // A newer ViewTouch online: what the helper's last line says.
            if (ok && action === "online-check") {
                const words = (message ?? "").split(" ")
                root.onlineReady = words[0] === "READY" ? words[1] : ""
                root.say(words[0] === "READY" ? qsTr("ViewTouch %1 is ready (this computer has %2).").arg(words[1]).arg(words[2] ?? "")
                                              : qsTr("This computer has the newest ViewTouch (%1).").arg(words[1] ?? ""), true)
            } else if (ok && action === "online-update") {
                root.onlineReady = ""
                root.say(qsTr("ViewTouch %1 is installed. Starting it again…").arg((message ?? "").split(" ")[1] ?? ""), true)
                restartSoon.start()   // the home screen on the new version (its service starts it again)
            }
            if (action === "maintainer") {
                root.say(message, ok)
                return
            }
            if (ok && what[0] !== "")
                root.say(what[0], true)
            else if (!ok)
                root.say(what[1] + (message ? ": " + message : "."), false)
            if (action === "forget" && ok) {
                setupState.reset()
                root.panel = "setup"
            }
            if (action === "erase" && ok)
                root.station.startPos("setup")
        }
    }

    // --- pieces ------------------------------------------------------------------------

    component Tile: Rectangle {
        id: tile
        property string icon
        property string label
        property string detail
        property color tint: "#2a313c"
        property bool locked: false
        signal tapped()
        implicitWidth: root.tileWidth
        implicitHeight: root.tileHeight
        radius: root.u * 0.8
        color: tap.pressed ? Qt.darker(tint, 1.3) : tint
        border.width: 1
        border.color: Qt.lighter(tint, 1.3)
        // A short tile (a small screen): a smaller icon and less space, so a
        // name on two lines and its detail stay inside.
        readonly property bool low: root.tileHeight < root.u * 7
        ColumnLayout {
            anchors.centerIn: parent
            width: parent.width - root.u
            spacing: root.u * (tile.low ? 0.15 : 0.4)
            StationIcon {
                name: tile.icon
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: root.u * (tile.low ? 2.1 : 2.7)
                Layout.preferredHeight: Layout.preferredWidth
            }
            Label {
                text: tile.label
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: root.u * 1.0
                font.bold: true
                color: "white"
                maximumLineCount: 2
                elide: Text.ElideRight
            }
            Label {
                visible: tile.detail !== ""
                text: tile.detail
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                font.pixelSize: root.u * 0.8
                color: "#c4cbd6"
            }
        }
        StationIcon {
            visible: tile.locked && root.station.hasPin && !root.unlocked
            name: "lock"
            anchors { top: parent.top; right: parent.right; margins: root.u * 0.5 }
            width: root.u * 1.3
            height: width
            color: "#9aa3b2"
        }
        TapHandler { id: tap; onTapped: tile.tapped() }
    }

    component BigButton: PosButton {
        id: big
        property color tint: Ui.button
        Layout.preferredHeight: root.u * 3.6
        Layout.minimumWidth: root.u * 9
        font.pixelSize: root.u * 1.1
        font.bold: true
        background: Rectangle {
            radius: root.u * 0.5
            color: !big.enabled ? "#262b33" : big.down ? Qt.darker(big.tint, 1.3) : big.tint
        }
        contentItem: Label {
            text: big.text
            font: big.font
            color: big.enabled ? "white" : "#6b7585"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.WordWrap
        }
    }

    component Heading: Label {
        font.pixelSize: root.u * 1.8
        font.bold: true
        color: "white"
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    component Note: Label {
        font.pixelSize: root.u * 1.05
        color: "#aab3c2"
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    component Section: Label {
        font.pixelSize: root.u * 0.95
        font.bold: true
        font.capitalization: Font.AllUppercase
        color: "#7f8a9c"
        Layout.topMargin: root.u * 0.6
    }

    // A panel: a title with ‹ Back, and what's in it.
    component Panel: ColumnLayout {
        id: p
        property string title
        default property alias content: body.data
        spacing: root.u
        RowLayout {
            spacing: root.u
            BigButton {
                text: "‹ " + qsTr("Back")
                onClicked: root.panel = root.station.role === "" ? "setup" : "home"
            }
            Heading { text: p.title }
        }
        ColumnLayout {
            id: body
            spacing: root.u
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    component Field: TextField {
        Layout.fillWidth: true
        Layout.preferredHeight: root.u * 3.2
        font.pixelSize: root.u * 1.4
        onActiveFocusChanged: if (activeFocus) { keyboard.target = this; keyboard.visible = true }
        background: Rectangle {
            radius: 8
            color: "#0f1216"
            border.width: parent.activeFocus ? 2 : 1
            border.color: parent.activeFocus ? Ui.accent : Ui.line
        }
    }

    // --- the top bar -----------------------------------------------------------------

    Rectangle {
        id: topBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: root.u * 4.4
        color: "#0f1216"
        RowLayout {
            anchors { fill: parent; leftMargin: root.u * 1.4; rightMargin: root.u * 1.4 }
            spacing: root.u
            Label {
                text: "ViewTouch"
                font.pixelSize: root.u * 1.6
                font.bold: true
                color: "white"
            }
            Label {
                text: root.station.role === "" ? qsTr("New computer")
                      : root.station.computerName + "  ·  " + (root.roleNames[root.station.role] ?? "")
                font.pixelSize: root.u * 1.05
                color: "#9aa3b2"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            StationIcon {
                name: root.station.network.wifi ? "wifi" : "network"
                color: root.station.network.online ? "#6fcf97" : "#eb8a7a"
                Layout.preferredWidth: root.u * 1.8
                Layout.preferredHeight: root.u * 1.8
            }
            Label {
                text: root.station.network.online
                      ? (root.station.network.wifi ? root.station.network.wifi + "  " : "") + (root.station.network.address ?? "")
                      : qsTr("No network")
                font.pixelSize: root.u * 0.95
                color: root.station.network.online ? "#c4cbd6" : "#eb8a7a"
            }
            Label {
                id: clock
                font.pixelSize: root.u * 1.5
                font.bold: true
                color: "white"
                Layout.leftMargin: root.u
                function tick() { text = Qt.locale().toString(new Date(), "h:mm AP") }
                Component.onCompleted: tick()
                Timer { interval: 5000; running: true; repeat: true; onTriggered: clock.tick() }
            }
        }
    }

    // --- what's shown ------------------------------------------------------------------

    Flickable {
        id: page
        objectName: "stationPage"
        anchors { left: parent.left; right: parent.right; top: topBar.bottom; bottom: keyboard.visible ? keyboard.top : parent.bottom }
        anchors.margins: root.u * 1.4
        contentHeight: loader.item ? loader.item.implicitHeight : 0
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // More than fits (a small screen): a bar says so, and drags it. Over
        // the tiles' right edge rather than narrowing them (the tiles decide
        // the height, which decides the bar).
        ScrollBar.vertical: TouchScrollBar { id: pageBar }
        Loader {
            id: loader
            width: page.width
            sourceComponent: ({
                home: homePanel, network: networkPanel, time: timePanel, updates: updatesPanel,
                help: helpPanel, reset: resetPanel, about: aboutPanel, setup: setupPanel, maintainer: maintainerPanel
            })[root.panel]
        }
    }

    // --- home ---------------------------------------------------------------------------

    Component {
        id: homePanel
        ColumnLayout {
            spacing: root.u

            // ViewTouch stopped by itself.
            Rectangle {
                visible: root.station.posState === "crashed" || root.station.posState === "gaveUp"
                Layout.fillWidth: true
                implicitHeight: crashRow.implicitHeight + root.u * 2
                radius: root.u * 0.6
                color: "#3a2326"
                border.color: "#b8545a"
                RowLayout {
                    id: crashRow
                    anchors { fill: parent; margins: root.u }
                    spacing: root.u
                    StationIcon { name: "warning"; color: "#ffb4a8"; Layout.preferredWidth: root.u * 2.6; Layout.preferredHeight: root.u * 2.6 }
                    Note {
                        color: "#ffd9d2"
                        text: root.station.posState === "crashed"
                              ? qsTr("ViewTouch closed unexpectedly. Starting it again in %1 s.").arg(root.station.restartIn)
                              : qsTr("ViewTouch stopped three times in a few minutes, so it isn't starting by itself. Try again, restart the computer, or call for help.")
                    }
                    BigButton { text: qsTr("Start Now"); tint: "#2f8a52"; onClicked: root.station.startPos() }
                    BigButton {
                        visible: root.station.posState === "crashed"
                        text: qsTr("Stay Here")
                        onClicked: root.station.stayHome()
                    }
                }
            }

            Tile {
                Layout.fillWidth: true
                implicitHeight: root.u * 5.6
                tint: "#22784a"
                icon: "start"
                label: root.station.posState === "running" ? qsTr("ViewTouch is running") : qsTr("Start ViewTouch")
                onTapped: root.station.startPos()
            }

            Section { visible: root.station.role === "store"; text: qsTr("The store") }
            Flow {
                visible: root.station.role === "store"
                Layout.fillWidth: true
                spacing: root.u
                Tile { icon: "setup"; label: qsTr("Setup Guide"); tint: "#2b4d7e"; onTapped: root.station.startPos("setup") }
                Tile { icon: "menu"; label: qsTr("Menu"); tint: "#2b4d7e"; onTapped: root.station.startPos("menu-builder") }
                Tile { icon: "staff"; label: qsTr("Staff"); tint: "#2b4d7e"; onTapped: root.station.startPos("admin-employees") }
                Tile { icon: "printer"; label: qsTr("Printers"); tint: "#2b4d7e"; onTapped: root.station.startPos("admin-printers") }
                Tile { icon: "addscreen"; label: qsTr("Add a Screen"); tint: "#2b4d7e"; onTapped: root.station.startPos("admin-terminals") }
                Tile { icon: "reports"; label: qsTr("Reports"); tint: "#2b4d7e"; onTapped: root.station.startPos("reports") }
                Tile {
                    icon: "backup"
                    label: qsTr("Back Up Now")
                    tint: "#2b4d7e"
                    onTapped: if (root.station.posState === "running") root.say(qsTr("ViewTouch backs up on its own while it runs."), true)
                              else root.station.backupNow()
                }
            }

            Section { text: qsTr("This computer") }
            Flow {
                Layout.fillWidth: true
                spacing: root.u
                Tile {
                    icon: root.station.network.wifi ? "wifi" : "network"
                    label: qsTr("Network")
                    detail: root.station.network.online ? (root.station.network.address ?? "") : qsTr("Not connected")
                    locked: true
                    onTapped: root.withPin(() => root.panel = "network")
                }
                Tile { icon: "clock"; label: qsTr("Date & Time"); detail: root.zoneName(root.station.timeZone); locked: true; onTapped: root.withPin(() => root.panel = "time") }
                Tile { icon: "update"; label: qsTr("Updates"); locked: true; onTapped: root.withPin(() => root.panel = "updates") }
                Tile {
                    icon: "help"
                    label: qsTr("Remote Help")
                    detail: root.station.remoteSupport ? qsTr("On") : qsTr("Off")
                    locked: true
                    onTapped: root.withPin(() => root.panel = "help")
                }
                Tile { icon: "restart"; label: qsTr("Restart"); tint: "#4a3f24"; onTapped: confirm.ask(qsTr("Restart this computer?"), qsTr("Restart"), () => root.station.restartComputer()) }
                Tile { icon: "power"; label: qsTr("Shut Down"); tint: "#4a3f24"; onTapped: confirm.ask(qsTr("Shut this computer down? Turn it on again with its power button."), qsTr("Shut Down"), () => root.station.shutDown()) }
                Tile { icon: "startover"; label: qsTr("Reset…"); tint: "#4a2a2e"; locked: true; onTapped: root.withPin(() => root.panel = "reset") }
                Tile {
                    objectName: "tileMaintainer"
                    icon: "about"
                    label: qsTr("Maintainer Code")
                    locked: true
                    onTapped: root.withPin(() => root.panel = "maintainer")
                }
                Tile { icon: "about"; label: qsTr("About"); onTapped: root.panel = "about" }
            }
        }
    }

    // --- network ---------------------------------------------------------------------

    Component {
        id: networkPanel
        Panel {
            id: net
            title: qsTr("Network")
            property string chosen: ""
            Component.onCompleted: { root.station.refreshNetwork(); root.station.scanWifi() }
            Note {
                text: root.station.network.wired
                      ? qsTr("Connected with a network cable (%1). A cable is the most reliable.").arg(root.station.network.address)
                      : root.station.network.wifi
                        ? qsTr("Connected to Wi-Fi %1 (%2).").arg(root.station.network.wifi).arg(root.station.network.address)
                        : root.station.network.online
                          ? qsTr("Connected (%1).").arg(root.station.network.address)
                          : qsTr("Not connected. Plug in a network cable, or choose a Wi-Fi network.")
            }
            Note {
                text: qsTr("Keep ViewTouch on the store's own network, never on the Wi-Fi guests use.")
                color: "#e8c46a"
            }
            Section { text: qsTr("Wi-Fi") }
            Note {
                visible: root.station.wifiNetworks.length === 0
                text: root.station.network.wifiAvailable ? qsTr("Looking for Wi-Fi networks…")
                                                          : qsTr("No Wi-Fi networks found. Choosing one turns Wi-Fi on; touch Look Again.")
            }
            Repeater {
                model: root.station.wifiNetworks
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: root.u * 3.6
                    radius: root.u * 0.4
                    color: net.chosen === modelData.name ? "#24426e" : "#232a34"
                    RowLayout {
                        anchors { fill: parent; leftMargin: root.u; rightMargin: root.u }
                        StationIcon { name: "wifi"; color: modelData.signal > 60 ? "#6fcf97" : modelData.signal > 30 ? "#e8c46a" : "#eb8a7a"; Layout.preferredWidth: root.u * 2; Layout.preferredHeight: root.u * 2 }
                        Label { text: modelData.name; font.pixelSize: root.u * 1.2; color: "white"; Layout.fillWidth: true; elide: Text.ElideRight }
                        Label { visible: modelData.connected; text: qsTr("Connected"); color: "#6fcf97"; font.pixelSize: root.u }
                        StationIcon { visible: modelData.secure; name: "lock"; color: "#9aa3b2"; Layout.preferredWidth: root.u * 1.4; Layout.preferredHeight: root.u * 1.4 }
                    }
                    TapHandler {
                        onTapped: {
                            net.chosen = modelData.name
                            password.text = ""
                            if (modelData.secure)
                                password.forceActiveFocus()
                        }
                    }
                }
            }
            RowLayout {
                visible: net.chosen !== ""
                Layout.fillWidth: true
                spacing: root.u
                Field {
                    id: password
                    placeholderText: qsTr("Password for %1").arg(net.chosen)
                    echoMode: TextInput.Password
                }
                BigButton {
                    text: qsTr("Connect")
                    tint: Ui.accent
                    enabled: !root.station.busy
                    onClicked: { keyboard.visible = false; root.station.connectWifi(net.chosen, password.text) }
                }
            }
            BigButton { text: qsTr("Look Again"); onClicked: root.station.scanWifi() }
        }
    }

    // --- date and time -----------------------------------------------------------------

    Component {
        id: timePanel
        Panel {
            id: tz
            title: qsTr("Date & Time")
            property bool all: false
            Note {
                text: qsTr("Time zone: %1.").arg(root.station.timeZone) + " "
                      + (root.station.timeSynced ? qsTr("The time is set from the internet, and is right.")
                                                 : qsTr("The time isn't set from the internet yet: check the network. Until then the computer's own clock keeps it."))
                color: root.station.timeSynced ? "#aab3c2" : "#e8c46a"
            }
            Flow {
                Layout.fillWidth: true
                spacing: root.u * 0.6
                Repeater {
                    model: [["America/New_York", qsTr("Eastern")], ["America/Chicago", qsTr("Central")],
                            ["America/Denver", qsTr("Mountain")], ["America/Phoenix", qsTr("Arizona")],
                            ["America/Los_Angeles", qsTr("Pacific")], ["America/Anchorage", qsTr("Alaska")],
                            ["Pacific/Honolulu", qsTr("Hawaii")], ["America/Puerto_Rico", qsTr("Puerto Rico")]]
                    delegate: BigButton {
                        required property var modelData
                        text: modelData[1]
                        tint: root.station.timeZone === modelData[0] ? Ui.accent : Ui.button
                        onClicked: root.station.setTimeZone(modelData[0])
                    }
                }
                BigButton { text: qsTr("Other…"); onClicked: tz.all = !tz.all }
            }
            ListView {
                visible: tz.all
                Layout.fillWidth: true
                Layout.preferredHeight: root.u * 24
                clip: true
                model: tz.all ? root.station.timeZones() : []
                delegate: ItemDelegate {
                    required property string modelData
                    width: ListView.view.width
                    height: root.u * 2.8
                    text: modelData.replace(/_/g, " ")
                    font.pixelSize: root.u * 1.1
                    highlighted: modelData === root.station.timeZone
                    onClicked: root.station.setTimeZone(modelData)
                }
            }
        }
    }

    // --- updates ---------------------------------------------------------------------

    Component {
        id: updatesPanel
        Panel {
            title: qsTr("Updates")
            Note { text: qsTr("A new ViewTouch comes from the internet: Check for a New ViewTouch, then install it (a few minutes; it's checked to be ViewTouch's own, and never an older one). The tablets' app comes with it.") }
            RowLayout {
                spacing: root.u
                BigButton {
                    objectName: "checkOnline"
                    visible: root.onlineReady === ""
                    text: qsTr("Check for a New ViewTouch")
                    tint: Ui.accent
                    enabled: !root.station.busy
                    onClicked: root.station.checkOnline()
                }
                BigButton {
                    objectName: "installOnline"
                    visible: root.onlineReady !== ""
                    text: qsTr("Install ViewTouch %1").arg(root.onlineReady)
                    tint: Ui.go
                    enabled: !root.station.busy
                    onClicked: root.station.installOnline()
                }
            }
            Note { text: qsTr("Security updates for the computer install by themselves each night; Update Now gets them all at once.") }
            RowLayout {
                spacing: root.u
                BigButton { text: qsTr("Update Now"); enabled: !root.station.busy; onClicked: root.station.runUpdates() }
                BigButton { text: qsTr("New ViewTouch from a USB Stick"); enabled: !root.station.busy; onClicked: root.station.updateFromUsb() }
            }
            Note { text: qsTr("For a USB stick: put the ViewTouch package (vtmodern_….deb) on it, plug it in, then touch the button.") }
            Log {}
        }
    }

    component Log: Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: root.u * 18
        visible: root.station.output !== "" || root.station.busy
        color: "#0b0d10"
        radius: root.u * 0.4
        Flickable {
            id: logView
            anchors { fill: parent; margins: root.u * 0.6 }
            contentHeight: logText.implicitHeight
            clip: true
            onContentHeightChanged: contentY = Math.max(0, contentHeight - height)
            Label {
                id: logText
                width: logView.width
                text: root.station.output + (root.station.busy ? "\n…" : "")
                font.family: "monospace"
                font.pixelSize: root.u * 0.8
                color: "#c4cbd6"
                wrapMode: Text.WrapAnywhere
            }
        }
    }

    // --- remote help -----------------------------------------------------------------

    Component {
        id: helpPanel
        Panel {
            title: qsTr("Remote Help")
            Note {
                text: root.station.remoteSupport
                      ? qsTr("On: a technician can connect to this computer at %1 with its administrator password. Turn it off when they're done.").arg(root.station.network.address)
                      : qsTr("Off: nobody can connect to this computer over the network. Turn it on only while a technician helps you.")
            }
            BigButton {
                text: root.station.remoteSupport ? qsTr("Turn Off") : qsTr("Turn On")
                tint: root.station.remoteSupport ? "#8a3a3a" : Ui.accent
                enabled: !root.station.busy
                onClicked: root.station.setRemoteSupport(!root.station.remoteSupport)
            }
        }
    }

    // --- the maintainer's code ----------------------------------------------------------

    Component {
        id: maintainerPanel
        Panel {
            id: mp
            title: qsTr("Maintainer Code")
            property string code: ""
            property string again: ""
            property bool confirming: false
            Note {
                text: root.station.role === "store"
                      ? qsTr("A new code for this computer's settings and for the store's maintainer (erasing the store, updating screens, pairing a standby...). 4 to 8 digits; 6 or more is much harder to guess.")
                      : qsTr("A new code for this computer's settings. 4 to 8 digits; 6 or more is much harder to guess. (The store's own is set on its main register.)")
            }
            RowLayout {
                spacing: root.u * 3
                PinKeys {
                    Layout.alignment: Qt.AlignTop
                    onDigit: (d) => {
                        if (!mp.confirming && mp.code.length < 8) mp.code += d
                        else if (mp.confirming && mp.again.length < 8) mp.again += d
                    }
                    onClear: { if (mp.confirming) mp.again = ""; else mp.code = "" }
                }
                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    spacing: root.u
                    Label {
                        text: mp.confirming ? qsTr("Type the same code again") : qsTr("Type the new code")
                        color: "white"
                        font.pixelSize: root.u * 1.3
                        font.bold: true
                    }
                    Label { text: "●".repeat(mp.confirming ? mp.again.length : mp.code.length) || "–"; color: "white"; font.pixelSize: root.u * 1.8 }
                    Note {
                        visible: mp.confirming && mp.again.length >= mp.code.length && mp.again !== mp.code
                        text: qsTr("The two don't match. Touch Clear and type it again.")
                        color: "#eb8a7a"
                    }
                    BigButton {
                        visible: !mp.confirming
                        enabled: mp.code.length >= 4
                        text: qsTr("Next")
                        tint: Ui.accent
                        onClicked: mp.confirming = true
                    }
                    BigButton {
                        objectName: "maintainerSave"
                        visible: mp.confirming
                        enabled: mp.code === mp.again
                        text: qsTr("Save")
                        tint: "#22784a"
                        onClicked: {
                            root.station.setMaintainerCode(mp.code)
                            mp.code = ""
                            mp.again = ""
                            mp.confirming = false
                        }
                    }
                }
            }
        }
    }

    // --- reset ---------------------------------------------------------------------------

    Component {
        id: resetPanel
        Panel {
            title: qsTr("Reset")
            Section { text: qsTr("Start over") }
            Note { text: qsTr("Ask again what this computer is (main register, register, kitchen...). A register forgets the store it joined; the main register's sales, menu and staff are kept.") }
            BigButton {
                text: qsTr("Start Over")
                tint: "#8a6a2a"
                onClicked: confirm.ask(qsTr("Start over and choose what this computer is?"), qsTr("Start Over"), () => root.station.startOver())
            }
            Section { visible: root.station.role === "store"; text: qsTr("Erase the store") }
            Note {
                visible: root.station.role === "store"
                text: qsTr("Erases every sale, the menu, the staff and the settings, and starts with the starter ones, like a new store. A backup is made first.")
            }
            BigButton {
                visible: root.station.role === "store"
                text: qsTr("Erase the Store…")
                tint: "#8a3a3a"
                onClicked: confirm.ask(qsTr("Erase every sale, the menu, the staff and the settings?"), qsTr("Erase"),
                                       () => confirm.ask(qsTr("Really erase the store? This can't be undone here (only from the backup)."),
                                                         qsTr("Erase Everything"), () => root.station.eraseStore()))
            }
            Log {}
        }
    }

    // --- about ---------------------------------------------------------------------------

    Component {
        id: aboutPanel
        Panel {
            title: qsTr("About")
            GridLayout {
                columns: 2
                columnSpacing: root.u * 2
                rowSpacing: root.u * 0.6
                Repeater {
                    model: [[qsTr("ViewTouch"), root.station.version],
                            [qsTr("This computer"), root.station.computerName],
                            [qsTr("Used as"), root.roleNames[root.station.role] ?? "—"],
                            [qsTr("Address"), root.station.network.address || "—"],
                            [qsTr("Time zone"), root.station.timeZone],
                            [qsTr("Hardware"), root.station.model]]
                    delegate: RowLayout {
                        required property var modelData
                        Layout.columnSpan: 2
                        Label { text: modelData[0]; color: "#9aa3b2"; font.pixelSize: root.u * 1.1; Layout.preferredWidth: root.u * 12 }
                        Label { text: modelData[1]; color: "white"; font.pixelSize: root.u * 1.1 }
                    }
                }
            }
        }
    }

    // --- the first-time setup ----------------------------------------------------------

    // The first-time setup's answers so far: kept here, not in the setup
    // screen, so leaving it for Wi-Fi and coming back keeps them (the screen
    // starts from them and writes each change back).
    QtObject {
        id: setupState
        property int step: 0
        property string role: ""
        property string name: ""
        property bool zoneChosen: false
        property bool guestDisplay: false
        function reset() { step = 0; role = ""; name = ""; zoneChosen = false; guestDisplay = false }
    }

    Component {
        id: setupPanel
        ColumnLayout {
            id: wiz
            objectName: "setupWizard"
            spacing: root.u
            property int step: setupState.step
            onStepChanged: setupState.step = step
            property string role: setupState.role
            onRoleChanged: setupState.role = role
            property string pin: ""
            property string pinAgain: ""
            property bool confirming: false
            property bool zoneChosen: setupState.zoneChosen
            onZoneChosenChanged: setupState.zoneChosen = zoneChosen
            property bool guestDisplay: setupState.guestDisplay
            onGuestDisplayChanged: setupState.guestDisplay = guestDisplay
            readonly property var defaultNames: ({ store: "Main-Register", terminal: "Register-2", kitchen: "Kitchen",
                                                   selforder: "Kiosk", timeclock: "Time-Clock" })
            readonly property var steps: [qsTr("Welcome"), qsTr("This computer"), qsTr("Its name"), qsTr("Network"),
                                          qsTr("Time zone"), qsTr("Maintainer code")]

            // Where we are (not while typing: room for the keyboard).
            RowLayout {
                visible: !keyboard.visible
                spacing: root.u * 0.5
                Repeater {
                    model: wiz.steps
                    delegate: Rectangle {
                        required property int index
                        required property string modelData
                        implicitWidth: stepLabel.implicitWidth + root.u * 1.6
                        implicitHeight: root.u * 2.2
                        radius: height / 2
                        color: index === wiz.step ? Ui.accent : index < wiz.step ? "#24426e" : "#232a34"
                        Label { id: stepLabel; anchors.centerIn: parent; text: modelData; color: "white"; font.pixelSize: root.u * 0.85 }
                    }
                }
            }

            // 0: welcome
            ColumnLayout {
                visible: wiz.step === 0
                spacing: root.u
                Heading { text: qsTr("Welcome to ViewTouch") }
                Note { text: qsTr("A few questions to set up this computer: what it is in the store, its name, the network, the time zone and the maintainer's code. Then ViewTouch starts.") }
                BigButton { text: qsTr("Start"); tint: "#22784a"; onClicked: wiz.step = 1 }
            }

            // 1: what it is
            ColumnLayout {
                visible: wiz.step === 1
                spacing: root.u
                Heading { text: qsTr("What is this computer?") }
                Flow {
                    Layout.fillWidth: true
                    spacing: root.u
                    Repeater {
                        model: [["store", "store", qsTr("Keeps the store's sales, menu and staff. Set up this one first.")],
                                ["terminal", "register", qsTr("Another register that joins the main one.")],
                                ["kitchen", "kitchen", qsTr("Shows the orders to make; touch one when it's ready.")],
                                ["selforder", "kiosk", qsTr("Guests order on their own and pay at the counter.")],
                                ["timeclock", "timeclock", qsTr("Staff clock in and out and see their schedules.")]]
                        delegate: Tile {
                            required property var modelData
                            implicitWidth: root.u * 14
                            implicitHeight: root.u * 11
                            icon: modelData[1]
                            label: root.roleNames[modelData[0]]
                            detail: ""
                            tint: wiz.role === modelData[0] ? Ui.accent : "#2a313c"
                            onTapped: {
                                wiz.role = modelData[0]
                                nameField.text = wiz.defaultNames[modelData[0]]
                                wiz.step = 2
                            }
                            Label {
                                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: root.u * 0.6 }
                                text: modelData[2]
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: root.u * 0.8
                                color: "#c4cbd6"
                            }
                        }
                    }
                }
            }

            // 2: its name
            ColumnLayout {
                visible: wiz.step === 2
                spacing: root.u
                Heading { text: qsTr("What should this computer be called?") }
                Note { text: qsTr("Letters, numbers and dashes, like Register-2 or Bar. Other screens show it by this name.") }
                Field {
                    id: nameField
                    Layout.maximumWidth: root.u * 30
                    text: setupState.name
                    onTextChanged: setupState.name = text
                }
                CheckBox {
                    visible: wiz.role === "store" || wiz.role === "terminal"
                    text: qsTr("A second screen faces the guests (their order and the total)")
                    font.pixelSize: root.u * 1.1
                    checked: wiz.guestDisplay
                    onToggled: wiz.guestDisplay = checked
                }
                RowLayout {
                    spacing: root.u
                    BigButton { text: "‹ " + qsTr("Back"); onClicked: wiz.step = 1 }
                    BigButton {
                        text: qsTr("Next")
                        tint: Ui.accent
                        enabled: /^[A-Za-z0-9][A-Za-z0-9-]{0,62}$/.test(nameField.text.trim())
                        onClicked: { keyboard.visible = false; wiz.step = 3; root.station.refreshNetwork() }
                    }
                }
            }

            // 3: network
            ColumnLayout {
                visible: wiz.step === 3
                spacing: root.u
                Heading { text: qsTr("Network") }
                Note {
                    text: root.station.network.online
                          ? qsTr("Connected (%1). ").arg(root.station.network.address)
                            + (wiz.role === "store" ? qsTr("Other screens find this one on this network.")
                                                    : qsTr("This screen finds the main register on this network."))
                          : qsTr("Not connected yet. Plug in a network cable (best), or choose Wi-Fi.")
                }
                Note {
                    text: qsTr("Use the store's own network, never the Wi-Fi guests use.")
                    color: "#e8c46a"
                }
                RowLayout {
                    spacing: root.u
                    BigButton { text: "‹ " + qsTr("Back"); onClicked: wiz.step = 2 }
                    BigButton { text: qsTr("Wi-Fi…"); onClicked: root.panel = "network" }
                    BigButton { text: qsTr("Next"); tint: Ui.accent; onClicked: wiz.step = 4 }
                }
            }

            // 4: time zone
            ColumnLayout {
                visible: wiz.step === 4
                spacing: root.u
                Heading { text: qsTr("Where is the store? Touch its time zone.") }
                Note {
                    text: qsTr("Now: %1, where it's %2.").arg(root.zoneName(root.station.timeZone)).arg(zoneClock.text)
                    Label { id: zoneClock; visible: false; text: Qt.locale().toString(new Date(), "h:mm AP")
                            Timer { interval: 5000; running: true; repeat: true; onTriggered: zoneClock.text = Qt.locale().toString(new Date(), "h:mm AP") }
                            Connections { target: root.station; function onTimeZoneChanged() { zoneClock.text = Qt.locale().toString(new Date(), "h:mm AP") } } }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: root.u * 0.6
                    Repeater {
                        model: [["America/New_York", qsTr("Eastern")], ["America/Chicago", qsTr("Central")],
                                ["America/Denver", qsTr("Mountain")], ["America/Phoenix", qsTr("Arizona")],
                                ["America/Los_Angeles", qsTr("Pacific")], ["America/Anchorage", qsTr("Alaska")],
                                ["Pacific/Honolulu", qsTr("Hawaii")], ["America/Puerto_Rico", qsTr("Puerto Rico")]]
                        delegate: BigButton {
                            required property var modelData
                            text: modelData[1]
                            tint: wiz.zoneChosen && root.station.timeZone === modelData[0] ? Ui.accent : Ui.button
                            onClicked: { wiz.zoneChosen = true; root.station.setTimeZone(modelData[0]) }
                        }
                    }
                    BigButton { text: qsTr("Other…"); onClicked: { wiz.zoneChosen = true; root.panel = "time" } }
                }
                RowLayout {
                    spacing: root.u
                    BigButton { text: "‹ " + qsTr("Back"); onClicked: wiz.step = 3 }
                    BigButton { text: qsTr("Next"); tint: Ui.accent; enabled: wiz.zoneChosen; onClicked: wiz.step = 5 }
                }
            }

            // 5: the PIN, then done
            ColumnLayout {
                visible: wiz.step === 5
                spacing: root.u
                Heading { text: qsTr("The maintainer's code") }
                Note {
                    text: wiz.role === "store"
                          ? qsTr("4 to 8 digits (6 or more is much harder to guess). It's asked before this computer's network, updates and reset, and it's the store's maintainer code too: the store's technician logs in with it to erase the store, update screens, pair a standby and the like, which managers and the owner can't. Keep it between the owner and the technician; it isn't anyone's staff PIN.")
                          : qsTr("4 to 8 digits (6 or more is much harder to guess). It's asked before this computer's network, updates and reset. (The store's own maintainer code is set on its main register.)")
                }
                // The keys on the left, what's typed and the buttons beside them
                // (all on screen at once).
                RowLayout {
                    spacing: root.u * 3
                    PinKeys {
                        Layout.alignment: Qt.AlignTop
                        onDigit: (d) => {
                            if (!wiz.confirming && wiz.pin.length < 8)
                                wiz.pin += d
                            else if (wiz.confirming && wiz.pinAgain.length < 8)
                                wiz.pinAgain += d
                        }
                        onClear: {
                            if (wiz.confirming)
                                wiz.pinAgain = ""
                            else
                                wiz.pin = ""
                        }
                    }
                    ColumnLayout {
                        Layout.alignment: Qt.AlignTop
                        spacing: root.u
                        Label {
                            text: wiz.confirming ? qsTr("Type the same code again") : qsTr("Type the code")
                            color: "white"
                            font.pixelSize: root.u * 1.3
                            font.bold: true
                        }
                        RowLayout {
                            spacing: root.u * 2
                            ColumnLayout {
                                Label { text: qsTr("Code"); color: "#9aa3b2"; font.pixelSize: root.u }
                                Label { text: "●".repeat(wiz.pin.length) || "–"; color: "white"; font.pixelSize: root.u * 1.8 }
                            }
                            ColumnLayout {
                                visible: wiz.confirming
                                Label { text: qsTr("Again"); color: "#9aa3b2"; font.pixelSize: root.u }
                                Label { text: "●".repeat(wiz.pinAgain.length) || "–"; color: "white"; font.pixelSize: root.u * 1.8 }
                            }
                        }
                        Note {
                            visible: wiz.confirming && wiz.pinAgain.length >= wiz.pin.length && wiz.pinAgain !== wiz.pin
                            text: qsTr("The two don't match. Touch Clear and type it again.")
                            color: "#eb8a7a"
                        }
                        BigButton {
                            visible: !wiz.confirming
                            enabled: wiz.pin.length >= 4
                            text: qsTr("Next")
                            tint: Ui.accent
                            onClicked: wiz.confirming = true
                        }
                        BigButton {
                            visible: wiz.confirming
                            text: root.station.busy ? qsTr("Setting up…") : qsTr("Finish")
                            tint: "#22784a"
                            enabled: !root.station.busy && wiz.pin === wiz.pinAgain
                            onClicked: root.station.setup(wiz.role, nameField.text.trim(), wiz.guestDisplay, wiz.pin)
                        }
                        BigButton {
                            text: "‹ " + qsTr("Back")
                            onClicked: {
                                if (wiz.confirming) {
                                    wiz.confirming = false
                                    wiz.pinAgain = ""
                                } else {
                                    wiz.step = 4
                                }
                            }
                        }
                    }
                }
            }
            Connections {
                target: root.station
                function onConfigChanged() { if (root.station.role !== "") root.panel = "home" }
            }
        }
    }

    // A number pad for PINs.
    component PinKeys: GridLayout {
        id: keys
        signal digit(string d)
        signal clear()
        signal enter()
        property bool withEnter: false
        columns: 3
        rowSpacing: root.u * 0.5
        columnSpacing: root.u * 0.5
        Repeater {
            model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "clear", "0", keys.withEnter ? "ok" : ""]
            delegate: BigButton {
                required property string modelData
                visible: modelData !== ""
                Layout.preferredWidth: root.u * 6
                Layout.preferredHeight: root.u * 4.2
                Layout.minimumWidth: root.u * 6
                text: modelData === "clear" ? qsTr("Clear") : modelData === "ok" ? qsTr("OK") : modelData
                tint: modelData === "ok" ? Ui.accent : Ui.button
                font.pixelSize: root.u * 1.6
                onClicked: modelData === "clear" ? keys.clear() : modelData === "ok" ? keys.enter() : keys.digit(modelData)
            }
        }
    }

    // --- the PIN, before settings ------------------------------------------------------

    Popup {
        id: pinPad
        objectName: "stationPin"
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        padding: root.u * 1.4
        property string typed: ""
        property bool wrong: false
        onOpened: { typed = ""; wrong = false }
        background: Rectangle { color: "#1c2128"; radius: root.u * 0.8; border.color: Ui.line }
        ColumnLayout {
            spacing: root.u
            Heading { text: qsTr("This computer's PIN"); horizontalAlignment: Text.AlignHCenter }
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: pinPad.wrong ? (root.station.pinWait() > 0 ? qsTr("Too many wrong PINs: wait %1 s").arg(root.station.pinWait())
                                                                  : qsTr("Not that one"))
                                   : ("●".repeat(pinPad.typed.length) || " ")
                color: pinPad.wrong ? "#eb8a7a" : "white"
                font.pixelSize: root.u * 2
            }
            PinKeys {
                withEnter: true
                Layout.alignment: Qt.AlignHCenter
                onDigit: (d) => { pinPad.wrong = false; if (pinPad.typed.length < 8) pinPad.typed += d }
                onClear: pinPad.typed = ""
                onEnter: {
                    if (root.station.checkPin(pinPad.typed)) {
                        root.unlocked = true
                        lockTimer.restart()
                        pinPad.close()
                        const action = root.afterPin
                        root.afterPin = null
                        if (action)
                            action()
                    } else {
                        pinPad.wrong = true
                        pinPad.typed = ""
                    }
                }
            }
            BigButton { text: qsTr("Cancel"); Layout.alignment: Qt.AlignHCenter; onClicked: pinPad.close() }
        }
    }

    // --- are you sure? -------------------------------------------------------------------

    Popup {
        id: confirm
        objectName: "stationConfirm"
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        padding: root.u * 1.6
        width: Math.min(root.width - root.u * 4, root.u * 40)
        property string question
        property string yes
        property var action: null
        function ask(q, y, a) {
            question = q
            yes = y
            action = a
            open()
        }
        background: Rectangle { color: "#1c2128"; radius: root.u * 0.8; border.color: Ui.line }
        ColumnLayout {
            width: parent.width
            spacing: root.u * 1.4
            Heading { text: confirm.question }
            RowLayout {
                spacing: root.u
                Layout.alignment: Qt.AlignRight
                BigButton { text: qsTr("Cancel"); onClicked: confirm.close() }
                BigButton {
                    text: confirm.yes
                    tint: "#8a3a3a"
                    onClicked: {
                        const action = confirm.action
                        confirm.close()
                        if (action)
                            Qt.callLater(action)
                    }
                }
            }
        }
    }

    // --- messages and the keyboard -----------------------------------------------------

    Rectangle {
        id: toast
        property alias text: toastText.text
        property bool good: true
        visible: false
        z: 50
        anchors { horizontalCenter: parent.horizontalCenter; bottom: keyboard.visible ? keyboard.top : parent.bottom; bottomMargin: root.u * 2 }
        width: Math.min(root.width - root.u * 4, toastText.implicitWidth + root.u * 3)
        height: toastText.implicitHeight + root.u * 1.6
        radius: root.u * 0.6
        color: good ? "#1f5a3a" : "#6a2a2a"
        Label {
            id: toastText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, root.width - root.u * 7)
            wrapMode: Text.WordWrap
            color: "white"
            font.pixelSize: root.u * 1.1
        }
        TapHandler { onTapped: toast.visible = false }
        Timer { id: toastTimer; interval: 6000; onTriggered: toast.visible = false }
    }

    TouchKeyboard {
        id: keyboard
        visible: false
        z: 40
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        onDismissed: visible = false
    }
}
