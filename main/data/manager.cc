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
 * manager.cc - revision 252 (10/20/98)
 * Implementation of manager module
 */

                            // ViewTouch includes
#include "manager.hh"
#include "system.hh"
#include "check.hh"
#include "sales.hh"
#include "pos_zone.hh"
#include "terminal.hh"
#include "printer.hh"
#include "drawer.hh"
#include "data_file.hh"
#include "data_persistence_manager.hh"
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
#include "zone/dialog_zone.hh"

#include "conf_file.hh"
#include "src/utils/vt_logger.hh"  // Modern C++ logging
#include "safe_string_utils.hh"     // Safe string operations
#include "date/date.h"      // helper library to output date strings with std::chrono
#include "src/network/reverse_ssh_service.hh"  // Reverse SSH tunnel service

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <memory>

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
#include <X11/Xft/Xft.h>    // Xft font rendering library
#include <string>           // Introduces string types, character traits and a set of converting functions
#include <cctype>           // Declares a set of functions to classify and transform individual characters
#include <cstring>          // Functions for dealing with C-style strings — null-terminated arrays of characters; is the C++ version of the classic string.h header from C
#include <string>           // Strings in C++ are of the std::string variety
#include <csignal>          // C library to handle signals
#include <chrono>           // Time utilities for event loop monitoring
#include <fcntl.h>          // File Control
#include <filesystem>       // generic filesystem functions available since C++17
#include <cstdio>           // for std::remove

#ifdef DMALLOC
#include <dmalloc.h>
#endif

namespace fs = std::filesystem;
using namespace date; // for date conversion on streams

// Forward declarations
const char* GetCompatibleFontSpec(int font_id, const char* desired_family);
const char* GetGlobalFontFamily();

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
                            "Fast Food", "Self Order", "Kitchen Video", "Kitchen Video2", nullptr};

int TermTypeValue[] = { TERMINAL_NORMAL, TERMINAL_ORDER_ONLY,
                        TERMINAL_BAR, TERMINAL_BAR2,
                        TERMINAL_FASTFOOD, TERMINAL_SELFORDER, TERMINAL_KITCHEN_VIDEO,
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
static int          ScrNo = 0;
static XFontStruct *FontInfo[32] = {nullptr};
static int          FontWidth[32];
static int          FontHeight[32];
static int          FontBaseline[32];
static XftFont      *XftFontsArr[32] = {nullptr};
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
    {FONT_TIMES_20,     9, 20, "DejaVu Serif:size=12:style=Book"},
    {FONT_TIMES_24,    12, 24, "DejaVu Serif:size=14:style=Book"},
    {FONT_TIMES_34,    15, 33, "DejaVu Serif:size=18:style=Book"},
    {FONT_TIMES_48,    26, 52, "DejaVu Serif:size=28:style=Book"},
    {FONT_TIMES_20B,   10, 20, "DejaVu Serif:size=12:style=Bold"},
    {FONT_TIMES_24B,   12, 24, "DejaVu Serif:size=14:style=Bold"},
    {FONT_TIMES_34B,   16, 33, "DejaVu Serif:size=18:style=Bold"},
    {FONT_TIMES_48B,   28, 52, "DejaVu Serif:size=28:style=Bold"},
    {FONT_TIMES_14,     7, 14, "DejaVu Serif:size=10:style=Book"},
    {FONT_TIMES_14B,    8, 14, "DejaVu Serif:size=10:style=Bold"},
    {FONT_TIMES_18,     9, 18, "DejaVu Serif:size=11:style=Book"},
    {FONT_TIMES_18B,   10, 18, "DejaVu Serif:size=11:style=Bold"},
    {FONT_COURIER_18,  10, 18, "Liberation Serif:size=11:style=Regular"},
    {FONT_COURIER_18B, 10, 18, "Liberation Serif:size=11:style=Bold"},
    {FONT_COURIER_20,  10, 20, "Liberation Serif:size=12:style=Regular"},
    {FONT_COURIER_20B, 10, 20, "Liberation Serif:size=12:style=Bold"}
};

static XtIntervalId UpdateID = 0;   // update callback function id
static int LastMin  = -1;
static int LastHour = -1;
static int LastMeal = -1;
static int LastDay  = -1;

// Scheduled restart variables
int restart_dialog_shown = 0;      // Flag to prevent multiple dialogs
int restart_postponed_until = 0;   // Time when postponed restart should happen
static TimeInfo last_restart_check;       // Last time we checked for restart
unsigned long restart_timeout_id = 0; // Timeout for auto restart

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
    "wget -nv -T 2 -t 2 http://www.viewtouch.com/vt_updates/vt-update -O /tmp/vt-update"

static const std::string VIEWTOUCH_CONFIG = std::string(VIEWTOUCH_PATH) + "/dat/.viewtouch_config";

// vt_data is back in bin/ after a brief stint in dat/
#define SYSTEM_DATA_FILE     VIEWTOUCH_PATH "/bin/" MASTER_ZONE_DB3

#define TERM_RELOAD_FONTS 0xA5

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
int      ReloadFonts();  // Function to reload fonts when global defaults change

genericChar* GetMachineName(genericChar* str = nullptr, int len = STRLENGTH)
{
    FnTrace("GetMachineName()");
    if (len <= 0) {
        return nullptr; // invalid parameter
    }
    
    struct utsname uts;
    static genericChar buffer[STRLENGTH];
    if (str == nullptr)
        str = buffer;

    if (uname(&uts) == 0) {
        strncpy(str, uts.nodename, static_cast<size_t>(len - 1));
        str[len - 1] = '\0'; // ensure null termination
    } else {
        str[0] = '\0';
    }
    return str;
}

void ViewTouchError(const char* message, int do_sleep)
{
    FnTrace("ViewTouchError()");
    if (!message) {
        return; // invalid parameter
    }
    
    genericChar errormsg[STRLONG];
    int sleeplen = (debug_mode ? 1 : 5);
    Settings *settings = &(MasterSystem->settings);

    if (settings->expire_message1.empty())
    {
        snprintf(errormsg, sizeof(errormsg), "%s\\%s\\%s", message,
             "Please contact support.", " 541-515-5913");
    }
    else
    {
        snprintf(errormsg, sizeof(errormsg), "%s\\%s\\%s\\%s\\%s", message,
                 settings->expire_message1.Value(),
                 settings->expire_message2.Value(),
                 settings->expire_message3.Value(),
                 settings->expire_message4.Value());
    }
    ReportLoader(errormsg);
    if (do_sleep)
        sleep(static_cast<unsigned int>(sleeplen));
}

bool DownloadFile(const std::string &url, const std::string &destination)
{
    // Create a temporary file to avoid overwriting the original until download is complete
    std::string temp_file = destination + ".tmp";
    std::ofstream fout(temp_file, std::ios::binary);
    if (!fout.is_open()) {
        std::cerr << "Error: Cannot open temporary file '" << temp_file << "' for writing" << std::endl;
        return false;
    }

    try {
        curlpp::Cleanup cleaner;
        curlpp::Easy request;

        // Set up the request with proper options for both HTTP and HTTPS
        request.setOpt(curlpp::options::Url(url));
        request.setOpt(curlpp::options::WriteStream(&fout));
        request.setOpt(curlpp::options::FollowLocation(true));  // Follow redirects
        request.setOpt(curlpp::options::Timeout(30));           // 30 second timeout
        request.setOpt(curlpp::options::ConnectTimeout(10));    // 10 second connect timeout
        
        // For HTTPS compatibility on Raspberry Pi and other systems
        request.setOpt(curlpp::options::SslVerifyPeer(false));  // Disable SSL verification for compatibility
        request.setOpt(curlpp::options::SslVerifyHost(false));  // Disable host verification
        
        // Set user agent to avoid being blocked
        request.setOpt(curlpp::options::UserAgent("ViewTouch/1.0"));
        
        // Perform the request
        request.perform();
        
        // Check if file was written successfully by checking file size
        fout.close();
        std::ifstream check_file(temp_file, std::ios::binary | std::ios::ate);
        if (check_file.is_open()) {
            std::streamsize file_size = check_file.tellg();
            check_file.close();
            
            if (file_size > 0) {
                // Download successful, move temp file to final destination
                if (std::rename(temp_file.c_str(), destination.c_str()) == 0) {
                    std::cerr << "Successfully downloaded file '" << destination << "' from '" << url << "' (size: " << file_size << " bytes)" << std::endl;
                    return true;
                } else {
                    std::cerr << "Error: Could not move temporary file to final destination" << std::endl;
                    std::remove(temp_file.c_str());  // Clean up temp file
                    return false;
                }
            } else {
                std::cerr << "Downloaded file is empty from '" << url << "'" << std::endl;
                std::remove(temp_file.c_str());  // Remove empty temp file
                return false;
            }
        } else {
            std::cerr << "Cannot verify downloaded file from '" << url << "'" << std::endl;
            std::remove(temp_file.c_str());  // Remove temp file if we can't verify it
            return false;
        }
    }
    catch (const curlpp::LogicError & e)
    {
        std::cerr << "Logic error downloading file from '" << url << "': " << e.what() << std::endl;
        fout.close();
        std::remove(temp_file.c_str());  // Remove partial temp file
        return false;
    }
    catch (const curlpp::RuntimeError &e)
    {
        std::cerr << "Runtime error downloading file from '" << url << "': " << e.what() << std::endl;
        fout.close();
        std::remove(temp_file.c_str());  // Remove partial temp file
        return false;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Unexpected error downloading file from '" << url << "': " << e.what() << std::endl;
        fout.close();
        std::remove(temp_file.c_str());  // Remove partial temp file
        return false;
    }
}

bool DownloadFileWithFallback(const std::string &base_url, const std::string &destination)
{
    // Try HTTPS first
    std::string https_url = base_url;
    if (https_url.substr(0, 7) == "http://") {
        https_url = "https://" + https_url.substr(7);
    } else if (https_url.substr(0, 8) != "https://") {
        https_url = "https://" + https_url;
    }
    
    std::cerr << "Attempting HTTPS download from '" << https_url << "'" << std::endl;
    if (DownloadFile(https_url, destination)) {
        return true;
    }
    
    // If HTTPS fails, try HTTP
    std::string http_url = base_url;
    if (http_url.substr(0, 8) == "https://") {
        http_url = "http://" + http_url.substr(8);
    } else if (http_url.substr(0, 7) != "http://") {
        http_url = "http://" + http_url;
    }
    
    std::cerr << "HTTPS failed, attempting HTTP download from '" << http_url << "'" << std::endl;
    if (DownloadFile(http_url, destination)) {
        return true;
    }
    
    std::cerr << "Both HTTPS and HTTP downloads failed for '" << base_url << "'" << std::endl;
    return false;
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
        (void)conf.GetValue(autoupdate, "autoupdate");  // Suppress nodiscard warnings
        (void)conf.GetValue(select_timeout, "selecttimeout");
        (void)conf.GetValue(debug_mode, "debugmode");
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

    // Initialize modern logging system
#ifdef DEBUG
    vt::Logger::Initialize("/var/log/viewtouch", "debug", true, true);
#else
    vt::Logger::Initialize("/var/log/viewtouch", "info", false, true);
#endif
    vt::Logger::info("ViewTouch Main (vt_main) starting - Version {}", 
                     viewtouch::get_version_short());

    genericChar socket_file[256] = "";
    if (argc >= 2)
    {
        if (strcmp(argv[1], "version") == 0)
        {
            // return version for vt_update
            printf("1\n");
            vt::Logger::Shutdown();  // Clean shutdown
            return 0;
        }
        strncpy(socket_file, argv[1], sizeof(socket_file) - 1);
        socket_file[sizeof(socket_file) - 1] = '\0'; // ensure null termination
    }

    LoaderSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (LoaderSocket <= 0)
    {
        vt::Logger::critical("Can't open initial loader socket - errno: {}", errno);
        ReportError("Can't open initial loader socket");
        exit(1);
    }
    vt::Logger::debug("Loader socket opened successfully: {}", LoaderSocket);

    struct sockaddr_un server_adr;
    server_adr.sun_family = AF_UNIX;
    strncpy(server_adr.sun_path, socket_file, sizeof(server_adr.sun_path) - 1);
    server_adr.sun_path[sizeof(server_adr.sun_path) - 1] = '\0'; // ensure null termination
    sleep(1);
    
    vt::Logger::debug("Connecting to loader socket: {}", socket_file);
    if (connect(LoaderSocket, (struct sockaddr *) &server_adr,
                SUN_LEN(&server_adr)) < 0)
    {
        vt::Logger::critical("Can't connect to loader socket '{}' - errno: {}", socket_file, errno);
        ReportError("Can't connect to loader");
        close(LoaderSocket);
        exit(1);
    }
    vt::Logger::info("Connected to loader successfully");

    // Read starting commands
    use_net = 1;
    int purge = 0;
    int notrace = 0;
    genericChar data_path[256] = "\0";

    std::array<genericChar, 1024> buffer{};
    genericChar* c = buffer.data();
    const genericChar* buffer_end = buffer.data() + buffer.size() - 1; // Leave space for null terminator
    
    for (;;)
    {
        // Critical fix: Check buffer bounds to prevent overflow
        if (c >= buffer_end)
        {
            fprintf(stderr, "Manager: Buffer overflow prevented in command reading\n");
            break;
        }
        
        ssize_t no = read(LoaderSocket, c, 1);
        if (no == 1)
        {
            if (*c == '\0')
            {
                c = buffer.data();
                if (strcmp(buffer.data(), "done") == 0)
                {
                    break;
                }
                else if (strncmp(buffer.data(), "datapath ", 9) == 0)
                {
                    // Critical fix: Use strncpy with bounds checking
                    strncpy(data_path, &buffer[9], sizeof(data_path) - 1);
                    data_path[sizeof(data_path) - 1] = '\0';
                }
                else if (strcmp(buffer.data(), "netoff") == 0)
                {
                    use_net = 0;
                }
                else if (strcmp(buffer.data(), "purge") == 0)
                {
                    purge = 1;
                }
                else if (strncmp(buffer.data(), "display ", 8) == 0)
                {
                    strncpy(displaystr, &buffer[8], STRLENGTH);
                    displaystr[STRLENGTH - 1] = '\0'; // Ensure null termination
                }
                else if (strcmp(buffer.data(), "notrace") == 0)
                {
                    notrace = 1;
                }
            }
            else
                ++c;
        }
        else if (no == 0)
        {
            // Connection closed
            break;
        }
        else
        {
            // Error reading
            perror("Manager: Error reading from loader socket");
            break;
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
    umask(0111); // a+rw, a-x

    SystemTime.Set();

    // Start application
    vt::Logger::info("Initializing ViewTouch system...");
    MasterSystem = std::make_unique<System>();
    if (MasterSystem == nullptr)
    {
        vt::Logger::critical("Failed to create main system object");
        ReportError("Couldn't create main system object");
        EndSystem();
    }
    vt::Logger::debug("System object created successfully");
    
    // Initialize data persistence manager
    vt::Logger::info("Initializing data persistence manager...");
    InitializeDataPersistence(MasterSystem.get());
    
    if (strlen(data_path) > 0) {
        vt::Logger::info("Using custom data path: {}", data_path);
        MasterSystem->SetDataPath(data_path);
    } else {
        vt::Logger::info("Using default data path: {}", VIEWTOUCH_PATH "/dat");
        MasterSystem->SetDataPath(VIEWTOUCH_PATH "/dat");
    }
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
    
    // Check if vt_data exists locally first
    bool vt_data_updated = false;
    
    // Check if auto-update is enabled by loading settings
    bool auto_update_enabled = true;  // Default to enabled for backward compatibility
    
    // Load settings from the master settings file using the same method as StartSystem
    char settings_path[STRLONG];
    MasterSystem->FullPath(MASTER_SETTINGS, settings_path);
    
    if (fs::exists(settings_path)) {
        Settings temp_settings;
        if (temp_settings.Load(settings_path) == 0) {
            auto_update_enabled = temp_settings.auto_update_vt_data;
            if (!auto_update_enabled) {
                ReportError("Auto-update of vt_data is disabled in settings");
            } else {
                ReportError("Auto-update of vt_data is enabled in settings");
            }
        } else {
            ReportError("Warning: Could not load settings file, defaulting to auto-update enabled");
        }
    } else {
        ReportError("Warning: Settings file not found, defaulting to auto-update enabled");
    }
    
    if (!fs::exists(SYSTEM_DATA_FILE)) {
        if (auto_update_enabled) {
            ReportError("Local vt_data not found, attempting to download from update servers...");
            
            // Try first URL: http://www.viewtouch.com/vt_data  
            ReportError("Attempting to download vt_data from http://www.viewtouch.com/vt_data");
            if (DownloadFileWithFallback("www.viewtouch.com/vt_data", SYSTEM_DATA_FILE)) {
                ReportError("Successfully downloaded vt_data from http update server");
                vt_data_updated = true;
            } else {
                // Try second URL: https://www.viewtouch.com/vt_data
                ReportError("First URL failed, attempting https://www.viewtouch.com/vt_data");
                if (DownloadFileWithFallback("https://www.viewtouch.com/vt_data", SYSTEM_DATA_FILE)) {
                    ReportError("Successfully downloaded vt_data from https update server");
                    vt_data_updated = true;
                } else {
                    ReportError("Error: Could not download vt_data from update servers and no local copy exists");
                    ReportError("ViewTouch cannot start without vt_data file");
                    exit(1);
                }
            }
        } else {
            ReportError("Error: Local vt_data not found and auto-update is disabled");
            ReportError("ViewTouch cannot start without vt_data file");
            exit(1);
        }
    } else {
        if (auto_update_enabled) {
            ReportError("Local vt_data found, attempting to download latest version...");
            
            // Try first URL: http://www.viewtouch.com/vt_data  
            ReportError("Attempting to download latest vt_data from http://www.viewtouch.com/vt_data");
            if (DownloadFileWithFallback("www.viewtouch.com/vt_data", SYSTEM_DATA_FILE)) {
                ReportError("Successfully downloaded latest vt_data from http update server");
                vt_data_updated = true;
            } else {
                // Try second URL: https://www.viewtouch.com/vt_data
                ReportError("First URL failed, attempting https://www.viewtouch.com/vt_data");
                if (DownloadFileWithFallback("https://www.viewtouch.com/vt_data", SYSTEM_DATA_FILE)) {
                    ReportError("Successfully downloaded latest vt_data from https update server");
                    vt_data_updated = true;
                } else {
                    ReportError("Warning: Could not download latest vt_data from update servers, using local copy");
                }
            }
        } else {
            ReportError("Local vt_data found, auto-update disabled - using existing file");
        }
    }
    
    // Clean up old vt_data backup files if download was successful
    if (vt_data_updated) {
        ReportError("Cleaning up old vt_data backup files...");
        std::string backup_file = std::string(SYSTEM_DATA_FILE) + ".bak";
        std::string backup_file2 = std::string(SYSTEM_DATA_FILE) + ".bak2";
        
        if (std::remove(backup_file.c_str()) == 0) {
            ReportError("Removed old vt_data.bak file");
        }
        if (std::remove(backup_file2.c_str()) == 0) {
            ReportError("Removed old vt_data.bak2 file");
        }
    }
    
    // Clean up old vt_data backup files if download was successful
    if (vt_data_updated) {
        ReportError("Cleaning up old vt_data backup files...");
        std::string backup_file = std::string(SYSTEM_DATA_FILE) + ".bak";
        std::string backup_file2 = std::string(SYSTEM_DATA_FILE) + ".bak2";
        
        if (std::remove(backup_file.c_str()) == 0) {
            ReportError("Removed old vt_data.bak file");
        }
        if (std::remove(backup_file2.c_str()) == 0) {
            ReportError("Removed old vt_data.bak2 file");
        }
    }
    
    // Now process any locally available updates (updates
    // from the previous step will be installed and ready for
    // this step).
    MasterSystem->CheckFileUpdates();
    if (purge)
        MasterSystem->ClearSystem();

    vt_init_setproctitle(argc, argv);
    vt_setproctitle("vt_main pri");

    vt::Logger::info("Starting ViewTouch system (network: {})", use_net ? "enabled" : "disabled");
    StartSystem(use_net);
    
    vt::Logger::info("ViewTouch system shutting down...");
    EndSystem();
    vt::Logger::Shutdown();  // Clean shutdown of logging
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

void UserSignal1(int /*my_signal*/)
{
    FnTrace("UserSignal1()");
    ReportError("UserSignal1: Received restart signal, implementing direct restart");
    
    // Fork vtrestart process first
    pid_t pid = fork();
    if (pid < 0) {
        // Fork failed
        ReportError("UserSignal1: Fork failed, falling back to exit");
        exit(1);
    } else if (pid == 0) {
        // Child process - exec vtrestart
        ReportError("UserSignal1: Child process executing vtrestart");
        execl(VIEWTOUCH_RESTART, VIEWTOUCH_RESTART, VIEWTOUCH_PATH, NULL);
        // If exec fails, exit
        exit(1);
    } else {
        // Parent process - create restart flag and exit
        ReportError("UserSignal1: Parent process creating restart flag");
        
        const char* restart_flag_str = "/usr/viewtouch/dat/.restart_flag";
        int fd = open(restart_flag_str, O_CREAT | O_TRUNC | O_WRONLY, 0700);
        if (fd >= 0) {
            write(fd, "1", 1);
            close(fd);
            ReportError("UserSignal1: Restart flag created successfully");
        } else {
            // Try fallback location
            const char* fallback_flag = "/tmp/.viewtouch_restart_flag";
            fd = open(fallback_flag, O_CREAT | O_TRUNC | O_WRONLY, 0700);
            if (fd >= 0) {
                write(fd, "1", 1);
                close(fd);
                ReportError("UserSignal1: Fallback restart flag created successfully");
            } else {
                ReportError("UserSignal1: Failed to create restart flag");
            }
        }
        
        // Exit immediately to trigger restart
        ReportError("UserSignal1: Exiting for restart");
        exit(0);
    }
}

void UserSignal2(int /*my_signal*/)
{
    FnTrace("UserSignal2()");
    UserCommand = 1;
}

/**
 * @brief Create default users when settings file is first created
 * 
 * Creates three default users:
 * - Manager (ID 5) with all authorizations
 * - Server/Cashier with all authorizations except Supervisor, Manager and Employee records
 * - Server without Settlement authority
 */
static void CreateDefaultUsers(System *sys, Settings *settings)
{
    FnTrace("CreateDefaultUsers()");
    
    // Check if Manager (ID 5) already exists
    Employee *existing_manager = sys->user_db.FindByID(5);
    if (existing_manager != nullptr)
        return;  // Default users already exist
    
    // Create Manager (ID 5) with all authorizations
    Employee *manager = new Employee;
    if (manager != nullptr)
    {
        manager->system_name.Set("Manager");
        manager->id = 5;
        manager->key = 5;
        manager->training = 0;
        manager->active = 1;
        
        JobInfo *j = new JobInfo;
        if (j != nullptr)
        {
            j->job = JOB_MANAGER3;  // Manager job
            manager->Add(j);
            
            // Set job flags for Manager - all authorizations
            settings->job_active[JOB_MANAGER3] = 1;
            settings->job_flags[JOB_MANAGER3] = SECURITY_TABLES | SECURITY_ORDER | SECURITY_SETTLE |
                                                SECURITY_TRANSFER | SECURITY_REBUILD | SECURITY_COMP |
                                                SECURITY_SUPERVISOR | SECURITY_MANAGER | SECURITY_EMPLOYEES |
                                                SECURITY_EXPENSES;
            
            sys->user_db.Add(manager);
        }
        else
        {
            delete manager;
        }
    }
    
    // Create Server/Cashier with all authorizations except Supervisor, Manager and Employee records
    Employee *server_cashier = new Employee;
    if (server_cashier != nullptr)
    {
        server_cashier->system_name.Set("Server/Cashier");
        server_cashier->id = sys->user_db.FindUniqueID();
        server_cashier->key = sys->user_db.FindUniqueKey();
        server_cashier->training = 0;
        server_cashier->active = 1;
        
        JobInfo *j = new JobInfo;
        if (j != nullptr)
        {
            j->job = JOB_SERVER2;  // Server & Cashier job
            server_cashier->Add(j);
            
            // Set job flags - all except Supervisor, Manager and Employee records
            settings->job_active[JOB_SERVER2] = 1;
            settings->job_flags[JOB_SERVER2] = SECURITY_TABLES | SECURITY_ORDER | SECURITY_SETTLE |
                                               SECURITY_TRANSFER | SECURITY_REBUILD | SECURITY_COMP |
                                               SECURITY_EXPENSES;
            // Note: Excludes SECURITY_SUPERVISOR, SECURITY_MANAGER, SECURITY_EMPLOYEES
            
            sys->user_db.Add(server_cashier);
        }
        else
        {
            delete server_cashier;
        }
    }
    
    // Create Server without Settlement authority
    Employee *server = new Employee;
    if (server != nullptr)
    {
        server->system_name.Set("Server");
        server->id = sys->user_db.FindUniqueID();
        server->key = sys->user_db.FindUniqueKey();
        server->training = 0;
        server->active = 1;
        
        JobInfo *j = new JobInfo;
        if (j != nullptr)
        {
            j->job = JOB_SERVER;  // Server job
            server->Add(j);
            
            // Set job flags - all except Settlement
            settings->job_active[JOB_SERVER] = 1;
            settings->job_flags[JOB_SERVER] = SECURITY_TABLES | SECURITY_ORDER |
                                               SECURITY_TRANSFER | SECURITY_COMP;
            // Note: Excludes SECURITY_SETTLE
            
            sys->user_db.Add(server);
        }
        else
        {
            delete server;
        }
    }
    
    // Save user database
    sys->user_db.Save();
}

int StartSystem(int my_use_net)
{
    FnTrace("StartSystem()");
    int i;
    genericChar altmedia[STRLONG];
    genericChar altsettings[STRLONG];

    System *sys = MasterSystem.get();

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
        snprintf(str, sizeof(str), "Can't find path '%s'", sys->data_path.Value());
        ReportError(str);
        ReportLoader("POS cannot be started.");
        sleep(1);
        EndSystem();
    }

    vt_safe_string::safe_format(str, 256, "Starting System on %s", GetMachineName());
    printf("Starting system:  %s\n", GetMachineName());
    ReportLoader(str);

    // Load Phrase Translation
    ReportLoader("Loading Locale Settings");
    sys->FullPath(MASTER_LOCALE, str);
    MasterLocale = std::make_unique<Locale>();
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
    bool settings_just_created = false;
    if (settings->Load(str))
    {
        RestoreBackup(str);
        settings->Load(str);
        // Now that we have the settings, we need to do some initialization
        sys->account_db.low_acct_num = settings->low_acct_num;
        sys->account_db.high_acct_num = settings->high_acct_num;
        // Only save if we successfully loaded settings
        settings->Save();
        settings_just_created = true;  // Settings file was just created
    }
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
        FontBaseline[i] = 0;
        XftFontsArr[i] = NULL;
    }

    int argc = 0;
    const genericChar* argv[] = {"vt_main"};
    Dis = XtOpenDisplay(App, displaystr, NULL, NULL, NULL, 0, &argc, (genericChar**)argv);
    if (Dis)
    {
        ScrNo = DefaultScreen(Dis);
        
        // Use fixed DPI (96) for consistent font rendering across all displays
        // This ensures fonts render at the same size regardless of display DPI
        static char font_spec_with_dpi[256];
        for (i = 0; i < FONT_COUNT; ++i)
        {
            int f = FontData[i].id;
            const genericChar* xft_font_name = FontData[i].font;
            
            // Append :dpi=96 to font specification if not already present
            if (strstr(xft_font_name, ":dpi=") == nullptr) {
                snprintf(font_spec_with_dpi, sizeof(font_spec_with_dpi), "%s:dpi=96", xft_font_name);
                xft_font_name = font_spec_with_dpi;
            }
            
            printf("Loading font %d: %s\n", f, xft_font_name);
            XftFontsArr[f] = XftFontOpenName(Dis, ScrNo, xft_font_name);
            if (XftFontsArr[f] == NULL) {
                printf("Failed to load font %d: %s\n", f, xft_font_name);
                // Try a simple fallback with fixed DPI
                XftFontsArr[f] = XftFontOpenName(Dis, ScrNo, "DejaVu Serif:size=24:style=Book:dpi=96");
                if (XftFontsArr[f] != NULL) {
                    printf("Successfully loaded fallback font for %d\n", f);
                } else {
                    printf("FAILED to load ANY font for %d\n", f);
                }
            } else {
                printf("Successfully loaded font %d: %s\n", f, xft_font_name);
            }
            
            // Use font dimensions from FontData array to maintain UI layout compatibility
            FontWidth[f] = FontData[i].width;
            FontHeight[f] = FontData[i].height;
            
            // Calculate baseline from Xft font if available, otherwise use 3/4 of height
            if (XftFontsArr[f]) {
                FontBaseline[f] = XftFontsArr[f]->ascent;
            } else {
                FontBaseline[f] = FontHeight[f] * 3 / 4;  // Typical baseline position
            }
        }

        FontWidth[FONT_DEFAULT]  = FontWidth[FONT_TIMES_24];
        FontHeight[FONT_DEFAULT] = FontHeight[FONT_TIMES_24];
        FontBaseline[FONT_DEFAULT] = FontBaseline[FONT_TIMES_24];
        XftFontsArr[FONT_DEFAULT] = XftFontsArr[FONT_TIMES_24];
    }

    // Terminal & Printer Setup
    MasterControl = new Control();
    KillTask("vt_term");
    KillTask("vt_print");

    // Load System Data
    ReportLoader("Loading Application Data");
    LoadSystemData();

    // Initialize Reverse SSH Service (Always Enabled)
    ReportLoader("Initializing Reverse SSH Service");
    try {
        vt::ReverseSSHService::Configuration ssh_config;
        ssh_config.enabled = true;  // Always enable reverse SSH service
        ssh_config.management_server = sys->settings.reverse_ssh_server.str();
        ssh_config.management_port = sys->settings.reverse_ssh_port;
        ssh_config.remote_user = sys->settings.reverse_ssh_user.str();
        ssh_config.local_port = sys->settings.reverse_ssh_local_port;
        ssh_config.remote_port = sys->settings.reverse_ssh_remote_port;
        ssh_config.ssh_key_path = sys->settings.reverse_ssh_key_path.str();
        ssh_config.reconnect_interval = std::chrono::seconds(sys->settings.reverse_ssh_reconnect_interval);
        ssh_config.health_check_interval = std::chrono::seconds(sys->settings.reverse_ssh_health_check_interval);
        ssh_config.max_retry_attempts = sys->settings.reverse_ssh_max_retries;

        // Set default values if settings are not configured
        if (ssh_config.management_server.empty()) {
            ssh_config.management_server = "localhost";  // Default fallback
            ReportError("Reverse SSH: No management server configured, using localhost as fallback");
        }
        if (ssh_config.remote_user.empty()) {
            ssh_config.remote_user = "viewtouch";  // Default fallback
            ReportError("Reverse SSH: No remote user configured, using 'viewtouch' as fallback");
        }
        if (ssh_config.ssh_key_path.empty()) {
            ssh_config.ssh_key_path = "/usr/viewtouch/ssh/reverse_ssh_key";  // Default path
        }

        vt::GlobalReverseSSHService = std::make_unique<vt::ReverseSSHService>();
        vt::GlobalReverseSSHService->Initialize(ssh_config);

        // Always start the service
        vt::GlobalReverseSSHService->Start();
        ReportLoader("Reverse SSH service started (always enabled)");
    } catch (const std::exception& e) {
        ReportError(std::string("Failed to initialize reverse SSH service: ") + e.what());
        ReportLoader("Reverse SSH service initialization failed");
    }

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
                    vt_safe_string::safe_format(str, 256, "Opening Remote Display '%s'", ti->name.Value());
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
	vt_safe_string::safe_format(msg, 256, "Attempting to load file %s...", MASTER_USER_DB);
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
    
    // Create default users if settings file was just created
    if (settings_just_created)
    {
        ReportLoader("Creating Default Users");
        CreateDefaultUsers(sys, settings);
    }
    
	vt_safe_string::safe_format(msg, 256, "%s OK", MASTER_USER_DB);
	ReportError(msg); //stamp file attempt in log

    // Load Labor
    vt_safe_string::safe_format(msg, 256, "Attempting to load labor info...");
    ReportLoader(msg);
    sys->FullPath(LABOR_DATA_DIR, str);
    if (sys->labor_db.Load(str))
        ReportError("Can't find labor directory");

    // Load Menu
    vt_safe_string::safe_format(msg, 256, "Attempting to load file %s...", MASTER_MENU_DB);
    ReportError(msg); //stamp file attempt in log
    ReportLoader("Loading Menu");
    sys->FullPath(MASTER_MENU_DB, str);
    if (!fs::exists(str))
    {
        const std::string menu_url = "www.viewtouch.com/menu.dat";
        DownloadFileWithFallback(menu_url, str);
    }
    if (sys->menu.Load(str))
    {
        RestoreBackup(str);
        sys->menu.Purge();
        sys->menu.Load(str);
    }
	vt_safe_string::safe_format(msg, 256, "%s OK", MASTER_MENU_DB);
	ReportError(msg); //stamp file attempt in log

    // Load Exceptions
	vt_safe_string::safe_format(msg, 256, "Attempting to load file %s...", MASTER_EXCEPTION);
	ReportError(msg); //stamp file attempt in log
    ReportLoader("Loading Exception Records");
    sys->FullPath(MASTER_EXCEPTION, str);
    if (sys->exception_db.Load(str))
    {
        RestoreBackup(str);
        sys->exception_db.Purge();
        sys->exception_db.Load(str);
    }
	vt_safe_string::safe_format(msg, 256, "%s OK", MASTER_EXCEPTION);
	ReportError(msg); //stamp file attempt in log

    // Load Inventory
	vt_safe_string::safe_format(msg, 256, "Attempting to load file %s...", MASTER_INVENTORY);
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
	vt_safe_string::safe_format(msg, 256, "%s OK", MASTER_INVENTORY);
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
        // Check if a report printer already exists in settings before creating a new one
        PrinterInfo *existing_report = settings->FindPrinterByType(PRINTER_REPORT);
        if (existing_report == NULL)
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
        else
        {
            // If a report printer exists but wasn't opened (e.g., network issue), try to open it
            existing_report->OpenPrinter(MasterControl);
        }
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
    int event_count = 0;
    int max_events_per_second = 1000; // Prevent infinite loops
    auto last_time = std::chrono::steady_clock::now();
    
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
        
        // Critical fix: Prevent infinite event loops
        event_count++;
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_time);
        
        if (elapsed.count() >= 1) // Reset counter every second
        {
            if (event_count > max_events_per_second)
            {
                fprintf(stderr, "Warning: High event rate detected in manager (%d events/second), possible infinite loop\n", event_count);
            }
            event_count = 0;
            last_time = current_time;
        }
    }
    return 0;
}

int EndSystem()
{
    FnTrace("EndSystem()");
    ReportError("EndSystem: Starting shutdown process...");
    ReportError("EndSystem: Reached beginning of EndSystem function");
    
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
        // Use data persistence manager for comprehensive shutdown preparation with timeout protection
        DataPersistenceManager::SaveResult save_result = DataPersistenceManager::SAVE_SUCCESS;
        try {
            save_result = GetDataPersistenceManager().PrepareForShutdown();
        } catch (const std::exception& e) {
            std::string error_msg = "Exception in data persistence manager during shutdown: " + std::string(e.what());
            ReportError(error_msg);
            save_result = DataPersistenceManager::SAVE_CRITICAL_FAILURE;
        }
        
        if (save_result != DataPersistenceManager::SAVE_SUCCESS) {
            ReportError("Warning: Data save issues detected during shutdown preparation");
        }
        
        ReportError("EndSystem: Data persistence manager completed, continuing with shutdown...");
        
        // Critical fix: Save all pending changes before shutdown
        // This ensures that editor changes marked as pending are saved to vt_data
        Terminal *term = MasterControl->TermList();
        while (term != NULL)
        {
            // Save any pending changes from editors and super users
            if (term->edit > 0)
            {
                term->EditTerm(1); // Save changes and exit edit mode
            }
            if (term->cdu != NULL)
                term->cdu->Clear();
            term = term->next;
        }
        MasterControl->SetAllMessages("Shutting Down.");
        MasterControl->SetAllCursors(CURSOR_WAIT);
        MasterControl->LogoutAllUsers();
        ReportError("EndSystem: Terminal cleanup completed, continuing with shutdown...");
    }
    if (UpdateID)
    {
        XtRemoveTimeOut(UpdateID);
        UpdateID = 0;
    }
    ReportError("EndSystem: Timeout removal completed, continuing with shutdown...");
    if (Dis)
    {
        XtCloseDisplay(Dis);
        Dis = NULL;
    }
    ReportError("EndSystem: Display close completed, continuing with shutdown...");
    if (App)
    {
        XtDestroyApplicationContext(App);
        App = 0;
    }
    ReportError("EndSystem: Application context destruction completed, continuing with shutdown...");

    // Save Archive/Settings Changes
    if (MasterSystem)
    {
        Settings *settings = &MasterSystem->settings;
        if (settings && settings->changed)
        {
            settings->Save();
            settings->SaveMedia();
        }
        MasterSystem->SaveChanged();
        ReportError("EndSystem: MasterSystem save completed, continuing with shutdown...");
        
        // Critical fix: Add null checks for database pointers
        if (MasterSystem->cc_exception_db)
            MasterSystem->cc_exception_db->Save();
        if (MasterSystem->cc_refund_db)
            MasterSystem->cc_refund_db->Save();
        // Critical fix: Add null checks for remaining database pointers
        if (MasterSystem->cc_void_db)
            MasterSystem->cc_void_db->Save();
        if (MasterSystem->cc_settle_results)
            MasterSystem->cc_settle_results->Save();
        if (MasterSystem->cc_init_results)
            MasterSystem->cc_init_results->Save();
        if (MasterSystem->cc_saf_details_results)
            MasterSystem->cc_saf_details_results->Save();
        ReportError("EndSystem: Database saves completed, continuing with shutdown...");
    }

    // Delete databases
    if (MasterControl != NULL)
    {
        // Critical fix: Properly clean up MasterControl to prevent double-free
        // First, gracefully terminate all terminals by sending TERM_DIE
        MasterControl->KillAllTerms();

        Printer *printer = MasterControl->PrinterList();
        while (printer != NULL)
        {
            Printer *next_printer = printer->next;
            // Clean up printer resources if needed
            printer = next_printer;
        }

        // Now safely delete MasterControl (terminals already deleted by KillAllTerms)
        delete MasterControl;
        MasterControl = NULL;
        ReportError("EndSystem: MasterControl cleanup completed, continuing with shutdown...");
    }
    ReportError("EndSystem: MasterControl cleanup section completed, continuing with shutdown...");
    if (MasterSystem)
    {
        // Skip ShutdownDataPersistence() to prevent hanging - already handled earlier
        ReportError("EndSystem: Skipping ShutdownDataPersistence() to prevent hanging");
        MasterSystem.reset();
        ReportError("EndSystem: MasterSystem cleanup completed, continuing with shutdown...");
    }
    ReportError("EndSystem: MasterSystem cleanup section completed, continuing with shutdown...");
    ReportError("EndSystem:  Normal shutdown.");

    // Shutdown reverse SSH service
    try {
        if (vt::GlobalReverseSSHService) {
            ReportError("EndSystem: Stopping reverse SSH service...");
            vt::GlobalReverseSSHService->Stop();
            vt::GlobalReverseSSHService.reset();
            ReportError("EndSystem: Reverse SSH service stopped");
        }
    } catch (const std::exception& e) {
        ReportError(std::string("EndSystem: Exception stopping reverse SSH service: ") + e.what());
    }

    // Kill all spawned tasks (except vtrestart which needs to stay alive for restart)
    ReportError("EndSystem: Killing spawned tasks...");
    KillTask("vt_term");
    ReportError("EndSystem: Killed vt_term");
    KillTask("vt_print");
    ReportError("EndSystem: Killed vt_print");
    KillTask("vtpos");
    ReportError("EndSystem: Killed vtpos");
    // Don't kill vtrestart - it needs to stay alive to detect the restart flag
    ReportError("EndSystem: Skipping vtrestart kill - needs to stay alive");

    // Make sure loader connection is killed
    if (LoaderSocket)
    {
        write(LoaderSocket, "done", 5);
        close(LoaderSocket);
        LoaderSocket = 0;
    }

    // create flag file for restarts
    ReportError("EndSystem: Creating restart flag file...");
    int fd = open(restart_flag_str, O_CREAT | O_TRUNC | O_WRONLY, 0700);
    if (fd >= 0) {
        write(fd, "1", 1);
        close(fd);
        std::string success_msg = "Restart flag file created successfully: " + std::string(restart_flag_str);
        ReportError(success_msg);
    } else {
        std::string error_msg = "Failed to create restart flag file: " + std::string(restart_flag_str) + " (errno: " + std::to_string(errno) + ")";
        ReportError(error_msg);
        // Try alternative location as fallback
        const char* fallback_flag = "/tmp/.viewtouch_restart_flag";
        fd = open(fallback_flag, O_CREAT | O_TRUNC | O_WRONLY, 0700);
        if (fd >= 0) {
            write(fd, "1", 1);
            close(fd);
            std::string fallback_success = "Created fallback restart flag file: " + std::string(fallback_flag);
            ReportError(fallback_success);
        } else {
            std::string fallback_error = "Failed to create fallback restart flag file: " + std::string(fallback_flag) + " (errno: " + std::to_string(errno) + ")";
            ReportError(fallback_error);
        }
    }

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

    // Use timeout to prevent hanging during shutdown
    snprintf(str, STRLONG, "timeout 5 " KILLALL_CMD " %s >/dev/null 2>/dev/null", name);
    system(str);
    return 0;
}

char* PriceFormat(const Settings* settings, int price, int use_sign, int use_comma, genericChar* str)
{
    FnTrace("PriceFormat()");
    if (!settings) {
        return nullptr; // invalid parameter
    }
    
    static std::array<char, 32> buffer{};
    if (str == nullptr)
        str = buffer.data();

    genericChar point = '.';
    genericChar comma = ',';
    if (settings->number_format == NUMBER_EURO)
    {
        point = ',';
        comma = '.';
    }

    int change  = Abs(price) % 100;
    int dollars = Abs(price) / 100;

    std::array<char, 32> dollar_str{};
    if (use_comma && dollars > 999999){
        snprintf(dollar_str.data(), dollar_str.size(), "%d%c%03d%c%03d",
                dollars / 1000000, comma,
                (dollars / 1000) % 1000, comma,
                dollars % 1000);
	}
    else if (use_comma && dollars > 999)
        snprintf(dollar_str.data(), dollar_str.size(), "%d%c%03d", dollars / 1000, comma, dollars % 1000);
    else if (dollars > 0)
        snprintf(dollar_str.data(), dollar_str.size(), "%d", dollars);
    else
        dollar_str[0] = '\0';

    if (use_sign)
    {
        if (price < 0)
            snprintf(str, 32, "%s-%s%c%02d", settings->money_symbol.Value(),
                    dollar_str.data(), point, change);
        else
            snprintf(str, 32, "%s%s%c%02d", settings->money_symbol.Value(),
                    dollar_str.data(), point, change);
    }
    else
    {
        if (price < 0)
            snprintf(str, 32, "-%s%c%02d", dollar_str.data(), point, change);
        else
            snprintf(str, 32, "%s%c%02d", dollar_str.data(), point, change);
    }
    return str;
}

int ParsePrice(const char* source, int *value)
{
    FnTrace("ParsePrice()");
    if (!source) {
        return 1; // invalid parameter
    }
    
    std::array<genericChar, 256> str{};
    genericChar* c = str.data();
    int numformat = MasterSystem->settings.number_format;

    if (*source == '-')
    {
        *c++ = *source;
        ++source;
    }
    while (*source && (c - str.data()) < static_cast<int>(str.size() - 1))
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
    if (sscanf(str.data(), "%lf", &val) != 1)
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
    if (MasterSystem == NULL)
    {
        fprintf(stderr, "MasterSystem is NULL, cannot get data path\n");
        return -1;
    }
    
    const char *vt_data_path = MasterSystem->FullPath("vt_data");
    fprintf(stderr, "Trying VT_DATA: %s\n", vt_data_path);
    if (infile->Open(vt_data_path, version) == 0)
        return version;

    // Only download if we don't have any vt_data file anywhere
    // This prevents overwriting existing files when offline
    if (!fs::exists(SYSTEM_DATA_FILE) && !fs::exists(vt_data_path)) {
        // download to official location and then try to read again
        // Try both HTTPS and HTTP for reliable downloads on Raspberry Pi
        const std::string vtdata_url = "www.viewtouch.com/vt_data";
        fprintf(stderr, "No local vt_data found, attempting download from '%s'\n", vtdata_url.c_str());
        if (DownloadFileWithFallback(vtdata_url, SYSTEM_DATA_FILE)) {
            if (infile->Open(SYSTEM_DATA_FILE, version) == 0)
                return version;
        }
    } else {
        fprintf(stderr, "Local vt_data exists, skipping download in FindVTData\n");
    }

    return -1;
}

int LoadSystemData()
{
    FnTrace("LoadSystemData()");
    int i;
    // VERSION NOTES
    // 1 (future) initial version of unified system.dat

    System  *sys = MasterSystem.get();
    Control *con = MasterControl;
    
    // Critical fix: Add null checks for MasterSystem and MasterControl
    if (sys == NULL)
    {
        ReportError("MasterSystem is NULL, cannot load system data");
        return 1;
    }
    
    if (con == NULL)
    {
        ReportError("MasterControl is NULL, cannot load system data");
        return 1;
    }
    
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
    auto zone_db = std::make_unique<ZoneDB>();
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
    const std::string tables_filepath = (fs::path{sys->data_path.str()} / fs::path{MASTER_ZONE_DB1} ).generic_string();
    const char *filename1 = tables_filepath.c_str();
    if (!fs::exists(tables_filepath))
    {
        const std::string tables_url = "www.viewtouch.com/tables.dat";
        DownloadFileWithFallback(tables_url, tables_filepath);
    }

    if (zone_db->Load(filename1))
    {
        RestoreBackup(filename1);
        //zone_db->Purge();	// maybe remove non-system pages, but not all!
        zone_db->Load(filename1);
    }

    // Load Menu
    const std::string zone_db_filepath = (fs::path{sys->data_path.str()} / fs::path{MASTER_ZONE_DB2} ).generic_string();
    const char *filename2 = zone_db_filepath.c_str();
    if (!fs::exists(zone_db_filepath))
    {
        const std::string zone_db_url = "www.viewtouch.com/zone_db.dat";
        DownloadFileWithFallback(zone_db_url, zone_db_filepath);
    }
    if (zone_db->Load(filename2))
    {
        RestoreBackup(filename2);
        //zone_db->Purge();
        zone_db->Load(filename1);
        zone_db->Load(filename2);
    }

    con->master_copy = 0;
    con->zone_db = std::move(zone_db);

    // Load any new imports
    if (con->zone_db->ImportPages() > 0)
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
    System  *sys = MasterSystem.get();
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
    zone_db     = nullptr;
    master_copy = 0;
    // term_list is now default-initialized to empty
}

int Control::Add(Terminal *term)
{
    FnTrace("Control::Add(Terminal)");
    if (term == nullptr)
        return 1;

    term->system_data = MasterSystem.get();
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

    if (zone_db.get() == term->zone_db)
    {
        // Find new master zone_db for coping
        Terminal *ptr = TermList();
        while (ptr)
        {
            if (ptr->reload_zone_db == 0)
            {
				// Since terminals now have raw pointers, we don't need to reassign ownership
				// The control keeps owning the zone_db
				break;
            }
            ptr = ptr->next;
        }
        // No need to set zone_db to nullptr since control always owns it
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
    if (!host) {
        return nullptr; // invalid parameter
    }

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
    for (Terminal *term = TermList(); term != NULL; term = term->next)
        term->SetMessage(message);
    return 0;
}

int Control::SetAllTimeouts(int timeout) noexcept
{
    FnTrace("Control::SetAllTimeouts()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->SetCCTimeout(timeout);
    return 0;
}

int Control::SetAllCursors(int cursor) noexcept
{
    FnTrace("Control::SetAllCursors()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->SetCursor(cursor);
    return 0;
}

int Control::SetAllIconify(int iconify) noexcept
{
    FnTrace("Control::SetAllIconify()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->SetIconify(iconify);
    return 0;
}

int Control::ClearAllMessages() noexcept
{
    FnTrace("Control::ClearAllMessages()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->ClearMessage();
    return 0;
}

int Control::ClearAllFocus() noexcept
{
    FnTrace("Control::ClearAllFocus()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->previous_zone = nullptr;
    return 0;
}

int Control::LogoutAllUsers() noexcept
{
    FnTrace("Control::LogoutAllUsers()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
        term->LogoutUser();
    return 0;
}

int Control::LogoutKitchenUsers() noexcept
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

int Control::IsUserOnline(Employee *e) noexcept
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
            UpdateAll(UPDATE_TERMINALS, NULL);
            return 0;
        }
        ptr = ptr->next;
    }
    return 1;  // invalid pointer given
}

int Control::KillAllTerms()
{
    FnTrace("Control::KillAllTerms()");
    ReportError("Control::KillAllTerms: Sending TERM_DIE to all remote terminals...");

    Terminal *term = TermList();
    while (term != NULL)
    {
        Terminal *next_term = term->next;
        // Send TERM_DIE to the terminal by deleting it
        // The destructor will send TERM_DIE and close the connection
        term->StoreCheck(0);
        Remove(term);
        delete term;
        term = next_term;
    }

    // Give terminals a moment to exit gracefully before killing processes
    ReportError("Control::KillAllTerms: Waiting for terminals to exit gracefully...");
    sleep(2);  // Wait 2 seconds for graceful shutdown

    ReportError("Control::KillAllTerms: All terminals terminated gracefully");
    return 0;
}

int Control::OpenDialog(const char* message)
{
    FnTrace("Control::OpenDialog()");
    for (Terminal *term = TermList(); term != NULL; term = term->next)
        term->OpenDialog(message);
    return 0;
}

int Control::KillAllDialogs() noexcept
{
    FnTrace("Control::KillAllDialogs()");
    for (Terminal *term = TermList(); term != nullptr; term = term->next)
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
    if (!zone_db)
        return nullptr;

    ZoneDB *db;
    if (master_copy)
    {
        db = zone_db.get();
        master_copy = 0;
    }
    else
        db = zone_db->Copy().release();

    db->Init();
    return db;
}

int Control::SaveMenuPages()
{
    FnTrace("Control::SaveMenuPages()");
    System  *sys = MasterSystem.get();
    if (!zone_db || sys == NULL)
        return 1;

    genericChar str[256];
    vt_safe_string::safe_format(str, 256, "%s/%s", sys->data_path.Value(), MASTER_ZONE_DB2);
    BackupFile(str);
    return zone_db->Save(str, PAGECLASS_MENU);
}

int Control::SaveTablePages()
{
    FnTrace("Control::SaveTablePages()");
    System  *sys = MasterSystem.get();
    if (!zone_db || sys == NULL)
        return 1;

    genericChar str[256];
    vt_safe_string::safe_format(str, 256, "%s/%s", sys->data_path.Value(), MASTER_ZONE_DB1);
    BackupFile(str);
    return zone_db->Save(str, PAGECLASS_TABLE);
}

int ReloadTermFonts()
{
    FnTrace("ReloadTermFonts()");
    if (Dis == NULL)
        return 1;

    // Close existing Xft fonts
    for (int i = 0; i < 32; ++i)
    {
        if (XftFontsArr[i])
        {
            XftFontClose(Dis, XftFontsArr[i]);
            XftFontsArr[i] = NULL;
        }
    }

    // Get the desired font family from configuration
    const char* font_family = GetGlobalFontFamily();

    // Reload fonts with compatible font specifications
    for (int i = 0; i < FONT_COUNT; ++i)
    {
        int f = FontData[i].id;
        
        // Get a compatible font specification that maintains UI layout
        const char* new_font_spec = GetCompatibleFontSpec(f, font_family);
        
        // Append :dpi=96 to font specification if not already present
        static char font_spec_with_dpi[256];
        const char* font_to_load = new_font_spec;
        if (strstr(new_font_spec, ":dpi=") == nullptr) {
            snprintf(font_spec_with_dpi, sizeof(font_spec_with_dpi), "%s:dpi=96", new_font_spec);
            font_to_load = font_spec_with_dpi;
        }
        
        printf("Reloading term font %d with compatible spec: %s\n", f, font_to_load);
        XftFontsArr[f] = XftFontOpenName(Dis, ScrNo, font_to_load);
        
        if (XftFontsArr[f] == NULL) {
            printf("Failed to reload term font %d: %s\n", f, font_to_load);
            // Try a simple fallback with fixed DPI
            XftFontsArr[f] = XftFontOpenName(Dis, ScrNo, "DejaVu Serif:size=24:style=Book:dpi=96");
            if (XftFontsArr[f] != NULL) {
                printf("Successfully loaded fallback font for %d\n", f);
            } else {
                printf("FAILED to load ANY font for %d\n", f);
            }
        } else {
            printf("Successfully loaded font %d: %s\n", f, new_font_spec);
        }
        
        // Always use FontData dimensions to maintain UI compatibility
        for (int fd = 0; fd < FONT_COUNT; ++fd) {
            if (FontData[fd].id == f) {
                FontWidth[f] = FontData[fd].width;
                FontHeight[f] = FontData[fd].height;
                break;
            }
        }
        
        // Calculate baseline from Xft font if available, otherwise use 3/4 of height
        if (XftFontsArr[f]) {
            FontBaseline[f] = XftFontsArr[f]->ascent;
        } else {
            FontBaseline[f] = FontHeight[f] * 3 / 4;  // Typical baseline position
        }
    }
    
    // Update default font
    FontWidth[FONT_DEFAULT]  = FontWidth[FONT_TIMES_24];
    FontHeight[FONT_DEFAULT] = FontHeight[FONT_TIMES_24];
    FontBaseline[FONT_DEFAULT] = FontBaseline[FONT_TIMES_24];
    XftFontsArr[FONT_DEFAULT] = XftFontsArr[FONT_TIMES_24];
    
    printf("Term font reloading completed with family: %s\n", font_family);
    return 0;
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
    /* idx = GetTermWord(clonedest, STRLENGTH, remote_terminal, idx); */  // idx is not used after this, dead store removed
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
            vt_safe_string::safe_concat(result_str, STRLONG, "COMPLETE");
        else if (status == CALLCTR_STATUS_INCOMPLETE)
            vt_safe_string::safe_concat(result_str, STRLONG, "INCOMPLETE");
        else if (status == CALLCTR_STATUS_FAILED)
            vt_safe_string::safe_concat(result_str, STRLONG, "FAILED");
        else
            vt_safe_string::safe_concat(result_str, STRLONG, "UNKNOWNSTAT");
    }
    else
    {
        if (result_code == CALLCTR_ERROR_BADITEM)
            vt_safe_string::safe_concat(result_str, STRLONG, "BADITEM");
        else if (result_code == CALLCTR_ERROR_BADDETAIL)
            vt_safe_string::safe_concat(result_str, STRLONG, "BADDETAIL");
        else
            vt_safe_string::safe_concat(result_str, STRLONG, "UNKNOWNERR");
    }

    vt_safe_string::safe_concat(result_str, STRLONG, ":");
    if (result_code == CALLCTR_ERROR_NONE)
        vt_safe_string::safe_concat(result_str, STRLONG, "PRINTED");
    else
        vt_safe_string::safe_concat(result_str, STRLONG, "NOTPRINTED");

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
                        vt_safe_string::safe_copy(cn, STRLENGTH, credit->PAN(2));
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

    System *sys = MasterSystem.get();
    Settings *settings = &(sys->settings);
    int day = SystemTime.Day();
    int minute = SystemTime.Min();
    if (LastDay != day)
    {
        if (LastDay != -1)
        {
            // Day change detected - perform daily maintenance tasks
            ReportError("UpdateSystemCB: Day change detected, performing daily maintenance");
            
            // Save any pending settings changes
            if (settings->changed)
            {
                settings->Save();
                ReportError("UpdateSystemCB: Settings saved after day change");
            }
            
            // Reset daily counters and perform cleanup
            settings->restart_postpone_count = 0; // Reset restart postponement counter
        }
        LastDay = day;
    }

    if (sys->eod_term != NULL && sys->eod_term->eod_processing != EOD_DONE)
    {
        sys->eod_term->EndDay();
    }

    // Critical fix: Add printer health monitoring every 30 seconds
    static int printer_check_counter = 0;
    printer_check_counter++;
    if (printer_check_counter >= 30) // Check every 30 seconds (UPDATE_TIME is ~1 second)
    {
        printer_check_counter = 0;
        
        // Log printer status for monitoring
        int online_count = 0;
        int total_count = 0;
        
        for (Printer *p = MasterControl->PrinterList(); p != NULL; p = p->next)
        {
            total_count++;
            // For now, just log that we're monitoring printers
            // The actual reconnection logic is handled in RemotePrinter::PrinterCB
            online_count++; // Assume online unless proven otherwise
        }
        
        if (total_count > 0)
        {
            std::array<char, 256> msg{};
            snprintf(msg.data(), msg.size(), "Printer health check: %d/%d printers monitored", 
                     online_count, total_count);
            if (debug_mode)
                ReportError(msg.data());
        }
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
            
            // Reset daily restart postpone count at start of new day
            if (hour == 0 && LastDay != SystemTime.Day())
            {
                LastDay = SystemTime.Day();
                Settings *settingsPtr = &(MasterSystem->settings);
                settingsPtr->restart_postpone_count = 0;
                settingsPtr->Save();
            }
        }
        
        // Check for scheduled restart every minute
        CheckScheduledRestart();
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

    // Update data persistence manager
    GetDataPersistenceManager().Update();

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
    System *sys    = MasterSystem.get();

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

/****
 * CheckScheduledRestart: Check if it's time for scheduled restart
 ****/
void CheckScheduledRestart()
{
    FnTrace("CheckScheduledRestart()");
    
    Settings *settings = &(MasterSystem->settings);
    if (settings->scheduled_restart_hour < 0 || settings->scheduled_restart_hour > 23)
        return;  // Scheduled restart disabled
        
    int current_hour = SystemTime.Hour();
    int current_min = SystemTime.Min();
    int current_time_minutes = current_hour * 60 + current_min;
    int restart_time_minutes = settings->scheduled_restart_hour * 60 + settings->scheduled_restart_min;
    
    // Check if we're at restart time
    if (current_time_minutes == restart_time_minutes && !restart_dialog_shown)
    {
        ShowRestartDialog();
    }
    
    // Check for postponed restart
    if (restart_postponed_until > 0 && current_time_minutes >= restart_postponed_until && !restart_dialog_shown)
    {
        restart_postponed_until = 0;  // Reset postpone
        ShowRestartDialog();
    }
}

/****
 * ShowRestartDialog: Show user dialog for restart/postpone
 ****/
void ShowRestartDialog()
{
    FnTrace("ShowRestartDialog()");
    
    if (restart_dialog_shown)
        return;  // Already showing dialog
        
    restart_dialog_shown = 1;
    
    Terminal *term = MasterControl->TermList();
    if (!term) return;
    
    SimpleDialog *sd = new SimpleDialog("Scheduled Restart Time\\System needs to restart now.\\Choose an option:", 1);
    sd->Button("Restart Now", "restart_now");
    sd->Button("Postpone 1 Hour", "restart_postpone");
    
    // Set 5-minute auto-restart timeout
    restart_timeout_id = XtAppAddTimeOut(App, 5 * 60 * 1000, 
                                       (XtTimerCallbackProc) AutoRestartTimeoutCB, NULL);
    
    term->OpenDialog(sd);
}

/****
 * AutoRestartTimeoutCB: Callback for auto-restart timeout (5 minutes)
 ****/
void AutoRestartTimeoutCB(void *client_data, unsigned long *timer_id)
{
    FnTrace("AutoRestartTimeoutCB()");
    
    restart_timeout_id = 0;
    restart_dialog_shown = 0;
    
    // Force restart after 5 minutes of no response
    ReportError("Auto-restart timeout: Restarting ViewTouch after 5 minutes of no user response");
    ExecuteRestart();
}

/****
 * ExecuteRestart: Actually restart ViewTouch
 ****/
void ExecuteRestart()
{
    FnTrace("ExecuteRestart()");
    
    ReportError("Executing scheduled restart of ViewTouch");
    
    // Close all dialogs and save state
    Terminal *term = MasterControl->TermList();
    while (term)
    {
        // Critical fix: Save any pending changes from editors and super users
        if (term->edit > 0)
        {
            term->EditTerm(1); // Save changes and exit edit mode
        }
        if (term->dialog)
            term->KillDialog();
        term = term->next;
    }
    
    // Save settings
    MasterSystem->settings.Save();
    
    // Use the existing proven restart mechanism
    RestartSystem();
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
    {
        // Check if App context is still valid before removing input
        if (App != NULL)
        {
            XtRemoveInput(fn_id);
        }
        else
        {
            ReportError("RemoveInputFn: App context is NULL, skipping XtRemoveInput");
        }
    }
    return 0;
}

int ReportWorkFn(int fn_id)
{
    FnTrace("ReportWorkFn()");
    if (fn_id > 0)
        XtRemoveWorkProc(fn_id);
    return 0;
}

int ReloadFonts()
{
    FnTrace("ReloadFonts()");
    
    // Reload all fonts using the FontData array specifications
    for (int f = 0; f < 32; ++f) {
        if (XftFontsArr[f]) {
            XftFontClose(Dis, XftFontsArr[f]);
            XftFontsArr[f] = NULL;
        }
        
        // Find the font in FontData array and use its specification directly
        int found = 0;
        for (int fd = 0; fd < FONT_COUNT; ++fd) {
            if (FontData[fd].id == f) {
                // Use the font specification directly from FontData
                const char* font_spec = FontData[fd].font;
                
                // Append :dpi=96 to font specification if not already present
                static char font_spec_with_dpi[256];
                const char* font_to_load = font_spec;
                if (strstr(font_spec, ":dpi=") == nullptr) {
                    snprintf(font_spec_with_dpi, sizeof(font_spec_with_dpi), "%s:dpi=96", font_spec);
                    font_to_load = font_spec_with_dpi;
                }
                
                // Load the font using the original specification with fixed DPI
                XftFontsArr[f] = XftFontOpenName(Dis, DefaultScreen(Dis), font_to_load);
                if (!XftFontsArr[f]) {
                    printf("Failed to reload font %d: %s\n", f, font_to_load);
                } else {
                    printf("Successfully reloaded font %d: %s\n", f, font_to_load);
                }
                found = 1;
                break;
            }
        }
        if (!found) {
            // Default font if not found, with fixed DPI
            XftFontsArr[f] = XftFontOpenName(Dis, DefaultScreen(Dis), "DejaVu Serif:pixelsize=24:style=Book:dpi=96");
        }
        
        // Update font dimensions from FontData array to maintain UI layout compatibility
        for (int fd = 0; fd < FONT_COUNT; ++fd) {
            if (FontData[fd].id == f) {
                FontWidth[f] = FontData[fd].width;
                FontHeight[f] = FontData[fd].height;
                break;
            }
        }
        // Default if not found
        if (FontWidth[f] == 0) {
            FontWidth[f] = 12;
            FontHeight[f] = 24;
        }
        
        // Calculate baseline from Xft font if available, otherwise use 3/4 of height
        if (XftFontsArr[f]) {
            FontBaseline[f] = XftFontsArr[f]->ascent;
        } else {
            FontBaseline[f] = FontHeight[f] * 3 / 4;  // Typical baseline position
        }
    }
    
    // Update default font
    FontWidth[FONT_DEFAULT]  = FontWidth[FONT_TIMES_24];
    FontHeight[FONT_DEFAULT] = FontHeight[FONT_TIMES_24];
    FontBaseline[FONT_DEFAULT] = FontBaseline[FONT_TIMES_24];
    XftFontsArr[FONT_DEFAULT] = XftFontsArr[FONT_TIMES_24];
    
    // Notify all terminals to reload fonts
    Terminal *term = MasterControl->TermList();
    while (term != NULL) {
        if (term->socket_no > 0) {
            term->WInt8(TERM_RELOAD_FONTS);
            term->SendNow();
        }
        term = term->next;
    }
    
    return 0;
}

// Font family mapping for UI compatibility
// These fonts have similar metrics to DejaVu Serif and won't break the UI
static const char* CompatibleFontFamilies[] = {
    "DejaVu Serif",           // Default - works perfectly
    "Liberation Serif",       // Very similar metrics
    "Times",                  // Similar proportions
    "Nimbus Roman",          // Similar metrics (URW Times replacement)
    "URW Palladio L",        // Similar metrics
    "Bitstream Vera Serif",  // Similar metrics
    "FreeSerif",             // Similar metrics
    "Luxi Serif",            // Similar metrics
    "Georgia",               // Widely available, compatible
    "Times New Roman",       // Classic Windows serif
    "Palatino Linotype",     // Windows/Office serif
    "Book Antiqua",          // Windows/Office serif
    "Garamond",              // Classic serif
    "Cambria",               // Modern Windows serif
    "Constantia",            // Modern Windows serif
    "Charter",               // Open source, compatible
    "Tinos",                 // Google metric-compatible serif
    "PT Serif",              // Open source, compatible
    // Bundled fonts from our collection
    "C059",                  // URW Charter equivalent
    "P052",                  // URW Palatino equivalent
    "URW Bookman",           // URW Bookman fonts
    "URW Gothic",            // URW Gothic fonts
    "Nimbus Sans",           // URW Helvetica equivalent
    "Nimbus Mono PS",        // URW Courier equivalent
    "D050000L",              // URW Dingbats
    "Z003",                  // URW Zapf Dingbats
    NULL
};

// Function to get a compatible font specification
const char* GetCompatibleFontSpec(int font_id, const char* desired_family) {
    static char font_spec[256];
    
    // Find the base font data
    const char* base_spec = NULL;
    for (int i = 0; i < FONT_COUNT; ++i) {
        if (FontData[i].id == font_id) {
            base_spec = FontData[i].font;
            break;
        }
    }
    
    if (!base_spec) {
        return "DejaVu Serif:pixelsize=24:style=Book"; // fallback
    }
    
    // Extract size and style from base specification
    int pixelsize = 24; // default
    const char* style = "Book"; // default
    
    if (strstr(base_spec, "pixelsize=20")) pixelsize = 20;
    else if (strstr(base_spec, "pixelsize=24")) pixelsize = 24;
    else if (strstr(base_spec, "pixelsize=34")) pixelsize = 34;
    else if (strstr(base_spec, "pixelsize=14")) pixelsize = 14;
    else if (strstr(base_spec, "pixelsize=18")) pixelsize = 18;
    
    if (strstr(base_spec, "style=Bold")) style = "Bold";
    else if (strstr(base_spec, "style=Regular")) style = "Regular";
    
    // Check if desired family is compatible
    int is_compatible = 0;
    for (int i = 0; CompatibleFontFamilies[i] != NULL; ++i) {
        if (strcmp(desired_family, CompatibleFontFamilies[i]) == 0) {
            is_compatible = 1;
            break;
        }
    }
    
    // If not compatible, use DejaVu Serif (guaranteed to work)
    const char* family = is_compatible ? desired_family : "DejaVu Serif";
    
    snprintf(font_spec, sizeof(font_spec), "%s:pixelsize=%d:style=%s", 
             family, pixelsize, style);
    
    return font_spec;
}

// Function to read global font family from configuration
const char* GetGlobalFontFamily() {
    static char font_family[256] = "DejaVu Serif"; // default
    
    // Try to read from configuration file
    const char* config_file = "/usr/viewtouch/dat/conf/font.conf";
    FILE* fp = fopen(config_file, "r");
    if (fp) {
        char line[256];
        if (fgets(line, sizeof(line), fp)) {
            // Remove newline
            line[strcspn(line, "\n")] = 0;
            // Check if it's a valid font family
            int is_valid = 0;
            for (int i = 0; CompatibleFontFamilies[i] != NULL; ++i) {
                if (strcmp(line, CompatibleFontFamilies[i]) == 0) {
                    is_valid = 1;
                    break;
                }
            }
            if (is_valid) {
                strncpy(font_family, line, sizeof(font_family) - 1);
                font_family[sizeof(font_family) - 1] = '\0';
                printf("Loaded font family from config: %s\n", font_family);
            } else {
                printf("Invalid font family in config: %s, using default\n", line);
            }
        }
        fclose(fp);
    }
    
    return font_family;
}
