#include "layout/menu_screens.hh"

using namespace Qt::StringLiterals;

namespace vt::layout {

namespace {

bool selfFilling(const Page &p)
{
    for (const Zone &z : p.zones)
        if (z.kind == u"menuGrid" || z.kind == u"menuCategories")
            return true;
    return false;
}

// A hand-placed category button: it goes to an item page with no
// self-filling menu (Everything and Popular go to self-filling ones).
bool categoryButton(const Layout &l, const Zone &z)
{
    if ((z.kind != u"button" && z.kind != u"image") || z.actions.isEmpty())
        return false;
    for (const Action &a : z.actions) {
        if (a.type() != u"jump")
            continue;
        const Page *target = l.page(a.str(u"page"));
        return target && target->kind == u"items" && !selfFilling(*target);
    }
    return false;
}

bool handBuilt(const Layout &l, const Page &p)
{
    if (p.kind != u"index" || selfFilling(p))
        return false;
    for (const Zone &z : p.zones)
        if (categoryButton(l, z))
            return true;
    return false;
}

QString freeZoneId(const Page &p, const QString &wanted)
{
    QString id = wanted;
    for (int n = 2; p.zone(id); ++n)
        id = wanted + u'-' + QString::number(n);
    return id;
}

} // namespace

bool hasHandBuiltMenu(const Layout &layout)
{
    for (const Page &p : layout.pages)
        if (handBuilt(layout, p))
            return true;
    return false;
}

Layout withSelfFillingMenu(const Layout &layout)
{
    Layout out = layout;
    QRect menuArea;
    QString menuTemplate;
    QSize menuCanvas;
    for (Page &p : out.pages) {
        if (!handBuilt(layout, p))
            continue;
        QRect area;
        QList<Zone> kept;
        for (const Zone &z : p.zones) {
            if (categoryButton(layout, z))
                area = area.isNull() ? z.rect : area.united(z.rect);
            else
                kept.append(z);
        }
        Zone panel;
        panel.id = freeZoneId(p, u"categories"_s);
        panel.kind = u"menuCategories"_s;
        panel.rect = area;
        panel.behavior = u"none"_s;
        // Columns as the buttons were (about square ones), 2 to 4.
        const int columns = std::clamp(int(std::lround(double(area.width()) / std::max(1, area.height()) * 1.5)), 2, 4);
        panel.props.insert(u"columns"_s, columns);
        if (!p.mealPeriod.isEmpty())
            panel.props.insert(u"period"_s, p.mealPeriod);
        kept.append(panel);
        p.zones = kept;
        if (menuArea.isNull()) {
            // The menu page's grid: where the buttons were, up to the page's title.
            menuArea = area;
            for (const Zone &z : p.zones)
                if (z.kind == u"label" && z.rect.bottom() <= area.top() && z.rect.left() <= area.right()
                    && z.rect.right() >= area.left())
                    menuArea = menuArea.united(z.rect);
            menuTemplate = p.templateId;
            menuCanvas = p.canvas;
        }
    }
    // An Everything page already (the whole menu, a button for each
    // category): it's the menu page.
    if (!menuArea.isNull() && !out.pageByRole(u"menu"_s)) {
        for (Page &p : out.pages) {
            const bool whole = p.role.isEmpty() && std::ranges::any_of(p.zones, [](const Zone &z) {
                return z.kind == u"menuGrid" && z.props.value(u"family").toString().isEmpty()
                       && !z.props.value(u"search").toBool() && !z.props.value(u"popular").toBool();
            });
            if (whole) {
                p.role = u"menu"_s;
                break;
            }
        }
    }
    if (!menuArea.isNull() && !out.pageByRole(u"menu"_s)) {
        Page menu;
        menu.id = u"menu-all"_s;
        for (int n = 2; out.page(menu.id); ++n)
            menu.id = u"menu-all-"_s + QString::number(n);
        menu.name = u"Menu"_s;
        menu.kind = u"items"_s;
        menu.role = u"menu"_s;
        menu.templateId = menuTemplate;
        menu.canvas = menuCanvas;
        Zone grid;
        grid.id = u"menu"_s;
        grid.kind = u"menuGrid"_s;
        grid.rect = menuArea;
        grid.behavior = u"none"_s;
        menu.zones.append(grid);
        out.pages.append(menu);
    }
    return out;
}

} // namespace vt::layout
