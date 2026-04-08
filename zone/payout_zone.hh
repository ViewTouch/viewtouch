/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025, 2026
  
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

#ifndef _PAYOUT_ZONE_HH
#define _PAYOUT_ZONE_HH

#include "layout_zone.hh"

#include <memory>

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
    std::unique_ptr<Report> report;

public:
    // Constructor
    PayoutZone();
    // Destructor
    ~PayoutZone() override;

    // Member Functions
    int          Type() override { return ZONE_PAYOUT; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;

    Flt *Spacing() override { return &spacing; }

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
    int          Type() override { return ZONE_END_DAY; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    int          Update(Terminal *term, int update_message, const genericChar* value) override;
    int          EndOfDay(Terminal *term, int force = 0);
};

#endif
