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
 * cdu_zone.hh
 * For Customer Display Units, we need to allow for various configuration details.
 * May allow for multiple advertisements, different text display effects, et al.
 */

#ifndef _CDU_ZONE_HH
#define _CDU_ZONE_HH

#include "cdu.hh"
#include "form_zone.hh"
#include "utility.hh"

#define CDU_ZONE_COLUMNS  2

class CDUZone : public FormZone
{
    Flt list_header;
    Flt list_footer;
    Flt list_spacing;
    int lines_shown;
    Report *report;
    int page;
    int show_item;
    CDUString *cdustring;
    CDUString *saved_cdustring;
public:
    CDUZone();
    ~CDUZone();

    int          Type() { return ZONE_CDU; }
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Signal(Terminal *term, const genericChar *message);
    SignalResult Touch(Terminal *term, int tx, int ty);
    int          Update(Terminal *term, int update_message, const genericChar *value)
                    { return FormZone::Update( term, update_message, value); }
    int          UpdateForm(Terminal *term, int record);
    int          HideFields();
    int          ShowFields();

    Flt *Spacing() { return &list_spacing; }

    int ColumnSpacing(Terminal *term, int num_columns);
    int LoadRecord(Terminal *term, int record);
    int SaveRecord(Terminal *term, int record, int write_file);
    int RestoreRecord(Terminal *term);
    int NewRecord(Terminal *term);
    int KillRecord(Terminal *term, int record);
    int PrintRecord(Terminal *term, int record);
    int Search(Terminal *term, int record, const genericChar *word);
    int ListReport(Terminal *term, Report *report);
    int RecordCount(Terminal *term);
};

#endif
