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
 * user_edit_zone.hh - revision 29 (10/28/97)
 * Zone for editing users & job security information
 */

#ifndef _USER_EDIT_ZONE_HH
#define _USER_EDIT_ZONE_HH

#include "form_zone.hh"


/**** Types ****/
class Employee;

// Employee Info Viewing/Editing
class UserEditZone : public ListFormZone
{
    int view_active;
    Employee *user;

public:
    // Constructor
    UserEditZone();

    // Member Functions
    int          Type() { return ZONE_USER_EDIT; }
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Signal(Terminal *term, const char *message);
    int          Update(Terminal *term, int update_message, const char *value);
    Flt         *Spacing() { return &list_spacing; }

    int AddStartPages(Terminal *term, FormField *field);
    int LoadRecord(Terminal *term, int record);
    int SaveRecord(Terminal *term, int record, int write_file);
    int NewRecord(Terminal *t);
    int KillRecord(Terminal *term, int record);
    int PrintRecord(Terminal *term, int record);
    int Search(Terminal *term, int record, const char *word);
    int ListReport(Terminal *term, Report *r);
    int RecordCount(Terminal *term);
};

// Job Security settings
class JobSecurityZone : public FormZone
{
    FormField *last_focus;  // Only for "disable verification"
    int columns;
    int DisablingCategory();
    int EmployeeIsUsing(Terminal *term, int active_job);

public:
    // Constructor
    JobSecurityZone();

    // Member Functions
    int          Type() { return ZONE_JOB_SECURITY; }
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Signal(Terminal *term, const char *message);
    SignalResult Touch(Terminal *term, int tx, int ty);
    SignalResult Mouse(Terminal *term, int action, int mx, int my);
    int          LoadRecord(Terminal *term, int record);
    int          SaveRecord(Terminal *term, int record, int write_file);
    int          UpdateForm(Terminal *term, int record);
};

#endif
