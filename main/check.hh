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
 * check.hh - revision 197 (10/8/98)
 * Definition of check management classes
 */

#ifndef _CHECK_HH
#define _CHECK_HH

#include "utility.hh"
#include "list_utility.hh"
#include "terminal.hh"

/**** Module Definitions & Global Data ****/
#define CHECK_VERSION        25

// Check Status
#define CHECK_OPEN           1
#define CHECK_CLOSED         2
#define CHECK_VOIDED         3

// Check Type
#define CHECK_RESTAURANT     1   // normal check - has table location
#define CHECK_TAKEOUT        2   // take out order - has phone number
#define CHECK_BAR            3   // drink from bar - no guest count or table
#define CHECK_MERCHANDISE    4   // merchandise sale - no guest count or table
#define CHECK_DELIVERY       5
#define CHECK_CATERING       6
#define CHECK_HOTEL          7
#define CHECK_RETAIL         8
#define CHECK_FASTFOOD       9
#define CHECK_DINEIN        10
#define CHECK_TOGO          11
#define CHECK_CALLIN        12

// Check Flags
#define CF_PRINTED           1   // Has been sent to the kitcen at least once
#define CF_REPRINT           2   // Has been reprinted (in full) at least once
#define CF_TRAINING          4   // Check is training check (doesn't save)
#define CF_SHOWN             8   // Has been shown and double-clicked on Kitchen Video
#define CF_KITCHEN_MADE      16  // Kitchen has marked their portion as made/ready
#define CF_BAR_MADE          32  // Bar has marked their portion as made/ready
#define CF_KITCHEN_SERVED    64  // Kitchen has marked their portion as served
#define CF_BAR_SERVED        128 // Bar has marked their portion as served

// Tender Types
#define TENDER_CASH          0   // cash
#define TENDER_CHECK         1   // checks
#define TENDER_CHARGE_CARD   2   // charge card (as defined by user)
#define TENDER_COUPON        3   // reduction in price
#define TENDER_GIFT          4   // gift certificate
#define TENDER_COMP          5   // comp of meal
#define TENDER_ACCOUNT       6   // on account
#define TENDER_CHARGE_ROOM   7   // account billed to room
#define TENDER_DISCOUNT      8   // reduction in price
#define TENDER_CAPTURED_TIP  9   // tips taken in
#define TENDER_EMPLOYEE_MEAL 10  // comp of meal
#define TENDER_CREDIT_CARD   11  // standard credit card
#define TENDER_DEBIT_CARD    12  // debit/atm card
#define TENDER_CHARGED_TIP   13  // tip added to credit card
#define TENDER_PAID_TIP      16  // tips paid out of drawer
#define TENDER_OVERAGE       17  // amount overpaid that can't be refunded
#define TENDER_CHANGE        18  // change returned
#define TENDER_PAYOUT        19
#define TENDER_MONEY_LOST    20  // amount lost due to nickel rounding
#define TENDER_GRATUITY      21  // automatic percentage tip
#define TENDER_ITEM_COMP     22  // single item comps
#define TENDER_EXPENSE       23  // expense payouts
#define TENDER_CASH_AVAIL    24  // cash - expense, for balancing only

#define NUMBER_OF_TENDERS    25  // true number + 1 for no known reason
extern int tender_order[];

// Order States
#define ORDER_FINAL  1   // Finalize has been selected for order's check
#define ORDER_SENT   2   // Order has been sent to kitchen
#define ORDER_MADE   4   // Order has been prepaired
#define ORDER_SERVED 8   // Order has been served
#define ORDER_COMP   16  // Order has been comped (no charge)
#define ORDER_SHOWN  32  // Order has been shown and double-clicked on Kitchen Video

#define CHECK_DISPLAY_CASH   128 // Display money Info
#define CHECK_DISPLAY_ALL    255 // Display everything

// Data
extern const genericChar* CheckStatusName[];
extern int   CheckStatusValue[];


/**** Types ****/
class Archive;
class Terminal;
class Check;
class SubCheck;
class Employee;
class SalesItem;
class ItemDB;
class Order;
class Payment;
class Report;
class Printer;
class Drawer;
class Credit;
class Settings;
class System;
class InputDataFile;
class OutputDataFile;
class CustomerInfo;
class ReportZone;

class Order
{
public:
    // General
    Order *next, *fore;   // linked list pointers
    Order *modifier_list; // list of orders modifying this order
    Order *parent;        // used for modifiers

    // Calculated
    Str   script;      // modifier script this order follows
    int   cost;        // total cost of order
    int   total_cost;  // price including modifiers
    int   total_comp;  // amount item is comped for
    int   page_id;     // ID of page where item was ordered from
    Uchar discount;    // boolean - has order been discounted?
    Uchar call_order;  // order priority for modifiers

    // Saved State
    Uchar item_type;   // type of item
    Uchar item_family; // family item belongs to
    Str   item_name;   // name of item
    int   item_cost;   // cost of one item
    int   reduced_cost; // cost of item after item specific coupon reduction
    int   qualifier;   // values of orders qualifier (0 is none)
    int   user_id;     // ID of user who created the order
    int   sales_type;  // item sales type
    short printer_id;  // alternate printer target for order
    short seat;        // seat which order belongs to
    short status;      // orders current status
    short count;       // number of 'item' being ordered
    short checknum;    // for kitchen video, ReportZone::Touch
    short employee_meal; // was this item ordered by an employee at a reduced price?
    short is_reduced;  // 1 = had "each item" coupon applied, 2 = auto coupon
    short allow_increase;	// item allows increase - not really needed?
    short ignore_split;	// ignore split kitchen
    int   auto_coupon_id;

    // Constructors
    Order();
    Order(Settings *settings, SalesItem *si, Terminal *t, int price = -1);
    Order(const genericChar* name, int price);
    // Destructor
    ~Order();

    // Member Functions
    Order     *Copy();  // Returns copy of order (with modifiers)
    int        Read(InputDataFile &df, int version);  // Reads order from a file
    int        Write(OutputDataFile &df, int version);  // Write order to a file
    int        Add(Order *o);  // Add a modifier order
    int        Remove(Order *o);  // Removes a modifier order
    int        FigureCost();  // Totals up order
    genericChar* Description(Terminal *t, genericChar* buffer = NULL);  // Returns string with order description
    genericChar* PrintDescription( genericChar* str=NULL, short int pshort = 0 );  // Returns string with printed order description
    int        IsEntree();  // boolean - is this item an "Entree"?
    int        FindPrinterID(Settings *settings);  // PrinterID (based on family) for order
    SalesItem *Item(ItemDB *db);  // Returns menu item this order points to
    int        PrintStatus(Terminal *t, int printer_id, int reprint = 0, int flag_sent = ORDER_SENT);  // Returns 0-don't print, 1-print, 2-notify only
    genericChar* Seat(Settings *settings, genericChar* buffer = NULL);  // Returns string with order's seat
    int        IsModifier();  // Boolean - Is this order a modifier?
    int        CanDiscount(int discount_alcohol, Payment *p);  // Boolean - Does this discount apply?
    int        Finalize();  // Finalizes order
    int        IsEqual(Order *o); // Boolean - Are both orders the same?
    int        IsEmployeeMeal(int set = -1);
    int        IsReduced(int set = -1);
    int        VideoTarget(Settings *settings); // returns the video target for this order
    int        AddQualifier(const char* qualifier_str);
};

class Payment
{
public:
    Payment *next, *fore; // linked list pointers
    int   user_id;        // user who entered payment
    int   value;          // final cash value of payment
    int   amount;         // cash amount or percent
    int   tender_id;      // defined for Credit, Discount, Coupon, Comp
    short tender_type;    // type of payment
    int   flags;          // tender flags (defined in settings.hh)
    int   drawer_id;      // drawer payment is stored in
    Credit *credit;       // Credit Card/ATM/Debit info

    // Constructors
    Payment();
    Payment(int type, int id, int flags, int amount);
    // Destructor
    ~Payment();

    // Member Functions
    Payment *Copy();  // Returns exact copy of payment object
    int      Read(InputDataFile &df, int version);  // Reads payment from a file
    int      Write(OutputDataFile &df, int version);  // Write payment to a file
    genericChar* Description(Settings *settings, genericChar* str = NULL);  // Returns description of discount
    int      Priority();  // Sorting priority (higher goes 1st)
    int      Suppress();  // Boolean - Should payment be shown?
    int      IsDiscount();  // Boolean - Is payment a discount?
    int      IsEqual(Payment *p);  // Boolean - Are both payments the same?
    int      IsTab();
    int      TabRemain();
    int      FigureTotals(int also_preauth = 0);
    int      SetBatch(const char* termid, const char* batch);
};

class SubCheck
{
    DList<Order>   order_list;
    DList<Payment> payment_list;

public:
    // General
    SubCheck *next, *fore; // linked list pointers
    int       number;      // check number
    Archive  *archive;     // mostly for FigureTotals

    // Saved State
    int      id;           // unique id (obsolete)
    int      status;       // current subcheck status (open, closed, void)
    int      settle_user;  // who settled this subcheck
    TimeInfo settle_time;  // time subcheck was settled
    int      drawer_id;    // drawer check was settled in
    short    check_type;   // duplicated from parent check. Needed for tax-free calculations.

    // Calculated
    int raw_sales;         // all sales unadjusted
    int total_sales;       // all sales (maybe minus discounts)
    int total_tax_alcohol; // total alcohol tax based on total sales
    int total_tax_food;    // total food tax based on total sales
    int total_tax_room;    // total room tax based on total sales
    int total_tax_merchandise;

	// Explicit Fix for Canada implementation
	int total_tax_GST;
	int total_tax_PST;
	int total_tax_HST;
	int total_tax_QST;     
    int new_QST_method;
    int total_tax_VAT;

    int total_cost;        // total customer must pay;
    int item_comps;        // total item comps
    int payment;           // sum of the amount paid
    int balance;           // calculated balance
    int tab_total;
    int delivery_charge;

    // Tax Exempt
    Str tax_exempt;

    // Constructor
    SubCheck();

    // Member Functions
    Order   *OrderList()      { return order_list.Head(); }
    Order   *OrderListEnd()   { return order_list.Tail(); }

    Payment *PaymentList()    { return payment_list.Head(); }
    Payment *PaymentListEnd() { return payment_list.Tail(); }
    int      PaymentCount()   { return payment_list.Count(); }

    int TotalTax()
    {
		int allTaxes = 0;

        allTaxes = (total_tax_alcohol + 
                    total_tax_food + 
                    total_tax_room +
                    total_tax_merchandise +
                    total_tax_GST +
                    total_tax_PST +
                    total_tax_HST + 
                    total_tax_QST +
                    total_tax_VAT);

		return allTaxes;
    }

    SubCheck *Copy(Settings *settings);  // Creates a subcheck copy
    int       Copy(SubCheck *sc, Settings *settings = NULL, int restore = 0);  // Copies the contents of a subcheck
    int       Read(Settings *settings, InputDataFile &df, int version);  // Reads subcheck from file
    int       Write(OutputDataFile &df, int version);  // Writes subcheck to file
    int       Add(Order *o, Settings *settings = NULL);  // Adds an order - recalculates if settings are given
    int       Add(Payment *p, Settings *settings = NULL);  // Adds a payment - recalculates if settings are given
    int       Remove(Order *o, Settings *settings = NULL);  // Removes an order - recalculates if settings are given
    int       Remove(Payment *p, Settings *settings = NULL);  // Removes a payment - recalculates if settings are given
    int       Purge(int restore = 0);  // Removes all payments & orders
    Order    *RemoveOne(Order *o);  // Removes one order & returns it
    Order    *RemoveCount(Order *o, int count = 1);
    int       CancelOrders(Settings *settings);  // Cancels all non-final items
    int       CancelPayments(Terminal *term);  // Cancels all non-final payments
    int       UndoPayments(Terminal *term, Employee *e);  // Payments cleared & check reopened
    int       FigureTotals(Settings *settings);  // Totals costs & payments
    int       TabRemain();
    int       SettleTab(Terminal *term, int payment_type, int payment_id, int payment_flags);
    int       ConsolidateOrders(Settings *settings = NULL, int relaxed = 0);  // Combines like orders - recalculates if settings are given
    int       ConsolidatePayments(Settings *settings = NULL);  // Combines like payments - recalculates if settings are given
    int       FinalizeOrders();  // Makes all ordered items final
    int       Void();  // Voids check
    int       SeatsUsed();  // number of seats with orders
    int       PrintReceipt(Terminal *t, Check *c, Printer *p,
                           Drawer *d = NULL, int open_drawer = 0);  // Prints receipt
    int       ReceiptReport(Terminal *t, Check *c, Drawer *d, Report *r);  // Makes report of receipt
    const genericChar* StatusString(Terminal *t);  // Returns string with subcheck status (Open, Closed, Voided, etc.)
    int       IsSeatOnCheck(int seat);  // Boolean - Are any of the orders for this seat?
    Order    *LastOrder(int seat);  // Returns last order for seat (Could be modifier)
    Order    *LastParentOrder(int seat);  // Returns last order for seat (Could NOT be modifier)
    int       TotalTip();  // Returns amount of captured tip
    void      ClearTips(); // Zeros out captured and charged tips
    int       GrossSales(Check *c, Settings *settings, int sales_group);  // Returns total sales on this subcheck for a sales group
    int       Settle(Terminal *t);
    int       Close(Terminal *t);  // Closes subcheck
    Payment  *FindPayment(int tender_type, int id = -1);  // Returns payment if it exists
    int       TotalPayment(int tender_type, int id = -1);  // Returns total of payment given
    Order    *FindOrder(int number, int seat = -1);  // Returns nth order on subcheck
    int       CompOrder(Settings *settings, Order *o, int comp);  // Marks an order as comped
    int       OrderCount(int seat = -1);  // Returns number of orders for seat
    int       OrderPage(Order *o, int lines_per_page, int seat = -1);  // Returns page order would appear on
    Payment  *NewPayment(int type, int id, int flags, int amount);  // Creates a new order for this subcheck and returns it
    Credit   *CurrentCredit();  // Returns creditcard info for this subcheck (or NULL if there is none)
    int       IsEqual(SubCheck *sc);  // Boolean - Are both subchecks the same?
    int       IsTaxExempt();
    int       IsBalanced();
    int       HasAuthedCreditCards();
    int       HasOpenTab();
    int       OnlyCredit();
    int       SetBatch(const char* termid, const char* batch);
};

class Check
{
    DList<SubCheck> sub_list;

public:
    // Not saved
    Check        *next;
    Check        *fore;          // linked list pointers
    Archive      *archive;       // where does this check belong?
    SubCheck     *current_sub;   // current subcheck being edited
    int           user_current;  // employee currently using check

    // Saved
    int           serial_number;  // unique number for saving
    int           call_center_id; // order number from call center
    TimeInfo      time_open;      // Time Check was created
    TimeInfo      check_in;       // check_in and check_out are for hotel checks
    TimeInfo      check_out;      //    possibly check_in and check_out are deprecated
    TimeInfo      date;           // For Takeout/Delivery/Catering times
    int           user_open;      // employee who opened check
    int           user_owner;     // employee who owns check
    short         flags;          // general check flags
    short         type;           // check type
    Str           filename;       // filename for check (if not in archive)
    int           check_state;    // uses ORDER_yada states, ORDER_MADE, et al.
    TimeInfo      chef_time;      /* time check was sent to kitchen (all closed
                                   *   checks as of 4/15/2002) */
    TimeInfo      made_time;      // time chef bumped check to ORDER_MADE
    int           checknum;       /* matches ReportZone->check_disp_num of zone
                                   * displaying this check */
    short         copy;           /* whether this check is a copy */
    Str           termname;       // originating terminal of check; for Kitchen Video only
    Str           label;
    Str           comment;
    CustomerInfo *customer;
    int           customer_id;
    int           guests;
    int           has_takeouts;
    int           undo;

    // Constructors
    Check();
    Check(Settings *settings, int customer_type, Employee *e = NULL);
    // Destructor
    ~Check();

    // Member Functions
    SubCheck *SubList()    { return sub_list.Head(); }
    SubCheck *SubListEnd() { return sub_list.Tail(); }
    int       SubCount()   { return sub_list.Count(); }

    Check    *Copy(Settings *settings);
    int       Load(Settings *settings, const genericChar* filename); // Loads check from file
    int       Save();  // Saves check to disk
    int       Read(Settings *settings, InputDataFile &df, int version);  // Reads check data from file
    int       ReadFix(InputDataFile &datFile, int version);
    int       Write(OutputDataFile &df, int version);  // Writes check data to file
    int       Add(SubCheck *sc);  // Adds check to check
    int       Remove(SubCheck *sc);  // Removes check from check
    int       Purge();  // Removes & deletes all subchecks from check
    int       Count();  // counts number of checks in list starting with current check
    int       DestroyFile();  // Deletes check's file from disk
    SubCheck *NewSubCheck();  // Adds & returns new subcheck
    int       CancelOrders(Settings *settings);  // Cancels all non final items
    int       SetOrderStatus(SubCheck *subCheck, int set_status);
    int       ClearOrderStatus(SubCheck *subCheck, int clear_status);
    int       FinalizeOrders(Terminal *t, int reprint = 0);  // Marks all items as final & prints work order
    int       Settle(Terminal *t);
    int       Close(Terminal *t);  // Tries to close check (fails if not settled)
    int       Update(Settings *settings);  // updates check & all subchecks
    int       Status();  // returns status of check
    const genericChar* StatusString(Terminal *t);  // Returns status string
    int       MoveOrdersBySeat(SubCheck *sb1, SubCheck *sb2, int seat);
    int       MergeOpenChecks(Settings *settings);
    int       SplitCheckBySeat(Settings *settings);

    int       PrintCount(Terminal *t, int printer_id, int reprint = 0,
                         int flag_sent = ORDER_SENT);  // returns # of orders to be printed
    int       SendWorkOrder(Terminal *term, int printer_target, int reprint);
    int       PrintWorkOrder(Terminal *term, Report *report, int printer_id, int reprint,
                             ReportZone *rzone = NULL, Printer *printer = NULL);
    int       PrintDeliveryOrder(Report *report, int pwidth = 80);
    int       PrintCustomerInfo(Printer *printer, int mode);
    int       PrintCustomerInfoReport(Report *report, int mode, int columns = 1, int pwidth = 40);
    int       ListOrdersForReport(Terminal *term, Report *report);
    int       MakeReport(Terminal *t, Report *r, int show_what = CHECK_DISPLAY_ALL,
                         int video_target = PRINTER_DEFAULT, ReportZone *rzone = NULL);  // makes report showing all subchecks
    int       HasOpenTab();
    int       IsEmpty();  // boolean - is check blank?
    int       IsTraining();  // boolean - is this a training check?
    int       EntreeCount(int seat);  // counts total entrees at seat
    SubCheck *FirstOpenSubCheck(int seat = -1);  // returns 1st open subcheck (by seat if needed) - sets current_sub
    SubCheck *NextOpenSubCheck(SubCheck *sc = NULL);  // returns next open subcheck in check - sets current_sub
    TimeInfo *TimeClosed();  // returns ptr to time closed
    int       WhoGetsSale(Settings *settings);  // returns user_id of server
    int       SecondsOpen();  // total number of seconds open
    int       SeatsUsed();  // seat count across all subchecks
    const genericChar* PaymentSummary(Terminal *t);
    int       Search(const genericChar* word);
    int       SetBatch(const char* termid, const char* batch);
    int       IsBatchSet();

    // CustomerInfo stuff
    int       IsTakeOut();  // FIX - bad name now  // true if there is no room/table for check
    int       IsFastFood();
    int       IsToGo();
    int       IsForHere();
    int       CustomerType(int set = -1);
    const genericChar* Table(const genericChar* set = NULL);  // FIX - name not general enough
    int       Guests(int guests = -1);
    int              CallCenterID(int set = -1);
    int              CustomerID(int set = -1);
    const genericChar* LastName(const genericChar* set = NULL);
    const genericChar* FirstName(const genericChar* set = NULL);
    genericChar* FullName(genericChar* dest = NULL);
    const genericChar* Company(const genericChar* set = NULL);
    const genericChar* Address(const genericChar* set = NULL);
    const genericChar* Address2(const genericChar* set = NULL);
    const genericChar* CrossStreet(const genericChar* set = NULL);
    const genericChar* City(const genericChar* set = NULL);
    const genericChar* State(const genericChar* set = NULL);
    const genericChar* Postal(const genericChar* set = NULL);
    const genericChar* Vehicle(const genericChar* set = NULL);
    const genericChar* CCNumber(const genericChar* set = NULL);
    const genericChar* CCExpire(const genericChar* set = NULL);
    const genericChar* License(const genericChar* set = NULL);
    const genericChar* Comment(const genericChar* set = NULL);
    const genericChar* PhoneNumber(const genericChar* set = NULL);
    const genericChar* Extension(const genericChar* set = NULL);
    TimeInfo        *Date(TimeInfo *set = NULL);
    TimeInfo        *CheckIn(TimeInfo *timevar = NULL);
    TimeInfo        *CheckOut(TimeInfo *timevar = NULL);
};

/**** General Functions ****/
genericChar* SeatName( int seat, genericChar* str, int guests = -1 ); // return pointer to string with seat letter

#endif
