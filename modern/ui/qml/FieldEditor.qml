import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Dialogs
import QtQuick.Layouts

// One editable property, rendered by schema type. Pure view: the owner feeds
// value/mixed/isSet/resolved and handles commit()/reset().
ColumnLayout {
    id: fe

    required property var field
    property EditorController editor
    // For pictures: the store's picture library.
    property PosService pos
    property var pages: []   // page choices when there's no editor: [{value, text}]
    property var value
    property bool mixed: false
    property bool isSet: value !== undefined
    property var resolved
    property bool readOnly: false
    // Settings forms: edited but not saved yet; and, in search results, the
    // section the setting is in.
    property bool changed: false
    property string section: ""

    signal commit(var newValue)
    signal reset()

    readonly property string type: field.type
    readonly property bool inheritable: field.inheritable === true
    readonly property real scaleBy: field.scale ?? 1
    // What to show when nothing is set here: the inherited value.
    readonly property var shown: mixed ? undefined : (isSet ? value : (resolved ?? field.default))

    spacing: 2
    Layout.fillWidth: true

    Label {
        visible: fe.section !== ""
        text: fe.section
        color: EditorStyle.accent
        font.pixelSize: 11
        font.bold: true
    }
    RowLayout {
        Layout.fillWidth: true
        Label {
            text: fe.field.label
            color: EditorStyle.muted
            font.pixelSize: 12
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Label {
            visible: fe.changed
            text: qsTr("changed")
            color: EditorStyle.warning
            font.pixelSize: 11
            font.bold: true
        }
        Label {
            visible: fe.mixed
            text: qsTr("mixed")
            color: EditorStyle.warning
            font.pixelSize: 11
        }
        Label {
            visible: fe.inheritable && !fe.isSet && !fe.mixed
            text: qsTr("inherited")
            color: EditorStyle.muted
            font.pixelSize: 11
            font.italic: true
        }
        ToolButton {
            visible: fe.inheritable && fe.isSet
            text: "↺"
            implicitHeight: 20
            implicitWidth: 24
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Reset to inherited")
            onClicked: fe.reset()
        }
    }

    Loader {
        Layout.fillWidth: true
        sourceComponent: {
            switch (fe.type) {
            case "text": return textArea
            case "int": return spin
            case "bool": return check
            case "enum": return combo
            case "color": return colorField
            case "texture": return textureCombo
            case "font": return fontCombo
            case "page": return pageCombo
            case "pageList": return pageList
            case "money": return numberField
            case "percent": return numberField
            case "number": return numberField
            case "pin": return pinField
            case "password": return passwordField
            case "image": return imagePicker
            default: return textField
            }
        }
    }

    Label {
        visible: !!fe.field.hint
        text: fe.field.hint ?? ""
        color: EditorStyle.muted
        font.pixelSize: 11
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    // A picture: one of the store's, the store logo, or a file from this computer.
    Component {
        id: imagePicker
        RowLayout {
            spacing: 6
            readonly property var library: fe.pos ? fe.pos.storeImages.filter(p => p.kind !== "font") : []
            readonly property string current: fe.mixed || !fe.isSet || fe.value == null ? "" : String(fe.value)
            readonly property var opts: {
                const o = [{ ref: "", name: fe.inheritable && !fe.isSet ? qsTr("(inherit)") : qsTr("(none)") },
                           { ref: "logo:", name: qsTr("The store logo") }]
                for (const p of library)
                    o.push({ ref: p.ref, name: p.name })
                if (current !== "" && !o.some(x => x.ref === current))
                    o.push({ ref: current, name: current })   // a path or resource set earlier
                return o
            }
            Rectangle {
                objectName: "imagePreview"
                implicitWidth: 56
                implicitHeight: 40
                color: "#10141a"
                border.color: EditorStyle.muted
                radius: 4
                Image {
                    anchors.fill: parent
                    anchors.margins: 2
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    source: {
                        const ref = parent.parent.current || (fe.resolved ?? "")
                        return fe.pos ? (fe.pos.imageRevision < 0 ? undefined : fe.pos.imageUrl(ref)) : ref
                    }
                }
            }
            ComboBox {
                objectName: "imageChoice"
                Layout.fillWidth: true
                enabled: !fe.readOnly
                model: parent.opts.map(o => o.name)
                currentIndex: fe.mixed ? -1 : Math.max(0, parent.opts.findIndex(o => o.ref === parent.current))
                displayText: fe.mixed ? qsTr("(mixed)") : currentText
                onActivated: index => {
                    const ref = parent.opts[index].ref
                    if (ref === "" && fe.inheritable) fe.reset()
                    else fe.commit(ref)
                }
            }
            Button {
                objectName: "imageAdd"
                text: qsTr("Add Picture…")
                enabled: !fe.readOnly && !!fe.pos
                onClicked: pictureFile.open()
            }
            FileDialog {
                id: pictureFile
                title: qsTr("A picture for the store")
                nameFilters: [qsTr("Pictures (*.png *.jpg *.jpeg *.webp *.gif *.bmp *.svg)")]
                onAccepted: {
                    const ref = fe.pos.addImageFile(selectedFile.toString())
                    if (ref !== "")
                        fe.commit(ref)
                }
            }
        }
    }

    Component {
        id: textField
        TextField {
            text: fe.mixed || !fe.isSet || fe.value == null ? "" : String(fe.value)
            placeholderText: fe.mixed ? qsTr("(mixed)") : (fe.resolved !== undefined ? String(fe.resolved) : "")
            selectByMouse: true
            readOnly: fe.readOnly
            onEditingFinished: if (text !== (fe.isSet && fe.value != null ? String(fe.value) : "")) fe.commit(text)
        }
    }

    Component {
        id: textArea
        TextArea {
            text: fe.mixed ? "" : (fe.value ?? "")
            placeholderText: fe.mixed ? qsTr("(mixed)") : ""
            wrapMode: TextEdit.Wrap
            selectByMouse: true
            onActiveFocusChanged: if (!activeFocus && text !== (fe.value ?? "")) fe.commit(text)
        }
    }

    Component {
        id: spin
        SpinBox {
            from: (fe.field.min ?? -99999)
            to: (fe.field.max ?? 99999)
            editable: true
            value: fe.shown !== undefined ? Math.round(Number(fe.shown) * fe.scaleBy) : 0
            opacity: fe.isSet || fe.mixed ? 1 : 0.6
            onValueModified: fe.commit(fe.scaleBy === 1 ? value : value / fe.scaleBy)
        }
    }

    Component {
        id: check
        CheckBox {
            tristate: fe.mixed
            checkState: fe.mixed ? Qt.PartiallyChecked : (fe.shown === true ? Qt.Checked : Qt.Unchecked)
            text: qsTr("Yes")
            onClicked: fe.commit(checkState === Qt.Checked)
        }
    }

    Component {
        id: combo
        ComboBox {
            readonly property var opts: (fe.inheritable ? [{ value: "__inherit__", text: qsTr("(inherit)") }] : [])
                                        .concat(fe.field.options ?? [])
            model: opts
            textRole: "text"
            valueRole: "value"
            currentIndex: {
                if (fe.mixed) return -1
                if (fe.inheritable && !fe.isSet) return 0
                return Math.max(0, opts.findIndex(o => o.value === fe.value))
            }
            displayText: fe.mixed ? qsTr("(mixed)") : currentText
            onActivated: index => {
                const v = opts[index].value
                if (v === "__inherit__") fe.reset()
                else fe.commit(v)
            }
        }
    }

    Component {
        id: colorField
        RowLayout {
            spacing: 6
            Rectangle {
                implicitWidth: 32
                implicitHeight: 28
                radius: 4
                color: fe.shown ?? "transparent"
                border.color: EditorStyle.border
                opacity: fe.isSet ? 1 : 0.6
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: colorDialog.open()
                }
            }
            TextField {
                Layout.fillWidth: true
                // value and isSet update separately; never hand undefined to text.
                readonly property string current: fe.mixed || !fe.isSet || fe.value == null ? "" : String(fe.value)
                text: current
                placeholderText: fe.mixed ? qsTr("(mixed)") : (fe.resolved ?? "#rrggbb")
                selectByMouse: true
                onEditingFinished: {
                    if (text === current) return
                    if (text === "") fe.reset()
                    else fe.commit(text)
                }
            }
            // The store's colors, by touch (the same as the Menu Builder's).
            Popup {
                id: colorDialog
                objectName: "fieldColorPopup"
                parent: Overlay.overlay
                anchors.centerIn: parent
                width: Math.min(parent ? parent.width - 32 : 600, 600)
                modal: true
                padding: 16
                contentItem: ColorPicker {
                    pos: fe.pos
                    noneText: fe.inheritable ? qsTr("(inherit)") : ""
                    color: fe.isSet && !fe.mixed && fe.value ? String(fe.value) : ""
                    onPicked: c => {
                        if (c === "") fe.reset()
                        else fe.commit(c)
                        colorDialog.close()
                    }
                }
            }
        }
    }

    Component {
        id: textureCombo
        ComboBox {
            readonly property var opts: (fe.inheritable ? [qsTr("(inherit)")] : []).concat([qsTr("(none)")])
                                        .concat(fe.editor ? fe.editor.textures : [])
            model: opts
            currentIndex: {
                if (fe.mixed) return -1
                if (fe.inheritable && !fe.isSet) return 0
                if (!fe.value) return fe.inheritable ? 1 : 0
                return Math.max(0, opts.indexOf(fe.value))
            }
            displayText: fe.mixed ? qsTr("(mixed)") : currentText
            onActivated: index => {
                const first = fe.inheritable ? 1 : 0
                if (fe.inheritable && index === 0) fe.reset()
                else if (index === first) fe.commit("")
                else fe.commit(opts[index])
            }
        }
    }

    // A font: this screen's and the store's (Add Font… brings one in for every screen).
    Component {
        id: fontCombo
        RowLayout {
            spacing: 6
            ComboBox {
                objectName: "fontChoice"
                Layout.fillWidth: true
                // Again when the store's fonts change.
                readonly property var opts: [qsTr("(inherit)")].concat(fe.pos ? (fe.pos.imageRevision < 0 ? undefined : Qt.fontFamilies())
                                                                              : Qt.fontFamilies())
                model: opts
                currentIndex: fe.mixed ? -1 : (!fe.isSet ? 0 : Math.max(0, opts.indexOf(fe.value)))
                displayText: fe.mixed ? qsTr("(mixed)") : (!fe.isSet && fe.resolved ? fe.resolved + qsTr(" (inherited)") : currentText)
                onActivated: index => index === 0 ? fe.reset() : fe.commit(opts[index])
                font.family: currentIndex > 0 ? currentText : Qt.application.font.family   // what it looks like
            }
            Button {
                objectName: "fontAdd"
                visible: !!fe.pos
                text: qsTr("Add Font…")
                onClicked: fontFile.open()
            }
            FileDialog {
                id: fontFile
                title: qsTr("A font for the store")
                nameFilters: [qsTr("Fonts (*.ttf *.otf)")]
                onAccepted: fe.pos.addImageFile(selectedFile.toString())
            }
        }
    }

    Component {
        id: pageCombo
        ComboBox {
            // From the editor, or the owner's list (admin forms); field.emptyText names "none".
            readonly property var opts: {
                const list = fe.editor ? (fe.editor.revision >= 0 ? fe.editor.pageOptions() : []) : fe.pages   // compared: new pages show
                return list.map((o, i) => i === 0 && o.value === "" && fe.field.emptyText
                                ? { value: "", text: fe.field.emptyText } : o)
            }
            model: opts
            textRole: "text"
            valueRole: "value"
            currentIndex: fe.mixed ? -1 : Math.max(0, opts.findIndex(o => o.value === (fe.value ?? "")))
            displayText: fe.mixed ? qsTr("(mixed)") : currentText
            onActivated: index => fe.commit(opts[index].value)
        }
    }

    // Money (dollars.cents) and percent: decimal entry, committed as a number.
    Component {
        id: numberField
        TextField {
            id: numField
            readonly property int decimals: fe.type === "money" ? 2 : 3
            text: fe.mixed || fe.value === undefined ? "" : Number(fe.value).toFixed(decimals)
            placeholderText: fe.mixed ? qsTr("(mixed)") : "0"
            readOnly: fe.readOnly
            selectByMouse: true
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: DoubleValidator { bottom: 0; decimals: numField.decimals; notation: DoubleValidator.StandardNotation }
            onEditingFinished: {
                const v = Number(text === "" ? 0 : text)
                if (!isNaN(v) && v !== Number(fe.value)) fe.commit(v)
            }
        }
    }

    // Write-only PIN: shows dots, never the stored value.
    Component {
        id: pinField
        TextField {
            text: fe.value ?? ""
            echoMode: TextInput.Password
            placeholderText: fe.field.hint ?? ""
            inputMethodHints: Qt.ImhDigitsOnly
            validator: RegularExpressionValidator { regularExpression: /[0-9]{0,8}/ }
            onEditingFinished: if (text !== (fe.value ?? "")) fe.commit(text)
        }
    }

    Component {
        id: passwordField
        TextField {
            text: fe.value ?? ""
            echoMode: TextInput.Password
            selectByMouse: true
            onEditingFinished: if (text !== (fe.value ?? "")) fe.commit(text)
        }
    }

    Component {
        id: pageList
        TextField {
            text: Array.isArray(fe.value) ? fe.value.join(", ") : ""
            placeholderText: qsTr("page-id, page-id, ...")
            selectByMouse: true
            onEditingFinished: {
                const ids = text.split(",").map(s => s.trim()).filter(s => s.length > 0)
                if (ids.join(",") !== (Array.isArray(fe.value) ? fe.value.join(",") : ""))
                    fe.commit(ids)
            }
        }
    }
}
