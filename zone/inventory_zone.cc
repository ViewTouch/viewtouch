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
 * inventory_zone.cc - revision 87 (9/8/98)
 * Implementation of inventory_zone module
 *
 * $Log: inventory_zone.cc,v $
 * Revision 1.15  2006/08/19 15:10:16  walterle
 * bug solved: save changes correct at Item Editing
 *
 * Revision 1.13  2005/09/26 22:17:37  brking
 * Removed term from report->FormalPrint().  It really wasn't necessary.
 *
 * Revision 1.12  2004/11/25 00:54:38  brking
 * Allow the "Increase Item Quantity" button to be disabled on a per-item
 * basis.
 *
 * Revision 1.11  2004/11/19 22:53:06  brking
 * Add employee price to the item list zone.
 *
 * Revision 1.10  2004/06/22 20:37:10  brking
 * When an item's name changed, we were updating all of the terminals, but not
 * the master zone_db, and so if you then restarted ViewTouch without going
 * into edit mode, you'd end up with <unknown> entries.  Now we update the
 * master zone_db, so that shouldn't happen.
 *
 * Revision 1.9  2004/03/19 01:41:47  brking
 * Add dmalloc header file
 *
 * Revision 1.8  2003/02/13 20:35:53  brking
 * Added a phrases_changed variable to the System class to track the last
 * time phrase translations were updated.  This way zones that care about
 * translations can refresh their textual items each time phrases change,
 * but they don't have to refresh the items more frequently than that.
 *
 * Revision 1.7  2002/10/31 23:03:54  brking
 * Added some NULL pointer checks.  I was getting segfaults with empty
 * data when leaving the Invoices page.
 *
 * Revision 1.6  2002/08/21 17:47:09  brking
 * Unshadowed variables.
 *
 * Revision 1.5  2002/08/21 15:34:57  brking
 * Unshadowed variables.
 *
 * Revision 1.4  2002/07/16 22:43:51  brking
 * Fixed a divide by zero error in InvoiceZone::Render().
 * InvoiceZone::Mouse() was processing every mouse movement, not just
 * clicks.  That slowed things down impossibly.  It's fixed.
 *
 * Revision 1.3  2002/05/10 19:44:03  dwilliams
 * Added "genericChar" typedef in basic.hh
 *
 * Revision 1.2  2002/04/26 00:17:15  brking
 * Changed indenting to 4 spaces
 *
 *
 */

#include "inventory_zone.hh"
#include "terminal.hh"
#include "inventory.hh"
#include "sales.hh"
#include "report.hh"
#include "dialog_zone.hh"
#include "settings.hh"
#include "labels.hh"
#include "system.hh"
#include "image_data.hh"
#include "manager.hh"
#include <cstring>
#include "admission.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** ProductZone Class ****/
// Constructor
ProductZone::ProductZone()
{
    list_header = 3;
    form_header = 1;
    AddTextField("Name", 24);
    AddNewLine();

    AddListField("Recipe Unit", RecipeUnitName, RecipeUnitValue, 11);
    AddTextField("Amount", 5);
    AddNewLine();

    AddListField("Invoice Unit", PurchaseUnitName, PurchaseUnitValue, 11);
    AddTextField("Amount", 5);
    AddTextField("Cost", 7);
    AddNewLine();
    AddTextField("Servings in Invoice Unit", 8, 0);
    AddTextField("Serving Cost", 8, 0);
}

// Member Functions
RenderResult ProductZone::Render(Terminal *t, int update_flag)
{
    FnTrace("ProductZone::Render()");
    if (update_flag == RENDER_NEW)
        record_no = 0;

    if (t->stock == NULL)
        t->stock = t->system_data->inventory.CurrentStock();

    ListFormZone::Render(t, update_flag);
    int final = 0;
    if (t->stock)
        final = t->stock->end_time.IsSet();

    int col = color[0];
    genericChar str[256];
    if (show_list)
    {
        genericChar tm1[32], tm2[32];
        if (t->stock && t->stock->fore)
            t->TimeDate(tm1, t->stock->fore->end_time, TD4);
        else
            strcpy(tm1, "System Start");
        if (t->stock && t->stock->end_time.IsSet())
            t->TimeDate(tm2, t->stock->end_time, TD4);
        else
            strcpy(tm2, "Now");

        if (t->stock && final)
            sprintf(str, "Actual Count #%d (%s - %s)", t->stock->id, tm1, tm2);
        else
            sprintf(str, "Current Inventory (%s - %s)", tm1, tm2);
        TextC(t, 0, str, col);

        TextL(t, 2.4, "Product Name", col);
        if (final)
        {
            TextPosR(t, size_x - 22, 1.4, "Actual", COLOR_RED);
            TextPosR(t, size_x - 22, 2.4, "Count", COLOR_RED);
            TextPosR(t, size_x - 11, 1.4, "Estimated", col);
            TextPosR(t, size_x - 11, 2.4, "Level", col);
            TextPosR(t, size_x,      2.4, "Variance", col);
        }
        else
        {
            TextPosR(t, size_x - 22, 1.4, "Carried Over", col);
            TextPosR(t, size_x - 22, 2.4, "+ Newly Received", col);
            TextPosR(t, size_x - 11, 1.4, "Sold Since", col);
            TextPosR(t, size_x - 11, 2.4, "Last Count", col);
            TextPosR(t, size_x,      1.4, "Estimated", col);
            TextPosR(t, size_x,      2.4, "Level Now", col);
        }
    }
    else
    {
        if (records == 1)
            strcpy(str, "Invoice Product");
        else
            sprintf(str, "Invoice Product %d of %d", record_no + 1, records);
        TextC(t, 0, str, col);
    }
    return RENDER_OKAY;
}

SignalResult ProductZone::Signal(Terminal *term, const genericChar* message)
{
	FnTrace("ProductZone::Signal()");
	static const genericChar* commands[] = {
		"count", "increase", "decrease", "cancel", "save",
		"input", "next stock", "prior stock", "check", "print", NULL};

    int idx = -1;
    if (StringCompare(message, "amount ") == 0)
        idx = 99;
    else
        idx = CompareList(message, commands);
    if (idx < 0)
        return ListFormZone::Signal(term, message);

    if (term->stock == NULL)
        return SIGNAL_IGNORED;

    System *sys = term->system_data;
    Product *pr = sys->inventory.FindProductByRecord(record_no);
    if (pr == NULL)
        return SIGNAL_IGNORED;

    int final = term->stock->end_time.IsSet();
    StockEntry *stock_entry = NULL;

    if (final)
        stock_entry = term->stock->FindStock(pr->id, 1);

    switch (idx)
    {
    case 0:  // count
        term->stock->Save();
        if (final)
            term->stock = sys->inventory.CurrentStock();
        else
            term->stock->end_time = SystemTime;
        break;

    case 1:  // increase
        if (stock_entry)
            stock_entry->final += pr->purchase;
        break;

    case 2:  // decrease
        if (stock_entry)
        {
            stock_entry->final -= pr->purchase;
            if (stock_entry->final.amount < 0)
                stock_entry->final.amount = 0;
        }
        break;

    case 3:  // cancel
        term->stock->end_time.Clear();
        break;

    case 4:  // save
        term->stock->Save();
        break;

    case 5:  // input
        if (stock_entry)
        {
            if (stock_entry->final.type == UNIT_NONE)
                stock_entry->final.type = pr->purchase.type;
            UnitAmountDialog *d = new UnitAmountDialog("Enter Amount", stock_entry->final);
            d->target_zone = this;
            term->OpenDialog(d);
            return SIGNAL_OKAY;
        }
        return SIGNAL_IGNORED;

    case 6:  // next stock
        if (term->stock == NULL || term->stock->next == NULL)
            return SIGNAL_IGNORED;
        term->stock = term->stock->next;
        break;

    case 7:  // prior stock
        if (term->stock == NULL || term->stock->fore == NULL)
            return SIGNAL_IGNORED;
        term->stock = term->stock->fore;
        break;

    case 8:  // check
        if (term->stock == NULL)
            return SIGNAL_IGNORED;
        if (term->stock->next)
            term->stock = sys->inventory.CurrentStock();
        else
            term->stock = term->stock->fore;
        break;

    case 9:  // print
        if (show_list)
        {
            Printer *p = term->FindPrinter(PRINTER_REPORT);
            list_report.CreateHeader(term, p, term->user);
            list_report.FormalPrint(p);
            return SIGNAL_OKAY;
        }
        return SIGNAL_IGNORED;

    case 99: // amount
    {
        int ut;
        Flt amt = 0;
        sscanf(&message[6], "%d %lf", &ut, &amt);
        if (stock_entry)
        {
            stock_entry->final.type   = ut;
            stock_entry->final.amount = amt;
        }
        break;
    }

    default:
        break;
    } //end switch
    Draw(term, 1);

    return SIGNAL_OKAY;
}

SignalResult ProductZone::Mouse(Terminal *term, int action, int mx, int my)
{
    FnTrace("ProductZone::Mouse()");
    if ((action & MOUSE_PRESS) == 0)
        return SIGNAL_IGNORED;

    SignalResult sig = ListFormZone::Mouse(term, action, mx, my);
    if (show_list == 0)
        return sig;

    if (selected_x > (size_x - 8))
    {
        if (action & MOUSE_LEFT)
            return Signal(term, "receive");
        else if (action & MOUSE_RIGHT)
            return Signal(term, "remove");
    }
    return sig;
}

int ProductZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("ProductZone::LoadRecord()");
    Product *prod = term->system_data->inventory.FindProductByRecord(record);

    if (prod == NULL)
    {
        printf("Can'term Load Record %d\n", record);
        return 1;
    }

    UnitAmount unit_amount = prod->purchase;
    unit_amount.Convert(prod->serving.type);
    Flt no = 0.0;
    if (prod->serving.amount > 0.0)
        no = unit_amount.amount / prod->serving.amount;

    FormField *currForm = FieldList();
    currForm->Set(prod->name); currForm = currForm->next;
    currForm->Set(prod->serving.type); currForm = currForm->next;
    if (prod->serving.amount == 0)
        currForm->Set("");
    else
        currForm->Set(prod->serving.amount);
    currForm = currForm->next;

    currForm->Set(prod->purchase.type); currForm = currForm->next;
    if (prod->purchase.amount == 0)
        currForm->Set("");
    else
        currForm->Set(prod->purchase.amount);
    currForm = currForm->next;

    if (prod->cost == 0)
        currForm->Set("");
    else
        currForm->Set(term->SimpleFormatPrice(prod->cost));
    currForm = currForm->next;

    if (no <= 0.0)
        currForm->Set("--");
    else
        currForm->Set(no);
    currForm = currForm->next;

    if (no <= 0)
        currForm->Set("--");
    else
        currForm->Set(term->SimpleFormatPrice((int) (prod->cost / no)));
    return 0;
}

int ProductZone::SaveRecord(Terminal *t, int record, int write_file)
{
    FnTrace("ProductZone::SaveRecord()");
    System *sys = t->system_data;
    Product *pr = sys->inventory.FindProductByRecord(record);
    if (pr)
    {
        Str pr_name;
        FormField *f = FieldList();
        f->Get(pr_name); f = f->next;
        f->Get(pr->serving.type); f = f->next;
        f->Get(pr->serving.amount); f = f->next;
        f->Get(pr->purchase.type); f = f->next;
        f->Get(pr->purchase.amount); f = f->next;
        f->GetPrice(pr->cost); f = f->next;

        if (pr->name != pr_name)
        {
            pr->name = pr_name;
            sys->inventory.Remove(pr);
            sys->inventory.Add(pr);
            record_no = 0;
            while (pr->fore)
            {
                pr = pr->fore;
                ++record_no;
            }
        }
    }
    if (write_file)
        sys->inventory.Save();
    return 0;
}

int ProductZone::UpdateForm(Terminal *t, int record)
{
    FnTrace("ProductZone::UpdateForm()");
    int cost;
    UnitAmount pur, ser;
    FormField *f = FieldList();
    f = f->next;
    f->Get(pur.type); f = f->next;
    f->Get(pur.amount); f = f->next;
    f->GetPrice(cost); f = f->next;
    f->Get(ser.type); f = f->next;
    f->Get(ser.amount); f = f->next;

    pur.Convert(ser.type);
    Flt no = 0.0;
    if (ser.amount > 0.0)
        no = pur.amount / ser.amount;
    if (no <= 0.0)
        f->Set("--");
    else
        f->Set(no);
    f = f->next;
    if (no <= 0.0)
        f->Set("--");
    else
        f->Set(t->SimpleFormatPrice((int) (cost / no)));
    return 0;
}

int ProductZone::NewRecord(Terminal *t)
{
    FnTrace("ProductZone::NewRecord()");
    Product *pr = new Product;
    if (t->system_data->inventory.Add(pr))
    {
        if (pr)
            delete pr;
        return 1;
    }
    record_no = 0;
    while (pr->fore)
    {
        ++record_no;
        pr = pr->fore;
    }
    return 0;
}

int ProductZone::KillRecord(Terminal *t, int record)
{
    FnTrace("ProductZone::KillRecord()");
    System *sys = t->system_data;
    Product *pr = sys->inventory.FindProductByRecord(record);
    if (pr == NULL)
        return 1;
    sys->inventory.Remove(pr);
    delete pr;
    sys->inventory.Save();
    return 0;
}

int ProductZone::ListReport(Terminal *t, Report *r)
{
    FnTrace("ProductZone::ListReport()");
    return t->system_data->inventory.ProductListReport(t, t->stock, r);
}

int ProductZone::RecordCount(Terminal *t)
{
    FnTrace("ProductZone::RecordCount()");
    return t->system_data->inventory.ProductCount();
}

int ProductZone::Search(Terminal *t, int record, const genericChar* word)
{
    FnTrace("ProductZone::Search()");
    int r = 0;
    Product *pr = t->system_data->inventory.FindProductByWord(word, r);
    if (pr == NULL)
        return 0;  // no matches
    record_no = r;
    return 1;  // one match (only one for now)
}


/**** RC_Part class****/
// Constructor
RC_Part::RC_Part()
{
    next = NULL;
    rc   = NULL;
    pr   = NULL;
    rp   = NULL;
    page = 0;
    lit  = 0;
}

// Destructor
RC_Part::~RC_Part()
{
}

// Member Functions
int RC_Part::Render(Terminal *t)
{
    FnTrace("RC_Part::Render()");
    if (lit)
        t->RenderButton(x, y, w, h, ZF_RAISED, IMAGE_LIT_SAND);
    else
        t->RenderButton(x, y, w, h, ZF_RAISED, IMAGE_SAND);

    int font, yy;
    if (h > 35)
    {
        font = FONT_TIMES_24;
        yy   = y + ((h - 25) / 2);
    }
    else
    {
        font = FONT_TIMES_20;
        yy   = y + ((h - 20) / 2);
    }

    int color = COLOR_BLACK;
    if (rc)
        color = COLOR_BLUE;

    const genericChar* n = Name(t);
    if (rp)
    {
        genericChar str[256];
        sprintf(str, "%s %s", rp->amount.Description(), n);
        t->RenderText(str, x + 6, yy, color, font, ALIGN_LEFT, w - 10);
    }
    else
        t->RenderText(n, x + (w/2), yy, color, font, ALIGN_CENTER, w - 6);
    return 0;
}

int RC_Part::Draw(Terminal *t)
{
    FnTrace("RC_Part::Draw()");
    Render(t);
    t->UpdateArea(x, y, w, h);
    return 0;
}

const char* RC_Part::Name(Terminal *t)
{
    FnTrace("RC_Part::Name()");
    static genericChar str[256];
    if (rc)
        strcpy(str, rc->name.Value());
    else if (pr)
        strcpy(str, pr->name.Value());
    else if (rp)
        sprintf(str, "%s (%d)", t->Translate(UnknownStr), rp->part_id);
    else
        strcpy(str, t->Translate(UnknownStr));
    return str;
}

int RC_Part::PartID()
{
    if (rc)
        return rc->id;
    else if (pr)
        return pr->id;
    else
        return 0;
}

int RC_Part::AddIngredient(Recipe *r)
{
    FnTrace("RC_Part::AddIngredient()");
    if (rc)
        return r->AddIngredient(rc->id, rc->serving);
    else if (pr)
        return r->AddIngredient(pr->id, pr->serving);
    else
        return 1;
}

int RC_Part::RemoveIngredient(Recipe *r)
{
    FnTrace("RC_Part::RemoveIngredient()");
    if (rc)
        return r->RemoveIngredient(rc->id, rc->serving);
    else if (pr)
        return r->RemoveIngredient(pr->id, pr->serving);
    else if (rp)
    {
        r->Remove(rp);
        delete rp;
        rp = NULL;
        return 0;
    }
    else
        return 1;
}

/**** RecipeZone Class ****/
// Constructor
RecipeZone::RecipeZone()
{
    part_page   = 0;
    max_pages   = 0;
    list_header = 2;

    AddTextField("Name", 32);
    AddTextField("Minutes to Prepare", 5);
    AddNewLine();
    AddListField("Portion Unit", PurchaseUnitName, PurchaseUnitValue, 11);
    AddTextField("Amount", 5);
    AddNewLine();
    AddListField("Recipe Unit", PurchaseUnitName, PurchaseUnitValue, 11);
    AddTextField("Amount", 5);
}

// Member Functions
RenderResult RecipeZone::Render(Terminal *t, int update_flag)
{
    FnTrace("RecipeZone::Render()");
    System *sys = t->system_data;
    if (update_flag)
    {
        if (update_flag == RENDER_NEW)
        {
            part_page = 0;
            record_no = 0;
            show_list = 1;
        }

        part_list.Purge();
        Product *pr = sys->inventory.ProductList();
        while (pr)
        {
            RC_Part *p = new RC_Part;
            p->pr = pr;
            part_list.AddToTail(p);
            pr = pr->next;
        }

        Recipe *rc = sys->inventory.RecipeList();
        while (rc)
        {
            if (rc->in_menu == 0)
            {
                RC_Part *p = new RC_Part;
                p->rc = rc;
                part_list.AddToTail(p);
            }
            rc = rc->next;
        }
        MakeRecipe(t, sys->inventory.FindRecipeByRecord(record_no));
    }

    if (show_list)
        left_margin = 0;
    else
        left_margin = 216;
    ListFormZone::Render(t, update_flag);

    if (show_list)
    {
        TextL(t, 1.4, "Recipe Name", color[0]);
    }
    else
    {
        RC_Part *p;
        LayoutParts();
        for (p = part_list.Head(); p != NULL; p = p->next)
            if (p->page == part_page)
                p->Render(t);

        LayoutRecipe();
        for (p = recipe_list.Head(); p != NULL; p = p->next)
            p->Render(t);
    }

    genericChar str[64];
    if (records <= 0)
        strcpy(str, "No Recipes Defined");
    else if (records == 1)
        strcpy(str, "Recipe");
    else
        sprintf(str, "Recipe %d of %d", record_no + 1, records);

    TextC(t, 0, str, color[0]);
    return RENDER_OKAY;
}

SignalResult RecipeZone::Touch(Terminal *t, int tx, int ty)
{
    FnTrace("RecipeZone::Touch()");
    System *sys = t->system_data;
    if (!show_list)
    {
        if (tx < 232)
        {
            int new_page = part_page;
            int b = border + (font_height * 2);
            if (ty < (y + b))
                --new_page;
            else if (ty > (y + h - b))
                ++new_page;
            if (new_page > max_pages)
                new_page = 0;
            else if (new_page < 0)
                new_page = max_pages;
            if (new_page != part_page)
            {
                part_page = new_page;
                Draw(t, 0);
                return SIGNAL_OKAY;
            }
        }
        RC_Part *p = part_list.Head();
        while (p)
        {
            if (p->page == part_page && p->IsPointIn(tx, ty))
            {
                Recipe *rc = sys->inventory.FindRecipeByRecord(record_no);
                if (rc == NULL)
                    return SIGNAL_IGNORED;
                p->lit = 1;
                p->Draw(t);
                p->AddIngredient(rc);
                Draw(t, 1);
                return SIGNAL_OKAY;
            }
            p = p->next;
        }
        p = recipe_list.Head();
        while (p)
        {
            RC_Part *p_next = p->next;
            if (p->IsPointIn(tx, ty))
            {
                Recipe *rc = sys->inventory.FindRecipeByRecord(record_no);
                if (rc == NULL)
                    return SIGNAL_IGNORED;
                p->lit = 1;
                p->Draw(t);
                p->RemoveIngredient(rc);
                Draw(t, 1);
                return SIGNAL_OKAY;
            }
            p = p_next;
        }
    }
    return ListFormZone::Touch(t, tx, ty);
}

int RecipeZone::LoadRecord(Terminal *t, int record)
{
    FnTrace("RecipeZone::LoadRecord()");
    Recipe *rc = t->system_data->inventory.FindRecipeByRecord(record);
    if (rc == NULL)
        return 1;

    FormField *f = FieldList();
    f->Set(rc->name); f->modify = !rc->in_menu; f = f->next;
    f->Set(rc->prepare_time); f = f->next;
    f->Set(rc->production.type); f = f->next;
    f->Set(rc->production.amount); f = f->next;
    f->Set(rc->serving.type); f = f->next;
    f->Set(rc->serving.amount); f = f->next;
    return 0;
}

int RecipeZone::SaveRecord(Terminal *t, int record, int write_file)
{
    FnTrace("RecipeZone::SaveRecord()");
    System *sys = t->system_data;
    Recipe *rc = sys->inventory.FindRecipeByRecord(record);
    if (rc)
    {
        Str rc_name;
        FormField *f = FieldList();
        f->Get(rc_name); f = f->next;
        f->Get(rc->prepare_time); f = f->next;
        if (rc->name != rc_name)
        {
            rc->name = rc_name;
            sys->inventory.Remove(rc);
            sys->inventory.Add(rc);
            record_no = 0;
            while (rc->fore)
            {
                rc = rc->fore;
                ++record_no;
            }
        }
    }
    if (write_file)
        sys->inventory.Save();
    return 0;
}

int RecipeZone::NewRecord(Terminal *t)
{
    FnTrace("RecipeZone::NewRecord()");
    Recipe *rc = new Recipe;
    if (rc == NULL)
        return 1;
    if (t->system_data->inventory.Add(rc))
    {
        delete rc;
        return 1;
    }
    record_no = 0;
    while (rc->fore)
    {
        ++record_no;
        rc = rc->fore;
    }
    return 0;
}

int RecipeZone::KillRecord(Terminal *t, int record)
{
    FnTrace("RecipeZone::KillRecord()");
    System *sys = t->system_data;
    Recipe *rc = sys->inventory.FindRecipeByRecord(record);
    if (rc == NULL)
        return 1;
    sys->inventory.Remove(rc);
    delete rc;
    sys->inventory.Save();
    return 0;
}

int RecipeZone::Search(Terminal *t, int record, const genericChar* word)
{
    FnTrace("RecipeZone::Search()");
    int r = 0;
    Recipe *rc = t->system_data->inventory.FindRecipeByWord(word, r);
    if (rc == NULL)
        return 0;  // no matches
    record_no = r;
    return 1;  // one match (only one for now)
}

int RecipeZone::ListReport(Terminal *t, Report *r)
{
    FnTrace("RecipeZone::ListReport()");
    if (r == NULL)
        return 1;

    r->update_flag = UPDATE_MENU;
    Recipe *rc = t->system_data->inventory.RecipeList();
    if (rc == NULL)
    {
        r->TextC(t->Translate("There are no recipes defined"));
        return 0;
    }
    while (rc)
    {
        int c = COLOR_DEFAULT;
        if (!rc->in_menu)
            c = COLOR_BLUE;
        r->TextL(rc->name.Value(), c);
        r->NewLine();
        rc = rc->next;
    }
    return 0;
}

int RecipeZone::RecordCount(Terminal *t)
{
    FnTrace("RecipeZone::RecordCount()");
    return t->system_data->inventory.RecipeCount();
}

int RecipeZone::LayoutParts()
{
    FnTrace("RecipeZone::LayoutParts()");
    int b = (font_height * 2) + border;
    int my_page = 0;
    int xx = x + border;
    int yy = y + b;
    for (RC_Part *rpart = part_list.Head(); rpart != NULL; rpart = rpart->next)
    {
        rpart->lit  = 0;
        rpart->x    = xx;
        rpart->y    = yy;
        rpart->h    = 32;
        rpart->w    = 200;
        rpart->page = my_page;
        max_pages = my_page;

        if ((rpart->y + rpart->h) > (y + h - b - 32))
        {
            ++my_page;
            yy = y + b;
        }
        else
            yy += rpart->h;
    }
    return 0;
}

int RecipeZone::MakeRecipe(Terminal *t, Recipe *rc)
{
    FnTrace("RecipeZone::MakeRecipe()");
    if (rc == NULL)
        return 1;

    System *sys = t->system_data;

    recipe_list.Purge();
    RecipePart *rp = rc->PartList();
    while (rp)
    {
        RC_Part *i = new RC_Part;
        i->rp = rp;
        Product *p = sys->inventory.FindProductByID(rp->part_id);
        if (p)
            i->pr = p;
        else
        {
            Recipe *r = sys->inventory.FindRecipeByID(rp->part_id);
            if (r)
                i->rc = r;
        }
        recipe_list.AddToTail(i);
        rp = rp->next;
    }
    return 0;
}

int RecipeZone::LayoutRecipe()
{
    FnTrace("RecipeZone::LayoutRecipe()");
    int topx = x + border + 232;
    int topy = y + border +
        (int) (FieldListEnd()->y + FieldListEnd()->h) * font_height;
    int ww   = 300;
    int hh   = 36;
    int xx = topx, yy = topy;

    for (RC_Part *p = recipe_list.Head(); p != NULL; p = p->next)
    {
        p->lit = 0;
        p->x = xx;
        p->y = yy;
        p->w = ww;
        p->h = hh;
        if ((p->y + p->h) > (y + h - border - 32))
        {
            yy  = topy;
            xx += ww;
        }
        else
            yy += hh;
    }
    return 0;
}


/**** VendorZone Class ****/
// Constructor
VendorZone::VendorZone()
{
    list_header = 2;
    AddTextField("Name", 24);
    AddTextField("Address", 50);
    AddTextField("Contact", 24);
    AddTemplateField("Phone", "(___) ___-____");
    AddTemplateField("Fax", "(___) ___-____");
}

// Member Functions
RenderResult VendorZone::Render(Terminal *t, int update_flag)
{
    FnTrace("VendorZone::Render()");
    if (update_flag == RENDER_NEW)
        record_no = 0;

    ListFormZone::Render(t, update_flag);

    int c = color[0];
    genericChar str[64];
    if (records <= 0)
        strcpy(str, "No Vendors Defined");
    else if (records == 1)
        strcpy(str, "Vendor");
    else
        sprintf(str, "Vendor %d of %d", record_no + 1, records);
    TextC(t, 0, str, c);

    if (show_list)
    {
        TextL(t, 1.4, "Vendor Name", c);
        TextR(t, 1.4, "Phone Number", c);
    }
    return RENDER_OKAY;
}

int VendorZone::LoadRecord(Terminal *t, int record)
{
    FnTrace("VendorZone::LoadRecord()");
    Vendor *v = t->system_data->inventory.FindVendorByRecord(record);
    if (v == NULL)
        return 1;

    FormField *f = FieldList();
    f->Set(v->name); f = f->next;
    f->Set(v->address); f = f->next;
    f->Set(v->contact); f = f->next;
    f->Set(v->phone); f = f->next;
    f->Set(v->fax); f = f->next;
    return 0;
}

int VendorZone::SaveRecord(Terminal *t, int record, int write_file)
{
    FnTrace("VendorZone::SaveRecord()");
    System *sys = t->system_data;
    Vendor *v = sys->inventory.FindVendorByRecord(record);
    if (v)
    {
        Str v_name;
        FormField *f = FieldList();
        f->Get(v_name); f = f->next;
        f->Get(v->address); f = f->next;
        f->Get(v->contact); f = f->next;
        f->Get(v->phone); f = f->next;
        f->Get(v->fax); f = f->next;
        if (v->name != v_name)
        {
            v->name = v_name;
            sys->inventory.Remove(v);
            sys->inventory.Add(v);
            record_no = 0;
            while (v->fore)
            {
                v = v->fore;
                ++record_no;
            }
        }
    }
    if (write_file)
        sys->inventory.Save();
    return 0;
}

int VendorZone::NewRecord(Terminal *t)
{
    FnTrace("VendorZone::NewRecord()");
    Vendor *v = new Vendor;
    if (t->system_data->inventory.Add(v))
    {
        if (v)
            delete v;
        return 1;
    }
    record_no = 0;
    while (v->fore)
    {
        ++record_no;
        v = v->fore;
    }
    return 0;
}

int VendorZone::KillRecord(Terminal *t, int record)
{
    FnTrace("VendorZone::KillRecord()");
    System *sys = t->system_data;
    Vendor *v = sys->inventory.FindVendorByRecord(record);
    if (v == NULL)
        return 1;
    sys->inventory.Remove(v);
    delete v;
    sys->inventory.Save();
    return 0;
}

int VendorZone::Search(Terminal *t, int record, const genericChar* word)
{
    FnTrace("VendorZone::Search()");
    int r = 0;
    Vendor *v = t->system_data->inventory.FindVendorByWord(word, r);
    if (v == NULL)
        return 0;  // no matches
    record_no = r;
    return 1;  // one match (only one for now)
}

int VendorZone::ListReport(Terminal *t, Report *r)
{
    FnTrace("VendorZone::ListReport()");
    if (r == NULL)
        return 1;

    Vendor *v = t->system_data->inventory.VendorList();
    if (v == NULL)
    {
        r->TextC(t->Translate("There are no vendors defined"));
        return 0;
    }
    while (v)
    {
        r->TextL(v->name.Value());
        r->TextR(FormatPhoneNumber(v->phone));
        r->NewLine();
        v = v->next;
    }
    return 0;
}

int VendorZone::RecordCount(Terminal *t)
{
    FnTrace("VendorZone::RecordCount()");
    return t->system_data->inventory.VendorCount();
}


/**** ItemListZone Class ****/
// Constructor
ItemListZone::ItemListZone()
{
    phrases_changed = 0;
    name_change = 0;
    list_header = 2;
    AddFields();
}

// Member Functions
int ItemListZone::AddFields()  
{
    FnTrace("ItemListZone::AddFields()");

    AddTextField("Item Name", 32);
    AddTextField("Button Name (if different)", 36);
    AddTextField("Printed Name (if different)", 36);
    AddTextField("Call Center Name", 10);
    AddTextField("Item Code", 10);
    AddListField("Item Type", ItemTypeName, ItemTypeValue);
    AddNewLine(1);
    AddTextField("Price", 10);
    AddTextField("Substitute Price", 10);
    AddTextField("Employee Price", 10);
    AddTextField("Takeout Price", 10);
    AddTextField("Delivery Price", 10);
    AddListField("Price Type", PriceTypeName, PriceTypeValue);
    AddListField("Family", FamilyName, FamilyValue);
    AddListField("Tax/Discount Class", SalesTypeName, SalesTypeValue);
    AddListField("Printer Target", PrinterIDName, PrinterIDValue);
    AddListField("Call Order", CallOrderName, CallOrderValue);
    AddListField("Is Item Stocked?", NoYesName);
    AddListField("Allow Increase Button?", NoYesName);
    AddListField("Ignore Split Kitchen?", NoYesName);

    return 0;
}

RenderResult ItemListZone::Render(Terminal *t, int update_flag)
{
    FnTrace("ItemListZone::Render()");

    if (phrases_changed < t->system_data->phrases_changed)
    {
        Purge();
        AddFields();
        phrases_changed = t->system_data->phrases_changed;
    }

    if (update_flag == RENDER_NEW)
        record_no = 0;

    ListFormZone::Render(t, update_flag);

    int c = color[0];
    genericChar str[64];
    if (records <= 0)
        strcpy(str, "No Menu Items Defined");
    else if (records == 1)
        strcpy(str, "Menu Item");
    else
        sprintf(str, "Menu Item %d of %d", record_no + 1, records);
    TextC(t, 0, str, c);

    if (show_list)
    {
        TextL(t, 1.4, "Item Name", c);
        TextR(t, 1.4, "Sale Price", c);
    }
    return RENDER_OKAY;
}

int ItemListZone::LoadRecord(Terminal *t, int record)
{
    FnTrace("ItemListZone::LoadRecord()");
    SalesItem *si = t->system_data->menu.FindByRecord(record);
    if (si == NULL)
        return 1;

    FormField *f = FieldList();
    f->Set(si->item_name); f = f->next;
    f->Set(si->zone_name); f = f->next;
    f->Set(si->print_name); f = f->next;
    f->Set(si->call_center_name); f = f->next;
    f->Set(si->item_code); f = f->next;
    f->Set(si->type); f = f->next;
/*    f->Set(si->location); f = f->next; //the layout here is determined by AddFields
    f->Set(si->event_time); f = f->next;
    f->Set(si->total_tickets); f=f->next;
    f->Set(si->available_tickets); f = f->next;
    f->Set(si->price_label); f=f->next;*/
    f->Set(t->SimpleFormatPrice(si->cost)); f = f->next;
    f->Set(t->SimpleFormatPrice(si->sub_cost)); f = f->next;
    f->Set(t->SimpleFormatPrice(si->employee_cost)); f = f->next;
    f->Set(t->SimpleFormatPrice(si->takeout_cost)); f = f->next;
    f->Set(t->SimpleFormatPrice(si->delivery_cost)); f = f->next;
    f->Set(si->price_type); f = f->next;
    f->Set(si->family); f = f->next;
    f->Set(si->sales_type); f = f->next;
    f->Set(si->printer_id); f = f->next;
    f->Set(si->call_order); f = f->next;
    f->Set(si->stocked); f = f->next;
    f->Set(si->allow_increase); f = f->next;
    f->Set(si->ignore_split);
    return 0;
}

int ItemListZone::SaveRecord(Terminal *t, int record, int write_file)
{
    FnTrace("ItemListZone::SaveRecord()");
    System *sys = t->system_data;
    SalesItem *si = sys->menu.FindByRecord(record);
    if (si)
    {
        int tmp;
        Str item_name, tmp_name;
        FormField *f = FieldList();
        f->Get(item_name); f = f->next;
        f->Get(si->zone_name); f = f->next;
        f->Get(tmp_name); f = f->next;
        si->print_name.Set(FilterName(tmp_name.Value()));
        f->Get(si->call_center_name); f = f->next;
        f->Get(si->item_code); f = f->next;
        f->Get(tmp); si->type = tmp; f = f->next;
	
	/*f->Get(si->location); f=f->next;
	f->Get(si->event_time); f=f->next;
	f->Get(si->total_tickets); f=f->next;
	f->Get(si->available_tickets); f=f->next;
	f->Get(si->price_label); f=f->next;*/
        f->GetPrice(si->cost); f = f->next;
        f->GetPrice(si->sub_cost); f = f->next;
        f->GetPrice(si->employee_cost); f = f->next;     
        f->GetPrice(si->takeout_cost); f = f->next;
        f->GetPrice(si->delivery_cost); f = f->next;       
        f->Get(tmp); f = f->next; si->price_type = tmp;
        f->Get(tmp); f = f->next; si->family = tmp;
        f->Get(tmp); f = f->next; si->sales_type = tmp;
        f->Get(tmp); f = f->next; si->printer_id = tmp;
        f->Get(tmp); f = f->next; si->call_order = tmp;
        f->Get(tmp); f = f->next; si->stocked = tmp;
        f->Get(tmp); f = f->next; si->allow_increase = tmp;
        f->Get(tmp); f = f->next; si->ignore_split = tmp;
        if (item_name != si->item_name)
        {
            name_change = 1;
            const std::string old_name = si->item_name.Value();
            const std::string new_name = FilterName(item_name.Value());
            sys->inventory.ChangeRecipeName(old_name.c_str(), new_name.c_str());
            Terminal *term = t->parent->TermList();
            while (term)
            {
                term->zone_db->ChangeItemName(old_name.c_str(), new_name.c_str());
                term = term->next;
            }
            t->parent->zone_db->ChangeItemName(old_name.c_str(), new_name.c_str());
            si->item_name.Set(new_name);
            sys->menu.Remove(si);
            sys->menu.Add(si);
            record_no = 0;
            while (si->fore)
            {
                si = si->fore;
                ++record_no;
            }
            t->UpdateOtherTerms(UPDATE_MENU, NULL);
        }
    }

    if (write_file)
    {
        if (name_change)
        {
            name_change = 0;
            t->parent->SaveMenuPages();
            sys->inventory.Save();
        }
        sys->menu.Save();
    }
    return 0;
}

int ItemListZone::Search(Terminal *t, int record, const genericChar* word)
{
    FnTrace("ItemListZone::Search()");
    int r = 0;
    SalesItem *mi = t->system_data->menu.FindByWord(word, r);
    if (mi == NULL)
        return 0;  // no matches
    record_no = r;
    return 1;  // one match (only 1 for now)
}

int ItemListZone::ListReport(Terminal *t, Report *r)
{
    FnTrace("ItemListZone::ListRecord()");
    if (r == NULL)
        return 1;

    r->update_flag = UPDATE_MENU;
    SalesItem *si = t->system_data->menu.ItemList();
    if (si == NULL)
    {
        r->TextC(t->Translate("There are no menu items defined"));
        return 0;
    }

    while (si)
    {
        int my_color = COLOR_DEFAULT;
        if (si->type == ITEM_MODIFIER)
            my_color = COLOR_DK_BLUE;
        else if (si->type == ITEM_METHOD)
            my_color = COLOR_DK_GREEN;
        else if (si->type == ITEM_SUBSTITUTE)
            my_color = COLOR_DK_RED;
	Str iname;
	admission_parse_hash_name(iname,si->item_name);
        r->TextL(iname.Value(), my_color);
        r->TextR(t->FormatPrice(si->cost), my_color);
        r->NewLine();
        si = si->next;
    }
    return 0;
}

int ItemListZone::RecordCount(Terminal *t)
{
    FnTrace("ItemListZone::RecordCount()");
    return t->system_data->menu.ItemCount();
}


/**** InvoiceZone Class ****/
// Constructor
InvoiceZone::InvoiceZone()
{
    invoice_report = NULL;
    invoice_page   = 0;
    edit           = 0;
    list_header = 3;
    form_header = 2;

    AddListField("Vendor", NULL);
    AddTextField("ID", 9);
    AddDateField("Date", 1, 0);
}

// Destructor
InvoiceZone::~InvoiceZone()
{
    if (invoice_report)
        delete invoice_report;
}

// Member Functions
RenderResult InvoiceZone::Render(Terminal *t, int update_flag)
{
    FnTrace("InvoiceZone::Render()");
    if (update_flag)
    {
        if (invoice_report)
        {
            delete invoice_report;
            invoice_report = NULL;
        }
        if (update_flag == RENDER_NEW)
        {
            entry_no  = 0;
            record_no = 0;
            edit  = 0;
        }
    }

    System *sys = t->system_data;
    if (t->stock == NULL)
        t->stock = sys->inventory.CurrentStock();

    FormField *f = FieldList();
    f->active = edit; f = f->next;
    f->active = edit; f = f->next;
    f->active = edit;

    no_line = !show_list;
    ListFormZone::Render(t, update_flag);
    if (t->stock == NULL)
        return RENDER_OKAY;

    Invoice *in = t->stock->FindInvoiceByRecord(record_no);
    int col = color[0];
    if (in == NULL || show_list)
    {
        // normal list mode
        genericChar tm1[32], tm2[32];
        if (t->stock->fore)
            t->TimeDate(tm1, t->stock->fore->end_time, TD4);
        else
            strcpy(tm1, "System Start");
        if (t->stock->end_time.IsSet())
            t->TimeDate(tm2, t->stock->end_time, TD4);
        else
            strcpy(tm2, "Now");

        edit = 0;
        genericChar str[256];
        sprintf(str, "List of Invoices (%s - %s)", tm1, tm2);
        TextC(t, 0, str, col);
        TextL(t, 2.3, "Invoice Date", col);
        TextC(t, 2.3, "Vendor", col);
        TextR(t, 2.3, "Reference", col);
        if (invoice_report)
        {
            delete invoice_report;
            invoice_report = NULL;
        }
    }
    else
    {
        if (edit)
        {
            // edit invoice
            TextC(t, 0, "Create Invoice", col);
            TextL(t, 4, "Product", col);
            TextPosR(t, size_x - 20, 4, "Amount", COLOR_RED);
            TextPosR(t, size_x - 10, 4, "Unit Cost", col);
            TextPosR(t, size_x,      4, "Total Cost", col);
            if (invoice_report == NULL)
            {
                invoice_report = new Report;
                sys->inventory.ProductListReport(t, in, invoice_report);
            }
            int lines = invoice_report->lines_shown;
            if (lines == 0)
                lines = 1;
            invoice_page = entry_no / lines;
            invoice_report->selected_line = entry_no;
        }
        else
        {
            // invoice view
            TextC(t, 0, "View Invoice", col);
            if (invoice_report == NULL)
            {
                invoice_report = new Report;
                sys->inventory.InvoiceReport(t, in, invoice_report);
            }
            invoice_report->selected_line = -1;
        }

        if (invoice_report)
            invoice_report->Render(t, this, 3 + (edit * 2), 1, invoice_page, 0,
                                   list_spacing);
    }
    return RENDER_OKAY;
}

SignalResult InvoiceZone::Signal(Terminal *t, const genericChar* message)
{
    FnTrace("InvoiceZone::Signal()");
    static const genericChar* commands[] = {
        "print", "save", "next", "prior", "input", "cancel", "edit",
            "next stock", "prior stock", NULL};

    int idx = -1;
    if (StringCompare(message, "amount ") == 0)
        idx = 99;
    else
        idx = CompareList(message, commands);
    if (idx < 0)
        return ListFormZone::Signal(t, message);

    System *sys = t->system_data;
    Invoice *in = t->stock ? t->stock->FindInvoiceByRecord(record_no) : NULL;
    switch (idx)
    {
    case 0:  // print
        break;
    case 1:  // save
        if (t && t->stock)
            t->stock->Save();
        break;
    case 2:  // next
        if (edit == 0)
            return ListFormZone::Signal(t, message);
        ++entry_no;
        if (entry_no >= sys->inventory.ProductCount())
            entry_no = 0;
        Draw(t, 0);
        break;
    case 3:  // prior
        if (edit == 0)
            return ListFormZone::Signal(t, message);
        --entry_no;
        if (entry_no < 0)
            entry_no = sys->inventory.ProductCount() - 1;
        Draw(t, 0);
        break;
    case 4:  // input
        if (edit && in)
        {
            Product *pr = sys->inventory.FindProductByRecord(entry_no);
            if (pr)
            {
                InvoiceEntry *ie = in->FindEntry(pr->id, 1);
                if (ie)
                {
                    if (ie->amount.type == UNIT_NONE)
                        ie->amount.type = pr->purchase.type;
                    UnitAmountDialog *d = new UnitAmountDialog("Enter Amount", ie->amount);
                    d->target_zone = this;
                    t->OpenDialog(d);
                }
                return SIGNAL_OKAY;
            }
        }
        return SIGNAL_IGNORED;
    case 5:  // cancel
        if (edit && in && t->stock)
        {
            show_list = 1;
            record_no = 0;
            t->stock->Remove(in);
            delete in;
            Draw(t, 1);
            return SIGNAL_OKAY;
        }
        return SIGNAL_IGNORED;
    case 6:  // edit
        show_list = 0;
        edit = 1;
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 7:  // next stock
        if (t->stock == NULL || t->stock->next == NULL)
            return SIGNAL_IGNORED;
        t->stock = t->stock->next;
        record_no = 0;
        LoadRecord(t, 0);
        show_list = 1;
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 8:  // prior stock
        if (t->stock == NULL || t->stock->fore == NULL)
            return SIGNAL_IGNORED;
        t->stock = t->stock->fore;
        record_no = 0;
        LoadRecord(t, 0);
        show_list = 1;
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 99: // amount
        if (invoice_report && in)
        {
            int ut;
            Flt amt = 0;
            sscanf(&message[6], "%d %lf", &ut, &amt);
            if (edit)
            {
                Product *pr = sys->inventory.FindProductByRecord(entry_no);
                if (pr)
                {
                    InvoiceEntry *ie = in->FindEntry(pr->id, 1);
                    ie->amount.type   = ut;
                    ie->amount.amount = amt;
                    delete invoice_report;
                    invoice_report = NULL;
                    Draw(t, 0);
                    return SIGNAL_OKAY;
                }
            }
        }
        return SIGNAL_IGNORED;
    }
    return SIGNAL_OKAY;
}

SignalResult InvoiceZone::Touch(Terminal *t, int tx, int ty)
{
    FnTrace("InvoiceZone::Touch()");
    if (edit && invoice_report)
    {
        // FIX - support multiple pages
        LayoutZone::Touch(t, tx, ty);
        int line = invoice_report->TouchLine(list_spacing, selected_y);
        if (line >= 0 && line < t->system_data->inventory.ProductCount())
        {
            entry_no = line;
            Draw(t, 0);
            return SIGNAL_OKAY;
        }
    }
    return ListFormZone::Touch(t, tx, ty);
}

SignalResult InvoiceZone::Mouse(Terminal *t, int action, int mx, int my)
{
    FnTrace("InvoiceZone::Mouse()");
    if ((action & MOUSE_PRESS) == 0)
        return SIGNAL_IGNORED;

    if (edit && invoice_report)
    {
        // FIX - support multiple pages
        LayoutZone::Touch(t, mx, my);
        int line = invoice_report->TouchLine(list_spacing, selected_y);
        if (line >= 0 && line < t->system_data->inventory.ProductCount())
        {
            entry_no = line;
            Draw(t, 0);
            return SIGNAL_OKAY;
        }
    }
    return ListFormZone::Mouse(t, action, mx, my);
}

int InvoiceZone::LoadRecord(Terminal *t, int record)
{
    FnTrace("InvoiceZone::LoadRecord()");
    if (invoice_report)
    {
        delete invoice_report;
        invoice_report = NULL;
        entry_no = 0;
    }

    if (t->stock == NULL)
        return 1;

    Invoice *in = t->stock->FindInvoiceByRecord(record);
    if (in == NULL)
        return 1;

    FormField *f = FieldList();
    f->ClearEntries();
    f->AddEntry("None", 0);
    Vendor *v = t->system_data->inventory.VendorList();
    while (v)
    {
        f->AddEntry(v->name.Value(), v->id);
        v = v->next;
    }
    f->Set(in->vendor_id); f->active = edit; f = f->next;
    if (in->id == 0)
        f->Set("");
    else
        f->Set(in->id);
    f->active = edit; f = f->next;
    f->Set(in->time); f->active = edit; f = f->next;
    return 0;
}

int InvoiceZone::SaveRecord(Terminal *t, int record, int write_file)
{
    FnTrace("InvoiceZone::SaveRecord()");
    if (t->stock == NULL)
        return 1;

    Invoice *in = t->stock->FindInvoiceByRecord(record);
    if (in == NULL)
        return 1;
    FormField *f = FieldList();
    f->Get(in->vendor_id); f = f->next;
    f->Get(in->id); f = f->next;
    f->Get(in->time); f = f->next;
    return 0;
}

int InvoiceZone::NewRecord(Terminal *t)
{
    FnTrace("InvoiceZone::NewRecord()");
    if (t->stock == NULL)
        return 1;
    t->stock->NewInvoice(0);
    edit = 1;
    return 0;
}

int InvoiceZone::KillRecord(Terminal *t, int record)
{
    FnTrace("InvoiceZone::KillRecord()");
    if (t->stock == NULL)
        return 1;
    return 1;
}

int InvoiceZone::Search(Terminal *t, int record, const genericChar* word)
{
    FnTrace("InvoiceZone::Search()");
    return 1;
}

int InvoiceZone::ListReport(Terminal *t, Report *r)
{
    FnTrace("InvoiceZone::ListReport()");
    if (r == NULL)
        return 1;

    Stock *s = t->stock;
    if (s == NULL || s->InvoiceList() == NULL)
    {
        r->TextC(t->Translate("No Invoices for this period"));
        return 0;
    }

    genericChar str[256];
    System *sys = t->system_data;
    Invoice *in = s->InvoiceList();
    while (in)
    {
        r->TextL(t->TimeDate(in->time, TD_DATE));
        Vendor *v = sys->inventory.FindVendorByID(in->vendor_id);
        if (v)
            r->TextC(v->name.Value());
        else
            r->TextC("Unknown Vendor");
        sprintf(str, "%d", in->id);
        r->TextR(str);
        r->NewLine();
        in = in->next;
    }
    return 0;
}

int InvoiceZone::RecordCount(Terminal *t)
{
    FnTrace("InvoiceZone::RecordCount()");
    if (t->stock == NULL)
        return 0;

    return t->stock->InvoiceCount();
}
