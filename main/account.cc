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
 * account.cc - revision 20 (9/8/98)
 * implementation of account classes
 */

#include "account.hh"
#include "data_file.hh"
#include "system.hh"
#include <sys/types.h>
#include <sys/file.h>
#include <dirent.h>
#include <cstring>
#include <unistd.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/**** AccountEntry class ****/
// Constructors
AccountEntry::AccountEntry()
{
    amount = 0;
    flags = 0;
}

AccountEntry::AccountEntry(const char* desc, int amt)
{
    description.Set(desc);
    amount = amt;
    flags = 0;
}

// Member Functions
int AccountEntry::Read(InputDataFile &df, int version)
{
    FnTrace("AccountEntry::Read()");
    int error = 0;
    error += df.Read(time);
    error += df.Read(amount);
    error += df.Read(flags);
    error += df.Read(description);
    return error;
}

int AccountEntry::Write(OutputDataFile &df, int version)
{
    FnTrace("AccountEntry::Write()");
    int error = 0;
    error += df.Write(time);
    error += df.Write(amount);
    error += df.Write(flags);
    error += df.Write(description, 1);
    return error;
}

int AccountEntry::Search(const genericChar* word)
{
    FnTrace("AccountEntry::Search()");
    int found = 0;
    if (StringCompare(description.Value(), word) == 0)
    {
        found = 1;
    }
    return found;
}

/**** Account class ****/
// Constructors
Account::Account()
{
    number = 0;
    name.Set("");
    user_created = 0;
    pathname.Set("");
    balance = 0;
    time_created.Set();
}

Account::Account(int no)
{
    number = no;
    name.Set("");
    user_created = 0;
    pathname.Set("");
    balance = 0;
    time_created.Set();
}

Account::Account(int no, const char* namestr)
{
    number = no;
    name.Set(namestr);
    user_created = 0;
    pathname.Set("");
    balance = 0;
    time_created.Set();
}

Account::Account(const char* path, int no, const genericChar* namestr)
{
    number = no;
    name.Set(namestr);
    user_created = 0;
    pathname.Set(path);
    balance = 0;
    time_created.Set();
}

// Member Functions
Account *Account::Copy()
{
    FnTrace("Account::Copy()");
    Account *newAccount = new Account();

    newAccount->number = number;
    newAccount->name.Set(name);
    newAccount->user_created = user_created;
    newAccount->pathname.Set(pathname);
    newAccount->balance = balance;
    newAccount->time_created.Set(time_created);
    return newAccount;
}

int Account::Load(const char* path)
{
    FnTrace("Account::Load()");
    InputDataFile df;
    int version = 0;
    pathname.Set(path);

    genericChar filename[STRLENGTH];
    sprintf(filename, "%s/%04d", path, number);
    if (df.Open(filename, version))
        return 1;
    
    df.Read(name);
    df.Read(balance);
    df.Read(time_created);
    df.Read(user_created);

    if (ReadEntries(df))
    {
        genericChar str[STRLENGTH];
        sprintf(str, "Error in reading Account #%d Entries", number);
        ReportError(str);
        return 1;  // df destructor will close the file
    }
    df.Close();
    return 0;
}

int Account::Save()
{
    FnTrace("Account::Save()");
    genericChar filename[STRLENGTH];
    OutputDataFile df;

    sprintf(filename, "%s/%04d", pathname.Value(), number);
    if (df.Open(filename, ACCOUNT_VERSION))
        return 1;
    df.Write(name);
    df.Write(balance);
    df.Write(time_created);
    df.Write(user_created);

    WriteEntries(df, ACCOUNT_ENTRY_VERSION);
    df.Close();
    return 0;
}

int Account::ReadEntries(InputDataFile &df)
{
    FnTrace("Account::ReadEntries()");
    int no = 0;
    int entry_version;
    df.Read(entry_version);
    df.Read(no);
    for (int i = 0; i < no; ++i)
    {
        AccountEntry *ae = new AccountEntry;
        ae->Read(df, entry_version);
        Add(ae);
    }
    return 0;
}

int Account::WriteEntries(OutputDataFile &df, int version)
{
    FnTrace("Account::WriteEntries()");
    // write entries out to file
    df.Write(version);
    df.Write(EntryCount());  
    for (AccountEntry *ae = EntryList(); ae != NULL; ae = ae->next)
        ae->Write(df, version);
    return 0;
}

int Account::Add(AccountEntry *ae)
{
    FnTrace("Account::Add()");
    return entry_list.AddToTail(ae);
}

/****
 * Remove:  Do a bit of an overload here:  if ae == NULL, remove the entire
 *   account (AKA, delete the associated file).  Otherwise, just delete
 *   the account entry.
 ****/
int Account::Remove(AccountEntry *ae = NULL)
{
    FnTrace("Account::Remove()");
    genericChar filename[STRLENGTH];
    int retval = 0;

    if (ae == NULL)
    {
        snprintf(filename, STRLENGTH, "%s/%04d", pathname.Value(), number);
        retval = unlink(filename);
    }
    else
    {
        entry_list.Remove(ae);
    }
    return retval;
}

int Account::Purge()
{
    FnTrace("Account::Purge()");
    entry_list.Purge();
    return 0;
}

int Account::AddEntry(const char* desc, int amount)
{
    FnTrace("Account::AddEntry()");
    AccountEntry *ae = new AccountEntry(desc, amount);
    Add(ae);
    return 0;
}

/****
 * IsBlank:  an account is blank if the name is empty and the balance
 *   is 0.  Because we're using floating points, we'll consider 0.00
 *   -0.001 < 0 < 0.001.  Let's also assume that if the account
 *   number has never been set to a valid value, this account is
 *   bogus.
 ****/
int Account::IsBlank()
{
    FnTrace("Account::IsBlank()");
    int ibalance = (int)balance; // convert to int for comparison with 0

    if (number == 0)
        return 1;
    if ((name.empty()) &&
        (ibalance == 0))
    {
        return 1;
    }   
    return 0;
}

/****
 * Search:  Return 1 if word is found in the account, 0 otherwise.
 *   Searches word fields in the account as well as account entries.
 ****/
int Account::Search(const genericChar* word)
{
    FnTrace("Account::Search()");
    int found = 0;

    if (StringCompare(name.Value(), word) == 0)
    {
        found = 1;
    }
    else
    {
        AccountEntry *currAE = entry_list.Head();
        while (currAE != NULL && found != 1)
        {
            if (currAE->Search(word))
                found = 1;
            else
                currAE = currAE->next;
        }
    }
    return found;
}


/**** AccountDB class ****/
// Constructor
AccountDB::AccountDB()
{
    // the settings for this are in the Settings object and should
    // be set in manager.cc->StartSystem()
    low_acct_num = 1000;
    high_acct_num = 9999;
    curr_item = NULL;
}

// Member Functions

int AccountDB::GetAccountNumber(int number)
{
    FnTrace("AccountDB::GetAccountNumber()");
    int retval = ACCOUNT_FIRST_NUMBER;
    Account *my_account = account_list.Head();

    while (my_account != NULL)
    {
        if ((my_account->number > number) &&
            (my_account->number > retval))
        {
            my_account = NULL;  // exit the loop
        }
        else
        {
            retval = my_account->number + 1;
            my_account = my_account->next;
        }
    }
    return retval;
}

int AccountSort(Account *acct1, Account *acct2)
{
    FnTrace("AccountDB::AccountSort()");
    if (acct1->number < acct2->number)
        return -1;
    else if (acct1->number == acct2->number)
        return 0;
    else
        return 1;
}

/****
 * RemoveBlank:  As it says, removes blank records.  Any record having an
 *   account number < ACCOUNT_FIRST_NUMBER, or having an empty name
 *   field and a balance of 0 (or right around 0), is considered
 *   a blank record.
 *   Returns the number of records deleted.
 ****/
int AccountDB::RemoveBlank()
{
    FnTrace("AccountDB::RemoveBlank()");
    Account *nextAcct = NULL;
    Account *currAcct = account_list.Head();
    int count = 0;

    while (currAcct != NULL)
    {
        nextAcct = currAcct->next;
        if (currAcct->IsBlank())
        {
            account_list.Remove(currAcct);
            delete(currAcct);
            count += 1;
        }
        currAcct = nextAcct;
    }
    account_list.Sort(AccountSort);
    return count;
}

/****
 * NewAccount:  The account number will be initialized to 0
 *   unless number is given.  In that case, the first available
 *   number greater than number will be used (so if you have
 *   accounts 1001, 1002, 1003, 1005, and you specify
 *   number = 1002, you'll get a new account with number 1004).
 ****/
Account *AccountDB::NewAccount(int number)
{
    FnTrace("AccountDB::NewAccount()");
    Account *newAcct;

    RemoveBlank();
    newAcct = new Account(pathname.Value());
    newAcct->number = GetAccountNumber(number);
    newAcct->time_created.Set();
    Add(newAcct);
    return newAcct;
}

/****
 * Save:  This function saves all records after removing any blank records.
 ****/
int AccountDB::Save()
{
    FnTrace("AccountDB::Save()");
    Account *currAcct = account_list.Head();

    RemoveBlank();
    while (currAcct != NULL)
    {
        Save(currAcct, 0);
        currAcct = currAcct->next;
    }
    return 0;
}

/****
 * Save:  Just save the given record.  This function also cleans out
 *   blank records.  It seems a slightly odd place to do that, but
 *   the save function above actually isn't called.  So we need some
 *   way to ensure blank records don't get saved.
 * //FIX BAK--> There needs to be a good, central location to verify account
 *  numbers against the settings in the Settings object.  This is it.
 *  We can double-check before we save.  However, it is difficult from
 *  here to send a message to the user.  Need to find a good error message
 *  system.
 ****/
int AccountDB::Save(Account *my_account, int do_sort)
{
    FnTrace("AccountDB::SaveAccount()");
    int retval = 0;
    
    if (do_sort)
        RemoveBlank();
    my_account->Save();
    return retval;
}

int AccountDB::Load(const char* path)
{
    FnTrace("AccountDB::Load()");
    if (path)
        pathname.Set(path);

    if (pathname.empty())
        return 1;

    DIR *dp = opendir(pathname.Value());
    if (dp == NULL)
        return 1;  // Error - can't find directory

    int no = 0;
    struct dirent *record = NULL;
    do
    {
        record = readdir(dp);
        if (record)
        {
            const genericChar* name = record->d_name;
            no = atoi(name);
            if (no > 0)
            {
                Account *my_account = new Account(no);
                if (my_account->Load(pathname.Value()))
                    ReportError("Error loading account");
                else
                    Add(my_account);
            }
        }
    }
    while (record);

    closedir(dp);
    return 0;
}

int AccountDB::Add(Account *my_account)
{
    FnTrace("AccountDB::Add()");
    if (my_account == NULL)
        return 1;

    Account *list = AccountListEnd();
    while (list && my_account->number < list->number)
        list = list->fore;

    return account_list.AddAfterNode(list, my_account);
}

int AccountDB::AddDefault(Account *my_account)
{
    FnTrace("AccountDB::AddDefault()");
    if (my_account == NULL)
        return 1;

    Account *list = DefaultListEnd();
    while (list && my_account->number < list->number)
        list = list->fore;

    return default_list.AddAfterNode(list, my_account);
}

int AccountDB::Remove(Account *my_account)
{
    FnTrace("AccountDB::Remove()");
    int retval = 0;
    // need to remove the file
    my_account->Remove();
    retval = account_list.Remove(my_account);
    Save();
    return retval;
}

int AccountDB::RemoveDefault(Account *my_account)
{
    FnTrace("AccountDB::RemoveDefault()");
    return default_list.Remove(my_account);
}

int AccountDB::Purge()
{
    FnTrace("AccountDB::Purge()");
    account_list.Purge();
    return 0;
}

Account *AccountDB::FindByNumber(int no)
{
    FnTrace("AccountDB::FindByNumber()");
    for (Account *my_account = AccountList(); my_account != NULL; my_account = my_account->next)
        if (my_account->number == no)
            return my_account;
    return NULL;
}

Account *AccountDB::FindByRecord(int rec_num)
{
    FnTrace("AccountDB::FindByRecord()");
    int count = 0;
    Account *my_account = account_list.Head();
    Account *return_acct = NULL;

    while (my_account != NULL)
    {
        if (count == rec_num)
        {
            return_acct = my_account;
            my_account = NULL;  // exit the loop
        }
        else
        {
            count += 1;
            my_account = my_account->next;
        }
    }
    return return_acct;
}

int AccountDB::FindRecordByNumber(int num)
{
    FnTrace("AccountDB::FindRecordByNumber()");
    int count = 0;
    int record = -1;  // -1 indicates record was not found
    Account *currAccount = account_list.Head();

    while (currAccount != NULL)
    {
        if (currAccount->number == num)
        {
            record = count;
            currAccount = NULL;  // exit the loop
        }
        else
        {
            count += 1;
            currAccount = currAccount->next;
        }
    }
    return record;
}

int AccountDB::FindRecordByWord(const genericChar* word, int record)
{
    FnTrace("AccountDB::FindRecordByWord()");
    int retval = -1;
    int curr_rec = 0;
    Account *currAcct = account_list.Head();

    if (record > 0)
    {
        while ((curr_rec <= record) && (currAcct != NULL))
        {
            currAcct = currAcct->next;
            curr_rec += 1;
        }
    }
    while (currAcct != NULL)
    {
        if (currAcct->Search(word))
        {
            retval = curr_rec;
            currAcct = NULL;  // end the loop
        }
        else
        {
            curr_rec += 1;
            currAcct = currAcct->next;
        }
    }
    return retval;
}

// GENERAL FUNCTIONS

/****
 * IsValidAccountNumber:  determines whether the given account
 *   number is within the allowable range.
 * Returns 1 for valid number, 0 for invalid.
 ****/
int IsValidAccountNumber(Terminal *term, int number)
{
    FnTrace("AccountDB::IsValidAccountNumber()");
    Settings *settings = term->GetSettings();
    int lo = settings->low_acct_num;
    int hi = settings->high_acct_num;
    int retval = 1;  // assume the number is fine

    if (lo && (number < lo))
        retval = 0;
    else if (hi && (number > hi))
        retval = 0;
    return retval;
}

/****
 * Next:  This is an abstraction function that isn't really necessary.
 *   I was not going to sort the accounts, just use this function
 *   to pretend they were sorted, but sorting became necessary anyway.
 *   I'm leaving this hear because the operation should probably be
 *   abstracted anywa.  BAK
 ****/
Account *AccountDB::Next()
{
    FnTrace("AccountDB::Next()");
    if (curr_item == NULL)
        curr_item = account_list.Head();
    else
        curr_item = curr_item->next;
    return curr_item;
}
