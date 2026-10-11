import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts

// Reports on screen: pick a report and a day (today is live; closed days
// show what was saved at End of Day). props.report picks the first report.
Item {
    id: w
    property ZoneItem zone
    readonly property PosService pos: zone ? zone.pos : null
    readonly property string face: zone.st.font ?? "DejaVu Sans"
    readonly property color ink: zone.st.textColor ?? "white"
    // Upright (a phone): bigger, and the choices in fewer columns.
    readonly property bool narrow: zone ? zone.narrow : false
    readonly property real unit: narrow ? Math.max(12, Math.min(40, w.width * 0.038))
                                        : Math.max(12, Math.min(26, w.width * 0.022))
    // Two rows of names, or more when the longest (in this language) would
    // have to shrink to fit its key.
    readonly property int reportColumns: {
        if (narrow)
            return 4
        const gap = w.unit * 0.3
        for (let rows = 2; rows < 5; ++rows) {
            const cols = Math.ceil(reportIds.length / rows)
            if ((w.width - w.unit * 1.2 - (cols - 1) * gap) / cols >= longestName.width + 14)
                return cols
        }
        return Math.ceil(reportIds.length / 5)
    }
    TextMetrics {
        id: longestName
        font.pixelSize: Math.max(10, w.keyH * 0.36)
        font.bold: true
        text: w.reportIds.reduce((a, r) => r.label.length > a.length ? r.label : a, "")
    }
    readonly property real keyH: Math.max(w.unit * 2.2, w.zone ? w.zone.touch(46) : 0)

    readonly property var reportIds: [
        { id: "sales", label: qsTr("Sales") }, { id: "items", label: qsTr("Items") },
        { id: "categories", label: qsTr("Categories") }, { id: "hourly", label: qsTr("By Hour") },
        { id: "servers", label: qsTr("Servers") }, { id: "tips", label: qsTr("Tips") },
        { id: "labor", label: qsTr("Labor") },
        { id: "drawer", label: qsTr("Drawer") }, { id: "expenses", label: qsTr("Expenses") }, { id: "purchases", label: qsTr("Purchases") }, { id: "audit", label: qsTr("Audit") },
        { id: "exceptions", label: qsTr("Exceptions") }, { id: "deposit", label: qsTr("Deposit") }, { id: "customers", label: qsTr("Customers") },
        { id: "royalty", label: qsTr("Royalty") }, { id: "accounting", label: qsTr("Accounting") },
        { id: "accounts", label: qsTr("Gift Cards") },
        { id: "kitchen", label: qsTr("Kitchen") },
        { id: "foodcost", label: qsTr("Food Cost") },
        { id: "engineering", label: qsTr("Menu Mix") },
        { id: "forecast", label: qsTr("Forecast") },
        { id: "turns", label: qsTr("Turns") },
        { id: "checklists", label: qsTr("Checklists") },
        { id: "drivers", label: qsTr("Drivers") },
    ]
    property string reportId: zone && zone.props && zone.props.report ? zone.props.report : "sales"
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: w.unit * 0.6
        spacing: w.unit * 0.5

        // Two rows or more (on a phone, four columns), so each name has room to be read.
        GridLayout {
            readonly property int keyRows: Math.ceil(w.reportIds.length / w.reportColumns)
            Layout.fillWidth: true
            Layout.preferredHeight: keyRows * w.keyH + (keyRows - 1) * w.unit * 0.3
            Layout.minimumHeight: Layout.preferredHeight
            Layout.fillHeight: false   // nested layouts fill by default
            columns: w.reportColumns
            rowSpacing: w.unit * 0.3
            columnSpacing: w.unit * 0.3
            Repeater {
                model: w.reportIds
                delegate: WidgetKey {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: modelData.label
                    fontScale: 0.36
                    baseColor: w.reportId === modelData.id ? "#2f6fd6" : "#343c49"
                    onClicked: w.reportId = modelData.id
                }
            }
        }

        // The forecast's day: today and the six after.
        GridLayout {
            visible: w.forecasting
            Layout.fillWidth: true
            readonly property int keyRows: w.narrow ? 2 : 1
            Layout.preferredHeight: keyRows * w.keyH + (keyRows - 1) * w.unit * 0.3
            Layout.minimumHeight: Layout.preferredHeight
            Layout.fillHeight: false
            columns: w.narrow ? 4 : 7
            rowSpacing: w.unit * 0.3
            columnSpacing: w.unit * 0.3
            Repeater {
                model: 7
                delegate: WidgetKey {
                    required property int index
                    objectName: "forecastDay-" + index
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: index === 0 ? qsTr("Today") : index === 1 ? qsTr("Tomorrow")
                        : new Date(Date.now() + index * 86400000).toLocaleDateString(Qt.locale(), "ddd d")
                    fontScale: 0.34
                    baseColor: w.forecastDay === index ? "#2f6fd6" : "#343c49"
                    onClicked: w.forecastDay = index
                }
            }
        }

        // Which days.
        GridLayout {
            visible: !w.forecasting
            Layout.fillWidth: true
            readonly property int keyRows: w.narrow ? Math.ceil((w.periods.length + 1) / 4) : 1
            Layout.preferredHeight: keyRows * w.keyH + (keyRows - 1) * w.unit * 0.3
            Layout.minimumHeight: Layout.preferredHeight
            Layout.fillHeight: false
            columns: w.narrow ? 4 : 99
            rowSpacing: w.unit * 0.3
            columnSpacing: w.unit * 0.3
            Repeater {
                model: w.periods
                delegate: WidgetKey {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: modelData.label
                    fontScale: 0.34
                    baseColor: w.period === modelData.id ? "#2f6fd6" : "#343c49"
                    onClicked: w.period = modelData.id
                }
            }
            WidgetKey {
                visible: w.period !== "day"
                Layout.preferredWidth: w.unit * 7
                Layout.fillHeight: true
                text: qsTr("vs Last Year")
                fontScale: 0.34
                baseColor: w.compare ? "#1f8a4c" : "#343c49"
                onClicked: w.compare = !w.compare
            }
        }

        // Dates… : from and to.
        RowLayout {
            visible: w.period === "custom"
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

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(w.unit * 2.4, w.zone ? w.zone.touch(46) : 0)
            Layout.fillHeight: false
            spacing: w.unit * 0.3
            Text {
                visible: w.period !== "day" || w.forecasting
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: (w.range.label ?? "") + (w.range.loading ? "  ·  " + qsTr("reading…")
                                               : w.range.checks !== undefined ? "  ·  " + qsTr("%1 checks").arg(w.range.checks) : "")
                color: w.ink
                font.family: w.face
                font.pixelSize: w.unit
                elide: Text.ElideRight
            }
            WidgetKey {
                visible: w.period === "day" && !w.forecasting
                Layout.preferredWidth: w.unit * 3
                Layout.fillHeight: true
                text: "◀"
                baseColor: w.dayIndex < w.days.length - 1 ? "#343c49" : "#23282f"
                onClicked: if (w.dayIndex < w.days.length - 1) w.dayIndex++
            }
            Text {
                visible: w.period === "day" && !w.forecasting
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: w.day.label
                color: w.ink
                font.family: w.face
                font.pixelSize: w.unit
                elide: Text.ElideRight
            }
            WidgetKey {
                visible: w.period === "day" && !w.forecasting
                Layout.preferredWidth: w.unit * 3
                Layout.fillHeight: true
                text: "▶"
                baseColor: w.dayIndex > 0 ? "#343c49" : "#23282f"
                onClicked: if (w.dayIndex > 0) w.dayIndex--
            }
            WidgetKey {
                visible: w.period === "day" || w.forecasting
                Layout.preferredWidth: w.unit * 5
                Layout.fillHeight: true
                text: qsTr("Print")
                fontScale: 0.36
                onClicked: w.pos.printReport(w.reportId, w.day.id)
            }
            WidgetKey {
                Layout.preferredWidth: w.unit * 4
                Layout.fillHeight: true
                text: "CSV"
                fontScale: 0.36
                onClicked: w.zone.controller.exportReport(w.report, "csv")
            }
            WidgetKey {
                Layout.preferredWidth: w.unit * 4
                Layout.fillHeight: true
                text: "PDF"
                fontScale: 0.36
                onClicked: w.zone.controller.exportReport(w.report, "pdf")
            }
        }

        Text {
            text: w.report.title ?? ""
            color: w.ink
            font.family: w.face
            font.pixelSize: w.unit * 1.4
            font.bold: true
        }

        // Column headings (reports with more than one value column)
        RowLayout {
            visible: (w.report.columns ?? []).length > 2
            Layout.fillWidth: true
            Layout.fillHeight: false
            Repeater {
                model: w.report.columns ?? []
                delegate: Text {
                    required property string modelData
                    required property int index
                    Layout.fillWidth: index === 0
                    Layout.preferredWidth: index === 0 ? -1 : w.unit * 7
                    horizontalAlignment: index === 0 ? Text.AlignLeft : Text.AlignRight
                    text: modelData
                    color: "#8a94a6"
                    font.family: w.face
                    font.pixelSize: w.unit * 0.8
                }
            }
        }

        ListView {
            id: rows
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.vertical: TouchScrollBar { id: rowsBar }
            model: w.report.rows ?? []
            delegate: Item {
                id: row
                required property var modelData
                readonly property bool section: modelData.kind === "section"
                readonly property bool total: modelData.kind === "total"
                width: ListView.view.width - rowsBar.room
                height: (section ? w.unit * 2.4 : w.unit * 1.6)

                Rectangle {
                    visible: row.total
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: "#3a4250"
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.topMargin: row.section ? w.unit * 0.8 : 0
                    Repeater {
                        model: row.modelData.cells
                        delegate: Text {
                            required property string modelData
                            required property int index
                            Layout.fillWidth: index === 0
                            Layout.preferredWidth: index === 0 ? -1 : w.unit * 7
                            horizontalAlignment: index === 0 ? Text.AlignLeft : Text.AlignRight
                            text: modelData
                            color: row.modelData.kind === "note" ? "#8a94a6" : w.ink
                            font.family: w.face
                            font.pixelSize: row.section ? w.unit * 1.1 : w.unit
                            font.bold: row.section || row.total
                            font.italic: row.modelData.kind === "note"
                            elide: index === 0 ? Text.ElideRight : Text.ElideNone
                        }
                    }
                }
            }
        }
    }
}
