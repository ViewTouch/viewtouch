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
 * inventory_zone.hh - revision 28 (8/13/98)
 * Venders, Raw Products & Recipes
 */

#ifndef _INVENTORY_ZONE_HH
#define INVENTORY_ZONE_HH

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
    ~RC_Part() override;

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
    int          Type() override { return ZONE_INVENTORY; }
    RenderResult Render(Terminal *t, int update_flag) override;
    SignalResult Signal(Terminal *t, const genericChar* message) override;
    SignalResult Mouse(Terminal *t, int action, int mx, int my) override;
    Flt         *Spacing() override { return &list_spacing; }

    int LoadRecord(Terminal *t, int record) override;
    int SaveRecord(Terminal *t, int record, int write_file) override;
    int UpdateForm(Terminal *t, int record) override;
    int NewRecord(Terminal *t) override;
    int KillRecord(Terminal *t, int record) override;
    int Search(Terminal *t, int record, const genericChar* word) override;
    int ListReport(Terminal *t, Report *r) override;
    int RecordCount(Terminal *t) override;
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
    int          Type() override { return ZONE_RECIPE; }
    RenderResult Render(Terminal *t, int update_flag) override;
    SignalResult Touch(Terminal *t, int tx, int ty) override;
    Flt         *Spacing() override { return &list_spacing; }

    int LoadRecord(Terminal *t, int record) override;
    int SaveRecord(Terminal *t, int record, int write_file) override;
    int NewRecord(Terminal *t) override;
    int KillRecord(Terminal *t, int record) override;
    int Search(Terminal *t, int record, const genericChar* word) override;
    int ListReport(Terminal *t, Report *r) override;
    int RecordCount(Terminal *t) override;

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
    int          Type() override { return ZONE_VENDOR; }
    RenderResult Render(Terminal *t, int update_flag) override;
    Flt         *Spacing() override { return &list_spacing; }

    int LoadRecord(Terminal *t, int record) override;
    int SaveRecord(Terminal *t, int record, int write_file) override;
    int NewRecord(Terminal *t) override;
    int KillRecord(Terminal *t, int record) override;
    int Search(Terminal *t, int record, const genericChar* word) override;
    int ListReport(Terminal *t, Report *r) override;
    int RecordCount(Terminal *t) override;
};

class ItemListZone : public ListFormZone
{
    unsigned long phrases_changed;
    int filter_type;  // -1 = show all, or specific ITEM_* type to filter
public:
    int name_change;

    // Constructor
    ItemListZone();

    // Member Functions
    int          Type() override { return ZONE_ITEM_LIST; }
    int          AddFields();
    RenderResult Render(Terminal *t, int update_flag) override;
    SignalResult Signal(Terminal *t, const genericChar* message) override;
    SignalResult Mouse(Terminal *t, int action, int mx, int my) override;
    Flt         *Spacing() override { return &list_spacing; }

    int LoadRecord(Terminal *t, int record) override;
    int SaveRecord(Terminal *t, int record, int write_file) override;
    int Search(Terminal *t, int record, const genericChar* word) override;
    int ListReport(Terminal *t, Report *r) override;
    int RecordCount(Terminal *t) override;
    
    // Helper functions for filtering
    int GetItemTypeColor(int item_type);
    const genericChar* GetItemTypeName(int item_type);
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
    ~InvoiceZone() override;

    // Member Functions
    int          Type() override { return ZONE_INVOICE; }
    RenderResult Render(Terminal *t, int update_flag) override;
    SignalResult Signal(Terminal *t, const genericChar* message) override;
    SignalResult Touch(Terminal *t, int tx, int ty) override;
    SignalResult Mouse(Terminal *t, int action, int mx, int my) override;
    Flt         *Spacing() override { return &list_spacing; }

    int LoadRecord(Terminal *t, int record) override;
    int SaveRecord(Terminal *t, int record, int write_file) override;
    int NewRecord(Terminal *t) override;
    int KillRecord(Terminal *t, int record) override;
    int Search(Terminal *t, int record, const genericChar* word) override;
    int ListReport(Terminal *t, Report *r) override;
    int RecordCount(Terminal *t) override;
};

#endif
