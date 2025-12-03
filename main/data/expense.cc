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
 * expense.cc
 * Expense tracking and reporting.
 */

#include "data_file.hh"
#include "expense.hh"
#include "system.hh"
#include "expense_zone.hh"
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int compareString(const char* big, const genericChar* little)
{
    FnTrace("compareString()");
    int lenbig = strlen(big);
    int lenlit = strlen(little);

    if (lenlit > lenbig)
        return 0;
    if (NULL != strstr(big, little))
        return 1;
    else
        return 0;
}


/*********************************************************************
 * The Expense class
 ********************************************************************/

Expense::Expense()
{
    eid = 0;
    account_id = 0;
    tax_account_id = 0;
    employee_id = 0;
    drawer_id = 0;
    amount = 0;
    tax = 0;
    entered = 0;
    dest_account_id = 0;
    exp_date.Set();
    document.Set("");
    explanation.Set("");
    flags = 0;
}

Expense::Expense(int &no)
{
    eid = no;
    account_id = 0;
    tax_account_id = 0;
    employee_id = 0;
    drawer_id = 0;
    amount = 0;
    tax = 0;
    entered = 0;
    dest_account_id = 0;
    exp_date.Set();
    document.Set("");
    explanation.Set("");
    flags = 0;
}

int Expense::SetFileName(char* buffer, int maxlen, const genericChar* path)
{
    FnTrace("Expense::SetFileName()");
    snprintf(buffer, maxlen, "%s/expense_%d", path, eid);
    return 0;
}

int Expense::Read(InputDataFile &infile, int version)
{
    FnTrace("Expense::Read()");
    infile.Read(eid);
    infile.Read(account_id);
    infile.Read(employee_id);
    infile.Read(drawer_id);
    infile.Read(amount);
    infile.Read(document);
    if (version >= 2)
        infile.Read(explanation);
    if (version >= 3)
    {
        infile.Read(tax);
        infile.Read(tax_account_id);
    }
    if (version >= 5)
        infile.Read(flags);
    if (version >= 6)
        infile.Read(dest_account_id);
    if (version >= 7)
        infile.Read(entered);
    if (version >= 8)
        infile.Read(exp_date);
    return 0;
}

int Expense::Load(const char* path)
{
    FnTrace("Expense::Load()");
    InputDataFile infile;
    int result;
    int version = 0;

    result = infile.Open(path, version);
    if (result)
        return 1;
    Read(infile, version);
    infile.Close();
    return 0;
}

int Expense::Write(OutputDataFile &outfile, int version)
{
    FnTrace("Expense::Write()");
    outfile.Write(eid);
    outfile.Write(account_id);
    outfile.Write(employee_id);
    outfile.Write(drawer_id);
    outfile.Write(amount);
    outfile.Write(document);
    outfile.Write(explanation);
    outfile.Write(tax);
    outfile.Write(tax_account_id);
    outfile.Write(flags);
    outfile.Write(dest_account_id);
    outfile.Write(entered);
    outfile.Write(exp_date);
    return 0;
}

int Expense::Save(const char* path)
{
    FnTrace("Expense::Save()");
    genericChar filename[STRLENGTH];
    OutputDataFile outfile;
    int result;

    if (IsBlank())
    {
        return 1;  // don't save empty records
    }
    SetFileName(filename, STRLENGTH, path);
    result = outfile.Open(filename, EXPENSE_VERSION, 0);
    if (result)
        return 1;
    Write(outfile, EXPENSE_VERSION);
    outfile.Close();
    return 0;
}

int Expense::IsBlank()
{
    FnTrace("Expense::IsBlank()");
    int retval = 0;

    if (amount == 0)
        retval = 1;

    return retval;
}

//NOTE BAK-->For these functions, the length of the string is assumed to
// be at least STRLENGTH (normally 256).
//FIX BAK-->Should have a length parameter to these functions
int Expense::Author( Terminal* term, genericChar* employee_name )
{
    FnTrace("Expense::Author()");
    if (term == NULL || term->system_data == NULL || employee_name == NULL)
        return 1;
    UserDB *employee_db = &(term->system_data->user_db);
    Employee *employee;

    employee = employee_db->FindByID(employee_id);
    if (employee)
        snprintf(employee_name, STRLENGTH, "%s", employee->system_name.Value());
    else
        snprintf(employee_name, STRLENGTH, "Unknown");
    return 0;
}

int Expense::DrawerOwner(Terminal *term, genericChar* drawer_name, Archive *archive)
{
    FnTrace("Expense::DrawerOwner(term)");
    if (term == NULL || drawer_name == NULL)
        return 1;
    UserDB *employee_db = &(term->system_data->user_db);
    Drawer *drawerlist;
    if (archive != NULL)
        drawerlist = archive->DrawerList();
    else
        drawerlist = term->system_data->DrawerList();
    Drawer *drawer = NULL;
    Employee *drawer_owner = NULL;

    drawer_name[0] = '\0';
    drawer = drawerlist->FindBySerial(drawer_id);
    if (drawer != NULL)
    {
        drawer_owner = employee_db->FindByID(drawer->owner_id);
        if (drawer_owner)
            snprintf(drawer_name, STRLENGTH, "%s", drawer_owner->system_name.Value());
        else
            snprintf(drawer_name, STRLENGTH, "%s %d", term->Translate("Drawer"), drawer->number);
    }
    // last resort
    if (drawer_name[0] == '\0' && drawer_id >= 0)
    {
        snprintf(drawer_name, STRLENGTH, "Drawer %d", drawer_id);
        return 1;
    }
    return 0;
}

/****
 * AccountName:
 *   //NOTE BAK-->archive can be passed in, but it is not currently used.  The accounts
 *   are stored globally, but they should be moved into the archives so that when
 *   accounts are modified they don't affect the historical accuracy.  When accounts
 *   are moved into the archvies, the archive argument here will become relevant.
 ****/
int Expense::AccountName(Terminal *term, genericChar* account_name, Archive *archive)
{
    FnTrace("Expense::AccountName()");
    if (term == NULL || account_name == NULL)
        return 1;
    AccountDB *acct_db = &(term->system_data->account_db);
    Account *account = NULL;
    
    account = acct_db->FindByNumber(account_id);
    if (account != NULL)
        snprintf(account_name, STRLENGTH, "%s", account->name.Value());
    else
        account_name[0] = '\0';
    return 0;
}

int Expense::IsTraining()
{
    return (flags & EF_TRAINING);
}

int Expense::SetFlag(int flagval)
{
    flags = flags | flagval;
    return flagval;
}

int Expense::Copy(Expense *original)
{
    FnTrace("Expense::Copy()");
    eid = original->eid;
    account_id = original->account_id;
    tax_account_id = original->tax_account_id;
    employee_id = original->employee_id;
    drawer_id = original->drawer_id;
    amount = original->amount;
    tax = original->tax;
    entered = original->entered;
    dest_account_id = original->dest_account_id;
    exp_date.Set(original->exp_date);
    document.Set(original->document.Value());
    explanation.Set(original->explanation.Value());
    flags = 0;
    return 0;
}

int my_atoi(const genericChar* source)
{
    FnTrace("my_atoi");
    int retval = 0;
    int done = 0;
    const genericChar* curr = source;

    while (done == 0 && *curr != '\0')
    {
        if (isdigit(*curr))
        {
            retval *= 10;
            retval += (*curr - 48);
        }
        else if (*curr != '.')
        {
            done = 1;
        }
        if (done == 0)
            ++curr;
    }
    return retval;
}

int Expense::WordMatch(Terminal *term, const genericChar* word)
{
    FnTrace("Expense::WordMatch()");
    genericChar employee_name[STRLONG];
    genericChar drawer_name[STRLONG];
    genericChar account_name[STRLONG];
    int retval = 0;
    int numeric = my_atoi(word);

    // get the author and drawer names
    Author(term, employee_name);
    DrawerOwner(term, drawer_name);
    AccountName(term, account_name);

    if (numeric)
    {
        if (eid == numeric)
            retval = 1;
        else if (account_id == numeric)
            retval = 2;
        else if (amount == numeric)
            retval = 3;
        else if (entered == numeric)
            retval = 4;
    }
    else if (strlen(word) > 0)
    {
        // only need to return 0 for false, 1 for true.
        // Using different positive values so that we can later determine
        // what matched.  Mostly for debugging.
        if (compareString(employee_name, word))
            retval = 5;
        else if (compareString(drawer_name, word))
            retval = 6;
        else if (compareString(account_name, word))
            retval = 7;
        else if (compareString(document.Value(), word))
            retval = 8;
        else if (compareString(explanation.Value(), word))
            retval = 9;
    }
    return retval;
}


/*********************************************************************
 * The ExpenseDB class
 ********************************************************************/

ExpenseDB::ExpenseDB()
{
    entered = 0;
    pathname.Set("");
}

ExpenseDB::~ExpenseDB()
{
    Save();
}

int ExpenseDB::StatusMatch(int status, int drawer_status)
{
    int retval = 0;
    if (status == DRAWER_ANY)
    {
        retval = 1;
    }
    else if ((status == DRAWER_OPEN) &&
             (drawer_status == DRAWER_OPEN || drawer_status == DRAWER_PULLED))
    {
        retval = 1;
    }
    else if ((status == DRAWER_BALANCED) && (drawer_status == DRAWER_BALANCED))
    {
        retval = 1;
    }
    return retval;
}

int ExpenseDB::ExpenseCount(Terminal *term, int status)
{
    FnTrace("ExpenseDB::ExpenseCount()");
    Expense *currExpense = NULL;
    Drawer *dlist = NULL;
    Drawer *drawer = NULL;
    int dstat;
    int count = 0;

    if (term == NULL)
    {
        count = expense_list.Count();
    }
    else
    {
        dlist = term->system_data->DrawerList();
        currExpense = expense_list.Head();
        while (currExpense != NULL)
        {
            drawer = dlist->FindBySerial(currExpense->drawer_id);
            if (drawer)
            {
                dstat = drawer->GetStatus();
                if (StatusMatch(status, dstat))
                    count += 1;
            }
            else
            {  // assume new or account expense
                count += 1;
            }
            currExpense = currExpense->next;
        }
    }
    return count;
}

/****
 * Read:  This is for archives.  We save all expenses in a
 *   single file.
 ****/
int ExpenseDB::Read(InputDataFile &infile, int version)
{
    FnTrace("ExpenseDB::Read()");
    int count = 0;
    int idx;
    Expense *currExp;

    if (version >= 4)
    { // load ExpenseDB::entered
        infile.Read(entered);
    }

    infile.Read(count);
    for (idx = 0; idx < count; idx++)
    {
        currExp = new Expense;
        currExp->Read(infile, version);
        Add(currExp);
    }
    return 0;
}

/****
 * Write:  This is for archives.  We save all expenses in a
 *   single file.
 ****/
int ExpenseDB::Write(OutputDataFile &outfile, int version)
{
    FnTrace("ExpenseDB::Write()");
    Expense *currExp = expense_list.Head();

    outfile.Write(entered);

    outfile.Write(expense_list.Count());
    while (currExp != NULL)
    {
        if (! currExp->IsTraining())
            currExp->Write(outfile, version);
        currExp = currExp->next;
    }
    return 0;
}

int ExpenseDB::Load(const char* path)
{
    FnTrace("ExpenseDB::Load()");
    InputDataFile infile;
    struct dirent *record = NULL;
    const genericChar* name;
    genericChar fullpath[STRLENGTH];
    int version = 0;
    int error = 0;

    if (path)
        pathname.Set(path);
    
    if (pathname.empty())
        return 1;

    // first read the global ExpenseDB file
    snprintf(fullpath, STRLENGTH, "%s/expensedb", pathname.Value());
    error = infile.Open(fullpath, version);
    if (error == 0)
    {
        if (version >= 4)
            infile.Read(entered);
        infile.Close();
    }

    // then read the individual expenses
    DIR *dp = opendir(pathname.Value());
    if (dp == NULL)
        return 1;  // Error - can't find directory
    do
    {
        record = readdir(dp);
        if (record)
        {
            name = record->d_name;
            if (strncmp("expense_", name, 8) == 0)
            {
                int len = strlen(name);
                if (strcmp(&name[len-4], ".fmt") == 0)
                    break;
                snprintf(fullpath, STRLENGTH, "%s/%s", pathname.Value(), name);
                Expense *exp = new Expense();
                if (exp->Load(fullpath))
                    ReportError("Error loading expense");
                else
                    Add(exp);
            }
        }
    }
    while (record);

    closedir(dp);
    return 0;
}

/****
 * RemoveBlank:  let's get rid of records that don't actually spend anything.
 * Returns the highest ID
 ****/
int ExpenseDB::RemoveBlank()
{
    FnTrace("ExpenseDB::RemoveBlank()");
    Expense *curr = expense_list.Head();
    Expense *next = NULL;
    int new_id = 0;

    // wipe out all blank records and find the highest id
    while (curr != NULL)
    {
        next = curr->next;
        if (curr->IsBlank())
        {
            Remove(curr);
            delete(curr);
        }
        else if (curr->eid > new_id)
        {
            new_id = curr->eid;
        }
        curr = next;
    }
    new_id += 1;
    return new_id;
}

/****
 * Save:  Do not save training records.
 ****/
int ExpenseDB::Save()
{
    FnTrace("ExpenseDB::Save()");
    if (pathname.empty())
        return 1;
    genericChar fullpath[STRLENGTH];
    OutputDataFile outfile;

    RemoveBlank();
    // save off the global ExpenseDB stuff
    snprintf(fullpath, STRLENGTH, "%s/expensedb", pathname.Value());
    if (outfile.Open(fullpath, EXPENSE_VERSION) == 0)
    {
        outfile.Write(entered);
        outfile.Close();
    }
    
    // save individual expenses
    Expense *thisexp = expense_list.Head();
    while (thisexp != NULL)
    {
        if (! thisexp->IsTraining())
            thisexp->Save(pathname.Value());
        thisexp = thisexp->next;
    }
    return 0;
}

int ExpenseDB::Save(int id)
{
    FnTrace("ExpenseDB::Save()");
    int retval = 1;

    RemoveBlank();
    Expense *exp = FindByID(id);
    if (exp != NULL && exp->IsTraining() == 0)
    {
        exp->Save(pathname.Value());
        retval = 0;
    }
    return retval;
}

int ExpenseDB::SaveEntered(int entered_val, int drawer_serial)
{
    FnTrace("ExpenseDB::SaveEntered()");
    int retval = 0;
    Expense *expense = expense_list.Head();

    while (expense != NULL)
    {
        if (expense->drawer_id == drawer_serial)
        {
            expense->entered = entered_val;
            expense = NULL;  // end the loop
        }
        else
        {
            expense = expense->next;
        }
    }
    retval = Save();
    return retval;
}

Expense *ExpenseDB::NewExpense()
{
    FnTrace("ExpenseDB::NewExpense()");
    Expense *newExp;
    int new_id = RemoveBlank();
    // create a new one
    newExp = new Expense(new_id);
    newExp->exp_date.Set();
    // add it to the DB
    Add(newExp);
    return newExp;
}

int ExpenseDB::Add(Expense *expense)
{
    FnTrace("ExpenseDB::Add()");
    return expense_list.AddToTail(expense);
    return 0;
}

/****
 * AddDrawerPayments:
 ****/
int ExpenseDB::AddDrawerPayments(Drawer *drawer_list)
{
    FnTrace("ExpenseDB::AddDrawerPayments()");
    Drawer *currDrawer = drawer_list;
    DrawerBalance *currBalance;
    DrawerBalance *nextBalance;
    int retval = 0;
    int amount;
    int my_entered;
    int count;

    // First, clear out the payment balances
    while (currDrawer != NULL)
    {
        currBalance = currDrawer->BalanceList();
        while (currBalance != NULL)
        {
            nextBalance = currBalance->next;
            if (currBalance->tender_type == TENDER_EXPENSE)
            {
                currDrawer->Remove(currBalance);
            }
            currBalance = nextBalance;
        }
        currDrawer = currDrawer->next;
    }

    currDrawer = drawer_list;
    while (currDrawer != NULL)
    {
        amount = 0;
        my_entered = 0;
        count = 0;
        Expense *currExpense = expense_list.Head();
        while (currExpense != NULL)
        {
            if ((currExpense->IsTraining() == 0) &&
                (currExpense->drawer_id == currDrawer->serial_number))
            {
                amount += currExpense->amount;
                my_entered += currExpense->entered;
                count  += 1;
            }
            currExpense = currExpense->next;
        }
        if (amount || my_entered)
        {
            currBalance = currDrawer->FindBalance(TENDER_EXPENSE, 0, 1);
            if (currBalance)
            {
                currBalance->amount  = amount;
                currBalance->count   = count;
                currBalance->entered = my_entered;
                currDrawer->Total(NULL, 1);
            }
        }
        currDrawer = currDrawer->next;
    }
    return retval;
}

int ExpenseDB::Remove(Expense *expense)
{
    FnTrace("ExpenseDB::Remove()");
    genericChar filename[STRLENGTH];

    expense->SetFileName(filename, STRLENGTH, pathname.Value());
    expense_list.Remove(expense);
    unlink(filename);
    return 0;
}

int ExpenseDB::Purge()
{
    FnTrace("ExpenseDB::Purge()");
    //FIX BAK-->Should remove the files, too, right?
    //NOTE BAK-->Nope.  This is for resets.  When System::EndDay() runs all archives
    // are set to reload.  The reload process does a purge so that the expenses and
    // other items are not loaded twice.  So if we delete the files here we'll lose
    // everything.  Leave file deletion for the Remove() method.
    expense_list.Purge();
    return 0;
}

/****
 * MoveTo:  Move closed expenses to the archive (the given ExpenseDB).
 *   Expenses paid from drawers that are still open are considered
 *   active and will not be archived at this time.  All other expenses
 *   will be archived.
 ****/
int ExpenseDB::MoveTo(ExpenseDB *exp_db, Drawer *DrawerList)
{
    FnTrace("ExpenseDB::MoveTo()");
    Expense *currExpense = expense_list.Head();
    Expense *prevExpense = NULL;
    Drawer *currDrawer;
    int move;

    exp_db->entered = entered;
    while (currExpense != NULL)
    {
        move = 0;
        // if the Expense is from an account, or if the drawer has been balanced,
        // we'll move it across.  Otherwise, we leave it alone.
        if (currExpense->IsTraining())
        {
            move = 0;  // null action; just don't archive training expenses
        }
        if (currExpense->drawer_id > -1)
        {
            currDrawer = DrawerList->FindBySerial(currExpense->drawer_id);
            if (currDrawer != NULL && currDrawer->GetStatus() == DRAWER_BALANCED)
            {
                move = 1;
            }
        }
        else if (currExpense->account_id > -1)
        {
            move = 1;
        }
        // now move it or not; either way, we need to go to the correct
        // next record.  If we're still occupying the head, we'll get
        // the head, otherwise we'll use prev->next;
        if (move)
        {
            Remove(currExpense);
            exp_db->Add(currExpense);
            if (prevExpense)
                currExpense = prevExpense->next;
            else
                currExpense = expense_list.Head();
        }
        else
        {
            prevExpense = currExpense;
            currExpense = currExpense->next;
        }
    }
    entered = 0;
    return 0;
}

/****
 * MoveAll:  this method will move all records regardless of whether drawers
 *   have been balanced, et al.  Giving exp_db as NULL acts as a Purge()
 *   (MoveAll to /dev/NULL).
 ****/
int ExpenseDB::MoveAll(ExpenseDB *exp_db)
{
    FnTrace("ExpenseDB::MoveAll()");
    Expense *currExpense = expense_list.Head();

    while (currExpense != NULL)
    {
        Remove(currExpense);
        if (exp_db != NULL)
            exp_db->Add(currExpense);
        currExpense = expense_list.Head();
    }
    return 0;
}

Expense *ExpenseDB::FindByRecord(Terminal *term, int no, int drawer_type)
{
    FnTrace("ExpenseDB::FindByRecord()");
    Drawer *dlist = NULL;
    Drawer *drawer = NULL;
    Expense *thisexp = expense_list.Head();
    Expense *retexp = NULL;
    int count = 0;
    int maxcount = expense_list.Count();

    if (term != NULL)
        dlist = term->system_data->DrawerList();
    while (thisexp != NULL && count < maxcount)
    {
        if (dlist != NULL)
            drawer = dlist->FindBySerial(thisexp->drawer_id);
        if ((drawer == NULL) || (StatusMatch(drawer_type, drawer->GetStatus())))
        {
            if (count == no)
            {
                retexp = thisexp;
                thisexp = NULL;  //end the loop
            }
            else
            {
                count += 1;
            }
        }
        if (thisexp != NULL)
            thisexp = thisexp->next;
    }
    return retexp;
}

Expense *ExpenseDB::FindByID(int id)
{
    FnTrace("ExpenseDB::FindByID()");
    Expense *thisexp = expense_list.Head();
    Expense *retexp = NULL;

    while (thisexp != NULL)
    {
        if (id == thisexp->eid)
        {
            retexp = thisexp;
            thisexp = NULL;  //end the loop
        }
        else
            thisexp = thisexp->next;
    }
    return retexp;
}

/****
 *  FindRecordByWord:  returns record number of matching expense
 *    or -1 on failure
 ****/
int ExpenseDB::FindRecordByWord(Terminal *term, const genericChar* word, int start, Archive *archive)
{
    FnTrace("ExpenseDB::FindRecordByWord()");
    Drawer *dlist = NULL;
    Drawer *drawer = NULL;
    Expense *thisexp = expense_list.Head();
    int count = 0;
    int maxcount = expense_list.Count();
    int retval = -1;

    if (archive != NULL)
        dlist = archive->DrawerList();
    else if (term != NULL)
        dlist = term->system_data->DrawerList();
    while (thisexp != NULL && count < maxcount)
    {
        if (dlist != NULL)
            drawer = dlist->FindBySerial(thisexp->drawer_id);
        if ((drawer == NULL) || (StatusMatch(DRAWER_OPEN, drawer->GetStatus())))
        {
            if ((count > start) && thisexp->WordMatch(term, word))
            {
                retval = count;
                thisexp = NULL;  //end the loop
            }
            else
            {
                count += 1;
            }
        }
        if (thisexp != NULL)
            thisexp = thisexp->next;
    }
    return retval;
}

/****
 * CountFromDrawer:
 * NOTE:  training should be 0 or 1, indicating whether we are in training
 *   mode (term->user->training).  The value is compared with the result
 *   of expense->IsTraining() which should also be simply 0 or 1.
 ****/
int ExpenseDB::CountFromDrawer(int drawer_id, int training)
{
    FnTrace("ExpenseDB::CountFromDrawer()");
    Expense *currExp = expense_list.Head();
    int drawer_count = 0;

    while (currExp != NULL)
    {
        if ((currExp->drawer_id == drawer_id) &&
            (currExp->IsTraining() == training))
        {
            drawer_count += 1;
        }
        currExp = currExp->next;
    }
    return drawer_count;
}

/****
 * BalanceFromDrawer:
 * NOTE:  training should be 0 or 1, indicating whether we are in training
 *   mode (term->user->training).  The value is compared with the result
 *   of expense->IsTraining() which should also be simply 0 or 1.
 ****/
int ExpenseDB::BalanceFromDrawer(int drawer_id, int training)
{
    FnTrace("ExpenseDB::BalanceFromDrawer()");
    Expense *currExp = expense_list.Head();
    int drawer_expenses = 0;

    while (currExp != NULL)
    {
        if ((currExp->drawer_id == drawer_id) &&
            (currExp->IsTraining() == training))
        {
            drawer_expenses += currExp->amount;
        }
        currExp = currExp->next;
    }
    return drawer_expenses;
}

int ExpenseDB::CountFromAccount(int account_id, int training)
{
    FnTrace("ExpenseDB::CountFromAccount()");
    Expense *currExp = expense_list.Head();
    int account_count = 0;

    while (currExp != NULL)
    {
        if ((currExp->account_id == account_id) &&
            (currExp->IsTraining() == training))
        {
            account_count += 1;
        }
        currExp = currExp->next;
    }
    return account_count;
}

int ExpenseDB::BalanceFromAccount(int account_id, int training)
{
    FnTrace("ExpenseDB::BalanceFromAccount()");
    Expense *currExp = expense_list.Head();
    int account_expenses = 0;

    while (currExp != NULL)
    {
        if ((currExp->account_id == account_id) &&
            (currExp->IsTraining() == training))
        {
            account_expenses += currExp->amount;
        }
        currExp = currExp->next;
    }
    return account_expenses;
}

int ExpenseDB::EnteredFromAccount(int account_id, int training)
{
    FnTrace("ExpenseDB::EnteredFromAccount()");
    return entered;  //NOTE:  this method isn't really needed
}

/****
 * TotalExpenses:
 * NOTE:  training should be 0 or 1, indicating whether we are in training
 *   mode (term->user->training).  The value is compared with the result
 *   of expense->IsTraining() which should also be simply 0 or 1.
 ****/
int ExpenseDB::TotalExpenses(int training)
{
    FnTrace("ExpenseDB::TotalExpenses()");
    Expense *currExp = expense_list.Head();
    int total_expenses = 0;

    while (currExp != NULL)
    {
        if (currExp->IsTraining() == training)
            total_expenses += currExp->amount;
        currExp = currExp->next;
    }
    return total_expenses;
}

int ExpenseDB::EnteredFromDrawer(int drawer_id, int training)
{
    FnTrace("ExpenseDB::EnteredFromDrawer()");
    Expense *currExp = expense_list.Head();
    int drawer_entered = 0;

    while (currExp != NULL)
    {
        if ((currExp->drawer_id == drawer_id) &&
            (currExp->IsTraining() == training))
        {
            drawer_entered += currExp->entered;
        }
        currExp = currExp->next;
    }
    return drawer_entered;
}

int ExpenseDB::PrintExpenses()
{
    FnTrace("ExpenseDB::PrintExpenses()");
    Expense *currExpense = expense_list.Head();
    
    printf("Print Start...\n");
    while (currExpense != NULL)
    {
        printf("  Expense %s\n", currExpense->document.Value());
        currExpense = currExpense->next;
    }
    printf("Print End.\n");
    return 0;
}
