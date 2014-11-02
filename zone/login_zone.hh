/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997  
  
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
 * login_zone.hh - revision 39 (10/3/97)
 * touch zone object for employees to identify themselves
 */

#ifndef _LOGIN_ZONE_HH
#define _LOGIN_ZONE_HH

#include "layout_zone.hh"

/**** Types ****/
class WorkEntry;

class LoginZone : public LayoutZone
{
    int      state;
    int      input;
    TimeInfo time;
    int      clocking_on;

public:
    // Constructor
    LoginZone();

    // Member Functions
    int          Type() { return ZONE_LOGIN; }
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Signal(Terminal *term, genericChar *message);
    SignalResult Keyboard(Terminal *term, int key, int state);
    int          Update(Terminal *term, int update_message, genericChar *value);

    int ClockOn(Terminal *term, int job = -1);
    int ClockOff(Terminal *term);
    int Enter(Terminal *term);
    int Start(Terminal *term, short expedite = 0);
};

class LogoutZone : public LayoutZone
{
    TimeInfo   time_out;
    WorkEntry *work;

public:
    // Constructor
    LogoutZone();

    // Member Functions
    int          Type() { return ZONE_LOGOUT; }
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Signal(Terminal *term, genericChar *message);
    SignalResult Keyboard(Terminal *term, int key, int state);

    int RenderPaymentEntry(Terminal *term, int line);
    int DrawPaymentEntry(Terminal *term, int line);
    int Input(Terminal *term, int val);
    int ClockOff(Terminal *term, int end_shift);
};

#endif
