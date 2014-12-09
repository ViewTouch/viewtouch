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
 * system_report.cc - revision 135 (10/13/98)
 * Implementation of system report generation functions
 */

#include "system.hh"
#include "check.hh"
#include "drawer.hh"
#include "sales.hh"
#include "report.hh"
#include "employee.hh"
#include "labor.hh"
#include "settings.hh"
#include "labels.hh"
#include "exception.hh"
#include "manager.hh"
#include "archive.hh"
#include "customer.hh"
#include "expense.hh"
#include "report_zone.hh"
#include "utility.hh"

#include <cstring>
#include <iostream>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/*********************************************************************
 * MediaList class:  I need to process various media types, like
 *   coupons and comps and such.  So we'll create a class here that
 *   will manage them all into a lump, accumulating all coupons and
 *   whatnot that were available during a given, arbitrary period
 *   (we may need to span several archives, or use today's data).
 ********************************************************************/
MediaList::MediaList()
{
    FnTrace("MediaList::MediaList()");
    next = NULL;
    name[0] = '\0';
    total = 0;
    int i;
    for (i = 0; i < MAX_SHIFTS; i++)
    {
        shift_total[i] = 0;
    }
}

MediaList::MediaList(const genericChar* namestr, int value)
{
    FnTrace("MediaList::MediaList(const char* , int)");
    next = NULL;
    strncpy(name, namestr, STRLONG);
    total = value;
    int i;
    for (i = 0; i < MAX_SHIFTS; i++)
    {
        shift_total[i] = 0;
    }
}

MediaList::MediaList(const genericChar* namestr, int value, int shift)
{
    FnTrace("MediaList::MediaList(const char* , int, int)");

    next = NULL;
    strncpy(name, namestr, STRLONG);
    total = value;
    int i;
    for (i = 0; i < MAX_SHIFTS; i++)
    {
        shift_total[i] = 0;
    }
    if (shift >= 0 && shift < MAX_SHIFTS)
        shift_total[shift] = value;
}

MediaList::~MediaList()
{
    FnTrace("MediaList::~MediaList()");
    if (next != NULL)
        delete next;
}

int MediaList::Add(const genericChar* namestr, int value, int shift)
{
    FnTrace("MediaList::Add()");
    int retval = 0;

    if (name[0] == '\0')
    {  // this entry is empty, just store the data here
        strncpy(name, namestr, STRLONG);
        total = value;
        if (shift >= 0 && shift < MAX_SHIFTS)
            shift_total[shift] = value;
    }
    else if (strcmp(name, namestr) == 0)
    {  // match; add value to our total
        total += value;
        if (shift >= 0 && shift < MAX_SHIFTS)
            shift_total[shift] += value;
    }
    else if (next != NULL)
    {  // try the next entry
        next->Add(namestr, value, shift);
    }
    else
    {  // we got to the end without finding a match, add it to the end
        MediaList *newentry = new MediaList(namestr, value, shift);
        next = newentry;
    }
    return retval;
}

int MediaList::Total(int shift)
{
    FnTrace("MediaList::Total()");
    int retval = 0;
    if (shift >= 0 && shift < MAX_SHIFTS)
        retval = shift_total[shift];
    else
        retval = total;
    if (next != NULL)
        retval += next->Total(shift);
    return retval;
}

int MediaList::Print()
{
    FnTrace("MediaList::Print()");
    printf("%s:  $%d\n", name, total);
    if (next != NULL)
        next->Print();
    return 0;
}


/**** System Class ****/
// Member Functions

/** Server Report **/
int System::ServerReport(Terminal *term, TimeInfo &time_start, 
                         TimeInfo &end_time, Employee *thisEmployee, Report *ptrReport)
{
    FnTrace("System::ServerReport()");
    if (ptrReport == NULL)
        return 1;

    ptrReport->update_flag = UPDATE_ARCHIVE | UPDATE_CHECKS | UPDATE_SERVER;
    term->SetCursor(CURSOR_WAIT);
    int user_id = 0;
    if (thisEmployee)
        user_id = thisEmployee->id;

    TimeInfo end;
    if (end_time.IsSet())
        end = end_time;
    else
        end = SystemTime;
    if (end >= SystemTime)
        ptrReport->update_flag = UPDATE_MINUTE;

    int sitdown_sales  = 0;
    int takeout_sales  = 0;
	int fastfood_sales = 0;
    int opened         = 0;
    int closed         = 0;
    int guests         = 0;
    int takeouts       = 0;
    int fastfood       = 0;
    int captured_tip   = 0;
    int tips_paid      = 0;
    int tip_variance   = 0;

    Settings *s = &settings;
    Archive *thisArchive = FindByTime(time_start);
    for (;;)
    {
        if (thisArchive == NULL)
            ptrReport->update_flag |= UPDATE_MINUTE;

        Check *thisCheck = FirstCheck(thisArchive);
        while (thisCheck)
        {
            if (((user_id == 0 && thisCheck->IsTraining() == 0) ||
                 thisCheck->user_open == user_id) &&
                (thisCheck->time_open >= time_start) &&
                (thisCheck->time_open < end) &&
                (!thisCheck->IsTakeOut()))
            {
                ++opened;
            }

            TimeInfo *timevar = thisCheck->TimeClosed();
            if ((timevar && *timevar >= time_start && *timevar < end) &&
                ((user_id == 0 && thisCheck->IsTraining() == 0) ||
                 thisCheck->WhoGetsSale(s) == user_id))
            {
                if (thisCheck->IsTakeOut())
                    ++takeouts;
                else if (thisCheck->IsFastFood())
					++fastfood;
				else
                {
                    guests += thisCheck->Guests();
                    ++closed;
                }

                for (SubCheck *sc = thisCheck->SubList(); sc != NULL; sc = sc->next)
                {
                    if (thisCheck->IsTakeOut())
                        takeout_sales += sc->total_sales;
					else if(thisCheck->IsFastFood())
						fastfood_sales += sc->total_sales;
                    else
                        sitdown_sales += sc->total_sales;

                    captured_tip += sc->TotalTip();
                }
            }
            thisCheck = thisCheck->next;
        }
        if (thisArchive == NULL || thisArchive->end_time > end)
            break; // kill loop
        thisArchive = thisArchive->next;
    }

    // Update the tips database.  Otherwise we can get very bogus values in
    // some cases.
    tip_db.Update(this);
    TipEntry *currTip = tip_db.FindByUser(user_id);
    if (currTip)
    {
        tips_paid = captured_tip - currTip->amount;
        if (tips_paid < 0)
            tips_paid = 0;

        tip_variance = currTip->amount;
    }

    ptrReport->Mode(PRINT_BOLD);
    if (thisEmployee)
        ptrReport->TextC(thisEmployee->system_name.Value());
    else
        ptrReport->TextC("Everyone");

    ptrReport->Mode(0);
    ptrReport->NewLine();

    ptrReport->TextPosR(6, "Start:");

    if (time_start.IsSet())
        ptrReport->TextPosL(7, term->TimeDate(time_start, TD2));
    else
        ptrReport->TextPosL(7, term->Translate("System Start"));
    ptrReport->NewLine();

    ptrReport->TextPosR(6, "End:");
    ptrReport->TextPosL(7, term->TimeDate(end, TD2));
    ptrReport->NewLine(2);

    ptrReport->TextL(term->Translate("Dining"));
    ptrReport->TextR(term->FormatPrice(sitdown_sales, 1));
    ptrReport->NewLine();

    ptrReport->TextL(term->Translate("Takeout"));
    ptrReport->TextR(term->FormatPrice(takeout_sales, 1));
    ptrReport->NewLine();

    ptrReport->TextL(term->Translate("Fast Food"));
    ptrReport->TextR(term->FormatPrice(fastfood_sales, 1));
    ptrReport->NewLine();

    ptrReport->TextL(term->Translate("Total"));
    ptrReport->TextR(term->FormatPrice(sitdown_sales + takeout_sales + fastfood_sales, 1));
    ptrReport->NewLine(2);

    ptrReport->TextL(term->Translate("Checks Opened"));
    ptrReport->NumberR(opened);
    ptrReport->NewLine();

    ptrReport->TextL(term->Translate("Checks Closed"));
    ptrReport->NumberR(closed);
    ptrReport->NewLine();

    ptrReport->TextL(term->Translate("Guests Served"));
    ptrReport->NumberR(guests);
    ptrReport->NewLine();

    ptrReport->TextL(term->Translate("Average Guest"));
    if (guests > 0)
        ptrReport->TextR(term->FormatPrice(sitdown_sales / guests, 1));
    else
        ptrReport->TextR("--");
    ptrReport->NewLine();

    ptrReport->TextL(term->Translate("Takeout Orders"));
    ptrReport->NumberR(takeouts);
    ptrReport->NewLine();

    ptrReport->TextL(term->Translate("Average Takeout"));
    if (takeouts > 0)
        ptrReport->TextR(term->FormatPrice(takeout_sales / takeouts, 1));
    else
        ptrReport->TextR("--");
    ptrReport->NewLine();

    ptrReport->TextL(term->Translate("FastFood Orders"));
    ptrReport->NumberR(fastfood);
    ptrReport->NewLine();

    ptrReport->TextL(term->Translate("Average FastFood"));
    if (fastfood > 0)
        ptrReport->TextR(term->FormatPrice(fastfood_sales / fastfood, 1));
    else
        ptrReport->TextR("--");
    ptrReport->NewLine(2);

    ptrReport->TextL(term->Translate("Captured Tips"));
    ptrReport->TextR(term->FormatPrice(captured_tip, 1));
    ptrReport->NewLine();

    ptrReport->TextL(term->Translate("Tips Paid"));
    ptrReport->TextR(term->FormatPrice(tips_paid, 1));
    ptrReport->NewLine();

    if (thisEmployee)
    {
        ptrReport->TextL(term->Translate("Tips Unpaid"));
        ptrReport->TextR(term->FormatPrice(tip_variance, 1));
        ptrReport->NewLine();
    }
    term->SetCursor(CURSOR_POINTER);
    return 0;
}

/** ShiftBalance Report **/
#define SHIFT_BALANCE_TITLE "Revenue and Productivity by Shift"
int System::ShiftBalanceReport(Terminal *term, TimeInfo &ref, Report *ptrReport)
{
    FnTrace("System::ShiftBalanceReport()");
    if (ptrReport == NULL)
        return 1;

    ptrReport->SetTitle(SHIFT_BALANCE_TITLE);
    ptrReport->Mode(PRINT_BOLD | PRINT_LARGE);
    ptrReport->TextC(SHIFT_BALANCE_TITLE);
    ptrReport->NewLine();
    ptrReport->TextC(term->GetSettings()->store_name.Value());
    ptrReport->NewLine();
    ptrReport->Mode(0);
    Settings *currSettings = &settings;
    int first_shift = currSettings->FirstShift();
    if (first_shift < 0)
    {
        // no shifts defined
        ptrReport->TextC("Please Define The Shifts..");
        return 0;
    }

    term->SetCursor(CURSOR_WAIT);
    int max_shifts = currSettings->ShiftCount();
    int shifts = max_shifts;
    int final  = 1;

    TimeInfo time_start, end;
    currSettings->ShiftStart(time_start, first_shift, ref);
    end = time_start;
    end.AdjustDays(1);

    if (end > SystemTime)
    {
        ptrReport->update_flag |= UPDATE_MINUTE;
        if (time_start <= SystemTime)
        {
            int sn = currSettings->ShiftNumber(SystemTime);
            shifts = currSettings->ShiftPosition(sn);
            final  = 0;
        }
        else
        {
            shifts = 0;
            final  = 0;
        }
    }

    // define and clear counters
    genericChar str[256];
    genericChar str2[256];
    int takeout_sales[MAX_SHIFTS];
    int total_takeout_sales = 0;
	int fastfood_sales[MAX_SHIFTS];
    int total_fastfood_sales = 0;
    int takeout[MAX_SHIFTS];
    int total_takeout = 0;
	int fastfood[MAX_SHIFTS];
    int total_fastfood = 0;

    int guests[MAX_SHIFTS];
    int total_guests = 0;
    int group_sales[MAX_SHIFTS][8];
    int total_group_sales[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int sales[MAX_SHIFTS];
    int total_sales = 0;
    int net_sales[MAX_SHIFTS];
    int total_net_sales = 0;
    int item_comp[MAX_SHIFTS];
    int total_item_comp = 0;
    int total_discount = 0;
    int total_coupon = 0;
    int total_comp = 0;
    int total_emeal = 0;
    int adjust[MAX_SHIFTS];
    int total_adjust = 0;
    int labor_mins[MAX_SHIFTS];
    int total_labor_mins = 0;
    int labor_cost[MAX_SHIFTS];
    int total_labor_cost = 0;
    int labor_otmins[MAX_SHIFTS];
    int total_labor_otmins = 0;
    int labor_otcost[MAX_SHIFTS];
    int total_labor_otcost = 0;
    int job_mins[MAX_SHIFTS][16];
    int total_job_mins[16];
    int job_cost[MAX_SHIFTS][16];
    int total_job_cost[16];
    int job_otmins[MAX_SHIFTS][16];
    int total_job_otmins[16];
    int job_otcost[MAX_SHIFTS][16];
    int total_job_otcost[16];
    Flt labor_percent[MAX_SHIFTS];
    int x;
    int i;
    int j;

    MediaList discountlist;
    MediaList couponlist;
    MediaList complist;
    MediaList meallist;

    for (i = 0; i < MAX_SHIFTS; ++i)
    {
        takeout[i]       = 0;
		fastfood[i]      = 0;
        takeout_sales[i] = 0;
		fastfood_sales[i]= 0;
        guests[i]        = 0;
        net_sales[i]     = 0;
        labor_mins[i]    = 0;
        labor_cost[i]    = 0;
        labor_otmins[i]  = 0;
        labor_otcost[i]  = 0;
        labor_percent[i] = 0.0;
        item_comp[i]     = 0;
        adjust[i]        = 0;
        sales[i]         = 0;
        for (j = 0; j < 8; ++j)
            group_sales[i][j] = 0;
        for (j = 0; j < 16; ++j)
        {
            job_mins[i][j] = 0;
            job_cost[i][j] = 0;
            job_otmins[i][j] = 0;
            job_otcost[i][j] = 0;
        }
    }

    for (i = 0; i < 16; ++i)
    {
        total_job_mins[i] = 0;
        total_job_cost[i] = 0;
        total_job_otmins[i] = 0;
        total_job_otcost[i] = 0;
    }

    // Calculate sales
    Archive *a = FindByTime(time_start);
    for (;;)
    {
        for (Check *thisCheck = FirstCheck(a); thisCheck != NULL; thisCheck = thisCheck->next)
		{
            if (thisCheck->IsTraining() > 0)
                continue;
            
            TimeInfo *timevar = thisCheck->TimeClosed();
            if (timevar == NULL && thisCheck->CustomerType() == CHECK_HOTEL)
                timevar = &thisCheck->time_open;
            if (timevar == NULL || *timevar < time_start || *timevar >= end)
                continue;
            
            int sn = currSettings->ShiftNumber(*timevar);
            if (thisCheck->IsTakeOut())
                ++takeout[sn];
            else if(thisCheck->IsFastFood())
                ++fastfood[sn];
			else
				guests[sn] += thisCheck->Guests();

            // Add all of the media
            CompInfo *compinfo = a ? a->CompList() : settings.CompList();
            while (compinfo != NULL)
            {
                complist.Add(compinfo->name.Value(), 0);
                compinfo = compinfo->next;
            }
            MealInfo *mealinfo = a ? a->MealList() : settings.MealList();
            while (mealinfo != NULL)
            {
                meallist.Add(mealinfo->name.Value(), 0);
                mealinfo = mealinfo->next;
            }
            DiscountInfo *discinfo = a ? a->DiscountList() : settings.DiscountList();
            while (discinfo != NULL)
            {
                discountlist.Add(discinfo->name.Value(), 0);
                discinfo = discinfo->next;
            }
            CouponInfo *coupinfo = a ? a->CouponList() : settings.CouponList();
            while (coupinfo != NULL)
            {
                couponlist.Add(coupinfo->name.Value(), 0);
                coupinfo = coupinfo->next;
            }
            
			for (SubCheck *sc = thisCheck->SubList(); sc != NULL; sc = sc->next)
			{
				for (int sg = SALESGROUP_FOOD; sg <= SALESGROUP_ROOM; ++sg)
					group_sales[sn][sg] += sc->GrossSales(thisCheck, currSettings, sg);

				int my_sales = sc->GrossSales(thisCheck, currSettings, 0);
				sales[sn] += my_sales;

				if (thisCheck->IsTakeOut())
					takeout_sales[sn] += my_sales;

				if (thisCheck->IsFastFood())
					fastfood_sales[sn] += my_sales;

				item_comp[sn] += sc->item_comps;
				for (Payment *thisPayment = sc->PaymentList(); thisPayment != NULL; thisPayment = thisPayment->next)
				{
					switch (thisPayment->tender_type)
					{
                    case TENDER_COMP:
                    {
                        compinfo = NULL;
                        if (a)
                            compinfo = a->FindCompByID(thisPayment->tender_id);
                        else
                            compinfo = settings.FindCompByID(thisPayment->tender_id);
                        if (compinfo)
                            complist.Add(compinfo->name.Value(), thisPayment->value, sn);
                    }
                        break;
                    case TENDER_EMPLOYEE_MEAL:
                    {
                        mealinfo = NULL;
                        if (a)
                            mealinfo = a->FindMealByID(thisPayment->tender_id);
                        else
                            mealinfo = settings.FindMealByID(thisPayment->tender_id);
                        if (mealinfo)
                            meallist.Add(mealinfo->name.Value(), thisPayment->value, sn);
                    }
                        break;
                    case TENDER_DISCOUNT:
                    {
                        discinfo = NULL;
                        if (a)
                            discinfo = a->FindDiscountByID(thisPayment->tender_id);
                        else
                            discinfo = settings.FindDiscountByID(thisPayment->tender_id);
                        if (discinfo)
                            discountlist.Add(discinfo->name.Value(), thisPayment->value, sn);
                    }
                        break;
                    case TENDER_COUPON:
                    {
                        coupinfo = NULL;
                        if (a)
                            coupinfo = a->FindCouponByID(thisPayment->tender_id);
                        else
                            coupinfo = settings.FindCouponByID(thisPayment->tender_id);
                        if (coupinfo)
                            couponlist.Add(coupinfo->name.Value(), thisPayment->value, sn);
                    }
                        break;
                    default:
                        break;
					}
				}
			}
		}

        if (a == NULL || a->end_time > end)
            break;

        a = a->next;
    }

    // Shift Totals
    for (i = 0; i < MAX_SHIFTS; ++i)
    {
        for (int sg = SALESGROUP_FOOD; sg <= SALESGROUP_ROOM; ++sg)
        {
            total_group_sales[sg] += group_sales[i][sg];
        }

        int discount = discountlist.Total(i);
        int comp = complist.Total(i);
        int coupon = couponlist.Total(i);
        int emeal = meallist.Total(i);

        total_takeout        += takeout[i];
		total_fastfood       += fastfood[i];
        total_takeout_sales  += takeout_sales[i];
		total_fastfood_sales += fastfood_sales[i];
        total_guests         += guests[i];
        total_sales          += sales[i];
        total_item_comp      += item_comp[i];
        total_discount       += discount;
        total_comp           += comp;
        total_coupon         += coupon;
        total_emeal          += emeal;

        adjust[i] = comp + emeal + discount + coupon + item_comp[i];
        net_sales[i] = sales[i] - adjust[i];

        total_net_sales += net_sales[i];
        total_adjust    += adjust[i];
    }

    int shift[MAX_SHIFTS+1];
    int sn;
    for (i = 0, sn = first_shift; i <= MAX_SHIFTS; ++i, sn = currSettings->NextShift(sn))
    {
        shift[i] = sn;
    }

    // labor calculations
    for (i = 0; i < shifts; ++i)
    {
        // get time_start and end time range for shift
        int sh = shift[i];
        TimeInfo t1, t2;
        currSettings->ShiftStart(t1, shift[i],   time_start);
        currSettings->ShiftStart(t2, shift[i+1], time_start);
        if (t2 < t1)
            t2.AdjustDays(1);

        // calculate labor in the period
        if (term->expand_labor)
        {
            j = 1;
            while (JobValue[j] > 0)
            {
                labor_db.FigureLabor(currSettings, t1, t2, JobValue[j],
                                     job_mins[sh][j], job_cost[sh][j],
                                     job_otmins[sh][j], job_otcost[sh][j]);
                total_job_mins[j]   += job_mins[sh][j];
                total_job_cost[j]   += job_cost[sh][j];
                total_job_otmins[j] += job_otmins[sh][j];
                total_job_otcost[j] += job_otcost[sh][j];
                ++j;
            }
        }

        labor_db.FigureLabor(currSettings, t1, t2, 0, labor_mins[sh], labor_cost[sh],
                             labor_otmins[sh], labor_otcost[sh]);
        total_labor_mins   += labor_mins[sh];
        total_labor_cost   += labor_cost[sh];
        total_labor_otmins += labor_otmins[sh];
        total_labor_otcost += labor_otcost[sh];
    }

    // Report Setup
    int cr[MAX_SHIFTS+1] = {10};
    for (i = 1; i <= MAX_SHIFTS; ++i)
        cr[i] = i * 11 + 17;

    int last_color = COLOR_DK_RED;
    if (final)
        last_color = COLOR_DEFAULT;

    int color[MAX_SHIFTS];
    if (shifts <= 1)
    {
        for (i = 0; i < MAX_SHIFTS; ++i)
            color[i] = last_color;
    }
    else
    {
        for (i = 0; i < (shifts - 1); ++i)
            color[i] = COLOR_DEFAULT;
        for (i = (shifts - 1); i < MAX_SHIFTS; ++i)
            color[i] = last_color;
    }

    // Main header
    sprintf(str, "%s --  %s", term->TimeDate(str2, time_start, TD0),
            term->TimeDate(end, TD0));
    ptrReport->TextC(str, COLOR_DK_BLUE);
    ptrReport->NewLine(2);

    // Column headers
    ptrReport->Mode(PRINT_UNDERLINE);
    int last_col = 1;
    if (max_shifts > 1)
    {
        last_col = max_shifts + 1;
        for (i = 0; i < max_shifts; ++i)
        {
            currSettings->ShiftText(str, shift[i]);
            ptrReport->TextPosR(cr[i + 1], str);
        }
    }

    int last_pos = cr[last_col];
    int percent_pos = last_pos + 2;
    Flt per;

    ptrReport->TextPosR(last_pos, "All Day");
    ptrReport->Mode(0);
    ptrReport->NewLine();

    // Report Entries
    for (int g = SALESGROUP_FOOD; g <= SALESGROUP_ROOM; ++g)
	{
        if (currSettings->IsGroupActive(g) || total_group_sales[g] != 0)
        {
            if (term->hide_zeros == 0 || total_group_sales[g] != 0)
            {
                sprintf(str, "%s Sales", SalesGroupName[g]);
                ptrReport->TextL(str);
                if (max_shifts > 1)
                {
                    for (i = 0; i < shifts; ++i)
                    {
                        ptrReport->TextPosR(cr[i + 1], term->FormatPrice(group_sales[shift[i]][g]), color[i]);
                    }
                }
                
                ptrReport->TextPosR(last_pos, term->FormatPrice(total_group_sales[g]), last_color);
                per = 0;
                if (total_sales > 0)
                    per = 100.0 * ((Flt) total_group_sales[g] / (Flt) total_sales);
                sprintf(str, "%.2f%%", per);
                ptrReport->TextPosL(percent_pos, str, COLOR_DK_BLUE);
                ptrReport->NewLine();
            }
        }
	}

    ptrReport->TextPosL(3, "Total Sales");
    if (max_shifts > 1)
    {
        for (i = 0; i < shifts; ++i)
        {
            ptrReport->TextPosR(cr[i + 1], term->FormatPrice(sales[shift[i]]), color[i]);
        }
    }

    ptrReport->TextPosR(last_pos, term->FormatPrice(total_sales), last_color);
    ptrReport->TextPosL(percent_pos, "100%", COLOR_DK_BLUE);
    ptrReport->NewLine();

    // Adjustments
    ptrReport->NewLine();
    ptrReport->Mode(PRINT_BOLD);
    ptrReport->TextL("Goodwill Adjustments", COLOR_DK_BLUE);
    ptrReport->NewLine();
    ptrReport->Mode(0);

    if (term->hide_zeros == 0 || total_comp != 0)
    {
        ptrReport->TextL("Whole Check Comps");
        if (term->expand_goodwill == 0)
        {
            if (max_shifts > 1)
            {
                for (i = 0; i < shifts; ++i)
                {
                    ptrReport->TextPosR(cr[i + 1], term->FormatPrice(complist.Total(i)), color[i]);
                }
            }
            ptrReport->TextPosR(last_pos, term->FormatPrice(total_comp), last_color);
        }
        ptrReport->NewLine();
        if (term->expand_goodwill)
        {
            MediaList *comps = &complist;
            while (comps != NULL)
            {
                if (comps->name[0] != '\0' && (comps->total != 0 || term->hide_zeros == 0))
                {
                    ptrReport->TextPosL(3, comps->name);
                    if (max_shifts > 1)
                    {
                        for (i = 0; i < shifts; ++i )
                        {
                            ptrReport->TextPosR(cr[i + 1], term->FormatPrice(comps->shift_total[i]), color[i]);
                        }
                    }
                    ptrReport->TextPosR(last_pos, term->FormatPrice(comps->total), color[i]);
                    ptrReport->NewLine();
                }
                comps = comps->next;
            }
        }
    }

    if (term->hide_zeros == 0 || total_item_comp != 0)
    {
        ptrReport->TextL("Line Item Comps");
        if (max_shifts > 1)
        {
            for (i = 0; i < shifts; ++i)
            {
                ptrReport->TextPosR(cr[i + 1], term->FormatPrice(item_comp[shift[i]]), color[i]);
            }
        }
        ptrReport->TextPosR(last_pos, term->FormatPrice(total_item_comp), last_color);
        ptrReport->NewLine();
    }

    if (term->hide_zeros == 0 || total_emeal != 0)
    {
        ptrReport->TextL("Employee Discounts");
        if (term->expand_goodwill == 0)
        {
            if (max_shifts > 1)
            {
                for (i = 0; i < shifts; ++i)
                {
                    ptrReport->TextPosR(cr[i + 1], term->FormatPrice(meallist.Total(i)), color[i]);
                }
            }
            ptrReport->TextPosR(last_pos, term->FormatPrice(total_emeal), last_color);
        }
        ptrReport->NewLine();
        if (term->expand_goodwill)
        {
            MediaList *meals = &meallist;
            while (meals != NULL)
            {
                if (meals->name[0] != '\0' && (meals->total != 0 || term->hide_zeros == 0))
                {
                    ptrReport->TextPosL(3, meals->name);
                    if (max_shifts > 1)
                    {
                        for (i = 0; i < shifts; ++i )
                        {
                            ptrReport->TextPosR(cr[i + 1], term->FormatPrice(meals->shift_total[i]), color[i]);
                        }
                    }
                    ptrReport->TextPosR(last_pos, term->FormatPrice(meals->total), color[i]);
                    ptrReport->NewLine();
                }
                meals = meals->next;
            }
        }
    }

    if (term->hide_zeros == 0 || total_discount != 0)
    {
        ptrReport->TextL("Customer Discounts");
        if (term->expand_goodwill == 0)
        {
            if (max_shifts > 1)
            {
                for (i = 0; i < shifts; ++i)
                {
                    ptrReport->TextPosR(cr[i + 1], term->FormatPrice(discountlist.Total(i)), color[i]);
                }
            }
            ptrReport->TextPosR(last_pos, term->FormatPrice(total_discount), last_color);
        }
        ptrReport->NewLine();
        if (term->expand_goodwill)
        {
            MediaList *discounts = &discountlist;
            while (discounts != NULL)
            {
                if (discounts->name[0] != '\0' && (discounts->total != 0 || term->hide_zeros == 0))
                {
                    ptrReport->TextPosL(3, discounts->name);
                    if (max_shifts > 1)
                    {
                        for (i = 0; i < shifts; ++i )
                        {
                            ptrReport->TextPosR(cr[i + 1], term->FormatPrice(discounts->shift_total[i]), color[i]);
                        }
                    }
                    ptrReport->TextPosR(last_pos, term->FormatPrice(discounts->total), color[i]);
                    ptrReport->NewLine();
                }
                discounts = discounts->next;
            }
        }
    }

    if (term->hide_zeros == 0 || total_coupon != 0)
    {
        ptrReport->TextL("Coupons");
        if (term->expand_goodwill == 0)
        {
            if (max_shifts > 1)
            {
                for (i = 0; i < shifts; ++i)
                {
                    ptrReport->TextPosR(cr[i + 1], term->FormatPrice(couponlist.Total(i)), color[i]);
                }
            }
            ptrReport->TextPosR(last_pos, term->FormatPrice(total_coupon), last_color);
        }
        ptrReport->NewLine();
        if (term->expand_goodwill)
        {
            MediaList *coupons = &couponlist;
            while (coupons != NULL)
            {
                if (coupons->name[0] != '\0' && (coupons->total != 0 || term->hide_zeros == 0))
                {
                    ptrReport->TextPosL(3, coupons->name);
                    if (max_shifts > 1)
                    {
                        for (i = 0; i < shifts; ++i )
                        {
                            ptrReport->TextPosR(cr[i + 1], term->FormatPrice(coupons->shift_total[i]), color[i]);
                        }
                    }
                    ptrReport->TextPosR(last_pos, term->FormatPrice(coupons->total), color[i]);
                    ptrReport->NewLine();
                }
                coupons = coupons->next;
            }
        }
    }

    if (term->expand_goodwill)
        ptrReport->TextL("Total Adjustments");
    else
        ptrReport->TextPosL(3, "Total Adjustments");
    if (max_shifts > 1)
	{
        for (i = 0; i < shifts; ++i)
        {
            ptrReport->TextPosR(cr[i + 1], term->FormatPrice(adjust[shift[i]]), color[i]);
        }
	}

    per = 0;
    if (total_sales > 0)
    {
        per = 100.0 * ((Flt) total_adjust / (Flt) total_sales);
    }

    ptrReport->TextPosR(last_pos, term->FormatPrice(total_adjust), last_color);
    sprintf(str, "%.2f%%", per);
    ptrReport->TextPosL(percent_pos, str, COLOR_DK_BLUE);
    ptrReport->NewLine();

    ptrReport->NewLine();

    if (term->hide_zeros == 0 || total_net_sales != 0)
    {
        ptrReport->TextL("Cash Sales");
        if (max_shifts > 1)
        {
            for (i = 0; i < shifts; ++i)
            {
                ptrReport->TextPosR(cr[i + 1], term->FormatPrice(net_sales[shift[i]], 1), color[i]);
            }
        }
        ptrReport->TextPosR(last_pos, term->FormatPrice(total_net_sales, 1), last_color);
        sprintf(str, "%.2f%%", 100.0 - per);
        ptrReport->TextPosL(percent_pos, str, COLOR_DK_BLUE);
        ptrReport->NewLine(2);
    }

    // Guest Info
    ptrReport->TextL("Guest Count");
    if (max_shifts > 1)
	{
        for (i = 0; i < shifts; ++i)
        {
            ptrReport->NumberPosR(cr[i + 1], guests[shift[i]], color[i]);
        }
	}
    ptrReport->NumberPosR(last_pos, total_guests, last_color);
    ptrReport->NewLine();
    ptrReport->TextL("Average Guest");
    if (max_shifts > 1)
	{
        for (i = 0; i < shifts; ++i)
        {
            if (guests[shift[i]] > 0)
				x = (sales[shift[i]] - ( takeout_sales[shift[i]] + fastfood_sales[shift[i]] )) / guests[shift[i]];
            else
				x = 0;

            ptrReport->TextPosR(cr[i+1], term->FormatPrice(x, 1), color[i]);
        }
	}

    if (total_guests > 0)
        x = (total_sales - (total_takeout_sales + total_fastfood_sales )) / total_guests;
    else
        x = 0;
    ptrReport->TextPosR(last_pos, term->FormatPrice(x, 1), last_color);
    ptrReport->NewLine();

    // Takeout Info
    if (term->hide_zeros == 0 || total_takeout != 0)
    {
        ptrReport->TextL("Takeout Orders");
        if (max_shifts > 1)
        {
            for (i = 0; i < shifts; ++i)
            {
                ptrReport->NumberPosR(cr[i + 1], takeout[shift[i]], color[i]);
            }
        }
        ptrReport->NumberPosR(last_pos, total_takeout, last_color);
        ptrReport->NewLine();
        ptrReport->TextL("Average Takeout");
        if (max_shifts > 1)
        {
            for (i = 0; i < shifts; ++i)
            {
                if (takeout[shift[i]] > 0)
                    x = takeout_sales[shift[i]] / takeout[shift[i]];
                else
                    x = 0;
                ptrReport->TextPosR(cr[i+1], term->FormatPrice(x, 1), color[i]);
            }
        }
        if (total_takeout > 0)
            x = (total_takeout_sales / total_takeout);
        else
            x = 0;
        ptrReport->TextPosR(last_pos, term->FormatPrice(x, 1), last_color);
        ptrReport->NewLine();
    }

	// fast food sales reporting
    if (term->hide_zeros == 0 || total_fastfood != 0)
    {
        ptrReport->TextL("FastFood Orders");
        if(max_shifts > 1)
        {
            for(i=0; i< shifts; ++i)
            {
                ptrReport->NumberPosR(cr[i+1], fastfood[shift[i]], color[i]); // fix - change for fast food; JMK
            }
        }
        ptrReport->NumberPosR(last_pos, total_fastfood, last_color);
        ptrReport->NewLine();
        ptrReport->TextL("Average FastFood");
        if(max_shifts > 1)
        {
            for(i=0; i< shifts; ++i)
            {
                if(fastfood[shift[i]] > 0)
                    x = (fastfood_sales[shift[i]] / fastfood[shift[i]]);
                else
                    x = 0;
                ptrReport->TextPosR(cr[i+1], term->FormatPrice(x,1), color[i]);
            }
        }
        if(total_fastfood > 0)
            x = total_fastfood_sales / total_fastfood;
        else
            x = 0;
        ptrReport->TextPosR(last_pos, term->FormatPrice(x, 1), last_color);
        ptrReport->NewLine(2);
    }

    // Labor Info
    if (term->hide_zeros == 0 || total_labor_cost != 0)
    {
        ptrReport->TextL("Regular Hours");
        if (max_shifts > 1)
        {
            for (i = 0; i < shifts; ++i)
            {
                sprintf(str, "%.1f", (Flt) labor_mins[shift[i]] / 60.0);
                ptrReport->TextPosR(cr[i + 1], str, color[i]);
            }
        }
        
        sprintf(str, "%.1f", (Flt) total_labor_mins / 60.0);
        ptrReport->TextPosR(last_pos, str, last_color);
        ptrReport->NewLine();
        
        ptrReport->TextL("Regular Cost");
        if (max_shifts > 1)
        {
            for (i = 0; i < shifts; ++i)
            {
                ptrReport->TextPosR(cr[i+1], term->FormatPrice(labor_cost[shift[i]], 1), color[i]);
            }
        }
        ptrReport->TextPosR(last_pos, term->FormatPrice(total_labor_cost, 1), last_color);
        ptrReport->NewLine();
    }

    if (term->hide_zeros == 0 || total_labor_otcost != 0)
    {
        ptrReport->TextL("Overtime Hours");
        if (max_shifts > 1)
        {
            for (i = 0; i < shifts; ++i)
            {
                sprintf(str, "%.1f", (Flt) labor_otmins[shift[i]] / 60.0);
                ptrReport->TextPosR(cr[i + 1], str, color[i]);
            }
        }
        
        sprintf(str, "%.1f", (Flt) total_labor_otmins / 60.0);
        ptrReport->TextPosR(last_pos, str, last_color);
        ptrReport->NewLine();
        
        ptrReport->TextL("Overtime Cost");
        if (max_shifts > 1)
        {
            for (i = 0; i < shifts; ++i)
            {
                ptrReport->TextPosR(cr[i+1], term->FormatPrice(labor_otcost[shift[i]], 1), color[i]);
            }
        }
        ptrReport->TextPosR(last_pos, term->FormatPrice(total_labor_otcost, 1), last_color);
        ptrReport->NewLine();
    }

    if (term->expand_labor)
    {
        j = 1;
        while (JobValue[j] > 0)
        {
            if (currSettings->job_active[JobValue[j]] && (term->hide_zeros == 0 || total_job_mins[j] != 0))
            {
                sprintf(str, "%s Hours", JobName[j]);
                ptrReport->TextL(str);
				if (max_shifts > 1)
				{
					for (i = 0; i < shifts; ++i)
					{
						sprintf(str, "%.1f", (Flt) job_mins[shift[i]][j] / 60.0);
						ptrReport->TextPosR(cr[i + 1], str, color[i]);
					}
				}
				sprintf(str, "%.1f", (Flt) total_job_mins[j] / 60.0);
				ptrReport->TextPosR(last_pos, str, last_color);
				ptrReport->NewLine();
				if (total_job_otmins[j] > 0)
				{
					ptrReport->TextL("Overtime");
					if (max_shifts > 1)
                    {
						for (i = 0; i < shifts; ++i)
						{
							sprintf(str, "%.1f", (Flt) job_otmins[shift[i]][j] / 60.0);
							ptrReport->TextPosR(cr[i + 1], str, color[i]);
						}
                    }
					sprintf(str, "%.1f", (Flt) total_job_otmins[j] / 60.0);
					ptrReport->TextPosR(last_pos, str, last_color);
					ptrReport->NewLine();
				}
            }
            ++j;
        }
        ptrReport->TextPosL(3, "Total Hours");
    }
    else
    {
        ptrReport->TextL("Total Hours");
    }

    if (max_shifts > 1)
	{
        for (i = 0; i < shifts; ++i)
        {
            sprintf(str, "%.1f", (Flt) (labor_mins[shift[i]] + labor_otmins[shift[i]]) / 60.0);
            ptrReport->TextPosR(cr[i + 1], str, color[i]);
        }
	}

    sprintf(str, "%.1f", (Flt) (total_labor_mins + total_labor_otmins) / 60.0);
    ptrReport->TextPosR(last_pos, str, last_color);
    ptrReport->NewLine();

    if (term->expand_labor)
    {
        j = 1;
        while (JobValue[j] > 0)
        {
            if (currSettings->job_active[JobValue[j]] && (term->hide_zeros == 0 || total_job_cost[j] != 0))
            {
                sprintf(str, "%s Cost", JobName[j]);
                ptrReport->TextL(str);
                if (max_shifts > 1)
                {
                    for (i = 0; i < shifts; ++i)
                    {
                        ptrReport->TextPosR(cr[i+1], term->FormatPrice(job_cost[shift[i]][j], 1), color[i]);
                    }
                }
                ptrReport->TextPosR(last_pos, term->FormatPrice(total_job_cost[j], 1), last_color);
                ptrReport->NewLine();
                if (total_job_otcost[j] > 0)
                {
                    ptrReport->TextL("Overtime");
                    if (max_shifts > 1)
                    {
                        for (i = 0; i < shifts; ++i)
                        {
                            ptrReport->TextPosR(cr[i+1], term->FormatPrice(job_otcost[shift[i]][j], 1), color[i]);
                        }
                    }
                    ptrReport->TextPosR(last_pos, term->FormatPrice(total_job_otcost[j], 1), last_color);
                    ptrReport->NewLine();
                }
            }
            ++j;
        }
        ptrReport->TextPosL(3, "Total Cost");
    }
    else
    {
        ptrReport->TextL("Total Cost");
    }

    if (max_shifts > 1)
    {
        for (i = 0; i < shifts; ++i)
        {
            ptrReport->TextPosR(cr[i+1], term->FormatPrice(labor_cost[shift[i]] + labor_otcost[shift[i]], 1), color[i]);
        }
	}

    ptrReport->TextPosR(last_pos, term->FormatPrice(total_labor_cost + total_labor_otcost, 1), last_color);
    ptrReport->NewLine();

    ptrReport->NewLine();
    ptrReport->TextL("Sales/Wage Hour");
    if (max_shifts > 1)
	{
        for (i = 0; i < shifts; ++i)
		{
			int curr_settings = shift[i];
			if (labor_mins[curr_settings] > 0)
				x = FltToPrice(PriceToFlt(sales[curr_settings] * 60) / labor_mins[curr_settings]);
			else
				x = 0;
			ptrReport->TextPosR(cr[i+1], term->FormatPrice(x, 1), color[i]);
		}
	}

    if (total_labor_mins > 0)
        x = FltToPrice(PriceToFlt(total_sales * 60) / total_labor_mins);
    else
        x = 0;
    ptrReport->TextPosR(last_pos, term->FormatPrice(x, 1), last_color);

    ptrReport->NewLine();
    ptrReport->TextL("Sales/Wage Dollar");
    if (max_shifts > 1)
	{
        for (i = 0; i < shifts; ++i)
        {
            if (labor_cost[i] > 0)
				x = (sales[shift[i]] * 100) / (labor_cost[shift[i]] + labor_otcost[shift[i]]);
            else
				x = 0;

            ptrReport->TextPosR(cr[i+1], term->FormatPrice(x, 1), color[i]);
        }
	}

    if (total_labor_cost > 0)
        x = (total_sales * 100) / (total_labor_cost + total_labor_otcost);
    else
        x = 0;

    ptrReport->TextPosR(last_pos, term->FormatPrice(x, 1), last_color);

    Flt f;
    ptrReport->NewLine();
    ptrReport->TextL("Labor Cost %");
    if (max_shifts > 1)
	{
        for (i = 0; i < shifts; ++i)
        {
            int curr_settings = shift[i];
            if (sales[curr_settings] > 0)
                f = (Flt) ((labor_cost[curr_settings] + labor_otcost[curr_settings]) * 100) / (Flt) sales[curr_settings];
            else
				f = 0;

            sprintf(str, "%.2f%%", f);
            ptrReport->TextPosR(cr[i + 1] + 1, str, color[i]);
        }
	}

    if (total_sales > 0)
        f = (Flt) ((total_labor_cost + total_labor_otcost) * 100) / (Flt) total_sales;
    else
        f = 0;

    sprintf(str, "%.2f%%", f);
    ptrReport->TextPosR(last_pos, str, last_color);
    term->SetCursor(CURSOR_POINTER);
    return 0;
}

/** Balance Report **/
class BRData
{
public:
    System *system;
    Report *report;
    Terminal *term;
    Archive *archive;
    Archive *lastArchive;
    Check   *check;
    TimeInfo start;
    TimeInfo end;
    int guests;
    int sales;
    int takeout_sales;
    int takeout;
    int fastfood_sales;
    int fastfood;
    int item_comp;
    int group_sales[8];

    MediaList discountlist;
    MediaList couponlist;
    MediaList complist;
    MediaList meallist;

    // Constructor
    BRData()
    {
        system = NULL;
        report = NULL;
        term = NULL;
        archive = NULL;
        lastArchive = NULL;
        check = NULL;
        guests = 0;
        sales = 0;
		takeout_sales = 0;
        takeout = 0;
		fastfood_sales = 0;
        fastfood = 0;
        item_comp = 0;
        for (int i = 0; i < 8; ++i)
        {
            group_sales[i] = 0;
        }
    }
};

int BalanceReportWorkFn(BRData *brdata)
{
    FnTrace("BalanceReportWorkFn()");
    Terminal *term         = brdata->term;
    Report   *thisReport   = brdata->report;
    System   *sys          = brdata->system;
    Settings *currSettings = &sys->settings;

    // Calculate sales
    Check *c = brdata->check;
    if (c == NULL)
        c = sys->FirstCheck(brdata->archive);

    while (c && c->IsTraining())
        c = c->next;

    // Process Media entries if both archive and lastArchive are null, in which
    // case we just process the media entries in settings, or once for each
    // archive.
    if ((brdata->archive == NULL && brdata->lastArchive == NULL) ||
        (brdata->archive && (brdata->archive != brdata->lastArchive)))
    {
        // Add the media titles for this archive
        CompInfo *compinfo = brdata->archive ? brdata->archive->CompList() : currSettings->CompList();
        while (compinfo != NULL)
        {
            brdata->complist.Add(compinfo->name.Value(), 0);
            compinfo = compinfo->next;
        }
        MealInfo *mealinfo = brdata->archive ? brdata->archive->MealList() : currSettings->MealList();
        while (mealinfo != NULL)
        {
            brdata->meallist.Add(mealinfo->name.Value(), 0);
            mealinfo = mealinfo->next;
        }
        DiscountInfo *discinfo = brdata->archive ? brdata->archive->DiscountList() : currSettings->DiscountList();
        while (discinfo != NULL)
        {
            brdata->discountlist.Add(discinfo->name.Value(), 0);
            discinfo = discinfo->next;
        }
        CouponInfo *coupinfo = brdata->archive ? brdata->archive->CouponList() : currSettings->CouponList();
        while (coupinfo != NULL)
        {
            brdata->couponlist.Add(coupinfo->name.Value(), 0);
            coupinfo = coupinfo->next;
        }
        brdata->lastArchive = brdata->archive;
    }

    while (c)
    {
        if (c->IsTraining() == 0)
        {
            TimeInfo *timevar = c->TimeClosed();
            if ((timevar && *timevar >= brdata->start && *timevar < brdata->end) ||
                (c->CustomerType() == CHECK_HOTEL && c->time_open >= brdata->start && c->time_open < brdata->end))
            {
                if (c->IsTakeOut())
                    ++brdata->takeout;
                else if(c->IsFastFood())
                    ++brdata->fastfood;
                else
                    brdata->guests += c->Guests();
                
                for (SubCheck *sc = c->SubList(); sc != NULL; sc = sc->next)
                {
                    for (int sg = SALESGROUP_FOOD; sg <= SALESGROUP_ROOM; ++sg)
                    {
                        brdata->group_sales[sg] += sc->GrossSales(c, currSettings, sg);
                    }
                    
                    int x = sc->GrossSales(c, currSettings, 0);
                    brdata->sales += x;

                    if (c->IsTakeOut())
                        brdata->takeout_sales += x;
                    
                    if (c->IsFastFood())
                        brdata->fastfood_sales += x;
                    
                    CompInfo *compinfo;
                    DiscountInfo *discinfo;
                    CouponInfo *coupinfo;
                    MealInfo *mealinfo;
                    
                    brdata->item_comp += sc->item_comps;
                    for (Payment *p = sc->PaymentList(); p != NULL; p = p->next)
                    {
                        switch (p->tender_type)
                        {
                        case TENDER_COMP:
                            compinfo = NULL;
                            if (brdata->archive)
                                compinfo = brdata->archive->FindCompByID(p->tender_id);
                            else
                                compinfo = currSettings->FindCompByID(p->tender_id);
                            if (compinfo)
                                brdata->complist.Add(compinfo->name.Value(), p->value);
                            break;
                        case TENDER_EMPLOYEE_MEAL:
                            mealinfo = NULL;
                            if (brdata->archive)
                                mealinfo = brdata->archive->FindMealByID(p->tender_id);
                            else
                                mealinfo = currSettings->FindMealByID(p->tender_id);
                            if (mealinfo)
                                brdata->meallist.Add(mealinfo->name.Value(), p->value);
                            break;
                        case TENDER_DISCOUNT:
                            discinfo = NULL;
                            if (brdata->archive)
                                discinfo = brdata->archive->FindDiscountByID(p->tender_id);
                            else
                                discinfo = currSettings->FindDiscountByID(p->tender_id);
                            if (discinfo)
                                brdata->discountlist.Add(discinfo->name.Value(), p->value);
                            break;
                        case TENDER_COUPON:
                            coupinfo = NULL;
                            if (brdata->archive)
                                coupinfo = brdata->archive->FindCouponByID(p->tender_id);
                            else
                                coupinfo = currSettings->FindCouponByID(p->tender_id);
                            if (coupinfo)
                                brdata->couponlist.Add(coupinfo->name.Value(), p->value);
                            break;
                        }
                    }
                }
            }
        }
        c = c->next;
        brdata->check = c;
        if (brdata->archive && c)
        {
            return 0; // continue work fn
        }
    }

    if (brdata->archive && brdata->archive->end_time <= brdata->end)
    {
        brdata->archive = brdata->archive->next;
        return 0; // continue work fn
    }

    // Totals
    int adjust =
        brdata->complist.Total() +
        brdata->meallist.Total() +
        brdata->discountlist.Total() +
        brdata->couponlist.Total() +
        brdata->item_comp;

    // calculate labor
    int labor_mins = 0;
    int labor_cost = 0;
    int job_mins[16];
    int job_cost[16];
    int labor_otmins = 0;
    int labor_otcost = 0;
    int job_otcost[16];
    int job_otmins[16];
    int i;
    for (i = 0; i < 16; ++i)
    {
        job_mins[i] = 0;
        job_cost[i] = 0;
        job_otmins[i] = 0;
        job_otcost[i] = 0;
    }

    LaborDB *ldb = &(sys->labor_db);
    ldb->FigureLabor(currSettings, brdata->start, brdata->end, 0, labor_mins, 
                     labor_cost, labor_otmins, labor_otcost);

    if (term->expand_labor)
	{
        int j;
        for (j = 1; JobValue[j] > 0; ++j)
		{
            ldb->FigureLabor(currSettings, brdata->start, brdata->end, JobValue[j], 
                             job_mins[j], job_cost[j], job_otmins[j], job_otcost[j]);
		}
	}

    // Setup
    genericChar str[256];
    int last_pos    = 34;
    int percent_pos = 36;
    Flt per;

    int color = COLOR_DEFAULT;
    if (brdata->end > SystemTime)
        color = COLOR_DK_RED;

    // Report Entries
    for (int g = SALESGROUP_FOOD; g <= SALESGROUP_ROOM; ++g)
	{
        if (currSettings->IsGroupActive(g) || brdata->group_sales[g] != 0)
        {
            if (term->hide_zeros == 0 || brdata->group_sales[g] != 0)
            {
                sprintf(str, "%s Sales", SalesGroupName[g]);
                thisReport->TextL(str);
                thisReport->TextPosR(last_pos, term->FormatPrice(brdata->group_sales[g]), color);
                
                per = 0;
                if (brdata->sales > 0)
                    per = 100.0 * ((Flt) brdata->group_sales[g] / (Flt) brdata->sales);
                
                sprintf(str, "%.2f%%", per);
                
                thisReport->TextPosL(percent_pos, str, COLOR_DK_BLUE);
                thisReport->NewLine();
            }
        }
	}

    thisReport->TextPosL(3, "Total Sales");
    thisReport->TextPosR(last_pos, term->FormatPrice(brdata->sales), color);
    thisReport->TextPosL(percent_pos, "100%", COLOR_DK_BLUE);
    thisReport->NewLine();

    // Adjustments
    thisReport->NewLine();
    thisReport->Mode(PRINT_BOLD);
    thisReport->TextL("Goodwill Adjustments", COLOR_DK_BLUE);
    thisReport->NewLine();
    thisReport->Mode(0);
    if ((term->hide_zeros == 0) || (brdata->complist.Total() != 0))
    {
        thisReport->TextL("Whole Check Comps");
        if (term->expand_goodwill == 0)
            thisReport->TextPosR(last_pos, term->FormatPrice(brdata->complist.Total()), color);
        thisReport->NewLine();
        if (term->expand_goodwill)
        {
            MediaList *comps = &(brdata->complist);
            while (comps != NULL)
            {
                if (comps->name[0] != '\0' && (comps->total != 0 || term->hide_zeros == 0))
                {
                    thisReport->TextPosL(3, comps->name);
                    thisReport->TextPosR(last_pos, term->FormatPrice(comps->total), color);
                    thisReport->NewLine();
                }
                comps = comps->next;
            }
        }
    }

    if ((term->hide_zeros == 0) || (brdata->item_comp != 0))
    {
        thisReport->TextL("Line Item Comps");
        thisReport->TextPosR(last_pos, term->FormatPrice(brdata->item_comp), color);
        thisReport->NewLine();
    }

    if ((term->hide_zeros == 0) || (brdata->meallist.Total() != 0))
    {
        thisReport->TextL("Employee Discounts");
        if (term->expand_goodwill == 0)
            thisReport->TextPosR(last_pos, term->FormatPrice(brdata->meallist.Total()), color);
        thisReport->NewLine();
        if (term->expand_goodwill)
        {
            MediaList *meals = &(brdata->meallist);
            while (meals != NULL)
            {
                if (meals->name[0] != '\0' && (meals->total != 0 || term->hide_zeros == 0))
                {
                    thisReport->TextPosL(3, meals->name);
                    thisReport->TextPosR(last_pos, term->FormatPrice(meals->total), color);
                    thisReport->NewLine();
                }
                meals = meals->next;
            }
        }
    }

    if ((term->hide_zeros == 0) || (brdata->discountlist.Total() != 0))
    {
        thisReport->TextL("Customer Discounts");
        if (term->expand_goodwill == 0)
            thisReport->TextPosR(last_pos, term->FormatPrice(brdata->discountlist.Total()), color);
        thisReport->NewLine();
        if (term->expand_goodwill)
        {
            MediaList *discounts = &(brdata->discountlist);
            while (discounts != NULL)
            {
                if (discounts->name[0] != '\0' && (discounts->total != 0 || term->hide_zeros == 0))
                {
                    thisReport->TextPosL(3, discounts->name);
                    thisReport->TextPosR(last_pos, term->FormatPrice(discounts->total), color);
                    thisReport->NewLine();
                }
                discounts = discounts->next;
            }
        }
    }

    if ((term->hide_zeros == 0) || (brdata->couponlist.Total() != 0))
    {
        thisReport->TextL("Coupons");
        if (term->expand_goodwill == 0)
            thisReport->TextPosR(last_pos, term->FormatPrice(brdata->couponlist.Total()), color);
        thisReport->NewLine();
        if (term->expand_goodwill)
        {
            MediaList *coupons = &(brdata->couponlist);
            while (coupons != NULL)
            {
                if (coupons->name[0] != '\0' && (coupons->total != 0 || term->hide_zeros == 0))
                {
                    thisReport->TextPosL(3, coupons->name);
                    thisReport->TextPosR(last_pos, term->FormatPrice(coupons->total), color);
                    thisReport->NewLine();
                }
                coupons = coupons->next;
            }
        }
    }

    thisReport->TextPosL(3, "Total Adjustments");
    per = 0;
    if (brdata->sales > 0)
        per = (Flt) adjust / (Flt) brdata->sales;

    thisReport->TextPosR(last_pos, term->FormatPrice(adjust), color);
    sprintf(str, "%.2f%%", per * 100.0);
    thisReport->TextPosL(percent_pos, str, COLOR_DK_BLUE);
    thisReport->NewLine(2);

    thisReport->TextL("Cash Sales");
    thisReport->TextPosR(last_pos, term->FormatPrice(brdata->sales - adjust, 1), color);
    sprintf(str, "%.2f%%", (1 - per) * 100.0);
    thisReport->TextPosL(percent_pos, str, COLOR_DK_BLUE);
    thisReport->NewLine(2);

    // Guest Info
    int normal_sales = brdata->sales - (brdata->takeout_sales + brdata->fastfood_sales);
    if (term->hide_zeros == 0 || brdata->guests != 0)
    {
        thisReport->TextL("Guest Count");
        thisReport->NumberPosR(last_pos, brdata->guests, color);
        thisReport->NewLine();
        thisReport->TextL("Average Guest");
        if(brdata->guests > 0)
        {
            thisReport->TextPosR(last_pos, term->FormatPrice(normal_sales / brdata->guests, 1), color);
        }
        else
        {
            thisReport->TextPosR(last_pos, term->FormatPrice(0, 1), color);
        }
        thisReport->NewLine();
    }

    // Takeout Info
    if (term->hide_zeros == 0 || brdata->takeout != 0)
    {
        thisReport->TextL("Takeout Orders");
        thisReport->NumberPosR(last_pos, brdata->takeout, color);
        thisReport->NewLine();
        thisReport->TextL("Average Takeout");
        if(brdata->takeout > 0)
            thisReport->TextPosR(last_pos, term->FormatPrice(brdata->takeout_sales / brdata->takeout, 1), color);
        else
            thisReport->TextPosR(last_pos, term->FormatPrice(0, 1), color);
        thisReport->NewLine();
    }

    // fastfood Info
    if (term->hide_zeros == 0 || brdata->fastfood != 0)
    {
        thisReport->TextL("FastFood Orders");
        thisReport->NumberPosR(last_pos, brdata->fastfood, color);
        thisReport->NewLine();
        thisReport->TextL("Average FastFood");
        if(brdata->fastfood > 0)
            thisReport->TextPosR(last_pos, term->FormatPrice(brdata->fastfood_sales / brdata->fastfood, 1), color);
        else
            thisReport->TextPosR(last_pos, term->FormatPrice(0, 1), color);
        thisReport->NewLine();
    }

    thisReport->NewLine();

    // Labor Info
    if (term->hide_zeros == 0 || labor_cost != 0)
    {
        thisReport->TextL("Regular Hours");
        sprintf(str, "%.1f", (Flt) labor_mins / 60.0);
        thisReport->TextPosR(last_pos, str, color);
        thisReport->NewLine();
        thisReport->TextL("Regular Cost");
        thisReport->TextPosR(last_pos, term->FormatPrice(labor_cost, 1), color);
        thisReport->NewLine();
    }

    if (term->hide_zeros == 0 || labor_otcost != 0)
    {
        thisReport->TextL("Overtime Hours");
        sprintf(str, "%.1f", (Flt) labor_otmins / 60.0);
        thisReport->TextPosR(last_pos, str, color);
        thisReport->NewLine();
        thisReport->TextL("Overtime Cost");
        thisReport->TextPosR(last_pos, term->FormatPrice(labor_otcost, 1), color);
        thisReport->NewLine();
    }

    if (term->expand_labor)
    {
        int j = 1;
        while (JobValue[j] > 0)
        {
            if (currSettings->job_active[JobValue[j]] && (term->hide_zeros == 0 || job_mins[j] != 0))
            {
                sprintf(str, "%s Hours", JobName[j]);
                thisReport->TextL(str);
                sprintf(str, "%.1f", (Flt) job_mins[j] / 60.0);
                thisReport->TextPosR(last_pos, str, color);
                thisReport->NewLine();

                if (job_otmins[j] > 0)
                {
                    thisReport->TextL("Overtime");
                    sprintf(str, "%.1f", (Flt) job_otmins[j] / 60.0);
                    thisReport->TextPosR(last_pos, str, color);
                    thisReport->NewLine();
                }
            }
            ++j;
        }
        thisReport->TextPosL(3, "Total Hours");
    }
    else
        thisReport->TextL("Total Hours");
    sprintf(str, "%.1f", (Flt) (labor_mins + labor_otmins) / 60.0);
    thisReport->TextPosR(last_pos, str, color);
    thisReport->NewLine();

    if (term->expand_labor)
    {
        int j = 1;
        while (JobValue[j] > 0)
        {
            if (currSettings->job_active[JobValue[j]] && (term->hide_zeros == 0 || job_mins[j] != 0))
            {
                sprintf(str, "%s Cost", JobName[j]);
                thisReport->TextL(str);
                thisReport->TextPosR(last_pos, term->FormatPrice(job_cost[j], 1), color);
                thisReport->NewLine();

                if (job_otcost[j] > 0)
                {
                    thisReport->TextL("Overtime");
                    thisReport->TextPosR(last_pos, term->FormatPrice(job_otcost[j], 1), color);
                    thisReport->NewLine();
                }
            }
            ++j;
        }
        thisReport->TextPosL(3, "Total Cost");
    }
    else
        thisReport->TextL("Total Cost");
    thisReport->TextPosR(last_pos, term->FormatPrice(labor_cost + labor_otcost, 1), color);
    thisReport->NewLine(2);

    thisReport->TextL("Sales/Wage Hour");
    int x = 0;
    if (labor_mins > 0)
    {
        int tmp = labor_mins + labor_otmins;
        if (tmp > 0)
            x = FltToPrice(PriceToFlt(brdata->sales * 60) / (Flt) tmp);
    }
    thisReport->TextPosR(last_pos, term->FormatPrice(x, 1), color);

    thisReport->NewLine();
    thisReport->TextL("Sales/Wage Dollar");

    x = 0;
    if (labor_cost > 0)
        x = (int) (((Flt)(brdata->sales * 100)) / ((Flt)(labor_cost + labor_otcost)));
    thisReport->TextPosR(last_pos, term->FormatPrice(x, 1), color);

    thisReport->NewLine();
    thisReport->TextL("Labor Cost %");

    Flt f = 0;
    if (brdata->sales > 0)
        f = (Flt) ((labor_cost + labor_otcost) * 100) / (Flt) brdata->sales;
    sprintf(str, "%.2f%%", f);
    thisReport->TextPosR(last_pos, str, color);

    thisReport->is_complete = 1;
    brdata->term->Update(UPDATE_REPORT, NULL);
    delete brdata;

    return 1;  // end work fn
}

#define BALANCE_TITLE "Revenue and Productivity"
int System::BalanceReport(Terminal *term, TimeInfo &start_time, TimeInfo &end_time, Report *report)
{
    FnTrace("System::BalanceReport()");
    if (report == NULL)
        return 1;

    report->SetTitle(BALANCE_TITLE);
    report->Mode(PRINT_BOLD | PRINT_LARGE);
    report->TextC(BALANCE_TITLE);
    report->NewLine();
    report->TextC(term->GetSettings()->store_name.Value());
    report->NewLine();
    report->Mode(0);
    TimeInfo end;
    if (end_time.IsSet())
        end = end_time;
    else
        end = SystemTime;

    if (end >= SystemTime)
        report->update_flag |= UPDATE_MINUTE | UPDATE_SALE;

    BRData *brdata = new BRData;
    brdata->report  = report;
    brdata->start   = start_time;
    brdata->end     = end;
    brdata->term    = term;
    brdata->system  = this;
    brdata->archive = FindByTime(start_time);

    // report setup
    genericChar str[256];
    genericChar str2[32];
    report->is_complete = 0;
    sprintf(str, "%s  --  %s", term->TimeDate(str2, brdata->start, TD0),
            term->TimeDate(brdata->end, TD0));
    report->TextC(str, COLOR_DK_BLUE);
    report->NewLine(3);

    AddWorkFn((WorkFn) BalanceReportWorkFn, brdata);

    return 0;
}


/** Deposit Report **/
#define DEPOSIT_TITLE "Final Balance Report"
int System::DepositReport(Terminal *term, TimeInfo &start_time, 
                          TimeInfo &end_time, Archive *archive, 
                          Report *report)
{
    FnTrace("System::DepositReport()");
    if (report == NULL)
        return 1;

    report->SetTitle(DEPOSIT_TITLE);
    report->update_flag = UPDATE_ARCHIVE | UPDATE_CHECKS | UPDATE_SERVER;
    term->SetCursor(CURSOR_WAIT);
    Settings *s = &settings;
    TimeInfo end;
    if (end_time.IsSet())
        end = end_time;
    else
        end = SystemTime;
    if (end >= SystemTime)
        report->update_flag |= UPDATE_MINUTE;

    Archive *start_a;
    if (archive)
        start_a = archive;
    else
        start_a = FindByTime(start_time);

    MediaList couponlist;
    MediaList discountlist;
    MediaList creditcardlist;
    MediaList complist;
    MediaList meallist;

    int sales[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int tax_food    = 0;
    int tax_alcohol = 0;
    int tax_room    = 0;
    int tax_merchandise = 0;
	int tax_GST			= 0;
	int tax_PST			= 0;
	int tax_HST			= 0;
	int tax_QST			= 0;
    int tax_VAT         = 0;
    int total_sales = 0;
    int drawer_diff = 0;
    int cash        = 0;
    int check       = 0;
    int item_comp   = 0;
    int gift        = 0;
    int account     = 0;
    int room_charge = 0;
    int overage     = 0;
    int expenses    = 0;
    int tips_paid   = 0;
    int tips_held   = 0;
    int captured_tips = 0;
    int paid_tips     = 0;
    int charged_tips  = 0;
    int visa        = 0;
    int mastercard  = 0;
    int amex        = 0;
    int diners      = 0;
    int debit       = 0;

    int incomplete = 0;
    if (start_time < SystemTime)
    {
        // It's not very clear here, but this block uses archives only if there
        // is an archive currently defined.  FirstCheck() and FirstDrawer() don't
        // automatically get their results from the archive.  They get their
        // results from the archive if it exists, or the current system data
        // otherwise.
        Archive *a = start_a;
        for (;;)
        {
            Check *firstcheck = FirstCheck(a);
            // Scan checks
            for (Check *c = firstcheck; c != NULL; c = c->next)
            {
                if (c->IsTraining() > 0)
                    continue;
                if (c->Status() != CHECK_CLOSED && c->CustomerType() != CHECK_HOTEL)
					continue;

                TimeInfo *timevar = c->TimeClosed();
                if (timevar != NULL && *timevar >= start_time && *timevar < end_time)
                {
                    // bury the incomplete check here.  That way it won't be incomplete just
                    // because we hit the end of the archives, but because we hit the end
                    // of the archives and still had qualifying data.
                    if (a == NULL)
                        incomplete = 1;
                    for (SubCheck *subcheck = c->SubList();
                         subcheck != NULL;
                         subcheck = subcheck->next)
                    {
                        if (subcheck->settle_time.IsSet() &&
                            subcheck->settle_time > start_time &&
                            subcheck->settle_time < end_time)
                        {
                            for (int g = SALESGROUP_FOOD; g <= SALESGROUP_ROOM; ++g)
                                sales[g] += subcheck->GrossSales(c, s, g);
                            total_sales     += subcheck->total_sales;
                            if (subcheck->IsTaxExempt() == 0)
                            {
                                tax_food        += subcheck->total_tax_food;
                                tax_alcohol     += subcheck->total_tax_alcohol;
                                tax_room        += subcheck->total_tax_room;
                                tax_merchandise += subcheck->total_tax_merchandise;
                                tax_GST         += subcheck->total_tax_GST;
                                tax_PST         += subcheck->total_tax_PST;
                                tax_HST         += subcheck->total_tax_HST;
                                tax_QST         += subcheck->total_tax_QST;
                                tax_VAT         += subcheck->total_tax_VAT;
                            }
                            item_comp       += subcheck->item_comps;
                        }
                    }
                }
			}

            // Scan drawers
            Drawer *drawer = FirstDrawer(a);
            do {
                // the drawer must exist and it either must be today's drawer while today is
                // being processed (incomplete != 0) or it must fit into the date range
                // of the report.
                if (drawer != NULL &&
                    (incomplete ||
                     (drawer->balance_time >= start_time &&
                      drawer->balance_time < end_time)))
                {
                    drawer->Total(firstcheck, 1);
                    CreditCardInfo *credinfo = a ? a->CreditCardList() : s->CreditCardList();
                    while (credinfo != NULL)
                    {
                        int balance = 0;
                        if (drawer)
                            balance = drawer->Balance(TENDER_CHARGE_CARD, credinfo->id);
                        creditcardlist.Add(credinfo->name.Value(), balance);
                        credinfo = credinfo->next;
                    }

                    DiscountInfo *discinfo = a ? a->DiscountList() : s->DiscountList();
                    while (discinfo != NULL)
                    {
                        int balance = 0;
                        if (drawer)
                            balance = drawer->Balance(TENDER_DISCOUNT, discinfo->id);
                        discountlist.Add(discinfo->name.Value(), balance);
                        discinfo = discinfo->next;
                    }

                    CouponInfo *coupinfo = a ? a->CouponList() : s->CouponList();
                    while (coupinfo != NULL)
                    {
                        int balance = 0;
                        if (drawer)
                            balance = drawer->Balance(TENDER_COUPON, coupinfo->id);
                        couponlist.Add(coupinfo->name.Value(), balance);
                        coupinfo = coupinfo->next;
                    }

                    CompInfo *compinfo = a ? a->CompList() : s->CompList();
                    while (compinfo != NULL)
                    {
                        int balance = 0;
                        if (drawer)
                            balance = drawer->Balance(TENDER_COMP, compinfo->id);
                        complist.Add(compinfo->name.Value(), balance);
                        compinfo = compinfo->next;
                    }

                    MealInfo *mealinfo = a ? a->MealList() : s->MealList();
                    while (mealinfo != NULL)
                    {
                        int balance = 0;
                        if (drawer)
                            balance = drawer->Balance(TENDER_EMPLOYEE_MEAL, mealinfo->id);
                        meallist.Add(mealinfo->name.Value(), balance);
                        mealinfo = mealinfo->next;
                    }
                    
                    captured_tips += drawer->TotalBalance(TENDER_CAPTURED_TIP);
                    paid_tips     += drawer->TotalBalance(TENDER_PAID_TIP);
                    charged_tips  += drawer->TotalBalance(TENDER_CHARGED_TIP);
                    cash          += drawer->TotalBalance(TENDER_CASH_AVAIL);
                    check         += drawer->TotalBalance(TENDER_CHECK);
                    gift          += drawer->TotalBalance(TENDER_GIFT);
                    account       += drawer->TotalBalance(TENDER_ACCOUNT);
                    room_charge   += drawer->TotalBalance(TENDER_CHARGE_ROOM);
                    overage       += drawer->TotalBalance(TENDER_OVERAGE);
                    expenses      += drawer->TotalBalance(TENDER_EXPENSE);
                    debit         += drawer->TotalBalance(TENDER_DEBIT_CARD);
                    visa          += drawer->Balance(TENDER_CREDIT_CARD, CREDIT_TYPE_VISA);
                    mastercard    += drawer->Balance(TENDER_CREDIT_CARD, CREDIT_TYPE_MASTERCARD);
                    amex          += drawer->Balance(TENDER_CREDIT_CARD, CREDIT_TYPE_AMEX);
                    diners        += drawer->Balance(TENDER_CREDIT_CARD, CREDIT_TYPE_DINERSCLUB);
                    drawer_diff   += drawer->total_difference;
                    
                    // skip ahead to the next drawer
                }
                if (drawer)
                    drawer = drawer->next;
            } while (drawer != NULL);

            // Scan Tips
            if (a)
            {
                a->tip_db.Total();
                tips_paid += a->tip_db.total_paid;
                tips_held += a->tip_db.total_held;
            }
            else
            {
                tip_db.Total();
                tips_paid += tip_db.total_paid;
                tips_held += tip_db.total_held;
            }

            if (a == NULL || archive)
                break;
            if (a->end_time >= end)
            {
                end = a->end_time;
                break;
            }
            a = a->next;
        }
    }

    if (tips_paid == 0 && captured_tips > 0)
    {
        tips_paid = paid_tips;
        tips_held = captured_tips + charged_tips;
    }

    // Figure totals
    int total_tax    = tax_food + tax_alcohol + tax_room +
        tax_merchandise + tax_GST + tax_PST + tax_HST +
        tax_QST + tax_VAT;
    int total_adjust = item_comp;
    total_adjust += complist.Total();
    total_adjust += discountlist.Total();
    total_adjust += couponlist.Total();
    total_adjust += meallist.Total();
    
    // Make report
    genericChar str[256];
    int  col = COLOR_DEFAULT;
    report->Mode(PRINT_BOLD | PRINT_LARGE);
    report->TextC(DEPOSIT_TITLE, COLOR_DK_BLUE);
    report->NewLine();
    report->TextC(s->store_name.Value());
    report->NewLine();
    report->Mode(0);

    report->TextPosR(6, "Start:");
    if (archive && archive->start_time.IsSet())
        term->TimeDate(str, archive->start_time, TD3);
    else if (start_time.IsSet())
    {
        if (start_time > SystemTime)
            col = COLOR_DK_RED;
        term->TimeDate(str, start_time, TD3);
    }
    else
        strcpy(str, "System Start");

    report->TextPosL(7, str, col);
    report->NewLine();

    report->TextPosR(6, "End:");
    if (incomplete)
        col = COLOR_DK_RED;

    if (archive)
        term->TimeDate(str, archive->end_time, TD3);
    else
        term->TimeDate(str, end, TD3);

    report->TextPosL(7, str, col);
    report->NewLine(2);

    // Sales
    report->Mode(PRINT_BOLD);
    report->TextL("Revenue Group Totals", COLOR_DK_BLUE);
    report->NewLine();
    report->Mode(0);
    for (int g = SALESGROUP_FOOD; g <= SALESGROUP_ROOM; ++g)
	{
        if (s->IsGroupActive(g) || sales[g] != 0)
        {
            report->TextL(SalesGroupName[g]);
            report->TextPosR(-6, term->FormatPrice(sales[g]), col);
            report->NewLine();
        }
	}

    report->TextPosL(3, "All Revenue");
    report->TextPosR(0, term->FormatPrice(total_sales), col);
    report->NewLine();

    // Tax
    report->TextL("Sales Tax: Food");
    report->TextPosR(-6, term->FormatPrice(tax_food), col);
    report->NewLine();

    report->TextL("Sales Tax: Alcohol");
    report->TextPosR(-6, term->FormatPrice(tax_alcohol), col);
    report->NewLine();

    report->TextL("Sales Tax: Room");
    report->TextPosR(-6, term->FormatPrice(tax_room), col);
    report->NewLine();

    report->TextL("Sales Tax: Merchandise");
    report->TextPosR(-6, term->FormatPrice(tax_merchandise), col);
    report->NewLine();
	
    report->TextL("Sales Tax: GST");
    report->TextPosR(-6, term->FormatPrice(tax_GST), col);
    report->NewLine();

    report->TextL("Sales Tax: PST");
    report->TextPosR(-6, term->FormatPrice(tax_PST), col);
    report->NewLine();

    report->TextL("Sales Tax: HST");
    report->TextPosR(-6, term->FormatPrice(tax_HST), col);
    report->NewLine();

    report->TextL("Sales Tax: QST");
    report->TextPosR(-6, term->FormatPrice(tax_QST), col);
    report->NewLine();

    report->TextL("Value Added Tax");
    report->TextPosR(-6, term->FormatPrice(tax_VAT), col);
    report->NewLine();

    report->TextPosL(3, "All Taxes");

    report->TextPosR(0, term->FormatPrice(total_tax), col);
    report->UnderlinePosR(0, 7, col);
    report->NewLine();

    report->TextPosL(3, "Gross Receipts");
    report->TextR(term->FormatPrice(total_sales + total_tax), col);
    report->NewLine(2);

    // Adjustments
    report->Mode(PRINT_BOLD);
    report->TextL("Adjustments", COLOR_DK_BLUE);
    report->NewLine();
    report->Mode(0);

    MediaList *mediacomp = &complist;
    while (mediacomp != NULL)
    {
        if (mediacomp->name[0] != '\0')
        {
            report->TextL(mediacomp->name);
            report->TextPosR(-6, term->FormatPrice(mediacomp->total), col);
            report->NewLine();
        }
        mediacomp = mediacomp->next;
    }

    report->TextL("Line Item Comps");
    report->TextPosR(-6, term->FormatPrice(item_comp), col);
    report->NewLine();

    MediaList *mediameal = &meallist;
    while (mediameal != NULL)
    {
        if (mediameal->name[0] != '\0')
        {
            report->TextL(mediameal->name);
            report->TextPosR(-6, term->FormatPrice(mediameal->total), col);
            report->NewLine();
        }
        mediameal = mediameal->next;
    }

    MediaList *mediadiscount = &discountlist;
    while (mediadiscount != NULL)
    {
        if (mediadiscount->name[0] != '\0')
        {
            report->TextL(mediadiscount->name);
            report->TextPosR(-6, term->FormatPrice(mediadiscount->total), col);
            report->NewLine();
        }
        mediadiscount = mediadiscount->next;
    }

    MediaList *mediacoupon = &couponlist;
    while (mediacoupon != NULL)
    {
        if (mediacoupon->name[0] != '\0')
        {
            report->TextL(mediacoupon->name);
            report->TextPosR(-6, term->FormatPrice(mediacoupon->total), col);
            report->NewLine();
        }
        mediacoupon = mediacoupon->next;
    }

    report->TextPosL(3, "All Adjustments");
    report->TextR(term->FormatPrice(-total_adjust), col);
    report->NewLine(2);

    report->Mode(PRINT_BOLD);
    report->TextPosL(0, "Other Transactions", COLOR_DK_BLUE);
    report->NewLine();
    report->Mode(0);

    report->TextL("Captured Tips Held");
    report->TextPosR(-6, term->FormatPrice(tips_held), col);
    report->NewLine();

    report->TextL("Charged on Account");
    report->TextPosR(-6, term->FormatPrice(-account), col);
    report->NewLine();

    report->TextL("Charged to Room");
    report->TextPosR(-6, term->FormatPrice(-room_charge), col);
    report->NewLine();

    report->TextL("Received on Account");
    report->TextPosR(-6, term->FormatPrice(0), col);
    report->NewLine();

    report->TextL("Certif's Redeemed");
    report->TextPosR(-6, term->FormatPrice(-gift), col);
    report->NewLine();

    report->TextL("Certif's Sold");
    report->TextPosR(-6, term->FormatPrice(0), col);
    report->NewLine();

    report->TextL(term->Translate("Expenses Paid From Cash"));
    report->TextPosR(-6, term->FormatPrice(expenses), col);
    report->NewLine();

    report->TextL(term->Translate("Overage"));
    report->TextPosR(-6, term->FormatPrice(overage), col);
    report->NewLine();

    int sub = -room_charge + -account + -gift + tips_held + -expenses;
    report->TextPosL(3, term->Translate("Subtotal"));
    report->TextPosR(0, term->FormatPrice(sub), col);
    report->UnderlinePosR(0, 7, col);
    report->NewLine();

    report->TextPosL(3, term->Translate("Receipts To Account For"));
    report->TextR(term->FormatPrice(total_sales + total_tax - total_adjust + sub), col);
    report->NewLine(2);

    report->Mode(PRINT_BOLD);
    report->TextL(term->Translate("Media Accounted For"), COLOR_DK_BLUE);
    report->NewLine();
    report->Mode(0);

    // Cash Deposit
    report->TextL(term->Translate("Cash"));
    report->TextPosR(-6, term->FormatPrice(cash), col);
    report->NewLine();

    // Expenses
    report->TextL(term->Translate("Expenses"));
    report->TextPosR(-6, term->FormatPrice(-expenses), col);
    report->NewLine();
    //cash -= expenses;

    report->TextL(term->Translate("Checks"));
    report->TextPosR(-6, term->FormatPrice(check), col);
    report->NewLine();

    report->TextPosL(3, term->Translate("All Cash & Checks"));
    report->TextPosR(0, term->FormatPrice(cash + check), col);
    report->NewLine();

    // Credit
    int total_credit = 0;
    if (settings.authorize_method == CCAUTH_NONE)
    {
        MediaList *mediacredit = &creditcardlist;
        while (mediacredit != NULL)
        {
            if (mediacredit->name[0] != '\0')
            {
                report->TextL(mediacredit->name);
                report->TextPosR(-6, term->FormatPrice(mediacredit->total), col);
                report->NewLine();
                total_credit += mediacredit->total;
            }
            mediacredit = mediacredit->next;
        }
    }
    else
    {
        report->TextL("Visa");
        report->TextPosR(0, term->FormatPrice(visa), col);
        report->NewLine();

        report->TextL("MasterCard");
        report->TextPosR(0, term->FormatPrice(mastercard), col);
        report->NewLine();

        report->TextL("American Express");
        report->TextPosR(0, term->FormatPrice(amex), col);
        report->NewLine();

        report->TextL("Diners");
        report->TextPosR(0, term->FormatPrice(diners), col);
        report->NewLine();

        report->TextL("Debit");
        report->TextPosR(0, term->FormatPrice(debit), col);
        report->NewLine();


        total_credit = visa + mastercard + amex + diners + debit;
    }

    report->TextPosL(3, "All Credit Cards");
    report->TextPosR(0, term->FormatPrice(total_credit), col);
    report->UnderlinePosR(0, 7, col);
    report->NewLine();

    report->TextPosL(3, "Total");
    report->TextPosR(0, term->FormatPrice(cash + check + total_credit), col);
    report->NewLine(2);

    if (tips_held > 0)
        sprintf(str, "Set Aside %s Of Held Tips.", term->FormatPrice(tips_held, 1));
    else if (tips_held < 0)
        sprintf(str, "Tips Overpaid By %s.", term->FormatPrice(-tips_held, 1));
    else
        strcpy(str, "All Captured Tips Have Been Paid.");

    report->Mode(PRINT_UNDERLINE);
    report->TextPosL(0, str, col);
    report->Mode(0);
    report->NewLine();

    report->TextL("Captured Tips Paid");
    report->TextPosR(-6, term->FormatPrice(tips_paid), col);
    report->NewLine(2);

    if (settings.authorize_method == CCAUTH_NONE)
    {
        report->TextPosL(3, "Total Deposit");
        report->TextPosR(0, term->FormatPrice(cash + check + total_credit - tips_held), col);
        report->NewLine(2);
    }
    else
    {
        report->TextPosL(3, "Total Cash Deposit");
        report->TextPosR(0, term->FormatPrice(cash + check - tips_held), col);
        report->NewLine();

        report->TextPosL(3, "Total Debit/Credit Deposit");
        report->TextPosR(0, term->FormatPrice(total_credit), col);
        report->NewLine(2);
    }

    if (drawer_diff < 0)
    {
        report->TextL("Combined Drawers Short By");
        report->TextPosR(-6, term->FormatPrice(-drawer_diff), col);
    }
    else if (drawer_diff > 0)
    {
        report->TextL("Combined Drawers Over By");
        report->TextPosR(-6, term->FormatPrice(drawer_diff), col);
    }
    else
    {
        report->Mode(PRINT_BOLD);
        report->TextC("Combined Drawers Are Balanced.");
        report->Mode(0);
    }
    term->SetCursor(CURSOR_POINTER);
    report->is_complete = 1;

    return 0;
}

/** ClosedCheck Report **/
class CCRData
{
public:
    Report *report;
    TimeInfo start;
    TimeInfo end;
    Terminal *term;
    System   *system;
    Archive  *archive;
    Check    *check;
    int user_id;
    int training;
    int none;
    int total_amount;
    int total_guests;
    int total_number;

    // Constructor
    CCRData()
    {
        report = NULL;
        none = 1;
        total_amount = 0;
        total_guests = 0;
        total_number = 0;
        term = NULL;
        system = NULL;
        archive = NULL;
        check = NULL;
        user_id = 0;
        training = 0;
    }
};

int ClosedCheckReportWorkFn(CCRData *ccrdata)
{
    FnTrace("ClosedCheckReportWorkFn()");
    Terminal *term = ccrdata->term;
    Report   *thisReport = ccrdata->report;
    System *sys = ccrdata->system;
    Settings *s = &(sys->settings);
    genericChar str[256];

    Check *thisCheck = ccrdata->check;
    if (thisCheck == NULL)
        thisCheck = sys->FirstCheck(ccrdata->archive);

    while (thisCheck)
    {
        if ((ccrdata->user_id == 0 && thisCheck->IsTraining() == ccrdata->training) ||
            thisCheck->WhoGetsSale(s) == ccrdata->user_id)
        {
            int amount = 0, flag = 0;
            for (SubCheck *sc = thisCheck->SubList(); sc != NULL; sc = sc->next)
                if (sc->status == CHECK_CLOSED && sc->settle_time < ccrdata->end &&
                    sc->settle_time >= ccrdata->start)
                {
                    amount += sc->total_sales;
                    flag = 1;
                }

            if (flag)
            {
                sprintf(str, "%06d", thisCheck->serial_number);
                thisReport->TextPosL(0, str);
                ccrdata->none = 0;
                ccrdata->total_amount += amount;
                ccrdata->total_guests += thisCheck->Guests();
                ++ccrdata->total_number;

                if (thisCheck->IsTakeOut())
                {
                    thisReport->TextPosL(7, "TERM.O");
                    thisReport->TextPosL(11, "--");
                }
				else if (thisCheck->IsFastFood())
				{
					thisReport->TextPosL(7, "FF");
                    thisReport->TextPosL(11, "--");
				}
                else
                {
                    thisReport->TextPosL(7, thisCheck->Table());
                    thisReport->NumberPosL(11, thisCheck->Guests());
                }
                thisReport->TextPosR(22, term->FormatPrice(amount));
                thisReport->TextPosL(23, thisCheck->PaymentSummary(term));
                thisReport->NewLine();
            }
        }
        thisCheck = thisCheck->next;
        ccrdata->check = thisCheck;
        if (ccrdata->archive && thisCheck)
            return 0; // continue work fn
    }

    if (ccrdata->archive && ccrdata->archive->end_time <= ccrdata->end)
    {
        ccrdata->archive = ccrdata->archive->next;
        return 0; // continue work fn
    }

    thisReport->NewLine();
    thisReport->TextPosL(0, "Total");
    thisReport->NumberPosL(7, ccrdata->total_number);
    thisReport->NumberPosL(11, ccrdata->total_guests);
    thisReport->TextPosR(22, term->FormatPrice(ccrdata->total_amount));

    thisReport->is_complete = 1;
    ccrdata->term->Update(UPDATE_REPORT, NULL);
    delete ccrdata;
    return 1;  // end work fn
}

int System::ClosedCheckReport(Terminal *term, TimeInfo &start_time, TimeInfo &end_time,
                              Employee *thisEmployee, Report *thisReport)
{
    FnTrace("System::ClosedCheckReport()");
    if (thisReport == NULL)
        return 1;

    thisReport->update_flag = UPDATE_ARCHIVE | UPDATE_CHECKS | UPDATE_SERVER;
    TimeInfo end;
    if (end_time.IsSet())
        end = end_time;
    else
        end = SystemTime;

    if (end >= SystemTime)
        thisReport->update_flag |= UPDATE_MINUTE;

    CCRData *ccrdata = new CCRData;
    ccrdata->report  = thisReport;
    ccrdata->start   = start_time;
    ccrdata->end     = end;
    ccrdata->term    = term;
    ccrdata->system  = this;
    ccrdata->archive = FindByTime(start_time);
    if (thisEmployee)
        ccrdata->user_id = thisEmployee->id;

    // Setup report
    genericChar str[256], str2[32];

    thisReport->is_complete = 0;
    thisReport->TextC(term->Translate("Closed Check Summary"), COLOR_DK_BLUE);
    thisReport->NewLine();
    sprintf(str, "(%s  to  %s)", term->TimeDate(str2, start_time, TD5),
            term->TimeDate(end, TD5));
    thisReport->TextC(str, COLOR_DK_BLUE);
    thisReport->NewLine(2);

    thisReport->Mode(PRINT_UNDERLINE);
    thisReport->TextPosL(0,  "Check#", COLOR_DK_BLUE);
    thisReport->TextPosL(7,  "Tbl", COLOR_DK_BLUE);
    thisReport->TextPosL(11, "Gst", COLOR_DK_BLUE);
    thisReport->TextPosR(22, "Amount", COLOR_DK_BLUE);
    thisReport->TextPosL(23, "Payment", COLOR_DK_BLUE);
    thisReport->Mode(0);
    thisReport->NewLine();

    AddWorkFn((WorkFn) ClosedCheckReportWorkFn, ccrdata);
    return 0;
}


/** ItemException Report **/
int System::ItemExceptionReport(Terminal *term, TimeInfo &start_time,
                                TimeInfo &end_time, int type, 
                                Employee *thisEmployee, Report *thisReport)
{
    FnTrace("System::ItemExceptionReport()");
    if (thisReport == NULL)
        return 1;

    thisReport->update_flag = UPDATE_ARCHIVE | UPDATE_SERVER;
    term->SetCursor(CURSOR_WAIT);

    TimeInfo end;

    if (end_time.IsSet())
        end = end_time;
    else
        end = SystemTime;

    if (end >= SystemTime)
        thisReport->update_flag |= UPDATE_MINUTE;

    int user_id = 0;

    if (thisEmployee)
        user_id = thisEmployee->id;

    genericChar str[256];
    genericChar str2[32];

    if (type == EXCEPTION_COMP)
        strcpy(str, "Line Item Comps");
    else if (type == EXCEPTION_VOID)
        strcpy(str, "Line Item Voids");
    else
        strcpy(str, "Line Item Exceptions");

    thisReport->SetTitle(str);
    thisReport->TextC(term->Translate(str), COLOR_DK_BLUE);
    thisReport->NewLine();

    if (user_id > 0)
    {
        thisReport->TextC(term->UserName(user_id), COLOR_DK_BLUE);
        thisReport->NewLine();
    }

    sprintf(str, "(%s  to  %s)", term->TimeDate(str2, start_time, TD5),
            term->TimeDate(end, TD5));

    thisReport->TextC(str, COLOR_DK_BLUE);
    thisReport->NewLine(2);

    thisReport->Mode(PRINT_UNDERLINE);
    thisReport->TextPosL(0,  "Time", COLOR_DK_BLUE);
    thisReport->TextPosL(15, "By", COLOR_DK_BLUE);
    thisReport->TextPosL(27, "Item", COLOR_DK_BLUE);
    thisReport->TextR("Value", COLOR_DK_BLUE);
    thisReport->Mode(0);
    thisReport->NewLine();

    Settings *ptrSettings = &settings;
    Archive  *thisArchive = FindByTime(start_time);

    for (;;)
	{
		TimeInfo time_is;
        TimeInfo time_was;
		int id_is;
        int id_was;
		int	cost_is;
        int cost_was;
		short exception_is;
        short exception_was;
		short reason_is;
        short reason_was;
		const char* item_is = NULL;
        const char* item_was = NULL;

		ItemException *currException = FirstItemException(thisArchive);
		while (currException)
		{
			// initialize containers for current attributes
			time_is = currException->time;
			id_is = currException->user_id;
			cost_is = currException->item_cost;
			exception_is = currException->exception_type;
			reason_is = currException->reason;
			item_is = (const char* )currException->item_name.Value();

			// Duplicate Filter:
			// if exact same item at exact same time, it's a duplicate
			// (duplicate exceptions are a bug with the current
			// exception tracking.  Need to fix)
			bool IsSame  = ((time_is == time_was) && (item_is == item_was));

			if(! IsSame)
			{
				if ((id_is == user_id || user_id <= 0) &&
                    (exception_is == type || type <= 0) &&
                    time_is >= start_time && time_is < end)
				{
					thisReport->TextL(term->TimeDate(time_is, TD5)); 

					thisReport->TextPosL(15, term->UserName(id_is)); 
					thisReport->TextPosL(27, currException->item_name.Value());

					thisReport->TextR(term->FormatPrice(cost_is, 1));
					thisReport->NewLine();

					// find valid label for current reason code
					if (reason_is >= 0)
					{
						CompInfo *thisComp = ptrSettings->FindCompByID(reason_is);
						if (thisComp)
						{
							thisReport->TextPosL(2, thisComp->name.Value());
							thisReport->NewLine();
						}
					}

				}
			}

			// set up "sticky" vars to hold soon-to-be previous
			// exception info (for duplicate filter)

			time_was = time_is;
			id_was = id_is;
			cost_was = cost_is;
			exception_was = exception_is;
			reason_was = reason_is;
			item_was = item_is;

			currException = currException->next;
		}

		if (thisArchive == NULL || (thisArchive->end_time > end))
			break;

		thisArchive = thisArchive->next;
	}

    term->SetCursor(CURSOR_POINTER);

    return 0;
}

/** TableException Report **/
int System::TableExceptionReport(Terminal *term, TimeInfo &start_time,
                                 TimeInfo &end_time, Employee *e, Report *report)
{
    FnTrace("System::TableExceptionReport()");
    if (report == NULL)
        return 1;

    report->update_flag = UPDATE_ARCHIVE | UPDATE_SERVER;
    term->SetCursor(CURSOR_WAIT);
    TimeInfo end;
    if (end_time.IsSet())
        end = end_time;
    else
        end = SystemTime;
    if (end >= SystemTime)
        report->update_flag |= UPDATE_MINUTE;

    int user_id = 0;
    if (e)
        user_id = e->id;

    genericChar str[256], str2[32];
    report->TextC(term->Translate("Transfered Checks"), COLOR_DK_BLUE);
    report->NewLine();

    if (user_id > 0)
    {
        report->TextC(term->UserName(user_id), COLOR_DK_BLUE);
        report->NewLine();
    }

    sprintf(str, "(%s  to  %s)", term->TimeDate(str2, start_time, TD5),
            term->TimeDate(end, TD5));
    report->TextC(str, COLOR_DK_BLUE);
    report->NewLine(2);

    report->Mode(PRINT_UNDERLINE);
    report->TextPosL(0,  "Time", COLOR_DK_BLUE);
    report->TextPosL(15, "By", COLOR_DK_BLUE);
    report->TextPosL(27, "Table", COLOR_DK_BLUE);
    report->TextPosL(35, "From", COLOR_DK_BLUE);
    report->TextPosL(47, "To", COLOR_DK_BLUE);
    report->Mode(0);
    report->NewLine();

    Archive *a = FindByTime(start_time);
    for (;;)
    {
        TableException *te = FirstTableException(a);
        while (te)
        {
            if ((te->user_id == user_id || user_id <= 0) &&
                te->time >= start_time && te->time < end)
            {
                report->TextL(term->TimeDate(te->time, TD5));
                report->TextPosL(15, term->UserName(te->user_id));
                report->TextPosL(27, te->table.Value());
                report->TextPosL(35, term->UserName(te->source_id));
                report->TextPosL(47, term->UserName(te->target_id));
                report->NewLine();
            }
            te = te->next;
        }
        if (a == NULL || a->end_time > end)
            break;
        a = a->next;
    }
    term->SetCursor(CURSOR_POINTER);
    return 0;
}

/** RebuildException Report **/
int System::RebuildExceptionReport(Terminal *term, TimeInfo &start_time,
                                   TimeInfo &end_time, Employee *e, Report *report)
{
    FnTrace("System::RebuildExceptionReport()");
    if (report == NULL)
        return 1;

    report->update_flag = UPDATE_ARCHIVE | UPDATE_SERVER;
    term->SetCursor(CURSOR_WAIT);
    TimeInfo end;
    if (end_time.IsSet())
        end = end_time;
    else
        end = SystemTime;
    if (end >= SystemTime)
        report->update_flag |= UPDATE_MINUTE;

    int user_id = 0;
    if (e)
        user_id = e->id;

    genericChar str[256], str2[32];
    report->TextC(term->Translate("Closed Check Edits"), COLOR_DK_BLUE);
    report->NewLine();

    if (user_id > 0)
    {
        report->TextC(term->UserName(user_id), COLOR_DK_BLUE);
        report->NewLine();
    }

    sprintf(str, "(%s  to  %s)", term->TimeDate(str2, start_time, TD5),
            term->TimeDate(end, TD5));
    report->TextC(str, COLOR_DK_BLUE);
    report->NewLine(2);

    report->Mode(PRINT_UNDERLINE);
    report->TextPosL(0,  "Time", COLOR_DK_BLUE);
    report->TextPosL(15, "By", COLOR_DK_BLUE);
    report->TextPosL(27, "Check", COLOR_DK_BLUE);
    report->Mode(0);
    report->NewLine();

    Archive *a = FindByTime(start_time);
    for (;;)
    {
        RebuildException *re = FirstRebuildException(a);
        while (re)
        {
            if ((re->user_id == user_id || user_id <= 0) &&
                re->time >= start_time && re->time < end)
            {
                report->TextL(term->TimeDate(re->time, TD5));
                report->TextPosL(15, term->UserName(re->user_id));
                sprintf(str, "%06d", re->check_serial);
                report->TextPosL(27, str);
                report->NewLine();
            }
            re = re->next;
        }

        if (a == NULL || a->end_time > end)
            break;
        a = a->next;
    }
    term->SetCursor(CURSOR_POINTER);
    return 0;
}

int System::DrawerSummaryReport(Terminal *term, Drawer *my_drawer_list,
                                Check *my_check_list, Report *report)
{
    FnTrace("System::DrawerSummaryReport()");
    if (my_check_list == NULL || report == NULL)
        return 1;

    report->update_flag = UPDATE_ARCHIVE | UPDATE_CHECKS | UPDATE_SERVER;
    report->Mode(PRINT_BOLD);
    report->TextC("Drawer Summary Report");
    report->Mode(0);
    report->NewLine(2);

    int open = 0, closed = 0, diff = 0;
    for (Drawer *drawer = my_drawer_list; drawer != NULL; drawer = drawer->next)
	{
		drawer->Total(my_check_list);
		int status = drawer->Status();
		if (status == DRAWER_OPEN)
		{
			if (!drawer->IsEmpty())
				++open;
		}
		else
			++closed;
		diff += drawer->total_difference;
	}

    report->TextL("Open Drawers");
    report->NumberR(open);
    report->NewLine();

    report->TextL("Closed Drawers");
    report->NumberR(closed);
    report->NewLine();

    // FIX - finish drawer summary report
    return 0;
}

int System::CustomerDetailReport(Terminal *term, Employee *e, Report *report)
{
    FnTrace("System::CustomerDetailReport()");
    if (report == NULL || e == NULL)
        return 1;

    report->Mode(PRINT_UNDERLINE);
    report->TextL("Room");
    report->TextPosL(12, "Name");
    report->TextPosL(-35, "Phone");
    report->TextPosR(-9, "Check Out");
    report->TextR("Balance");
    report->Mode(0);
    report->NewLine();

    genericChar name[256];
    int training = e->training;
    report->update_flag = UPDATE_CHECKS;
    for (Check *c = FirstCheck(); c != NULL; c = c->next)
	{
		if (c->CustomerType() != CHECK_HOTEL || 
            c->IsTraining() != training ||
            c->Status() != CHECK_OPEN)
        {
			continue;
        }

		int balance = 0;
		for (SubCheck *sc = c->SubList(); sc != NULL; sc = sc->next)
			balance += sc->balance;

		TimeInfo *timevar = c->CheckOut();
		if (strlen(c->LastName()) <= 0)
		{
			if (strlen(c->FirstName()) <= 0)
				strcpy(name, "--");
			else
				strcpy(name, c->FirstName());
		}
		else
			sprintf(name, "%s, %s", c->LastName(), c->FirstName());

		name[24] = '\0';

		report->TextL(c->Table());
		report->TextPosL(12, name);
		report->TextPosL(-35, c->PhoneNumber());
		if (timevar)
			report->TextPosR(-9, term->TimeDate(*timevar, TD_SHORT_DATE | TD_SHORT_DAY | TD_NO_TIME));
		if (balance > 0)
			report->TextR(term->FormatPrice(balance));
		else
			report->TextR("PAID");
		report->NewLine();
	}
    return 0;
}


/*********************************************************************
 * Define some classes to help with expense reporting.  The idea is
 * that we can have a list organized by type of expense.  Something
 * like
 *   Expenses from Drawer 1
 *      Expense 1
 *      Expense 2
 *   Expenses from Drawer 2
 *      ...
 *   Expenses from Account 1
 *      ...
 * That way we don't try listing everything for Drawer 3 when there
 * aren't any expenses for Drawer 3.  We've accumulated ahead of time
 * everything we'll need to display.
 *
 * class Expenses is a linked list of expenses for a particular
 *   drawer or account
 * class ExpenseTypes is a linked list of all expenses organized
 *   by type and account/drawer.
 *
 *   ExpenseTypes (drawer 1)
 *    |  |
 *    |  -> Expenses ($12.55)
 *    |       |
 *    |       -> Expenses ($17.21)
 *    |            |
 *    |            -> Expenses ($10.00)
 *    |
 *    ->ExpenseTypes (drawer 2)
 *       |  |
 *       |  -> Expenses ($7.00)
 *       |
 *       -> ExpenseTypes (account 1)
 *              |
 *              -> Expenses ($350.00)
 ********************************************************************/


#define EXPENSE_TYPE_UNKNOWN 0
#define EXPENSE_TYPE_DRAWER  1
#define EXPENSE_TYPE_ACCOUNT 2

// store both sort method and sort order in the same variable
// first sort method
#define EXPENSE_SORTBY_SOURCE     0
#define EXPENSE_SORTBY_DATE       1
#define EXPENSE_SORTBY_DEST       2
#define EXPENSE_SORTBY_PAYER      3
#define EXPENSE_SORTBY_AMOUNT     4
// masq out order flags to determine method
#define EXPENSE_SORTBY_MASQ      15
// next sort order
#define EXPENSE_SORTBY_ASCEND    16  // sort in descending order


/*********************************************************************
 * Expenses class
 ********************************************************************/

class Expenses
{
public:
    Expenses *next;
    TimeInfo date;
    int payer_id;
    genericChar payer_name[STRLENGTH];
    int source_num;
    genericChar source_name[STRLENGTH];
    int tax_account_num;
    genericChar tax_account_name[STRLENGTH];
    int tax_amount;
    int dest_account_num;
    genericChar dest_account_name[STRLENGTH];
    int amount;
    genericChar document[STRLENGTH];
    genericChar explanation[STRLENGTH];

    Expenses();
    Expenses(Expense *expense, Terminal *term, Archive *archive);
    ~Expenses();
    int Copy(Expenses *exp2);
    Expenses *ImportExpenseDB(ExpenseDB *expense_db, Terminal *term,
                              Archive *archive, int sortby = 0);
    Expenses *Insert(Expense *expense, Terminal *term, int sortby, Archive *archive = NULL);
    int LessThan(Expenses *exp2, int sortby);
    int GreaterThan(Expenses *exp2, int sortby);
    void Print();
};

Expenses::Expenses()
{
    next = NULL;
    payer_id = 0;
    payer_name[0] = '\0';
    date.Set();
    amount = 0;
    source_num = 0;
    source_name[0] = '\0';
    tax_account_num = 0;
    tax_account_name[0] = '\0';
    tax_amount = 0;
    dest_account_num = 0;
    dest_account_name[0] = '\0';
    document[0] = '\0';
    explanation[0] = '\0';
}

Expenses::Expenses(Expense *expense, Terminal *term, Archive *archive)
{
    UserDB *userdb = &(term->system_data->user_db);
    Employee *employee = userdb->FindByID(expense->employee_id);
    Drawer *drawer_list;
    if (archive != NULL)
        drawer_list = archive->DrawerList();
    else
        drawer_list = term->system_data->DrawerList();
    AccountDB *acctdb = &(term->system_data->account_db);
    Account *tax_account = acctdb->FindByNumber(expense->tax_account_id);
    Account *dest_account = acctdb->FindByNumber(expense->dest_account_id);

    next = NULL;

    date = expense->exp_date;

    payer_id = expense->employee_id;
    if (employee != NULL)
        strncpy(payer_name, employee->system_name.Value(), STRLENGTH);
    else
        strncpy(payer_name, "Unknown", STRLENGTH);

    source_name[0] = '\0';
    if (expense->drawer_id > -1)
        expense->DrawerOwner(term, source_name, archive);
    else if (expense->account_id > -1)
        expense->AccountName(term, source_name);
    if (source_name[0] == '\0')
        strncpy(source_name, "Unknown", STRLENGTH);

    tax_account_num = expense->tax_account_id;
    if (tax_account != NULL)
        strncpy(tax_account_name, tax_account->name.Value(), STRLENGTH);
    else
        strncpy(tax_account_name, "Unknown", STRLENGTH);
    tax_amount = expense->tax;

    dest_account_num = expense->dest_account_id;
    if (dest_account != NULL)
        strncpy(dest_account_name, dest_account->name.Value(), STRLENGTH);
    else
        strncpy(dest_account_name, "Unknown", STRLENGTH);

    strncpy(document, expense->document.Value(), STRLENGTH);
    strncpy(explanation, expense->explanation.Value(), STRLENGTH);
    amount = expense->amount;
}

Expenses::~Expenses()
{
    if (next != NULL)
        delete next;
}

int Expenses::Copy(Expenses *exp2)
{
    FnTrace("Expenses::Copy()");
    next = NULL;
    payer_id = exp2->payer_id;
    strncpy(payer_name, exp2->payer_name, STRLENGTH);
    date.Set(exp2->date);
    amount = exp2->amount;
    source_num = exp2->amount;
    strncpy(source_name, exp2->source_name, STRLENGTH);
    tax_account_num = 0;
    strncpy(tax_account_name, exp2->tax_account_name, STRLENGTH);
    tax_amount = 0;
    dest_account_num = 0;
    strncpy(dest_account_name, exp2->dest_account_name, STRLENGTH);
    strncpy(document, exp2->document, STRLENGTH);
    strncpy(explanation, exp2->explanation, STRLENGTH);
    return 0;
}

Expenses *Expenses::ImportExpenseDB(ExpenseDB *expense_db, Terminal *term,
                                    Archive *archive, int sortby)
{
    FnTrace("Expenses::ImportExpenseDB()");
    Expenses *retNode = this;
    Expense *currExpense = expense_db->ExpenseList();

    while (currExpense != NULL)
    {
        retNode = retNode->Insert(currExpense, term, sortby, archive);
        currExpense = currExpense->next;
    }

    return retNode;
}

Expenses *Expenses::Insert(Expense *expense, Terminal *term, int sortby, Archive *archive)
{
    FnTrace("Expenses::Insert()");
    Expenses *currNode = this;
    Expenses *prevNode = NULL;
    Expenses *newNode = new Expenses(expense, term, archive);
    Expenses *retNode = this;  // we'll return this
    int comparison;
    int done = 0;

    if (currNode->amount == 0)
    {
        currNode->Copy(newNode);
        delete newNode;
        done = 1;
    }

    while (currNode != NULL && done != 1)
    {
        if (sortby & EXPENSE_SORTBY_ASCEND)
            comparison = newNode->LessThan(currNode, sortby);
        else
            comparison = newNode->GreaterThan(currNode, sortby);

        if (comparison)
        {
            if (prevNode == NULL)
            { // at the head
                retNode = newNode;
                newNode->next = currNode;
            }
            else
            {
                newNode->next = prevNode->next;
                prevNode->next = newNode;
            }
            done = 1;
        }
        else
        {
            prevNode = currNode;
            currNode = currNode->next;
        }
    }
    if (done == 0)
    {
        if (prevNode == NULL)
        { // at the head
            retNode = newNode;
            newNode->next = currNode;
        }
        else
        {
            newNode->next = prevNode->next;
            prevNode->next = newNode;
        }
    }
    return retNode;
}

/****
 *  LessThan:  For sorting, we'll return 1 for LessThan, 0 for equal or greater than.
 *    Secondary sorts:
 *    Source, Account, and Payer all use a secondary sort of date
 *    Date uses a secondary sort of Payer
 ****/
int Expenses::LessThan(Expenses *exp2, int sortby)
{
    FnTrace("Expenses::LessThan()");
    int retval = 0;
    int comparison;
    int sort_method = sortby & EXPENSE_SORTBY_MASQ;  // strip of ascend/descend bit

    switch (sort_method)
    {
    case EXPENSE_SORTBY_SOURCE:
        comparison = strcmp(source_name, exp2->source_name);
        if (comparison == 0)
            retval = (date < exp2->date);
        else
            retval = (comparison < 0);
        break;
    case EXPENSE_SORTBY_DATE:
        if (date == exp2->date)
            retval = (strcmp(payer_name, exp2->payer_name) < 0);
        else
            retval = (date < exp2->date);
        break;
    case EXPENSE_SORTBY_DEST:
        comparison = strcmp(dest_account_name, exp2->dest_account_name);
        if (comparison == 0)
            retval = (date < exp2->date);
        else
            retval = (comparison < 0);
        break;
    case EXPENSE_SORTBY_PAYER:
        comparison = strcmp(payer_name, exp2->payer_name);
        if (comparison == 0)
            retval = (date < exp2->date);
        else
            retval = (comparison < 0);
        break;
    case EXPENSE_SORTBY_AMOUNT:
        if (amount == exp2->amount)
            retval = (date < exp2->date);
        else
        {
            retval = (amount < exp2->amount);
        }
    }
    return retval;
}

int Expenses::GreaterThan(Expenses *exp2, int sortby)
{
    FnTrace("Expenses::GreaterThan()");
    int retval = 0;
    int comparison;
    int sort_method = sortby & EXPENSE_SORTBY_MASQ;  // strip of ascend/descend bit

    switch (sort_method)
    {
    case EXPENSE_SORTBY_SOURCE:
        comparison = strcmp(source_name, exp2->source_name);
        if (comparison == 0)
            retval = (date > exp2->date);
        else
            retval = (comparison > 0);
        break;
    case EXPENSE_SORTBY_DATE:
        if (date == exp2->date)
            retval = (strcmp(payer_name, exp2->payer_name) > 0);
        else
            retval = (date > exp2->date);
        break;
    case EXPENSE_SORTBY_DEST:
        comparison = strcmp(dest_account_name, exp2->dest_account_name);
        if (comparison == 0)
            retval = (date > exp2->date);
        else
            retval = (comparison > 0);
        break;
    case EXPENSE_SORTBY_PAYER:
        comparison = strcmp(payer_name, exp2->payer_name);
        if (comparison == 0)
            retval = (date > exp2->date);
        else
            retval = (comparison > 0);
        break;
    case EXPENSE_SORTBY_AMOUNT:
        if (amount == exp2->amount)
            retval = (date > exp2->date);
        else
            retval = (amount > exp2->amount);
    }
    return retval;
}

void Expenses::Print()
{
    FnTrace("Expenses::Print()");
    Expenses *currNode = this;

    while (currNode != NULL)
    {
        printf("    %d\n", currNode->amount);
        currNode = currNode->next;
    }
}


/*********************************************************************
 * Expense Report Functions
 ********************************************************************/

#define EXPENSE_REPORT_TITLE "Expense Report"
int System::ExpenseReport(Terminal *term, TimeInfo &start_time, TimeInfo &end_time,
                          Archive *archive, Report *report, ReportZone *rzone)
{
    FnTrace("System::ExpenseReport()");
    if (report == NULL)
        return 1;

    Expenses *expenselist = new Expenses;
    Expenses *currExpense;
    genericChar buffer[STRLENGTH];
    int column;
    int width;
    int color = COLOR_DEFAULT;
    int sortby;
    int incomplete = 0;

    report->SetTitle(EXPENSE_REPORT_TITLE);
    report->Mode(PRINT_BOLD | PRINT_LARGE);
    report->TextC(EXPENSE_REPORT_TITLE);
    report->NewLine();
    report->TextC(term->GetSettings()->store_name.Value());
    report->NewLine();
    report->Mode(0);
    sortby = report_sort_method;

    int total_expenses = 0;

    archive = FindByTime(start_time);
    if (archive == NULL)
    {  // didn't find any archive; process today's expenses
        ExpenseDB *my_expense_db = &(term->system_data->expense_db);
        expenselist = expenselist->ImportExpenseDB(my_expense_db, term, NULL, sortby);
        incomplete = 1;
    }
    else
    {
        Expense *expense;
        Archive *currArchive = archive;

        while ((currArchive != NULL) && (currArchive->end_time <= end_time))
        {
            if (currArchive->loaded == 0)
                currArchive->LoadPacked(term->GetSettings());
            expense = currArchive->expense_db.ExpenseList();
            while (expense != NULL)
            {
                if (expense->exp_date >= start_time && expense->exp_date < end_time)
                {
                    expenselist = expenselist->Insert(expense, term, sortby, currArchive);
                }
                expense = expense->next;
            }
            currArchive = currArchive->next;
        }
        if (currArchive == NULL)
            incomplete = 1;
    }

    // Create the headers
    snprintf(buffer, STRLENGTH, "Start:  %s", term->TimeDate(start_time, TD0));
    report->TextL(buffer, color);
    report->NewLine();
    snprintf(buffer, STRLENGTH, "End:  %s", term->TimeDate(end_time, TD0));
    if (incomplete)
        report->TextL(buffer, COLOR_DK_RED);
    else
        report->TextL(buffer, color);
    report->NewLine(2);

    if (rzone != NULL)
    {
        column_spacing = rzone->ColumnSpacing(term, 5);
        width = rzone->Width(term);
    }
    else
    {
        column_spacing = 16;
        width = 80;
    }

    // write the column headers
    report->Mode(PRINT_BOLD);
    column = 0;
    report->TextPosL(column, term->Translate("Date"), color);
    column += column_spacing;
    report->TextPosL(column, term->Translate("Owner"), color);
    column += column_spacing;
    report->TextPosL(column, term->Translate("Source"), color);
    column += column_spacing;
    report->TextPosL(column, term->Translate("Dest"), color);
    column += column_spacing;
    report->TextPosL(column, term->Translate("Amount"), color);
    report->UnderlinePosL(0, width - 1, COLOR_DK_BLUE);
    report->Mode(0);
    report->NewLine();

    // now walk through the generated lists of expenses and create the report
    currExpense = expenselist;
    while (currExpense != NULL)
    {
        column = 0;
        report->TextPosL(column, term->TimeDate(currExpense->date, TD_DATE), color);
        column += column_spacing;
        report->TextPosL(column, currExpense->payer_name, color);
        column += column_spacing;
        report->TextPosL(column, currExpense->source_name, color);
        column += column_spacing;
        report->TextPosL(column, currExpense->dest_account_name, color);
        column += column_spacing;
        report->TextPosL(column, term->FormatPrice(currExpense->amount), color);
        report->NewLine();
        column = 10;
        report->TextPosL(column, currExpense->document, color);
        column = (column_spacing * 2) + 10;
        report->TextPosL(column, currExpense->explanation, color);
        if (currExpense->next == NULL)
            color = COLOR_DK_BLUE;
        report->UnderlinePosL(0, width - 1, color);
        report->NewLine();
        total_expenses += currExpense->amount;
        currExpense = currExpense->next;
    }

    if (total_expenses > 0)
    {
        report->NewLine();
        report->Mode(PRINT_BOLD);
        report->TextPosR(-10, "Total", COLOR_DEFAULT);
        report->TextR(term->FormatPrice(total_expenses));
        report->Mode(0);
    }
    else
    {
        report->TextC("There are no expenses for this period");
    }
    report->is_complete = 1;

    delete expenselist;
    return 0;
}

class RoyaltyData {
public:
    System *system;
    Report *report;
    Terminal *term;
    Settings *settings;
    Archive *archive;
    TimeInfo start_time;
    TimeInfo end_time;
    int maxdays;
    int incomplete;
    int customers[31];
    int sales[31];
    int total_sales;
    int total_guests;
    int total_vouchers;
    int total_voucher_amt;
    int total_adjust_amt;
    int zone_width;
    int column_width;
    int dcolumns;  // number of (major) columns for the top area
    int done;
    RoyaltyData()
    {
        int idx;
        for (idx = 0; idx < 31; idx++)
        {
            customers[idx] = 0;
            sales[idx]     = 0;
        }
        system            = NULL;
        report            = NULL;
        term              = NULL;
        archive           = NULL;
        maxdays           = 0;
        incomplete        = 0;
        total_sales       = 0;
        total_guests      = 0;
        total_vouchers    = 0;
        total_voucher_amt = 0;
        total_adjust_amt  = 0;
        dcolumns          = 2;  // default, days 1-16 in col 1, 17-end in col 2
        done = 0;
    }
};

/*--------------------------------------------------------------------
 * The Vouchers class is for tracking the various vouchers that need
 *   to be calculated into the Royalty report.  The vouchers should
 *   be added to the total sales before calculating the percentage
 *   for royalty.  Then the total vouchers should be subtracted from
 *   the royalty amount to be paid.
 *   Normally there will probably be only one voucher (Head Office
 *   for Donato), but we'll be flexible and allow for any number.
 *   Normally the vouchers will only be coupons, but we'll allow for
 *   them to be any media type.
 *------------------------------------------------------------------*/
class Vouchers
{
public:
    Vouchers();
    Vouchers(int vtype, int vid);
    Vouchers *next;
    Vouchers *fore;
    int type;
    int id;
};

Vouchers::Vouchers()
{
    next = NULL;
    fore = NULL;
    type = -1;
    id   = -1;
}

Vouchers::Vouchers(int vtype, int vid)
{
    next = NULL;
    fore = NULL;
    type = vtype;
    id   = vid;
}

#define ROYALTY_REPORT_TITLE "Royalty Report"
/****
 * RoyaltyReportWorkFn:  FIX:  This report has a fairly severe problem:  it
 * doesn't track tax changes over time.  The tax values are stored in the
 * archives, but because this report crosses archival boundaries that doesn't
 * work well.  What probably is needed is another way of storing historical
 * data.  Possibly a linked list with dates, so when we run a report like
 * this, we find the tax value fitting in the date range.   Even that could be
 * problematic because how do we calculate this report if the tax rate changed
 * mid-month?
 ****/
int RoyaltyReportWorkFn(RoyaltyData *rdata)
{
    FnTrace("RoyaltyReportWorkFn()");

    Archive *archive = rdata->archive;
    int day;
    int vouchers;
    Check *currCheck = NULL;
    SubCheck *currSubcheck = NULL;
    CouponInfo *currCoupon = NULL;
    DList<Vouchers> voucher_list;
    Vouchers *currVoucher;
    int guests_counted = 0;

    while (rdata->done == 0)
    {
        if (archive)
        {
            // add this archive to the report
            if (archive->loaded == 0)
                archive->LoadPacked(rdata->settings);
            
            currCheck = archive->CheckList();
            currCoupon = archive->CouponList();
        }
        else if (SystemTime < rdata->end_time)
        {
            currCheck = rdata->system->CheckList();
            currCoupon = rdata->settings->CouponList();
        }

        while (currCoupon != NULL)
        {
            if ((currCoupon->flags & TF_ROYALTY) ||
                (strcmp(currCoupon->name.Value(), "Head Office") == 0))
             {
                Vouchers *newVoucher = new Vouchers(TENDER_COUPON, currCoupon->id);
                voucher_list.AddToTail(newVoucher);
            }
            currCoupon = currCoupon->next;
        }

        while (currCheck != NULL)
        {
            if ((currCheck->IsTraining() == 0) &&
                (currCheck->time_open >= rdata->start_time) &&
                (currCheck->time_open < rdata->end_time))
            {
                guests_counted = 0;
                day = currCheck->time_open.Day() - 1;  // day is an index, start at 0
                if (day < rdata->maxdays)
                {
                    currSubcheck = currCheck->SubList();
                    while (currSubcheck != NULL)
                    {
                        if (currSubcheck->settle_time.IsSet() &&
                            currSubcheck->settle_time > rdata->start_time &&
                            currSubcheck->settle_time < rdata->end_time &&
                            (archive == NULL ||
                             (currSubcheck->settle_time >= archive->start_time &&
                              currSubcheck->settle_time <= archive->end_time)))
                        {
                            currSubcheck->FigureTotals(rdata->settings);
                            if (guests_counted == 0)
                            {
                                if (currCheck->IsTakeOut() || currCheck->IsFastFood())
                                {
                                    rdata->customers[day] += 1;
                                    rdata->total_guests += 1;
                                }
                                else
                                {
                                    rdata->customers[day] += currCheck->Guests();
                                    rdata->total_guests += currCheck->Guests();
                                }
                                guests_counted = 1;
                            }
                            rdata->sales[day] += currSubcheck->total_sales;
                            rdata->total_sales += currSubcheck->total_sales;
                            // now check vouchers
                            currVoucher = voucher_list.Head();
                            while (currVoucher != NULL)
                            {
                                vouchers = currSubcheck->TotalPayment(currVoucher->type, currVoucher->id);
                                if (vouchers)
                                {
                                    rdata->total_vouchers += 1;
                                    rdata->total_voucher_amt += vouchers;
                                }
                                currVoucher = currVoucher->next;
                            }
                        }
                        currSubcheck = currSubcheck->next;
                    }
                }
                else if (debug_mode)
                {
                    printf("Too many days:  %d\n", day);
                }
            }
            currCheck = currCheck->next;
        }
        voucher_list.Purge();  // always clean the voucher list

        // process for next archive and check exit condition
        // exit if we've gone one loop without an archive, or if
        // the next archive is past the end time
        if (archive)
        {
            rdata->archive = archive->next;
            if (rdata->archive && rdata->archive->start_time > rdata->end_time)
                    rdata->done = 1;
        }
        else
            rdata->done = 1;
        return 0; // cycle the work fn again until done
    }

    // we now have all the data we need to generate the report, so let's get it done
    Report *report     = rdata->report;
    Settings *settings = rdata->settings;
    Terminal *term     = rdata->term;
    int maxdays        = rdata->maxdays;
    int column_width   = rdata->column_width;
    int zone_width     = rdata->zone_width;
    int *customers     = rdata->customers;
    int *sales         = rdata->sales;
    int dcolumns       = rdata->dcolumns;

    int royalty_due   = 0;  // total sales * royalty rate
    int advertise_due = 0;
    int gst_due       = 0;  // royalty rate * gst
    int qst_due       = 0;  // royalty rate + gst * qst
    int total_due     = 0;  // total royalty & taxes
    int check_avg;
    int idx           = 0;
    int x             = 0;  // temporary index variable
    int color         = COLOR_DEFAULT;
    int scwidth       = (column_width - 2) / 4;  // 4 small columns
    int column        = 0;  // current column
    int column1       = 0;                  // left justified
    int column2       = scwidth;            // left justified
    int column3       = (scwidth * 3) - 1;  // right justified
    int column4       = (scwidth * 4) - 1;  // right justified

    int wday;
    int month = rdata->start_time.Month();
    int year  = rdata->start_time.Year();

    report->SetTitle(ROYALTY_REPORT_TITLE);
    report->Mode(PRINT_BOLD | PRINT_LARGE);
    report->TextC(ROYALTY_REPORT_TITLE);
    report->NewLine();
    report->Mode(0);

    report->TextL(settings->store_name.Value(), COLOR_DEFAULT);
    if (rdata->incomplete)
        report->TextR(term->TimeDate(rdata->start_time, TD_MONTH), COLOR_DK_RED);
    else
        report->TextR(term->TimeDate(rdata->start_time, TD_MONTH), color);
    report->NewLine(2);

    // print the daily totals
    int y;
    column = 0;
    int dayspercol = 16;
    if (dcolumns == 1)
        dayspercol = 31;
    report->UnderlinePosL(0, zone_width - 1, COLOR_BLUE);
    for (y = 0; y < dcolumns; y++)
    {
        report->Mode(PRINT_BOLD);
        report->TextPosL(column + column1, term->Translate("Day"), color);
        report->TextPosL(column + column2, term->Translate("Guests"), color);
        report->TextPosR(column + column3, term->Translate("Sales"), color);
        report->TextPosR(column + column4, term->Translate("Average"), color);
        if (dcolumns > 1)
            column += column_width;
    }
    report->NewLine();
    report->Mode(0);
    for (x = 0; x < dayspercol; x++)
    {
        // print left columns
        idx = x;
        column = 0;
        for (y = 0; y < dcolumns; y++)
        {
            if (idx < maxdays)
            {
                wday = DayOfTheWeek(idx + 1, month, year);
                if (wday == 0 || wday == 6)
                    color = COLOR_DK_BLUE;
                else
                    color = COLOR_DEFAULT;
                report->NumberPosL(column + column1, idx + 1, color);
                report->NumberPosL(column + column2, customers[idx], color);
                report->TextPosR(column + column3, term->FormatPrice(sales[idx]), color);
                check_avg = 0;
                if (customers[idx] > 0)
                    check_avg = sales[idx] / customers[idx];
                report->TextPosR(column + column4, term->FormatPrice(check_avg), color);
                if (dcolumns > 1)
                {
                    idx += 16;
                    column += column_width;
                }
                color = COLOR_DEFAULT;
            }
        }
        if (x < dayspercol) // no underline on the last line
        {
            report->UnderlinePosL(0, zone_width - 1, color);
            report->NewLine();
        }
    }
    // totals on last line
    report->Mode(PRINT_BOLD);
    report->TextPosL(column + column1, term->Translate("Total"), color);
    report->NumberPosL(column + column2, rdata->total_guests, color);
    report->TextPosR(column + column3, term->FormatPrice(rdata->total_sales), color);
    check_avg = 0;
    if (rdata->total_guests > 0)
        check_avg = rdata->total_sales / rdata->total_guests;
    report->TextPosR(column + column4, term->FormatPrice(check_avg), color);
    report->Mode(0);
                
    report->UnderlinePosL(0, zone_width - 1, COLOR_BLUE);
    report->NewLine(2);

    int far_column = column_width + column2;
    if (far_column > zone_width)
        far_column = column_width - 2;
    // total sales
    report->Mode(0);
    report->TextL(term->Translate("Total Sales"), color);
    report->TextPosR(far_column, term->FormatPrice(rdata->total_sales), color);
    report->NewLine();

    // vouchers
    report->TextL(term->Translate("Vouchers"), color);
    report->TextPosR(far_column, term->FormatPrice(rdata->total_voucher_amt), color);
    report->NewLine();

    // total sales for calculating royalty (total sales + vouchers)
    int total_for_royalty = rdata->total_sales + rdata->total_voucher_amt;
    report->Mode(PRINT_BOLD);
    report->TextL(term->Translate("Total Sales for Royalty Calc."), color);
    report->TextPosR(far_column, term->FormatPrice(total_for_royalty), color);
    report->UnderlinePosR(far_column, 10, color);
    report->NewLine();
    report->Mode(0);

    // royalty due
    royalty_due = (int) (total_for_royalty * settings->royalty_rate);
    report->TextPosL(column1, term->Translate("Royalty Due"), color);
    report->TextPosR(far_column, term->FormatPrice(royalty_due), color);
    report->NewLine();

    // GST
    if (settings->tax_GST > 0)
    {
        report->NewLine();
        gst_due = settings->FigureGST(royalty_due, SystemTime);
        report->TextPosL(column1, term->Translate("GST Due"), color);
        report->TextPosR(far_column, term->FormatPrice(gst_due), color);
    }

    // QST
    if (settings->tax_QST > 0)
    {
        report->NewLine();
        qst_due = settings->FigureQST(royalty_due, gst_due, SystemTime, 0);
        report->TextPosL(column1, "QST Due:", color);
        report->TextPosR(far_column, term->FormatPrice(qst_due), color);
    }

    if (settings->tax_GST <= 0 && settings->tax_QST <= 0)
    {
        report->NewLine();
        report->TextPosL(column1, "Taxes Due", color);
        report->TextPosR(far_column, term->FormatPrice(0), color);
    }

    report->UnderlinePosR(far_column, 10, color);
    report->NewLine();

    // royalty + taxes
    int total_royalty = royalty_due + gst_due + qst_due;
    report->Mode(PRINT_BOLD);
    report->TextPosL(column1, term->Translate("Total Royalty and Taxes"), color);
    report->TextPosR(far_column, term->FormatPrice(total_royalty), color);
    report->NewLine(2);
    report->Mode(0);

    // subtract the vouchers (list them again)
    report->TextPosL(column1, term->Translate("Minus Home Office Vouchers"), color);
    report->TextPosR(far_column, term->FormatPrice(rdata->total_voucher_amt), color);
    report->NewLine();

    // handle adjustments
    report->TextPosL(column1, term->Translate("+/- Adjustments"), color);
    report->TextPosR(far_column, term->FormatPrice(rdata->total_adjust_amt), color);
    report->UnderlinePosR(far_column, 10, color);
    report->NewLine();

    // Total Due
    total_due = total_royalty - rdata->total_voucher_amt - rdata->total_adjust_amt;
    report->Mode(PRINT_BOLD);
    report->TextPosL(column1, term->Translate("Royalty Check Total"), color);
    report->TextPosR(far_column, term->FormatPrice(total_due), color);
    report->NewLine(2);
    report->Mode(0);

    // Advertising Fund Due
    advertise_due = (int) (total_for_royalty * settings->advertise_fund);
    advertise_due = advertise_due + gst_due + qst_due;
    report->Mode(PRINT_BOLD);
    report->TextPosL(column1, term->Translate("Ad Check Total"), color);
    report->TextPosR(far_column, term->FormatPrice(advertise_due), color);
    report->NewLine();
    report->Mode(0);
    
    report->is_complete = 1;
    term->Update(UPDATE_REPORT, NULL);
    delete rdata;

    return 1;  // end of work fn
}

int System::RoyaltyReport(Terminal *term, TimeInfo &start_time, TimeInfo &end_time,
                          Archive *archive, Report *report, ReportZone *rzone)
{
    FnTrace("System::RoyaltyReport()");
    if (report == NULL)
        return 1;

    RoyaltyData *rdata = new RoyaltyData();
    rdata->maxdays = DaysInMonth(start_time.Month(), start_time.Year());
    rdata->settings = &settings;
    rdata->system = this;
    rdata->report = report;
    rdata->term = term;
    rdata->start_time.Set(start_time);
    rdata->end_time.Set(end_time);
    rdata->archive = FindByTime(start_time);
    if (report->destination == RP_DEST_PRINTER)
        rdata->zone_width = report->max_width;
    else if (rzone != NULL)
        rdata->zone_width   = rzone->Width(term);
    else
        rdata->zone_width = 80;
    if (rdata->zone_width < 60)
    {  // single column
        rdata->column_width = rdata->zone_width;
        rdata->dcolumns = 1;
    }
    else
    {  // two columns
        if (rzone != NULL && report->destination != RP_DEST_PRINTER)
            rdata->column_width = rzone->ColumnSpacing(term, 2);
        else
            rdata->column_width = rdata->zone_width / 2;
        rdata->dcolumns = 2;
    }

    report->is_complete = 0;
    AddWorkFn((WorkFn) RoyaltyReportWorkFn, rdata);
    return 0;
}


/*********************************************************************
 * AuditingData Class
 ********************************************************************/

class AuditingData
{
public:
    Terminal *term;
    System   *system;
    Settings *settings;
    Report   *report;
    TimeInfo  start_time;
    TimeInfo  end_time;
    Archive  *archive;
    int       done;

    MediaList coupons;
    MediaList discounts;
    MediaList comps;
    MediaList meals;
    MediaList creditcards;

    int total_payments;
    int total_cash;
    int total_checks;
    int total_gift_certificates;
    int total_tips;
    int total_change;
    int total_voids;

    // guest counts
    int total_dinein;
    int total_dinein_sales;
    int total_togo;
    int total_togo_sales;

    // total of each family
    int by_family[MAX_FAMILIES];

    int total_sales;
    int total_item_sales;
    int total_taxes;
    int total_adjusts;

    int incomplete;

    AuditingData();
};

AuditingData::AuditingData()
{
    FnTrace("AuditingData::AuditingData()");
    int idx = 0;

    term                    = NULL;
    system                  = NULL;
    settings                = NULL;
    report                  = NULL;
    start_time.Clear();
    end_time.Clear();
    archive                 = NULL;
    done                    = 0;

    total_payments          = 0;
    total_cash              = 0;
    total_checks            = 0;
    total_gift_certificates = 0;
    total_tips              = 0;
    total_change            = 0;
    
    total_dinein            = 0;
    total_dinein_sales      = 0;
    total_togo              = 0;
    total_togo_sales        = 0;

    total_sales             = 0;
    total_item_sales        = 0;
    total_taxes             = 0;
    total_adjusts           = 0;
    
    incomplete              = 0;

    for (idx = 0; idx < MAX_FAMILIES; idx += 1)
        by_family[idx] = 0;
}


/*********************************************************************
 * Auditing Report
 ********************************************************************/

int GatherAuditChecks(AuditingData *adata)
{
    FnTrace("GatherAuditChecks()");
    Archive *archive            = adata->archive;
    Settings *settings          = adata->settings;
    Check *check                = NULL;
    SubCheck *subcheck          = NULL;
    Order *order                = NULL;
    Order *modifier             = NULL;
    Payment *payment            = NULL;
    DiscountInfo *discount      = NULL;
    CouponInfo *coupon          = NULL;
    CompInfo *comp              = NULL;
    MealInfo *meal              = NULL;
    CreditCardInfo *creditcard  = NULL;
    const char* temp                  = NULL;
    int guests_counted          = 0;
    int is_dinein               = 0;
    int sales                   = 0;  // payment - taxes - tips - adjustments

    if (archive)
    {
        // add this archive to the report
        if (archive->loaded == 0)
            archive->LoadPacked(settings);
        check     = archive->CheckList();
    }
    else
    {
        check     = adata->system->CheckList();
    }

    while (check != NULL)
    {
        if (check->IsTraining() == 0)
        {
            guests_counted = 0;
            is_dinein = 0;
            if (check->Status() == CHECK_VOIDED)
            {
                // haven't found any voided checks yet, so I'm not
                // exactly sure what to do with them.
                if (debug_mode)
                    printf("Check Voided:  %s\n", check->time_open.DebugPrint());
            }

            subcheck = check->SubList();
            while (subcheck != NULL)
            {
                if (subcheck->settle_time.IsSet() &&
                    subcheck->settle_time > adata->start_time &&
                    subcheck->settle_time < adata->end_time)
                {
                    // Count the guests.  They're stored in check, but we only want
                    // to process them if the check was settled, which is stored in
                    // subcheck.  So we use a guests_counted flag to make sure we
                    // only count once for all guests.
                    if (guests_counted == 0)
                    {
                        switch(check->type)
                        {
                        case CHECK_RESTAURANT:
                            adata->total_dinein += check->Guests();
                            is_dinein = 1;
                            break;
                        case CHECK_FASTFOOD:
                        case CHECK_TAKEOUT:
                        case CHECK_DELIVERY:
                        case CHECK_CATERING:
                            if (check->Guests())
                                adata->total_togo += check->Guests();
                            else
                                adata->total_togo += 1;
                            break;
                        default:
                            if (debug_mode)
                                printf("Unknown Check Type:  %d\n", check->type);
                            break;
                        }
                        guests_counted = 1;
                    }

                    sales = 0;
                    subcheck->FigureTotals(settings);
                    if (subcheck->status == CHECK_VOIDED)
                    {
                        // haven't found any voided checks yet, so I'm
                        // not exactly sure what to do with them.
                    }
                    
                    adata->total_sales += subcheck->raw_sales;
                    if (subcheck->IsTaxExempt() == 0)
                        adata->total_taxes += subcheck->TotalTax();
                    order = subcheck->OrderList();
                    while (order != NULL)
                    {
                        order->FigureCost();
                        adata->by_family[order->item_family] += order->cost;
                        adata->total_item_sales += order->cost;
                        modifier = order->modifier_list;
                        while (modifier != NULL)
                        {
                            adata->by_family[modifier->item_family] += modifier->cost;
                            adata->total_item_sales += modifier->cost;
                            modifier = modifier->next;
                        }
                        order = order->next;
                    }

                    payment = subcheck->PaymentList();
                    while (payment != NULL)
                    {
                        switch (payment->tender_type)
                        {
                        case TENDER_CASH:
                            adata->total_cash += payment->amount;
                            adata->total_payments += payment->amount;
                            sales += payment->value;
                            break;
                        case TENDER_CHECK:
                            adata->total_checks += payment->value;
                            adata->total_payments += payment->amount;
                            sales += payment->value;
                            break;
                        case TENDER_CAPTURED_TIP:
                        case TENDER_CHARGED_TIP:
                            adata->total_tips += payment->value;
                            sales -= payment->value;
                            break;
                        case TENDER_CHANGE:
                            adata->total_change += payment->value;
                            adata->total_payments -= payment->amount;
                            sales -= payment->value;
                            break;
                        case TENDER_GIFT:
                            adata->total_gift_certificates += payment->value;
                            break;
                        case TENDER_CHARGE_CARD:
                            if (archive)
                                creditcard = archive->FindCreditCardByID(payment->tender_id);
                            else
                                creditcard = settings->FindCreditCardByID(payment->tender_id);
                            if (creditcard != NULL)
                                adata->creditcards.Add(creditcard->name.Value(), payment->value);
                            adata->total_payments += payment->amount;
                            sales += payment->value;
                            break;
                        case TENDER_CREDIT_CARD:
                            temp = FindStringByValue(payment->tender_id, CreditCardValue, CreditCardName);
                            if (temp != NULL)
                            {
                                adata->creditcards.Add(temp, payment->value);
                                adata->total_payments += payment->amount;
                                sales += payment->value;
                            }
                            break;
                        case TENDER_DEBIT_CARD:
                            temp = FindStringByValue(CARD_TYPE_DEBIT, CardTypeValue, CardTypeName);
                            if (temp != NULL)
                            {
                                adata->creditcards.Add(temp, payment->value);
                                adata->total_payments += payment->amount;
                                sales += payment->value;
                            }
                            break;
                        case TENDER_COUPON:
                            if (archive)
                                coupon = archive->FindCouponByID(payment->tender_id);
                            else
                                coupon = settings->FindCouponByID(payment->tender_id);
                            if (coupon != NULL)
                                adata->coupons.Add(coupon->name.Value(), payment->value);
                            adata->total_adjusts += payment->value;
                            break;
                        case TENDER_DISCOUNT:
                            if (archive)
                                discount = archive->FindDiscountByID(payment->tender_id);
                            else
                                discount = settings->FindDiscountByID(payment->tender_id);
                            if (discount != NULL)
                                adata->discounts.Add(discount->name.Value(), payment->value);
                            adata->total_adjusts += payment->value;
                            break;
                        case TENDER_COMP:
                            if (archive)
                                comp = archive->FindCompByID(payment->tender_id);
                            else
                                comp = settings->FindCompByID(payment->tender_id);
                            if (comp != NULL)
                                adata->comps.Add(comp->name.Value(), payment->value);
                            adata->total_adjusts += payment->value;
                            break;
                        case TENDER_EMPLOYEE_MEAL:
                            if (archive)
                                meal = archive->FindMealByID(payment->tender_id);
                            else
                                meal = settings->FindMealByID(payment->tender_id);
                            if (meal != NULL)
                                adata->meals.Add(meal->name.Value(), payment->value);
                            adata->total_adjusts += payment->value;
                            break;
                        default:
                            if (debug_mode)
                            {
                                printf("Unknown Tender Type:  %d, %d\n", payment->tender_type,
                                       payment->amount);
                            }
                            break;
                        }
                        payment = payment->next;
                    }

                    if (subcheck->IsTaxExempt() == 0)
                        sales -= subcheck->TotalTax();
                    if (is_dinein)
                        adata->total_dinein_sales += sales;
                    else
                        adata->total_togo_sales += sales;
                }
                subcheck = subcheck->next;
            }
        }
        check = check->next;
    }

    return 0;
}

#define AUDITING_REPORT_TITLE "Auditing Report"
int AuditingReportWorkFn(AuditingData *adata)
{
    FnTrace("AuditingReportWorkFn()");

    Terminal *term      = adata->term;
    Archive  *archive   = adata->archive;

    // gather the information for the report
    while (adata->done == 0)
    {
        GatherAuditChecks(adata);

        // process for next archive and check exit condition
        // exit if we've gone one loop without an archive, or if
        // the next archive is past the end time
        if (archive)
        {
            adata->archive = archive->next;
            if (adata->archive && adata->archive->start_time > adata->end_time)
                    adata->done = 1;
        }
        else
            adata->done = 1;

        return 0;
    }

    // now generate the report
    Report   *report    = adata->report;
    Settings *settings  = adata->settings;
    char      str[STRLONG];
    int       family_idx;

    int color       = COLOR_DEFAULT;
    int date_format = TD_NO_TIME | TD_NO_DAY;
    int indent      = 3;
    int idx         = 0;

    MediaList *coupon      = &adata->coupons;
    MediaList *discount    = &adata->discounts;
    MediaList *comp        = &adata->comps;
    MediaList *meal        = &adata->meals;
    MediaList *creditcard  = &adata->creditcards;
    int total_coupons      = adata->coupons.Total();
    int total_discounts    = adata->discounts.Total();
    int total_comps        = adata->comps.Total();
    int total_meals        = adata->meals.Total();
    int total_creditcards  = adata->creditcards.Total();
    int total_cash         = adata->total_cash - adata->total_change;
    int total_payments     = 0;

    int total_guests = adata->total_dinein + adata->total_togo;

    int gross_sales = adata->total_payments - adata->total_tips;
    int net_sales = gross_sales - adata->total_taxes;

    report->SetTitle(AUDITING_REPORT_TITLE);
    report->Mode(PRINT_BOLD | PRINT_LARGE);
    report->TextC(AUDITING_REPORT_TITLE);
    report->NewLine();
    report->Mode(0);

    report->Mode(PRINT_BOLD | PRINT_LARGE);
    report->TextL(settings->store_name.Value(), COLOR_DEFAULT);
    if (adata->start_time.Year() == adata->end_time.Year() &&
        adata->start_time.Month() == adata->end_time.Month() &&
        adata->start_time.Day() == (adata->end_time.Day() - 1))
    {
        snprintf(str, STRLONG, "%s", term->TimeDate(adata->start_time, date_format));
    }
    else
    {
        char tstart[STRLENGTH];
        char tend[STRLENGTH];
        term->TimeDate(tstart, adata->start_time, date_format);
        term->TimeDate(tend, adata->end_time, date_format);
        snprintf(str, STRLONG, "%s - %s", tstart, tend);
    }
    report->TextR(str, color);
    report->Mode(0);
    report->NewLine(2);

    if ((term->hide_zeros == 0) || (adata->total_item_sales))
    {
        report->TextL(term->Translate("Total Adjustments"), color);
        report->TextR(term->FormatPrice(adata->total_adjusts), color);
        report->NewLine();
    }
    if ((term->hide_zeros == 0) || (adata->total_tips > 0))
    {
        report->TextL(term->Translate("Total Tips"), color);
        report->TextR(term->FormatPrice(adata->total_tips), color);
        report->NewLine();
    }
    if ((term->hide_zeros == 0) || (gross_sales > 0))
    {
        report->TextL(term->Translate("Gross Sales"), color);
        report->TextR(term->FormatPrice(gross_sales), color);
        report->NewLine();
    }
    if ((term->hide_zeros == 0) || (adata->total_taxes > 0))
    {
        report->TextL(term->Translate("Sales Tax"), color);
        report->TextR(term->FormatPrice(adata->total_taxes), color);
        report->NewLine();
    }
    if ((term->hide_zeros == 0) || (net_sales > 0))
    {
        report->TextL(term->Translate("Net Sales"), color);
        report->TextR(term->FormatPrice(net_sales), color);
        report->NewLine();
    }

    if ((term->hide_zeros == 0) || (total_guests > 0))
    {
        report->NewLine();
        report->TextL(term->Translate("Guest Count"), color);
        report->NumberR(total_guests, color);
        report->NewLine();

        if ((term->hide_zeros == 0) || (adata->total_dinein > 0))
        {
            report->TextPosL(indent, term->Translate("Total Dine In Guests"), color);
            report->NumberR(adata->total_dinein, color);
            report->NewLine();
        }
        if ((term->hide_zeros == 0) || (adata->total_dinein_sales > 0))
        {
            report->TextPosL(indent, term->Translate("Total Dine In Sales"), color);
            report->TextR(term->FormatPrice(adata->total_dinein_sales), color);
            report->NewLine();
        }
        if ((term->hide_zeros == 0) || (adata->total_togo > 0))
        {
            report->TextPosL(indent, term->Translate("Total To Go/Carry Out Count"), color);
            report->NumberR(adata->total_togo, color);
            report->NewLine();
        }
        if ((term->hide_zeros == 0) || (adata->total_togo_sales > 0))
        {
            report->TextPosL(indent, term->Translate("Total To Go/Carry Out Sales"), color);
            report->TextR(term->FormatPrice(adata->total_togo_sales), color);
            report->NewLine();
        }
    }

    // print the breakdowns of the families
    if ((term->hide_zeros == 0) || (adata->total_item_sales > 0))
    {
        report->NewLine();
        report->TextL(term->Translate("Total Item Sales"), color);
        report->TextR(term->FormatPrice(adata->total_item_sales), color);
        report->NewLine();
        idx = 0;
        while (FamilyName[idx] != NULL)
        {
            family_idx = FamilyValue[idx];
            if ((term->hide_zeros == 0) || (adata->by_family[family_idx] > 0))
            {
                report->TextPosL(indent, term->Translate(FamilyName[idx]), color);
                report->TextR(term->FormatPrice(adata->by_family[family_idx]), color);
                report->NewLine();
            }
            idx += 1;
        }
    }

    // print the breakdown of payments
    report->NewLine();
    if ((term->hide_zeros == 0) || (adata->total_cash > 0))
    {
        report->TextL(term->Translate("Total Cash Payments"), color);
        report->TextR(term->FormatPrice(total_cash), color);
        report->NewLine();
        total_payments += total_cash;
    }
    if ((term->hide_zeros == 0) || (adata->total_checks > 0))
    {
        report->TextL(term->Translate("Total Check Payments"), color);
        report->TextR(term->FormatPrice(adata->total_checks), color);
        report->NewLine();
        total_payments += adata->total_checks;
    }
    if ((term->hide_zeros == 0) || (adata->total_gift_certificates > 0))
    {
        report->TextL(term->Translate("Total Gift Certificates"), color);
        report->TextR(term->FormatPrice(adata->total_gift_certificates), color);
        report->NewLine();
        total_payments += adata->total_gift_certificates;
    }
    if ((term->hide_zeros == 0) || (total_creditcards > 0))
    {
        while (creditcard != NULL)
        {
            if ((term->hide_zeros == 0) || (creditcard->total > 0))
            {
                report->TextL(term->Translate(creditcard->name), color);
                report->TextR(term->FormatPrice(creditcard->total), color);
                report->NewLine();
                total_payments += creditcard->total;
            }
            creditcard = creditcard->next;
        }
    }

    if ((term->hide_zeros == 0) || (adata->total_adjusts > 0))
    {
        report->NewLine();
        report->TextL("Breakdown of Adjustments", color);
        report->NewLine();
        if ((term->hide_zeros == 0) || (total_coupons > 0))
        {
            report->TextPosL(indent, term->Translate("Total Coupon"), color);
            report->TextR(term->FormatPrice(total_coupons));
            report->NewLine();
        }
        if ((term->hide_zeros == 0) || (total_discounts > 0))
        {
            report->TextPosL(indent, term->Translate("Total Discount"), color);
            report->TextR(term->FormatPrice(total_discounts));
            report->NewLine();
        }
        if ((term->hide_zeros == 0) || (total_comps > 0))
        {
            report->TextPosL(indent, term->Translate("Total Comp"), color);
            report->TextR(term->FormatPrice(total_comps));
            report->NewLine();
        }
        if ((term->hide_zeros == 0) || (total_meals > 0))
        {
            report->TextPosL(indent, term->Translate("Total Employee Meal"), color);
            report->TextR(term->FormatPrice(total_meals));
            report->NewLine();
        }
    }

    // print breakdowns of the coupons, discounts, comps, and meals
    if (total_coupons > 0)
    {
        report->NewLine();
        report->TextL(term->Translate("Breakdown of Coupons"), color);
        report->NewLine();
        while (coupon != NULL)
        {
            report->TextPosL(indent, term->Translate(coupon->name), color);
            report->TextR(term->FormatPrice(coupon->total), color);
            report->NewLine();
            coupon = coupon->next;
        }
    }

    if (total_discounts > 0)
    {
        report->NewLine();
        report->TextL(term->Translate("Breakdown of Discounts"), color);
        report->NewLine();
        while (discount != NULL)
        {
            report->TextPosL(indent, term->Translate(discount->name), color);
            report->TextR(term->FormatPrice(discount->total), color);
            report->NewLine();
            discount = discount->next;
        }
    }

    if (total_comps > 0)
    {
        report->NewLine();
        report->TextL(term->Translate("Breakdown of Comps"), color);
        report->NewLine();
        while (comp != NULL)
        {
            report->TextPosL(indent, term->Translate(comp->name), color);
            report->TextR(term->FormatPrice(comp->total), color);
            report->NewLine();
            comp = comp->next;
        }
    }

    if (total_meals > 0)
    {
        report->NewLine();
        report->TextL(term->Translate("Breakdown of Employee Meals"), color);
        report->NewLine();
        while (meal != NULL)
        {
            report->TextPosL(indent, term->Translate(meal->name), color);
            report->TextR(term->FormatPrice(meal->total), color);
            report->NewLine();
            meal = meal->next;
        }
    }

    report->is_complete = 1;
    term->Update(UPDATE_REPORT, NULL);
    delete adata;

    return 1;  // end of work fn
}

int System::AuditingReport(Terminal *term, TimeInfo &start_time, TimeInfo &end_time,
                           Archive *archive, Report *report, ReportZone *rzone)
{
    FnTrace("System::AuditingReport()");
    if (report == NULL)
        return 1;

    AuditingData *adata = new AuditingData;

    adata->term = term;
    adata->system = this;
    adata->settings = &settings;
    adata->report = report;
    adata->start_time.Set(start_time);
    adata->end_time.Set(end_time);
    adata->archive = FindByTime(start_time);

    report->is_complete = 0;
    AddWorkFn((WorkFn) AuditingReportWorkFn, adata);

    return 0;
}


/*********************************************************************
 * CreditCardData Class
 ********************************************************************/
class CCData
{
public:
    CCData();

    Terminal   *term;
    System     *system;
    Settings   *settings;
    Report     *report;
    TimeInfo    start_time;
    TimeInfo    end_time;
    Archive    *archive;
    ReportZone *report_zone;
    int         done;
};

CCData::CCData()
{
    term = NULL;
    system = NULL;
    settings = NULL;
    report = NULL;
    start_time.Clear();
    end_time.Clear();
    archive = NULL;
    report_zone = NULL;
    done = 0;
}

int GetCreditCardPayments(CCData *ccdata, Payment *payment)
{
    FnTrace("GetCreditCardPayments()");
    int retval = 0;

    while (payment != NULL)
    {
        switch (payment->tender_type)
        {
        case TENDER_CREDIT_CARD:
            break;
        case TENDER_DEBIT_CARD:
            break;
        case TENDER_CHARGED_TIP:
            break;
        }
        payment = payment->next;
    }

    return retval;
}

#define CREDITCARD_REPORT_TITLE "Credit Card Report"
int CreditCardReportWorkFn(CCData *ccdata)
{
    FnTrace("CreditCardReportWorkFn()");
    Report   *report = ccdata->report;
    Settings *settings = ccdata->settings;
    Terminal *term = ccdata->term;
    Archive  *archive = ccdata->archive;
    Check    *check = NULL;
    SubCheck *subcheck = NULL;

    //////
    // Collect the data
    //////
    if (ccdata->done == 0)
    {
        if (archive)
        {
            // add this archive to the report
            if (archive->loaded == 0)
                archive->LoadPacked(settings);
            check     = archive->CheckList();
        }
        else
        {
            check     = ccdata->system->CheckList();
        }
        
        while (check != NULL)
        {
            if (check->IsTraining() == 0)
            {
                subcheck = check->SubList();
                while (subcheck != NULL)
                {
                    if (subcheck->settle_time.IsSet() &&
                        subcheck->settle_time > ccdata->start_time &&
                        subcheck->settle_time < ccdata->end_time)
                    {
                        GetCreditCardPayments(ccdata, subcheck->PaymentList());
                    }
                    subcheck = subcheck->next;
                }
            }
            check = check->next;
        }
        
        // process for next archive and check exit condition
        // exit if we've gone one loop without an archive, or if
        // the next archive is past the end time
        if (archive)
        {
            ccdata->archive = archive->next;
            if (ccdata->archive && ccdata->archive->start_time > ccdata->end_time)
                ccdata->done = 1;
        }
        else
            ccdata->done = 1;

        return 0;
    }

    //////
    // Generate the report
    //////

    report->is_complete = 1;
    term->Update(UPDATE_REPORT, NULL);
    delete ccdata;

    return 1;
}

int System::CreditCardReport(Terminal *term, TimeInfo &start_time, TimeInfo &end_time,
                             Archive *archive, Report *report, ReportZone *rzone)
{
    FnTrace("System::CreditCardReport()");
    int     retval = 1;
    CCData *ccdata = NULL;
    int     color       = COLOR_DEFAULT;
    int     date_format = TD_SHORT_MONTH | TD_NO_DAY;
    char    str[STRLONG];

    if (report == NULL)
        return retval;

    //////
    // Generate the report header
    //////
    report->Mode(PRINT_BOLD | PRINT_LARGE);
    if (cc_report_type == CC_REPORT_BATCH)
        strcpy(str, term->Translate("Batch Close Report"));
    else if (cc_report_type == CC_REPORT_INIT)
        strcpy(str, term->Translate("Initialization Results"));
    else if (cc_report_type == CC_REPORT_TOTALS)
        strcpy(str, term->Translate("Credit Card Totals"));
    else if (cc_report_type == CC_REPORT_DETAILS)
        strcpy(str, term->Translate("Credit Card Details"));
    else if (cc_report_type == CC_REPORT_SAF)
        strcpy(str, term->Translate("Store and Forward Details"));
    else if (cc_report_type == CC_REPORT_VOIDS)
        strcpy(str, term->Translate("Credit Card Voids"));
    else if (cc_report_type == CC_REPORT_REFUNDS)
        strcpy(str, term->Translate("Credit Card Refunds"));
    else if (cc_report_type == CC_REPORT_EXCEPTS)
        strcpy(str, term->Translate("Credit Card Voids"));
    else if (cc_report_type == CC_REPORT_FINISH)
        strcpy(str, term->Translate("Results of PreAuth Finish"));
    else
        strcpy(str, CREDITCARD_REPORT_TITLE);
    report->SetTitle(str);
    report->TextC(str);
    report->NewLine(2);
    report->Mode(0);

    report->Mode(PRINT_BOLD | PRINT_LARGE);
    report->TextL(settings.store_name.Value(), COLOR_DEFAULT);
    if (start_time.Year() == end_time.Year() &&
        start_time.Month() == end_time.Month() &&
        start_time.Day() == (end_time.Day() - 1))
    {
        strcpy(str, term->TimeDate(start_time, date_format));
    }
    else
    {
        char tstart[STRLENGTH];
        char tend[STRLENGTH];
        term->TimeDate(tstart, start_time, date_format);
        term->TimeDate(tend, end_time, date_format);
        snprintf(str, STRLONG, "%s - %s", tstart, tend);
    }
    report->TextR(str, color);
    report->NewLine();
    report->Mode(0);

    //////
    // Generate or begin the report.  If we're doing a normal report, we'll
    // set things up and send it off to a WorkFn.  Otherwise, we'll just do
    // it here.
    //////
    if (cc_report_type == CC_REPORT_NORMAL)
    {
        ccdata = new CCData();
        
        ccdata->term = term;
        ccdata->system = this;
        ccdata->settings = &settings;
        ccdata->report = report;
        ccdata->start_time.Set(start_time);
        ccdata->end_time.Set(end_time);
        ccdata->archive = FindByTime(start_time);
        ccdata->report_zone = rzone;
        
        report->is_complete = 0;
        AddWorkFn((WorkFn) CreditCardReportWorkFn, ccdata);
        retval = 0;
    }
    else if (cc_report_type == CC_REPORT_BATCH)
    {
        cc_settle_results->MakeReport(term, report, rzone);
    }
    else if (cc_report_type == CC_REPORT_INIT)
    {
        cc_init_results->MakeReport(term, report, rzone);
    }
    else if (cc_report_type == CC_REPORT_TOTALS)
    {
        if (term->GetSettings()->authorize_method == CCAUTH_MAINSTREET)
            cc_totals_results->MakeReport(term, report, rzone);
        else
            term->cc_totals.MakeReport(term, report, rzone);
    }
    else if (cc_report_type == CC_REPORT_DETAILS)
    {
        cc_details_results->MakeReport(term, report, rzone);
    }
    else if (cc_report_type == CC_REPORT_SAF)
    {
        if (term->cc_saf_details.IsEmpty())
            cc_saf_details_results->MakeReport(term, report, rzone);
        else
            term->cc_saf_details.MakeReport(term, report, rzone);
    }
    else if (cc_report_type == CC_REPORT_VOIDS)
    {
        if (term->archive)
            term->archive->cc_void_db->MakeReport(term, report, rzone);
        else
            cc_void_db->MakeReport(term, report, rzone);
    }
    else if (cc_report_type == CC_REPORT_REFUNDS)
    {
        if (term->archive)
            term->archive->cc_refund_db->MakeReport(term, report, rzone);
        else
            cc_refund_db->MakeReport(term, report, rzone);
    }
    else if (cc_report_type == CC_REPORT_EXCEPTS)
    {
        if (term->archive)
            term->archive->cc_exception_db->MakeReport(term, report, rzone);
        else
            cc_exception_db->MakeReport(term, report, rzone);
    }
    else if (cc_report_type == CC_REPORT_FINISH)
    {
        if (cc_finish != NULL)
        {
            if (rzone != NULL)
                rzone->Page(0);
            report->NewLine();
            report->TextL(cc_finish->Code(), COLOR_DEFAULT);
            report->NewLine();
            report->TextL(cc_finish->Verb(), COLOR_DEFAULT);
            report->NewLine();
            report->is_complete = 1;
        }
    }

    return retval;
}
