#pragma once

#include "layout/layout.hh"

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QUndoStack>

#include <functional>

namespace vt::app {

// Edits a draft copy of the layout. Every change is an undoable snapshot on
// the undo stack; the running app keeps using the saved layout until the
// draft is committed. All operations address zones by page id + zone ids so
// this class has no UI state and is fully unit-testable.
class LayoutEditor {
public:
    enum class Align { Left, HCenter, Right, Top, VCenter, Bottom };

    explicit LayoutEditor(layout::Layout base);
    ~LayoutEditor();
    LayoutEditor(const LayoutEditor &) = delete;
    LayoutEditor &operator=(const LayoutEditor &) = delete;

    const layout::Layout &layout() const { return layout_; }
    QUndoStack *undoStack() { return &undo_; }
    const QUndoStack *undoStack() const { return &undo_; }
    bool isDirty() const { return !undo_.isClean(); }
    void markClean() { undo_.setClean(); }

    // Called after every change, including undo and redo.
    void setChangedCallback(std::function<void()> callback) { changed_ = std::move(callback); }

    // --- zones ------------------------------------------------------------
    // Empty rect: a default size centered on the canvas, nudged off any
    // zone already occupying that spot. Returns the new zone id.
    QString addZone(const QString &pageId, const QString &kind, QRect rect = {});
    QStringList duplicateZones(const QString &pageId, const QStringList &zoneIds);
    bool deleteZones(const QString &pageId, const QStringList &zoneIds);
    // Commit a drag or resize. Rects are clamped to the canvas.
    bool setZoneRects(const QString &pageId, const QHash<QString, QRect> &rects);
    // Consecutive nudges of the same zones merge into one undo step.
    bool nudgeZones(const QString &pageId, const QStringList &zoneIds, int dx, int dy);
    // Turn zones about their middles: by `degrees` (by), or to it.
    bool rotateZones(const QString &pageId, const QStringList &zoneIds, int degrees, bool by);
    bool bringToFront(const QString &pageId, const QStringList &zoneIds);
    bool sendToBack(const QString &pageId, const QStringList &zoneIds);
    // Align to the selection's bounding box.
    bool align(const QString &pageId, const QStringList &zoneIds, Align how);
    // Equal gaps between 3+ zones, ordered by position.
    bool distribute(const QString &pageId, const QStringList &zoneIds, Qt::Orientation orientation);
    // Size every zone like the first one in the list.
    bool matchSize(const QString &pageId, const QStringList &zoneIds, bool width, bool height);

    // Generic property edit by dotted path ("label", "style.normal.fill").
    // An undefined value removes the key (inherit). Applies to every zone.
    bool setZoneField(const QString &pageId, const QStringList &zoneIds, const QString &path,
                      const QJsonValue &value);
    QJsonValue zoneField(const QString &pageId, const QString &zoneId, const QString &path) const;

    // --- clipboard (not undoable itself; paste is) --------------------------
    void copyZones(const QString &pageId, const QStringList &zoneIds);
    bool hasClipboard() const { return !clipboard_.isEmpty(); }
    // Pasted zones get fresh ids; returns them.
    QStringList paste(const QString &pageId);

    // --- pages --------------------------------------------------------------
    QString addPage(const QString &name, const QString &kind, const QString &templateId = {});
    QString duplicatePage(const QString &pageId);
    bool deletePage(const QString &pageId, QString *why = nullptr);
    // Path "id" renames the page and rewrites every reference to it.
    bool setPageField(const QString &pageId, const QString &path, const QJsonValue &value,
                      QString *why = nullptr);
    QJsonValue pageField(const QString &pageId, const QString &path) const;
    // Human-readable list of what points at a page (templates, jumps, sequences).
    QStringList referencesTo(const QString &pageId) const;

    // --- theme --------------------------------------------------------------
    bool setThemeField(const QString &path, const QJsonValue &value);
    // The whole theme at once (a ready-made look), one undo step.
    bool setTheme(const layout::Theme &theme, const QString &text);
    QJsonValue themeField(const QString &path) const;

    // --- import / export ----------------------------------------------------
    QJsonObject exportPage(const QString &pageId) const;
    // Adds the page; a clashing id gets a suffix. Returns the id used.
    QString importPage(const QJsonObject &json, QString *why = nullptr);
    // A page's zones and background replaced (a ready-made layout, or a page
    // file used for this page); its id, name and role stay. One undo step.
    bool arrangePage(const QString &pageId, const QJsonArray &zones, const QJsonObject &background,
                     const QString &description);
    bool replaceLayout(const layout::Layout &layout, const QString &description);

    static QString slugify(const QString &text);

private:
    class SnapshotCommand;

    bool apply(const QString &text, const std::function<bool(layout::Layout &)> &mutate,
               const QString &mergeKey = {});
    void setLayout(const layout::Layout &layout);

    layout::Layout layout_;
    QUndoStack undo_;
    QList<layout::Zone> clipboard_;
    QString clipboardPage_;
    std::function<void()> changed_;
};

} // namespace vt::app
