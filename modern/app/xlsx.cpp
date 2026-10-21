#include "app/xlsx.hh"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QXmlStreamReader>

#include <array>
#include <cmath>

using namespace Qt::StringLiterals;

namespace vt::app {

namespace {

QString tr(const char *text) { return QCoreApplication::translate("Xlsx", text); }

void say(QString *error, const QString &what)
{
    if (error)
        *error = what;
}

// --- DEFLATE (RFC 1951), after Mark Adler's puff.c -----------------------------------

constexpr int kMaxBits = 15;

struct Bits {
    const uchar *p;
    qsizetype n;
    qsizetype at = 0;
    quint32 buf = 0;
    int count = 0;
    bool ok = true;

    int need(int k)
    {
        quint32 v = buf;
        while (count < k) {
            if (at >= n) {
                ok = false;
                return 0;
            }
            v |= quint32(p[at++]) << count;
            count += 8;
        }
        buf = k < 32 ? v >> k : 0;
        count -= k;
        return int(v & ((1u << k) - 1));
    }
};

struct Huffman {
    std::array<short, kMaxBits + 1> count{};
    std::array<short, 288> symbol{};
};

// < 0: too many codes for their lengths; > 0: some codes unused; 0: complete.
int build(Huffman &h, const short *length, int n)
{
    h.count.fill(0);
    for (int s = 0; s < n; ++s)
        ++h.count[length[s]];
    if (h.count[0] == n)
        return 0;
    int left = 1;
    for (int len = 1; len <= kMaxBits; ++len) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0)
            return left;
    }
    std::array<short, kMaxBits + 1> offs{};
    for (int len = 1; len < kMaxBits; ++len)
        offs[len + 1] = short(offs[len] + h.count[len]);
    for (int s = 0; s < n; ++s)
        if (length[s] != 0)
            h.symbol[offs[length[s]]++] = short(s);
    return left;
}

int decode(Bits &b, const Huffman &h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= kMaxBits; ++len) {
        code |= b.need(1);
        if (!b.ok)
            return -1;
        const int count = h.count[len];
        if (code - count < first)
            return h.symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

bool codes(Bits &b, QByteArray &out, qint64 limit, const Huffman &lencode, const Huffman &distcode)
{
    static constexpr short lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static constexpr short lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static constexpr short dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
                                        1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static constexpr short dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        int sym = decode(b, lencode);
        if (sym < 0)
            return false;
        if (sym < 256) {
            if (out.size() >= limit)
                return false;
            out.append(char(sym));
            continue;
        }
        if (sym == 256)
            return true;
        sym -= 257;
        if (sym >= 29)
            return false;
        const int len = lbase[sym] + b.need(lext[sym]);
        sym = decode(b, distcode);
        if (sym < 0 || sym >= 30)
            return false;
        const qsizetype dist = dbase[sym] + b.need(dext[sym]);
        if (!b.ok || dist > out.size() || out.size() + len > limit)
            return false;
        for (int i = 0; i < len; ++i)   // may overlap what it's copying
            out.append(out.at(out.size() - dist));
    }
}

} // namespace

std::optional<QByteArray> inflate(const QByteArray &deflated, qint64 limit)
{
    Bits b{reinterpret_cast<const uchar *>(deflated.constData()), deflated.size()};
    QByteArray out;
    int last = 0;
    do {
        last = b.need(1);
        const int type = b.need(2);
        if (!b.ok)
            return std::nullopt;
        if (type == 0) {   // stored: the rest of this byte dropped, then LEN, NLEN
            b.buf = 0;
            b.count = 0;
            if (b.at + 4 > b.n)
                return std::nullopt;
            const int len = b.p[b.at] | (b.p[b.at + 1] << 8);
            const int nlen = b.p[b.at + 2] | (b.p[b.at + 3] << 8);
            b.at += 4;
            if (len != (~nlen & 0xffff) || b.at + len > b.n || out.size() + len > limit)
                return std::nullopt;
            out.append(reinterpret_cast<const char *>(b.p + b.at), len);
            b.at += len;
        } else if (type == 1) {   // the fixed codes
            static const auto fixed = [] {
                std::pair<Huffman, Huffman> h;
                std::array<short, 288> lengths{};
                int s = 0;
                for (; s < 144; ++s) lengths[s] = 8;
                for (; s < 256; ++s) lengths[s] = 9;
                for (; s < 280; ++s) lengths[s] = 7;
                for (; s < 288; ++s) lengths[s] = 8;
                build(h.first, lengths.data(), 288);
                lengths.fill(5);
                build(h.second, lengths.data(), 30);
                return h;
            }();
            if (!codes(b, out, limit, fixed.first, fixed.second))
                return std::nullopt;
        } else if (type == 2) {   // codes sent with the block
            const int nlen = b.need(5) + 257;
            const int ndist = b.need(5) + 1;
            const int ncode = b.need(4) + 4;
            if (!b.ok || nlen > 286 || ndist > 30)
                return std::nullopt;
            static constexpr short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            std::array<short, 286 + 30> lengths{};
            for (int i = 0; i < ncode; ++i)
                lengths[order[i]] = short(b.need(3));
            Huffman lencode, distcode;
            if (!b.ok || build(lencode, lengths.data(), 19) != 0)
                return std::nullopt;
            int index = 0;
            while (index < nlen + ndist) {
                int sym = decode(b, lencode);
                if (sym < 0)
                    return std::nullopt;
                if (sym < 16) {
                    lengths[index++] = short(sym);
                    continue;
                }
                short len = 0;
                if (sym == 16) {
                    if (index == 0)
                        return std::nullopt;
                    len = lengths[index - 1];
                    sym = 3 + b.need(2);
                } else if (sym == 17) {
                    sym = 3 + b.need(3);
                } else {
                    sym = 11 + b.need(7);
                }
                if (!b.ok || index + sym > nlen + ndist)
                    return std::nullopt;
                while (sym--)
                    lengths[index++] = len;
            }
            if (lengths[256] == 0)
                return std::nullopt;
            int err = build(lencode, lengths.data(), nlen);
            if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1))
                return std::nullopt;
            err = build(distcode, lengths.data() + nlen, ndist);
            if (err < 0 || (err > 0 && ndist - distcode.count[0] != 1))
                return std::nullopt;
            if (!codes(b, out, limit, lencode, distcode))
                return std::nullopt;
        } else {
            return std::nullopt;
        }
    } while (!last);
    return out;
}

// --- zip ----------------------------------------------------------------------------

namespace {

quint32 crc32(const QByteArray &data)
{
    static const auto table = [] {
        std::array<quint32, 256> t{};
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    quint32 c = 0xffffffffu;
    for (const char ch : data)
        c = table[(c ^ uchar(ch)) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

quint32 le(const QByteArray &b, qsizetype at, int bytes)
{
    quint32 v = 0;
    for (int i = bytes - 1; i >= 0; --i)
        v = (v << 8) | uchar(b[at + i]);
    return v;
}

} // namespace

std::optional<QList<std::pair<QString, QByteArray>>> unzip(const QByteArray &zip, qint64 limit, QString *error)
{
    const auto damaged = [&] {
        say(error, tr("The file is damaged (or isn't a spreadsheet)."));
        return std::nullopt;
    };
    // The end of the central directory: the last 22 bytes, or before a comment.
    qsizetype end = -1;
    for (qsizetype at = zip.size() - 22; at >= 0 && at >= zip.size() - 22 - 0xffff; --at)
        if (le(zip, at, 4) == 0x06054b50) {
            end = at;
            break;
        }
    if (end < 0)
        return damaged();
    const int entries = int(le(zip, end + 10, 2));
    const qsizetype dirSize = le(zip, end + 12, 4), dirAt = le(zip, end + 16, 4);
    if (dirAt + dirSize > end)
        return damaged();
    QList<std::pair<QString, QByteArray>> files;
    qint64 total = 0;
    qsizetype p = dirAt;
    for (int i = 0; i < entries; ++i) {
        if (p + 46 > end || le(zip, p, 4) != 0x02014b50)
            return damaged();
        const quint32 flags = le(zip, p + 8, 2), method = le(zip, p + 10, 2), crc = le(zip, p + 16, 4);
        const qsizetype packed = le(zip, p + 20, 4), size = le(zip, p + 24, 4);
        const int nameLen = int(le(zip, p + 28, 2)), extraLen = int(le(zip, p + 30, 2)), commentLen = int(le(zip, p + 32, 2));
        const qsizetype local = le(zip, p + 42, 4);
        if (p + 46 + nameLen > end)
            return damaged();
        const QString name = QString::fromUtf8(zip.mid(p + 46, nameLen));
        p += 46 + nameLen + extraLen + commentLen;
        if (name.endsWith(u'/'))
            continue;   // a folder
        if (flags & 1) {
            say(error, tr("The spreadsheet has a password: save it again without one."));
            return std::nullopt;
        }
        if (packed == 0xffffffff || size == 0xffffffff || local + 30 > zip.size() || le(zip, local, 4) != 0x04034b50)
            return damaged();
        const qsizetype data = local + 30 + le(zip, local + 26, 2) + le(zip, local + 28, 2);
        if (data + packed > zip.size() || (total += size) > limit)
            return damaged();
        std::optional<QByteArray> contents;
        if (method == 0 && packed == size)
            contents = zip.mid(data, packed);
        else if (method == 8)
            contents = inflate(zip.mid(data, packed), size);
        if (!contents || contents->size() != size || crc32(*contents) != crc)
            return damaged();
        files.append({name, *contents});
    }
    return files;
}

// --- the workbook -------------------------------------------------------------------

namespace {

constexpr const char *kRels = "http://schemas.openxmlformats.org/officeDocument/2006/relationships";

// "B12" -> 1 (the column, from 0); -1 for no letters.
int column(QStringView ref)
{
    int c = 0, letters = 0;
    for (const QChar ch : ref) {
        if (ch < u'A' || ch > u'Z')
            break;
        c = c * 26 + (ch.unicode() - u'A' + 1);
        if (++letters > 3)
            return -1;
    }
    return letters ? c - 1 : -1;
}

// What a relationships file says: Id -> the file, from `dir`.
QHash<QString, QString> relationships(const QByteArray &xml, const QString &dir)
{
    QHash<QString, QString> out;
    QXmlStreamReader x(xml);
    while (!x.atEnd())
        if (x.readNext() == QXmlStreamReader::StartElement && x.name() == u"Relationship"_s) {
            const QString target = x.attributes().value(u"Target"_s).toString();
            const QString path = target.startsWith(u'/') ? target.mid(1) : dir + u'/' + target;
            out.insert(x.attributes().value(u"Id"_s).toString(), QDir::cleanPath(path));
        }
    return out;
}

QStringList sharedStrings(const QByteArray &xml)
{
    QStringList out;
    QString cur;
    QXmlStreamReader x(xml);
    while (!x.atEnd()) {
        x.readNext();
        if (x.isStartElement()) {
            if (x.name() == u"si"_s)
                cur.clear();
            else if (x.name() == u"rPh"_s)   // a reading aid (Japanese), not the text
                x.skipCurrentElement();
            else if (x.name() == u"t"_s)
                cur += x.readElementText();
        } else if (x.isEndElement() && x.name() == u"si"_s) {
            out << cur;
        }
    }
    return out;
}

QString cellText(QStringView type, const QString &value, const QStringList &strings)
{
    if (type == u"s"_s) {
        bool ok = false;
        const int i = value.toInt(&ok);
        return ok ? strings.value(i) : QString();
    }
    if (type == u"b"_s)
        return value == u"1"_s ? u"TRUE"_s : u"FALSE"_s;
    if (type == u"e"_s)
        return {};
    if (type.isEmpty() || type == u"n"_s) {
        // 1.2345678905E+11 (a barcode) as its digits; 12.990000000000002 as 12.99.
        bool ok = false;
        const double d = value.toDouble(&ok);
        if (!ok)
            return value;
        if (d == std::floor(d) && std::abs(d) < 1e15)
            return QString::number(qint64(d));
        return QString::number(d, 'g', 15);
    }
    return value;   // str, inlineStr
}

std::optional<QList<QStringList>> sheetRows(const QByteArray &xml, const QStringList &strings)
{
    constexpr qsizetype kMaxCells = 2'000'000;
    QList<QStringList> rows;
    qsizetype cells = 0;
    int col = 0;
    QString type, value;
    QXmlStreamReader x(xml);
    while (!x.atEnd()) {
        x.readNext();
        if (x.isStartElement()) {
            const QStringView n = x.name();
            if (n == u"row"_s) {
                const int r = x.attributes().value(u"r"_s).toInt();
                while (r > 0 && rows.size() < r - 1 && rows.size() < 1'048'576)
                    rows.append(QStringList());
                rows.append(QStringList());
                col = 0;
            } else if (n == u"c"_s) {
                if (const int c = column(x.attributes().value(u"r"_s)); c >= 0)
                    col = c;
                type = x.attributes().value(u"t"_s).toString();
                value.clear();
            } else if (n == u"v"_s) {
                value = x.readElementText();
            } else if (n == u"t"_s) {   // in an inline string
                value += x.readElementText();
            }
        } else if (x.isEndElement() && x.name() == u"c"_s && !rows.isEmpty()) {
            QStringList &row = rows.last();
            if (++cells > kMaxCells)
                return std::nullopt;
            while (row.size() < col)
                row.append(QString());
            const QString text = cellText(type, value, strings);
            if (row.size() == col)
                row.append(text);
            else
                row[col] = text;
            ++col;
        }
    }
    if (x.hasError())
        return std::nullopt;
    return rows;
}

} // namespace

std::optional<QList<Sheet>> readXlsx(const QByteArray &file, QString *error)
{
    const auto files = unzip(file, 64 * 1024 * 1024, error);
    if (!files)
        return std::nullopt;
    QHash<QString, QByteArray> byName;
    for (const auto &[name, data] : *files)
        byName.insert(name, data);
    // The workbook: where the package says, usually xl/workbook.xml.
    QString workbook = u"xl/workbook.xml"_s;
    {
        QXmlStreamReader x(byName.value(u"_rels/.rels"_s));
        while (!x.atEnd())
            if (x.readNext() == QXmlStreamReader::StartElement && x.name() == u"Relationship"_s
                && x.attributes().value(u"Type"_s).endsWith(u"/officeDocument"_s)) {
                const QString t = x.attributes().value(u"Target"_s).toString();
                workbook = QDir::cleanPath(t.startsWith(u'/') ? t.mid(1) : t);
            }
    }
    if (!byName.contains(workbook)) {
        say(error, tr("That isn't an Excel workbook (.xlsx)."));
        return std::nullopt;
    }
    const QString dir = QFileInfo(workbook).path();
    const QString relsFile = dir + u"/_rels/"_s + QFileInfo(workbook).fileName() + u".rels"_s;
    const QHash<QString, QString> rels = relationships(byName.value(relsFile), dir);
    QStringList strings;
    for (auto it = rels.begin(); it != rels.end(); ++it)
        if (it.value().endsWith(u"sharedStrings.xml"_s))
            strings = sharedStrings(byName.value(it.value()));
    if (strings.isEmpty() && byName.contains(dir + u"/sharedStrings.xml"_s))
        strings = sharedStrings(byName.value(dir + u"/sharedStrings.xml"_s));

    QList<Sheet> sheets;
    QXmlStreamReader x(byName.value(workbook));
    while (!x.atEnd()) {
        if (x.readNext() != QXmlStreamReader::StartElement || x.name() != u"sheet"_s)
            continue;
        QString id = x.attributes().value(QLatin1String(kRels), u"id"_s).toString();
        if (id.isEmpty())
            for (const QXmlStreamAttribute &a : x.attributes())
                if (a.name() == u"id"_s)
                    id = a.value().toString();
        const QString path = rels.value(id);
        if (!byName.contains(path))
            continue;   // a chart sheet, or one that's missing
        const auto rows = sheetRows(byName.value(path), strings);
        if (!rows) {
            say(error, tr("The file is damaged (or isn't a spreadsheet)."));
            return std::nullopt;
        }
        sheets.append({x.attributes().value(u"name"_s).toString(), *rows});
    }
    if (sheets.isEmpty()) {
        say(error, tr("That workbook has no sheets."));
        return std::nullopt;
    }
    return sheets;
}

} // namespace vt::app
