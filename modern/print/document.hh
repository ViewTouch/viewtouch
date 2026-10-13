#pragma once

#include <memory>
#include <string>
#include <vector>

namespace vt::print {

// A printer-independent ticket: what to print, not how. Rendered to plain
// text (files, previews, tests) or to ESC/POS bytes for thermal printers.
// A black-and-white picture for a thermal printer (the store logo): rows of
// (width + 7) / 8 bytes, the leftmost dot in the high bit, 1 = black.
struct Raster {
    int width = 0;    // dots
    int height = 0;   // dots
    std::vector<unsigned char> bits;

    int rowBytes() const { return (width + 7) / 8; }
    bool dot(int x, int y) const { return bits[std::size_t(y * rowBytes() + x / 8)] & (0x80 >> (x % 8)); }
};

struct Document {
    enum class Align { Left, Center, Right };
    enum class Kind { Text, Columns, Rule, Blank, Image };

    struct Line {
        Kind kind = Kind::Text;
        std::string text;      // Text; left side of Columns
        std::string right;     // Columns
        Align align = Align::Left;
        bool bold = false;
        bool big = false;      // double width and height
        std::shared_ptr<const Raster> raster;   // Image: printed centered (plain text skips it)
    };

    std::vector<Line> lines;
    bool cut = true;
    bool kickDrawer = false;

    Document &text(std::string s, Align a = Align::Left, bool bold = false, bool big = false);
    Document &center(std::string s, bool bold = false, bool big = false) { return text(std::move(s), Align::Center, bold, big); }
    Document &columns(std::string left, std::string right, bool bold = false, bool big = false);
    Document &rule();
    Document &blank();
    Document &image(std::shared_ptr<const Raster> raster);
};

// Plain letters a printer without accents can show (é -> e, € -> EUR, no control characters).
std::string plainLetters(const std::string &s);

// Number of characters (UTF-8 code points) in s.
std::size_t displayWidth(const std::string &s);

// Every row fits `width` (a line too wide for its columns is split onto
// rows of its own); line breaks and control characters in the text don't
// reach the printer; typographic marks become plain ones.
std::string renderText(const Document &doc, int width);
// ESC/POS for Epson-compatible printers. `accents`: the PC858 character set
// (accented letters, ñ ¿ ¡, £ €); else plain letters (é -> e, € -> EUR) for
// printers without it.
std::string renderEscPos(const Document &doc, int width, bool accents = true);

} // namespace vt::print
