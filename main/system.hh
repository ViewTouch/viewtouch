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
 * system.hh - revision 70 (10/6/98)
 * Data storage & report generation for current/previous business days
 */

#ifndef _SYSTEM_HH
#define _SYSTEM_HH

#include "tips.hh"
#include "labor.hh"
#include "employee.hh"
#include "sales.hh"
#include "inventory.hh"
#include "exception.hh"
#include "settings.hh"
#include "account.hh"
#include "list_utility.hh"
#include "archive.hh"
#include "expense.hh"
#include <string>
#include <array>
#include <memory>

#define CC_REPORT_NORMAL  1
#define CC_REPORT_INIT    2
#define CC_REPORT_TOTALS  3
#define CC_REPORT_DETAILS 4
#define CC_REPORT_VOIDS   5
#define CC_REPORT_REFUNDS 6
#define CC_REPORT_EXCEPTS 7
#define CC_REPORT_BATCH   8
#define CC_REPORT_SAF     9
#define CC_REPORT_FINISH  10

/**** Types ****/
class Check;
class Drawer;
class Terminal;
class Archive;
class InputDataFile;
class OutputDataFile;
class Expenses;
class ReportZone;

struct BatchItem {
    BatchItem *next;
    long long batch;

    BatchItem(long long newitem)
        {
            batch = newitem;
        }
};

//NOTE: MediaList is defined in system_report.cc, not system.cc
class MediaList
{
public:
    MediaList *next;
    std::string name;
    int total;
    std::array<int, MAX_SHIFTS> shift_total{};

    MediaList();
    MediaList(const std::string& namestr, int value);
    MediaList(const std::string& namestr, int value, int shift);
    ~MediaList();
    int Add(const std::string& namestr, int value, int shift = -1);
    int Total(int shift = -1);
    int Print();
} ;

class System
{
    DList<Archive> archive_list;
    DList<Check>   check_list;
    DList<Drawer>  drawer_list;

    int CheckFileUpdate(const char* file);

public:
    Str archive_path;
    Str current_path;
    Str backup_path;
    int last_archive_id;
    int last_serial_number;
    int report_sort_method;
    int report_detail;         // how much detail to show on the reports
    int column_spacing;        // for reports;

    // phrases_changed keeps track of the last time a user edited the phrase
    // translations so that zones can track whether they need to refresh any
    // textual information.
    unsigned long phrases_changed;

    int non_eod_settle;
    Terminal *eod_term;

    // Current Data
    Str       data_path; // directory containing system data
    Str       temp_path; // directory for temporary file storage
    TimeInfo  start;     // time system started

    Settings         settings;
    TipDB            tip_db;
    WorkDB           work_db;
    UserDB           user_db;
    ExceptionDB      exception_db;
    LaborDB          labor_db;  // FIX - obsolete - move everything to work_db
    ItemDB           menu;
    Inventory        inventory;
    AccountDB        account_db;
    ExpenseDB        expense_db;
    CustomerInfoDB   customer_db;
    CDUStrings       cdustrings;

    // Credit Card Stuff
    std::unique_ptr<CreditDB>        cc_void_db;
    std::unique_ptr<CreditDB>        cc_exception_db;
    std::unique_ptr<CreditDB>        cc_refund_db;
    std::unique_ptr<CCInit>          cc_init_results;
    std::unique_ptr<CCDetails>       cc_details_results;
    std::unique_ptr<CCDetails>       cc_totals_results;
    std::unique_ptr<CCSAFDetails>    cc_saf_details_results;
    std::unique_ptr<CCSettle>        cc_settle_results;
    int              cc_report_type;
    int              cc_processing;  // flag to prevent multiple transactions simultaneously
    Credit          *cc_finish;
    SList<BatchItem> BatchList;

    // Constructor
    System();
    ~System();

    // Member Functions
    Archive *ArchiveList()    { return archive_list.Head(); }
    Archive *ArchiveListEnd() { return archive_list.Tail(); }
    Check   *CheckList()      { return check_list.Head(); }
    Check   *CheckListEnd()   { return check_list.Tail(); }
    Drawer  *DrawerList()     { return drawer_list.Head(); }
    Drawer  *DrawerListEnd()  { return drawer_list.Tail(); }

    int SetDataPath(const char* path);
    // specify directory where system data is kept
    int CheckFileUpdates();
    genericChar* FullPath(const char* filename, genericChar* buffer = NULL);
    // returns string containing full filename for system datafile
    int LoadCurrentData(const char* path);
    // loads current day's data ('current' directory)
    int BackupCurrentData();
    int ScanArchives(const char* path, const char* altmedia);
    // Loads all archive headers
    int UnloadArchives();
    // purges all archive info from memory
    int InitCurrentDay();
    // call after current data is loaded
    int Add(Archive *archive);
    int Remove(Archive *archive);
    Archive *NewArchive();
    // start a new archive
    Archive *FindByTime(const TimeInfo &tm);
    // finds archive containing time
    Archive *FindByStart(TimeInfo &tm);
    // finds 1st archive starting at or after time
    int SaveChanged();
    // save all archive with change flag set
    int EndDay();
    // archives current day
    int LastEndDay();
    // returns the number of hours since the last end of day
    int CheckEndDay(Terminal *term);
    int ClearSystem(int all = 1);
    // removes all check, archive & work data
    int NewSerialNumber();
    // returns a unique id number for checks & drawers
    char* NewPrintFile( char* str );
    // returns filename to be used for output buffer
    void ClearCapturedTips(TimeInfo &start_time, TimeInfo &end_time, Archive *archive);
    // clear 'captured tips held' (from checks and tip DB)

    // Check functions
    int Add(Check *check);
    // adds check to current data
    int Remove(Check *check);
    // removes check from current check list (doesn't delete)
    Check *FirstCheck(Archive *archive = NULL);
    // returns first check of archive or current checks
    int CountOpenChecks(Employee *e = NULL);
    // counts open checks owned by user (or by everyone)
    int NumberStacked(const char* table, Employee *e);
    // Returns number of open checks at tables
    Check *FindOpenCheck(const char* table, Employee *e);
    // Finds open check by table number & training status
    Check *FindCheckByID(int check_id);
    Check *ExtractOpenCheck(Check *check);
    // Pulls out open subs as new check
    int SaveCheck(Check *check);
    // saves check to file
    int DestroyCheck(Check *check);
    // Deletes a check from memory (& disk for current checks)

    // Drawer functions
    int Add(Drawer *drawer);
    // adds drawer to current data
    int Remove(Drawer *drawer);
    // removes drawer from current drawer list (doesn't delete)
    Drawer *FirstDrawer(Archive *archive = NULL);
    // returns first drawer of archive or current drawers
    Drawer *GetServerBank(Employee *e);
    // returns server bank for user (creates new one if needed)
    int CreateFixedDrawers();
    // scans hardware and creates drawer objects for all defined drawers
    int SaveDrawer(Drawer *drawer);
    // writes drawer object to disk
    int CountDrawersOwned(int user_id);
    // returns number of drawers curently owned by user
    int AllDrawersPulled();
    // boolean - are all drawers pulled or balanced?

    // Exception functions
    ItemException *FirstItemException(Archive *archive = NULL);
    // returns first item exception of archive or current exceptions
    TableException *FirstTableException(Archive *archive = NULL);
    // returns first table exception of archive or current exceptions
    RebuildException *FirstRebuildException(Archive *archive = NULL);
    // returns first rebuild exception of archive or current exceptions

    // report functions (see system_report.cc)
    int SalesMixReport(Terminal *term, const TimeInfo &start, const TimeInfo &end,
                       Employee *e, Report *report); // see system_salesmix.cc
    int ServerReport(Terminal *term, TimeInfo &start, TimeInfo &end,
                     Employee *e, Report *report);
    int ShiftBalanceReport(Terminal *term, TimeInfo &ref, Report *report);
    int DepositReport(Terminal *term, TimeInfo &start, TimeInfo &end,
                      Archive *archive, Report *report);
    int BalanceReport(Terminal *term, TimeInfo &start, TimeInfo &end, Report *report);
    int ClosedCheckReport(Terminal *term, TimeInfo &start, TimeInfo &end,
                          Employee *e, Report *report);
    int ItemExceptionReport(Terminal *term, TimeInfo &start, TimeInfo &end,
                            int view, Employee *e, Report *report);
    int TableExceptionReport(Terminal *term, TimeInfo &start, TimeInfo &end,
                             Employee *e, Report *report);
    int RebuildExceptionReport(Terminal *term, TimeInfo &start, TimeInfo &end,
                               Employee *e, Report *report);
    int DrawerSummaryReport(Terminal *term, Drawer *drawer_list, Check *check_list,
                            Report *report);
    int CustomerDetailReport(Terminal *term, Employee *e, Report *report);
    int ExpenseReport(Terminal *term, TimeInfo &start, TimeInfo &end,
                      Archive *archive, Report *report, ReportZone *rzone);
    int RoyaltyReport(Terminal *term, TimeInfo &start_time, TimeInfo &end_time,
                      Archive *archive, Report *report, ReportZone *rzone);
    int AuditingReport(Terminal *term, TimeInfo &start_time, TimeInfo &end_time,
                       Archive *archive, Report *report, ReportZone *rzone);
    int CreditCardReport(Terminal *term, TimeInfo &start_time, TimeInfo &end_time,
                         Archive *archive, Report *report, ReportZone *rzone);
    int AddBatch(long long batchnum);
};


/**** Globals ****/
extern System *MasterSystem;

#endif
