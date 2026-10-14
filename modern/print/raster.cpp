#include "print/raster.hh"

#include <QImage>

#include <algorithm>
#include <vector>

namespace vt::print {

std::shared_ptr<const Raster> rasterize(const QByteArray &picture, int maxWidth, int maxHeight)
{
    QImage image = QImage::fromData(picture);
    if (image.isNull() || maxWidth <= 0 || maxHeight <= 0)
        return nullptr;
    // Transparent parts are paper: flatten onto white.
    QImage flat(image.size(), QImage::Format_RGB32);
    flat.fill(Qt::white);
    {
        const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < argb.height(); ++y) {
            const QRgb *in = reinterpret_cast<const QRgb *>(argb.constScanLine(y));
            QRgb *out = reinterpret_cast<QRgb *>(flat.scanLine(y));
            for (int x = 0; x < argb.width(); ++x) {
                const int a = qAlpha(in[x]);
                const auto mix = [a](int c) { return (c * a + 255 * (255 - a)) / 255; };
                out[x] = qRgb(mix(qRed(in[x])), mix(qGreen(in[x])), mix(qBlue(in[x])));
            }
        }
    }
    const QSize fit = flat.size().scaled(std::min(maxWidth, flat.width() * 2), std::min(maxHeight, flat.height() * 2),
                                         Qt::KeepAspectRatio);
    const QImage gray = flat.scaled(fit, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                            .convertToFormat(QImage::Format_Grayscale8);

    auto r = std::make_shared<Raster>();
    r->width = gray.width();
    r->height = gray.height();
    r->bits.assign(std::size_t(r->rowBytes()) * std::size_t(r->height), 0);
    // Floyd-Steinberg: each dot's error spread to the ones not yet decided.
    std::vector<float> row(std::size_t(r->width) + 2, 0.f), next(std::size_t(r->width) + 2, 0.f);
    for (int y = 0; y < r->height; ++y) {
        const uchar *line = gray.constScanLine(y);
        for (int x = 0; x < r->width; ++x)
            row[std::size_t(x) + 1] += float(line[x]);
        for (int x = 0; x < r->width; ++x) {
            const float v = row[std::size_t(x) + 1];
            const bool black = v < 128.f;
            if (black)
                r->bits[std::size_t(y * r->rowBytes() + x / 8)] |= static_cast<unsigned char>(0x80 >> (x % 8));
            const float err = v - (black ? 0.f : 255.f);
            row[std::size_t(x) + 2] += err * 7.f / 16.f;
            next[std::size_t(x)] += err * 3.f / 16.f;
            next[std::size_t(x) + 1] += err * 5.f / 16.f;
            next[std::size_t(x) + 2] += err * 1.f / 16.f;
        }
        std::swap(row, next);
        std::fill(next.begin(), next.end(), 0.f);
    }
    return r;
}

} // namespace vt::print
