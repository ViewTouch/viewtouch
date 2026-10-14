# Plan: Modern ViewTouch ("Modernization" branch)

## Context

ViewTouch has about 137k lines of first-party C++. The code is from the 1990s and has been partly upgraded to C++23. Its main problems:

- **Server does the drawing.** The server (`vt_main`) draws every pixel for the terminals over a custom X11 protocol. Terminals use X11, Xt and Motif, and pages are laid out at fixed pixel sizes.
- **One thread does everything.** A single Xt event loop does all file I/O, runs `system()` calls and prints. This caused the 10–12h freeze fixed on 2026-06-02 and the heavy-load freeze fixed on 2026-06-09.
- **Fragile file format.** Data files store values by position with no field names. If a zone type's accessor list changes, old files of that type load wrong without any error.
- **Magic numbers.** System pages are identified by fixed negative IDs (-1, -3, -20, -99…). Edit permission is tied to employee IDs 1 and 2.
- **Weak editor.** The editor uses Motif dialogs. It has no undo (the only option is "discard the whole session") and only one terminal can edit at a time.
- **Starter pages are not in the repo.** `vt_data`, `zone_db.dat` and `tables.dat` are downloaded from viewtouch.com.

What is worth keeping is the concept: every screen is a **page** made of freely placed, styled **buttons (zones)**. Pages inherit from template pages. An in-app **edit mode** lets the owner build the whole UI with no code. Modifier "scripts" chain pages together.

The goal is a new modern POS in **C++23 + Qt6/QML**. It lives in a new top-level `modern/` directory on a new `Modernization` branch. It starts with fresh data (no import of old data). It keeps the fully customizable page and button system, with a much better editor.

---

## How the current system works (reference for the rewrite)

**Pages** (`zone/zone.hh:234`, `zone/zone.cc:503-1180`)
- Each page has: name, id, parent_id, type, meal-period `index`, size, and per-page default font, frame, texture and color for three states (normal, selected, disabled).
- Page types: SYSTEM, TABLE, TABLE2, INDEX, INDEX_WITH_TABS, ITEM, SCRIPTED 1-3 (modifier pages), LIBRARY (a page used as a clipboard), KITCHEN_VID, BAR, MODIFIER_KEYBOARD.
- **Inheritance:** `Page::Init` forces the parent from the page type (INDEX→-99, ITEM→-98, TABLE→-3…). The parent's zones are drawn and hit-tested as part of the child page.
- **Style defaults resolve in this order:** zone value → page default → global default (`terminal.cc:3579-3617`).
- **One page per screen size:** there can be one copy of a page ID for each resolution. `FindByID(id,size)` (`zone.cc:1651`) picks the largest one that fits.

**Zones** (`zone/zone.hh:121`, `zone/pos_zone.hh:31-117`, factory `pos_zone.cc:137-397`)
- Each zone has: position and size (x, y, w, h), behavior (BLINK / TOGGLE / SELECT / DOUBLE / MISS / NONE), shape, shadow, font, key shortcut, group_id, image_path, and frame/texture/color/image for each of the three states.
- Each zone type turns on its own fields through virtual accessors: JumpType, JumpID, Message, ItemName, Script, TenderType, ReportType, Expression, and others.
- There are about 75 zone types. Many are dead or broken: CHECK_DISPLAY and LICENSE have no class, and ORDER_DISPLAY and RECEIPT_SET are missing from the factory.

**Navigation** (`terminal.cc:897-1145`)
- Jump types: NONE, NORMAL (push to a 32-deep stack), STEALTH, RETURN, HOME, SCRIPT, INDEX, PASSWORD.
- Modifier scripts are page-ID lists. `RunScript` (`terminal.cc:1020`) pushes them onto the page stack.

**Edit mode** (`terminal.cc:3108-3283`, `4854-5340`, `5398-6356`; dialogs in `term/term_dialog.cc`)
- **Keys:** F1 toggles edit mode (and saves). F9 edits system pages.
- **Toolbar:** New Button, New Page, Select All, Copy, Move, Delete, Global Defaults, Info, Page List, Prior/Next.
- **Mouse:** left-drag near an edge moves; middle-drag resizes; rubber-band box selects; shift-click adds to the selection.
- **Grid:** 4px snap. Arrow keys nudge; Ctrl+arrow duplicates; w/W/h/H change size.
- **Dialogs:** right-click opens the zone, multi-zone or page dialog. Saving rebuilds the zone, so its type can change.
- **Across pages:** selections persist across pages, so "Copy/Move Selected" pulls zones onto the current page.

**Required system pages:** login -1, tables -3/-4, guest count -5, logout -7, bar/quick-start -8, item target -9, manager -10, settle -20, tab settle -85, and the templates -94…-99.

---

## Target architecture

```
modern/
  CMakeLists.txt            # standalone project (Qt6 >= 6.5, C++23)
  core/                     # pure C++ domain, no Qt GUI dependency (QtCore allowed)
    money.hh                # int64 cents, rounding, tax math
    check/ menu/ employee/ table/ payment/ tax/ drawer/ shift/
  storage/                  # SQLite (Qt6Sql) repositories, migrations, WAL, transactions
  layout/                   # Page / Zone / Action / Style model + JSON (de)serialization + schema version
  app/                      # services + command bus (OrderService, PaymentService, SessionService…)
  ui/qml/                   # QML: PageView, ZoneDelegate, widgets, theme
  ui/qml/editor/            # Edit mode: canvas overlay, palette, inspector, page tree
  ui/cpp/                   # QML-exposed models (PageModel, ZoneModel, CheckModel, EditorController)
  print/                    # ESC/POS receipt + kitchen printing on worker thread
  seed/                     # starter pages/menu/theme as JSON, shipped in repo
  tests/                    # Catch2 (reuse external/catch2) + Qt Quick Test
  main.cpp
```

**Core principles**

1. **Separate the domain from the UI.** Business logic lives in `core/` and `app/` and knows nothing about pages. Zones call **actions**, and actions call services. This undoes the old coupling between zones and business logic.
2. **Run one process first, but design for many terminals.** Every UI→logic call goes through a serializable command bus: a `Command` goes in and `Event`s come out. Phase 1 runs it in-process. Phase 5 carries the same messages over QtWebSockets so multiple terminals can share one server.
3. **Never block the UI thread.** SQLite (WAL mode) writes, printing and network run on worker threads through `QThreadPool`/`QtConcurrent`, with results sent back to the UI as signals. Every state change is committed in a transaction immediately; there is no 30s bulk save. This removes the class of bugs behind both recent freeze fixes.
4. **Resolution independence.** Each page has a logical design canvas (default 1920×1080 units), scaled uniformly with letterboxing to fit the screen. This replaces keeping one copy of every page per resolution.
5. **Named, versioned data.** Pages are JSON documents with a `schemaVersion` and named fields, stored in SQLite (`pages` table) and exportable/importable as `.vtpage.json`. Unknown fields are kept when a page is loaded and saved again.

---

## Layout model (the heart of the customizable UI)

```jsonc
Page {
  id: "uuid", name, kind: "login|tables|index|items|modifier|settle|manager|kitchen|template|library|custom",
  role?: "login|tables|guestCount|settle|manager|logout|...",   // replaces magic negative IDs
  templateId?: "uuid",          // inheritance (any page can be a template, chain depth ≤ 8)
  mealPeriod?: "breakfast|lunch|dinner|all",
  canvas: {w:1920, h:1080}, grid: 8,
  style: StyleRef,              // page defaults (per state)
  zones: [Zone...]
}
Zone {
  id, kind: "button" | widget-kind, name, rect:{x,y,w,h}, z,
  shape: "rect|rounded|circle|hex|octagon|diamond",
  behavior: "blink|toggle|select|double|passthrough|none",
  style: { normal:{fill,texture,image,text,font,frame,shadow}, selected:{...}, disabled:{...} },  // omitted → inherits page → theme
  label, icon?, imagePath?, hotkey?, group?,
  visibleWhen?/enabledWhen?: Condition,   // replaces CONDITIONAL
  confirm?: {message},
  actions: [Action...],                  // executed in order
  props: {...}                           // widget-specific settings
}
Action = jump{page|role, mode: push|replace|back|home|index|nextInSequence, password?}
       | command{name, args}             // logout, openDrawer, sendOrder, printReceipt, voidItem…
       | addItem{itemId, modifierSequence?:[pageId…]}   // replaces ITEM + RunScript
       | qualifier{type}                 // no/extra/lite/side
       | tender{tenderId, amount?}       // replaces TENDER
       | setting{key, toggle}            // replaces SWITCH
       | cycle{states:[…]}               // replaces TOGGLE
```

About 15 of the old zone types (STANDARD, SIMPLE, CONDITIONAL, TOGGLE, SWITCH, ITEM×6, QUALIFIER, TENDER, ORDER_ADD/DELETE/PAGE/FLOW, IMAGE_BUTTON, LANGUAGE, INDEX_TAB) become **one `button` kind with an action list**. Complex panels become **widgets**. This cuts about 75 types down to about 20, and a new behavior is just a new action.

**Keep these ideas:** template inheritance, style resolution in three levels (zone → page → theme) and three states, meal-period index pages, index tabs inherited onto item pages, modifier page sequences, library pages as a shared clipboard, and the page stack.

---

## Starter set (MVP buttons, widgets and seed pages)

**Generic:** `button` (with the actions above), `label`/`comment`, `image`, `statusBar` (messages), `clock`.

**Core POS widgets:**

| Widget | Replaces | What it does |
|---|---|---|
| `loginPad` | LOGIN | PIN entry, clock in/out |
| `logoutPanel` | LOGOUT | Clock out, break, save |
| `table` | TABLE | One table per zone, placed in the editor, showing live status (an earlier single `tableMap` panel is converted on load) |
| `guestCount` | GUEST_COUNT | Enter number of guests |
| `checkList` | CHECK_LIST | List of open checks |
| `orderList` | ORDER_ENTRY | Current check: select, modify, seat/check paging |
| `paymentPanel` | PAYMENT_ENTRY | Payments applied and balance |
| `numPad` | STANDARD digit buttons | Built-in number pad |
| `keyboard` | STANDARD letter buttons | Built-in keyboard |
| `splitCheck` | SPLIT_CHECK | Split items across checks |
| `drawerPanel` | DRAWER_MANAGE | Drawer pull and balance, basic version |
| `reportView` | REPORT | Sales and server reports to start |
| `endOfDay` | END_DAY | End-of-day processing |

**Admin widgets, form-based:** `menuEditor` (items, prices, families, printer routing), `employeeEditor` (roles/permissions replace the hard-coded ID 1/2 check), `settingsForm`, `taxSettings`, `tenderSettings`, `printerSettings`.

**Seed pages (JSON in `modern/seed/`, shipped in the repo, no downloads):**
- Login
- Tables (one floor)
- Guest Count
- Order template, with orderList, a flow bar (Send, Pay, Void, Qualifiers) and index tabs
- Index pages: Breakfast, Lunch, Dinner
- About 3 item pages with sample items
- One modifier page ("Temperature")
- Settle, with paymentPanel, tender buttons and numPad
- Logout
- Manager hub, with links to editors and reports
- One Library page

---

## Editor mode (much better than the old one, same idea)

- **Enter:** toolbar button or F1, permission `layout.edit`. Leaving asks Save / Discard. Edits happen on a **draft copy** of the layout, and saving commits it to SQLite in one transaction.
- **Canvas overlay (QML):** click to select; shift-click or rubber-band for multi-select; drag to move; 8 resize handles; snap to grid plus smart alignment guides against other zones' edges and centers; arrow keys nudge (Shift = ×10); Ctrl+D duplicates; Delete removes; z-order up/down.
- **Palette panel:** drag a new button or widget onto the page.
- **Inspector panel** (replaces the Motif dialogs): fields are generated from a per-kind property schema (`layout/ZoneSchema`). It has an action-list editor, a style editor with live preview (normal/selected/disabled tabs), and a menu-item picker. It can **change kind** and keeps any compatible fields.
- **Multi-select edits:** fields that differ show as "mixed". Tools for align, distribute and match size.
- **Page tree panel:** create, rename, delete and duplicate pages; set the template; show "references to this page" (the old `ZoneDB::References`); and jump to any page.
- **Undo/redo:** a `QUndoStack` of command objects, one per edit (move, resize, property change, add, delete, page ops). This is real per-action undo.
- **Clipboard:** copy and paste zones across pages; library pages stay as reusable button sets.
- **Import/export:** single page or whole layout as `.vtpage.json` / `.vtlayout.json`. This is the modern version of the old F7 export and pageimports.
- **Inherited zones** are drawn dimmed and locked, with a "Go to template" link.
- **Theme editor:** global palette, fonts and default styles, which replace Global Page Defaults. The old textures from `assets/images/xpm` are converted to PNG and used as texture presets.

---

## What we reuse from the old code (as reference, ported by hand, not linked)

| Take from | For |
|---|---|
| `SubCheck::FigureTotals` and tax/coupon logic (`main/business/check.cc`), plus `tests/` tax and coupon cases | Port to `core/tax` and `core/check`, and reuse the existing test cases as test data |
| `Check`/`SubCheck`/`Order`/`Payment` concepts, `CheckType`, order status bits, `TENDER_*` list | Starting point for the domain model |
| ESC/POS command sequences (`main/hardware/printer.cc` Epson/Star) and `SubCheck::PrintReceipt` / `Check::PrintWorkOrder` layout | Base for `print/` |
| `main/ui/labels.cc` name tables (jump types, shapes, frames, colors, textures) | Lists of values for the editor |
| `fonts/`, `assets/images/xpm/` textures | Theme assets, converted to PNG |
| Jump semantics (`terminal.cc:897-1145`) and modifier script flow | `app/NavigationService` |
| Page inheritance and style resolution (`zone.cc:532-752`, `terminal.cc:3579`) | `layout/` resolver |

**Left behind:** X11/Xt/Motif, drawing on the server, the positional `.dat` format, intrusive linked lists, negative magic page IDs, `system()` shell-outs, the vt_data download, and dead zone types.

---

## Milestones

**M0 – Branch and scaffold**
- `git checkout -b Modernization`.
- Add `modern/` with a standalone CMake project. The root CMake gets an `option(VT_BUILD_MODERN)` so the legacy build is untouched.
- Hello-world QML window, Catch2 test target, and a `modern/README.md` with the architecture above.
- Prereq: `sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwebsockets-devel sqlite-devel`. Only the runtime Qt packages are installed today.

**M1 – Layout engine, read-only**
- `layout/` model and JSON (de)serialization with round-trip tests.
- Template inheritance and style resolution.
- `PageView.qml` draws a page scaled from its canvas.
- `button` renders all shapes and states.
- `jump` actions and the page stack work.
- Seed pages load from `seed/`.

**M2 – Editor mode**
- Overlay, selection, move/resize, snap and guides.
- Palette, inspector (schema-driven), page tree, `QUndoStack`.
- Draft/save/discard; persistence in the SQLite `pages` table; import/export.
- *This is the first big user-visible win.*

**M3 – Core POS flow**
- Domain: money, tax, menu, check, order, modifiers, employee/roles.
- SQLite repos with migrations.
- Widgets: loginPad, tableMap, guestCount, orderList, numPad, keyboard, paymentPanel, checkList.
- Actions: addItem with modifier sequences, qualifier, tender, and the commands send/void/pay/logout.
- End-to-end result: log in → table → order → send → pay → close.

**M4 – Printing and back office**
- ESC/POS receipt and kitchen tickets on a worker thread (network/USB/CUPS).
- Printer routing per family.
- Drawer pull, reports, end of day (business-day archive in SQLite).
- Admin editors: menu, employees, settings, taxes, tenders, printers.

**M5 – Multi-terminal and extras**
- Command bus over QtWebSockets: a server plus N QML terminals, with live layout reload after an edit.
- Kitchen display page kind, split check, customer info/takeout.
- Later: card processor integration (see `docs/PAYMENT_PROCESSOR_INTEGRATION.md`).

---

## Verification

- **Unit tests** (`ctest` in `modern/build`):
  - Layout JSON round-trip, including unknown fields.
  - Inheritance and style resolution.
  - Undo/redo sequences.
  - Tax and check totals, using the cases ported from `tests/`.
  - Modifier-sequence navigation.
- **QML tests** (`qmltestrunner`, `QT_QPA_PLATFORM=offscreen`):
  - PageView renders the seed pages.
  - An editor drag changes the rect.
  - Undo restores it.
- **Manual run:** `cmake -S modern -B modern/build && cmake --build modern/build && ./modern/build/vtmodern`.
  - F1 → move, resize and restyle a button, add a new button with a jump action → save, restart, and confirm it persisted.
  - Run the full order-to-pay flow (from M3).
- **Freeze regression test:** a stress test fires thousands of orders and prints with a slow mock printer, and asserts that the UI thread latency stays under 50ms.
- **Legacy build still passes:** the legacy tree still builds with the new option OFF.

## Immediate next step after approval

Do M0: create the `Modernization` branch, scaffold `modern/`, and commit. Then start M1: the layout model, the JSON format and seed pages, and PageView rendering.

---

## Roadmap after M5 (agreed 2026-09-29)

M0–M5 are done. Work continues on the `Modernization` branch; the pull request
comes once everything works as intended.

- **M6: Tips & cash handling** (done)
  - Tips on card payments (15/18/20% or a typed amount).
  - Auto-gratuity for large parties (settings).
  - A drawer and a receipt printer per terminal.
  - Drawer pay-outs and paid-ins; servers cash out tips from the drawer.
  - Tips report.
- **M7: Install & run as a service** (done)
  - Packages for Fedora and Debian/Ubuntu/Raspberry Pi OS (x86_64 + aarch64).
  - Server as a systemd service; terminals start in kiosk mode at boot.
  - Automatic database backups.
  - Meal periods as settings (Manager → Meal Periods). Separate store opening hours were not needed: the business day runs from End of Day to End of Day.
- **Hands-on fixing** (ongoing): issues found on real hardware get fixed as they come in.
- **M8: Card payments** (last): processor to be chosen (Stripe Terminal, Square, …) behind a payment-processor interface with a simulator for tests.
