#pragma once

#include <QColor>
#include <QImage>
#include <QList>
#include <QPageSize>
#include <QString>
#include <QStringList>

// A menu to print or hand out: the Menu Builder's categories and items laid
// out on pages (one or two columns), with prices, sizes, descriptions and
// pictures. Made fresh each time, so it has today's prices.
struct MenuPrintItem {
    QString name;
    QString description;
    QString price;          // "$9.50"; empty with sizes
    QStringList sizes;      // "Small $8.00", "Large $11.00"
    QString section;        // a heading over it (and those after it), from the Menu Builder
    QImage picture;
};

struct MenuPrintCategory {
    QString name;
    QColor color;           // its heading (too light: dark gray)
    QList<MenuPrintItem> items;
};

struct MenuPrint {
    QString title;
    QString subtitle;       // the address, the hours...
    QList<MenuPrintCategory> categories;
    int columns = 2;
    bool pictures = true;
    bool descriptions = true;
    QPageSize page = QPageSize(QPageSize::Letter);
};

// Saves it as a PDF at `file`; false with `error` set.
bool exportMenuPdf(const MenuPrint &menu, const QString &file, QString *error = nullptr);
// The first page as a picture `width` pixels wide (to look at before saving).
QImage menuPreview(const MenuPrint &menu, int width);
// How many pages it takes.
int menuPageCount(const MenuPrint &menu);
