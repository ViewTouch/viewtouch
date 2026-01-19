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
 * user_edit_zone.hh - revision 29 (10/28/97)
 * Zone for editing users & job security information
 */

#ifndef _USER_EDIT_ZONE_HH
#define USER_EDIT_ZONE_HH

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
    int          Type() override { return ZONE_USER_EDIT; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const char* message) override;
    int          Update(Terminal *term, int update_message, const char* value) override;
    Flt         *Spacing() override { return &list_spacing; }

    int AddStartPages(Terminal *term, FormField *field);
    int LoadRecord(Terminal *term, int record) override;
    int SaveRecord(Terminal *term, int record, int write_file) override;
    int NewRecord(Terminal *t) override;
    int KillRecord(Terminal *term, int record) override;
    int PrintRecord(Terminal *term, int record) override;
    int Search(Terminal *term, int record, const char* word) override;
    int ListReport(Terminal *term, Report *r) override;
    int RecordCount(Terminal *term) override;
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
    int          Type() override { return ZONE_JOB_SECURITY; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const char* message) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Mouse(Terminal *term, int action, int mx, int my) override;
    int          LoadRecord(Terminal *term, int record) override;
    int          SaveRecord(Terminal *term, int record, int write_file) override;
    int          UpdateForm(Terminal *term, int record) override;
};

#endif
