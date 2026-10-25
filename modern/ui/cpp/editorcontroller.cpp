#include "editorcontroller.hh"

#include "app/pos_session.hh"
#include "layout/json_path.hh"
#include "layout/schema.hh"

#include <QCoreApplication>
#include <QHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

using namespace Qt::StringLiterals;
using vt::app::LayoutEditor;
using vt::layout::Layout;
using vt::layout::Page;
using vt::layout::Zone;
using vt::layout::ZoneState;

namespace {

QJsonValue toJson(const QVariant &v)
{
    // QML `undefined` arrives as an invalid QVariant: remove the key.
    if (!v.isValid())
        return QJsonValue(QJsonValue::Undefined);
    return QJsonValue::fromVariant(v);
}

std::optional<ZoneState> parseState(QStringView s)
{
    if (s == u"normal") return ZoneState::Normal;
    if (s == u"selected") return ZoneState::Selected;
    if (s == u"disabled") return ZoneState::Disabled;
    return std::nullopt;
}

std::optional<QJsonObject> readJsonFile(const QUrl &url, QString *why)
{
    QFile f(url.toLocalFile());
    if (!f.open(QIODevice::ReadOnly)) {
        *why = QCoreApplication::translate("EditorController", "Cannot open %1: %2").arg(f.fileName(), f.errorString());
        return std::nullopt;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (!doc.isObject()) {
        *why = QCoreApplication::translate("EditorController", "%1 is not a ViewTouch file: %2").arg(QFileInfo(f).fileName(), err.errorString());
        return std::nullopt;
    }
    return doc.object();
}

bool writeJsonFile(const QUrl &url, const QJsonObject &o, QString *why)
{
    QSaveFile f(url.toLocalFile());
    if (!f.open(QIODevice::WriteOnly)) {
        *why = QCoreApplication::translate("EditorController", "Cannot write %1: %2").arg(f.fileName(), f.errorString());
        return false;
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        *why = QCoreApplication::translate("EditorController", "Cannot write %1: %2").arg(f.fileName(), f.errorString());
        return false;
    }
    return true;
}

// The schema's words in the screen's language: labels, section names,
// hints, choices; composed ones (Hide “Station…”) part by part. `groupKey`
// keeps the English section name for code that looks for it.
QString schemaText(const QString &english)
{
    return english.isEmpty() ? english : QCoreApplication::translate("Schema", english.toUtf8().constData());
}

QJsonObject translatedField(QJsonObject f)
{
    if (f.contains(u"labelTemplate"_s))
        f.insert(u"label"_s, schemaText(f.value(u"labelTemplate"_s).toString()).arg(schemaText(f.value(u"labelArg"_s).toString())));
    else if (f.contains(u"label"_s))
        f.insert(u"label"_s, schemaText(f.value(u"label"_s).toString()));
    if (f.contains(u"hintTemplate"_s))
        f.insert(u"hint"_s, schemaText(f.value(u"hintTemplate"_s).toString()).arg(f.value(u"hintArg"_s).toString()));
    else if (f.contains(u"hint"_s))
        f.insert(u"hint"_s, schemaText(f.value(u"hint"_s).toString()));
    if (f.contains(u"group"_s)) {
        f.insert(u"groupKey"_s, f.value(u"group"_s));
        f.insert(u"group"_s, schemaText(f.value(u"group"_s).toString()));
    }
    if (f.contains(u"options"_s)) {
        QJsonArray options;
        for (const QJsonValue &v : f.value(u"options"_s).toArray()) {
            QJsonObject o = v.toObject();
            if (o.value(u"text"_s).isString())
                o.insert(u"text"_s, schemaText(o.value(u"text"_s).toString()));
            options.append(o);
        }
        f.insert(u"options"_s, options);
    }
    if (f.contains(u"fields"_s)) {   // an action type's
        QJsonArray fields;
        for (const QJsonValue &v : f.value(u"fields"_s).toArray())
            fields.append(translatedField(v.toObject()));
        f.insert(u"fields"_s, fields);
    }
    return f;
}

QVariantList translated(const QJsonArray &fields)
{
    QVariantList out;
    for (const QJsonValue &v : fields)
        out.append(translatedField(v.toObject()).toVariantMap());
    return out;
}

} // namespace

EditorController::EditorController(Layout base, QObject *parent)
    : QObject(parent)
    , editor_(std::move(base))
{
    editor_.setChangedCallback([this] { onLayoutChanged(); });
    QUndoStack *stack = editor_.undoStack();
    connect(stack, &QUndoStack::indexChanged, this, &EditorController::stateChanged);
    connect(stack, &QUndoStack::cleanChanged, this, &EditorController::stateChanged);
}

void EditorController::setPageId(const QString &pageId)
{
    if (pageId == pageId_)
        return;
    pageId_ = pageId;
    selection_.clear();
    emit pageChanged();
    emit selectionChanged();
    emit layoutChanged();   // geometry & references are per page
    bump();
}

void EditorController::onLayoutChanged()
{
    // Drop selected ids that an undo or delete removed.
    const Page *page = editor_.layout().page(pageId_);
    QStringList kept;
    for (const QString &id : std::as_const(selection_)) {
        if (page && page->zone(id))
            kept.append(id);
    }
    if (kept != selection_) {
        selection_ = kept;
        emit selectionChanged();
    }
    emit layoutChanged();
    bump();
}

void EditorController::setSelection(QStringList ids)
{
    ids.removeDuplicates();
    if (ids == selection_)
        return;
    selection_ = ids;
    emit selectionChanged();
    bump();
}

void EditorController::bump()
{
    ++revision_;
    emit revisionChanged();
}

bool EditorController::fail(const QString &why)
{
    setNotice(why);
    return false;
}

void EditorController::setNotice(const QString &text)
{
    notice_ = text;
    emit noticeChanged();
}

QString EditorController::selectionKind() const
{
    const Page *page = editor_.layout().page(pageId_);
    QString kind;
    for (const QString &id : selection_) {
        const Zone *z = page ? page->zone(id) : nullptr;
        if (!z)
            continue;
        if (!kind.isEmpty() && kind != z->kind)
            return {};
        kind = z->kind;
    }
    return kind;
}

QVariantList EditorController::geometry() const
{
    QVariantList out;
    for (const Layout::PlacedZone &pz : editor_.layout().effectiveZones(pageId_)) {
        const Zone &z = *pz.zone;
        out.append(QVariantMap{
            {u"id"_s, z.id}, {u"kind"_s, z.kind}, {u"label"_s, z.label},
            {u"x"_s, z.rect.x()}, {u"y"_s, z.rect.y()}, {u"w"_s, z.rect.width()}, {u"h"_s, z.rect.height()},
            {u"inherited"_s, pz.inherited}, {u"owner"_s, pz.owner->id}, {u"ownerName"_s, pz.owner->name},
            {u"rot"_s, vt::layout::rotationOf(z)},
        });
    }
    return out;
}

QVariantList EditorController::pages() const
{
    QVariantList out;
    for (const Page &p : editor_.layout().pages) {
        out.append(QVariantMap{
            {u"id"_s, p.id}, {u"name"_s, p.name}, {u"kind"_s, p.kind}, {u"role"_s, p.role},
            {u"templateId"_s, p.templateId}, {u"zoneCount"_s, p.zones.size()},
            {u"variantOf"_s, p.variantOf}, {u"hasPhone"_s, editor_.layout().variantFor(p.id, u"phone"_s) != nullptr},
        });
    }
    return out;
}

bool EditorController::canUndo() const { return editor_.undoStack()->canUndo(); }
bool EditorController::canRedo() const { return editor_.undoStack()->canRedo(); }
QString EditorController::undoText() const { return editor_.undoStack()->undoText(); }
QString EditorController::redoText() const { return editor_.undoStack()->redoText(); }

QStringList EditorController::textures() const
{
    QStringList names;
    for (const QString &file : QDir(u":/textures"_s).entryList({u"*.xpm"_s}, QDir::Files, QDir::Name))
        names.append(QFileInfo(file).completeBaseName());
    return names;
}

QStringList EditorController::basicKinds() const { return vt::layout::schema::basicKinds(); }
QStringList EditorController::widgetKinds() const { return vt::layout::schema::widgetKinds(); }

// What a kind is called on screen (the + Panel menu, the Type box).
QString EditorController::kindName(const QString &kind) const
{
    static const QHash<QString, const char *> names = {
        {u"button"_s, QT_TR_NOOP("Button")},
        {u"label"_s, QT_TR_NOOP("Text")},
        {u"image"_s, QT_TR_NOOP("Picture")},
        {u"comment"_s, QT_TR_NOOP("Note (editing only)")},
        {u"table"_s, QT_TR_NOOP("Table")},
        {u"tableGrid"_s, QT_TR_NOOP("Table map")},
        {u"staffPicker"_s, QT_TR_NOOP("Staff picker")},
        {u"checkHistory"_s, QT_TR_NOOP("Check history")},
        {u"modifierPicker"_s, QT_TR_NOOP("Choices picker")},
        {u"soldOutList"_s, QT_TR_NOOP("Sold out list")},
        {u"orderList"_s, QT_TR_NOOP("Order list")},
        {u"loginPad"_s, QT_TR_NOOP("Login keypad")},
        {u"guestCount"_s, QT_TR_NOOP("Guest count")},
        {u"checkList"_s, QT_TR_NOOP("Open checks")},
        {u"paymentPanel"_s, QT_TR_NOOP("Payment panel")},
        {u"numPad"_s, QT_TR_NOOP("Number keypad")},
        {u"keyboard"_s, QT_TR_NOOP("Keyboard")},
        {u"splitCheck"_s, QT_TR_NOOP("Split check")},
        {u"drawerPanel"_s, QT_TR_NOOP("Cash drawer")},
        {u"reportView"_s, QT_TR_NOOP("Reports")},
        {u"endOfDay"_s, QT_TR_NOOP("End of day")},
        {u"logoutPanel"_s, QT_TR_NOOP("Log out panel")},
        {u"clock"_s, QT_TR_NOOP("Clock")},
        {u"statusBar"_s, QT_TR_NOOP("Status bar")},
        {u"adminPanel"_s, QT_TR_NOOP("Manager editor")},
        {u"kitchenDisplay"_s, QT_TR_NOOP("Kitchen display")},
        {u"customerInfo"_s, QT_TR_NOOP("Customer details")},
        {u"customerLookup"_s, QT_TR_NOOP("Customer lookup")},
        {u"giftCard"_s, QT_TR_NOOP("Gift cards")},
        {u"waitlist"_s, QT_TR_NOOP("Waitlist")},
        {u"schedule"_s, QT_TR_NOOP("Schedule")},
        {u"factoryReset"_s, QT_TR_NOOP("Start over (erase)")},
        {u"messageComposer"_s, QT_TR_NOOP("Messages")},
        {u"network"_s, QT_TR_NOOP("Network")},
        {u"receiveDelivery"_s, QT_TR_NOOP("Receive a delivery")},
        {u"stockCount"_s, QT_TR_NOOP("Count stock")},
        {u"wasteLog"_s, QT_TR_NOOP("Log waste")},
        {u"storeCheck"_s, QT_TR_NOOP("Store check")},
        {u"prepLabels"_s, QT_TR_NOOP("Prep labels")},
        {u"checkSearch"_s, QT_TR_NOOP("Find a check")},
        {u"orderLater"_s, QT_TR_NOOP("Order for later")},
        {u"menuGrid"_s, QT_TR_NOOP("Menu (self-filling)")},
        {u"menuCategories"_s, QT_TR_NOOP("Menu categories (self-filling)")},
        {u"menuBuilder"_s, QT_TR_NOOP("Menu Builder")},
        {u"timeClock"_s, QT_TR_NOOP("Time clock")},
        {u"dashboard"_s, QT_TR_NOOP("Dashboard")},
        {u"checklist"_s, QT_TR_NOOP("Checklists")},
        {u"hostStand"_s, QT_TR_NOOP("Seating (host stand)")},
        {u"deliveryBoard"_s, QT_TR_NOOP("Deliveries board")},
        {u"decor"_s, QT_TR_NOOP("Floor plan piece")},
        {u"floorPlan"_s, QT_TR_NOOP("Floor plans (with tabs)")},
        {u"tableKey"_s, QT_TR_NOOP("Table colors (what they mean)")},
    };
    const auto it = names.constFind(kind);
    return it == names.cend() ? kind : tr(*it);
}
QStringList EditorController::pageKinds() const { return vt::layout::schema::pageKinds(); }

QString EditorController::pageKindName(const QString &kind) const
{
    for (const QJsonValue &o : vt::layout::schema::pageKindOptions())
        if (o.toObject().value(u"value"_s).toString() == kind)
            return schemaText(o.toObject().value(u"text"_s).toString());
    return kind;
}

QString EditorController::pageRoleName(const QString &role) const
{
    for (const QJsonValue &o : vt::layout::schema::pageRoleOptions())
        if (o.toObject().value(u"value"_s).toString() == role)
            return schemaText(o.toObject().value(u"text"_s).toString());
    return role;
}

// --- selection -----------------------------------------------------------------

void EditorController::select(const QString &zoneId, bool additive)
{
    QStringList ids = additive ? selection_ : QStringList();
    if (additive && ids.contains(zoneId))
        ids.removeAll(zoneId);
    else
        ids.append(zoneId);
    setSelection(ids);
}

void EditorController::selectOnly(const QStringList &zoneIds)
{
    setSelection(zoneIds);
}

void EditorController::selectInRect(qreal x, qreal y, qreal w, qreal h, bool additive)
{
    const Page *page = editor_.layout().page(pageId_);
    if (!page)
        return;
    const QRect band = QRectF(x, y, w, h).normalized().toAlignedRect();
    QStringList ids = additive ? selection_ : QStringList();
    for (const Zone &z : page->zones) {
        if (band.intersects(z.rect))
            ids.append(z.id);
    }
    setSelection(ids);
}

void EditorController::selectAll()
{
    QStringList ids;
    if (const Page *page = editor_.layout().page(pageId_)) {
        for (const Zone &z : page->zones)
            ids.append(z.id);
    }
    setSelection(ids);
}

void EditorController::clearSelection()
{
    setSelection({});
}

// --- zone editing --------------------------------------------------------------

QString EditorController::addZone(const QString &kind)
{
    const QString id = editor_.addZone(pageId_, kind);
    if (!id.isEmpty())
        setSelection({id});
    return id;
}

void EditorController::deleteSelection()
{
    if (!selection_.isEmpty())
        editor_.deleteZones(pageId_, selection_);
}

void EditorController::duplicateSelection()
{
    const QStringList ids = editor_.duplicateZones(pageId_, selection_);
    if (!ids.isEmpty())
        setSelection(ids);
}

void EditorController::nudge(int dx, int dy)
{
    if (!selection_.isEmpty())
        editor_.nudgeZones(pageId_, selection_, dx, dy);
}

void EditorController::commitRects(const QVariantMap &rects)
{
    QHash<QString, QRect> map;
    for (auto it = rects.begin(); it != rects.end(); ++it) {
        const QVariantMap r = it.value().toMap();
        map.insert(it.key(), QRect(qRound(r.value(u"x"_s).toDouble()), qRound(r.value(u"y"_s).toDouble()),
                                   qRound(r.value(u"w"_s).toDouble()), qRound(r.value(u"h"_s).toDouble())));
    }
    editor_.setZoneRects(pageId_, map);
}

void EditorController::rotateSelection(int degrees) { editor_.rotateZones(pageId_, selection_, degrees, true); }
void EditorController::setRotation(int degrees) { editor_.rotateZones(pageId_, selection_, degrees, false); }
void EditorController::bringToFront() { editor_.bringToFront(pageId_, selection_); }
void EditorController::sendToBack() { editor_.sendToBack(pageId_, selection_); }

void EditorController::align(const QString &how)
{
    static const QHash<QString, LayoutEditor::Align> names = {
        {u"left"_s, LayoutEditor::Align::Left}, {u"hcenter"_s, LayoutEditor::Align::HCenter},
        {u"right"_s, LayoutEditor::Align::Right}, {u"top"_s, LayoutEditor::Align::Top},
        {u"vcenter"_s, LayoutEditor::Align::VCenter}, {u"bottom"_s, LayoutEditor::Align::Bottom},
    };
    if (selection_.size() < 2)
        fail(tr("Select two or more zones to align."));
    else if (names.contains(how))
        editor_.align(pageId_, selection_, names.value(how));
}

void EditorController::distribute(const QString &axis)
{
    if (selection_.size() < 3)
        fail(tr("Select three or more zones to distribute."));
    else
        editor_.distribute(pageId_, selection_, axis == u"vertical" ? Qt::Vertical : Qt::Horizontal);
}

void EditorController::matchSize(const QString &which)
{
    if (selection_.size() < 2)
        fail(tr("Select two or more zones; the first one sets the size."));
    else
        editor_.matchSize(pageId_, selection_, which != u"height", which != u"width");
}

void EditorController::copy()
{
    editor_.copyZones(pageId_, selection_);
    emit stateChanged();
}

void EditorController::cut()
{
    copy();
    deleteSelection();
}

void EditorController::paste()
{
    const QStringList ids = editor_.paste(pageId_);
    if (!ids.isEmpty())
        setSelection(ids);
}

void EditorController::undo() { editor_.undoStack()->undo(); }
void EditorController::redo() { editor_.undoStack()->redo(); }

// --- inspector -------------------------------------------------------------------

QVariantList EditorController::zoneFields(const QString &kind) const
{
    QVariantList fields = translated(vt::layout::schema::zoneFields(kind));
    // How phones lay pages out by themselves: not for a phone version.
    if (const Page *page = editor_.layout().page(pageId_); page && !page->variantOf.isEmpty())
        fields.removeIf([](const QVariant &f) { return f.toMap().value(u"path"_s).toString().startsWith(u"phone."_s); });
    return fields;
}

QVariantList EditorController::pageFields() const
{
    QVariantList fields = translated(vt::layout::schema::pageFields());
    if (mealPeriods_.isEmpty())
        return fields;
    QVariantList choices{QVariantMap{{u"value"_s, QString()}, {u"text"_s, tr("(none)")}}};
    for (const QVariant &v : mealPeriods_) {
        const QVariantMap m = v.toMap();
        choices.append(QVariantMap{{u"value"_s, m.value(u"id"_s)}, {u"text"_s, m.value(u"name"_s)}});
    }
    choices.append(QVariantMap{{u"value"_s, u"all"_s}, {u"text"_s, tr("All day")}});
    for (QVariant &f : fields) {
        QVariantMap m = f.toMap();
        if (m.value(u"path"_s) == u"mealPeriod"_s) {
            m.insert(u"options"_s, choices);
            f = m;
        }
    }
    return fields;
}

QVariantList EditorController::themeFields() const
{
    return translated(vt::layout::schema::themeFields());
}

QVariantList EditorController::actionTypes() const
{
    return translated(vt::layout::schema::actionTypes());
}

QVariantList EditorController::pageOptions() const
{
    QVariantList out{QVariantMap{{u"value"_s, QString()}, {u"text"_s, tr("(none)")}}};
    for (const Page &p : editor_.layout().pages)
        out.append(QVariantMap{{u"value"_s, p.id}, {u"text"_s, u"%1  ·  %2"_s.arg(p.name, p.id)}});
    return out;
}

QVariantMap EditorController::fieldInfo(const QString &target, const QString &path) const
{
    const Layout &l = editor_.layout();
    QJsonValue value(QJsonValue::Undefined);
    QJsonValue resolved(QJsonValue::Undefined);
    bool mixed = false;

    const QStringList parts = path.split(u'.');
    const auto state = parts.size() == 3 && parts[0] == u"style" ? parseState(parts[1]) : std::nullopt;

    if (target == u"zone") {
        const Page *page = l.page(pageId_);
        if (!page || selection_.isEmpty())
            return {};
        bool first = true;
        for (const QString &id : selection_) {
            const QJsonValue v = editor_.zoneField(pageId_, id, path);
            if (first) {
                value = v;
                first = false;
            } else if (v != value) {
                mixed = true;
            }
        }
        if (state) {
            if (const Zone *z = page->zone(selection_.first()))
                resolved = l.resolveStyle(*z, pageId_, *state).value(parts[2]);
        }
    } else if (target == u"page") {
        value = editor_.pageField(pageId_, path);
        if (parts.size() == 2 && parts[0] == u"background") {
            // What the page would show without its own value.
            Layout without = l;
            if (Page *p = without.page(pageId_))
                p->background.remove(parts[1]);
            resolved = without.resolveBackground(pageId_).value(parts[1]);
        } else if (state) {
            Layout without = l;
            if (Page *p = without.page(pageId_))
                p->style.state(*state).remove(parts[2]);
            Zone probe;
            resolved = without.resolveStyle(probe, pageId_, *state).value(parts[2]);
        }
    } else if (target == u"theme") {
        value = editor_.themeField(path);
    }

    return {{u"value"_s, value.toVariant()}, {u"mixed"_s, mixed}, {u"resolved"_s, resolved.toVariant()},
            {u"isSet"_s, !value.isUndefined()}};
}

bool EditorController::setField(const QString &target, const QString &path, const QVariant &value)
{
    const QJsonValue v = toJson(value);
    if (target == u"zone") {
        if (selection_.isEmpty() || !editor_.setZoneField(pageId_, selection_, path, v))
            return false;
        warnOwnOnPhone(path);
        return true;
    }
    if (target == u"theme")
        return editor_.setThemeField(path, v);
    if (target == u"page") {
        QString why;
        const QString before = pageId_;
        if (!editor_.setPageField(pageId_, path, v, &why))
            return why.isEmpty() ? false : fail(why);
        if (path == u"id" && !editor_.layout().page(before))
            emit showPageRequested(v.toString());
        return true;
    }
    return false;
}

bool EditorController::clearField(const QString &target, const QString &path)
{
    return setField(target, path, QVariant());
}

QVariantList EditorController::actions() const
{
    if (selection_.size() != 1)
        return {};
    return editor_.zoneField(pageId_, selection_.first(), u"actions"_s).toArray().toVariantList();
}

void EditorController::setActions(const QVariantList &actions)
{
    if (selection_.size() == 1)
        editor_.setZoneField(pageId_, selection_, u"actions"_s, QJsonArray::fromVariantList(actions));
}

// --- pages ---------------------------------------------------------------------------

QString EditorController::newPage(const QString &name, const QString &kind, const QString &templateId)
{
    const QString id = editor_.addPage(name, kind, templateId);
    if (!id.isEmpty())
        emit showPageRequested(id);
    return id;
}

QString EditorController::duplicatePage()
{
    const QString id = editor_.duplicatePage(pageId_);
    if (!id.isEmpty())
        emit showPageRequested(id);
    return id;
}

bool EditorController::deletePage()
{
    QString why;
    if (!editor_.deletePage(pageId_, &why))
        return fail(why);
    return true;   // LayoutController leaves the vanished page
}

QString EditorController::templateOf(const QString &zoneId) const
{
    for (const Layout::PlacedZone &pz : editor_.layout().effectiveZones(pageId_)) {
        if (pz.zone->id == zoneId && pz.inherited)
            return pz.owner->id;
    }
    return {};
}

// --- phone versions ----------------------------------------------------------------

QVariantMap EditorController::screens() const
{
    const Layout &l = editor_.layout();
    const Page *page = l.page(pageId_);
    if (!page)
        return {};
    const bool isPhone = !page->variantOf.isEmpty() && page->formFactor == u"phone";
    const QString base = isPhone ? page->variantOf : page->id;
    const Page *phone = isPhone ? page : l.variantFor(base, u"phone"_s);
    return {{u"page"_s, base}, {u"phone"_s, phone ? phone->id : QString()}, {u"isPhone"_s, isPhone},
            {u"canPhone"_s, page->variantOf.isEmpty() || isPhone}};
}

void EditorController::showScreen(const QString &which)
{
    const QVariantMap s = screens();
    const QString id = which == u"phone" ? s.value(u"phone"_s).toString() : s.value(u"page"_s).toString();
    if (!id.isEmpty() && id != pageId_)
        emit showPageRequested(id);
}

bool EditorController::makePhoneVersion()
{
    QString why;
    const QString id = editor_.makePhoneVersion(pageId_, &why);
    if (id.isEmpty())
        return why.isEmpty() ? false : fail(why);
    setNotice(tr("Phone version made: move and size its buttons; their words, colors and actions stay shared"));
    emit showPageRequested(id);
    return true;
}

bool EditorController::removePhoneVersion()
{
    const QVariantMap s = screens();
    const QString phone = s.value(u"phone"_s).toString();
    if (phone.isEmpty())
        return false;
    QString why;
    if (!editor_.deletePage(phone, &why))
        return fail(why);
    emit showPageRequested(s.value(u"page"_s).toString());
    return true;
}

QVariantList EditorController::notOnPhone() const
{
    QVariantList out;
    const Layout &l = editor_.layout();
    for (const Layout::NotShown &n : l.notOnVariant(pageId_)) {
        const Zone &z = *n.zone;
        out.append(QVariantMap{{u"id"_s, z.id}, {u"label"_s, z.label.simplified()},
                               {u"kind"_s, kindName(z.kind)}, {u"hidden"_s, n.hidden}});
    }
    return out;
}

void EditorController::placeOnPhone(const QStringList &zoneIds)
{
    if (editor_.placeOnVariant(pageId_, zoneIds))
        setSelection(zoneIds);
}

void EditorController::hideOnPhone(const QStringList &zoneIds)
{
    editor_.hideOnVariant(pageId_, zoneIds);
}

QVariantMap EditorController::phoneShare() const
{
    const Page *page = editor_.layout().page(pageId_);
    if (!page || page->variantOf.isEmpty())
        return {{u"shared"_s, 0}};
    int shared = 0;
    QStringList own;
    bool first = true;
    for (const QString &id : selection_) {
        const Zone *z = page->zone(id);
        if (!z || !z->shared)
            continue;
        ++shared;
        if (first)
            own = z->own;
        else
            own.removeIf([&](const QString &p) { return !z->own.contains(p); });
        first = false;
    }
    return {{u"shared"_s, shared}, {u"own"_s, own}};
}

void EditorController::setOwnOnPhone(const QString &path, bool own)
{
    editor_.setOwnOnVariant(pageId_, selection_, path, own);
}

namespace {

// A field a phone version can keep for itself, as the editor names it.
QString ownName(const QString &path)
{
    if (path == u"label")
        return EditorController::tr("words");
    if (path == u"style.normal.fontSize")
        return EditorController::tr("text size");
    return path;
}

QString shownValue(const QJsonValue &v)
{
    if (v.isUndefined() || v.isNull())
        return EditorController::tr("(the page's look)");
    return v.isString() ? v.toString().simplified() : v.toVariant().toString();
}

} // namespace

QVariantList EditorController::phoneDifferences() const
{
    QVariantList out;
    const Layout &l = editor_.layout();
    const Page *page = l.page(pageId_);
    if (!page || page->variantOf.isEmpty())
        return out;
    for (const Zone &z : page->zones) {
        const QString label = z.label.isEmpty() ? z.id : z.label.simplified();
        if (!z.shared) {
            if (z.kind != u"comment" && z.kind != u"decor")
                out.append(QVariantMap{{u"id"_s, z.id}, {u"label"_s, label}, {u"phoneOnly"_s, true},
                                       {u"what"_s, kindName(z.kind)}});
            continue;
        }
        const Layout::PlacedZone from = l.sourceOf(*page, z.id);
        if (!from.zone)
            continue;
        const QJsonObject theirs = from.zone->toJson();
        const QJsonObject mine = z.toJson();
        for (const QString &path : z.own) {
            const QJsonValue a = vt::layout::jsonGet(theirs, path);
            const QJsonValue b = vt::layout::jsonGet(mine, path);
            out.append(QVariantMap{{u"id"_s, z.id}, {u"label"_s, label}, {u"phoneOnly"_s, false},
                                   {u"path"_s, path}, {u"what"_s, ownName(path)},
                                   {u"standard"_s, shownValue(a)}, {u"phone"_s, shownValue(b)},
                                   {u"same"_s, a == b}});
        }
    }
    return out;
}

void EditorController::followPage(const QString &zoneId, const QString &path)
{
    editor_.setOwnOnVariant(pageId_, {zoneId}, path, false);
}

// Changed on the standard page, but phones keep their own: say so, so the
// two don't drift apart unnoticed.
void EditorController::warnOwnOnPhone(const QString &path)
{
    const Layout &l = editor_.layout();
    const Page *page = l.page(pageId_);
    if (!page || !page->variantOf.isEmpty())
        return;
    const Page *phone = l.variantFor(pageId_, u"phone"_s);
    if (!phone)
        return;
    for (const QString &id : selection_) {
        const Zone *z = phone->zone(id);
        if (!z || !z->shared)
            continue;
        for (const QString &own : z->own) {
            if (own == path || own.startsWith(path + u'.') || path.startsWith(own + u'.')) {
                setNotice(tr("Phones keep their own %1 for “%2”: still “%3” there. Change it on Phone too, or make it the same there (Phone version ▾).")
                              .arg(ownName(own), z->label.isEmpty() ? z->id : z->label.simplified(),
                                   shownValue(vt::layout::jsonGet(z->toJson(), own))));
                return;
            }
        }
    }
}

// --- files ---------------------------------------------------------------------------

QJsonObject EditorController::withPictures(QJsonObject json) const
{
    if (!pos_)
        return json;
    // The store pictures the pages name ("store:menu-board.png"), and every
    // store font (pages name fonts by family, not file).
    QSet<QString> wanted;
    static const QRegularExpression ref(uR"re("store:([^"]+)")re"_s);
    const QString text = QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact));
    for (auto it = ref.globalMatch(text); it.hasNext();)
        wanted.insert(it.next().captured(1));
    for (const QVariant &v : pos_->storeImages())
        if (v.toMap().value(u"kind"_s).toString() == u"font")
            wanted.insert(v.toMap().value(u"name"_s).toString());
    QJsonObject pictures;
    for (const QString &name : std::as_const(wanted)) {
        QFile f(QUrl(pos_->imageUrl(u"store:"_s + name)).toLocalFile());
        if (f.open(QIODevice::ReadOnly))
            pictures.insert(name, QString::fromLatin1(f.readAll().toBase64()));
    }
    if (!pictures.isEmpty())
        json.insert(u"images"_s, pictures);
    return json;
}

int EditorController::addPictures(QJsonObject *json)
{
    const QJsonObject pictures = json->value(u"images"_s).toObject();
    json->remove(u"images"_s);   // not part of the pages themselves
    if (!pos_ || pictures.isEmpty())
        return 0;
    QSet<QString> have;
    for (const QVariant &v : pos_->storeImages())
        have.insert(v.toMap().value(u"name"_s).toString());
    int added = 0;
    for (auto it = pictures.begin(); it != pictures.end(); ++it) {
        if (have.contains(it.key()) || !it.value().isString())
            continue;
        pos_->invoke(u"addStoreImage"_s, {it.key(), it.value().toString()});
        ++added;
    }
    return added;
}

QVariantMap EditorController::previewInfo() const
{
    const Layout &l = editor_.layout();
    const vt::layout::Page *page = l.page(pageId_);
    if (!page)
        return {};
    int smallest = 0;
    for (const Layout::PlacedZone &pz : l.effectiveZones(pageId_)) {
        const vt::layout::Zone &z = *pz.zone;
        if (z.kind != u"button" || z.actions.isEmpty() || z.rect.isEmpty())
            continue;
        const int side = std::min(z.rect.width(), z.rect.height());
        smallest = smallest == 0 ? side : std::min(smallest, side);
    }
    QString phone;
    for (const vt::layout::Page &p : l.pages)
        if (p.variantOf == page->id && p.formFactor == u"phone")
            phone = p.name;
    return {{u"canvasW"_s, page->canvas.width()}, {u"canvasH"_s, page->canvas.height()}, {u"smallest"_s, smallest},
            {u"phone"_s, phone}, {u"isPhone"_s, page->formFactor == u"phone"}};
}

bool EditorController::exportPage(const QUrl &file)
{
    QString why;
    if (!writeJsonFile(file, withPictures(editor_.exportPage(pageId_)), &why))
        return fail(why);
    setNotice(tr("Exported page to %1").arg(file.fileName()));
    return true;
}

bool EditorController::importPage(const QUrl &file)
{
    QString why;
    auto json = readJsonFile(file, &why);
    if (!json)
        return fail(why);
    addPictures(&*json);
    const QString id = editor_.importPage(*json, &why);
    if (id.isEmpty())
        return fail(why);
    emit showPageRequested(id);
    return true;
}

namespace {
// The ready-made layouts shipped with the app for this page (qrc:/seed/layouts).
QJsonObject arrangementsFor(const vt::layout::Page *page)
{
    if (!page)
        return {};
    const QDir dir(u":/seed/layouts"_s);
    for (const QString &name : dir.entryList({u"*.json"_s}, QDir::Files)) {
        QFile f(dir.filePath(name));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        const QString role = o.value(u"role").toString();
        if ((!role.isEmpty() && role == page->role) || (role.isEmpty() && o.value(u"page").toString() == page->id))
            return o;
    }
    return {};
}
} // namespace

QString EditorController::arrangementPage() const
{
    // This page's own, else its template's (a menu page: the order screen around it).
    const vt::layout::Layout &l = editor_.layout();
    for (const vt::layout::Page *p = l.page(pageId_); p; p = l.page(p->templateId)) {
        if (!arrangementsFor(p).isEmpty())
            return p->id;
        if (p->templateId.isEmpty() || p->templateId == p->id)
            break;
    }
    return {};
}

QVariantList EditorController::arrangements() const
{
    QVariantList out;
    const vt::layout::Page *target = editor_.layout().page(arrangementPage());
    for (const QJsonValue &v : arrangementsFor(target).value(u"variants").toArray()) {
        QVariantMap m = v.toObject().toVariantMap();
        m.insert(u"pageName"_s, target->name);
        out.append(m);
    }
    return out;
}

bool EditorController::useArrangement(const QString &id)
{
    const QString target = arrangementPage();
    for (const QJsonValue &v : arrangementsFor(editor_.layout().page(target)).value(u"variants").toArray()) {
        const QJsonObject a = v.toObject();
        if (a.value(u"id").toString() != id)
            continue;
        if (!editor_.arrangePage(target, a.value(u"zones").toArray(), a.value(u"background").toObject(),
                                 tr("Layout: %1").arg(a.value(u"name").toString())))
            return fail(tr("Could not use that layout."));
        setNotice(tr("Layout: %1 (Undo takes it back; Save keeps it)").arg(a.value(u"name").toString()));
        return true;
    }
    return fail(tr("There is no such layout for this page."));
}

bool EditorController::importPageHere(const QUrl &file)
{
    QString why;
    auto json = readJsonFile(file, &why);
    if (!json)
        return fail(why);
    addPictures(&*json);
    QStringList errors;
    if (!Layout::checkSchema(*json, u"page"_s, &errors))
        return fail(errors.join(u'\n'));
    const vt::layout::Page imported = vt::layout::Page::fromJson(*json);
    if (imported.zones.isEmpty())
        return fail(tr("The file does not contain a page."));
    QJsonArray zones;
    for (const vt::layout::Zone &z : imported.zones)
        zones.append(z.toJson());
    if (!editor_.arrangePage(pageId_, zones, imported.background, tr("Use page file")))
        return fail(tr("Could not use that page."));
    setNotice(tr("This page now looks like %1 (Undo takes it back; Save keeps it)").arg(file.fileName()));
    return true;
}

bool EditorController::exportLayout(const QUrl &file)
{
    QString why;
    const QJsonObject out = withPictures(editor_.layout().toJson());
    if (!writeJsonFile(file, out, &why))
        return fail(why);
    const int pictures = out.value(u"images"_s).toObject().size();
    setNotice(pictures ? tr("Exported all pages to %1, with %n picture(s) and font(s)", nullptr, pictures).arg(file.fileName())
                       : tr("Exported all pages to %1").arg(file.fileName()));
    return true;
}

bool EditorController::importLayout(const QUrl &file)
{
    QString why;
    auto json = readJsonFile(file, &why);
    if (!json)
        return fail(why);
    const int pictures = addPictures(&*json);
    QStringList errors;
    const auto imported = Layout::fromJson(*json, &errors);
    if (!imported || imported->pages.isEmpty())
        return fail(errors.isEmpty() ? tr("The file does not contain a layout.") : errors.join(u'\n'));
    if (!imported->pageByRole(u"login"_s))
        return fail(tr("That layout has no login page."));
    editor_.replaceLayout(*imported, tr("Import layout"));
    if (pictures)
        setNotice(tr("Imported every page, and added %n picture(s) and font(s) to the store", nullptr, pictures));
    return true;
}
