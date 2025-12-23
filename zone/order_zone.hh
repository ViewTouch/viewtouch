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
 * order_zone.hh - revision 72 (2/27/98)
 * order entry related zone objects
 */

#ifndef _ORDER_ZONE_HH
#define ORDER_ZONE_HH

#include "layout_zone.hh"


/**** Types ****/
class Order;
class SubCheck;
class SalesItem;
class ItemDB;

// Hard code this for now.  Should probably be more flexible than this.
#define EMPLOYEE_TABLE "Employee"


// class OrderEntryZone
//   order entry window - shows order as it's being constructed
class OrderEntryZone : public LayoutZone
{
    Order *orders_shown[32];
    int    shown_count;
    int    total_orders;
    int    orders_per_page;
    int    page_no, max_pages;
    Flt    spacing;

public:
    // Constructor
    OrderEntryZone();

    // Member Functions
    int          Type() override { return ZONE_ORDER_ENTRY; }
    int          RenderInit(Terminal *term, int update_flag) override;
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Keyboard(Terminal *term, int key, int state) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          Update(Terminal *term, int update_message, const genericChar* value) override;
    int          AddQualifier(Terminal *term, int qualifier_type);
    Flt         *Spacing() override { return &spacing; }
    Flt          SpacingValue(Terminal *term);  // returns spacing value
    int          SetSize(Terminal *term, int width, int height) override;
    int          SetPosition(Terminal *term, int pos_x, int pos_y) override;

    int  CancelOrders(Terminal *term);
    int  DeleteOrder(Terminal *term, int is_void = 0);
    int  RebuildOrder(Terminal *term);
    int  NextCheck(Terminal *term);
    int  PriorCheck(Terminal *term);
    int  NextPage(Terminal *term);
    int  PriorPage(Terminal *term);
    int  ClearQualifier(Terminal *term);
    int  ShowSeat(Terminal *term, int seat);
    int  CompOrder(Terminal *term, int reason);
    int  VoidOrder(Terminal *term, int reason);
};

// class OrderPageZone
//   prior/next seat/check buttons on order entry pages
class OrderPageZone : public PosZone
{
    int amount;

public:
    // Constructor
    OrderPageZone();

    // Member Functions
    int          Type() override { return ZONE_ORDER_PAGE; }
    int          RenderInit(Terminal *term, int update_flag) override;
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          ZoneStates() override { return 3; }
    const genericChar* TranslateString(Terminal *term) override;

    int *Amount() override { return &amount; }
};

// class OrderFlowZone
//   continue button on order entry page
class OrderFlowZone : public PosZone
{
    int meal;

public:
    // Member Functions
    int          Type() override { return ZONE_ORDER_FLOW; }
    int          RenderInit(Terminal *term, int update_flag) override;
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          Update(Terminal *term, int update_message, const genericChar* value) override;
    int          ZoneStates() override { return 3; }
};

// class OrderAddZone
//   add/increase/item-count button on order entry pages
class OrderAddZone : public PosZone
{
    int mode;

public:
    // Constructor
    OrderAddZone();

    // Member Functions
    int          Type() override { return ZONE_ORDER_ADD; }
    int          RenderInit(Terminal *term, int update_flag) override;
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          Update(Terminal *term, int update_message, const genericChar* value) override;
    int          ZoneStates() override { return 3; }
};

// class OrderDeleteZone
//   delete/decrease/rebuild button on order entry pages
class OrderDeleteZone : public PosZone
{
    int mode;

public:
    // Constructor
    OrderDeleteZone();

    // Member Functions
    int          Type() override { return ZONE_ORDER_DELETE; }
    int          RenderInit(Terminal *term, int update_flag) override;
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          Update(Terminal *term, int update_message, const genericChar* value) override;
    int          ZoneStates() override { return 3; }
};

// class OrderCommentZone
//   add comment button on order entry pages
class OrderCommentZone : public PosZone
{
public:
    // Constructor
    OrderCommentZone();

    // Member Functions
    int          Type() override { return ZONE_ORDER_COMMENT; }
    int          RenderInit(Terminal *term, int update_flag) override;
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          Update(Terminal *term, int update_message, const genericChar* value) override;
    int          ZoneStates() override { return 3; }
};

// class OrderDisplayZone
//   kitchen work order display window
class OrderDisplayZone : public PosZone
{
public:
    // Constructor
    OrderDisplayZone();

    // Member Functions
    int          Type() override { return ZONE_ORDER_DISPLAY; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Keyboard(Terminal *term, int key, int state) override;
    int          ZoneStates() override { return 1; }
};

// class ItemZone
//   item/modifier ordering button on order entry pages
class ItemZone : public PosZone
{
    Str item_name;
    Str modifier_script;
    int jump_type, jump_id;
    SalesItem *item;
    int addanyway;

public:
    // Constructor
    ItemZone();

    // Member Functions
    int          Type() override { return ZONE_ITEM; }
    std::unique_ptr<Zone> Copy() override;
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const char* message) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          AddPayment(Terminal *term, int ptype, int pid, int pflags, int pamount);
    int          Update(Terminal *term, int update_message, const genericChar* value) override;
    SalesItem   *Item(ItemDB *db) override;
    const genericChar* TranslateString(Terminal *term) override { return nullptr; }

    Str *ItemName() override { return &item_name; }
    Str *Script() override   { return &modifier_script; }
    int *JumpType() override { return &jump_type; }
    int *JumpID() override   { return &jump_id;   }
};

// class QualifierZone
//   item qualifier button on order entry pages
class QualifierZone : public PosZone
{
    int qualifier_type;
    int jump_type;
    int jump_id;
    int index;

public:
    // Constructor
    QualifierZone();

    // Member Functions
    int          Type() override { return ZONE_QUALIFIER; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          Update(Terminal *term, int update_message, const genericChar* value) override;
    const genericChar* TranslateString(Terminal *term) override { return nullptr; }

    int *QualifierType() override { return &qualifier_type; }
    int *JumpType() override      { return &jump_type; }
    int *JumpID() override        { return &jump_id;   }
};

#endif
