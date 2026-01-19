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
 * creditcard_list_zone.hh
 * Touch zone for managing credit cards
 */

#ifndef CREDITCARD_LIST_ZONE_HH
#define CREDITCARD_LIST_ZONE_HH

#include "form_zone.hh"
#include "credit.hh"

#include <memory>

class CreditCardListZone : public ListFormZone
{
    Flt       list_header;
    Flt       list_footer;
    Flt       list_spacing;
    int       lines_shown;
    std::unique_ptr<Report> report;
    Credit   *credit;
    CreditDB *creditdb;
    Archive  *archive;
    int       page;
    int       mode;

public:
    CreditCardListZone();
    ~CreditCardListZone() override;

    int          Type() override { return ZONE_CREDITCARD_LIST; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;

    Flt *Spacing() override { return &list_spacing; }

    int LoadRecord(Terminal *term, int record) override;
    int SaveRecord(Terminal *term, int record, int write_file) override;
    int RestoreRecord(Terminal *term);
    int NewRecord(Terminal *term) override;
    int KillRecord(Terminal *term, int record) override;
    int PrintRecord(Terminal *term, int record) override;
    int Search(Terminal *term, int record, const genericChar* word) override;
    int ListReport(Terminal *term, Report *report) override;
    int RecordCount(Terminal *term) override;
    CreditDB *GetDB(int in_system = 1);
    CreditDB *NextCreditDB(Terminal *term);
    CreditDB *PrevCreditDB(Terminal *term);
};

#endif
