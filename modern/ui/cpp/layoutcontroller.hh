#pragma once

#include "app/navigator.hh"
#include "app/pos_session.hh"
#include "cardreader.hh"
#include "editorcontroller.hh"
#include "layout/layout.hh"
#include "zonemodel.hh"

#include <QObject>
#include <QPointer>
#include <QSize>
#include <QTime>
#include <QSet>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>

namespace vt::storage { class LayoutStore; }

// Drives the page UI: owns the running layout and the navigator, publishes
// the current page to QML, and runs zone actions when touched. In edit mode
// it shows the editor's draft instead and ignores touches (the editor overlay
// handles them); the running layout only changes on saveEdits().
//
// Zone actions run in order through the POS session. A remote session
// answers later, so each action continues in its reply; a failed step, a
// page change, or entering edit mode ends the chain.
class LayoutController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")

    Q_PROPERTY(QString pageId READ pageId NOTIFY pageChanged)
    Q_PROPERTY(QString pageName READ pageName NOTIFY pageChanged)
    Q_PROPERTY(QString pageKind READ pageKind NOTIFY pageChanged)
    Q_PROPERTY(QSize canvasSize READ canvasSize NOTIFY pageChanged)
    Q_PROPERTY(int pageGrid READ pageGrid NOTIFY pageChanged)
    Q_PROPERTY(QVariantMap background READ background NOTIFY pageChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY pageChanged)
    Q_PROPERTY(ZoneModel *zones READ zones CONSTANT)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    // What a held button does: {title, text}; empty when nothing is explained.
    Q_PROPERTY(QVariantMap explanation READ explanation NOTIFY explanationChanged)
    Q_PROPERTY(bool editing READ editing NOTIFY editingChanged)
    // Screen saver: dimmed after no touches for a while (Store Settings).
    Q_PROPERTY(bool asleep READ asleep NOTIFY asleepChanged)
    Q_PROPERTY(EditorController *editor READ editor NOTIFY editorChanged)
    Q_PROPERTY(vt::app::PosSession *pos READ pos NOTIFY posChanged)
    // This terminal's card reader (Manager -> Terminals -> Card reader).
    Q_PROPERTY(CardReader *cardReader READ cardReader CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // "phone": phone versions of pages (and phone-sized layouts) are shown;
    // "standard": pages as designed.
    Q_PROPERTY(QString formFactor READ formFactor NOTIFY formFactorChanged)
    // Breakfast, lunch, dinner... now (zones can show only during one).
    Q_PROPERTY(QString mealPeriod READ mealPeriod NOTIFY mealPeriodChanged)
    // The theme's status colors (theme "status": tableMine, kitchenLate...).
    Q_PROPERTY(QVariantMap statusColors READ statusColors NOTIFY statusColorsChanged)
    // The menu category being shown (a self-filling menu with no category of
    // its own shows this one).
    Q_PROPERTY(QString menuCategory READ menuCategory WRITE setMenuCategory NOTIFY menuCategoryChanged)
    // The store's meal pages have category buttons placed by hand (new
    // items show up on none): the Menu Builder offers to switch them.
    Q_PROPERTY(bool menuScreensHandBuilt READ menuScreensHandBuilt NOTIFY menuScreensChanged)
    // Font families this screen can use: its own and the store's (installed here).
    Q_PROPERTY(QStringList fontFamilies READ fontFamilies NOTIFY fontsChanged)
    // The setup guide is showing (opens for managers until it's finished).
    Q_PROPERTY(bool setupOpen READ setupOpen NOTIFY setupOpenChanged)

public:
    // Persists a layout; false (with a message) when it could not.
    using Saver = std::function<bool(const vt::layout::Layout &, QString *error)>;

    explicit LayoutController(vt::layout::Layout layout, QObject *parent = nullptr);
    ~LayoutController() override;

    // Where saveEdits() goes. setStore() saves to a local database; a remote
    // terminal sends the layout to its server instead. Neither: edits apply
    // in memory only (tests, --layout).
    void setStore(vt::storage::LayoutStore *store);
    void setSaver(Saver saver) { saver_ = std::move(saver); }

    // Pages were changed elsewhere (another terminal saved): run them now.
    // While editing, the draft is kept; the new pages show when leaving
    // without saving.
    void replaceLayout(vt::layout::Layout layout);

    // The POS session that zone actions and widgets operate on. Not owned.
    // Without one, POS actions only show a status message (layout-only use).
    void setPos(vt::app::PosSession *pos);
    CardReader *cardReader() { return &cardReader_; }
    vt::app::PosSession *pos() const { return pos_; }

    QString pageId() const { return nav_.current(); }
    QString pageName() const;
    QString pageKind() const;
    QSize canvasSize() const;
    int pageGrid() const;
    QVariantMap background() const;
    bool canGoBack() const { return nav_.canGoBack(); }
    ZoneModel *zones() { return &zones_; }
    QString statusText() const { return status_; }
    bool editing() const { return editing_; }
    bool asleep() const { return asleep_; }
    // The touch that wakes the screen does nothing else (Main.qml's dim layer takes it).
    Q_INVOKABLE void wake();
    void setScreenSaverForTesting(int ms) { sleepOverrideMs_ = ms; restartSleep(); }
    EditorController *editor() const { return editor_; }
    bool busy() const { return pending_ > 0; }

    // The running layout, and what is on screen (draft while editing).
    const vt::layout::Layout &layout() const { return layout_; }
    const vt::layout::Layout &activeLayout() const;

    // Meal period for index jumps. Follows the clock and the store's meal
    // periods (checked every minute) unless set here.
    void setMealPeriod(const QString &period);
    // With the built-in periods, or with a session's mealPeriods list.
    static QString mealPeriodAt(QTime time);
    static QString mealPeriodAt(const QVariantList &periods, QTime time);

    QString formFactor() const { return formFactor_; }
    QString mealPeriod() const { return nav_.mealPeriod(); }
    QVariantMap statusColors() const { return statusColors_; }
    QStringList fontFamilies() const;
    bool setupOpen() const { return setupOpen_; }
    Q_INVOKABLE void openSetup();
    Q_INVOKABLE void closeSetup();
    // The store's fonts that came in, installed for this screen.
    void installStoreFonts();
    // A zone's "showWhen" rules hold now (login, check, check type, meal period, screen).
    bool ruleShows(const QJsonObject &rule) const;
    // Force "phone" or "standard" (command line); empty: the terminal's
    // setting, else automatic.
    void setFormFactorOverride(const QString &formFactor);
    // Tests: the idle log-out after `ms` instead of the store's minutes.
    void setIdleTimeoutForTesting(int ms) { idleOverrideMs_ = ms; restartIdle(); }
    // Automatic: a window whose shorter side is under 600 (logical pixels,
    // Android's phone/tablet line) is a phone. On for Android.
    void setAutoFormFactor(bool on);
    Q_INVOKABLE void windowResized(qreal width, qreal height);
    // Every table in the pages, for the phone table list: [{name, seats}].
    Q_INVOKABLE QVariantList tables() const;

    // Save the report on screen as "csv" or "pdf" in the export folder.
    Q_INVOKABLE void exportReport(const QVariantMap &report, const QString &format);
    // A menu to print (Menu Builder -> Print Menu…), from the menu as it is
    // now. options: {title, subtitle, categories: [ids] (empty: all),
    // columns, pictures, descriptions, soldOut (list them too), paper:
    // "letter" | "a4"}. Saved as a PDF in the exports folder; returns it.
    Q_INVOKABLE QString printMenu(const QVariantMap &options);
    // Its first page as a picture to look at (a file: url), and how many pages.
    Q_INVOKABLE QString menuPreview(const QVariantMap &options, int width);
    Q_INVOKABLE int menuPages(const QVariantMap &options);
    void setExportDirectory(const QString &dir) { exportDir_ = dir; }

    // Change the choices of an unsent item: opens the Choose page for it.
    Q_INVOKABLE void chooseLine(qint64 lineId);
    // Modifier choices done: back to the menu once the required ones are made.
    Q_INVOKABLE void finishChoosing();

    Q_INVOKABLE void activate(const QString &zoneId);
    // Order a menu item as an Add Item button would (menuGrid panels).
    // clearTyped: the typed text cleared too (the Find page, ready for the next).
    Q_INVOKABLE void orderItem(const QString &itemId, bool clearTyped = false);
    // Ready-made looks: [{id, name, colors: [background, buttons, panels, text, accent]}],
    // with two made from the store logo's colors when there is one.
    Q_INVOKABLE QVariantList looks() const;
    // Recolor the theme with a look: in edit mode one undo step (Save keeps
    // it); otherwise saved at once (managers: the setup guide).
    Q_INVOKABLE bool applyLook(const QString &id);
    // Small choices a panel keeps while pages are rebuilt (a menuGrid's family...).
    Q_INVOKABLE QVariant widgetState(const QString &key) const { return widgetState_.value(key); }
    Q_INVOKABLE void setWidgetState(const QString &key, const QVariant &value) { widgetState_.insert(key, value); }
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goHome();
    Q_INVOKABLE bool jumpTo(const QString &pageId);
    // A category's items: the menu page (role "menu", else "menu-all") on it.
    Q_INVOKABLE bool openCategory(const QString &categoryId);
    bool menuScreensHandBuilt() const;
    // The meal pages made self-filling, in the page editor (one step: look it
    // over, then Save, or Undo); false when there's nothing to switch or
    // this person may not edit pages.
    Q_INVOKABLE bool switchToSelfFillingMenu();
    // A spreadsheet saved as CSV, on this device: {items: [{row, name, price,
    // category, description, onIt}], problems, columns, error}.
    Q_INVOKABLE QVariantMap readMenuFile(const QUrl &file) const;
    // The whole menu as a file (Exports, or the folder set for them), for
    // Import... at another store.
    Q_INVOKABLE void exportMenu();
    // A ready-made picture for an item: the emoji drawn big into a PNG the
    // store keeps ("store:emoji-1f32e.png"); "" if it can't be.
    Q_INVOKABLE QString emojiPicture(const QString &emoji);
    // Pages of buttons placed by hand that an item's button can go on:
    // [{id, name, has}] (has: one for `itemId` is there already).
    Q_INVOKABLE QVariantList handBuiltPages(const QString &itemId) const;
    // A button for the item on that page, like its other item buttons, in
    // the next free place; saved like the page editor saves.
    Q_INVOKABLE bool addItemButton(const QString &pageId, const QString &itemId, const QString &label);
    // An item taken off the menu: its buttons on pages built by hand go too
    // (saved like the page editor saves). Returns how many.
    Q_INVOKABLE int removeItemButtons(const QString &itemId);
    // Something a panel keeps while this screen shows other pages (a card
    // not saved yet); take() hands it back once and forgets it.
    Q_INVOKABLE void keep(const QString &key, const QVariant &value) { if (value.isValid()) kept_.insert(key, value); else kept_.remove(key); }
    Q_INVOKABLE QVariant take(const QString &key) { return kept_.take(key); }
    QString menuCategory() const { return menuCategory_; }
    void setMenuCategory(const QString &id);
    // Holding a button: what it does, in plain words, instead of doing it.
    Q_INVOKABLE void explain(const QString &zoneId);
    Q_INVOKABLE void clearExplanation();
    QVariantMap explanation() const { return explanation_; }
    QString describeAction(const vt::layout::Action &action) const;
    // Pages to start on, for the admin forms: [{value: id, text: name}], empty first.
    Q_INVOKABLE QVariantList pageChoices() const;
    // Where the screen rests with nobody logged in: the login page, or the
    // Time Clock on a terminal set to it.
    QString loginPage() const;
    void restAtLoginPage();
    // Switch pages without remembering the current one (editor page list).
    Q_INVOKABLE bool showPage(const QString &pageId);
    Q_INVOKABLE bool triggerHotkey(const QString &key);
    // On the order screen, a keyboard's digits then Enter ring in the item
    // with that number (Backspace, Escape). True: the key was used.
    Q_INVOKABLE bool numberKey(const QString &key);
    // What a barcode scanner read (keys too fast for a person, then Enter):
    // an item's barcode or number rings it in on the order screens; else a
    // coupon or a gift card (PosService::scanCode).
    Q_INVOKABLE void scanned(const QString &code);

    // Widget entry points.
    Q_INVOKABLE void selectTable(const QString &label);
    Q_INVOKABLE void openCheck(qint64 checkId);
    Q_INVOKABLE void login();

    // F1 / "Edit Pages" / --edit: the only way in from the UI. Needs the
    // layout.edit permission when a POS session is attached.
    Q_INVOKABLE bool requestEditMode();
    // Unchecked, for C++ only (tests). Deliberately not Q_INVOKABLE, so no
    // page or widget can skip the permission check.
    void enterEditMode();
    // Persist the draft and make it the running layout; stays in edit mode.
    Q_INVOKABLE bool saveEdits();
    // Leave edit mode, saving first when `save` (and staying if saving fails).
    Q_INVOKABLE bool leaveEditMode(bool save);

signals:
    void pageChanged();
    void formFactorChanged();
    // A manager asked to close ViewTouch (the closeApp command).
    void closeRequested();
    void statusChanged();
    void explanationChanged();
    void editingChanged();
    void asleepChanged();
    void mealPeriodChanged();
    void statusColorsChanged();
    void menuCategoryChanged();
    void menuScreensChanged();
    void fontsChanged();
    void setupOpenChanged();
    // A command for the widgets on the page (kitchenStation, kitchenAllDay...).
    void widgetCommand(const QString &name, const QVariantMap &args);
    void editorChanged();
    void posChanged();
    void busyChanged();
    void itemAdded(const QString &item);
    void commandRequested(const QString &name, const QVariantMap &args);

private:
    using Done = std::function<void(bool ok)>;

    void runChain(QList<vt::layout::Action> actions, int index, QString startPage);
    void runAction(const vt::layout::Action &action, Done done);
    void runCommand(const QString &name, const QVariantMap &args, Done done);
    // invoke() on the session, counted as pending until answered.
    void call(const QString &method, const QVariantList &args, std::function<void(const QVariant &)> then);
    bool navigate(vt::app::Navigator::Mode mode, const QString &target = {});
    bool mayOpen(const QString &pageId);
    QString rolePage(const QString &role) const;
    void onLoggedInChanged(bool loggedIn);
    void onDraftChanged();
    void ensureCurrentPageExists();
    void refresh();
    void setStatus(const QString &text);
    const vt::layout::Page *currentPage() const { return activeLayout().page(nav_.current()); }
    // What this screen shows for the current page: its phone version, or
    // the phone template with the page's own zones laid out in its content
    // area (moved), or the page itself.
    struct Shown {
        QString pageId;
        QSize canvas;
        QList<vt::layout::Layout::PlacedZone> zones;
        QHash<const vt::layout::Zone *, QRect> moved;   // reflowed: rect on this screen
    };
    Shown shown() const;
    void updateFormFactor();
    QString formFactor_ = QStringLiteral("standard");
    QString formFactorOverride_;
    bool timeClockScreen_ = false;   // --screen timeClock
    bool autoFormFactor_ = false;
    bool phoneSizedWindow_ = false;
    QString exportDir_;

    vt::layout::Layout layout_;
    vt::app::Navigator nav_;
    ZoneModel zones_;
    QString status_;
    // The pages in this terminal's own look (Manager -> Terminals), when it has one.
    std::optional<vt::layout::Layout> lookedLayout_;
    QString lookedId_;
    void updateTerminalLook();
    QVariantMap explanation_;
    QString typedNumber_;
    // While leaving edit mode the editor object outlives `editing_` briefly,
    // so QML panels unload before the editor they bind to goes away.
    QPointer<EditorController> editor_;
    bool editing_ = false;
    QTimer mealTimer_;
    // Idle log-out: restarted by every touch, key or click (eventFilter).
    QTimer idleTimer_;
    QTimer sleepTimer_;
    QVariantMap statusColors_;
    QString menuCategory_;
    QVariantMap widgetState_;
    bool setupOpen_ = false;
    QSet<QString> installedFonts_;   // by content hash
    bool asleep_ = false;
    int sleepOverrideMs_ = 0;
    void restartSleep();
    void sleepTimeout();
    int idleOverrideMs_ = 0;
    void restartIdle();
    void idleTimeout();
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    bool mealPeriodFixed_ = false;
    void updateMealPeriod();
    Saver saver_;
    QVariantMap kept_;
    CardReader cardReader_;
    int readerTokenSeq_ = 0;   // the last connection token handed to the reader
    void takeCard(const QString &tenderId, Done done);
    void cardTaken(const QVariantMap &result);
    // Cards approved while the store couldn't be reached: sent when it can.
    QList<QVariantMap> unsentCards_;
    void sendUnsentCards();
    void saveUnsentCards();
    void loadUnsentCards();
    vt::app::PosSession *pos_ = nullptr;
    int pending_ = 0;
};
