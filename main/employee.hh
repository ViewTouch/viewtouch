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
 * employee.hh - revision 110 (8/26/98)
 * Employee information classes
 */

#ifndef _EMPLOYEE_HH
#define _EMPLOYEE_HH

#include "utility.hh"
#include "list_utility.hh"


/**** Definitions & Global Data ****/
// Job Values
#define JOB_NONE        0
#define JOB_DISHWASHER  1
#define JOB_BUSPERSON   2
#define JOB_COOK        3
#define JOB_COOK2       4
#define JOB_CASHIER     5
#define JOB_SERVER      6
#define JOB_SERVER2     7   // server & cashier
#define JOB_HOST        8
#define JOB_BOOKKEEPER  9
#define JOB_MANAGER     10  // shift supervisor
#define JOB_MANAGER2    11  // assistant manager
#define JOB_MANAGER3    12  // manager
#define JOB_BARTENDER   13
#define JOB_COOK3       14

#define JOB_DEVELOPER   50
#define JOB_SUPERUSER   51

#define SUPERUSER_KEY 13524

// Security Flags
#define SECURITY_TABLES     (1<<0)  // go to table page
#define SECURITY_ORDER      (1<<1)  // place an order
#define SECURITY_SETTLE     (1<<2)  // settle a check
#define SECURITY_TRANSFER   (1<<3)  // move check to different table
#define SECURITY_REBUILD    (1<<4)  // alter a check after its finalized
#define SECURITY_COMP       (1<<5)  // comp/void items on check
#define SECURITY_SUPERVISOR (1<<6)  // supervisor page
#define SECURITY_MANAGER    (1<<7)  // manager page
#define SECURITY_EMPLOYEES  (1<<8)  // view/alter employee records
#define SECURITY_DEVELOPER  (1<<9)  // alter aplication
#define SECURITY_EXPENSES   (1<<10) // payout from revenue

// Data
extern genericChar *PayRateName[];
extern int   PayRateValue[];
extern genericChar *JobName[];
extern int   JobValue[];

extern genericChar *StartPageName[];
extern int   StartPageValue[];


/**** Types ****/
class LaborDB;
class Report;
class UserDB;
class Terminal;
class Settings;
class InputDataFile;
class OutputDataFile;

class JobInfo
{
public:
    JobInfo *next, *fore;   // linked list pointers
    int      job;           // employee job title/security level
    int      pay_rate;      // hour/day/week/month
    int      pay_amount;    // salary per pay period
    int      starting_page; // user's initial page after login
    int      curr_starting_page;
    int      dept_code;     // generic id field

    // Constructor
    JobInfo();

    // Member Functions
    int   Read(InputDataFile &df, int version);
    int   Write(OutputDataFile &df, int version);
    genericChar *Title(Terminal *t);
};

class Employee
{
    DList<JobInfo> job_list;

public:
    // Employee state
    Employee *next;
    Employee *fore; // linked list pointers
    int       current_job; // current job - 0 if not logged in
    int       last_job;

    // Employee properties
    int  key;            // user system id number
    int  id;             // internal id number
    int  access_code;    // numeric password for employee
    int  employee_no;    // company employee number
    Str  system_name;    // Name shown on system
    Str  last_name;      // Employee last name
    Str  first_name;     // Employee first name
    Str  address;        // Employee home address
    Str  city;           // City of address
    Str  state;          // 2 letter postal abrv. of state
    Str  phone;          // phone number with area code
    Str  ssn;            // social security number
    Str  description;    // job description
    int  drawer;         // which drawer does server use? (0 or 1)
    int  training;       // boolean - is this employee in training?
    Str  password;       // user's password
    int  security_flags; // user security permissions
    int  active;         // is employee active?  (still employed at store)

    // Constructor
    Employee();

    // Member Functions
    JobInfo *JobList()  { return job_list.Head(); }
    int      JobCount() { return job_list.Count(); }

    int Read(InputDataFile &df, int version);
    // read employee data from file
    int      Write(OutputDataFile &df, int version);
    // write employee data to file
    int Add(JobInfo *j);
    int Remove(JobInfo *j);
    JobInfo *FindJobByType(int job = -1);
    JobInfo *FindJobByNumber(int no);
    genericChar *JobTitle(Terminal *t);
    // returns pointer to job string
    int Security(Settings *s);
    // returns security values
    genericChar *SSN();
    // returns ssn nicely formated
    int StartingPage();
    int SetStartingPage(int page_id);
    int Show(Terminal *t, int active = -1);
    // returns 1 if employee isn't filtered

    int IsBlank();
    // Is employee record blank?
    int UseClock();
    // does user need to clock into system?
    int CanEdit();
    // can user edit non-system pages?
    int CanEditSystem();
    // can user edit system pages?
    int UsePassword(Settings *s);
    // does user use passwords?

    int IsTraining(Settings *s = NULL) { return training; }
    int CanEnterSystem(Settings *s) { return Security(s) & SECURITY_TABLES; }
    int CanOrder(Settings *s)       { return Security(s) & SECURITY_ORDER; }
    int CanSettle(Settings *s)      { return Security(s) & SECURITY_SETTLE; }
    int CanMoveTables(Settings *s)  { return Security(s) & SECURITY_TRANSFER; }
    int CanCompOrder(Settings *s)   { return Security(s) & SECURITY_COMP; }
    int CanRebuild(Settings *s)     { return Security(s) & SECURITY_REBUILD; }
    int IsSupervisor(Settings *s)   { return Security(s) & SECURITY_SUPERVISOR; }
    int IsManager(Settings *s)      { return Security(s) & SECURITY_MANAGER; }
    int CanEditUsers(Settings *s)   { return Security(s) & SECURITY_EMPLOYEES; }
    //NOTE BAK-->May need to add an additional security setting for expense
    // payouts instead of setting it as a manager level action
    int CanPayExpenses(Settings *s) { return Security(s) & SECURITY_MANAGER; }
};

class UserDB
{
    DList<Employee> user_list;

public:
    Employee *super_user;
    Employee *developer;
    Str       filename;
    int       changed;

    Employee **name_array; // cached name list
    Employee **id_array;   // cached id list

    // Constructor
    UserDB();
    // Destructor
    ~UserDB();

    // Member Functions
    Employee *UserList()    { return user_list.Head(); }
    Employee *UserListEnd() { return user_list.Tail(); }
    int       UserCount()   { return user_list.Count(); }

    int       Load(char *file);
    int       Save();
    int       Add(Employee *e);
    int       Remove(Employee *e);
    int       Purge();
    int       Init(LaborDB *db);            // sets last_job flags
    Employee *FindByID(int id);
    Employee *FindByKey(int key);
    Employee *FindByName(char *name);
    Employee *FindByRecord(Terminal *t, int record, int active = 1);
    Employee *NameSearch(char *name, Employee *user);
    int       FindRecordByWord(Terminal *t, genericChar *word, int active = 1,
                               int start = -1);
    int       FindUniqueID();
    int       FindUniqueKey();
    int       ListReport(Terminal *t, int active, Report *r);
    int       UserCount(Terminal *t, int active = 1);
    Employee *NextUser(Terminal *t, Employee *e, int active = 1);
    // returns next user (with job filter)
    Employee *ForeUser(Terminal *t, Employee *e, int active = 1);
    // returns prior user (with job filter)
    int       ChangePageID(int old_id, int new_id);
    Employee *NewUser();
    Employee *KeyConflict(Employee *e);  // returns conflicting employee if any

    Employee **NameArray(int resort = 0);
    // returns sorted (by name) array of users
    Employee **IdArray(int resort = 0);
    // returns sorted (by id) array of users
};


/**** Functions ****/
int   FixPhoneNumber(Str &phone);
char *FormatPhoneNumber(Str &phone);

#endif
