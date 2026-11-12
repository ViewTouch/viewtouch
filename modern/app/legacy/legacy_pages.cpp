#include "app/legacy/legacy_pages.hh"

#include <QJsonArray>

#include <algorithm>
#include <map>
#include <optional>

using namespace Qt::StringLiterals;

namespace vt::legacy {

namespace {

// --- legacy's look (term/term_view.hh, src/core/image_data.cc) ---------------

const QStringList kColors{u"#000000"_s, u"#ffffff"_s, u"#eb0000"_s, u"#008000"_s, u"#0000e6"_s, u"#ffff00"_s,
                          u"#844c26"_s, u"#ff5400"_s, u"#6400c8"_s, u"#0084a8"_s, u"#606060"_s, u"#c03088"_s,
                          u"#ff3800"_s, u"#008060"_s, u"#502d19"_s, u"#a50000"_s, u"#005a00"_s, u"#000091"_s,
                          u"#005c82"_s, u"#a0206e"_s, u"#006248"_s};
constexpr int kColorDefault = 255, kColorPageDefault = 254, kColorClear = 253;
const QStringList kTextures{
    u"sand"_s,          u"litsand"_s,           u"darksand"_s,      u"litewood"_s,      u"wood"_s,
    u"darkwood"_s,      u"grayparchment"_s,     u"graymarble"_s,    u"greenmarble"_s,   u"parchment"_s,
    u"pearl"_s,         u"canvas"_s,            u"tanparchment"_s,  u"smoke"_s,         u"leather"_s,
    u"blueparchment"_s, u"gradient"_s,          u"gradientbrown"_s, u"black"_s,         u"greysand"_s,
    u"whitemesh"_s,     u"carbonfiber"_s,       u"whitetexture"_s,  u"darkorangetexture"_s, u"yellowtexture"_s,
    u"greentexture"_s,  u"orangetexture"_s,     u"bluetexture"_s,   u"pooltable"_s,     u"test"_s,
    u"diamondleather"_s, u"bread"_s,            u"lava"_s,          u"darkmarble"_s};
constexpr int kImageClear = 253, kImageDefault = 255;

struct Font {
    QString family;
    int px;     // on the 1920x1080 page: legacy's Xft fonts at 96 dpi
    bool bold;
};
const Font &fontOf(int f)
{
    static const QString serif = u"DejaVu Serif"_s, plain = u"Liberation Serif"_s;
    static const std::map<int, Font> fonts{
        {1, {serif, 37, false}},  {2, {serif, 37, true}},   {4, {serif, 16, false}},  {5, {serif, 19, false}},
        {6, {serif, 24, false}},  {7, {serif, 16, true}},   {8, {serif, 19, true}},   {9, {serif, 24, true}},
        {10, {serif, 13, false}}, {11, {serif, 13, true}},  {12, {serif, 15, false}}, {13, {serif, 15, true}},
        {14, {plain, 15, false}}, {15, {plain, 15, true}},  {16, {plain, 16, false}}, {17, {plain, 16, true}}};
    const auto it = fonts.find(f);
    return it != fonts.end() ? it->second : fonts.at(9);
}

QString shapeOf(int s)
{
    static const std::map<int, QString> shapes{{1, u"rect"_s},    {2, u"diamond"_s}, {3, u"circle"_s},
                                               {4, u"hexagon"_s}, {5, u"octagon"_s}, {6, u"diamond"_s}};
    const auto it = shapes.find(s);
    return it != shapes.end() ? it->second : u"rect"_s;
}

QString behaviorOf(int b)
{
    static const std::map<int, QString> behaviors{{0, u"none"_s},   {1, u"toggle"_s}, {2, u"blink"_s},
                                                  {3, u"toggle"_s}, {4, u"double"_s}, {5, u"passthrough"_s}};
    const auto it = behaviors.find(b);
    return it != behaviors.end() ? it->second : u"blink"_s;
}

std::pair<int, int> sizeOf(int s)
{
    static const std::map<int, std::pair<int, int>> sizes{
        {1, {640, 480}},    {2, {768, 1024}},   {3, {800, 480}},    {4, {800, 600}},    {5, {1024, 600}},
        {6, {1024, 768}},   {7, {1280, 800}},   {8, {1280, 1024}},  {9, {1366, 768}},   {10, {1440, 900}},
        {11, {1600, 900}},  {12, {1600, 1200}}, {13, {1680, 1050}}, {14, {1920, 1080}}, {15, {1920, 1200}},
        {16, {2560, 1440}}, {17, {2560, 1600}}};
    const auto it = sizes.find(s);
    return it != sizes.end() ? it->second : std::pair{1920, 1080};
}

// ZF_* -> Modern's frame and its width; nullopt: the page's or store's.
std::optional<std::pair<QString, int>> frameOf(int code)
{
    if (code == 0 || code == 1)
        return std::nullopt;
    if (code == 2)
        return std::pair{u"hidden"_s, 0};
    if (code == 3)
        return std::pair{u"none"_s, 0};
    if (code >= 10 && code <= 13)
        return std::pair{u"raised"_s, 3};
    if (code >= 20 && code <= 23)
        return std::pair{u"inset"_s, 3};
    if (code >= 30 && code <= 33)
        return std::pair{u"raised"_s, 6};
    if (code >= 40 && code <= 51)
        return std::pair{u"border"_s, 6};
    return std::pair{u"raised"_s, 3};
}

// --- what each page is ---------------------------------------------------------

QString kindOf(int type)
{
    static const std::map<int, QString> kinds{
        {0, u"custom"_s},    {1, u"tables"_s},  {2, u"index"_s},    {3, u"items"_s},    {4, u"modifier"_s},
        {5, u"modifier"_s},  {6, u"modifier"_s}, {7, u"custom"_s},  {8, u"library"_s},  {9, u"items"_s},
        {10, u"tables"_s},   {12, u"custom"_s}, {13, u"kitchen"_s}, {14, u"kitchen"_s}, {15, u"kitchen"_s},
        {16, u"kitchen"_s},  {17, u"modifier"_s}, {18, u"template"_s}};
    const auto it = kinds.find(type);
    return it != kinds.end() ? it->second : u"custom"_s;
}

// Legacy's menu area on its order screen (-99: the check at the left, keys along the bottom).
const QJsonObject kOrderContent{{u"x"_s, 460}, {u"y"_s, 8}, {u"w"_s, 1452}, {u"h"_s, 944}};

QString ownId(int lid) { return lid < 0 ? u"legacy-n%1"_s.arg(-lid) : u"legacy-%1"_s.arg(lid); }

// --- what each zone does ---------------------------------------------------------

QString jumpOf(int t)
{
    static const std::map<int, QString> jumps{{1, u"push"_s}, {2, u"replace"_s}, {3, u"back"_s}, {4, u"home"_s},
                                              {5, u"back"_s}, {6, u"index"_s},   {7, u"push"_s}};
    const auto it = jumps.find(t);
    return it != jumps.end() ? it->second : QString();
}

const QHash<QString, QString> kMessages{
    {u"logout"_s, u"logout"_s},           {u"clockin"_s, u"clockIn"_s},        {u"clockout"_s, u"clockOut"_s},
    {u"open drawer"_s, u"noSale"_s},      {u"takeout"_s, u"startTakeout"_s},   {u"starttakeout"_s, u"startTakeout"_s},
    {u"quicktogo"_s, u"startTakeout"_s},  {u"quickdinein"_s, u"startQuick"_s}, {u"faststart"_s, u"startQuick"_s},
    {u"start"_s, u"startQuick"_s},        {u"taxexempt"_s, u"taxExempt"_s},    {u"final"_s, u"sendOrder"_s},
    {u"finalize"_s, u"closeCheck"_s},     {u"print"_s, u"printReceipt"_s},     {u"payout"_s, u"payout"_s},
    {u"backspace"_s, u"lineLess"_s},      {u"cancel"_s, u"releaseCheck"_s}};

// Keys: legacy sends a button's name when it has no message ("1", "Clear"),
// to the page's PIN or amount entry (button_zone.cc SendandJump).
QString keyOf(const QString &word)
{
    static const QHash<QString, QString> keys = [] {
        QHash<QString, QString> k{{u"00"_s, u"00"_s}, {u"clear"_s, u"clear"_s}, {u"enter"_s, u"enter"_s},
                                  {u"backspace"_s, u"back"_s}};
        for (int d = 0; d < 10; ++d)
            k.insert(QString::number(d), QString::number(d));
        return k;
    }();
    return keys.value(word);
}

const std::map<int, QString> kQualifiers{{1, u"no"_s}, {2, u"side"_s}, {4, u"sub"_s}, {8, u"lite"_s}, {32, u"extra"_s}};
// The words legacy draws on a qualifier key (main/business/sales.hh, bit by bit).
const QStringList kQualifierNames{u"No"_s,     u"Side"_s,    u"Sub"_s,       u"Lite"_s,  u"Only"_s,  u"Extra"_s,
                                  u"Double"_s, u"Dry"_s,     u"Plain"_s,     u"Toasted"_s, u"Untoasted"_s,
                                  u"Crispy"_s, u"Hard"_s,    u"Soft"_s,      u"Grilled"_s, u"Left"_s, u"Right"_s,
                                  u"Whole"_s,  u"Cut/2"_s,   u"Cut/3"_s,     u"Cut/4"_s, u"Easy"_s,  u"Add"_s,
                                  u"Senior Share"_s};
const std::map<int, QString> kTenders{{0, u"cash"_s},  {2, u"credit"_s},   {11, u"credit"_s},
                                      {12, u"credit"_s}, {4, u"gift"_s},   {6, u"house"_s},
                                      {8, u"discount"_s}, {5, u"comp"_s},  {10, u"staff-meal"_s}};
const QHash<QString, QString> kWidgets{
    {u"ZONE_LOGIN"_s, u"loginPad"_s},          {u"ZONE_ORDER_ENTRY"_s, u"orderList"_s},
    {u"ZONE_PAYMENT_ENTRY"_s, u"paymentPanel"_s}, {u"ZONE_CHECK_LIST"_s, u"checkList"_s},
    {u"ZONE_GUEST_COUNT"_s, u"guestCount"_s},  {u"ZONE_END_DAY"_s, u"endOfDay"_s},
    {u"ZONE_SCHEDULE"_s, u"schedule"_s},       {u"ZONE_CUSTOMER_INFO"_s, u"customerInfo"_s},
    {u"ZONE_DRAWER_MANAGE"_s, u"drawerPanel"_s}, {u"ZONE_LOGOUT"_s, u"logoutPanel"_s},
    {u"ZONE_LABOR"_s, u"timeClock"_s},         {u"ZONE_INVENTORY"_s, u"stockCount"_s},
    {u"ZONE_SEARCH"_s, u"checkSearch"_s},      {u"ZONE_SPLIT_CHECK"_s, u"splitCheck"_s},
    {u"ZONE_KITCHEN_VIDEO"_s, u"kitchenDisplay"_s}};
// What each legacy zone kind was for, said when touched (not in Modern yet).
const QHash<QString, QString> kNames{
    {u"ZONE_REPORT"_s, u"this report"_s},             {u"ZONE_SWITCH"_s, u"this setting switch"_s},
    {u"ZONE_TOGGLE"_s, u"this toggle"_s},             {u"ZONE_SETTINGS"_s, u"these settings"_s},
    {u"ZONE_TAX_SETTINGS"_s, u"these tax settings"_s}, {u"ZONE_KILL_SYSTEM"_s, u"quitting ViewTouch from here"_s},
    {u"ZONE_CLEAR_SYSTEM"_s, u"clearing the system here"_s}, {u"ZONE_DEVELOPER"_s, u"the developer's settings"_s},
    {u"ZONE_HARDWARE"_s, u"these hardware settings"_s}};
// Item buttons, of each kind legacy has.
const QSet<QString> kItems{u"ZONE_ITEM"_s,          u"ZONE_ITEM_NORMAL"_s,   u"ZONE_ITEM_MODIFIER"_s,
                           u"ZONE_ITEM_METHOD"_s,   u"ZONE_ITEM_SUBSTITUTE"_s, u"ZONE_ITEM_POUND"_s,
                           u"ZONE_ITEM_ADMISSION"_s};
const QSet<QString> kPlain{u"ZONE_SIMPLE"_s, u"ZONE_STANDARD"_s, u"ZONE_CONDITIONAL"_s};

QJsonObject act(const QString &type, std::initializer_list<std::pair<QString, QJsonValue>> more = {})
{
    QJsonObject a{{u"type"_s, type}};
    for (const auto &[k, v] : more)
        a.insert(k, v);
    return a;
}

QString labelOf(QString name)
{
    // A toggle's words for each state: "!Zero Exclusion OFF!Zero Exclusion ON" (the first shows).
    if (name.startsWith(u'!') && name.count(u'!') >= 2)
        name = name.split(u'!').at(1);
    return name.replace(u"{termname}"_s, u"{terminal}"_s).replace(u'\\', u'\n').trimmed();
}

struct Context {
    const PageOptions &options;
    const QHash<int, QString> &pageIds;   // legacy id -> Modern page id (a stand-in's Modern page)
    QList<NotInModern> &missing;
};

QJsonArray actionsOf(const Zone &z, const Page &page, const QString &label, Context &cx)
{
    const auto missing = [&](const QString &what) {
        cx.missing.append({page.name, label.isEmpty() ? u"(no label)"_s : QString(label).replace(u'\n', u' '), what});
        return QJsonArray{act(u"notInModern"_s, {{u"what"_s, what}})};
    };
    const QString &kind = z.kind;
    QJsonArray acts;
    const QString msg = z.fields.value(u"Message"_s).toString().trimmed();
    if (kind == u"ZONE_QUALIFIER") {
        const auto q = kQualifiers.find(z.fields.value(u"QualifierType"_s).toInt());
        if (q != kQualifiers.end())
            return {act(u"qualifier"_s, {{u"qualifier"_s, q->second}})};
        cx.missing.append({page.name, label.isEmpty() ? u"(no label)"_s : QString(label).replace(u'\n', u' '),
                           u"the qualifier '%1'"_s.arg(label)});
        return {act(u"notInModern"_s, {{u"what"_s, label.isEmpty() ? u"this qualifier"_s : label}})};
    }
    if (kind == u"ZONE_TENDER") {
        const int tt = z.fields.value(u"TenderType"_s, -1).toInt();
        const auto t = kTenders.find(tt);
        if (t != kTenders.end())
            return {act(u"tender"_s, {{u"tender"_s, t->second}})};
        if (tt == 3)
            return {act(u"command"_s, {{u"name"_s, u"coupon"_s}})};
        if (tt == 21) {
            const int amount = z.fields.value(u"TenderAmount"_s).toInt();
            return {act(u"command"_s, {{u"name"_s, u"gratuity"_s},
                                       {u"args"_s, QJsonObject{{u"percent"_s, (amount ? amount : 1500) / 100.0}}}})};
        }
        cx.missing.append({page.name, label.isEmpty() ? u"(no label)"_s : QString(label).replace(u'\n', u' '),
                           u"the payment '%1'"_s.arg(label)});
        return {act(u"notInModern"_s, {{u"what"_s, label.isEmpty() ? u"this payment"_s : label}})};
    }
    if (kItems.contains(kind)) {
        const QString item = z.fields.value(u"ItemName"_s).toString();
        if (!item.isEmpty() && !cx.options.item)
            return {act(u"addItem"_s, {{u"item"_s, item}})};
        if (!item.isEmpty()) {
            if (const QString id = cx.options.item(item); !id.isEmpty())
                return {act(u"addItem"_s, {{u"item"_s, id}})};
            // A button for an item since taken off the menu (legacy showed it; touching did nothing).
            return missing(u"\"%1\", no longer on the menu"_s.arg(item));
        }
    }
    if (kind == u"ZONE_ORDER_FLOW" || kind == u"ZONE_ORDER_PAGE")
        return {act(u"jump"_s, {{u"mode"_s, u"index"_s}})};
    if (kind == u"ZONE_ORDER_ADD")
        return {act(u"command"_s, {{u"name"_s, u"repeatLine"_s}})};
    if (kind == u"ZONE_ORDER_DELETE")
        return {act(u"command"_s, {{u"name"_s, u"voidItem"_s}})};
    if (kind == u"ZONE_ORDER_COMMENT")
        return {act(u"jump"_s, {{u"mode"_s, u"push"_s}, {u"page"_s, u"note"_s}})};
    const QString key = keyOf((msg.isEmpty() ? z.name : msg).trimmed().toLower());
    const bool plainOrCommand = kind == u"ZONE_STANDARD" || kind == u"ZONE_CONDITIONAL" || kind == u"ZONE_COMMAND";
    if ((kind == u"ZONE_STANDARD" || kind == u"ZONE_CONDITIONAL") && !key.isEmpty()) {
        acts.append(act(u"key"_s, {{u"key"_s, key}}));
    } else if (plainOrCommand && !msg.isEmpty()) {
        if (msg.startsWith(u"RUNCMD:"_s))
            return missing(u"running a program on the computer"_s);
        const QString cmd = kMessages.value(msg.toLower());
        if (!cmd.isEmpty())
            acts.append(act(u"command"_s, {{u"name"_s, cmd}}));
        else if (z.fields.value(u"JumpType"_s).toInt()
                 && QStringList{u"save"_s, u"restore"_s, u"cancel"_s, u"clear"_s}.contains(msg.toLower()))
            ;   // a form's Save/Return: Modern keeps what's typed as it goes; the jump is what's left
        else {
            cx.missing.append({page.name, label.isEmpty() ? u"(no label)"_s : QString(label).replace(u'\n', u' '),
                               u"the command '%1'"_s.arg(msg)});
            return {act(u"notInModern"_s, {{u"what"_s, label.isEmpty() ? msg : label}})};
        }
    }
    if (const int jt = z.fields.value(u"JumpType"_s).toInt()) {
        const QString mode = jumpOf(jt);
        if (mode == u"push" || mode == u"replace") {
            const QString target = cx.pageIds.value(z.fields.value(u"JumpID"_s).toInt());
            if (!target.isEmpty())
                acts.append(act(u"jump"_s, {{u"mode"_s, mode}, {u"page"_s, target}}));
        } else if (!mode.isEmpty()) {
            acts.append(act(u"jump"_s, {{u"mode"_s, mode}}));
        }
    }
    if (acts.isEmpty() && kNames.contains(kind))
        return missing(kNames.value(kind));
    if (acts.isEmpty() && !kPlain.contains(kind) && kind != u"ZONE_COMMENT" && !kItems.contains(kind))
        return missing(QString(kind).remove(0, 5).replace(u'_', u' ').toLower());
    return acts;
}

// A zone's look in a state (0 normal, 1 selected), legacy's defaults resolved.
QJsonObject lookOf(const Zone &z, int state, const Page &page, const PageDefaults &store)
{
    const Look &pd = page.defaults[state];
    const Look &sd = store.looks[state];
    const auto frame = frameOf(z.frame[state]).value_or(
        frameOf(pd.frame).value_or(frameOf(sd.frame).value_or(std::pair{u"raised"_s, 3})));
    int tex = z.texture[state];
    if (tex == kImageDefault)
        tex = pd.texture != kImageDefault ? pd.texture : sd.texture;
    int color = z.color[state];
    if (color == kColorDefault || color == kColorPageDefault)
        color = pd.color != kColorDefault && pd.color != kColorPageDefault ? pd.color : sd.color;
    const int font = z.font ? z.font : page.font ? page.font : store.font;
    QJsonObject st;
    if (frame.first == u"hidden") {
        st.insert(u"frame"_s, u"none"_s);
        st.insert(u"fill"_s, u"transparent"_s);
        st.insert(u"textColor"_s, u"transparent"_s);
        st.insert(u"texture"_s, QString());
    } else {
        st.insert(u"frame"_s, frame.first);
        if (frame.second)
            st.insert(u"frameWidth"_s, frame.second);
        if (tex == kImageClear || tex < 0 || tex >= kTextures.size()) {
            st.insert(u"fill"_s, u"transparent"_s);
            st.insert(u"texture"_s, QString());
        } else {
            st.insert(u"texture"_s, kTextures.at(tex));
        }
        st.insert(u"textColor"_s, color == kColorClear || color < 0 || color >= kColors.size() ? u"transparent"_s
                                                                                              : kColors.at(color));
    }
    const Font &f = fontOf(font);
    st.insert(u"font"_s, f.family);
    st.insert(u"fontSize"_s, f.px);
    st.insert(u"bold"_s, f.bold);
    st.insert(u"shadow"_s, z.shadow < 256 ? z.shadow : page.shadow < 256 ? page.shadow : store.shadow);
    st.insert(u"radius"_s, 0);
    return st;
}

QJsonObject convertZone(const Zone &z, int n, const Page &page, std::pair<int, int> canvas, Context &cx)
{
    QString label = labelOf(z.name);
    if (label.isEmpty() && kItems.contains(z.kind))   // legacy draws the item's name on its button
        label = z.fields.value(u"ItemName"_s).toString().trimmed();
    if (label.isEmpty() && z.kind == u"ZONE_QUALIFIER") {
        const int q = z.fields.value(u"QualifierType"_s).toInt();
        QStringList words;
        for (int i = 0; i < kQualifierNames.size(); ++i)
            if (q & (1 << i))
                words.append(kQualifierNames.at(i));
        label = words.join(u' ');
    }
    // Its id says its page (a page's zones and its template's are one set): "n99-8".
    const QString tag = page.id < 0 ? u"n%1"_s.arg(-page.id) : QString::number(page.id);
    // On the page: legacy drew what was past its edge cut off there.
    const int x = std::max(0, z.x), y = std::max(0, z.y);
    const int w = std::min(z.w - (x - z.x), canvas.first - x), h = std::min(z.h - (y - z.y), canvas.second - y);
    QJsonObject out{{u"id"_s, u"%1-%2"_s.arg(tag).arg(n)},
                    {u"rect"_s, QJsonObject{{u"x"_s, x}, {u"y"_s, y}, {u"w"_s, std::max(1, w)}, {u"h"_s, std::max(1, h)}}},
                    {u"shape"_s, shapeOf(z.shape)},
                    {u"behavior"_s, behaviorOf(z.behave)}};
    if (z.kind == u"ZONE_COMMENT" || z.kind == u"ZONE_TABLE") {   // (a dining room's tables: drawn, not the store's)
        out.insert(u"kind"_s, u"comment"_s);
        out.insert(u"label"_s, label);
        out.insert(u"behavior"_s, u"none"_s);
    } else if (kWidgets.contains(z.kind)) {
        out.insert(u"kind"_s, kWidgets.value(z.kind));
        out.insert(u"behavior"_s, u"none"_s);
        if (z.kind == u"ZONE_LOGIN")
            out.insert(u"props"_s, QJsonObject{{u"keys"_s, false}});   // legacy's PIN keys are buttons of their own
    } else {
        out.insert(u"kind"_s, u"button"_s);
        out.insert(u"label"_s, label);
        const QJsonArray acts = actionsOf(z, page, label, cx);
        if (!acts.isEmpty())
            out.insert(u"actions"_s, acts);
        else if (kPlain.contains(z.kind)) {
            out.insert(u"kind"_s, u"label"_s);   // legacy text on the page: touching it does nothing
            out.insert(u"behavior"_s, u"none"_s);
        }
    }
    out.insert(u"style"_s, QJsonObject{{u"normal"_s, lookOf(z, 0, page, cx.options.defaults)},
                                       {u"selected"_s, lookOf(z, 1, page, cx.options.defaults)}});
    // (A legacy zone's picture is a file on that store's computer: not here.)
    QJsonObject legacy{{u"kind"_s, z.kind}};
    if (const int jumpId = z.fields.value(u"JumpID"_s).toInt())
        legacy.insert(u"id"_s, jumpId);
    out.insert(u"legacy"_s, legacy);
    return out;
}

QJsonObject convertPage(const Page &p, Context &cx)
{
    const auto [w, h] = sizeOf(p.size);
    const int bg = p.image != kImageDefault ? p.image : cx.options.defaults.image;
    const QString kind = kindOf(p.type);
    QJsonObject out{
        {u"schemaVersion"_s, 1},
        {u"id"_s, ownId(p.id)},
        {u"name"_s, QString(p.name).replace(u"{termname}"_s, u"Terminal"_s)},
        {u"kind"_s, kind == u"tables" && !standsIn().contains(p.id) ? u"custom"_s : kind},
        {u"inStyle"_s, u"legacy"_s},
        {u"canvas"_s, QJsonObject{{u"w"_s, w}, {u"h"_s, h}}},
        {u"grid"_s, 4},
        {u"background"_s, bg >= 0 && bg < kTextures.size() ? QJsonObject{{u"texture"_s, kTextures.at(bg)}}
                                                            : QJsonObject{{u"fill"_s, u"#000000"_s}}},
        {u"legacyId"_s, p.id}};
    if (standsIn().contains(p.id))
        out.insert(u"variantOf"_s, standsIn().value(p.id));
    if (cx.pageIds.contains(p.parent) && p.parent != p.id)
        out.insert(u"templateId"_s, ownId(p.parent));
    if (p.id == -99) {
        out.insert(u"kind"_s, u"template"_s);
        out.insert(u"contentArea"_s, kOrderContent);
    }
    QList<Zone> zones = p.zones;
    // The login page's day-closing panel: legacy showed it only while closing the day, over the PIN entry.
    if (std::ranges::any_of(zones, [](const Zone &z) { return z.kind == u"ZONE_LOGIN"; }))
        zones.removeIf([](const Zone &z) { return z.kind == u"ZONE_END_DAY"; });
    QJsonArray converted;
    for (int n = 0; n < zones.size(); ++n) {
        const QJsonObject z = convertZone(zones[n], n + 1, p, {w, h}, cx);
        // Off the page (what's left of it under 16x16): legacy never showed it either.
        const QJsonObject r = z.value(u"rect"_s).toObject();
        if (r.value(u"w"_s).toInt() >= 16 && r.value(u"h"_s).toInt() >= 16)
            converted.prepend(z);   // legacy draws last to first (zone/zone.cc Page::Render): its first on top
    }
    out.insert(u"zones"_s, converted);
    return out;
}

} // namespace

const QHash<int, QString> &standsIn()
{
    static const QHash<int, QString> s{{-1, u"login"_s},       {-20, u"settle"_s},      {-5, u"guest-count"_s},
                                       {-10, u"manager"_s},    {-72, u"time-clock"_s},  {-7, u"logout"_s},
                                       {-24, u"check-list"_s}, {-901, u"kitchen"_s},    {-921, u"bar-display"_s},
                                       {-99, u"order-template"_s}};
    // (Legacy's dining room, page 50, is its sample store's: in Legacy a store
    // keeps its own tables, and that page is kept as a picture of legacy's.)
    return s;
}

PageSet convertPages(const QList<const PageFile *> &files, const PageOptions &options)
{
    // One page per legacy id: the 1920x1080 one where there are two (-1), the first otherwise.
    std::map<int, const Page *> pages;
    for (const PageFile *file : files)
        for (const Page &p : file->pages) {
            const auto have = pages.find(p.id);
            if (have == pages.end() || (have->second->size != 14 && p.size == 14))
                pages[p.id] = &p;
        }
    QHash<int, QString> pageIds;
    for (const auto &[lid, p] : pages)
        pageIds.insert(lid, standsIn().value(lid, ownId(lid)));
    for (const int lid : options.knownPages)
        if (!pageIds.contains(lid))
            pageIds.insert(lid, standsIn().value(lid, ownId(lid)));
    PageSet out;
    Context cx{options, pageIds, out.missing};
    for (const auto &[lid, p] : pages) {
        out.pages.append(convertPage(*p, cx));
        out.zones += int(p->zones.size());
    }
    return out;
}

QString notInModernReport(const QList<NotInModern> &missing)
{
    QStringList lines{u"# Legacy buttons not in ViewTouch Modern yet"_s, QString(),
                      u"Made by `vtmodern --legacy-pages` from the legacy ViewTouch's own pages. In the Legacy style"_s,
                      u"these buttons are where legacy had them; touching one says it isn't in Modern yet."_s,
                      QString()};
    std::map<QString, QList<const NotInModern *>> byPage;
    for (const NotInModern &m : missing)
        byPage[m.page].append(&m);
    for (const auto &[page, list] : byPage) {
        lines.append(u"## "_s + page);
        for (const NotInModern *m : list)
            lines.append(u"- %1: %2"_s.arg(m->label, m->what));
        lines.append(QString());
    }
    return lines.join(u'\n');
}

} // namespace vt::legacy
