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
 * inventory_zone.hh - revision 28 (8/13/98)
 * Venders, Raw Products & Recipes
 */

#ifndef _INVENTORY_ZONE_HH
#define _INVENTORY_ZONE_HH

#include "form_zone.hh"


/**** Types ****/
class Invoice;
class Recipe;
class RecipePart;
class Product;
class Stock;

class RC_Part : public RegionInfo
{
public:
    RC_Part    *next;
    Recipe     *rc;
    Product    *pr;
    RecipePart *rp;
    int page;
    int lit;

    // Constructor
    RC_Part();
    
    // Destructor
    virtual ~RC_Part();

    // Member Functions
    int   Render(Terminal *t);
    int   Draw(Terminal *t);
    const genericChar* Name(Terminal *t);
    int   PartID();
    int   AddIngredient(Recipe *rc);
    int   RemoveIngredient(Recipe *rc);
};

class ProductZone : public ListFormZone
{
public:
    // Constructor
    ProductZone();

    // Member Functions
    int          Type() { return ZONE_INVENTORY; }
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Signal(Terminal *t, const genericChar* message);
    SignalResult Mouse(Terminal *t, int action, int mx, int my);
    Flt         *Spacing() { return &list_spacing; }

    int LoadRecord(Terminal *t, int record);
    int SaveRecord(Terminal *t, int record, int write_file);
    int UpdateForm(Terminal *t, int record);
    int NewRecord(Terminal *t);
    int KillRecord(Terminal *t, int record);
    int Search(Terminal *t, int record, const genericChar* word);
    int ListReport(Terminal *t, Report *r);
    int RecordCount(Terminal *t);
};

class RecipeZone : public ListFormZone
{
    SList<RC_Part> part_list;
    SList<RC_Part> recipe_list;
    int part_page;
    int max_pages;

public:
    // Constructor
    RecipeZone();

    // Member Functions
    int          Type() { return ZONE_RECIPE; }
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Touch(Terminal *t, int tx, int ty);
    Flt         *Spacing() { return &list_spacing; }

    int LoadRecord(Terminal *t, int record);
    int SaveRecord(Terminal *t, int record, int write_file);
    int NewRecord(Terminal *t);
    int KillRecord(Terminal *t, int record);
    int Search(Terminal *t, int record, const genericChar* word);
    int ListReport(Terminal *t, Report *r);
    int RecordCount(Terminal *t);

    int  MakeRecipe(Terminal *t, Recipe *rc);
    int  LayoutParts();
    int  LayoutRecipe();
};

class VendorZone : public ListFormZone
{
public:
    // Constructor
    VendorZone();

    // Member Functions
    int          Type() { return ZONE_VENDOR; }
    RenderResult Render(Terminal *t, int update_flag);
    Flt         *Spacing() { return &list_spacing; }

    int LoadRecord(Terminal *t, int record);
    int SaveRecord(Terminal *t, int record, int write_file);
    int NewRecord(Terminal *t);
    int KillRecord(Terminal *t, int record);
    int Search(Terminal *t, int record, const genericChar* word);
    int ListReport(Terminal *t, Report *r);
    int RecordCount(Terminal *t);
};

class ItemListZone : public ListFormZone
{
    unsigned long phrases_changed;
public:
    int name_change;

    // Constructor
    ItemListZone();

    // Member Functions
    int          Type() { return ZONE_ITEM_LIST; }
    int          AddFields();
    RenderResult Render(Terminal *t, int update_flag);
    Flt         *Spacing() { return &list_spacing; }

    int LoadRecord(Terminal *t, int record);
    int SaveRecord(Terminal *t, int record, int write_file);
    int Search(Terminal *t, int record, const genericChar* word);
    int ListReport(Terminal *t, Report *r);
    int RecordCount(Terminal *t);
};

class InvoiceZone : public ListFormZone
{
    Report *invoice_report;
    int     invoice_page;
    int     edit;
    int     entry_no;

public:
    // Constructor
    InvoiceZone();
    // Destructor
    ~InvoiceZone();

    // Member Functions
    int          Type() { return ZONE_INVOICE; }
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Signal(Terminal *t, const genericChar* message);
    SignalResult Touch(Terminal *t, int tx, int ty);
    SignalResult Mouse(Terminal *t, int action, int mx, int my);
    Flt         *Spacing() { return &list_spacing; }

    int LoadRecord(Terminal *t, int record);
    int SaveRecord(Terminal *t, int record, int write_file);
    int NewRecord(Terminal *t);
    int KillRecord(Terminal *t, int record);
    int Search(Terminal *t, int record, const genericChar* word);
    int ListReport(Terminal *t, Report *r);
    int RecordCount(Terminal *t);
};

#endif
