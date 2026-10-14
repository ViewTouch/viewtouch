#include "menuprint.hh"

#include <QFontMetricsF>
#include <QPainter>
#include <QPdfWriter>
#include <QTextOption>

#include <functional>
#include <vector>

using namespace Qt::StringLiterals;

namespace {

// Laid out in points (72 an inch); the painter scales to its device.
constexpr qreal kMargin = 40;
constexpr qreal kGutter = 28;
constexpr qreal kPicture = 54;

QFont font(qreal size, bool bold = false, bool italic = false)
{
    QFont f(u"DejaVu Sans"_s);
    f.setPixelSize(int(size));
    f.setBold(bold);
    f.setItalic(italic);
    return f;
}

const QFont &titleFont() { static const QFont f = font(28, true); return f; }
const QFont &subtitleFont() { static const QFont f = font(11); return f; }
const QFont &headingFont() { static const QFont f = font(17, true); return f; }
const QFont &sectionFont() { static const QFont f = font(10, true); return f; }
const QFont &nameFont() { static const QFont f = font(11, true); return f; }
const QFont &sizesFont() { static const QFont f = font(9.5); return f; }
const QFont &descriptionFont() { static const QFont f = font(9, false, true); return f; }

qreal wrappedHeight(const QFont &f, const QString &text, qreal width)
{
    if (text.isEmpty())
        return 0;
    return QFontMetricsF(f).boundingRect(QRectF(0, 0, width, 1e6), Qt::TextWordWrap, text).height();
}

// A heading on a light page reads only when it's dark enough.
QColor headingColor(const QColor &c)
{
    if (!c.isValid())
        return QColor(u"#1f2430"_s);
    return 0.299 * c.redF() + 0.587 * c.greenF() + 0.114 * c.blueF() > 0.6 ? c.darker(220) : c;
}

// The picture as a square, cut from its middle, sharp enough for print.
QImage squared(const QImage &img)
{
    const int side = std::min(img.width(), img.height());
    const QImage cut = img.copy((img.width() - side) / 2, (img.height() - side) / 2, side, side);
    return side > 360 ? cut.scaled(360, 360, Qt::IgnoreAspectRatio, Qt::SmoothTransformation) : cut;
}

struct Block {
    qreal height = 0;
    bool keepWithNext = false;   // a heading: not alone at the bottom of a column
    std::function<void(QPainter &, qreal x, qreal y, qreal w)> draw;
};

// Lays the menu out on pages of `pageSize` (points); `paint` is null to only
// count the pages. `newPage` is called between pages; false stops.
int layOut(const MenuPrint &menu, QSizeF pageSize, QPainter *paint, const std::function<bool()> &newPage)
{
    const int columns = std::clamp(menu.columns, 1, 3);
    const qreal fullW = pageSize.width() - 2 * kMargin;
    const qreal colW = (fullW - kGutter * (columns - 1)) / columns;
    const qreal bottom = pageSize.height() - kMargin;

    std::vector<Block> blocks;
    for (const MenuPrintCategory &cat : menu.categories) {
        if (cat.items.isEmpty())
            continue;
        const QColor ink = headingColor(cat.color);
        const qreal headH = QFontMetricsF(headingFont()).height() + 12;
        blocks.push_back({headH + 6, true, [cat, ink, headH](QPainter &p, qreal x, qreal y, qreal w) {
            p.setFont(headingFont());
            p.setPen(ink);
            p.drawText(QRectF(x, y + 6, w, headH - 8), Qt::AlignLeft | Qt::AlignBottom, cat.name);
            p.setPen(QPen(ink, 1.2));
            p.drawLine(QPointF(x, y + headH + 1), QPointF(x + w, y + headH + 1));
        }});
        QString section;
        for (const MenuPrintItem &item : cat.items) {
            if (!item.section.isEmpty() && item.section != section) {
                section = item.section;
                const qreal h = QFontMetricsF(sectionFont()).height() + 10;
                blocks.push_back({h, true, [section, h](QPainter &p, qreal x, qreal y, qreal w) {
                    p.setFont(sectionFont());
                    p.setPen(QColor(u"#5a6270"_s));
                    p.drawText(QRectF(x, y, w, h - 2), Qt::AlignLeft | Qt::AlignBottom, section.toUpper());
                }});
            }
            const bool picture = menu.pictures && !item.picture.isNull();
            const qreal textX = picture ? kPicture + 10 : 0;
            const qreal textW = colW - textX;
            const QFontMetricsF nameM(nameFont());
            const qreal priceW = item.price.isEmpty() ? 0 : nameM.horizontalAdvance(item.price) + 10;
            const qreal nameH = std::max(nameM.height(), wrappedHeight(nameFont(), item.name, textW - priceW));
            const QString sizes = item.sizes.join(u"  ·  "_s);
            const qreal sizesH = wrappedHeight(sizesFont(), sizes, textW);
            const QString about = menu.descriptions ? item.description : QString();
            const qreal aboutH = wrappedHeight(descriptionFont(), about, textW);
            const qreal textH = nameH + (sizesH ? sizesH + 2 : 0) + (aboutH ? aboutH + 2 : 0);
            const QImage pic = picture ? squared(item.picture) : QImage();
            blocks.push_back({std::max(textH, picture ? kPicture : 0.0) + 10, false,
                              [item, pic, textX, textW, priceW, nameH, sizes, sizesH, about](QPainter &p, qreal x, qreal y, qreal) {
                if (!pic.isNull())
                    p.drawImage(QRectF(x, y + 2, kPicture, kPicture), pic);
                qreal ty = y;
                p.setPen(QColor(u"#14171c"_s));
                p.setFont(nameFont());
                p.drawText(QRectF(x + textX, ty, textW - priceW, nameH), Qt::TextWordWrap, item.name);
                if (!item.price.isEmpty())
                    p.drawText(QRectF(x + textX, ty, textW, QFontMetricsF(nameFont()).height()),
                               Qt::AlignRight | Qt::AlignTop, item.price);
                ty += nameH + 2;
                if (!sizes.isEmpty()) {
                    p.setFont(sizesFont());
                    p.drawText(QRectF(x + textX, ty, textW, sizesH), Qt::TextWordWrap, sizes);
                    ty += sizesH + 2;
                }
                if (!about.isEmpty()) {
                    p.setFont(descriptionFont());
                    p.setPen(QColor(u"#4a5260"_s));
                    p.drawText(QRectF(x + textX, ty, textW, 1e5), Qt::TextWordWrap, about);
                }
            }});
        }
    }

    int pages = 1;
    qreal top = kMargin;
    // The title, across the top of the first page.
    if (!menu.title.isEmpty() || !menu.subtitle.isEmpty()) {
        const qreal titleH = wrappedHeight(titleFont(), menu.title, fullW);
        const qreal subH = wrappedHeight(subtitleFont(), menu.subtitle, fullW);
        if (paint) {
            paint->setPen(QColor(u"#14171c"_s));
            paint->setFont(titleFont());
            paint->drawText(QRectF(kMargin, top, fullW, titleH), Qt::AlignHCenter | Qt::TextWordWrap, menu.title);
            paint->setFont(subtitleFont());
            paint->setPen(QColor(u"#4a5260"_s));
            paint->drawText(QRectF(kMargin, top + titleH + 4, fullW, subH), Qt::AlignHCenter | Qt::TextWordWrap, menu.subtitle);
        }
        top += titleH + (subH ? subH + 4 : 0) + 18;
    }

    int col = 0;
    qreal y = top;
    const auto nextColumn = [&]() -> bool {
        if (++col < columns) {
            y = top;
            return true;
        }
        col = 0;
        top = kMargin;
        y = top;
        ++pages;
        return newPage();
    };
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const Block &b = blocks[i];
        // A heading takes the next item with it.
        qreal need = b.height;
        for (std::size_t k = i; k + 1 < blocks.size() && blocks[k].keepWithNext; ++k)
            need += blocks[k + 1].height;
        const bool atTop = y <= top + 0.5;
        if (y + need > bottom && !atTop && !nextColumn())
            return pages;
        if (paint)
            b.draw(*paint, kMargin + col * (colW + kGutter), y, colW);
        y += b.height;
    }
    return pages;
}

QSizeF pagePoints(const QPageSize &size)
{
    return size.size(QPageSize::Point);
}

} // namespace

bool exportMenuPdf(const MenuPrint &menu, const QString &file, QString *error)
{
    QPdfWriter pdf(file);
    pdf.setPageSize(menu.page);
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
    pdf.setResolution(300);
    pdf.setTitle(menu.title);
    pdf.setCreator(u"ViewTouch"_s);
    QPainter p;
    if (!p.begin(&pdf)) {
        if (error)
            *error = u"Cannot write %1"_s.arg(file);
        return false;
    }
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal scale = 300.0 / 72.0;
    p.scale(scale, scale);
    layOut(menu, pagePoints(menu.page), &p, [&] { return pdf.newPage(); });
    if (!p.end()) {
        if (error)
            *error = u"Cannot write %1"_s.arg(file);
        return false;
    }
    return true;
}

QImage menuPreview(const MenuPrint &menu, int width)
{
    const QSizeF page = pagePoints(menu.page);
    const qreal scale = width / page.width();
    QImage img(width, int(page.height() * scale), QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.scale(scale, scale);
    layOut(menu, page, &p, [] { return false; });   // the first page only
    return img;
}

int menuPageCount(const MenuPrint &menu)
{
    return layOut(menu, pagePoints(menu.page), nullptr, [] { return true; });
}
