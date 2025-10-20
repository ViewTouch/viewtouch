/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 2025
  
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
 * report_zone.hh - revision 45 (10/30/97)
 * Definition of report information display zone
 */

#ifndef _REPORT_ZONE_HH
#define _REPORT_ZONE_HH

#include "layout_zone.hh"
#include "report.hh"


/**** Types ****/
class Employee;

// General report viewing zone
class ReportZone : public LayoutZone
{
    Report   *report;
    Report   *temp_report;
    int       lines_shown;
    int       page;
    int       header;
    int       footer;
    TimeInfo  ref;
    int       report_type;
    int       check_disp_num;
    int       video_target;
    int       columns;
    int       print;
    int       printer_dest;
    int       period_view;
    Flt       spacing;
    TimeInfo  day_start;
    TimeInfo  day_end;
    TimeInfo  *period_fiscal;
    int       rzstate;
    int       printing_to_printer;
    int       blink_state;  // for flashing long-waiting orders

public:
    // Constructor
    ReportZone();
    // Destructor
    ~ReportZone();

    // Member Functions
    int          Type() { return ZONE_REPORT; }
    RenderResult Render(Terminal *t, int update_flag);
    int          ZoneStates() { return 2; }
    RenderResult DisplayCheckReport(Terminal *term, Report *sel_report);
    int          IsKitchenCheck(Terminal *term, Check *check);
    int          ShowCheck(Terminal *term, Check *check);
    int          UndoRecentCheck(Terminal *term);
    Check       *NextCheck(Check *check, int sort_order);
    Check       *GetDisplayCheck(Terminal *term);
    Check       *GetCheckByNum(Terminal *term);
    SignalResult Signal(Terminal *t, const genericChar* message);
    SignalResult Touch(Terminal *t, int tx, int ty);
    SignalResult Mouse(Terminal *t, int action, int mx, int my);
    SignalResult ToggleCheckReport(Terminal *term);
    SignalResult Keyboard(Terminal *t, int key, int state);
    int          Update(Terminal *t, int update_message, const genericChar* value);
    int          State(Terminal *term);
    int         *ReportType()        { return &report_type; }
    int         *CheckDisplayNum()   { return &check_disp_num; }
    int         *VideoTarget()       { return &video_target; }
    int         *ReportPrint()       { return &print; }
    Flt         *Spacing()           { return &spacing; }
    int         *Columns()           { return &columns; }
    int          Page(int new_page)  { int old_page = page; page = new_page; return old_page; }
    int          BlinkState()       { return blink_state; }

    int Print(Terminal *t, int print_mode);
    SignalResult QuickBooksExport(Terminal *term);

private:
    int last_page_touch = -1;
    int last_selected_y_touch = -10000;
};

// Textfile viewing zone
class ReadZone : public LayoutZone
{
    Report report;
    Str    filename;
    int    page;
    int    loaded;

public:
    // Constructor
    ReadZone();

    // Member Functions
    int          Type() { return ZONE_READ; }
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Signal(Terminal *t, const genericChar* message);
    SignalResult Touch(Terminal *t, int tx, int ty);
    SignalResult Keyboard(Terminal *t, int key, int state);

    Str *FileName() { return &filename; }
};

#endif
