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
 * drawer.hh - revision 50 (8/10/98)
 * Drawer balance, report, use classes
 */

#pragma once  // REFACTOR: Replaced #ifndef _DRAWER_HH guard with modern pragma once

#include "utility.hh"
#include "list_utility.hh"


/**** Definitions ****/
#define DRAWER_VERSION   5

// Drawer Status
#define DRAWER_ANY       0
#define DRAWER_OPEN      1  // Drawer in use
#define DRAWER_PULLED    2  // Drawer pulled but not balanced
#define DRAWER_BALANCED  3  // Drawer has been balanced


/**** Types ****/
class Settings;
class Archive;
class Terminal;
class Employee;
class Report;
class Drawer;
class Check;
class SubCheck;
class InputDataFile;
class OutputDataFile;

class DrawerPayment
{
public:
    DrawerPayment *next;
    DrawerPayment *fore; // linked list pointers
    int            tender_type; // tender type of payment
    int            amount;      // cash amount of payment
    int            entered;     //
    TimeInfo       time;        // time payment was given out
    int            user_id;     // user who made payment
    int            target_id;   // who gets the payment

    // Constructors
    DrawerPayment();
    DrawerPayment(int tender, int amt, int user, TimeInfo &tm, int target);

    // Member Functions
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
};

class DrawerBalance
{
public:
    DrawerBalance *next;
    DrawerBalance *fore;
    int            tender_type;
    int            tender_id;
    int            amount;   // calculated total amount of this tender type
    int            count;    // calculated number of this tender type
    int            entered;  // amount entered for balancing

    // Constructor
    DrawerBalance();
    DrawerBalance(int type, int id);

    // Member Functions
    int   Read(InputDataFile &df, int version);
    int   Write(OutputDataFile &df, int version);
    genericChar* Description(Settings *s, genericChar* str = nullptr);  // REFACTOR: Changed NULL to nullptr for modern C++
};

class Drawer
{
    DList<DrawerPayment> payment_list;
    DList<DrawerBalance> balance_list;

public:
    Drawer    *next;
    Drawer    *fore;      // linked list pointers
    Archive   *archive;          // parent archive (nullptr if current drawer)  // REFACTOR: Changed NULL to nullptr for modern C++
    TimeInfo   start_time;       // time of drawer initialization
    TimeInfo   pull_time;        // time of drawer pull
    TimeInfo   balance_time;     // time of drawer balance
    int        position;         // location of drawer (top, bottom)
    int        owner_id;         // drawer's cashier
    int        puller_id;        // who pulled the drawer
    Terminal  *term;             // terminal drawer is connected to
    int        serial_number;    // serial number of drawer
    Str        host;             // name of terminal
    int        number;           // system number for drawer
    int        media_balanced;   // bitfield of media flags
    int        total_difference; // total over/short
    int        total_checks;     // total checks in drawer
    int        total_payments;   // total payments in drawer
    Str        filename;         // file drawer is saved as (if not archived)

    // Constructors
    Drawer();
    Drawer(TimeInfo &tm);

    // Member Functions
    DrawerPayment *PaymentList() { return payment_list.Head(); }
    DrawerBalance *BalanceList() { return balance_list.Head(); }

    int Status();
    // returns current drawer status
    int Load(const char* file);
    // Loads file containing drawer data
    int Read(InputDataFile &df, int version);
    // Reads drawer data from file
    int Write(OutputDataFile &df, int version);
    // Writes drawer data to file
    int Add(DrawerPayment *dp);
    // Adds DrawerPayment to drawer
    int Add(DrawerBalance *db);
    // Adds DrawerBalance to drawer
    int Remove(DrawerPayment *dp);
    // Removes DrawerPayment from drawer (doesn't delete)
    int Remove(DrawerBalance *db);
    // Removes DrawerBalance from drawer (doesn't delete)
    int Purge();
    // Removes/deletes all DrawerPayments & DrawerBalances
    int Count();
    // Returns number of drawrers in list starting with current drawer
    int Save();
    // Saves drawer to file (or makes parent archive changed)
    int DestroyFile();
    // Kills drawer file on disk only
    int MakeReport(Terminal *t, Check *check_list, Report *r);
    int Open();
    int Total(Check *check_list, int force = 0);
    int ChangeOwner(int user_id);
    int RecordSale(SubCheck *sc);
    int RecordPayment(int tender, int amount, int user, TimeInfo &tm,
                      int target = 0);
    int TotalPaymentAmount(int tender_type);
    int IsOpen();
    int IsServerBank();
    int IsEmpty();
    int IsBalanced();

    DrawerBalance *FindBalance(int tender, int id, int make_new = 0);
    void ListBalances();  // for debugging
    int Balance(int tender, int id);
    int TotalBalance(int tender);

    Drawer *FindByOwner(Employee *e, int status = DRAWER_OPEN);
    // returns drawer with matching owner & status
    Drawer *FindBySerial(int serial);
    // returns drawer with matching serial number
    Drawer *FindByNumber(int no, int status = DRAWER_OPEN);
    // returns drawer with matching physical position number

    int Balance(int user_id);
    // mark drawer as balanced
    int Pull(int user_id);
    // mark open drawer as pulled (also creates new empty open drawer)
    int MergeServerBanks();
    // all other server banks of the same owner will be merged with this one
    int MergeSystems(int mergeall = 0);  // merge drawers from the same host
};

int MergeSystems(Terminal *term, int mergeall = 0);
