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
 * hardware_zone.hh - revision 1 (8/27/98)
 * Hardware settings (taken from settings_zone.hh)
 */

#pragma once

#include "form_zone.hh"


/**** Types ****/
class HardwareZone : public ListFormZone
{
    FormField *term_start;
    FormField *printer_start;
    FormField *display_host_field;
    FormField *printer_host_field;
    FormField *kitchen_mode_field;
    FormField *drawer_pulse_field;
    int        page;
    int        section;

public:
    // Constructor
    HardwareZone();

    // Member Functions
    int          Type() { return ZONE_HARDWARE; }
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Signal(Terminal *t, const genericChar* message);
    Flt         *Spacing() { return &list_spacing; }

    int UpdateForm(Terminal *term, int record);
    int LoadRecord(Terminal *t, int record);
    int SaveRecord(Terminal *t, int record, int write_file);
    int NewRecord(Terminal *t);
    int KillRecord(Terminal *t, int record);
    int ListReport(Terminal *t, Report *r);
    int RecordCount(Terminal *t);
    int ChangeSection(Terminal *t, int no);
    int ChangeStatus(Terminal *t);
    int Calibrate(Terminal *t);
    Printer *FindPrinter(Terminal *t);
};
