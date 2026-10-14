#pragma once

#include "print/document.hh"

#include <QByteArray>

#include <memory>

namespace vt::print {

// A picture file (PNG, JPEG...) as a black-and-white raster for a thermal
// printer: scaled to fit maxWidth x maxHeight dots (never enlarged past
// twice its size), transparent parts white, gray dithered (Floyd-Steinberg).
// Null if the bytes aren't a picture.
std::shared_ptr<const Raster> rasterize(const QByteArray &picture, int maxWidth, int maxHeight);

} // namespace vt::print
