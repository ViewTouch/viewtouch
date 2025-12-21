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
 * tips.hh - revision 29 (8/10/98)
 * Handeling of captured tips and tip payout
 */

#ifndef TIPS_HH
#define TIPS_HH

#include "utility.hh"
#include "list_utility.hh"


/**** Definitions ****/
#define TIP_VERSION 1


/**** Types ****/
class Archive;
class Check;
class Drawer;
class Employee;
class Report;
class Terminal;
class Settings;
class System;
class InputDataFile;
class OutputDataFile;

class TipEntry
{
public:
    TipEntry *next;
    TipEntry *fore; // linked list pointers
    int  user_id;          // id of employee
    int  amount;           // amount owed to employee from captured tips
    int  previous_amount;  // amount carried over from last business day
    int  paid;             // amount paid to employee this business day

    // Constructor
    TipEntry();

    // Member Functions
    int Read(InputDataFile &df, int version);
    // Reads TipEntry data from file
    int Write(OutputDataFile &df, int version);
    // Writes TipEntry data to file
    TipEntry *Copy();
    // Makes copy of TipEntry
    int Count();
    // Returns number of TipEntrys in list starting with current one
};


class TipDB
{
    DList<TipEntry> tip_list;

public:
    Archive *archive;
    int      total_paid;
    int      total_held;
    int      total_previous;

    // Constructor
    TipDB();

    // Member Functions
    TipEntry *TipList() { return tip_list.Head(); }

    int Add(TipEntry *te);
    int Remove(TipEntry *te);
    int Purge();
    TipEntry *FindByUser(int id);
    TipEntry *FindByRecord(int record, Employee *e = NULL);
    int CaptureTip(int user_id, int amount);
    int TransferTip(int user_id, int amount);
    int PayoutTip(int user_id, int amount);
    int Calculate(Settings *s, TipDB *prev,
                  Check *check_list, Drawer *drawer_list);
    int Copy(TipDB *db);
    int Total();
    // calculates total_paid, total_held, total_previous
    void ClearHeld();	// clears total_held
    int PaidReport(Terminal *t, Report *r);
    int PayoutReceipt(Terminal *t, Employee *e, int amount, Report *r);
    int ListReport(Terminal *t, Employee *e, Report *r);
    int Update(System *sys);
};

#endif
