#pragma once  // REFACTOR: Replaced #ifndef VT_MANAGER_HH guard with modern pragma once

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
 * manager.hh - revision 136 (8/10/98)
 * Functions for managing control flow in the application and
 * Standard POS utility functions
 */

#include "utility.hh"
#include "list_utility.hh"
#include <string>

#define MASTER_USER_DB       "employee.dat"
#define MASTER_MENU_DB       "menu.dat"
#define MASTER_SETTINGS      "settings.dat"
#define MASTER_DISCOUNTS     "media.dat"
#define MASTER_DISCOUNT_SAVE "media-archive.dat"
#define MASTER_SETTINGS_OLD  "settings-archives.dat"
#define MASTER_LOCALE        "locale.dat"
#define MASTER_TIP_DB        "tips.dat"
#define MASTER_INVENTORY     "inventory.dat"
#define MASTER_EXCEPTION     "exception.dat"
#define MASTER_ZONE_DB1      "tables.dat"
#define MASTER_ZONE_DB2      "zone_db.dat"
#define MASTER_ZONE_DB3      "vt_data"
#define MASTER_CDUSTRING     "cdustrings.dat"
#define ARCHIVE_DATA_DIR     "archive"
#define ACCOUNTS_DATA_DIR    "accounts"
#define BACKUP_DATA_DIR      "backups"
#define CURRENT_DATA_DIR     "current"
#define CUSTOMER_DATA_DIR    "customers"
#define EXPENSE_DATA_DIR     "expenses"
#define HTML_DATA_DIR        "html"
#define LABOR_DATA_DIR       "labor"
#define LANGUAGE_DATA_DIR    "languages"
#define PAGEEXPORTS_DIR      "pageexports"
#define PAGEIMPORTS_DIR      "pageimports"
#define STOCK_DATA_DIR       "stock"
#define TEXT_DATA_DIR        "text"
#define UPDATES_DATA_DIR     "updates"


/**** Types ****/
using TimeOutFn = void (*)();
using InputFn = void (*)();
using WorkFn = int (*)();

class Settings;
class Terminal;
class Printer;
class ZoneDB;
class Employee;
class MachineInfo;

class Control
{
    DList<Terminal> term_list;
    DList<Printer>  printer_list;

public:
    ZoneDB *zone_db;          // pointer to most current zone_db
    int     master_copy;      // boolean - is zone_db only pointer to object?

    Control();

    Terminal *TermList()       { return term_list.Head(); }
    Terminal *TermListEnd()    { return term_list.Tail(); }
    Printer  *PrinterList()    { return printer_list.Head(); }
    Printer  *PrinterListEnd() { return printer_list.Tail(); }

    int Add(Terminal *t);
    int Add(Printer *p);
    int Remove(Terminal *t);
    int Remove(Printer *p);
    Terminal *FindTermByHost(const char* host);
    int SetAllCursors(int cursor);
    int SetAllMessages(const char* message);
    int SetAllTimeouts(int timeout);
    int SetAllIconify(int iconify);
    int ClearAllMessages();
    int ClearAllFocus();
    int LogoutAllUsers();
    int LogoutKitchenUsers();
    int UpdateAll(int update_message, const genericChar* value);
    int UpdateOther(Terminal *local, int update_message, const genericChar* value);
    int IsUserOnline(Employee *e);
    int KillTerm(Terminal *t);
    int OpenDialog(const char* message);
    int KillAllDialogs();
    Printer *FindPrinter(const char* host, int port);
    Printer *FindPrinter(const char* term_name);
    Printer *FindPrinter(int printer_type);
    Printer *NewPrinter(const char* host, int port, int model);
    Printer *NewPrinter(const char* term_name, const char* host, int port, int model);
    int KillPrinter(Printer *p, int update = 0);
    int TestPrinters(Terminal *t, int report);

    // FIX - these functions should be moved to System
    ZoneDB *NewZoneDB();
    int SaveMenuPages();
    int SaveTablePages();
};


/**** Functions ****/
void ViewTouchError(const char* message, int do_sleep = 1);  // reports an error with contact info

int EndSystem();                     // Closes down the application & saves states
int RestartSystem();                 // Sets up a system where ViewTouch will be nicely shut down and restarted.
int KillTask(const char* name);            // kills all tasks matching name
int ReportError(const std::string &message); // error logging & reporting function
int ReportLoader(const char* message);     // gives a message to the loader program if it is still active

char* PriceFormat(Settings *s, int price, int use_sign, int use_comma,
                  genericChar* buffer = NULL); // formats price into string
int ParsePrice(const char* source, int *val = NULL); // returns price value from given string

// Load/Save system pages & default system data - 'vt_data' file
// (i.e. information specific to all pos systems)
int LoadSystemData();
int SaveSystemData();

// Load/Save application pages
// (i.e. information specific to a store chain)
int LoadAppData();
int SaveAppData();

// Load/Save table pages
// (i.e. information specific to one store)
int LoadLocalData();
int SaveLocalData();

// Add/Remove timeout function
unsigned long AddTimeOutFn(TimeOutFn fn, int time, void *client_data);
int RemoveTimeOutFn(unsigned long fn_id);

// Add/Remove input watching function
unsigned long AddInputFn(InputFn fn, int device_no, void *client_data);
int RemoveInputFn(unsigned long fn_id);

// Add/Remove work function
unsigned long AddWorkFn(WorkFn fn, void *client_data);
int RemoveWorkFn(unsigned long fn_id);

// looks at local copy of fonts so requests don't go to term programs
// (layout functions should be moved to terms so these arn't needed here)
int GetFontSize(int font_id, int &w, int &h);
int GetTextWidth(const char* string, int len, int font_id);
const char* GetScalableFontName(int font_id);

/**** Global ****/
extern int ReleaseDay;
extern int ReleaseMonth;
extern int ReleaseYear;

extern Control     *MasterControl;   // terminal/printer control object
extern int          MachineID;       // planar id of system
extern MachineInfo *ThisMachineInfo; // MachineInfo for this system
extern int          AllowLogins;     // whether terms should permit logins

// commonly used strings
extern const genericChar* DayName[];
extern const genericChar* ShortDayName[];
extern const genericChar* MonthName[];
extern const genericChar* ShortMonthName[];
extern const genericChar* CheckRefName[];
extern int   CheckRefValue[];
extern const genericChar* TermTypeName[];
extern int   TermTypeValue[];
extern const genericChar* PrinterTypeName[];
extern int   PrinterTypeValue[];
