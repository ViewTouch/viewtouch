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
 * payout_zone.hh - revision 13 (7/2/97)
 * Definition of captured tip payouts
 */

#pragma once

#include "layout_zone.hh"


/**** Types ****/
class Archive;
class Report;
class TipEntry;
class TipDB;

class PayoutZone : public LayoutZone
{
    int      selected;
    int      payout;
    int      user_id;
    int      amount;
    int      page;
    Archive *archive;
    TipDB   *tip_db;
    Flt      spacing;
    Report  *report;

public:
    // Constructor
    PayoutZone();
    // Destructor
    ~PayoutZone();

    // Member Functions
    int          Type() { return ZONE_PAYOUT; }
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Signal(Terminal *term, const genericChar* message);
    SignalResult Touch(Terminal *term, int tx, int ty);

    Flt *Spacing() { return &spacing; }

    int PayoutTips(Terminal *term);
    int Print(Terminal *term, int mode);
};

class EndDayZone : public LayoutZone
{
    short flag1;
    short flag2;
    short flag3;
    short flag4;
    short flag5;

public:
    // Constructor
    EndDayZone();

    // Member Functions
    int          Type() { return ZONE_END_DAY; }
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Signal(Terminal *term, const genericChar* message);
    int          Update(Terminal *term, int update_message, const genericChar* value);
    int          EndOfDay(Terminal *term, int force = 0);
};
