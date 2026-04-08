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
 * expense_zone.hh
 * Functionality for paying expenses from revenue.
 */

#ifndef _EXPENSE_ZONE_HH
#define _EXPENSE_ZONE_HH

#include "form_zone.hh"
#include "expense.hh"

#include <memory>

#define EXPENSE_ZONE_COLUMNS 5

class ExpenseZone : public FormZone
{
    Expense *expense;
    Expense *saved_expense;
    int show_expense;  // whether to display the edit form with expense
    Flt list_header;
    Flt list_footer;
    Flt list_spacing;
    int lines_shown;
    std::unique_ptr<Report> report;
    int page;
    int allow_balanced;
public:
    ExpenseZone();
    ~ExpenseZone() override;

    int          Type() override { return ZONE_EXPENSE; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          Update(Terminal *term, int update_message, const genericChar* value) override
                    { return FormZone::Update( term, update_message, value); }
    int          UpdateForm(Terminal *term, int record) override;
    int          HideFields();
    int          ShowFields();

    Flt *Spacing() override { return &list_spacing; }

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
