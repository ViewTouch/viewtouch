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
 * check_list_zone.hh - revision 26 (8/21/97)
 * Touch zone for managing current checks
 */

#ifndef _CHECK_LIST_ZONE_HH
#define _CHECK_LIST_ZONE_HH

#include "form_zone.hh"
#include "layout_zone.hh"


/**** Types ****/
class Archive;
class Check;
class Employee;

class CheckListZone : public LayoutZone
{
    Check *check_array[32];
    int    array_size;
    int    array_max_size;
    int    possible_size;
    int    status;
    int    page_no;
    int    max_pages;
    Flt    spacing;

public:
    // Constructor
    CheckListZone();

    // Member Functions
    int          Type() { return ZONE_CHECK_LIST; }
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Signal(Terminal *term, const genericChar *message);
    SignalResult Touch(Terminal *term, int tx, int ty);
    int          Update(Terminal *term, int update_message, const genericChar *value);
    Flt         *Spacing() { return &spacing; }

    int  MakeList(Terminal *term);
    int  Search(Terminal *term, const genericChar *name, Employee *start);
};

class CheckEditZone : public FormZone
{
    Flt list_header;
    Flt list_footer;
    Flt list_spacing;
    int lines_shown;
    int page;
    int my_update;
    Check *check;
    Report *report;
    int view;

public:
    CheckEditZone();
    ~CheckEditZone();

    int          Type() { return ZONE_CHECK_EDIT; }
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Keyboard(Terminal *term, int my_key, int state);
    SignalResult Touch(Terminal *term, int tx, int ty);
    SignalResult Mouse(Terminal *term, int action, int mx, int my);
    SignalResult Signal(Terminal *term, const genericChar *message);
    int          LoseFocus(Terminal *term, Zone *newfocus);

    Flt *Spacing() { return &list_spacing; }

    int LoadRecord(Terminal *term, int record);
    int SaveRecord(Terminal *term, int record, int write_file);
    int Search(Terminal *term, int record, const genericChar *word);
    int ListReport(Terminal *term, Report *report);
    int RecordCount(Terminal *term);
};

#endif
