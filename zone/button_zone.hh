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
 * button_zone.hh - revision 51 (2/3/98)
 * Standard push button zone objects
 */

#ifndef _BUTTON_ZONE_HH
#define _BUTTON_ZONE_HH

#include "pos_zone.hh"
#include "layout_zone.hh"


/**** Types ****/
class ButtonZone : public PosZone
{
public:
    int jump_type, jump_id;

    // Constructor
    ButtonZone();

    // Member Functions
    virtual int          Type() { return ZONE_SIMPLE; }
    virtual int          AcceptSignals() { return 0; }
    virtual Zone        *Copy();
    virtual SignalResult Touch(Terminal *term, int tx, int ty);
    virtual int          GainFocus(Terminal *term, Zone *oldfocus) { return 0; }

    int *JumpType() { return &jump_type; }
    int *JumpID()   { return &jump_id;   }
};

class MessageButtonZone : public ButtonZone
{
    SignalResult SendandJump(Terminal *term);

public:
    Str message;
    int confirm;
    Str confirm_msg;

    // Member Functions
    MessageButtonZone();
    virtual int          Type() { return ZONE_STANDARD; }
    virtual int          AcceptSignals() { return 1; }
    virtual Zone        *Copy();
    virtual SignalResult Touch(Terminal *term, int tx, int ty);
    virtual SignalResult Signal(Terminal *term, char *signal_msg);
    virtual char        *ValidateCommand(char *command);

    Str *Message()    { return &message; }
    int *Confirm()    { return &confirm; }
    Str *ConfirmMsg() { return &confirm_msg; }
};

class ConditionalZone : public MessageButtonZone
{
    Str expression;
    int keyword, op, val;

public:
    // Constructor
    ConditionalZone();

    // Member Functions
    int          Type() { return ZONE_CONDITIONAL; }
    Zone        *Copy();
    int          RenderInit(Terminal *term, int update_flag);
    SignalResult Touch(Terminal *term, int tx, int ty);
    int          Update(Terminal *term, int update_message, genericChar *value);

    Str *Expression() { return &expression; }
    int  ZoneStates() { return 3; }

    int EvalExp(Terminal *term);
};

class ToggleZone : public PosZone
{
    Str message;
    int state;
    int max_states;

public:
    // Constructor
    ToggleZone();

    // Member Functions
    int          Type() { return ZONE_TOGGLE; }
    virtual int  AcceptSignals() { return 0; }
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Touch(Terminal *term, int tx, int ty);
    genericChar *TranslateString(Terminal *term);
    virtual int  GainFocus(Terminal *term, Zone *oldfocus) { return 0; }

    Str *Message() { return &message; }
};

class CommentZone : public PosZone
{
public:
    // Member Functions
    int          Type() { return ZONE_COMMENT; }
    virtual int  AcceptSignals() { return 0; }
    int          RenderInit(Terminal *term, int update_flag);
    int          ZoneStates() { return 1; }
    virtual int  GainFocus(Terminal *term, Zone *oldfocus) { return 0; }
};

class KillSystemZone : public PosZone
{
public:
    // Member Functions
    int          Type() { return ZONE_KILL_SYSTEM; }
    virtual int  AcceptSignals() { return 0; }
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Touch(Terminal *term, int tx, int ty);
    int          Update(Terminal *term, int update_message, genericChar *value);
};

class StatusZone : public LayoutZone
{
    Str status;
public:
    StatusZone();
    int          Type() { return ZONE_STATUS_BUTTON; }
    virtual int  AcceptSignals() { return 1; }
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Signal(Terminal *term, genericChar *message);
    virtual int  GainFocus(Terminal *term, Zone *oldfocus) { return 0; }
};

#endif
