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
 * cdu_zone.hh
 * For Customer Display Units, we need to allow for various configuration details.
 * May allow for multiple advertisements, different text display effects, et al.
 */

#ifndef _CDU_ZONE_HH
#define _CDU_ZONE_HH

#include "cdu.hh"
#include "form_zone.hh"
#include "utility.hh"

#include <memory>

#define CDU_ZONE_COLUMNS  2

class CDUZone : public FormZone
{
    Flt list_header;
    Flt list_footer;
    Flt list_spacing;
    int lines_shown;
    std::unique_ptr<Report> report;
    int page;
    int show_item;
    CDUString *cdustring;
    CDUString *saved_cdustring;
public:
    CDUZone();
    ~CDUZone() override;

    int          Type() override { return ZONE_CDU; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          Update(Terminal *term, int update_message, const genericChar* value) override
                    { return FormZone::Update( term, update_message, value); }
    int          UpdateForm(Terminal *term, int record) override;
    int          HideFields();
    int          ShowFields();

    Flt *Spacing() override { return &list_spacing; }

    int ColumnSpacing(Terminal *term, int num_columns);
    int LoadRecord(Terminal *term, int record) override;
    int SaveRecord(Terminal *term, int record, int write_file) override;
    int RestoreRecord(Terminal *term);
    int NewRecord(Terminal *term) override;
    int KillRecord(Terminal *term, int record) override;
    int PrintRecord(Terminal *term, int record) override;
    int Search(Terminal *term, int record, const genericChar* word) override;
    int ListReport(Terminal *term, Report *report);
    int RecordCount(Terminal *term) override;
};

#endif
