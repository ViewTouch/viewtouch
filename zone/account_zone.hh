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
 * account_zone.hh - revision 4 (1/6/98)
 * Chart of Accounts entry/edit/list module
 */

#ifndef _ACCOUNT_ZONE_HH
#define _ACCOUNT_ZONE_HH

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
    int          Type() { return ZONE_ACCOUNT; }
    virtual RenderResult Render(Terminal *term, int update_flag);
    virtual SignalResult Signal(Terminal *term, genericChar *message);
    virtual SignalResult Keyboard(Terminal *term, int key, int state);
    virtual SignalResult Touch(Terminal *term, int tx, int ty);
    virtual SignalResult Mouse(Terminal *term, int action, int mx, int my);

    int LoadRecord(Terminal *term, int record_no);
    int SaveRecord(Terminal *term, int record_no, int write_file);
    int NewRecord(Terminal *term);
    int KillRecord(Terminal *term, int record);
    int PrintRecord(Terminal *term, int record);
    int Search(Terminal *term, int record, char *word);
    int ListReport(Terminal *term, Report *report);
    int RecordCount(Terminal *term);
    int CheckAccountNumber(Terminal *term, int sendmsg = 1);
};

#endif
