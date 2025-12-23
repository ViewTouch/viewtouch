/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025
  
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

#ifndef _TABLE_ZONE_HH
#define TABLE_ZONE_HH

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
    ~CustomerInfoZone() override;

    // Member Functions
    int          Type() override { return ZONE_CUSTOMER_INFO; }
    int          RenderInit(Terminal *term, int update_flag) override;
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          LoseFocus(Terminal *term, Zone *newfocus) override;

    int LoadRecord(Terminal *term, int record) override;
    int SaveRecord(Terminal *term, int record, int write_file) override;
    int UpdateForm(Terminal *term, int record) override;
    int RecordCount(Terminal *term) override;
    int Search(Terminal *term, int record, const genericChar* word) override;
};

class CommandZone : public LayoutZone
{
    genericChar buffer[32];

public:
    CommandZone();
    // Member Functions
    int          Type() override { return ZONE_COMMAND; }
    RenderResult Render(Terminal *t, int update_flag) override;
    SignalResult Signal(Terminal *t, const genericChar* message) override;
    SignalResult Touch(Terminal *t, int tx, int ty) override;
    SignalResult Keyboard(Terminal *t, int key, int state) override;
    int          Update(Terminal *t, int update_message, const genericChar* value) override;
    const genericChar* TranslateString(Terminal *t) override;
    int          ZoneStates() override { return 1; }

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
    int          Type() override { return ZONE_TABLE; }
    std::unique_ptr<Zone> Copy() override;
    RenderResult Render(Terminal *t, int update_flag) override;
    SignalResult Signal(Terminal *t, const genericChar* message) override;
    SignalResult Touch(Terminal *t, int tx, int ty) override;
    int          Update(Terminal *t, int update_message, const genericChar* value) override;
    int         *CustomerType() override { return &customer_type; }
    Check       *GetCheck() override { return check; }
};

class GuestCountZone : public LayoutZone
{
    int min_guests;
    int okay, count;

public:
    // Constructor
    GuestCountZone();

    // Member Functions
    int          Type() override { return ZONE_GUEST_COUNT; }
    RenderResult Render(Terminal *t, int update_flag) override;
    SignalResult Signal(Terminal *t, const genericChar* message) override;
    SignalResult Keyboard(Terminal *t, int key, int state) override;
    int          Update(Terminal *t, int update_message, const genericChar* value) override;
};

class TableAssignZone : public PosZone
{
    ZoneObjectList servers;

public:
    // Member Functions
    int          Type() override { return ZONE_TABLE_ASSIGN; }
    RenderResult Render(Terminal *t, int update_flag) override;
    SignalResult Signal(Terminal *t, const genericChar* message) override;
    SignalResult Touch(Terminal *t, int tx, int ty) override;
    int          Update(Terminal *t, int update_message, const genericChar* value) override;
    int          ZoneStates() override { return 1; }

    int MoveTables(Terminal *t, ServerTableObj *sto);
};

#endif
