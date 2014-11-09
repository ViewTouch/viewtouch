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
#include <string.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** SalesMix Report ****/
#define MAX_FAMILIES 64

class ItemCount;
class ItemCountList
{
    ItemCount *itemlist;
public:
    ItemCountList();
    int AddCount(Order *item);
    int HaveItems() { return (itemlist != NULL); }
    ItemCount *FirstItem() { return itemlist; }
};

class ItemCount
{
public:
    ItemCount *left;  // left and right are for ItemCountTree
    ItemCount *right;
    ItemCount *next;  // for ItemCountList
    ItemCountList mods;
    Str name;
    int family;
    int cost;
    int count;
    int type;

    ItemCount(Order *o);
    ItemCount();

    int AddCount(Order *o);
    int HaveMods() { return (mods.HaveItems()); }
    ItemCount *ModList() { return mods.FirstItem(); }
};

class ItemCountTree
{
public:
    ItemCount *head;

    // Constructor
    ItemCountTree()  { head = NULL; }
    // Destructor
    ~ItemCountTree() { Purge(); }

    // Functions
    int AddToBranch(ItemCount *branch, ItemCount *ic);
    int KillBranch(ItemCount *ic);
    ItemCount *SearchBranch(ItemCount *ic, const genericChar *name, int cost, int family);
    int CountOrder(Order *o);
    int CountOrderNoFamily(Order *o);
    
    int Add(ItemCount *ic)
        {
            if (ic == NULL)
                return 1;
            else if (head == NULL)
            {
                head = ic;
                return 0;
            }
            else
                return AddToBranch(head, ic);
        }
    
    int Purge() {
        return KillBranch(head); }
    ItemCount *Find(const char *name, int cost, int family) {
        return SearchBranch(head, name, cost, family); }
    ItemCount *Find(const char *name, int cost) {
        return SearchBranch(head, name, cost, FAMILY_UNKNOWN); }
};


/*********************************************************************
 * ItemCountList Class
 ********************************************************************/
ItemCountList::ItemCountList()
{
    FnTrace("ItemCountList::ItemCountList()");
    itemlist = NULL;
}

int ItemCountList::AddCount(Order *item)
{
    FnTrace("ItemCountList::AddCount()");
    ItemCount *newitem = new ItemCount(item);
    ItemCount *curritem;
    ItemCount *previtem;
    int match;
    int item_count;
    int item_cost;

    if (itemlist == NULL)
    {
        itemlist = newitem;
    }
    else
    {
        curritem = itemlist;
        previtem = NULL;
        while (curritem != NULL)
        {
            match = strcmp(newitem->name.Value(), curritem->name.Value());
            if (match == 0)
            {
                // oddly item->count is not the actual count.  It will
                // always be 1.  But item->cost is item->item_cost
                // multiplied by the original count.  So we divide
                // item->cost by item->item_cost to get the correct count.
                item_cost = item->item_cost;
                item_count = item->cost / item->item_cost;
                curritem->count += item_count;
                curritem = NULL;  // break the loop
                delete newitem;  // don't need it
            }
            else if (match < 0)
            {
                newitem = new ItemCount(item);
                if (previtem == NULL)
                {  // add to head
                    newitem->next = curritem;
                    itemlist = newitem;
                }
                else
                {  // add in place
                    newitem->next = curritem;
                    previtem->next = newitem;
                }
                curritem = NULL;  // break the loop
            }
            else if (curritem->next == NULL)
            {  // add to tail
                curritem->next = newitem;
                curritem = NULL;
            }
            else
            {
                previtem = curritem;
                curritem = curritem->next;
            }
        }
    }

    return 0;
}


/*********************************************************************
 * ItemCount Class
 ********************************************************************/
ItemCount::ItemCount(Order *o)
{
    FnTrace("ItemCount::ItemCount(Order)");

    left  = NULL;
    right = NULL;
    next  = NULL;

    if (o)
    {
        int oidx = 0;
        int didx = 0;
        char dest[STRLENGTH];
        char oname[STRLENGTH];

        strcpy(oname, o->item_name.Value());
        while (oname[oidx] == '.' && oname[oidx] != '\0')
            oidx++;
        while (oname[oidx] != '\0')
        {
            dest[didx] = oname[oidx];
            didx++;
            oidx++;
        }
        dest[didx] = '\0';
        name.Set(dest);
        family    = o->item_family;
        cost      = o->item_cost;
        count     = o->count;
        type      = o->item_type;
    }
    else
    {
        name.Set(UnknownStr);
        family    = FAMILY_UNKNOWN;
        cost      = 0;
        count     = 1;
        type      = 0;
    }
}

ItemCount::ItemCount()
{
    FnTrace("ItemCount::ItemCount()");

    left   = NULL;
    right  = NULL;
    next   = NULL;
    family = FAMILY_UNKNOWN;
    cost   = 0;
    count  = 0;
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
    if (branch == NULL || ic == NULL)
        return 1;
    
    // NOTE - the nature of the data order should keep the tree sort of balanced
    int compare = StringCompare(ic->name.Value(), branch->name.Value());
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
    if (ic == NULL)
        return 1;
    if (ic->left)
        KillBranch(ic->left);
    if (ic->right)
        KillBranch(ic->right);
    delete ic;
    return 0;
}

ItemCount *ItemCountTree::SearchBranch(ItemCount *ic, const genericChar *name, int cost,
                                       int family)
{
    FnTrace("ItemCountTree::SearchBranch()");
    if (ic == NULL)
        return NULL;

    int compare = StringCompare(name, ic->name.Value());
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

    if (o == NULL)
        return 1;
    if ((o->qualifier & QUALIFIER_NO) || o->count == 0)
        return 0;
    o->FigureCost();
    
    ItemCount *ic = Find(o->item_name.Value(), o->item_cost, o->item_family);
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
        while (mod != NULL)
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
    Order *mod = NULL;
    ItemCount *ic = NULL;

    if (o == NULL)
        return 1;
    if ((o->qualifier & QUALIFIER_NO) || o->count == 0)
        return 0;
    o->FigureCost();
    
    ic = Find(o->item_name.Value(), o->item_cost);
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
        while (mod != NULL)
        {
            if (mod->cost > 0)
                ic->mods.AddCount(mod);
            mod = mod->next;
        }
    }

    return 0;
}


#define COUNT_POS  -11
#define WEIGHT_POS -17

/*********************************************************************
 * SalesMixReport Function
 ********************************************************************/
int FamilyItemReport(Terminal *t, ItemCount *branch, Report *report_list[],
                     int count_list[], int cost_list[], int weight_list[])
{
    FnTrace("FamilyItemReport()");
    int f = 0;
    int sales = 0;
    int modsales;
    Report *r = NULL;
    genericChar str[STRLENGTH];
    ItemCount *modifier = NULL;

    if (branch == NULL)
        return 1;
    
    if (branch->left)
        FamilyItemReport(t, branch->left, report_list, count_list, cost_list, weight_list);
    
    f = branch->family;
    if (f < MAX_FAMILIES)
    {
        if (report_list[f] == NULL)
        {
            report_list[f] = new Report;
            const genericChar *s = FindStringByValue(branch->family, FamilyValue, FamilyName, UnknownStr);
            sprintf(str, "%s: %s", t->Translate("Family"), t->Translate(s));
            report_list[f]->Mode(PRINT_UNDERLINE | PRINT_BOLD);
            report_list[f]->TextL(str);
            report_list[f]->Mode(0);
        }
        r = report_list[f];
        r->NewLine();
        r->TextPosL(2, branch->name.Value());
        sales = branch->count * branch->cost;
        if (branch->type == ITEM_POUND)
        {
            r->TextPosR(WEIGHT_POS, t->FormatPrice(branch->count));
            weight_list[f] += branch->count;
            sales = sales / 100;
        }
        else
        {
            r->NumberPosR(COUNT_POS, branch->count);
            count_list[f] += branch->count;
        }

        r->TextPosR(0, t->FormatPrice(sales));
        cost_list[f]  += sales;

        if (branch->HaveMods() && t->GetSettings()->show_modifiers)
        {
            modifier = branch->ModList();
            while (modifier != NULL)
            {
                modsales = modifier->cost * modifier->count;
                // display the modifier name and count
                r->NewLine();
                r->TextPosL(5, modifier->name.Value());
                r->NumberPosR(COUNT_POS, modifier->count);
                r->TextPosR(0, t->FormatPrice(modsales));
                count_list[f] += modifier->count;
                cost_list[f] += modsales;
                modifier = modifier->next;
            }
        }
    }
    
    if (branch->right)
        FamilyItemReport(t, branch->right, report_list, count_list, cost_list, weight_list);

    return 0;
}

int NoFamilyItemReport(Terminal *t, ItemCount *branch, Report *r,
                       int &total_count, int &total_cost, int &total_weight)
{
    FnTrace("NoFamilyItemReport()");
    int sales;
    int modsales;
    ItemCount *modifier;

    if (branch == NULL)
        return 1;
    
    if (branch->left)
        NoFamilyItemReport(t, branch->left, r, total_count, total_cost, total_weight);
    
    r->NewLine();
    r->TextPosL(0, branch->name.Value());
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

    if (branch->HaveMods() && t->GetSettings()->show_modifiers)
    {
        modifier = branch->ModList();
        while (modifier != NULL)
        {
            // display the modifier name and count
            modsales = modifier->cost * modifier->count;
            r->NewLine();
            r->TextPosL(5, modifier->name.Value());
            r->NumberPosR(COUNT_POS, modifier->count);
            r->TextPosR(0, t->FormatPrice(modsales));
            total_cost += modsales;
            total_count += modifier->count;
            modifier = modifier->next;
        }
    }
    
    if (branch->right)
        NoFamilyItemReport(t, branch->right, r, total_count, total_cost, total_weight);

    return 0;
}

#define SALESMIX_TITLE "Item Sales By Family"
int System::SalesMixReport(Terminal *t, TimeInfo &start_time, TimeInfo &end,
                           Employee *e, Report *r)
{
    FnTrace("System::SalesMixReport()");
    if (r == NULL)
        return 1;
    
    r->update_flag = UPDATE_SERVER;
    t->SetCursor(CURSOR_WAIT);
    int user_id = 0;
    if (e)
        user_id = e->id;
    
    TimeInfo et;
    if (end.IsSet())
        et = end;
    else
        et = SystemTime;
    
    // Go through archives
    int show_family = t->show_family;
    Settings *s = &settings;
    ItemCountTree tree;
    Archive *a = FindByTime(start_time);
    for (;;)
    {
        for (Check *c = FirstCheck(a); c != NULL; c = c->next)
        {
            if ((c->IsTraining() == 0) && (user_id == 0 || user_id == c->WhoGetsSale(s)))
            {
                for (SubCheck *sc = c->SubList(); sc != NULL; sc = sc->next)
                {
                    if (sc->settle_time.IsSet() &&
                        sc->settle_time < end &&
                        sc->settle_time > start_time)
                    {
                        for (Order *o = sc->OrderList(); o != NULL; o = o->next)
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
        
        if (a == NULL || a->end_time > end)
            break; // kill loop
        a = a->next;
    }
    
    // Make report header
    r->Mode(PRINT_BOLD | PRINT_LARGE);
    r->TextC(t->Translate(SALESMIX_TITLE));
    r->SetTitle(SALESMIX_TITLE);
    r->NewLine();
    r->TextC(t->GetSettings()->store_name.Value());
    r->Mode(0);
    
    if (e)
    {
        r->NewLine();
        r->Mode(PRINT_BOLD);
        r->TextC(e->system_name.Value());
        r->Mode(0);
    }
    r->NewLine();
    
    r->TextPosR(6, "Start:");
    if (start_time.IsSet())
        r->TextPosL(7, t->TimeDate(start_time, TD3));
    else
        r->TextPosL(7, "System Start");
    r->NewLine();
    
    r->TextPosR(6, "End:");
    r->TextPosL(7, t->TimeDate(end, TD3));
    r->NewLine(2);
    
    int total_count = 0;
    int total_cost = 0;
    int total_weight = 0;
    if (show_family == 0)
    {
        NoFamilyItemReport(t, tree.head, r, total_count, total_cost, total_weight);
        r->NewLine();
    }
    else
    {
        // Make report for each family
        genericChar str[STRLENGTH];
        const genericChar *str2;
        int cost[MAX_FAMILIES];
        int count[MAX_FAMILIES];
        int weight[MAX_FAMILIES];
        Report *fr[MAX_FAMILIES];
        int i;
        for (i = 0; i < MAX_FAMILIES; ++i)
        {
            cost[i] = 0;
            count[i] = 0;
            weight[i] = 0;
            fr[i] = NULL;
        }
        FamilyItemReport(t, tree.head, fr, count, cost, weight);
        for (i = 0; i < MAX_FAMILIES; ++i)
        {
            total_count  += count[i];
            total_cost   += cost[i];
            total_weight += weight[i];
        }
        
        for (i = 0; i < MAX_FAMILIES; ++i)
        {
            if (fr[i])
            {
                r->Append(fr[i]);
                r->UnderlinePosR(0, 12);
                r->NewLine();
                r->Mode(PRINT_BOLD | PRINT_BLUE);
                str2 = FindStringByValue(i, FamilyValue, FamilyName, UnknownStr);
                sprintf(str, "%s Total", MasterLocale->Translate(str2));
                r->TextPosL(0, str, COLOR_DK_BLUE);
                if (count[i])
                    r->NumberPosR(COUNT_POS, count[i], COLOR_DK_BLUE);
                else
                    r->TextPosR(WEIGHT_POS, t->FormatPrice(weight[i]), COLOR_DK_BLUE);
                r->TextPosR(0, t->FormatPrice(cost[i], 1), COLOR_DK_BLUE);
                r->NewLine();
                r->Mode(0);
                if (total_cost > 0)
                {
                    sprintf(str, "(%.1f%%)", ((Flt) cost[i] / (Flt) total_cost) * 100.0);
                    r->TextPosR(0, str);
                    r->NewLine();
                }
            }
        }
    }
    
    // Make report footer
    r->NewLine();
    r->Mode(PRINT_BOLD);
    r->TextPosL(0, "Total For Period");
    r->NumberPosR(COUNT_POS, total_count);
    r->TextPosR(WEIGHT_POS, t->FormatPrice(total_weight));
    r->TextPosR(0, t->FormatPrice(total_cost, 1));
    r->Mode(0);
    t->SetCursor(CURSOR_POINTER);
    r->is_complete = 1;
    return 0;
}
