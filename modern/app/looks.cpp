#include "app/looks.hh"

#include <QJsonObject>

#include <algorithm>
#include <map>

using namespace Qt::StringLiterals;

namespace vt::app {

QList<Look> builtInLooks()
{
    return {
        {u"dark"_s, u"ViewTouch Dark"_s, QColor(u"#171a1f"_s), QColor(u"#2d3440"_s), QColor(u"#232933"_s),
         QColor(u"#f2f4f7"_s), QColor(u"#2f6fd6"_s)},
        {u"light"_s, u"Daylight"_s, QColor(u"#e9edf2"_s), QColor(u"#ffffff"_s), QColor(u"#f7f8fa"_s),
         QColor(u"#1d2430"_s), QColor(u"#2f6fd6"_s)},
        {u"contrast"_s, u"High Contrast"_s, QColor(u"#000000"_s), QColor(u"#1c1c1c"_s), QColor(u"#000000"_s),
         QColor(u"#ffffff"_s), QColor(u"#ffd400"_s)},
        {u"cafe"_s, u"Warm Café"_s, QColor(u"#2b1d14"_s), QColor(u"#4a3426"_s), QColor(u"#36261b"_s),
         QColor(u"#fbefe2"_s), QColor(u"#d9822b"_s)},
        {u"ocean"_s, u"Ocean"_s, QColor(u"#0f1f2e"_s), QColor(u"#1d3a52"_s), QColor(u"#162c40"_s),
         QColor(u"#e8f3fb"_s), QColor(u"#1fa3c4"_s)},
        {u"forest"_s, u"Forest"_s, QColor(u"#142018"_s), QColor(u"#24382a"_s), QColor(u"#1b2b20"_s),
         QColor(u"#ecf5ee"_s), QColor(u"#3fa55b"_s)},
        {u"berry"_s, u"Berry"_s, QColor(u"#241526"_s), QColor(u"#3e2541"_s), QColor(u"#301c33"_s),
         QColor(u"#f7eaf8"_s), QColor(u"#c2417e"_s)},
    };
}

QColor textOn(const QColor &c)
{
    return 0.299 * c.redF() + 0.587 * c.greenF() + 0.114 * c.blueF() > 0.6 ? QColor(u"#1b1b1b"_s) : QColor(Qt::white);
}

QList<QColor> mainColors(const QImage &picture, int count)
{
    QList<QColor> out;
    if (picture.isNull())
        return out;
    // Counted in coarse buckets (on a small copy), colorful dots weighing most.
    const QImage small = picture.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                             .convertToFormat(QImage::Format_ARGB32);
    struct Bucket { double weight = 0, r = 0, g = 0, b = 0; };
    std::map<int, Bucket> buckets;
    for (int y = 0; y < small.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(small.constScanLine(y));
        for (int x = 0; x < small.width(); ++x) {
            const QColor c = QColor::fromRgba(line[x]);
            if (c.alpha() < 128)
                continue;
            const float s = c.hsvSaturationF(), v = c.valueF();
            if (s < 0.2f || v < 0.15f)   // grays, white, black: not the brand's color
                continue;
            const int key = (c.red() / 32) * 64 + (c.green() / 32) * 8 + c.blue() / 32;
            Bucket &b = buckets[key];
            const double w = 0.5 + s;
            b.weight += w;
            b.r += c.red() * w;
            b.g += c.green() * w;
            b.b += c.blue() * w;
        }
    }
    std::vector<Bucket> sorted;
    for (const auto &[k, b] : buckets)
        sorted.push_back(b);
    std::ranges::sort(sorted, [](const Bucket &a, const Bucket &b) { return a.weight > b.weight; });
    for (const Bucket &b : sorted) {
        const QColor c(int(b.r / b.weight), int(b.g / b.weight), int(b.b / b.weight));
        // Distinct hues only: not another shade of one already picked.
        const bool near = std::ranges::any_of(out, [&](const QColor &o) {
            const int dh = std::abs(o.hsvHue() - c.hsvHue());
            return std::min(dh, 360 - dh) < 25;
        });
        if (!near)
            out << c;
        if (out.size() >= count)
            break;
    }
    return out;
}

Look lookFromColors(const QList<QColor> &colors, bool dark)
{
    const QColor main = colors.value(0, QColor(u"#2f6fd6"_s));
    const QColor second = colors.value(1, main);
    const auto shade = [](const QColor &c, double sat, double light) {
        return QColor::fromHslF(std::max(0.f, c.hslHueF()), float(sat), float(light));
    };
    Look l;
    l.id = dark ? u"logo-dark"_s : u"logo-light"_s;
    l.name = dark ? u"From the logo (dark)"_s : u"From the logo (light)"_s;
    // The accent: the logo's own color, bright enough to read as "on".
    l.accent = shade(main, std::clamp(main.hslSaturationF(), 0.45f, 0.85f), dark ? 0.5 : 0.42);
    if (dark) {
        l.background = shade(second, 0.25, 0.08);
        l.surface = shade(second, 0.22, 0.22);
        l.panel = shade(second, 0.22, 0.15);
        l.text = shade(second, 0.25, 0.95);
    } else {
        l.background = shade(second, 0.25, 0.92);
        l.surface = QColor(Qt::white);
        l.panel = shade(second, 0.2, 0.97);
        l.text = shade(second, 0.3, 0.14);
    }
    return l;
}

void applyLook(layout::Theme &theme, const Look &look)
{
    const auto hex = [](const QColor &c) { return c.name(QColor::HexRgb); };
    const auto set = [](QJsonObject &o, const QString &key, const QJsonValue &v) { o.insert(key, v); };
    theme.name = look.name;
    set(theme.background, u"fill"_s, hex(look.background));
    theme.background.remove(u"texture"_s);

    set(theme.style.normal, u"fill"_s, hex(look.surface));
    set(theme.style.normal, u"textColor"_s, hex(look.text));
    set(theme.style.normal, u"borderColor"_s, hex(look.surface.darker(140)));
    set(theme.style.selected, u"fill"_s, hex(look.accent));
    set(theme.style.selected, u"textColor"_s, hex(textOn(look.accent)));
    // Panels' own buttons.
    set(theme.style.normal, u"keyFill"_s, hex(look.surface));
    set(theme.style.normal, u"keyTextColor"_s, hex(look.text));
    set(theme.style.normal, u"keyLitFill"_s, hex(look.accent));

    for (auto it = theme.kinds.begin(); it != theme.kinds.end(); ++it) {
        QJsonObject &normal = it.value().normal;
        if (it.key() == u"label") {
            set(normal, u"textColor"_s, hex(look.text));
        } else if (it.key() != u"comment" && it.key() != u"image" && normal.contains(u"fill"_s)) {
            set(normal, u"fill"_s, hex(look.panel));   // panels (widgets)
            set(normal, u"textColor"_s, hex(look.text));
        }
    }
    QJsonObject status = theme.extra.value(u"status").toObject();
    set(status, u"tableCurrent"_s, hex(look.accent));
    // High contrast: every button outlined in white, and the colors that mean
    // something at their brightest (a bright kitchen, eyes that need it).
    if (look.id == u"contrast") {
        set(theme.style.normal, u"frame"_s, u"border"_s);
        set(theme.style.normal, u"frameWidth"_s, 3);
        set(theme.style.normal, u"borderColor"_s, u"#ffffff"_s);
        set(theme.style.normal, u"bold"_s, true);
        set(theme.style.selected, u"textColor"_s, u"#000000"_s);
        for (const auto &[key, color] : {std::pair{"tableOpen", "#ff9f1a"}, std::pair{"tableMine", "#00e676"},
                                         std::pair{"tableLong", "#ff1744"}, std::pair{"kitchenNew", "#00c853"},
                                         std::pair{"kitchenWarn", "#ffab00"}, std::pair{"kitchenLate", "#ff1744"},
                                         std::pair{"kitchenReady", "#2979ff"}, std::pair{"soldOut", "#ff1744"}})
            set(status, QString::fromLatin1(key), QString::fromLatin1(color));
        theme.extra.insert(u"contrast"_s, true);
    } else if (theme.extra.value(u"contrast").toBool()) {
        // Back from high contrast: the usual frames and status colors.
        theme.style.normal.remove(u"borderColor"_s);
        set(theme.style.normal, u"frame"_s, u"raised"_s);
        for (const char *key : {"tableOpen", "tableMine", "tableLong", "kitchenNew", "kitchenWarn", "kitchenLate",
                                "kitchenReady", "soldOut"})
            status.remove(QString::fromLatin1(key));
        theme.extra.remove(u"contrast"_s);
    }
    theme.extra.insert(u"status"_s, status);
}

} // namespace vt::app
