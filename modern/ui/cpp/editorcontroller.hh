#pragma once

#include "app/layout_editor.hh"

#include <QJsonObject>

namespace vt::app { class PosSession; }

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// QML face of the page editor: wraps LayoutEditor with the current page,
// the selection, and QVariant-friendly calls for the toolbar, canvas overlay,
// inspector, and page list. Created by LayoutController on entering edit mode.
class EditorController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by LayoutController")

    Q_PROPERTY(QString pageId READ pageId NOTIFY pageChanged)
    Q_PROPERTY(QStringList selection READ selection NOTIFY selectionChanged)
    Q_PROPERTY(QString selectionKind READ selectionKind NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList geometry READ geometry NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList pages READ pages NOTIFY layoutChanged)
    Q_PROPERTY(QStringList issues READ issues NOTIFY layoutChanged)
    Q_PROPERTY(QStringList references READ references NOTIFY layoutChanged)
    // The page being edited on standard screens and on phones:
    // {page, phone (its phone version, or ""), isPhone, canPhone}.
    Q_PROPERTY(QVariantMap screens READ screens NOTIFY revisionChanged)
    // Bumps on any layout, page, or selection change; inspector bindings
    // read it to re-query field values.
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY stateChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY stateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY stateChanged)
    Q_PROPERTY(QString undoText READ undoText NOTIFY stateChanged)
    Q_PROPERTY(QString redoText READ redoText NOTIFY stateChanged)
    Q_PROPERTY(bool hasClipboard READ hasClipboard NOTIFY stateChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
    Q_PROPERTY(QStringList textures READ textures CONSTANT)
    Q_PROPERTY(QStringList basicKinds READ basicKinds CONSTANT)
    Q_PROPERTY(QStringList widgetKinds READ widgetKinds CONSTANT)
    Q_PROPERTY(QStringList pageKinds READ pageKinds CONSTANT)

public:
    explicit EditorController(vt::layout::Layout base, QObject *parent = nullptr);

    vt::app::LayoutEditor &editor() { return editor_; }
    const vt::layout::Layout &layout() const { return editor_.layout(); }

    QString pageId() const { return pageId_; }
    void setPageId(const QString &pageId);

    QStringList selection() const { return selection_; }
    QString selectionKind() const;
    QVariantList geometry() const;
    QVariantList pages() const;
    QStringList issues() const { return editor_.layout().validate(); }
    QStringList references() const { return editor_.referencesTo(pageId_); }
    int revision() const { return revision_; }
    bool dirty() const { return editor_.isDirty(); }
    bool canUndo() const;
    bool canRedo() const;
    QString undoText() const;
    QString redoText() const;
    bool hasClipboard() const { return editor_.hasClipboard(); }
    QString notice() const { return notice_; }
    QStringList textures() const;
    QStringList basicKinds() const;
    Q_INVOKABLE QString kindName(const QString &kind) const;
    // A page kind's / role's name on screen ("manager" -> "Manager").
    Q_INVOKABLE QString pageKindName(const QString &kind) const;
    Q_INVOKABLE QString pageRoleName(const QString &role) const;
    QStringList widgetKinds() const;
    QStringList pageKinds() const;

    // --- selection ---
    Q_INVOKABLE void select(const QString &zoneId, bool additive);
    Q_INVOKABLE void selectOnly(const QStringList &zoneIds);
    Q_INVOKABLE void selectInRect(qreal x, qreal y, qreal w, qreal h, bool additive);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();

    // --- zone editing (current page, current selection) ---
    Q_INVOKABLE QString addZone(const QString &kind);
    Q_INVOKABLE void deleteSelection();
    Q_INVOKABLE void duplicateSelection();
    Q_INVOKABLE void nudge(int dx, int dy);
    Q_INVOKABLE void commitRects(const QVariantMap &rects);   // id -> {x,y,w,h}
    Q_INVOKABLE void rotateSelection(int degrees);   // by, clockwise
    Q_INVOKABLE void setRotation(int degrees);       // to (the rotate handle)
    Q_INVOKABLE void bringToFront();
    Q_INVOKABLE void sendToBack();
    Q_INVOKABLE void align(const QString &how);       // left|hcenter|right|top|vcenter|bottom
    Q_INVOKABLE void distribute(const QString &axis); // horizontal|vertical
    Q_INVOKABLE void matchSize(const QString &which); // width|height|both
    Q_INVOKABLE void copy();
    Q_INVOKABLE void cut();
    Q_INVOKABLE void paste();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // --- inspector ---
    Q_INVOKABLE QVariantList zoneFields(const QString &kind) const;
    // The store's meal periods, offered for index pages' meal period.
    void setMealPeriods(const QVariantList &periods) { mealPeriods_ = periods; }
    Q_INVOKABLE QVariantList pageFields() const;
    Q_INVOKABLE QVariantList themeFields() const;
    Q_INVOKABLE QVariantList actionTypes() const;
    Q_INVOKABLE QVariantList pageOptions() const;
    // target: "zone" (selection) | "page" | "theme".
    // Returns {value, mixed, resolved}; resolved is the inherited value.
    Q_INVOKABLE QVariantMap fieldInfo(const QString &target, const QString &path) const;
    Q_INVOKABLE bool setField(const QString &target, const QString &path, const QVariant &value);
    Q_INVOKABLE bool clearField(const QString &target, const QString &path);
    Q_INVOKABLE QVariantList actions() const;
    Q_INVOKABLE void setActions(const QVariantList &actions);

    // --- pages ---
    Q_INVOKABLE QString newPage(const QString &name, const QString &kind, const QString &templateId);
    Q_INVOKABLE QString duplicatePage();
    Q_INVOKABLE bool deletePage();
    Q_INVOKABLE QString templateOf(const QString &zoneId) const;   // owner page of an inherited zone

    // --- phone versions (the Standard | Phone switch) ---
    QVariantMap screens() const;
    // "standard" | "phone": edit that screen's version of this page.
    Q_INVOKABLE void showScreen(const QString &which);
    // A phone version laid out as phones show the page now, then edited.
    Q_INVOKABLE bool makePhoneVersion();
    // Phones go back to laying the page out by themselves.
    Q_INVOKABLE bool removePhoneVersion();
    // On a phone version: the page's buttons not on it,
    // [{id, label, kind, hidden}] (hidden: left off on purpose).
    Q_INVOKABLE QVariantList notOnPhone() const;
    Q_INVOKABLE void placeOnPhone(const QStringList &zoneIds);
    Q_INVOKABLE void hideOnPhone(const QStringList &zoneIds);
    // The selection on a phone version: {shared (how many of it are the
    // page's), own: [fields every one of them keeps for phones]}.
    Q_INVOKABLE QVariantMap phoneShare() const;
    Q_INVOKABLE void setOwnOnPhone(const QString &path, bool own);
    // On a phone version, where it differs from the page: a button's own
    // fields, [{id, label, path, what, standard, phone, same}], and the
    // buttons only it has, [{id, label, what, phoneOnly: true}].
    Q_INVOKABLE QVariantList phoneDifferences() const;
    // A button's own field on the phone follows the page's again.
    Q_INVOKABLE void followPage(const QString &zoneId, const QString &path);

    // --- files ---
    Q_INVOKABLE bool exportPage(const QUrl &file);
    Q_INVOKABLE bool importPage(const QUrl &file);
    Q_INVOKABLE bool exportLayout(const QUrl &file);
    // For Preview: this page's canvas, its smallest button (canvas units),
    // and its phone version if it has one: {canvasW, canvasH, smallest, phone}.
    Q_INVOKABLE QVariantMap previewInfo() const;
    // The store's pictures and fonts, for page and layout files to carry.
    void setPos(vt::app::PosSession *pos) { pos_ = pos; }
    Q_INVOKABLE bool importLayout(const QUrl &file);
    // Ready-made layouts for the page being edited (by its role, else its id):
    // [{id, name, description, zones, background}].
    Q_INVOKABLE QVariantList arrangements() const;
    Q_INVOKABLE bool useArrangement(const QString &id);
    // The page those layouts are for: this one, or its template.
    QString arrangementPage() const;
    // A page file (Export this page) used for the page being edited.
    Q_INVOKABLE bool importPageHere(const QUrl &file);

    Q_INVOKABLE void setNotice(const QString &text);

signals:
    void pageChanged();
    void selectionChanged();
    void layoutChanged();
    void revisionChanged();
    void stateChanged();
    void noticeChanged();
    // The editor wants the app to show another page (new/duplicated page).
    void showPageRequested(const QString &pageId);

private:
    QVariantList mealPeriods_;
    void onLayoutChanged();
    void setSelection(QStringList ids);
    void bump();
    bool fail(const QString &why);
    void warnOwnOnPhone(const QString &path);

    vt::app::LayoutEditor editor_;
    QString pageId_;
    QStringList selection_;
    QString notice_;
    int revision_ = 0;
    vt::app::PosSession *pos_ = nullptr;
    // Page and layout files carry the store pictures they use and the store's
    // fonts ("images": {name: base64}); a file's are added where missing.
    QJsonObject withPictures(QJsonObject json) const;
    int addPictures(QJsonObject *json);
};
