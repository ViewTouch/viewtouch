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
 * labor_zone.hh - revision 13 (1/15/98)
 * Labor Cost/Hours Report/Editor
 */

#ifndef _LABOR_ZONE_HH
#define LABOR_ZONE_HH

#include "form_zone.hh"


/**** Types ****/
class Employee;
class Report;
class LaborPeriod;
class WorkEntry;

// Employee timeclock viewing/editing
class LaborZone : public FormZone
{
    Report      *report;
    LaborPeriod *period;
    WorkEntry   *work;
    int          page;
    TimeInfo     start;
    TimeInfo     end;
    TimeInfo     ref;
    int          day_view;
    Flt          spacing;

public:
    // Constructor
    LaborZone();
    // Destructor
    ~LaborZone() override;

    // Member Functions
    int          Type() override { return ZONE_LABOR; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          Update(Terminal *term, int update_message, const genericChar* value) override;
    int          LoadRecord(Terminal *term, int record) override;
    int          SaveRecord(Terminal *term, int record, int write_file) override;
    int          UpdateForm(Terminal *term, int record) override;
    Flt         *Spacing() override { return &spacing; }

    int Print(Terminal *term, int mode);
};

// Employee labor scheduling
class ScheduleZone : public PosZone
{
    int name_len;

public:
    // Constructor
    ScheduleZone();

    // Member Functions
    int          Type() override { return ZONE_SCHEDULE; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Mouse(Terminal *term, int action, int mx, int my) override;
    int ZoneStates() override { return 1; }
};

#endif
