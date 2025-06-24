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
 * table_zone.hh - revision 27 (9/16/98)
 * Table page & table control zone objects
 */

#pragma once  // REFACTOR: Replaced #ifndef _TABLE_ZONE_HH guard with modern pragma once

#include "check.hh"
#include "customer.hh"
#include "form_zone.hh"
#include "layout_zone.hh"
#include "report.hh"
#include "utility.hh"
#include "zone_object.hh"


/**** Types ****/
class Check;
class Employee;
class ServerTableObj;

class CustomerInfoZone : public FormZone
{
    CustomerInfo *customer;
    int           my_update;

public:
    CustomerInfoZone();
    ~CustomerInfoZone();

    // Member Functions
    int          Type() { return ZONE_CUSTOMER_INFO; }
    int          RenderInit(Terminal *term, int update_flag);
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Signal(Terminal *term, const genericChar* message);
    SignalResult Touch(Terminal *term, int tx, int ty);
    int          LoseFocus(Terminal *term, Zone *newfocus);

    int LoadRecord(Terminal *term, int record);
    int SaveRecord(Terminal *term, int record, int write_file);
    int UpdateForm(Terminal *term, int record);
    int RecordCount(Terminal *term);
    int Search(Terminal *term, int record, const genericChar* word);
};

class CommandZone : public LayoutZone
{
    genericChar buffer[32];

public:
    CommandZone();
    // Member Functions
    int          Type() { return ZONE_COMMAND; }
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Signal(Terminal *t, const genericChar* message);
    SignalResult Touch(Terminal *t, int tx, int ty);
    SignalResult Keyboard(Terminal *t, int key, int state);
    int          Update(Terminal *t, int update_message, const genericChar* value);
    const genericChar* TranslateString(Terminal *t);
    int          ZoneStates() { return 1; }

    int   TakeOut(Terminal *t);
    int   FastFood(Terminal *t);
    Zone *FindTableZone(Terminal *t, const genericChar* table);
};

class TableZone : public PosZone
{
    Check *check;
    int    stack_depth;
    int    blink;
    int    current;
    int    customer_type;

public:
    // Constructor
    TableZone();

    // Member Functions
    int          Type() { return ZONE_TABLE; }
    Zone        *Copy();
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Touch(Terminal *t, int tx, int ty);
    int          Update(Terminal *t, int update_message, const genericChar* value);
    int         *CustomerType() { return &customer_type; }
    Check       *GetCheck()     { return check; }
};

class GuestCountZone : public LayoutZone
{
    int min_guests;
    int okay, count;

public:
    // Constructor
    GuestCountZone();

    // Member Functions
    int          Type() { return ZONE_GUEST_COUNT; }
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Signal(Terminal *t, const genericChar* message);
    SignalResult Keyboard(Terminal *t, int key, int state);
    int          Update(Terminal *t, int update_message, const genericChar* value);
};

class TableAssignZone : public PosZone
{
    ZoneObjectList servers;

public:
    // Member Functions
    int          Type() { return ZONE_TABLE_ASSIGN; }
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Signal(Terminal *t, const genericChar* message);
    SignalResult Touch(Terminal *t, int tx, int ty);
    int          Update(Terminal *t, int update_message, const genericChar* value);
    int          ZoneStates() { return 1; }

    int MoveTables(Terminal *t, ServerTableObj *sto);
};
