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
 * split_check_zone.cc - revision 69 (10/13/98)
 * Implementaion of SplitCheckZone
 */

#include "split_check_zone.hh"
#include "terminal.hh"
#include "check.hh"
#include "src/utils/cpp23_utils.hh"
#include "image_data.hh"
#include "settings.hh"
#include "labels.hh"
#include "manager.hh"
#include "system.hh"
#include "dialog_zone.hh"
#include "safe_string_utils.hh"
#include <string.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define CHECKS_SHOWN 4


/**** ItemObj Class ****/
class ItemObj : public ZoneObject
{
public:
    Order *order;
    int    seat;

    // Constructors
    ItemObj(Order *o, int font_id = FONT_DEFAULT);
    ItemObj(int seat_no, int font_id = FONT_DEFAULT);

    // Member Function
    int Render(Terminal *t) override;
};

// Constructors
ItemObj::ItemObj(Order *o, int font_id)
{
    FnTrace("ItemObj::ItemObj()");
    order = o;
    seat = -1;
    w = 280;
    h = 80;
    font = font_id;
}

ItemObj::ItemObj(int seat_no, int font_id)
{
    order = nullptr;
    seat = seat_no;
    w = 150;
    h = 168;
    font = font_id;
}

// Member Functions
int ItemObj::Render(Terminal *t)
{
    FnTrace("ItemObj::Render()");
    genericChar str[STRLENGTH];

    int frame_type = selected ? ZF_DOUBLE : ZF_RAISED;
    int texture = selected ? IMAGE_YELLOW_TEXTURE : IMAGE_SAND;
    t->RenderButton(x, y, w, h, frame_type, texture);

    if (seat >= 0)
    {
        SeatName(seat, str);
        t->RenderText(str, x + (w/2), y + (h/2), COLOR_BLACK, font, ALIGN_CENTER);
    }
    else if (order)
    {
        order->Description(t, str);
        t->RenderText(str, x + 8, y + (h/2), COLOR_BLACK, font, ALIGN_LEFT);
    }
    return 0;
}


/**** CheckObj Class ****/
class CheckObj : public ZoneObject
{
public:
    ZoneObjectList items;
    int page, max_pages;
    SubCheck *sub;

    // Constructor
    CheckObj(SubCheck *sc, int seat_mode = 0, int font_id = FONT_DEFAULT);

    // Member Functions
    int   Layout(Terminal *t, int lx, int ly, int lw, int lh) override;
    int   Render(Terminal *t) override;
    void *Data() { return sub; }
};

// Constructor
CheckObj::CheckObj(SubCheck *sc, int seat_mode, int font_id)
{
    FnTrace("CheckObj::CheckObj()");
    sub       = sc;
    page      = 0;
    max_pages = 0;
    active    = 0;
    font      = font_id;
    int i;

    if (sc == nullptr)
        return;

    if (seat_mode)
    {
        // Make seat buttons
        int seat_count[32];
        for (i = 0; i < 32; ++i)
            seat_count[i] = 0;

        // count orders per seat
        for (Order *o = sc->OrderList(); o != nullptr; o = o->next)
        {
            if (o->seat >= 0 && o->seat < 32)
                ++seat_count[o->seat];
        }

        // create seats items
        for (i = 0; i < 32; ++i)
        {
            if (seat_count[i] > 0)
                items.Add(new ItemObj(i, font_id));  // Add new seat item
        }
    }
    else
    {
        for (Order *o = sc->OrderList(); o != nullptr; o = o->next)
        {
            if (o->item_type == ITEM_POUND)
            {
                items.Add(new ItemObj(o, font_id));
            }
            else
            {
                for (i = 0; i < o->count; ++i)
                    items.Add(new ItemObj(o, font_id));
            }
        }
    }
}

// Member Functions
int CheckObj::Layout(Terminal *t, int lx, int ly, int lw, int lh)
{
    FnTrace("CheckObj::Layout()");
    SetRegion(lx, ly, lw, lh);

    int height_left = h - 100;
    int width_left = w - 10;
    int max_width = 0, p = 0;

    ZoneObject *zo = items.List();
    while (zo)
    {
        if (zo->w > (w - 10))
            zo->w = w - 10;

        if (zo->w > max_width)
            max_width = zo->w;

        if (zo->h > height_left)
        {
            height_left = h - 100;
            width_left -= max_width;
            max_width   = 0;
        }

        if (zo->w > width_left)
        {
            if (zo == items.List())
                return 1;  // Can't fit any items on check

            width_left  = w - 10;
            height_left = h - 100;
            max_width   = 0;
            ++p;
        }

        zo->x = x + w - width_left  - 4;
        zo->y = y + h - height_left - 50;
        zo->active = (page == p);
        max_pages = p + 1;

        height_left -= zo->h;
        zo = zo->next;
    }
    return 0;
}

int CheckObj::Render(Terminal *t)
{
    FnTrace("CheckObj::Render()");

    t->RenderButton(x, y, w, h, ZF_DOUBLE, IMAGE_SAND);

    genericChar str[256];
    if (sub) {
        vt_safe_string::safe_format(str, 256, "%s %d", t->Translate("Check"), sub->number);
        t->RenderText(str, x + (w/2), y + 20, COLOR_BLACK, font, ALIGN_CENTER);

        // Show tax amount above total if there's tax
        int tax_amount = sub->TotalTax();
        if (tax_amount > 0) {
            vt_safe_string::safe_format(str, 256, "%s %s", t->Translate("Tax"), t->FormatPrice(tax_amount));
            t->RenderText(str, x + (w/2), y + h - 64, COLOR_BLACK, FONT_TIMES_20B, ALIGN_CENTER);
        }

        t->RenderText(t->FormatPrice(sub->total_cost), x + (w/2), y + h - 40, COLOR_BLACK, FONT_TIMES_20B, ALIGN_CENTER);
    } else {
        t->RenderText(t->Translate("New Check"), x + (w/2), y + (h/2), COLOR_GRAY, font, ALIGN_CENTER);
    }

    items.Render(t);
    return 0;
}


/**** SplitCheckZone Class ****/
// Member Functions
RenderResult SplitCheckZone::Render(Terminal *t, int update_flag)
{
    FnTrace("SplitCheckZone::Render()");
    RenderZone(t, nullptr, update_flag);

    if (t->check == nullptr || t->check->SubList() == nullptr)
    {
        t->RenderText(t->Translate("Add items to your order first"), x + (w/2), y + (h/2), COLOR_BLACK, font, ALIGN_CENTER);
        return RENDER_OKAY;
    }

    Settings *s = t->GetSettings();
    if (s == nullptr)
        return RENDER_ERROR;

    if (update_flag)
    {
        start_check = 0;
        seat_mode = 0;
        if (s->split_check_view == SPLIT_CHECK_SEAT)
            seat_mode = s->use_seats;
        CreateChecks(t);
    }

    // Render instruction header
    RenderInstructions(t);

    // Render checks
    LayoutChecks(t);
    checks.Render(t);

    return RENDER_OKAY;
}

SignalResult SplitCheckZone::Signal(Terminal *t, const genericChar* message)
{
    FnTrace("SplitCheckZone::Signal()");
    static const genericChar* commands[] = {
        "change view", "print", "split by seat", "merge", "next", "prior",
        "amount ", nullptr};

    Check *c = t->check;
    if (c == nullptr)
        return SIGNAL_IGNORED;

    Settings *s = t->GetSettings();
    if (s == nullptr)
        return SIGNAL_IGNORED;
    int subs;
    int error = 1;
    int idx = CompareListN(commands, message);
    int amount;

    switch (idx)
    {
    case 0:  // change view
        if (s->use_seats)
        {
            seat_mode ^= 1;
            CreateChecks(t);
            Draw(t, 0);
            error = 0;
        }
        break;
    case 1:  // print
        error = PrintReceipts(t);
        break;
    case 2:  // split by seat
        start_check = 0;
        error = c->SplitCheckBySeat(s);
        CreateChecks(t);
        Draw(t, 0);
        break;
    case 3:  // merge
        start_check = 0;
        error = c->MergeOpenChecks(s);
        CreateChecks(t);
        Draw(t, 0);
        break;
    case 4:  // next page
        subs = c->SubCount();
        if (subs > CHECKS_SHOWN)
        {
            if (start_check >= (subs - 1))
                start_check = 0;
            else
            {
                start_check += CHECKS_SHOWN;
                if (start_check >= (subs - CHECKS_SHOWN))
                    start_check = subs - 1;
            }
            LayoutChecks(t);
            Draw(t, 0);
        }
        break;
    case 5:  // prior page
        subs = c->SubCount();
        if (subs > CHECKS_SHOWN)
        {
            if (start_check <= 0)
                start_check = subs - 1;
            else
            {
                start_check -= CHECKS_SHOWN;
                if (start_check < 0)
                    start_check = 0;
            }
            LayoutChecks(t);
            Draw(t, 0);
        }
        break;
    case 6:  // amount
        if (from_check != nullptr && to_check != nullptr && item_object != nullptr)
        {
            int last = 0;
            if (to_check->sub == nullptr)
            {
                last = 1;
                to_check->sub = t->check->NewSubCheck();
            }
            amount = atoi(&message[7]);
            to_check->sub->Add(from_check->sub->RemoveCount(item_object->order, amount));
            from_check = nullptr;
            to_check = nullptr;
            item_object = nullptr;
            t->check->Update(s);
            if (last)
                start_check = t->check->SubCount();
            CreateChecks(t);
            Draw(t, 0);
        }
        break;
    }
    if (error)
        return SIGNAL_IGNORED;
    else
        return SIGNAL_OKAY;
}

SignalResult SplitCheckZone::Touch(Terminal *t, int tx, int ty)
{
    FnTrace("SplitCheckZone::Touch()");
    if (t->check == nullptr)
        return SIGNAL_IGNORED;

    ZoneObject *zo = checks.Find(tx, ty);
    if (zo)
    {
        CheckObj *co = (CheckObj *) zo;
        if (ty >= (zo->y + zo->h - 52))
        {
            if (co->max_pages <= 1)
                return SIGNAL_IGNORED;
            ++co->page;
            if (co->page >= co->max_pages)
                co->page = 0;
            co->Draw(t);
            return SIGNAL_OKAY;
        }

        ZoneObject *io = co->items.Find(tx, ty);
        if (io)
            io->Touch(t, tx, ty);
        else
            MoveItems(t, co);
        return SIGNAL_OKAY;
    }
    return SIGNAL_IGNORED;
}

int SplitCheckZone::CreateChecks(Terminal *t)
{
    FnTrace("SplitCheckZone::CreateChecks()");
    checks.Purge();
    Check *check = t->check;
    if (check == nullptr || check->SubList() == nullptr)
        return 1;

    for (SubCheck *sc = check->SubList(); sc != nullptr; sc = sc->next)
    {
        if (sc->status == CHECK_OPEN)
            checks.Add(new CheckObj(sc, seat_mode, font));
    }

    // Add Blank Check
    checks.Add(new CheckObj(nullptr, 0, font));
    return 0;
}

int SplitCheckZone::LayoutChecks(Terminal *t)
{
    FnTrace("SplitCheckZone::LayoutChecks()");
    int cx = x + border, cy = y + border;
    int ch = h - border * 2;
    int cw = w - border * 2;
    int i;

    int count = checks.Count();
    if (count > 0)
    {
        int cmax = count;
        if (cmax > (CHECKS_SHOWN + 1))
            cmax = CHECKS_SHOWN + 1;
        cw = (cw * 2) / ((cmax * 2) - 1);
    }

    int end = Min(start_check, count - CHECKS_SHOWN - 1);
    checks.SetActive(0);

    ZoneObject *zo = checks.List();
    for (i = 0; i < end; ++i)
        zo = zo->next;

    for (i = 0; i <= CHECKS_SHOWN; ++i)
    {
        if (i >= CHECKS_SHOWN)
            zo = checks.ListEnd(); // blank check is always shown

        zo->active = 1;
        int lw;
        if (zo->next == nullptr)
            lw = w - cx - border;
        else
            lw = cw;
        zo->Layout(t, cx, cy, lw, ch);
        cx += cw;
        zo = zo->next;
        if (zo == nullptr)
            break;
    }
    return 0;
}

int SplitCheckZone::MoveItems(Terminal *t, CheckObj *target, int move_amount)
{
    FnTrace("SplitCheckZone::MoveItems()");
    TenKeyDialog *dialog;

    if (target == nullptr)
        return 1;
    target->items.SetSelected(0);

    int count = 0;
    ZoneObject *list = checks.List();
    while (list)
    {
        count += ((CheckObj *)list)->items.CountSelected();
        list = list->next;
    }

    if (count <= 0)
    {
        target->Draw(t);
        return 1;  // No items to move
    }

    int last = 0;
    if (target->sub == nullptr)
    {
        // target is blankcheck area - create new sub check for moved items
        last = 1;
        target->sub = t->check->NewSubCheck();
    }

    // Move orders
    list = checks.List();
    while (list)
    {
        CheckObj *co = (CheckObj *) list;
        ZoneObject *zo = co->items.List();
        while (zo)
        {
            if (zo->selected)
            {
                ItemObj *io = (ItemObj *) zo;
                if (io->seat >= 0)
                {
                    t->check->MoveOrdersBySeat(co->sub, target->sub, io->seat);
                }
                else if (io->order)
                {
                    if (io->order->item_type == ITEM_POUND)
                    {
                        if (count == 1 && move_amount < 0)
                        {
                            from_check = co;
                            to_check   = target;
                            item_object = io;
                            dialog = new TenKeyDialog(GlobalTranslate("Enter Amount to Move"), io->order->count);
                            dialog->target_zone = this;
                            t->OpenDialog(dialog);
                        }
                        else
                        {
                            if (move_amount < 0)
                                move_amount = io->order->count;
                            Order *order = co->sub->RemoveCount(io->order, move_amount);
                            target->sub->Add(order);
                        }
                    }
                    else
                    {
                        target->sub->Add(co->sub->RemoveOne(io->order));
                    }
                }
            }
            zo = zo->next;
        }
        list = list->next;
    }

    Settings *s = t->GetSettings();
    if (s != nullptr)
        t->check->Update(s);
    if (last)
        start_check = t->check->SubCount();
    CreateChecks(t);
    Draw(t, 0);

    return 0;
}

void SplitCheckZone::RenderInstructions(Terminal *t)
{
    FnTrace("SplitCheckZone::RenderInstructions()");
    t->RenderText(t->Translate("Select items, then tap destination check"), x + (w/2), y + 25, COLOR_BLACK, font, ALIGN_CENTER);
}

int SplitCheckZone::PrintReceipts(Terminal *t)
{
    FnTrace("SplitCheckZone::PrintReceipts()");
    Check *c = t->check;
    if (c == nullptr)
        return 1;

    Printer *p = t->FindPrinter(PRINTER_RECEIPT);
    for (SubCheck *sc = c->SubList(); sc != nullptr; sc = sc->next)
        if (sc->status == CHECK_OPEN)
            sc->PrintReceipt(t, c, p);
    return 0;
}


/**** PrintTargetObj Class ****/
class PrintTargetObj : public ZoneObject
{
public:
    ZoneObjectList items;
    int page, max_pages, type;
    Str name;

    // Constructor
    PrintTargetObj(Terminal *t, Check *c, int printer_id);

    // Member Functions
    int Render(Terminal *t) override;
    int Layout(Terminal *t, int lx, int ly, int lw, int lh) override;
};

// Constructors
PrintTargetObj::PrintTargetObj(Terminal *t, Check *c, int printer_id)
{
    FnTrace("PrintTargetObj::PrintTargetObj()");
    type = printer_id;
    page = 0;
    max_pages = 0;

    int pid;
    Settings *s = t->GetSettings();
    for (SubCheck *sc = c->SubList(); sc != nullptr; sc = sc->next)
        for (Order *o = sc->OrderList(); o != nullptr; o = o->next)
            if (!(o->status & ORDER_SENT))
            {
                pid = o->printer_id;
                if (pid == PRINTER_DEFAULT)
                    pid = (s != nullptr) ? o->FindPrinterID(s) : 0;
                if (pid == printer_id)
                    for (int i = 0; i < o->count; ++i)
                        items.Add(new ItemObj(o));
            }
}

// Member Functions
int PrintTargetObj::Render(Terminal *t)
{
    FnTrace("PrintTargetObj::Render()");
    t->RenderButton(x, y, w, h, ZF_RAISED, IMAGE_SAND);
    t->RenderText(name.Value(), x + (w/2), y + 16, COLOR_BLACK,
                  FONT_TIMES_20B, ALIGN_CENTER);
    items.Render(t);

    if (max_pages > 1)
        t->RenderText(t->PageNo(page + 1, max_pages), x + 8, y + h - 24,
                      COLOR_RED, FONT_TIMES_20);
    return 0;
}

int PrintTargetObj::Layout(Terminal *t, int lx, int ly, int lw, int lh)
{
    FnTrace("PrintTargetObj::Layout()");
    SetRegion(lx, ly, lw, lh);

    int height_left = h - 100;
    int width_left = w - 10;
    int max_width = 0, p = 0;

    ZoneObject *zo = items.List();
    while (zo)
    {
        if (zo->w > (w - 10))
            zo->w = w - 10;
        if (zo->w > max_width)
            max_width = zo->w;

        if (zo->h > height_left)
        {
            height_left = h - 100;
            width_left -= max_width;
            max_width   = 0;
        }

        if (zo->w > width_left)
        {
            if (zo == items.List())
                return 1;  // Can't fit any items on view
            width_left  = w - 10;
            height_left = h - 100;
            max_width   = 0;
            ++p;
        }

        zo->x = x + w - width_left  - 4;
        zo->y = y + h - height_left - 50;
        zo->active = (page == p);
        max_pages = p + 1;
        height_left -= zo->h;
        zo = zo->next;
    }
    return 0;
}

/**** ItemPrintTargetZone Class ****/
// Member Functions
RenderResult ItemPrintTargetZone::Render(Terminal *t, int update_flag)
{
    FnTrace("ItemPrintTargetZone::Render()");
    RenderZone(t, nullptr, update_flag);
    Check *c = t->check;
    if (c == nullptr)
        return RENDER_OKAY;

    Settings *s = t->GetSettings();
    if (s == nullptr)
        return RENDER_ERROR;
    if (update_flag)
    {
        targets.Purge();
        empty_targets.Purge();
        PrintTargetObj *pto;
        PrinterInfo *pi = s->PrinterList();
        while (pi)
        {
            if (pi->type != PRINTER_RECEIPT && pi->type != PRINTER_REPORT)
            {
                pto = new PrintTargetObj(t, c, pi->type);
                if (pi->name.size() > 0)
                    pto->name.Set(pi->name);
                else
                    pto->name.Set(FindStringByValue(pi->type,
                                                    const_cast<int*>(PrinterTypeValue.data()),
                                                    const_cast<const genericChar**>(PrinterTypeName.data()),
                                                    UnknownStr));
                if (pto->items.Count() > 0 || pi->type == PRINTER_KITCHEN1)
                    targets.Add(pto);
                else
                    empty_targets.Add(pto);
            }
            pi = pi->next;
        }
        pto = new PrintTargetObj(t, c, PRINTER_RECEIPT);
        pto->name.Set(GlobalTranslate("Local Receipt"));
        if (pto->items.Count() > 0)
            targets.Add(pto);
        else
            empty_targets.Add(pto);
    }

    int lx = x + border, ly = y + border;
    int lw = w - (border * 2), lh = h - (border * 2);

    int e_count = empty_targets.Count();
    if (e_count > 0)
    {
        int no = targets.Count();
        int ww = lw / (no + 1);
        targets.LayoutColumns(t, lx, ly, lw - ww, lh);
        empty_targets.LayoutRows(t, lx + (lw - ww), ly, ww, lh);
    }
    else
        targets.LayoutColumns(t, lx, ly, lw, lh);
  
    targets.Render(t);
    empty_targets.Render(t);
    return RENDER_OKAY;
}

SignalResult ItemPrintTargetZone::Signal(Terminal *t, const genericChar* message)
{
    FnTrace("ItemPrintTargetZone::Signal()");
    static const genericChar* commands[] = {"final", "reset", nullptr};
    int idx = CompareList(message, commands);
    switch (idx)
    {
    case 0:  // final
        t->FinalizeOrders();
        return SIGNAL_OKAY;
    case 1:  // reset
    {
        ZoneObject *list = targets.List();
        while (list)
        {
            PrintTargetObj *pto = (PrintTargetObj *) list;
            ZoneObject *zo = pto->items.List();
            while (zo)
            {
                ItemObj *io = (ItemObj *) zo;
                io->order->printer_id = PRINTER_DEFAULT;
                zo = zo->next;
            }
            list = list->next;
        }
        Draw(t, 1);
        return SIGNAL_OKAY;
    }
    default:
        return SIGNAL_IGNORED;
    }
}

SignalResult ItemPrintTargetZone::Touch(Terminal *t, int tx, int ty)
{
    FnTrace("ItemPrintTargetZone::Touch()");
    if (t->check == nullptr)
        return SIGNAL_IGNORED;

    ZoneObject *zo = empty_targets.Find(tx, ty);
    if (zo)
    {
        MoveItems(t, (PrintTargetObj *) zo);
        return SIGNAL_OKAY;
    }

    zo = targets.Find(tx, ty);
    if (zo)
    {
        PrintTargetObj *pto = (PrintTargetObj *) zo;
        if (ty >= (zo->y + zo->h - 52))
        {
            if (pto->max_pages <= 1)
                return SIGNAL_IGNORED;
            ++pto->page;
            if (pto->page >= pto->max_pages)
                pto->page = 0;
            pto->Draw(t);
            return SIGNAL_OKAY;
        }

        ZoneObject *io = pto->items.Find(tx, ty);
        if (io)
            io->Touch(t, tx, ty);
        else
            MoveItems(t, pto);
        return SIGNAL_OKAY;
    }
    return SIGNAL_IGNORED;
}

int ItemPrintTargetZone::MoveItems(Terminal *t, PrintTargetObj *target)
{
    FnTrace("ItemPrintTargetZone::MoveItems()");
    if (target == nullptr)
        return 1;

    int count = 0;
    ZoneObject *list = targets.List();
    while (list)
    {
        count += ((PrintTargetObj *)list)->items.CountSelected();
        list = list->next;
    }

    if (count <= 0)
        return 1;  // No items to move

    // Move orders
    list = targets.List();
    while (list)
    {
        ZoneObject *zo = ((PrintTargetObj *)list)->items.List();
        while (zo)
        {
            if (zo->selected)
                ((ItemObj *)zo)->order->printer_id = target->type;
            zo = zo->next;
        }
        list = list->next;
    }
    Draw(t, 1);
    return 0;
}
