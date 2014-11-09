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
 * inventory.hh - revision 48 (8/13/98)
 * Raw Product, Receipe & Vendor data bases
 */

#ifndef _INVENTORY_HH
#define _INVENTORY_HH

#include "utility.hh"
#include "list_utility.hh"


/**** Definitions ****/
// UnitAmount Types
#define UNIT_NONE     0
#define COUNT_SINGLE  1
#define COUNT_DOZEN   2
#define COUNT_GROSS   3
#define WEIGHT_G      21
#define WEIGHT_KG     22
#define WEIGHT_DASH   23
#define WEIGHT_OUNCE  24
#define WEIGHT_POUND  25
#define VOLUME_ML     41
#define VOLUME_L      42
#define VOLUME_TSP    43
#define VOLUME_TBS    44
#define VOLUME_OUNCE  45
#define VOLUME_QUART  46
#define VOLUME_GALLON 47
#define VOLUME_DRAM   48
#define VOLUME_CUP    49
#define VOLUME_PINT   50


/**** Global Data ****/
extern const genericChar *PurchaseUnitName[];
extern int   PurchaseUnitValue[];

extern const genericChar *RecipeUnitName[];
extern int   RecipeUnitValue[];


/**** Types ****/
// Raw Product Measurement class
class UnitAmount
{
public:
    Flt amount;
    int type;

    // Constructor
    UnitAmount();
    UnitAmount(Flt amount, int type);

    // Member Functions
    int   Clear();
    int   Read(InputDataFile &df, int version);
    int   Write(OutputDataFile &df, int version);
    int   Convert(int new_type);
    const genericChar *Description(const char *str = NULL);
    const genericChar *Measurement(const char *str = NULL);

    UnitAmount &operator = (UnitAmount &u) {
        amount = u.amount; type = u.type; return *this; }

    UnitAmount &operator *= (Flt a) {
        amount *= a; return *this; }
    UnitAmount &operator /= (Flt a) {
        amount /= a; return *this; }

    UnitAmount &operator += (UnitAmount &ua);
    UnitAmount &operator -= (UnitAmount &ua);
};

class Report;
class ItemDB;
class Check;
class Terminal;
class Inventory;

class Product
{
public:
    Product   *next, *fore;
    int        id;
    Str        name;
    UnitAmount purchase;
    int        cost;
    UnitAmount serving;

    // Constructor
    Product();

    // Member Functions
    int Read(InputDataFile &df, int version); // reads product from stream
    int Write(OutputDataFile &df, int version);  // writes product to stream
    int DoesVendorHave(int vendor_id); // boolean - does vendor carry this item?
};

class RecipePart
{
public:
    RecipePart *next, *fore;
    int         part_id; // recipe or raw product id
    UnitAmount  amount;  // amount in recipe

    // Constructor
    RecipePart();

    // Member Functions
    int Read(Inventory *inv, InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
};

class Recipe
{
    DList<RecipePart> part_list;

public:
    Recipe    *next, *fore;
    int        id;
    Str        name;
    int        prepare_time;
    int        in_menu;
    UnitAmount production;
    UnitAmount serving;

    // Constructor
    Recipe();

    // Member Functions
    RecipePart *PartList()  { return part_list.Head(); }
    int         PartCount() { return part_list.Count(); }

    int  Read(Inventory *inv, InputDataFile &df, int version);
    int  Write(OutputDataFile &df, int version);
    int  Add(RecipePart *rp);
    int  Remove(RecipePart *rp);
    int  Purge();
    int  AddIngredient(int part_id, UnitAmount &ua);
    int  RemoveIngredient(int part_id, UnitAmount &ua);
};

class Vendor
{
public:
    Vendor *next, *fore;
    int     id;
    Str     name;
    Str     address;
    Str     contact;
    Str     phone;
    Str     fax;

    // Constructor
    Vendor();

    // Member Functions
    int  Read(InputDataFile &df, int version);
    int  Write(OutputDataFile &df, int version);
};

class InvoiceEntry
{
public:
    InvoiceEntry *next, *fore;
    int        product_id;  // product in question
    UnitAmount amount;

    // Constructor
    InvoiceEntry();

    // Member Functions
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
};

class Invoice
{
    DList<InvoiceEntry> entry_list;

public:
    Invoice *next, *fore;
    int      ventor_id;
    TimeInfo time;
    int      vendor_id;
    int      id;
    Str      tracking_id;

    // Constructor
    Invoice();

    // Member Functions
    InvoiceEntry *EntryList()  { return entry_list.Head(); }
    int           EntryCount() { return entry_list.Count(); }

    int  Add(InvoiceEntry *ie);
    int  Remove(InvoiceEntry *ie);
    int  Purge();
    int  Read(InputDataFile &df, int version);
    int  Write(OutputDataFile &df, int version);
    InvoiceEntry *FindEntry(int product_id, int create = 0);
};

class StockEntry
{
public:
    StockEntry *next, *fore;
    int         product_id;
    UnitAmount  received; // calculated invoice totals
    UnitAmount  used;     // estimated amount used
    UnitAmount  final;    // final counted amount

    // Constructor
    StockEntry();

    // Member Functions
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
};

class Stock
{
    DList<StockEntry> entry_list;
    DList<Invoice>    invoice_list;

public:
    Stock   *next, *fore;
    Str      file_name;
    int      id;
    TimeInfo end_time;

    // Constructor
    Stock();

    // Member Functions
    StockEntry *EntryList()      { return entry_list.Head(); }
    StockEntry *EntryListEnd()   { return entry_list.Tail(); }
    int         EntryCount()     { return entry_list.Count(); }
    Invoice    *InvoiceList()    { return invoice_list.Head(); }
    Invoice    *InvoiceListEnd() { return invoice_list.Tail(); }
    int         InvoiceCount()   { return invoice_list.Count(); }

    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
    int Add(StockEntry *se);
    int Add(Invoice *in);
    int Remove(StockEntry *se);
    int Remove(Invoice *in);
    int Purge();
    int Load(const char *file = NULL);
    int Save();
    int Total();
    Invoice *NewInvoice(int vendor_id);

    StockEntry *FindStock(int product_id, int create = 0);
    Invoice    *FindInvoiceByRecord(int record);
};

class Inventory
{
    DList<Product> product_list;
    DList<Recipe>  recipe_list;
    DList<Vendor>  vendor_list;
    DList<Stock>   stock_list;

public:
    Str filename;
    int last_id;
    Str stock_path;
    int last_stock_id;

    // Constructor
    Inventory();

    // Member Functions
    Product *ProductList()    { return product_list.Head(); }
    Product *ProductListEnd() { return product_list.Tail(); }
    int      ProductCount()   { return product_list.Count(); }
    Recipe  *RecipeList()     { return recipe_list.Head(); }
    Recipe  *RecipeListEnd()  { return recipe_list.Tail(); }
    int      RecipeCount()    { return recipe_list.Count(); }
    Vendor  *VendorList()     { return vendor_list.Head(); }
    Vendor  *VendorListEnd()  { return vendor_list.Tail(); }
    int      VendorCount()    { return vendor_list.Count(); }
    Stock   *StockList()      { return stock_list.Head(); }
    Stock   *StockListEnd()   { return stock_list.Tail(); }
    int      StockCount()     { return stock_list.Count(); }

    int Load(const char *file);
    int Save();
    int Add(Product *pr);
    int Add(Recipe *rc);
    int Add(Vendor *v);
    int Add(Stock *s);
    int Remove(Product *pr);
    int Remove(Recipe *rc);
    int Remove(Vendor *v);
    int Remove(Stock *s);
    int Purge();
    int LoadStock(const char *path);

    int PartMatches(const char *word);
    Product *FindProductByRecord(int record);
    Product *FindProductByWord(const char *word, int &record);
    Product *FindProductByID(int id);
    Recipe  *FindRecipeByRecord(int record);
    Recipe  *FindRecipeByWord(const char *word, int &record);
    Recipe  *FindRecipeByID(int id);
    Recipe  *FindRecipeByName(const char *name);
    Vendor  *FindVendorByRecord(int record);
    Vendor  *FindVendorByWord(const char *word, int &record);
    Vendor  *FindVendorByID(int id);

    int ProductListReport(Terminal *t, Stock *s, Report *r);
    int ProductListReport(Terminal *t, Invoice *in, Report *r);
    int InvoiceReport(Terminal *t, Invoice *in, Report *r);

    int    ScanItems(ItemDB *db);
    int    ChangeRecipeName(const char *old_name, const genericChar *new_name);
    Stock *CurrentStock();
    int    MakeOrder(Check *c);
};

#endif
