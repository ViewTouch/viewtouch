/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025, 2026
  
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
 * system_salesmix.cc - revision 124 (8/13/98)
 * SalesMixReport Function for system module
 */

#include "system.hh"
#include "check.hh"
#include "report.hh"
#include "labels.hh"
#include "locale.hh"
#include "manager.hh"
#include "archive.hh"
#include "admission.hh"

#include <algorithm>
#include <string.h>
#include <string>
#include <map>
#include "safe_string_utils.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** SalesMix Report ****/
#define MAX_FAMILIES 64

class ItemCount;
class ItemCountList
{
public:
    // ordered list by item name
    std::map<std::string, ItemCount> itemlist;
    int AddCount(Order *item);
    bool empty() { return itemlist.empty(); }
};
class ItemCount
{
public:
    ItemCount *left = nullptr;  // left and right are for ItemCountTree
    ItemCount *right = nullptr;
    ItemCount *next = nullptr;  // for ItemCountList
    ItemCountList mods;
    std::string name;
    uint8_t family = FAMILY_UNKNOWN;
    int cost = 0;
    int count = 0;
    int type = 0;

    ItemCount(Order *o);
    ItemCount();

    int AddCount(Order *o);
};


class ItemCountTree
{
public:
    ItemCount *head = nullptr;

    // Constructor
    ItemCountTree() = default;
    // Destructor
    ~ItemCountTree() { Purge(); }

    // Functions
    int AddToBranch(ItemCount *branch, ItemCount *ic);
    int KillBranch(ItemCount *ic);
    ItemCount *SearchBranch(ItemCount *ic, const std::string &name, int cost, int family);
    int CountOrder(Order *o);
    int CountOrderNoFamily(Order *o);
    
    int Add(ItemCount *ic)
        {
            if (ic == nullptr)
                return 1;
            else if (head == nullptr)
            {
                head = ic;
                return 0;
            }
            else
                return AddToBranch(head, ic);
        }
    
    int Purge() {
        return KillBranch(head); }
    ItemCount *Find(const std::string &name, int cost, int family) {
        return SearchBranch(head, name, cost, family); }
    ItemCount *Find(const std::string &name, int cost) {
        return SearchBranch(head, name, cost, FAMILY_UNKNOWN); }
};


/*********************************************************************
 * ItemCountList Class
 ********************************************************************/
int ItemCountList::AddCount(Order *item)
{
    FnTrace("ItemCountList::AddCount()");

    ItemCount newitem(item);

    if (itemlist.find(newitem.name) == itemlist.end())
    {
        // new item, just insert it
        itemlist.insert(std::make_pair(newitem.name, newitem));
    } else
    {
        // update existing item
        ItemCount &old_item = itemlist.at(newitem.name);
        // oddly item->count is not the actual count.  It will
        // always be 1.  But item->cost is item->item_cost
        // multiplied by the original count.  So we divide
        // item->cost by item->item_cost to get the correct count.
        int item_count = item->cost / item->item_cost;
        old_item.count += item_count;
    }

    return 0;
}


/*********************************************************************
 * ItemCount Class
 ********************************************************************/
ItemCount::ItemCount(Order *o)
{
    FnTrace("ItemCount::ItemCount(Order)");
    if (o)
    {
        // trim leading '.' characters
        std::string oname = o->item_name.str();
        oname.erase(oname.begin(), std::find_if(oname.begin(), oname.end(),
                     [](const char c) {return c != '.';}));

        name      = oname;
        family    = o->item_family;
        cost      = o->item_cost;
        count     = o->count;
        type      = o->item_type;
    }
    else
    {
        name = UnknownStr;
        count = 1;
    }
}

ItemCount::ItemCount()
{
    FnTrace("ItemCount::ItemCount()");
}

int ItemCount::AddCount(Order *o)
{
    FnTrace("ItemCount::AddCount()");

    count += o->count;

    return count;
}


/*********************************************************************
 * ItemCountTree Class
 ********************************************************************/
int ItemCountTree::AddToBranch(ItemCount *branch, ItemCount *ic)
{
    FnTrace("ItemCountTree::AddToBranch()");
    if (branch == nullptr || ic == nullptr)
        return 1;
    
    // NOTE - the nature of the data order should keep the tree sort of balanced
    int compare = StringCompare(ic->name, branch->name);
    if (compare < 0)
        goto add_left;
    else if (compare > 0)
        goto add_right;
    
    if (ic->cost < branch->cost)
        goto add_left;
    else if (ic->cost > branch->cost)
        goto add_right;
    
    if (ic->family < branch->family)
        goto add_left;
    else if (ic->family > branch->family)
        goto add_right;
    
    // node already in tree - can't add
    return 1;
    
add_left:
    if (branch->left)
        return AddToBranch(branch->left, ic);
    branch->left = ic;
    return 0;
    
add_right:
    if (branch->right)
        return AddToBranch(branch->right, ic);
    branch->right = ic;
    return 0;
}

int ItemCountTree::KillBranch(ItemCount *ic)
{
    FnTrace("ItemCountTree::KillBranch()");
    if (ic == nullptr)
        return 1;
    if (ic->left)
        KillBranch(ic->left);
    if (ic->right)
        KillBranch(ic->right);
    delete ic;
    return 0;
}

ItemCount *ItemCountTree::SearchBranch(ItemCount *ic, const std::string &name, int cost,
                                       int family)
{
    FnTrace("ItemCountTree::SearchBranch()");
    if (ic == nullptr)
        return nullptr;

    int compare = StringCompare(name, ic->name);
    if (compare < 0)
        goto search_left;
    else if (compare > 0)
        goto search_right;
    
    if (cost < ic->cost)
        goto search_left;
    else if (cost > ic->cost)
        goto search_right;

    if (family != FAMILY_UNKNOWN)
    {
        if (family < ic->family)
            goto search_left;
        else if (family > ic->family)
            goto search_right;
    }
    
    // item found!
    return ic;
    
search_left:
    return SearchBranch(ic->left, name, cost, family);
search_right:
    return SearchBranch(ic->right, name, cost, family);
}

int ItemCountTree::CountOrder(Order *o)
{
    FnTrace("ItemCountTree::CountOrder()");
    Order *mod;

    if (o == nullptr)
        return 1;
    if ((o->qualifier & QUALIFIER_NO) || o->count == 0)
        return 0;
    o->FigureCost();
    
    ItemCount *ic = Find(o->item_name.str(), o->item_cost, o->item_family);
    if (ic)
        ic->AddCount(o);
    else
    {
        ic = new ItemCount(o);
        Add(ic);
    }

    if (ic)
    {
        mod = o->modifier_list;
        while (mod != nullptr)
        {
            if (mod->total_cost > 0)
                ic->mods.AddCount(mod);
            mod = mod->next;
        }
    }

    return 0;
}

int ItemCountTree::CountOrderNoFamily(Order *o)
{
    FnTrace("ItemCountTree::CountOrderNoFamily()");
    Order *mod = nullptr;
    ItemCount *ic = nullptr;

    if (o == nullptr)
        return 1;
    if ((o->qualifier & QUALIFIER_NO) || o->count == 0)
        return 0;
    o->FigureCost();
    
    ic = Find(o->item_name.str(), o->item_cost);
    if (ic)
        ic->AddCount(o);
    else
    {
        ic = new ItemCount(o);
        Add(ic);
    }

    if (ic)
    {
        mod = o->modifier_list;
        while (mod != nullptr)
        {
            if (mod->cost > 0)
                ic->mods.AddCount(mod);
            mod = mod->next;
        }
    }

    return 0;
}


#define COUNT_POS  (-11)
#define WEIGHT_POS (-17)

/*********************************************************************
 * SalesMixReport Function
 ********************************************************************/
struct FamilyItem
{
    bool initialized = false;
    Report fr;
    int cost = 0;
    int count = 0;
    int weight = 0;
};
int FamilyItemReport(Terminal *t, ItemCount *branch,
                     std::vector<FamilyItem> &family_items)
{
    FnTrace("FamilyItemReport()");
    int sales = 0;
    int modsales;
    std::string str;
    str.resize(STRLENGTH);

    if (branch == nullptr)
        return 1;
    
    if (branch->left)
        FamilyItemReport(t, branch->left, family_items);
    
    uint8_t f = branch->family;
    if (f < family_items.size())
    {
        FamilyItem &fi = family_items.at(f);
        Report &r = fi.fr;
        if (!fi.initialized)
        {
            const genericChar* s = FindStringByValue(branch->family, FamilyValue, FamilyName, UnknownStr);
            str = std::string() + t->Translate("Family") + ": " + t->Translate(s);
            r.NewLine();
            r.Mode(PRINT_BOLD | PRINT_UNDERLINE);
            r.Text(std::string() + str, COLOR_DK_RED, ALIGN_CENTER, 0);
            r.Mode(0);
            r.NewLine();
            fi.initialized = true;
        }
        r.NewLine();
        r.TextPosL(2, admission_filteredname(branch->name));  //here
        sales = branch->count * branch->cost;
        if (branch->type == ITEM_POUND)
        {
            r.TextPosR(WEIGHT_POS, t->FormatPrice(branch->count));
            fi.weight += branch->count;
            sales = sales / 100;
        }
        else
        {
            r.NumberPosR(COUNT_POS, branch->count);
            fi.count += branch->count;
        }

        r.TextPosR(0, t->FormatPrice(sales));
        fi.cost += sales;

        if (!branch->mods.empty() && t->GetSettings()->show_modifiers)
        {
            for (const auto &entry : branch->mods.itemlist)
            {
                const ItemCount &modifier = entry.second;
                modsales = modifier.cost * modifier.count;
                // display the modifier name and count
                r.NewLine();
                r.TextPosL(5, modifier.name);
                r.NumberPosR(COUNT_POS, modifier.count);
                r.TextPosR(0, t->FormatPrice(modsales));
                fi.count += modifier.count;
                fi.cost += modsales;
            }
        }
    }
    
    if (branch->right)
        FamilyItemReport(t, branch->right, family_items);

    return 0;
}

int NoFamilyItemReport(Terminal *t, ItemCount *branch, Report *r,
                       int &total_count, int &total_cost, int &total_weight)
{
    FnTrace("NoFamilyItemReport()");
    int sales;
    int modsales;

    if (branch == nullptr)
        return 1;
    
    if (branch->left)
        NoFamilyItemReport(t, branch->left, r, total_count, total_cost, total_weight);
    
    r->NewLine();
    r->TextPosL(0, admission_filteredname(branch->name)); //here
    sales = branch->count * branch->cost;
    if (branch->type == ITEM_POUND)
    {
        r->TextPosR(WEIGHT_POS, t->FormatPrice(branch->count));
        total_weight += branch->count;
        sales = sales / 100;
    }
    else
    {
        r->NumberPosR(COUNT_POS, branch->count);
        total_count += branch->count;
    }
    r->TextPosR(0, t->FormatPrice(sales));
    total_cost  += sales;

    if (!branch->mods.empty() && t->GetSettings()->show_modifiers)
    {
        for (const auto &entry : branch->mods.itemlist)
        {
            const ItemCount &modifier = entry.second;
            // display the modifier name and count
            modsales = modifier.cost * modifier.count;
            r->NewLine();
            r->TextPosL(5, modifier.name);
            r->NumberPosR(COUNT_POS, modifier.count);
            r->TextPosR(0, t->FormatPrice(modsales));
            total_cost += modsales;
            total_count += modifier.count;
        }
    }
    
    if (branch->right)
        NoFamilyItemReport(t, branch->right, r, total_count, total_cost, total_weight);

    return 0;
}

#define SALESMIX_TITLE "Item Sales By Family"
int System::SalesMixReport(Terminal *t, const TimeInfo &start_time, const TimeInfo &end,
                           Employee *e, Report *r)
{
    FnTrace("System::SalesMixReport()");
    if (r == nullptr)
        return 1;
    
    r->update_flag = UPDATE_SERVER;
    t->SetCursor(CURSOR_WAIT);
    int user_id = 0;
    if (e)
        user_id = e->id;
    
    TimeInfo et = end.IsSet() ? end : SystemTime;
    
    // Go through archives
    int show_family = t->show_family;
    Settings *s = &settings;
    ItemCountTree tree;
    Archive *a = FindByTime(start_time);
    for (;;)
    {
        for (Check *c = FirstCheck(a); c != nullptr; c = c->next)
        {
            if ((c->IsTraining() == 0) && (user_id == 0 || user_id == c->WhoGetsSale(s)))
            {
                for (SubCheck *sc = c->SubList(); sc != nullptr; sc = sc->next)
                {
                    if (sc->settle_time.IsSet() &&
                        sc->settle_time < end &&
                        sc->settle_time > start_time)
                    {
                        for (Order *o = sc->OrderList(); o != nullptr; o = o->next)
                        {
                            if (show_family)
                                tree.CountOrder(o);
                            else
                                tree.CountOrderNoFamily(o);
                        }
                    }
                }
            }
        }
        
        if (a == nullptr || a->end_time > end)
            break; // kill loop
        a = a->next;
    }
    
    // Make report header
    r->SetTitle(SALESMIX_TITLE);
    
    // Store name header
    r->Mode(PRINT_BOLD);
    r->TextC(t->GetSettings()->store_name.Value(), COLOR_DK_BLUE);
    r->Mode(0);
    r->NewLine();
    
    // Employee name if specified
    if (e)
    {
        r->Mode(PRINT_BOLD);
        r->TextC(e->system_name.Value(), COLOR_DK_BLUE);
        r->Mode(0);
        r->NewLine();
    }
    
    // Date range with better formatting (MM/DD/YY - MM/DD/YY)
    genericChar date_str[256], start_date[32], end_date[32];
    if (start_time.IsSet())
        t->TimeDate(start_date, start_time, TD_SHORT_DATE | TD_NO_DAY | TD_NO_TIME);
    else
        vt_safe_string::safe_copy(start_date, 32, GlobalTranslate("System Start"));
    t->TimeDate(end_date, et, TD_SHORT_DATE | TD_NO_DAY | TD_NO_TIME);
    vt_safe_string::safe_format(date_str, 256, "%s - %s", start_date, end_date);
    
    r->Mode(PRINT_BOLD);
    r->TextC(date_str, COLOR_DK_BLUE);
    r->Mode(0);
    r->NewLine();
    r->NewLine();
    
    int total_count = 0;
    int total_cost = 0;
    int total_weight = 0;
    if (show_family == 0)
    {
        r->Mode(PRINT_BOLD);
        r->TextC(GlobalTranslate("ITEM SALES"), COLOR_DK_GREEN);
        r->Mode(0);
        r->NewLine();
        NoFamilyItemReport(t, tree.head, r, total_count, total_cost, total_weight);
        r->NewLine();
    }
    else
    {
        // Make report for each family
        genericChar str[STRLENGTH];
        const genericChar* str2;
        std::vector<FamilyItem> family_items(MAX_FAMILIES);
        FamilyItemReport(t, tree.head, family_items);
        for (const FamilyItem &fi : family_items)
        {
            total_count  += fi.count;
            total_cost   += fi.cost;
            total_weight += fi.weight;
        }
        
        int i = 0;
        for (const FamilyItem &fi : family_items)
        {
            i++;
            if (fi.initialized)
            {
                r->Append(fi.fr);
                r->NewLine();
                r->Mode(PRINT_BOLD | PRINT_UNDERLINE);
                str2 = FindStringByValue(i, FamilyValue, FamilyName, UnknownStr);
                vt_safe_string::safe_format(str, STRLENGTH, "%s Total", MasterLocale->Translate(str2));
                r->TextPosL(0, str, COLOR_DK_BLUE);
                if (fi.count)
                    r->NumberPosR(COUNT_POS, fi.count, COLOR_DK_BLUE);
                else
                    r->TextPosR(WEIGHT_POS, t->FormatPrice(fi.weight), COLOR_DK_BLUE);
                r->TextPosR(0, t->FormatPrice(fi.cost, 1), COLOR_DK_BLUE);
                r->Mode(0);
                r->NewLine();
                if (total_cost > 0)
                {
                    vt_safe_string::safe_format(str, STRLENGTH, "(%.1f%%)", ((Flt) fi.cost / (Flt) total_cost) * 100.0);
                    r->TextPosR(0, str);
                    r->NewLine();
                }
                r->NewLine();
            }
        }
    }
    
    // Make report footer
    r->NewLine();
    r->Mode(PRINT_BOLD);
    r->TextC(GlobalTranslate("TOTAL FOR PERIOD"), COLOR_DK_BLUE);
    r->Mode(0);
    r->NewLine();
    r->Mode(PRINT_BOLD | PRINT_UNDERLINE);
    r->TextPosL(0, GlobalTranslate("Total"));
    r->NumberPosR(COUNT_POS, total_count, COLOR_DK_BLUE);
    r->TextPosR(WEIGHT_POS, t->FormatPrice(total_weight), COLOR_DK_BLUE);
    r->TextPosR(0, t->FormatPrice(total_cost, 1), COLOR_DK_BLUE);
    r->Mode(0);
    t->SetCursor(CURSOR_POINTER);
    r->is_complete = 1;
    return 0;
}
