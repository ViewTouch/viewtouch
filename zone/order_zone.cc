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
 * order_zone.cc - revision 188 (10/13/98)
 * Implementation of order_zone module
 */

#include "order_zone.hh"
#include "terminal.hh"
#include "check.hh"
#include "sales.hh"
#include "dialog_zone.hh"
#include "employee.hh"
#include "settings.hh"
#include "image_data.hh"
#include "labels.hh"
#include "system.hh"
#include "exception.hh"
#include "manager.hh"
#include "customer.hh"
#include <string.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/***********************************************************************
 * Definitions
 ***********************************************************************/
#define HEADER_SIZE 3
#define FOOTER_SIZE 3


/***********************************************************************
 * OrderEntryZone Class
 ***********************************************************************/
OrderEntryZone::OrderEntryZone()
{
    shown_count     = 0;
    total_orders    = 0;
    page_no         = 0;
    min_size_x      = 19;
    min_size_y      = (HEADER_SIZE + FOOTER_SIZE + 1);
    max_pages       = 1;
    spacing         = 2;
    orders_per_page = 0;
}

int OrderEntryZone::RenderInit(Terminal *term, int update_flag)
{
    FnTrace("OrderEntryZone::RenderInit()");
    Settings *currSettings = term->GetSettings();

    // set size & position
    int size = page->size - 1;
    if (currSettings->oewindow[size].IsSet())
    {
        x = currSettings->oewindow[size].x;
        y = currSettings->oewindow[size].y;
        w = currSettings->oewindow[size].w;
        h = currSettings->oewindow[size].h;
    }

    Check *thisCheck = term->check;
    if (thisCheck == NULL || orders_per_page <= 0)
        return 0;  // Error

    int use_seats = currSettings->use_seats;
    if (thisCheck->CustomerType() != CHECK_RESTAURANT)
        use_seats = 0;

    SubCheck *thisSubCheck = thisCheck->current_sub;
    if (thisSubCheck == NULL)
    {
        term->order = NULL;
        term->seat  = 0;
        if (use_seats)
            thisSubCheck = thisCheck->FirstOpenSubCheck(0);
        else
            thisSubCheck = thisCheck->FirstOpenSubCheck();

        if (thisSubCheck == NULL)
            return 0;
    }

    // Calculate lines of orders & pages
    if (use_seats)
        total_orders = thisSubCheck->OrderCount(term->seat);
    else
        total_orders = thisSubCheck->OrderCount();
    if (term->qualifier)
        ++total_orders;
    max_pages = ((total_orders - 1) / orders_per_page) + 1;
    if (page_no >= max_pages)
        page_no = max_pages - 1;

    // Build array of displayed orders
    int offset = orders_per_page * page_no;
    shown_count = 0;

    for (Order *o = thisSubCheck->OrderList(); o != NULL; o = o->next)
    {
        if (use_seats == 0 || o->seat == term->seat)
        {
            if (offset > 0)
                --offset;
            else if (shown_count < orders_per_page)
                orders_shown[shown_count++] = o;

            for (Order *mod = o->modifier_list; mod != NULL; mod = mod->next)
            {
                if (offset > 0)
                    --offset;
                else if (shown_count < orders_per_page)
                    orders_shown[shown_count++] = mod;
            }
        }
    }

    // Select Last Order (no modifier) if no orders are already selected
    if (term->order == NULL && thisCheck->current_sub)
        term->order = thisCheck->current_sub->LastParentOrder(term->seat);

    return 0;
}

RenderResult OrderEntryZone::Render(Terminal *t, int update_flag)
{
    FnTrace("OrderEntryZone::Render()");
    LayoutZone::Render(t, update_flag);
    Check *c = t->check;
    if (c == NULL)
        return RENDER_OKAY;

    // See if list size has changed
    int n = (int) ((size_y - FOOTER_SIZE - HEADER_SIZE - 1) /
                   SpacingValue(t)) + 1;
    if (n > 32)
        n = 32;

    if (n != orders_per_page)
    {
        page_no         = 0;
        orders_per_page = n;
        RenderInit(t, update_flag);
    }

    SubCheck *sc = c->current_sub;
    if (sc == NULL)
        return RENDER_OKAY;

    genericChar str[256];
    genericChar str2[256];
    genericChar str3[256];
    int col = color[0];

    // Draw Header
    int subs = c->SubCount();
    Settings *s = t->GetSettings();
    int use_seats = s->use_seats;
    if (c->CustomerType() != CHECK_RESTAURANT)
        use_seats = 0;

    TextC(t, 0, t->UserName(c->user_owner), col);

    switch (c->CustomerType())
	{
    case CHECK_RESTAURANT:
        sprintf(str, "%s %s", t->Translate("Tbl"), c->Table());
        TextL(t, 1, str, col);
        if (use_seats)
        {
            sprintf(str, "%s %s", t->Translate("Seat"),
                    SeatName(t->seat, NULL, c->Guests()));
            TextC(t, 1, str, col);
        }
        else if (subs > 1)
        {
            sprintf(str, "(%d of %d)", sc->number, subs);
            TextC(t, 1, str, COLOR_DK_RED);
        }
        sprintf(str, "%s %2d", t->Translate("Guests"), c->Guests());
        TextR(t, 1, str, col);
        break;
    case CHECK_HOTEL:
        sprintf(str, "%s %s", t->Translate("Room"), c->Table());
        TextL(t, 1, str, col);
        if (subs > 1)
        {
            sprintf(str, "(%d of %d)", sc->number, subs);
            TextC(t, 1, str, COLOR_DK_RED);
        }
        sprintf(str, "%s %2d", t->Translate("Guests"), c->Guests());
        TextR(t, 1, str, col);
        break;
    case CHECK_TAKEOUT:
        TextL(t, 1, t->Translate("Take Out"), col);
        sprintf(str, "%s %d of %d", t->Translate("Part"), sc->number, subs);
        TextR(t, 1, str, col);
        break;
    case CHECK_FASTFOOD:
        TextL(t, 1, t->Translate("Fast Food"), col);
        sprintf(str, "%s %d of %d", t->Translate("Part"), sc->number, subs);
        TextR(t, 1, str, col);
        break;
    case CHECK_DELIVERY:
        TextL(t, 1, t->Translate("Delivery"), col);
        sprintf(str, "%s %d of %d", t->Translate("Part"), sc->number, subs);
        TextR(t, 1, str, col);
        break;
    case CHECK_RETAIL:
        TextL(t, 1, t->Translate("Retail"), col);
        sprintf(str, "%s %d of %d", t->Translate("Part"), sc->number, subs);
        TextR(t, 1, str, col);
        break;
    case CHECK_TOGO:
        TextL(t, 1, "To Go", col);
        sprintf(str, "%s %d of %d", t->Translate("Part"), sc->number, subs);
        TextR(t, 1, str, col);
        break;
    case CHECK_DINEIN:
        TextL(t, 1, "Here", col);
        sprintf(str, "%s %d of %d", t->Translate("Part"), sc->number, subs);
        TextR(t, 1, str, col);
        break;
	}

    // Draw Footer
    TextPosR(t, size_x - 8, size_y - 2, t->Translate("Total"), col);
    TextR(t, size_y - 2, t->FormatPrice(sc->raw_sales - sc->item_comps), col);
    if (max_pages > 1)
        TextL(t, size_y - 1, t->PageNo(page_no + 1, max_pages), COLOR_DK_RED);

    if (sc->status == CHECK_CLOSED)
        TextC(t, size_y - 2, t->Translate("CLOSED"), COLOR_GREEN);
    else if (sc->status == CHECK_VOIDED)
        TextC(t, size_y - 2, t->Translate("VOID"), COLOR_RED);

    Flt my_spacing = SpacingValue(t);
    Flt line    = HEADER_SIZE;

    // Render Order Select Hilight
    if (t->order)
    {
        int select_start = -1, select_len = 0;
        for (int i = 0; i < shown_count; ++i)
        {
            Order *o = orders_shown[i];
            if (o == t->order || o->parent == t->order)
            {
                if (select_start < 0)
                    select_start = i;
                ++select_len;
            }
        }
        if (select_start >= 0)
            Background(t, HEADER_SIZE + (select_start * my_spacing) -
                       ((my_spacing - 1)/2), select_len * my_spacing, IMAGE_LIT_SAND);
    }

    // Render Orders
    for (int i = 0; i < shown_count; ++i)
    {
        Order *o = orders_shown[i];
        int tc;
        if (o == t->order ||
            (t->order && o->parent == t->order))
        {
            if (o->status & ORDER_COMP)
                tc = COLOR_RED;
            else if (o->status & ORDER_SENT)
                tc = COLOR_BLACK;
            else
                tc = COLOR_BLUE;
        }
        else
        {
            if (o->status & ORDER_COMP)
                tc = COLOR_DK_RED;
            else if (o->status & ORDER_SENT)
                tc = COLOR_BLACK;
            else
                tc = COLOR_DK_BLUE;
        }

        if (o->sales_type == SALES_TAKE_OUT)
            strcpy(str3, "TO ");
        else
            str3[0] = '\0';
        o->Description(t, str2);
        if (o->IsModifier())
            sprintf(str, "    %s", str2);
        else if (o->item_type == ITEM_POUND)
            sprintf(str, "%s%.2f %s", str3, ((Flt)o->count / 100), str2);
        else
            sprintf(str, "%s%d %s", str3, o->count, str2);

        if (o->cost != 0.0 || (o->status & ORDER_COMP))
        {
            if (o->status & ORDER_COMP)
                strcpy(str2, t->Translate("COMP"));
            else
                t->FormatPrice(str2, o->cost);
            TextLR(t, line, str, tc, str2, tc);
        }
        else
            TextL(t, line, str, tc);

        line += my_spacing;
    }

    if (t->qualifier)
    {
        PrintItem(str, t->qualifier, "--");
        TextL(t, line, str, COLOR_LT_BLUE);
    }

    Line(t, HEADER_SIZE - 1, col);
    Line(t, size_y - FOOTER_SIZE, col);
    return RENDER_OKAY;
}

SignalResult OrderEntryZone::Signal(Terminal *term, const genericChar* message)
{
	FnTrace("OrderEntryZone::Signal()");
	static const genericChar* commands[] = {
		"cancel", "delete", "consolidate", "final",
        "next check", "prior check", "next seat", "prior seat",
        "takeoutseat", "takeoutattach", NULL};

    Check *currCheck = term->check;
    if (currCheck == NULL)
        return SIGNAL_IGNORED;

    SubCheck *subcheck = currCheck->current_sub;
    if (subcheck == NULL)
        return SIGNAL_IGNORED;

    Settings *settings = term->GetSettings();
    int result = 0;
    int idx = CompareList(message, commands);
    switch (idx)
    {
    case 0:  // Cancel
        term->is_bar_tab = 0;
        result = CancelOrders(term);
        break;

    case 1:  // Delete/Rebuild
        result = DeleteOrder(term);
        break;

    case 2:  // Consolidate
        ClearQualifier(term);
        subcheck->ConsolidateOrders();
        term->order = NULL;
        term->Update(UPDATE_ORDERS, NULL);
        break;

    case 3:  // Final
        if (settings->use_item_target)
        {
            term->Jump(JUMP_NORMAL, PAGEID_ITEM_TARGET);
            return SIGNAL_OKAY;
        }
        else
        {
            result = term->FinalizeOrders();
        }
        break;

    case 4:  // Next Check
        result = NextCheck(term);
        break;
    case 5:  // Prior Check
        result = PriorCheck(term);
        break;
    case 6:  // Next Seat
        result = ShowSeat(term, term->seat + 1);
        break;
    case 7:  // Prior Seat
        result = ShowSeat(term, term->seat - 1);
        break;
    case 8:  // Takeout Seat
        currCheck->has_takeouts = 1;
        if (!currCheck->date.IsSet())
            currCheck->date.Set();
        result = ShowSeat(term, currCheck->Guests());
        break;
    case 9:  // Takeout Attach
        if (term && term->order)
        {
            if (term->order->sales_type & SALES_TAKE_OUT)
                term->order->sales_type &= ~SALES_TAKE_OUT;
            else
            {
                term->order->sales_type |= SALES_TAKE_OUT;
                if (!currCheck->date.IsSet())
                    currCheck->date.Set();
            }
            Draw(term, 1);
        }
        result = 1;
        break;
    default:
        if (StringCompare(message, "void", 4) == 0)
        {
            VoidOrder(term, atoi(&message[4]));
            return SIGNAL_OKAY;
        }
        else if (StringCompare(message, "comp", 4) == 0)
        {
            CompOrder(term, atoi(&message[4]));
            return SIGNAL_OKAY;
        }
        else if (StringCompare(message, "amount ", 7) == 0)
        {
            int count = atoi(&message[7]);
            if (count <= 0)
                count = 1;
            term->order->count = count;
            subcheck->FigureTotals(settings);
            term->Update(UPDATE_ORDERS, NULL);
            return SIGNAL_OKAY;
        }

        // Check for qualifier message
        idx = CompareList(message, QualifierShortName);
        if (idx < 0)
            return SIGNAL_IGNORED;
        MergeQualifier(term->qualifier, QualifierValue[idx]);
        page_no = max_pages;
        Draw(term, 1);
        return SIGNAL_OKAY;
    }

    if (result)
        return SIGNAL_IGNORED;
    else
        return SIGNAL_OKAY;
}

SignalResult OrderEntryZone::Keyboard(Terminal *t, int my_key, int state)
{
    FnTrace("OrderEntryZone::Keyboard()");
    int error = 1;
    switch (my_key)
    {
    case 14:  // ^N - page down
        error = NextPage(t); break;
    case 16:  // ^P - page up
        error = PriorPage(t); break;
    }

    if (error)
        return SIGNAL_IGNORED;
    else
        return SIGNAL_OKAY;
}

SignalResult OrderEntryZone::Touch(Terminal *t, int tx, int ty)
{
    FnTrace("OrderEntryZone::Touch()");
    if (t->check == NULL)
        return SIGNAL_IGNORED;

    LayoutZone::Touch(t, tx, ty);
    if (selected_y < HEADER_SIZE)
    {
        if (PriorPage(t))
            return SIGNAL_IGNORED;
        else
            return SIGNAL_OKAY;
    }
    else if (selected_y >= (size_y - FOOTER_SIZE))
    {
        if (NextPage(t))
            return SIGNAL_IGNORED;
        else
            return SIGNAL_OKAY;
    }

    Flt my_spacing = SpacingValue(t);
    int line = (int) ((selected_y - HEADER_SIZE) / my_spacing);
    if (line >= shown_count || line < 0)
        return SIGNAL_IGNORED;

    if (t->order != orders_shown[line])
    {
        t->order = orders_shown[line];
        t->Update(UPDATE_ORDERS, NULL);
        return SIGNAL_OKAY;
    }
    return SIGNAL_IGNORED;
}

int OrderEntryZone::Update(Terminal *t, int update_message, const genericChar* value)
{
    FnTrace("OrderEntryZone::Update()");
    if (update_message & UPDATE_ORDERS)
    {
        Check *c = t->check;
        if (c)
        {
            SubCheck *sc = c->current_sub;
            if (sc && t->order)
                page_no = sc->OrderPage(t->order, orders_per_page, t->seat);
            else
                page_no = max_pages;
        }
        Draw(t, 1);
    }
    else if (update_message & UPDATE_QUALIFIER)
    {
        page_no = max_pages;
        Draw(t, 0);
    }
    return 0;
}

int OrderEntryZone::SetSize(Terminal *t, int width, int height)
{
    FnTrace("OrderEntryZone::SetSize()");
    if (width < 100)
        width = 100;
    if (height < 100)
        height = 100;
    w = width;
    h = height;

    int size = page->size - 1;
    Settings *s = t->GetSettings();
    s->oewindow[size].SetRegion(x, y, w, h);
    return 0;
}

int OrderEntryZone::SetPosition(Terminal *t, int pos_x, int pos_y)
{
    FnTrace("OrderEntryZone::SetPosition()");
    if (pos_x < 0)
        pos_x = 0;
    if (pos_y < 32)
        pos_y = 32;
    x = pos_x;
    y = pos_y;

    int size = page->size - 1;
    Settings *s = t->GetSettings();
    s->oewindow[size].SetRegion(x, y, w, h);
    return 0;
}

int OrderEntryZone::CancelOrders(Terminal *t)
{
    FnTrace("OrderEntryZone::CancelOrders()");
    Check *c = t->check;
    if (c == NULL)
        return 1;  // Error -- No current check

    Settings  *s = t->GetSettings();
    t->seat = 0;
    t->qualifier = QUALIFIER_NONE;
    c->CancelOrders(s);

    t->order = NULL;
    t->Update(UPDATE_ORDERS, NULL);
    t->UpdateOtherTerms(UPDATE_CHECKS, NULL);
    return 0;
}

int OrderEntryZone::AddQualifier(Terminal *t, int qualifier_type)
{
    FnTrace("OrderEntryZone::AddQualifier()");
    Check *c = t->check;
    if (c == NULL)
        return 1;

    SubCheck *sc = c->current_sub;
    if (sc == NULL || sc->status != CHECK_OPEN)
        return 1;

    MergeQualifier(t->qualifier, qualifier_type);
    page_no = max_pages;
    Draw(t, 0);
    return 0;
}

int OrderEntryZone::DeleteOrder(Terminal *term, int is_void)
{
    FnTrace("OrderEntryZone::DeleteOrder()");

    Check *thisCheck = term->check;
    if (thisCheck == NULL || term->order == NULL)
        return 1;

    SubCheck *sc = thisCheck->current_sub;
    if (sc == NULL)
        return 1;

    int jump = 0;
    if (term->order->count > 1)
    {
        if (term->order->item_type == ITEM_POUND)
            is_void = 1;  // just remove the whole By the Pound order
        else
            --term->order->count;
    }
    else if (term->order->modifier_list)
	{
		// Delete all modifiers - start modifier script  
		while (term->order->modifier_list)
		{
			Order *o = term->order->modifier_list;
			sc->Remove(o);
			delete o; // deleting member from order obj? If so, this should be handled elswhere (like within the class)
		}

		term->Update(UPDATE_ORDERS, NULL);
		term->RunScript(term->order->script.Value(), JUMP_NONE, 0);
	}
    else
    {
        // Finish the two-step deletion
        is_void = 1;
    }

    if (is_void)
    {
        // Delete order - jump to order's page
        Order *o = term->order;
        jump     = term->order->page_id;
        if (term->order->parent)
            term->order = term->order->parent;
        else
        {
            if (term->order->next && term->order->next->seat == term->order->seat)
                term->order = term->order->next;
            else
                term->order = NULL; // next order isn'term on same seat
        }
        sc->Remove(o);
        delete o;
    }

    sc->FigureTotals(term->GetSettings());
    if (jump && jump != term->page->id)
        term->Jump(JUMP_NORMAL, jump);
    else
        term->Update(UPDATE_ORDERS, NULL);
    return 0;
}

int OrderEntryZone::CompOrder(Terminal *term, int reason)
{
    FnTrace("OrderEntryZone::CompOrder()");

    Check    *thisCheck = term->check;
    Employee *employee = term->user;
    Settings *currSettings = term->GetSettings();

    if (thisCheck == NULL || employee == NULL || !employee->CanCompOrder(currSettings))
        return 1;

    SubCheck *thisSubCheck = thisCheck->current_sub;
    if (thisSubCheck == NULL)
        return 1;

    if (reason < 0)
    {
        genericChar str[32];
        SimpleDialog *d = new SimpleDialog("Reason for comping this item:", 1);
        CompInfo *compInfo = currSettings->CompList();
        while (compInfo)
        {
            sprintf(str, "comp %d", compInfo->id);
            d->Button(compInfo->name.Value(), str);
            compInfo = compInfo->next;
        }
        term->OpenDialog(d);

        return 0;
    }

    if (! thisCheck->IsTraining()) 
	{
        term->system_data->exception_db.AddItemException(term, thisCheck, term->order, EXCEPTION_COMP, reason);
	}

	// Should this call be nested in the if() block above? 
	// Does its current location cause unintended duplication?
	thisSubCheck->CompOrder(currSettings, term->order, 1);

    term->Update(UPDATE_ORDERS, NULL);
    return 0;
}

int OrderEntryZone::VoidOrder(Terminal *term, int reason)
{
    FnTrace("OrderEntryZone::VoidOrder()");
    Check    *thisCheck = term->check;
    Employee *employee = term->user;
    Settings *currSettings = term->GetSettings();

    if (thisCheck == NULL || employee == NULL || !employee->CanCompOrder(currSettings))
        return 1;

    if (reason < 0)
    {
        genericChar str[32];
        SimpleDialog *d = new SimpleDialog("Reason for voiding this item:", 1);
        CompInfo *compInfo = currSettings->CompList();
        while (compInfo)
        {
            sprintf(str, "void %d", compInfo->id);
            d->Button(compInfo->name.Value(), str);
            compInfo = compInfo->next;
        }
        term->OpenDialog(d);
        return 0;
    }

    if (! thisCheck->IsTraining())
	{
        term->system_data->exception_db.AddItemException(term, thisCheck, term->order, EXCEPTION_VOID, reason);
	}

	// nuke 'subcheck' (like comps)
    DeleteOrder(term, 1);

    return 0;
}

int OrderEntryZone::NextCheck(Terminal *t)
{
    FnTrace("OrderEntryZone::NextCheck()");
    Check *c = t->check;
    if (c == NULL)
        return 1;

    SubCheck *sc = c->current_sub;
    if (sc == NULL)
        return 1;

    if (sc->next)
        c->current_sub = sc->next;
    else if (sc->OrderList() == NULL || c->Status() != CHECK_OPEN)
    {
        c->Update(t->GetSettings());
        c->current_sub = c->SubList();
    }
    else
        c->NewSubCheck();

    if (c->current_sub)
        // Select Last Order (no modifier)
        t->order = c->current_sub->LastParentOrder(t->seat);

    t->Update(UPDATE_ORDERS, NULL);
    return 0;
}

int OrderEntryZone::PriorCheck(Terminal *t)
{
    FnTrace("OrderEntryZone::PriorCheck()");
    Check *c = t->check;
    if (c == NULL)
        return 1;

    SubCheck *sc = c->current_sub;
    if (sc == NULL)
        return 1;

    if (sc->fore)
    {
        c->current_sub = sc->fore;
        c->Update(t->GetSettings());
    }
    else if (c->Status() == CHECK_OPEN)
        c->NewSubCheck();
    else
        c->current_sub = c->SubListEnd();

    if (c->current_sub)
        // Select Last Order (no modifier)
        t->order = c->current_sub->LastParentOrder(t->seat);

    t->Update(UPDATE_ORDERS, NULL);
    return 0;
}

int OrderEntryZone::ShowSeat(Terminal *t, int seat)
{
    FnTrace("OrderEntryZone::ShowSeat()");
    Settings *s = t->GetSettings();
    Check    *c = t->check;
    if (c == NULL || !s->use_seats)
        return 1;

    SubCheck *sc = c->current_sub;
    if (sc == NULL)
        return 1;

    int guests = c->Guests();
    if (guests <= 0 && c->has_takeouts)           // start at takeout seat
        seat = -1;
    else if (guests <= 0)                         // start at 0
        seat = 0;
    else if (seat < -1 && c->has_takeouts)        // wrap to takeout seat
        seat = guests - 1;
    else if (seat < 0 && !c->has_takeouts)        // wrap to last seat
        seat = guests - 1;
    else if (seat >= guests && c->has_takeouts)   // wrap to takeout seat
        seat = -1;
    else if (seat >= guests)                      // wrap to first seat
        seat = 0;
    t->seat = seat;

    sc->ConsolidateOrders();
    sc = c->FirstOpenSubCheck(t->seat);
    if (sc == NULL)
        return 1;

    t->order = NULL;
    t->Update(UPDATE_ORDERS, NULL);

    if (s->store == STORE_SUNWEST)
    {
        if (c->EntreeCount(t->seat) <= 0 || c->IsTakeOut())
            t->Jump(JUMP_NORMAL, 200);
        else
            t->Jump(JUMP_NORMAL, 206);
    }
    return 0;
}

int OrderEntryZone::NextPage(Terminal *t)
{
    FnTrace("OrderEntryZone::NextPage()");
    if (max_pages <= 1)
        return 1;

    t->qualifier = QUALIFIER_NONE;
    // Pagedown
    ++page_no;
    if (page_no >= max_pages)
        page_no = 0;
    Draw(t, 1);
    return 0;
}

int OrderEntryZone::PriorPage(Terminal *t)
{
    FnTrace("OrderEntryZone::PriorPage()");
    if (max_pages <= 1)
        return 1;

    t->qualifier = QUALIFIER_NONE;
    // Pageup
    --page_no;
    if (page_no < 0)
        page_no = max_pages - 1;
    Draw(t, 1);
    return 0;
}

int OrderEntryZone::ClearQualifier(Terminal *t)
{
    FnTrace("OrderEntryZone::ClearQualifier()");
    Check *c = t->check;
    if (c == NULL)
        return 1;

    SubCheck *sc = c->current_sub;
    if (sc == NULL)
        return 1;

    // attach "on the side" qualifier to the order item if needed
    if (sc && (t->qualifier & QUALIFIER_SIDE))
    {
        Order *o = sc->LastOrder(t->seat);
        if (o)
            o->qualifier |= QUALIFIER_SIDE;
    }

    if (t->qualifier)
    {
        t->qualifier = QUALIFIER_NONE;
        Draw(t, 1);
    }
    return 0;
}

Flt OrderEntryZone::SpacingValue(Terminal *t)
{
    FnTrace("OrderEntryZone::SpacingValue()");
    if (spacing > 0.0)
        return spacing;

    Flt df = (Flt) t->page->default_spacing;
    if (df > 0)
        return df;
    return (Flt) t->zone_db->default_spacing;
}


/***********************************************************************
 * OrderPageZone Class
 ***********************************************************************/
OrderPageZone::OrderPageZone()
{
    amount = 0;
}

int OrderPageZone::RenderInit(Terminal *t, int update_flag)
{
    FnTrace("OrderPageZone::RenderInit()");
    
    // next/prior seat only enabled on index- and item-pages
    active = (t->page->type == PAGE_INDEX ||
              t->page->type == PAGE_ITEM);
    return 0;
}

RenderResult OrderPageZone::Render(Terminal *t, int update_flag)
{
    FnTrace("OrderPageZone::Render()");
    RenderZone(t, t->Translate(TranslateString(t)), update_flag);
    return RENDER_OKAY;
}

SignalResult OrderPageZone::Touch(Terminal *t, int tx, int ty)
{
    FnTrace("OrderPageZone::Touch()");
    Settings *s = t->GetSettings();
    Check *c = t->check;

    if (s->use_seats && (c == NULL || c->CustomerType() == CHECK_RESTAURANT))
    {
        if (amount > 0)
            return t->Signal("next seat", group_id);
        else
            return t->Signal("prior seat", group_id);
    }
    else if (amount > 0)
        return t->Signal("next check", group_id);
    else
        return t->Signal("prior check", group_id);
}

const char* OrderPageZone::TranslateString(Terminal *t)
{
    FnTrace("OrderPageZone::TranslateString()");
    Settings *s = t->GetSettings();
    Check *c = t->check;

    static genericChar str[256];
    if (s->use_seats && (c == NULL || c->CustomerType() == CHECK_RESTAURANT))
    {
        if (amount > 0)
            strcpy(str, "Next\\Seat");
        else
            strcpy(str, "Prior\\Seat");
    }
    else if (amount > 0)
        strcpy(str, "Next\\Check");
    else
        strcpy(str, "Prior\\Check");
    return str;
}


/***********************************************************************
 * OrderFlowZone Class
 ***********************************************************************/
int OrderFlowZone::RenderInit(Terminal *t, int update_flag)
{
    FnTrace("OrderFlowZone::RenderInit()");
    Page *p = t->page;
    int pt = p->type;

    Settings *s = t->GetSettings();
    if (pt == PAGE_SCRIPTED3)
        active = 0;
    else if (pt == PAGE_SYSTEM || pt == PAGE_CHECKS || p->IsTable())
    {
        Employee *e = t->user;
        Check    *c = t->check;
        active = !(c == NULL || (t->guests <= 0 && !( c->IsTakeOut() || c->IsFastFood())) ||
                   e == NULL || !e->CanOrder(s) ||
                   (!e->IsSupervisor(s) && c->user_owner != e->id));
    }
    else
        active = 1;
    return 0;
}

RenderResult OrderFlowZone::Render(Terminal *term, int update_flag)
{
    FnTrace("OrderFlowZone::Render()");
    int  idx = term->last_index;
    genericChar str[64] = "";

    Settings *settings = term->GetSettings();
    if (update_flag)
	{
		if (term->type == TERMINAL_BAR || term->type == TERMINAL_BAR2)
			meal = INDEX_BAR;
		else if(term->type == TERMINAL_FASTFOOD)
		{
			// find the appropriate index page for the current shift
			int nMealIndex = settings->MealPeriod(SystemTime);
			meal = IndexValue[nMealIndex];  // IndexValue[] defined in main/labels.cc
		}
		else
			meal = settings->MealPeriod(SystemTime);
	}

    int customer_type = 0;
    Check *currCheck = term->check;
    if (currCheck)
        customer_type = currCheck->CustomerType();

    Page *currPage = term->page;
    int pt = currPage->type;
    if (pt == PAGE_SYSTEM || pt == PAGE_CHECKS || currPage->IsTable())
    {
        // Start Order
        if (meal == INDEX_GENERAL || customer_type == CHECK_HOTEL || customer_type == CHECK_RETAIL)
            strcpy(str, "Order Entry");
        else
        {
            int cl = CompareList(meal, IndexValue, 0);
            sprintf(str, "Order %s", IndexName[cl]);
        }
    }
    else if (pt == PAGE_ITEM)
    {
        // Order Index
        if (idx == INDEX_GENERAL)
            strcpy(str, "Index");
        else
        {
            int cl = CompareList(idx, IndexValue, 0);
            sprintf(str, "%s Index", IndexName[cl]);
        }
    }
    else if (pt == PAGE_SCRIPTED || pt == PAGE_SCRIPTED2)
    {
        // Continue Order Script
        if (idx == INDEX_GENERAL)
            strcpy(str, "Continue Ordering");
        else
        {
            int cl = CompareList(idx, IndexValue, 0);
            sprintf(str, "Continue Ordering %s", IndexName[cl]);
        }
    }

    RenderZone(term, str, update_flag);
    return RENDER_OKAY;
}

SignalResult OrderFlowZone::Touch(Terminal *t, int tx, int ty)
{
    FnTrace("OrderFlowZone::Touch()");
    Check *c = t->check;
    if (c == NULL)
        return SIGNAL_IGNORED;

    int error = 1;
    Page *p = t->page;
    int pt = p->type;

    //FIX BAK-->Kludge:  This should not signal a save.  It is for the TakeOut
    //page where we need to save the check and customer before moving on to the
    //next page.  But OrderFlowZone should not be in charge of that.  I want a
    //button that is invisible and allows other buttons to be activated.  That
    //way I can stack two buttons on top of this button, one to send a save
    //message to group 0 and one to send a save message to group 1.  They'll
    //only send messages, nothing else, so there will be no conflict with three
    //stacked buttons that all want to jump to a different page.  This new
    //functionality will require significant changes in Page::FindZone() and
    //all functions that call that method.
    t->Signal("save", 0);
    if (pt == PAGE_SYSTEM || pt == PAGE_CHECKS || p->IsTable())
    {
        int customer_type = c->CustomerType();

        c->current_sub = c->FirstOpenSubCheck();
        t->Signal("ordering", group_id);
        if (customer_type == CHECK_HOTEL)
            error = t->JumpToIndex(INDEX_ROOM);
        else if (customer_type == CHECK_RETAIL)
            error = t->JumpToIndex(INDEX_RETAIL);
        else
            error = t->JumpToIndex(meal);
    }
    else if (pt == PAGE_ITEM)
        error = t->Jump(JUMP_INDEX, 0);
    else if (pt == PAGE_SCRIPTED || pt == PAGE_SCRIPTED2 || pt == PAGE_SCRIPTED3)
        error = t->Jump(JUMP_SCRIPT, 0);

    t->move_check = 0;
    if (error)
        return SIGNAL_IGNORED;
    else
        return SIGNAL_OKAY;
}

int OrderFlowZone::Update(Terminal *t, int update_message, const genericChar* value)
{
    FnTrace("OrderFlowZone::Update()");
    Page *p = t->page;

    if ((p->type == PAGE_SYSTEM || p->type == PAGE_CHECKS || p->IsTable()) &&
        (update_message & (UPDATE_MEAL_PERIOD | UPDATE_CHECKS | UPDATE_GUESTS)))
    {
        Draw(t, RENDER_NEW);
    }

    return 0;
}


/***********************************************************************
 * OrderAddZone Class
 ***********************************************************************/
OrderAddZone::OrderAddZone()
{
    mode = 0;
}

int OrderAddZone::RenderInit(Terminal *t, int update_flag)
{
    FnTrace("OrderAddZone::RenderInit()");
    Order *o = t->order;
    
    // Select Last Order (no modifier) if no orders are already selected
    Check *thisCheck = t->check;

    if ( ! thisCheck )
    {
       mode = 0;
       return 0;
    }

    if (o == NULL && thisCheck->current_sub)
        o = thisCheck->current_sub->LastParentOrder(t->seat);
    
    if (o == NULL)
        mode = 0;
    else if (o->allow_increase == 0)
        mode = 0;
    else if (o->IsReduced())
        mode = 0;
    else if (o->item_type == ITEM_POUND)
        mode = 5;
    else if (o->status & ORDER_COMP)
        mode = 3;
    else if (o->IsModifier())
        mode = 0;
    else if (o->status & ORDER_FINAL)
        mode = 2;
    else if (o->count < 5)
        mode = 1;
    else
        mode = 4;
    return 0;
}

RenderResult OrderAddZone::Render(Terminal *t, int update_flag)
{
    FnTrace("OrderAddZone::Render()");
    RenderInit(t, update_flag);
    const genericChar* str;
    switch (mode)
    {
    case 1: str = "Increase\\Item"; break;
    case 2: str = "Reorder\\Item"; break;
    case 3: str = "Undo\\Comp"; break;
    case 4: str = "Enter Item Count"; break;
    case 5: str = "Enter Quantity"; break;
    default: str = ""; break;
    }
    RenderZone(t, str, update_flag);
    return RENDER_OKAY;
}

SignalResult OrderAddZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("OrderAddZone::Touch()");
    Check *c = term->check;
    if (c == NULL || term->order == NULL)
        return SIGNAL_IGNORED;

    SubCheck *sc = c->current_sub;
    if (sc == NULL || sc->status != CHECK_OPEN)
        return SIGNAL_IGNORED;

    Order *order = term->order;
    Settings *s = term->GetSettings();
    if (order == NULL)
    {
        return SIGNAL_IGNORED;
    }
    else if (order->allow_increase == 0)
    {
        return SIGNAL_IGNORED;
    }
    else if (order->item_type == ITEM_POUND)
    {
        TenKeyDialog *d = new TenKeyDialog("Enter Quantity", 0, 1, 1);
        d->max_amount = 10000; // this will allow up to 999
        term->OpenDialog(d);
        return SIGNAL_OKAY;
    }
    else if (order->IsReduced())
    {
        return SIGNAL_IGNORED;
    }
    else if (order->status & ORDER_COMP)
    {
        sc->CompOrder(s, order, 0); // Undo a comp
    }
    else if (order->IsModifier())
    {
        return SIGNAL_IGNORED;
    }
    else if (s->store == STORE_SUNWEST && order->IsEntree() && !(c->IsTakeOut() || c->IsFastFood()))
    {
        // Can't increase 'entree' order (for SunWest)
        return SIGNAL_IGNORED;
    }
    else if (order->status & ORDER_FINAL)
    {
        // Make a non final copy of a finalized order
        Order *thisOrder = order->Copy();
        thisOrder->status = 0;
        thisOrder->count = 1;
        Order *mod = thisOrder->modifier_list;
        while (mod)
        {
            mod->status = 0;
            mod = mod->next;
        }
        sc->Add(thisOrder);
        term->order = NULL;
    }
    else
    {
        if (order->count >= 5)
        {
            //The tenkey dialog pops up and gets a number from the user.
            //There is no destination associated to the dialog, so when
            //the user presses "Enter" the dialog simply sends a
            //term->Signal() and exits.  On the order page the OrderEntry
            //zone will normally trap the message and apply it to the
            //current order.
            TenKeyDialog *d = new TenKeyDialog("Enter Item Count", 0, 1);
            d->max_amount = 100; // this will allow up to 999
            term->OpenDialog(d);
            return SIGNAL_OKAY;
        }

        // Non-final order - just up the amount
        ++order->count;
        sc->FigureTotals(s);
    }

    term->Update(UPDATE_ORDERS, NULL);
    return SIGNAL_OKAY;
}

int OrderAddZone::Update(Terminal *t, int update_message, const genericChar* value)
{
    FnTrace("OrderAddZone::Update()");
    if (update_message & UPDATE_ORDERS)
    {
        int old_mode = mode;
        RenderInit(t, 1);
        if (old_mode != mode)
            Draw(t, 0);
    }
    return 0;
}


/***********************************************************************
 * OrderDeleteZone Class
 ***********************************************************************/
OrderDeleteZone::OrderDeleteZone()
{
    mode = 0;
}

int OrderDeleteZone::RenderInit(Terminal *t, int update_flag)
{
    FnTrace("OrderDeleteZone::RenderInit()");
    Order *o = t->order;
        
    // Select Last Order (no modifier) if no orders are already selected
    Check *thisCheck = t->check;

    if ( ! thisCheck )
    {
       mode = 0;
       return 0;
    }

    if (o == NULL && thisCheck->current_sub)
        o = thisCheck->current_sub->LastParentOrder(t->seat);
    
    if (o == NULL)
        mode = 0;
    else if (o->status & ORDER_FINAL)
    {
        if (o->IsModifier())
            mode = 6;
        else
            mode = 5;
    }
    else if (o->IsModifier())
        mode = 2;
    else if (o->count > 1)
        mode = 3;
    else if (o->modifier_list)
        mode = 4;
    else
        mode = 1;
    return 0;
}

RenderResult OrderDeleteZone::Render(Terminal *t, int update_flag)
{
    FnTrace("OrderDeleteZone::Render()");
    RenderInit(t, update_flag);
    const genericChar* str;
    switch (mode)
    {
    case 1: str = "Delete\\Item"; break;
    case 2: str = "Delete\\Modifier"; break;
    case 3: str = "Decrease\\Item"; break;
    case 4: str = "Rebuild\\Item"; break;
    case 5: str = "Comp or Void Item"; break;
    case 6: str = "Comp or Void Modifier"; break;
    default: str = "";
    }
    RenderZone(t, str, update_flag);
    return RENDER_OKAY;
}

SignalResult OrderDeleteZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("OrderDeleteZone::Touch()");

    Check    *thisCheck = term->check;
    Employee *thisEmployee = term->user;
    if (thisCheck == NULL || thisEmployee == NULL || term->order == NULL)
        return SIGNAL_IGNORED;

    SubCheck *sc = thisCheck->current_sub;
    if (sc == NULL || sc->status != CHECK_OPEN)
        return SIGNAL_IGNORED;

    Settings *s = term->GetSettings();
    if(term->order->status & ORDER_SENT)
    {
        // Can't delete order - must void or comp
        if (!thisEmployee->CanRebuild(s))
            return SIGNAL_IGNORED;

        SimpleDialog *d = new SimpleDialog("What do you want to do with this Item?");
        d->Button("Comp this Item", "comp-1");
        d->Button("Void this Item", "void-1");
        d->Button("Cancel");
        term->OpenDialog(d);
        return SIGNAL_OKAY;
    }
    return term->Signal("delete", group_id);
}

int OrderDeleteZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    FnTrace("OrderDeleteZone::Update()");
    if (update_message & UPDATE_ORDERS)
    {
        int old_mode = mode;
        RenderInit(term, 1);
        if (mode != old_mode)
            Draw(term, 0);
    }
    return 0;
}


/***********************************************************************
 * ItemZone Class
 ***********************************************************************/
ItemZone::ItemZone()
{
    jump_type = JUMP_NONE;
    jump_id   = 0;
    item      = NULL;
    footer    = 14;
    iscopy    = 0;
    addanyway = 0;
}

Zone *ItemZone::Copy()
{
    FnTrace("ItemZone::Copy()");
    ItemZone *z = new ItemZone;
    z->SetRegion(this);
    z->item_name.Set(item_name);
    z->modifier_script.Set(modifier_script);
    z->name.Set(name);
    z->key       = key;
    z->behave    = behave;
    z->font      = font;
    z->shape     = shape;
    z->group_id  = group_id;
    z->jump_type = jump_type;
    z->jump_id   = jump_id;
    for (int i = 0; i < 3; ++i)
    {
        z->color[i]   = color[i];
        z->image[i]   = image[i];
        z->frame[i]  = frame[i];
        z->texture[i] = texture[i];
    }
    z->iscopy = 1;
    return z;
}

RenderResult ItemZone::Render(Terminal *t, int update_flag)
{
    FnTrace("ItemZone::Render()");
    if (update_flag)
    {
        item = Item(&(t->system_data->menu));
        if (name.length > 0)
        {
            if (item && item->zone_name.length <= 0)
            {
                item->zone_name.Set(name);
                item->changed = 1;
            }
            name.Clear();
        }
    }

    if (item == NULL)
    {
        RenderZone(t, "<Unknown>", update_flag);
        return RENDER_OKAY;
    }
    const char* zn= item->type == ITEM_ADMISSION ? "" : item->ZoneName();
    RenderZone(t, zn, update_flag);
    Settings *s = t->GetSettings();
    int sub = (item->type == ITEM_SUBSTITUTE && (t->qualifier & QUALIFIER_SUB));
    int cost = item->Price(s, t->qualifier);
    
    int col = color[State(t)];
    if (col == COLOR_PAGE_DEFAULT || col == COLOR_DEFAULT)
    {
	col =  t->TextureTextColor(texture[State(t)]);
    } 
    
    int font_height, font_width;
    t->FontSize(font, font_width, font_height);

    if (t->check != NULL &&
        strcmp(t->Translate(EMPLOYEE_TABLE), t->check->Table()) == 0 &&
        item->cost != item->employee_cost)
    {
        cost = item->employee_cost;
        col = COLOR_DK_RED;
    }
    else
    {
        CouponInfo *coupon = s->FindCouponByItem(item, 1);
        if (coupon != NULL)
        {
            // If this coupon is only supposed to be applied once, then we
            // need to verify there isn't already an applicable item on the
            // subcheck
            SubCheck *currsub = NULL;
            if (t->check != NULL)
                currsub = t->check->current_sub;
            int count = coupon->Applies(currsub, 1);
            if ((coupon->flags & TF_APPLY_EACH) || (count < 1))
            {
                cost = coupon->Amount(cost);
                col = COLOR_DK_GREEN;
            }
        }
    }

    if (cost > 0.0 || item->type == ITEM_NORMAL || item->type == ITEM_SUBSTITUTE || item->type == ITEM_ADMISSION)
    {
        genericChar price[32];
        t->FormatPrice(price, cost);
      /*  int o = 14;
        int f = FONT_TIMES_20B;
	
        if (t->page->size >= SIZE_1280x1024)
        {
            o = 19;
            f = FONT_TIMES_24B;
        }*/
      
        t->RenderText(price, x + w - border, y + h - border - font_height,
                      col, font, ALIGN_RIGHT);
    }
    
    if(item->type == ITEM_ADMISSION)
    {
	int offsety=border;
	t->RenderText(item->ZoneName(),x + w / 2.0,y + offsety,col,font,ALIGN_CENTER);
	offsety+=font_height;
	t->RenderText(item->event_time.Value(),x + w / 2.0,y + offsety,col,font,ALIGN_CENTER);
	if(item->location.length > 0)
	{
		offsety+=font_height;
		t->RenderText(item->location.Value(),x + w / 2.0,y + offsety,col,font,ALIGN_CENTER);
	}
	if(s->store_name.length > 0)
	{
		offsety+=font_height;
		t->RenderText(s->store_name.Value(),x + w / 2.0,y + offsety,col,font,ALIGN_CENTER);
	}
	if(item->price_label.length > 0)
	{
		offsety+=font_height;
		t->RenderText(item->price_label.Value(),x + w / 2.0,y + offsety,col,font,ALIGN_CENTER);
	}
	if((item->available_tickets.length > 0) && (item->total_tickets.length > 0))
	{
		offsety+=font_height;
		genericChar buffer[64];
		snprintf(buffer,64,"%s/%s",item->available_tickets.Value(),item->total_tickets.Value());
		t->RenderText(buffer,x + w / 2.0,y + offsety,col,font,ALIGN_CENTER);
	}
    }

    if (item->type == ITEM_MODIFIER || sub)
    {
        t->RenderText("*", x + border, y + h - border - 16, col, FONT_TIMES_34);
    }
    else if (item->type == ITEM_METHOD)
    {
        t->RenderText("*", x + border, y + h - border - 16,
                      COLOR_GRAY, FONT_TIMES_34);
    }

    if (t->show_info)
    {
        const genericChar* str = item->item_name.Value();
        t->RenderText(str, x + border, y + border, col, FONT_TIMES_14,
                      ALIGN_LEFT, w - border);
        str = item->Family(t);
        t->RenderText(str, x + border, y + border + 15, col, FONT_TIMES_14,
                      ALIGN_LEFT, w - border);
        str = item->Printer(t);
        t->RenderText(str, x + border, y + border + 30, col, FONT_TIMES_14,
                      ALIGN_LEFT, w - border);
        str = CallOrderName[item->call_order];
        t->RenderText(str, x + border, y + border + 45, col, FONT_TIMES_14,
                      ALIGN_LEFT, w - border);
    }
    return RENDER_OKAY;
}

SignalResult ItemZone::Signal(Terminal *t, const char* message)
{
    FnTrace("ItemZone::Signal()");
    SignalResult retval = SIGNAL_IGNORED;
	static const genericChar* commands[] = { "addanyway", "addandopentab", NULL };
    int idx = CompareList(message, commands);

    switch (idx)
    {
    case 0:  // addanyway
        if (addanyway)
            retval = Touch(t, 5, 5);  // arbitrary coordinates
        break;
    case 1:  // addandopentab
        if (addanyway)
        {
            retval = Touch(t, 5, 5);  // arbitrary coordinates
            t->Signal("opentabamount", 0);
        }
        break;
    }
    addanyway = 0;

    return retval;
}

SignalResult ItemZone::Touch(Terminal *t, int tx, int ty)
{
    FnTrace("ItemZone::Touch()");
    Settings   *s = t->GetSettings();
    Employee   *e = t->user;
    Check      *c = t->check;
    CouponInfo *coupon;
    int employee = -1;
    int reduced = -1;
    int reduced_price = 0;
    int coupon_id = -1;

    if (c == NULL || e == NULL || !e->CanOrder(s) || item == NULL)
        return SIGNAL_IGNORED;

    SubCheck *sc = c->current_sub;
    if (sc == NULL || sc->status != CHECK_OPEN)
        return SIGNAL_IGNORED;

    if (strcmp("Employee", c->Table()) == 0)
    {
        employee = 1;
        reduced_price = item->employee_cost;
    }
    else
    {
        coupon = s->FindCouponByItem(item, 1);
        if (coupon != NULL)
        {
            int count = coupon->Applies(sc, 1);
            if ((coupon->flags & TF_APPLY_EACH) ||
                (count < 1))
            {
                coupon_id = coupon->id;
                reduced_price = coupon->Amount(item->cost);
                reduced = 2;
            }
        }
    }

    // Create new order
    Order *o = new Order(s, item, t);
    if (o == NULL)
        return SIGNAL_IGNORED;
    o->IsEmployeeMeal(employee);
    o->IsReduced(reduced);
    o->reduced_cost = reduced_price;
    o->auto_coupon_id = coupon_id;

    // "By the pound" orders need to be set to 1.00.
    if (item->type == ITEM_POUND)
        o->count = 100;

    // If we have a tab, we need to verify we aren't exceeding the tab.
    if (addanyway == 0 && t->check != NULL && t->is_bar_tab)
    {
        o->FigureCost();
        if ((sc->TabRemain() - o->total_cost) < 0)
        {
            char str[STRLENGTH] = "";
            strcpy(str, "This order will reduce the tab remaining below 0.\\");
            strcpy(str, "Would you like to extend the tab?");
            SimpleDialog *sd = new SimpleDialog(str);
            sd->Button("Yes", "addandopentab");
            sd->Button("No, just add the order", "addanyway");
            addanyway = 1;
            sd->Button("Cancel", "skipit");
            t->OpenDialog(sd);
            return SIGNAL_OKAY;
        }
    }
    addanyway = 0;

    o->user_id = e->id;
    o->seat    = t->seat;
    if (o->seat == -1)
        o->sales_type |= SALES_TAKE_OUT;
    o->page_id = t->page->id;
    o->script.Set(modifier_script.Value());

    if (t->order && t->order->parent)
        t->order = t->order->parent;
    if (t->order && o->IsModifier())
    {
        if (t->order->Add(o))
        {
            delete o;
            return SIGNAL_IGNORED;
        }
    }
    else
    {
        if (sc->Add(o))
        {
            delete o;
            return SIGNAL_IGNORED;
        }
        t->order = o;
    }

    sc->FigureTotals(s);

    int my_update = UPDATE_ORDERS;
    if (t->qualifier != QUALIFIER_NONE)
    {
        my_update |= UPDATE_QUALIFIER;
        t->qualifier = QUALIFIER_NONE;
    }

    t->Update(my_update, NULL);
    t->RunScript(modifier_script.Value(), jump_type, jump_id);
    if (t->cdu != NULL)
    {
        genericChar buffer[STRLONG];
        int buflen;
        int width;

        t->cdu->Refresh(20);
        t->cdu->Clear();
        width = t->cdu->Width();
        snprintf(buffer, width + 1, "%s",item->PrintName());
        t->cdu->Write(buffer);
        strcpy(buffer, t->FormatPrice(item->cost));
        buflen = strlen(buffer);
        t->cdu->ToPos(-(buflen-1), 2);
        t->cdu->Write(buffer);
    }
    return SIGNAL_OKAY;
}

int ItemZone::Update(Terminal *t, int update_message, const genericChar* value)
{
    FnTrace("ItemZone::Update()");
    if (update_message & UPDATE_MENU)
        Draw(t, 1);
    else if ((update_message & UPDATE_QUALIFIER) && item)
        Draw(t, 0);
    return 0;
}

SalesItem *ItemZone::Item(ItemDB *db)
{
    FnTrace("ItemZone::Item()");
    return db->FindByName(item_name.Value());
}


/***********************************************************************
 * QualifierZone Class
 ***********************************************************************/
QualifierZone::QualifierZone()
{
    jump_type      = JUMP_NONE;
    jump_id        = 0;
    qualifier_type = QUALIFIER_NO;
    index          = -1;
}

RenderResult QualifierZone::Render(Terminal *t, int update_flag)
{
    FnTrace("QualifierZone::Render()");
    behave = BEHAVE_NONE;
    if (update_flag)
        index = CompareList(qualifier_type, QualifierValue);

    if (t->qualifier & qualifier_type)
        stay_lit = 1;
    else
        stay_lit = 0;

    if (index < 0)
        RenderZone(t, t->Translate(UnknownStr), update_flag);
    else
        RenderZone(t, QualifierName[index], update_flag);
    return RENDER_OKAY;
}

SignalResult QualifierZone::Touch(Terminal *t, int tx, int ty)
{
    FnTrace("QualifierZone::Touch()");
    Check *c = t->check;
    if (qualifier_type <= 0 || c == NULL)
        return SIGNAL_IGNORED;

    SubCheck *sc = c->current_sub;
    if (sc == NULL || sc->status != CHECK_OPEN)
        return SIGNAL_IGNORED;

    if (t->qualifier & qualifier_type)
        t->qualifier -= qualifier_type;
    else
        MergeQualifier(t->qualifier, qualifier_type);

    t->Update(UPDATE_QUALIFIER, NULL);
    t->Jump(jump_type, jump_id);
    return SIGNAL_OKAY;
}

int QualifierZone::Update(Terminal *t, int update_message, const genericChar* value)
{
    FnTrace("QualifierZone::Update()");
    if (update_message & UPDATE_QUALIFIER)
        Draw(t, 0);
    return 0;
}
