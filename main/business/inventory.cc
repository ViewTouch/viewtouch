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
 * inventory.cc - revision 65 (9/8/98)
 * Implementation of inventory module
 */

#include "inventory.hh"
#include "data_file.hh"
#include "report.hh"
#include "sales.hh"
#include "check.hh"
#include "employee.hh"
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <dirent.h>
#include <cstring>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** Definitions ****/
#define MAX_PARTS 18

/**** Global Data ****/
const char* PurchaseUnitName[] = {
    "Count - Single", "Count - Dozen", "Count - Gross",
    "Weight - Ounce", "Weight - Pound",
    "Volume - Ounce", "Volume - Pint",
    "Volume - Quart", "Volume - Gallon",
    "Weight - Gram", "Weight - Kilogram",
    "Volume - Mililiter", "Volume - Liter", NULL};
int PurchaseUnitValue[] = {
    COUNT_SINGLE, COUNT_DOZEN, COUNT_GROSS,
    WEIGHT_OUNCE, WEIGHT_POUND,
    VOLUME_OUNCE, VOLUME_PINT,
    VOLUME_QUART, VOLUME_GALLON,
    WEIGHT_G, WEIGHT_KG, VOLUME_ML, VOLUME_L, -1};

const char* RecipeUnitName[] = {
    "Count - Single",
    "Weight - Dash", "Weight - Ounce",
    "Volume - Dram", "Volume - TSP", "Volume - TBS", "Volume - Ounce",
    "Volume - Cup", "Volume - Pint", "Volume - Quart",
    "Weight - Gram", "Weight - Kilogram",
    "Volume - Mililiter", "Volume - Liter", NULL};
int RecipeUnitValue[] = {
    COUNT_SINGLE,
    WEIGHT_DASH, WEIGHT_OUNCE,
    VOLUME_DRAM, VOLUME_TSP, VOLUME_TBS, VOLUME_OUNCE,
    VOLUME_CUP, VOLUME_PINT, VOLUME_QUART,
    WEIGHT_G, WEIGHT_KG, VOLUME_ML, VOLUME_L, -1};


/**** UnitAmount Class ****/
// Constructors
UnitAmount::UnitAmount()
{
    amount = 0.0;
    type   = UNIT_NONE;
}

UnitAmount::UnitAmount(Flt a, int t)
{
    amount = a;
    type   = t;
}

// Member Functions
int UnitAmount::Clear()
{
    type   = UNIT_NONE;
    amount = 0.0;
    return 0;
}

int UnitAmount::Read(InputDataFile &df, int version)
{
    FnTrace("UnitAmount::Read()");
    int error = 0, val = 0;
    error += df.Read(val);
    amount = (Flt) val / 100.0;
    error += df.Read(type);
    return error;
}

int UnitAmount::Write(OutputDataFile &df, int version)
{
    FnTrace("UnitAmount::Write()");
    int error = 0, val = (int) (amount * 100 + .5);
    error += df.Write(val);
    error += df.Write(type);
    return error;
}

int UnitAmount::Convert(int new_type)
{
    FnTrace("UnitAmount::Convert()");
    if (new_type == type)
        return 0; // that was easy!

    //printf("%s to ", Description());

    int metric = 0;
    Flt count = 0.0, volume = 0.0, weight = 0.0;

    switch (type)
    {
    case COUNT_SINGLE:  count  = amount; break;
    case COUNT_DOZEN:   count  = amount * 12; break;
    case COUNT_GROSS:   count  = amount * 144; break;

        // Standard
    case WEIGHT_DASH:   weight = amount / 16; break;
    case WEIGHT_OUNCE:  weight = amount; break;
    case WEIGHT_POUND:  weight = amount * 16; break;
    case VOLUME_DRAM:   volume = amount / 16; break;
    case VOLUME_TSP:    volume = amount / 6; break;
    case VOLUME_TBS:    volume = amount / 2; break;
    case VOLUME_OUNCE:  volume = amount; break;
    case VOLUME_CUP:    volume = amount * 8; break;
    case VOLUME_PINT:   volume = amount * 16; break;
    case VOLUME_QUART:  volume = amount * 32; break;
    case VOLUME_GALLON: volume = amount * 128; break;

        // Metric
    case WEIGHT_G:  metric = 1; weight = amount; break;
    case WEIGHT_KG: metric = 1; weight = amount * 1000; break;
    case VOLUME_ML: metric = 1; volume = amount; break;
    case VOLUME_L:  metric = 1; volume = amount * 1000; break;
    }

    type   = new_type;
    amount = 0.0;

    if (metric)
    {
        if (type == VOLUME_DRAM || type == VOLUME_TSP || type == VOLUME_TBS ||
            type == VOLUME_OUNCE || type == VOLUME_CUP || type == VOLUME_PINT ||
            type == VOLUME_QUART || type == VOLUME_GALLON)
            ; // FIX - do mililiter to fluid ounce conversion
        else if (type == WEIGHT_DASH || type == WEIGHT_OUNCE ||
                 type == WEIGHT_POUND)
        {
            // FIX - do gram to ounce conversion
        }
    }
    else
    {
        if (type == WEIGHT_G || type == WEIGHT_KG)
        {
            // FIX - do ounce to gram conversion
        }
        else if (type == VOLUME_ML || type == VOLUME_L)
        {
            // FIX - do fluid ounce to mililiter conversion
        }
    }

    switch (type)
    {
    case COUNT_SINGLE:  amount = count; break;
    case COUNT_DOZEN:   amount = count / 12; break;
    case COUNT_GROSS:   amount = count / 144; break;

        // Standard
    case WEIGHT_DASH:   amount = weight * 16; break;
    case WEIGHT_OUNCE:  amount = weight; break;
    case WEIGHT_POUND:  amount = weight / 16; break;
    case VOLUME_DRAM:   amount = volume * 16; break;
    case VOLUME_TSP:    amount = volume * 6; break;
    case VOLUME_TBS:    amount = volume * 2; break;
    case VOLUME_OUNCE:  amount = volume; break;
    case VOLUME_CUP:    amount = volume / 8; break;
    case VOLUME_PINT:   amount = volume / 16; break;
    case VOLUME_QUART:  amount = volume / 32; break;
    case VOLUME_GALLON: amount = volume / 128; break;

        // Metric
    case WEIGHT_G:  amount = weight; break;
    case WEIGHT_KG: amount = weight / 1000; break;
    case VOLUME_ML: amount = volume; break;
    case VOLUME_L:  amount = volume / 1000; break;
    }

    //printf("%s\n", Description());
    return 0;
}

char* UnitAmount::Description(char* str)
{
    FnTrace("UnitAmount::Description()");
    static genericChar buffer[256];
    if (str == NULL)
        str = buffer;

    str[0] = '\0';
    switch (type)
    {
    case UNIT_NONE:     strcpy(str, "---"); break;
    case COUNT_SINGLE:  sprintf(str, "%g", amount); break;
    case COUNT_DOZEN:   sprintf(str, "%g Dzn.", amount); break;
    case COUNT_GROSS:   sprintf(str, "%g Grs.", amount); break;

        // Standard Units
    case WEIGHT_DASH:   sprintf(str, "%g", amount); break;
    case WEIGHT_OUNCE:  sprintf(str, "%g Ou.", amount); break;
    case WEIGHT_POUND:  sprintf(str, "%g Lbs.", amount); break;
    case VOLUME_DRAM:   sprintf(str, "%g Dram", amount); break;
    case VOLUME_TSP:    sprintf(str, "%g Tsp.", amount); break;
    case VOLUME_TBS:    sprintf(str, "%g Tbs.", amount); break;
    case VOLUME_OUNCE:  sprintf(str, "%g Oz.", amount); break;
    case VOLUME_CUP:    sprintf(str, "%g Cup", amount); break;
    case VOLUME_PINT:   sprintf(str, "%g Pint", amount); break;
    case VOLUME_QUART:  sprintf(str, "%g Qt.", amount); break;
    case VOLUME_GALLON: sprintf(str, "%g Gal.", amount); break;

        // Metric Units
    case WEIGHT_G:  sprintf(str, "%g g", amount); break;
    case WEIGHT_KG: sprintf(str, "%g kg", amount); break;
    case VOLUME_ML: sprintf(str, "%g ml", amount); break;
    case VOLUME_L:  sprintf(str, "%g l", amount); break;
    }
    return str;
}

char* UnitAmount::Measurement( char* str)
{
    FnTrace("UnitAmount::Measurement()");
    static genericChar buffer[16];
    if (str == NULL)
        str = buffer;

    str[0] = '\0';
    switch (type)
    {
    case UNIT_NONE:     strcpy(str, "---"); break;
    case COUNT_DOZEN:   strcpy(str, "Dzn."); break;
    case COUNT_GROSS:   strcpy(str, "Grs."); break;

        // Standard Units
    case WEIGHT_OUNCE:  strcpy(str, "Ou."); break;
    case WEIGHT_POUND:  strcpy(str, "Lbs."); break;
    case VOLUME_DRAM:   strcpy(str, "Dram"); break;
    case VOLUME_TSP:    strcpy(str, "Tsp."); break;
    case VOLUME_TBS:    strcpy(str, "Tbs."); break;
    case VOLUME_OUNCE:  strcpy(str, "Oz."); break;
    case VOLUME_CUP:    strcpy(str, "Cup"); break;
    case VOLUME_PINT:   strcpy(str, "Pint"); break;
    case VOLUME_QUART:  strcpy(str, "Qt."); break;
    case VOLUME_GALLON: strcpy(str, "Gal."); break;

        // Metric Units
    case WEIGHT_G:      strcpy(str, "g"); break;
    case WEIGHT_KG:     strcpy(str, "kg"); break;
    case VOLUME_ML:     strcpy(str, "ml"); break;
    case VOLUME_L:      strcpy(str, "l"); break;
    }
    return str;
}

UnitAmount &UnitAmount::operator+= (UnitAmount &ua)
{
    FnTrace("UnitAmount::operator +=()");
    if (type == UNIT_NONE)
    {
        type   = ua.type;
        amount = 0.0;
    }

    UnitAmount tmp = ua;
    tmp.Convert(type);
    amount += tmp.amount;
    return *this;
}

UnitAmount &UnitAmount::operator-= (UnitAmount &ua)
{
    FnTrace("UnitAmount::operator -=()");
    if (type == UNIT_NONE)
    {
        type   = ua.type;
        amount = 0.0;
    }

    UnitAmount tmp = ua;
    tmp.Convert(type);
    amount -= tmp.amount;
    return *this;
}


/**** Product Class ****/
// Constructor
Product::Product()
{
    next = NULL;
    fore = NULL;
    cost = 0;
    id   = 0;
}

// Member Functions
int Product::Read(InputDataFile &df, int version)
{
    FnTrace("Product::Read()");
    int error = 0;
    error += df.Read(id);
    error += df.Read(name);
    error += purchase.Read(df, 1);
    error += df.Read(cost);
    error += serving.Read(df, 1);
    if (version == 5)
    {
        Flt val;
        error += df.Read(val);
    }
    return error;
}

int Product::Write(OutputDataFile &df, int version)
{
    FnTrace("Product::Write()");
    // Write version 7
    int error = 0;
    error += df.Write(id);
    error += df.Write(name);
    error += purchase.Write(df, 1);
    error += df.Write(cost);
    error += serving.Write(df, 1);
    return error;
}

int Product::DoesVendorHave(int vendor_id)
{
    FnTrace("Product::DoesVendorHave()");
    // FIX - vendor filtering needs to be finished
    return 1;
}

/**** RecipePart Class ****/
// Constructor
RecipePart::RecipePart()
{
    next    = NULL;
    fore    = NULL;
    part_id = 0;
}

// Member Functions
int RecipePart::Read(Inventory *inv, InputDataFile &df, int version)
{
    FnTrace("RecipePart::Read()");
    int error = 0;
    error += df.Read(part_id);
    if (version <= 6)
    {
        int amt;
        error += df.Read(amt);
        Product *p = inv->FindProductByID(part_id);
        if (p)
        {
            amount = p->serving;
            amount *= amt;
        }
    }
    else
        error += amount.Read(df, 1);
    return error;
}

int RecipePart::Write(OutputDataFile &df, int version)
{
    FnTrace("RecipePart::Write()");
    int error = 0;
    error += df.Write(part_id);
    error += amount.Write(df, 1);
    return error;
}

/**** Recipe Class ****/
// Constructor
Recipe::Recipe()
{
    next              = NULL;
    fore              = NULL;
    prepare_time      = 0;
    id                = 0;
    in_menu           = 0;
    production.amount = 1.0;
    production.type   = COUNT_SINGLE;
    serving.amount    = 1.0;
    serving.type      = COUNT_SINGLE;
}

// Member Functions
int Recipe::Read(Inventory *inv, InputDataFile &df, int version)
{
    FnTrace("Recipe::Read()");
    int error = 0;
    int i = 0;
    int n = 0;

    error += df.Read(id);
    error += df.Read(name);
    error += df.Read(prepare_time);
    if (version >= 4)
    {
        error += production.Read(df, 1);
        error += serving.Read(df, 1);
    }
    error += df.Read(n);
    for (i = 0; i < n; ++i)
    {
        if (df.end_of_file)
            return 1;
        RecipePart *rp = new RecipePart;
        error += rp->Read(inv, df, version);
        Add(rp);
    }
    if (version == 5)
    {
        Flt val;
        error += df.Read(val);
    }
    return error;
}

int Recipe::Write(OutputDataFile &df, int version)
{
    FnTrace("Recipe::Write()");
    // Write version 7
    int error = 0;
    error += df.Write(id);
    error += df.Write(name);
    error += df.Write(prepare_time);
    error += production.Write(df, 1);
    error += serving.Write(df, 1);

    error += df.Write(PartCount());
    for (RecipePart *rp = PartList(); rp != NULL; rp = rp->next)
        rp->Write(df, version);
    return error;
}

int Recipe::Add(RecipePart *rp)
{
    FnTrace("Recipe::Add()");
    return part_list.AddToTail(rp);
}

int Recipe::Remove(RecipePart *rp)
{
    FnTrace("Recipe::Remove()");
    return part_list.Remove(rp);
}

int Recipe::Purge()
{
    FnTrace("Recipe::Purge()");
    part_list.Purge();
    return 0;
}

int Recipe::AddIngredient(int part_id, UnitAmount &ua)
{
    FnTrace("Recipe::AddIngredient()");
    if (part_id <= 0)
        return 1;

    RecipePart *rp = PartList();
    while (rp)
    {
        if (part_id == rp->part_id)
        {
            rp->amount += ua;
            return 0;
        }
        rp = rp->next;
    }

    if (PartCount() >= MAX_PARTS)
        return 1;

    rp = new RecipePart;
    rp->part_id = part_id;
    rp->amount  = ua;
    Add(rp);
    return 0;
}

int Recipe::RemoveIngredient(int part_id, UnitAmount &ua)
{
    FnTrace("Recipe::RemoveIngredient()");
    RecipePart *rp = PartList();
    while (rp)
    {
        if (rp->part_id == part_id)
        {
            rp->amount -= ua;
            if (rp->amount.amount <= 0)
            {
                Remove(rp);
                delete rp;
            }
            return 0;
        }
        rp = rp->next;
    }
    return 1;
}

/**** Vendor Class ****/
// Constructor
Vendor::Vendor()
{
    next = NULL;
    fore = NULL;
    id   = 0;
}

// Member Functions
int Vendor::Read(InputDataFile &df, int version)
{
    FnTrace("Vendor::Read()");
    int error = 0;
    error += df.Read(id);
    error += df.Read(name);
    error += df.Read(address);
    error += df.Read(contact);
    error += df.Read(phone);
    FixPhoneNumber(phone);
    error += df.Read(fax);
    FixPhoneNumber(fax);
    return error;
}

int Vendor::Write(OutputDataFile &df, int version)
{
    FnTrace("Vendor::Write()");
    int error = 0;
    error += df.Write(id);
    error += df.Write(name);
    error += df.Write(address);
    error += df.Write(contact);
    error += df.Write(phone);
    error += df.Write(fax);
    return error;
}

/**** Inventory Class ****/
// Constructor
Inventory::Inventory()
{
    last_id          = 0;
    last_stock_id    = 0;
}

// Member Functions
int Inventory::Load(const char* file)
{
    FnTrace("Inventory::Load()");
    if (file)
        filename.Set(file);

    int version = 0;
    InputDataFile df;
    if (df.Open(filename.Value(), version))
        return 1;

    if (version < 3)
    {
        genericChar str[256];
        sprintf(str, "Unknown Inventory version %d", version);
        ReportError(str);
        return 1;
    }

    // VERSION NOTES
    // 3 (??/??/96)  earliest supported version
    // 7 (< 1/21/97) started keeping notes on this date

    int n = 0;
    int error = df.Read(n);
    int i;

    for (i = 0; i < n; ++i)
    {
        if (df.end_of_file)
        {
            ReportError("Unexpected end of Products in 'inventory.dat' file");
            return 1;
        }

        Product *pr = new Product;
        error += pr->Read(df, version);
        Add(pr);
    }

    error += df.Read(n);
    for (i = 0; i < n; ++i)
    {
        if (df.end_of_file)
        {
            ReportError("Unexpected end of Recipes in 'inventory.dat' file");
            return 1;
        }

        Recipe *rc = new Recipe;
        error += rc->Read(this, df, version);
        Add(rc);
    }

    error += df.Read(n);
    for (i = 0; i < n; ++i)
    {
        if (df.end_of_file)
        {
            ReportError("Unexpected end of Vendors in 'inventory.dat' file");
            return 1;
        }

        Vendor *v = new Vendor;
        error += v->Read(df, version);
        Add(v);
    }
    return error;
}

int Inventory::Save()
{
    FnTrace("Inventory::Save()");
    if (filename.empty())
        return 1;

    BackupFile(filename.Value());

    // Save version 7
    OutputDataFile df;
    if (df.Open(filename.Value(), 7))
        return 1;

    int error = 0;
    error += df.Write(ProductCount());
    for (Product *pr = ProductList(); pr != NULL; pr = pr->next)
        error += pr->Write(df, 7);

    error += df.Write(RecipeCount());
    for (Recipe *rc = RecipeList(); rc != NULL; rc = rc->next)
        error += rc->Write(df, 7);

    error += df.Write(VendorCount());
    for (Vendor *v = VendorList(); v != NULL; v = v->next)
        error += v->Write(df, 7);

    return error;
}

int Inventory::Add(Product *pr)
{
    FnTrace("Inventory::Add(Product)");
    if (pr == NULL)
        return 1;

    // Start at end of list and work backwords
    Product *ptr = ProductListEnd();
    while (ptr && pr->name < ptr->name)
        ptr = ptr->fore;

    // Insert pr after ptr
    product_list.AddAfterNode(ptr, pr);

    // Assign an ID if product doesn't have one
    if (pr->id <= 0)
        pr->id = ++last_id;
    else if (pr->id > last_id)
        last_id = pr->id;
    return 0;
}

int Inventory::Add(Recipe *rc)
{
    FnTrace("Inventory::Add(Recipe)");
    if (rc == NULL)
        return 1;

    // Start at end of list and work backwords
    Recipe *ptr = RecipeListEnd();
    while (ptr && rc->name < ptr->name)
        ptr = ptr->fore;

    // Insert rc after ptr
    recipe_list.AddAfterNode(ptr, rc);

    // Assign an ID if recipe doesn't have one
    if (rc->id <= 0)
        rc->id = ++last_id;
    else if (rc->id > last_id)
        last_id = rc->id;
    return 0;
}

int Inventory::Add(Vendor *v)
{
    FnTrace("Inventory::Add(Vendor)");
    if (v == NULL)
        return 1;

    // Start at end of list and work backwords
    Vendor *ptr = VendorListEnd();
    while (ptr && v->name < ptr->name)
        ptr = ptr->fore;

    // Insert v after ptr
    vendor_list.AddAfterNode(ptr, v);

    // Assign an ID if vendor doesn't have one
    if (v->id <= 0)
        v->id = ++last_id;
    else if (v->id > last_id)
        last_id = v->id;
    return 0;
}

int Inventory::Add(Stock *s)
{
    FnTrace("Inventory::Add(Stock)");
    if (s == NULL)
        return 1;

    stock_list.AddToTail(s);

    if (s->id <= 0)
        s->id = ++last_stock_id;
    else if (s->id > last_stock_id)
        last_stock_id = s->id;
    return 0;
}

int Inventory::Remove(Product *pr)
{
    FnTrace("Inventory::Remove(Product)");
    return product_list.Remove(pr);
}

int Inventory::Remove(Recipe *rc)
{
    FnTrace("Inventory::Remove(Recipe)");
    return recipe_list.Remove(rc);
}

int Inventory::Remove(Vendor *v)
{
    FnTrace("Inventory::Remove(Vendor)");
    return vendor_list.Remove(v);
}

int Inventory::Remove(Stock *s)
{
    FnTrace("Inventory::Remove(Stock)");
    return stock_list.Remove(s);
}

int Inventory::Purge()
{
    FnTrace("Inventory::Purge()");
    product_list.Purge();
    recipe_list.Purge();
    vendor_list.Purge();
    stock_list.Purge();
    return 0;
}

int Inventory::LoadStock(const char* path)
{
    FnTrace("Inventory::LoadStock()");
    if (path)
        stock_path.Set(path);

    DIR *dp = opendir(stock_path.Value());
    if (dp == NULL)
        return 1;  // Error - can't find directory

    struct dirent *record = NULL;
    do
    {
        record = readdir(dp);
        if (record)
        {
            const genericChar* name = record->d_name;
            int len = strlen(name);
            if (strcmp(&name[len-4], ".fmt") == 0)
                continue;
            if (strcmp(&name[len-4], ".bak") == 0)
                continue;
            if (strncmp(name, "stock_", 6) == 0)
            {
                genericChar str[256];
                sprintf(str, "%s/%s", stock_path.Value(), name);
                Stock *s = new Stock;
                if (s == NULL)
                    ReportError("Couldn't create stock");
                else
                {
                    s->Load(str);
                    Add(s);
                }
            }
        }
    }
    while (record);

    closedir(dp);
    return 0;
}

int Inventory::PartMatches(const char* word)
{
    FnTrace("Inventory::PartMatches()");
    if (word == NULL)
        return 0;

    int match = 0;
    int len = strlen(word);
    for (Product *pr = ProductList(); pr != NULL; pr = pr->next)
        if (StringCompare(pr->name.Value(), word, len) == 0)
            ++match;

    for (Recipe *rc = RecipeList(); rc != NULL; rc = rc->next)
        if (StringCompare(rc->name.Value(), word, len) == 0)
            ++match;
    return match;
}

Product *Inventory::FindProductByRecord(int record)
{
    FnTrace("Inventory::FindProductByRecord()");
    return product_list.Index(record);
}

Product *Inventory::FindProductByWord(const char* word, int &record)
{
    FnTrace("Inventory::FindProductByWord()");
    if (word == NULL)
        return NULL;

    record = 0;
    int len = strlen(word);
    for (Product *pr = ProductList(); pr != NULL; pr = pr->next)
    {
        if (StringCompare(pr->name.Value(), word, len) == 0)
            return pr;
        ++record;
    }
    return NULL;
}

Product *Inventory::FindProductByID(int id)
{
    FnTrace("Inventory::FindProductByID()");
    for (Product *pr = ProductList(); pr != NULL; pr = pr->next)
        if (pr->id == id)
            return pr;
    return NULL;
}

Recipe *Inventory::FindRecipeByRecord(int record)
{
    FnTrace("Inventory::FindRecipeByRecord()");
    return recipe_list.Index(record);
}

Recipe *Inventory::FindRecipeByWord(const char* word, int &record)
{
    FnTrace("Inventory::FindRecipeByWord()");
    if (word == NULL)
        return NULL;

    record = 0;
    int len = strlen(word);
    for (Recipe *rc = RecipeList(); rc != NULL; rc = rc->next)
    {
        if (StringCompare(rc->name.Value(), word, len) == 0)
            return rc;
        ++record;
    }
    return NULL;
}

Recipe *Inventory::FindRecipeByID(int id)
{
    FnTrace("Inventory::FindRecipeByID()");
    for (Recipe *rc = RecipeList(); rc != NULL; rc = rc->next)
        if (rc->id == id)
            return rc;
    return NULL;
}

Recipe *Inventory::FindRecipeByName(const char* name)
{
    FnTrace("Inventory::FindRecipeByName()");
    for (Recipe *rc = RecipeList(); rc != NULL; rc = rc->next)
        if (StringCompare(rc->name.Value(), name) == 0)
            return rc;
    return NULL;
}

Vendor *Inventory::FindVendorByRecord(int record)
{
    FnTrace("Inventory::FindVendorByRecord()");
    return vendor_list.Index(record);
}

Vendor *Inventory::FindVendorByWord(const char* word, int &record)
{
    FnTrace("Inventory::FindVendorByWord()");
    if (word == NULL)
        return NULL;

    record = 0;
    int len = strlen(word);
    for (Vendor *v = VendorList(); v != NULL; v = v->next)
    {
        if (StringCompare(v->name.Value(), word, len) == 0)
            return v;
        ++record;
    }
    return NULL;
}

Vendor *Inventory::FindVendorByID(int id)
{
    FnTrace("Inventory::FindVendorByID()");
    for (Vendor *v = VendorList(); v != NULL; v = v->next)
        if (id == v->id)
            return v;
    return NULL;
}

int Inventory::ProductListReport(Terminal *t, Stock *s, Report *r)
{
    FnTrace("Inventory::ProductListReport(Stock)");
    if (r == NULL)
        return 1;

    if (s == NULL)
    {
        r->TextC("Can't find stock information");
        return 0;
    }

    Product *pr = ProductList();
    if (pr == NULL)
    {
        r->TextC("There are no products definied");
        return 0;
    }

    s->Total();
    genericChar str[256];
    UnitAmount ua1, ua2, ua3, ua4, used;
    while (pr)
    {
        int type = pr->purchase.type;
        r->TextL(pr->name.Value());
        StockEntry *se = s->FindStock(pr->id);
        if (se)
        {
            used = se->used;
            ua1  = se->received;
            ua2  = ua1;
            ua2 -= used;
            ua3  = se->final;
            ua4  = ua2;
            ua4 -= ua3;
        }
        else
        {
            ua1.Clear();
            ua2.Clear();
            ua3.Clear();
            ua4.Clear();
            used.Clear();
        }
        ua1.Convert(type);
        ua2.Convert(type);
        ua3.Convert(type);
        ua4.Convert(type);

        r->TextPosL(-35, ua1.Measurement());
        if (s->end_time.IsSet())
        {
            sprintf(str, "%g", ua3.amount);
            r->TextPosR(-22, str);
            sprintf(str, "%g", ua2.amount);
            r->TextPosR(-11, str);
            sprintf(str, "%g", ua4.amount);
            r->TextR(str);
        }
        else
        {
            sprintf(str, "%g", ua1.amount);
            r->TextPosR(-22, str);
            sprintf(str, "%g", used.amount);
            r->TextPosR(-11, str);
            sprintf(str, "%g", ua2.amount);
            r->TextR(str);
        }

        r->NewLine();
        pr = pr->next;
    }
    return 0;
}

int Inventory::ProductListReport(Terminal *t, Invoice *in, Report *r)
{
    FnTrace("Inventory::ProductListReport(Invoice)");
    if (in == NULL || r == NULL)
        return 1;

    Product *pr = ProductList();
    if (pr == NULL)
    {
        r->TextC("There are no products definied");
        return 0;
    }

    genericChar str[256];
    while (pr)
    {
        r->TextL(pr->name.Value());
        r->TextPosL(-32, pr->purchase.Measurement());

        UnitAmount ua;
        InvoiceEntry *ie = in->FindEntry(pr->id);
        if (ie)
            ua = ie->amount;
        else
            ua.Clear();

        ua.Convert(pr->purchase.type);
        sprintf(str, "%g", ua.amount);
        r->TextPosR(-20, str);
        r->TextPosR(-10, t->FormatPrice((int) ((Flt)pr->cost / pr->purchase.amount)));

        int cost = (int) ((ua.amount /= pr->purchase.amount) * (Flt) pr->cost);
        r->TextR(t->FormatPrice(cost));

        r->NewLine();
        pr = pr->next;
    }
    return 0;
}

int Inventory::ScanItems(ItemDB *db)
{
    FnTrace("Inventory::ScanItems()");
    if (db == NULL)
        return 1;

    // Clear 'in_menu' flags
    Recipe *rc = RecipeList();
    while (rc)
    {
        rc->in_menu = 0;
        rc = rc->next;
    }

    // Make sure every menu item has a recipe
    int change = 0;
    SalesItem *si = db->ItemList();
    while (si)
    {
        int found = 0;
        rc = RecipeList();
        while (rc && found == 0)
        {
            if (StringCompare(si->item_name.Value(), rc->name.Value()) == 0)
            {
                rc->in_menu = 1;
                found = 1;
            }
            rc = rc->next;
        }

        if (found == 0 && si->type != ITEM_METHOD)
        {
            change = 1;
            rc = new Recipe;
            rc->name    = si->item_name;
            rc->in_menu = 1;
            Add(rc);
        }
        si = si->next;
    }

    // Remove worthless recipes
    rc = RecipeList();
    while (rc)
    {
        Recipe *rcnext = rc->next;
        if (rc->in_menu == 0 && rc->PartCount() <= 0)
        {
            change = 1;
            Remove(rc);
            delete rc;
        }
        rc = rcnext;
    }

    if (change)
        Save();
    return 0;
}

bool Inventory::ChangeRecipeName(const std::string &old_name, const std::string &new_name)
{
    FnTrace("Inventory::ChangeRecipeName()");
    if (old_name == new_name)
    {
        return true;
    }

    for (Recipe *r = RecipeList(); r != NULL; r = r->next)
    {
        if (StringCompare(r->name.Value(), old_name) == 0)
        {
            r->name.Set(new_name);
        }
    }

    return 0;
}

Stock *Inventory::CurrentStock()
{
    FnTrace("Inventory::CurrentStock()");
    Stock *end = StockListEnd();
    if (end == NULL || end->end_time.IsSet())
    {
        Stock *s = new Stock;
        Add(s);

        genericChar str[256];
        sprintf(str, "%s/stock_%09d", stock_path.Value(), s->id);
        s->file_name.Set(str);
    }
    return end;
}

int Inventory::MakeOrder(Check *c)
{
    FnTrace("Inventory::MakeOrder()");
    Stock *s = CurrentStock();
    if (s == NULL)
        return 1;

    int changed = 0;
    for (SubCheck *sc = c->SubList(); sc != NULL; sc = sc->next)
        for (Order *o = sc->OrderList(); o != NULL; o = o->next)
            if (!(o->status & ORDER_MADE) && (o->status & ORDER_SENT))
            {
                o->status |= ORDER_MADE;
                if (!(o->qualifier & QUALIFIER_NO))
                {
                    Recipe *rc = FindRecipeByName(o->item_name.Value());
                    if (rc)
                        for (RecipePart *rp = rc->PartList(); rp != NULL; rp = rp->next)
                        {
                            StockEntry *se = s->FindStock(rp->part_id, 1);
                            UnitAmount ua = rp->amount;
                            ua *= o->count;
                            se->used += ua;
                            changed = 1;
                        }
                }
            }

    if (changed)
        s->Save();
    return 0;
}

int Inventory::InvoiceReport(Terminal *t, Invoice *in, Report *r)
{
    FnTrace("Inventory::InvoiceReport()");
    if (r == NULL)
        return 1;

    if (in == NULL)
    {
        r->TextC("No Invoice");
        return 0;
    }

    genericChar str[256];
    Vendor *v = FindVendorByID(in->vendor_id);
    if (v)
        r->TextL(v->name.Value());
    else
        r->TextL("No Vendor Set");

    sprintf(str, "Invoice Date %s", t->TimeDate(in->time, TD_DATE));
    r->Mode(PRINT_UNDERLINE);
    r->TextC(str);
    r->Mode(0);

    sprintf(str, "REF: %d", in->id);
    r->TextR(str);
    r->NewLine(2);

    r->Mode(PRINT_UNDERLINE);
    r->TextL("Amt");
    r->TextPosL(5, "Unit");
    r->TextPosL(10, "Item");
    r->TextPosR(-20, "Amount");
    r->TextPosR(-10, "Unit Cost");
    r->TextR("Extension");
    r->Mode(0);
    r->NewLine();
    int total_cost = 0;
    InvoiceEntry *ie = in->EntryList();
    while (ie)
    {
        sprintf(str, "%g", ie->amount.amount);
        r->TextL(str);
        r->TextPosL(5, ie->amount.Measurement());
        Product *pr = FindProductByID(ie->product_id);
        if (pr)
        {
            r->TextPosL(10, pr->name.Value());
            UnitAmount ua;
            ua = ie->amount;
            ua.Convert(pr->purchase.type);
            sprintf(str, "%g", ua.amount);
            r->TextPosR(-20, str);
            r->TextPosR(-10, t->FormatPrice((int) ((Flt)pr->cost / pr->purchase.amount)));
            int cost = (int) ((ua.amount /= pr->purchase.amount) * (Flt) pr->cost);
            total_cost += cost;
            r->TextR(t->FormatPrice(cost));
        }
        r->NewLine();
        ie = ie->next;
    }
    r->TextR("--------");
    r->NewLine();
    r->TextPosR(-10, "Invoice Total");
    r->TextR(t->FormatPrice(total_cost, 1));
    return 0;
}

/**** InvoiceEntry Class ****/
// Constructor
InvoiceEntry::InvoiceEntry()
{
    next       = NULL;
    fore       = NULL;
    product_id = 0;
}

// Member Functions
int InvoiceEntry::Read(InputDataFile &df, int version)
{
    FnTrace("InvoiceEntry::Read()");
    int error = 0;
    error += df.Read(product_id);
    error += amount.Read(df, 1);
    return error;
}

int InvoiceEntry::Write(OutputDataFile &df, int version)
{
    FnTrace("InvoiceEntry::Write()");
    int error = 0;
    error += df.Write(product_id);
    error += amount.Write(df, 1);
    return 0;
}


/**** Invoice Class ****/
// Constructor
Invoice::Invoice()
{
    next = NULL;
    fore = NULL;
    vendor_id = 0;
    id = 0;
}

// Member Functions
int Invoice::Read(InputDataFile &df, int version)
{
    FnTrace("Invoice::Read()");
    int error = 0;
    if (version >= 2)
    {
        error += df.Read(id);
        error += df.Read(vendor_id);
    }
    error += df.Read(time);

    int n = 0;
    int i;

    error += df.Read(n);
    for (i = 0; i < n; ++i)
    {
        if (df.end_of_file)
            return 1;

        InvoiceEntry *ie = new InvoiceEntry;
        error += ie->Read(df, version);
        Add(ie);
    }
    return 0;
}

int Invoice::Write(OutputDataFile &df, int version)
{
    FnTrace("Invoice::Write()");
    int error = 0;
    error += df.Write(id);
    error += df.Write(vendor_id);
    error += df.Write(time);

    error += df.Write(EntryCount());
    for (InvoiceEntry *ie = EntryList(); ie != NULL; ie = ie->next)
        error += ie->Write(df, version);
    return error;
}

int Invoice::Add(InvoiceEntry *ie)
{
    FnTrace("Invoice::Add()");
    return entry_list.AddToTail(ie);
}

int Invoice::Remove(InvoiceEntry *ie)
{
    FnTrace("Invoice::Remove()");
    return entry_list.Remove(ie);
}

int Invoice::Purge()
{
    FnTrace("Invoice::Purge()");
    entry_list.Purge();
    return 0;
}

InvoiceEntry *Invoice::FindEntry(int product_id, int create)
{
    FnTrace("Invoice::FindEntry()");
    InvoiceEntry *ie;

    for (ie = EntryList(); ie != NULL; ie = ie->next)
    {
        if (ie->product_id == product_id)
            return ie;
    }

    if (create <= 0)
        return NULL;

    ie = new InvoiceEntry;
    ie->product_id = product_id;
    Add(ie);
    return ie;
}

/**** StockEntry Class ****/
// Constructor
StockEntry::StockEntry()
{
    next = NULL;
    fore = NULL;
    product_id = 0;
}

// Member Functions
int StockEntry::Read(InputDataFile &df, int version)
{
    FnTrace("StockEntry::Read()");
    int error = 0;
    error += df.Read(product_id);
    error += used.Read(df, 1);
    error += final.Read(df, 1);
    return error;
}

int StockEntry::Write(OutputDataFile &df, int version)
{
    FnTrace("StockEntry::Write()");
    int error = 0;
    error += df.Write(product_id);
    error += used.Write(df, 1);
    error += final.Write(df, 1);
    return error;
}

/**** Stock Class ****/
// Constructor
Stock::Stock()
{
    next = NULL;
    fore = NULL;
    id   = 0;
}

// Member Functions
int Stock::Read(InputDataFile &df, int version)
{
    FnTrace("Stock::Read()");
    int error = 0;
    error += df.Read(id);
    error += df.Read(end_time);

    int n = 0;
    int i;

    error += df.Read(n);
    for (i = 0; i < n; ++i)
    {
        if (df.end_of_file)
            return 1;

        StockEntry *se = new StockEntry;
        error += se->Read(df, version);
        Add(se);
    }

    n = 0;
    error += df.Read(n);
    for (i = 0; i < n; ++i)
    {
        if (df.end_of_file)
            return 1;

        Invoice *in = new Invoice;
        error += in->Read(df, version);
        Add(in);
    }
    return error;
}

int Stock::Write(OutputDataFile &df, int version)
{
    FnTrace("Stock::Write()");
    int error = 0;
    error += df.Write(id);
    error += df.Write(end_time);

    error += df.Write(EntryCount());
    for (StockEntry *se = EntryList(); se != NULL; se = se->next)
        error += se->Write(df, version);

    error += df.Write(InvoiceCount());
    for (Invoice *in = InvoiceList(); in != NULL; in = in->next)
        error += in->Write(df, version);
    return error;
}

int Stock::Add(StockEntry *se)
{
    FnTrace("Stock::Add(StockEntry)");
    return entry_list.AddToTail(se);
}

int Stock::Add(Invoice *in)
{
    FnTrace("Stock::Add(Invoice)");
    return invoice_list.AddToTail(in);
}

int Stock::Remove(StockEntry *se)
{
    FnTrace("Stock::Remove(StockEntry)");
    return entry_list.Remove(se);
}

int Stock::Remove(Invoice *in)
{
    FnTrace("Stock::Remove(Invoice)");
    return invoice_list.Remove(in);
}

int Stock::Purge()
{
    FnTrace("Stock::Purge()");
    entry_list.Purge();
    invoice_list.Purge();
    return 0;
}

int Stock::Load(const char* file)
{
    FnTrace("Stock::Load()");
    if (file)
        file_name.Set(file);

    int version = 0;
    InputDataFile df;
    if (df.Open(file_name.Value(), version))
        return 1;
    else
        return Read(df, version);
}

int Stock::Save()
{
    FnTrace("Stock::Save()");
    // Save version 2;
    OutputDataFile df;
    if (df.Open(file_name.Value(), 2))
        return 1;
    else
        return Write(df, 2);
}

StockEntry *Stock::FindStock(int product_id, int create)
{
    FnTrace("Stock::FindStock()");
    StockEntry *se;

    for (se = EntryList(); se != NULL; se = se->next)
    {
        if (se->product_id == product_id)
            return se;
    }

    if (create <= 0)
        return NULL;

    se = new StockEntry;
    se->product_id = product_id;
    Add(se);
    return se;
}

Invoice *Stock::FindInvoiceByRecord(int record)
{
    FnTrace("Stock::FindInvoiceByRecord()");
    return invoice_list.Index(record);
}

int Stock::Total()
{
    FnTrace("Stock::Total()");
    StockEntry *se = EntryList();
    while (se)
    {
        se->received.Clear();
        se = se->next;
    }

    if (fore)
    {
        se = fore->EntryList();
        while (se)
        {
            StockEntry *s2 = FindStock(se->product_id, 1);
            s2->received += se->final;
            se = se->next;
        }
    }

    Invoice *in = InvoiceList();
    while (in)
    {
        InvoiceEntry *ie = in->EntryList();
        while (ie)
        {
            se = FindStock(ie->product_id, 1);
            se->received += ie->amount;
            ie = ie->next;
        }
        in = in->next;
    }
    return 0;
}

Invoice *Stock::NewInvoice(int vendor_id)
{
    FnTrace("Stock::NewInvoice()");
    Invoice *list_end = InvoiceListEnd();
    if (list_end)
    {
        int blank = 1;
        InvoiceEntry *ie = list_end->EntryList();
        while (ie)
        {
            if (ie->amount.amount != 0.0)
                blank = 0;
            ie = ie->next;
        }

        if (blank)
        {
            list_end->vendor_id = vendor_id;
            list_end->time      = SystemTime;
            return list_end;
        }
    }

    Invoice *in = new Invoice;
    in->vendor_id = vendor_id;
    in->time      = SystemTime;
    Add(in);
    return in;
}
