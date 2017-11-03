/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998  *  All Rights Reserved
 * Confidential and Proprietary Information
 *
 * labels.cc - revision 46 (10/6/98)
 * Implementation of labels module
 */

//FIX BAK-->Most of these items should probably be ripped out of here and put into
// the correct files.  For instance, FullZoneTypeName could probably go into zone.cc

#include <string>
#include <utility>

#include "labels.hh"
#include "pos_zone.hh"
#include "terminal.hh"
#include "settings.hh"
#include "sales.hh"
#include "check.hh"
#include "credit.hh"
#include "report.hh"
#include "image_data.hh"
#include "check.hh"
#include "video_zone.hh"
#include "drawer_zone.hh"
#include "cdu.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/**** Global Data ****/
const genericChar* UnknownStr = "Unknown";

const genericChar* FullZoneTypeName[] = {
	"Comment", "Simple",
	"Standard", "Menu Item",
	"Qualifier", "Table",
	"Conditional", "State Toggle",
	"Tender Button", "User Login",
	"User Logout", "Command Window",
	"Guest Count", "Order Entry",
	"Order Flow", "Order Page",
	"Order Add", "Order Delete",
	"Payment Entry", "Check List",
	"User Edit", "Setting Switch",
	"General Settings", "Tax Settings",
    "Receipt Settings", "Editor Settings",
	"Charge Card Settings", "Report", "Scheduler",
	"Split Check", "Drawer",
	"Drawer Assign", "Printing Targets",
	"Hardware", "Time Settings",
	"Table Assign", "Kill System",
	"Search for keyword", "Split Kitchen",
	"Cash Payout", "End Day",
	"Read Zone", "Job Security Settings",
	"Inventory", "Recipes",
	"Vendors", "Sales Item List",
	"Labor", "Tender Settings",
	"Invoice", "Phrase Translation",
	"Item Printer Target", "Merchant Info",
	"Chart Of Accounts", "Video Targets",
    "Expense", "Status", "CDU Messages",
    "Customer Information", "Check Edit",
    "Credit Card List", "Credit Card Messages",
    "Expire Messages", NULL
};

int FullZoneTypeValue[] = {
	ZONE_COMMENT, ZONE_SIMPLE,
	ZONE_STANDARD, ZONE_ITEM,
	ZONE_QUALIFIER, ZONE_TABLE,
	ZONE_CONDITIONAL, ZONE_TOGGLE,
	ZONE_TENDER, ZONE_LOGIN,
	ZONE_LOGOUT, ZONE_COMMAND,
	ZONE_GUEST_COUNT, ZONE_ORDER_ENTRY,
	ZONE_ORDER_FLOW, ZONE_ORDER_PAGE,
	ZONE_ORDER_ADD, ZONE_ORDER_DELETE,
	ZONE_PAYMENT_ENTRY, ZONE_CHECK_LIST,
	ZONE_USER_EDIT, ZONE_SWITCH,
	ZONE_SETTINGS, ZONE_TAX_SETTINGS,
    ZONE_RECEIPTS, ZONE_DEVELOPER,
	ZONE_CC_SETTINGS, ZONE_REPORT, ZONE_SCHEDULE,
	ZONE_SPLIT_CHECK, ZONE_DRAWER_MANAGE,
	ZONE_DRAWER_ASSIGN, ZONE_PRINT_TARGET,
	ZONE_HARDWARE, ZONE_TIME_SETTINGS,
	ZONE_TABLE_ASSIGN, ZONE_KILL_SYSTEM,
	ZONE_SEARCH, ZONE_SPLIT_KITCHEN,
	ZONE_PAYOUT, ZONE_END_DAY,
	ZONE_READ, ZONE_JOB_SECURITY,
	ZONE_INVENTORY, ZONE_RECIPE,
	ZONE_VENDOR, ZONE_ITEM_LIST,
	ZONE_LABOR, ZONE_TENDER_SET,
	ZONE_INVOICE, ZONE_PHRASE,
	ZONE_ITEM_TARGET, ZONE_MERCHANT,
	ZONE_ACCOUNT, ZONE_VIDEO_TARGET,
    ZONE_EXPENSE, ZONE_STATUS_BUTTON, ZONE_CDU,
    ZONE_CUSTOMER_INFO, ZONE_CHECK_EDIT,
    ZONE_CREDITCARD_LIST, ZONE_CC_MSG_SETTINGS,
    ZONE_EXPIRE_MSG, -1
};

const genericChar* ZoneTypeName[] = {
    "Standard", "Menu", "Qualifier", "Table", NULL};
int ZoneTypeValue[] = {
    ZONE_SIMPLE, ZONE_ITEM, ZONE_QUALIFIER, ZONE_TABLE, -1};

const genericChar* ZoneBehaveName[] = {
    "No Response", "Blink", "Toggle", "Turn On", "Double Touch",
    "Touches Pass Through", NULL};
int ZoneBehaveValue[] = {
    BEHAVE_NONE, BEHAVE_BLINK, BEHAVE_TOGGLE,
    BEHAVE_SELECT, BEHAVE_DOUBLE, BEHAVE_MISS, -1};

const genericChar* ZoneFrameName[] = {
    "Default", "Hide Button", "None",
    "Raised Edge", "Inset Edge", "Double Raised Edge",
    "Raised Border", "Clear Border", "Sand Border",
	"Lit Sand Border", "Inset Border", "Parchment Border",
    "Double Border", "Lit Double Border", NULL};
int ZoneFrameValue[] = {
    ZF_DEFAULT, ZF_HIDDEN, ZF_NONE,
    ZF_RAISED, ZF_INSET, ZF_DOUBLE,
    ZF_BORDER, ZF_CLEAR_BORDER, ZF_SAND_BORDER,
	ZF_LIT_SAND_BORDER, ZF_INSET_BORDER, ZF_PARCHMENT_BORDER,
    ZF_DOUBLE_BORDER, ZF_LIT_DOUBLE_BORDER, -1};

const genericChar* TextureName[] = {
    "Default",
	"Transparent",
	"Sand",
	"Lit Sand",
	"Dark Sand",
    "Lite Wood",
	"Medium Wood",
	"Dark Wood",
    "Gray Marble",
	"Green Marble",
	"Gray Parchment",
	"Pearl",
	"Parchment",
	"Canvas",
	"Tan Parchment",
	"Smoke",
	"Leather",
	"Blue Parchment",
        "Gradient",
        "Brown Gradient",
        "Black",
        "Gray Sand",
        "White Mesh",
	NULL
};

int TextureValue[] = {
    IMAGE_DEFAULT,
	IMAGE_CLEAR,
	IMAGE_SAND,
	IMAGE_LIT_SAND,
	IMAGE_DARK_SAND,
    IMAGE_LITE_WOOD,
	IMAGE_WOOD,
	IMAGE_DARK_WOOD,
    IMAGE_GRAY_MARBLE,
	IMAGE_GREEN_MARBLE,
	IMAGE_GRAY_PARCHMENT,
	IMAGE_PEARL,
	IMAGE_PARCHMENT,
	IMAGE_CANVAS,
	IMAGE_TAN_PARCHMENT,
	IMAGE_SMOKE,
	IMAGE_LEATHER,
	IMAGE_BLUE_PARCHMENT,
	IMAGE_GRADIENT,
	IMAGE_GRADIENTBROWN,
	IMAGE_BLACK,
	IMAGE_GREYSAND,
	IMAGE_WHITEMESH,
	-1
};

const genericChar* PageTypeName[] = {
    "Tables", "Tables Showing Guest Check",
    "Index", "Menu Items",
    "Modifiers", "Modifiers A",
	"Modifiers B", "Menu Board",
	"System", "Guest Check List",
    "Kitchen 1", "Kitchen 2",
    "Bar 1", "Bar 2",
    NULL};

int PageTypeValue[] = {
    PAGE_TABLE, PAGE_TABLE2,
	PAGE_INDEX, PAGE_ITEM,
    PAGE_SCRIPTED, PAGE_SCRIPTED2,
	PAGE_SCRIPTED3, PAGE_LIBRARY,
	PAGE_SYSTEM, PAGE_CHECKS,
    PAGE_KITCHEN_VID, PAGE_KITCHEN_VID2,
    PAGE_BAR1, PAGE_BAR2,
    -1};

const genericChar* PageType2Name[] = {
    "Tables", "Tables Showing Guest Check",
    "Index", "Menu Items",
    "Modifiers", "Modifiers A",
	"Modifiers B", "Menu Board", NULL};

int PageType2Value[] = {
    PAGE_TABLE, PAGE_TABLE2,
	PAGE_INDEX, PAGE_ITEM,
    PAGE_SCRIPTED, PAGE_SCRIPTED2,
	PAGE_SCRIPTED3, PAGE_LIBRARY, -1};

const genericChar* JumpTypeName[] = {
    "* No Jump *", "Jump", "Move",
    "Return From A Jump", "Follow A Script", "Return to Index", NULL};
int JumpTypeValue[] = {
    JUMP_NONE, JUMP_NORMAL, JUMP_STEALTH, JUMP_RETURN,
    JUMP_SCRIPT, JUMP_INDEX, -1};

const genericChar* FullJumpTypeName[] = {
    "Remain On This Page", "Jump To A Modifier Page", "Move To A Menu Item Page",
    "Return From A Jump", "Follow The Script",
    "Return to Index", "Return To The Starting Page",
    "Query Password Then Jump", NULL};
int FullJumpTypeValue[] = {
    JUMP_NONE, JUMP_NORMAL, JUMP_STEALTH, JUMP_RETURN, JUMP_SCRIPT,
    JUMP_INDEX, JUMP_HOME, JUMP_PASSWORD, -1};

const genericChar* ShadowName[] = {
    "Default", "No Shadow", "Minimal", "Normal", "Maximum", NULL};
int ShadowValue[] = {
    256, 0, 4, 6, 9, -1};

const genericChar* PageShadowName[] = {
    "No Shadow", "Minimal", "Normal", "Maximum", NULL};
int PageShadowValue[] = {
    0, 4, 6, 9, -1};

const genericChar* PageSizeName[] = {
  "640x480", "768x1024", "800x480", "800x600", "1024x600", "1024x768", "1280x800", "1280x1024", "1366x768", "1440x900", "1600x900", "1600x1200", "1680x1050",
  "1920x1080", "1920x1200", "2560x1440", "2560x1600", NULL};
int PageSizeValue[] = {
  SIZE_640x480, SIZE_768x1024, SIZE_800x480, SIZE_800x600, SIZE_1024x600, SIZE_1024x768, SIZE_1280x800, SIZE_1280x1024, SIZE_1366x768,
  SIZE_1440x900, SIZE_1600x900, SIZE_1600x1200, SIZE_1680x1050, SIZE_1920x1080, SIZE_1920x1200, SIZE_2560x1440, SIZE_2560x1600, -1};

const genericChar* ColorName[] = {
    "Default", "Black", "White", "Red", "Green", "Blue", "Yellow",
    "Brown", "Orange", "Purple", "Teal", "Gray", "Magenta", "Red-Orange",
    "Sea Green", "Light Blue", "Dark Red", "Dark Green",
    "Dark Blue", "Dark Teal", "Dark Magenta", "Dark Sea Green",
    "Transparent", NULL};
int ColorValue[] = {
    COLOR_PAGE_DEFAULT, COLOR_BLACK, COLOR_WHITE, COLOR_RED, COLOR_GREEN,
    COLOR_BLUE, COLOR_YELLOW, COLOR_BROWN, COLOR_ORANGE, COLOR_PURPLE,
    COLOR_TEAL, COLOR_GRAY, COLOR_MAGENTA, COLOR_REDORANGE,
    COLOR_SEAGREEN, COLOR_LT_BLUE, COLOR_DK_RED, COLOR_DK_GREEN,
    COLOR_DK_BLUE, COLOR_DK_TEAL, COLOR_DK_MAGENTA, COLOR_DK_SEAGREEN,
    COLOR_CLEAR, -1};

const genericChar* FontName[] = {
    "Default", "Times 14", "Times 14 Bold", "Times 18", "Times 18 Bold",
    "Times 20", "Times 20 Bold", "Times 24", "Times 24 Bold",
    "Times 34", "Times 34 Bold", "Courier 18", "Courier 18 Bold",
    "Courier 20", "Courier 20 Bold", NULL};
int FontValue[] = {
    FONT_DEFAULT, FONT_TIMES_14, FONT_TIMES_14B, FONT_TIMES_18, FONT_TIMES_18B,
    FONT_TIMES_20, FONT_TIMES_20B, FONT_TIMES_24, FONT_TIMES_24B,
    FONT_TIMES_34, FONT_TIMES_34B, FONT_COURIER_18, FONT_COURIER_18B,
    FONT_COURIER_20, FONT_COURIER_20B, -1};

const genericChar* IndexName[] = {
    "General", "Breakfast", "Brunch", "Lunch",
    "Early Dinner", "Dinner", "Late Night",
    "Bar", "Wine", "Cafe",
    "Room", "Retail", NULL };

int IndexValue[] = {
    INDEX_GENERAL, INDEX_BREAKFAST, INDEX_BRUNCH, INDEX_LUNCH,
    INDEX_EARLY_DINNER, INDEX_DINNER, INDEX_LATE_NIGHT,
    INDEX_BAR, INDEX_WINE, INDEX_CAFE,
    INDEX_ROOM, INDEX_RETAIL, -1 };

const genericChar* ShapeName[] = {
    "Rectangle", "Diamond", "Circle", NULL };

int ShapeValue[] = {
    SHAPE_RECTANGLE, SHAPE_DIAMOND, SHAPE_CIRCLE, -1 };

const genericChar* ItemTypeName[] = {
    "Menu Item", "Modifier", "Non-Tracking Modifier", "Menu Item + Substitute", "Priced By Weight", "Event Admission", NULL };

int ItemTypeValue[] = {
    ITEM_NORMAL, ITEM_MODIFIER, ITEM_METHOD, ITEM_SUBSTITUTE, ITEM_POUND, ITEM_ADMISSION, -1};

const genericChar* FamilyName[] = {
    "A La Carte", "Appetizer", "Breakfast Entree",
    "Burger", "Lunch Entree", "Light Dinner",
    "Dinner Entree", "Soup", "Child Menu",
    "Dessert", "Salad", "Sandwich", "Seafood",
    "Side Order", "Pizza", "Specialty", "Specialty Entree",
    "Banquet", "Bakery",
    "Beverage", "Beer", "Bottled Beer",
    "Wine", "Bottled Wine", "Alcohol", "Cocktail",
    "Malt Beverage", "Modifier", "Reorder",
    "Merchandise", "Room", NULL};

int FamilyValue[] = {
    FAMILY_ALACARTE, FAMILY_APPETIZERS, FAMILY_BREAKFAST_ENTREES,
    FAMILY_BURGERS, FAMILY_LUNCH_ENTREES, FAMILY_LIGHT_DINNER,
    FAMILY_DINNER_ENTREES, FAMILY_SOUP, FAMILY_CHILDRENS_MENU,
    FAMILY_DESSERTS, FAMILY_SALADS, FAMILY_SANDWICHES, FAMILY_SEAFOOD,
    FAMILY_SIDE_ORDERS, FAMILY_PIZZA, FAMILY_SPECIALTY, FAMILY_SPECIALTY_ENTREE,
    FAMILY_BANQUET, FAMILY_BAKERY,
    FAMILY_BEVERAGES, FAMILY_BEER, FAMILY_BOTTLED_BEER,
    FAMILY_WINE, FAMILY_BOTTLED_WINE, FAMILY_RESERVED_WINE, FAMILY_COCKTAIL,
    FAMILY_BOTTLED_COCKTAIL, FAMILY_MODIFIER, FAMILY_REORDER,
    FAMILY_MERCHANDISE, FAMILY_ROOM,
    -1};

const genericChar* SalesTypeName[] = {
    "Food",
    "Food (No Customer Discount)",
    "Food (No Employee Discount)",
    "Food (No Discount)",
    "Food (No Comp or Discount)",
    "Alcohol",
    "Alcohol (No Employee Discount)",
    "Alcohol (No Comp or Discount)",
    "Room",
    "Room (No Comp or Discount)",
    "Merchandise",
    "Merchandise (No Comp or Discount)",
    "Not Taxed (No Comp or Discount)",
    NULL};

int SalesTypeValue[] = {
    SALES_FOOD,
    SALES_FOOD | SALES_NO_DISCOUNT,
    SALES_FOOD | SALES_NO_EMPLOYEE,
    SALES_FOOD | SALES_NO_DISCOUNT | SALES_NO_EMPLOYEE,
    SALES_FOOD | SALES_NO_COMP | SALES_NO_DISCOUNT | SALES_NO_EMPLOYEE,
    SALES_ALCOHOL,
    SALES_ALCOHOL | SALES_NO_EMPLOYEE,
    SALES_ALCOHOL | SALES_NO_COMP | SALES_NO_DISCOUNT | SALES_NO_EMPLOYEE,
    SALES_ROOM,
    SALES_ROOM | SALES_NO_DISCOUNT | SALES_NO_EMPLOYEE | SALES_NO_COMP,
    SALES_MERCHANDISE,
    SALES_MERCHANDISE | SALES_NO_DISCOUNT | SALES_NO_EMPLOYEE | SALES_NO_COMP,
    SALES_UNTAXED | SALES_NO_COMP | SALES_NO_DISCOUNT | SALES_NO_EMPLOYEE,
    -1};

const genericChar* CallOrderName[] = {
    "1st (top)", "2nd", "3rd (middle)", "4th", "5th (bottom)", NULL};
int CallOrderValue[] = {
    0, 1, 2, 3, 4, -1};

const genericChar* QualifierName[] = {
    "No", "Sub", "On Side", "Lite", "Only", "Extra", "Double", "Dry",
    "Plain", "Toast", "UnToast", "Crisp", "Soft", "Hard", "Grill",
    "< Left", "Right >", "Whole", "Cut/2", "Cut/3", "Cut/4", NULL};
const genericChar* QualifierShortName[] = {
    "no", "sub", "side", "lite", "only", "extra", "double", "dry",
    "plain", "toast", "untoast", "crisp", "soft", "hard", "grill",
    "< left", "right >", "whole", "cut/2", "cut/3", "cut/4", NULL};
int QualifierValue[] = {
    QUALIFIER_NO, QUALIFIER_SUB, QUALIFIER_SIDE, QUALIFIER_LITE, QUALIFIER_ONLY, QUALIFIER_EXTRA, QUALIFIER_DOUBLE, QUALIFIER_DRY,
    QUALIFIER_PLAIN, QUALIFIER_TOASTED, QUALIFIER_UNTOASTED, QUALIFIER_CRISPY, QUALIFIER_SOFT, QUALIFIER_HARD, QUALIFIER_GRILLED,
    QUALIFIER_LEFT, QUALIFIER_RIGHT, QUALIFIER_WHOLE, QUALIFIER_CUT2, QUALIFIER_CUT3, QUALIFIER_CUT4, -1};

const genericChar* SwitchName[] = {
    "Seat Based Ordering", "Drawer Mode", "Use Passwords", "Credit For Sale",
    "Allow Discounts For Alcohol", "Make Change For Checks",
    "Make Change For Credit Cards", "Make Change For Gift Certificates",
    "Make Change For Room Charges", "Company", "Sales Price Rounding",
    "Auto Receipt Printing", "Expand Labor", "Suppress Zero Values",
    "Show Family Groupings",
    "Date Format", "Number Format", "Language/Locale",
    "Measurement Standard", "Credit Authorization Method",
    "24 Hour Mode", "Item Targeting",
    "Expand Goodwill Adjustments", "Monetary Symbol",
    "Show Modifiers", "Allow Multiple Coupons",
    "Print All Modifiers on Receipt", "Auto Print Drawer Report",
    "Balance Automatic Coupons", NULL};
int SwitchValue[] = {
    SWITCH_SEATS, SWITCH_DRAWER_MODE, SWITCH_PASSWORDS, SWITCH_SALE_CREDIT,
    SWITCH_DISCOUNT_ALCOHOL, SWITCH_CHANGE_FOR_CHECKS,
    SWITCH_CHANGE_FOR_CREDIT, SWITCH_CHANGE_FOR_GIFT,
    SWITCH_CHANGE_FOR_ROOM, SWITCH_COMPANY, SWITCH_ROUNDING,
    SWITCH_RECEIPT_PRINT, SWITCH_EXPAND_LABOR, SWITCH_HIDE_ZEROS,
    SWITCH_SHOW_FAMILY,
    SWITCH_DATE_FORMAT, SWITCH_NUMBER_FORMAT, SWITCH_LOCALE,
    SWITCH_MEASUREMENTS, SWITCH_AUTHORIZE_METHOD, SWITCH_24HOURS,
    SWITCH_ITEM_TARGET, SWITCH_GOODWILL, SWITCH_MONEY_SYMBOL,
    SWITCH_SHOW_MODIFIERS, SWITCH_ALLOW_MULT_COUPON,
    SWITCH_RECEIPT_ALL_MODS, SWITCH_DRAWER_PRINT,
    SWITCH_BALANCE_AUTO_CPNS, -1};

const genericChar* ReportTypeName[] = {
    "Server", "Closed Check", "Drawer",
    "Check", "Sales", "System Balance", "Deposit/Book Balance",
    "Server Labor", "Item Comp Exception", "Item Void Exception",
    "Table Exception", "Check Rebuild Exception",
    "Customer Detail", "Expenses", "Royalty",
    "Auditing", "CreditCard", NULL};
int ReportTypeValue[] = {
    REPORT_SERVER, REPORT_CLOSEDCHECK, REPORT_DRAWER,
    REPORT_CHECK, REPORT_SALES, REPORT_BALANCE, REPORT_DEPOSIT,
    REPORT_SERVERLABOR, REPORT_COMPEXCEPTION, REPORT_VOIDEXCEPTION,
    REPORT_TABLEEXCEPTION, REPORT_REBUILDEXCEPTION,
    REPORT_CUSTOMERDETAIL, REPORT_EXPENSES, REPORT_ROYALTY,
    REPORT_AUDITING, REPORT_CREDITCARD, -1};

const genericChar* CheckDisplayOrderName[] = {
    "Oldest to Newest", "Newest to Oldest", NULL };
int CheckDisplayOrderValue[] = {
    CHECK_ORDER_OLDNEW, CHECK_ORDER_NEWOLD, -1 };

const genericChar* ReportPrintName[] = {
    "Don't Print On Touch", "Print On Local Printer", "Print On Report Printer",
    "Ask User", NULL};
int ReportPrintValue[] = {
    RP_NO_PRINT, RP_PRINT_LOCAL, RP_PRINT_REPORT, RP_ASK, -1};

const genericChar* TenderName[] = {
    "Cash", "Check", "Gift Certificate", "House Account",
    "Charge Room", "Gratuity", "Credit Card", "Discount",
    "Coupon", "Comp", "Employee Meal", "Tip", NULL};
int TenderValue[] = {
    TENDER_CASH, TENDER_CHECK, TENDER_GIFT, TENDER_ACCOUNT,
    TENDER_CHARGE_ROOM, TENDER_GRATUITY, TENDER_CHARGE_CARD, TENDER_DISCOUNT,
    TENDER_COUPON, TENDER_COMP, TENDER_EMPLOYEE_MEAL, TENDER_CAPTURED_TIP, -1};

const genericChar* PrinterIDName[] = {
    "Default", "Kitchen1", "Kitchen2", "Bar1", "Bar2", "Expediter",
    "Kitchen1 notify2", "Kitchen2 notify1",
    "Remote Order", "None", NULL};
int PrinterIDValue[] = {
    PRINTER_DEFAULT, PRINTER_KITCHEN1, PRINTER_KITCHEN2, PRINTER_BAR1,
    PRINTER_BAR2, PRINTER_EXPEDITER,
    PRINTER_KITCHEN1_NOTIFY, PRINTER_KITCHEN2_NOTIFY,
    PRINTER_REMOTEORDER, PRINTER_NONE, -1};

const genericChar* CustomerTypeName[] = {
    "Restaurant", "Take Out", "Delivery", "Hotel", "Retail", "Fast Food", "For Here", "To Go", "Call In", NULL};
int CustomerTypeValue[] = {
    CHECK_RESTAURANT, CHECK_TAKEOUT, CHECK_DELIVERY, CHECK_HOTEL, CHECK_RETAIL, CHECK_FASTFOOD, CHECK_DINEIN, CHECK_TOGO, CHECK_CALLIN, -1};

const genericChar* PriceTypeName[] = {
    "Price/Item", "Price/Hour", "Price/Day", NULL };
int PriceTypeValue[] = {
    PRICE_PERITEM, PRICE_PERHOUR, PRICE_PERDAY, -1};

const genericChar* DrawerZoneTypeName[] = {
    "Pull/Balance", "Drawer Selecter", NULL };
int   DrawerZoneTypeValue[] = {
    DRAWER_ZONE_BALANCE, DRAWER_ZONE_SELECT, -1 };

const genericChar* CustDispUnitName[] = {
    "None", "Epson", "Ba63", NULL };
int          CustDispUnitValue[] = {
    CDU_TYPE_NONE, CDU_TYPE_EPSON, CDU_TYPE_BA63, -1 };

const genericChar* CCTypeName[] = {
    "VISA", "MasterCard", "American Express",
    "Discover", "Diner's Club", "Debit", NULL };
int          CCTypeValue[] = {
    CREDIT_TYPE_VISA, CREDIT_TYPE_MASTERCARD, CREDIT_TYPE_AMEX,
    CREDIT_TYPE_DISCOVER, CREDIT_TYPE_DINERSCLUB, CREDIT_TYPE_DEBIT, -1 };

const genericChar* ReportPeriodName[] = {
    "Archive", "Day", "Week", "2 Weeks", "Month", "Quarter", "Year to Date", NULL };
int ReportPeriodValue[] = {
    SP_NONE, SP_DAY, SP_WEEK, SP_2WEEKS, SP_MONTH, SP_QUARTER, SP_YTD, -1 };

const char* YesNoName[] = {"Yes", "No", NULL};
int   YesNoValue[] = {1, 0, -1};

const char* NoYesName[] = {"No", "Yes", NULL};
int   NoYesValue[] = {0, 1, -1};

const char* NoYesGlobalName[] = {"No", "Yes", "Global", NULL};
int   NoYesGlobalValue[] = {0, 1, -1, -1};

const char* SplitCheckName[]  = {"Item", "Seat", NULL};
int   SplitCheckValue[] = {SPLIT_CHECK_ITEM, SPLIT_CHECK_SEAT, -1};

const char* ModSeparatorName[] = {"New Line", "Comma", NULL};
int   ModSeparatorValue[] = {MOD_SEPARATE_NL, MOD_SEPARATE_CM, -1};

const char* CouponApplyName[] = {"Once", "Each", NULL};
int   CouponApplyValue[] = {COUPON_APPLY_ONCE, COUPON_APPLY_EACH, -1};

const char* DateTimeName[] = {"Always Available", "Once", "Daily", "Monthly", NULL};
int   DateTimeValue[] = {DATETIME_NONE, DATETIME_ONCE, DATETIME_DAILY,
                         DATETIME_MONTHLY, -1};

const char* PrintModeName[] = {
    "Normal", "Tall", "Wide", "Wide & Tall", NULL};
int PrintModeValue[] = {
    0, PRINT_TALL, PRINT_WIDE, PRINT_TALL | PRINT_WIDE, -1};

const char* KVPrintMethodName[] = {
    "Unmatched", "Matched", NULL};
int KVPrintMethodValue[] = {
    KV_PRINT_UNMATCHED, KV_PRINT_MATCHED, -1};

const char* WOHeadingName[] = {"Standard", "Simple", NULL};
int WOHeadingValue[] = {0, 1, -1};
