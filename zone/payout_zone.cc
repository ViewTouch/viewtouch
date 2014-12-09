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
 * payout_zone.cc - revision 75 (8/10/98)
 * Implementation of payout zone module
 */

#include "payout_zone.hh"
#include "tips.hh"
#include "system.hh"
#include "dialog_zone.hh"
#include "report.hh"
#include "employee.hh"
#include "settings.hh"
#include "manager.hh"
#include "check.hh"
#include "drawer.hh"
#include "archive.hh"
#include <string.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define ERR_BALANCE_CASH1 "All cash drawers and server banks must be"
#define ERR_BALANCE_CASH2 "pulled or turned in before day can be ended"
#define ERR_CLOSE_CHECKS  "All open checks must be closed for end of day"
#define ERR_CC_EXCEPT     "All credit card exceptions must be handled"
#define ERR_LOGOUT        "Other terminals must be free for end of day"
#define ERR_INSUFF_TIME   "%d hours must pass since last end of day"

/**** PayoutZone Class ****/
#define HEADER 4

// Constructor
PayoutZone::PayoutZone()
{
    selected = -1;
    spacing  = 1;
    payout   = -1;
    amount   = 0;
    user_id  = 0;
    page     = 0;
    archive  = NULL;
    tip_db   = NULL;
    report   = NULL;
}

// Destructor
PayoutZone::~PayoutZone()
{
    if (report)
        delete report;
}

// Member Functions
RenderResult PayoutZone::Render(Terminal *term, int update_flag)
{
    System *sys = term->system_data;
    LayoutZone::Render(term, update_flag);
    if (update_flag)
    {
        if (update_flag == RENDER_NEW)
        {
            sys->tip_db.Update(sys);
            archive = NULL;
        }
        if (report)
        {
            delete report;
            report = NULL;
        }
        if (archive)
            tip_db = &(archive->tip_db);
        else
            tip_db = &(sys->tip_db);
        selected = -1;
        payout   = -1;
    }

    if (tip_db == NULL)
        return RENDER_OKAY;

    if (report == NULL)
    {
        report = new Report;
        tip_db->ListReport(term, term->user, report);
    }

    genericChar str[256], price[64];
    int col = color[0];

    // Entries
    int line = HEADER;
    if (payout >= 0)
    {
        Employee *e = sys->user_db.FindByID(user_id);
        term->FormatPrice(price, amount, 1);
        if (e)
            sprintf(str, "Pay out %s to %s", price, e->system_name.Value());
        else
            sprintf(str, "Pay out %s", price);

        TextC(term, ++line, str);
        TextC(term, ++line, "Press any button to continue");
    }
    else
    {
        TextL(term, 2.3, "Employee", col);
        TextC(term, 2.3, "Amount Paid", col);
        TextR(term, 2.3, "Amount Owed", col);
        report->selected_line = selected;
        report->Render(term, this, HEADER-1, 0, page, 0, spacing);
    }

    // Header
    TextC(term, 0, name.Value(), col);
    genericChar t1[32], t2[32];
    TimeInfo timevar;

    if (archive && archive->fore)
    {
        timevar = archive->fore->end_time;
        term->TimeDate(t1, timevar, TD0);
    }
    else if (archive == NULL && sys->ArchiveListEnd())
    {
        timevar = sys->ArchiveListEnd()->end_time;
        term->TimeDate(t1, timevar, TD0);
    }
    else
        strcpy(t1, "System start");

    if (archive)
    {
        timevar = archive->end_time;
        term->TimeDate(t2, timevar, TD0);
    }
    else
        strcpy(t2, "Now");

    sprintf(str, "%s - %s", t1, t2);
    TextC(term, 1, str, COLOR_BLUE);
    return RENDER_OKAY;
}

SignalResult PayoutZone::Signal(Terminal *term, const genericChar* message)
{
    static const genericChar* commands[] = {
        "payout", "next", "prior", "print", "localprint", "reportprint", NULL};

    if (payout >= 0)
    {
        payout = -1;
        Draw(term, 1);
        return SIGNAL_OKAY;
    }

    int idx = CompareList(message, commands);
    switch (idx)
    {
    case 0:  // Payout
        PayoutTips(term);
        return SIGNAL_OKAY;
    case 1:  // Next
        if (archive == NULL)
            break;
        archive = archive->next;
        Draw(term, 1);
        return SIGNAL_OKAY;
    case 2:  // Prior
        if (archive)
        {
            if (archive->fore == NULL)
                return SIGNAL_IGNORED;
            archive = archive->fore;
        }
        else
            archive = term->system_data->ArchiveListEnd();
        Draw(term, 1);
        return SIGNAL_OKAY;
    case 3:  // print
        Print(term, RP_ASK);
        return SIGNAL_OKAY;
    case 4:  // localprint
        Print(term, RP_PRINT_LOCAL);
        return SIGNAL_OKAY;
    case 5:  // reportprint
        Print(term, RP_PRINT_REPORT);
        return SIGNAL_OKAY;
    }
    return SIGNAL_IGNORED;
}

SignalResult PayoutZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("PayoutZone::Touch()");
    if (report == NULL)
        return SIGNAL_IGNORED;

    int new_page = page;
    LayoutZone::Touch(term, tx, ty);
    int line = report->TouchLine(spacing, selected_y);
    if (line == -1)      // header
        --new_page;
    else if (line == -2) // footer
        ++new_page;
    else if (archive == NULL)
    {
        selected = line;
        Draw(term, 0);
    }

    int max_page = report->max_pages;
    if (new_page < 0)
        new_page = max_page - 1;
    if (new_page >= max_page)
        new_page = 0;

    if (page == new_page)
        return SIGNAL_IGNORED;

    page = new_page;
    Draw(term, 0);
    return SIGNAL_OKAY;
}

int PayoutZone::PayoutTips(Terminal *term)
{
    Employee *e = term->user;
    if (e == NULL || e->training || tip_db == NULL || report == NULL)
        return 1;

    System *sys = term->system_data;
    Settings *s = &(sys->settings);
    TipEntry *te = NULL;
    if (e->IsSupervisor(s))
        te = tip_db->FindByRecord(selected, NULL);
    else
        te = tip_db->FindByRecord(selected, e);
    if (te == NULL)
        return 1;

    Drawer *d = term->FindDrawer();
    if (d == NULL)
        return 1;

    amount  = te->amount;
    user_id = te->user_id;
    if (tip_db->PayoutTip(user_id, amount))
        return 1;

    payout = selected;
    if (d->RecordPayment(TENDER_PAID_TIP, amount, e->id, SystemTime, user_id))
        ReportError("Error in recording payment");
    else
        d->Save();

    d->Open();
    Draw(term, 0);

    Printer *p = term->FindPrinter(PRINTER_RECEIPT);
    if (p == NULL)
        return 0;

    Report r;
    if (tip_db->PayoutReceipt(term, e, amount, &r))
        return 1;

    r.Print(p);
    return 0;
}

int PayoutZone::Print(Terminal *term, int print_mode)
{
    if (print_mode == RP_NO_PRINT)
        return 0;

    Employee *e = term->user;
    if (e == NULL || tip_db == NULL)
        return 1;

    Printer *p1 = term->FindPrinter(PRINTER_RECEIPT);
    Printer *p2 = term->FindPrinter(PRINTER_REPORT);
    if (p1 == NULL && p2 == NULL)
        return 1;

    if (print_mode == RP_ASK)
    {
        DialogZone *d = NewPrintDialog(p1 == p2);
        d->target_zone = this;
        term->OpenDialog(d);
        return 0;
    }

    Printer *p = p1;
    if ((print_mode == RP_PRINT_REPORT && p2) || p1 == NULL)
        p = p2;

    if (p == NULL)
        return 1;

    Report r;
    if (tip_db->PaidReport(term, &r))
        return 1;

    r.CreateHeader(term, p, term->user);
    r.FormalPrint(p);
    return 0;
}


/**** EndDayZone Class ****/
// Constructor
EndDayZone::EndDayZone()
{
    flag1 = 0;
    flag2 = 0;
    flag3 = 0;
    flag4 = 0;
    flag5 = 0;
}

// Member Functions
RenderResult EndDayZone::Render(Terminal *term, int update_flag)
{
    LayoutZone::Render(term, update_flag);

    System *sys = term->system_data;
    Settings *s = &(sys->settings);
    Archive *a = sys->ArchiveListEnd();
    genericChar buffer[STRLENGTH];
    int min_day_secs = term->GetSettings()->min_day_length;
    int min_day_hrs = min_day_secs / 60 / 60;

    int line = 0;
    if (a == NULL)
    {
        TextC(term, ++line, term->Translate("This is the first business day"));
        flag3 = 0;
    }
    else
    {
        TextC(term, ++line, term->Translate("This business day started"));
        TextC(term, ++line, term->TimeDate(a->end_time, TD0));
        flag3 = (SecondsElapsed(SystemTime, a->end_time) < min_day_secs);
    }

    flag1 = !sys->AllDrawersPulled();
    flag2 = term->OtherTermsInUse(1);
    flag4 = (!s->always_open && sys->CountOpenChecks() > 0);
    // flag5 = sys->cc_exception_db->HaveOpenCards();
    ++line;

    if (flag1 == 0 && flag2 == 0 && flag3 == 0 && flag4 == 0 && flag5 == 0)
    {
        TextC(term, ++line, term->Translate("You may end the day when ready"),
              COLOR_DK_GREEN);
        return RENDER_OKAY;
    }

    if (flag1)
    {
        TextC(term, ++line, ERR_BALANCE_CASH1);
        TextC(term, ++line, ERR_BALANCE_CASH2);
        ++line;
    }
    if (flag4)
    {
        TextC(term, ++line, ERR_CLOSE_CHECKS);
        ++line;
    }
    if (flag5)
    {
        TextC(term, ++line, ERR_CC_EXCEPT);
        ++line;
    }
    if (flag2)
    {
        TextC(term, ++line, ERR_LOGOUT);
        ++line;
    }
    if (flag3)
    {
        snprintf(buffer, STRLENGTH, ERR_INSUFF_TIME, min_day_hrs);
        TextC(term, ++line, buffer);
        ++line;
    }
    return RENDER_OKAY;
}

SignalResult EndDayZone::Signal(Terminal *term, const genericChar* message)
{
    SimpleDialog *d = NULL;
    char buffer[STRLENGTH];
    SignalResult retval = SIGNAL_IGNORED;
    static const genericChar* commands[] = {"end", "force end", "enddaydone",
                                      "enddayfailed", "cceodnosettle", NULL};
    int idx = CompareList(message, commands);

    if (flag1 || flag2 || flag3 || flag4 || flag5)
    {
        if (idx == 0)
        {
            if (flag1)
                d = new SimpleDialog(ERR_BALANCE_CASH1 "\\" ERR_BALANCE_CASH2);
            else if (flag2)
                d = new SimpleDialog(ERR_LOGOUT);
            else if (flag3)
            {
                int min_day_secs = term->GetSettings()->min_day_length;
                int min_day_hrs = min_day_secs / 60 / 60;
                snprintf(buffer, STRLENGTH, ERR_INSUFF_TIME, min_day_hrs);
                d = new SimpleDialog(buffer);
            }
            else if (flag4)
                d = new SimpleDialog(ERR_CLOSE_CHECKS);
            else if (flag5)
                d = new SimpleDialog(ERR_CC_EXCEPT);
            
            if (d != NULL)
            {
                d->font = FONT_TIMES_24B;
                d->color[0] = COLOR_RED;
                d->force_width = 640;
                d->Button("Okay");
                term->OpenDialog(d);
            }
            
            return SIGNAL_OKAY;
        }
        else
            return SIGNAL_IGNORED;
    }

    switch (idx)
    {
    case 0: // end
        EndOfDay(term, 0);
        retval = SIGNAL_OKAY;
        break;
    case 1: // force end
        EndOfDay(term, 1);
        retval = SIGNAL_OKAY;
        break;
    case 2:  // enddaydone
        term->parent->KillAllDialogs();
        term->Draw(1);
        retval = SIGNAL_OKAY;
        break;
    case 3:  // enddayfailed
        term->parent->KillAllDialogs();
        term->Draw(1);
        d = new SimpleDialog("End of Day Credit Card Processing Failed");
        d->Button("Okay");
        d->Button("End Without Settlement", "cceodnosettle");
        term->OpenDialog(d);
        retval = SIGNAL_OKAY;
        break;
    case 4:  // cceodnosettle
        term->parent->OpenDialog("Ending Day\\\\Please Wait");
        term->eod_processing = EOD_NOSETTLE;
        term->system_data->eod_term = term;
        term->Draw(1);
        break;
    default:
        retval = SIGNAL_IGNORED;
        break;
    }

    return retval;
}

int EndDayZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    if (update_message & UPDATE_MINUTE)
        Draw(term, 0);
    return 0;
}

int EndDayZone::EndOfDay(Terminal *term, int force)
{
    System *sys = term->system_data;
    Archive *a = sys->ArchiveListEnd();

    if (a && SecondsElapsed(SystemTime, a->end_time) < term->GetSettings()->min_day_length)
        return 1;

    if (force == 0)
    {
        // confirm end of day
        SimpleDialog *d = new SimpleDialog("Confirm end of business day:");
        d->Button("End the Day Now", "force end");
        d->Button("Cancel,\\Don't end the Day");
        d->target_zone = this;
        term->OpenDialog(d);
        return 0;
    }

    MasterControl->LogoutKitchenUsers();
    term->parent->OpenDialog("Ending Day\\\\Please Wait");
    term->StoreCheck();
    term->eod_processing = EOD_BEGIN;
    sys->eod_term = term;

    term->Draw(1);

    return 0;
}
