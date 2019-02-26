/*
 * Copyright, ViewTouch Inc., 1995, 1996, 1997, 1998 
  
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
 * report_zone.cc - revision 139 (10/7/98)
 * Implementation of report information display zone
 */

#include <cstring>
#include "image_data.hh"
#include "report_zone.hh"
#include "terminal.hh"
#include "check.hh"
#include "drawer.hh"
#include "sales.hh"
#include "printer.hh"
#include "dialog_zone.hh"
#include "employee.hh"
#include "labor.hh"
#include "settings.hh"
#include "system.hh"
#include "archive.hh"
#include "manager.hh"
#include "labels.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** ReportZone Class ****/
// Constructor
ReportZone::ReportZone()
{
    min_size_x       = 10;
    min_size_y       = 5;
    report_type      = REPORT_SERVER;
    report           = NULL;
    temp_report      = NULL;
    lines_shown      = 0;
    page             = 0;
    columns          = 1;
    print            = RP_PRINT_LOCAL;
    printer_dest     = RP_PRINT_LOCAL;
    period_view      = 0;
    period_fiscal    = NULL;
    spacing          = 1;
    check_disp_num   = 0;
    video_target     = PRINTER_DEFAULT;
    rzstate          = 0;
    printing_to_printer = 0;
}

// Destructor
ReportZone::~ReportZone()
{
    if (report)
        delete report;
    if (temp_report)
        delete temp_report;
}

// Member Functions
RenderResult ReportZone::Render(Terminal *term, int update_flag)
{
    FnTrace("ReportZone::Render()");
    Employee *e = term->user;
    System *sys = term->system_data;
    Settings *s = &(sys->settings);
    Report *r = temp_report;

    // allow no user signin for kitchen display
    if (e == NULL && report_type != REPORT_CHECK)
        return RENDER_OKAY;

    if (r)
    {
        update_flag = 0;
        if (r->is_complete)
        {
            if (report)
                delete report;
            report = r;
            temp_report = NULL;
            if (printing_to_printer)
            {
                Print(term, printer_dest);
                printing_to_printer = 0;
                if (report != NULL)
                    delete report;
                report = NULL;
                temp_report = new Report();
                sys->RoyaltyReport(term, day_start, day_end, term->archive, temp_report, this);
                return RENDER_OKAY;
            }
        }
    }

    if (update_flag)
    {
        if (report)
        {
            delete report;
            report = NULL;
            page = 0;
        }

        if (update_flag == RENDER_NEW)
        {  // set relevant variables to default values
 //   		printf("ReportZone::Render() update_flag == RENDER_NEW\n");
            day_start.Clear();
            day_end.Clear();
            ref  = SystemTime;
            period_view = s->default_report_period;
            page = 0;

            if (term->server == NULL && s->drawer_mode == DRAWER_SERVER)
                term->server = e;
//            printf("ReportZone::Render() drawer_mode == DRAWER_SERVER\n");

//            period_fiscal = &s->sales_start;
//            period_view = SP_DAY;
            period_fiscal = NULL;
            period_view = s->default_report_period;
            if (report_type == REPORT_SALES)
            {
//	    		printf("ReportZone::Render() repoort_type == REPORT_SALES\n");
                period_view = SP_DAY;
//                period_view = s->sales_period;
                period_fiscal = &s->sales_start;
                term->server = NULL;  // sales report defaults to all users
            }
            else if (report_type == REPORT_BALANCE &&
                     s->report_start_midnight == 0)
            {
//	    		printf("ReportZone::Render() report_type == REPORT_BALANCE\n");
                period_view = SP_DAY;
                period_fiscal = &s->sales_start;
            }
            else if (report_type == REPORT_SERVERLABOR)
            {
//                printf("ReportZone::Render() report_type == REPORT_SERVERLABOR\n");
                period_view = s->labor_period;
                period_fiscal = &s->labor_start;
            }
            else if (report_type == REPORT_DEPOSIT &&
                     s->report_start_midnight == 0)
            {
//                printf("ReportZone::Render() report_type == REPORT_DEPOSIT\n");
                period_view = s->default_report_period;
                period_fiscal = &s->sales_start;
            }
            else if (report_type == REPORT_ROYALTY)
            {
//                printf("ReportZone::Render() report_type == REPORT_ROYALTY\n");
                period_view = SP_MONTH;
                period_fiscal = NULL;
            }
            else if (report_type == REPORT_AUDITING)
            {
//                printf("ReportZone::Render() report_type == REPORT_AUDITING\n");
                period_view = SP_DAY;
                period_fiscal = NULL;
            }
        }

        Archive *a = term->archive;
        if (a)
        {
            day_start = a->start_time;
            day_end   = a->end_time;
        }
        else
        {
            if (sys->ArchiveListEnd())
                day_start = sys->ArchiveListEnd()->end_time;
            else
                day_start.Clear();
            day_end = SystemTime;
        }

        // calculate the start and end times
        if (period_view != SP_NONE)
            s->SetPeriod(ref, day_start, day_end, period_view, period_fiscal);

        TimeInfo user_start;
        TimeInfo user_end;
        user_start = day_start;
        user_end   = day_end;

        WorkEntry *work_entry = sys->labor_db.CurrentWorkEntry(term->server);
        if (work_entry)
        {
            user_start = work_entry->start;
            user_end   = SystemTime;
        }

        Drawer *drawer_list = sys->DrawerList();
        Check  *check_list  = sys->CheckList();
        if (a)
        {
            if (a->loaded == 0)
                a->LoadPacked(s);
            check_list  = a->CheckList();
            drawer_list = a->DrawerList();
        }

        Drawer *d = NULL;
        if (a == NULL)
        {
            if (term->server)
                d = drawer_list->FindByOwner(term->server, DRAWER_OPEN);
            else
                d = term->FindDrawer();
        }

        temp_report = new Report;
        switch (report_type)
        {
        case REPORT_DRAWER:
            if (term->server == NULL)
                sys->DrawerSummaryReport(term, drawer_list, check_list, temp_report);
            else if (d)
                d->MakeReport(term, check_list, temp_report);
            break;
        case REPORT_CLOSEDCHECK:
            sys->ClosedCheckReport(term, day_start, day_end, term->server, temp_report);
            break;
        case REPORT_SERVERLABOR:
            sys->labor_db.ServerLaborReport(term, e, day_start, day_end, temp_report);
            break;
        case REPORT_CHECK:
            DisplayCheckReport(term, temp_report);
            break;
        case REPORT_SERVER:
            sys->ServerReport(term, day_start, day_end, term->server, temp_report);
            break;
        case REPORT_SALES:
            sys->SalesMixReport(term, day_start, day_end, term->server, temp_report);
            break;
        case REPORT_BALANCE:
            if (period_view != SP_DAY)
                sys->BalanceReport(term, day_start, day_end, temp_report);
            else
                sys->ShiftBalanceReport(term, ref, temp_report);
            break;
        case REPORT_DEPOSIT:
            if (period_view != SP_NONE)
                sys->DepositReport(term, day_start, day_end, NULL, temp_report);
            else
                sys->DepositReport(term, day_start, day_end, term->archive, temp_report);
            break;
        case REPORT_COMPEXCEPTION:
            sys->ItemExceptionReport(term, day_start, day_end, 1, term->server, temp_report);
            break;
        case REPORT_VOIDEXCEPTION:
            sys->ItemExceptionReport(term, day_start, day_end, 2, term->server, temp_report);
            break;
        case REPORT_TABLEEXCEPTION:
            sys->TableExceptionReport(term, day_start, day_end, term->server, temp_report);
            break;
        case REPORT_REBUILDEXCEPTION:
            sys->RebuildExceptionReport(term, day_start, day_end, term->server, temp_report);
            break;
        case REPORT_CUSTOMERDETAIL:
            sys->CustomerDetailReport(term, e, temp_report);
            break;
        case REPORT_EXPENSES:
            sys->ExpenseReport(term, day_start, day_end, NULL, temp_report, this);
            break;
        case REPORT_ROYALTY:
            sys->RoyaltyReport(term, day_start, day_end, term->archive, temp_report, this);
            break;
        case REPORT_AUDITING:
            sys->AuditingReport(term, day_start, day_end, term->archive, temp_report, this);
            break;
        case REPORT_CREDITCARD:
            sys->CreditCardReport(term, day_start, day_end, term->archive, temp_report, this);
            break;
        }
        
        //ref = day_start;

        if (temp_report && temp_report->is_complete)
        {
            report = temp_report;
            temp_report = NULL;
        }
    }

    LayoutZone::Render(term, update_flag);
    int hs = 0;
    if (name.size() > 0)
    {
        hs = 1;
        TextC(term, 0, name.Value(), color[0]);
    }

    if (report)
    {
        report->Render(term, this, hs, 0, page, print, (Flt) spacing);
        page = report->page;
    }
    else
    {
        TextC(term, 4, "Working...", color[0]);
    }
 
    return RENDER_OKAY;
}

int ReportZone::State(Terminal *term)
{
    FnTrace("ReportZone::State()");
    return rzstate;
}

RenderResult ReportZone::DisplayCheckReport(Terminal *term, Report *disp_report)
{
    FnTrace("ReportZone::DisplayCheckReport()");
    Settings *settings = term->GetSettings();
    rzstate = 0;
    if (check_disp_num)
    {
        // This is only for the Kitchen Video reports.
        Check *disp_check = GetDisplayCheck(term);
        if (disp_check != NULL)
        {
            if (disp_check->check_state & ORDER_MADE)
                rzstate = 1;
            else if (disp_check->check_state & ORDER_SENT && disp_check->made_time.IsSet())
                rzstate = 2;
            int display_flags = CHECK_DISPLAY_ALL;
            if (video_target != PRINTER_DEFAULT)
                display_flags =~ CHECK_DISPLAY_CASH;  //Remove cash info
            term->curr_font_id = font;
            ColumnSpacing(term, 1);  // just to set font_width
            term->curr_font_width = font_width;
            if (settings->kv_print_method == KV_PRINT_UNMATCHED)
                disp_check->MakeReport(term, disp_report, display_flags, video_target, this);
            else
                disp_check->PrintWorkOrder(term, disp_report, video_target, 0, this);
            term->curr_font_id = -1;
            term->curr_font_width = -1;
        }
        else
        {
            disp_report->update_flag = UPDATE_CHECKS;
            disp_report->TextC(term->Translate("No Check Selected"));
        }
    }
    else if (term->check)
    {
        // This is the non-Kitchen Video report when we have a check.
        term->check->MakeReport(term, disp_report, CHECK_DISPLAY_ALL, video_target);
    }
    else
    {
        disp_report->update_flag = UPDATE_CHECKS;
        disp_report->TextC(term->Translate("No Check Selected"));
    }
    return RENDER_OKAY;
}

/****
 * IsKitchenCheck:  For Kitchen Video, we only want to display checks that have
 *   been closed, but have not yet been served.
 ****/
int ReportZone::IsKitchenCheck(Terminal *term, Check *check)
{
    FnTrace("ReportZone::IsKitchenCheck()");
    int retval = 1;

    // I don't like huge conditionals ("if (a && b || (c || d)...)"),
    // so I'm going to break it out into an if-elsif block instead.
    if (check == NULL)
        retval = 0;
    else if (check->Status() == CHECK_VOIDED)
        retval = 0;
    else if (check->check_state < ORDER_FINAL || check->check_state >= ORDER_SERVED)
        retval = 0;
    else if (!ShowCheck(term, check))
        retval = 0;

    return retval;
}

/****
 * ShowCheck:  Process through the subchecks and orders.  If there are no items
 *  to be displayed on this report_zone's video target, return 0, otherwise
 *  return 1.  This is used to determine whether a check should be sent to the
 *  kitchen.
 ****/
int ReportZone::ShowCheck(Terminal *term, Check *check)
{
    FnTrace("ReportZone::ShowCheck()");
    if (video_target == PRINTER_DEFAULT)
        return 1;  // always show everything on the default
    Settings *settings = term->GetSettings();
    int show = 0;
    int vtarget;
    Order *order;
    SubCheck *scheck = check->SubList();
    while (!show && (scheck != NULL))
    {
        order = scheck->OrderList();
        while (!show && (order != NULL))
        {
            vtarget = order->VideoTarget(settings);
            if (vtarget == video_target)
                show = 1;
            order = order->next;
        }
        scheck = scheck->next;
    }
    return show;
}

/****
 * NextCheck:  Gets the next check in the direction determined by sort_order.
 ****/
Check *ReportZone::NextCheck(Check *check, int sort_order)
{
    FnTrace("ReportZone::NextCheck()");
    Check *retcheck = NULL;
    if (sort_order == CHECK_ORDER_OLDNEW)
        retcheck = check->next;
    else
        retcheck = check->fore;
    return retcheck;
}

/****
 * GetDisplayCheck:  For kitchen video, we want to know which check to display.
 *   Search through the checks based on sort order.  Returns pointer to
 *   the check object to display, or NULL if no matching checks are found.
 ****/
Check *ReportZone::GetDisplayCheck(Terminal *term)
{
    FnTrace("ReportZone::GetDisplayCheck()");
    Check *checklist = NULL;
    Check *disp_check = NULL;
    int counter = 0;

    if (term->sortorder == CHECK_ORDER_NEWOLD)
        checklist = term->system_data->CheckListEnd();
    else
        checklist = term->system_data->CheckList();

    if (checklist)
    {
        while ((counter < check_disp_num) && (checklist != NULL))
        {
            if (IsKitchenCheck(term, checklist))
            {
                disp_check = checklist;
                counter += 1;
            }
            checklist = NextCheck(checklist, term->sortorder);
        }
        // verify we have a check we want
        if (!IsKitchenCheck(term, disp_check) ||
            (counter < check_disp_num))
            disp_check = NULL;
    }

    if (disp_check)
    {
        disp_check->checknum = check_disp_num;
        if (disp_check->check_state == 0)
        {
            disp_check->chef_time.Set();
            disp_check->check_state = ORDER_SENT;
        }
    }
    return disp_check;
}

/****
 * GetCheckByNum:  Search through the checks, returning NULL or whichever check's
 *   checknum value matches the current ReportZone's check_disp_num.
 * //FIX BAK-->This feels like a kludge.  It seems more sensible to associated the
 * check directly with the ReportZone, say as a public member.  But I'm worried that
 * the check will at some point be deleted out from under us, causing a crash later.
 * This method does have the virtue of being totally reliable.  BUT: when I
 * understand the code better, assuming there's no danger of having the check
 * deleted at a bad time, it would be MUCH faster to use the public member method.
 ****/
Check *ReportZone::GetCheckByNum(Terminal *term)
{
    FnTrace("ReportZone::GetCheckByNum()");
    Check *checkptr = term->system_data->CheckList();
    Check *retcheck = NULL;
    // Steve McConnell would probably hate this:  stop when we have no more
    // checks to look at or when we have a return value
    while ((checkptr != NULL) && (retcheck == NULL))
    {
        if (check_disp_num == checkptr->checknum)
            retcheck = checkptr;
        checkptr = checkptr->next;
    }
    return retcheck;
}

/****
 * UndoRecentCheck:
 ****/
int ReportZone::UndoRecentCheck(Terminal *term)
{
    FnTrace("ReportZone::UndoRecentCheck()");
    int retval = 0;

    if (term->same_signal < 1)
    {
        term->same_signal = 1;
        Check *currcheck = term->system_data->CheckListEnd();
        Check *lastcheck = NULL;
        TimeInfo lasttime = term->system_data->start;
        lasttime.AdjustYears(-1);
        
        while (currcheck != NULL)
        {
            if (currcheck->check_state == ORDER_SERVED &&
                currcheck->made_time > lasttime)
            {
                lastcheck = currcheck;
                lasttime = currcheck->made_time;
            }
            currcheck = currcheck->fore;
        }
        if (lastcheck != NULL)
        {
            lastcheck->check_state = ORDER_SENT;
            lastcheck->undo = 1;
            lastcheck->ClearOrderStatus(NULL, ORDER_SHOWN);
            term->Draw(1);
        }
    }
    return retval;
}

/****
 * AdjustPeriod:  adjust should be either 1 or -1.
 ****/
int AdjustPeriod(TimeInfo &ref, int period, int adjust)
{
    FnTrace("AdjustPeriod()");

    // verify adjust has a sane value
    adjust = (adjust >= 0) ? 1 : -1;

    switch (period)
    {
    case SP_DAY:
        ref += date::days(1 * adjust);
        break;
    case SP_WEEK:
        ref += date::days(7 * adjust);
        break;
    case SP_2WEEKS:
        ref += date::days(14 * adjust);
        break;
    case SP_4WEEKS:
        ref += date::days(28 * adjust);
        break;
    case SP_MONTH:
        ref += date::months(1 * adjust);
        break;
    case SP_HALF_MONTH:
        ref.half_month_jump(adjust, 1, 15);
        break;
    case SP_HM_11:
        ref.half_month_jump(adjust, 11, 26);
        break;
    case SP_QUARTER:
        ref += date::months(3 * adjust);
        break;
    case SP_YTD:
        ref.AdjustYears(1 * adjust);
        break;
    }

    return 0;
}

SignalResult ReportZone::Signal(Terminal *t, const genericChar* message)
{
    FnTrace("ReportZone::Signal()");
    static const genericChar* commands[] = {
        "next", "prior", "print", "localprint", "reportprint",
        "day period", "sales period", "labor period", "month period",
        "nextperiod", "sortby ", "undo", "ccinit", "cctotals", "cctotals ",
        "ccdetails", "ccsettle", "ccsettle2 ", "ccclearsaf", "ccsafdetails",
        "ccsafdone", "ccsettledone", "ccinitdone", "cctotalsdone",
        "ccdetailsdone", "ccrefund", "ccvoids", "ccrefunds",
        "ccexceptions", "ccfinish", "ccfinish2 ", "ccfinish3 ",
        "ccprocessed", "ccrefundamount ", "ccvoidttid ", 
	"zero captured tips", "bump", NULL};

    Employee         *e = t->user;
    System           *sys = t->system_data;
    Settings         *s = &sys->settings;
    TenKeyDialog     *tkdialog = NULL;
    CreditCardDialog *ccdialog = NULL;
    GetTextDialog    *gtdialog = NULL;
    const char* batchnum = NULL;

    if (e == NULL)
        return SIGNAL_IGNORED;

    int idx = CompareListN(commands, message);
    switch (idx)
    {
    case 0:  // next
        if (report_type == REPORT_CREDITCARD)
        {
            switch (sys->cc_report_type)
            {
            case CC_REPORT_BATCH:
                sys->cc_settle_results->Next(t);
                break;
            case CC_REPORT_INIT:
                sys->cc_init_results->Next(t);
                break;
            case CC_REPORT_SAF:
                sys->cc_saf_details_results->Next(t);
                break;
            }
        }
	else if (t->page && t->page->IsKitchen() && t->page->ZoneList()) 
	{
	    // highlight next report zone with a check
	    Zone *zcur = t->active_zone;
	    Zone *z = zcur;
	    while (1)
	    {
	    	if (z == NULL)
		    z = t->page->ZoneList();    // (re)start at top of list
		else
		    z = z->next;
		if (z == zcur)
		    break;
	   	if (z && z->Type() == ZONE_REPORT && 
				((ReportZone *)z)->GetDisplayCheck(t))
		{
		    t->active_zone = z;
		    Draw(t, 1);
		    return SIGNAL_END;
		}
	    }
	}
        else
        {
            AdjustPeriod(ref, period_view, 1);
            if (t->archive)
                t->archive = t->archive->next;
            else if (t->archive == NULL)
                t->archive = sys->ArchiveList();
        }
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 1:  // prior
        if (report_type == REPORT_CREDITCARD)
        {
            switch (sys->cc_report_type)
            {
            case CC_REPORT_BATCH:
                sys->cc_settle_results->Fore(t);
                break;
            case CC_REPORT_INIT:
                sys->cc_init_results->Fore(t);
                break;
            case CC_REPORT_SAF:
                sys->cc_saf_details_results->Fore(t);
                break;
            }
        }
	else if (t->page && t->page->IsKitchen() && t->page->ZoneList()) 
	{
	    // highlight previous report zone with a check
	    Zone *zcur = t->active_zone;
	    Zone *z = zcur;
	    while (1)
	    {
	    	if (z == NULL)
		    z = t->page->ZoneListEnd();    // (re)start at end of list
		else
		    z = z->fore;
		if (z == zcur)
		    break;
	   	if (z && z->Type() == ZONE_REPORT && 
				((ReportZone *)z)->GetDisplayCheck(t))
		{
		    t->active_zone = z;
		    Draw(t, 1);
		    return SIGNAL_END;
		}
	    }
	}
        else
        {
            AdjustPeriod(ref, period_view, -1);
            if (t->archive)
                t->archive = t->archive->fore;
            else if (t->archive == NULL)
                t->archive = sys->ArchiveListEnd();
        }
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 2:  // print
        Print(t, print);
        return SIGNAL_OKAY;
    case 3:  // localprint
        Print(t, RP_PRINT_LOCAL);
        return SIGNAL_OKAY;
    case 4:  // reportprint
        Print(t, RP_PRINT_REPORT);
        return SIGNAL_OKAY;
    case 5:  // day period
      	printf("report_zone : day_period: \n");
        period_view = SP_DAY;
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 6:  // sales period
      	printf("report_zone : sales_period\n");
        period_view = s->sales_period;
        period_fiscal = &s->sales_start;
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 7:  // labor period
        period_view = s->labor_period;
        period_fiscal = &s->labor_start;
        printf("report : labor_period: view = %d; fiscal=%d/%d/%d\n", period_view, period_fiscal->Month(), period_fiscal->Day(), period_fiscal->Year());
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 8:  // month period
        period_view = SP_MONTH;
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 9:  // nextperiod
      	printf("report_zone : nextperiod\n");
        period_view = NextValue(period_view, ReportPeriodValue);
        if (period_view == SP_NONE)
        {
            period_view = SP_DAY;
        }
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 10:  // sortby
        t->system_data->report_sort_method = atoi(&message[6]);
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 11: // undo
        if (report_type == REPORT_CHECK)
            UndoRecentCheck(t);
        return SIGNAL_OKAY;
    case 12: // ccinit
        if (e->training == 0)
        {
            sys->cc_report_type = CC_REPORT_INIT;
            t->CC_Init();
        }
        return SIGNAL_OKAY;
    case 13: // cctotals
        t->cc_totals.Clear();
        sys->cc_report_type = CC_REPORT_TOTALS;
        t->CC_Totals();
        return SIGNAL_OKAY;
    case 14:  // cctotals with value
        if (strcmp(&message[9], "fetch") == 0)
        {
            tkdialog = new TenKeyDialog("Enter batch number", "cctotals", 0, 0);
            tkdialog->target_zone = this;
            t->OpenDialog(tkdialog);
        }
        else
        {
            t->cc_totals.Clear();
            sys->cc_report_type = CC_REPORT_TOTALS;
            if (strcmp(&message[9], "0") == 0)
                t->CC_Totals();
            else
                t->CC_Totals(&message[9]);
        }
        return SIGNAL_OKAY;
    case 15: // ccdetails
        sys->cc_report_type = CC_REPORT_DETAILS;
        t->CC_Details();
        return SIGNAL_OKAY;
    case 16: // ccsettle
        gtdialog = new GetTextDialog("Enter Batch Number (leave empty to settle last batch)",
                                     "ccsettle2");
        t->OpenDialog(gtdialog);
        return SIGNAL_OKAY;
    case 17: // ccsettle2
        if (e->training == 0 && e->IsManager(s))
        {
            sys->non_eod_settle = 1;
            if (strlen(&message[10]) > 0)
                batchnum = &message[10];
            else
                batchnum = NULL;
            if (t->CC_Settle(batchnum) >= 0)
            {
                sys->non_eod_settle = 0;
                return SIGNAL_IGNORED;
            }
        }
        return SIGNAL_OKAY;
    case 18: // ccclearsaf
        if (e->training == 0 && e->IsManager(s))
        {
            if (t->CC_ClearSAF() >= 0)
                return SIGNAL_IGNORED;
        }
        return SIGNAL_OKAY;
    case 19: // ccsafdetails
        t->CC_SAFDetails();
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 20: // ccsafdone
        sys->cc_report_type = CC_REPORT_SAF;
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 21: // ccsettledone
        sys->cc_report_type = CC_REPORT_BATCH;
        sys->non_eod_settle = 0;
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 22: // ccinitdone
        sys->cc_report_type = CC_REPORT_INIT;
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 23: // cctotalsdone
        sys->cc_report_type = CC_REPORT_TOTALS;
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 24: // ccdetailsdone
        sys->cc_report_type = CC_REPORT_DETAILS;
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 25: // ccrefund
        tkdialog = new TenKeyDialog("Enter Amount of Refund", "ccrefundamount", 0, 1);
        tkdialog->target_zone = this;
        t->OpenDialog(tkdialog);
        return SIGNAL_OKAY;
    case 26: // ccvoids
        sys->cc_report_type = CC_REPORT_VOIDS;
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 27: // ccrefunds
        sys->cc_report_type = CC_REPORT_REFUNDS;
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 28: // ccexceptions
        sys->cc_report_type = CC_REPORT_EXCEPTS;
        Draw(t, 1);
        return SIGNAL_OKAY;
    case 29: // ccfinish
        if (t->GetSettings()->authorize_method == CCAUTH_MAINSTREET)
            gtdialog = new GetTextDialog("Enter TTID Number", "ccfinish2");
        else
            gtdialog = new GetTextDialog("Enter Authorization Code", "ccfinish2");
        t->OpenDialog(gtdialog);
        return SIGNAL_OKAY;
    case 30: // ccfinish2
        if (t->credit == NULL)
        {
            t->credit = new Credit();
            if (t->GetSettings()->authorize_method == CCAUTH_MAINSTREET)
                t->credit->SetTTID(atol(&message[10]));
            else
                t->credit->SetAuth(&message[10]);
            tkdialog = new TenKeyDialog("Enter the Amount", "ccfinish3", 0, 1);
            t->OpenDialog(tkdialog);
        }
        return SIGNAL_OKAY;
    case 31: // ccfinish3
        if (t->credit != NULL)
        {
            t->credit->Amount(atoi(&message[10]));
            t->CC_GetFinalApproval();
        }
        return SIGNAL_OKAY;
    case 32: // ccprocessed
        if (t->credit != NULL && t->credit->IsRefunded(1) == 0)
        {
            sys->cc_exception_db->Add(t, t->credit);
            t->credit = NULL;
            sys->cc_finish = sys->cc_exception_db->CreditListEnd();
            sys->cc_report_type = CC_REPORT_FINISH;
            Draw(t, 1);
        }
        return SIGNAL_OKAY;
    case 33: // ccrefundamount
        // got refund amount for Credit Card Report.
        t->credit = NULL;
        t->auth_amount = atoi(&message[15]);
        t->auth_action = AUTH_REFUND;
        t->auth_message = REFUND_MSG;
        ccdialog = new CreditCardDialog(t);
        t->NextDialog(ccdialog);
        return SIGNAL_OKAY;
    case 34: // ccvoidttid
        if (debug_mode)
        {
            int ttid = atoi(&message[11]);
            t->credit = new Credit();
            t->credit->SetTTID((long)ttid);
            t->CC_GetVoid();
        }
        return SIGNAL_OKAY;
    case 35: // zero captured tips
        sys->ClearCapturedTips(day_start, day_end, t->archive);
        Draw(t, 1);
	return SIGNAL_OKAY;
    case 36: // bump active check
        if (check_disp_num && this == t->active_zone)
            return ToggleCheckReport(t);
        break;
    default:
        if (strncmp(message, "search ", 7) == 0)
        {
            e = sys->user_db.NameSearch(&message[7], NULL);
            if (e)
            {
                t->server = e;
                Draw(t, 1);
            }
            return SIGNAL_OKAY;
        }
        else if (strncmp(message, "nextsearch ", 11) == 0)
        {
            e = sys->user_db.NameSearch(&message[11], t->server);
            if (e)
            {
                t->server = e;
                Draw(t, 1);
            }
            return SIGNAL_OKAY;
        }
        return SIGNAL_IGNORED;
    }

    return SIGNAL_IGNORED;
}

SignalResult ReportZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("ReportZone::Touch()");
    if (report == NULL)
        return SIGNAL_IGNORED;

    int new_page = page;
    LayoutZone::Touch(term, tx, ty);
    if (selected_y <= 3.0)
    {
        --new_page;
    }
    else if (selected_y >= (size_y - 3.0))
    {
        ++new_page;
    }
    else if (check_disp_num)
    {
        return ToggleCheckReport(term);
    }
    else
    {
        if (Print(term, print))
            return SIGNAL_IGNORED;
        return SIGNAL_OKAY;
    }

    int max_page = report->max_pages;
    if (new_page >= max_page)
        new_page = 0;
    else if (new_page < 0)
        new_page = max_page - 1;

    if (new_page == page)
        return SIGNAL_IGNORED;

    page = new_page;
    Draw(term, 0);
    return SIGNAL_OKAY;
}

SignalResult ReportZone::Mouse(Terminal *term, int action, int mx, int my)
{
    FnTrace("ReportZone::Mouse()");
    if (!(action & MOUSE_PRESS) || report == NULL)
        return SIGNAL_IGNORED;

    int new_page = page;
    LayoutZone::Touch(term, mx, my);
    if (selected_y <= 3.0)
    {
        if (action & MOUSE_LEFT)
            --new_page;
        else if (action & MOUSE_RIGHT)
            ++new_page;
    }
    else if (selected_y >= (size_y - 3.0))
    {
        if (action & MOUSE_LEFT)
            ++new_page;
        else if (action & MOUSE_RIGHT)
            --new_page;
    }
    else if (check_disp_num)
    {
        return ToggleCheckReport(term);
    }
    else
    {
        if (Print(term, print))
            return SIGNAL_IGNORED;
        return SIGNAL_OKAY;
    }

    int max_page = report->max_pages;
    if (new_page >= max_page)
        new_page = 0;
    else if (new_page < 0)
        new_page = max_page - 1;

    if (new_page == page)
        return SIGNAL_IGNORED;

    page = new_page;
    Draw(term, 0);
    return SIGNAL_OKAY;
}

SignalResult ReportZone::ToggleCheckReport(Terminal *term)
{
    FnTrace("ReportZone::ToggleCheckReport()");
    //toggle a check done, reusing ORDER_yada states because they have exactly
    // what we want.
    Check *reportcheck = GetDisplayCheck(term);
    if ( reportcheck )
    {
        if ( reportcheck->check_state < ORDER_MADE )
        {
            // first toggle cooked
            reportcheck->check_state = ORDER_MADE;
            reportcheck->made_time.Set();  //FIX BAK-->should be current time
        }
        else
        {
            // then toggle served to remove from Kitchen Video
            reportcheck->check_state = ORDER_SERVED;
            reportcheck->flags |= CF_SHOWN;
            reportcheck->SetOrderStatus(NULL, ORDER_SHOWN);
        }
        update = 1;
        reportcheck->Save();
        term->Draw(UPDATE_CHECKS | UPDATE_ORDERS | UPDATE_ORDER_SELECT | UPDATE_PAYMENTS);
        return SIGNAL_OKAY;
    }
    else
    {
        return SIGNAL_IGNORED;
    }
}

SignalResult ReportZone::Keyboard(Terminal *t, int my_key, int state)
{
    FnTrace("ReportZone::Keyboard()");
    if (report == NULL)
        return SIGNAL_IGNORED;

    // automatically accept check number (in ascii) as keyboard shortcut to bump
    if (my_key == check_disp_num + '0')
	return ToggleCheckReport(t);

    int new_page = page;
    switch (my_key)
    {
    case 16:  // page up
        --new_page;
        break;
    case 14:  // page down
        ++new_page;
        break;
    case 118:  // v
        if (debug_mode)
        {
            TenKeyDialog *tk = new TenKeyDialog("Enter TTID", "ccvoidttid", 0);
            t->OpenDialog(tk);
            return SIGNAL_TERMINATE;
        }
        break;
    default:
        return SIGNAL_IGNORED;
    }

    int max_page = report->max_pages;
    if (new_page >= max_page)
        new_page = 0;
    else if (new_page < 0)
        new_page = max_page - 1;

    if (new_page == page)
        return SIGNAL_IGNORED;

    page = new_page;
    Draw(t, 0);
    return SIGNAL_OKAY;
}

int ReportZone::Update(Terminal *t, int update_message, const genericChar* value)
{
    FnTrace("ReportZone::Update()");
    if ((update_message & UPDATE_REPORT) && temp_report)
    {
        Draw(t, 0);
        return 0;
    }
    else if ((update_message & UPDATE_BLINK) && report_type == REPORT_CHECK)
    {
        Draw(t, 1);
        return 0;
    }

    Report *r = report;
    if (r == NULL)
        return 0;

    if ((update_message & r->update_flag) && r->is_complete)
        return Draw(t, 1);

    // FIX - obsolete - should be part of r->update_flag
    switch (report_type)
    {
    case REPORT_AUDITING:
    case REPORT_BALANCE:
        if (update_message & UPDATE_SETTINGS)
            return Draw(t, 1);
        break;
    case REPORT_SALES:
        if ((update_message & UPDATE_SETTINGS) && value)
        {
            int sw = atoi(value);
            if (sw == SWITCH_SHOW_FAMILY || sw == SWITCH_SHOW_MODIFIERS)
                return Draw(t, 1);
        }
        break;
    }
    return 0;
}

int ReportZone::Print(Terminal *t, int print_mode)
{
    FnTrace("ReportZone::Print()");
    if (print_mode == RP_NO_PRINT)
        return 0;

    Employee *e = t->user;
    if (e == NULL || report == NULL)
        return 1;

    Printer *p1 = t->FindPrinter(PRINTER_RECEIPT);
    Printer *p2 = t->FindPrinter(PRINTER_REPORT);
    if (p1 == NULL && p2 == NULL)
        return 1;

    // If we have RP_ASK and there are two different printers, ask.
    // If there's only one printer, then don't bother asking.
    if (print_mode == RP_ASK && p1 && p2 && (p1 != p2))
    {
        DialogZone *d = NewPrintDialog(p1 == p2);
        d->target_zone = this;
        t->OpenDialog(d);
        return 0;
    }
    else
        printer_dest = print_mode;

    Printer *p = p1;
    if ((print_mode == RP_PRINT_REPORT && p2) || p1 == NULL)
        p = p2;

    if (p == NULL)
        return 1;

    if (report_type == REPORT_ROYALTY && printing_to_printer == 0)
    {
        System *sys = t->system_data;
        printing_to_printer = 1;
        if (report != NULL)
        {
            delete report;
            report = NULL;
        }
        if (temp_report != NULL)
        {
            delete temp_report;
            temp_report = NULL;
        }
        temp_report = new Report;
        temp_report->max_width = p->MaxWidth();
        temp_report->destination = RP_DEST_PRINTER;
        sys->RoyaltyReport(t, day_start, day_end, t->archive, temp_report, this);
        return 0;
    }

    if (t->GetSettings()->print_report_header)
        report->CreateHeader(t, p, e);
    report->FormalPrint(p);
    return 0;
}


/**** ReadZone Class ****/
ReadZone::ReadZone()
{
    loaded = 0;
    page = 0;
}

RenderResult ReadZone::Render(Terminal *t, int update_flag)
{
    FnTrace("ReadZone::Render()");
    if (update_flag)
    {
        loaded = 0;
        page = 0;
    }

    LayoutZone::Render(t, update_flag);
    if (loaded == 0 && filename.size() > 0)
    {
        loaded = 1;
        report.Clear();
        if (report.Load(filename.Value(), color[0]) != 0)
        {
            return RENDER_ERROR;
        }
    }

    int hs = 0;
    if (name.size() > 0)
    {
        TextC(t, 0, name.Value(), color[0]);
        hs = 1;
    }

    if(report.Render(t, this, hs, 0, page, 0) == 0)
    {
        return RENDER_OKAY;
    } else
    {
        return RENDER_ERROR;
    }
}

SignalResult ReadZone::Signal(Terminal *t, const char* message)
{
    FnTrace("ReadZone::Signal()");
    SignalResult retval = SIGNAL_IGNORED;
    char newfile[STRLONG];

    if (strncmp(message, "findfile ", 9) == 0)
    {
        if (message[9] != '/')
        {
            MasterSystem->FullPath("text/", newfile);
            strcat(newfile, &message[9]);
            filename.Set(newfile);
        }
        else
        {
            filename.Set(&message[9]);
        }
        Draw(t, 1);
    }

    return retval;
}

SignalResult ReadZone::Touch(Terminal *t, int tx, int ty)
{
    FnTrace("ReadZone::Touch()");
    int new_page = 0;
    LayoutZone::Touch(t, tx, ty);
    if (selected_y < 3.0)
        new_page = page - 1;
    else if (selected_y > (size_y - 3.0))
        new_page = page + 1;
    else
        return SIGNAL_IGNORED;

    int max_page = report.max_pages;
    if (new_page >= max_page)
        new_page = 0;
    else if (new_page < 0)
        new_page = max_page - 1;

    if (new_page != page)
    {
        page = new_page;
        Draw(t, 0);
    }
    return SIGNAL_OKAY;
}

SignalResult ReadZone::Keyboard(Terminal *t, int my_key, int state)
{
    FnTrace("ReadZone::Keyboard()");
    int new_page = page;
    switch (my_key)
    {
    case 16:  // page up
        --new_page;
        break;
    case 14:  // page down
        ++new_page;
        break;
    default:
        return SIGNAL_IGNORED;
    }

    int max_page = report.max_pages;
    if (new_page >= max_page)
        new_page = 0;
    else if (new_page < 0)
        new_page = max_page - 1;

    if (new_page == page)
        return SIGNAL_IGNORED;

    page = new_page;
    Draw(t, 0);
    return SIGNAL_OKAY;
}
