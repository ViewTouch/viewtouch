/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998  
  
 *   This program is free software: you can redistribute it and/or modify 
 *   it under the terms of the GNU General Public License as published by 
 *   the Free Software Foundation, either version 3 of the License, or 
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful, 
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 *   GNU General Public License for more details. 
 * 
 *   You should have received a copy of the GNU General Public License 
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. 
 *
 * drawer_zone.hh - revision 29 (2/5/98)
 * Drawer assignment, balancing zones
 */

#ifndef _DRAWER_ZONE_HH
#define _DRAWER_ZONE_HH

#include "layout_zone.hh"
#include "zone_object.hh"
#include "expense.hh"


/**** Definitions ****/
#define MAX_BALANCES 64
#define MAX_PAYMENTS 3

#define DRAWER_ZONE_BALANCE  1  // the normal drawer pull/balance zone
#define DRAWER_ZONE_SELECT   2  // for payouts, select a zone

/**** Types ****/
class Employee;
class Report;
class Check;
class Drawer;
class DrawerBalance;
class DrawerPayment;
class DrawerObj;

class DrawerAssignZone : public PosZone
{
    ZoneObjectList servers;

public:
    // Member Functions
    int          Type() { return ZONE_DRAWER_ASSIGN; }
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Touch(Terminal *t, int tx, int ty);
    int          Update(Terminal *t, int update_message, const genericChar *value);
    int          ZoneStates() { return 1; }

    int MoveDrawers(Terminal *t, Employee *user);
};

class DrawerManageZone : public LayoutZone
{
    ZoneObjectList drawers;
    ZoneObject    *current;
    int            drawers_shown;
    int            group;
    Report        *report;
    int            mode;
    int            view;
    Drawer        *drawer_list;
    Check         *check_list;
    int            page;
    int            max_pages;  // page controls
    int            media;
    Flt            spacing;
    DrawerBalance *balance_list[MAX_BALANCES];
    DrawerPayment *payment_list[MAX_PAYMENTS];
    int            balances;
    int            expenses;
    int            drawer_zone_type;

public:
    // Constructor
    DrawerManageZone();
    // Destructor
    ~DrawerManageZone();

    // Member Functions
    int          Type() { return ZONE_DRAWER_MANAGE; }
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Signal(Terminal *t, const genericChar *message);
    SignalResult Touch(Terminal *t, int tx, int ty);
    SignalResult Keyboard(Terminal *t, int key, int state);
    int          Update(Terminal *t, int update_message, const genericChar *value);
    Flt         *Spacing() { return &spacing; }
    int         *DrawerZoneType() { return &drawer_zone_type; }

    int CreateDrawers(Terminal *t);
    int Print(Terminal *t, int mode);
};

#endif
