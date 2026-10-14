#pragma once

#include "layout/page.hh"

#include <QColor>
#include <QImage>
#include <QList>
#include <QString>

namespace vt::app {

// A ready-made look: the colors a theme is recolored with. Fonts, sizes,
// frames and the pages' own colors stay as they are.
struct Look {
    QString id;
    QString name;
    QColor background;   // behind the pages
    QColor surface;      // buttons
    QColor panel;        // panels (the check, the keypad...)
    QColor text;
    QColor accent;       // pressed, chosen, lit
};

QList<Look> builtInLooks();
// The main colors of a picture (a logo), most used first: transparent,
// near-white, near-black and gray parts left out.
QList<QColor> mainColors(const QImage &picture, int count = 3);
// A look in these colors: dark (light text on deep shades of them) or light.
Look lookFromColors(const QList<QColor> &colors, bool dark);
// Text that reads on this color: near-black or white.
QColor textOn(const QColor &c);
// The theme recolored with the look.
void applyLook(layout::Theme &theme, const Look &look);

} // namespace vt::app
