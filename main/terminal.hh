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
 * terminal.hh - revision 190 (10/13/98)
 * POS terminal state class
 */

#ifndef _TERMINAL_HH
#define _TERMINAL_HH

#include "cdu.hh"
#include "credit.hh"
#include "customer.hh"
#include "locale.hh"
#include "utility.hh"

#include <string>
#include <mutex>

// FIX - split Terminal into core class and PosTerm

/**** Definitions ****/
// Button position grid
#define GRID_X  4
#define GRID_Y  4

#define EOD_DONE     0
#define EOD_BEGIN    1
#define EOD_SAF      2
#define EOD_SETTLE   3
#define EOD_FINAL    4
#define EOD_NOSETTLE 4


enum page_id {
/************************************************************* 
 * NOTE: enums always increment.  Initializing this structure to
 * -10 is done for consistancy with original #define
 * statements.  If you need to add elements to this
 * structure, add them at the top and update the initial
 * value
 *************************************************************/ 
	PAGEID_MANAGER = -10,
	PAGEID_ITEM_TARGET,
	PAGEID_BAR_SETTLE,
	PAGEID_LOGOUT,
	PAGEID_GUESTCOUNT2,
	PAGEID_GUESTCOUNT, 
	PAGEID_TABLE2,
	PAGEID_TABLE,
	PAGEID_LOGIN2,
	PAGEID_LOGIN 
};

enum jump_tags {	
	JUMP_NONE, 			// Don't jump
	JUMP_NORMAL, 		// Jump to page, push current page onto stack
	JUMP_STEALTH, 	    // Jump to page (don't put current page on stack)
	JUMP_RETURN,		// Pop page off stack, jump to it
	JUMP_HOME, 			// Jump to employee home page
	JUMP_SCRIPT, 		// Jump to next page in script
	JUMP_INDEX, 		// Jump to current page's index
	JUMP_PASSWORD 	    // Like JUMP_NORMAL but password must be entered
};

// Other
constexpr int PAGE_STACK_SIZE   = 32;
constexpr int SCRIPT_STACK_SIZE = 32;
constexpr int TITLE_HEIGHT      = 32;

// Terminal Types
enum term_types {
	TERMINAL_ORDER_ONLY,    // can order but no settling at this term
	TERMINAL_NORMAL,        // normal operation
	TERMINAL_BAR,           // alternate menu index, pay & settle at once
	TERMINAL_BAR2,          // bar with all local work orders
	TERMINAL_FASTFOOD,      // no table view, pay & settle at once
    TERMINAL_KITCHEN_VIDEO, // display of several checks on one page for cooks
    TERMINAL_KITCHEN_VIDEO2 // secondary check display page (e.g. one for pizza, one for other)
};

// Printer Types
constexpr int PRINTER_DEFAULT         = 0;
constexpr int PRINTER_KITCHEN1        = 1;
constexpr int PRINTER_KITCHEN2        = 2;
constexpr int PRINTER_BAR1            = 3;
constexpr int PRINTER_BAR2            = 4;
constexpr int PRINTER_EXPEDITER       = 5;
constexpr int PRINTER_RECEIPT         = 6;
constexpr int PRINTER_REPORT          = 7;
constexpr int PRINTER_CREDITRECEIPT   = 8;
constexpr int PRINTER_REMOTEORDER     = 9;

constexpr int PRINTER_KITCHEN3        = 12;
constexpr int PRINTER_KITCHEN4        = 13;

// Special printer codes
constexpr int PRINTER_KITCHEN1_NOTIFY = 10;
constexpr int PRINTER_KITCHEN2_NOTIFY = 11;
constexpr int PRINTER_KITCHEN3_NOTIFY = 14;
constexpr int PRINTER_KITCHEN4_NOTIFY = 15;
constexpr int PRINTER_NONE            = 99;

// Update Messages
constexpr int UPDATE_MINUTE       = (1<< 0); // Minute has passed
constexpr int UPDATE_HOUR         = (1<< 1); // Hour has passed
constexpr int UPDATE_TIMEOUT      = (1<< 2); // Too much time has passed with no input
constexpr int UPDATE_BLINK        = (1<< 3); // Time to blink (local term)
constexpr int UPDATE_MEAL_PERIOD  = (1<< 4); // Meal period has changed
constexpr int UPDATE_USERS        = (1<< 5); // User or User list has changed
constexpr int UPDATE_CHECKS       = (1<< 6); // Check list has changed
constexpr int UPDATE_ORDERS       = (1<< 7); // Orders for current check have changed
constexpr int UPDATE_ORDER_SELECT = (1<< 8); // Order selection has changed (local term)
constexpr int UPDATE_PAYMENTS     = (1<< 9); // Payments for current check have changed
constexpr int UPDATE_TABLE        = (1<<10); // Specific table has changed
constexpr int UPDATE_ALL_TABLES   = (1<<11); // Check every table for change
constexpr int UPDATE_MENU         = (1<<12); // Menu Items have changed
constexpr int UPDATE_DRAWER       = (1<<13); // Drawer contents changed
constexpr int UPDATE_SALE         = (1<<14); // New sale
constexpr int UPDATE_QUALIFIER    = (1<<15); // terminal's qualifier state has changed
constexpr int UPDATE_GUESTS       = (1<<16); // guest count entry changed
constexpr int UPDATE_DRAWERS      = (1<<17); // drawer assignment/status has changed
constexpr int UPDATE_ARCHIVE      = (1<<18); // terminal viewed archive changed
constexpr int UPDATE_SETTINGS     = (1<<19); // system setting was changed
constexpr int UPDATE_JOB_FILTER   = (1<<20); // job filter changed (local message only)
constexpr int UPDATE_TERMINALS    = (1<<21); // terminal has been started/killed
constexpr int UPDATE_PRINTERS     = (1<<22); // printer has been started/killed
constexpr int UPDATE_AUTHORIZE    = (1<<23); // credit authorization done
constexpr int UPDATE_SERVER       = (1<<24); // terminal viewed server changed
constexpr int UPDATE_REPORT       = (1<<25); // requested report is done

// Colors
constexpr int COLOR_DEFAULT      = 255; // color determined by zone
constexpr int COLOR_PAGE_DEFAULT = 254; // color determined by page setting
constexpr int COLOR_CLEAR        = 253; // text not rendered
constexpr int COLOR_UNCHANGED    = 252; // don't change value (or treat as default)

enum colors { 
	COLOR_BLACK, COLOR_WHITE, COLOR_RED, COLOR_GREEN,
	COLOR_BLUE, COLOR_YELLOW, COLOR_BROWN, COLOR_ORANGE,
	COLOR_PURPLE, COLOR_TEAL, COLOR_GRAY, COLOR_MAGENTA,
	COLOR_REDORANGE, COLOR_SEAGREEN, COLOR_LT_BLUE, COLOR_DK_RED,
	COLOR_DK_GREEN, COLOR_DK_BLUE, COLOR_DK_TEAL, COLOR_DK_MAGENTA,
	COLOR_DK_SEAGREEN
};

constexpr int SHADOW_DEFAULT = 256;

// Text Alignment
enum text_align {
	ALIGN_LEFT,
	ALIGN_CENTER,
	ALIGN_RIGHT
};

// Shape Types
enum shapes {
	SHAPE_RECTANGLE = 1,
	SHAPE_DIAMOND,
	SHAPE_CIRCLE,
	SHAPE_HEXAGON,
	SHAPE_OCTAGON
};

// Frame Properties
constexpr int FRAME_LIT     =  8;  // alternate palette for frame
constexpr int FRAME_DARK    = 16;  // (use 1)
constexpr int FRAME_INSET   = 32;  // top-bottom, left-right colors switched
constexpr int FRAME_2COLOR  = 64;  // 2 colors used instead of 4

// Fonts
enum font_info {
	FONT_DEFAULT     ,
	FONT_FIXED_14    ,
	FONT_FIXED_20    ,
	FONT_FIXED_24    ,
	FONT_TIMES_20    ,
	FONT_TIMES_24    ,
	FONT_TIMES_34    ,
	FONT_TIMES_20B   ,
	FONT_TIMES_24B   ,
	FONT_TIMES_34B   ,
	FONT_TIMES_14    ,
	FONT_TIMES_14B   ,
	FONT_TIMES_18    ,
	FONT_TIMES_18B   ,
    FONT_COURIER_18  ,
    FONT_COURIER_18B ,
    FONT_COURIER_20  ,
    FONT_COURIER_20B
};

#define FONT_UNDERLINE  128

// Mouse Messages
#define MOUSE_LEFT    1
#define MOUSE_MIDDLE  2
#define MOUSE_RIGHT   4
#define MOUSE_PRESS   8
#define MOUSE_DRAG    16
#define MOUSE_RELEASE 32
#define MOUSE_SHIFT   64

// TimeDate Format Options
#define TD_SHORT_MONTH 1    // Abrv. month names
#define TD_SHORT_DAY   2    // Abrv. day names
#define TD_SHORT_DATE  4    // Date in numeric form (dd/mm/yy)
#define TD_SHORT_TIME  8    // One letter for am/pm (ignored if 24 hour time)
#define TD_NO_DATE     16   // Don't add date
#define TD_NO_TIME     32   // Don't add time of day
#define TD_NO_YEAR     64   // Don't add year to date
#define TD_NO_DAY      128  // Don't add day of week to date
#define TD_PAD         256  // Pad short numbers with spaces
#define TD_SECONDS     512  // Add seconds to time
#define TD_MONTH_ONLY  1024 // Do not display day at all (e.g. "October" or "October, 2002")

// Some combinded format options
#define TD_SHORT_NAMES  (TD_SHORT_MONTH | TD_SHORT_DAY | TD_SHORT_TIME)

// For compatibility with old TimeInfo functions
#define TD0 (TD_SHORT_MONTH | TD_NO_YEAR)
#define TD1 (TD_SHORT_MONTH)
#define TD2 (TD_SHORT_MONTH | TD_NO_YEAR | TD_NO_DAY | TD_SHORT_TIME)
#define TD3 (TD_SHORT_MONTH | TD_SHORT_DAY | TD_PAD | TD_SHORT_TIME)
#define TD4 (TD_SHORT_DATE | TD_NO_DAY | TD_SHORT_TIME | TD_PAD)
#define TD5 (TD_SHORT_DATE | TD_NO_DAY | TD_SHORT_TIME)

#define TD_TIME      (TD_SHORT_TIME  | TD_NO_DATE | TD_NO_DAY)
#define TD_TIMEPAD   (TD_SHORT_TIME  | TD_NO_DATE | TD_NO_DAY | TD_PAD)
#define TD_DATE      (TD_SHORT_DATE  | TD_NO_TIME | TD_NO_DAY)
#define TD_DATEPAD   (TD_SHORT_DATE  | TD_NO_TIME | TD_NO_DAY | TD_PAD)
#define TD_DATETIME  (TD_SHORT_MONTH | TD_NO_YEAR | TD_NO_DAY)
#define TD_MONTH     (TD_NO_TIME     | TD_NO_DAY  | TD_MONTH_ONLY)
#define TD_DATETIMEY (TD_SHORT_MONTH | TD_NO_DAY)

#define TABOPEN_START   0
#define TABOPEN_AMOUNT  1
#define TABOPEN_CARD    2
#define TABOPEN_FINISH  3
#define TABOPEN_CANCEL  4

// Cursor Types
enum cursors_style {
	CURSOR_DEFAULT,
	CURSOR_BLANK,
	CURSOR_POINTER,
	CURSOR_WAIT
};


/**** Types ****/
class Archive;
class ZoneDB;
class Page;
class Zone;
class Employee;
class Check;
class SubCheck;
class Printer;
class Drawer;
class Order;
class Stock;
class Control;
class Printer;
class Locale;
class System;
class CharQueue;
class Settings;
class BatchItem;

class Terminal
{
// private data
private:
    int page_stack[PAGE_STACK_SIZE];     // Page ID stack
    int page_stack_size;                 // size of page_stack
    DList<Terminal> clone_list;
    SList<Str>     term_id_list;  // for CreditCheq Batch Settle
	 
#ifdef VT_TESTING
public:
    // Constructor
    Terminal();
private:
#else
    // Constructor
    Terminal();
#endif
    int CC_TermIDIsDupe(const char* termid);
    int CC_GetTermIDList(Terminal *start_term);

public:
    // General State
    Terminal *next;
    Terminal *fore;           // linked list pointers
    Control  *parent;         // parent Control class
    ZoneDB   *zone_db;        // general & system zone_db
    Page     *page;           // Current page for terminal
    int       org_page_id;    // For Shift-F1, exit edit without save
    Zone     *dialog;         // active dialog zone
    Zone     *next_dialog;
    int       original_type;  // terminal type
    int       type;           // terminal type
    int       sortorder;      // kitchen video sort order; newest to oldest, et al
    Str       printer_host;   // location of receipt printer
    int       printer_port;   // connection type of printer
    int	      print_workorder; // okay to print work order on kitchen/bar printer?
    int	      workorder_heading; // 0=normal, 1=simple kitchen display mode
    int	      tax_inclusive[4];  // settings override, prices include tax?
    CustDispUnit *cdu;        // the CDU object
    TimeInfo  last_input;     // time of last user input
    TimeInfo  time_out;       // time of last user input or timeout
    Zone     *selected_zone;  // zone currently selected (highlighted)
    Zone     *previous_zone;  // zone previously having focus
    Zone     *active_zone;    // active kitchen display report zone
    int       timeout;        // current timeout length in seconds
    Locale   *locale_main;
    Locale   *locale_default;
    Str       host;           // display host of terminal
    Str       name;           // name given to terminal

    int       curr_font_id;   // hack for Aldridge's Kitchen Video
    int       curr_font_width;
    int       mouse_x;        // current mouse position
    int       mouse_y;
    int       allow_blanking;
    int       page_variant;   // 0=Page -1, 1=Page -2

    // POS Data
    Archive      *archive;          // Current archive being viewed
    Check        *check;            // Current check shown on terminal
    CustomerInfo *customer;         // Currently selected customer for takeout orders, et al
    Employee     *server;           // server focus for reports
    Employee     *user;             // Current user on terminal
    Order        *order;            // selected order in order entry window
    Stock        *stock;            // currently viewed inventory stock
    System       *system_data;      // system data for this terminal
    int           password_jump;    // jump id for JUMP_PASSWORD
    int           kitchen;          // which kitchen (for split kitchen printing)
    int           guests;           // current guest count amount being entered
    int           last_index;       // last index page type
    int           job_filter;       // bitfield of jobs to filter
    int           seat;             // Current seat on check
    int           qualifier;        // current qualifer for ordering
    int           drawer_count;     // number of cash drawers at this terminal
    int           password_given;   // boolean - has password already been given?
    int           move_check;       // boolean - is user moving a check?
    Drawer       *expense_drawer;   // drawer selected for expenses
    int           record_activity;  // record mouse clicks and keyboard activity
    int           record_fd;        // the file descriptor for the macro file
    Credit       *credit;
    CCSettle     *settle;           // the current batch settle results
    CCSettle      cc_totals;
    CCSAFDetails  cc_saf_details;

    // These variables retain information between instances of CreditCardDialog.
    // CreditCardDialog will temporarily hand off to CreditCardTipDialog or
    // CreditCardEntryDialog, and needs to be able to recover information
    // afterward.  So these variables allow it to retain state.  Something of a
    // hack, but it works.
    SubCheck     *pending_subcheck;
    int           auth_amount;
    int           void_amount;
    int           auth_action;
    int           auth_swipe;
    const char* auth_message;
    const char* auth_message2;
    Str           auth_voice;
    int           admin_forcing;

    // This is a global flag that signal methods can use to ensure a signal only gets
    // processed once.  In particular, there may be several Check Reports on a Kitchen
    // Video page and the Undo signal wants to be processed by only one button.
    // Each time Terminal sends signals, it resets this variable to 0.  The first zone
    // to process the signal sets it to 1 and no (cooperating) zone will process the
    // signal unless it is 0.
    int           same_signal;

    // Network info
    CharQueue *buffer_in;
    CharQueue *buffer_out;
    int socket_no;
    unsigned long input_id = 0;
    unsigned long redraw_id = 0;
    std::mutex redraw_id_mutex;
    int message_set;
    int last_page_type;
    int last_page_size;

    // Edit / translate mode
    Page *edit_page;
    Zone *edit_zone;
    int   edit;          // 0=normal mode, 1=edit mode, 2=system edit mode
    int   translate;     // boolean - terminal in translate mode?
    int   last_x;
    int   last_y;
    int   zone_modify;
    int   select_on;
    int   select_x1;
    int   select_y1;
    int   select_x2;
    int   select_y2;

    // Language settings
    int   current_language;  // current UI language (LANG_ENGLISH, LANG_FRENCH, etc.)

    // Flags
    int failure;         // communications failure count
    int reload_zone_db;  // boolean - does zone_db file needs updating?
    int show_info;       // boolean - show additional zone info
    int kill_me;         // boolean - delete term during next update?
    int is_server;       // boolean - is the terminal the server display?
    int expand_labor;    // boolean - expand report labor?
    int hide_zeros;      // boolean - hide zero amounts in reports?
    int show_family;     // boolean - show family grouping in reports?
    int expand_goodwill; // boolean - show expanded goodwill adjustments list?

    // Font/Graphics Info
    int size;            // screen size
    int width;           // size of terminal window
    int height;
    int depth;           // pixel depth
    int grid_x;          // grid spacing for zones
    int grid_y;

    Str cc_credit_termid;
    Str cc_debit_termid;
    int cc_processing;   // current status of any settle, saf, etc. transactions
    int eod_processing;  // current status of End Day processing (settle, clear saf)
    int eod_failed;

    short check_balanced;
    short has_payments;
    short is_bar_tab;    // managing tabs, not adding payments.
    int   force_jump;
    int   force_jump_source;

    // Destructor
    ~Terminal();

    // Member Functions
    Terminal *CloneList() { return clone_list.Head(); }
    int RemoveClone(Terminal *remterm) { return clone_list.Remove(remterm); }
    int AddClone(Terminal *cloneterm) { return clone_list.AddToTail(cloneterm); }
    int Initialize();
    int AllowBlanking(int allow = 1);
    int TerminalError(const char* message);         // method for reporting page errors
    int SendTranslations(const char* *name_list);   // send translations for term_dialog
    int ChangePage(Page *p);                  // Changes current page
    int ClearPageStack();                     // clears stack
    int Draw(int update_flag);
    int Draw(int update_flag, int x, int y, int w, int h);
    int Jump(int jump_type, int jump_id = 0); // Standard jump function
    int JumpToIndex(int period);              // Jumps to index for current page
    int NextTablePage();
    int PopPage();                  // pulls page id off stack (0 - empty)
    int PriorTablePage();
    int PushPage(int page_id);      // puts page id on stack
    int RunScript(const char* script, int jump_type, int jump_id);
    int FastStartLogin();
    int OpenTab(int phase = TABOPEN_START, const char* message = NULL);
    int ContinueTab(int serial_number = -1);
    int CloseTab(int serial_number = -1);
    int OpenTabList(const char* message);

	// Message handling
    SignalResult Signal(const char* message, int group_id); // Send message to terminal
    SignalResult Touch(int x, int y);                 // Send touch to terminal
    SignalResult Mouse(int action, int x, int y);     // Send mouse to terminal
    SignalResult Keyboard(int key, int state);        // Send keypress to terminal
    int          SetFocus(Zone *newzone);

    // Macro recording.  We record keystrokes, mouse clicks, and touches for later
    // playback.  This is for debugging, so that I can start ViewTouch, log in,
    // and run a report without having to go through the process manually every
    // time I need to test the report.
    int          OpenRecordFile();
    int          RecordTouch(int x, int y);
    int          RecordKey(int key, int my_code, int state);
    int          RecordMouse(int my_code, int x, int y);
    int          ReadRecordFile();

    int LoginUser(Employee *e, bool home_page=false);
    int LogoutUser(int update = 1); // Logout current user & stores check
    int GetCheck(const char* label, int customer_type); // Gets check (or creates new one)
    int NewTakeOut(int customer_type); // Creates new take out order
    int NewFastFood(int customer_type); // Creates new fastfood order
    int QuickMode(int customer_type); // Creates new fastfood or takeout order
    int SetCheck(Check *c, int update_local = 1); // Makes 'c' current check
    int StoreCheck(int update = 1); // Save check to disk & clears current check
    int NextPage();                 // jump to next page in edit mode
    int ForePage();                 // jump to previous page in edit mode
    int Update(int update_message, const genericChar* value); // Send update to current page
    int OpenDrawer(int pos = 0);    // Send command to printer to open drawer
    int NeedDrawerBalanced(Employee *e);
    int CanSettleCheck();           // boolean - can current user settle?
    Drawer *FindDrawer();           // returns drawer available (if any)
    int StackCheck(int customer_type);
    int OpenDialog(Zone *z);    // opens new dialog
    int OpenDialog(const char* message);
    int NextDialog(Zone *z);    // stores a dialog to pop up after the current dialog closes
    int KillDialog();           // closes open dialog
    int HomePage();             // returns home page #
    int UpdateAllTerms(int update_message, const genericChar* value);
    int UpdateOtherTerms(int update_message, const genericChar* value);
    int TermsInUse();           // # of terms in use (total)
    int OtherTermsInUse(int no_kitchen = 0);     // # of terms in use (except current)
    int EditTerm(int save_data = 1, int mode = 1);  // toggles edit mode for terminal
    int TranslateTerm();        // toggles translate mode for terminal
    int UpdateZoneDB(Control *con); // updates zone_db for terminal
    const genericChar* ReplaceSymbols(const char* str);
    Printer *FindPrinter(int printer_id); // returns printer object based on printer id
    int FrameBorder(int appear, int shape); // returns border width of given appearence type
    int TextureTextColor(int appear); // nice default text color for appearence
    int FontSize(int font_id, int &w, int &h);
    int TextWidth(const char* string, int len = -1, int font_id = -1);
    int IsUserOnline(Employee *e);
    int FinalizeOrders();
    bool CanEditSystem();

    // resolve defaults to page or global setting
    int FontID(int font_id);
    int ColorID(int color);
    int ReloadFonts();  // Reload fonts when global defaults change
    int TextureID(int texture, int state=0);
    int FrameID(int frame, int state=0);

    const genericChar* Translate(const char* string, int lang = LANG_PHRASE, int clear = 0); // calls proper local object for text translation
    const genericChar* TimeDate(const TimeInfo &tm, int format, int lang = LANG_PHRASE);
    const genericChar* TimeDate(char* str, const TimeInfo &tm, int format, int lang = LANG_PHRASE); // returns time and date formated & translated
    const genericChar* PageNo(int current, int max, int lang = LANG_PHRASE); // returns nicely formated & translated page numbers
    const genericChar* UserName(int user_id);
    genericChar* UserName(char* str, int user_id); // returns string with user name
    genericChar* FormatPrice(int price, int sign = 0);
    genericChar* FormatPrice(char* str, int price, int sign = 0); // returns string with format price in default currency & notation
    genericChar* SimpleFormatPrice(int price);
    genericChar* SimpleFormatPrice(char* str, int price); // same as format price but with no commas
    int          PriceToInteger(const char* price);

    // Language management
    int SetLanguage(int lang);
    int GetLanguage() { return current_language; }
    int OpenLanguageDialog();

    int UserInput();
    int ClearSelectedZone();
    int DrawTitleBar();
    int RenderBlankPage();
    int RenderBackground();
    int RenderText(const std::string& str, int x, int y, int color, int font,
                   int align = ALIGN_LEFT, int max_pixel_width = 0, int mode = 0);
    int RenderTextLen(const char* str, int len, int x, int y, int color, int font,
                      int align = ALIGN_LEFT, int mode = 0, int max_pixel_width = 0);
    int RenderZoneText(const char* str, int x, int y, int w, int h,
                       int color, int font);
    int RenderHLine(int x, int y, int len, int color, int lw = 1);
    int RenderVLine(int x, int y, int len, int color, int lw = 1);
    int RenderRectangle(int x, int y, int w, int h, int image);
    int RenderFrame(int x, int y, int w, int h, int thickness, int flags);
    int RenderFilledFrame(int x, int y, int w, int h, int thick, int texture, int flags = 0);
    int RenderStatusBar(Zone *z, int bar_color, const genericChar* text, int text_color);
    int RenderZone(Zone *z);
    int RedrawZone(Zone *z, int time);
    int RenderEditCursor(int x, int y, int w, int h);
    int RenderButton(int x, int y, int w, int h,
                     int frame, int texture, int shape = SHAPE_RECTANGLE);
    int RenderShadow(int x, int y, int w, int h, int s, int shape);
    int UpdateAll();
    int UpdateArea(int x, int y, int w, int h);
    int SetClip(int x, int y, int w, int h);
    int SetCursor(int type);
    int Bell();

    int CalibrateTS();
    int SetMessage(const char* string);
    int ClearMessage();
    int SetIconify(int iconify);
    int SetEmbossedText(int embossed);
    int SetTextAntialiasing(int antialiased);
    int SetDropShadow(int drop_shadow);
    int SetShadowOffset(int offset_x, int offset_y);
    int SetShadowBlur(int blur_radius);
    int KeyboardInput(char key, int code, int state);
    int MouseInput(int action, int x, int y);
    int MouseToolbar(int action, int x, int y);
    int SizeToMouse();

    int ButtonCommand(int command);
    int EditZone(Zone *z);
    int ReadZone();
    int KillZone();
    int EditPage(Page *p);
    int ReadPage();
    int KillPage();
    int EditMultiZone(Page *p);
    int ReadMultiZone();
    int EditDefaults();
    int ReadDefaults();
    int TranslateZone(Zone *z);
    int TranslatePage(Page *p);
    int ShowPageList();
    int JumpList(int selected);

    // Data read/write functions
    int   WInt8(int val);
    int   WInt8(int *val);
    int   RInt8(int *val = NULL);
    int   WInt16(int val);
    int   WInt16(int *val);
    int   RInt16(int *val = NULL);
    int   WInt32(int val);
    int   WInt32(int *val);
    int   RInt32(int *val = NULL);
    long  WLong(long val);
    long  WLong(long *val);
    long  RLong(long *val = NULL);
    long long WLLong(long long val);
    long long WLLong(long long *val);
    long long RLLong(long long *val = NULL);
    int   WFlt(Flt val);
    int   WFlt(Flt *val);
    Flt   RFlt(Flt *val = NULL);
    int   WStr(const std::string &s, int len = 0);
    int   WStr(const Str &s);
    int   WStr(const Str *s);
    genericChar* RStr(char* s = NULL);
    genericChar* RStr(Str *s);
    int   Send();
    int   SendNow();

    Settings *GetSettings();

    int   EndDay();
    int   WriteCreditCard(int amount = 0);
    int   ReadCreditCard();
    int   CC_GetApproval();
    int   CC_GetPreApproval();
    int   CC_GetFinalApproval();
    int   CC_GetVoid();
    int   CC_GetVoidCancel();
    int   CC_GetRefund();
    int   CC_GetRefundCancel();
    Terminal *CC_NextTermWithID(Terminal *cc_term);
    int   CC_NextTermID( int* cc_state, char* termid );
    int   CC_NextBatch(int *state, BatchItem **currbatch, long long *batch);
    int   CC_Settle(const char* batch = NULL, int reset = 0);
    int   CC_Init();
    int   CC_Totals(const char* batch = NULL);
    int   CC_Details();
    int   CC_ClearSAF(int reset = 0);
    int   CC_SAFDetails();
    int   CC_GetSettlementResults();
    int   CC_GetInitResults();
    int   CC_GetTotalsResults();
    int   CC_GetDetailsResults();
    int   CC_GetSAFClearedResults();
    int   CC_GetSAFDetails();
    int   SetCCTimeout(int cc_timeout);

    // friend functions
    friend Terminal *NewTerminal(const char* , int, int);
    friend int       CloneTerminal(Terminal *, const char* , const char* );
};


/**** Funtions ****/
int OpenTerminalSocket(const char* hostname, int hardware_type = 0, int isserver = 0,
                       int width = -1, int height = -1);
Terminal *NewTerminal(const char* host_name, int hardware_type = 0, int isserver = 0);
int CloneTerminal(Terminal *term, const char* dest, const char* name);

#endif
