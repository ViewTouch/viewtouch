<div align="center">

# ViewTouch©

**The original graphical touchscreen restaurant point of sale, rebuilt for today.**

[![Latest release](https://img.shields.io/github/v/release/ViewTouch/viewtouch?label=release&color=2f6fe0)](https://github.com/ViewTouch/viewtouch/releases/latest)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue)](https://en.wikipedia.org/wiki/C%2B%2B23)
[![Qt 6](https://img.shields.io/badge/Qt-6.8%2B-41cd52)](https://www.qt.io/)
[![Linux · Raspberry Pi · Android](https://img.shields.io/badge/runs%20on-Linux%20%C2%B7%20Raspberry%20Pi%20%C2%B7%20Android-555)](#get-started)
[![Discord](https://img.shields.io/badge/chat-Discord-5865f2)](https://discord.com/invite/ySmH2U2Mzb)

**[Download](https://github.com/ViewTouch/viewtouch/releases/latest)** · **[User manual (PDF)](https://github.com/ViewTouch/viewtouch/releases/download/modern-manual-0.10/ViewTouch-User-Manual.pdf)** · **[Developer README](modern/README.md)** · **[Website](http://www.viewtouch.com)**

*ViewTouch is a registered trademark in the USA*

</div>

---

ViewTouch has been built from the ground up in **C++23 and Qt 6**. It keeps ViewTouch's best idea, that every screen is a page of buttons you design yourself, and adds what a restaurant needs today: floor plans, handhelds at the table, a kitchen display, a self-order kiosk, reports and a standby computer. It also runs **food trucks** (no login, straight to ordering and paying) and **retail** counters. It runs **entirely on site**, on Linux PCs, touch screens, Raspberry Pi and Android tablets and phones. There are no monthly fees, and your data stays in the store.

<table>
<tr>
<td width="50%"><img src="modern/docs/screenshots/style-classic.jpg" alt="The order screen in the Classic style"><br><sub><b>Classic.</b> Raised buttons and textures, as ViewTouch has always looked.</sub></td>
<td width="50%"><img src="modern/docs/screenshots/style-modern.jpg" alt="The same order in the Modern style"><br><sub><b>Modern.</b> Flat buttons, calm colors, the categories in a column, and Void, Send and Pay together.</sub></td>
</tr>
</table>
<p align="center"><sub>The same order, the same buttons. Pick the look in <b>Store Settings → Screens → Style</b> and switch back any time.</sub></p>

## Get started

### Set up a store computer

One line sets up a fresh **Raspberry Pi, Debian 13, Ubuntu 26.04 or Fedora** machine. It installs ViewTouch, locks the computer down for the store and starts on the ViewTouch home screen:

```sh
curl -fsSL https://raw.githubusercontent.com/ViewTouch/viewtouch/main/modern/packaging/install-viewtouch.sh | sudo sh
```

No `curl` on a clean install? Use `wget -qO- https://raw.githubusercontent.com/ViewTouch/viewtouch/main/modern/packaging/install-viewtouch.sh | sudo sh`. No `sudo` (Debian installed with a root password)? Run `su -` first and leave out `sudo`.

Every other screen is an **Android tablet or phone** (Android 9 or newer) running the ViewTouch app, or another Linux computer. Pair each one with a code from **Manager → Terminals**. After that, ViewTouch keeps itself up to date:

- the store computer checks for signed updates and installs them when you touch **Install**;
- phones and tablets get the new app from the store computer by themselves, while nobody is logged in on them.

The packages and the Android app are on the **[latest release](https://github.com/ViewTouch/viewtouch/releases/latest)**.

### Try it on your own computer

You need **Linux with Qt 6.8 or newer**: Fedora 42+, Debian 13 (and Raspberry Pi OS based on it) or Ubuntu 25.04+.

```sh
# Fedora
sudo dnf install git gcc-c++ cmake ninja-build qt6-qtbase-devel qt6-qtdeclarative-devel

# Debian / Ubuntu / Raspberry Pi OS
sudo apt install git g++ cmake ninja-build qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools libgl-dev \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts qml6-module-qtquick-shapes \
    qml6-module-qtquick-dialogs qml6-module-qtquick-window qml6-module-qtqml-workerscript libqt6sql6-sqlite
```

```sh
git clone https://github.com/ViewTouch/viewtouch.git
cd viewtouch
cmake -S modern -B modern/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build modern/build
ctest --test-dir modern/build -j4      # optional: about 650 tests

./modern/build/vtmodern --windowed     # a demo café opens
./modern/build/vtmodern --demo-data    # optional, once: two months of sales to look at
```

To update it later, run these in the `viewtouch` folder: `git pull`, then the two `cmake` lines again. Press **F11** to switch between full screen and a window. The demo store's data lives in `~/.local/share/ViewTouch/ViewTouch/`.

| Demo PIN | Who |
|---|---|
| 1234 | Morgan, manager (everything) |
| 1111 | Sam, server |
| 2222 | Casey, cashier |
| 5555 | Rosa, server whose screens are in Spanish |

When you're ready for real, **Manager → Factory Reset → An empty store** starts over with no menu, tables or sample staff, and **Menu Builder → Import** brings in the menu you already have.

## A look around

<table>
<tr>
<td width="50%"><img src="modern/docs/screenshots/floor-plan.jpg" alt="Floor plan"><br><sub><b>Floor plans.</b> Main, Dining Area, Bar and Patio, with walls and chairs. Tables show who has them, the total and how long they've been seated.</sub></td>
<td width="50%"><img src="modern/docs/screenshots/order-choices.jpg" alt="Taking an order"><br><sub><b>Taking an order.</b> Items ask for their choices (temperature, sides, dressings), by seat and by course.</sub></td>
</tr>
<tr>
<td><img src="modern/docs/screenshots/modern-tables.jpg" alt="Tables in the Modern style"><br><sub><b>Tables, Modern.</b> Your open checks beside the plan, and a key to what each table's color means.</sub></td>
<td><img src="modern/docs/screenshots/modern-pay.jpg" alt="Pay in the Modern style"><br><sub><b>Pay, Modern.</b> Payment types in a grid, Exact, $20, $40, $50 and $100 at one touch, and the keypad on the right.</sub></td>
</tr>
<tr>
<td><img src="modern/docs/screenshots/payment.jpg" alt="Payment"><br><sub><b>Paying.</b> Cash, cards, gift cards, house accounts, discounts, splits and party gratuity.</sub></td>
<td><img src="modern/docs/screenshots/kitchen-display.jpg" alt="Kitchen display"><br><sub><b>Kitchen display.</b> Touch to bump. Rush Radar says when a station is about to back up.</sub></td>
</tr>
<tr>
<td><img src="modern/docs/screenshots/self-order-kiosk.jpg" alt="Self-order kiosk"><br><sub><b>Self-order kiosk.</b> Guests order on their own and pay at the counter.</sub></td>
<td><img src="modern/docs/screenshots/customer-display.jpg" alt="Customer display"><br><sub><b>Customer display.</b> The guest sees the order, picks a tip and earns rewards.</sub></td>
</tr>
<tr>
<td><img src="modern/docs/screenshots/food-truck.jpg" alt="A food truck's order screen"><br><sub><b>Food trucks.</b> No login: straight to ordering and paying. Each order gets a number, the guest's name and the pager handed to them.</sub></td>
<td><img src="modern/docs/screenshots/food-truck-orders.jpg" alt="A food truck's orders, ready to call"><br><sub><b>Calling orders.</b> Ready says whose order it is and which pager to buzz; Picked Up frees the pager. A manager's PIN opens the Manager screens.</sub></td>
</tr>
<tr>
<td><img src="modern/docs/screenshots/food-truck-now-serving.jpg" alt="Now serving on the customer display"><br><sub><b>Now serving.</b> The customer display shows guests what's ready and what's being made.</sub></td>
<td><img src="modern/docs/screenshots/food-truck-setup.jpg" alt="How do you serve? in the setup guide"><br><sub><b>Restaurant, food truck or retail.</b> The setup guide asks; Store Settings changes it any time, for the store or one screen.</sub></td>
</tr>
<tr>
<td><img src="modern/docs/screenshots/manager.jpg" alt="Manager"><br><sub><b>Manager.</b> Menu, staff, schedule, inventory, promotions, reports and End of Day.</sub></td>
<td><img src="modern/docs/screenshots/page-editor.jpg" alt="Page editor"><br><sub><b>Design your own screens.</b> Move, resize and restyle any button, with undo.</sub></td>
</tr>
<tr>
<td><img src="modern/docs/screenshots/spanish.jpg" alt="Spanish screens"><br><sub><b>Five languages.</b> English, Spanish, French, Portuguese and Chinese: each person's screens are in their own.</sub></td>
<td><img src="modern/docs/screenshots/network.jpg" alt="Network"><br><sub><b>Keep running.</b> A standby computer takes over if the main one fails.</sub></td>
</tr>
<tr>
<td><img src="modern/docs/manual/shots/m180-reports-list.jpg" alt="Reports"><br><sub><b>Reports.</b> Every report in a list by kind; swipe across a report for the next one, and the days fold away as you read.</sub></td>
<td><img src="modern/docs/manual/shots/m184-builder-list.jpg" alt="The whole menu in one list"><br><sub><b>The whole menu in one list.</b> Sort by any column, find by name, number or barcode, and change prices or tax for many items at once.</sub></td>
</tr>
<tr>
<td><img src="modern/docs/manual/shots/m186-builder-track-stock.jpg" alt="Track stock on an item"><br><sub><b>Track stock.</b> On hand, low at, cost and margin on the item itself: counted down as it sells, sold out at none.</sub></td>
<td><img src="modern/docs/manual/shots/m187-unknown-code.jpg" alt="A barcode nothing has"><br><sub><b>Scan to add.</b> A barcode nothing has becomes a new item in two touches, for shops putting new stock on the shelf.</sub></td>
</tr>
<tr>
<td><img src="modern/docs/manual/shots/m155-rush-radar.jpg" alt="Rush Radar on the dashboard"><br><sub><b>Today at a glance.</b> Sales, labor and open checks, and Rush Radar: which kitchen station backs up in the next half hour, and what to do.</sub></td>
<td><img src="modern/docs/manual/shots/m132-host-free.jpg" alt="Host stand"><br><sub><b>The host stand.</b> Every room on one screen: who is seated, which tables are free soon, the waitlist and reservations.</sub></td>
</tr>
<tr>
<td><img src="modern/docs/manual/shots/m135-forecast.jpg" alt="Forecast"><br><sub><b>Forecast.</b> Tomorrow from the same day in past weeks: guests, sales by hour, servers needed and what to order.</sub></td>
<td><img src="modern/docs/manual/shots/m94-looks.jpg" alt="Ready-made looks"><br><sub><b>Your look.</b> Ready-made looks, your logo, colors and fonts, for every screen or each one on its own.</sub></td>
</tr>
<tr>
<td><img src="modern/docs/manual/shots/m147-home-screen.jpg" alt="The home screen"><br><sub><b>A computer only for ViewTouch.</b> The home screen: start ViewTouch, set up the store, Wi-Fi, updates, backups and remote help, by touch.</sub></td>
<td><img src="modern/docs/manual/shots/m97-setup-welcome.jpg" alt="The setup guide"><br><sub><b>Your first day.</b> The setup guide asks for your name, logo, colors, taxes, first menu items and team, one step at a time.</sub></td>
</tr>
<tr>
<td><img src="modern/docs/manual/shots/m146-chinese-order.jpg" alt="Taking an order in Chinese"><br><sub><b>Each person in their language.</b> Chinese at one register…</sub></td>
<td><img src="modern/docs/manual/shots/m146-french-pay.jpg" alt="The Pay screen in French"><br><sub>…French at the next, the same store.</sub></td>
</tr>
</table>

### On phones and tablets

<table align="center"><tr>
<td align="center"><img src="modern/docs/screenshots/handheld-seats.jpg" alt="Ordering by seat on a phone" width="180"><br><sub>At the table: seats, a guest's<br>allergy, "+ Lemonade · Seat 3".</sub></td>
<td align="center"><img src="modern/docs/screenshots/modern-phone.jpg" alt="Modern on a phone" width="180"><br><sub>Modern on a phone: a short<br>check, the menu with the room.</sub></td>
<td align="center"><img src="modern/docs/screenshots/handheld-tables.jpg" alt="Tables as a list on a phone" width="180"><br><sub>Your tables first, with<br>what hasn't been sent.</sub></td>
<td align="center"><img src="modern/docs/screenshots/kiosk-portrait.jpg" alt="Self-order kiosk, portrait" width="200"><br><sub>The self-order kiosk on a tall<br>21.5" stand, buttons within reach.</sub></td>
<td align="center"><img src="modern/docs/screenshots/food-truck-phone.jpg" alt="A food truck on a phone" width="180"><br><sub>A food truck on a phone or<br>small tablet: #1 Lee, pager 7.</sub></td>
</tr></table>

<table align="center"><tr>
<td align="center"><img src="modern/docs/manual/shots/m182-reports-phone.jpg" alt="Reports on a phone" width="180"><br><sub>Reports on a phone: the<br>report fills the screen.</sub></td>
<td align="center"><img src="modern/docs/manual/shots/m183-reports-phone-list.jpg" alt="The list of reports on a phone" width="180"><br><sub>Its name opens the list<br>of every report.</sub></td>
<td align="center"><img src="modern/docs/manual/shots/m132-phone-floor.jpg" alt="Floor plan on a phone" width="180"><br><sub>The floor plan, tables where<br>they stand, on a phone.</sub></td>
<td align="center"><img src="modern/docs/manual/shots/m149-phone-setup-guide.jpg" alt="Setup guide on a phone" width="180"><br><sub>Setting up the store<br>from a phone.</sub></td>
</tr></table>

### Every screen

Every page of the starter store, from a store filled with two months of demo service. Touch one to see it full size; all of them are also in [ViewTouch-Screens.zip](https://github.com/ViewTouch/viewtouch/releases/download/modern-manual-0.10/ViewTouch-Screens.zip).

<details>
<summary><b>Classic</b> (74 screens)</summary>

<table>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/floor-bar.jpg"><img src="modern/docs/manual/shots/pages/classic/floor-bar.jpg" alt="Bar" width="200"></a><br><sub>Bar</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/bar-display.jpg"><img src="modern/docs/manual/shots/pages/classic/bar-display.jpg" alt="Bar Display" width="200"></a><br><sub>Bar Display</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/tabs.jpg"><img src="modern/docs/manual/shots/pages/classic/tabs.jpg" alt="Bar Tabs" width="200"></a><br><sub>Bar Tabs</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/index-breakfast.jpg"><img src="modern/docs/manual/shots/pages/classic/index-breakfast.jpg" alt="Breakfast" width="200"></a><br><sub>Breakfast</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/library.jpg"><img src="modern/docs/manual/shots/pages/classic/library.jpg" alt="Button Library" width="200"></a><br><sub>Button Library</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-card-readers.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-card-readers.jpg" alt="Card Readers" width="200"></a><br><sub>Card Readers</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/check-options.jpg"><img src="modern/docs/manual/shots/pages/classic/check-options.jpg" alt="Check Options" width="200"></a><br><sub>Check Options</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/checklists.jpg"><img src="modern/docs/manual/shots/pages/classic/checklists.jpg" alt="Checklists" width="200"></a><br><sub>Checklists</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/modifiers.jpg"><img src="modern/docs/manual/shots/pages/classic/modifiers.jpg" alt="Choose" width="200"></a><br><sub>Choose</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/closed-checks.jpg"><img src="modern/docs/manual/shots/pages/classic/closed-checks.jpg" alt="Closed Checks" width="200"></a><br><sub>Closed Checks</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/stock-count.jpg"><img src="modern/docs/manual/shots/pages/classic/stock-count.jpg" alt="Count Stock" width="200"></a><br><sub>Count Stock</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/customer.jpg"><img src="modern/docs/manual/shots/pages/classic/customer.jpg" alt="Customer" width="200"></a><br><sub>Customer</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/customers.jpg"><img src="modern/docs/manual/shots/pages/classic/customers.jpg" alt="Customers" width="200"></a><br><sub>Customers</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/dashboard.jpg"><img src="modern/docs/manual/shots/pages/classic/dashboard.jpg" alt="Dashboard" width="200"></a><br><sub>Dashboard</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/deliveries.jpg"><img src="modern/docs/manual/shots/pages/classic/deliveries.jpg" alt="Deliveries" width="200"></a><br><sub>Deliveries</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/floor-dining.jpg"><img src="modern/docs/manual/shots/pages/classic/floor-dining.jpg" alt="Dining Area" width="200"></a><br><sub>Dining Area</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/index-dinner.jpg"><img src="modern/docs/manual/shots/pages/classic/index-dinner.jpg" alt="Dinner" width="200"></a><br><sub>Dinner</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/drawer.jpg"><img src="modern/docs/manual/shots/pages/classic/drawer.jpg" alt="Drawer" width="200"></a><br><sub>Drawer</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/drawer-reason.jpg"><img src="modern/docs/manual/shots/pages/classic/drawer-reason.jpg" alt="Drawer Reason" width="200"></a><br><sub>Drawer Reason</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-employees.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-employees.jpg" alt="Employees" width="200"></a><br><sub>Employees</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/end-of-day.jpg"><img src="modern/docs/manual/shots/pages/classic/end-of-day.jpg" alt="End of Day" width="200"></a><br><sub>End of Day</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/expo.jpg"><img src="modern/docs/manual/shots/pages/classic/expo.jpg" alt="Expo Display" width="200"></a><br><sub>Expo Display</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/factory-reset.jpg"><img src="modern/docs/manual/shots/pages/classic/factory-reset.jpg" alt="Factory Reset" width="200"></a><br><sub>Factory Reset</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/find-check.jpg"><img src="modern/docs/manual/shots/pages/classic/find-check.jpg" alt="Find a Check" width="200"></a><br><sub>Find a Check</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/find-item.jpg"><img src="modern/docs/manual/shots/pages/classic/find-item.jpg" alt="Find an Item" width="200"></a><br><sub>Find an Item</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/gift-card.jpg"><img src="modern/docs/manual/shots/pages/classic/gift-card.jpg" alt="Gift Card" width="200"></a><br><sub>Gift Card</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/guest-count.jpg"><img src="modern/docs/manual/shots/pages/classic/guest-count.jpg" alt="Guest Count" width="200"></a><br><sub>Guest Count</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/host.jpg"><img src="modern/docs/manual/shots/pages/classic/host.jpg" alt="Host Stand" width="200"></a><br><sub>Host Stand</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-inventory.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-inventory.jpg" alt="Inventory" width="200"></a><br><sub>Inventory</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/kitchen.jpg"><img src="modern/docs/manual/shots/pages/classic/kitchen.jpg" alt="Kitchen Display" width="200"></a><br><sub>Kitchen Display</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/logout.jpg"><img src="modern/docs/manual/shots/pages/classic/logout.jpg" alt="Log Out" width="200"></a><br><sub>Log Out</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/waste-log.jpg"><img src="modern/docs/manual/shots/pages/classic/waste-log.jpg" alt="Log Waste" width="200"></a><br><sub>Log Waste</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/login.jpg"><img src="modern/docs/manual/shots/pages/classic/login.jpg" alt="Login" width="200"></a><br><sub>Login</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/index-lunch.jpg"><img src="modern/docs/manual/shots/pages/classic/index-lunch.jpg" alt="Lunch" width="200"></a><br><sub>Lunch</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/tables.jpg"><img src="modern/docs/manual/shots/pages/classic/tables.jpg" alt="Main floor" width="200"></a><br><sub>Main floor</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/manager.jpg"><img src="modern/docs/manual/shots/pages/classic/manager.jpg" alt="Manager" width="200"></a><br><sub>Manager</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-meal-periods.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-meal-periods.jpg" alt="Meal Periods" width="200"></a><br><sub>Meal Periods</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/menu-all.jpg"><img src="modern/docs/manual/shots/pages/classic/menu-all.jpg" alt="Menu" width="200"></a><br><sub>Menu</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/menu-builder.jpg"><img src="modern/docs/manual/shots/pages/classic/menu-builder.jpg" alt="Menu Builder" width="200"></a><br><sub>Menu Builder</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-menu.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-menu.jpg" alt="Menu Items" width="200"></a><br><sub>Menu Items</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/merge.jpg"><img src="modern/docs/manual/shots/pages/classic/merge.jpg" alt="Merge Checks" width="200"></a><br><sub>Merge Checks</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-modifier-groups.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-modifier-groups.jpg" alt="Modifier Groups" width="200"></a><br><sub>Modifier Groups</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/move-table.jpg"><img src="modern/docs/manual/shots/pages/classic/move-table.jpg" alt="Move Check" width="200"></a><br><sub>Move Check</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/network.jpg"><img src="modern/docs/manual/shots/pages/classic/network.jpg" alt="Network" width="200"></a><br><sub>Network</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/new-tab.jpg"><img src="modern/docs/manual/shots/pages/classic/new-tab.jpg" alt="New Tab" width="200"></a><br><sub>New Tab</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/note.jpg"><img src="modern/docs/manual/shots/pages/classic/note.jpg" alt="Note" width="200"></a><br><sub>Note</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/check-list.jpg"><img src="modern/docs/manual/shots/pages/classic/check-list.jpg" alt="Open Checks" width="200"></a><br><sub>Open Checks</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/order-template.jpg"><img src="modern/docs/manual/shots/pages/classic/order-template.jpg" alt="Order" width="200"></a><br><sub>Order</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/order-later.jpg"><img src="modern/docs/manual/shots/pages/classic/order-later.jpg" alt="Order for Later" width="200"></a><br><sub>Order for Later</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/truck-orders.jpg"><img src="modern/docs/manual/shots/pages/classic/truck-orders.jpg" alt="Orders (food truck)" width="200"></a><br><sub>Orders (food truck)</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/floor-patio.jpg"><img src="modern/docs/manual/shots/pages/classic/floor-patio.jpg" alt="Patio" width="200"></a><br><sub>Patio</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/settle.jpg"><img src="modern/docs/manual/shots/pages/classic/settle.jpg" alt="Pay" width="200"></a><br><sub>Pay</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-tenders.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-tenders.jpg" alt="Payment Types" width="200"></a><br><sub>Payment Types</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/menu-popular.jpg"><img src="modern/docs/manual/shots/pages/classic/menu-popular.jpg" alt="Popular" width="200"></a><br><sub>Popular</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/prep-labels.jpg"><img src="modern/docs/manual/shots/pages/classic/prep-labels.jpg" alt="Prep Labels" width="200"></a><br><sub>Prep Labels</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-printers.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-printers.jpg" alt="Printers" width="200"></a><br><sub>Printers</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-promotions.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-promotions.jpg" alt="Promotions" width="200"></a><br><sub>Promotions</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/receive-delivery.jpg"><img src="modern/docs/manual/shots/pages/classic/receive-delivery.jpg" alt="Receive a Delivery" width="200"></a><br><sub>Receive a Delivery</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/reports.jpg"><img src="modern/docs/manual/shots/pages/classic/reports.jpg" alt="Reports" width="200"></a><br><sub>Reports</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-schedule.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-schedule.jpg" alt="Schedule" width="200"></a><br><sub>Schedule</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/seating.jpg"><img src="modern/docs/manual/shots/pages/classic/seating.jpg" alt="Seating" width="200"></a><br><sub>Seating</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/message.jpg"><img src="modern/docs/manual/shots/pages/classic/message.jpg" alt="Send a Message" width="200"></a><br><sub>Send a Message</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/sold-out.jpg"><img src="modern/docs/manual/shots/pages/classic/sold-out.jpg" alt="Sold Out (86)" width="200"></a><br><sub>Sold Out (86)</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/split.jpg"><img src="modern/docs/manual/shots/pages/classic/split.jpg" alt="Split Check" width="200"></a><br><sub>Split Check</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/store-check.jpg"><img src="modern/docs/manual/shots/pages/classic/store-check.jpg" alt="Store Check" width="200"></a><br><sub>Store Check</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-store.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-store.jpg" alt="Store Settings" width="200"></a><br><sub>Store Settings</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-taxes.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-taxes.jpg" alt="Taxes" width="200"></a><br><sub>Taxes</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-terminals.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-terminals.jpg" alt="Terminals" width="200"></a><br><sub>Terminals</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/time-clock.jpg"><img src="modern/docs/manual/shots/pages/classic/time-clock.jpg" alt="Time Clock" width="200"></a><br><sub>Time Clock</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-requests.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-requests.jpg" alt="Time Off & Swaps" width="200"></a><br><sub>Time Off & Swaps</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-punches.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-punches.jpg" alt="Time Punches" width="200"></a><br><sub>Time Punches</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/transfer.jpg"><img src="modern/docs/manual/shots/pages/classic/transfer.jpg" alt="Transfer Check" width="200"></a><br><sub>Transfer Check</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/admin-vendors.jpg"><img src="modern/docs/manual/shots/pages/classic/admin-vendors.jpg" alt="Vendors" width="200"></a><br><sub>Vendors</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/classic/weigh.jpg"><img src="modern/docs/manual/shots/pages/classic/weigh.jpg" alt="Weigh" width="200"></a><br><sub>Weigh</sub></td>
</tr>
</table>

</details>

<details>
<summary><b>Modern</b> (74 screens)</summary>

<table>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/floor-bar.jpg"><img src="modern/docs/manual/shots/pages/modern/floor-bar.jpg" alt="Bar" width="200"></a><br><sub>Bar</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/bar-display.jpg"><img src="modern/docs/manual/shots/pages/modern/bar-display.jpg" alt="Bar Display" width="200"></a><br><sub>Bar Display</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/tabs.jpg"><img src="modern/docs/manual/shots/pages/modern/tabs.jpg" alt="Bar Tabs" width="200"></a><br><sub>Bar Tabs</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/index-breakfast.jpg"><img src="modern/docs/manual/shots/pages/modern/index-breakfast.jpg" alt="Breakfast" width="200"></a><br><sub>Breakfast</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/library.jpg"><img src="modern/docs/manual/shots/pages/modern/library.jpg" alt="Button Library" width="200"></a><br><sub>Button Library</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-card-readers.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-card-readers.jpg" alt="Card Readers" width="200"></a><br><sub>Card Readers</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/check-options.jpg"><img src="modern/docs/manual/shots/pages/modern/check-options.jpg" alt="Check Options" width="200"></a><br><sub>Check Options</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/checklists.jpg"><img src="modern/docs/manual/shots/pages/modern/checklists.jpg" alt="Checklists" width="200"></a><br><sub>Checklists</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/modifiers.jpg"><img src="modern/docs/manual/shots/pages/modern/modifiers.jpg" alt="Choose" width="200"></a><br><sub>Choose</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/closed-checks.jpg"><img src="modern/docs/manual/shots/pages/modern/closed-checks.jpg" alt="Closed Checks" width="200"></a><br><sub>Closed Checks</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/stock-count.jpg"><img src="modern/docs/manual/shots/pages/modern/stock-count.jpg" alt="Count Stock" width="200"></a><br><sub>Count Stock</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/customer.jpg"><img src="modern/docs/manual/shots/pages/modern/customer.jpg" alt="Customer" width="200"></a><br><sub>Customer</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/customers.jpg"><img src="modern/docs/manual/shots/pages/modern/customers.jpg" alt="Customers" width="200"></a><br><sub>Customers</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/dashboard.jpg"><img src="modern/docs/manual/shots/pages/modern/dashboard.jpg" alt="Dashboard" width="200"></a><br><sub>Dashboard</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/deliveries.jpg"><img src="modern/docs/manual/shots/pages/modern/deliveries.jpg" alt="Deliveries" width="200"></a><br><sub>Deliveries</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/floor-dining.jpg"><img src="modern/docs/manual/shots/pages/modern/floor-dining.jpg" alt="Dining Area" width="200"></a><br><sub>Dining Area</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/index-dinner.jpg"><img src="modern/docs/manual/shots/pages/modern/index-dinner.jpg" alt="Dinner" width="200"></a><br><sub>Dinner</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/drawer.jpg"><img src="modern/docs/manual/shots/pages/modern/drawer.jpg" alt="Drawer" width="200"></a><br><sub>Drawer</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/drawer-reason.jpg"><img src="modern/docs/manual/shots/pages/modern/drawer-reason.jpg" alt="Drawer Reason" width="200"></a><br><sub>Drawer Reason</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-employees.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-employees.jpg" alt="Employees" width="200"></a><br><sub>Employees</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/end-of-day.jpg"><img src="modern/docs/manual/shots/pages/modern/end-of-day.jpg" alt="End of Day" width="200"></a><br><sub>End of Day</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/expo.jpg"><img src="modern/docs/manual/shots/pages/modern/expo.jpg" alt="Expo Display" width="200"></a><br><sub>Expo Display</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/factory-reset.jpg"><img src="modern/docs/manual/shots/pages/modern/factory-reset.jpg" alt="Factory Reset" width="200"></a><br><sub>Factory Reset</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/find-check.jpg"><img src="modern/docs/manual/shots/pages/modern/find-check.jpg" alt="Find a Check" width="200"></a><br><sub>Find a Check</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/find-item.jpg"><img src="modern/docs/manual/shots/pages/modern/find-item.jpg" alt="Find an Item" width="200"></a><br><sub>Find an Item</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/gift-card.jpg"><img src="modern/docs/manual/shots/pages/modern/gift-card.jpg" alt="Gift Card" width="200"></a><br><sub>Gift Card</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/guest-count.jpg"><img src="modern/docs/manual/shots/pages/modern/guest-count.jpg" alt="Guest Count" width="200"></a><br><sub>Guest Count</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/host.jpg"><img src="modern/docs/manual/shots/pages/modern/host.jpg" alt="Host Stand" width="200"></a><br><sub>Host Stand</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-inventory.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-inventory.jpg" alt="Inventory" width="200"></a><br><sub>Inventory</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/kitchen.jpg"><img src="modern/docs/manual/shots/pages/modern/kitchen.jpg" alt="Kitchen Display" width="200"></a><br><sub>Kitchen Display</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/logout.jpg"><img src="modern/docs/manual/shots/pages/modern/logout.jpg" alt="Log Out" width="200"></a><br><sub>Log Out</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/waste-log.jpg"><img src="modern/docs/manual/shots/pages/modern/waste-log.jpg" alt="Log Waste" width="200"></a><br><sub>Log Waste</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/login.jpg"><img src="modern/docs/manual/shots/pages/modern/login.jpg" alt="Login" width="200"></a><br><sub>Login</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/index-lunch.jpg"><img src="modern/docs/manual/shots/pages/modern/index-lunch.jpg" alt="Lunch" width="200"></a><br><sub>Lunch</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/tables.jpg"><img src="modern/docs/manual/shots/pages/modern/tables.jpg" alt="Main floor" width="200"></a><br><sub>Main floor</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/manager.jpg"><img src="modern/docs/manual/shots/pages/modern/manager.jpg" alt="Manager" width="200"></a><br><sub>Manager</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-meal-periods.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-meal-periods.jpg" alt="Meal Periods" width="200"></a><br><sub>Meal Periods</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/menu-all.jpg"><img src="modern/docs/manual/shots/pages/modern/menu-all.jpg" alt="Menu" width="200"></a><br><sub>Menu</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/menu-builder.jpg"><img src="modern/docs/manual/shots/pages/modern/menu-builder.jpg" alt="Menu Builder" width="200"></a><br><sub>Menu Builder</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-menu.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-menu.jpg" alt="Menu Items" width="200"></a><br><sub>Menu Items</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/merge.jpg"><img src="modern/docs/manual/shots/pages/modern/merge.jpg" alt="Merge Checks" width="200"></a><br><sub>Merge Checks</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-modifier-groups.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-modifier-groups.jpg" alt="Modifier Groups" width="200"></a><br><sub>Modifier Groups</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/move-table.jpg"><img src="modern/docs/manual/shots/pages/modern/move-table.jpg" alt="Move Check" width="200"></a><br><sub>Move Check</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/network.jpg"><img src="modern/docs/manual/shots/pages/modern/network.jpg" alt="Network" width="200"></a><br><sub>Network</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/new-tab.jpg"><img src="modern/docs/manual/shots/pages/modern/new-tab.jpg" alt="New Tab" width="200"></a><br><sub>New Tab</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/note.jpg"><img src="modern/docs/manual/shots/pages/modern/note.jpg" alt="Note" width="200"></a><br><sub>Note</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/check-list.jpg"><img src="modern/docs/manual/shots/pages/modern/check-list.jpg" alt="Open Checks" width="200"></a><br><sub>Open Checks</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/order-template.jpg"><img src="modern/docs/manual/shots/pages/modern/order-template.jpg" alt="Order" width="200"></a><br><sub>Order</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/order-later.jpg"><img src="modern/docs/manual/shots/pages/modern/order-later.jpg" alt="Order for Later" width="200"></a><br><sub>Order for Later</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/truck-orders.jpg"><img src="modern/docs/manual/shots/pages/modern/truck-orders.jpg" alt="Orders (food truck)" width="200"></a><br><sub>Orders (food truck)</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/floor-patio.jpg"><img src="modern/docs/manual/shots/pages/modern/floor-patio.jpg" alt="Patio" width="200"></a><br><sub>Patio</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/settle.jpg"><img src="modern/docs/manual/shots/pages/modern/settle.jpg" alt="Pay" width="200"></a><br><sub>Pay</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-tenders.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-tenders.jpg" alt="Payment Types" width="200"></a><br><sub>Payment Types</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/menu-popular.jpg"><img src="modern/docs/manual/shots/pages/modern/menu-popular.jpg" alt="Popular" width="200"></a><br><sub>Popular</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/prep-labels.jpg"><img src="modern/docs/manual/shots/pages/modern/prep-labels.jpg" alt="Prep Labels" width="200"></a><br><sub>Prep Labels</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-printers.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-printers.jpg" alt="Printers" width="200"></a><br><sub>Printers</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-promotions.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-promotions.jpg" alt="Promotions" width="200"></a><br><sub>Promotions</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/receive-delivery.jpg"><img src="modern/docs/manual/shots/pages/modern/receive-delivery.jpg" alt="Receive a Delivery" width="200"></a><br><sub>Receive a Delivery</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/reports.jpg"><img src="modern/docs/manual/shots/pages/modern/reports.jpg" alt="Reports" width="200"></a><br><sub>Reports</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-schedule.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-schedule.jpg" alt="Schedule" width="200"></a><br><sub>Schedule</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/seating.jpg"><img src="modern/docs/manual/shots/pages/modern/seating.jpg" alt="Seating" width="200"></a><br><sub>Seating</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/message.jpg"><img src="modern/docs/manual/shots/pages/modern/message.jpg" alt="Send a Message" width="200"></a><br><sub>Send a Message</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/sold-out.jpg"><img src="modern/docs/manual/shots/pages/modern/sold-out.jpg" alt="Sold Out (86)" width="200"></a><br><sub>Sold Out (86)</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/split.jpg"><img src="modern/docs/manual/shots/pages/modern/split.jpg" alt="Split Check" width="200"></a><br><sub>Split Check</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/store-check.jpg"><img src="modern/docs/manual/shots/pages/modern/store-check.jpg" alt="Store Check" width="200"></a><br><sub>Store Check</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-store.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-store.jpg" alt="Store Settings" width="200"></a><br><sub>Store Settings</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-taxes.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-taxes.jpg" alt="Taxes" width="200"></a><br><sub>Taxes</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-terminals.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-terminals.jpg" alt="Terminals" width="200"></a><br><sub>Terminals</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/time-clock.jpg"><img src="modern/docs/manual/shots/pages/modern/time-clock.jpg" alt="Time Clock" width="200"></a><br><sub>Time Clock</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-requests.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-requests.jpg" alt="Time Off & Swaps" width="200"></a><br><sub>Time Off & Swaps</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-punches.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-punches.jpg" alt="Time Punches" width="200"></a><br><sub>Time Punches</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/transfer.jpg"><img src="modern/docs/manual/shots/pages/modern/transfer.jpg" alt="Transfer Check" width="200"></a><br><sub>Transfer Check</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/admin-vendors.jpg"><img src="modern/docs/manual/shots/pages/modern/admin-vendors.jpg" alt="Vendors" width="200"></a><br><sub>Vendors</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/modern/weigh.jpg"><img src="modern/docs/manual/shots/pages/modern/weigh.jpg" alt="Weigh" width="200"></a><br><sub>Weigh</sub></td>
</tr>
</table>

</details>

<details>
<summary><b>On a phone, Classic</b> (74 screens)</summary>

<table>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/floor-bar.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/floor-bar.jpg" alt="Bar" width="120"></a><br><sub>Bar</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/bar-display.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/bar-display.jpg" alt="Bar Display" width="120"></a><br><sub>Bar Display</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/tabs.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/tabs.jpg" alt="Bar Tabs" width="120"></a><br><sub>Bar Tabs</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/index-breakfast.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/index-breakfast.jpg" alt="Breakfast" width="120"></a><br><sub>Breakfast</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/library.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/library.jpg" alt="Button Library" width="120"></a><br><sub>Button Library</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-card-readers.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-card-readers.jpg" alt="Card Readers" width="120"></a><br><sub>Card Readers</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/check-options.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/check-options.jpg" alt="Check Options" width="120"></a><br><sub>Check Options</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/checklists.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/checklists.jpg" alt="Checklists" width="120"></a><br><sub>Checklists</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/modifiers.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/modifiers.jpg" alt="Choose" width="120"></a><br><sub>Choose</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/closed-checks.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/closed-checks.jpg" alt="Closed Checks" width="120"></a><br><sub>Closed Checks</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/stock-count.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/stock-count.jpg" alt="Count Stock" width="120"></a><br><sub>Count Stock</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/customer.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/customer.jpg" alt="Customer" width="120"></a><br><sub>Customer</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/customers.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/customers.jpg" alt="Customers" width="120"></a><br><sub>Customers</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/dashboard.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/dashboard.jpg" alt="Dashboard" width="120"></a><br><sub>Dashboard</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/deliveries.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/deliveries.jpg" alt="Deliveries" width="120"></a><br><sub>Deliveries</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/floor-dining.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/floor-dining.jpg" alt="Dining Area" width="120"></a><br><sub>Dining Area</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/index-dinner.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/index-dinner.jpg" alt="Dinner" width="120"></a><br><sub>Dinner</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/drawer.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/drawer.jpg" alt="Drawer" width="120"></a><br><sub>Drawer</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/drawer-reason.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/drawer-reason.jpg" alt="Drawer Reason" width="120"></a><br><sub>Drawer Reason</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-employees.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-employees.jpg" alt="Employees" width="120"></a><br><sub>Employees</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/end-of-day.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/end-of-day.jpg" alt="End of Day" width="120"></a><br><sub>End of Day</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/expo.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/expo.jpg" alt="Expo Display" width="120"></a><br><sub>Expo Display</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/factory-reset.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/factory-reset.jpg" alt="Factory Reset" width="120"></a><br><sub>Factory Reset</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/find-check.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/find-check.jpg" alt="Find a Check" width="120"></a><br><sub>Find a Check</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/find-item.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/find-item.jpg" alt="Find an Item" width="120"></a><br><sub>Find an Item</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/gift-card.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/gift-card.jpg" alt="Gift Card" width="120"></a><br><sub>Gift Card</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/guest-count.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/guest-count.jpg" alt="Guest Count" width="120"></a><br><sub>Guest Count</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/host.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/host.jpg" alt="Host Stand" width="120"></a><br><sub>Host Stand</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-inventory.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-inventory.jpg" alt="Inventory" width="120"></a><br><sub>Inventory</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/kitchen.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/kitchen.jpg" alt="Kitchen Display" width="120"></a><br><sub>Kitchen Display</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/logout.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/logout.jpg" alt="Log Out" width="120"></a><br><sub>Log Out</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/waste-log.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/waste-log.jpg" alt="Log Waste" width="120"></a><br><sub>Log Waste</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/login.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/login.jpg" alt="Login" width="120"></a><br><sub>Login</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/index-lunch.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/index-lunch.jpg" alt="Lunch" width="120"></a><br><sub>Lunch</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/tables.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/tables.jpg" alt="Main floor" width="120"></a><br><sub>Main floor</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/manager.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/manager.jpg" alt="Manager" width="120"></a><br><sub>Manager</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-meal-periods.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-meal-periods.jpg" alt="Meal Periods" width="120"></a><br><sub>Meal Periods</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/menu-all.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/menu-all.jpg" alt="Menu" width="120"></a><br><sub>Menu</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/menu-builder.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/menu-builder.jpg" alt="Menu Builder" width="120"></a><br><sub>Menu Builder</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-menu.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-menu.jpg" alt="Menu Items" width="120"></a><br><sub>Menu Items</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/merge.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/merge.jpg" alt="Merge Checks" width="120"></a><br><sub>Merge Checks</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-modifier-groups.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-modifier-groups.jpg" alt="Modifier Groups" width="120"></a><br><sub>Modifier Groups</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/move-table.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/move-table.jpg" alt="Move Check" width="120"></a><br><sub>Move Check</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/network.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/network.jpg" alt="Network" width="120"></a><br><sub>Network</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/new-tab.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/new-tab.jpg" alt="New Tab" width="120"></a><br><sub>New Tab</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/note.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/note.jpg" alt="Note" width="120"></a><br><sub>Note</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/check-list.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/check-list.jpg" alt="Open Checks" width="120"></a><br><sub>Open Checks</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/order-template.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/order-template.jpg" alt="Order" width="120"></a><br><sub>Order</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/order-later.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/order-later.jpg" alt="Order for Later" width="120"></a><br><sub>Order for Later</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/truck-orders.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/truck-orders.jpg" alt="Orders (food truck)" width="120"></a><br><sub>Orders (food truck)</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/floor-patio.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/floor-patio.jpg" alt="Patio" width="120"></a><br><sub>Patio</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/settle.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/settle.jpg" alt="Pay" width="120"></a><br><sub>Pay</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-tenders.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-tenders.jpg" alt="Payment Types" width="120"></a><br><sub>Payment Types</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/menu-popular.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/menu-popular.jpg" alt="Popular" width="120"></a><br><sub>Popular</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/prep-labels.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/prep-labels.jpg" alt="Prep Labels" width="120"></a><br><sub>Prep Labels</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-printers.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-printers.jpg" alt="Printers" width="120"></a><br><sub>Printers</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-promotions.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-promotions.jpg" alt="Promotions" width="120"></a><br><sub>Promotions</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/receive-delivery.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/receive-delivery.jpg" alt="Receive a Delivery" width="120"></a><br><sub>Receive a Delivery</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/reports.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/reports.jpg" alt="Reports" width="120"></a><br><sub>Reports</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-schedule.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-schedule.jpg" alt="Schedule" width="120"></a><br><sub>Schedule</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/seating.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/seating.jpg" alt="Seating" width="120"></a><br><sub>Seating</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/message.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/message.jpg" alt="Send a Message" width="120"></a><br><sub>Send a Message</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/sold-out.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/sold-out.jpg" alt="Sold Out (86)" width="120"></a><br><sub>Sold Out (86)</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/split.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/split.jpg" alt="Split Check" width="120"></a><br><sub>Split Check</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/store-check.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/store-check.jpg" alt="Store Check" width="120"></a><br><sub>Store Check</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-store.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-store.jpg" alt="Store Settings" width="120"></a><br><sub>Store Settings</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-taxes.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-taxes.jpg" alt="Taxes" width="120"></a><br><sub>Taxes</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-terminals.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-terminals.jpg" alt="Terminals" width="120"></a><br><sub>Terminals</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/time-clock.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/time-clock.jpg" alt="Time Clock" width="120"></a><br><sub>Time Clock</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-requests.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-requests.jpg" alt="Time Off & Swaps" width="120"></a><br><sub>Time Off & Swaps</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-punches.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-punches.jpg" alt="Time Punches" width="120"></a><br><sub>Time Punches</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/transfer.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/transfer.jpg" alt="Transfer Check" width="120"></a><br><sub>Transfer Check</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/admin-vendors.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/admin-vendors.jpg" alt="Vendors" width="120"></a><br><sub>Vendors</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-classic/weigh.jpg"><img src="modern/docs/manual/shots/pages/phone-classic/weigh.jpg" alt="Weigh" width="120"></a><br><sub>Weigh</sub></td>
</tr>
</table>

</details>

<details>
<summary><b>On a phone, Modern</b> (74 screens)</summary>

<table>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/floor-bar.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/floor-bar.jpg" alt="Bar" width="120"></a><br><sub>Bar</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/bar-display.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/bar-display.jpg" alt="Bar Display" width="120"></a><br><sub>Bar Display</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/tabs.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/tabs.jpg" alt="Bar Tabs" width="120"></a><br><sub>Bar Tabs</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/index-breakfast.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/index-breakfast.jpg" alt="Breakfast" width="120"></a><br><sub>Breakfast</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/library.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/library.jpg" alt="Button Library" width="120"></a><br><sub>Button Library</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-card-readers.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-card-readers.jpg" alt="Card Readers" width="120"></a><br><sub>Card Readers</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/check-options.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/check-options.jpg" alt="Check Options" width="120"></a><br><sub>Check Options</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/checklists.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/checklists.jpg" alt="Checklists" width="120"></a><br><sub>Checklists</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/modifiers.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/modifiers.jpg" alt="Choose" width="120"></a><br><sub>Choose</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/closed-checks.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/closed-checks.jpg" alt="Closed Checks" width="120"></a><br><sub>Closed Checks</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/stock-count.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/stock-count.jpg" alt="Count Stock" width="120"></a><br><sub>Count Stock</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/customer.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/customer.jpg" alt="Customer" width="120"></a><br><sub>Customer</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/customers.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/customers.jpg" alt="Customers" width="120"></a><br><sub>Customers</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/dashboard.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/dashboard.jpg" alt="Dashboard" width="120"></a><br><sub>Dashboard</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/deliveries.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/deliveries.jpg" alt="Deliveries" width="120"></a><br><sub>Deliveries</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/floor-dining.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/floor-dining.jpg" alt="Dining Area" width="120"></a><br><sub>Dining Area</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/index-dinner.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/index-dinner.jpg" alt="Dinner" width="120"></a><br><sub>Dinner</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/drawer.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/drawer.jpg" alt="Drawer" width="120"></a><br><sub>Drawer</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/drawer-reason.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/drawer-reason.jpg" alt="Drawer Reason" width="120"></a><br><sub>Drawer Reason</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-employees.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-employees.jpg" alt="Employees" width="120"></a><br><sub>Employees</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/end-of-day.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/end-of-day.jpg" alt="End of Day" width="120"></a><br><sub>End of Day</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/expo.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/expo.jpg" alt="Expo Display" width="120"></a><br><sub>Expo Display</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/factory-reset.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/factory-reset.jpg" alt="Factory Reset" width="120"></a><br><sub>Factory Reset</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/find-check.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/find-check.jpg" alt="Find a Check" width="120"></a><br><sub>Find a Check</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/find-item.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/find-item.jpg" alt="Find an Item" width="120"></a><br><sub>Find an Item</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/gift-card.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/gift-card.jpg" alt="Gift Card" width="120"></a><br><sub>Gift Card</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/guest-count.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/guest-count.jpg" alt="Guest Count" width="120"></a><br><sub>Guest Count</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/host.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/host.jpg" alt="Host Stand" width="120"></a><br><sub>Host Stand</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-inventory.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-inventory.jpg" alt="Inventory" width="120"></a><br><sub>Inventory</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/kitchen.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/kitchen.jpg" alt="Kitchen Display" width="120"></a><br><sub>Kitchen Display</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/logout.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/logout.jpg" alt="Log Out" width="120"></a><br><sub>Log Out</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/waste-log.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/waste-log.jpg" alt="Log Waste" width="120"></a><br><sub>Log Waste</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/login.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/login.jpg" alt="Login" width="120"></a><br><sub>Login</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/index-lunch.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/index-lunch.jpg" alt="Lunch" width="120"></a><br><sub>Lunch</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/tables.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/tables.jpg" alt="Main floor" width="120"></a><br><sub>Main floor</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/manager.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/manager.jpg" alt="Manager" width="120"></a><br><sub>Manager</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-meal-periods.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-meal-periods.jpg" alt="Meal Periods" width="120"></a><br><sub>Meal Periods</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/menu-all.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/menu-all.jpg" alt="Menu" width="120"></a><br><sub>Menu</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/menu-builder.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/menu-builder.jpg" alt="Menu Builder" width="120"></a><br><sub>Menu Builder</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-menu.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-menu.jpg" alt="Menu Items" width="120"></a><br><sub>Menu Items</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/merge.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/merge.jpg" alt="Merge Checks" width="120"></a><br><sub>Merge Checks</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-modifier-groups.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-modifier-groups.jpg" alt="Modifier Groups" width="120"></a><br><sub>Modifier Groups</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/move-table.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/move-table.jpg" alt="Move Check" width="120"></a><br><sub>Move Check</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/network.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/network.jpg" alt="Network" width="120"></a><br><sub>Network</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/new-tab.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/new-tab.jpg" alt="New Tab" width="120"></a><br><sub>New Tab</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/note.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/note.jpg" alt="Note" width="120"></a><br><sub>Note</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/check-list.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/check-list.jpg" alt="Open Checks" width="120"></a><br><sub>Open Checks</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/order-template.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/order-template.jpg" alt="Order" width="120"></a><br><sub>Order</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/order-later.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/order-later.jpg" alt="Order for Later" width="120"></a><br><sub>Order for Later</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/truck-orders.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/truck-orders.jpg" alt="Orders (food truck)" width="120"></a><br><sub>Orders (food truck)</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/floor-patio.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/floor-patio.jpg" alt="Patio" width="120"></a><br><sub>Patio</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/settle.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/settle.jpg" alt="Pay" width="120"></a><br><sub>Pay</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-tenders.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-tenders.jpg" alt="Payment Types" width="120"></a><br><sub>Payment Types</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/menu-popular.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/menu-popular.jpg" alt="Popular" width="120"></a><br><sub>Popular</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/prep-labels.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/prep-labels.jpg" alt="Prep Labels" width="120"></a><br><sub>Prep Labels</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-printers.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-printers.jpg" alt="Printers" width="120"></a><br><sub>Printers</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-promotions.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-promotions.jpg" alt="Promotions" width="120"></a><br><sub>Promotions</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/receive-delivery.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/receive-delivery.jpg" alt="Receive a Delivery" width="120"></a><br><sub>Receive a Delivery</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/reports.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/reports.jpg" alt="Reports" width="120"></a><br><sub>Reports</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-schedule.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-schedule.jpg" alt="Schedule" width="120"></a><br><sub>Schedule</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/seating.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/seating.jpg" alt="Seating" width="120"></a><br><sub>Seating</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/message.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/message.jpg" alt="Send a Message" width="120"></a><br><sub>Send a Message</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/sold-out.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/sold-out.jpg" alt="Sold Out (86)" width="120"></a><br><sub>Sold Out (86)</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/split.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/split.jpg" alt="Split Check" width="120"></a><br><sub>Split Check</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/store-check.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/store-check.jpg" alt="Store Check" width="120"></a><br><sub>Store Check</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-store.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-store.jpg" alt="Store Settings" width="120"></a><br><sub>Store Settings</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-taxes.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-taxes.jpg" alt="Taxes" width="120"></a><br><sub>Taxes</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-terminals.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-terminals.jpg" alt="Terminals" width="120"></a><br><sub>Terminals</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/time-clock.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/time-clock.jpg" alt="Time Clock" width="120"></a><br><sub>Time Clock</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-requests.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-requests.jpg" alt="Time Off & Swaps" width="120"></a><br><sub>Time Off & Swaps</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-punches.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-punches.jpg" alt="Time Punches" width="120"></a><br><sub>Time Punches</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/transfer.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/transfer.jpg" alt="Transfer Check" width="120"></a><br><sub>Transfer Check</sub></td>
</tr>
<tr>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/admin-vendors.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/admin-vendors.jpg" alt="Vendors" width="120"></a><br><sub>Vendors</sub></td>
<td align="center"><a href="modern/docs/manual/shots/pages/phone-modern/weigh.jpg"><img src="modern/docs/manual/shots/pages/phone-modern/weigh.jpg" alt="Weigh" width="120"></a><br><sub>Weigh</sub></td>
</tr>
</table>

</details>

## What it does

- **Orders:** tables and seats, courses with hold and fire, modifier choices, notes, rush and VIP. Transfer, move, merge, split and reopen checks.
- **At the table on a handheld:**
  - seat chips across the check, with a buzz and a flash for every item;
  - allergies by seat: only that guest's items are marked, and the kitchen ticket says whose;
  - each server's own usual items, and the tables as a list showing what hasn't been sent;
  - pages fitted to the phone's screen, with every key a finger tall;
  - taps kept through a Wi-Fi dead spot and sent once it's back.
- **Payments:**
  - cash, gift cards, house accounts, discounts, party gratuity, and tips asked on the customer display;
  - server banks or a drawer per screen;
  - Stripe card readers (optional; see below), with a credit card surcharge that debit cards never pay, or a cash discount with both prices shown.
- **Restaurant, food truck or retail:** chosen in the setup guide or Store Settings → **How you serve**, for the whole store or one screen.
  - **Food trucks:** no login, the screen opens on the menu and is ready for the next order after each payment. One computer is enough (a kitchen screen is optional);
  - orders are numbered from #1 each day, with the guest's name and the number of the pager handed to them;
  - **Orders** lists what's being made and waiting: **Ready** says "#27 Maria is ready: buzz pager 12", **Picked Up** frees the pager;
  - a manager's PIN opens the Manager screens, and sales can be the Counter's or, optionally, whoever is working;
  - the customer display shows **Now serving**: what's ready and what's being made;
  - a second screen cut off from the main computer keeps selling, with its own order numbers.
  - **Retail:** staff log in and start on the menu; a barcode scanned on any screen rings the item in, and nothing goes to a kitchen.
- **Guests:** a customer display, a self-order kiosk, customer records, loyalty points and rewards, promotions such as happy hour, and a waitlist and reservations with "your table is ready" texts.
- **Kitchen:** kitchen, bar and expo touchscreens with ticket times and printers. **Rush Radar** says each station's next half hour ("Grill backs up to 27 min at 7:42") from what's on the line, the tables just seated and the store's usual nights.
- **Back office:** reports by day or any range against last year (sales, items, labor, tips, kitchen times, food cost, audit), schedule and overtime by your state's rules, tip-outs and tip pools, inventory with recipes and automatic 86, and CSV and PDF export.
- **Switching over:** import the menu you already have from Square, Clover or Toast, or from any spreadsheet (Excel or CSV), with its categories, sizes, modifier groups, barcodes and photos.
- **Your look:** **Classic** or **Modern** for the whole store. Ready-made Looks, or colors taken from your logo, and every page and button editable on the screen.
- **Staff:** PIN login, clock in and out, breaks, roles with per-person permissions, manager approval on the spot, practice mode for training, and messages between screens.
- **Many screens, one store:** paired, encrypted connections. Tablets and phones find the store on the network and update themselves from it. A standby server keeps running if the main computer fails.
- **Keeps selling when the network drops:** a screen cut off from the store's computer works on its own (new checks, kitchen tickets, cash) and hands its checks in when it's back.
- **Safe:** every sale is saved at once, with automatic and optionally encrypted backups, a second copy on USB, and a damaged-database check at start.
- **Easy to read for everyone:** bigger text, high contrast, and colors that color-blind eyes tell apart, for each person or for one screen.

## Running a real store

Build your own installable packages (`.rpm` / `.deb`) with `modern/packaging/build-packages.sh`, or use the ready-made ones from the [latest release](https://github.com/ViewTouch/viewtouch/releases/latest). Install one on each computer and pick its part:

```sh
sudo vtmodern-setup store       # this computer keeps the data and has a screen
sudo vtmodern-setup server      # keeps the data, no screen (back office)
sudo vtmodern-setup terminal    # a screen that joins the store (pair it with a code from Manager → Terminals)
```

Screens start full screen at boot, with no way out for staff. Printers, backups, the standby server, the self-order kiosk, food trucks and everything else are in the **[user manual](https://github.com/ViewTouch/viewtouch/releases/download/modern-manual-0.10/ViewTouch-User-Manual.pdf)**. How it's built is in the **[developer README](modern/README.md)**.

**Card payments.** ViewTouch can take cards through Stripe card readers, but this is optional and off to start. It hasn't been tried yet on a real Stripe reader. Until it has, take cards on your processor's own terminal and ring them up in ViewTouch as Credit Card. Card numbers then never touch ViewTouch.

---

## The original ViewTouch

The original ViewTouch (X11/Motif, 1995–2025) is on the **[`legacy`](../../tree/legacy)** branch, unchanged, and its releases (v25.03.1 and earlier) stay on the [Releases](../../releases) page. New work happens here on `main`.

## History

### Origins (1979-1986)

Gene Mosher began writing Point of Sale code on an Apple ][ in 1979. ViewTouch© as we know it today was first created by restaurateur Gene Mosher and C programmer Nick Colley in **1986**, making it the **ORIGINAL Graphical Touchscreen Restaurant Point of Sale Interface**.

**First Public Demonstration:** ComDex, Las Vegas, November 1986  
[See historical photo](https://commons.wikimedia.org/wiki/File:Comdex_1986.png)

**Early Platform:** Atari ST computers (1986-1993/4 under Jack Tramiel)

**Initial Funding & Support:**
- Ed Ramsay (1986 - code development funding)
- Barbara Mosher (1986 - present, ongoing financial support)
- John King, M.D., Chicago (early funding)

### Modern Era (1995-1998)

The current version of ViewTouch was created by Gene Mosher and C programmer Richard Bradley, transitioning to:
- **UNIX (AIX)** on Motorola PowerPC (1995)
- **MIT's X Window System** for network-transparent graphical interface (1995)
- **Linux on Intel x86** (1997)

**Major Funding:** Billy Foster (1995-1998)

### Enhancement Period (2000-2004)

- **C to C++ transition** (2000-present day)
- Extensive code enhancement by Bruce King (2000-2004) Ariel Brambila Pelayo (2025)
- **Major Funding:** Doug DeLeeuw

### Open Source Era (2014-Present)

ViewTouch released under GNU Public License (GPL v3) with code hosted on GitHub. Significant modernization and refinement by the open-source community:

**Key Contributors:**
- **Jack Morrison** - Amazing debugging skills
- **NeroBurner (Reinhold Gschweicher)** - Major refactoring and standardization
- **NoOne558 (Ariel Brambila Pelayo)** - Upgraded to Xft scalable fonts, mobile integration, AI-empowered refactoring of the entire ViewTouch code base.
- **Gene and Barbara Mosher** - Lifetime support and funding
- **Ariel Brambila Pelayo** - awarded co-ownership status of ViewTouch code and documentation at GitHub

### Platform Evolution

| Year | Platform | Key Technology |
|------|----------|----------------|
| 1979 | Apple ][ | Initial POS code |
| 1986 | Atari ST | First GUI touchscreen POS |
| 1995 | AIX/PowerPC | UNIX + X Window System |
| 1997 | Linux/x86 | Intel architecture |
| 2000 | Linux/x86 | C++ transition |
| 2016+ | Raspberry Pi | Default platform (Pi 4/5) |
| 2026 | Linux · Raspberry Pi · Android | Rebuilt in C++23 and Qt 6 (QML) |

**The original's stack** (the [`legacy`](../../tree/legacy) branch):
- **OS:** Debian Linux with the XFCE desktop, and Ubuntu 24.04
- **Display:** X Window System (network-transparent)
- **Default hardware:** the Raspberry Pi family
- **Auto-update:** a desktop icon compiled and installed the latest code from GitHub

**Today's ViewTouch** (`main`): C++23 and Qt 6 (QML) on Debian 13, Raspberry Pi OS, Ubuntu 26.04 and Fedora, with Android tablets and phones as screens, signed packages and updates, and the store computer locked down for the store.

Gene Mosher's passion and vision has overseen the development, management, and maintenance of ViewTouch code across six decades, spanning a wide array of computers and point of sale equipment. Gene owns the ViewTouch copyright and remains active in ViewTouch in 2025.  A personal note to everyone from Gene: I am getting on in years and everyone has always wanted me to tell them what will happen to ViewTouch if/when something happens to me. In 2025 I can reassure that I will be participating in the development of ViewTouch until my last breath, which could easily be 2050 (I'll be 101 then), and with Ariel, who is about half a century younger than I am, being awarded co-ownership of the ViewTouch code and documentation at ViewTouch/GitHub, nothing bad can happen, is going to happen, to ViewTouch.  I myself used to wonder about the future of ViewTouch, but I no longer do wonder about the future of ViewTouch.  It's going to be alive, healthy, vibrant and dynamic for a long, long time.  A new chapter has been opened for ViewTouch in 2025 and I could not be happier about it.  Anyone can be a part of the future of ViewTouch - I welcome all who will join us.

---

## Contact & Support

### Commercial Support

**ViewTouch Official**
- **Website:** [www.viewtouch.com](http://www.viewtouch.com)
- **Email:** gene@viewtouch.com
- **Phone:** 541-515-5913

### Community

- **Discord:** [Join our server](https://discord.com/invite/ySmH2U2Mzb)
- **GitHub Issues:** Bug reports and feature requests
- **Gitter Chat:** [Join the conversation](https://gitter.im/ViewTouch/viewtouch)
- **Wiki:** Documentation and guides

The availability of ViewTouch source code and documentation at GitHub benefits clients, customers, and associates of Gene Mosher, facilitating the enhancement and extension of ViewTouch source code.

---

## License

ViewTouch is released under the **GNU General Public License, version 3 (GPL v3)**.

See [LICENSE](LICENSE) for full details.

---

## Payment Processing

ViewTouch can take cards through Stripe card readers, but this is optional and off to start (see **Card payments** under [Running a real store](#running-a-real-store)). Please make your own decisions with regard to Electronic Funds Transfer functionality, with your processor.

---

<div align="center">

**ViewTouch** - *Six decades of innovation in restaurant technology*
## Be sure to reach out to our team for any questions, we are always happy to help.😊

Made with ❤️ by the ViewTouch community

</div>
