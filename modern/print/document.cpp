#include "print/document.hh"

#include <QChar>
#include <QList>
#include <QString>

#include <algorithm>
#include <array>

namespace vt::print {

Document &Document::text(std::string s, Align a, bool bold, bool big)
{
    lines.push_back({Kind::Text, std::move(s), {}, a, bold, big});
    return *this;
}

Document &Document::columns(std::string left, std::string right, bool bold, bool big)
{
    lines.push_back({Kind::Columns, std::move(left), std::move(right), Align::Left, bold, big});
    return *this;
}

Document &Document::rule()
{
    lines.push_back({Kind::Rule});
    return *this;
}

Document &Document::blank()
{
    lines.push_back({Kind::Blank});
    return *this;
}

Document &Document::image(std::shared_ptr<const Raster> raster)
{
    if (raster && raster->width > 0 && raster->height > 0) {
        Line l;
        l.kind = Kind::Image;
        l.raster = std::move(raster);
        lines.push_back(std::move(l));
    }
    return *this;
}

namespace {

bool isContinuation(unsigned char c) { return (c & 0xC0) == 0x80; }

// First `n` code points of s.
std::string takeChars(const std::string &s, std::size_t n)
{
    std::size_t i = 0, count = 0;
    while (i < s.size() && count < n) {
        ++i;
        while (i < s.size() && isContinuation(static_cast<unsigned char>(s[i])))
            ++i;
        ++count;
    }
    return s.substr(0, i);
}

std::string dropChars(const std::string &s, std::size_t n)
{
    return s.substr(takeChars(s, n).size());
}

// Word-wrap to `width` characters.
std::vector<std::string> wrap(const std::string &s, std::size_t width)
{
    std::vector<std::string> out;
    std::string rest = s;
    while (displayWidth(rest) > width) {
        std::string head = takeChars(rest, width);
        // At the last space, unless only an indent is before it (a long
        // reference): then the word is split.
        const std::size_t space = head.rfind(' ');
        if (space != std::string::npos && head.find_first_not_of(' ') < space)
            head = head.substr(0, space);
        out.push_back(head);
        rest = rest.substr(head.size());
        while (!rest.empty() && rest.front() == ' ')
            rest.erase(0, 1);
    }
    out.push_back(rest);
    return out;
}

std::string pad(std::size_t n) { return std::string(n, ' '); }

// PC858 (Epson ESC t 19), bytes 0x80..0xFF: Latin-1 letters, box drawing, €.
constexpr std::array<char32_t, 128> kPc858 = {
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
    0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
    0x00FF, 0x00D6, 0x00DC, 0x00F8, 0x00A3, 0x00D8, 0x00D7, 0x0192,
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
    0x00BF, 0x00AE, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x00C1, 0x00C2, 0x00C0,
    0x00A9, 0x2563, 0x2551, 0x2557, 0x255D, 0x00A2, 0x00A5, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x00E3, 0x00C3,
    0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x00A4,
    0x00F0, 0x00D0, 0x00CA, 0x00CB, 0x00C8, 0x20AC, 0x00CD, 0x00CE,
    0x00CF, 0x2518, 0x250C, 0x2588, 0x2584, 0x00A6, 0x00CC, 0x2580,
    0x00D3, 0x00DF, 0x00D4, 0x00D2, 0x00F5, 0x00D5, 0x00B5, 0x00FE,
    0x00DE, 0x00DA, 0x00DB, 0x00D9, 0x00FD, 0x00DD, 0x00AF, 0x00B4,
    0x00AD, 0x00B1, 0x2017, 0x00BE, 0x00B6, 0x00A7, 0x00F7, 0x00B8,
    0x00B0, 0x00A8, 0x00B7, 0x00B9, 0x00B3, 0x00B2, 0x25A0, 0x00A0,
};

std::string utf8(char32_t cp)
{
    return QString::fromUcs4(&cp, 1).toStdString();
}

// Text a printer can show, one column a character: composed letters,
// breaks and control characters as spaces or gone, typographic marks plain;
// with `accents` what PC858 has stays, else plain letters.
std::string printable(const std::string &s, bool accents)
{
    static const char *latin1 =
        "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTs"   // U+00C0..U+00DF
        "aaaaaaaceeeeiiiidnooooo/ouuuuyty";  // U+00E0..U+00FF
    std::string out;
    for (const char32_t cp : QString::fromStdString(s).normalized(QString::NormalizationForm_C).toUcs4()) {
        if (QChar::isSpace(cp))
            out += ' ';   // every kind: the time's "11:23 PM" has a narrow no-break space
        else if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0))
            continue;   // control characters: they'd be printer commands
        else if (cp < 0x80)
            out += char(cp);
        else if ((cp >= 0x300 && cp < 0x370) || (cp >= 0x200B && cp <= 0x200F) || (cp >= 0xFE00 && cp <= 0xFE0F)
                 || cp >= 0x1F000)
            continue;   // leftover accents, joiners, emoji
        else if (cp == 0x2018 || cp == 0x2019 || cp == 0x201A || cp == 0x2032)
            out += '\'';
        else if (cp == 0x201C || cp == 0x201D || cp == 0x201E || cp == 0x2033)
            out += '"';
        else if (cp == 0x2010 || cp == 0x2011 || cp == 0x2013 || cp == 0x2014 || cp == 0x2212)
            out += '-';
        else if (cp == 0x2026)
            out += "...";
        else if (cp == 0x2022 || cp == 0x25CF)
            out += '*';
        else if (cp == 0x2192)
            out += "->";
        else if (cp == 0x2190)
            out += "<-";
        else if (cp == 0x2122)
            out += "TM";
        else if (accents && std::ranges::find(kPc858, cp) != kPc858.end())
            out += utf8(cp);
        else if (cp >= 0xC0 && cp <= 0xFF)
            out += latin1[cp - 0xC0];
        else if (cp == 0x20AC)
            out += "EUR";
        else if (cp == 0xA3)
            out += "GBP";
        else if (cp == 0xA5)
            out += "JPY";
        else if (cp == 0xA2)
            out += 'c';
        else if (cp == 0xB7)
            out += '*';
        else if (cp == 0xBF || cp == 0xA1)
            continue;   // ¿ ¡: the closing mark says it
        else if (cp == 0xBD)
            out += "1/2";
        else if (cp == 0xBC)
            out += "1/4";
        else if (cp == 0xBE)
            out += "3/4";
        else if (cp == 0xB0)
            out += " deg";
        else if (cp == 0xAE)
            out += "(R)";
        else if (cp == 0xA9)
            out += "(C)";
        else if (cp == 0xD7)
            out += 'x';
        else if (const auto cat = QChar::category(cp); cat >= QChar::Symbol_Math && cat <= QChar::Symbol_Other)
            continue;   // ☎ ★ ♥: decoration
        else
            out += '?';   // a letter the printer hasn't got
    }
    return out;
}

// A row (already printable) as the printer's bytes.
std::string encode(const std::string &row, bool accents)
{
    if (!accents)
        return row;
    std::string out;
    for (const char32_t cp : QString::fromStdString(row).toUcs4()) {
        if (cp < 0x80) {
            out += char(cp);
        } else if (const auto it = std::ranges::find(kPc858, cp); it != kPc858.end()) {
            out += char(0x80 + (it - kPc858.begin()));
        } else {
            out += '?';
        }
    }
    return out;
}

std::vector<std::string> splitBreaks(const std::string &s)
{
    std::vector<std::string> out;
    std::size_t from = 0;
    for (std::size_t at; (at = s.find('\n', from)) != std::string::npos; from = at + 1)
        out.push_back(s.substr(from, at - from));
    out.push_back(s.substr(from));
    return out;
}

std::string aligned(const std::string &part, std::size_t width, Document::Align align)
{
    const std::size_t w = displayWidth(part);
    if (align == Document::Align::Center)
        return pad((width - std::min(w, width)) / 2) + part;
    if (align == Document::Align::Right)
        return pad(width - std::min(w, width)) + part;
    return part;
}

// Plain-text rows for one line at a given character width; every row fits.
std::vector<std::string> layoutLine(const Document::Line &l, std::size_t width, bool accents)
{
    std::vector<std::string> rows;
    switch (l.kind) {
    case Document::Kind::Image:   // not text
        break;
    case Document::Kind::Blank:
        rows.emplace_back();
        break;
    case Document::Kind::Rule:
        rows.push_back(std::string(width, '-'));
        break;
    case Document::Kind::Text:
        // A typed line break (an address, a note) starts a new row.
        for (const std::string &paragraph : splitBreaks(l.text))
            for (const std::string &part : wrap(printable(paragraph, accents), width))
                rows.push_back(aligned(part, width, l.align));
        break;
    case Document::Kind::Columns: {
        const std::string left = printable(l.text, accents);
        const std::string right = printable(l.right, accents);
        const std::size_t lw = displayWidth(left), rw = displayWidth(right);
        // Side by side when the left keeps a sensible width (or all of it).
        if (rw + 1 + std::min<std::size_t>(lw, 10) <= width) {
            const std::size_t room = width - rw - 1;
            std::vector<std::string> parts = wrap(left, room);
            for (std::size_t i = 0; i + 1 < parts.size(); ++i)
                rows.push_back(parts[i]);
            const std::size_t last = displayWidth(parts.back());
            rows.push_back(parts.back() + pad(width - std::min(width, last + rw)) + right);
        } else {
            // Too wide (narrow paper, many report columns): the left on its
            // own rows, the right under it, against the right edge.
            if (!left.empty())
                for (const std::string &part : wrap(left, width))
                    rows.push_back(part);
            for (const std::string &part : wrap(right, width))
                rows.push_back(aligned(part, width, Document::Align::Right));
        }
        break;
    }
    }
    return rows;
}

} // namespace

std::size_t displayWidth(const std::string &s)
{
    return std::size_t(std::ranges::count_if(s, [](char c) { return !isContinuation(static_cast<unsigned char>(c)); }));
}

std::string plainLetters(const std::string &s)
{
    return printable(s, false);
}

std::string renderText(const Document &doc, int width)
{
    std::string out;
    for (const Document::Line &l : doc.lines) {
        if (l.kind == Document::Kind::Image)   // pictures are for thermal printers
            continue;
        // Double-width text takes two columns per character.
        const std::size_t w = std::size_t(std::max(8, l.big ? width / 2 : width));
        for (const std::string &row : layoutLine(l, w, true))
            out += row + "\n";
    }
    if (doc.kickDrawer)
        out += "[drawer opened]\n";
    if (doc.cut)
        out += std::string(std::size_t(width), '=') + "\n";
    return out;
}

std::string renderEscPos(const Document &doc, int width, bool accents)
{
    const std::string ESC = "\x1b", GS = "\x1d";
    std::string out = ESC + "@";                     // initialize
    if (accents)
        out += ESC + "t" + std::string(1, '\x13');   // character set PC858
    for (const Document::Line &l : doc.lines) {
        if (l.kind == Document::Kind::Image) {
            // GS v 0: a raster picture, centered; in bands so small printer
            // buffers keep up.
            const Raster &r = *l.raster;
            const int bytes = r.rowBytes();
            out += ESC + "a" + std::string(1, '\x01');
            constexpr int kBand = 128;
            for (int y = 0; y < r.height; y += kBand) {
                const int rows = std::min(kBand, r.height - y);
                out += GS + "v0" + std::string(1, '\0');
                out += char(bytes & 0xff);
                out += char((bytes >> 8) & 0xff);
                out += char(rows & 0xff);
                out += char((rows >> 8) & 0xff);
                out.append(reinterpret_cast<const char *>(r.bits.data()) + std::size_t(y) * std::size_t(bytes),
                           std::size_t(rows) * std::size_t(bytes));
            }
            out += ESC + "a" + std::string(1, '\0');
            continue;
        }
        const std::size_t w = std::size_t(std::max(8, l.big ? width / 2 : width));
        out += GS + "!" + (l.big ? std::string("\x11") : std::string(1, '\0'));   // character size
        out += ESC + "E" + (l.bold ? std::string("\x01") : std::string(1, '\0')); // emphasis
        // Layout is done here (padding), so the printer stays left-aligned.
        out += ESC + "a" + std::string(1, '\0');
        for (const std::string &row : layoutLine(l, w, accents))
            out += encode(row, accents) + "\n";
    }
    out += GS + "!" + std::string(1, '\0') + ESC + "E" + std::string(1, '\0');
    if (doc.kickDrawer)
        out += ESC + "p" + std::string(1, '\0') + "\x19\xfa";   // pulse pin 2
    if (doc.cut)
        out += "\n\n\n" + GS + "V" + "\x42" + std::string(1, '\0');   // feed and partial cut
    return out;
}

} // namespace vt::print
