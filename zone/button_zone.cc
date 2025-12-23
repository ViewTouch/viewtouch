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
#include "safe_string_utils.hh"
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

RenderResult ButtonZone::Render(Terminal *term, int update_flag)
{
    FnTrace("ButtonZone::Render()");

    int show_images = term->show_button_images;
    Settings *settings = term->GetSettings();
    int text_position = settings ? settings->button_text_position : 0;
    
    Str *path = ImagePath();
    if (path && path->size() > 0 && show_images)
    {
        // Render base zone visuals (frame, shadows)
        if (text_position == 0)
        {
            // Text over image - render frame without text, then image, then text
            RenderZone(term, "", update_flag);
            
            // Calculate interior area to keep frames visible
            int horizontal_pad = Max(border - 2, 0);
            int vertical_pad   = Max(border - 4, 0);
            
            int px = x + horizontal_pad;
            int py = y + vertical_pad + header;
            int pw = w - (horizontal_pad * 2);
            int ph = h - ((vertical_pad * 2) + header + footer);
            
            if (pw <= 0 || ph <= 0)
            {
                px = x;
                py = y;
                pw = w;
                ph = h;
            }
            
            // Draw the image
            term->RenderPixmap(px, py, pw, ph, path->Value());
            
            // Draw text overlay
            const genericChar* text = term->ReplaceSymbols(name.Value());
            if (text)
            {
                int state = State(term);
                int c = color[state];
                if (c == COLOR_PAGE_DEFAULT || c == COLOR_DEFAULT)
                    c = term->page->default_color[state];
                if (c != COLOR_CLEAR)
                {
                    int bx = Max(border - 2, 0);
                    int by = Max(border - 4, 0);
                    term->RenderZoneText(text, x + bx, y + by + header, w - (bx*2),
                                        h - (by*2) - header - footer, c, font);
                }
            }
            
            return RENDER_OKAY;
        }
        else if (text_position == 1)
        {
            // Text above image - split button vertically
            RenderZone(term, "", update_flag);
            
            // Text area is top 30% of button
            const genericChar* text = term->ReplaceSymbols(name.Value());
            if (text)
            {
                int state = State(term);
                int c = color[state];
                if (c == COLOR_PAGE_DEFAULT || c == COLOR_DEFAULT)
                    c = term->page->default_color[state];
                if (c != COLOR_CLEAR)
                {
                    int bx = Max(border - 2, 0);
                    int text_height = (h * 30) / 100;  // Top 30%
                    term->RenderZoneText(text, x + bx, y + bx + header, w - (bx*2),
                                        text_height, c, font);
                }
            }
            
            // Image area is bottom 70% of button
            int horizontal_pad = Max(border - 2, 0);
            int text_height = (h * 30) / 100;
            int px = x + horizontal_pad;
            int py = y + text_height + header;
            int pw = w - (horizontal_pad * 2);
            int ph = h - text_height - horizontal_pad - header - footer;
            
            if (pw > 0 && ph > 0)
                term->RenderPixmap(px, py, pw, ph, path->Value());
            
            return RENDER_OKAY;
        }
        else if (text_position == 2)
        {
            // Text below image - split button vertically
            RenderZone(term, "", update_flag);
            
            // Image area is top 70% of button
            int horizontal_pad = Max(border - 2, 0);
            int text_height = (h * 30) / 100;
            int px = x + horizontal_pad;
            int py = y + horizontal_pad + header;
            int pw = w - (horizontal_pad * 2);
            int ph = h - text_height - horizontal_pad - header - footer;
            
            if (pw > 0 && ph > 0)
                term->RenderPixmap(px, py, pw, ph, path->Value());
            
            // Text area is bottom 30% of button
            const genericChar* text = term->ReplaceSymbols(name.Value());
            if (text)
            {
                int state = State(term);
                int c = color[state];
                if (c == COLOR_PAGE_DEFAULT || c == COLOR_DEFAULT)
                    c = term->page->default_color[state];
                if (c != COLOR_CLEAR)
                {
                    int bx = Max(border - 2, 0);
                    int image_bottom = y + ph + horizontal_pad + header;
                    term->RenderZoneText(text, x + bx, image_bottom, w - (bx*2),
                                        text_height, c, font);
                }
            }
            
            return RENDER_OKAY;
        }
    }

    // Default: call parent Zone render for normal button appearance (no image or images disabled)
    return Zone::Render(term, update_flag);
}

// Member Functions
std::unique_ptr<Zone> ButtonZone::Copy()
{
    FnTrace("ButtonZone::Copy()");
    auto z = std::make_unique<ButtonZone>();
    (void)z->SetRegion(this);  // SetRegion has nodiscard, but we only need side effect
    z->name.Set(name);
    z->key       = key;
    z->behave    = behave;
    z->font      = font;
    z->shape     = shape;
    z->group_id  = group_id;
    z->jump_type = jump_type;
    z->jump_id   = jump_id;
    if (ImagePath() && z->ImagePath())
    {
        z->ImagePath()->Set(*ImagePath());
        FILE *debugfile = fopen("/tmp/viewtouch_debug.log", "a");
        if (debugfile) {
            fprintf(debugfile, "ButtonZone::Copy: Copied image path '%s' for zone '%s'\n",
                    ImagePath()->Value(), name.Value());
            fclose(debugfile);
        }
    }
    for (int i = 0; i < 3; ++i)
    {
        z->color[i]   = color[i];
        z->image[i]   = image[i];
        z->frame[i]  = frame[i];
        z->texture[i] = texture[i];
    }
    return z;
}

SignalResult ButtonZone::Touch(Terminal *term, int /*tx*/, int /*ty*/)
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

std::unique_ptr<Zone> MessageButtonZone::Copy()
{
    FnTrace("MessageButtonZone::Copy()");
    auto z = std::make_unique<MessageButtonZone>();
    (void)z->SetRegion(this);  // SetRegion has nodiscard, but we only need side effect
    z->message.Set(message);
    z->name.Set(name);
    z->key       = key;
    z->behave    = behave;
    z->font      = font;
    z->shape     = shape;
    z->group_id  = group_id;
    z->jump_type = jump_type;
    z->jump_id   = jump_id;
    if (ImagePath() && z->ImagePath())
        z->ImagePath()->Set(*ImagePath());
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
    const char* validcommand = nullptr;
    int len = 0;
    int idx = 0;

    if (message.size() > 0)
        vt_safe_string::safe_copy(signal, STRLONG, message.Value());
    else if (name.size() > 0)
        vt_safe_string::safe_copy(signal, STRLONG, name.Value());

    len = static_cast<int>(strlen(signal));
    if (len > 0)
    {
        if (strncmp(signal, "RUNCMD:", 7) == 0)
        {
            idx = 7;
            while (signal[idx] == ' ' && idx < len)
                idx++;
            validcommand = ValidateCommand(&signal[idx]);
            if (validcommand != nullptr)
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
        
        // For SelfOrder terminals with "cancel" message, don't jump - let custom cancel logic handle navigation
        if (term->type == TERMINAL_SELFORDER && strcmp(signal, "cancel") == 0)
        {
            // Skip the jump - our custom cancel logic in OrderEntryZone will handle navigation
        }
        else
        {
            term->Jump(jump_type, jump_id);
        }
    }

    return sig;
}

SignalResult MessageButtonZone::Touch(Terminal *term, int /*tx*/, int /*ty*/)
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
	"starttakeout", "pickup", "quicktogo", "quickdinein", "quickselforder", nullptr};

    Settings *settings = term->GetSettings();
    int idx = CompareListN(command_list, signal_msg);

    switch (idx)
    {
    case 0: // sendandjump
        sig = SendandJump(term);
        break;
    case 1: // starttakeout
    case 2:  // pickup/delivery
    {
        int mode = (idx == 1) ? CHECK_TAKEOUT : CHECK_CALLIN;
        if (term->QuickMode(mode))
            return SIGNAL_IGNORED;
        term->Jump(JUMP_STEALTH, -8);
        break;
    }
    case 3:  // quick to-go
    case 4:  // quick dine-in
    case 5:  // quick self-order
    {
        int mode = (idx == 3) ? CHECK_SELFTAKEOUT : (idx == 4 ? CHECK_SELFDINEIN : CHECK_SELFORDER);
        if (term->QuickMode(mode))
            return SIGNAL_IGNORED;
        term->JumpToIndex(IndexValue[settings->MealPeriod(SystemTime)]);
        break;
    }
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
    char* retval = nullptr;
    int len = static_cast<int>(strlen(source));
    int sidx = 0;
    char dest[STRLONG] = "";
    int didx = static_cast<int>(strlen(dest));
    int badchar = 0;

    // do not allow the command to start with a dot.
    if (source[sidx] == '.')
        return nullptr;

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
        retval = nullptr;
    else
    {
        dest[didx] = '\0';
        vt_safe_string::safe_copy(source, STRLONG, dest);
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
    "payexpenses", "fastfood", "selforder", "lastendday",
    "checkbalanced", "haspayments", "training", "selectedorder", nullptr};
enum comms {
    CHECK, GUESTS, SUBCHECKS, SETTLE, ORDER, DRAWER,
    DRAWERCOUNT, ORDERBYSEAT, DEVELOPER, FLOW, ASSIGNED,
    LOCAL, SUPERVISOR, MANAGER, EDITUSERS, MERCHANDISE,
    MOVETABLE, TABLEPAGES, PASSWORDS, SUPERUSER,
    PAYEXPENSES, FASTFOOD, SELFORDER, LASTENDDAY,
    CHECKBALANCED, HASPAYMENTS, TRAINING, SELECTORDER};

static const genericChar* OperatorWords[] = {
    "=", ">", "<", "!=", ">=", nullptr};
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
std::unique_ptr<Zone> ConditionalZone::Copy()
{
    FnTrace("ConditionalZone::Copy()");
    auto z = std::make_unique<ConditionalZone>();
    (void)z->SetRegion(this);  // SetRegion has nodiscard, but we only need side effect
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
    if (ImagePath() && z->ImagePath())
        z->ImagePath()->Set(*ImagePath());
    for (int i = 0; i < 3; ++i)
    {
        z->color[i]   = color[i];
        z->image[i]   = image[i];
        z->frame[i]  = frame[i];
        z->texture[i] = texture[i];
    }
    return z;
}

int ConditionalZone::RenderInit(Terminal *term, int /*update_flag*/)
{
    FnTrace("ConditionalZone::RenderInit()");
    if (keyword < 0)
    {
        genericChar str1[256] = "", str2[256] = "";

        // FIX - really lame expression parser
        // Use safer scanf with field width limits to prevent buffer overflow
        sscanf(expression.Value(), "%255s %255s %d", str1, str2, &val);
        keyword = CompareList(str1, KeyWords);
        op      = CompareList(str2, OperatorWords);
    }
    active = static_cast<Uchar>(EvalExp(term));
    return 0;
}

SignalResult ConditionalZone::Touch(Terminal *term, int /*tx*/, int /*ty*/)
{
    FnTrace("ConditionalZone::Touch()");
    SignalResult sig = SIGNAL_OKAY;

    const char* to_send = nullptr;
    if (message.size() > 0)
    {
        // broadcast button's message
        to_send = message.Value();
    }
    else if (name.size() > 0)
    {
        // broadcast button's name
        to_send = name.Value();
    }
    if (to_send)
        sig = term->Signal(to_send, group_id);

    if (sig != SIGNAL_ERROR)
        term->Jump(jump_type, jump_id);
    return sig;
}

int ConditionalZone::Update(Terminal *term, int update_message, const genericChar* /*value*/)
{
    FnTrace("ConditionalZone::Update()");
    if (update_message & (UPDATE_CHECKS))
    {
        int eval = EvalExp(term);
        if (eval != active)
        {
            active = static_cast<Uchar>(eval);
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
    case SELFORDER: // selforder
        n = (term->type == ((keyword == FASTFOOD) ? TERMINAL_FASTFOOD : TERMINAL_SELFORDER));
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
    if (max_states == 0)
        max_states = 1;  // Prevent division by zero
    StrString(str, name.Value(), state % max_states);
    RenderZone(term, str, update_flag);
    return RENDER_OKAY;
}

SignalResult ToggleZone::Touch(Terminal *term, int /*tx*/, int /*ty*/)
{
    FnTrace("ToggleZone::Touch()");
    genericChar str[256];
    if (max_states == 0)
        max_states = 1;  // Prevent division by zero
    StrString(str, message.Value(), state % max_states);
    term->Signal(str, group_id);

    ++state;
    if (state >= max_states)
        state = 0;
    Draw(term, 0);
    return SIGNAL_OKAY;
}

const char* ToggleZone::TranslateString(Terminal * /*term*/)
{
    FnTrace("ToggleZone::TranslateString()");
    static genericChar str[256];
    if (max_states == 0)
        max_states = 1;  // Prevent division by zero
    StrString(str, name.Value(), state % max_states);
    return str;
}

/**** CommentZone Class ****/
// Member Functions
int CommentZone::RenderInit(Terminal *term, int /*update_flag*/)
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
    const char* label = nullptr;
    genericChar str[32];
    if (users <= 0)
        label = name.Value();
    else if (users == 1)
        label = "1 Terminal Busy";
    else
    {
        vt_safe_string::safe_format(str, 32, "%d Terminals Busy", users);
        label = str;
    }
    RenderZone(term, label, update_flag);
    return RENDER_OKAY;
}

SignalResult KillSystemZone::Touch(Terminal *term, int /*tx*/, int /*ty*/)
{
    FnTrace("KillSystemZone::Touch()");
    Employee *e = term->user;
    int users = term->OtherTermsInUse();
    if (users > 0 && (e == nullptr || !e->CanEdit()))
        return SIGNAL_IGNORED;

    SimpleDialog *d =
        new SimpleDialog(term->Translate("Confirm Your Choice:"));
    d->Button("Quit ViewTouch and Return To The Desktop", "shutdown");
    d->Button("Refresh ViewTouch", "systemrestart");
    d->Button("Don't Quit or Refresh");
    term->OpenDialog(d);
    return SIGNAL_OKAY;
}

int KillSystemZone::Update(Terminal *term, int update_message, const genericChar* /*value*/)
{
    FnTrace("KillSystemZone::Update()");
    if (update_message & UPDATE_USERS)
    {
        Employee *e = term->user;
        int users = term->OtherTermsInUse();
        if (users > 0 && (e == nullptr || !e->CanEdit()))
            term->KillDialog();
        return Draw(term, 1);
    }
    return 0;
}

/**** ClearSystemZone Class ****/
// Member Functions
ClearSystemZone::ClearSystemZone()
{
    FnTrace("ClearSystemZone::ClearSystemZone()");
    countdown = 10;
}

RenderResult ClearSystemZone::Render(Terminal *term, int update_flag)
{
    FnTrace("ClearSystemZone::Render()");
    genericChar str[64];
    if (countdown > 0)
        snprintf(str, sizeof(str), "Clear System (%d)", countdown);
    else
        snprintf(str, sizeof(str), "Clear System");
    
    RenderZone(term, str, update_flag);
    return RENDER_OKAY;
}

SignalResult ClearSystemZone::Touch(Terminal *term, int /*tx*/, int /*ty*/)
{
    FnTrace("ClearSystemZone::Touch()");
    
    if (countdown > 0)
    {
        countdown--;
        Draw(term, 1);  // Redraw to show new countdown
        
        if (countdown == 0)
        {
            // Show the dialog to choose whether to clear labor data
            SimpleDialog *sd = new SimpleDialog(term->Translate("Also clear labor data?"));
            sd->Button("Yes", "clearsystemall");
            sd->Button("No", "clearsystemsome");
            sd->Button("Cancel", "clearsystemcancel");
            sd->target_zone = this;
            term->OpenDialog(sd);
        }
        return SIGNAL_OKAY;
    }
    
    return SIGNAL_IGNORED;
}

SignalResult ClearSystemZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("ClearSystemZone::Signal()");
    
    const genericChar* commands[] = {"clearsystemall", "clearsystemsome", "clearsystemcancel", nullptr};
    int idx = CompareList(message, commands);
    
    switch (idx)
    {
    case 0:  // clearsystemall - clear everything including labor
        term->system_data->ClearSystem(1);
        return SIGNAL_OKAY;
    case 1:  // clearsystemsome - clear everything except labor
        term->system_data->ClearSystem(0);
        return SIGNAL_OKAY;
    case 2:  // clearsystemcancel - reset countdown
        countdown = 10;
        Draw(term, 1);
        return SIGNAL_OKAY;
    }
    
    return SIGNAL_IGNORED;
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
        "status", "clearstatus", nullptr };
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

/**** ImageButtonZone Class ****/
// Constructor
ImageButtonZone::ImageButtonZone()
{
    FnTrace("ImageButtonZone::ImageButtonZone()");
    image_loaded = 0;
    // Image buttons should not participate in selection system
    // Override ZoneStates to return 1 so selection logic is skipped
    // image_path is now inherited from ButtonZone
}

// Member Functions
std::unique_ptr<Zone> ImageButtonZone::Copy()
{
    FnTrace("ImageButtonZone::Copy()");
    auto z = std::make_unique<ImageButtonZone>();
    (void)z->SetRegion(this);  // SetRegion has nodiscard, but we only need side effect
    z->name.Set(name);
    z->key       = key;
    z->behave    = behave;
    z->font      = font;
    z->shape     = shape;
    z->group_id  = group_id;
    z->jump_type = jump_type;
    z->jump_id   = jump_id;
    if (ImagePath() && z->ImagePath())
        z->ImagePath()->Set(*ImagePath());
    z->image_loaded = image_loaded;
    // image_path is now handled by ButtonZone::Copy()
    for (int i = 0; i < 3; ++i)
    {
        z->color[i]   = color[i];
        z->image[i]   = image[i];
        z->frame[i]  = frame[i];
        z->texture[i] = texture[i];
    }
    return z;
}

int ImageButtonZone::CanSelect(Terminal *t)
{
    FnTrace("ImageButtonZone::CanSelect()");

    // Check permissions - only Editor (id 1 or 2) and Super User (id 1) can select this zone
    Employee *e = t->user;
    if (e == nullptr)
        return 0;

    // Allow Editor (id 2) and Super User (id 1)
    return (e->id == 1 || e->id == 2);
}

int ImageButtonZone::RenderInit(Terminal * /*term*/, int update_flag)
{
    FnTrace("ImageButtonZone::RenderInit()");

    active = 1;

    // Image loading is now handled in Render() method on the X server side
    // This keeps the terminal side lightweight

    return 0;
}

// ImageButtonZone inherits image rendering from ButtonZone
// No need to override Render() anymore

int ImageButtonZone::ZoneStates()
{
    FnTrace("ImageButtonZone::ZoneStates()");
    // Return 1 to indicate only 1 state (normal), so selection logic is skipped
    // This prevents the button from participating in the selection system
    return 1;
}

int ImageButtonZone::State(Terminal *t)
{
    FnTrace("ImageButtonZone::State()");
    // Always return normal state (0) for image buttons to prevent
    // selection colors from tinting the image
    return 0;
}

SignalResult ImageButtonZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("ImageButtonZone::Touch()");

    // Handle the button touch (this might trigger jump actions, etc.)
    // Clear the selection so the button doesn't stay highlighted
    term->ClearSelectedZone();

    // Force a complete screen redraw to ensure image renders correctly
    term->Draw(0);

    // Return ignored since image buttons don't have specific actions yet
    // In the future, this could trigger custom actions based on the image
    return SIGNAL_IGNORED;
}

/**** IndexTabZone Class ****/
// Constructor
IndexTabZone::IndexTabZone()
{
    jump_type = JUMP_NONE;
    jump_id   = 0;
}

std::unique_ptr<Zone> IndexTabZone::Copy()
{
    FnTrace("IndexTabZone::Copy()");
    auto z = std::make_unique<IndexTabZone>();
    z->SetRegion(this);
    z->name.Set(name);
    z->key       = key;
    z->behave    = behave;
    z->font      = font;
    z->shape     = shape;
    z->group_id  = group_id;
    z->jump_type = jump_type;
    z->jump_id   = jump_id;
    if (ImagePath() && z->ImagePath()) {
        z->ImagePath()->Set(*ImagePath());
    }
    for (int i = 0; i < 3; ++i)
    {
        z->color[i]   = color[i];
        z->image[i]   = image[i];
        z->frame[i]  = frame[i];
        z->texture[i] = texture[i];
    }
    return z;
}

int IndexTabZone::CanSelect(Terminal *t)
{
    FnTrace("IndexTabZone::CanSelect()");
    if (page == nullptr)
        return 1;

    // Index Tab buttons can only exist on Index pages or be inherited by Menu Item pages
    // If we're on an Index page, allow selection
    if (page->type == PAGE_INDEX || page->type == PAGE_INDEX_WITH_TABS)
        return ButtonZone::CanSelect(t);
    
    // If we're on a Menu Item page, check if this zone is from the parent Index page
    if (page->type == PAGE_ITEM || page->type == PAGE_ITEM2)
    {
        // Check if this zone belongs to the parent Index page
        if (page->parent_page && page->parent_page->type == PAGE_INDEX)
        {
            // This is an inherited Index Tab from parent Index page
            return ButtonZone::CanSelect(t);
        }
    }
    
    return 0; // Not allowed on other page types
}

int IndexTabZone::CanEdit(Terminal *t)
{
    FnTrace("IndexTabZone::CanEdit()");
    if (page == nullptr)
        return 1;

    Employee *e = t->user;
    if (e == nullptr)
        return 0;

    // Index Tab buttons can only be edited when on an Index page
    // They cannot be edited when inherited on Menu Item pages
    if (page->type == PAGE_INDEX || page->type == PAGE_INDEX_WITH_TABS)
    {
        if (page->id < 0 && !e->CanEditSystem())
            return 0;
        return e->CanEdit();
    }
    
    // Cannot edit Index Tab buttons when they're inherited on Menu Item pages
    return 0;
}

// LanguageButtonZone implementation

LanguageButtonZone::LanguageButtonZone()
{
    FnTrace("LanguageButtonZone::LanguageButtonZone()");
}

std::unique_ptr<Zone> LanguageButtonZone::Copy()
{
    FnTrace("LanguageButtonZone::Copy()");

    auto z = std::make_unique<LanguageButtonZone>();
    (void)z->SetRegion(this);  // SetRegion has nodiscard, but we only need side effect
    z->name.Set(name);
    z->key       = key;
    z->behave    = behave;
    z->font      = font;
    z->shape     = shape;
    z->jump_type = jump_type;
    z->jump_id   = jump_id;
    return z;
}

RenderResult LanguageButtonZone::Render(Terminal *term, int update_flag)
{
    FnTrace("LanguageButtonZone::Render()");

    // Render the button background first
    RenderResult result = ButtonZone::Render(term, update_flag);

    // Add "English" text to the button
    int text_color = COLOR_DEFAULT;
    if (term->selected_zone == this)
        text_color = COLOR_WHITE;  // Use white for selected state

    term->RenderText(term->Translate("Language"), x + 5, y + 2, text_color, FONT_DEFAULT, ALIGN_LEFT);

    return result;
}

SignalResult LanguageButtonZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("LanguageButtonZone::Touch()");

    // For now, show a dialog indicating that only English is supported
    // In the future, this could show a language selection menu

    SimpleDialog *d = new SimpleDialog(term->Translate("Current Language: English\\Language switching is not currently available.\\Only English is supported."));
    d->Button(term->Translate("Okay"));
    term->OpenDialog(d);

    return SIGNAL_OKAY;
}

