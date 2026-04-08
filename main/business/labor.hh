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
 * labor.hh - revision 106 (8/13/98)
 * Work scheduling & time clock classes
 */

#ifndef LABOR_HH
#define LABOR_HH

#include "utility.hh"
#include "list_utility.hh"

#include <string>


/**** Types ****/
class WorkEntry;
class LaborPeriod;
class LaborDB;
class Employee;
class Report;
class Terminal;
class Archive;
class Settings;
class InputDataFile;
class OutputDataFile;

class WorkEntry
{
public:
    WorkEntry *next, *fore;
    int        user_id;
    TimeInfo   start;
    TimeInfo   end;
    int        pay_rate;
    int        pay_amount;
    int        tips;
    short      job;
    short      end_shift;
    int        overtime;

    int        edit_id;
    WorkEntry *original;

    // Constructors
    WorkEntry();
    WorkEntry(Employee *e, int j);
    // Destructor
    ~WorkEntry();

    // Member Functions
    WorkEntry *Copy();
    // return copy of work entry
    int IsWorkDone();
    // boolean - is the work entry complete?
    int LaborCost();
    // total labor cost of entry
    int MinutesWorked();
    int MinutesWorked(TimeInfo &end_work);
    // total minutes worked for entry
    int MinutesOvertime(Settings *s, TimeInfo &end_ot);
    // total overtime minutes for entry
    int Read(InputDataFile &df, int version);
    // reads work entry data from file
    int Write(OutputDataFile &df, int version);
    // writes work entry data to file
    int Edit(int user_id);
    // mark entry for edit
    int Update(LaborPeriod *lp);
    // removes unnessary edits
    int UndoEdit();
    // undoes last edit
    int EndEntry(TimeInfo &tm);
    // ends work entry
    int Overlap(TimeInfo &st, TimeInfo &et);
    // returns minutes overlap between work entry and given period
};

// obsolete
class LaborPeriod
{
    DList<WorkEntry> work_list;

public:
    LaborPeriod *next;
    LaborPeriod *fore;
    TimeInfo end_time;
    int      serial_number;
    int      loaded;
    Str      file_name;

    // Constructor
    LaborPeriod();

    // Member Functions
    WorkEntry *WorkList()    { return work_list.Head(); }
    WorkEntry *WorkListEnd() { return work_list.Tail(); }
    int        WorkCount()   { return work_list.Count(); }

    int Add(WorkEntry *w);
    int Remove(WorkEntry *w);
    int Purge();
    int Scan(const char* filename);
    int Load();
    int Unload();
    int Save();

    int ShiftReport(Terminal *t, WorkEntry *w, Report *r);
    int WorkReport(Terminal *t, Employee *e, TimeInfo &start,
                   TimeInfo &end, Report *r);
    WorkEntry *WorkReportEntry(Terminal *t, int line, Employee *e,
                               TimeInfo &start, TimeInfo &end);
    int        WorkReportLine(Terminal *t, WorkEntry *w, Employee *e,
                              TimeInfo &start, TimeInfo &end);
};

// obsolete
class LaborDB
{
private:
    DList<LaborPeriod> period_list;

public:
    int last_serial;
    Str pathname;

    // Constructor
    LaborDB();

    // Member Functions
    LaborPeriod *PeriodList()    { return period_list.Head(); }
    LaborPeriod *PeriodListEnd() { return period_list.Tail(); }
    int          PeriodCount()   { return period_list.Count(); }

    int Load(const char* path);
    int Add(LaborPeriod *lp);
    int Remove(LaborPeriod *lp);
    int Purge();
    int NewLaborPeriod();
    LaborPeriod *CurrentPeriod();
    WorkEntry *CurrentWorkEntry(Employee *e);
    int IsUserOnClock(Employee *e);
    int IsUserOnBreak(Employee *e);
    int CurrentJob(Employee *e);
    WorkEntry *NewWorkEntry(Employee *e, int job);
    int EndWorkEntry(Employee *e, int end_shift);
    int ServerLaborReport(Terminal *t, Employee *e,
                          TimeInfo &start, TimeInfo &end, Report *r);
    WorkEntry *StartOfShift(Employee *e);
    WorkEntry *NextEntry(WorkEntry *w);
    int WorkReceipt(Terminal *t, Employee *e, Report *r);
    int FigureLabor(Settings *s, TimeInfo &start, TimeInfo &end, int job,
                    int &mins, int &cost, int &otmins, int &otcost);
};


// Will replace LaborPeriod & LaborDB
constexpr int WORK_VERSION = 1;

class WorkDB
{
    DList<WorkEntry> work_list;

public:
    Archive *archive;
    Str      filename;

    // Constructor
    WorkDB();

    // Member Functions
    WorkEntry *WorkList()    { return work_list.Head(); }
    WorkEntry *WorkListEnd() { return work_list.Tail(); }
    int        WorkCount()   { return work_list.Count(); }

    int Add(WorkEntry *we);
    int Remove(WorkEntry *we);
    int Purge();
    int Load(const char* file);
    int Save();
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
};

#endif
