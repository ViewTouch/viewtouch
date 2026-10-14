#include "reportexport.hh"

#include "app/i18n.hh"

#include <QDateTime>
#include <QDir>
#include <QFont>
#include <QFontMetrics>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QSaveFile>

using namespace Qt::StringLiterals;

namespace {

QString fileFor(const QVariantMap &report, const QString &dir, const QString &suffix, QString *error)
{
    if (!QDir().mkpath(dir)) {
        if (error)
            *error = u"Cannot create %1"_s.arg(dir);
        return {};
    }
    static const QRegularExpression nonWord(u"[^a-z0-9]+"_s);
    QString name = report.value(u"title"_s).toString().toLower().replace(nonWord, u"-"_s);
    name = name.isEmpty() ? u"report"_s : name;
    return QDir(dir).filePath(u"%1-%2.%3"_s.arg(name, QDateTime::currentDateTime().toString(u"yyyyMMdd-HHmmss"_s), suffix));
}

QString csvField(QString s)
{
    // Text a spreadsheet would run as a formula (a guest named "=HYPERLINK(...)"):
    // a ' in front keeps it text. Amounts like -5.00 stay numbers.
    static const QRegularExpression number(u"^-?[$€£]?[0-9.,]+%?$"_s);
    if (!s.isEmpty() && (s.front() == u'=' || s.front() == u'+' || s.front() == u'@' || s.front() == u'\t'
                         || s.front() == u'\r' || (s.front() == u'-' && !number.match(s).hasMatch())))
        s.prepend(u'\'');
    if (s.contains(u',') || s.contains(u'"') || s.contains(u'\n')) {
        s.replace(u"\""_s, u"\"\""_s);
        return u'"' + s + u'"';
    }
    return s;
}

} // namespace

QString exportReportCsv(const QVariantMap &report, const QString &dir, QString *error)
{
    const QString file = fileFor(report, dir, u"csv"_s, error);
    if (file.isEmpty())
        return {};
    QStringList lines;
    lines << csvField(report.value(u"title"_s).toString());
    if (const QString sub = report.value(u"subtitle"_s).toString(); !sub.isEmpty())
        lines << csvField(sub);
    lines << QString();
    QStringList head;
    for (const QVariant &c : report.value(u"columns"_s).toList())
        head << csvField(c.toString());
    if (!head.isEmpty())
        lines << head.join(u',');
    for (const QVariant &v : report.value(u"rows"_s).toList()) {
        QStringList cells;
        for (const QVariant &c : v.toMap().value(u"cells"_s).toList())
            cells << csvField(c.toString());
        lines << cells.join(u',');
    }
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = f.errorString();
        return {};
    }
    f.write(lines.join(u'\n').toUtf8() + '\n');
    if (!f.commit()) {
        if (error)
            *error = f.errorString();
        return {};
    }
    return file;
}

QString exportReportPdf(const QVariantMap &report, const QString &dir, QString *error)
{
    const QString file = fileFor(report, dir, u"pdf"_s, error);
    if (file.isEmpty())
        return {};
    QPdfWriter pdf(file);
    pdf.setPageSize(QPageSize(QPageSize::Letter));
    pdf.setResolution(150);
    pdf.setTitle(report.value(u"title"_s).toString());
    pdf.setCreator(u"ViewTouch"_s);
    QPainter p;
    if (!p.begin(&pdf)) {
        if (error)
            *error = u"Cannot write %1"_s.arg(file);
        return {};
    }
    const int margin = 90;   // 0.6 in at 150 dpi
    const int width = pdf.width() - 2 * margin;
    const int bottom = pdf.height() - margin;
    QFont body(u"DejaVu Sans"_s, 10);
    QFont bold = body;
    bold.setBold(true);
    QFont title = bold;
    title.setPointSize(16);
    const int lineH = QFontMetrics(body, &pdf).height() + 6;
    int y = margin;

    // Columns: the first takes what the others leave.
    const QStringList columns = report.value(u"columns"_s).toStringList();
    const int numberCols = std::max<int>(0, int(columns.size()) - 1);
    const int numberW = numberCols > 0 ? std::min(width / 5, width / (numberCols + 2)) : 0;
    auto drawRow = [&](const QStringList &cells, const QFont &font) {
        if (y + lineH > bottom) {
            pdf.newPage();
            y = margin;
        }
        p.setFont(font);
        const int firstW = width - numberW * std::max<int>(0, int(cells.size()) - 1);
        int x = margin;
        for (int i = 0; i < cells.size(); ++i) {
            const int w = i == 0 ? firstW : numberW;
            const QRect r(x, y, w, lineH);
            const Qt::Alignment a = (i == 0 ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter;
            p.drawText(r, a, QFontMetrics(font, &pdf).elidedText(cells[i], Qt::ElideRight, w - 10));
            x += w;
        }
        y += lineH;
    };

    p.setFont(title);
    p.drawText(QRect(margin, y, width, lineH * 2), Qt::AlignLeft | Qt::AlignVCenter, report.value(u"title"_s).toString());
    y += lineH * 2;
    if (const QString sub = report.value(u"subtitle"_s).toString(); !sub.isEmpty())
        drawRow({sub}, body);
    y += lineH / 2;
    if (!columns.isEmpty()) {
        drawRow(columns, bold);
        p.drawLine(margin, y, margin + width, y);
        y += 6;
    }
    for (const QVariant &v : report.value(u"rows"_s).toList()) {
        const QVariantMap row = v.toMap();
        const QString kind = row.value(u"kind"_s).toString();
        const QStringList cells = row.value(u"cells"_s).toStringList();
        if (kind == u"section")
            y += lineH / 3;
        if (kind == u"total") {
            p.drawLine(margin + width / 2, y, margin + width, y);
            y += 3;
        }
        drawRow(cells, kind == u"section" || kind == u"total" ? bold : body);
    }
    p.setFont(body);
    p.drawText(QRect(margin, bottom, width, lineH), Qt::AlignRight | Qt::AlignVCenter,
               u"ViewTouch · "_s + vt::i18n::dateTimeText(QDateTime::currentDateTime()));
    p.end();
    return file;
}
