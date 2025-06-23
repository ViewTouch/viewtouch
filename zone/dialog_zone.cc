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
 * dialog_zone.cc - revision 55 (10/13/98)
 * Implementation of dialog_zone module
 */

#include "dialog_zone.hh"
#include "terminal.hh"
#include "settings.hh"
#include "employee.hh"
#include "inventory.hh"
#include "system.hh"
#include "image_data.hh"
#include <assert.h>
#include <cstring>
#include <cctype>
#include <unistd.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/*********************************************************************
 * Definitions
 ********************************************************************/

#define CCD_WIDTH 640
#define CCD_HEIGHT 420


/*********************************************************************
 * Functions
 ********************************************************************/

DialogZone *NewPrintDialog(int no_report)
{
    FnTrace("NewPrintDialog()");
    SimpleDialog *d;
    if (no_report)
    {
        d = new SimpleDialog("Confirm:");
        d->Button("Print", "localprint");
        d->Button("Cancel");
    }
    else
    {
        d = new SimpleDialog("Select A Printer:");
        d->Button("Receipt Printer", "localprint");
        d->Button("Full Page Printer", "reportprint");
        d->Button("Cancel");
    }
    return d;
}


/*********************************************************************
 * MessageDialog Class
 ********************************************************************/

MessageDialog::MessageDialog(const char* text)
{
    FnTrace("Messagedialog::MessageDialog()");
    name.Set(text);
    color[0]   = COLOR_BLACK;
    frame[0]   = ZF_RAISED;
    texture[0] = IMAGE_LITE_WOOD;
    font       = FONT_TIMES_24B; // Changed from FONT_TIMES_34 - temporary fix for oversized dialog text
    shadow     = 16;
    h          = 360;
    w          = 600;
}


/*********************************************************************
 * ButtonObj Class
 ********************************************************************/

ButtonObj::ButtonObj(const char* text, const genericChar* msg)
{
    FnTrace("ButtonObj::ButtonObj()");
    color = COLOR_BLACK;
    label.Set(text);
    if (msg)
        message.Set(msg);
    else
        message.Set(text);
    font = FONT_TIMES_24B;
}

int ButtonObj::Render(Terminal *term)
{
    FnTrace("ButtonObj::Render()");

    if (selected)
        term->RenderButton(x, y, w, h, ZF_RAISED, IMAGE_LIT_SAND);
    else
        term->RenderButton(x, y, w, h, ZF_RAISED, IMAGE_SAND);
    term->RenderZoneText(term->Translate(label.Value()), x + 3, y + 3, w - 6, h - 6,
                      color, font);
    return 0;
}


/*********************************************************************
 * DialogZone Class
 ********************************************************************/
DialogZone::DialogZone()
{
    target_zone = NULL;
    target_index = 0;
    target_signal[0] = '\0';
    color[0]   = COLOR_BLACK;
    frame[0]   = ZF_RAISED;
    texture[0] = IMAGE_LITE_WOOD;
    font       = FONT_TIMES_24B; // Changed from FONT_TIMES_34 - temporary fix for oversized dialog text
    shadow     = 16;
}

RenderResult DialogZone::Render(Terminal *term, int update_flag)
{
    FnTrace("DialogZone::Render()");

    RenderZone(term, name.Value(), update_flag);
    buttons.Render(term);

    return RENDER_OKAY;
}

SignalResult DialogZone::Touch(Terminal *term, int tx, int ty)
{
    return SIGNAL_IGNORED;
}

SignalResult DialogZone::Mouse(Terminal *term, int action, int mx, int my)
{
    FnTrace("DialogZone::Mouse()");
    SignalResult retval = SIGNAL_IGNORED;

    if (action & MOUSE_PRESS)
        retval = Touch(term, mx, my);

    return retval;
}

ButtonObj *DialogZone::Button(const char* text, const genericChar* message)
{
    FnTrace("DialogZone::Button()");

    ButtonObj *b = new ButtonObj(text, message);
    buttons.Add(b);

    return b;
}

/****
 * ClosingAction:  Allows us to specify the dialog's
 *  final behavior (e.g. cancel jumps to this page, successful
 *  entry runs a preauth, etc.).
 ****/
int DialogZone::ClosingAction(int action_type, int action, int arg)
{
    FnTrace("DialogZone::ClosingAction()");
    int retval = 0;

    switch (action_type)
    {
    case ACTION_SUCCESS:
        success_action.type = action;
        success_action.arg = arg;
        break;
    case ACTION_CANCEL:
        cancel_action.type = action;
        cancel_action.arg = arg;
        break;
    }

    return retval;
}

int DialogZone::ClosingAction(int action_type, int action, const char* message)
{
    FnTrace("DialogZone::ClosingAction()");
    int retval = 0;

    switch (action_type)
    {
    case ACTION_SUCCESS:
        success_action.type = action;
        strncpy(success_action.msg, message, STRLENGTH);
        break;
    case ACTION_CANCEL:
        cancel_action.type = action;
        strncpy(cancel_action.msg, message, STRLENGTH);
        break;
    }

    return retval;
}

int DialogZone::SetAllActions(DialogZone *dest)
{
    FnTrace("DialogZone::SetAllActions()");
    int retval = 0;

    if (success_action.type != ACTION_DEFAULT)
    {
        if (success_action.msg[0] != '\0')
            dest->ClosingAction(ACTION_SUCCESS, success_action.type, success_action.msg);
        else
            dest->ClosingAction(ACTION_SUCCESS, success_action.type, success_action.arg);
        retval = 1;
    }

    if (cancel_action.type != ACTION_DEFAULT)
    {
        if (cancel_action.msg[0] != '\0')
            dest->ClosingAction(ACTION_CANCEL, cancel_action.type, cancel_action.msg);
        else
            dest->ClosingAction(ACTION_CANCEL, cancel_action.type, cancel_action.arg);
        retval = 1;
    }

    return retval;
}

/****
 * PrepareForClose:  This sets the actual closing actions.  Until now,
 * they've been held in the (success|cancel)_action variables.  Now,
 * we set them where Terminal will be able to act on them.
 ****/
int DialogZone::PrepareForClose(int action_type)
{
    int retval = 0;
    DialogAction *action;

    if (action_type == ACTION_SUCCESS)
        action = &success_action;
    else
        action = &cancel_action;

    if (action->type == ACTION_SIGNAL)
        strcpy(target_signal, action->msg);
    else if (action->type == ACTION_JUMPINDEX)
        target_index = action->arg;

    return retval;
}


/*********************************************************************
 * SimpleDialog Class
 ********************************************************************/
SimpleDialog::SimpleDialog()
{
    FnTrace("SimpleDialog::SimpleDialog()");
}

SimpleDialog::SimpleDialog(const char* title, int form)
{
    FnTrace("SimpleDialog::SimpleDialog(const char* , int)");
    // format = 0 is horizontal layout of buttons
    // format = 1 is 2 columns and as many rows as necessary
    // format = 2 is as many rows and columns as necessary, with paging
    //   if there are too many buttons
    format = form;
    name.Set(title);
    gap = 8;
    zofont = FONT_TIMES_24B;
    // the following variables are only used for format > 0
    head_height = 200;
    btn_height  = 92;  // not used for format == 2
    force_width = 0;
}

int SimpleDialog::RenderInit(Terminal *term, int update_flag)
{
    FnTrace("SimpleDialog::RenderInit()");
    if (format == 0)
    {
        h = 400;
        if (force_width > 0)
            w = force_width;
        else
            w = 400 + (buttons.Count() * 80);
    }
    else
    {
        int bcount = buttons.Count();

        // set up sizes based on the number of buttons
        if ( bcount < 3 )
            zofont = FONT_TIMES_24B; // Changed from FONT_TIMES_34B - temporary fix for oversized dialog text
        else if ( bcount < 7 )
            zofont = FONT_TIMES_24B;
        else if ( bcount < 11 )
        {
            zofont = FONT_TIMES_20B;
            head_height = 100;
            btn_height = 72;
        }
        else
        {
            zofont = FONT_TIMES_18B;
            head_height = 50;
            btn_height = 50;
        }
        if (format == 1)
            h = head_height + ((buttons.Count() + 1) / 2) * (btn_height + gap);
        else
        {
            int rows = ((buttons.Count() + 2) / 3);
            h = head_height + (rows * (btn_height + gap));
        }
        w = 640;
    }

    return 0;
}

RenderResult SimpleDialog::Render(Terminal *term, int update_flag)
{
    FnTrace("SimpleDialog::Render()");
    RenderZone(term, NULL, update_flag);
    int bx = 0;
    int by = 0;
    int bw = 0;
    int z  = 0;

    // Layout Buttons & Render
    if (format == 0)
    {
        int hh = ((h * 3) / 5);
        term->RenderZoneText(term->Translate(name.Value()), x + border + 10, y + border,
                          w - (border * 2) - 20, hh - border, color[0], font);

        buttons.LayoutColumns(term, x + border, y + hh,
                              w - (border * 2), h - hh - border, gap);
    }
    else if (format == 1)
    {
        by = y + head_height;
        bw = (w - (border * 2) - gap) / 2;
        z = 1;

        term->RenderZoneText(name.Value(), x + border, y + border,
                          w - (border * 2), head_height, color[0], font);
        ZoneObject *zo = buttons.List();
        while (zo)
        {
            if ((z % 2) == 0)
                bx += bw + gap;
            else
                bx = x + border;
            // last button, if odd number of buttons, is width of dialog
            // minus border width * 2
            if (zo->next == NULL && (buttons.Count() % 2))
                bw = w - (border * 2);
            zo->font = zofont;
            zo->Layout(term, bx, by, bw, btn_height);
            if (((z % 2) == 0) && zo->next != NULL)
                by += btn_height + gap;
            z += 1;
            zo = zo->next;
        }
    }
    else
    {
        by = y + head_height;
        bw = (w - (border * 2) - (gap * 2)) / 3;
        z = 1;
        term->RenderZoneText(name.Value(), x + border, y + border,
                          w - (border * 2), head_height, color[0], font);
        ZoneObject *zo = buttons.List();
        while (zo)
        {
            if (z == 1)
                bx = x + border;
            else
                bx += bw + gap;
            if (zo->next == NULL)
            {
                if (z == 1)
                    bw = w - (border * 2);
                else if (z == 2)
                    bw = (bw * 2) + gap;
            }
            zo->font = zofont;
            zo->Layout(term, bx, by, bw, btn_height);
            if (z == 3 && zo->next != NULL)
            {
                by += btn_height + gap;
                z = 1;
            }
            else
                z += 1;
            zo = zo->next;
        }
    }

    buttons.Render(term);
    return RENDER_OKAY;
}

SignalResult SimpleDialog::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("SimpleDialog::Touch()");
    ZoneObject *zo = buttons.List();
    if (zo == NULL || ty < zo->y)
        return SIGNAL_TERMINATE;

    zo = buttons.Find(tx, ty);
    if (zo)
    {
        zo->Draw(term, 1);
        const genericChar* msg = ((ButtonObj *) zo)->message.Value();
        if (target_zone)
            target_zone->Signal(term, msg);
        else
            term->Signal(msg, group_id);
        return SIGNAL_TERMINATE;
    }
    return SIGNAL_IGNORED;
}


/*********************************************************************
 * UnitAmountDialog Class
 ********************************************************************/

UnitAmountDialog::UnitAmountDialog(const char* title, UnitAmount &u)
{
    FnTrace("UnitAmountDialog::UnitAmountDialog()");
    int i;

    lit = NULL;
    name.Set(title);
    if (u.amount != 0.0)
        snprintf(buffer, STRLENGTH, "%g", u.amount);
    unit_type = u.type;

    genericChar str[256];

    for (i = 0; i < 10; ++i)
    {
        snprintf(str, STRLENGTH, "%d", i);
        key[i] = Button(str, str);
    }

    key[10] = Button(".", ".");
    key[11] = Button("Enter", "enter");
    key[12] = Button("Back Space", "backspace");
    key[13] = Button("Cancel", "cancel");

    // Add unit keys here
    int ul[6] = {-1, -1, -1, -1, -1, -1};
    switch (u.type)
    {
    case UNIT_NONE:
        break;
    default:
    case COUNT_SINGLE:
    case COUNT_DOZEN:
    case COUNT_GROSS:
        ul[0] = COUNT_SINGLE; ul[1] = COUNT_DOZEN; ul[2] = COUNT_GROSS; break;
    case WEIGHT_G:
    case WEIGHT_KG:
        ul[0] = WEIGHT_G; ul[1] = WEIGHT_KG; break;
    case WEIGHT_DASH:
    case WEIGHT_OUNCE:
    case WEIGHT_POUND:
        ul[0] = WEIGHT_OUNCE; ul[1] = WEIGHT_POUND; break;
    case VOLUME_ML:
    case VOLUME_L:
        ul[0] = VOLUME_ML; ul[1] = VOLUME_L; break;
    case VOLUME_TSP:
    case VOLUME_TBS:
    case VOLUME_OUNCE:
    case VOLUME_QUART:
    case VOLUME_GALLON:
    case VOLUME_DRAM:
    case VOLUME_CUP:
    case VOLUME_PINT:
        ul[0] = VOLUME_OUNCE; ul[1] = VOLUME_PINT; ul[2] = VOLUME_QUART;
        ul[3] = VOLUME_GALLON; break;
    }

    units = 0;
    while (ul[units] >= 0)
    {
        UnitAmount tmp;
        tmp.type = ul[units];
        unit[units] = Button(tmp.Measurement(), NULL);
        ut[units] = tmp.type;
        ++units;
    }

    w = 480;
    h = 580;
}

RenderResult UnitAmountDialog::Render(Terminal *term, int update_flag)
{
    FnTrace("UnitAmountDialog::Render()");
    int i;

    if (update_flag)
        lit = NULL;
    if (lit)
    {
        lit->Draw(term, 0);
        lit = NULL;
        return RENDER_OKAY;
    }

    LayoutZone::Render(term, update_flag);

    // Layout buttons
    int gap = 8;
    int bw = (w - (border * 2) - (gap * 4)) / 5;
    int bh = (h - (border * 2) - (gap * 4) - 100) / 5;

    int col[5];
    int row[5];
    col[0] = x + border;
    row[0] = y + border + 100;
    for (i = 1; i < 5; ++i)
    {
        col[i] = col[i-1] + bw + gap;
        row[i] = row[i-1] + bh + gap;
    }

    key[0]->SetRegion(col[1], row[3], bw*2 + gap, bh);
    key[1]->SetRegion(col[1], row[0], bw, bh);
    key[2]->SetRegion(col[2], row[0], bw, bh);
    key[3]->SetRegion(col[3], row[0], bw, bh);
    key[4]->SetRegion(col[1], row[1], bw, bh);
    key[5]->SetRegion(col[2], row[1], bw, bh);
    key[6]->SetRegion(col[3], row[1], bw, bh);
    key[7]->SetRegion(col[1], row[2], bw, bh);
    key[8]->SetRegion(col[2], row[2], bw, bh);
    key[9]->SetRegion(col[3], row[2], bw, bh);

    key[10]->SetRegion(col[3], row[3], bw, bh);
    key[11]->SetRegion(col[4], row[2], bw, bh*2 + gap);
    key[12]->SetRegion(col[4], row[0], bw, bh*2 + gap);
    key[13]->SetRegion(col[1], row[4], bw*3 + gap*2, bh);

    if (units > 0)
    {
        Flt u  = (Flt) units;
        Flt hh = h - (border * 2) - 100 - bh;
        Flt yoffset = hh / u;
        bh = (int) ((hh - (gap * u)) / u);
        for (i = 0; i < units; ++i)
        {
            unit[i]->SetRegion(col[0], y + border + 100 +
                               (int) (yoffset * (Flt) i), bw, bh);
        }
    }

    // Render
    TextC(term, 0, name.Value());
    RenderEntry(term);

    for (i = 0; i < 14; ++i)
    {
        key[i]->selected = 0;
        key[i]->Render(term);
    }
    for (i = 0; i < units; ++i)
    {
        unit[i]->selected = (ut[i] == unit_type);
        unit[i]->Render(term);
    }
    return RENDER_OKAY;
}

SignalResult UnitAmountDialog::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("UnitAmountDialog::Touch()");
    int i;

    for (i = 0; i < 14; ++i)
    {
        ButtonObj *b = key[i];
        if (b->IsPointIn(tx, ty))
        {
            if (lit)
                lit->Draw(term, 0);
            lit = b;
            b->Draw(term, 1);
            term->RedrawZone(this, 500);
            return Signal(term, b->message.Value());
        }
    }

    for (i = 0; i < units; ++i)
    {
        ButtonObj *b = unit[i];
        if (b->IsPointIn(tx, ty))
        {
            if (lit)
                lit->Draw(term, 0);
            lit = NULL;
            unit_type = ut[i];
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
    }

    return SIGNAL_IGNORED;
}

SignalResult UnitAmountDialog::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("UnitAmountDialog::Signal()");
    static const genericChar* command[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".",
            "enter", "backspace", "cancel", NULL};

    int idx = CompareList(message, command);
    if (idx < 0)
        return SIGNAL_IGNORED;

    int len = strlen(buffer);
    switch (idx)
    {
    default:
        if (len < 10 && (len > 0 || idx != 0))
            strncat(buffer, message, sizeof(buffer) - strlen(buffer) - 1);
        break;
    case 10:  // .
    {
        int dp = 0;
        const genericChar* c = buffer;
        while (*c)
            if (*c++ == '.')
                dp = 1;
        if (dp == 0)
            strncat(buffer, ".", sizeof(buffer) - strlen(buffer) - 1);
        break;
    }
    case 11:  // enter
    {
        genericChar str[256];
        snprintf(str, STRLENGTH, "amount %d %s", unit_type, buffer);
        if (target_zone)
            target_zone->Signal(term, str);
        else
            term->Signal(str, group_id);
        return SIGNAL_TERMINATE;
    }
    case 12:  // backspace
        if (len > 0)
            buffer[len - 1] = '\0';
        break;
    case 13:  // cancel
        return SIGNAL_TERMINATE;
    }

    RenderEntry(term);
    term->UpdateArea(x, y + font_height, w, font_height * 2);
    return SIGNAL_OKAY;
}

SignalResult UnitAmountDialog::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("UnitAmountDialog::Keyboard()");
    switch (my_key)
    {
    case 27:  // ESC
        return Signal(term, "cancel");
    case 13:  // return
        return Signal(term, "enter");
    case 8:   // backspace
        return Signal(term, "backspace");
    }

    genericChar str[] = {(char) my_key, '\0'};
    return Signal(term, str);
}

int UnitAmountDialog::RenderEntry(Terminal *term)
{
    FnTrace("UnitAmountDialog::RenderEntry()");
    Entry(term, (size_x/2) - 10, 1.5, 20);

    UnitAmount ua;
    ua.type = unit_type;
    genericChar str[256];
    if (buffer[0] == '\0')
        snprintf(str, STRLENGTH, "0 %s", ua.Measurement());
    else
        snprintf(str, STRLENGTH, "%s %s", buffer, ua.Measurement());
    TextC(term, 1.5, str, COLOR_WHITE);
    return 0;
}


/*********************************************************************
 * TenKeyDialog Class
 ********************************************************************/

TenKeyDialog::TenKeyDialog()
{
    FnTrace("TenKeyDialog::TenKeyDialog()");
    int i;

    lit = NULL;
    name.Set("Enter Amount");
    buffer = 0;
    max_amount = 100000;

    genericChar str[256];
    for (i = 0; i < 10; ++i)
    {
        snprintf(str, STRLENGTH, "%d", i);
        key[i] = Button(str, str);
    }

    key[10] = Button("Enter", "enter");
    key[11] = Button("Back Space", "backspace");
    key[12] = Button("Cancel", "cancel");
    key[13] = NULL;

    w = 420;
    h = 580;
    first_row = 100;
    strncpy(return_message, "amount", STRLENGTH);
}

TenKeyDialog::TenKeyDialog(const char* title, int amount, int cancel, int dp)
{
    FnTrace("TenKeyDialog::TenKeyDialog(const char* , int, int, int)");
    int i;

    lit = NULL;
    name.Set(title);
    buffer = amount;
    max_amount = 100000;

    genericChar str[256];
    for (i = 0; i < 10; ++i)
    {
        snprintf(str, STRLENGTH, "%d", i);
        key[i] = Button(str, str);
    }

    key[10] = Button("Enter", "enter");
    key[11] = Button("Back Space", "backspace");
    if (cancel)
        key[12] = Button("Cancel", "cancel");
    else
        key[12] = NULL;

    // the decimal will be automatic, so we won't show the button.
    decimal = dp;
    key[13] = NULL;

    w = 420;
    h = 580;
    first_row = 100;
    strncpy(return_message, "amount", STRLENGTH);
}

TenKeyDialog::TenKeyDialog(const char* title, const char* retmsg, int amount, int dp)
{
    FnTrace("TenKeyDialog::TenKeyDialog(const char* , const char* , int, int)");
    int i;

    lit = NULL;
    name.Set(title);
    buffer = amount;
    max_amount = 100000;

    genericChar str[256];
    for (i = 0; i < 10; ++i)
    {
        snprintf(str, STRLENGTH, "%d", i);
        key[i] = Button(str, str);
    }

    key[10] = Button("Enter", "enter");
    key[11] = Button("Back Space", "backspace");
    key[12] = Button("Cancel", "cancel");

    // the decimal will be automatic, so we won't show the button.
    decimal = dp;
    key[13] = NULL;

    w = 420;
    h = 580;
    first_row = 100;
    strncpy(return_message, retmsg, STRLENGTH);
}

RenderResult TenKeyDialog::Render(Terminal *term, int update_flag)
{
    FnTrace("TenKeyDialog::Render()");
    int i;

    if (update_flag)
        lit = NULL;
    if (lit)
    {
        lit->Draw(term, 0);
        lit = NULL;
        return RENDER_OKAY;
    }

    LayoutZone::Render(term, update_flag);
    first_row_y = y + first_row;

    // Layout Buttons
    int gap = 8;
    int bw = (w - (border * 2) - (gap * 3)) / 4;
    int bh;
    if (key[12])
        bh = (h - (border * 2) - (gap * 4) - first_row) / 5;
    else
        bh = (h - (border * 2) - (gap * 3) - first_row) / 4;

    int col[4];
    int row[5];
    col[0] = x + border;
    row[0] = y + border + first_row;
    for (i = 1; i < 5; ++i)
        row[i] = row[i-1] + bh + gap;

    for (i = 1; i < 4; ++i)
        col[i] = col[i-1] + bw + gap;

    key[0]->SetRegion(col[0], row[3], bw*2 + gap, bh);
    key[1]->SetRegion(col[0], row[0], bw, bh);
    key[2]->SetRegion(col[1], row[0], bw, bh);
    key[3]->SetRegion(col[2], row[0], bw, bh);
    key[4]->SetRegion(col[0], row[1], bw, bh);
    key[5]->SetRegion(col[1], row[1], bw, bh);
    key[6]->SetRegion(col[2], row[1], bw, bh);
    key[7]->SetRegion(col[0], row[2], bw, bh);
    key[8]->SetRegion(col[1], row[2], bw, bh);
    key[9]->SetRegion(col[2], row[2], bw, bh);

    key[10]->SetRegion(col[3], row[2], bw, bh*2 + gap);
    key[11]->SetRegion(col[3], row[0], bw, bh*2 + gap);
    if (key[12])
        key[12]->SetRegion(col[1], row[4], bw*2 + gap, bh);
    if (key[13])
        key[13]->SetRegion(col[2], row[3], bw, bh);

    // Render
    if (name.size() > 0)
        TextC(term, 0, name.Value());
    RenderEntry(term);
    buttons.Render(term);

    return RENDER_OKAY;
}

SignalResult TenKeyDialog::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("TenKeyDialog::Touch()");
    ZoneObject *zo = buttons.Find(tx, ty);
    if (zo)
    {
        if (lit)
            lit->Draw(term, 0);
        ButtonObj *b = (ButtonObj *) zo;
        lit = b;
        b->Draw(term, 1);
        term->RedrawZone(this, 500);
        return Signal(term, b->message.Value());
    }
    return SIGNAL_IGNORED;
}

SignalResult TenKeyDialog::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("TenKeyDialog::Signal()");
    static const genericChar* command[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
            "enter", "backspace", "cancel", NULL};

    int idx = CompareList(message, command);
    if (idx < 0)
        return SIGNAL_IGNORED;

    genericChar str[256];
    switch (idx)
    {
    case 10:  // enter
        snprintf(str, STRLENGTH, "%s %d", return_message, buffer);
        if (target_zone)
            target_zone->Signal(term, str);
        else
            term->Signal(str, group_id);
        return SIGNAL_TERMINATE;
    case 11:  // backspace
        buffer /= 10;
        break;
    case 12:  // cancel
        return SIGNAL_TERMINATE;
    default:
        if (buffer < max_amount)
            buffer = buffer * 10 + idx;
        break;
    }

    RenderEntry(term);
    term->UpdateArea(x, y + font_height, w, font_height * 2);
    return SIGNAL_OKAY;
}

SignalResult TenKeyDialog::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("TenKeyDialog::Keyboard()");
    switch (my_key)
    {
    case 27:  // ESC
        return Signal(term, "cancel");
    case 13:  // return
        return Signal(term, "enter");
    case 8:   // backspace
        return Signal(term, "backspace");
    }

    genericChar str[] = {(char) my_key, '\0'};
    return Signal(term, str);
}

int TenKeyDialog::RenderEntry(Terminal *term)
{
    FnTrace("TenKeyDialog::RenderEntry()");

    Entry(term, (size_x/2) - 10, 1.5, 20);
    genericChar str[16];
    if (decimal)
    {
        Flt amount = 0;
        if (buffer > 0)
            amount = (Flt)buffer / 100;
        snprintf(str, sizeof(str), "%.2f", amount);
    }
    else if (buffer == 0)
    {
        str[0] = '\0';
    }
    else
    {
        snprintf(str, sizeof(str), "%d", buffer);
    }
    TextC(term, 1.5, str, COLOR_WHITE);
    return 0;
}


/*********************************************************************
 * GetTextDialog Class
 ********************************************************************/

GetTextDialog::GetTextDialog()
{
    FnTrace("GetTextDialog::GetTextDialog()");
    int i;

    lit = NULL;
    w   = 950;
    h   = 680;
    hh  = 90;
    buffer[0] = '\0';
    display_string[0] = '\0';
    strcpy(return_message, "gettext");
    buffidx = 0;
    max_len = 20;
    first_row = 200;

    genericChar str[2];
    str[1] = '\0';
    genericChar keys[] = "1234567890QWERTYUIOPASDFGHJKLZXCVBNM";
    for (i = 0; i < (int)(sizeof(keys)-1); ++i)
    {
        str[0] = keys[i];
        key[i] = Button(str, str);
        key[i]->color = COLOR_DK_BLUE;
    }

    cancelkey = Button("Cancel", "cancel");
    clearkey  = Button("Clear", "clear");
    spacekey  = Button("Space", " ");
    spacekey->color = COLOR_DK_BLUE;
    bskey     = Button("Back Space", "backspace");
    enterkey  = Button("Enter", "enter");
}

GetTextDialog::GetTextDialog(const char* msg, const char* retmsg, int mlen)
{
    FnTrace("GetTextDialog::GetTextDialog(const char* )");
    int i;

    lit = NULL;
    w   = 950;
    h   = 680;
    hh  = 90;
    buffer[0] = '\0';
    strcpy(display_string, msg);
    strcpy(return_message, retmsg);
    buffidx = 0;
    max_len = mlen > STRLENGTH ? STRLENGTH : mlen;
    first_row = 200;

    genericChar str[2];
    str[1] = '\0';
    genericChar keys[] = "1234567890QWERTYUIOPASDFGHJKLZXCVBNM";
    for (i = 0; i < (int)(sizeof(keys)-1); ++i)
    {
        str[0] = keys[i];
        key[i] = Button(str, str);
        key[i]->color = COLOR_DK_BLUE;
    }

    cancelkey = Button("Cancel", "cancel");
    clearkey  = Button("Clear", "clear");
    spacekey  = Button("Space", " ");
    spacekey->color = COLOR_DK_BLUE;
    bskey     = Button("Back Space", "backspace");
    enterkey  = Button("Enter", "enter");
}

RenderResult GetTextDialog::Render(Terminal *term, int update_flag)
{
    FnTrace("GetTextDialog::Render()");
    RenderResult retval = RENDER_OKAY;

    if (update_flag)
        lit = NULL;
    if (lit)
    {
        lit->Draw(term, 0);
        lit = NULL;
        return RENDER_OKAY;
    }

    LayoutZone::Render(term, update_flag);
    first_row_y = y + first_row;

    // Layout buttons
    int kx = 0;
    int kw = 0;
    int ww = w - (border * 2);
    int ky = y + h - (hh * 5) - border - 4;
    int i;
    int col = color[0];

    for (i = 0; i < 10; ++i)
    {
        kx = (ww * i * 2) / 21;
        kw = ((ww * (i * 2 + 2)) / 21) - kx;
        key[i]->SetRegion(x + border + kx, ky, kw, hh);
    }

    ky += hh;
    for (i = 10; i < 20; ++i)
    {
        kx = (ww * (i * 2 - 19)) / 21;
        kw = ((ww * (i * 2 - 17)) / 21) - kx;
        key[i]->SetRegion(x + border + kx, ky, kw, hh);
    }

    ky += hh;
    for (i = 20; i < 29; ++i)
    {
        kx = (ww * (i * 2 - 38)) / 21;
        kw = ((ww * (i * 2 - 36)) / 21) - kx;
        key[i]->SetRegion(x + border + kx, ky, kw, hh);
    }

    ky += hh;
    for (i = 29; i < 36; ++i)
    {
        kx = (ww * (i * 2 - 55)) / 21;
        kw = ((ww * (i * 2 - 53)) / 21) - kx;
        key[i]->SetRegion(x + border + kx, ky, kw, hh);
    }

    // cancel
    kx = (ww * 34) / 40;
    kw = ww - kx;
    cancelkey->SetRegion(x + border + kx, y + border, kw, hh);

    // clear
    ky += hh + 4;
    kw = ((ww * 6) / 40);
    clearkey->SetRegion(x + border, ky, kw, hh);

    // space
    kx = (ww * 9) / 40;
    kw = ((ww * 24) / 40) - kx;
    spacekey->SetRegion(x + border + kx, ky, kw, hh);

    // backspace
    kx = (ww * 27) / 40;
    kw = ((ww * 33) / 40) - kx;
    bskey->SetRegion(x + border + kx, ky, kw, hh);

    // enter
    kx = (ww * 34) / 40;
    kw = ww - kx;
    enterkey->SetRegion(x + border + kx, ky - hh, kw, hh * 2);

    if (display_string[0] != '\0')
        TextC(term, 1, term->Translate(display_string), col);
    RenderEntry(term);
    buttons.Render(term);

    return retval;
}

SignalResult GetTextDialog::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("GetTextDialog::Signal()");
    SignalResult retval = SIGNAL_OKAY;
    static const genericChar* commands[] = {
        "backspace", "clear", "enter", "cancel", NULL};
    int idx = CompareList(message, commands);
    int error = 0;
    genericChar msgbuf[STRLENGTH];

    switch (idx)
    {
    case 0:  // backspace
        error = Backspace(term);
        break;
    case 1:  // clear
        buffer[0] = '\0';
        buffidx = 0;
        DrawEntry(term);
        break;
    case 2:  // enter
        snprintf(msgbuf, STRLENGTH, "%s %s", return_message, buffer);
        term->Signal(msgbuf, group_id);
        term->Draw(1);
        retval = SIGNAL_TERMINATE;
        break;
    case 3:  // cancel
        term->Signal("gettextcancel", group_id);
        term->Draw(1);
        retval = SIGNAL_TERMINATE;
        break;
    default:
        if (message[1] == '\0')
            error = AddChar(term, message[0]);
        break;
    }

    if (error)
        retval = SIGNAL_IGNORED;

    return retval;
}

SignalResult GetTextDialog::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("GetTextDialog::Touch()");
    SignalResult retval = SIGNAL_IGNORED;
    ZoneObject *zo = buttons.Find(tx, ty);

    if (zo)
    {
        if (lit)
            lit->Draw(term, 0);
        ButtonObj *b = (ButtonObj *) zo;
        lit = b;
        b->Draw(term, 1);
        term->RedrawZone(this, 100);
        retval = Signal(term, b->message.Value());
    }

    return retval;
}

SignalResult GetTextDialog::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("GetTextDialog::Keyboard()");
    SignalResult retval = SIGNAL_IGNORED;

    switch (my_key)
    {
    case 8:  // Backspace
        retval = Signal(term, "backspace");
        break;
    case 13: // Enter
        retval = Signal(term, "enter");
        break;
    case 27: // Esc
        retval = Signal(term, "cancel");
        break;
    default:
        genericChar str[] = {(char) my_key, '\0'};
        retval = Signal(term, str);
        break;
    }

    return retval;
}

int GetTextDialog::RenderEntry(Terminal *term)
{
    FnTrace("GetTextDialog::RenderEntry()");

    Entry(term, (size_x/2) - 15, 2.5, 30);
    TextC(term, 2.5, buffer, COLOR_WHITE);

    return 0;
}

int GetTextDialog::DrawEntry(Terminal *term)
{
    FnTrace("GetTextDialog::DrawEntry()");

    RenderEntry(term);
    term->UpdateArea(x, y + font_height * 2, w, font_height * 2);

    return 0;
}

int GetTextDialog::AddChar(Terminal *term, genericChar val)
{
    FnTrace("GetTextDialog::AddChar()");
    static const genericChar* okay = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    if (buffidx >= max_len)
        return 1;

    genericChar  v = toupper(val);
    const genericChar* c = okay;
    while (*c)
    {
        if (*c == v)
        {
            buffer[buffidx]   = *c;
            buffidx += 1;
            buffer[buffidx] = '\0';
            DrawEntry(term);
            return 0;
        }
        ++c;
    }

    return 1;
}

int GetTextDialog::Backspace(Terminal *term)
{
    FnTrace("GetTextDialog::Backspace()");

    if (buffidx <= 0)
        return 1;

    buffidx -= 1;
    buffer[buffidx] = '\0';
    DrawEntry(term);

    return 0;
}


/*********************************************************************
 * PasswordDialog Class
 ********************************************************************/

PasswordDialog::PasswordDialog(const char* pw)
{
    FnTrace("PasswordDialog::PasswordDialog(const char* )");

    strcpy(password, pw);
    force_change = 0;
    stage = 0;
    new_password[0] = '\0';
    buffer[0] = '\0';
    min_len = 0;
    max_len = 20;

    changekey = Button("Change Password", "change");
}

RenderResult PasswordDialog::Render(Terminal *term, int update_flag)
{
    FnTrace("PasswordDialog::Render()");
    Settings *s = term->GetSettings();

    if (update_flag)
    {
        if (update_flag == RENDER_NEW)
        {
            force_change = ((int)strlen(password) < s->min_pw_len);
            stage = force_change;
        }
    }

    GetTextDialog::Render(term, update_flag);

    // change password
    int ww = w - (border * 2);
    int kw = (ww * 6) / 40;
    changekey->SetRegion(x + border, y + border, kw, hh);

    // Render
    genericChar str[256];
    int col = color[0];
    switch (stage)
    {
    case 0:
        TextC(term, 1, term->Translate("Enter Your Password"), col);
        break;
    case 1:
        TextC(term, 1, term->Translate("Enter Your Old Password"), col);
        if (force_change)
        {
            snprintf(str, STRLENGTH, "(%s)", term->Translate("You Must Change Your Password To Continue"));
            TextC(term, 4.5, str);
        }
        break;
    case 2:
        TextC(term, 1, term->Translate("Enter Your New Password"), col);
        break;
    case 3:
        TextC(term, 1, term->Translate("Enter Your New Password Again"), col);
        break;
    }

    min_len = s->min_pw_len;
    if (min_len > 0 && (stage == 2 || stage == 3))
    {
        snprintf(str, STRLENGTH, "(%s %d)",
                term->Translate("Minimum Password Length Is"), min_len);
        TextC(term, 4.5, str);
    }

    buttons.Render(term);

    return RENDER_OKAY;
}

SignalResult PasswordDialog::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("PasswordDialog::Signal()");
    static const genericChar* commands[] = {
        "enter", "change", "cancel", NULL};

    Employee *e = term->user;
    int error = 0;
    int idx = CompareList(message, commands);

    switch (idx)
    {
    case 0:  // enter
        switch (stage)
        {
        case 1:
            if (StringCompare(buffer, password))
            {
                // password failed - try backdoor
                if (atoi(buffer) == SUPERUSER_KEY)
                    PasswordOkay(term);
                else
                    PasswordFailed(term);
                return SIGNAL_TERMINATE;
            }
            ++stage;
            buffer[0] = '\0';
            buffidx = 0;
            Draw(term, 0);
            return SIGNAL_OKAY;

        case 2:
            if ((int) strlen(buffer) < min_len)
            {
                PasswordFailed(term);
                return SIGNAL_TERMINATE;
            }
            ++stage;
            strcpy(new_password, buffer);
            buffer[0] = '\0';
            buffidx = 0;
            Draw(term, 0);
            return SIGNAL_OKAY;

        case 3:
            if (StringCompare(new_password, buffer) || e == NULL)
                PasswordFailed(term);
            else
            {
                e->password.Set(new_password);
                term->system_data->user_db.Save();
                PasswordOkay(term);
            }
            return SIGNAL_TERMINATE;

        default:
            if (StringCompare(buffer, password) &&
                (atoi(buffer) != SUPERUSER_KEY))
            {
                PasswordFailed(term);
            }
            else
                PasswordOkay(term);
            return SIGNAL_TERMINATE;
        }
        break;

    case 1:  // change
        if (stage == 0 && e)
        {
            stage = 1;
            buffer[0] = '\0';
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        break;

    case 3:  // cancel
        term->Signal("passwordcancel", group_id);
        return SIGNAL_TERMINATE;
        break;

    default:
        return GetTextDialog::Signal(term, message);
    }

    if (error)
        return SIGNAL_IGNORED;
    else
        return SIGNAL_OKAY;
}

int PasswordDialog::RenderEntry(Terminal *term)
{
    FnTrace("PasswordDialog::RenderEntry()");
    Entry(term, (size_x/2) - 15, 2.5, 30);
    int  len = strlen(buffer);
    std::string str(len, '*');
    TextC(term, 2.5, str.c_str(), COLOR_WHITE);
    return 0;
}

int PasswordDialog::PasswordOkay(Terminal *term)
{
    FnTrace("PasswordDialog::PasswordkOkay()");
    term->password_given = 1;
    if (term->password_jump)
    {
        term->Jump(JUMP_STEALTH, term->password_jump);
        term->password_jump = 0;
    }
    else
        term->Signal("passwordgood", group_id);
    return 0;
}

int PasswordDialog::PasswordFailed(Terminal *term)
{
    FnTrace("PasswordDialog::PasswordFailed()");
    term->password_given = 0;
    term->password_jump  = 0;
    term->Signal("passwordfailed", group_id);
    return 0;
}


/*********************************************************************
 * CreditCardAmountDialog Class
 ********************************************************************/

CreditCardAmountDialog::CreditCardAmountDialog()
{
    FnTrace("CreditCardAmountDialog::CreditCardAmountDialog()");

    decimal = 1;
    cct_type = CC_TIP;
    name.Set("Enter Amount of Tip");
}

CreditCardAmountDialog::CreditCardAmountDialog(Terminal *term, const char* title, int type)
{
    FnTrace("CreditCardAmountDialog::CreditCardAmountDialog()");

    decimal = 1;
    cct_type = type;
    name.Set(title);
    if (cct_type == CC_AMOUNT)
        buffer = term->auth_amount;
}

SignalResult CreditCardAmountDialog::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("CreditCardAmountDialog::Signal()");
    SignalResult retval = SIGNAL_IGNORED;
    static const genericChar* command[] = { "cancel", "enter", NULL };
    CreditCardDialog *ccm = NULL;

    int idx = CompareList(message, command);
    switch (idx)
    {
    case 0:  // cancel
        // we'll fall through because cancel works the same way enter does
        // except that we don't want to modify term->credit->tip
    case 1:  // enter
        if (cct_type == CC_REFUND)
            ccm = new CreditCardDialog(term, AUTH_REFUND, REFUND_MSG);
        else
            ccm = new CreditCardDialog(term, 0, NULL);
        term->NextDialog(ccm);
        if (idx == 1 && term->credit != NULL)
        {
            if (cct_type == CC_TIP)
            {
                term->credit->Tip(buffer);
                if (term->GetSettings()->auto_authorize &&
                    term->credit != NULL &&
                    term->credit->IsPreauthed())
                {
                    term->auth_action = AUTH_COMPLETE;
                }
            }
            else
                term->auth_amount = buffer;
        }
        retval = SIGNAL_TERMINATE;
        break;
    default:
        retval = TenKeyDialog::Signal(term, message);
        break;
    }

    return retval;
}


/*********************************************************************
 * CreditCardEntryDialog Class
 ********************************************************************/

CreditCardEntryDialog::CreditCardEntryDialog()
{
    FnTrace("CreditCardEntryDialog::CreditCardEntryDialog()");

    name.Set("Enter Credit/Debit Card");
    decimal      = 0;
    w            = 480;
    h            = 640;
    first_row    = 210;
    cc_num[0]    = '\0';
    max_num      = 23;  // 'nnnn nnnn nnnn nnnn nnn' == 19 + 4
    cc_expire[0] = '\0';
    max_expire   = 5;   // 'mm/yy'  == 4 + 1
    current      = cc_num;
    last_current = NULL;
    max_current  = max_num;
    curr_entry   = &entry_pos[0];
}

/****
 * FormatCCInfo:  Prepares the information to be sent as a ViewTouch signal.
 *  The format is 'swipe manual <PAN>=<Expiry>', with all non-numeric characters
 *  removed from both PAN and Expiry.
 ****/
int CreditCardEntryDialog::FormatCCInfo(char* dest, const char* number, const char* expire)
{
    FnTrace("CreditCardEntryDialog::FormatCCInfo()");
    int retval = 0;
    int idx;
    int didx = 0;

    strcpy(dest, "manual ");
    didx = strlen(dest);

    idx = 0;
    while (number[idx] != '\0')
    {
        if (number[idx] != ' ')
        {
            dest[didx] = number[idx];
            didx += 1;
        }
        idx += 1;
    }

    dest[didx] = '=';
    didx += 1;
    idx = 0;
    while (expire[idx] != '\0')
    {
        if (expire[idx] != '/')
        {
            dest[didx] = expire[idx];
            didx += 1;
        }
        idx += 1;
    }
    dest[didx] = '\0';

    return retval;
}

int CreditCardEntryDialog::SetCurrent(Terminal *term, const char* set)
{
    FnTrace("CreditCardEntryDialog::SetCurrent()");
    int retval = 0;

    if (set == cc_num)
    {
        current = cc_num;
        max_current = max_num;
        curr_entry = &entry_pos[0];
        RenderEntry(term);
    }
    else
    {
        current = cc_expire;
        max_current = max_expire;
        curr_entry = &entry_pos[1];
        RenderEntry(term);
    }

    return retval;
}

RenderResult CreditCardEntryDialog::Render(Terminal *term, int update_flag)
{
    FnTrace("CreditCardEntryDialog::Render()");
    RenderResult retval = RENDER_OKAY;

    font       = FONT_TIMES_24B; // Changed from FONT_TIMES_34 - temporary fix for oversized dialog text
    retval = TenKeyDialog::Render(term, update_flag);

    return retval;
}

SignalResult CreditCardEntryDialog::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("CreditCardEntryDialog::Touch()");
    SignalResult retval = SIGNAL_OKAY;

    if (ty >= first_row_y)
        retval = TenKeyDialog::Touch(term, tx, ty);
    else if (entry_pos[0].IsPointIn(tx, ty))
        SetCurrent(term, cc_num);
    else if (entry_pos[1].IsPointIn(tx, ty))
        SetCurrent(term, cc_expire);

    return retval;
}

SignalResult CreditCardEntryDialog::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("CreditCardEntryDialog::Signal()");
    static const genericChar* command[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "enter", "backspace", "cancel", NULL };
    int len;
    SignalResult retval = SIGNAL_OKAY;
    CreditCardDialog *ccm = NULL;

    int idx = CompareList(message, command);
    if (idx < 0)
        return SIGNAL_IGNORED;

    genericChar str[256];
    switch (idx)
    {
    case 10:  // enter
        if (cc_num[0] == '\0')
        {
            SetCurrent(term, cc_num);
        }
        else if (cc_expire[0] == '\0')
        {
            SetCurrent(term, cc_expire);
        }
        else
        {
            FormatCCInfo(str, cc_num, cc_expire);
            ccm = new CreditCardDialog(term, str);
            SetAllActions(ccm);
            term->NextDialog(ccm);
            retval = SIGNAL_TERMINATE;
        }
        break;
    case 11:  // backspace
        if (current != NULL)
        {
            len = strlen(current);
            if (current[len - 1] == ' ' || current[len - 1] == '/')
                len -= 1;
            current[len - 1] = '\0';
        }
        break;
    case 12:  // cancel
        ccm = new CreditCardDialog();
        SetAllActions(ccm);
        term->NextDialog(ccm);
        retval = SIGNAL_TERMINATE;
        break;
    default:
        if (current != NULL)
        {
            len = strlen(current);
            if (len < max_current)
            {
                if (current == cc_num)
                {
                    if ((len > 0) && (((len + 1) % 5) == 0))
                        current[len++] = ' ';
                }
                else
                {
                    if (len == 2)
                        current[len++] = '/';
                }
                current[len++] = (char)message[0];
                current[len] = '\0';
            }
        }
        break;
    }

    RenderEntry(term);
    return retval;
}

SignalResult CreditCardEntryDialog::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("CreditCardEntryDialog::Keyboard()");
    SignalResult retval;

    switch (my_key)
    {
    case 9:  // tab
        if (current == cc_num)
            SetCurrent(term, cc_expire);
        else
            SetCurrent(term, cc_num);
        retval = SIGNAL_OKAY;
        break;
    default:
        retval = TenKeyDialog::Keyboard(term, my_key, state);
        break;
    }

    return retval;
}

int CreditCardEntryDialog::RenderEntry(Terminal *term)
{
    FnTrace("CreditCardEntryDialog::RenderEntry()");
    int  retval = 0;
    Flt  entry_width = (size_x / 2) - 10;
    Flt  num_pos = 1.5;
    Flt  exp_pos = 3.75;
    char buff[STRLENGTH];

    font = FONT_TIMES_24B;

    // Render the credit card number entry
    TextL(term, num_pos, "Credit Card Number", COLOR_BLACK);
    Entry(term, entry_width, num_pos + 1.0, 20, &entry_pos[0]);
    entry_pos[0].y -= 35;
    entry_pos[0].h += 35;
    if (cc_num[0] == '\0')
        buff[0] = '\0';
    else
        strncpy(buff, cc_num, STRLENGTH);
    if (current == cc_num)
        strncat(buff, "_", sizeof(buff) - strlen(buff) - 1);
    TextPosL(term, 6, num_pos + 1.0, buff, COLOR_WHITE);
    term->UpdateArea(entry_pos[0].x, entry_pos[0].y, entry_pos[0].w, entry_pos[0].h);

    // Render the expiration entry
    TextL(term, exp_pos, "Expiration Date", COLOR_BLACK);
    Entry(term, entry_width, exp_pos + 1.0, 20, &entry_pos[1]);
    entry_pos[1].y -= 35;
    entry_pos[1].h += 35;
    if (cc_expire[0] == '\0')
        buff[0] = '\0';
    else
        strncpy(buff, cc_expire, STRLENGTH);
    if (current == cc_expire)
        strncat(buff, "_", sizeof(buff) - strlen(buff) - 1);
    TextPosL(term, 6, exp_pos + 1.0, buff, COLOR_WHITE);
    term->UpdateArea(entry_pos[1].x, entry_pos[1].y, entry_pos[1].w, entry_pos[1].h);

    last_current = current;

    return retval;
}


/*********************************************************************
 * CreditCardVoiceDialog:  Provides a mechanism for adding voice
 *  authorizations.  GetTextDialog really does almost everything we
 *  need, but we also have to get back to CreditCardDialog.
 ********************************************************************/

// quick_mode determines how we'll return.  If it's 0, we'll open a CreditCardDialog.
// If it's 1, we'll send a return_message so that somebody else can handle the
// results.
#define CCVD_DISPLAY "Enter Voice Authorization Number"
#define CCVD_RETURN  "ccvoiceauth"

CreditCardVoiceDialog::CreditCardVoiceDialog()
{
    FnTrace("CreditCardVoiceDialog::CreditCardVoiceDialog()");

    strcpy(display_string, CCVD_DISPLAY);
    return_message[0] = '\0';
    max_len = 20;
    hh = 80;
}

CreditCardVoiceDialog::CreditCardVoiceDialog(const char* msg, const char* retmsg, int mlen)
{
    FnTrace("CreditCardVoiceDialog::CreditCardVoiceDialog(const char* , const char* , int)");

    if (msg != NULL)
        strcpy(display_string, msg);
    else
        strcpy(display_string, CCVD_DISPLAY);

    if (retmsg != NULL)
        strcpy(return_message, retmsg);
    else
        strcpy(return_message, CCVD_RETURN);

    max_len = mlen;
    hh = 80;
}

CreditCardVoiceDialog::~CreditCardVoiceDialog()
{
}

RenderResult CreditCardVoiceDialog::Render(Terminal *term, int update_flag)
{
    FnTrace("CreditCardVoiceDialog::Render()");
    Settings *settings = term->GetSettings();
    int font_old = font;
    Flt lineh = .75;
    Flt linec = 4.0;

    GetTextDialog::Render(term, update_flag);

    font = FONT_TIMES_24B;
    TextC(term, linec, term->ReplaceSymbols(settings->cc_voice_message1.Value()), COLOR_BLACK);
    linec += lineh;
    TextC(term, linec, term->ReplaceSymbols(settings->cc_voice_message2.Value()), COLOR_BLACK);
    linec += lineh;
    TextC(term, linec, term->ReplaceSymbols(settings->cc_voice_message3.Value()), COLOR_BLACK);
    linec += lineh;
    TextC(term, linec, term->ReplaceSymbols(settings->cc_voice_message4.Value()), COLOR_BLACK);
    font = font_old;

    return RENDER_OKAY;
}

SignalResult CreditCardVoiceDialog::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("CreditCardVoiceDialog::Signal()");
    SignalResult retval = SIGNAL_OKAY;
    static const genericChar* commands[] = {
        "enter", "cancel", NULL};
    int idx = CompareList(message, commands);
    int error = 0;
    CreditCardDialog *ccm = NULL;

    switch (idx)
    {
    case 0:  // enter
    case 1:  // cancel
        if (buffidx > 0)
            term->auth_voice.Set(buffer);
        if (return_message[0] == '\0')
        {
            ccm = new CreditCardDialog(term);
            term->NextDialog(ccm);
        }
        else
        {
            term->Signal(return_message, 0);
        }
        retval = SIGNAL_TERMINATE;
        break;
    default:
        retval = GetTextDialog::Signal(term, message);
    }

    if (error)
        retval = SIGNAL_IGNORED;


    return retval;
}


/*********************************************************************
 * CreditCardDialog Class:  Provides the various functions (swipe,
 *  authorize, refund, etc.) that are needed for credit card
 *  operations.
 ********************************************************************/

CreditCardDialog::CreditCardDialog()
{
    FnTrace("CreditCardDialog::CreditCardDialog()");

    authorizing = AUTH_NONE;
    SetMessage(NULL, NULL, NULL);
    Init(NULL, NULL, NULL);
}

CreditCardDialog::CreditCardDialog(Terminal *term, const char* swipe_value)
{
    FnTrace("CreditCardDialog::CreditCardDialog(Terminal, const char* )");

    authorizing = AUTH_NONE;
    SetMessage(NULL, NULL, NULL);
    Init(term, NULL, swipe_value);
}

CreditCardDialog::CreditCardDialog(Terminal *term, SubCheck *subch, const char* swipe_value)
{
    FnTrace("CreditCardDialog::CreditCardDialog(Terminal, SubCheck, const char* )");

    authorizing = AUTH_NONE;
    SetMessage(NULL, NULL, NULL);
    Init(term, subch, swipe_value);
}

CreditCardDialog::CreditCardDialog(Terminal *term, int action, const char* message)
{
    FnTrace("CreditCardDialog::CreditCardDialog(Terminal, int, const char* )");
    const char* swipe_msg = WAIT_MSG;

    authorizing = action;
    if (term != NULL && term->credit != NULL && term->credit->CardType() == CARD_TYPE_DEBIT)
        swipe_msg = SWIPE_MSG;
    SetMessage(term, message, swipe_msg);
    Init(term, NULL, NULL);
}



/*********************************************************************
 * Init:  I got tired of duplicating so much code and making sure
 *  everything was properly initialized, so all CreditCardDialog
 *  constructors will call this function, which will initialize
 *  everything.
 ********************************************************************/
void CreditCardDialog::Init(Terminal *term, SubCheck *subch, const char* swipe_value)
{
    FnTrace("CreditCardDialog::Init()");

    w              = CCD_WIDTH;
    h              = CCD_HEIGHT;

    preauth_key    = Button("Pre-Authorize", "ccpreauth");
    complete_key   = Button("Pre-Auth Complete", "cccomplete");
    auth_key       = Button("Authorize", "ccauthorize");
    advice_key     = Button("Pre-Auth Advice", "ccadvice");
    tip_key        = Button("Add Tip", "ccaddtip");
    cancel_key     = Button("Cancel", "cccancel");
    void_key       = Button("Void", "ccvoid");
    refund_key     = Button("Refund", "ccrefund");
    undorefund_key = Button("Undo Refund", "ccundorefund");
    manual_key     = Button("Manual Entry", "ccmanual");
    done_key       = Button("Done", "ccdone");
    credit_key     = Button("Credit", "cccredit");
    debit_key      = Button("Debit", "ccdebit");
    swipe_key      = Button("Swipe", "ccswipe");
    clear_key      = Button("Clear", "ccclear");
    voice_key      = Button("Voice Authorization", "ccvoice");

    lit            = NULL;
    saved_credit   = NULL;
    declined       = 0;
    finalizing     = 0;
    from_swipe     = 0;

    frame[1]       = frame[0];
    texture[1]     = texture[0];
    color[1]       = color[0];
    frame[2]       = frame[0];
    texture[2]     = texture[0];
    color[2]       = color[0];

    name.Set("CCM");
    last_message[0] = '\0';

    if (term != NULL)
    {
        if (subch != NULL)
            term->pending_subcheck = subch;
        if (swipe_value != NULL)
        {
            if (term->credit != NULL)
                term->credit->ParseSwipe(swipe_value);
            else
                ProcessSwipe(term, swipe_value);
            if (term->credit->IsValid() &&
                term->auth_action == 0 &&
                !term->credit->IsAuthed() &&
                !term->credit->IsPreauthed() &&
                (term->GetSettings()->authorize_method == CCAUTH_CREDITCHEQ ||
                 term->GetSettings()->auto_authorize > 0))
            {
                if ((term->credit->CardType() == CARD_TYPE_NONE) &&
                    ((term->GetSettings()->card_types & CARD_TYPE_DEBIT) == 0) &&
                    ((term->GetSettings()->card_types & CARD_TYPE_GIFT) == 0))
                {
                    term->credit->SetCardType(CARD_TYPE_CREDIT);
                }
                if (term->credit->CardType() == CARD_TYPE_NONE)
                    from_swipe = 1;
                else
                    SetAction(term, AUTH_PICK, NULL);
            }
        } else if (term->credit != NULL &&
            term->auth_voice.size() > 0 &&
            term->credit->Status() == CC_STATUS_NONE)
        {
            term->credit->SetAuth(term->auth_voice.Value());
            term->credit->SetStatus(CC_STATUS_VOICE);
            term->credit->SetState(CCAUTH_AUTHORIZE);
            term->credit->SetApproval(term->auth_voice.Value());
            term->credit->SetCode(term->auth_voice.Value());
        }
    }
}

const char* CreditCardDialog::SetMessage(Terminal *term, const char* msg1, const char* msg2)
{
    FnTrace("SetMessage()");

    if (msg1 == NULL)
    {
        message_str[0] = '\0';
    }
    else if (term != NULL)
    {
        if (msg2 == NULL)
        {
            if (term->credit->CardType() == CARD_TYPE_DEBIT)
                msg2 = SWIPE_MSG;
            else if (term->auth_swipe)
                msg2 = SWIPE_MSG;
            else
                msg2 = WAIT_MSG;
        }
        snprintf(message_str, STRLENGTH, "%s...%s",
                 term->Translate(msg1),
                 term->Translate(msg2));
    }
    else
    {
        if (msg2 == NULL)
            msg2 = WAIT_MSG;
        snprintf(message_str, STRLENGTH, "%s...%s", msg1, msg2);
    }

    return message_str;
}

RenderResult CreditCardDialog::Render(Terminal *term, int update_flag)
{
    FnTrace("CreditCardDialog::Render()");
    RenderResult retval = RENDER_OKAY;
    char str[STRLENGTH];
    int  bborder = 10;
    int  btall = 150;
    int  bshort = btall / 2;
    int  bwide;
    int  bnarrow;
    int  bypos;
    int  bxpos;
    int  font_old = font;
    int  status = -1;
    Flt  space = 0.7;
    Flt  line = 0.0;
    Settings *settings = term->GetSettings();
    SubCheck *sc = NULL;
    Employee *employee = term->user;
    int  ismanager = 0;
    int  font_color;
    int  have_tip = 0;
    int  query_line = 3;

    int  color_error   = COLOR_DK_RED;
    int  color_success = COLOR_DK_GREEN;
    int  color_text    = COLOR_DK_BLUE;
    int  color_button  = COLOR_BLUE;

    message_line = 4;

    if (term->pending_subcheck != NULL)
        sc = term->pending_subcheck;
    else if (term->check != NULL)
        sc = term->check->current_sub;

    if (employee != NULL && employee->IsManager(settings))
        ismanager = 1;
    if (sc != NULL && sc->TotalTip() > 0)
        have_tip = 1;

    if (lit != NULL)
    {
        lit->Draw(term, 0);
        lit = NULL;
    }

    LayoutZone::Render(term, update_flag);

    if (term->credit == NULL &&
        finalizing == 0 &&
        settings->CanDoDebit() == 0 &&
        settings->CanDoGift() == 0)
    {
        term->credit = new Credit();
        if (term->credit != NULL)
            term->credit->SetCardType(CARD_TYPE_CREDIT);
    }

    // First, find out what amount should go on the credit card and
    // initiate an action if all is done (card is valid and we haven't
    // already initiated action).
    if (term->credit != NULL)
    {
        if (term->auth_amount > 0)
        {
            term->credit->Amount(term->auth_amount);
        }
        else if (term->credit->Amount() == 0)
        {
            if (sc != NULL)
            {
                sc->FigureTotals(settings);
                term->credit->Amount(sc->balance);
            }
        }

        if (term->auth_swipe ||
            ((term->credit->IsValid() || term->credit->CardType() == CARD_TYPE_DEBIT) &&
             term->auth_action))
        {
            authorizing = term->auth_action;

            // Make sure we have the right message to display.
            if (authorizing == AUTH_PICK)
            {
                if (term->credit->CardType() == CARD_TYPE_DEBIT ||
                    settings->allow_cc_preauth == 0 ||
                    have_tip)
                {
                    authorizing = AUTH_AUTHORIZE;
                    if (term->auth_message != NULL)
                        SetMessage(term, term->auth_message, term->auth_message2);
                    else
                        SetMessage(term, AUTHORIZE_MSG);
                }
                else
                {
                    authorizing = AUTH_PREAUTH;
                    if (term->auth_message != NULL)
                        SetMessage(term, term->auth_message, term->auth_message2);
                    else
                        SetMessage(term, PREAUTH_MSG);
                }
            }
            else
                SetMessage(term, term->auth_message, term->auth_message2);
        }
    }

    // Render the top information (cc#, expiry, name)
    if (term->credit != NULL && term->credit->IsValid())
    {
        status = term->credit->Status();

        font = FONT_TIMES_24;
        snprintf(str, STRLENGTH, "%s:  %s", term->Translate("Card Number"),
                 term->credit->PAN(settings->show_entire_cc_num));
        TextC(term, line, str, color_text);
        line += space;
        snprintf(str, STRLENGTH, "%s:  %s", term->Translate("Expires"), term->credit->ExpireDate());
        TextC(term, line, str, color_text);
        if (strlen(term->credit->Name()) > 0)
        {
            line += space;
            snprintf(str, STRLENGTH, "%s:  %s", term->Translate("Holder"), term->credit->Name());
            TextC(term, line, str, color_text);
        }

        if (term->credit->Tip() == 0)
        {
            line += space;
            snprintf(str, STRLENGTH, "%s:  %s", term->Translate("Charge Amount"),
                     term->FormatPrice(term->credit->Amount(), 1));
            TextC(term, line, str, color_text);
        }
        else
        {
            char crdstr[STRLENGTH];
            char tipstr[STRLENGTH];
            char totstr[STRLENGTH];
            line += space;
            snprintf(crdstr, STRLENGTH, "Amount:  %s",
                     term->FormatPrice(term->credit->Amount() - term->credit->Tip(), 1));
            snprintf(tipstr, STRLENGTH, "Tip:  %s",
                     term->FormatPrice(term->credit->Tip(), 1));
            snprintf(totstr, STRLENGTH, "Total:  %s",
                     term->FormatPrice(term->credit->Total(1), 1));
            snprintf(str, STRLENGTH, "%s, %s, %s", crdstr, tipstr, totstr);
            TextC(term, line, str, color_text);
        }
        if (term->auth_voice.size() > 0)
        {
            line += space;
            snprintf(str, STRLENGTH, "%s:  %s", term->Translate("Voice Auth"),
                     term->auth_voice.Value());
            TextC(term, line, str, color_text);
        }

        font = font_old;
    }

    // Do we at least have an error message of some sort?
    if (term->credit != NULL && authorizing == 0)
    {
        if (term->credit->IsVoided())
            strcpy(str, term->Translate("Void Successful"));
        else if (term->credit->IsRefunded())
            strcpy(str, term->Translate("Refund Successful"));
        else
            strcpy(str, term->credit->Verb());
        if (str[0] == '\0')
            snprintf(str, STRLENGTH, "%s %s", term->credit->Code(), term->credit->Auth());
        if (strlen(str) > 0)
        {
            if (term->credit->IsAuthed(1))
                font_color = color_success;
            else
                font_color = color_error;
            font = FONT_TIMES_24B; // Changed from FONT_TIMES_34B - temporary fix for oversized dialog text
            TextC(term, 4, str, font_color);
        }
    }

    // Blank these keys, then determine which ones to display
    preauth_key->SetRegion(0, 0, 0, 0);
    complete_key->SetRegion(0, 0, 0, 0);
    auth_key->SetRegion(0, 0, 0, 0);
    advice_key->SetRegion(0, 0, 0, 0);
    tip_key->SetRegion(0, 0, 0, 0);
    cancel_key->SetRegion(0, 0, 0, 0);
    void_key->SetRegion(0, 0, 0, 0);
    refund_key->SetRegion(0, 0, 0, 0);
    undorefund_key->SetRegion(0, 0, 0, 0);
    manual_key->SetRegion(0, 0, 0, 0);
    done_key->SetRegion(0, 0, 0, 0);
    credit_key->SetRegion(0, 0, 0, 0);
    debit_key->SetRegion(0, 0, 0, 0);
    swipe_key->SetRegion(0, 0, 0, 0);
    clear_key->SetRegion(0, 0, 0, 0);
    voice_key->SetRegion(0, 0, 0, 0);

    bwide   = w - (bborder * 2);
    bnarrow = (w / 2) - (bborder * 2) + (bborder / 2);
    bxpos   = x + bborder;
    bypos   = y + h - (bshort + bborder);

    // display buttons
    if (term->credit == NULL ||
        term->credit->CardType() == CARD_TYPE_NONE)
    {
        TextC(term, query_line, term->Translate("Please select card type."), color_text);
        cancel_key->SetRegion(bxpos, bypos, bwide, bshort);
        bypos = bypos - (btall + bborder);

        // display "Credit or Debit" buttons
        if (settings->CanDoDebit())
        {
            credit_key->SetRegion(bxpos, bypos, bnarrow, btall);
            credit_key->color = color_button;
            bxpos = x + w - bnarrow - bborder;
            debit_key->SetRegion(bxpos, bypos, bnarrow, btall);
            debit_key->color = color_button;
        }
        else
        {
            credit_key->SetRegion(bxpos, bypos, bwide, btall);
            credit_key->color = color_button;
        }
    }
    else if (authorizing)
    {
        // Null action.  Eventually, we may allow "in progress" cancellation.
        // For now, we don't display any buttons.
    }
    else if (term->credit->IsRefunded(1) &&
             term->auth_action != AUTH_REFUND_CORRECT)
    {
        bxpos = x + bborder;
        undorefund_key->SetRegion(bxpos, bypos, bwide, bshort);
        bxpos = x + bborder;
        bypos = bypos - (btall + bborder);
        done_key->SetRegion(bxpos, bypos, bwide, btall);
        done_key->color = color_button;
    }
    else if (term->credit->IsVoided(1) &&
             term->credit->LastAction() == CCAUTH_VOID)
    {
        bxpos = x + bborder;
        bypos = y + h - (btall + bborder);
        done_key->SetRegion(bxpos, bypos, bwide, btall);
        done_key->color = color_button;
    }
    else if (term->credit->IsAuthed(1) == 0 || term->credit->IsValid() == 0)
    {
        cancel_key->SetRegion(bxpos, bypos, bwide, bshort);
        bypos = bypos - (btall + bborder);
        if (settings->authorize_method == CCAUTH_CREDITCHEQ)
        {
            // display "Swipe or Manual" buttons
            if (declined)
            {
                voice_key->SetRegion(bxpos, bypos, bwide, btall);
                voice_key->color = color_button;
            }
            else if (authorizing == 0)
            {
                TextC(term, query_line, term->Translate("Please select card entry method."), color_text);
                swipe_key->SetRegion(bxpos, bypos, bnarrow, btall);
                swipe_key->color = color_button;
                bxpos = x + w - bnarrow - bborder;
                manual_key->SetRegion(bxpos, bypos, bnarrow, btall);
                manual_key->color = color_button;
            }
        }
        else if (settings->authorize_method == CCAUTH_MAINSTREET)
        {
            if (term->credit->IsValid() == 0)
            {
                // display "Manual" button and swipe message
                TextC(term, query_line, term->Translate("Please swipe the card"), color_text);
                TextC(term, query_line + 1, term->Translate("or select Manual Entry"), color_text);
                manual_key->SetRegion(bxpos, bypos, bwide, btall);
                manual_key->color = color_button;
            }
            else
            {
                // display the "Authorize or Manual" buttons
                preauth_key->SetRegion(bxpos, bypos, bnarrow, btall);
                preauth_key->color = color_button;
                bxpos = x + w - bnarrow - bborder;
                auth_key->SetRegion(bxpos, bypos, bnarrow, btall);
                auth_key->color = color_button;
            }
        }
    }
    else if (term->credit->IsVoiced())
    {
        cancel_key->SetRegion(bxpos, bypos, bwide, bshort);
        bypos = bypos - (btall + bborder);
        bxpos = x + bborder;
        advice_key->SetRegion(bxpos, bypos, bwide, btall);
        advice_key->color = color_button;
    }
    else
    {
        if (settings->authorize_method == CCAUTH_MAINSTREET &&
            term->credit->IsPreauthed())
        {
            tip_key->SetRegion(bxpos, bypos, bnarrow, bshort);
            tip_key->color = color_button;
            bxpos = x + w - bnarrow - bborder;
            void_key->SetRegion(bxpos, bypos, bnarrow, bshort);
        }
        else
        {
            void_key->SetRegion(bxpos, bypos, bwide, bshort);
        }

        bypos = bypos - (btall + bborder);
        bxpos = x + bborder;
        if (term->credit->IsPreauthed())
        {
            done_key->SetRegion(bxpos, bypos, bnarrow, btall);
            done_key->color = color_button;
            bxpos = x + w - bnarrow - bborder;
            complete_key->SetRegion(bxpos, bypos, bnarrow, btall);
            complete_key->color = color_button;
        }
        else if (term->credit->IsAuthed())
        {
            done_key->SetRegion(bxpos, bypos, bwide, btall);
            done_key->color = color_button;
            done_key->SetLabel(term->Translate("Close"));
        }
        else if (have_tip || term->credit->CardType() == CARD_TYPE_DEBIT)
        {
            done_key->SetRegion(bxpos, bypos, bwide, btall);
            done_key->color = color_button;
            done_key->SetLabel(term->Translate("Close"));
        }
        else
        {
            preauth_key->SetRegion(bxpos, bypos, bnarrow, btall);
            preauth_key->color = color_button;
            bxpos = x + w - bnarrow - bborder;
            done_key->SetRegion(bxpos, bypos, bnarrow, btall);
            done_key->color = color_button;
        }
    }

    // Display the message and prevent flickering by ensuring we
    // won't just keep showing the same message over and over.
    if (message_str[0] != '\0' && strcmp(last_message, message_str) != 0)
    {
        TextC(term, message_line, message_str, color_text);
        strcpy(last_message, message_str);
    }

    // This must come after all drawing commands to make sure we
    // display everything we need to display.
    buttons.Render(term);
    term->UpdateArea(x, y, w, h);

    // Now determine what action, if any, to take.  We do it here,
    // rather than in, for example, the Signal() method because it
    // could be a while before the screen gets updated again (while we
    // wait for network communications, modems, whatever), and we want
    // to make sure before that happens we let the user know there
    // could be a delay.  That's done above, so now we can initiate
    // the action.
    if (authorizing == AUTH_PREAUTH)
        term->credit->GetPreApproval(term);
    else if (authorizing == AUTH_COMPLETE)
        term->credit->GetFinalApproval(term);
    else if (authorizing == AUTH_AUTHORIZE)
        term->credit->GetApproval(term);
    else if (authorizing == AUTH_VOID)
        term->credit->GetVoid(term);
    else if (authorizing == AUTH_REFUND)
        term->credit->GetRefund(term);
    else if (authorizing == AUTH_REFUND_CORRECT)
        term->credit->GetRefundCancel(term);
    else if (authorizing == AUTH_ADVICE)
        term->credit->GetFinalApproval(term);

    // And set authorizing for the next pass.
    if (authorizing)
    {
        authorizing |= AUTH_IN_PROGRESS;
        term->auth_action = AUTH_NONE;
        term->RedrawZone(this, 500);
    }

    return retval;
}

SignalResult CreditCardDialog::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("CreditCardDialog::Touch()");
    SignalResult retval = SIGNAL_IGNORED;
    ZoneObject *zo = buttons.Find(tx, ty);

    if (zo != NULL)
    {
        if (lit)
            lit->Draw(term, 0);
        lit = NULL;
        ButtonObj *b = (ButtonObj *) zo;
        b->Draw(term, 1);
        retval = Signal(term, b->message.Value());
        lit = b;
        term->RedrawZone(this, 500);
    }

    return retval;
}

int CreditCardDialog::SetAction(Terminal *term, int action, const char* msg1, const char* msg2)
{
    FnTrace("CreditCardDialog::SetAction()");
    int retval = 1;

    if (term->credit != NULL)
    {
        saved_credit = term->credit->Copy();
        if (term->credit->RequireSwipe())
        {
            term->credit->ClearCardNumber();
            if (term->auth_amount == 0)
                term->auth_amount = term->credit->Total();
        }
        term->auth_action = action;
        term->auth_message = msg1;
        term->auth_message2 = msg2;

        retval = 0;
    }

    return retval;
}

int CreditCardDialog::ClearAction(Terminal *term, int all)
{
    FnTrace("CreditCardDialog::ClearAction()");
    int retval = 0;

    term->auth_amount   = 0;
    term->auth_action   = AUTH_NONE;
    term->auth_swipe    = 0;
    term->auth_message  = NULL;
    term->auth_message2 = NULL;
    term->auth_voice.Clear();
    authorizing = 0;

    SetMessage(NULL, NULL, NULL);
    last_message[0] = '\0';

    return retval;
}

int CreditCardDialog::DialogDone(Terminal *term)
{
    int retval = 0;

    if (term->credit != NULL)
    {
        FinishCreditCard(term);
        term->Signal("ccamountchanged", 0);
        term->credit = NULL;
        term->pending_subcheck = NULL;
        ClearAction(term);
        PrepareForClose(ACTION_SUCCESS);
    }

    return retval;
}

SignalResult CreditCardDialog::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("CreditCardDialog::Signal()");
    SignalResult retval = SIGNAL_OKAY;
    const char* commands[] = { "swipe ", "ccauthorize", "ccpreauth", "cccomplete",
                         "ccvoid", "ccrefund", "ccaddtip", "cccancel",
                         "ccmanual", "ccdone", "ccprocessed", "cccredit",
                         "ccdebit", "ccswipe", "ccundorefund", "ccclear",
                         "ccvoice", "ccadvice", NULL };
    int idx = CompareListN(commands, message);
    CreditCardEntryDialog *cce = NULL;
    CreditCardAmountDialog *cct = NULL;
    CreditCardVoiceDialog *ccv = NULL;
    SimpleDialog *sd = NULL;
    int draw = 0;
    Settings *settings = term->GetSettings();

    switch (idx)
    {
    case 0:  // swipe
        if (ProcessSwipe(term, &message[6]) == 0 &&
            settings->auto_authorize == 1)
        {
            if (term->auth_action == AUTH_NONE)
                SetAction(term, AUTH_PICK, NULL);
        }
        draw = 1;
        break;
    case 1:  // ccauthorize
        if (term->credit != NULL)
        {
            SetAction(term, AUTH_AUTHORIZE, AUTHORIZE_MSG);
            draw = 1;
        }
        else
            retval = SIGNAL_TERMINATE;
        break;
    case 2:  // ccpreauth
        if (term->credit != NULL)
        {
            SetAction(term, AUTH_PREAUTH, PREAUTH_MSG);
            draw = 1;
        }
        else
            retval = SIGNAL_TERMINATE;
        break;
    case 3:  // cccomplete
        if (term->credit != NULL)
        {
            SetAction(term, AUTH_COMPLETE, COMPLETE_MSG);
            draw = 1;
        }
        break;
    case 4:  // ccvoid
        if (term->user->IsManager(settings))
        {
            term->auth_amount = term->void_amount;
            if (term->credit != NULL)
            {
                SetAction(term, AUTH_VOID, VOID_MSG);
                draw = 1;
            }
            else
                retval = SIGNAL_TERMINATE;
        }
        else
        {
            sd = new SimpleDialog(term->Translate("Only managers can void credit cards."));
            sd->Button("Okay");
            term->NextDialog(sd);
            retval = SIGNAL_TERMINATE;
        }
        break;
    case 5:  // ccrefund
        if (term->user->IsManager(settings))
        {
            if (term->credit != NULL)
            {
                SetAction(term, AUTH_REFUND, REFUND_MSG);
                draw = 1;
            }
            else
                retval = SIGNAL_TERMINATE;
        }
        else
        {
            sd = new SimpleDialog(term->Translate("Only managers can void credit cards."));
            sd->Button("Okay");
            term->NextDialog(sd);
            retval = SIGNAL_TERMINATE;
        }
        break;
    case 6:  // ccaddtip
        // The CCTipDialog will set term->credit->tip, so we won't
        // need to respond to messages from it.
        cct = new CreditCardAmountDialog();
        term->NextDialog(cct);
        retval = SIGNAL_TERMINATE;
        break;
    case 7:  // cccancel
        if (term->credit != NULL)
        {
            if (term->credit->IsAuthed(1))
            {
                term->system_data->cc_exception_db->Add(term, term->credit);
                retval = SIGNAL_TERMINATE;
            }
            else
            {
                term->credit = NULL;
                retval = SIGNAL_TERMINATE;
            }
        }
        else
            retval = SIGNAL_TERMINATE;
        ClearAction(term);
        break;
    case 8:  // ccmanual
        cce = new CreditCardEntryDialog();
        SetAllActions(cce);
        term->NextDialog(cce);
        retval = SIGNAL_TERMINATE;
        break;
    case 9:  // ccdone
        DialogDone(term);
        retval = SIGNAL_TERMINATE;
        break;
    case 10:  // ccprocessed
        retval = ProcessCreditCard(term);
        ClearAction(term);
        if (term->credit != NULL &&
            term->credit->IsAuthed() &&
            settings->auto_authorize)
        {
            retval = Signal(term, "ccdone");
        }
        draw = 1;
        break;
    case 11:  // cccredit
        if (term->credit == NULL)
            term->credit = new Credit();
        else if (from_swipe == 0)
            term->credit->Clear();
        if (term->credit != NULL)
        {
            term->credit->SetCardType(CARD_TYPE_CREDIT);
            draw = 1;
        }
        from_swipe = 0;
        break;
    case 12:  // ccdebit
        if (term->credit == NULL)
            term->credit = new Credit();
        else if (from_swipe == 0)
            term->credit->Clear();
        if (term->credit != NULL)
        {
            term->credit->SetCardType(CARD_TYPE_DEBIT);
            SetAction(term, AUTH_AUTHORIZE, AUTHORIZE_MSG, SWIPE_MSG);
            draw = 1;
        }
        from_swipe = 0;
        break;
    case 13:  // ccswipe
        // This should only be called for CreditCheq Multi.
        // Starts the authorization process without a card number.
        if (term->credit != NULL)
        {
            term->auth_swipe = 1;
            if (term->auth_action == AUTH_NONE)
                term->auth_action = AUTH_PICK;
            draw = 1;
        }
        else
            retval = SIGNAL_TERMINATE;
        break;
    case 14:  // ccundorefund
        if (term->credit != NULL)
        {
            SetAction(term, AUTH_REFUND_CORRECT, REFUND_CANCEL_MSG);
            draw = 1;
        }
        break;
    case 15:  // ccclear
        if (term->credit != NULL)
        {
            term->credit->Clear();
            draw = 1;
        }
        break;
    case 16:  // ccvoice
        if (term->credit != NULL && term->credit->CardType() != CARD_TYPE_NONE)
        {
            ccv = new CreditCardVoiceDialog();
            term->NextDialog(ccv);
            retval = SIGNAL_TERMINATE;
        }
        else
            retval = SIGNAL_IGNORED;
        break;
    case 17:  // ccadvice
        if (term->credit != NULL)
        {
            SetAction(term, AUTH_ADVICE, ADVICE_MSG);
            draw = 1;
        }
        else
            retval = SIGNAL_TERMINATE;
        break;
    default:
        retval = SIGNAL_IGNORED;
        break;
    }

    if (draw == 1)
    {
        Render(term, 1);
        term->UpdateArea(x, y, w, h);
    }

    return retval;
}

SignalResult CreditCardDialog::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("CreditCardDialog::Keyboard()");
    SignalResult retval = SIGNAL_IGNORED;
    CreditCardVoiceDialog *ccv = NULL;

    switch (my_key)
    {
    case 118: // v
        if (debug_mode)
        {
            ccv = new CreditCardVoiceDialog();
            term->NextDialog(ccv);
            retval = SIGNAL_TERMINATE;
        }
        break;
    case 114: // r
        if (term->credit != NULL)
            term->credit->PrintReceipt(term);
        retval = SIGNAL_OKAY;
        break;
    }

    return retval;
}


int CreditCardDialog::ProcessSwipe(Terminal *term, const char* swipe_value)
{
    FnTrace("CreditCardDialog::ProcessSwipe()");
    int retval = 0;
    Employee *employee = term->user;

    if (employee == NULL)
        return 1;

    if (term->credit == NULL)
        term->credit = new Credit(swipe_value);
    else
        term->credit->ParseSwipe(swipe_value);

    if (term->credit == NULL || term->credit->IsValid() < 1)
    {
        if (term->credit != NULL)
            delete term->credit;
        term->credit = NULL;
        retval = 1;
    }

    return retval;
}

/****
 * FinishCreditCard: Returns 1 if the card is finalized, 0 otherwise.
 ****/
int CreditCardDialog::FinishCreditCard(Terminal *term)
{
    FnTrace("CreditCardDialog::FinishCreditCard()");
    int retval = 0;
    char str[STRLENGTH];
    int cc_id;
    int tender;
    int preauth_time_set;
    int auth_time_set;
    int last_action;
    SignalResult result = SIGNAL_IGNORED;

    // term->credit == NULL should not even be an option at this point,
    // but we'll double check it for safety.
    if (term->credit != NULL)
    {
        finalizing = 1;
        // The only time preauth_time or auth_time is set is from
        // Credit::Finalize, which is normally called from this
        // function.
        preauth_time_set = term->credit->preauth_time.IsSet();
        auth_time_set = term->credit->auth_time.IsSet();
        last_action = term->credit->LastAction();
        if (term->credit->IsVoided(1) && last_action == CCAUTH_VOID)
        {
            term->credit->Finalize(term);
            retval = 1;
        }
        else if (term->credit->IsRefunded(1) && last_action == CCAUTH_REFUND)
        {
            term->credit->Finalize(term);
            retval = 1;
        }
        else if (term->credit->IsAuthed() && last_action == CCAUTH_REFUND_CANCEL)
        {
            term->credit->Finalize(term);
            retval = 1;
        }
        else if (auth_time_set == 0)
        {
            if ((term->credit->IsPreauthed() && preauth_time_set == 0) ||
                (term->credit->IsAuthed() && auth_time_set == 0))
            {
                term->credit->Finalize(term);
                retval = 1;
            }

            if (term->credit->check_id == 0)
            {
                if (term->credit->Tip() > 0)
                {
                    snprintf(str, STRLENGTH, "tender %d %d %d %d",
                             TENDER_CHARGED_TIP, 0, 0, term->credit->Tip());
                    result = term->Signal(str, 0);
                }

                cc_id = term->credit->CreditType();
                if (cc_id == CARD_TYPE_DEBIT)
                    tender = TENDER_DEBIT_CARD;
                else
                    tender = TENDER_CREDIT_CARD;
                snprintf(str, STRLENGTH, "tender %d %d %d %d",
                         tender, cc_id, 0, term->credit->Total());
                result = term->Signal(str, 0);

                if (result == SIGNAL_IGNORED)
                {
                    // Probably opening a tab (otherwise: fatal error).  We
                    // need to add the payment manually.
                    if (term->check != NULL &&
                        term->page != NULL &&
                        term->page->id != -20)
                    {
                        SubCheck *sc = term->check->current_sub;
                        if (sc == NULL)
                            sc = term->check->NewSubCheck();
                        Payment *paymnt = sc->NewPayment(tender, cc_id, 0, term->credit->Total());
                        paymnt->credit = term->credit;
                        paymnt->credit->check_id = term->check->serial_number;
                        term->credit->Finalize(term);
                    }
                }
            }
        }
    }
    else if (debug_mode)
    {
        printf("Why did we get here?\n");
    }

    return retval;
}

SignalResult CreditCardDialog::ProcessCreditCard(Terminal *term)
{
    FnTrace("CreditCardDialog::ProcessCreditCard()");
    SignalResult retval = SIGNAL_OKAY;
    SimpleDialog *sd = NULL;
    Settings *settings = term->GetSettings();

    if (term->credit != NULL)
    {
        declined = 0;

        if (term->credit->Status() != CC_STATUS_ERROR && term->credit->IsValid())
        {
            if (settings->use_entire_cc_num == 0)
            {
                term->credit->CreditType();
                term->credit->MaskCardNumber();
            }
            if (term->is_bar_tab == 0)
            {
                // Do not print a receipt for PreAuth Completes or Voids unless the
                // settings require it.  Print receipts for all other transaction
                // types.
                int last_action = term->credit->LastAction();
                if ((last_action == CCAUTH_COMPLETE && settings->finalauth_receipt != 0) ||
                    (last_action == CCAUTH_VOID && settings->void_receipt != 0) ||
                    (last_action != CCAUTH_COMPLETE && last_action != CCAUTH_VOID))
                {
                    term->credit->PrintReceipt(term);
                }
            }
            else
            {
                DialogDone(term);
                return SIGNAL_TERMINATE;
            }
        }

        if (term->credit->Status() == CC_STATUS_ERROR ||
            term->credit->Status() == CC_STATUS_DENY)
        {
            declined = 1;
            if (saved_credit != NULL)
            {
                // We do not delete term->credit because we store it
                // in the credit card as an error.
                saved_credit->SetVerb(term->credit->Verb());
                saved_credit->AddError(term->credit);
                term->credit->Copy(saved_credit);
                saved_credit = NULL;
            }
            else if (debug_mode)
            {
                printf("We have an error or decline, but no saved card\n");
                printf("    Probably okay....\n");
            }
            if (strcmp(term->credit->Verb(), "No Card Information Entered") == 0)
            {
                char message[STRLONG] = "";
                strncat(message, term->credit->Verb(), sizeof(message) - strlen(message) - 1);
                strncat(message, "\\This could indicate a bad connection.", sizeof(message) - strlen(message) - 1);
                strncat(message, "\\Would you like to reset the connection?", sizeof(message) - strlen(message) - 1);
                sd = new SimpleDialog(message);
                sd->Button(term->Translate("Yes"), "ccqterminate");
                sd->Button(term->Translate("No"));
                term->NextDialog(sd);
                retval = SIGNAL_TERMINATE;
            }
        }
        else if (saved_credit != NULL)
        {
            delete saved_credit;
            saved_credit = NULL;
        }

        Draw(term, 1);
    }

    return retval;
}


/*********************************************************************
 * JobFilterDialog Class
 ********************************************************************/

JobFilterDialog::JobFilterDialog()
{
    FnTrace("JobFilterDialog::JobFilterDialog()");
    filter = 0;

    w = 920;
    h = 640;

    jobs = 0;
    genericChar str[256];

    int i = 1;
    while (JobName[i])
    {
        snprintf(str, STRLENGTH, "%d", i);
        job[jobs] = Button(JobName[i], str);
        ++jobs; ++i;
    }

    key[0] = Button("Okay", "okay");
    key[1] = Button("Cancel", "cancel");
}

RenderResult JobFilterDialog::Render(Terminal *term, int update_flag)
{
    FnTrace("JobFilterDialog::Render()");
    if (update_flag)
        filter = term->job_filter;

    LayoutZone::Render(term, update_flag);
    TextC(term, 0, term->Translate("Wage Group Filter"));

    int ww = w - border * 2;
    int kx, kw;

    int no[3];
    int remainder = jobs % 3;
    int i;

    for (i = 0; i < 3; ++i)
    {
        no[i] = jobs / 3;
        if (remainder > 0)
        {
            ++no[i];
            --remainder;
        }
    }

    for (i = 0; i < jobs; ++i)
    {
        int row;
        int col;
        if (i < no[0])
        {
            row = 0;
            col = i;
        }
        else if (i >= no[0] && i < (no[1] + no[0]))
        {
            row = 1;
            col = i - no[0];
        }
        else
        {
            row = 2;
            col = i - no[0] - no[1];
        }

        kx = (ww * col) / no[0];
        kw = ((ww * (col + 1)) / no[0]) - kx;

        job[i]->x = x + border + kx;
        job[i]->y = y + border + 40 + (row * 120);
        job[i]->w = kw;
        job[i]->h = 120;
    }

    key[0]->x = x + border;
    key[0]->y = y + h - border - 120;
    key[0]->w = 320;
    key[0]->h = 120;

    key[1]->x = x + w - border - 320;
    key[1]->y = y + h - border - 120;
    key[1]->w = 320;
    key[1]->h = 120;

    for (i = 0; i < jobs; ++i)
    {
        job[i]->selected = !(filter & (1 << JobValue[i + 1]));
        job[i]->Render(term);
    }
    key[0]->Render(term);
    key[1]->Render(term);
    return RENDER_OKAY;
}

SignalResult JobFilterDialog::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("JobFilterDialog::Signal()");
    static const genericChar* commands[] = {"okay", "cancel", NULL};

    int idx = CompareList(message, commands);
    switch (idx)
    {
    case 0:
        key[0]->Draw(term, 1);
        if (term->job_filter != filter)
        {
            term->job_filter = filter;
            term->Update(UPDATE_JOB_FILTER, NULL);
        }
        return SIGNAL_TERMINATE;
    case 1:
        key[1]->Draw(term, 1);
        return SIGNAL_TERMINATE;
    }

    int val = 0;
    if (sscanf(message, "%d", &val) != 1)
        return SIGNAL_IGNORED;

    int bit = 1 << JobValue[val];
    if (filter & bit)
    {
        filter -= bit;
        job[val - 1]->Draw(term, 1);
    }
    else
    {
        filter |= bit;
        job[val - 1]->Draw(term, 0);
    }
    return SIGNAL_OKAY;
}

SignalResult JobFilterDialog::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("JobFilterDialog::Touch()");
    ZoneObject *zo = buttons.Find(tx, ty);
    if (zo)
        return Signal(term, ((ButtonObj *) zo)->message.Value());
    else
        return SIGNAL_IGNORED;
}

SignalResult JobFilterDialog::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("JobFilterDialog::Keyboard()");
    switch (my_key)
    {
    case 27: // Esc
        return Signal(term, "cancel");
    }
    return SIGNAL_IGNORED;
}


/***********************************************************************
 * OpenTabDialog Class
 ***********************************************************************/
OpenTabDialog::OpenTabDialog(CustomerInfo *custinfo)
{
    FnTrace("OpenTabDialog::OpenTabDialog()");

    main_font = FONT_TIMES_24B; // Changed from FONT_TIMES_34 - temporary fix for oversized dialog text
    entry_font = FONT_TIMES_24B;

    customer = custinfo;
    h = 750;

    customer_name[0] = '\0';
    customer_phone[0] = '\0';
    customer_comment[0] = '\0';

    max_name = 20;
    max_phone = 15;
    max_comment = 35;

    current = customer_name;
    last_current = NULL;
    max_current = max_name;

    name.Set("Open Customer Tab");
}

RenderResult OpenTabDialog::Render(Terminal *term, int update_flag)
{
    FnTrace("OpenTabDialog::Render()");
    RenderResult retval = RENDER_ERROR;

    font   = main_font;
    retval = GetTextDialog::Render(term, update_flag);
    TextC(term, 0.5, term->Translate(name.Value()), COLOR_DEFAULT);

    return retval;
}

int OpenTabDialog::SetCurrent(Terminal *term, const char* set)
{
    FnTrace("OpenTabDialog::SetCurrent()");
    int retval = 0;

    if (set == customer_name)
    {
        current = customer_name;
        max_current = max_name;
        curr_entry = &entry_pos[0];
        RenderEntry(term);
    }
    else if (set == customer_phone)
    {
        current = customer_phone;
        max_current = max_phone;
        curr_entry = &entry_pos[1];
        RenderEntry(term);
    }
    else
    {
        current = customer_comment;
        max_current = max_comment;
        curr_entry = &entry_pos[2];
        RenderEntry(term);
    }

    return retval;
}

SignalResult OpenTabDialog::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("OpenTabDialog::Touch()");
    SignalResult retval = SIGNAL_IGNORED;

    if (entry_pos[0].IsPointIn(tx, ty))
        SetCurrent(term, customer_name);
    else if (entry_pos[1].IsPointIn(tx, ty))
        SetCurrent(term, customer_phone);
    else if (entry_pos[2].IsPointIn(tx, ty))
        SetCurrent(term, customer_comment);
    else
        retval = GetTextDialog::Touch(term, tx, ty);

    return retval;
}

SignalResult OpenTabDialog::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("OpenTabDialog::Signal()");
    SignalResult retval = SIGNAL_OKAY;
    static const char* command[] = { "enter", "backspace", "cancel", "clear", NULL };
    int len;
    int idx = CompareList(message, command);

    switch (idx)
    {
    case 0:  // enter
        if (customer != NULL &&
            term->check != NULL &&
            (customer_name[0] != '\0' ||
             customer_phone[0] != '\0' ||
             customer_comment[0] != '\0'))
        {
            // store the customer information
            customer->FirstName(customer_name);
            customer->PhoneNumber(customer_phone);
            customer->Comment(customer_comment);
            term->check->Save();
            // check->Save() also saves the customer, but let's make sure
            customer->Save();
            strcpy(target_signal, "opentabamount");
            retval = SIGNAL_TERMINATE;
        }
        else
        {
            retval = Signal(term, "cancel");
        }
        break;
    case 1:  // backspace
        if (current != NULL)
        {
            len = strlen(current);
            current[len - 1] = '\0';
        }
        break;
    case 2:  // cancel
        Signal(term, "cancelopentab");
        retval = SIGNAL_TERMINATE;
        break;
    case 3:  // clear
        customer_name[0] = '\0';
        customer_phone[0] = '\0';
        customer_comment[0] = '\0';
        break;
    default:
        if (current != NULL)
        {
            len = strlen(current);
            if (len < max_current)
            {
                current[len++] = (char)message[0];
                current[len] = '\0';
            }
        }
        break;
    }

    RenderEntry(term);
    return retval;
}

SignalResult OpenTabDialog::Keyboard(Terminal *term, int kb_key, int state)
{
    FnTrace("OpenTabDialog::Keyboard()");
    SignalResult retval = SIGNAL_IGNORED;

    switch (kb_key)
    {
    case 8:  // backspace
        retval = Signal(term, "backspace");
        break;
    case 9:  // tab
        if (current == customer_name)
            SetCurrent(term, customer_phone);
        else if (current == customer_phone)
            SetCurrent(term, customer_comment);
        else
            SetCurrent(term, customer_name);
        retval = SIGNAL_OKAY;
        break;
    case 13:  // enter
        retval = Signal(term, "enter");
        break;
    case 27:  // ESC
        retval = Signal(term, "cancel");
        break;
    default:
        retval = GetTextDialog::Keyboard(term, kb_key, state);
        break;
    }

    return retval;
}

int OpenTabDialog::RenderEntry(Terminal *term)
{
    FnTrace("OpenTabDialog::RenderEntry()");
    int retval = 0;
    int old_font = font;

    Flt  entry_width = size_x / 2;
    Flt  entry_x = (size_x - entry_width) / 2;
    Flt  name_pos = 1.5;
    Flt  phone_pos = 3.5;
    Flt  comment_pos = 5.5;
    char buff[STRLENGTH];

    font = entry_font;

    // Render the name entry
    TextL(term, name_pos, "First Name", COLOR_BLACK);
    Entry(term, entry_x, name_pos + 1.0, entry_width, &entry_pos[0]);
    entry_pos[0].y -= 35;
    entry_pos[0].h += 35;
    if (customer_name[0] == '\0')
        buff[0] = '\0';
    else
        strncpy(buff, customer_name, STRLENGTH);
    if (current == customer_name)
        strncat(buff, "_", sizeof(buff) - strlen(buff) - 1);
    TextPosL(term, entry_x + 1, name_pos + 1.0, buff, COLOR_WHITE);
    term->UpdateArea(entry_pos[0].x, entry_pos[0].y, entry_pos[0].w, entry_pos[0].h);

    // Render the phone entry
    TextL(term, phone_pos, "Phone Number", COLOR_BLACK);
    Entry(term, entry_x, phone_pos + 1.0, entry_width, &entry_pos[1]);
    entry_pos[1].y -= 35;
    entry_pos[1].h += 35;
    if (customer_phone[0] == '\0')
        buff[0] = '\0';
    else
        strncpy(buff, customer_phone, STRLENGTH);
    if (current == customer_phone)
        strncat(buff, "_", sizeof(buff) - strlen(buff) - 1);
    TextPosL(term, entry_x + 1, phone_pos + 1.0, buff, COLOR_WHITE);
    term->UpdateArea(entry_pos[1].x, entry_pos[1].y, entry_pos[1].w, entry_pos[1].h);

    // Render the Comment entry
    TextL(term, comment_pos, "Comment", COLOR_BLACK);
    Entry(term, entry_x, comment_pos + 1.0, entry_width, &entry_pos[2]);
    entry_pos[2].y -= 35;
    entry_pos[2].h += 35;
    if (customer_comment[0] == '\0')
        buff[0] = '\0';
    else
        strncpy(buff, customer_comment, STRLENGTH);
    if (current == customer_comment)
        strncat(buff, "_", sizeof(buff) - strlen(buff) - 1);
    TextPosL(term, entry_x + 1, comment_pos + 1.0, buff, COLOR_WHITE);
    term->UpdateArea(entry_pos[2].x, entry_pos[2].y, entry_pos[2].w, entry_pos[2].h);

    last_current = current;
    font = old_font;

    return retval;
}
