#pragma once

#include "print/document.hh"

#include <string>
#include <vector>

namespace vt::print {

// A sticky label (a prep label: what, made, use by): a big first line, then
// smaller ones. Label printers get it in their own language: ZPL (Zebra and
// the many that speak it) or TSPL (TSC, Xprinter, Munbyn, Rollo and most
// low-cost ones); a receipt printer gets it as a short slip.
struct Label {
    std::string title;
    std::vector<std::string> lines;
    std::string barcode;   // under the lines (a price tag), drawn by the printer; empty: none
};

// `widthMm` x `heightMm` labels at 203 dots per inch (8 dots a mm), one after
// another.
std::string renderZpl(const std::vector<Label> &labels, int widthMm, int heightMm);
std::string renderTspl(const std::vector<Label> &labels, int widthMm, int heightMm);
// For a receipt printer: the label as a slip, cut after it.
Document labelSlip(const Label &label);

} // namespace vt::print
