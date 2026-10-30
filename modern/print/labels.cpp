#include "print/labels.hh"

#include <algorithm>

namespace vt::print {

namespace {

constexpr int kDotsPerMm = 8;   // 203 dpi

// Text safe inside a label command: plain letters, and none of the
// characters that start or end one.
std::string field(const std::string &s, const std::string &forbidden)
{
    std::string out = plainLetters(s);
    for (char &c : out)
        if (forbidden.find(c) != std::string::npos)
            c = ' ';
    return out;
}

// Letters this tall (dots) fit the label: the title bigger, then the rest.
struct Sizes {
    int title;
    int line;
};

Sizes sizesFor(const Label &l, int heightDots)
{
    const int rows = 1 + int(l.lines.size());
    // The title counts for 1.6 lines; a little room above and below.
    const int line = std::clamp(int((heightDots - 24) / (1.6 + (rows - 1) * 1.15)), 16, 48);
    return {std::min(int(line * 1.6), 80), line};
}

} // namespace

std::string renderZpl(const std::vector<Label> &labels, int widthMm, int heightMm)
{
    const int w = widthMm * kDotsPerMm, h = heightMm * kDotsPerMm;
    std::string out;
    for (const Label &l : labels) {
        const Sizes s = sizesFor(l, h);
        out += "^XA^PW" + std::to_string(w) + "^LL" + std::to_string(h) + "^LH0,0";
        int y = 12;
        const auto text = [&](const std::string &t, int size, bool bold) {
            out += "^FO16," + std::to_string(y) + "^A0N," + std::to_string(size) + "," + std::to_string(bold ? size : size * 9 / 10)
                   + "^FB" + std::to_string(w - 32) + ",1,0,L,0^FD" + field(t, "^~") + "^FS";
            y += size + size / 6;
        };
        text(l.title, s.title, true);
        for (const std::string &line : l.lines)
            text(line, s.line, false);
        if (const std::string code = field(barcodeText(l.barcode), "^~"); !code.empty()) {
            // What room is left, at least 40 dots: EAN-13 / UPC-A, else Code 128.
            const int tall = std::max(40, h - y - 40);
            const bool digits = std::ranges::all_of(code, [](char c) { return c >= '0' && c <= '9'; });
            out += "^FO16," + std::to_string(y) + "^BY2";
            out += digits && code.size() == 13 ? "^BEN," : digits && code.size() == 12 ? "^BUN," : "^BCN,";
            out += std::to_string(tall) + ",Y,N^FD" + code + "^FS";
        }
        out += "^XZ\n";
    }
    return out;
}

std::string renderTspl(const std::vector<Label> &labels, int widthMm, int heightMm)
{
    const int h = heightMm * kDotsPerMm;
    std::string out = "SIZE " + std::to_string(widthMm) + " mm," + std::to_string(heightMm) + " mm\r\n"
                      "GAP 2 mm,0 mm\r\nDIRECTION 1\r\n";
    for (const Label &l : labels) {
        const Sizes s = sizesFor(l, h);
        out += "CLS\r\n";
        int y = 12;
        // The built-in font "3" is 16x24 dots; scaled by whole numbers.
        const auto text = [&](const std::string &t, int size) {
            const int scale = std::max(1, size / 24);
            out += "TEXT 16," + std::to_string(y) + ",\"3\",0," + std::to_string(scale) + "," + std::to_string(scale) + ",\""
                   + field(t, "\"\r\n") + "\"\r\n";
            y += 24 * scale + 6;
        };
        text(l.title, s.title);
        for (const std::string &line : l.lines)
            text(line, s.line);
        if (const std::string code = field(barcodeText(l.barcode), "\"\r\n"); !code.empty()) {
            const int tall = std::max(40, h - y - 40);
            const bool digits = std::ranges::all_of(code, [](char c) { return c >= '0' && c <= '9'; });
            const char *kind = digits && code.size() == 13 ? "EAN13" : digits && code.size() == 12 ? "UPCA" : "128";
            out += "BARCODE 16," + std::to_string(y) + ",\"" + kind + "\"," + std::to_string(tall) + ",1,0,2,2,\"" + code + "\"\r\n";
        }
        out += "PRINT 1,1\r\n";
    }
    return out;
}

Document labelSlip(const Label &label)
{
    Document d;
    d.center(label.title, true, true);
    for (const std::string &line : label.lines)
        d.center(line);
    if (!label.barcode.empty())
        d.barcode(label.barcode);
    d.cut = true;
    return d;
}

} // namespace vt::print
