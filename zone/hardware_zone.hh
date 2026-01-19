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
 * hardware_zone.hh - revision 1 (8/27/98)
 * Hardware settings (taken from settings_zone.hh)
 */

#ifndef _HARDWARE_ZONE_HH
#define _HARDWARE_ZONE_HH

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
    int          Type() override { return ZONE_HARDWARE; }
    RenderResult Render(Terminal *t, int update_flag) override;
    SignalResult Signal(Terminal *t, const genericChar* message) override;
    Flt         *Spacing() override { return &list_spacing; }

    int UpdateForm(Terminal *term, int record) override;
    int LoadRecord(Terminal *t, int record) override;
    int SaveRecord(Terminal *t, int record, int write_file) override;
    int NewRecord(Terminal *t) override;
    int KillRecord(Terminal *t, int record) override;
    int ListReport(Terminal *t, Report *r) override;
    int RecordCount(Terminal *t) override;
    int ChangeSection(Terminal *t, int no);
    int ChangeStatus(Terminal *t);
    int Calibrate(Terminal *t);
    Printer *FindPrinter(Terminal *t);
    SignalResult Touch(Terminal *t, int tx, int ty) override;
};

#endif
