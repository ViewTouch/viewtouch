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

// Font constants are now centralized in font_ids.hh - removed local definitions

#include "conf_file.hh"
#include "date/date.h"      // helper library to output date strings with std::chrono

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>

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
#include <sys/stat.h>       // for stat functions

#ifdef DMALLOC
#include <dmalloc.h>
#endif
using namespace date; // for date conversion on streams

// Add at the top with other includes
#include <X11/Xft/Xft.h>

// Forward declaration for GetFontInfo
XftFont *GetFontInfo(int font_id);

/**** System Globals ****/
int ReleaseYear  = 1998;
int ReleaseMonth = 10;
int ReleaseDay   = 20;

Control     *MasterControl = nullptr;
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
                    "Thursday", "Friday", "Saturday", nullptr};

const char* ShortDayName[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", nullptr};

const char* MonthName[] = { "January", "February", "March", "April", 
                      "May", "June", "July", "August", "September", 
                      "October", "November", "December", nullptr};

const char* ShortMonthName[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", nullptr};

/*************************************************************
 * Terminal Type values
 *************************************************************/
const char* TermTypeName[] = { 	"Normal", "Order Only", "Bar", "Bar2", 
                            "Fast Food", "Kitchen Video", "Kitchen Video2", nullptr};

int TermTypeValue[] = { TERMINAL_NORMAL, TERMINAL_ORDER_ONLY,
                        TERMINAL_BAR, TERMINAL_BAR2,
                        TERMINAL_FASTFOOD, TERMINAL_KITCHEN_VIDEO,
                        TERMINAL_KITCHEN_VIDEO2, -1};

/*************************************************************
 * Printer Type values
 *************************************************************/
const char* PrinterTypeName[] = { "Kitchen 1", "Kitchen 2", "Kitchen 3", "Kitchen 4",
                            "Bar 1", "Bar 2", "Expediter", "Report",
                            "Credit Receipt", "Remote Order", nullptr};

int PrinterTypeValue[] = { PRINTER_KITCHEN1, PRINTER_KITCHEN2,
                           PRINTER_KITCHEN3, PRINTER_KITCHEN4,
                           PRINTER_BAR1, PRINTER_BAR2,
                           PRINTER_EXPEDITER, PRINTER_REPORT,
                           PRINTER_CREDITRECEIPT, PRINTER_REMOTEORDER, -1};


/*************************************************************
 * Module Globals
 *************************************************************/
static XtAppContext App = 0;
static Display     *Dis = nullptr;
static XftFont     *FontInfo[80];  // Increased to accommodate new font families (Garamond, Bookman, Nimbus)
static int          FontWidth[80];  // Increased to accommodate new font families (Garamond, Bookman, Nimbus)
static int          FontHeight[80]; // Increased to accommodate new font families (Garamond, Bookman, Nimbus)
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
    "wget -nv -T 2 -t 2 https://www.viewtouch.com/vt_updates/vt-update -O " VIEWTOUCH_UPDATE_COMMAND

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

genericChar* GetMachineName(genericChar* str = nullptr, int len = STRLENGTH)
{
    FnTrace("GetMachineName()");
    struct utsname uts;
    static genericChar buffer[STRLENGTH];
    if (str == nullptr)
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

void DownloadFile(const std::string &url, const std::string &destination)
{
    std::cerr << "DEBUG: Starting download from '" << url << "' to '" << destination << "'" << std::endl;
    
    try {
        curlpp::Cleanup cleaner;
        curlpp::Easy request;
        
        // Open file for writing
        std::ofstream fout(destination, std::ios::binary);
        if (!fout.is_open()) {
            std::cerr << "ERROR: Cannot open destination file for writing: " << destination << std::endl;
            return;
        }

        // Configure curl request
        request.setOpt(curlpp::options::Url(url));
        request.setOpt(curlpp::options::WriteStream(&fout));
        request.setOpt(curlpp::options::FollowLocation(true));  // Follow redirects
        request.setOpt(curlpp::options::Timeout(30));           // 30 second timeout
        request.setOpt(curlpp::options::ConnectTimeout(10));    // 10 second connect timeout
        // Removed SSL options since we're using HTTP now
        // request.setOpt(curlpp::options::Verbose(true));  // Disable verbose for cleaner output
        
        std::cerr << "DEBUG: About to perform curl request..." << std::endl;
        request.perform();
        std::cerr << "DEBUG: Curl request completed, closing file..." << std::endl;
        
        fout.close();
        
        // Check if file was actually written
        struct stat st;
        if (stat(destination.c_str(), &st) == 0) {
            std::cerr << "Successfully downloaded file '" << destination << "' from '" << url << "' (" << st.st_size << " bytes)" << std::endl;
        } else {
            std::cerr << "ERROR: File was not created: " << destination << std::endl;
        }
    }
    catch (const curlpp::LogicError & e)
    {
        std::cerr << "CURL Logic Error downloading file: " << e.what() << std::endl;
    }
    catch (const curlpp::RuntimeError &e)
    {
        std::cerr << "CURL Runtime Error downloading file: " << e.what() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "General Error downloading file: " << e.what() << std::endl;
    }
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

/*************************************************************
 * Main
 *************************************************************/
int main(int argc, genericChar* argv[])
{
    FnTrace("main()");
    srand(static_cast<unsigned int>(time(nullptr)));
    StartupLocalization();
    ReadViewTouchConfig();

    genericChar socket_file[256] = "";
    if (argc >= 2)
    {
        if (strcmp(argv[1], "version") == 0)
        {
            // return version for vt_update
            printf("1\n");
            return 0;
        }
        strcpy(socket_file, argv[1]);
    }

    LoaderSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (LoaderSocket <= 0)
    {
        ReportError("Can't open initial loader socket");
        exit(1);
    }

    struct sockaddr_un server_adr;
    server_adr.sun_family = AF_UNIX;
    strcpy(server_adr.sun_path, socket_file);
    sleep(1);
    if (connect(LoaderSocket, (struct sockaddr *) &server_adr,
                SUN_LEN(&server_adr)) < 0)
    {
        ReportError("Can't connect to loader");
        close(LoaderSocket);
        exit(1);
    }

    // Read starting commands
    use_net = 1;
    int purge = 0;
    int notrace = 0;
    genericChar data_path[256] = "\0";

    genericChar buffer[1024];
    genericChar* c = buffer;
    for (;;)
    {
        int no = read(LoaderSocket, c, 1);
        if (no == 1)
        {
            if (*c == '\0')
            {
                c = buffer;
                if (strcmp(buffer, "done") == 0)
                {
                    break;
                }
                else if (strncmp(buffer, "datapath ", 9) == 0)
                {
                    strcpy(data_path, &buffer[9]);
                }
                else if (strcmp(buffer, "netoff") == 0)
                {
                    use_net = 0;
                }
                else if (strcmp(buffer, "purge") == 0)
                {
                    purge = 1;
                }
                else if (strncmp(buffer, "display ", 8) == 0)
                {
                    strncpy(displaystr, &buffer[8], STRLENGTH);
                }
                else if (strcmp(buffer, "notrace") == 0)
                {
                    notrace = 1;
                }
            }
            else
                ++c;
        }
    }

    // set up signal handlers
    if (debug_mode == 1 && notrace == 0)
    {
        signal(SIGBUS,  Terminate);
        signal(SIGFPE,  Terminate);
        signal(SIGILL,  Terminate);
        signal(SIGINT,  Terminate);
        signal(SIGSEGV, Terminate);
        signal(SIGQUIT, Terminate);
    }
    signal(SIGUSR1, UserSignal1);
    signal(SIGUSR2, UserSignal2);
    signal(SIGPIPE, SIG_IGN);

    // Set up default umask
    // REFACTOR FIX: Changed from umask(0111) to umask(0022) to fix directory access issues.
    // The previous umask(0111) removed execute permissions from directories, making them
    // inaccessible (644 instead of 755). This broke /usr/viewtouch/dat access in file managers.
    // umask(0022) maintains security by removing write for group/other while preserving
    // execute permissions needed for directory access.
    umask(0022); // u+rwx, g+rx, o+rx (removes write for group/other, preserves execute for directories)

    SystemTime.Set();

    // Start application
    MasterSystem = new System;
    if (MasterSystem == nullptr)
    {
        ReportError("Couldn't create main system object");
        EndSystem();
    }
    if (strlen(data_path) > 0)
        MasterSystem->SetDataPath(data_path);
    else
        MasterSystem->SetDataPath(VIEWTOUCH_PATH "/dat");
    // Check for updates from server if not disabled
    if (autoupdate)
    {
        ReportError("Automatic check for updates...");
	unlink(VIEWTOUCH_UPDATE_COMMAND);	// out with the old
    	system(VIEWTOUCH_UPDATE_REQUEST);	// in with the new
	chmod(VIEWTOUCH_UPDATE_COMMAND, 0755);	// set executable
	// try to run it, giving build-time base path
	system(VIEWTOUCH_UPDATE_COMMAND " " VIEWTOUCH_PATH);
    }
    // Now process any locally available updates (updates
    // from the previous step will be installed and ready for
    // this step).
    MasterSystem->CheckFileUpdates();
    if (purge)
        MasterSystem->ClearSystem();

    vt_init_setproctitle(argc, argv);
    vt_setproctitle("vt_main pri");

    StartSystem(use_net);
    EndSystem();
    return 0;
}


/*************************************************************
 * Functions
 *************************************************************/
int ReportError(const std::string &message)
{
    FnTrace("ReportError()");
    std::cerr << message << std::endl;


    const std::string err_file = MasterSystem ?
                std::string(MasterSystem->data_path.Value()) + "/error_log.txt" :
                VIEWTOUCH_PATH "/dat/error_log.txt";
    std::ofstream err_out(err_file, std::fstream::app);
    if (!err_out.is_open())
        return 1;  // Error creating error log

    // get time rounded to minutes
    auto now = date::floor<std::chrono::minutes>(std::chrono::system_clock::now());
    // round to days
    auto today = date::floor<date::days>(now);
    err_out << "[" << today << " " << date::make_time(now - today) << " UTC] "
            << message << std::endl;
    return 0;
}

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
        snprintf(str, sizeof(str), "Unknown my_signal %d received (ignored)", my_signal);
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
    EnsureDirExists(sys->data_path.Value());
    if (DoesFileExist(sys->data_path.Value()) == 0)
    {
        snprintf(str, sizeof(str), "Can't find path '%s'", sys->data_path.Value());
        ReportError(str);
        ReportLoader("POS cannot be started.");
        sleep(1);
        EndSystem();
    }

    snprintf(str, sizeof(str), "Starting System on %s", GetMachineName());
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
    for (i = 0; i < 80; ++i)  // Increased to accommodate new font families (Garamond, Bookman, Nimbus)
    {
        FontInfo[i]   = nullptr;
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
    Dis = XtOpenDisplay(App, displaystr, nullptr, nullptr, nullptr, 0, &argc, (genericChar**)argv);
    if (Dis)
    {
        // Load legacy fonts from FontData array
        for (i = 0; i < FONT_COUNT; ++i)
        {
            int f = FontData[i].id;
            // Use scalable font names instead of bitmapped font names
            const char* scalable_font_name = GetScalableFontName(FontData[i].id);
            FontInfo[f] = XftFontOpenName(Dis, DefaultScreen(Dis), scalable_font_name);
            if (FontInfo[f] == nullptr)
            {
                snprintf(str, sizeof(str), "Can't load font '%s'", scalable_font_name);
                ReportError(str);
            }
        }
        
        // Load new font families (Garamond, Bookman, Nimbus) - consistent with terminal
        int new_font_ids[] = {
            FONT_GARAMOND_14, FONT_GARAMOND_16, FONT_GARAMOND_18, FONT_GARAMOND_20, FONT_GARAMOND_24, FONT_GARAMOND_28,
            FONT_GARAMOND_14B, FONT_GARAMOND_16B, FONT_GARAMOND_18B, FONT_GARAMOND_20B, FONT_GARAMOND_24B, FONT_GARAMOND_28B,
            FONT_BOOKMAN_14, FONT_BOOKMAN_16, FONT_BOOKMAN_18, FONT_BOOKMAN_20, FONT_BOOKMAN_24, FONT_BOOKMAN_28,
            FONT_BOOKMAN_14B, FONT_BOOKMAN_16B, FONT_BOOKMAN_18B, FONT_BOOKMAN_20B, FONT_BOOKMAN_24B, FONT_BOOKMAN_28B,
            FONT_NIMBUS_14, FONT_NIMBUS_16, FONT_NIMBUS_18, FONT_NIMBUS_20, FONT_NIMBUS_24, FONT_NIMBUS_28,
            FONT_NIMBUS_14B, FONT_NIMBUS_16B, FONT_NIMBUS_18B, FONT_NIMBUS_20B, FONT_NIMBUS_24B, FONT_NIMBUS_28B
        };
        
        for (i = 0; i < sizeof(new_font_ids)/sizeof(new_font_ids[0]); ++i)
        {
            int f = new_font_ids[i];
            const char* scalable_font_name = GetScalableFontName(f);
            FontInfo[f] = XftFontOpenName(Dis, DefaultScreen(Dis), scalable_font_name);
            if (FontInfo[f] == nullptr)
            {
                // Fallback to default font if new font fails
                snprintf(str, sizeof(str), "Warning: Could not load new font '%s', falling back to default", scalable_font_name);
                ReportError(str);
                FontInfo[f] = XftFontOpenName(Dis, DefaultScreen(Dis), GetScalableFontName(FONT_TIMES_24));
                if (FontInfo[f] == nullptr)
                {
                    snprintf(str, sizeof(str), "Can't load fallback font '%s'", GetScalableFontName(FONT_TIMES_24));
                    ReportError(str);
                }
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
            while (ti != nullptr)
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
        while (ti != nullptr)
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
                    snprintf(str, sizeof(str), "Opening Remote Display '%s'", ti->name.Value());
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
    
    // TEMP FIX for Pi 5: Ensure archive directory exists before scanning
    EnsureDirExists(str);
    
    if (sys->ScanArchives(str, altmedia))
        ReportError("Can't scan archives");

    // Load Employees
	snprintf(msg, sizeof(msg), "Attempting to load file %s...", MASTER_USER_DB);
	ReportError(msg); //stamp file attempt in log
    ReportLoader("Loading Employees");
    sys->FullPath(MASTER_USER_DB, str);
    
    // TEMP DEBUG for Pi 5: Add detailed debug output
    ReportError("DEBUG: About to check if employee.dat exists");
    struct stat emp_st;
    if (stat(str, &emp_st) == 0) {
        snprintf(msg, sizeof(msg), "DEBUG: employee.dat exists, size: %ld bytes", emp_st.st_size);
        ReportError(msg);
    } else {
        ReportError("DEBUG: employee.dat does not exist, will try to load anyway");
    }
    
    ReportError("DEBUG: About to call sys->user_db.Load()");
    if (sys->user_db.Load(str))
    {
        ReportError("DEBUG: user_db.Load() failed, trying backup");
        ReportError("DEBUG: About to call RestoreBackup()");
        int backup_result = RestoreBackup(str);
        snprintf(msg, sizeof(msg), "DEBUG: RestoreBackup() returned: %d", backup_result);
        ReportError(msg);
        
        ReportError("DEBUG: About to call sys->user_db.Purge()");
        // TEMP FIX for Pi 5: Skip Purge() as it hangs on corrupted employee data
        // Since employee.dat doesn't exist, there's nothing meaningful to purge anyway
        // sys->user_db.Purge();  // DISABLED - causes hang on Pi 5
        ReportError("DEBUG: sys->user_db.Purge() skipped (Pi 5 fix)");
        
        ReportError("DEBUG: About to call sys->user_db.Load() again");
        int second_load_result = sys->user_db.Load(str);
        if (second_load_result == 0) {
            ReportError("DEBUG: Second sys->user_db.Load() completed successfully");
        } else {
            ReportError("DEBUG: Second sys->user_db.Load() also failed - will continue with default users only");
        }
    }
    ReportError("DEBUG: user_db.Load() completed successfully");
    
    // set developer key (this should be done somewhere else)
    ReportError("DEBUG: About to set developer key");
    sys->user_db.developer->key = settings->developer_key;
    ReportError("DEBUG: Developer key set successfully");
    
	snprintf(msg, sizeof(msg), "%s OK", MASTER_USER_DB);
	ReportError(msg); //stamp file attempt in log

    // Load Labor
    snprintf(msg, sizeof(msg), "Attempting to load labor info...");
    ReportLoader(msg);
    sys->FullPath(LABOR_DATA_DIR, str);
    EnsureDirExists(str);  // TEMP FIX for Pi 5: Ensure directory exists
    if (sys->labor_db.Load(str))
        ReportError("Can't find labor directory");

    // Load Menu
	snprintf(msg, sizeof(msg), "Attempting to load file %s...", MASTER_MENU_DB);
	ReportError(msg); //stamp file attempt in log
    ReportLoader("Loading Menu");
    sys->FullPath(MASTER_MENU_DB, str);
    struct stat st;
    if (stat(str, &st) != 0)
    {
        // EMERGENCY FIX: Use HTTP for reliable downloads on Pi 5
        const std::string menu_url = "http://www.viewtouch.com/menu.dat";
        DownloadFile(menu_url, str);
    }
    if (sys->menu.Load(str))
    {
        RestoreBackup(str);
        sys->menu.Purge();
        sys->menu.Load(str);
    }
	snprintf(msg, sizeof(msg), "%s OK", MASTER_MENU_DB);
	ReportError(msg); //stamp file attempt in log

    // Load Exceptions
	snprintf(msg, sizeof(msg), "Attempting to load file %s...", MASTER_EXCEPTION);
	ReportError(msg); //stamp file attempt in log
    ReportLoader("Loading Exception Records");
    sys->FullPath(MASTER_EXCEPTION, str);
    if (sys->exception_db.Load(str))
    {
        RestoreBackup(str);
        sys->exception_db.Purge();
        sys->exception_db.Load(str);
    }
	snprintf(msg, sizeof(msg), "%s OK", MASTER_EXCEPTION);
	ReportError(msg); //stamp file attempt in log

    // Load Inventory
	snprintf(msg, sizeof(msg), "Attempting to load file %s...", MASTER_INVENTORY);
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
    EnsureDirExists(str);  // TEMP FIX for Pi 5: Ensure directory exists
    sys->inventory.LoadStock(str);
	snprintf(msg, sizeof(msg), "%s OK", MASTER_INVENTORY);
	ReportError(msg); //stamp file attempt in log

    // Load Customers
    sys->FullPath(CUSTOMER_DATA_DIR, str);
    EnsureDirExists(str);  // TEMP FIX for Pi 5: Ensure directory exists
    ReportLoader("Loading Customers");
    sys->customer_db.Load(str);

    // Load Checks & Drawers
    sys->FullPath(CURRENT_DATA_DIR, str);
    EnsureDirExists(str);  // TEMP FIX for Pi 5: Ensure directory exists
    ReportLoader("Loading Current Checks & Drawers");
    sys->LoadCurrentData(str);

    // Load Accounts
    sys->FullPath(ACCOUNTS_DATA_DIR, str);
    EnsureDirExists(str);  // TEMP FIX for Pi 5: Ensure directory exists
    ReportLoader("Loading Accounts");
    sys->account_db.Load(str);

    // Load Expenses
    sys->FullPath(EXPENSE_DATA_DIR, str);
    EnsureDirExists(str);  // TEMP FIX for Pi 5: Ensure directory exists
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
    for (pi = settings->PrinterList(); pi != nullptr; pi = pi->next)
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
        snprintf(prtstr, sizeof(prtstr), "file:%s/", str);
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

    if (MasterControl->TermList() == nullptr)
    {
        ReportError("No terminals could be opened");
        EndSystem();
    }

    Terminal *term = MasterControl->TermList();
    while (term != nullptr)
    {
        term->Initialize();
        term = term->next;
    }

    // Cleanup/Init & start
    sys->InitCurrentDay();

    // Start update system timer
    UpdateID = XtAppAddTimeOut(App, UPDATE_TIME,
                               (XtTimerCallbackProc) UpdateSystemCB, nullptr);

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
        while (term != nullptr)
        {
            if (term->cdu != nullptr)
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
        Dis = nullptr;
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
    if (MasterControl != nullptr)
    {
        // Deleting MasterControl keeps giving me error messages:
        //     "vt_main in free(): warning: chunk is already free"
        // I'm tired of the error messages and don't want to take
        // the time right now to fix it, so I'm commenting it out.
        // There's no destructor, so this step shouldn't be necessary
        // anyway.
        // delete MasterControl;
        MasterControl = nullptr;
    }
    if (MasterSystem)
    {
        delete MasterSystem;
        MasterSystem = nullptr;
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
        execl(VIEWTOUCH_RESTART, VIEWTOUCH_RESTART, VIEWTOUCH_PATH, nullptr);
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
    if (str == nullptr)
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
        snprintf(dollar_str, sizeof(dollar_str), "%d%c%03d%c%03d",
                dollars / 1000000, comma,
                (dollars / 1000) % 1000, comma,
                dollars % 1000);
	}
    else if (use_comma && dollars > 999)
        snprintf(dollar_str, sizeof(dollar_str), "%d%c%03d", dollars / 1000, comma, dollars % 1000);
    else if (dollars > 0)
        snprintf(dollar_str, sizeof(dollar_str), "%d", dollars);
    else
        dollar_str[0] = '\0';

    if (use_sign)
    {
        if (price < 0)
            snprintf(str, sizeof(str), "%s-%s%c%02d", settings->money_symbol.Value(),
                    dollar_str, point, change);
        else
            snprintf(str, sizeof(str), "%s%s%c%02d", settings->money_symbol.Value(),
                    dollar_str, point, change);
    }
    else
    {
        if (price < 0)
            snprintf(str, sizeof(str), "-%s%c%02d", dollar_str, point, change);
        else
            snprintf(str, sizeof(str), "%s%c%02d", dollar_str, point, change);
    }
    return str;
}

int ParsePrice(const char* source, int *value)
{
    FnTrace("ParsePrice()");
    genericChar str[256];
    genericChar* c = str;
    int numformat = NUMBER_STANDARD;  // Default to standard format
    
    // Safely get number format from MasterSystem if available
    if (MasterSystem != nullptr)
        numformat = MasterSystem->settings.number_format;

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
    if (sscanf(str, "%f", &val) != 1)
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

    // download to official location and then try to read again
    // EMERGENCY FIX: Use HTTP for reliable downloads on Pi 5, HTTPS has SSL issues
    const std::string vtdata_url = "http://www.viewtouch.com/vt_data";
    fprintf(stderr, "Trying download VT_DATA: %s from '%s'\n", SYSTEM_DATA_FILE, vtdata_url.c_str());
    DownloadFile(vtdata_url, SYSTEM_DATA_FILE);
    if (infile->Open(SYSTEM_DATA_FILE, version) == 0)
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
    Page *p = nullptr;
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
    const std::string tables_filepath = std::string(sys->data_path.str()) + "/" + MASTER_ZONE_DB1;
    const char *filename1 = tables_filepath.c_str();
    struct stat st;
    if (stat(tables_filepath.c_str(), &st) != 0)
    {
        // EMERGENCY FIX: Use HTTP for reliable downloads on Pi 5
        const std::string tables_url = "http://www.viewtouch.com/tables.dat";
        DownloadFile(tables_url, tables_filepath);
    }

    if (zone_db->Load(filename1))
    {
        RestoreBackup(filename1);
        //zone_db->Purge();	// maybe remove non-system pages, but not all!
        zone_db->Load(filename1);
    }

    // Load Menu
    const std::string zone_db_filepath = std::string(sys->data_path.str()) + "/" + MASTER_ZONE_DB2;
    const char *filename2 = zone_db_filepath.c_str();
    struct stat st2;
    if (stat(zone_db_filepath.c_str(), &st2) != 0)
    {
        // EMERGENCY FIX: Use HTTP for reliable downloads on Pi 5
        const std::string zone_db_url = "http://www.viewtouch.com/zone_db.dat";
        DownloadFile(zone_db_url, zone_db_filepath);
    }
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
    if (con->zone_db == nullptr)
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
    zone_db     = nullptr;
    master_copy = 0;
    term_list   = nullptr;
}

int Control::Add(Terminal *term)
{
    FnTrace("Control::Add(Terminal)");
    if (term == nullptr)
        return 1;

    term->system_data = MasterSystem;
    term_list.AddToTail(term);
    term->UpdateZoneDB(this);
    return 0;
}

int Control::Add(Printer *p)
{
    FnTrace("Control::Add(Printer)");

    if (p == nullptr)
        return 1;

    p->parent = this;
    printer_list.AddToTail(p);
    return 0;
}

int Control::Remove(Terminal *term)
{
    FnTrace("Control::Remove(Terminal)");
    if (term == nullptr)
        return 1;

    term->parent = nullptr;
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
        if (ptr == nullptr)
            zone_db = nullptr;
    }
    return 0;
}

int Control::Remove(Printer *p)
{
    FnTrace("Control::Remove(Printer)");
    if (p == nullptr)
        return 1;

    p->parent = nullptr;
    printer_list.Remove(p);
    return 0;
}

Terminal *Control::FindTermByHost(const char* host)
{
    FnTrace("Control::FindTermByHost()");

    for (Terminal *term = TermList(); term != nullptr; term = term->next)
    {
        if (strcmp(term->host.Value(), host) == 0)
            return term;
    }

    return nullptr;
}

int Control::SetAllMessages(const char* message)
{
    FnTrace("Control::SetAllMessages()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->SetMessage(message);
    return 0;
}

int Control::SetAllTimeouts(int timeout)
{
    FnTrace("Control::SetAllTimeouts()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->SetCCTimeout(timeout);
    return 0;
}

int Control::SetAllCursors(int cursor)
{
    FnTrace("Control::SetAllCursors()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->SetCursor(cursor);
    return 0;
}

int Control::SetAllIconify(int iconify)
{
    FnTrace("Control::SetAllIconify()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->SetIconify(iconify);
    return 0;
}

int Control::ClearAllMessages()
{
    FnTrace("Control::ClearAllMessages()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->ClearMessage();
    return 0;
}

int Control::ClearAllFocus()
{
    FnTrace("Control::ClearAllFocus()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->previous_zone = nullptr;
    return 0;
}

int Control::LogoutAllUsers()
{
    FnTrace("Control::LogoutAllUsers()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->LogoutUser();
    return 0;
}

int Control::LogoutKitchenUsers()
{
    FnTrace("Control::LogoutKitchenUsers()");
    Terminal *term = TermList();
    int count = 0;

    while (term != nullptr)
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

    while (term != nullptr)
    {
        term->Update(update_message, value);
        term = term->next;
    }
    return 0;
}

int Control::UpdateOther(Terminal *local, int update_message, const genericChar* value)
{
    FnTrace("Control::UpdateOther()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        if (term != local)
            term->Update(update_message, value);
    return 0;
}

int Control::IsUserOnline(Employee *e)
{
    FnTrace("Control::IsUserOnline()");
    if (e == nullptr)
        return 0;

    for (Terminal *term = TermList(); term != nullptr; term = term->next)
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
            UpdateAll(UPDATE_TERMINALS, nullptr);
            return 0;
        }
        ptr = ptr->next;
    }
    return 1;  // invalid pointer given
}

int Control::OpenDialog(const char* message)
{
    FnTrace("Control::OpenDialog()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->OpenDialog(message);
    return 0;
}

int Control::KillAllDialogs()
{
    FnTrace("Control::KillAllDialogs()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->KillDialog();
    return 0;
}

Printer *Control::FindPrinter(const char* host, int port)
{
    FnTrace("Control::FindPrinter(const char* , int)");
    for (Printer *p = PrinterList(); p != nullptr; p = p->next)
	{
        if (p->MatchHost(host, port))
            return p;
	}

    return nullptr;
}

Printer *Control::FindPrinter(const char* term_name)
{
    FnTrace("Control::FindPrinter(const char* )");

    for (Printer *p = PrinterList(); p != nullptr; p = p->next)
    {
        if (strcmp(p->term_name.Value(), term_name) == 0)
            return p;
    }

    return nullptr;
}

Printer *Control::FindPrinter(int printer_type)
{
    FnTrace("Control::FindPrinter(int)");

    for (Printer *p = PrinterList(); p != nullptr; p = p->next)
    {
        if (p->IsType(printer_type))
            return p;
    }

    return nullptr;
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
    if (p == nullptr)
        return 1;

    Printer *ptr = PrinterList();
    while (ptr)
    {
        if (ptr == p)
        {
            Remove(p);
            delete p;
            if (update)
                UpdateAll(UPDATE_PRINTERS, nullptr);
            return 0;
        }
        ptr = ptr->next;
    }
    return 1;  // invalid pointer given
}

int Control::TestPrinters(Terminal *term, int report)
{

    FnTrace("Control::TestPrinters()");

    for (Printer *p = PrinterList(); p != nullptr; p = p->next)
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
    if (zone_db == nullptr)
        return nullptr;

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
    if (zone_db == nullptr || sys == nullptr)
        return 1;

    genericChar str[256];
    snprintf(str, sizeof(str), "%s/%s", sys->data_path.Value(), MASTER_ZONE_DB2);
    BackupFile(str);
    return zone_db->Save(str, PAGECLASS_MENU);
}

int Control::SaveTablePages()
{
    FnTrace("Control::SaveTablePages()");
    System  *sys = MasterSystem;
    if (zone_db == nullptr || sys == nullptr)
        return 1;

    genericChar str[256];
    snprintf(str, sizeof(str), "%s/%s", sys->data_path.Value(), MASTER_ZONE_DB1);
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
    if (termhost != nullptr)
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
    TermInfo *ti = nullptr;
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
        if (ti != nullptr)
        {
            term = ti->FindTerm(MasterControl);
            if (term == nullptr)
            {
                if (strcmp(update, "update") == 0)
                    SetTermInfo(ti, termname, nullptr, &remote_terminal[idx]);
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
    Terminal *term = nullptr;
    TermInfo *ti = nullptr;
    Printer  *printer = nullptr;

    idx = GetTermWord(termhost, STRLENGTH, remote_terminal, idx);
    ti = MasterSystem->settings.FindTerminal(termhost);
    if (ti != nullptr)
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
    Terminal *term = nullptr;
    TermInfo *ti = nullptr;

    idx = GetTermWord(termhost, STRLENGTH, remote_terminal, idx);
    idx = GetTermWord(clonedest, STRLENGTH, remote_terminal, idx);
    ti = MasterSystem->settings.FindTerminal(termhost);
    if (ti != nullptr)
    {
        term = ti->FindTerm(MasterControl);
        if (term != nullptr)
            retval = CloneTerminal(term, clonedest, termhost);
    }

    return retval;
}

int ProcessRemoteOrderEntry(SubCheck *subcheck, Order **order, const char* key, const char* value)
{
    FnTrace("ProcessRemoteOrderEntry()");
    int retval = CALLCTR_ERROR_NONE;
    static Order *detail = nullptr;
    SalesItem *sales_item;
    int record;  // not really used; only for FindByItemCode

    if ((strncmp(key, "ItemCode", 8) == 0) ||
        (strncmp(key, "ProductCode", 11) == 0))
    {
        if (*order != nullptr)
            ReportError("Have an order we should get rid of....");
        sales_item = MasterSystem->menu.FindByItemCode(value, record);
        if (sales_item)
            *order = new Order(&MasterSystem->settings, sales_item, nullptr);
        else
            retval = CALLCTR_ERROR_BADITEM;
    }
    else if ((strncmp(key, "DetailCode", 10) == 0) ||
             (strncmp(key, "AddonCode", 9) == 0))
    {
        if (detail != nullptr)
            ReportError("Have a detail we should get rid of....");
        sales_item = MasterSystem->menu.FindByItemCode(value, record);
        if (sales_item)
            detail = new Order(&MasterSystem->settings, sales_item, nullptr);
        else
            retval = CALLCTR_ERROR_BADDETAIL;
    }
    else if ((strncmp(key, "EndItem", 7) == 0) ||
             (strncmp(key, "EndProduct", 10) == 0))
    {
        subcheck->Add(*order, &MasterSystem->settings);
        *order = nullptr;
    }
    else if ((strncmp(key, "EndDetail", 9) == 0) ||
             (strncmp(key, "EndAddon", 8) == 0))
    {
        (*order)->Add(detail);
        detail = nullptr;
    }
    else if (*order != nullptr)
    {
        if (strncmp(key, "ItemQTY", 7) == 0)
            (*order)->count = atoi(value);
        else if (strncmp(key, "ProductQTY", 10) == 0)
            (*order)->count = atoi(value);
        else if (detail != nullptr && strncmp(key, "AddonQualifier", 14) == 0)
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
    SubCheck *subcheck = nullptr;
    Order    *order = nullptr;
    Printer  *printer = nullptr;
    Report   *report = nullptr;
    Terminal *term = MasterControl->TermList();

    subcheck = check->SubList();
    while (subcheck != nullptr)
    {
        order = subcheck->OrderList();
        while (order != nullptr)
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
        MasterControl->UpdateAll(UPDATE_CHECKS, nullptr);
        check->current_sub = check->FirstOpenSubCheck();

        // need to print the check
        printer = MasterControl->FindPrinter(PRINTER_REMOTEORDER);
        if (printer != nullptr) {
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
            strncat(result_str, "COMPLETE", sizeof(result_str) - strlen(result_str) - 1);
        else if (status == CALLCTR_STATUS_INCOMPLETE)
            strncat(result_str, "INCOMPLETE", sizeof(result_str) - strlen(result_str) - 1);
        else if (status == CALLCTR_STATUS_FAILED)
            strncat(result_str, "FAILED", sizeof(result_str) - strlen(result_str) - 1);
        else
            strncat(result_str, "UNKNOWNSTAT", sizeof(result_str) - strlen(result_str) - 1);
    }
    else
    {
        if (result_code == CALLCTR_ERROR_BADITEM)
            strncat(result_str, "BADITEM", sizeof(result_str) - strlen(result_str) - 1);
        else if (result_code == CALLCTR_ERROR_BADDETAIL)
            strncat(result_str, "BADDETAIL", sizeof(result_str) - strlen(result_str) - 1);
        else
            strncat(result_str, "UNKNOWNERR", sizeof(result_str) - strlen(result_str) - 1);
    }

    strncat(result_str, ":", sizeof(result_str) - strlen(result_str) - 1);
    if (result_code == CALLCTR_ERROR_NONE)
        strncat(result_str, "PRINTED", sizeof(result_str) - strlen(result_str) - 1);
    else
        strncat(result_str, "NOTPRINTED", sizeof(result_str) - strlen(result_str) - 1);

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
    Check     *check = nullptr;
    SubCheck  *subcheck = nullptr;
    Order     *order = nullptr;
    char       StoreNum[STRSHORT];
    int        status = CALLCTR_STATUS_INCOMPLETE;

    kvif.Set(sock_fd);

    write(sock_fd, "SENDORDER\n", 10);

    check = new Check(settings, CHECK_DELIVERY);
    if (check == nullptr)
        return retval;
    subcheck = check->NewSubCheck();
    if (subcheck == nullptr)
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
    Check    *ret_check = nullptr;
    Check    *curr_check = nullptr;
    Archive  *archive = nullptr;
    SubCheck *subcheck = nullptr;
    Payment  *payment = nullptr;
    Credit   *credit = nullptr;
    char      cn[STRLENGTH];

    curr_check = MasterSystem->CheckList();
    while (ret_check == nullptr && archive != MasterSystem->ArchiveList())
    {
        while (curr_check != nullptr && ret_check == nullptr)
        {
            subcheck = curr_check->SubList();
            while (subcheck != nullptr && ret_check == nullptr)
            {
                payment = subcheck->PaymentList();
                while (payment != nullptr && ret_check == nullptr)
                {
                    if (payment->credit != nullptr)
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
        if (ret_check == nullptr)
        {
            if (archive == nullptr)
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
    Check    *check = nullptr;
    SubCheck *subcheck = nullptr;
    Payment  *payment = nullptr;
    Credit   *credit = nullptr;

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
        while (subcheck != nullptr)
        {
            payment = subcheck->PaymentList();
            while (payment != nullptr)
            {
                if (payment->credit != nullptr)
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
        if (MasterControl->TermList() != nullptr &&
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

    if (sys->eod_term != nullptr && sys->eod_term->eod_processing != EOD_DONE)
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
        if (term->reload_zone_db && term->user == nullptr)
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
                term->Update(u, nullptr);
        }

        if (term->cdu != nullptr)
            term->cdu->Refresh();

        if (term->kill_me)
            con->KillTerm(term);
        term = tnext;
    }

    if (con->TermList() == nullptr)
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
    static Printer *printer = nullptr;
    static Report *report = nullptr;
    static KeyValueInputFile kvfile;
    static int exit_system = 0;

    if (!kvfile.IsOpen())
        kvfile.Open(VIEWTOUCH_COMMAND);

    if (working)
    {
        working = RunReport(nullptr, printer);
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
        if (printer != nullptr)
            delete printer;
        if (report != nullptr)
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
    Terminal *term = nullptr;

    count = MasterControl->TermList()->TermsInUse();
    snprintf(message, STRLENGTH, "UserCount:  %d users active", count);
    ReportError(message);

    if (count > 0)
    {
        term = MasterControl->TermList();
        while (term != nullptr)
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
    static Terminal *term = nullptr;
    static int count = 0;
    int retval = 0;

    if (term == nullptr)
        term = MasterControl->TermListEnd();

    while (term != nullptr && retval == 0)
    {
        if (term->page != nullptr)
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
    static Report *report = nullptr;
    genericChar report_name[STRLONG] = "";
    genericChar report_from[STRLONG] = "";
    genericChar report_to[STRLONG] = "";
    int idx = 0;
    Terminal *term = MasterControl->TermList();
    System *system_data = term->system_data;

    if (report == nullptr && report_string != nullptr)
    {
        TimeInfo from;
        TimeInfo to;
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
            system_data->DepositReport(term, from, to, nullptr, report);
        else if (strcmp(report_name, "expense") == 0)
            system_data->ExpenseReport(term, from, to, nullptr, report, nullptr);
        else if (strcmp(report_name, "revenue") == 0)
            system_data->BalanceReport(term, from, to, report);
        else if (strcmp(report_name, "royalty") == 0)
            system_data->RoyaltyReport(term, from, to, nullptr, report, nullptr);
        else if (strcmp(report_name, "sales") == 0)
            system_data->SalesMixReport(term, from, to, nullptr, report);
        else if (strcmp(report_name, "audit") == 0)
            system_data->AuditingReport(term, from, to, nullptr, report, nullptr);
        else if (strcmp(report_name, "batchsettle") == 0)
        {
            MasterSystem->cc_report_type = CC_REPORT_BATCH;
            system_data->CreditCardReport(term, from, to, nullptr, report, nullptr);
        }
        else
        {
            fprintf(stderr, "Unknown report '%s'\n", report_name);
            delete report;
            report = nullptr;
        }
    }

    if (report != nullptr)
    {
        if (report->is_complete > 0)
        {
            report->Print(printer);
            delete report;
            report = nullptr;
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
    Printer *retPrinter = nullptr;

    retPrinter = NewPrinterFromString(printer_description);
    return retPrinter;
}


/**** Functions ****/
int GetFontSize(int font_id, int &w, int &h)
{
    FnTrace("GetFontSize()");
    
    // Check if we have a valid font_id
    if (font_id < 0 || font_id >= 80)  // Increased to accommodate new font families
    {
        w = FontWidth[FONT_DEFAULT];
        h = FontHeight[FONT_DEFAULT];
        return 0;
    }
    
    // If we have Xft fonts loaded, use them
    if (FontInfo[font_id] && Dis)
    {
        // Use XftFont metrics
        w = FontInfo[font_id]->max_advance_width; // Approximate character width
        h = FontInfo[font_id]->height;
    }
    else
    {
        // Fall back to legacy font metrics
        w = FontWidth[font_id];
        h = FontHeight[font_id];
    }
    return 0;
}

int GetTextWidth(const char* my_string, int len, int font_id)
{
    FnTrace("GetTextWidth()");
    if (my_string == nullptr || len <= 0)
        return 0;
    
    // Check if we have a valid font_id
    if (font_id < 0 || font_id >= 80)  // Increased to accommodate new font families
        font_id = FONT_DEFAULT;
    
    // If we have Xft fonts loaded and display is available, use Xft
    if (FontInfo[font_id] && Dis)
    {
        // Use Xft for accurate text measurement
        XGlyphInfo extents;
        // FIXED: reinterpret_cast for Xft text width calculation in UI layout
        // Part of font system migration from bitmapped to scalable Xft fonts
        XftTextExtentsUtf8(Dis, FontInfo[font_id], reinterpret_cast<const unsigned char*>(my_string), len, &extents);
        return extents.width;
    }
    else
    {
        // Fall back to legacy calculation
        return FontWidth[font_id] * len;
    }
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

// Scalable font name mapping for Xft fonts
const char* GetScalableFontName(int font_id)
{
    switch (font_id)
    {
    // Legacy Times fonts (kept for compatibility)
    case FONT_TIMES_14:  return "Times New Roman-14:style=Regular";
    case FONT_TIMES_18:  return "Times New Roman-18:style=Regular";
    case FONT_TIMES_20:  return "Times New Roman-20:style=Regular";
    case FONT_TIMES_24:  return "Times New Roman-24:style=Regular";
    case FONT_TIMES_34:  return "Times New Roman-34:style=Regular";
    case FONT_TIMES_14B: return "Times New Roman-14:style=Bold";
    case FONT_TIMES_18B: return "Times New Roman-18:style=Bold";
    case FONT_TIMES_20B: return "Times New Roman-20:style=Bold";
    case FONT_TIMES_24B: return "Times New Roman-24:style=Bold";
    case FONT_TIMES_34B: return "Times New Roman-34:style=Bold";
    case FONT_COURIER_18: return "Courier New-18:style=Regular";
    case FONT_COURIER_18B: return "Courier New-18:style=Bold";
    case FONT_COURIER_20: return "Courier New-20:style=Regular";
    case FONT_COURIER_20B: return "Courier New-20:style=Bold";
    
    // Modern POS Fonts - DejaVu Sans (Superior readability for POS)
    case FONT_DEJAVU_14:  return "DejaVu Sans-14:style=Book";
    case FONT_DEJAVU_16:  return "DejaVu Sans-16:style=Book";
    case FONT_DEJAVU_18:  return "DejaVu Sans-18:style=Book";
    case FONT_DEJAVU_20:  return "DejaVu Sans-20:style=Book";
    case FONT_DEJAVU_24:  return "DejaVu Sans-24:style=Book";
    case FONT_DEJAVU_28:  return "DejaVu Sans-28:style=Book";
    case FONT_DEJAVU_14B: return "DejaVu Sans-14:style=Bold";
    case FONT_DEJAVU_16B: return "DejaVu Sans-16:style=Bold";
    case FONT_DEJAVU_18B: return "DejaVu Sans-18:style=Bold";
    case FONT_DEJAVU_20B: return "DejaVu Sans-20:style=Bold";
    case FONT_DEJAVU_24B: return "DejaVu Sans-24:style=Bold";
    case FONT_DEJAVU_28B: return "DejaVu Sans-28:style=Bold";
    
    // Monospace fonts - Perfect for prices, numbers, and financial data
    case FONT_MONO_14:    return "DejaVu Sans Mono-14:style=Book";
    case FONT_MONO_16:    return "DejaVu Sans Mono-16:style=Book";
    case FONT_MONO_18:    return "DejaVu Sans Mono-18:style=Book";
    case FONT_MONO_20:    return "DejaVu Sans Mono-20:style=Book";
    case FONT_MONO_24:    return "DejaVu Sans Mono-24:style=Book";
    case FONT_MONO_14B:   return "DejaVu Sans Mono-14:style=Bold";
    case FONT_MONO_16B:   return "DejaVu Sans Mono-16:style=Bold";
    case FONT_MONO_18B:   return "DejaVu Sans Mono-18:style=Bold";
    case FONT_MONO_20B:   return "DejaVu Sans Mono-20:style=Bold";
    case FONT_MONO_24B:   return "DejaVu Sans Mono-24:style=Bold";
    
    // Classic Serif Fonts - EB Garamond 8 (elegant serif)
    case FONT_GARAMOND_14:  return "EB Garamond-14:style=Regular";
    case FONT_GARAMOND_16:  return "EB Garamond-16:style=Regular";
    case FONT_GARAMOND_18:  return "EB Garamond-18:style=Regular";
    case FONT_GARAMOND_20:  return "EB Garamond-20:style=Regular";
    case FONT_GARAMOND_24:  return "EB Garamond-24:style=Regular";
    case FONT_GARAMOND_28:  return "EB Garamond-28:style=Regular";
    case FONT_GARAMOND_14B: return "EB Garamond-14:style=Bold";
    case FONT_GARAMOND_16B: return "EB Garamond-16:style=Bold";
    case FONT_GARAMOND_18B: return "EB Garamond-18:style=Bold";
    case FONT_GARAMOND_20B: return "EB Garamond-20:style=Bold";
    case FONT_GARAMOND_24B: return "EB Garamond-24:style=Bold";
    case FONT_GARAMOND_28B: return "EB Garamond-28:style=Bold";
    
    // Classic Serif Fonts - URW Bookman (warm, readable serif)
    case FONT_BOOKMAN_14:   return "URW Bookman-14:style=Light";
    case FONT_BOOKMAN_16:   return "URW Bookman-16:style=Light";
    case FONT_BOOKMAN_18:   return "URW Bookman-18:style=Light";
    case FONT_BOOKMAN_20:   return "URW Bookman-20:style=Light";
    case FONT_BOOKMAN_24:   return "URW Bookman-24:style=Light";
    case FONT_BOOKMAN_28:   return "URW Bookman-28:style=Light";
    case FONT_BOOKMAN_14B:  return "URW Bookman-14:style=Demi";
    case FONT_BOOKMAN_16B:  return "URW Bookman-16:style=Demi";
    case FONT_BOOKMAN_18B:  return "URW Bookman-18:style=Demi";
    case FONT_BOOKMAN_20B:  return "URW Bookman-20:style=Demi";
    case FONT_BOOKMAN_24B:  return "URW Bookman-24:style=Demi";
    case FONT_BOOKMAN_28B:  return "URW Bookman-28:style=Demi";
    
    // Classic Serif Fonts - Nimbus Roman (clean, professional serif)
    case FONT_NIMBUS_14:    return "Nimbus Roman-14:style=Regular";
    case FONT_NIMBUS_16:    return "Nimbus Roman-16:style=Regular";
    case FONT_NIMBUS_18:    return "Nimbus Roman-18:style=Regular";
    case FONT_NIMBUS_20:    return "Nimbus Roman-20:style=Regular";
    case FONT_NIMBUS_24:    return "Nimbus Roman-24:style=Regular";
    case FONT_NIMBUS_28:    return "Nimbus Roman-28:style=Regular";
    case FONT_NIMBUS_14B:   return "Nimbus Roman-14:style=Bold";
    case FONT_NIMBUS_16B:   return "Nimbus Roman-16:style=Bold";
    case FONT_NIMBUS_18B:   return "Nimbus Roman-18:style=Bold";
    case FONT_NIMBUS_20B:   return "Nimbus Roman-20:style=Bold";
    case FONT_NIMBUS_24B:   return "Nimbus Roman-24:style=Bold";
    case FONT_NIMBUS_28B:   return "Nimbus Roman-28:style=Bold";
    
    // Default to modern DejaVu Sans instead of Times New Roman
    default:                return "DejaVu Sans-18:style=Book";
    }
}
