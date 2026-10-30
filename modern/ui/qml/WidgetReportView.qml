import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Reports on screen: pick a report and a day (today is live; closed days
// show what was saved at End of Day). props.report picks the first report.
//
// The reports are a list by kind (Sales, Staff, Money...) beside the report
// when there's room, else behind one "Sales ▾" key that opens it over the
// report (a phone). The days are one row to slide; it folds away while the
// report is read further down. A swipe across the report goes to the next
// or the previous report.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string face: zone.st.font ?? Ui.font
    readonly property color ink: zone.st.textColor ?? "white"
    // Upright (a phone): bigger, and the list behind a key.
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real unit: narrow ? Math.max(12, Math.min(40, w.width * 0.038))
                                        : Math.max(12, Math.min(26, w.width * 0.016))
    readonly property real keyH: Math.max(w.unit * 2.2, w.zone ? w.zone.touch(46) : 0)
    readonly property real gap: w.unit * 0.3

    // The list beside the report: when the report keeps at least 30 units.
    readonly property real sideW: Math.max(w.unit * 12, Math.min(w.width * 0.24, w.unit * 17))
    readonly property bool docked: !narrow && w.width - sideW > w.unit * 30
    property bool listOpen: false   // the list over the report (not docked)

    // Every report, by kind. `range`: it can be read over several days
    // (the same list as kRangeReports in app/pos_day.cpp); the others are
    // one business day at a time.
    readonly property var groups: [
        { label: qsTr("Sales"), reports: [
            { id: "sales", label: qsTr("Sales"), range: true },
            { id: "items", label: qsTr("Items"), range: true },
            { id: "categories", label: qsTr("Categories"), range: true },
            { id: "hourly", label: qsTr("By Hour"), range: true },
            { id: "engineering", label: qsTr("Menu Mix"), range: true },
            { id: "forecast", label: qsTr("Forecast"), range: false } ] },
        { label: qsTr("Staff"), reports: [
            { id: "servers", label: qsTr("Servers"), range: true },
            { id: "tips", label: qsTr("Tips"), range: false },
            { id: "labor", label: qsTr("Labor"), range: false },
            { id: "kitchen", label: qsTr("Kitchen"), range: true },
            { id: "drivers", label: qsTr("Drivers"), range: false },
            { id: "checklists", label: qsTr("Checklists"), range: false } ] },
        { label: qsTr("Money"), reports: [
            { id: "drawer", label: qsTr("Drawer"), range: false },
            { id: "deposit", label: qsTr("Deposit"), range: false },
            { id: "expenses", label: qsTr("Expenses"), range: false },
            { id: "accounting", label: qsTr("Accounting"), range: true },
            { id: "accounts", label: qsTr("Gift Cards"), range: false },
            { id: "royalty", label: qsTr("Royalty"), range: true } ] },
        { label: qsTr("Stock"), reports: [
            { id: "stock", label: qsTr("Stock"), range: false },
            { id: "purchases", label: qsTr("Purchases"), range: false },
            { id: "foodcost", label: qsTr("Food Cost"), range: true } ] },
        { label: qsTr("Guests"), reports: [
            { id: "customers", label: qsTr("Customers"), range: true },
            { id: "turns", label: qsTr("Turns"), range: true } ] },
        { label: qsTr("Checks"), reports: [
            { id: "audit", label: qsTr("Audit"), range: true },
            { id: "exceptions", label: qsTr("Exceptions"), range: true },
            { id: "changes", label: qsTr("Changes"), range: false },
            { id: "irs8027", label: qsTr("IRS 8027"), range: true } ] },
    ]
    // The same, in one line: the order a swipe goes through them.
    readonly property var reportIds: groups.reduce((all, g) => all.concat(g.reports), [])
    readonly property var current: reportIds.find(r => r.id === reportId) ?? reportIds[0]

    property string reportId: zone && zone.props && zone.props.report ? zone.props.report : "sales"
    function pick(id) {
        const r = reportIds.find(x => x.id === id)
        if (!r)
            return
        // A report that is one day at a time goes back to a day.
        if (!r.range && period !== "day")
            period = "day"
        reportId = id
        listOpen = false
        folded = false
    }
    function step(by) {
        const i = reportIds.findIndex(r => r.id === reportId)
        pick(reportIds[(i + by + reportIds.length) % reportIds.length].id)
    }

    property int dayIndex: 0
    readonly property var days: pos ? pos.days : []
    readonly property var day: days[Math.min(dayIndex, days.length - 1)] ?? { id: 0, label: "" }
    // "day" (one business day, above), or several: week, lastWeek, month,
    // lastMonth, year, custom - read from the saved checks, maybe beside
    // the same days a year before.
    property string period: "day"
    property bool compare: false
    property string fromDate: ""
    property string toDate: ""
    readonly property var periods: [
        { id: "day", label: qsTr("Day") }, { id: "week", label: qsTr("This Week") },
        { id: "lastWeek", label: qsTr("Last Week") }, { id: "month", label: qsTr("This Month") },
        { id: "lastMonth", label: qsTr("Last Month") }, { id: "year", label: qsTr("This Year") },
        { id: "lastYear", label: qsTr("Last Year") },
        { id: "custom", label: qsTr("Dates…") },
    ]
    readonly property var range: pos ? pos.rangeReport : ({})
    // The forecast: a coming day (0 today ... 6), from past weeks.
    readonly property bool forecasting: reportId === "forecast"
    property int forecastDay: 1
    function refresh() {
        if (forecasting && pos) {
            pos.requestForecast(forecastDay)
            return
        }
        if (period === "day" || !pos)
            return
        if (period === "custom" && (fromDate === "" || toDate === ""))
            return
        pos.requestRangeReport(reportId, period, fromDate, toDate, compare)
    }
    onReportIdChanged: refresh()
    onPeriodChanged: refresh()
    onCompareChanged: refresh()
    onForecastDayChanged: refresh()
    readonly property var report: {
        if (!pos) return ({ rows: [] })
        if (forecasting)
            return range.id === "forecast" && range.report ? range.report
                 : ({ title: range.loading ? qsTr("Reading the checks…") : "", rows: [] })
        if (period !== "day")
            return range.report ?? ({ title: range.loading ? qsTr("Reading the checks…") : "", rows: [] })
        // Live: again when checks close, the drawer changes, or (remote
        // terminals) the server's answer arrives. Compared, not just read:
        // an unused read is compiled away.
        if (!pos.day || !pos.drawer || pos.queryRevision < 0) return ({ rows: [] })
        return pos.report(reportId, day.id)
    }

    // The value columns, each as wide as its widest cell (a heading too),
    // between 4 and 14 units; the first column takes what's left, at least
    // 9 units. Wider than the space: the table slides sideways.
    FontMetrics { id: cellFont; font.family: w.face; font.pixelSize: w.unit }
    FontMetrics { id: boldFont; font.family: w.face; font.pixelSize: w.unit; font.bold: true }
    readonly property var valueWidths: {
        // Read, so this is worked out again once the fonts have their size
        // (measured before, with the size they started at, the columns came out short).
        if (cellFont.font.pixelSize <= 0 || boldFont.font.pixelSize <= 0)
            return []
        const cols = report.columns ?? []
        const rowsIn = report.rows ?? []
        let n = Math.max(0, cols.length - 1)
        for (const r of rowsIn)
            if (r.kind !== "section" && r.kind !== "note")
                n = Math.max(n, (r.cells ?? []).length - 1)
        const out = []
        for (let i = 0; i < n; ++i)
            out.push(cols.length > i + 1 ? cellFont.advanceWidth(cols[i + 1]) * 0.8 : 0)
        for (const r of rowsIn) {
            if (r.kind === "section" || r.kind === "note")
                continue
            const m = r.kind === "total" ? boldFont : cellFont
            const cells = r.cells ?? []
            for (let i = 1; i < cells.length && i <= n; ++i)
                out[i - 1] = Math.max(out[i - 1], m.advanceWidth(cells[i]))
        }
        return out.map(x => Math.max(w.unit * 4, Math.min(w.unit * 14, x * 1.05 + w.unit)))
    }
    function valueWidth(i) { return i > 0 && i <= valueWidths.length ? valueWidths[i - 1] : w.unit * 7 }
    readonly property real valuesW: valueWidths.reduce((a, x) => a + x, 0)

    // Read further down: the days' row folds away; scrolled back up it returns.
    property bool folded: false

    RowLayout {
        anchors.fill: parent
        anchors.margins: w.unit * 0.6
        spacing: w.unit * 0.6

        ColumnLayout {
            id: main
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: w.unit * 0.4

            // Not docked: the report's name, touched for the list.
            WidgetKey {
                visible: !w.docked
                objectName: "reportPicker"
                Layout.fillWidth: true
                Layout.preferredHeight: w.keyH
                text: w.current.label + "  ▾"
                fontScale: 0.38
                baseColor: Ui.accent
                onClicked: w.listOpen = true
            }

            // The days (or, for the forecast, which coming day): one row to slide.
            Item {
                id: daysRow
                Layout.fillWidth: true
                property real shown: w.folded ? 0 : 1
                Behavior on shown { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                Layout.preferredHeight: shown * w.keyH
                Layout.fillHeight: false
                visible: shown > 0.01
                clip: true

                ChipStrip {
                    rv: w
                    visible: !w.forecasting
                    anchors.fill: parent
                    model: w.periods
                    labelOf: p => p.label
                    isOn: p => w.period === p.id
                    // Several days only for the reports that can be read that way.
                    isDim: p => p.id !== "day" && !w.current.range
                    onPicked: p => { if (p.id === "day" || w.current.range) w.period = p.id }
                }
                ChipStrip {
                    rv: w
                    visible: w.forecasting
                    anchors.fill: parent
                    model: [0, 1, 2, 3, 4, 5, 6]
                    namePrefix: "forecastDay-"
                    labelOf: i => i === 0 ? qsTr("Today") : i === 1 ? qsTr("Tomorrow")
                                : new Date(Date.now() + i * 86400000).toLocaleDateString(Qt.locale(), "ddd d")
                    isOn: i => w.forecastDay === i
                    onPicked: i => w.forecastDay = i
                }
            }

            // Dates… : from and to.
            RowLayout {
                visible: w.period === "custom" && !w.folded
                Layout.fillWidth: true
                Layout.fillHeight: false
                spacing: w.unit * 0.4
                Text { text: qsTr("From"); color: w.ink; font.family: w.face; font.pixelSize: w.unit * 0.8 }
                TextField {
                    implicitWidth: w.unit * 8
                    placeholderText: "2026-09-01"
                    inputMethodHints: Qt.ImhPreferNumbers
                    onTextEdited: w.fromDate = text
                }
                Text { text: qsTr("to"); color: w.ink; font.family: w.face; font.pixelSize: w.unit * 0.8 }
                TextField {
                    implicitWidth: w.unit * 8
                    placeholderText: "2026-09-30"
                    inputMethodHints: Qt.ImhPreferNumbers
                    onTextEdited: w.toDate = text
                    onAccepted: w.refresh()
                }
                WidgetKey {
                    Layout.preferredWidth: w.unit * 5
                    Layout.preferredHeight: Math.max(w.unit * 2, w.zone ? w.zone.touch(46) : 0)
                    text: qsTr("Show")
                    fontScale: 0.34
                    onClicked: w.refresh()
                }
                Item { Layout.fillWidth: true }
            }

            // ◀ the day ▶ (or the days read and their checks), then Print, CSV, PDF
            // (on a phone behind ⋯).
            RowLayout {
                id: dayBar
                readonly property real rowH: Math.max(w.unit * 2.2, w.zone ? w.zone.touch(46) : 0)
                readonly property bool oneDay: w.period === "day" && !w.forecasting
                Layout.fillWidth: true
                Layout.fillHeight: false
                spacing: w.gap
                WidgetKey {
                    visible: dayBar.oneDay
                    Layout.preferredWidth: w.unit * 3
                    Layout.preferredHeight: dayBar.rowH
                    text: "◀"
                    baseColor: w.dayIndex < w.days.length - 1 ? Ui.key : "#23282f"
                    onClicked: if (w.dayIndex < w.days.length - 1) w.dayIndex++
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: dayBar.oneDay ? w.day.label
                        : (w.range.label ?? "") + (w.range.loading ? "  ·  " + qsTr("reading…")
                                                   : w.range.checks !== undefined ? "  ·  " + qsTr("%1 checks").arg(w.range.checks) : "")
                    color: w.ink
                    font.family: w.face
                    font.pixelSize: w.unit
                    fontSizeMode: Text.HorizontalFit   // smaller before "…"
                    minimumPixelSize: Math.round(font.pixelSize * 0.7)
                    elide: Text.ElideRight
                }
                WidgetKey {
                    visible: dayBar.oneDay
                    Layout.preferredWidth: w.unit * 3
                    Layout.preferredHeight: dayBar.rowH
                    text: "▶"
                    baseColor: w.dayIndex > 0 ? Ui.key : "#23282f"
                    onClicked: if (w.dayIndex > 0) w.dayIndex--
                }
                WidgetKey {
                    visible: w.period !== "day" && !w.forecasting
                    Layout.preferredWidth: w.unit * 7
                    Layout.preferredHeight: dayBar.rowH
                    text: qsTr("vs Last Year")
                    fontScale: 0.34
                    baseColor: w.compare ? Ui.go : Ui.key
                    onClicked: w.compare = !w.compare
                }
                WidgetKey {
                    visible: !w.narrow && (w.period === "day" || w.forecasting)
                    Layout.preferredWidth: w.unit * 5
                    Layout.preferredHeight: dayBar.rowH
                    text: qsTr("Print")
                    fontScale: 0.36
                    onClicked: w.pos.printReport(w.reportId, w.day.id)
                }
                WidgetKey {
                    visible: !w.narrow
                    Layout.preferredWidth: w.unit * 4
                    Layout.preferredHeight: dayBar.rowH
                    text: "CSV"
                    fontScale: 0.36
                    onClicked: w.zone.controller.exportReport(w.report, "csv")
                }
                WidgetKey {
                    visible: !w.narrow
                    Layout.preferredWidth: w.unit * 4
                    Layout.preferredHeight: dayBar.rowH
                    text: "PDF"
                    fontScale: 0.36
                    onClicked: w.zone.controller.exportReport(w.report, "pdf")
                }
                WidgetKey {
                    id: moreKey
                    visible: w.narrow
                    Layout.preferredWidth: w.unit * 3
                    Layout.preferredHeight: dayBar.rowH
                    text: "⋯"
                    baseColor: shareSheet.visible ? Ui.accent : Ui.key
                    onClicked: shareSheet.visible = !shareSheet.visible
                }
            }
            // A phone: Print, CSV, PDF under ⋯.
            RowLayout {
                id: shareSheet
                visible: false
                Layout.fillWidth: true
                Layout.fillHeight: false
                spacing: w.gap
                WidgetKey {
                    visible: w.period === "day" || w.forecasting
                    Layout.fillWidth: true
                    Layout.preferredHeight: dayBar.rowH
                    text: qsTr("Print")
                    fontScale: 0.36
                    onClicked: { shareSheet.visible = false; w.pos.printReport(w.reportId, w.day.id) }
                }
                WidgetKey {
                    Layout.fillWidth: true
                    Layout.preferredHeight: dayBar.rowH
                    text: "CSV"
                    fontScale: 0.36
                    onClicked: { shareSheet.visible = false; w.zone.controller.exportReport(w.report, "csv") }
                }
                WidgetKey {
                    Layout.fillWidth: true
                    Layout.preferredHeight: dayBar.rowH
                    text: "PDF"
                    fontScale: 0.36
                    onClicked: { shareSheet.visible = false; w.zone.controller.exportReport(w.report, "pdf") }
                }
            }

            Text {
                Layout.fillWidth: true
                text: w.report.title ?? ""
                color: w.ink
                font.family: w.face
                font.pixelSize: w.unit * 1.4
                font.bold: true
                elide: Text.ElideRight
            }

            // The table: headings and rows slide sideways together when wider
            // than the space; the rows scroll up and down.
            Item {
                id: table
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                readonly property real firstW: Math.max(w.unit * 9, width - rowsBar.room - w.valuesW)
                readonly property real fullW: firstW + w.valuesW
                readonly property bool wide: fullW > width - rowsBar.room + 1

                Flickable {
                    id: across
                    anchors.fill: parent
                    anchors.rightMargin: rowsBar.room
                    contentWidth: table.fullW
                    contentHeight: height
                    flickableDirection: Flickable.HorizontalFlick
                    boundsBehavior: Flickable.StopAtBounds
                    interactive: table.wide
                    ScrollBar.horizontal: ScrollBar { policy: table.wide ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff }

                    ColumnLayout {
                        width: table.fullW
                        height: across.height
                        spacing: w.unit * 0.2

                        // Column headings (reports with more than one value column)
                        Row {
                            visible: (w.report.columns ?? []).length > 2
                            Layout.fillWidth: true
                            Repeater {
                                model: w.report.columns ?? []
                                delegate: Text {
                                    required property string modelData
                                    required property int index
                                    width: index === 0 ? table.firstW : w.valueWidth(index)
                                    horizontalAlignment: index === 0 ? Text.AlignLeft : Text.AlignRight
                                    text: modelData
                                    color: Ui.muted
                                    font.family: w.face
                                    font.pixelSize: w.unit * 0.8
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        ListView {
                            id: rows
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            flickableDirection: Flickable.VerticalFlick
                            boundsBehavior: Flickable.StopAtBounds
                            // The bar stays at the table's right edge, not the slid content's.
                            // (Placed here by hand: a bar given another parent isn't laid out by the list.)
                            ScrollBar.vertical: TouchScrollBar {
                                id: rowsBar
                                parent: table
                                x: table.width - width
                                y: 0
                                height: table.height
                            }
                            model: w.report.rows ?? []
                            property real lastY: 0
                            onContentYChanged: {
                                if (!moving)
                                    return
                                if (contentY > lastY + 4 && contentY > w.keyH)
                                    w.folded = true
                                else if (contentY < lastY - 24 || atYBeginning)
                                    w.folded = false
                                lastY = contentY
                            }
                            onMovementStarted: lastY = contentY
                            onModelChanged: { positionViewAtBeginning(); lastY = 0 }
                            delegate: Item {
                                id: row
                                required property var modelData
                                readonly property bool section: modelData.kind === "section"
                                readonly property bool total: modelData.kind === "total"
                                readonly property bool note: modelData.kind === "note"
                                width: table.fullW
                                height: (section ? w.unit * 2.4 : w.unit * 1.6)

                                Rectangle {
                                    visible: row.total
                                    anchors.top: parent.top
                                    width: parent.width
                                    height: 1
                                    color: Ui.line
                                }
                                Row {
                                    anchors.fill: parent
                                    anchors.topMargin: row.section ? w.unit * 0.8 : 0
                                    Repeater {
                                        model: row.modelData.cells
                                        delegate: Text {
                                            required property string modelData
                                            required property int index
                                            // A line with one cell (a section, a note): across the table.
                                            width: row.modelData.cells.length === 1 ? table.fullW
                                                 : index === 0 ? table.firstW : w.valueWidth(index)
                                            height: parent.height
                                            verticalAlignment: Text.AlignVCenter
                                            horizontalAlignment: index === 0 ? Text.AlignLeft : Text.AlignRight
                                            text: modelData
                                            color: row.note ? Ui.muted : w.ink
                                            font.family: w.face
                                            font.pixelSize: row.section ? w.unit * 1.1 : w.unit
                                            font.bold: row.section || row.total
                                            font.italic: row.note
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // A swipe across the report: the next (or previous) report.
                // On top, so it sees the finger first; it only takes it once
                // it has gone sideways, so the rows still scroll up and down.
                // Not on a table that slides sideways: the finger moves it.
                Item {
                    anchors.fill: parent
                    DragHandler {
                        enabled: !table.wide
                        target: null
                        yAxis.enabled: false
                        onActiveChanged: {
                            if (active)
                                return
                            const dx = centroid.position.x - centroid.pressPosition.x
                            if (Math.abs(dx) > Math.max(w.unit * 4, table.width * 0.15))
                                w.step(dx < 0 ? 1 : -1)
                        }
                    }
                }
            }
        }

        // Docked: the reports beside the report.
        ReportList {
            rv: w
            visible: w.docked
            Layout.preferredWidth: w.sideW
            Layout.fillHeight: true
        }
    }

    // Not docked: the list over the report.
    Rectangle {
        visible: w.listOpen && !w.docked
        anchors.fill: parent
        color: Ui.panel
        z: 10
        // Nothing behind it takes the touches.
        MouseArea { anchors.fill: parent }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: w.unit * 0.6
            spacing: w.gap
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Reports")
                    color: w.ink
                    font.family: w.face
                    font.pixelSize: w.unit * 1.2
                    font.bold: true
                }
                WidgetKey {
                    Layout.preferredWidth: w.keyH
                    Layout.preferredHeight: w.keyH
                    text: "✕"
                    takesTouch: true
                    onClicked: w.listOpen = false
                }
            }
            ReportList {
                rv: w
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: 2
            }
        }
    }

    // The reports by kind: a heading, then its reports two (docked: one or
    // two, as fits) to a row. Scrolls when taller than its space.
    component ReportList: TouchScrollColumn {
        id: list
        required property Item rv   // the report view (inline components don't see its id)
        property int columns: rv.sideW > rv.unit * 14 ? 2 : 1
        spacing: rv.gap
        Repeater {
            model: rv.groups
            delegate: ColumnLayout {
                id: group
                required property var modelData
                Layout.fillWidth: true
                spacing: rv.gap
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: rv.unit * 0.3
                    text: group.modelData.label.toUpperCase()
                    color: Ui.muted
                    font.family: rv.face
                    font.pixelSize: rv.unit * 0.7
                    font.bold: true
                    font.letterSpacing: 1
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: list.columns
                    rowSpacing: rv.gap
                    columnSpacing: rv.gap
                    Repeater {
                        model: group.modelData.reports
                        delegate: WidgetKey {
                            required property var modelData
                            objectName: "report-" + modelData.id
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1   // the columns share alike
                            Layout.preferredHeight: rv.keyH
                            text: modelData.label
                            fontScale: 0.36
                            takesTouch: rv.listOpen
                            baseColor: rv.reportId === modelData.id ? Ui.accent : Ui.key
                            onClicked: rv.pick(modelData.id)
                        }
                    }
                }
            }
        }
    }

    // A row of keys to slide sideways: every one is made (a short list), the
    // one that's on kept in view.
    component ChipStrip: Flickable {
        id: strip
        required property Item rv
        property var model: []
        property var labelOf: x => String(x)
        property var isOn: x => false
        property var isDim: x => false
        property string namePrefix: ""
        signal picked(var value)
        contentWidth: chips.width
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        Row {
            id: chips
            height: strip.height
            spacing: rv.gap
            Repeater {
                model: strip.model
                delegate: WidgetKey {
                    id: chip
                    required property var modelData
                    required property int index
                    objectName: strip.namePrefix !== "" ? strip.namePrefix + index : ""
                    readonly property real need: keyText.advanceWidth(text) + rv.unit * 1.6
                    // At least an even share of the row, so a few chips fill it.
                    width: Math.max(need, rv.unit * 5,
                                    (strip.width - (strip.model.length - 1) * rv.gap) / Math.max(1, strip.model.length))
                    height: strip.height
                    text: strip.labelOf(modelData)
                    fontScale: 0.34
                    baseColor: strip.isOn(modelData) ? Ui.accent : strip.isDim(modelData) ? "#23282f" : Ui.key
                    textColor: strip.isDim(modelData) ? Ui.muted : (keySt.keyTextColor ?? "white")
                    onClicked: strip.picked(modelData)
                    onBaseColorChanged: if (strip.isOn(modelData)) strip.show(chip)
                }
            }
        }
        FontMetrics { id: keyText; font.family: rv.face; font.pixelSize: rv.keyH * 0.34; font.bold: true }
        function show(item) {
            if (item.x < contentX)
                contentX = item.x
            else if (item.x + item.width > contentX + width)
                contentX = Math.min(contentWidth - width, item.x + item.width - width)
        }
    }
}
