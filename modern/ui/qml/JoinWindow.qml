import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// A terminal that is not paired yet: pick a store found on the network (or
// type its address), type the pairing code from Manager -> Terminals, name
// this terminal, and Join. Has its own keyboard for touch-only screens.
ApplicationWindow {
    id: root

    required property JoinController join
    property bool kiosk: false

    width: 1280
    height: 720
    visible: true
    title: qsTr("ViewTouch — Join a store")
    color: "#171a1f"

    readonly property real u: Math.max(10, Math.min(Math.max(width, height) / 64, Math.min(width, height) / 40))
    property TextField target: codeField   // where the keyboard types

    palette {
        window: "#171a1f"
        windowText: "#e6e9ef"
        base: "#0f1216"
        text: "#f2f4f7"
        button: "#2d3440"
        buttonText: "#f2f4f7"
        highlight: "#2f6fd6"
        highlightedText: "white"
        placeholderText: "#6b7585"
    }

    Shortcut {
        enabled: !root.kiosk
        sequences: [StandardKey.FullScreen, "F11", "Ctrl+Shift+F"]
        onActivated: root.visibility === Window.FullScreen ? root.showNormal() : root.showFullScreen()
    }

    component Caption: Label {
        color: "#8a94a6"
        font.pixelSize: root.u * 0.9
    }

    component Field: TextField {
        Layout.fillWidth: true
        Layout.preferredHeight: root.u * 3
        font.pixelSize: root.u * 1.4
        onActiveFocusChanged: if (activeFocus) root.target = this
        background: Rectangle {
            radius: 8
            color: "#0f1216"
            border.width: root.target === parent ? 2 : 1
            border.color: root.target === parent ? "#2f6fd6" : "#3a4250"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.u * 1.5
        spacing: root.u

        Label {
            text: qsTr("Join a ViewTouch store")
            font.pixelSize: root.u * 2.2
            font.bold: true
            color: "#f2f4f7"
        }
        Label {
            visible: root.join.problem !== ""
            Layout.fillWidth: true
            text: root.join.problem
            wrapMode: Text.WordWrap
            color: "#f5b940"
            font.pixelSize: root.u
        }

        // Side by side; one above the other on a portrait phone.
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: root.width >= root.height ? 2 : 1
            columnSpacing: root.u * 2
            rowSpacing: root.u

            // --- which store ---
            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                spacing: root.u * 0.6

                Caption { text: qsTr("Stores on this network") }
                ListView {
                    id: stores
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: root.u * 0.4
                    model: root.join.servers
                    delegate: Rectangle {
                        id: store
                        required property var modelData
                        width: ListView.view.width
                        height: root.u * 3.6
                        radius: 10
                        color: addressField.text === modelData.address ? "#2f6fd6" : "#232933"
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            x: root.u
                            Label {
                                text: store.modelData.name || qsTr("ViewTouch server")
                                font.pixelSize: root.u * 1.3
                                font.bold: true
                                color: "white"
                            }
                            Label {
                                text: store.modelData.machine + "  ·  " + store.modelData.address
                                font.pixelSize: root.u * 0.85
                                color: "#c8d0dc"
                            }
                        }
                        TapHandler { onTapped: addressField.text = store.modelData.address }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: stores.count === 0
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: qsTr("Looking for ViewTouch servers…\nThe server must be running with --serve on this network.")
                        color: "#8a94a6"
                        font.pixelSize: root.u
                    }
                }
                Caption { text: qsTr("…or the server's address") }
                Field {
                    id: addressField
                    text: root.join.address
                    placeholderText: qsTr("e.g. 192.168.1.10")
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhUrlCharactersOnly
                }
            }

            // --- code and name ---
            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                spacing: root.u * 0.6

                Caption { text: qsTr("Pairing code") }
                Field {
                    id: codeField
                    focus: true
                    placeholderText: "XXXXX-XXXXX"
                    font.letterSpacing: root.u * 0.2
                    font.capitalization: Font.AllUppercase
                    inputMethodHints: Qt.ImhPreferUppercase | Qt.ImhNoPredictiveText
                }
                Caption {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("A manager gets it on the store's screen: Manager → Terminals → Pair a Device.")
                }
                Caption { text: qsTr("This terminal's name") }
                Field {
                    id: nameField
                    text: root.join.suggestedName
                    placeholderText: qsTr("e.g. Patio Tablet")
                }
                Item { Layout.fillHeight: true }
                Label {
                    visible: root.join.error !== ""
                    Layout.fillWidth: true
                    text: root.join.error
                    wrapMode: Text.WordWrap
                    color: "#ff9a9e"
                    font.pixelSize: root.u
                }
                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.u * 3.4
                    text: root.join.busy ? qsTr("Joining…") : qsTr("Join")
                    enabled: !root.join.busy && codeField.text.length > 0
                    font.pixelSize: root.u * 1.4
                    font.bold: true
                    highlighted: true
                    focusPolicy: Qt.NoFocus
                    onClicked: root.join.join(addressField.text, codeField.text, nameField.text)
                }
            }
        }

        // --- keyboard (touch screens without one; Android brings its own) ---
        ColumnLayout {
            visible: Qt.platform.os !== "android"
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: root.u * 0.3
            Repeater {
                model: ["1234567890", "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM.:⌫"]
                delegate: RowLayout {
                    required property string modelData
                    Layout.fillWidth: true
                    spacing: root.u * 0.3
                    Repeater {
                        model: parent.modelData.split("")
                        delegate: Button {
                            required property string modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.u * 2.6
                            text: modelData
                            font.pixelSize: root.u * 1.2
                            focusPolicy: Qt.NoFocus
                            onClicked: {
                                const f = root.target
                                if (modelData === "⌫") {
                                    if (f.cursorPosition > 0)
                                        f.remove(f.cursorPosition - 1, f.cursorPosition)
                                } else {
                                    // Names read better in lower case after the first letter.
                                    const lower = f === nameField && f.text.length > 0 && f.text.slice(-1) !== " "
                                    f.insert(f.cursorPosition, lower ? modelData.toLowerCase() : modelData)
                                }
                            }
                        }
                    }
                }
            }
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: root.u * 2.6
                text: qsTr("space")
                font.pixelSize: root.u
                focusPolicy: Qt.NoFocus
                onClicked: root.target.insert(root.target.cursorPosition, " ")
            }
        }
    }
}
