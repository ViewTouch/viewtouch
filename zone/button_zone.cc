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
 * button_zone.cc - revision 99 (10/13/98)
 * Implementation of ButtonZone classes
 */

#include "button_zone.hh"
#include "manager.hh"
#include "terminal.hh"
#include "employee.hh"
#include "check.hh"
#include "sales.hh"
#include "dialog_zone.hh"
#include "labels.hh"
#include "system.hh"
#include <string.h>
#include <unistd.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define COMMAND_OUTPUT_FILE VIEWTOUCH_PATH "/dat/text/command.log"


/**** ButtonZone Class ****/
// Constructor
ButtonZone::ButtonZone()
{
    jump_type = JUMP_NONE;
    jump_id   = 0;
}

// Member Functions
Zone *ButtonZone::Copy()
{
    FnTrace("ButtonZone::Copy()");
    ButtonZone *z = new ButtonZone;
    z->SetRegion(this);
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
    return z;
}

SignalResult ButtonZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("ButtonZone::Touch()");
    if (term->Jump(jump_type, jump_id))
        return SIGNAL_IGNORED;
    else
        return SIGNAL_OKAY;
}


/**** MessageButtonZone Class ****/

MessageButtonZone::MessageButtonZone()
{
    FnTrace("MessageButtonZone::MessageButtonZone()");
    confirm = 0;
    confirm_msg.Set("");
}

Zone *MessageButtonZone::Copy()
{
    FnTrace("MessageButtonZone::Copy()");
    MessageButtonZone *z = new MessageButtonZone;
    z->SetRegion(this);
    z->message.Set(message);
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
    z->confirm   = confirm;
    z->confirm_msg.Set(confirm_msg);
    return z;
}

SignalResult MessageButtonZone::SendandJump(Terminal *term)
{
    FnTrace("MessageButtonZone::SendandJump()");
    SignalResult sig = SIGNAL_OKAY;
    char signal[STRLONG] = "";
    char command[STRLONG] = VIEWTOUCH_PATH;
    const char* validcommand = NULL;
    int len = 0;
    int idx = 0;

    if (message.size() > 0)
        strcpy(signal, message.Value());
    else if (name.size() > 0)
        strcpy(signal, name.Value());

    len = strlen(signal);
    if (len > 0)
    {
        if (strncmp(signal, "RUNCMD:", 7) == 0)
        {
            idx = 7;
            while (signal[idx] == ' ' && idx < len)
                idx++;
            validcommand = ValidateCommand(&signal[idx]);
            if (validcommand != NULL)
            {
                snprintf(command, STRLONG, "%s >%s 2>&1", validcommand, COMMAND_OUTPUT_FILE);
                system(command);
                term->Draw(1);
            }
        }
        else
            sig = term->Signal(signal, group_id);
    }

    if (sig != SIGNAL_ERROR)
    {
        // Critical fix: If the signal was "save", ensure save operation completes before jumping
        if (strcmp(signal, "save") == 0)
        {
            // Force immediate save to disk and wait for completion
            Settings *settings = term->GetSettings();
            if (settings)
            {
                settings->Save();
                // Give a small delay to ensure file write completes
                usleep(100000); // 100ms delay
            }
            
            // Also ensure any other pending saves are completed
            System *sys = term->system_data;
            if (sys)
            {
                sys->SaveChanged();
                usleep(50000); // Additional 50ms delay for system saves
            }
        }
        term->Jump(jump_type, jump_id);
    }

    return sig;
}

SignalResult MessageButtonZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("MessageButtonZone::Touch()");
    SignalResult sig = SIGNAL_OKAY;
    DialogZone *confirm_dg;

    if (confirm)
    {
        confirm_dg = new SimpleDialog(confirm_msg.Value());
        confirm_dg->Button("Yes", "sendandjump");
        confirm_dg->Button("No");
        confirm_dg->target_zone = this;
        term->OpenDialog(confirm_dg);
    }
    else
    {
        sig = SendandJump(term);
    }

    return sig;
}

SignalResult MessageButtonZone::Signal(Terminal *term, const char* signal_msg)
{
    FnTrace("MessageButtonZone::Signal()");
    SignalResult sig = SIGNAL_OKAY;
    const char* command_list[] = {
        "sendandjump",
	"starttakeout", "pickup", "quicktogo", "quickdinein", NULL};

    Settings *settings = term->GetSettings();
    int idx = CompareListN(command_list, signal_msg);

    switch (idx)
    {
    case 0: // sendandjump
        sig = SendandJump(term);
        break;
    case 1: // starttakeout
	if (term->QuickMode(CHECK_TAKEOUT))
	    return SIGNAL_IGNORED;
	term->Jump(JUMP_STEALTH, -8);
        break;
    case 2:  // pickup/delivery
	if (term->QuickMode(CHECK_CALLIN))
	    return SIGNAL_IGNORED;
	term->Jump(JUMP_STEALTH, -8);
        break;
    case 3:  // quick to-go
    	if (term->QuickMode(CHECK_TOGO))
	    return SIGNAL_IGNORED;
	term->JumpToIndex(IndexValue[settings->MealPeriod(SystemTime)]);
        break;
    case 4:  // quick dine-in
    	if (term->QuickMode(CHECK_DINEIN))
	    return SIGNAL_IGNORED;
	term->JumpToIndex(IndexValue[settings->MealPeriod(SystemTime)]);
        break;
    default:
        sig = SIGNAL_IGNORED;
        break;
    }

    return sig;
}

/****
 * ValidateCommand:  Validate the command we need to run.  We'll allow upper and
 *  lower case letters, numbers, space, dash, and underscore.  We really
 *  don't need to allow anything else.  Also prepends /usr/viewtouch/bin.
 *
 *  Returns the validated command, or NULL if the command is invalid.  The
 *  source will also be modified if the command is valid.
 ****/
char* MessageButtonZone::ValidateCommand(char* source)
{
    FnTrace("MessageButtonZone::ValidCommand()");
    char* retval = NULL;
    int len = strlen(source);
    int sidx = 0;
    char dest[STRLONG] = "";
    int didx = strlen(dest);
    int badchar = 0;

    // do not allow the command to start with a dot.
    if (source[sidx] == '.')
        return NULL;

    while (sidx < len)
    {
        char item = source[sidx];
        if ((item >= '0' && item <= '9') ||
            (item >= 'A' && item <= 'Z') ||
            (item >= 'a' && item <= 'z') ||
            item == ' ' || item == '-' || item == '_' ||
            item == '.' || item == '/' || item == ':' ||
            item == '$' || item == '|' || item == '&' ||
            item == '>' || item == '<' || item == ';' ||
            item == '(' || item == ')' || item == '[' ||
            item == ']' || item == '{' || item == '}' ||
            item == '"' || item == '\'' || item == '`' ||
            item == '!' || item == '?' || item == '*' ||
            item == '+' || item == '=' || item == '~' ||
            item == '@' || item == '#' || item == '%' ||
            item == '^' || item == '\\')
        {
            dest[didx] = source[sidx];
            didx += 1;
            sidx += 1;
        }
        else
        {
            badchar = 1;
            sidx = len;
        }
    }

    if (badchar)
        retval = NULL;
    else
    {
        dest[didx] = '\0';
        strcpy(source, dest);
        retval = source;
    }
    return retval;
}


/**** ConditionalZone class ****/
static const genericChar* KeyWords[] = {
    "check", "guests", "subchecks", "settle", "order", "drawer",
    "drawercount", "orderbyseat", "developer", "flow", "assigned",
    "local", "supervisor", "manager", "editusers", "merchandise",
    "movetable", "tablepages", "passwords", "superuser",
    "payexpenses", "fastfood", "lastendday",
    "checkbalanced", "haspayments", "training", "selectedorder", NULL};
enum comms {
    CHECK, GUESTS, SUBCHECKS, SETTLE, ORDER, DRAWER,
    DRAWERCOUNT, ORDERBYSEAT, DEVELOPER, FLOW, ASSIGNED,
    LOCAL, SUPERVISOR, MANAGER, EDITUSERS, MERCHANDISE,
    MOVETABLE, TABLEPAGES, PASSWORDS, SUPERUSER,
    PAYEXPENSES, FASTFOOD, LASTENDDAY,
    CHECKBALANCED, HASPAYMENTS, TRAINING, SELECTORDER};

static const genericChar* OperatorWords[] = {
    "=", ">", "<", "!=", ">=", NULL};
enum operators {
    EQUAL, GREATER, LESSER, NOTEQUAL, GREATEREQUAL
};

// Constructor
ConditionalZone::ConditionalZone()
{
    keyword = -1;
    op      = -1;
    val     =  0;
}

// Member Functions
Zone *ConditionalZone::Copy()
{
    FnTrace("ConditionalZone::Copy()");
    ConditionalZone *z = new ConditionalZone;
    z->SetRegion(this);
    z->expression.Set(expression);
    z->message.Set(message);
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
    return z;
}

int ConditionalZone::RenderInit(Terminal *term, int update_flag)
{
    FnTrace("ConditionalZone::RenderInit()");
    if (keyword < 0)
    {
        genericChar str1[256] = "", str2[256] = "";

        // FIX - really lame expression parser
        sscanf(expression.Value(), "%s %s %d", str1, str2, &val);
        keyword = CompareList(str1, KeyWords);
        op      = CompareList(str2, OperatorWords);
    }
    active = EvalExp(term);
    return 0;
}

SignalResult ConditionalZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("ConditionalZone::Touch()");
    SignalResult sig = SIGNAL_OKAY;

    if (message.size() > 0)
    {
        // broadcast button's message
        sig = term->Signal(message.Value(), group_id);
    }
    else if (name.size() > 0)
    {
        // broadcast button's name
        sig = term->Signal(name.Value(), group_id);
    }

    if (sig != SIGNAL_ERROR)
        term->Jump(jump_type, jump_id);
    return sig;
}

int ConditionalZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    FnTrace("ConditionalZone::Update()");
    if (update_message & (UPDATE_CHECKS))
    {
        int eval = EvalExp(term);
        if (eval != active)
        {
            active = eval;
            return Draw(term, 0);
        }
    }
    return 0;
}

int ConditionalZone::EvalExp(Terminal *term)
{
    FnTrace("ConditionalZone::EvalExp()");
    int n = 0;
    Check    *c = term->check;
    Employee *e = term->user;
    Settings *s = term->GetSettings();

    switch (keyword)
    {
    case CHECK: // Check
        if (c)
            n = 1;
        break;
    case GUESTS: // Guests
        if (c)
            n = c->Guests();
        break;
    case SUBCHECKS: // Subchecks
        if (c)
            n = c->SubCount();
        break;
    case SETTLE: // settle (can settle checks)
        n = term->CanSettleCheck();
        break;
    case ORDER: // order (can order food)
        if (e)
            n = e->CanOrder(s);
        break;
    case DRAWER: // drawer
        if (term->FindDrawer())
            n = 1;
        break;
    case DRAWERCOUNT: // drawercount
        n = term->drawer_count;
        break;
    case ORDERBYSEAT: // orderbyseat
        n = s->use_seats;
        break;
    case DEVELOPER: // developer
        if (e)
            n = e->CanEdit();
        break;
    case FLOW: // flow
        n = !(term->page->type == PAGE_SCRIPTED3);
        break;
    case ASSIGNED: // assigned
        n = (s->drawer_mode == DRAWER_ASSIGNED);
        break;
    case LOCAL: // local
        n = term->is_server;
        break;
    case SUPERVISOR: // supervisor
        if (e)
            n = e->IsSupervisor(s);
        break;
    case MANAGER: // manager
        if (e)
            n = e->IsManager(s);
        break;
    case EDITUSERS: // editusers
        if (e)
            n = e->CanEditUsers(s);
        break;
    case MERCHANDISE: // merchandise
        n = s->family_group[FAMILY_MERCHANDISE] != SALESGROUP_NONE;
        break;
    case MOVETABLE: // movetable
        if (e)
            n = e->CanMoveTables(s);
        break;
    case TABLEPAGES: // tablepages
        n = term->zone_db->table_pages;
        break;
    case PASSWORDS: // passwords
        n = s->password_mode;
        break;
    case SUPERUSER: // superuser
        if (e)
            n = e->CanEditSystem();
        break;
    case PAYEXPENSES: // payexpenses
        if (e)
            n = e->CanPayExpenses(s);
        break;
    case FASTFOOD: // fastfood
        n = (term->type == TERMINAL_FASTFOOD);
        break;
    case LASTENDDAY: // lastendday
        if (term && term->system_data && term->system_data->CheckEndDay(term) > 0)
            n = term->system_data->LastEndDay();
        else
            n = -1;
        break;
    case CHECKBALANCED: // checkbalanced
        n = term->check_balanced;
        break;
    case HASPAYMENTS: // haspayments
        n = term->has_payments;
        break;
    case TRAINING: // training
        // This is to allow buttons to be shown in training mode only.  The
        // buttons will primarily be for documentation.  Similar to the comment
        // button that only shows up for the superuser, but easier to program.
        if (e)
            n = e->training;
        break;
    case SELECTORDER:  // selectedorder
        n = (term->order ? 1 : 0);
        break;
    default:
        return 0;
    }

    int retval = 0;
    switch (op)
    {
    case EQUAL:
        retval = (n == val);
        break;
    case GREATER:
        retval = (n > val);
        break;
    case LESSER:
        retval = (n < val);
        break;
    case NOTEQUAL:
        retval = (n != val);
        break;
    case GREATEREQUAL:
        retval = (n >= val);
        break;
    default:
        // null case; return 0
        break;
    }
    return retval;
}

/**** ToggleZone Class ****/
int StrStates(const char* str)
{
    const genericChar* c = str, d = str[0];
    int count = 0;

    while (*c)
        if (*c++ == d)
            ++count;
    return count;
}

int StrString(char* buffer, const genericChar* str, int state)
{
    const genericChar* c = str, d = str[0];

    while (*c)
    {
        if (*c == d)
        {
            --state;
            if (state < 0)
            {
                ++c;
                while (*c && *c != d)
                    *buffer++ = *c++;

                *buffer = '\0';
                return 0;
            }
        }
        ++c;
    }

    buffer[0] = '\0';
    return 1;
}

// Constructor
ToggleZone::ToggleZone()
{
    state      = 0;
    max_states = 0;
}

// Member Functions
RenderResult ToggleZone::Render(Terminal *term, int update_flag)
{
    FnTrace("ToggleZone::Render()");
    if (update_flag)
        state = 0;

    genericChar str[256];
    max_states = StrStates(name.Value());
    StrString(str, name.Value(), state % max_states);
    RenderZone(term, str, update_flag);
    return RENDER_OKAY;
}

SignalResult ToggleZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("ToggleZone::Touch()");
    genericChar str[256];
    StrString(str, message.Value(), state % max_states);
    term->Signal(str, group_id);

    ++state;
    if (state >= max_states)
        state = 0;
    Draw(term, 0);
    return SIGNAL_OKAY;
}

const char* ToggleZone::TranslateString(Terminal *term)
{
    FnTrace("ToggleZone::TranslateString()");
    static genericChar str[256];
    StrString(str, name.Value(), state % max_states);
    return str;
}

/**** CommentZone Class ****/
// Member Functions
int CommentZone::RenderInit(Terminal *term, int update_flag)
{
    FnTrace("CommentZone::RenderInit()");
    behave = BEHAVE_MISS;
    frame[1] = ZF_HIDDEN;
    frame[2] = ZF_HIDDEN;
    Employee *e = term->user;
    active = (e && e->CanEditSystem());
    return 0;
}

/**** KillSystemZone Class ****/
// Member Functions
RenderResult KillSystemZone::Render(Terminal *term, int update_flag)
{
    FnTrace("KillSystemZone::Render()");
    int users = term->OtherTermsInUse(1);
    if (users <= 0)
        RenderZone(term, name.Value(), update_flag);
    else if (users == 1)
        RenderZone(term, "1 Terminal Busy", update_flag);
    else
    {
        genericChar str[32];
        sprintf(str, "%d Terminals Busy", users);
        RenderZone(term, str, update_flag);
    }
    return RENDER_OKAY;
}

SignalResult KillSystemZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("KillSystemZone::Touch()");
    Employee *e = term->user;
    int users = term->OtherTermsInUse();
    if (users > 0 && (e == NULL || !e->CanEdit()))
        return SIGNAL_IGNORED;

    SimpleDialog *d =
        new SimpleDialog(term->Translate("Confirm Your Choice:"));
    d->Button("Quit ViewTouch and Return To The Desktop", "shutdown");
    d->Button("Refresh ViewTouch", "systemrestart");
    d->Button("Don't Quit or Refresh");
    term->OpenDialog(d);
    return SIGNAL_OKAY;
}

int KillSystemZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    FnTrace("KillSystemZone::Update()");
    if (update_message & UPDATE_USERS)
    {
        Employee *e = term->user;
        int users = term->OtherTermsInUse();
        if (users > 0 && (e == NULL || !e->CanEdit()))
            term->KillDialog();
        return Draw(term, 1);
    }
    return 0;
}

/**** StatusZone class ****/
//Member Function

StatusZone::StatusZone()
{
    status.Set("");
}

RenderResult StatusZone::Render(Terminal *term, int update_flag)
{
    TextC(term, 0, status.Value(), color[0]);
    return LayoutZone::Render(term, update_flag);
}

SignalResult StatusZone::Signal(Terminal *term, const genericChar* message)
{
    const genericChar* command_list[] = {
        "status", "clearstatus", NULL };
    int idx = CompareListN(command_list, message);

    switch (idx)
    {
    case 0:  // status
        if (strlen(message) > 7)
            status.Set(&message[7]);
        else
            status.Set("");
        Draw(term, 1);
        status.Set("");
        term->RedrawZone(this, 2000);
        break;
    case 1:  // clearstatus
        status.Set("");
        Draw(term, 1);
        break;
    }
    return SIGNAL_IGNORED;
}

