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
 * login_zone.cc - revision 121 (9/8/98)
 * Implementation of LoginZone class
 */

#include "check.hh"
#include "dialog_zone.hh"
#include "employee.hh"
#include "labels.hh"
#include "labor.hh"
#include "login_zone.hh"
#include "report.hh"
#include "settings.hh"
#include "system.hh"
#include "terminal.hh"

#include <iostream>
#include <string>
#include <cctype>
#include <vector>
#include <map>
#include <string.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/***********************************************************************
 * Definitions
 ***********************************************************************/

enum status {
	STATE_GET_USER_ID,
	STATE_USER_ONLINE,
	STATE_PASSWORD_FAILED,
	STATE_UNKNOWN_USER,
	STATE_ON_ANOTHER_TERM,
	STATE_ALREADY_ON_CLOCK,
	STATE_NOT_ON_CLOCK,
	STATE_CLOCK_NOT_USED,
	STATE_OPEN_CHECK,
	STATE_ASSIGNED_DRAWER,
	STATE_USER_INACTIVE,
    STATE_NEED_BALANCE,
    STATE_NOT_ALLOWED_IN
};

enum {	// expedited login
	EXPEDITE_FASTFOOD   	= 1,
	EXPEDITE_TAKEOUT,

	EXPEDITE_PU_DELIV	= 11,
	EXPEDITE_TOGO,
	EXPEDITE_DINE_IN,

	EXPEDITE_KITCHEN1,
	EXPEDITE_KITCHEN2,
	EXPEDITE_BAR1,
	EXPEDITE_BAR2
};

/***********************************************************************
 * LoginZone Class 
 ***********************************************************************/

LoginZone::LoginZone()
{
    state       = STATE_GET_USER_ID;
    min_size_x  = 30;
    min_size_y  = 4;
    input       = 0;
    clocking_on = 0;
}

RenderResult LoginZone::Render(Terminal *term, int update_flag)
{
    FnTrace("LoginZone::Render()");
    if (update_flag)
    {
        clocking_on = 0;
        input       = 0;
    }

    LayoutZone::Render(term, update_flag);
    int col = color[0];
    int tmp;
    genericChar str[256];
    genericChar* currChar;

    Settings *settings = term->GetSettings();
    Employee *employee = term->user;

    if (employee == NULL && state == STATE_USER_ONLINE)
        state = STATE_GET_USER_ID;

	//this switch statement assigns the message in the upper
	//'frame' (center, top, wooden texture) on the login screen (screen -1)
    switch (state)
	{
    case STATE_GET_USER_ID:
        time.Clear();
        TextC(term, 0, term->Translate("Welcome"), col);
        TextC(term, 1, term->Translate("Please Enter Your User ID"), col);
        Entry(term, 2, 3, size_x - 4);
        currChar = str;
        tmp = input;
        while (tmp > 0)
        {
            tmp /= 10;
            *currChar++ = 'X';
        }
        *currChar++ = '_';
        *currChar = '\0';
        TextC(term, 3, str, COLOR_WHITE);
        break;

    case STATE_USER_ONLINE:
        snprintf(str, STRLENGTH, "%s %s", term->Translate("Hello"), employee->system_name.Value());
        TextC(term, .5, str, col);
        if (time.IsSet())
        {
            snprintf(str, STRLENGTH, "%s %s", term->Translate("Starting Time Is"),
                    term->TimeDate(time, TD_TIME));
            TextC(term, 1.5, str, col);
        }
        if (employee->CanEnterSystem(settings))
            TextC(term, 2.5, term->Translate("Press START To Enter"));
        break;

    case STATE_PASSWORD_FAILED:
        TextC(term,  .5, term->Translate("Password Incorrect"), col);
        TextC(term, 1.5, term->Translate("Please Try Again"), col);
        break;

    case STATE_UNKNOWN_USER:
        TextC(term, 1, term->Translate("Unknown User ID"), col);
        TextC(term, 2, term->Translate("Please Try Again"), col);
        break;

    case STATE_ON_ANOTHER_TERM:
        TextC(term, 1, term->Translate("You're Using Another Terminal"), col);
        break;

    case STATE_ALREADY_ON_CLOCK:
        TextC(term, 1, term->Translate("You're Already On The Clock"), col);
        break;

    case STATE_NOT_ON_CLOCK:
        TextC(term, 1, term->Translate("You're Not On The Clock"), col);
        break;

    case STATE_CLOCK_NOT_USED:
        TextC(term, 1, term->Translate("You Don't Use The Clock"), col);
        break;

    case STATE_OPEN_CHECK:
        TextC(term, 1, term->Translate("You Still Have Open Checks"), col);
        break;

    case STATE_ASSIGNED_DRAWER:
        TextC(term, 1, term->Translate("You Still Have An Assigned Drawer"), col);
        break;

    case STATE_USER_INACTIVE:
        TextC(term, 1, term->Translate("Your Record Is Inactive"), col);
        TextC(term, 2, term->Translate("Contact a manager to be reactivated"), col);
        break;

    case STATE_NEED_BALANCE:
        TextC(term, 1, term->Translate("You Need to Balance Your Drawer"), col);
        if (employee && !employee->IsManager(settings))
            TextC(term, 2, term->Translate("Contact a manager"), col);
        break;

    case STATE_NOT_ALLOWED_IN:
        TextC(term, 1, term->Translate("User is not allowed into the system"), col);
        break;

    default:
        break; /** do nothing **/
	} //end switch()

    return RENDER_OKAY;
}

SignalResult LoginZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("LoginZone::Signal()");
    const genericChar* commands[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "start", "clear", "backspace", "clockin", "clockout",
        "job0", "job1", "job2", "passwordgood", "passwordfailed",
        "passwordcancel", "faststart", "starttakeout", "gettextcancel", 
        "pickup", "quicktogo", "quickdinein", 
	"kds1", "kds2", "bar1", "bar2", NULL};
 
    int idx = CompareList(message, commands);
	if (idx < 0)
		return SIGNAL_IGNORED;

    // Update State
    if (state != STATE_GET_USER_ID && state != STATE_USER_ONLINE)
    {
        // Error Message Clears after touch/key/timeout
        state = STATE_GET_USER_ID;
        term->LogoutUser();
    }
    else if (state == STATE_USER_ONLINE && idx < 10)
    {
        state = STATE_GET_USER_ID;
        term->LogoutUser();
    }

    switch (idx)
    {
    case 10:  // start
        Start(term);
	if (term->user == NULL)
	    return SIGNAL_ERROR;
        break;
    case 11:  // clear
        if (term->user)
            term->LogoutUser();
        else
            Draw(term, 1);
        break;
    case 12:  // backspace
        if (input > 0) // backspace
        {
            if (state == STATE_GET_USER_ID)
            {
                input /= 10;
                Draw(term, 0);
            }
        }
        break;
    case 13:  // clockin
        if (input > 0) // clockon
            ClockOn(term);
        break;
    case 14:  // clockout
        if (input > 0)
            ClockOff(term);
        break;
    case 15:  // job0
        ClockOn(term, 0); 
        break;
    case 16:  // job1
        ClockOn(term, 1);
        break;
    case 17:  // job2
        ClockOn(term, 2);
        break;
    case 18:  // passwordgood
        if (clocking_on)
            ClockOn(term);
        else
            Start(term);
        break;
    case 19:  // passwordbad
        state = STATE_PASSWORD_FAILED;
        Draw(term, 0);
        break;
    case 20:  // passwordcancel
        term->LogoutUser();
        break;
    case 21:  // faststart
        // FastFood mode
        Start(term, EXPEDITE_FASTFOOD); //pass the 'expedite' flag to redirect the target page
        break;
    case 22:  // starttakeout
        Start(term, EXPEDITE_TAKEOUT);
        break;
    case 23:  // gettextcancel
        term->LogoutUser();
        break;
    case 24: // Pickup/Delivery
        Start(term, EXPEDITE_PU_DELIV);
        break;
    case 25: // Quick To-Go
        Start(term, EXPEDITE_TOGO);
        break;
    case 26: // Quick Dine-in
        Start(term, EXPEDITE_DINE_IN);
        break; 
    case 27: // direct to kitchen/bar video
        Start(term, EXPEDITE_KITCHEN1);
        break; 
    case 28: 
        Start(term, EXPEDITE_KITCHEN2);
        break; 
    case 29:
        Start(term, EXPEDITE_BAR1);
        break; 
    case 30: 
        Start(term, EXPEDITE_BAR2);
        break; 
    default:  // number keys or ignored
        if (idx >= 0 && idx <= 9 && input < 100000000)
        {
            if (state == STATE_GET_USER_ID)
            {
                input = (input * 10) + idx;
                Draw(term, 0);
            }
        }
        else
            return SIGNAL_IGNORED;
        break;
    }
    
    return SIGNAL_OKAY;
}

SignalResult LoginZone::Keyboard(Terminal *term, int my_key, int key_state)
{
    FnTrace("LoginZone::Keyboard()");
    // Update State
    if (my_key >= '0' && my_key <= '9')
    {
        genericChar str[] = {(char) my_key, '\0'};
        return Signal(term, str);
    }
    else if (my_key == 8)
    {
        return Signal(term, "backspace");
    }
    else if (my_key == 13)
    {
		//handle the ENTER key as 'normal' start
        return Signal(term, "start");
	}
    else
    {
        return SIGNAL_IGNORED;
    }
}

int LoginZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    FnTrace("LoginZone::Update()");
    if (update_message & UPDATE_TIMEOUT)
    {
        term->KillDialog();
        Signal(term, "clear");
    }
    return 0;
}

int LoginZone::ClockOn(Terminal *term, int job_no)
{
    FnTrace("LoginZone::ClockOn()");
	// param job_no defaults to -1

    System *sys = term->system_data;
    Employee *employee = term->user;
    Settings *settings = &(sys->settings);

    if (employee == NULL)
        employee = sys->user_db.FindByKey(input);

    if (employee == NULL)
    {
        state = STATE_UNKNOWN_USER;
        Draw(term, 0);
        return 0;
    }
    else if (!employee->active)
    {
        state = STATE_USER_INACTIVE;
        Draw(term, 0);
        return 0;
    }
    else if (!employee->UseClock())
    {
        state = STATE_CLOCK_NOT_USED;
        Draw(term, 0);
        return 0;
    }

    int job = sys->labor_db.CurrentJob(employee);
    if (job)
    {
        state = STATE_ALREADY_ON_CLOCK;
        Draw(term, 0);
        return 0;
    }

    if (job_no >= 0)
    {
        JobInfo *j = employee->FindJobByNumber(job_no);
        if (j == NULL)
        {
            printf("error finding job!\n");
            return 1; // shouldn't happen
        }
        job = j->job;
    }
    else
    {
        if (term->IsUserOnline(employee) && term->user != employee)
        {
            state = STATE_ON_ANOTHER_TERM;
            Draw(term, 0);
            return 0;
        }

        term->LoginUser(employee);
        if (employee->UsePassword(settings) && !term->password_given)
        {
            clocking_on = 1;
            term->OpenDialog(new PasswordDialog(employee->password.Value()));
            return 0;
        }

        // Ask user for job to work
        genericChar str[256];
        snprintf(str, STRLENGTH, "%s %s \\%s", term->Translate("Hello"), employee->system_name.Value(),
                term->Translate("Pick A Job For This Shift"));
        SimpleDialog *d = new SimpleDialog(str);

        int n = 0;
        for (JobInfo *j = employee->JobList(); j != NULL; j = j->next)
        {
            snprintf(str, STRLENGTH, "job%d", n++);
            d->Button(j->Title(term), str);
        }

        d->Button("Cancel", "clear");
        term->OpenDialog(d);

        return 0;
    }

    if (job == 0)
    {
        // User doesn't have any jobs
        state = STATE_UNKNOWN_USER;
        Draw(term, 0);
        return 0;
    }

    // Clockon user
    // need to check password 
    WorkEntry *worker = sys->labor_db.NewWorkEntry(employee, job);
    if (worker)
        time = worker->start;

    term->LoginUser(employee);
    state = STATE_USER_ONLINE;
    term->timeout = settings->start_page_timeout;
    Draw(term, 0);

    return 0;
}

int LoginZone::ClockOff(Terminal *term)
{
    FnTrace("LoginZone::ClockOff()");
    System *sys = term->system_data;
    Employee *employee = term->user;
    if (employee == NULL)
    {
        employee = sys->user_db.FindByKey(input);
        input = 0;
    }

    if (employee)
    {
        if (!employee->active)
            state = STATE_USER_INACTIVE;
        else if (!employee->UseClock())
            state = STATE_CLOCK_NOT_USED;
        else if (!sys->labor_db.IsUserOnClock(employee))
            state = STATE_NOT_ON_CLOCK;
        else if (sys->CountOpenChecks(employee) > 0)
            state = STATE_OPEN_CHECK;
        else if (term->IsUserOnline(employee) && term->user != employee)
            state = STATE_ON_ANOTHER_TERM;
        else if (term->NeedDrawerBalanced(employee))
            state = STATE_NEED_BALANCE;
        else
        {
            term->LoginUser(employee); // login without clock & starting page
            term->Jump(JUMP_PASSWORD, PAGEID_LOGOUT);
        }
    }
    else
        state = STATE_UNKNOWN_USER;

    Draw(term, 0);

    return 0;
}

int LoginZone::Start(Terminal *term, short expedite)
{
    FnTrace("LoginZone::Start()");
	// the 'expedite' param is the flag to invoke fast food mode
    System *sys = term->system_data;
    Settings *settings = &(sys->settings);
    Employee *employee = NULL;

	//establish the current meal period and use that info 
	//to determine which meal index page to load
	int nMealIndex = 0;
	int nFastFoodTarget = 0;

//    if (expedite == 1)
    if (// expedite == EXPEDITE_FASTFOOD ||
        expedite == EXPEDITE_PU_DELIV ||
        expedite == EXPEDITE_TOGO     ||
        expedite == EXPEDITE_DINE_IN)
	{
		// initialize index of the target page using time of day, to 
		// insure we go to the correct index page for the current
		// shift 
		nMealIndex = settings->MealPeriod(SystemTime);
		nFastFoodTarget = IndexValue[nMealIndex];  // IndexValue[] defined in main/labels.cc
	}

    if (input)
    {
		//pull from file if available
        employee = sys->user_db.FindByKey(input);
        input = 0;
    }
    else if (term->user)
    {
        employee = term->user;
    }
    else
    {
        return 0;
    }

    if (employee == NULL)
    {
        // no user found for given key
        state = STATE_UNKNOWN_USER;
        Draw(term, 0);
        return 0;
    }
    else if (! employee->active)
    {
        state = STATE_USER_INACTIVE;
        Draw(term, 0);
        return 0;
    }
    else if (term->user == employee)
    {
        // user already logged in - jump user to appropriate page
        if (employee->CanEnterSystem(settings))
        {
            if (expedite == EXPEDITE_FASTFOOD)
            {
            	if (settings->personalize_fast_food) {
                   term->QuickMode(CHECK_FASTFOOD);
                   term->Jump(JUMP_STEALTH, -8);
            	} else {
                   //jump immediately to food order page
                   term->QuickMode(CHECK_FASTFOOD);
                   term->JumpToIndex(nFastFoodTarget);
            	}
            }
            else if (expedite == EXPEDITE_TAKEOUT)
            {
                term->QuickMode(CHECK_TAKEOUT);
                term->Jump(JUMP_STEALTH, -8);
            }
            else if (expedite == EXPEDITE_PU_DELIV)
            {
            	term->QuickMode(CHECK_CALLIN);
            	term->Jump(JUMP_STEALTH, -8);
            }
            else if (expedite == EXPEDITE_TOGO)
            {
            	term->QuickMode(CHECK_TOGO);
                term->JumpToIndex(nFastFoodTarget);
            }
            else if (expedite == EXPEDITE_DINE_IN)
            {
            	term->QuickMode(CHECK_DINEIN);
                term->JumpToIndex(nFastFoodTarget);
            }
            else
            {
                // jump to special start page
		int ptype = 0;
            	if (expedite == EXPEDITE_KITCHEN1)
			ptype = PAGE_KITCHEN_VID;
            	else if (expedite == EXPEDITE_KITCHEN2)
			ptype = PAGE_KITCHEN_VID2;
            	else if (expedite == EXPEDITE_BAR1)
			ptype = PAGE_BAR1;
            	else if (expedite == EXPEDITE_BAR2)
			ptype = PAGE_BAR2;
		if (ptype) {
			Page *pg = term->zone_db->FindByType(ptype, -1, term->size);
			if (pg)
			{
            			term->Jump(JUMP_STEALTH, pg->id);
				return 0;
			}
		}
                // jump to home page
                term->Jump(JUMP_STEALTH, term->HomePage());
            }
        }
        else
        {
            state = STATE_NOT_ALLOWED_IN;
            return 1;
        }

        return 0;
    }
    else if (term->IsUserOnline(employee))
    {
        // user already logged into another term
        state = STATE_ON_ANOTHER_TERM;
        Draw(term, 0);
        return 0;
    }
    else if (! sys->labor_db.IsUserOnClock(employee))
    {
        state = STATE_NOT_ON_CLOCK;
        Draw(term, 0);
        return 0;
    }
    else if (employee->UsePassword(settings) &&
             ((employee->IsManager(settings) && (! term->password_given)) ||
              settings->min_pw_len > employee->password.size()))
    {
        term->LoginUser(employee);
        term->OpenDialog(new PasswordDialog(employee->password.Value()));
        return 0;
    }

	//no special conditions met, so handle as normal
    state = STATE_USER_ONLINE;

    if (employee->CanEnterSystem(settings))
    {
//        if (expedite == 1)
        if (expedite == EXPEDITE_FASTFOOD)
        {
            term->LoginUser(employee); //login, but don't jump to a page
            //create a new check and jump to the menu page
           	if (settings->personalize_fast_food) {
                term->QuickMode(CHECK_FASTFOOD);
                term->Jump(JUMP_STEALTH, -8);
           	} else {
                //jump immediately to food order page
                term->QuickMode(CHECK_FASTFOOD);
                term->JumpToIndex(nFastFoodTarget);
         	}
        }
//        else if (expedite == 2)
        else if (expedite == EXPEDITE_TAKEOUT)
        {
            term->LoginUser(employee);
            term->QuickMode(CHECK_TAKEOUT);
            term->Jump(JUMP_STEALTH, -8);
        }
        else if (expedite == EXPEDITE_PU_DELIV)
        {
            term->LoginUser(employee);
        	term->QuickMode(CHECK_CALLIN);
        	term->Jump(JUMP_STEALTH, -8);
        }
        else if (expedite == EXPEDITE_TOGO)
        {
            term->LoginUser(employee);
            term->QuickMode(CHECK_TOGO);
            term->JumpToIndex(nFastFoodTarget);
        }
        else if (expedite == EXPEDITE_DINE_IN)
        {
            term->LoginUser(employee);
        	term->QuickMode(CHECK_DINEIN);
            term->JumpToIndex(nFastFoodTarget);
        }
        else
        {
	    int ptype = 0;
            if (expedite == EXPEDITE_KITCHEN1)
		ptype = PAGE_KITCHEN_VID;
            else if (expedite == EXPEDITE_KITCHEN2)
		ptype = PAGE_KITCHEN_VID2;
            else if (expedite == EXPEDITE_BAR1)
		ptype = PAGE_BAR1;
            else if (expedite == EXPEDITE_BAR2)
		ptype = PAGE_BAR2;
	    if (ptype) {
		Page *pg = term->zone_db->FindByType(ptype, -1, term->size);
		if (pg) {
		    term->LoginUser(employee);
		    term->Jump(JUMP_STEALTH, pg->id);
		    return 0;
		}
	    }

            //send to homepage
            term->LoginUser(employee, 1); //login and jump to homepage
        }
    }
    else
    {
        state = STATE_NOT_ALLOWED_IN;
        Draw(term, 0);
        return 1;
    }

    return 0;
}


/***********************************************************************
 * LogoutZone Class
 ***********************************************************************/

#define ENTRY_LINE 11

LogoutZone::LogoutZone()
{
    work = NULL;
}

RenderResult LogoutZone::Render(Terminal *term, int update_flag)
{
    FnTrace("LogoutZone::Render()");
    LayoutZone::Render(term, update_flag);
    Employee *employee = term->user;
    if (employee == NULL)
    {
        TextC(term, 1, "No Employee Logged In");
        return RENDER_OKAY;
    }

    System *sys = term->system_data;
    Settings *settings = &(sys->settings);
    if (update_flag)
    {
        time_out = SystemTime;
        work     = sys->labor_db.CurrentWorkEntry(employee);
    }

    if (work == NULL)
    {
        TextC(term, 1, "Strange, No Work Info For You...");
        return RENDER_OKAY;
    }

    TextC(term, 1, "Shift Work Summary", color[0]);
    Line(term, 3, color[0]);
    int shift_min = work->MinutesWorked();

    TimeInfo start, end;
    start = work->start;
    start.Floor<std::chrono::minutes>();
    end   = time_out;
    end.Floor<std::chrono::minutes>();

    genericChar str[256];
    snprintf(str, STRLENGTH, "     Shift Start: %s", term->TimeDate(start, TD0));
    TextL(term, 5, str, color[0]);

    snprintf(str, STRLENGTH, "    Current Time: %s", term->TimeDate(end, TD0));
    TextL(term, 6, str, color[0]);

    genericChar hstr[32], mstr[32];
    int hour = (shift_min / 60), m = (shift_min % 60);
    if (hour == 1)
        snprintf(hstr, STRLENGTH, "1 hour");
    else
        snprintf(hstr, STRLENGTH, "%d hours", hour);

    if (m == 0)
        snprintf(mstr, STRLENGTH, "0 minutes");
    else
        snprintf(mstr, STRLENGTH, "%d minutes", m);

    genericChar str2[256];
    if (hour > 0 && m > 0)
        snprintf(str2, STRLENGTH, "%s, %s", hstr, mstr);
    else if (hour > 0)
        strcpy(str2, hstr);
    else
        strcpy(str2, mstr);

    snprintf(str, STRLENGTH, " Total Work Time: %s", str2);
    TextL(term, 7, str, color[0]);

    if (!employee->CanOrder(settings))
        return RENDER_OKAY;

    TextC(term, 9, "Enter Declared Tips Total", color[0]);
    RenderPaymentEntry(term, ENTRY_LINE);
    return RENDER_OKAY;
}

SignalResult LogoutZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("LogoutZone::Signal()");
    const genericChar* commands[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "00",
            "cancel", "clockoff", "backspace", "clear", "save", "break", NULL};
	int idx = CompareList(message, commands);

	if (idx < 0)
		return SIGNAL_IGNORED;

    if (idx == 11)
    {
        // cancel
        term->LogoutUser();
        return SIGNAL_OKAY;
    }

    Employee *employee = term->user;
    if (employee == NULL || work == NULL)
        return SIGNAL_IGNORED;

    System *sys = term->system_data;
    Settings *settings = &(sys->settings);

    switch (idx)
	{
    case 10: // 00
        if (employee->CanOrder(settings) && work->tips < 10000)
        {
            work->tips *= 100;
            DrawPaymentEntry(term, ENTRY_LINE);
            return SIGNAL_OKAY;
        }
        break;

    case 12: // clockoff
        ClockOff(term, 1);
        return SIGNAL_OKAY;

    case 13: // backspace
        if (employee->CanOrder(settings) && work->tips > 0)
        {
            work->tips /= 10;
            DrawPaymentEntry(term, ENTRY_LINE);
            return SIGNAL_OKAY;
        }
        break;

    case 14: // clear
        if (employee->CanOrder(settings) && work->tips > 0)
        {
            work->tips = 0;
            DrawPaymentEntry(term, ENTRY_LINE);
            return SIGNAL_OKAY;
        }
        break;

    case 15: // save
    {
        LaborPeriod *lp = sys->labor_db.CurrentPeriod();
        if (lp)
            lp->Save();
        return SIGNAL_OKAY;
    }

    case 16: // break;
        ClockOff(term, 0);
        return SIGNAL_OKAY;

    default:
        if (employee->CanOrder(settings) && work->tips < 100000)
        {
            work->tips *= 10;
            work->tips += idx;
            DrawPaymentEntry(term, ENTRY_LINE);
            return SIGNAL_OKAY;
        }
        break;
	}// end switch

    return SIGNAL_IGNORED;
}

SignalResult LogoutZone::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("LogoutZone::Keyboard()");
    if (my_key >= '0' && my_key <= '9')
    {
        genericChar str[] = {(char) my_key, '\0'};
        return Signal(term, str);
    }
    else if (my_key == 8)
        return Signal(term, "backspace");
    else if (my_key == 27)
        return Signal(term, "cancel");
    else
        return SIGNAL_IGNORED;
}

int LogoutZone::RenderPaymentEntry(Terminal *term, int line)
{
    FnTrace("LogoutZone::RenderPaymentEntry()");
    if (work == NULL)
        return 1;

    genericChar str[128];
    int cents   = work->tips % 100;
    int dollars = work->tips / 100;

    TextL(term, line, " Input Amount:", color[0]);
    if (dollars <= 0)
        snprintf(str, STRLENGTH, ".%02d", cents);
    else
        snprintf(str, STRLENGTH, "%d.%02d", dollars, cents);

    Entry(term, 16, line, 7);
    TextPosR(term, 23, line, str, COLOR_YELLOW);
    return 0;
}

int LogoutZone::DrawPaymentEntry(Terminal *term, int line)
{
    FnTrace("LogoutZone::DrawPaymentEntry()");
    RenderPaymentEntry(term, line);
    term->UpdateArea(x, y, w, h);
    return 0;
}

int LogoutZone::ClockOff(Terminal *term, int end_shift)
{
    FnTrace("LogoutZone::ClockOff()");
    Employee *e = term->user;
    if (e == NULL || work == NULL)
        return 1;

    System *sys = term->system_data;
    sys->labor_db.EndWorkEntry(e, end_shift);
    e->last_job = 0;
    if (end_shift)
    {
        Printer *p = term->FindPrinter(PRINTER_RECEIPT);
        if (p)
        {
            Report r;
            if (sys->labor_db.WorkReceipt(term, e, &r) == 0)
                r.Print(p);
        }
    }
    term->LogoutUser();
    return 0;
}
