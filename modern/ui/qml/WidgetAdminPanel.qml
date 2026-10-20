import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Manager editor for one kind of record (props.panel: menu | employees |
// tenders | printers | taxes | store | terminals | mealPeriods). List on the left, form on the right;
// the form's fields come from the service, like the page inspector.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string panel: zone && zone.props && zone.props.panel ? zone.props.panel : "menu"
    readonly property bool single: panel === "taxes" || panel === "store"

    readonly property var fields: {
        // Compared, not just read: an unused read is compiled away (no refresh).
        if (!pos || pos.adminRevision < 0 || pos.queryRevision < 0) return []
        return pos.adminFields(panel)
    }
    readonly property var records: {
        if (!pos || pos.adminRevision < 0 || pos.queryRevision < 0) return []
        return pos.adminRecords(panel)
    }

    property int index: -2       // -2 nothing chosen, -1 new record
    property var draft: ({})
    property bool dirty: false
    property bool armDelete: false

    function copy(o) { return o === undefined || o === null ? ({}) : JSON.parse(JSON.stringify(o)) }
    // A handheld asks the server for the records: the form may open before
    // they come. It is filled when they do (and what was typed meanwhile kept).
    property bool waiting: false
    function choose(i) {
        index = i
        waiting = i >= 0 && i >= records.length
        draft = copy(i >= 0 ? records[i] : pos.adminNewRecord(panel))
        dirty = false
        armDelete = false
    }

    // Changed here, not saved yet.
    readonly property var saved: index >= 0 && index < records.length ? records[index] : null
    function differs(path) {
        return saved !== null && JSON.stringify(draft[path] ?? "") !== JSON.stringify(saved[path] ?? "")
    }
    function changedIn(key) {
        if (!dirty) return 0
        return fields.filter(f => (key === undefined || f.groupKey === key) && differs(f.path)).length
    }
    readonly property int changes: changedIn()

    // Store Settings: in sections (the fields' groups), with a search across
    // them all. A field with "when" shows while that other setting is on.
    readonly property var sections: {
        const out = []
        for (const f of fields)
            if (f.groupKey && !out.find(x => x.key === f.groupKey)) out.push({ key: f.groupKey, name: f.group })
        return out
    }
    readonly property bool sectioned: single && sections.length > 1
    property string section: ""
    readonly property string sectionShown: sections.find(x => x.key === section) ? section
                                         : (sections.length ? sections[0].key : "")
    property bool sectionOpen: false   // upright: a section's settings, not the list of them
    property string query: ""
    function plain(t) { return String(t ?? "").toLowerCase().normalize("NFD").replace(/[\u0300-\u036f]/g, "") }
    function on(v) {
        if (typeof v === "string") return v !== "" && Number(v) !== 0
        return !!v
    }
    readonly property var words: plain(query).split(/\s+/).filter(x => x !== "")
    // Which "when" settings are on: the form is rebuilt only when one turns
    // on or off, not on every edit (that would drop the field being edited).
    readonly property var whenPaths: [...new Set(fields.filter(f => f.when).map(f => f.when))]
    readonly property string whenOn: whenPaths.map(p => on(draft[p]) ? "1" : "0").join("")
    function shows(f) { return !f.when || whenOn[whenPaths.indexOf(f.when)] === "1" }
    readonly property var shownFields: {
        if (!sectioned) return fields.filter(f => shows(f))
        if (words.length) {
            return fields.filter(f => {
                const hay = plain(f.label + " " + (f.hint ?? "") + " " + f.group)
                return words.every(x => hay.includes(x))
            })
        }
        return fields.filter(f => f.groupKey === sectionShown && shows(f))
    }
    // A page field's choices: field.pages "floor" lists only the floor plans
    // (start pages); a page chosen before stays listed while it's the choice.
    function pageChoices(f) {
        const list = zone.controller.pageChoices(f.pages ?? "")
        const now = draft[f.path] ?? ""
        if (now !== "" && !list.some(o => o.value === now)) {
            const page = zone.controller.pageChoices().find(o => o.value === now)
            list.push({ value: now, text: page ? page.text : now })
        }
        return list
    }
    function openSection(key) {
        section = key
        sectionOpen = true
        if (zone && zone.controller) zone.controller.setWidgetState("adminSection:" + panel, key)
    }
    // Saves may be answered by a server: react when the data changes.
    property string pending: ""   // "save-new" | "save" | "delete" while waiting
    property int countAtSave: 0
    function save() {
        pending = index === -1 ? "save-new" : "save"
        countAtSave = records.length
        pos.adminSave(panel, index, draft)
    }
    // The records changed (a server's answer comes after adminChanged, with
    // the saved values): a form with nothing typed in it shows them.
    onRecordsChanged: {
        if (pending === "save-new") {
            if (records.length > countAtSave) {
                pending = ""
                choose(records.length - 1)
            }
            return
        }
        if (index < 0 || index >= records.length)
            return
        if (waiting) {
            const typed = dirty ? draft : ({})
            const fresh = copy(records[index])
            for (const k in typed) fresh[k] = typed[k]
            draft = fresh
            waiting = false
        } else if (!dirty && pending === "") {
            draft = copy(records[index])
        }
    }
    function remove() {
        pending = "delete"
        pos.adminDelete(panel, index)
    }
    Connections {
        target: w.pos
        function onAdminChanged() {
            const what = w.pending
            w.pending = ""
            if (what === "save-new") {
                // Here already (this computer), or when the server's list comes.
                if (w.records.length > w.countAtSave) w.choose(w.records.length - 1)
                else w.pending = what
            } else if (what === "save") w.choose(w.index)
            else if (what === "delete") w.panel === "employees" ? w.choose(w.index) : (w.index = -2)
        }
        // A refused save only shows a notice; stop waiting.
        function onNotice() { if (w.pending !== "") Qt.callLater(() => w.pending = "") }
    }

    // This widget may be kept for the next manager page (another panel):
    // what's open is worked out from the panel itself, not from `single`,
    // which may not have caught up yet.
    property string openPanel: ""
    function keepUnsaved() {
        if (openPanel !== "" && zone && zone.controller)
            zone.controller.keep("admin:" + openPanel, dirty && index >= -1 ? { index: index, draft: draft } : undefined)
    }
    function open() {
        keepUnsaved()
        openPanel = panel
        query = ""
        sectionOpen = false
        dirty = false
        const one = panel === "taxes" || panel === "store"
        if (one) choose(0)
        else index = -2
        if (zone && zone.controller) {
            section = zone.controller.widgetState("adminSection:" + panel) ?? ""
            // Changes left unsaved when this page was left: back as they were.
            const k = zone.controller.take("admin:" + panel)
            if (k && k.index < records.length) {
                index = k.index
                draft = k.draft
                dirty = true
            }
        }
    }
    Component.onCompleted: open()
    Component.onDestruction: keepUnsaved()
    onPanelChanged: Qt.callLater(open)   // once the bindings have caught up

    // Controls are laid out at a comfortable size, then scaled with the page.
    // Upright (a phone): the list, then a record's form on its own, with ‹ List.
    readonly property bool narrow: zone ? zone.narrow : false
    // About: this screen, its name, size and pages.
    readonly property string screenText: !Window.window ? "" : qsTr("%1: %2 × %3, %4 pages")
        .arg(pos && pos.terminalName ? pos.terminalName : qsTr("This screen"))
        .arg(Window.window.width).arg(Window.window.height)
        .arg(zone && zone.controller && zone.controller.formFactor === "phone" ? qsTr("phone") : qsTr("standard"))
    readonly property real zoom: zone ? zone.formZoom(narrow ? 340 : 680) : 1.6
    Item {
        width: w.width / w.zoom
        height: w.height / w.zoom
        scale: w.zoom
        transformOrigin: Item.TopLeft

        // Terminals: pair a tablet or another screen. The code shows here
        // until the device uses it or 10 minutes pass.
        Rectangle {
            id: pairBar
            visible: w.panel === "terminals"
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
            height: visible ? 64 : 0
            radius: 8
            color: w.pos && w.pos.pairing.active ? "#173a26" : "#232933"
            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 12
                // Not on a phone: beside the two buttons it wrapped a word a line
                // and ran over the list (the buttons say what they do).
                Label {
                    visible: !(w.pos && w.pos.pairing.active) && !w.narrow
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    text: qsTr("Add a tablet or another screen: Pair a Device, then type the code on it (its Join screen).")
                    opacity: 0.8
                }
                Label {
                    visible: w.pos && w.pos.pairing.active
                    text: qsTr("Pairing code")
                    opacity: 0.8
                }
                Label {
                    visible: w.pos && w.pos.pairing.active
                    text: w.pos ? (w.pos.pairing.code ?? "") : ""
                    font.pixelSize: 30
                    font.bold: true
                    font.letterSpacing: 4
                    color: "#7ee2a8"
                }
                Label {
                    visible: w.pos && w.pos.pairing.active
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: (w.pos && w.pos.pairing.standby ? qsTr("For the standby computer. ") : "")
                          + qsTr("works once, until %1").arg(w.pos ? (w.pos.pairing.until ?? "") : "")
                    opacity: 0.8
                }
                PosButton {
                    Layout.minimumHeight: w.narrow ? 36 : 0
                    text: w.pos && w.pos.pairing.active ? qsTr("Stop") : qsTr("Pair a Device")
                    highlighted: !(w.pos && w.pos.pairing.active)
                    onClicked: w.pos.pairing.active ? w.pos.stopPairing() : w.pos.startPairing(false)
                }
                // A standby gets a copy of everything: its own kind of code.
                PosButton {
                    visible: !(w.pos && w.pos.pairing.active)
                    Layout.minimumHeight: w.narrow ? 36 : 0
                    text: qsTr("Pair a Standby Computer")
                    onClicked: w.pos.startPairing(true)
                }
            }
        }

        RowLayout {
            id: searchBar
            visible: w.sectioned
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
            height: visible ? 44 : 0
            spacing: 8
            TextField {
                id: search
                objectName: "settingsSearch"
                Layout.fillWidth: true
                Layout.fillHeight: true
                font.pixelSize: 16
                placeholderText: qsTr("Find a setting: tips, receipt, kiosk color…")
                text: w.query
                onTextEdited: w.query = text
            }
            PosButton {
                objectName: "settingsSearchClear"
                visible: w.query !== ""
                Layout.fillHeight: true
                text: qsTr("Clear")
                onClicked: { w.query = ""; search.focus = false }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            anchors.topMargin: pairBar.visible ? pairBar.height + 20 : searchBar.visible ? searchBar.height + 18 : 10
            spacing: 12

            // --- list ---
            ColumnLayout {
                visible: w.sectioned ? !w.narrow || (!w.sectionOpen && w.query === "")
                                     : !w.single && (!w.narrow || w.index === -2)
                Layout.fillWidth: w.narrow    // nested layouts fill by default
                Layout.preferredWidth: w.narrow ? parent.width : parent.width * (w.sectioned ? 0.3 : 0.36)
                Layout.fillHeight: true
                spacing: 6

                ListView {
                    id: list
                    objectName: "adminList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: w.sectioned ? w.sections : w.records
                    currentIndex: w.sectioned ? -1 : w.index
                    ScrollBar.vertical: TouchScrollBar { id: listBar }
                    // An empty list: say so, rather than a blank box.
                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 24
                        visible: list.count === 0
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: w.single ? "" : qsTr("None yet. + Add makes one.")
                        opacity: 0.6
                    }
                    delegate: ItemDelegate {
                        id: row
                        required property var modelData
                        required property int index
                        objectName: w.sectioned ? "settingsSection-" + row.modelData.key : ""
                        width: ListView.view.width - listBar.room
                        // A phone: each row a finger's height.
                        implicitHeight: Math.max(implicitContentHeight + topPadding + bottomPadding, w.narrow ? 36 : 0)
                        highlighted: w.sectioned ? w.query === "" && row.modelData.key === w.sectionShown : row.index === w.index
                        onClicked: {
                            if (!w.sectioned) return w.choose(row.index)
                            w.query = ""
                            w.openSection(row.modelData.key)
                        }
                        contentItem: ColumnLayout {
                            spacing: 0
                            Label {
                                text: (w.sectioned ? row.modelData.name : row.modelData._title) || qsTr("(unnamed)")
                                font.bold: true
                                // A section's name wraps (a narrow list, a longer language).
                                wrapMode: w.sectioned ? Text.WordWrap : Text.NoWrap
                                elide: w.sectioned ? Text.ElideNone : Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                visible: text !== ""
                                // A section: how many of its settings are changed.
                                text: !w.sectioned ? row.modelData._detail ?? ""
                                    : w.changedIn(row.modelData.key) > 0 ? qsTr("%n changed", "", w.changedIn(row.modelData.key)) : ""
                                color: w.sectioned ? EditorStyle.warning : palette.windowText
                                opacity: w.sectioned ? 1 : 0.7
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
                PosButton {
                    Layout.minimumHeight: w.narrow ? 36 : 0
                    visible: !w.sectioned
                    Layout.fillWidth: true
                    text: qsTr("+ Add")
                    onClicked: w.choose(-1)
                }
            }

            Rectangle {
                visible: (!w.single || w.sectioned) && !w.narrow
                Layout.fillHeight: true
                implicitWidth: 1
                color: "#3a4250"
            }

            // --- form ---
            ColumnLayout {
                visible: w.sectioned ? !w.narrow || w.sectionOpen || w.query !== ""
                                     : !w.narrow || w.index !== -2 || w.single
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                PosButton {
                    objectName: "adminBackToList"
                    visible: w.narrow && !w.single && w.index !== -2
                    text: qsTr("‹ List")
                    onClicked: { w.index = -2; w.dirty = false }
                }
                PosButton {
                    objectName: "adminBackToSections"
                    visible: w.sectioned && w.narrow && w.query === ""
                    text: qsTr("‹ Settings")
                    onClicked: w.sectionOpen = false
                }
                Label {
                    visible: w.index === -2
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("Choose something on the left, or Add.")
                    opacity: 0.7
                }

                Label {
                    visible: w.index !== -2
                    text: w.sectioned ? (w.query !== "" ? qsTr("%n setting(s) found", "", w.shownFields.length)
                                                        : (w.sections.find(x => x.key === w.sectionShown) ?? {}).name ?? "")
                        : w.index === -1 ? qsTr("New") : (w.draft.name || w.draft.storeName || (w.single ? "" : qsTr("Edit")))
                    font.bold: true
                    font.pixelSize: 18
                }

                ScrollView {
                    id: scroll
                    visible: w.index !== -2
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: availableWidth
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    // Finger-wide, shown when the form is longer than the panel.
                    ScrollBar.vertical: TouchScrollBar {
                        id: formBar
                        objectName: "adminFormScroll"
                        parent: scroll
                        x: scroll.width - width
                        y: scroll.topPadding
                        height: scroll.availableHeight
                    }

                    ColumnLayout {
                        width: scroll.availableWidth - formBar.room
                        spacing: 10
                        Label {
                            visible: w.sectioned && w.query !== "" && w.shownFields.length === 0
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("No setting has those words. Try fewer, or look through the sections.")
                            opacity: 0.7
                        }
                        Repeater {
                            model: w.shownFields
                            delegate: FieldEditor {
                                required property var modelData
                                minHeight: w.narrow ? 36 : 0
                                // A "look" field: an enum of this screen's looks.
                                field: modelData.type !== "look" || !w.zone ? modelData
                                     : Object.assign({}, modelData, {
                                           type: "enum",
                                           options: [{ value: "", text: qsTr("The store's look") }].concat(
                                               w.zone.controller.looks().map(l => ({ value: l.id, text: qsTranslate("Looks", l.name) })))
                                       })
                                pos: w.pos
                                touch: true
                                pages: modelData.type === "page" && w.zone ? w.pageChoices(modelData) : []
                                // This screen's own lines (About): from this screen, not the store.
                                value: modelData.path === "screen.version" && w.zone ? w.zone.controller.screenBuild()
                                     : modelData.path === "screen.display" ? w.screenText
                                     : w.draft[modelData.path]
                                isSet: true
                                changed: w.differs(modelData.path)
                                section: w.sectioned && w.query !== "" ? modelData.group : ""
                                readOnly: modelData.readonlyExisting === true && w.index >= 0
                                onCommit: v => {
                                    const d = w.copy(w.draft)
                                    d[modelData.path] = v
                                    w.dirty = true   // first: the new draft may rebuild this form
                                    w.draft = d
                                }
                                // Emptied (a color): back to the usual.
                                onReset: commit("")
                            }
                        }
                    }
                }

                RowLayout {
                    // Not on About (nothing there to change), unless changes elsewhere wait.
                    visible: w.index !== -2 && !(w.sectioned && w.query === "" && w.sectionShown === "about" && !w.dirty)
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    PosButton {
                        objectName: "adminSave"
                        text: w.changes > 0 ? qsTr("Save %n change(s)", "", w.changes) : qsTr("Save")
                        highlighted: true
                        // Not before the saved settings are here (a handheld).
                        enabled: (w.dirty || w.index === -1) && !w.waiting && w.pending === ""
                        onClicked: w.save()
                    }
                    PosButton {
                        text: qsTr("Undo changes")
                        enabled: w.dirty
                        onClicked: w.choose(w.index)
                    }
                    // Printers: a test page (saved settings), and its drawer.
                    PosButton {
                        objectName: "testPrint"
                        visible: w.panel === "printers" && w.index >= 0
                        enabled: !w.dirty
                        text: w.dirty ? qsTr("Save, then Test Print") : qsTr("Test Print")
                        onClicked: w.pos.testPrinter(w.draft.id, false)
                    }
                    PosButton {
                        visible: w.panel === "printers" && w.index >= 0 && w.draft.drawerKick === true
                        enabled: !w.dirty
                        text: qsTr("Test Print + Open Drawer")
                        onClicked: w.pos.testPrinter(w.draft.id, true)
                    }
                    Item { Layout.fillWidth: true }
                    PosButton {
                        visible: !w.single && w.index >= 0
                        palette.button: w.armDelete ? "#a42828" : "#343c49"   // red once it's armed
                        text: w.armDelete ? qsTr("Tap again to confirm")
                                          : (w.panel === "employees" ? qsTr("Deactivate") : qsTr("Remove"))
                        onClicked: {
                            if (!w.armDelete) {
                                w.armDelete = true
                                return
                            }
                            w.remove()
                            w.armDelete = false
                        }
                    }
                }
            }
        }
    }
}
