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
    // Manager settings forms (fingers): numbers with big - and +, switches,
    // lists as rows; short ones on the same line as their name.
    property bool touch: false
    readonly property bool inline: touch && ["int", "bool", "money", "percent", "number"].includes(type)
    readonly property Component editorComponent: {
        switch (fe.type) {
        case "text": return textArea
        case "int": return touch ? stepper : spin
        case "bool": return touch ? onOff : check
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
        case "rows": return rowsField
        case "info": return infoText
        default: return textField
        }
    }

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
            color: fe.inline ? palette.windowText : EditorStyle.muted
            font.pixelSize: fe.inline ? 14 : 12
            Layout.fillWidth: true
            elide: fe.inline ? Text.ElideNone : Text.ElideRight
            wrapMode: fe.inline ? Text.WordWrap : Text.NoWrap
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
        Loader {
            active: fe.inline
            visible: active
            sourceComponent: fe.inline ? fe.editorComponent : null
        }
    }

    Loader {
        active: !fe.inline
        visible: active
        Layout.fillWidth: true
        sourceComponent: fe.inline ? null : fe.editorComponent
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
            PicturePicker {
                id: pictureFile
                pos: fe.pos
                title: qsTr("A picture for the store")
                onPicked: (path) => {
                    const ref = fe.pos.addImageFile(path)
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
                implicitWidth: fe.touch ? 44 : 32
                implicitHeight: fe.touch ? 40 : 28
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
                placeholderText: fe.mixed ? qsTr("(mixed)") : (fe.resolved ?? fe.field.emptyText ?? "#rrggbb")
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
                    noneText: fe.inheritable ? qsTr("(inherit)") : (fe.field.emptyText ?? "")
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
            // Money with its cents; a percent as typed (18, 4.5), not 18.000.
            text: fe.mixed || fe.value === undefined ? ""
                : fe.type === "money" ? Number(fe.value).toFixed(2) : String(parseFloat(Number(fe.value).toFixed(decimals)))
            implicitWidth: fe.inline ? 150 : 200
            horizontalAlignment: fe.inline ? Text.AlignRight : Text.AlignLeft
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
    // A whole number by touch: big - and +, the number typed in between.
    // field.zeroText names 0 ("Never"); field.step (1).
    Component {
        id: stepper
        RowLayout {
            spacing: 4
            readonly property int lo: fe.field.min ?? 0
            readonly property int hi: fe.field.max ?? 99999
            readonly property int step: fe.field.step ?? 1
            readonly property int v: fe.shown !== undefined && fe.shown !== null && fe.shown !== "" ? Math.round(Number(fe.shown)) : lo
            function set(n) {
                n = Math.max(lo, Math.min(hi, n))
                if (n !== v) fe.commit(n)
            }
            Button {
                objectName: "stepDown"
                text: "−"
                font.pixelSize: 20
                implicitWidth: 48
                implicitHeight: 44
                autoRepeat: true
                enabled: !fe.readOnly && parent.v > parent.lo
                onClicked: parent.set(parent.v % parent.step ? parent.v - parent.v % parent.step : parent.v - parent.step)
            }
            TextField {
                id: stepValue
                objectName: "stepValue"
                implicitWidth: 112
                implicitHeight: 44
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 16
                readOnly: fe.readOnly
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 0; top: 99999 }
                readonly property string shownText: parent.v === 0 && fe.field.zeroText ? fe.field.zeroText : String(parent.v)
                text: activeFocus ? String(parent.v) : shownText
                onActiveFocusChanged: if (activeFocus) { text = String(parent.v); selectAll() }
                onEditingFinished: {
                    if (text !== "") parent.set(parseInt(text))
                    text = Qt.binding(() => activeFocus ? String(parent.v) : shownText)
                }
            }
            Button {
                objectName: "stepUp"
                text: "+"
                font.pixelSize: 20
                implicitWidth: 48
                implicitHeight: 44
                autoRepeat: true
                enabled: !fe.readOnly && parent.v < parent.hi
                onClicked: parent.set(parent.v - parent.v % parent.step + parent.step)
            }
        }
    }

    // Read only: something to know (Store Settings -> About).
    Component {
        id: infoText
        Label {
            objectName: "infoText"
            text: fe.value == null ? "" : String(fe.value)
            wrapMode: Text.Wrap   // between words; a long folder path anywhere
            textFormat: Text.PlainText
            font.pixelSize: 14
        }
    }

    // On or off by touch.
    Component {
        id: onOff
        Switch {
            objectName: "onOff"
            checked: fe.shown === true
            enabled: !fe.readOnly
            text: checked ? qsTr("On") : qsTr("Off")
            implicitHeight: 44
            onToggled: fe.commit(checked)
        }
    }

    // A list kept as text, edited as rows: field.columns [{label, type: text |
    // int | number | money | enum, options, width}]; each line read with
    // field.pattern (a regex, one group a column) and written with field.format
    // ("%1 = %2"); lines split by field.split (a regex, default a new line) and
    // joined with field.join. field.max rows; field.addText on the add button.
    Component {
        id: rowsField
        ColumnLayout {
            id: rowsBox
            spacing: 4
            readonly property var cols: fe.field.columns ?? [{ label: "", type: "text" }]
            readonly property string current: fe.mixed || fe.value == null ? "" : String(fe.value)
            property var rows: []
            function parse(text) {
                const split = new RegExp(fe.field.split ?? "\\n")
                const pattern = fe.field.pattern ? new RegExp(fe.field.pattern, "i") : null
                const out = []
                for (const line of text.split(split)) {
                    if (line.trim() === "") continue
                    const m = pattern ? line.match(pattern) : null
                    out.push(cols.map((c, i) => m ? (m[i + 1] ?? "").trim() : (i === 0 ? line.trim() : "")))
                }
                return out
            }
            function write(list) {
                const format = fe.field.format ?? "%1"
                // Only whole rows: one still being filled in waits on the screen.
                return list.filter(r => r.every(x => String(x).trim() !== ""))
                           .map(r => r.reduce((t, x, i) => t.split("%" + (i + 1)).join(String(x).trim()), format))
                           .join(fe.field.join ?? "\n")
            }
            function load() { if (write(rows) !== current) rows = parse(current) }
            function save() { const t = write(rows); if (t !== current) fe.commit(t) }
            Component.onCompleted: load()
            onCurrentChanged: load()

            // Column names over the rows (more than one column).
            RowLayout {
                visible: rowsBox.cols.length > 1 && rowsBox.rows.length > 0
                Layout.fillWidth: true
                spacing: 4
                Repeater {
                    model: rowsBox.cols
                    Label {
                        required property var modelData
                        text: modelData.label ?? ""
                        color: EditorStyle.muted
                        font.pixelSize: 11
                        Layout.fillWidth: !modelData.width
                        Layout.preferredWidth: modelData.width ?? -1
                    }
                }
                Item { implicitWidth: 44 }
            }
            Repeater {
                model: rowsBox.rows.length
                RowLayout {
                    id: rowLine
                    required property int index
                    Layout.fillWidth: true
                    spacing: 4
                    Repeater {
                        model: rowsBox.cols
                        Loader {
                            id: cell
                            required property var modelData
                            required property int index
                            readonly property int row: rowLine.index
                            readonly property string cellValue: (rowsBox.rows[row] ?? [])[index] ?? ""
                            function put(v) {
                                rowsBox.rows[row][index] = String(v)
                                rowsBox.save()
                            }
                            Layout.fillWidth: !modelData.width
                            Layout.preferredWidth: modelData.width ?? -1
                            sourceComponent: modelData.type === "enum" ? cellCombo : cellText
                            Component {
                                id: cellText
                                TextField {
                                    objectName: "rowCell"
                                    implicitHeight: 44
                                    text: cell.cellValue
                                    placeholderText: cell.modelData.placeholder ?? ""
                                    readOnly: fe.readOnly
                                    horizontalAlignment: cell.modelData.type === "text" || !cell.modelData.type ? Text.AlignLeft : Text.AlignRight
                                    inputMethodHints: cell.modelData.type === "int" ? Qt.ImhDigitsOnly
                                                    : cell.modelData.type === "number" || cell.modelData.type === "money" ? Qt.ImhFormattedNumbersOnly : Qt.ImhNone
                                    onEditingFinished: if (text !== cell.cellValue) cell.put(cell.modelData.type === "money" && text !== "" ? Number(text).toFixed(2) : text)
                                }
                            }
                            Component {
                                id: cellCombo
                                ComboBox {
                                    implicitHeight: 44
                                    model: cell.modelData.options ?? []
                                    textRole: "text"
                                    valueRole: "value"
                                    enabled: !fe.readOnly
                                    currentIndex: Math.max(0, (cell.modelData.options ?? []).findIndex(o => String(o.value).toLowerCase() === cell.cellValue.toLowerCase()))
                                    onActivated: i => cell.put(cell.modelData.options[i].value)
                                }
                            }
                        }
                    }
                    Button {
                        objectName: "rowRemove"
                        text: "✕"
                        implicitWidth: 44
                        implicitHeight: 44
                        enabled: !fe.readOnly
                        onClicked: {
                            const r = rowsBox.rows.slice()
                            r.splice(rowLine.index, 1)
                            rowsBox.rows = r
                            rowsBox.save()
                        }
                    }
                }
            }
            Button {
                objectName: "rowAdd"
                visible: !fe.readOnly && (!fe.field.max || rowsBox.rows.length < fe.field.max)
                text: fe.field.addText ?? qsTr("+ Add")
                implicitHeight: 44
                onClicked: {
                    // A new row: an enum starts on its first choice (one not
                    // in another row yet, for a column marked unique).
                    rowsBox.rows = rowsBox.rows.concat([rowsBox.cols.map((c, i) => {
                        if (c.type !== "enum" || !c.options || !c.options.length) return ""
                        const free = c.unique ? c.options.find(o => !rowsBox.rows.some(r => String(r[i]).toLowerCase() === String(o.value)))
                                              : null
                        return String((free ?? c.options[0]).value)
                    })])
                }
            }
        }
    }
}
