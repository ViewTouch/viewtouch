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
 * drawer_zone.cc - revision 103 (10/13/98)
 * Implementation of drawer_zone module
 */

#include "drawer_zone.hh"
#include "check.hh"
#include "employee.hh"
#include "drawer.hh"
#include "report.hh"
#include "system.hh"
#include "dialog_zone.hh"
#include "settings.hh"
#include "labor.hh"
#include "image_data.hh"
#include "archive.hh"
#include "manager.hh"
#include <string.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define ALL_DRAWERS   -1

/*********************************************************************
 * DrawerObj Class for DrawerManageZone
 ********************************************************************/

class DrawerObj : public ZoneObject
{
public:
    Drawer *drawer;
    int     status;

    DrawerObj(Drawer *d);
    int Render(Terminal *term);
};

DrawerObj::DrawerObj(Drawer *d)
{
    drawer = d;
    status = d->Status();
    w      = 120;
    h      = 80;
}

int DrawerObj::Render(Terminal *term)
{
    FnTrace("DrawerObj::Render()");
    int zt;
    int is_server_bank = drawer->IsServerBank();
    if (selected)
        zt = IMAGE_LIT_SAND;
    else if (drawer->serial_number == ALL_DRAWERS)
        zt = IMAGE_LITE_WOOD;
    else if (!is_server_bank)
        zt = IMAGE_DARK_WOOD;
    else
        zt = IMAGE_WOOD;
    term->RenderButton(x, y, w, h, ZF_RAISED, zt);

    status = drawer->Status();
    char str[STRLENGTH];
    int c;
    if (h >= 32 && drawer->serial_number != ALL_DRAWERS)
    {
        switch (status)
        {
        default:
        case DRAWER_OPEN:
            c = COLOR_GREEN;
            strcpy(str, term->Translate("Open"));
            break;
        case DRAWER_PULLED:
            strcpy(str, term->Translate("Pulled"));
            c = COLOR_RED;
            break;
        case DRAWER_BALANCED:
            if (drawer->total_difference > 0)
            {
                sprintf(str, "+ %s", term->FormatPrice(drawer->total_difference));
                c = COLOR_BLUE;
            }
            else if (drawer->total_difference < 0)
            {
                sprintf(str, "- %s", term->FormatPrice(-drawer->total_difference));
                c = COLOR_RED;
            }
            else
            {
                strcpy(str, term->Translate("Balanced"));
                c = COLOR_MAGENTA;
            }
            break;
        }
        int offset = h - 20;
        if (offset > 24) offset = 24;
        if (offset < 19) offset = 19;
        term->RenderText(str, x + (w/2), y + h - offset, c, FONT_TIMES_20,
                      ALIGN_CENTER, w - 4);
    }
    if (is_server_bank)
    {
        // personal balance - not a physical drawer
        Employee *employee = term->system_data->user_db.FindByID(drawer->owner_id);
        if (employee)
            strcpy(str, employee->system_name.Value());
        else
            strcpy(str, term->Translate("Server Bank"));
    }
    else if (drawer->serial_number == ALL_DRAWERS)
    {
        sprintf(str, "All Drawers");
    }
    else if (drawer->term)
    {
        sprintf(str, drawer->term->name.Value());
    }
    else
    {
        sprintf(str, "%s %d", term->Translate("Drawer"), drawer->number);
    }
    
    c = COLOR_WHITE;
    if (drawer->serial_number == ALL_DRAWERS)
        c = COLOR_DK_BLUE;
    else if (selected)
        c = COLOR_BLACK;

    if (h > 40)
        term->RenderZoneText(str, x + 3, y, w - 6, h - 14, c, FONT_TIMES_24);
    else
        term->RenderZoneText(str, x + 2, y, w - 4, h - 12, c, FONT_TIMES_20);
    return 0;
}


/*********************************************************************
 * ServerDrawerObj Class for use by the DrawerAssignZone class
 ********************************************************************/

class ServerDrawerObj : public ZoneObject
{
public:
    ZoneObjectList drawers;
    Employee *user;

    ServerDrawerObj(Terminal *term, Employee *employee);

    int Render(Terminal *term);
    int Layout(Terminal *term, int lx, int ly, int lw, int lh);
};

ServerDrawerObj::ServerDrawerObj(Terminal *term, Employee *employee)
{
    FnTrace("ServerDrawerObj::ServerDrawerObj()");
    user = employee;

    Drawer *d = term->system_data->FirstDrawer();
    while (d != NULL)
    {
        if (((employee == NULL && d->owner_id == 0) ||
             (employee != NULL && d->owner_id == employee->id)) &&
            d->Status() == DRAWER_OPEN && !d->IsServerBank())
            drawers.Add(new DrawerObj(d));
        d = d->next;
    }
}

int ServerDrawerObj::Layout(Terminal *term, int lx, int ly, int lw, int lh)
{
    FnTrace("ServerDrawerObj::Layou()");
    SetRegion(lx, ly, lw, lh);

    int width_left  = w - 10;
    int height_left = h - 46;
    int width       = 120;
    int height      = 80;

    if (width > width_left)
        width = width_left;
    if (height > height_left)
        height = height_left;

    // lay drawer out left to right, top to bottom
    for (ZoneObject *zo = drawers.List(); zo != NULL; zo = zo->next)
    {
        if (width > width_left)
        {
            width_left   = w - 10;
            height_left -= height;
            if (height_left <= 0)
                return 1;  // Ran out of room
        }
        zo->Layout(term, x + w - width_left - 4, y + h - height_left, width, height);
        width_left -= width;
    }
    return 0;
}

int ServerDrawerObj::Render(Terminal *term)
{
    FnTrace("ServerDrawerObj::Render()");
    static const genericChar* unassigned = "UNASSIGNED";

    term->RenderButton(x, y, w, h, ZF_RAISED, IMAGE_SAND);
    const genericChar* name;
    int color;
    if (user)
    {
        name  = user->system_name.Value();
        color = COLOR_BLACK;

        const genericChar* j = user->JobTitle(term);
        term->RenderText(j, x + (w/2), y + 26, COLOR_BLUE, FONT_TIMES_20B,
                      ALIGN_CENTER, w - 8);
    }
    else
    {
        name  = term->Translate(unassigned);
        color = COLOR_RED;
    }

    term->RenderText(name, x + (w/2), y + 6, color, FONT_TIMES_20B,
                  ALIGN_CENTER, w - 8);

    drawers.Render(term);
    return 0;
}


/*********************************************************************
 * DrawerAssignZone Class
 ********************************************************************/

RenderResult DrawerAssignZone::Render(Terminal *term, int update_flag)
{
    FnTrace("DrawerAssignZone::Render()");
    RenderZone(term, NULL, update_flag);
    System *sys = term->system_data;
    Settings *s = &(sys->settings);
    if (update_flag)
    {
        servers.Purge();
        for (Employee *employee = sys->user_db.UserList(); employee != NULL; employee = employee->next)
        {
            if ((employee->CanSettle(s) && sys->labor_db.IsUserOnClock(employee)) ||
                sys->CountDrawersOwned(employee->id) > 0)
            {
                servers.Add(new ServerDrawerObj(term, employee));
            }
        }

        // Create unassigned drawer area
        servers.Add(new ServerDrawerObj(term, NULL));
    }

    servers.LayoutGrid(term, x + border, y + border,
                       w - (border * 2), h - (border * 2));
    servers.Render(term);
    return RENDER_OKAY;
}

SignalResult DrawerAssignZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("DrawerAssignZone::Touch()");
    ZoneObject *so = servers.Find(tx, ty);
    if (so)
    {
        ZoneObject *dob = ((ServerDrawerObj *)so)->drawers.Find(tx, ty);
        if (dob)
            dob->Touch(term, tx, ty);
        else
            MoveDrawers(term, ((ServerDrawerObj *)so)->user);
        return SIGNAL_OKAY;
    }
    return SIGNAL_IGNORED;
}

int DrawerAssignZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    if (update_message & (UPDATE_USERS | UPDATE_DRAWERS))
        return Draw(term, 1);
    return 0;
}

int DrawerAssignZone::MoveDrawers(Terminal *term, Employee *user)
{
    FnTrace("DrawerAssignZone::MoveDrawers()");
    int count = 0;
    ZoneObject *list = servers.List();
    while (list != NULL)
    {
        count += ((ServerDrawerObj *)list)->drawers.CountSelected();
        list = list->next;
    }

    if (count <= 0)
        return 1;

    list = servers.List();
    while (list)
    {
        ZoneObject *zo = ((ServerDrawerObj *)list)->drawers.List();
        for (; zo != NULL; zo = zo->next)
        {
            if (zo->selected)
            {
                zo->selected = 0;
                Drawer *d = ((DrawerObj *)zo)->drawer;
                if (user == NULL)
                    d->ChangeOwner(0);
                else
                    d->ChangeOwner(user->id);
                d->Save();
            }
        }

        list = list->next;
    }
    Draw(term, 1);
    return 0;
}


/*********************************************************************
 * DrawerManageZone Class:
 ********************************************************************/

DrawerManageZone::DrawerManageZone()
{
    current          = NULL;
    report           = NULL;
    mode             = DRAWER_OPEN;
    left_margin      = 128;
    view             = DRAWER_OPEN;
    drawer_list      = NULL;
    check_list       = NULL;
    page             = 0;
    max_pages        = 0;
    media            = 0;
    balances         = 0;
    expenses         = 0;
    spacing          = 1.0;
    drawers_shown    = 0;
    group            = 0;
    drawer_zone_type = DRAWER_ZONE_BALANCE;
}

DrawerManageZone::~DrawerManageZone()
{
    if (report)
        delete report;
}

RenderResult DrawerManageZone::Render(Terminal *term, int update_flag)
{
    FnTrace("DrawerManageZone::Render()");
    DList<Check> checks;  // only used for "All Drawers" report
    System *sys = term->system_data;
    Settings *s = &(sys->settings);
    Archive *archive = term->archive;
    Check *check_list_save = NULL;
    int idx;

    if (drawer_zone_type == DRAWER_ZONE_SELECT)
        archive = NULL;
    LayoutZone::Render(term, update_flag);

    if (update_flag)
    {
        if (update_flag == RENDER_NEW)
        {
            if (archive)
                view = DRAWER_BALANCED;
            else
                view = DRAWER_OPEN;
        }

        if (archive)
        {
            if (archive->loaded == 0)
                archive->LoadPacked(s);
            drawer_list = archive->DrawerList();
            check_list  = archive->CheckList();
        }

        else
        {
            drawer_list = sys->DrawerList();
            check_list  = sys->CheckList();
        }

        if (report)
        {
            delete report;
            report = NULL;
        }
        CreateDrawers(term);
        group = 0;
    }

    term->RenderVLine(x + border + 122, y + border, h - (border * 2), color[0], 1);
    // Render the left side Drawer objects
    ZoneObject *zoneobj = drawers.List();
    if (zoneobj)
    {
        int max_line = (h - border * 2 - 40) / 32;
        int no = drawers.Count();
        if (no > max_line)
        {
            term->RenderZoneText("More Drawers\\(Touch Here)",
                              x + border, y + h - border - 40, 120, 40, COLOR_BLACK, FONT_TIMES_20);
            no = max_line;
        }
        else
            group = 0;

        int dh = (h - border * 2 - 40) / no;
        if (dh > 80)
            dh = 80;

        drawers_shown = no;
        int offset = group * max_line;
        int dy = y + border - 1;
        while (zoneobj)
        {
            if (offset > 0)
            {
                zoneobj->active = 0;
                --offset;
            }
            else if (no > 0)
            {
                zoneobj->active = 1;
                --no;
                zoneobj->Layout(term, x + border - 2, dy, 120, dh);
                dy += dh;
                zoneobj->selected = (zoneobj == current);
                zoneobj->Render(term);
            }
            zoneobj = zoneobj->next;
        }
    }

    // Render the right side report for the currently selected Drawer object
    Drawer *d = NULL;
    if (current)
        d = ((DrawerObj *)current)->drawer;

    // Set up the "All Drawers" case
    if (d && d->serial_number == ALL_DRAWERS)
    {
        // first copy all of the checks over
        int all = term->user->IsSupervisor(s);
        int dstat;
        Drawer *scdrawer = NULL;
        Check *currcheck = check_list;
        while (drawer_list != NULL && currcheck != NULL)
        {
            Check *newcheck = currcheck->Copy(s);
            // set all SubChecks to our virtual drawer
            SubCheck *scheck = newcheck->SubList();
            while (scheck != NULL)
            {
                scdrawer = drawer_list->FindBySerial(scheck->drawer_id);
                if (scdrawer != NULL)
                    dstat = scdrawer->Status();
                else
                    dstat = 0;
                if ((all == 1) ||
                    (view == DRAWER_OPEN && (dstat == DRAWER_OPEN || dstat == DRAWER_PULLED)) ||
                    (view == DRAWER_BALANCED && dstat == DRAWER_BALANCED))
                {
                    scheck->drawer_id = ALL_DRAWERS;
                }
                scheck = scheck->next;
            }
            checks.AddToTail(newcheck);
            currcheck = currcheck->next;
        }
        // save the original check list (we'll reset it later)
        // and use the copied list
        check_list_save = check_list;
        check_list = checks.Head();
        // set the drawer balances and all that
        d->Total(check_list);
    }

    for (idx = 0; idx < MAX_BALANCES; idx++)
        balance_list[idx] = NULL;
    
    // set up balance list
    if (d)
    {
        if (d->Status() == DRAWER_PULLED)
            d->media_balanced = s->media_balanced;

        balances = 0;
        int balidx = 0;
        int balance = tender_order[balidx];
        while (balance > -1)
        {
            if (d->media_balanced & (1<<balance))
            {
                if (balance == TENDER_EMPLOYEE_MEAL)
                {
                    MealInfo *mi = s->MealList();
                    while (mi)
                    {
                        if (balances < MAX_BALANCES)
                        {
                            balance_list[balances++] =
                                d->FindBalance(TENDER_EMPLOYEE_MEAL, mi->id, 1);
                        }
                        mi = mi->next;
                    }
                }
                else if (balance == TENDER_CHARGE_CARD)
                {
                    CreditCardInfo *cc = s->CreditCardList();
                    while (cc)
                    {
                        if (balances < MAX_BALANCES)
                        {
                            balance_list[balances++] =
                                d->FindBalance(TENDER_CHARGE_CARD, cc->id, 1);
                        }
                        cc = cc->next;
                    }
                }
                else if (balance == TENDER_CREDIT_CARD)
                {
                    idx = 0;
                    while (CreditCardValue[idx] > -1)
                    {
                        if (balances < MAX_BALANCES)
                        {
                            balance_list[balances++] =
                                d->FindBalance(TENDER_CREDIT_CARD, CreditCardValue[idx], 1);
                        }
                        idx += 1;
                    }
                }
                else if (balance == TENDER_DEBIT_CARD)
                {
                }
                else if (balance == TENDER_CHARGED_TIP)
                {
                }
                else if (balance == TENDER_DISCOUNT)
                {
                    DiscountInfo *ds = s->DiscountList();
                    while (ds)
                    {
                        if (balances < MAX_BALANCES)
                        {
                            balance_list[balances++] =
                                d->FindBalance(TENDER_DISCOUNT, ds->id, 1);
                        }
                        ds = ds->next;
                    }
                }
                else if (balance == TENDER_COUPON)
                {
                    CouponInfo *cp = s->CouponList();
                    while (cp)
                    {
                        if (balances < MAX_BALANCES &&
                            (s->balance_auto_coupons == 1 ||
                             cp->automatic == 0))
                        {
                            balance_list[balances++] =
                                d->FindBalance(TENDER_COUPON, cp->id, 1);
                        }
                        cp = cp->next;
                    }
                }
                else if (balance == TENDER_COMP)
                {
                    CompInfo *cm = s->CompList();
                    while (cm)
                    {
                        if (balances < MAX_BALANCES)
                        {
                            balance_list[balances++] =
                                d->FindBalance(TENDER_COMP, cm->id, 1);
                        }
                        cm = cm->next;
                    }
                }
                else if (balances < MAX_BALANCES)
                {
                    balance_list[balances++] = d->FindBalance(balance, 0, 1);
                }
            }
            balidx += 1;
            balance = tender_order[balidx];
        }
    }

    genericChar str[256];
    if (d == NULL)
    {
        if (report)
        {
            delete report;
            report = NULL;
        }

        report = new Report;
        if (view == DRAWER_OPEN)
            report->TextC(term->Translate("There Are No Open Drawers For"));
        else
            report->TextC(term->Translate("There Are No Balanced Drawers For"));
        report->NewLine();
        if (archive == NULL)
            strcpy(str, term->Translate("Today"));
        else
        {
            genericChar tm1[64], tm2[64];
            if (archive->fore)
                term->TimeDate(tm1, archive->fore->end_time, TD4);
            else
                strcpy(tm1, term->Translate("System Start"));
            term->TimeDate(tm2, archive->end_time, TD4);
            sprintf(str, "%s  -  %s", tm1, tm2);
        }
        report->TextC(str);

        report->Render(term, this, 0, 0, page, 0, spacing);
        page      = report->page;
        max_pages = report->max_pages;
    }
    else
    {
        mode = d->Status();
        switch (mode)
        {
        default:
            if (report == NULL)
            {
                report = new Report;
                d->MakeReport(term, check_list, report);
            }
            if (report)
            {
                int print = 1;
                if (drawer_zone_type == DRAWER_ZONE_SELECT)
                    print = 0;
                report->Render(term, this, 0, 0, page, print, spacing);
                page      = report->page;
                max_pages = report->max_pages;
            }
            break;

        case DRAWER_PULLED:
            if (report)
            {
                delete report;
                report = NULL;
            }

            int pcolor;
            int per_page = (int) ((size_y - 4) / 2.0);
            if (media >= 0)
                page = media / per_page;
            int yy = 3;
            int balance_count = 0;
            while (balance_count < balances)
            {
                DrawerBalance *db = balance_list[balance_count];
                int p = balance_count / per_page;
                if (p != page || db == NULL)
                {
                    balance_count += 1;
                    continue;
                }

                int dbamount = db->amount;
                int dbentered = db->entered;
                int diff = dbentered - dbamount;
                if (balance_count == media)
                    Background(term, yy - .5, 2.0, IMAGE_LIT_SAND);
                TextL(term, yy, db->Description(s), COLOR_BLACK);
                TextPosR(term, size_x - 17, yy, term->FormatPrice(dbamount), COLOR_BLACK);
                pcolor = COLOR_BLACK;
                if (diff < 0)
                    pcolor = COLOR_RED;
                else if (diff > 0)
                    pcolor = COLOR_BLUE;
                TextR(term, yy, term->FormatPrice(diff), pcolor);
                Entry(term, size_x - 16, yy, 7.5);
                TextPosR(term, size_x - 8.5, yy, term->FormatPrice(dbentered), COLOR_YELLOW);
                yy += 2;
                balance_count += 1;
            }

            int mp = (balances - 1) / per_page;
            if (mp > 0)
                TextL(term, size_y - 1, term->PageNo(page + 1, mp + 1), color[0]);

            TextC(term, 0, term->Translate("Please Enter These Amounts"), color[0]);
            // Add the terminal name
            if (d != NULL)
            {
                Terminal *termlist = term->parent->TermList();
                while (termlist != NULL)
                {
                    if (termlist->host == d->host)
                    {
                        snprintf(str, 256, "%s", termlist->name.Value());
                        TextC(term, 1, str, color[0]);
                        termlist = NULL;
                    }
                    else
                    {
                        termlist = termlist->next;
                    }
                }
            }
            Line(term, 2, color[0]);
            if (d->total_difference < 0)
            {
                strcpy(str, GlobalTranslate("Short"));
                pcolor = COLOR_RED;
            }
            else if (d->total_difference > 0)
            {
                strcpy(str, GlobalTranslate("Over"));
                pcolor = COLOR_BLUE;
            }
            else
            {
                strcpy(str, GlobalTranslate("Balanced"));
                pcolor = COLOR_BLACK;
            }

            TextPosR(term, size_x - 9, yy, str, pcolor, PRINT_BOLD);
            TextR(term, yy, term->FormatPrice(d->total_difference), pcolor, PRINT_BOLD);
            break;
        }
    }

    if (check_list_save != NULL)
        check_list = check_list_save;

    return RENDER_OKAY;
}

SignalResult DrawerManageZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("DrawerManageZone::Signal()");
    static const genericChar* commands[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "00", "-",
        "backspace", "clear", "enter", "pull", "save", "change view",
        "localprint", "reportprint", "merge", "mergeterm", "mergeall", NULL};
    Employee *employee;
    Drawer *drawer;
    DrawerBalance *db;
    int drawer_print = term->GetSettings()->drawer_print;

    employee = term->user;
    if (employee == NULL)
        return SIGNAL_IGNORED;

    drawer = NULL;
    if (current)
        drawer = ((DrawerObj *)current)->drawer;

    db = NULL;
    if (media >= 0 && media < balances)
        db = balance_list[media];
    
    int idx = CompareList(message, commands);
    switch (idx)
    {
    default:
        if (db && mode == DRAWER_PULLED && Abs(db->entered) <= 99999)
        {
            db->entered *= 10;
            db->entered += idx;
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        break;
    case 10: // 00
        if (db && mode == DRAWER_PULLED && Abs(db->entered) <= 9999)
        {
            db->entered *= 100;
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        break;
    case 11: // -
        if (db && mode == DRAWER_PULLED)
        {
            db->entered = -db->entered;
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        break;
    case 12: // backspace
        if (db && mode == DRAWER_PULLED)
        {
            db->entered /= 10;
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        break;
    case 13: // clear
        if (db && mode == DRAWER_PULLED)
        {
            db->entered = 0;
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        break;
    case 14: // enter
        if (drawer && mode == DRAWER_PULLED)
        {
            if (media < 0)
                media = 0;
            else
            {
                ++media;
                if (media >= balances)
                    media = 0;
                drawer->Total(check_list);
                drawer->Save();
            }
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        break;
    case 15: // pull
        page = 0;
        if (drawer && (drawer->serial_number != ALL_DRAWERS))
        {
            media = 0;
            if (mode == DRAWER_OPEN)
            {
                if (drawer_print == DRAWER_PRINT_BOTH || drawer_print == DRAWER_PRINT_PULL)
                    Print(term, RP_PRINT_REPORT);
                drawer->Pull(employee->id);
                Draw(term, 0);
            }
            else if (mode == DRAWER_PULLED)
            {
                if (drawer_print == DRAWER_PRINT_BOTH || drawer_print == DRAWER_PRINT_BALANCE)
                    Print(term, RP_PRINT_REPORT);
                drawer->Balance(employee->id);
                DrawerBalance *drawer_balance = drawer->FindBalance(TENDER_EXPENSE, 0);
                term->system_data->expense_db.SaveEntered(drawer_balance->entered, drawer->serial_number);
                Draw(term, 1);
            }
            else
            {
                if (employee->IsManager(term->GetSettings()))
                    drawer->balance_time.Clear();
                drawer->Save();
                Draw(term, 0);
            }
            return SIGNAL_OKAY;
        }
        break;
    case 16: // save
        if (drawer && mode == DRAWER_PULLED)
        {
            drawer->Balance(employee->id);
            Draw(term, 1);
            return SIGNAL_OKAY;
        }
        break;
    case 17: // change view
        if ( drawer_zone_type != DRAWER_ZONE_SELECT )
        {
            if (view == DRAWER_OPEN)
                view = DRAWER_BALANCED;
            else
                view = DRAWER_OPEN;
            Draw(term, 1);
        }
        else
        {
            Draw(term, 0);
        }
        return SIGNAL_OKAY;
    case 18: // localprint
        Print(term, RP_PRINT_LOCAL);
        return SIGNAL_OKAY;
    case 19: // reportprint
        Print(term, RP_PRINT_REPORT);
        return SIGNAL_OKAY;
    case 20:  // merge
        if (drawer && drawer->MergeServerBanks() == 0)
        {
            CreateDrawers(term);
            ZoneObject *zoneobj = drawers.List();
            while (zoneobj && ((DrawerObj *)zoneobj)->drawer != drawer)
            {
                zoneobj = zoneobj->next;
            }
            current = zoneobj;
            if (report)
            {
                delete report;
                report = NULL;
            }
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        break;
    case 21:
    case 22:  // mergeterm and mergeall
    {
        int mergetype = 0;
        if (idx == 22)
            mergetype = 1;
        if (MergeSystems(term, mergetype) == 0)
        {
            CreateDrawers(term);
            ZoneObject *zoneobj = drawers.List();
            while (zoneobj && ((DrawerObj *)zoneobj)->drawer != drawer)
            {
                zoneobj = zoneobj->next;
            }
            current = zoneobj;
            if (report)
            {
                delete report;
                report = NULL;
            }
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
    }
        break;
    case -1:
        break;
    }
    return SIGNAL_IGNORED;
}

SignalResult DrawerManageZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("DrawerManageZone::Touch()");
    LayoutZone::Touch(term, tx, ty);

    if (tx < (x + 120 + (border * 2)))
    {
        // drawer button in left margin touched
        int count = drawers.Count();
        if (ty > (y + h - 40 - border) && drawers_shown < count)
        {
            ++group;
            if ((drawers_shown * group) >= count)
                group = 0;
            Draw(term, 0);
            return SIGNAL_OKAY;
        }

        ZoneObject *zoneobj = drawers.Find(tx, ty);
        if (zoneobj)
        {
            current = (DrawerObj *)zoneobj;
            if ( drawer_zone_type == DRAWER_ZONE_SELECT )
            {
                DrawerObj *dobj = (DrawerObj *)zoneobj;
                term->expense_drawer = dobj->drawer;
            }
            if (report)
            {
                delete report;
                report = NULL;
            }
            media = TENDER_CASH;
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        return SIGNAL_IGNORED;
    }
    else if ( drawer_zone_type != DRAWER_ZONE_SELECT )
    {
        // report area touched
        if (report)
        {
            if (selected_y <= 3)
                return Keyboard(term, 16, 0);
            else if (selected_y >= (size_y - 3))
                return Keyboard(term, 14, 0);
            else
                Print(term, RP_ASK);
        }
        else
        {
            int per_page = (int) ((size_y - 4.0) / 2.0);
            if (selected_y > (size_y - 2.0))
            {  // turn the page
                page += 1;
                media += per_page;
                if (page > max_pages)
                {
                    page = 0;
                    media = 0;
                }
            }
            else
            {
                media = (int) ((selected_y - 0.5) / 2.0) + (page * per_page) - 1;
            }
            if (media >= (balances + expenses))
                media = -1;
            Draw(term, 0);
        }
        return SIGNAL_OKAY;
    }
    return SIGNAL_IGNORED;
}

SignalResult DrawerManageZone::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("DrawerManageZone::Keyboard()");
    genericChar str[16];

    int new_page = page;
    switch (my_key)
    {
    case 16:  // page_up
        --new_page;
        break;
    case 14:  // page down
        ++new_page;
        break;
    case 13:  // enter
        return Signal(term, "enter");
        break;
    case 8:   // backspace
        return Signal(term, "backspace");
        break;
    default:
        str[0] = (char) my_key;
        str[1] = '\0';
        return Signal(term, str);
        break;
    }

    if (report == NULL)
        return SIGNAL_IGNORED;

    int max_page = report->max_pages;
    if (new_page >= max_page)
        new_page = 0;
    else if (new_page < 0)
        new_page = max_page - 1;

    if (page == new_page)
        return SIGNAL_IGNORED;

    page = new_page;
    Draw(term, 0);
    return SIGNAL_OKAY;
}

int DrawerManageZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    if (update_message & UPDATE_ARCHIVE)
        return Draw(term, RENDER_NEW);
    else if (update_message & UPDATE_SETTINGS)
        return Draw(term, RENDER_NEW);
    else if (update_message & UPDATE_DRAWER)
    {
        if (report)
        {
            delete report;
            report = NULL;
        }
        return Draw(term, 0);
    }
    return 0;
}

int DrawerManageZone::CreateDrawers(Terminal *term)
{
    FnTrace("DrawerManageZone::CreateDrawers()");
    drawers.Purge();
    page = 0;
    Employee *employee = term->user;
    Settings *s = term->GetSettings();
    int all = employee->IsSupervisor(s);
    int count = 0;

    for (Drawer *drawer = drawer_list; drawer != NULL; drawer = drawer->next)
    {
        if (all || drawer->owner_id == employee->id)
        {
            // Server Bank drawers will not have term set, but we need it
            if (drawer->term == NULL)
                drawer->term = term;
            drawer->Total(check_list);
            if ((drawer_zone_type == DRAWER_ZONE_SELECT) || (drawer->IsEmpty() == 0))
            {
                int dstat = drawer->Status();
                if ((all == 1) ||
                    (view == DRAWER_OPEN && (dstat == DRAWER_OPEN || dstat == DRAWER_PULLED)) ||
                    (view == DRAWER_BALANCED && dstat == DRAWER_BALANCED))
                {
                    drawers.Add(new DrawerObj(drawer));
                    count += 1;
                }
            }
        }
    }

    if (count > 1)
    {
        Drawer *alldrawer = new Drawer();
        alldrawer->serial_number = ALL_DRAWERS;
        drawers.AddToHead(new DrawerObj(alldrawer));
    }
    current = (DrawerObj *) drawers.List();
    return 0;
}

int DrawerManageZone::Print(Terminal *term, int print_mode)
{
    FnTrace("DrawerManageZone::Print()");
    if (print_mode == RP_NO_PRINT)
        return 0;

    Employee *employee = term->user;
    if (employee == NULL || current == NULL || report == NULL)
        return 1;

    Printer *p1 = term->FindPrinter(PRINTER_RECEIPT);
    Printer *p2 = term->FindPrinter(PRINTER_REPORT);
    if (p1 == NULL && p2 == NULL)
        return 1;

    if (print_mode == RP_ASK)
    {
        DialogZone *drawer = NewPrintDialog(p1 == p2);
        drawer->target_zone = this;
        term->OpenDialog(drawer);
        return 0;
    }

    Printer *p = p1;
    if ((print_mode == RP_PRINT_REPORT && p2) || p1 == NULL)
        p = p2;

    if (p == NULL)
        return 1;

    report->CreateHeader(term, p, employee);
    report->FormalPrint(p);
    return 0;
}
