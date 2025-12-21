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
 * account.hh - revision 11 (8/10/98)
 * Account handling class objects
 */

#ifndef ACCOUNT_HH
#define ACCOUNT_HH

#include "terminal.hh"
#include "utility.hh"
#include "list_utility.hh"

#include <vector>

#define ACCOUNT_VERSION         1
#define ACCOUNT_ENTRY_VERSION   1
#define ACCOUNT_FIRST_NUMBER    1000 // no account number should be below this

/**** Types ****/
class InputDataFile;
class OutputDataFile;

class AccountEntry
{
public:
    Str      description;
    TimeInfo time;
    int      amount;
    int      flags;

    // Constructors
    AccountEntry();
    AccountEntry(const char* desc, int x);

    bool operator==(const AccountEntry &other) const;

    // Member Functions
    int Read(InputDataFile &df, [[maybe_unused]] int version);
    int Write(OutputDataFile &df, [[maybe_unused]] int version);
    int Search(const std::string &word) const;
};

class Account
{
private:
    std::vector<AccountEntry> entry_list;

public:
    Account *next, *fore;

    Str pathname;
    int number;
    Str name;
    int balance;
    TimeInfo time_created;
    int user_created;

    // Constructor
    Account();
    Account(int no);
    Account(int no, const genericChar* desc);
    Account(const char* path, int no = 0, const char* namestr = "");

    // Member Functions
    std::vector<AccountEntry> &EntryList()    { return entry_list; }

    Account *Copy();
    int      Load(const char* path);
    int      Save();
    int      ReadEntries(InputDataFile &df);
    int      WriteEntries(OutputDataFile &df, int version);
    void     Add(const AccountEntry &ae);
    int      Remove(const AccountEntry &ae);
    int      RemoveFile();
    void     Purge();
    int      AddEntry(const char* desc, int amount);
    int      IsBlank();
    int      Search(const std::string &word);
};


class AccountDB
{
    DList<Account> account_list;
    DList<Account> default_list;
    Account *curr_item;  // for Next() and iteration
public:
    Str pathname;
    int low_acct_num;
    int high_acct_num;

    // Constructor
    AccountDB();

    // Member Functions
    Account *AccountList()    { return account_list.Head(); }
    Account *AccountListEnd() { return account_list.Tail(); }
    int      AccountCount()   { return account_list.Count(); }

    Account *DefaultList()    { return default_list.Head(); }
    Account *DefaultListEnd() { return default_list.Tail(); }
    int      DefaultCount()   { return default_list.Count(); }

    int      GetAccountNumber(int number);
    int      RemoveBlank();
    Account *NewAccount(int number = 0);
    int      Sort();
    int      Load(const char* path);
    int      Save();
    int      Save(Account *acct, int do_sort = 1);
    int      Add(Account *acct);
    int      AddDefault(Account *acct);
    int      Remove(Account *acct);
    int      RemoveDefault(Account *acct);
    int      Purge();
    Account *FindByNumber(int no);
    Account *FindByRecord(int rec_num);
    int      FindRecordByNumber(int num);
    int      FindRecordByWord(const genericChar* word, int record = -1);
    Account *Next();
};


// GENERAL FUNCTIONS

int IsValidAccountNumber(Terminal *term, int number);


#endif
