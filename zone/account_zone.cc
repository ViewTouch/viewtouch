/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025
  
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
 * account_zone.cc - revision 5 (8/13/98)
 * Chart of Accounts entry/edit/list module implementation
 */

#include "account_zone.hh"
#include "system.hh"
#include "account.hh"
#include "report.hh"

#include <iostream>
#include "safe_string_utils.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** AccountZone Class ****/
// Constructor
AccountZone::AccountZone()
{
    list_header = 2;
    //list_footer = 1;
    account = NULL;
    show_list = 1;
    edit_default = 0;
    AddTextField(GlobalTranslate("Account Name"), 15);
    AddTextField(GlobalTranslate("Account No"), 5);
    SetFlag(FF_ONLYDIGITS);
    acctnumfld = FieldListEnd();
    AddTextField(GlobalTranslate("Balance"), 12);
    SetFlag(FF_MONEY);
}

// Member Functions
RenderResult AccountZone::Render(Terminal *term, int update_flag)
{
    FnTrace("AccountZone::Render()");
    genericChar buff[STRLENGTH];
    int col = COLOR_DEFAULT;
    int indent = 0;
    num_spaces = ColumnSpacing(term, ACCOUNT_ZONE_COLUMNS);
        genericChar str[STRLENGTH];

    ListFormZone::Render(term, update_flag);
    if (show_list)
    {
        TextC(term, 0, name.Value());
        indent = 0;
        TextPosL(term, indent, 1.3, "No.");
        indent += num_spaces;
        TextPosL(term, indent, 1.3, GlobalTranslate("Name"));
        indent += num_spaces;
        TextPosL(term, indent, 1.3, GlobalTranslate("Balance"));

        vt_safe_string::safe_format(str, STRLENGTH, "%s: %d", term->Translate("Total Accounts Active"),
                term->system_data->account_db.AccountCount());
        TextC(term, size_y - 1, str);
    }
    else
    {
        int my_records = term->system_data->account_db.AccountCount();
        if (account)
            snprintf(buff, STRLENGTH, "Account %d of %d", record_no + 1, my_records);
        else
            snprintf(buff, STRLENGTH, "%s", GlobalTranslate("No Accounts"));
        TextC(term, 0, buff, col);
    }
    return RENDER_OKAY;
}

SignalResult AccountZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("AccountZone::Signal()");
    SignalResult retval;

    //Need to check for valid account numbers, but if the account number
    //is invalid, that generates a signal and so we end up in a crashed
    //loop here.  So we only check the account number when processing
    //a signal that we handled (status messages are ignored by this zone)
    retval = ListFormZone::Signal(term, message);
    if (retval == SIGNAL_OKAY)
        CheckAccountNumber(term);
    return retval;
}

SignalResult AccountZone::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("AccountZone::Keyboard()");
    SignalResult retval;
    retval = ListFormZone::Keyboard(term, my_key, state);
    CheckAccountNumber(term);
    return retval;
}

SignalResult AccountZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("AccountZone::Touch()");
    SignalResult retval;
    retval = ListFormZone::Touch(term, tx, ty);
    CheckAccountNumber(term);
    return retval;
}

SignalResult AccountZone::Mouse(Terminal *term, int action, int mx, int my)
{
    FnTrace("AccountZone::Mouse()");
    SignalResult retval;
    retval = ListFormZone::Mouse(term, action, mx, my);
    CheckAccountNumber(term);
    return retval;
}

int AccountZone::LoadRecord(Terminal *term, int my_record_no)
{
    FnTrace("AccountZone::LoadRecord()");
    FormField *field = FieldList();
    account = term->system_data->account_db.FindByRecord(my_record_no);

    if (account != NULL)
    {
        // AddTextField("Account Name", 15, 0);
        field->Set(account->name.Value());
        field = field->next;
        // AddTextField("Account No", 5, 0);
        field->Set(account->number);
        field = field->next;
        // AddTextField("Balance", 7, 0);
        field->Set(account->balance);
        return 0;
    }
    return 1;
}

int AccountZone::SaveRecord(Terminal *term, int my_record_no, int write_file)
{
    int acct_no = 0;

    FnTrace("AccountZone::SaveRecord()");
    (void)my_record_no;
    if (account != NULL)
    {
        FormField *field = FieldList();
        field->Get(account->name);  field = field->next;
        field->Get(account->number);  field = field->next;
        field->Get(account->balance);  field = field->next;
        // there's a sorting process that happens during saves to keep everything
        // organized by account number.  So we'll save the current record's account
        // number and then get the record number of that after the save.  This
        // makes sure the currently selected account will continue to be selected.
        acct_no = account->number;
        term->system_data->account_db.Save(account);
        my_record_no = term->system_data->account_db.FindRecordByNumber(acct_no);
    }
    return 0;
}

int AccountZone::NewRecord(Terminal *term)
{
    FnTrace("AccountZone::NewRecord()");
    int acct_num = 0;

    if (account != NULL)
        acct_num = account->number;
    account = term->system_data->account_db.NewAccount(acct_num);
    records = RecordCount(term);
    record_no = term->system_data->account_db.FindRecordByNumber(account->number);
    return 0;
}

int AccountZone::KillRecord(Terminal *term, int record)
{
    FnTrace("AccountZone::KillRecord()");
    Account *remacct = term->system_data->account_db.FindByRecord(record);

    term->system_data->account_db.Remove(remacct);
    records = RecordCount(term);
    if (record_no > records)
        record_no = records - 1;
    return 0;
}

int AccountZone::PrintRecord(Terminal *term, int record)
{
    FnTrace("AccountZone::PrintRecord()");

    return 1;
}

int AccountZone::Search(Terminal *term, int record, const genericChar* word)
{
    FnTrace("AccountZone::Search()");
    AccountDB *acct_db = &(term->system_data->account_db);
    int srecord;
    int retval = 0;

    srecord = acct_db->FindRecordByWord(word, record);
    if (srecord > -1)
    {
        record_no = srecord;
        retval = 1;
    }
    return retval;
}

int AccountZone::ListReport(Terminal *term, Report *report)
{
    FnTrace("AccountZone::ListReport()");
    genericChar buff[STRLENGTH];
    num_spaces = ColumnSpacing(term, ACCOUNT_ZONE_COLUMNS);
    int indent = 0;
    int my_color = COLOR_DEFAULT;

    if (report == NULL)
        return 1;

    AccountDB *account_db = &(term->system_data->account_db);
    report->Clear();
    Account *acct = account_db->Next();
    if (acct == NULL)
        report->TextC(GlobalTranslate("No Accounts Defined"));
    while (acct != NULL)
    {
        indent = 0;
        snprintf(buff, STRLENGTH, "%d", acct->number);
        report->TextPosL(indent, buff, my_color);
        indent += num_spaces;
        report->TextPosL(indent, acct->name.Value(), my_color);
        indent += num_spaces;
        report->TextPosL(indent, term->FormatPrice(acct->balance), my_color);
        report->NewLine();
        acct = account_db->Next();
    }
    return 0;
}



int AccountZone::RecordCount(Terminal *term)
{
    FnTrace("AccountZone::RecordCount()");
    return term->system_data->account_db.AccountCount();
}

/****
 * CheckAccountNumber:  check for invalid account number and
 *   report error status if necessary
 * Returns 1 if account number is valid, 0 otherwise
 ****/
int AccountZone::CheckAccountNumber(Terminal *term, int sendmsg)
{
    int retval = 1;
    int number;
    genericChar msgbad[] = "status Account Number is out of range";
    genericChar msggood[] = "clearstatus";
    const genericChar* msgsend = msggood;

    if (account != NULL)
    {
        acctnumfld->Get(number);
        if (! IsValidAccountNumber(term, number))
        {
            msgsend = msgbad;
            retval = 0;
        }
    }
    if (sendmsg)
        term->Signal(msgsend, group_id);
    return retval;
}
