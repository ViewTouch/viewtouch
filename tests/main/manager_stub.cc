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
 * manager.cc - revision 252 (10/20/98)
 * Implementation of manager module
 */

                            // ViewTouch includes
#include "manager.hh"
#include "system.hh"
#include "check.hh"
#include "pos_zone.hh"
#include "terminal.hh"
#include "printer.hh"
#include "drawer.hh"
#include "data_file.hh"
#include "inventory.hh"
#include "employee.hh"
#include "labels.hh"
#include "labor.hh"
#include "settings.hh"
#include "locale.hh"
#include "credit.hh"
#include "debug.hh"
#include "socket.hh"
#include "version/vt_version_info.hh"

#include "conf_file.hh"
#include "date/date.h"      // helper library to output date strings with std::chrono

                            // Standard C++ libraries
#include <errno.h>          // system error numbers
#include <iostream>         // basic input and output controls (C++ alone contains no facilities for IO)
#include <fstream>          // basic file input and output
#include <unistd.h>         // standard symbolic constants and types
#include <sys/socket.h>     // main sockets header
#include <sys/stat.h>       // data returned by the stat() function
#include <sys/types.h>      // data types
#include <sys/un.h>         // definitions for UNIX domain sockets
#include <sys/utsname.h>    // system name structure
#include <sys/wait.h>       // declarations for waiting
#include <X11/Intrinsic.h>  // libXt provides the X Toolkit Intrinsics, an abstract widget library on which ViewTouch is based
#include <string>           // Introduces string types, character traits and a set of converting functions
#include <cctype>           // Declares a set of functions to classify and transform individual characters
#include <cstring>          // Functions for dealing with C-style strings — null-terminated arrays of characters; is the C++ version of the classic string.h header from C
#include <string>           // Strings in C++ are of the std::string variety
#include <csignal>          // C library to handle signals
#include <fcntl.h>          // File Control
#include <chrono>           // time durations and current clock

#ifdef DMALLOC
#include <dmalloc.h>
#endif

using namespace date; // for date conversion on streams

/**** System Globals ****/
int ReleaseYear  = 1998;
int ReleaseMonth = 10;
int ReleaseDay   = 20;

Control     *MasterControl = NULL;
int          MachineID = 0;

constexpr int CALLCTR_ERROR_NONE        = 0;
constexpr int CALLCTR_ERROR_BADITEM     = 1;
constexpr int CALLCTR_ERROR_BADDETAIL   = 2;

constexpr int CALLCTR_STATUS_INCOMPLETE = 0;
constexpr int CALLCTR_STATUS_COMPLETE   = 1;
constexpr int CALLCTR_STATUS_FAILED     = 2;

/**** System Data ****/

/*************************************************************
 * Calendar Values
 *************************************************************/
const char* DayName[] = { "Sunday", "Monday", "Tuesday", "Wednesday", 
                    "Thursday", "Friday", "Saturday", NULL};

const char* ShortDayName[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL};

const char* MonthName[] = { "January", "February", "March", "April", 
                      "May", "June", "July", "August", "September", 
                      "October", "November", "December", NULL};

const char* ShortMonthName[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};

/*************************************************************
 * Terminal Type values
 *************************************************************/
const char* TermTypeName[] = { 	"Normal", "Order Only", "Bar", "Bar2", 
                            "Fast Food", "Kitchen Video", "Kitchen Video2", NULL};

int TermTypeValue[] = { TERMINAL_NORMAL, TERMINAL_ORDER_ONLY,
                        TERMINAL_BAR, TERMINAL_BAR2,
                        TERMINAL_FASTFOOD, TERMINAL_KITCHEN_VIDEO,
                        TERMINAL_KITCHEN_VIDEO2, -1};

/*************************************************************
 * Printer Type values
 *************************************************************/
const char* PrinterTypeName[] = { "Kitchen 1", "Kitchen 2", "Kitchen 3", "Kitchen 4",
                            "Bar 1", "Bar 2", "Expediter", "Report",
                            "Credit Receipt", "Remote Order", NULL};

int PrinterTypeValue[] = { PRINTER_KITCHEN1, PRINTER_KITCHEN2,
                           PRINTER_KITCHEN3, PRINTER_KITCHEN4,
                           PRINTER_BAR1, PRINTER_BAR2,
                           PRINTER_EXPEDITER, PRINTER_REPORT,
                           PRINTER_CREDITRECEIPT, PRINTER_REMOTEORDER, -1};


/*************************************************************
 * Module Globals
 *************************************************************/
static XtAppContext App = 0;
static Display     *Dis = NULL;
static XFontStruct *FontInfo[32];
static int          FontWidth[32];
static int          FontHeight[32];
int                 LoaderSocket = 0;
int                 OpenTermPort = 10001;
int                 OpenTermSocket = -1;
int                 autoupdate = 0;

// run the user command on startup if it is available; after that,
// we'll only run it when we get SIGUSR2.  The 2 here indicates
// that we're just starting.  SIGUSR2 will set UserCommand to 1.
int                 UserCommand  = 2;  // see RunUserCommand() definition
int                 AllowLogins  = 1;
int                 UserRestart  = 0;

genericChar displaystr[STRLENGTH];
genericChar restart_flag_str[STRLENGTH];
int         use_net = 1;

struct FontDataType
{
    int id;
    int width;
    int height;
    const genericChar* font;
};

static FontDataType FontData[] =
{
    {FONT_TIMES_20,     9, 20, "-adobe-times-medium-r-normal--20-*-p-*"},
    {FONT_TIMES_24,    12, 24, "-adobe-times-medium-r-normal--24-*-p-*"},
    {FONT_TIMES_34,    15, 33, "-adobe-times-medium-r-normal--34-*-p-*"},
    {FONT_TIMES_20B,   10, 20, "-adobe-times-bold-r-normal--20-*-p-*"},
    {FONT_TIMES_24B,   12, 24, "-adobe-times-bold-r-normal--24-*-p-*"},
    {FONT_TIMES_34B,   16, 33, "-adobe-times-bold-r-normal--34-*-p-*"},
    {FONT_TIMES_14,     7, 14, "-adobe-times-medium-r-normal--14-*-p-*"},
    {FONT_TIMES_14B,    8, 14, "-adobe-times-bold-r-normal--14-*-p-*"},
    {FONT_TIMES_18,     9, 18, "-adobe-times-medium-r-normal--18-*-p-*"},
    {FONT_TIMES_18B,   10, 18, "-adobe-times-bold-r-normal--18-*-p-*"},
    {FONT_COURIER_18,  10, 18, "-adobe-courier-medium-r-normal--18-*-*-*-*-*-*-*"},
    {FONT_COURIER_18B, 10, 18, "-adobe-courier-bold-r-normal--18-*-*-*-*-*-*-*"},
    {FONT_COURIER_20,  10, 20, "-adobe-courier-medium-r-normal--20-*-*-*-*-*-*-*"},
    {FONT_COURIER_20B, 10, 20, "-adobe-courier-bold-r-normal--20-*-*-*-*-*-*-*"}
};

static XtIntervalId UpdateID = 0;   // update callback function id
static int LastMin  = -1;
static int LastHour = -1;
static int LastMeal = -1;
static int LastDay  = -1;

/*************************************************************
 * Definitions
 *************************************************************/
#define UPDATE_TIME 500
#define CDU_UPDATE_CYCLE 50

#ifdef DEBUG
#define OPENTERM_SLEEP 0
#define MAX_CONN_TRIES 1000
#else
#define OPENTERM_SLEEP 5
#define MAX_CONN_TRIES 10
#endif

#define FONT_COUNT (int)(sizeof(FontData)/sizeof(FontDataType))

#define RESTART_FLAG         ".restart_flag"

#define VIEWTOUCH_COMMAND   VIEWTOUCH_PATH "/bin/.viewtouch_command_file"
#define VIEWTOUCH_PINGCHECK VIEWTOUCH_PATH "/bin/.ping_check"

#define VIEWTOUCH_VTPOS     VIEWTOUCH_PATH "/bin/vtpos"
#define VIEWTOUCH_RESTART   VIEWTOUCH_PATH "/bin/vtrestart"

// downloaded script for auto update
#define VIEWTOUCH_UPDATE_COMMAND "/tmp/vt-update"
// command to download script; -nv=not verbose, -T=timeout seconds, -t=# tries, -O=output
#define VIEWTOUCH_UPDATE_REQUEST \
    "wget -nv -T 2 -t 2 https//www.viewtouch.com/vt_updates/vt-update -O " VIEWTOUCH_UPDATE_COMMAND

static const std::string VIEWTOUCH_CONFIG = VIEWTOUCH_PATH "/dat/.viewtouch_config";

// vt_data is back in bin/ after a brief stint in dat/
#define SYSTEM_DATA_FILE     VIEWTOUCH_PATH "/bin/" MASTER_ZONE_DB3


/*************************************************************
 * Prototypes
 *************************************************************/
void     Terminate(int signal);
void     UserSignal1(int signal);
void     UserSignal2(int signal);
void     UpdateSystemCB(XtPointer client_data, XtIntervalId *time_id);
int      StartSystem(int my_use_net);
int      RunUserCommand(void);
int      PingCheck();
int      UserCount();
int      RunEndDay(void);
int      RunMacros(void);
int      RunReport(const genericChar* report_string, Printer *printer);
Printer *SetPrinter(const genericChar* printer_description);
int      ReadViewTouchConfig();

genericChar* GetMachineName(genericChar* str = NULL, int len = STRLENGTH)
{
    FnTrace("GetMachineName()");
    struct utsname uts;
    static genericChar buffer[STRLENGTH];
    if (str == NULL)
        str = buffer;

    if (uname(&uts) == 0)
        strncpy(str, uts.nodename, STRLENGTH);
    else
        str[0] = '\0';
    return str;
}

void ViewTouchError(const char* message, int do_sleep)
{
    FnTrace("ViewTouchError()");
    genericChar errormsg[STRLONG];
    int sleeplen = (debug_mode ? 1 : 5);
    Settings *settings = &(MasterSystem->settings);

    if (settings->expire_message1.empty())
    {
        snprintf(errormsg, STRLONG, "%s\\%s\\%s", message,
             "Please contact support.", " 541-515-5913");
    }
    else
    {
        snprintf(errormsg, STRLONG, "%s\\%s\\%s\\%s\\%s", message,
                 settings->expire_message1.Value(),
                 settings->expire_message2.Value(),
                 settings->expire_message3.Value(),
                 settings->expire_message4.Value());
    }
    ReportLoader(errormsg);
    if (do_sleep)
        sleep(sleeplen);
}

/****
 * ReadViewTouchConfig:  This reads a global, very early configuration.
 *  Most settings should go into settings.dat and be configurable through the
 *  GUI.  However, in some cases we must access a setting too early to have
 *  read settings.dat.
 ****/
int ReadViewTouchConfig()
{
    FnTrace("ReadViewTouchConfig()");
    int retval = 0;

    try
    {

        ConfFile conf(VIEWTOUCH_CONFIG, true);
        ReportError(
                    std::string("ReadViewTouchConfig: ")
                    + "Read early config from config file: "
                    + VIEWTOUCH_CONFIG);
        conf.GetValue(autoupdate, "autoupdate");
        conf.GetValue(select_timeout, "selecttimeout");
        conf.GetValue(debug_mode, "debugmode");
    } catch (const std::runtime_error &e) {
        ReportError(
                    std::string("ReadViewTouchConfig: ")
                    + "Failed to read early config from config file: "
                    + VIEWTOUCH_CONFIG);
        ReportError(
                    std::string("ReadViewTouchConfig: ")
                    + "Exception: " + e.what());
    }

    return retval;
}

// main function removed for testing

int ReportLoader(const char* message)
{
    FnTrace("ReportLoader()");
    if (LoaderSocket == 0)
        return 1;

    write(LoaderSocket, message, strlen(message)+1);
    return 0;
}

void Terminate(int my_signal)
{
    FnTrace("Terminate()");
    switch (my_signal)
    {
    case SIGINT:
        fprintf(stderr, "\n** Control-C pressed - System Terminated **\n");
        FnPrintTrace();
        exit(0);
        break;

    case SIGILL:
        ReportError("Illegal instruction");
        break;
    case SIGFPE:
        ReportError("Floating point exception");
        break;
    case SIGBUS:
        ReportError("Bus error");
        break;
    case SIGSEGV:
        ReportError("Memory segmentation violation");
        break;
    case SIGPIPE:
        ReportError("Broken Pipe");
        break;

    default:
    {
        genericChar str[256];
        sprintf(str, "Unknown my_signal %d received (ignored)", my_signal);
        ReportError(str);
        return;
    }
    }

    ReportError("** Fatal Error - Terminating System **");
    FnPrintTrace();
    exit(1);
}

void UserSignal1(int my_signal)
{
    FnTrace("UserSignal1()");
    UserRestart = 1;
}

void UserSignal2(int my_signal)
{
    FnTrace("UserSignal2()");
    UserCommand = 1;
}

int StartSystem(int my_use_net)
{
    FnTrace("StartSystem()");
    int i;
    genericChar altmedia[STRLONG];
    genericChar altsettings[STRLONG];

    System *sys = MasterSystem;

    sys->FullPath(RESTART_FLAG, restart_flag_str);
    unlink(restart_flag_str);

    sys->start = SystemTime;

    TimeInfo release;
    release.Set(0, ReleaseYear);
    if (SystemTime <= release)
    {
        printf("\nYour computer clock is in error.\n");
        printf("Please correct your system time before starting again.\n");
        return 1;
    }

    genericChar str[256];
    EnsureFileExists(sys->data_path.Value());
    if (DoesFileExist(sys->data_path.Value()) == 0)
    {
        sprintf(str, "Can't find path '%s'", sys->data_path.Value());
        ReportError(str);
        ReportLoader("POS cannot be started.");
        sleep(1);
        EndSystem();
    }

    sprintf(str, "Starting System on %s", GetMachineName());
    printf("Starting system:  %s\n", GetMachineName());
    ReportLoader(str);

    // Load Phrase Translation
    ReportLoader("Loading Locale Settings");
    sys->FullPath(MASTER_LOCALE, str);
    MasterLocale = new Locale;
    if (MasterLocale->Load(str))
    {
        RestoreBackup(str);
        MasterLocale->Purge();
        MasterLocale->Load(str);
    }

    // Load Settings
    ReportLoader("Loading General Settings");
    Settings *settings = &sys->settings;
    sys->FullPath(MASTER_SETTINGS, str);
    if (settings->Load(str))
    {
        RestoreBackup(str);
        settings->Load(str);
        // Now that we have the settings, we need to do some initialization
        sys->account_db.low_acct_num = settings->low_acct_num;
        sys->account_db.high_acct_num = settings->high_acct_num;
    }
    settings->Save();
    // Create alternate media file for old archives if it does not already exist
    sys->FullPath(MASTER_DISCOUNT_SAVE, altmedia);
    settings->SaveAltMedia(altmedia);
    // Create alternate settings for old archives.  We'll store the stuff that should
    // have been archived, like tax settings
    sys->FullPath(MASTER_SETTINGS_OLD, altsettings);
    settings->SaveAltSettings(altsettings);

    // Load Discount Settings
    sys->FullPath(MASTER_DISCOUNTS, str);
    if (settings->LoadMedia(str))
    {
        RestoreBackup(str);
        settings->Load(str);
    }

    XtToolkitInitialize();
    App = XtCreateApplicationContext();

    // Set up local fonts (only used for formating info)
    for (i = 0; i < 32; ++i)
    {
        FontInfo[i]   = NULL;
        FontWidth[i]  = 0;
        FontHeight[i] = 0;
    }

    for (i = 0; i < FONT_COUNT; ++i)
    {
        int f = FontData[i].id;
        FontWidth[f] = FontData[i].width;
        FontHeight[f] = FontData[i].height;
    }

    FontWidth[FONT_DEFAULT]  = FontWidth[FONT_TIMES_24];
    FontHeight[FONT_DEFAULT] = FontHeight[FONT_TIMES_24];

    int argc = 0;
    const genericChar* argv[] = {"vt_main"};
    Dis = XtOpenDisplay(App, displaystr, NULL, NULL, NULL, 0, &argc, (genericChar**)argv);
    if (Dis)
    {
        for (i = 0; i < FONT_COUNT; ++i)
        {
            int f = FontData[i].id;
            FontInfo[f] = XLoadQueryFont(Dis, FontData[i].font);
            if (FontInfo[f] == NULL)
            {
                sprintf(str, "Can't load font '%s'", FontData[i].font);
                ReportError(str);
            }
        }
        FontInfo[FONT_DEFAULT] = FontInfo[FONT_TIMES_24];
    }

    // Terminal & Printer Setup
    MasterControl = new Control;
    KillTask("vt_term");
    KillTask("vt_print");

    // Load System Data
    ReportLoader("Loading Application Data");
    LoadSystemData();

    // Add Remote terminals
    int num_terms = 16384; // old value of license DEFAULT_TERMINALS
    if (my_use_net)
    {
        // Only allow as many terminals as the license permits, subtracting 1
        // for the local terminal.
        int count = 0;
        int allowed = num_terms - 1;
        int have_server = settings->HaveServerTerm();
        TermInfo *ti = settings->TermList();
        if (have_server > 1)
        {
            int found = 0;
            while (ti != NULL)
            {
                if (ti->display_host.size() > 0)
                {
                    if (found)
                        ti->IsServer(0);
                    else
                    {
                        ti->display_host.Set(displaystr);
                        found = 1;
                    }
                }
                ti = ti->next;
            }
        }
        while (ti != NULL)
        {
            // this early, the TermInfo entry is the server entry if its
            // isserver value is true or if display_host is equal to
            // displaystr.  So we only start up a remote terminal if
            // IsServer() returns false and the two display strings do
            // not match.  Otherwise, we do a little background maintenance.
            if (ti->display_host.empty() && have_server == 0)
            {
                ti->display_host.Set(displaystr);
                ti->IsServer(1);
            }
            else if (ti->IsServer())
            {
                // make sure the server's display host value is current
                ti->display_host.Set(displaystr);
            }
            else if (strcmp(ti->display_host.Value(), displaystr))
            {
                if (count < allowed)
                {
                    sprintf(str, "Opening Remote Display '%s'", ti->name.Value());
                    ReportLoader(str);
                    ReportError(str);
                    ti->OpenTerm(MasterControl);
                    if (ti->next)
                        sleep(OPENTERM_SLEEP);
                }
                else
                {
                    printf("Not licensed to run terminal '%s'\n", ti->name.Value());
                }
            }
            else if (have_server == 0)
            {
                // this entry isn't explicitly set as server, but we got a match on
                // the display string, so we'll set it now.
                ti->IsServer(1);
            }
            ti = ti->next;
        }
    }

	char msg[256]; //char string used for file load messages

    // Load Archive & Create System Object
    ReportLoader("Scanning Archives");
    sys->FullPath(ARCHIVE_DATA_DIR, str);
    sys->FullPath(MASTER_DISCOUNT_SAVE, altmedia);
    if (sys->ScanArchives(str, altmedia))
        ReportError("Can't scan archives");

    // Load Employees
	sprintf(msg, "Attempting to load file %s...", MASTER_USER_DB);
	ReportError(msg); //stamp file attempt in log
    ReportLoader("Loading Employees");
    sys->FullPath(MASTER_USER_DB, str);
    if (sys->user_db.Load(str))
    {
        RestoreBackup(str);
        sys->user_db.Purge();
        sys->user_db.Load(str);
    }
    // set developer key (this should be done somewhere else)
    sys->user_db.developer->key = settings->developer_key;
	sprintf(msg, "%s OK", MASTER_USER_DB);
	ReportError(msg); //stamp file attempt in log

    // Load Labor
    sprintf(msg, "Attempting to load labor info...");
    ReportLoader(msg);
    sys->FullPath(LABOR_DATA_DIR, str);
    if (sys->labor_db.Load(str))
        ReportError("Can't find labor directory");

    // Load Menu
	sprintf(msg, "Attempting to load file %s...", MASTER_MENU_DB);
	ReportError(msg); //stamp file attempt in log
    ReportLoader("Loading Menu");
    sys->FullPath(MASTER_MENU_DB, str);
    if (sys->menu.Load(str))
    {
        RestoreBackup(str);
        sys->menu.Purge();
        sys->menu.Load(str);
    }
	sprintf(msg, "%s OK", MASTER_MENU_DB);
	ReportError(msg); //stamp file attempt in log

    // Load Exceptions
	sprintf(msg, "Attempting to load file %s...", MASTER_EXCEPTION);
	ReportError(msg); //stamp file attempt in log
    ReportLoader("Loading Exception Records");
    sys->FullPath(MASTER_EXCEPTION, str);
    if (sys->exception_db.Load(str))
    {
        RestoreBackup(str);
        sys->exception_db.Purge();
        sys->exception_db.Load(str);
    }
	sprintf(msg, "%s OK", MASTER_EXCEPTION);
	ReportError(msg); //stamp file attempt in log

    // Load Inventory
	sprintf(msg, "Attempting to load file %s...", MASTER_INVENTORY);
	ReportError(msg); //stamp file attempt in log
    ReportLoader("Loading Inventory");
    sys->FullPath(MASTER_INVENTORY, str);
    if (sys->inventory.Load(str))
    {
        RestoreBackup(str);
        sys->inventory.Purge();
        sys->inventory.Load(str);
    }
    sys->inventory.ScanItems(&sys->menu);
    sys->FullPath(STOCK_DATA_DIR, str);
    sys->inventory.LoadStock(str);
	sprintf(msg, "%s OK", MASTER_INVENTORY);
	ReportError(msg); //stamp file attempt in log

    // Load Customers
    sys->FullPath(CUSTOMER_DATA_DIR, str);
    ReportLoader("Loading Customers");
    sys->customer_db.Load(str);

    // Load Checks & Drawers
    sys->FullPath(CURRENT_DATA_DIR, str);
    ReportLoader("Loading Current Checks & Drawers");
    sys->LoadCurrentData(str);

    // Load Accounts
    sys->FullPath(ACCOUNTS_DATA_DIR, str);
    ReportLoader("Loading Accounts");
    sys->account_db.Load(str);

    // Load Expenses
    sys->FullPath(EXPENSE_DATA_DIR, str);
    ReportLoader("Loading Expenses");
    sys->expense_db.Load(str);
    sys->expense_db.AddDrawerPayments(sys->DrawerList());

    // Load Customer Display Unit strings
    sys->FullPath(MASTER_CDUSTRING, str);
    sys->cdustrings.Load(str);

    // Load Credit Card Exceptions, Refunds, and Voids
    ReportLoader("Loading Credit Card Information");
    sys->cc_exception_db->Load(MASTER_CC_EXCEPT);
    sys->cc_refund_db->Load(MASTER_CC_REFUND);
    sys->cc_void_db->Load(MASTER_CC_VOID);
    sys->cc_settle_results->Load(MASTER_CC_SETTLE);
    sys->cc_init_results->Load(MASTER_CC_INIT);
    sys->cc_saf_details_results->Load(MASTER_CC_SAF);

    // Start work/report printers
    int have_report = 0;
    PrinterInfo *pi;
    for (pi = settings->PrinterList(); pi != NULL; pi = pi->next)
    {
        if (my_use_net || pi->port == 0)
        {
            pi->OpenPrinter(MasterControl);
            if (pi->type == PRINTER_REPORT)
                have_report = 1;
        }
    }
    // Create a report printer if we do not already have one.
    // Defaults:  print to HTML, file:/<dat dir>/html/
    if (have_report < 1)
    {
        genericChar prtstr[STRLONG];
        PrinterInfo *report_printer = new PrinterInfo;
        report_printer->name.Set("Report Printer");
        sys->FullPath("html", str);
        snprintf(prtstr, STRLONG, "file:%s/", str);
        report_printer->host.Set(prtstr);
        report_printer->model = MODEL_HTML;
        report_printer->type = PRINTER_REPORT;
        settings->Add(report_printer);
        report_printer->OpenPrinter(MasterControl);
    }

    // Add local terminal
    ReportLoader("Opening Local Terminal");
    TermInfo *ti = settings->FindServer(displaystr);
    ti->display_host.Set(displaystr);

    pi = settings->FindPrinterByType(PRINTER_RECEIPT);
    if (pi)
    {
        ti->printer_host.Set(pi->host);
        ti->printer_port  = pi->port;
        ti->printer_model = pi->model;

        settings->Remove(pi);
        delete pi;
        settings->Save();
    }

    if (num_terms > 0)
        ti->OpenTerm(MasterControl);
    else
        ViewTouchError("No terminals allowed.");

    if (MasterControl->TermList() == NULL)
    {
        ReportError("No terminals could be opened");
        EndSystem();
    }

    Terminal *term = MasterControl->TermList();
    while (term != NULL)
    {
        term->Initialize();
        term = term->next;
    }

    // Cleanup/Init & start
    sys->InitCurrentDay();

    // Start update system timer
    UpdateID = XtAppAddTimeOut(App, UPDATE_TIME,
                               (XtTimerCallbackProc) UpdateSystemCB, NULL);

    // Break connection with loader
    if (LoaderSocket)
    {
        write(LoaderSocket, "done", 5); // should cause loader to quit
        close(LoaderSocket);
        LoaderSocket = 0;
    }

    if (UserCommand)
        RunUserCommand();

    if (my_use_net)
        OpenTermSocket = Listen(OpenTermPort);

    // Event Loop
    XEvent event;
    for (;;)
    {
        XtAppNextEvent(App, &event);
        switch (event.type)
        {
        case MappingNotify:
            XRefreshKeyboardMapping((XMappingEvent *) &event);
            break;
        }
        XtDispatchEvent(&event);
    }
    return 0;
}

int EndSystem()
{
    FnTrace("EndSystem()");
    // Make sure this function is only called once
    static int flag = 0;
    ++flag;
    if (flag >= 2)
    {
        ReportError("Terminating without clean up - fatal error!");
        exit(0);
    }

    // The begining of the end
    if (MasterControl)
    {
        Terminal *term = MasterControl->TermList();
        while (term != NULL)
        {
            if (term->cdu != NULL)
                term->cdu->Clear();
            term = term->next;
        }
        MasterControl->SetAllMessages("Shutting Down.");
        MasterControl->SetAllCursors(CURSOR_WAIT);
        MasterControl->LogoutAllUsers();
    }
    if (UpdateID)
    {
        XtRemoveTimeOut(UpdateID);
        UpdateID = 0;
    }
    if (Dis)
    {
        XtCloseDisplay(Dis);
        Dis = NULL;
    }
    if (App)
    {
        XtDestroyApplicationContext(App);
        App = 0;
    }

    // Save Archive/Settings Changes
    Settings *settings = &MasterSystem->settings;
    if (settings->changed)
    {
        settings->Save();
        settings->SaveMedia();
    }
    if (MasterSystem)
        MasterSystem->SaveChanged();
    MasterSystem->cc_exception_db->Save();
    MasterSystem->cc_refund_db->Save();
    MasterSystem->cc_void_db->Save();
    MasterSystem->cc_settle_results->Save();
    MasterSystem->cc_init_results->Save();
    MasterSystem->cc_saf_details_results->Save();

    // Delete databases
    if (MasterControl != NULL)
    {
        // Deleting MasterControl keeps giving me error messages:
        //     "vt_main in free(): warning: chunk is already free"
        // I'm tired of the error messages and don't want to take
        // the time right now to fix it, so I'm commenting it out.
        // There's no destructor, so this step shouldn't be necessary
        // anyway.
        // delete MasterControl;
        MasterControl = NULL;
    }
    if (MasterSystem)
    {
        delete MasterSystem;
        MasterSystem = NULL;
    }
    ReportError("EndSystem:  Normal shutdown.");

    // Kill all spawned tasks
    KillTask("vt_term");
    KillTask("vt_print");
    KillTask("vtpos");

    // Make sure loader connection is killed
    if (LoaderSocket)
    {
        write(LoaderSocket, "done", 5);
        close(LoaderSocket);
        LoaderSocket = 0;
    }

    // create flag file for restarts
    int fd = open(restart_flag_str, O_CREAT | O_TRUNC | O_WRONLY, 0700);
    write(fd, "1", 1);
    close(fd);

    // The end
    unlink(LOCK_RUNNING);
    exit(0);
    return 0;
}

/****
 * RestartSystem: To start, we'll just use a simple method of restarting.  We'll
 *  simply set up a shell script to be called by atd.  The script will loop looking
 *  for the restart flag file.  Just before EndSystem exits, it will create the
 *  restart flag file.
 ****/
int RestartSystem()
{
    FnTrace("RestartSystem()");
    pid_t pid;

    if (OpenTermSocket > -1)
        close(OpenTermSocket);

    if (debug_mode)
        printf("Forking for RestartSystem\n");
    pid = fork();
    if (pid < 0)
    {  // error
        EndSystem();
    }
    else if (pid == 0)
    {  // child
        // Here we want to exec a script that will wait for EndSystem() to
        // complete and then start vtpos all over again with the exact
        // same arguments.
        execl(VIEWTOUCH_RESTART, VIEWTOUCH_RESTART, VIEWTOUCH_PATH, NULL);
    }
    else
    {  // parent
        EndSystem();
    }
    return 0;
}

int KillTask(const char* name)
{
    FnTrace("KillTask()");
    genericChar str[STRLONG];

    snprintf(str, STRLONG, KILLALL_CMD " %s >/dev/null 2>/dev/null", name);
    system(str);
    return 0;
}

char* PriceFormat(Settings *settings, int price, int use_sign, int use_comma, genericChar* str)
{
    FnTrace("PriceFormat()");
    static genericChar buffer[32];
    if (str == NULL)
        str = buffer;

    genericChar point = '.';
    genericChar comma = ',';
    if (settings->number_format == NUMBER_EURO)
    {
        point = ',';
        comma = '.';
    }

    int change  = Abs(price) % 100;
    int dollars = Abs(price) / 100;

    genericChar dollar_str[32];
    if (use_comma && dollars > 999999){
        sprintf(dollar_str, "%d%c%03d%c%03d",
                dollars / 1000000, comma,
                (dollars / 1000) % 1000, comma,
                dollars % 1000);
	}
    else if (use_comma && dollars > 999)
        sprintf(dollar_str, "%d%c%03d", dollars / 1000, comma, dollars % 1000);
    else if (dollars > 0)
        sprintf(dollar_str, "%d", dollars);
    else
        dollar_str[0] = '\0';

    if (use_sign)
    {
        if (price < 0)
            sprintf(str, "%s-%s%c%02d", settings->money_symbol.Value(),
                    dollar_str, point, change);
        else
            sprintf(str, "%s%s%c%02d", settings->money_symbol.Value(),
                    dollar_str, point, change);
    }
    else
    {
        if (price < 0)
            sprintf(str, "-%s%c%02d", dollar_str, point, change);
        else
            sprintf(str, "%s%c%02d", dollar_str, point, change);
    }
    return str;
}

int ParsePrice(const char* source, int *value)
{
    FnTrace("ParsePrice()");
    genericChar str[256];
    genericChar* c = str;
    int numformat = MasterSystem->settings.number_format;

    if (*source == '-')
    {
        *c++ = *source;
        ++source;
    }
    while (*source)
    {
        if (*source >= '0' && *source <= '9')
            *c++ = *source;
        else if (*source == '.' && numformat == NUMBER_STANDARD)
            *c++ = '.';
        else if (*source == ',' && numformat == NUMBER_EURO)
            *c++ = '.';
        ++source;
    }
    *c = '\0';

    Flt val;
    if (sscanf(str, "%lf", &val) != 1)
        return 1;
    int v = FltToPrice(val);
    if (value)
        *value = v;
    return v;
}

/*************************************************************
 * System Data Functions
 *************************************************************/

/****
 * FindVTData:  Should be in bin/ directory, but for compatibility check in
 *  current data path if not there.
 *
 *  Opens the file if found.  Returns the version of the file, or -1 on error.
 ****/
int FindVTData(InputDataFile *infile)
{
    FnTrace("FindVTData()");
    int version = -1;

    // try official location
    fprintf(stderr, "Trying VT_DATA: %s\n", SYSTEM_DATA_FILE);
    if (infile->Open(SYSTEM_DATA_FILE, version) == 0)
        return version;

    // fallback, try current data path
    const char *vt_data_path = MasterSystem->FullPath("vt_data");
    fprintf(stderr, "Trying VT_DATA: %s\n", vt_data_path);
    if (infile->Open(vt_data_path, version) == 0)
        return version;

    return -1;
}

int LoadSystemData()
{
    FnTrace("LoadSystemData()");
    int i;
    // VERSION NOTES
    // 1 (future) initial version of unified system.dat

    System  *sys = MasterSystem;
    Control *con = MasterControl;
    if (con->zone_db)
    {
        ReportError("system data already loaded");
        return 1;
    }

    int version = 0;
    InputDataFile df;
    version = FindVTData(&df);
    if (version < 0)
    {
        fprintf(stderr, "Unable to find vt_data file!!!\n");
        return 1;
    }

    if (version < 1 || version > 1)
    {
        ReportError("Unsupported version of system data");
        return 1;
    }

    // Read System Page Data
    Page *p = NULL;
    int zone_version = 0, count = 0;
    ZoneDB *zone_db = new ZoneDB;
    df.Read(zone_version);
    df.Read(count);
    for (i = 0; i < count; ++i)
    {
        p = NewPosPage();
        p->Read(df, zone_version);
        zone_db->Add(p);
    }

    // Read Default Accounts Data
    Account *ac;
    int account_version = 0;
    int no = 0;
    count = 0;
    df.Read(account_version);
    df.Read(count);
    for (i = 0; i < count; ++i)
    {
        df.Read(no);
        ac = new Account(no);
        df.Read(ac->name);
        sys->account_db.AddDefault(ac);
    }

    // Done with vt_data file
    df.Close();

    // Load Tables
    genericChar filename1[256];
    sprintf(filename1, "%s/%s", sys->data_path.Value(), MASTER_ZONE_DB1);

    if (zone_db->Load(filename1))
    {
        RestoreBackup(filename1);
        //zone_db->Purge();	// maybe remove non-system pages, but not all!
        zone_db->Load(filename1);
    }

    // Load Menu
    genericChar filename2[256];
    sprintf(filename2, "%s/%s", sys->data_path.Value(), MASTER_ZONE_DB2);
    if (zone_db->Load(filename2))
    {
        RestoreBackup(filename2);
        //zone_db->Purge();
        zone_db->Load(filename1);
        zone_db->Load(filename2);
    }

    con->master_copy = 0;
    con->zone_db = zone_db;

    // Load any new imports
    if (zone_db->ImportPages() > 0)
    {
        // SaveSystemData(); // disabled, only save on edit now
        con->SaveMenuPages();
        con->SaveTablePages();
    }

    return 0;
}

int SaveSystemData()
{
    FnTrace("SaveSystemData()");

    // Save version 1
    System  *sys = MasterSystem;
    Control *con = MasterControl;
    if (con->zone_db == NULL)
        return 1;

    BackupFile(SYSTEM_DATA_FILE);	// always save to normal location
    OutputDataFile df;
    if (df.Open(SYSTEM_DATA_FILE, 1, 1))
        return 1;

    // Write System Page Data
    int count = 0;
    Page *p = con->zone_db->PageList();
    while (p)
    {
        if (p->id < 0)
            ++count;
        p = p->next;
    }

    df.Write(ZONE_VERSION);  // see pos_zone.cc for notes on version
    df.Write(count, 1);
    p = con->zone_db->PageList();
    while (p)
    {
        if (p->id < 0)
            p->Write(df, ZONE_VERSION);
        p = p->next;
    }

    // Write Default Accounts Data
    count = 0;
    Account *ac = sys->account_db.DefaultList();
    while (ac)
    {
        ++count;
        ac = ac->next;
    }

    df.Write(1);
    df.Write(count, 1);
    ac = sys->account_db.DefaultList();
    while (ac)
    {
        df.Write(ac->number);
        df.Write(ac->name);
        ac = ac->next;
    }
    return 0;
}

/*************************************************************
 * Control Class
 *************************************************************/
Control::Control()
{
    FnTrace("Control::Control()");
    zone_db     = NULL;
    master_copy = 0;
    term_list   = NULL;
}

int Control::Add(Terminal *term)
{
    FnTrace("Control::Add(Terminal)");
    if (term == NULL)
        return 1;

    term->system_data = MasterSystem;
    term_list.AddToTail(term);
    term->UpdateZoneDB(this);
    return 0;
}

int Control::Add(Printer *p)
{
    FnTrace("Control::Add(Printer)");

    if (p == NULL)
        return 1;

    p->parent = this;
    printer_list.AddToTail(p);
    return 0;
}

int Control::Remove(Terminal *term)
{
    FnTrace("Control::Remove(Terminal)");
    if (term == NULL)
        return 1;

    term->parent = NULL;
    term_list.Remove(term);

    if (zone_db == term->zone_db)
    {
        // Find new master zone_db for coping
        Terminal *ptr = TermList();
        while (ptr)
        {
            if (ptr->reload_zone_db == 0)
            {
				zone_db = ptr->zone_db;
				break;
            }
            ptr = ptr->next;
        }
        if (ptr == NULL)
            zone_db = NULL;
    }
    return 0;
}

int Control::Remove(Printer *p)
{
    FnTrace("Control::Remove(Printer)");
    if (p == NULL)
        return 1;

    p->parent = NULL;
    printer_list.Remove(p);
    return 0;
}

Terminal *Control::FindTermByHost(const char* host)
{
    FnTrace("Control::FindTermByHost()");

    for (Terminal *term = TermList(); term != NULL; term = term->next)
    {
        if (strcmp(term->host.Value(), host) == 0)
            return term;
    }

    return NULL;
}

int Control::SetAllMessages(const char* message)
{
    FnTrace("Control::SetAllMessages()");
    for (Terminal *term = TermList(); term != NULL; term = term->next)
        term->SetMessage(message);
    return 0;
}

int Control::SetAllTimeouts(int timeout)
{
    FnTrace("Control::SetAllTimeouts()");
    for (Terminal *term = TermList(); term != NULL; term = term->next)
        term->SetCCTimeout(timeout);
    return 0;
}

int Control::SetAllCursors(int cursor)
{
    FnTrace("Control::SetAllCursors()");
    for (Terminal *term = TermList(); term != NULL; term = term->next)
        term->SetCursor(cursor);
    return 0;
}

int Control::SetAllIconify(int iconify)
{
    FnTrace("Control::SetAllIconify()");
    for (Terminal *term = TermList(); term != NULL; term = term->next)
        term->SetIconify(iconify);
    return 0;
}

int Control::ClearAllMessages()
{
    FnTrace("Control::ClearAllMessages()");
    for (Terminal *term = TermList(); term != NULL; term = term->next)
        term->ClearMessage();
    return 0;
}

int Control::ClearAllFocus()
{
    FnTrace("Control::ClearAllFocus()");
    for (Terminal *term = TermList(); term != NULL; term = term->next)
        term->previous_zone = NULL;
    return 0;
}

int Control::LogoutAllUsers()
{
    FnTrace("Control::LogoutAllUsers()");
    for (Terminal *term = TermList(); term != NULL; term = term->next)
        term->LogoutUser();
    return 0;
}

int Control::LogoutKitchenUsers()
{
    FnTrace("Control::LogoutKitchenUsers()");
    Terminal *term = TermList();
    int count = 0;

    while (term != NULL)
    {
        if ((term->type == TERMINAL_KITCHEN_VIDEO ||
             term->type == TERMINAL_KITCHEN_VIDEO2) &&
            term->user)
        {
            count += 1;
            term->LogoutUser();
        }
        term = term->next;
    }

    return count;
}

int Control::UpdateAll(int update_message, const genericChar* value)
{
    FnTrace("Control::UpdateAll()");
    Terminal *term = TermList();

    while (term != NULL)
    {
        term->Update(update_message, value);
        term = term->next;
    }
    return 0;
}

int Control::UpdateOther(Terminal *local, int update_message, const genericChar* value)
{
    FnTrace("Control::UpdateOther()");
    for (Terminal *term = TermList(); term != NULL; term = term->next)
        if (term != local)
            term->Update(update_message, value);
    return 0;
}

int Control::IsUserOnline(Employee *e)
{
    FnTrace("Control::IsUserOnline()");
    if (e == NULL)
        return 0;

    for (Terminal *term = TermList(); term != NULL; term = term->next)
    {
        if (term->user == e)
            return 1;
    }
    return 0;
}

int Control::KillTerm(Terminal *term)
{
    FnTrace("Control::KillTerm()");
    Terminal *ptr = TermList();
    while (ptr)
    {
        if (term == ptr)
        {
            term->StoreCheck(0);
            Remove(term);
            delete term;
            UpdateAll(UPDATE_TERMINALS, NULL);
            return 0;
        }
        ptr = ptr->next;
    }
    return 1;  // invalid pointer given
}

int Control::OpenDialog(const char* message)
{
    FnTrace("Control::OpenDialog()");
    for (Terminal *term = TermList(); term != NULL; term = term->next)
        term->OpenDialog(message);
    return 0;
}

int Control::KillAllDialogs()
{
    FnTrace("Control::KillAllDialogs()");
    for (Terminal *term = TermList(); term != NULL; term = term->next)
        term->KillDialog();
    return 0;
}

Printer *Control::FindPrinter(const char* host, int port)
{
    FnTrace("Control::FindPrinter(const char* , int)");
    for (Printer *p = PrinterList(); p != NULL; p = p->next)
	{
        if (p->MatchHost(host, port))
            return p;
	}

    return NULL;
}

Printer *Control::FindPrinter(const char* term_name)
{
    FnTrace("Control::FindPrinter(const char* )");

    for (Printer *p = PrinterList(); p != NULL; p = p->next)
    {
        if (strcmp(p->term_name.Value(), term_name) == 0)
            return p;
    }

    return NULL;
}

Printer *Control::FindPrinter(int printer_type)
{
    FnTrace("Control::FindPrinter(int)");

    for (Printer *p = PrinterList(); p != NULL; p = p->next)
    {
        if (p->IsType(printer_type))
            return p;
    }

    return NULL;
}

/****
 * NewPrinter:  First we'll see if the specified printer already exists.
 *  If it doesn't, we'll create it, slipping i
 ****/
Printer *Control::NewPrinter(const char* host, int port, int model)
{
    FnTrace("Control::NewPrinter(const char* , int, int)");

    Printer *p = FindPrinter(host, port);
    if (p)
        return p;

    p = NewPrinterObj(host, port, model);
    Add(p);

    return p;
}

Printer *Control::NewPrinter(const char* term_name, const char* host, int port, int model)
{
    FnTrace("Control::NewPrinter(const char* , const char* , int, int)");

    Printer *p = FindPrinter(term_name);
    if (p)
        return p;
    p = NewPrinterObj(host, port, model);
    Add(p);

    return p;
}

int Control::KillPrinter(Printer *p, int update)
{
    FnTrace("Control::KillPrinter()");
    if (p == NULL)
        return 1;

    Printer *ptr = PrinterList();
    while (ptr)
    {
        if (ptr == p)
        {
            Remove(p);
            delete p;
            if (update)
                UpdateAll(UPDATE_PRINTERS, NULL);
            return 0;
        }
        ptr = ptr->next;
    }
    return 1;  // invalid pointer given
}

int Control::TestPrinters(Terminal *term, int report)
{

    FnTrace("Control::TestPrinters()");

    for (Printer *p = PrinterList(); p != NULL; p = p->next)
	{
        if ((p->IsType(PRINTER_REPORT) && report) ||
            (!p->IsType(PRINTER_REPORT) && !report))
		{
            p->TestPrint(term);
		}
	}
    return 0;
}

/****
 * NewZoneDB:  Creates a copy of the zone database.  This is normally called
 *   to create a zone database for each terminal at startup and after editing.
 *   NOTE BAK->master_copy is not currently used.  It was used, apparently,
 *   so that there would be only as many copies of the zone database as there
 *   were terminals, and the first terminal started would have the master
 *   copy.  However, this blocked any simple attempts at undoing edits for
 *   that terminal if that terminal was the only terminal alive; once the
 *   database was modified, you'd have to dump the program (somehow avoiding
 *   any efforts to save off the zones) and restart.  So now the Control object
 *   keeps the master copy and all terminals, including the first, get a copy.
 *   That means we'll always have one more zone database than we use, but
 *   the extra copy gives some added flexibility.
 ****/
ZoneDB *Control::NewZoneDB()
{
    FnTrace("Control::NewZoneDB()");
    if (zone_db == NULL)
        return NULL;

    ZoneDB *db;
    if (master_copy)
    {
        db = zone_db;
        master_copy = 0;
    }
    else
        db = zone_db->Copy();

    db->Init();
    return db;
}

int Control::SaveMenuPages()
{
    FnTrace("Control::SaveMenuPages()");
    System  *sys = MasterSystem;
    if (zone_db == NULL || sys == NULL)
        return 1;

    genericChar str[256];
    sprintf(str, "%s/%s", sys->data_path.Value(), MASTER_ZONE_DB2);
    BackupFile(str);
    return zone_db->Save(str, PAGECLASS_MENU);
}

int Control::SaveTablePages()
{
    FnTrace("Control::SaveTablePages()");
    System  *sys = MasterSystem;
    if (zone_db == NULL || sys == NULL)
        return 1;

    genericChar str[256];
    sprintf(str, "%s/%s", sys->data_path.Value(), MASTER_ZONE_DB1);
    BackupFile(str);
    return zone_db->Save(str, PAGECLASS_TABLE);
}


/*************************************************************
 * More Functions
 *************************************************************/
int GetTermWord(char* dest, int maxlen, const char* src, int sidx)
{
    FnTrace("GetTermWord()");
    int didx = 0;

    while (src[sidx] != '\0' && src[sidx] != ' ' && didx < maxlen)
    {
        dest[didx] = src[sidx];
        didx += 1;
        sidx += 1;
    }
    dest[didx] = '\0';
    if (src[sidx] == ' ')
        sidx += 1;

    return sidx;
}

int SetTermInfo(TermInfo *ti, const char* termname, const char* termhost, const char* term_info)
{
    FnTrace("SetTermInfo()");
    int  retval = 0;
    char termtype[STRLENGTH];
    char printhost[STRLENGTH];
    char printmodl[STRLENGTH];
    char numdrawers[STRLENGTH];
    int  idx = 0;

    idx = GetTermWord(termtype, STRLENGTH, term_info, idx);
    idx = GetTermWord(printhost, STRLENGTH, term_info, idx);
    idx = GetTermWord(printmodl, STRLENGTH, term_info, idx);
    idx = GetTermWord(numdrawers, STRLENGTH, term_info, idx);

    if (debug_mode)
    {
        printf("     Type:  %s\n", termtype);
        printf("    Prntr:  %s\n", printhost);
        printf("     Type:  %s\n", printmodl);
        printf("    Drwrs:  %s\n", numdrawers);
    }

    ti->name.Set(termname);
    if (termhost != NULL)
        ti->display_host.Set(termhost);
    if (strcmp(termtype, "kitchen") == 0)
        ti->type = TERMINAL_KITCHEN_VIDEO;
    else
        ti->type = TERMINAL_NORMAL;
    if (strcmp(printhost, "none"))
    {
        ti->printer_host.Set(printhost);
        if (strcmp(printmodl, "epson") == 0)
            ti->printer_model = MODEL_EPSON;
        else if (strcmp(printmodl, "star") == 0)
            ti->printer_model = MODEL_STAR;
        else if (strcmp(printmodl, "ithaca") == 0)
            ti->printer_model = MODEL_ITHACA;
        else if (strcmp(printmodl, "text") == 0)
            ti->printer_model = MODEL_RECEIPT_TEXT;
        ti->drawers = atoi(numdrawers);
    }

    return retval;
}

/****
 * OpenDynTerminal:  The command should have been in the form:
 *  openterm termname termhost [termtype printhost printmodel drawers]
 * As in:
 *  openterm Wincor wincor:0.0 normal file:/viewtouch/output epson 1
 * Or:
 *  openterm Wincor wincor:0.0
 * Send everything to this function except the "openterm " portion.
 ****/
int OpenDynTerminal(const char* remote_terminal)
{
    FnTrace("OpenDynTerminal()");
    int retval = 1;
    TermInfo *ti = NULL;
    char termname[STRLENGTH];
    char termhost[STRLENGTH];
    char update[STRLENGTH];
    char str[STRLENGTH];
    int idx = 0;
    Terminal *term;

    idx = GetTermWord(termname, STRLENGTH, remote_terminal, idx);
    idx = GetTermWord(termhost, STRLENGTH, remote_terminal, idx);
    idx = GetTermWord(update, STRLENGTH, remote_terminal, idx);
    if (debug_mode)
    {
        snprintf(str, STRLENGTH, "  Term Name:  %s", termname);
        ReportError(str);
        snprintf(str, STRLENGTH, "       Host:  %s", termhost);
        ReportError(str);
        snprintf(str, STRLENGTH, "     Update:  %s", update);
        ReportError(str);
    }

    if (termname[0] != '\0' && termhost[0] != '\0')
    {
        ti = MasterSystem->settings.FindTerminal(termhost);
        if (ti != NULL)
        {
            term = ti->FindTerm(MasterControl);
            if (term == NULL)
            {
                if (strcmp(update, "update") == 0)
                    SetTermInfo(ti, termname, NULL, &remote_terminal[idx]);
                ti->OpenTerm(MasterControl, 1);
            }
        }
        else
        {
            ti = new TermInfo();
            SetTermInfo(ti, termname, termhost, &remote_terminal[idx]);
            MasterSystem->settings.Add(ti);
            ti->OpenTerm(MasterControl, 1);
            retval = 0;
        }
    }

    return retval;
}

int CloseDynTerminal(const char* remote_terminal)
{
    FnTrace("CloseDynTerminal()");
    int retval = 1;
    char termhost[STRLENGTH];
    int idx = 0;
    Terminal *term = NULL;
    TermInfo *ti = NULL;
    Printer  *printer = NULL;

    idx = GetTermWord(termhost, STRLENGTH, remote_terminal, idx);
    ti = MasterSystem->settings.FindTerminal(termhost);
    if (ti != NULL)
    {
        term = ti->FindTerm(MasterControl);
        if (term)
        {
            // disable term
            term->kill_me = 1;
            printer = ti->FindPrinter(MasterControl);
            MasterControl->KillPrinter(printer, 1);
        }
    }

    return retval;
}

int CloneDynTerminal(const char* remote_terminal)
{
    FnTrace("CloneDynTerminal()");
    int retval = 1;
    char termhost[STRLENGTH];
    char clonedest[STRLENGTH];
    int idx = 0;
    Terminal *term = NULL;
    TermInfo *ti = NULL;

    idx = GetTermWord(termhost, STRLENGTH, remote_terminal, idx);
    idx = GetTermWord(clonedest, STRLENGTH, remote_terminal, idx);
    ti = MasterSystem->settings.FindTerminal(termhost);
    if (ti != NULL)
    {
        term = ti->FindTerm(MasterControl);
        if (term != NULL)
            retval = CloneTerminal(term, clonedest, termhost);
    }

    return retval;
}

int ProcessRemoteOrderEntry(SubCheck *subcheck, Order **order, const char* key, const char* value)
{
    FnTrace("ProcessRemoteOrderEntry()");
    int retval = CALLCTR_ERROR_NONE;
    static Order *detail = NULL;
    SalesItem *sales_item;
    int record;  // not really used; only for FindByItemCode

    if ((strncmp(key, "ItemCode", 8) == 0) ||
        (strncmp(key, "ProductCode", 11) == 0))
    {
        if (*order != NULL)
            ReportError("Have an order we should get rid of....");
        sales_item = MasterSystem->menu.FindByItemCode(value, record);
        if (sales_item)
            *order = new Order(&MasterSystem->settings, sales_item, NULL);
        else
            retval = CALLCTR_ERROR_BADITEM;
    }
    else if ((strncmp(key, "DetailCode", 10) == 0) ||
             (strncmp(key, "AddonCode", 9) == 0))
    {
        if (detail != NULL)
            ReportError("Have a detail we should get rid of....");
        sales_item = MasterSystem->menu.FindByItemCode(value, record);
        if (sales_item)
            detail = new Order(&MasterSystem->settings, sales_item, NULL);
        else
            retval = CALLCTR_ERROR_BADDETAIL;
    }
    else if ((strncmp(key, "EndItem", 7) == 0) ||
             (strncmp(key, "EndProduct", 10) == 0))
    {
        subcheck->Add(*order, &MasterSystem->settings);
        *order = NULL;
    }
    else if ((strncmp(key, "EndDetail", 9) == 0) ||
             (strncmp(key, "EndAddon", 8) == 0))
    {
        (*order)->Add(detail);
        detail = NULL;
    }
    else if (*order != NULL)
    {
        if (strncmp(key, "ItemQTY", 7) == 0)
            (*order)->count = atoi(value);
        else if (strncmp(key, "ProductQTY", 10) == 0)
            (*order)->count = atoi(value);
        else if (detail != NULL && strncmp(key, "AddonQualifier", 14) == 0)
            detail->AddQualifier(value);
    }
    else if (debug_mode)
    {
        printf("Don't know what to do:  %s, %s\n", key, value);
    }

    return retval;
}

int CompleteRemoteOrder(Check *check)
{
    FnTrace("CompleteRemoteOrder()");
    int       status = CALLCTR_STATUS_INCOMPLETE;
    int       order_count = 0;
    SubCheck *subcheck = NULL;
    Order    *order = NULL;
    Printer  *printer = NULL;
    Report   *report = NULL;
    Terminal *term = MasterControl->TermList();

    subcheck = check->SubList();
    while (subcheck != NULL)
    {
        order = subcheck->OrderList();
        while (order != NULL)
        {
            order_count += 1;
            order = order->next;
        }
        subcheck = subcheck->next;
    }
    if (order_count > 0)
    {
        // need to save the check (also ensure proper serial_number)
        MasterSystem->Add(check);
        check->date.Set();
        check->FinalizeOrders(term);
        check->Save();
        MasterControl->UpdateAll(UPDATE_CHECKS, NULL);
        check->current_sub = check->FirstOpenSubCheck();

        // need to print the check
        printer = MasterControl->FindPrinter(PRINTER_REMOTEORDER);
        if (printer != NULL) {
            report = new Report();
            if (report)
            {
                check->PrintDeliveryOrder(report, 80);
                if (report->Print(printer))
                {
                }
            }
        }

        status = CALLCTR_STATUS_COMPLETE;
    }

    return status;
}

int SendRemoteOrderResult(int socket, Check *check, int result_code, int status)
{
    FnTrace("SendRemoteOrderResult()");
    int retval = 0;
    char result_str[STRLONG];

    result_str[0] = '\0';
    snprintf(result_str, STRLONG, "%d:%d:", check->CallCenterID(),
             check->serial_number);
    if (result_code == CALLCTR_ERROR_NONE)
    {
        if (status == CALLCTR_STATUS_COMPLETE)
            strcat(result_str, "COMPLETE");
        else if (status == CALLCTR_STATUS_INCOMPLETE)
            strcat(result_str, "INCOMPLETE");
        else if (status == CALLCTR_STATUS_FAILED)
            strcat(result_str, "FAILED");
        else
            strcat(result_str, "UNKNOWNSTAT");
    }
    else
    {
        if (result_code == CALLCTR_ERROR_BADITEM)
            strcat(result_str, "BADITEM");
        else if (result_code == CALLCTR_ERROR_BADDETAIL)
            strcat(result_str, "BADDETAIL");
        else
            strcat(result_str, "UNKNOWNERR");
    }

    strcat(result_str, ":");
    if (result_code == CALLCTR_ERROR_NONE)
        strcat(result_str, "PRINTED");
    else
        strcat(result_str, "NOTPRINTED");

    write(socket, result_str, strlen(result_str));

    return retval;
}

int DeliveryToInt(const char* cost)
{
    FnTrace("DeliveryToInt()");
    int retval = 0;
    float interm = atof(cost);

    retval = (int)(interm * 100.0);

    return retval;
}

int ProcessRemoteOrder(int sock_fd)
{
    FnTrace("ProcessRemoteOrder()");
    int        retval = 0;
    KeyValueInputFile kvif;
    char       key[STRLONG];
    char       value[STRLONG];
    Settings  *settings = &MasterSystem->settings;
    Check     *check = NULL;
    SubCheck  *subcheck = NULL;
    Order     *order = NULL;
    char       StoreNum[STRSHORT];
    int        status = CALLCTR_STATUS_INCOMPLETE;

    kvif.Set(sock_fd);

    write(sock_fd, "SENDORDER\n", 10);

    check = new Check(settings, CHECK_DELIVERY);
    if (check == NULL)
        return retval;
    subcheck = check->NewSubCheck();
    if (subcheck == NULL)
        return retval;

    while ((status == CALLCTR_STATUS_INCOMPLETE) &&
           (retval == CALLCTR_ERROR_NONE) &&
           (kvif.Read(key, value, STRLONG - 2) > 0))
    {
        if (debug_mode)
            printf("Key:  %s, Value:  %s\n", key, value);
        if (strncmp(key, "OrderID", 7) == 0)
            check->CallCenterID(atoi(value));
        else if (strncmp(key, "OrderType", 9) == 0)
            check->CustomerType((value[0] == 'D') ? CHECK_DELIVERY : CHECK_TAKEOUT);
        else if ( strncmp(key, "OrderStatus", 11) == 0)
            ; // ignore this
        else if (strncmp(key, "FirstName", 9) == 0)
            check->FirstName(value);
        else if (strncmp(key, "LastName", 8) == 0)
            check->LastName(value);
        else if (strncmp(key, "CustomerName", 12) == 0)
            check->FirstName(value);
        else if (strncmp(key, "PhoneNo", 7) == 0)
            check->PhoneNumber(value);
        else if (strncmp(key, "PhoneExt", 8) == 0)
            check->Extension(value);
        else if (strncmp(key, "Street", 6) == 0)
            check->Address(value);
        else if (strncmp(key, "Address", 7) == 0)
            check->Address(value);
        else if (strncmp(key, "Suite", 5) == 0)
            check->Address2(value);
        else if (strncmp(key, "CrossStreet", 11) == 0)
            check->CrossStreet(value);
        else if (strncmp(key, "City", 4) == 0)
            check->City(value);
        else if (strncmp(key, "State", 5) == 0)
            check->State(value);
        else if (strncmp(key, "Zip", 3) == 0)
            check->Postal(value);
        else if (strncmp(key, "DeliveryCharge", 14) == 0)
            subcheck->delivery_charge = DeliveryToInt(value);
        else if (strncmp(key, "RestaurantID", 12) == 0)
            strncpy(StoreNum, value, 10);  // arbitrary limit on StoreNum
        else if (strncmp(key, "Item", 4) == 0)
            retval = ProcessRemoteOrderEntry(subcheck, &order, key, value);
        else if (strncmp(key, "Detail", 6) == 0)
            retval = ProcessRemoteOrderEntry(subcheck, &order, key, value);
        else if (strncmp(key, "Product", 7) == 0)
            retval = ProcessRemoteOrderEntry(subcheck, &order, key, value);
        else if (strncmp(key, "Addon", 5) == 0)
            retval = ProcessRemoteOrderEntry(subcheck, &order, key, value);
        else if (strncmp(key, "SideNumber", 10) == 0)
            retval = ProcessRemoteOrderEntry(subcheck, &order, key, value);
        else if (strncmp(key, "EndItem", 7) == 0)
            retval = ProcessRemoteOrderEntry(subcheck, &order, key, value);
        else if (strncmp(key, "EndDetail", 9) == 0)
            retval = ProcessRemoteOrderEntry(subcheck, &order, key, value);
        else if (strncmp(key, "EndProduct", 10) == 0)
            retval = ProcessRemoteOrderEntry(subcheck, &order, key, value);
        else if (strncmp(key, "EndAddon", 8) == 0)
            retval = ProcessRemoteOrderEntry(subcheck, &order, key, value);
        else if (strncmp(key, "EndOrder", 8) == 0)
            status = CompleteRemoteOrder(check);
        else if (debug_mode)
            printf("Unknown Key:  %s, Value:  %s\n", key, value);
    }
    if (strncmp(key, "EndOrder", 8))
    {
        // There are still key/value pairs waiting, so we need to read them
        // all to clear out the queue.
        while (kvif.Read(key, value, STRLONG - 2) > 0)
        {
            if (strncmp(key, "EndOrder", 8) == 0)
                break;
        }
    }
    SendRemoteOrderResult(sock_fd, check, retval, status);

    return retval;
}

int CompareCardNumbers(const char* card1, const char* card2)
{
    FnTrace("CompreCardNumbers()");
    int retval = 0;
    int len1 = 0;
    int len2 = 0;

    if (card1[0] == 'x' || card2[0] == 'x')
    {
        len1 = strlen(card1);
        len2 = strlen(card2);
        if (len1 == len2)
        {
            if (strcmp(&card1[len1 - 4], &card2[len2 - 4]) == 0)
                retval = 1;
        }
    }
    else
    {
        if (strcmp(card1, card2) == 0)
            retval = 1;
    }

    return retval;
}

Check* FindCCData(const char* cardnum, int value)
{
    FnTrace("FindCCData()");
    Check    *ret_check = NULL;
    Check    *curr_check = NULL;
    Archive  *archive = NULL;
    SubCheck *subcheck = NULL;
    Payment  *payment = NULL;
    Credit   *credit = NULL;
    char      cn[STRLENGTH];

    curr_check = MasterSystem->CheckList();
    while (ret_check == NULL && archive != MasterSystem->ArchiveList())
    {
        while (curr_check != NULL && ret_check == NULL)
        {
            subcheck = curr_check->SubList();
            while (subcheck != NULL && ret_check == NULL)
            {
                payment = subcheck->PaymentList();
                while (payment != NULL && ret_check == NULL)
                {
                    if (payment->credit != NULL)
                    {
                        credit = payment->credit;
                        strcpy(cn, credit->PAN(2));
                        if (CompareCardNumbers(cn, cardnum) &&
                            credit->FullAmount() == value) {
                            ret_check = curr_check;
                        }
                    }
                    payment = payment->next;
                }
                subcheck = subcheck->next;
            }
            curr_check = curr_check->next;
        }
        if (ret_check == NULL)
        {
            if (archive == NULL)
                archive = MasterSystem->ArchiveListEnd();
            else
                archive = archive->fore;
            if (archive->loaded == 0)
                archive->LoadPacked(&MasterSystem->settings);
            curr_check = archive->CheckList();
        }
    }

    return ret_check;
}

int GetCCData(const char* data)
{
    FnTrace("GetCCData()");
    int       retval = 0;
    char      cardnum[STRLENGTH];
    char      camount[STRLENGTH];
    int       amount;
    int       sidx = 0;
    int       didx = 0;
    int       maxlen = 28;  // arbitrary:  19 chars for PAN, 8 for amount, 1 for space
    Check    *check = NULL;
    SubCheck *subcheck = NULL;
    Payment  *payment = NULL;
    Credit   *credit = NULL;

    // get cardnum
    while (data[sidx] != ' ' && data[sidx] != '\0' && sidx < maxlen)
    {
        cardnum[didx] = data[sidx];
        didx += 1;
        sidx += 1;
    }
    cardnum[didx] = '\0';
    sidx += 1;
    // get amount
    didx = 0;
    while (data[sidx] != ' ' && data[sidx] != '\0' && sidx < maxlen)
    {
        camount[didx] = data[sidx];
        didx += 1;
        sidx += 1;
    }
    camount[didx] = '\0';
    amount = atoi(camount);

    check = FindCCData(cardnum, amount);
    if (check)
    {
        printf("Card %s was processed on %s\n", cardnum, check->made_time.to_string().c_str());
        printf("    Check ID:  %d\n", check->serial_number);
        subcheck = check->SubList();
        while (subcheck != NULL)
        {
            payment = subcheck->PaymentList();
            while (payment != NULL)
            {
                if (payment->credit != NULL)
                {
                    credit = payment->credit;
                    printf("    Card Name:  %s\n", credit->Name());
                }
                payment = payment->next;
            }
            subcheck = subcheck->next;
        }
    }
    else
        ReportError("Unable to find associated check.");

    return retval;
}

int ProcessSocketRequest(char* request)
{
    FnTrace("ProcessSocketRequest()");
    int retval = 1;
    int idx = 0;
    char str[STRLONG];

    while (request[idx] != '\0' &&
           request[idx] != '\n' &&
           request[idx] != '\r' &&
           idx < STRLONG)
    {
        idx += 1;
    }
    request[idx] = '\0';

    snprintf(str, STRLONG, "Processing Request:  %s", request);
    ReportError(str);

    if (strncmp(request, "openterm ", 9) == 0)
        retval = OpenDynTerminal(&request[9]);
    else if (strncmp(request, "closeterm ", 10) == 0)
        retval = CloseDynTerminal(&request[10]);
    else if (strncmp(request, "cloneterm ", 10) == 0)
        retval = CloneDynTerminal(&request[10]);
    else if (strncmp(request, "finddata ", 9) == 0)
        retval = GetCCData(&request[9]);

    return retval;
}

int ReadSocketRequest(int listen_sock)
{
    FnTrace("ReadSocketRequest()");
    int  retval = 1;
    static int open_sock = -1;
    static int count = 0;
    char request[STRLONG] = "";
    int  bytes_read = 0;
    int  sel_result;

    if (open_sock < 0)
    {
        if (SelectIn(listen_sock, select_timeout) > 0)
            open_sock = Accept(listen_sock);
    }
    else if (open_sock >= 0)
    {
        sel_result = SelectIn(open_sock, select_timeout);
        if (sel_result > 0)
        {
            bytes_read = read(open_sock, request, sizeof(request) - 1);
            if (bytes_read > 0)
            {
                // In most cases we're only going to read once and then close the socket.
                // This really isn't intended to be a conversation at this point.
                if (strncmp(request, "remoteorder", 11) == 0)
                    retval = ProcessRemoteOrder(open_sock);
                else
                {
                    request[bytes_read] = '\0';
                    write(open_sock, "ACK", 3);
                    retval = ProcessSocketRequest(request);
                }
                close(open_sock);
                open_sock = -1;
            }
        }
        else if (sel_result < 0)
        {
            perror("ReadSocketRequest select");
            close(open_sock);
            open_sock = -1;
        }
        else
        {
            count += 1;
            if (count > MAX_CONN_TRIES)
            {
                close(open_sock);
                open_sock = -1;
                count = 0;
            }
        }
    }

    return retval;
}

void UpdateSystemCB(XtPointer client_data, XtIntervalId *time_id)
{
    FnTrace("UpdateSystemCB()");

    pid_t pid;
    int pstat;
    // First, let's clean up any children processes that may have been started
    while ((pid = waitpid(-1, &pstat, WNOHANG)) > 0)
    {
        if (debug_mode)
            printf("Child %d exited\n", pid);
    }

    if (UserRestart)
    {
        if (MasterControl->TermList() != NULL &&
            MasterControl->TermList()->TermsInUse() == 0)
        {
            RestartSystem();
        }
    }

    // Respond to remote OpenTerm requests
    if (OpenTermSocket > -1)
        ReadSocketRequest(OpenTermSocket);

    // Get current time & other info
    SystemTime.Set();
    int update = 0;

    System *sys = MasterSystem;
    Settings *settings = &(sys->settings);
    int day = SystemTime.Day();
    int minute = SystemTime.Min();
    if (LastDay != day)
    {
        if (LastDay != -1)
        {
            // TODO: what to do in this case?
            // old code:
            //ReportError("UpdateSystemCB checking license");
            //CheckLicense(settings);
            //settings->Save();
        }
        LastDay = day;
    }

    if (sys->eod_term != NULL && sys->eod_term->eod_processing != EOD_DONE)
    {
        sys->eod_term->EndDay();
    }

    if (LastMin != minute)
    {
        // Only execute once every minute
        LastMin = minute;
        int meal = settings->MealPeriod(SystemTime);
        if (LastMeal != meal)
        {
            LastMeal = meal;
            update |= UPDATE_MEAL_PERIOD;
        }

        update |= UPDATE_MINUTE;
        int hour = SystemTime.Hour();
        if (LastHour != hour)
        {
            LastHour = hour;
            update |= UPDATE_HOUR;
        }
    }

    // Update Terminals
    Control *con = MasterControl;
    Terminal  *term = con->TermList();
    while (term)
    {
        Terminal *tnext = term->next;
        if (term->reload_zone_db && term->user == NULL)
        {
            // Reload zone information if needed
            ReportError("Updating zone information");
            con->SetAllMessages("Updating System - Please Wait...");
            term->UpdateZoneDB(con);
            con->ClearAllMessages();
        }

        int u = update;
        if (term->edit == 0 && term->translate == 0 && term->timeout > 0)
        {
            // Check for general timeout
            int sec = SecondsElapsed(SystemTime, term->time_out);
            if (sec > term->timeout)
            {
                term->time_out = SystemTime;
                u |= UPDATE_TIMEOUT;
            }
        }

        if (term->page)
        {
            if (term->page->IsTable() || term->page->IsKitchen())
                u |= UPDATE_BLINK;  // half second blink message for table pages
            if (u)
                term->Update(u, NULL);
        }

        if (term->cdu != NULL)
            term->cdu->Refresh();

        if (term->kill_me)
            con->KillTerm(term);
        term = tnext;
    }

    if (con->TermList() == NULL)
    {
        ReportError("All terminals lost - shutting down system");
        EndSystem();
    }

    if (UserCommand != 0)
        RunUserCommand();

    // restart system timer
    UpdateID = XtAppAddTimeOut(App, UPDATE_TIME,
                               (XtTimerCallbackProc) UpdateSystemCB, client_data);
}

/****
 * RunUserCommand:  Intended to be a method of running background reports and
 *  processes.  The user will send SIGUSR2 to vt_main.  vt_main traps it and sets
 *  a global variable (UserCommand).  When that global variable is set, the
 *  UpdateSystemCB will call RunUserCommand.
 *
 *  This allows, for example, the administrator to remotely, through ssh, call
 *  for a Royalty report to be sent to the head company accountant.
 *
 *  The requested command should be stored in a file, VIEWTOUCH_COMMAND.
 *  The file will be read in and each command will be processed sequentially.
 *  The commands should define a printer to use, which reports to run, etc.
 *  Once the file is completed, it is destroyed and all commands are wiped
 *  clean (printers are deleted from memory, etc.).
 *
 *  Some commands take quite some time to complete.  For example, the Royalty
 *  report, if many archives need to be processed, can take several seconds
 *  to complete.  If we're running it as a background process during normal
 *  working hours, we don't want to tie up the entire system.  To keep things
 *  simple, we process one command per UpdateSystemCB cycle.  When all commands
 *  have been processed (or if there is no command file) we delete the command
 *  file and disable command processing until the next SIGUSR2 signal.
 ****/
int RunUserCommand(void)
{
    FnTrace("RunUserCommand()");
    int retval = 0;
    genericChar key[STRLENGTH];
    genericChar value[STRLENGTH];
    static int working = 0;
    static int macros  = 0;
    static int endday  = 0;
    static Printer *printer = NULL;
    static Report *report = NULL;
    static KeyValueInputFile kvfile;
    static int exit_system = 0;

    if (!kvfile.IsOpen())
        kvfile.Open(VIEWTOUCH_COMMAND);

    if (working)
    {
        working = RunReport(NULL, printer);
    }
    else if (endday)
    {
        endday = RunEndDay();
    }
    else if (macros)
    {
        macros = RunMacros();
    }
    else if (kvfile.IsOpen() && kvfile.Read(key, value, STRLENGTH))
    {
        if (strcmp(key, "report") == 0)
            working = RunReport(value, printer);
        else if (strcmp(key, "printer") == 0)
            printer = SetPrinter(value);
        else if (strcmp(key, "nologin") == 0)
            AllowLogins = 0;
        else if (strcmp(key, "allowlogin") == 0)
            AllowLogins = 1;
        else if (strcmp(key, "exitsystem") == 0)
            exit_system = 1;
        else if (strcmp(key, "endday") == 0)
            endday = RunEndDay();
        else if (strcmp(key, "runmacros") == 0)
            macros = RunMacros();
        else if (strcmp(key, "ping") == 0)
            PingCheck();
        else if (strcmp(key, "usercount") == 0)
            UserCount();
        else if (strlen(key) > 0)
            fprintf(stderr, "Unknown external command:  '%s'\n", key);
    }
    else
    {
        if (kvfile.IsOpen())
        {
            kvfile.Reset();
            unlink(VIEWTOUCH_COMMAND);
        }
        if (printer != NULL)
            delete printer;
        if (report != NULL)
            delete report;
        // only allow system exit if we're running at startup (to be used to
        // run multiple reports for multiple data sets, not to be used for
        // schduling system shut downs as that might make it very easy to do
        // DOS attacks.
        if (exit_system)
            EndSystem();
        UserCommand = 0;
    }

    return retval;
}

/****
 * PingCheck:  Start off simple:  if we're in an endless loop somewhere, we'll
 *  never get here.  This function will create a file.  If we're able to create
 *  that file, that means we're at least partially running.  Later, we should
 *  extend this to do more testing of internal functions.
 ****/
int PingCheck()
{
    FnTrace("PingCheck()");
    int retval = 1;
    int fd = 0;

    fd = open(VIEWTOUCH_PINGCHECK, O_CREAT | O_TRUNC, 0755);
    if (fd > -1)
    {
        retval = 0;
        close(fd);
    }

    return retval;
}

int UserCount()
{
    FnTrace("UserCount()");
    int retval = 0;
    int count = 0;
    char message[STRLENGTH];
    Terminal *term = NULL;

    count = MasterControl->TermList()->TermsInUse();
    snprintf(message, STRLENGTH, "UserCount:  %d users active", count);
    ReportError(message);

    if (count > 0)
    {
        term = MasterControl->TermList();
        while (term != NULL)
        {
            if (term->user)
            {
                const std::string msg = std::string("    ")
                        + term->user->system_name.str()
                        + " is logged in to "
                        + term->name.str()
                        + ", last input at "
                        + term->last_input.to_string()
                        + "\n";
                ReportError(msg);
            }
            term = term->next;
        }
    }

    return retval;

}


/****
 * RunEndDay:  runs the End Day process.  The drawers must be already balanced
 *  (by hand) or this will fail.
 ****/
int RunEndDay()
{
    FnTrace("RunEndDay()");
    Terminal *term = MasterControl->TermList();
    System *sys    = MasterSystem;

    // verify nobody is logged in, then run EndDay
    if (term->TermsInUse() == 0)
    {
        sys->eod_term = term;
        term->eod_processing = EOD_BEGIN;
    }
    return 0;
}

/****
 * RunMacros:
 ****/
int RunMacros()
{
    FnTrace("RunMacros()");
    static Terminal *term = NULL;
    static int count = 0;
    int retval = 0;

    if (term == NULL)
        term = MasterControl->TermListEnd();

    while (term != NULL && retval == 0)
    {
        if (term->page != NULL)
        {
            term->ReadRecordFile();
            term = term->next;
        }
        else if (count > 2)
        {
            count = 0;
            term = term->next;
        }
        else
        {
            retval = 1;
            count += 1;
        }

    }

    return retval;
}

/****
 * RunReport:  Compiles and prints a report.  Returns 0 if everything goes well,
 *  1 if the report has not been completed yet.  In the latter case, it should
 *  be called again with report_string set to NULL.
 ****/
int RunReport(const genericChar* report_string, Printer *printer)
{
    FnTrace("RunReport()");
    int retval = 0;
    static Report *report = NULL;
    genericChar report_name[STRLONG] = "";
    genericChar report_from[STRLONG] = "";
    TimeInfo from;
    genericChar report_to[STRLONG] = "";
    TimeInfo to;
    int idx = 0;
    Terminal *term = MasterControl->TermList();
    System *system_data = term->system_data;

    if (report == NULL && report_string != NULL)
    {
        report = new Report;

        report->Clear();
        report->is_complete = 0;

        // need to pull out "Report From To"
        // date will be in the format "DD/MM/YY,HH:MM" in 24hour format
        if (NextToken(report_name, report_string, ' ', &idx))
        {
            if (NextToken(report_from, report_string, ' ', &idx))
            {
                from.Set(report_from);
                if (NextToken(report_to, report_string, ' ', &idx))
                    to.Set(report_to);
            }
        }
        if (!from.IsSet())
        {  // set date to yesterday morning, 00:00
            from.Set();
            from -= date::days(1);
            from.Floor<date::days>();
        }
        if (!to.IsSet())
        {  // set date to last night, 23:59
            to.Set();
            to.Floor<date::days>();
            to -= std::chrono::seconds(1);
        }
        if (strcmp(report_name, "daily") == 0)
            system_data->DepositReport(term, from, to, NULL, report);
        else if (strcmp(report_name, "expense") == 0)
            system_data->ExpenseReport(term, from, to, NULL, report, NULL);
        else if (strcmp(report_name, "revenue") == 0)
            system_data->BalanceReport(term, from, to, report);
        else if (strcmp(report_name, "royalty") == 0)
            system_data->RoyaltyReport(term, from, to, NULL, report, NULL);
        else if (strcmp(report_name, "sales") == 0)
            system_data->SalesMixReport(term, from, to, NULL, report);
        else if (strcmp(report_name, "audit") == 0)
            system_data->AuditingReport(term, from, to, NULL, report, NULL);
        else if (strcmp(report_name, "batchsettle") == 0)
        {
            MasterSystem->cc_report_type = CC_REPORT_BATCH;
            system_data->CreditCardReport(term, from, to, NULL, report, NULL);
        }
        else
        {
            fprintf(stderr, "Unknown report '%s'\n", report_name);
            delete report;
            report = NULL;
        }
    }

    if (report != NULL)
    {
        if (report->is_complete > 0)
        {
            report->Print(printer);
            delete report;
            report = NULL;
            retval = 0;
        }
        else
            retval = 1;
    }

    return retval;
}

/****
 * SetPrinter:
 ****/
Printer *SetPrinter(const genericChar* printer_description)
{
    FnTrace("SetPrinter()");
    Printer *retPrinter = NULL;

    retPrinter = NewPrinterFromString(printer_description);
    return retPrinter;
}


/**** Functions ****/
int GetFontSize(int font_id, int &w, int &h)
{
    FnTrace("GetFontSize()");
    w = FontWidth[font_id];
    h = FontHeight[font_id];
    return 0;
}

int GetTextWidth(const char* my_string, int len, int font_id)
{
    FnTrace("GetTextWidth()");
    if (my_string == NULL || len <= 0)
        return 0;
    else if (FontInfo[font_id])
        return XTextWidth(FontInfo[font_id], my_string, len);
    else
        return FontWidth[font_id] * len;
}

unsigned long AddTimeOutFn(TimeOutFn fn, int timeint, void *client_data)
{
    FnTrace("AddTimeOutFn()");
    return XtAppAddTimeOut(App, timeint, (XtTimerCallbackProc) fn,
                           (XtPointer) client_data);
}

unsigned long AddInputFn(InputFn fn, int device_no, void *client_data)
{
    FnTrace("AddInputFn()");
    return XtAppAddInput(App, device_no, (XtPointer) XtInputReadMask,
                         (XtInputCallbackProc) fn, (XtPointer) client_data);
}

unsigned long AddWorkFn(WorkFn fn, void *client_data)
{
    FnTrace("AddWorkFn()");
    return XtAppAddWorkProc(App, (XtWorkProc) fn, (XtPointer) client_data);
}

int RemoveTimeOutFn(unsigned long fn_id)
{
    FnTrace("RemoveTimeOutFn()");
    if (fn_id > 0l)
        XtRemoveTimeOut(fn_id);
    return 0;
}

int RemoveInputFn(unsigned long fn_id)
{
    FnTrace("RemoveInputFn()");
    if (fn_id > 0)
        XtRemoveInput(fn_id);
    return 0;
}

int ReportWorkFn(int fn_id)
{
    FnTrace("ReportWorkFn()");
    if (fn_id > 0)
        XtRemoveWorkProc(fn_id);
    return 0;
}
