import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// The setup guide: a new store's name, logo, look, taxes, first menu items
// and staff, a step at a time. Opens for managers until it's finished;
// Manager -> Setup Guide... brings it back.
Rectangle {
    id: g
    objectName: "setupGuide"
    property LayoutController controller
    readonly property PosService pos: controller ? controller.pos : null
    readonly property var info: pos ? pos.setup : ({})
    readonly property var steps: ["welcome", "store", "logo", "look", "taxes", "menu", "staff", "done"]
    // Come back to it (Finish Later, or a manager's next login): at the
    // list of what's left, once the store has its own name.
    property int step: 0
    readonly property var ready: info.ready ?? []
    function go(row) {
        if ((row.step ?? "") !== "") {
            step = Math.max(0, steps.indexOf(row.step))
        } else if ((row.page ?? "") !== "") {
            controller.closeSetup()
            controller.showPage(row.page)
        }
    }
    function leaveFor(page) {
        controller.closeSetup()
        controller.showPage(page)
    }
    readonly property string current: steps[step]
    color: "#e6101318"

    // What's being typed, filled in from the store when the guide opens.
    property string storeName: ""
    property string receiptLines: ""
    property string logoRef: ""
    property bool receiptLogo: false
    property string foodTax: ""
    property string alcoholTax: ""
    property string zip: ""
    property string serviceMode: "restaurant"
    property string chosenLook: ""
    property var addedItems: []
    Component.onCompleted: {
        storeName = info.storeName ?? ""
        receiptLines = info.receiptHeader ?? ""
        logoRef = info.logo ?? ""
        receiptLogo = info.receiptLogo ?? false
        foodTax = String(info.foodTax ?? "")
        alcoholTax = String(info.alcoholTax ?? "")
        zip = info.zip ?? ""
        serviceMode = info.serviceMode || "restaurant"
        if (ready.some(r => r.id === "name" && r.done))
            step = steps.length - 1
    }
    // The rates found for the ZIP code: into the fields (still changeable).
    Connections {
        target: g.pos
        ignoreUnknownSignals: true
        function onAdminChanged() {
            if ((g.info.zip ?? "") !== "" && g.info.zip === g.zip.slice(0, 5)) {
                g.foodTax = String(g.info.foodTax ?? "")
                g.alcoholTax = String(g.info.alcoholTax ?? "")
            }
        }
    }

    function next() {
        // Each step keeps what it set before moving on.
        switch (current) {
        case "store": pos.setupStore(storeName, receiptLines, serviceMode); break
        case "logo": if (logoRef !== "") pos.setupLogo(logoRef, receiptLogo); break
        case "taxes": pos.setupTaxes(Number(foodTax) || 0, Number(alcoholTax) || 0); break
        case "done":
            // Finished once nothing needed is left; till then it comes back
            // (the sample manager who ran it is turned off either way).
            pos.setupFinish((info.left ?? 0) === 0)
            controller.closeSetup()
            return
        }
        step = Math.min(steps.length - 1, step + 1)
    }

    MouseArea { anchors.fill: parent }   // nothing behind it while it's open

    readonly property real zoom: Math.max(0.6, Math.min(1.6, width / 1100, height / 780))
    // A phone: the whole screen, things one under another.
    // (Sized by the window: the area it's in can be wider than the phone.)
    readonly property real shownWidth: Window.width > 0 ? Math.min(width, Window.width) : width
    readonly property real shownHeight: Window.height > 0 ? Math.min(height, Window.height) : height
    readonly property bool narrow: shownWidth < 760
    Rectangle {
        id: card
        objectName: "setupCard"
        width: g.narrow ? g.shownWidth - 12 : 1000
        height: g.narrow ? g.shownHeight - 12 : 700
        anchors.centerIn: parent
        scale: g.narrow ? 1 : g.zoom
        radius: 14
        color: "#20252e"
        border.color: Ui.line

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: g.narrow ? 14 : 28
            spacing: g.narrow ? 10 : 16

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Label {
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    text: qsTr("Set up your store")
                    font.pixelSize: g.narrow ? 24 : 30
                    font.bold: true
                    color: "white"
                }
                Repeater {
                    model: g.steps.length
                    delegate: Rectangle {
                        required property int index
                        width: 14; height: 14; radius: 7
                        color: index <= g.step ? Ui.accent : Ui.line
                    }
                }
            }
            Label {
                textFormat: Text.PlainText
                text: qsTr("Step %1 of %2").arg(g.step + 1).arg(g.steps.length)
                color: Ui.muted
                font.pixelSize: 15
            }

            // --- each step --- (scrolls when it's taller than the screen: a phone)
            Flickable {
                id: stepArea
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width - stepBar.room
                contentHeight: stepStack.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: TouchScrollBar { id: stepBar; needed: stepArea.contentHeight > stepArea.height + 24 }
            StackLayout {
                id: stepStack
                width: stepArea.width - stepBar.room
                height: Math.max(stepArea.height, implicitHeight)
                currentIndex: g.step
                onCurrentIndexChanged: stepArea.contentY = 0

                // Welcome
                ColumnLayout {
                    spacing: 14
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "white"
                        font.pixelSize: 22
                        text: qsTr("Welcome! A few steps make ViewTouch yours: your name, your logo, your colors, your taxes, your first menu items and your team.")
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Ui.textSoft
                        font.pixelSize: 17
                        text: qsTr("Skip any step with Next; everything can be changed later in Manager. Finish Later closes the guide until the next time a manager logs in.")
                    }
                    Item { Layout.fillHeight: true }
                }

                // Your store
                ColumnLayout {
                    spacing: 8
                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Your store's name"); color: "white"; font.pixelSize: 18; font.bold: true }
                    TextField {
                        objectName: "setupStoreName"
                        Layout.fillWidth: true
                        font.pixelSize: 22
                        text: g.storeName
                        onTextEdited: g.storeName = text
                    }
                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("At the top of receipts (address, phone, hours)"); color: "white"; font.pixelSize: 18; font.bold: true; Layout.topMargin: 10 }
                    TextArea {
                        objectName: "setupReceipt"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 140
                        font.pixelSize: 18
                        text: g.receiptLines
                        onTextChanged: g.receiptLines = text
                    }
                    // How it serves: a food truck's shift logs in once, straight to ordering.
                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("How do you serve?"); color: "white"; font.pixelSize: 18; font.bold: true; Layout.topMargin: 10 }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 10
                        Repeater {
                            model: [["restaurant", qsTr("Restaurant"), qsTr("Tables, staff log in")],
                                    ["foodTruck", qsTr("Food truck"), qsTr("A PIN each shift, then ordering and paying")],
                                    ["retail", qsTr("Retail"), qsTr("Staff log in, the items first")]]
                            delegate: PosButton {
                                required property var modelData
                                objectName: "setupServe-" + modelData[0]
                                implicitWidth: 340
                                implicitHeight: 76
                                font.pixelSize: 17
                                text: modelData[1] + "\n" + modelData[2]
                                highlighted: g.serviceMode === modelData[0]
                                onClicked: g.serviceMode = modelData[0]
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }

                // Your logo
                ColumnLayout {
                    spacing: 12
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "white"
                        font.pixelSize: 18
                        text: qsTr("Your logo goes on the login page, the screen saver, the customer display and the kiosk, and on receipts if you like.")
                    }
                    GridLayout {
                        columns: g.narrow ? 1 : 2
                        columnSpacing: 20
                        rowSpacing: 10
                        Rectangle {
                            implicitWidth: 260
                            implicitHeight: 200
                            color: "#10141a"
                            radius: 8
                            Image {
                                objectName: "setupLogoPreview"
                                anchors.fill: parent
                                anchors.margins: 10
                                fillMode: Image.PreserveAspectFit
                                source: g.pos && g.logoRef ? (g.pos.imageRevision < 0 ? undefined : g.pos.imageUrl(g.logoRef)) : ""
                            }
                            Label {
                                textFormat: Text.PlainText
                                anchors.centerIn: parent
                                visible: g.logoRef === ""
                                text: qsTr("No logo yet")
                                color: Ui.muted
                            }
                        }
                        ColumnLayout {
                            PosButton {
                                objectName: "setupLogoAdd"
                                text: qsTr("Add Picture…")
                                font.pixelSize: 18
                                implicitHeight: 56
                                onClicked: logoFile.choose()
                            }
                            CheckBox {
                                objectName: "setupReceiptLogo"
                                text: qsTr("Print it on receipts")
                                font.pixelSize: 17
                                checked: g.receiptLogo
                                onToggled: g.receiptLogo = checked
                            }
                        }
                    }
                    PicturePicker {
                        id: logoFile
                        pos: g.pos
                        title: qsTr("Your logo")
                        onPicked: (path) => {
                            const ref = g.pos.addImageFile(path)
                            if (ref !== "")
                                g.logoRef = ref
                        }
                    }
                    Item { Layout.fillHeight: true }
                }

                // Your look
                ColumnLayout {
                    spacing: 12
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "white"
                        font.pixelSize: 18
                        text: qsTr("Pick the colors for every screen. With a logo, the first two are made from it.")
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 12
                        Repeater {
                            model: g.current === "look" && g.controller ? (g.pos.imageRevision < 0 ? undefined : g.controller.looks()) : []
                            delegate: Rectangle {
                                id: look
                                required property var modelData
                                objectName: "setupLook-" + modelData.id
                                width: 220
                                height: 110
                                radius: 10
                                color: modelData.colors[0]
                                border.color: g.chosenLook === modelData.id ? "#ffffff" : Ui.line
                                border.width: g.chosenLook === modelData.id ? 4 : 1
                                Row {
                                    x: 12; y: 12
                                    spacing: 6
                                    Repeater {
                                        model: [1, 2, 4]
                                        delegate: Rectangle {
                                            required property int modelData
                                            width: 56; height: 36; radius: 6
                                            color: look.modelData.colors[modelData]
                                            border.color: Qt.rgba(0.5, 0.5, 0.5, 0.5)
                                        }
                                    }
                                }
                                Text {
                                    textFormat: Text.PlainText
                                    x: 12
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 12
                                    text: qsTranslate("Looks", look.modelData.name)
                                    color: look.modelData.colors[3]
                                    font.pixelSize: 17
                                    font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        if (g.controller.applyLook(look.modelData.id))
                                            g.chosenLook = look.modelData.id
                                    }
                                }
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }

                // Taxes
                ColumnLayout {
                    spacing: 8
                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("The store's ZIP code"); color: "white"; font.pixelSize: 18; font.bold: true }
                    RowLayout {
                        spacing: 10
                        TextField {
                            objectName: "setupZip"
                            Layout.preferredWidth: 160
                            font.pixelSize: 22
                            inputMethodHints: Qt.ImhDigitsOnly
                            maximumLength: 10
                            text: g.zip
                            onTextEdited: g.zip = text
                        }
                        PosButton {
                            objectName: "setupFindRate"
                            text: qsTr("Find the Rate")
                            font.pixelSize: 18
                            enabled: g.zip.length >= 5
                            onClicked: g.pos.setupTaxArea(g.zip)
                        }
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        visible: text !== ""
                        wrapMode: Text.WordWrap
                        color: Ui.textSoft
                        font.pixelSize: 15
                        text: g.info.zip === g.zip.slice(0, 5) ? (g.info.taxArea ?? "") : ""
                    }
                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Sales tax on food (%)"); color: "white"; font.pixelSize: 18; font.bold: true; Layout.topMargin: 10 }
                    TextField {
                        objectName: "setupFood"
                        Layout.preferredWidth: 220
                        font.pixelSize: 22
                        inputMethodHints: Qt.ImhDecimalNumbersOnly
                        validator: DoubleValidator { bottom: 0; top: 50; decimals: 4 }
                        text: g.foodTax
                        onTextEdited: g.foodTax = text
                    }
                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; wrapMode: Text.WordWrap; text: qsTr("Sales tax on alcohol (%)"); color: "white"; font.pixelSize: 18; font.bold: true; Layout.topMargin: 10 }
                    TextField {
                        objectName: "setupAlcohol"
                        Layout.preferredWidth: 220
                        font.pixelSize: 22
                        inputMethodHints: Qt.ImhDecimalNumbersOnly
                        validator: DoubleValidator { bottom: 0; top: 50; decimals: 4 }
                        text: g.alcoholTax
                        onTextEdited: g.alcoholTax = text
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        Layout.topMargin: 10
                        wrapMode: Text.WordWrap
                        color: Ui.textSoft
                        font.pixelSize: 16
                        text: qsTr("The ZIP code fills these in from the state's published rates; change them if your accountant says so. Other tax rates and cash rounding: Manager -> Taxes.")
                    }
                    Item { Layout.fillHeight: true }
                }

                // Menu
                ColumnLayout {
                    spacing: 10
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "white"
                        font.pixelSize: 18
                        text: qsTr("The menu has %1 items. Add yours here: they show on the Everything page right away. Prices, photos and choices: Manager -> Menu.").arg(g.info.items ?? 0)
                    }
                    GridLayout {
                        columns: g.narrow ? 2 : 4
                        columnSpacing: 10
                        rowSpacing: 8
                        TextField {
                            id: itemName
                            objectName: "setupItemName"
                            Layout.fillWidth: true
                            Layout.columnSpan: g.narrow ? 2 : 1
                            font.pixelSize: 20
                            placeholderText: qsTr("Item, like Fish Tacos")
                        }
                        TextField {
                            id: itemPrice
                            objectName: "setupItemPrice"
                            Layout.preferredWidth: g.narrow ? -1 : 140
                            Layout.fillWidth: g.narrow
                            font.pixelSize: 20
                            placeholderText: qsTr("Price")
                            inputMethodHints: Qt.ImhDecimalNumbersOnly
                            validator: DoubleValidator { bottom: 0; top: 100000; decimals: 2 }
                        }
                        ComboBox {
                            id: itemFamily
                            objectName: "setupItemFamily"
                            Layout.preferredWidth: g.narrow ? -1 : 200
                            Layout.fillWidth: g.narrow
                            editable: true
                            font.pixelSize: 18
                            model: g.info.families ?? []
                        }
                        PosButton {
                            objectName: "setupItemAdd"
                            Layout.columnSpan: g.narrow ? 2 : 1
                            Layout.fillWidth: g.narrow
                            text: qsTr("Add")
                            highlighted: true
                            font.pixelSize: 18
                            implicitHeight: 52
                            enabled: itemName.text.trim() !== "" && itemPrice.text !== ""
                            onClicked: {
                                const family = itemFamily.editText !== "" ? itemFamily.editText : itemFamily.currentText
                                g.pos.setupAddItem(itemName.text, Number(itemPrice.text), family)
                                g.addedItems = g.addedItems.concat([itemName.text + "  ·  " + itemPrice.text])
                                itemName.text = ""
                                itemPrice.text = ""
                            }
                        }
                    }
                    Repeater {
                        model: g.addedItems
                        delegate: Label { textFormat: Text.PlainText; required property string modelData; text: "✓  " + modelData; color: "#7ee2a8"; font.pixelSize: 17 }
                    }
                    // A whole menu at once, instead of typing it.
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        Layout.topMargin: 10
                        wrapMode: Text.WordWrap
                        color: Ui.textSoft
                        font.pixelSize: 16
                        text: qsTr("Have your menu already? Bring it all in at once (the guide waits for you on the Manager page):")
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: g.narrow ? 1 : 2
                        columnSpacing: 10
                        rowSpacing: 8
                        PosButton {
                            objectName: "setupMenuFile"
                            Layout.fillWidth: true
                            implicitHeight: 56
                            font.pixelSize: 17
                            text: qsTr("From a File (Excel, CSV, Toast, Square, Clover)…")
                            onClicked: g.leaveFor("menu-builder")
                        }
                        PosButton {
                            objectName: "setupMenuLegacy"
                            Layout.fillWidth: true
                            implicitHeight: 56
                            font.pixelSize: 17
                            visible: g.pos && g.pos.can("owner")
                            text: qsTr("From Legacy ViewTouch (menu and sales)…")
                            onClicked: g.leaveFor("legacy-import")
                        }
                    }
                    Item { Layout.fillHeight: true }
                }

                // Staff
                ColumnLayout {
                    spacing: 10
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "white"
                        font.pixelSize: 18
                        text: qsTr("Add yourself as a manager first, then your team. Everyone logs in with their own PIN (managers: 6 to 8 digits; others 4 to 8).")
                    }
                    GridLayout {
                        columns: g.narrow ? 2 : 4
                        columnSpacing: 10
                        rowSpacing: 8
                        TextField {
                            id: staffName
                            objectName: "setupStaffName"
                            Layout.fillWidth: true
                            Layout.columnSpan: g.narrow ? 2 : 1
                            font.pixelSize: 20
                            placeholderText: qsTr("Name")
                        }
                        ComboBox {
                            id: staffRole
                            objectName: "setupStaffRole"
                            Layout.preferredWidth: g.narrow ? -1 : 180
                            Layout.fillWidth: g.narrow
                            font.pixelSize: 18
                            textRole: "text"
                            valueRole: "value"
                            model: [{ value: "manager", text: qsTr("Manager") }, { value: "server", text: qsTr("Server") },
                                    { value: "bartender", text: qsTr("Bartender") }, { value: "cashier", text: qsTr("Cashier") },
                                    { value: "host", text: qsTr("Host") }, { value: "busser", text: qsTr("Busser") }]
                        }
                        TextField {
                            id: staffPin
                            objectName: "setupStaffPin"
                            Layout.preferredWidth: g.narrow ? -1 : 140
                            Layout.fillWidth: g.narrow
                            font.pixelSize: 20
                            placeholderText: qsTr("PIN")
                            echoMode: TextInput.Password
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: RegularExpressionValidator { regularExpression: /[0-9]{0,8}/ }
                        }
                        PosButton {
                            objectName: "setupStaffAdd"
                            Layout.columnSpan: g.narrow ? 2 : 1
                            Layout.fillWidth: g.narrow
                            text: qsTr("Add")
                            highlighted: true
                            font.pixelSize: 18
                            implicitHeight: 52
                            enabled: staffName.text.trim() !== "" && staffPin.text.length >= 4
                            onClicked: {
                                g.pos.setupAddEmployee(staffName.text, staffRole.currentValue, staffPin.text)
                                staffName.text = ""
                                staffPin.text = ""
                            }
                        }
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: g.info.staff ?? []
                            delegate: Rectangle {
                                required property var modelData
                                width: who.implicitWidth + 24
                                height: 40
                                radius: 20
                                color: modelData.sample ? "#3a2f1e" : "#1f3b2c"
                                Label {
                                    textFormat: Text.PlainText
                                    id: who
                                    anchors.centerIn: parent
                                    text: modelData.name + "  ·  " + modelData.role + (modelData.sample ? "  ·  " + qsTr("sample") : "")
                                    color: "white"
                                    font.pixelSize: 15
                                }
                            }
                        }
                    }
                    PosButton {
                        objectName: "setupRetire"
                        visible: (g.info.samples ?? 0) > 0
                        text: qsTr("Turn Off the Sample Staff")
                        font.pixelSize: 17
                        implicitHeight: 52
                        onClicked: g.pos.setupRetireSamples()
                    }
                    Label {
                        textFormat: Text.PlainText
                        visible: (g.info.samples ?? 0) > 0
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "#f5b940"
                        font.pixelSize: 15
                        text: qsTr("The sample staff's PINs (1234, 1111...) are in the manual for anyone to read: turn them off once you've added yourself.")
                    }
                    Item { Layout.fillHeight: true }
                }

                // Getting ready: what's done, what's left, and the way to each.
                ColumnLayout {
                    spacing: 8
                    Label {
                        textFormat: Text.PlainText
                        objectName: "setupReadyTitle"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: "white"
                        font.pixelSize: 24
                        font.bold: true
                        text: (g.info.left ?? 0) === 0 ? qsTr("Ready to open, %1!").arg(g.info.storeName ?? "")
                                                       : qsTr("Almost there: %n thing(s) left before you open", "", g.info.left ?? 0)
                    }
                    Repeater {
                        model: g.ready
                        delegate: Rectangle {
                            id: readyRow
                            required property var modelData
                            objectName: "setupReady-" + modelData.id
                            Layout.fillWidth: true
                            implicitHeight: Math.max(60, readyText.implicitHeight + 16)
                            radius: 8
                            color: Ui.sunken
                            border.width: !modelData.done && modelData.need ? 2 : 0
                            border.color: "#f5b940"
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 8
                                spacing: 10
                                Label {
                                    textFormat: Text.PlainText
                                    text: readyRow.modelData.done ? "✓" : readyRow.modelData.need ? "!" : "○"
                                    color: readyRow.modelData.done ? "#7ee2a8" : readyRow.modelData.need ? "#f5b940" : Ui.muted
                                    font.pixelSize: 24
                                    font.bold: true
                                    Layout.preferredWidth: 26
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                ColumnLayout {
                                    id: readyText
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: readyRow.modelData.title; color: "white"; font.pixelSize: 17; font.bold: true; wrapMode: Text.WordWrap }
                                    Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: readyRow.modelData.text; color: Ui.textSoft; font.pixelSize: 14; wrapMode: Text.WordWrap }
                                }
                                PosButton {
                                    objectName: "setupReadyGo-" + readyRow.modelData.id
                                    implicitWidth: 110
                                    implicitHeight: 48
                                    font.pixelSize: 16
                                    highlighted: !readyRow.modelData.done && readyRow.modelData.need
                                    text: readyRow.modelData.done ? qsTr("Change") : qsTr("Do It")
                                    onClicked: g.go(readyRow.modelData)
                                }
                            }
                        }
                    }
                    Label {
                        textFormat: Text.PlainText
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Ui.textSoft
                        font.pixelSize: 15
                        text: qsTr("This list stays on the Manager page as Setup Guide… until everything marked ! is done.")
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                spacing: 12
                PosButton {
                    objectName: "setupLater"
                    visible: g.current !== "done"   // (the list's own button closes it)
                    text: qsTr("Finish Later")
                    font.pixelSize: 18
                    implicitHeight: 56
                    onClicked: g.controller.closeSetup()
                }
                Item { Layout.fillWidth: true }
                PosButton {
                    objectName: "setupBack"
                    visible: g.step > 0
                    text: qsTr("‹ Back")
                    font.pixelSize: 18
                    implicitHeight: 56
                    implicitWidth: g.narrow ? 96 : 160
                    onClicked: g.step = Math.max(0, g.step - 1)
                }
                PosButton {
                    objectName: "setupNext"
                    text: g.current === "done" ? ((g.info.left ?? 0) === 0 ? qsTr("Finish") : qsTr("Close")) : qsTr("Next ›")
                    highlighted: true
                    font.pixelSize: 18
                    implicitHeight: 56
                    implicitWidth: g.narrow ? 120 : 200
                    onClicked: g.next()
                }
            }
        }
    }
}
