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
 * archive.hh - revision 2 (8/10/98)
 * Data storage past business days
 */

#ifndef _ARCHIVE_HH
#define _ARCHIVE_HH

#include "tips.hh"
#include "labor.hh"
#include "exception.hh"
#include "check.hh"
#include "drawer.hh"
#include "list_utility.hh"
#include "expense.hh"
#include "settings.hh"


/**** Definitions ****/
#define ARCHIVE_VERSION 14


/**** Types ****/
class Check;
class Drawer;
class Terminal;
class Settings;
class InputDataFile;
class OutputDataFile;
class CreditDB;

class Archive
{
    DList<Check>          check_list;
    DList<Drawer>         drawer_list;
    DList<DiscountInfo>   discount_list;
    DList<CouponInfo>     coupon_list;
    DList<CreditCardInfo> creditcard_list;
    DList<CompInfo>       comp_list;
    DList<MealInfo>       meal_list;
    short                 from_disk;  // if this is positive, we'll avoid writing

public:
    Archive *next, *fore;
    Str      filename;
    int      file_version;
    Str      altmedia;
    Str      altsettings;

    int      id;
    int      last_serial_number;
    TimeInfo start_time, end_time; // archive time period
    short    check_version;
    short    drawer_version;
    short    tip_version;
    short    work_version;
    short    exception_version;
    short    expense_version;
    int      media_version;
    int      settings_version;
    short    loaded;   // boolean - have archive contents been loaded?
    short    changed;  // has archive been changed since last load or save?
    short    corrupt;   // error in loading archive - no changes will save

    // Settings that shouldn't change (from settings.hh)
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
    int price_rounding;          // cost rounding setting
    int change_for_credit;       // boolean - make change for credit cards?
    int change_for_roomcharge;   // boolean - make change for room charge?
    int change_for_checks;       // boolean - make change for checks?
    int change_for_gift;         // boolean - make change for gift certificates?
    int discount_alcohol;        // boolean - allow discounts/comps for alcohol?

    TipDB         tip_db;
    WorkDB        work_db;
    ExceptionDB   exception_db;
    ExpenseDB     expense_db;
    CreditDB     *cc_exception_db;
    CreditDB     *cc_refund_db;
    CreditDB     *cc_void_db;
    CCInit       *cc_init_results;
    CCSAFDetails *cc_saf_details_results;
    CCSettle     *cc_settle_results;

    // Constructors
    Archive(TimeInfo &tm);
    Archive(Settings *s, const genericChar *file);
    // Destructor
    ~Archive() { Unload(); }

    // Member Functions
    Check          *CheckList()      { return check_list.Head(); }
    Check          *CheckListEnd()   { return check_list.Tail(); }
    Drawer         *DrawerList()     { return drawer_list.Head(); }
    Drawer         *DrawerListEnd()  { return drawer_list.Tail(); }
    DiscountInfo   *DiscountList()   { return discount_list.Head(); }
    CouponInfo     *CouponList()     { return coupon_list.Head(); }
    CreditCardInfo *CreditCardList() { return creditcard_list.Head(); }
    CompInfo       *CompList()       { return comp_list.Head(); }
    MealInfo       *MealList()       { return meal_list.Head(); }

    int LoadPacked(Settings *s, const genericChar *filename = NULL);
    // Loads archive from single file
    int LoadUnpacked(Settings *s, const genericChar *path);
    // Loads archive from files in directory (usually 'current' directory)
    int LoadAlternateMedia();
    int LoadAlternateSettings();
    int SavePacked();
    // Saves archive contents
    int Unload();
    // Purges archive contents - makes archive as unloaded

    int Add(Drawer *d);
    int Remove(Drawer *d);

    int Add(Check *c);
    int Remove(Check *c);

    int Add(WorkEntry *we);
    int Remove(WorkEntry *we);

    int Add(DiscountInfo *discount);
    int Add(CouponInfo *coupon);
    int Add(CreditCardInfo *creditcard);
    int Add(CompInfo *comp);
    int Add(MealInfo *meal);

    int DiscountCount();
    int CouponCount();
    int CreditCardCount();
    int CompCount();
    int MealCount();

    DiscountInfo   *FindDiscountByID(int discount_id);
    CouponInfo     *FindCouponByID(int coupon_id);
    CompInfo       *FindCompByID(int comp_id);
    CreditCardInfo *FindCreditCardByID(int creditcard_id);
    MealInfo       *FindMealByID(int meal_id);
};

#endif
