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
 * expense.hh
 * Expense tracking and reporting.
 */

//NOTE BAK--> ENTERED The "entered" values have gone through some
//  changes.  There should be an entered value per balanced drawer which
//  may cover multiple expenses.  We cannot safely and correctly divide
//  the value across multiple expenses, so we'll store the value in the
//  first expense for the specified drawer.  In one iteration of this
//  file, the "entered" value was stored in the expense_db, but while
//  this allowed for one value for multiple expenses, it did not allow
//  for one value per drawer.  Thus the current method which may, in
//  the end, not work.

#ifndef _EXPENSE_HH
#define _EXPENSE_HH

#include "list_utility.hh"
#include "utility.hh"
#include "terminal.hh"
#include "report.hh"
#include "drawer.hh"


#define EXPENSE_VERSION   8  // initial version, of course
// Expense Versions
// 2                explanation
// 3                tax info
// 4    5/2/2002    entered in ExpenseDB
// 5    5/2/2002    save the flags
// 6    6/13/2002   add expense destination account
// 7    9/27/2002   read/write the expense->entered value
// 8   10/10/2002   read/write the expense->exp_date value

#define HAVE_EID          1
#define HAVE_ACCOUNTID    2
#define HAVE_EMPLOYEEID   4
#define HAVE_DRAWERID     8
#define HAVE_AMOUNT      16
#define HAVE_DOCUMENT    32
#define HAVE_ALL         63  // total of all items

#define EF_TRAINING       1  // training flag

class Expense
{
    int flags;
public:
    Expense *next;
    Expense *fore;
    int eid;
    int account_id;
    int tax_account_id;
    int employee_id;
    int drawer_id;
    TimeInfo exp_date;
    int amount;
    int tax;
    int entered;  // see ENTERED note at top of file
    int dest_account_id;
    Str document;
    Str explanation;

    Expense();
    Expense(int &no);
    int SetFileName(char* buffer, int maxlen, const genericChar* path);
    int Read(InputDataFile &infile, int version);
    int Write(OutputDataFile &outfile, int version);
    int Load(const char* path);
    int Save(const char* path);
    int IsBlank();
    int Author(Terminal *term, genericChar* employee_name);
    int DrawerOwner(Terminal *term, genericChar* drawer_name, Archive *archive = NULL);
    int AccountName(Terminal *term, genericChar* account_name, Archive *archive = NULL);
    int IsTraining();
    int SetFlag(int flagval);
    int Copy(Expense *original);
    int WordMatch(Terminal *term, const genericChar* word);
};

class ExpenseDB
{
    DList<Expense> expense_list;
    Str pathname;
    int entered;  // see ENTERED note at top of file
public:
    ExpenseDB();
    ~ExpenseDB();
    Expense *ExpenseList()    { return expense_list.Head(); }
    Expense *ExpenseListEnd() { return expense_list.Tail(); }
    int      ExpenseCount(Terminal *term = NULL, int status = DRAWER_ANY);

    int      StatusMatch(int status, int drawer_status);
    int      Read(InputDataFile &infile, int version);
    int      Write(OutputDataFile &outfile, int version);
    int      Load(const char* path);
    int      RemoveBlank();
    int      Save();
    int      Save(int id);
    int      SaveEntered(int entered_val, int drawer_serial);
    Expense *NewExpense();
    int      Add(Expense *expense);
    int      AddDrawerPayments(Drawer *drawer_list);
    int      Remove(Expense *expense);
    int      Purge();
    int      MoveTo(ExpenseDB *exp_db, Drawer *DrawerList);
    int      MoveAll(ExpenseDB *exp_db);
    Expense *FindByRecord(Terminal *term, int no, int drawer_type = DRAWER_OPEN);
    Expense *FindByID(int id);
    int      FindRecordByWord(Terminal *term, const genericChar* word, int start = -1, Archive *archive = NULL);
    int      CountFromDrawer(int drawer_id, int training = 0);
    int      BalanceFromDrawer(int drawer_id, int training = 0);
    int      EnteredFromDrawer(int drawer_id, int training = 0);
    int      CountFromAccount(int account_id, int training = 0);
    int      BalanceFromAccount(int account_id, int training = 0);
    int      EnteredFromAccount(int account_id, int training = 0);
    int      TotalExpenses(int training = 0);
    int      PrintExpenses();  // debugging function
};

#endif
