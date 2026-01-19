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
 * account_zone.hh - revision 4 (1/6/98)
 * Chart of Accounts entry/edit/list module
 */

#ifndef _ACCOUNT_ZONE_HH
#define ACCOUNT_ZONE_HH

#include "form_zone.hh"

#define ACCOUNT_ZONE_COLUMNS 3
/**** Types ****/
class Account;

class AccountZone : public ListFormZone
{
    FormField *acctnumfld;
    Account *account;
public:
    int edit_default;

    // Constructor
    AccountZone();

    // Member Functions
    int          Type() override { return ZONE_ACCOUNT; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Keyboard(Terminal *term, int key, int state) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Mouse(Terminal *term, int action, int mx, int my) override;

    int LoadRecord(Terminal *term, int record_no) override;
    int SaveRecord(Terminal *term, int record_no, int write_file) override;
    int NewRecord(Terminal *term) override;
    int KillRecord(Terminal *term, int record) override;
    int PrintRecord(Terminal *term, int record) override;
    int Search(Terminal *term, int record, const char* word) override;
    int ListReport(Terminal *term, Report *report) override;
    int RecordCount(Terminal *term) override;
    int CheckAccountNumber(Terminal *term, int sendmsg = 1);
};

#endif
