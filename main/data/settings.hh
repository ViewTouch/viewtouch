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
 * settings.hh - revision 64 (10/7/98)
 * General system settings
 */

#ifndef _SETTINGS_HH
#define _SETTINGS_HH

#include "utility.hh"
#include "list_utility.hh"
#include "cdu.hh"
#include "check.hh"
#include "credit.hh"

// NOTE:  WHEN UPDATING SETTINGS DO NOT FORGET that you may also
// need to update archive.hh and archive.cc for settings which
// should be maintained historically.
constexpr int SETTINGS_VERSION = 100;  // READ ABOVE


/**** Definitions & Data ****/
// General
#define MAX_MEALS        12
#define MAX_SHIFTS       12
#define MAX_FAMILIES     64
#define MAX_HEADER_LINES 4
#define MAX_FOOTER_LINES 4
#define MAX_JOBS         20

// Receipt printing options
#define RECEIPT_SEND      1
#define RECEIPT_FINALIZE  2
#define RECEIPT_BOTH      3
#define RECEIPT_NONE      0

// Drawer report options
#define DRAWER_PRINT_PULL    1
#define DRAWER_PRINT_BALANCE 2
#define DRAWER_PRINT_BOTH    3
#define DRAWER_PRINT_NEVER   0

// Cash Drawer options
#define DRAWER_NORMAL     0 // unrestricted access to drawers for employees
#define DRAWER_ASSIGNED   1 // drawers must be assigned to be used
#define DRAWER_SERVER     2 // each server has a drawer to maintain for shift

// Terminal Hardware -- seems to be obsolete, BAK
#define HARDWARE_NONE     0 // No additional hardware for terminal
#define HARDWARE_RECEIPT  1 // Receipt printer (on Xplora parallel)
#define HARDWARE_DRAWER   2 // Receipt printer & cash drawer
#define HARDWARE_2DRAWERS 3 // Receipt printer & 2 cash drawers

// Price Rounding
#define ROUNDING_NONE         0  // no price rounding
#define ROUNDING_DROP_PENNIES 1  // price rounded down to nearest nickel
#define ROUNDING_UP_GRATUITY  2  // price rounded up to nearest nickel

// Meal periods
#define INDEX_ANY         -1  // any period
#define INDEX_GENERAL      0  // All day
#define INDEX_BREAKFAST    1
#define INDEX_BRUNCH       2
#define INDEX_LUNCH        3
#define INDEX_EARLY_DINNER 4
#define INDEX_DINNER       5
#define INDEX_LATE_NIGHT   6
#define INDEX_BAR          7  // Bar menu
#define INDEX_WINE         8  // Wine list
#define INDEX_CAFE         9  // ?
#define INDEX_ROOM         10 // hotel/motel rooms
#define INDEX_RETAIL       11 // general merchandise

// Sales/Labor Periods
#define SP_NONE       0
#define SP_WEEK       1
#define SP_2WEEKS     2
#define SP_4WEEKS     3
#define SP_MONTH      4
#define SP_HALF_MONTH 5
#define SP_DAY        6
#define SP_QUARTER    7
#define SP_YTD        8   // year to date
#define SP_HM_11      9   // half month broken into 11-25 and 26-10

// Pay Period
#define PERIOD_UNDEFINED 0
#define PERIOD_HOUR      1
#define PERIOD_DAY       2
#define PERIOD_WEEK      3
#define PERIOD_2WEEKS    4
#define PERIOD_4WEEKS    5
#define PERIOD_HALFMONTH 6
#define PERIOD_MONTH     7
#define PERIOD_HM_11     8

// Store/Company
#define STORE_OTHER   0  // Other
#define STORE_SUNWEST 1  // Jerry's, Keuken Dutch

// Tender Flags
#define TF_IS_PERCENT      1    // amount calculated by percent
#define TF_NO_REVENUE      2    // sale total is reduce by this payment
#define TF_NO_TAX          4    // this revenue cannot be taxed
#define TF_NO_TIP          8    // revenue not counted against server's tip base
#define TF_COVER_TAX       16   // this payment will pay for tax also (comps)
#define TF_NO_RESTRICTIONS 32   // all items can be covered with this payment
#define TF_MANAGER         64   // only manager can use
#define TF_FINAL           128  // payment is final (money recieved)
#define TF_ROYALTY         256  // for royalty (see System::RoyaltyReport())
#define TF_SUBSTITUTE      512  // substitute price for coupon
#define TF_APPLY_EACH      1024 // applies to each item rather than just once
#define TF_ITEM_SPECIFIC   2048 // coupon is specific to a family or item
#define TF_IS_TAB          4096 // payment is an open bar tab

// Settings Switch Types
#define SWITCH_SEATS             1  // seat based ordering
#define SWITCH_DRAWER_MODE       2  // drawer mode: standard, assigned, server
#define SWITCH_PASSWORDS         3  // use passwords?
#define SWITCH_SALE_CREDIT       4  // who get credit for sales
#define SWITCH_DISCOUNT_ALCOHOL  5  // can alcohol be discounted or comped?
#define SWITCH_CHANGE_FOR_CHECKS 6  // allow change for checks?
#define SWITCH_COMPANY           8  // specific company settings
#define SWITCH_ROUNDING          9  // price rounding method
#define SWITCH_RECEIPT_PRINT     10 // when is receipt automatically printed?
#define SWITCH_EXPAND_LABOR      11 // Breakdown labor in reports
#define SWITCH_HIDE_ZEROS        12 // Hide zero values or not
#define SWITCH_CHANGE_FOR_CREDIT 13 // allow change for credit cards?
#define SWITCH_CHANGE_FOR_GIFT   14 // allow change for gift certificates?
#define SWITCH_DATE_FORMAT       15 // US/Euro date format
#define SWITCH_NUMBER_FORMAT     16 // US/Euro number format
#define SWITCH_LOCALE            17 // current language/conventions
#define SWITCH_MEASUREMENTS      18 // English/Metric selection
#define SWITCH_AUTHORIZE_METHOD  19 // Software used for credit card authorize
#define SWITCH_24HOURS           20 // store open 24 hours a day?
#define SWITCH_CHANGE_FOR_ROOM   21 // allow change for room charges?
#define SWITCH_TIME_FORMAT       22 // 12/24 hour clock
#define SWITCH_ITEM_TARGET       23 // force item target page after orders?
#define SWITCH_SHOW_FAMILY       24 // report family grouping on/off
#define SWITCH_GOODWILL          25 // show expanded goodwill list
#define SWITCH_MONEY_SYMBOL      26 // whether to use $ or ¤ or what
#define SWITCH_SHOW_MODIFIERS    27 // for Item Sales report
#define SWITCH_ALLOW_MULT_COUPON 28 // whether to allow multiple coupons per (sub)check
#define SWITCH_RECEIPT_ALL_MODS  29 // whether to print all modifiers on the receipt
#define SWITCH_DRAWER_PRINT      30 // whether and when to auto print drawer reports
#define SWITCH_BALANCE_AUTO_CPNS 31 // whether to list auto-coupons in drawer balance
#define SWITCH_F3_F4_RECORDING   32 // whether to enable F3/F4 recording/replay feature
#define SWITCH_AUTO_UPDATE_VT_DATA 33 // whether to automatically download vt_data on startup

#define MOD_SEPARATE_NL           1 // New line
#define MOD_SEPARATE_CM           2 // Comma

#define COUPON_APPLY_EACH         1
#define COUPON_APPLY_ONCE         2

#define DATETIME_NONE             1
#define DATETIME_ONCE             2
#define DATETIME_DAILY            3
#define DATETIME_MONTHLY          4

#define WEEKDAY_SUNDAY            1
#define WEEKDAY_MONDAY            2
#define WEEKDAY_TUESDAY           4
#define WEEKDAY_WEDNESDAY         8
#define WEEKDAY_THURSDAY          16
#define WEEKDAY_FRIDAY            32
#define WEEKDAY_SATURDAY          64

#define KV_PRINT_UNMATCHED        0
#define KV_PRINT_MATCHED          1

extern int WeekDays[];

// Measurements Types
#define MEASURE_STANDARD 1
#define MEASURE_METRIC   2

// Number Formats
#define NUMBER_STANDARD  1
#define NUMBER_EURO      2

// Date Formats
#define DATE_MMDDYY      1
#define DATE_DDMMYY      2

// Time Formats
#define TIME_12HOUR      1
#define TIME_24HOUR      2

// Password Settings
#define PW_NONE     0
#define PW_ALL      1
#define PW_MANAGERS 2

#define LOCAL_MEDIA       1
#define GLOBAL_MEDIA      0
#define ALL_MEDIA        -1
#define GLOBAL_MEDIA_ID   50000

#define ACTIVE_MEDIA      1
#define INACTIVE_MEDIA    0

#define SPLIT_CHECK_ITEM  0
#define SPLIT_CHECK_SEAT  1


// Data
extern const genericChar* StoreName[];
extern int   StoreValue[];

extern const genericChar* PayPeriodName[];
extern int   PayPeriodValue[];

extern const genericChar* MealStartName[];
extern int   MealStartValue[];

extern const genericChar* DrawerModeName[];
extern int   DrawerModeValue[];

extern const genericChar* SaleCreditName[];
extern int   SaleCreditValue[];

extern const genericChar* SalesPeriodName[];
extern int   SalesPeriodValue[];

extern const genericChar* ReceiptPrintName[];
extern int   ReceiptPrintValue[];

extern const genericChar* DrawerPrintName[];
extern int   DrawerPrintValue[];

extern const genericChar* RoundingName[];
extern int   RoundingValue[];

extern const genericChar* PrinterName[];
extern int   PrinterValue[];

extern const genericChar* MeasureSystemName[];
extern int   MeasureSystemValue[];
extern const genericChar* DateFormatName[];
extern int   DateFormatValue[];
extern const genericChar* NumberFormatName[];
extern int   NumberFormatValue[];
extern const genericChar* TimeFormatName[];
extern int   TimeFormatValue[];

extern const genericChar* AuthorizeName[];
extern int   AuthorizeValue[];

extern const genericChar* MarkName[];
extern int   MarkValue[];

extern const genericChar* HourName[];


/**** Types ****/
class Report;
class Printer;
class Terminal;
class Control;
class Locale;
class InputDataFile;
class OutputDataFile;

class MoneyInfo
{
public:
    MoneyInfo *next, *fore;
    int id;
    Str name;
    Str short_name;
    Str symbol;
    int rounding;
    int round_amount;	
    Flt exchange;

    // Constructor
    MoneyInfo();
	
    // Member Functions
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
};

class TaxInfo
{
public:
    TaxInfo *next, *fore;
    int id;
    Str name;
    Str short_name;
    int amount;
    int flags;

    // Constructor
    TaxInfo();

    // Member Functions
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
};

class MediaInfo
{
public:
    MediaInfo *next;
    MediaInfo *fore;
    int id;
    Str name;
    int local;

    MediaInfo();
    virtual ~MediaInfo() {}

    virtual MediaInfo *Copy() = 0;
    virtual int        Read(InputDataFile &df, int version) = 0;
    virtual int        Write(OutputDataFile &df, int version) = 0;

    virtual MediaInfo *Next() = 0;
    virtual MediaInfo *Fore() = 0;

    int IsLocal() { return local; }
    int IsGlobal() { return !local; }
};

class DiscountInfo : public MediaInfo
{
public:
    DiscountInfo *next;
    DiscountInfo *fore;
    int amount;
    int flags;
    short active;

    // Constructor
    DiscountInfo();
    ~DiscountInfo() {}

    // Member Functions
    DiscountInfo *Copy();
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);

    DiscountInfo *Next() { return next; }
    DiscountInfo *Fore() { return fore; }
};

class CompInfo : public MediaInfo
{
public:
    CompInfo *next;
    CompInfo *fore;
    int flags;
    short active;

    // Constructor
    CompInfo();
    ~CompInfo() {}

    // Member Functions
    CompInfo *Copy();
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);

    CompInfo *Next() { return next; }
    CompInfo *Fore() { return fore; }
};

class CouponInfo : public MediaInfo
{
public:
    CouponInfo *next;
    CouponInfo *fore;
    int amount;
    int flags;
    short active;
    int automatic;
    int family;
    int item_id;  // maybe obsolete;  seems non-unique
    Str item_name;  
    TimeInfo start_time;
    TimeInfo end_time;
    TimeInfo start_date;
    TimeInfo end_date;
    int days;
    int months;

    // Constructor
    CouponInfo();
    ~CouponInfo() {}

    // Member Functions
    CouponInfo *Copy();
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);

    CouponInfo *Next() { return next; }
    CouponInfo *Fore() { return fore; }
    int Apply(SubCheck *subcheck, Payment *payment = NULL);
    int Applies(SubCheck *subcheck, int aut = 0);
    int Applies(SalesItem *item, int aut = 0);
    int AppliesTime();
    int AppliesItem(SubCheck *subcheck);
    int AppliesItem(SalesItem *item);
    int Amount(SubCheck *subcheck);
    int CPAmount(SubCheck *subcheck);
    int Amount(int item_cost, int item_count = 1);
    int CPAmount(int item_cost, int item_count = 1);
};

class CreditCardInfo : public MediaInfo
{
public:
    CreditCardInfo *next;
    CreditCardInfo *fore;
    short active;

    // Constructor
    CreditCardInfo();

    // Member Functions
    CreditCardInfo *Copy();
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);

    CreditCardInfo *Next() { return next; }
    CreditCardInfo *Fore() { return fore; }
};

class MealInfo : public MediaInfo
{
public:
    MealInfo *next;
    MealInfo *fore;
    int amount;
    int flags;
    short active;

    // Constructor
    MealInfo();

    // Member Functions
    MealInfo *Copy();
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);

    MediaInfo *Next() { return next; }
    MediaInfo *Fore() { return fore; }
};

class TermInfo
{
public:
    TermInfo *next;
    TermInfo *fore;
    Str name;
    int type;
    int sortorder;
    Str display_host;
    Str printer_host;
    int printer_model;
    int printer_port;
    Str cdu_path;
    int cdu_type;
    int drawers;
    int dpulse;
    int stripe_reader;
    int kitchen;
    int sound_volume;
    int term_hardware;
    int isserver;
    int print_workorder;
    int workorder_heading;	// 0=standard, 1=simple
    Str cc_credit_termid;
    Str cc_debit_termid;
    int page_variant;        // 0=Page -1, 1=Page -2

    // Tax settings override
    //  0=prices don't include tax
    //  1=prices already include tax
    // -1=use global settings
    int tax_inclusive[4];	// food/room/alcohol/merchandise

    // Constructor
    TermInfo();

    // Member Functions
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
    int OpenTerm(Control *db, int update = 0);
    Terminal *FindTerm(Control *db);
    Printer  *FindPrinter(Control *db);
    int IsServer(int set = -1);
};

// alternate names
#define food_inclusive			tax_inclusive[0]
#define room_inclusive			tax_inclusive[1]
#define alcohol_inclusive		tax_inclusive[2]
#define merchandise_inclusive	tax_inclusive[3]

class PrinterInfo
{
public:
    PrinterInfo *next;
    PrinterInfo *fore;
    Str name;
    Str host;
    int type;
    int model;
    int port;
    int kitchen_mode;
    int order_margin;		// blank lines at top of work order

    // Constructor
    PrinterInfo();

    // Member Functions
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
    int OpenPrinter(Control *db, int update = 0);
    Printer *FindPrinter(Control *db);
    const genericChar* Name();
    void DebugPrint(int printall = 0);
};

class Settings
{
    DList<DiscountInfo>   discount_list;
    DList<CouponInfo>     coupon_list;
    DList<CreditCardInfo> creditcard_list;
    DList<CompInfo>       comp_list;
    DList<MealInfo>       meal_list;
    DList<MoneyInfo>      money_list;
    DList<TaxInfo>        tax_list;
    DList<TermInfo>       term_list;
    DList<PrinterInfo>    printer_list;

public:
    // General State
    Str filename;                // filename for saving
    Str discount_filename;       // filename for discounts, coupons, and comps
    Str altdiscount_filename;    // discount, coupons, etc. for old archives
    Str altsettings_filename;    // filename for old tax settings, et al
    int changed;                 // boolean - has a setting been changed?
    Str email_send_server;       // what SMTP server to use for sending email
    Str email_replyto;           // Reply To address for outgoing emails
    int allow_iconify;           // Whether user can iconify window
    int use_embossed_text;       // Whether to use embossed text effects
    int use_text_antialiasing;   // Whether to use text anti-aliasing
    int use_drop_shadows;        // Whether to use drop shadows on text
    int shadow_offset_x;         // Shadow offset in X direction (pixels)
    int shadow_offset_y;         // Shadow offset in Y direction (pixels)
    int shadow_blur_radius;      // Shadow blur radius (0-10)
    int enable_f3_f4_recording;  // Whether to enable F3/F4 recording/replay feature
    
    // Scheduled restart settings
    int scheduled_restart_hour;  // Hour (0-23) for scheduled restart (-1 = disabled)
    int scheduled_restart_min;   // Minute (0-59) for scheduled restart
    int restart_postpone_count;  // Number of times restart has been postponed today
    
    // QuickBooks Export Settings
    Str quickbooks_export_path;   // Path for QuickBooks CSV exports
    int quickbooks_auto_export;   // Enable automatic daily export
    int quickbooks_export_format; // Export format (0=daily, 1=monthly, 2=custom)

    // General Settings
    Str store_name;              // printed on title bar
    Str store_address;           // street
    Str store_address2;          // city, state zip
    int screen_blank_time;       // time in seconds
    int start_page_timeout;      // time to leave user logged in but inactive on login page
    int delay_time1;
    int delay_time2;
    int use_seats;               // boolean - use seat based ordering?
    int password_mode;           // boolean - should passwords be used?
    int min_pw_len;              // min password length
    int sale_credit;             // who gets credit for a sale?
    int drawer_mode;             // drawer handling mode of system
    int require_drawer_balance;  // require users to balance their drawers in ServerBank?
    int day_start;
    int day_end;                 // time in hours (obsolete)
    int shifts_used;             // number of shifts in app
    int shift_start[MAX_SHIFTS]; // time in minutes
    int meal_active[MAX_MEALS];  // boolean - is meal used in app?
    int meal_start[MAX_MEALS];   // period start time in minutes
    int region;                  // region code - location of store
    int store;                   // store code - for company specific features
    int developer_key;           // key code for developer
    int price_rounding;          // cost rounding setting
    int double_mult;
    int double_add;              // how double qualifier effects price
    int combine_accounts;        // boolean - combine tax & revenus accounts?
    int always_open;             // is store 24 hours?
    int use_item_target;         // boolean - use item printer target selection?
    RegionInfo oewindow[4];      // [0]-640, [1]-800, [2]-1024, [3]-1280
    int low_acct_num;            // where account numbers start
    int high_acct_num;           // where account numbers end
    // min_day_length indicates the number of seconds that must elapse before EndDayZone
    // will allow the day to be ended
    int min_day_length;
    int fast_takeouts;           // whether to use FastFood mode for takeouts
    int split_check_view;        // default to item or seat list for SplitCheckZone?
    int allow_multi_coupons;

    // allow_user_select is primarily for ExpenseZone, but may be used elsewhere
    int allow_user_select;       // yes == user selection, no == force term->user

    // drawer floats --> how much cash does the drawer have before any payments come in?
    int drawer_day_start;
    int drawer_night_start;
    int drawer_day_float;
    int drawer_night_float;

    int default_tab_amount;

    int country_code;
    int store_code;
    int drawer_account;  // which account number to use when paying expenses from drawers

    // Tax/Currency Settings (global defaults, terminal can override)
    int last_money_id;
    int last_tax_id;

    // Do prices already include tax? (terminal can override)
    int tax_inclusive[4];	// food/room/alcohol/merchandise

    Flt tax_food;
    Flt tax_alcohol;
    Flt tax_room;
    Flt tax_merchandise;
    Flt tax_GST;
    Flt tax_PST;
    Flt tax_HST;
    Flt tax_QST; // updated for Canada release
    Flt tax_VAT;
    Flt royalty_rate;
    Flt advertise_fund;
    // additional settings (Jan 2006 project)
    // will be added to *.ini files, /not/ to settings.dat; no new file version
    Flt debit_cost; // $ fee per debit transaction
    Flt credit_rate; // % fee per credit transaction
    Flt credit_cost; // $ fee per credit transaction
    Flt line_item_cost; // $ fee (per additional line in bank statement)
    Flt daily_cert_fee; // $ fee assessed per day per franchise location
    
    int tax_takeout_food; // needed for California - no tax on takeout orders
    int personalize_fast_food; // force customer info on all fast checks

    // Locale/Region Settings
    Str language;                // name of default language
    int date_format;
    int number_format;
    int time_format;
    int measure_system;
    Str money_symbol;

    // Media Settings
    int last_discount_id;
    int last_coupon_id;
    int last_creditcard_id;
    int last_comp_id;
    int last_meal_id;
    int change_for_checks;       // boolean - make change for checks?
    int change_for_credit;       // boolean - make change for credit cards?
    int change_for_gift;         // boolean - make change for gift certificates?
    int change_for_roomcharge;   // boolean - make change for room charge?
    int discount_alcohol;        // boolean - allow discounts/comps for alcohol?
    int media_balanced;          // bitfield of tenders that are balanced
    int balance_auto_coupons;

    // Report Settings
    int      sales_period; // defined sales period
    TimeInfo sales_start;
    int      labor_period; // defined labor period
    TimeInfo labor_start;
    int      show_modifiers; // for Item Sales report
    int      default_report_period; // day, week, 2 week, month, six month, or year to date
    int      print_report_header;
    int      report_start_midnight;
    int      kv_print_method;  // whether to print matched or unmatched
    int      kv_show_user;     // whether to put the user data on kitchen video

    // Job/Security/Overtime Settings
    int job_active[MAX_JOBS];
    int job_flags[MAX_JOBS];
    int job_level[MAX_JOBS];
    int wage_week_start;
    int overtime_shift;
    int overtime_week;

    // Hardware settings
    Str cdu_header[MAX_CDU_LINES];        // headers for customer display unit
    Str receipt_header[MAX_HEADER_LINES]; // receipt header text
    Str receipt_footer[MAX_FOOTER_LINES]; // receipt footer text
    int header_flags;
    int footer_flags;
    int receipt_header_length;
    int order_num_style;
    int table_num_style;
    int family_printer[MAX_FAMILIES]; // printer destination for family
    int family_group[MAX_FAMILIES];   // sales group of family
    int receipt_print;                // receipt printing mode
    int receipt_all_modifiers;
    int drawer_print;                 // when to print the drawer balance report
    int split_kitchen;                // boolen - split kitchen printing mode?
    int video_target[MAX_FAMILIES];   // which food families get displayed where?
    int mod_separator;                // how to separate modifiers, with comma, CR, etc.
    int print_timeout;                // how long to keep trying when a printer lockup happens

    // Credit/Debit Card Authorization
    int authorize_method;   // credit/charge card verification software
    int card_types;         // credit/debit/gift, etc.
    int auto_authorize;     // authorize on c card scan, or on command?
    int use_entire_cc_num;  // keep the number until save, or mask immediately?
    int save_entire_cc_num; // save entire cc number or just the last four digits?
    int show_entire_cc_num; // show the entire cc number, or just the last four digits?
    int allow_cc_preauth;   // whether to permit Pre-Auths.
    int allow_cc_cancels;   // whether to allow leaving the settlement page with authed cards
    int merchant_receipt;   // whether to print a merchant receipt as well as customer
    int finalauth_receipt;  // whether to print a pre-auth complete receipt
    int void_receipt;       // whether to print a receipt for voids
    int cash_receipt;       // whether to also print a cash receipt
    int cc_bar_mode;        // affects what amounts are pre-auth completed, possibly other items
    Str cc_merchant_id;
    Str cc_server;          // credit/charge card processing server
    Str cc_port;            // credit/charge card processing port
    Str cc_user;
    Str cc_password;
    int cc_connect_timeout; // how long to wait before timing out connection
    int cc_preauth_add;     // percentage to add to preauth (see Terminal::CC_GetPreApproval())
    Str cc_noconn_message1;
    Str cc_noconn_message2;
    Str cc_noconn_message3;
    Str cc_noconn_message4;
    Str cc_voice_message1;
    Str cc_voice_message2;
    Str cc_voice_message3;
    Str cc_voice_message4;
    int cc_print_custinfo;

    Str visanet_acquirer_bin;
    Str visanet_merchant;
    Str visanet_store;
    Str visanet_terminal;
    int visanet_currency;
    int visanet_country;
    int visanet_city;
    int visanet_language;
    int visanet_timezone;
    int visanet_category;

    // internet update settings
    Str update_address;
    Str update_user;
    Str update_password;
    int auto_update_vt_data;        // boolean - automatically download vt_data on startup?

    Str expire_message1;
    Str expire_message2;
    Str expire_message3;
    Str expire_message4;

    // Constructor
    Settings();

    // Member Functions
    int Load(const genericChar* filename);
    // Loads settings from file
    int Save();
    // Saves settings to file
    int LoadMedia(const genericChar* filename);
    int SaveMedia();
    int SaveAltMedia(const genericChar* filename);
    int SaveAltSettings(const genericChar* filename);
    int MealPeriod(TimeInfo &tm);
    // Returns current meal period
    int FirstShift();
    // Returns first shift number
    int ShiftCount();
    // Returns number of active shifts
    int ShiftPosition(int shift);
    // Returns shift order position
    int ShiftNumber(TimeInfo &tm);
    // Returns current shift number
    int NextShift(int shift);
    // Returns shift following this one
    int ShiftText( char* str, int shift );
    int ShiftStart(TimeInfo &start_time, int shift, TimeInfo &ref);
    // sets start_time to shift start
    int IsGroupActive(int sales_group);

    int FigureFoodTax(int amount, TimeInfo &time, Flt tax = -1);
    int FigureAlcoholTax(int amount, TimeInfo &time, Flt tax = -1);
    int FigureGST(int amount, TimeInfo &time, Flt tax = -1); // Canada
    int FigurePST(int amount, TimeInfo &time, bool isBeverage, Flt tax = -1); //Canada
    int FigureHST(int amount, TimeInfo &time, Flt tax = -1); // Canada
    int FigureQST(int amount, int gst, TimeInfo &time, bool isBeverage, Flt tax = -1); //Canada
    int FigureVAT(int amount, TimeInfo &time, Flt tax = -1);  // Euro
    int FigureRoomTax(int amount, TimeInfo &time, Flt tax = -1);
    int FigureMerchandiseTax(int amount, TimeInfo &time, Flt tax = -1);
    // returns tax on specified amount (at specified time)

    char* TenderName( int tender_type, int tender_id, genericChar* str );
    // returns text name of tender
    int LaborPeriod(TimeInfo &ref, TimeInfo &start, TimeInfo &end);
    int SetPeriod(TimeInfo &ref, TimeInfo &start, TimeInfo &end, int period_view, TimeInfo *fiscal = NULL);
    // Calculates start & end of periods given reference time
    int OvertimeWeek(const TimeInfo &ref, TimeInfo &start, TimeInfo &end);
    // Calculates wage overtime week for given time
    char* StoreNum( char* dest = 0);

    int MediaFirstID(MediaInfo *mi, int idnum);
    int MediaIsDupe(MediaInfo *mi, int id, int thresh = 0);

    // discount functions
    DiscountInfo *DiscountList() { return discount_list.Head(); }
    DiscountInfo *FindDiscountByRecord(int record);
    DiscountInfo *FindDiscountByID(int id);
    int DiscountCount(int local = ALL_MEDIA, int active = ALL_MEDIA);
    int Add(DiscountInfo *d);
    int Remove(DiscountInfo *d);
    int DiscountReport(Terminal *t, Report *r);

    // coupon functions
    CouponInfo *CouponList() { return coupon_list.Head(); }
    CouponInfo *FindCouponByRecord(int record);
    CouponInfo *FindCouponByID(int id);
    CouponInfo *FindCouponByItem(SalesItem *item, int aut = 0);
    int CouponCount(int local = ALL_MEDIA, int active = ALL_MEDIA);
    int Add(CouponInfo *c);
    int Remove(CouponInfo *c);
    int CouponReport(Terminal *t, Report *r);

    // creditcard functions
    CreditCardInfo *CreditCardList() { return creditcard_list.Head(); }
    CreditCardInfo *FindCreditCardByRecord(int record);
    CreditCardInfo *FindCreditCardByID(int id);
    int CreditCardCount(int local = ALL_MEDIA, int active = ALL_MEDIA);
    int Add(CreditCardInfo *cc);
    int Remove(CreditCardInfo *cc);
    int CreditCardReport(Terminal *t, Report *r);
    int CanDoCredit() { return (card_types & CARD_TYPE_CREDIT) ? 1 : 0; }
    int CanDoDebit() { return (card_types & CARD_TYPE_DEBIT) ? 1 : 0; }
    int CanDoGift() { return (card_types & CARD_TYPE_GIFT) ? 1 : 0; }

    // comp functions
    CompInfo *CompList() { return comp_list.Head(); }
    CompInfo *FindCompByRecord(int record);
    CompInfo *FindCompByID(int id);
    int CompCount(int local = ALL_MEDIA, int active = ALL_MEDIA);
    int Add(CompInfo *cm);
    int Remove(CompInfo *cm);
    int CompReport(Terminal *t, Report *r);

    // meal functions
    MealInfo *MealList() { return meal_list.Head(); }
    MealInfo *FindMealByRecord(int record);
    MealInfo *FindMealByID(int id);
    int MealCount(int local = ALL_MEDIA, int active = ALL_MEDIA);
    int Add(MealInfo *mi);
    int Remove(MealInfo *mi);
    int MealReport(Terminal *t, Report *r);

    int RemoveInactiveMedia();

    // money functions
    MoneyInfo *MoneyList() { return money_list.Head(); }
    int MoneyCount()       { return money_list.Count(); }
    int Add(MoneyInfo *my);
    int Remove(MoneyInfo *my);

    // tax functions
    TaxInfo *TaxList() { return tax_list.Head(); }
    int TaxCount()     { return tax_list.Count(); }
    int Add(TaxInfo *tx);
    int Remove(TaxInfo *tx);

    // term functions
    TermInfo *TermList() { return term_list.Head(); }
    TermInfo *FindServer(const genericChar* displaystr = NULL);
    TermInfo *FindTerminal(const char* displaystr);
    TermInfo *FindTermByRecord(int record);
    int TermCount()      { return term_list.Count(); }
    
    int HaveServerTerm();
    int Add(TermInfo *ti);
    int AddFront(TermInfo *ti);
    int Remove(TermInfo *ti);
    int TermReport(Terminal *t, Report *r);

    // printer functions
    PrinterInfo *PrinterList() { return printer_list.Head(); }
    PrinterInfo *FindPrinterByRecord(int record);
    PrinterInfo *FindPrinterByType(int type);
    int PrinterCount()         { return printer_list.Count(); }
    int Add(PrinterInfo *pi);
    int Remove(PrinterInfo *pi);
    int PrinterReport(Terminal *t, Report *r);
    int GetDrawerFloatValue();
};

#endif
