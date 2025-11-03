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
 * sales.hh - revision 96 (8/27/98)
 * Definitions of sale item classes
 */

#ifndef _SALES_HH
#define _SALES_HH

#include "utility.hh"
#include "list_utility.hh"
#include <string>


/**** Definitions ****/

#define SALES_ITEM_VERSION       16

// Family Difinitions
#define FAMILY_APPETIZERS        0
#define FAMILY_BEVERAGES         1
#define FAMILY_LUNCH_ENTREES     2
#define FAMILY_CHILDRENS_MENU    3
#define FAMILY_DESSERTS          4
#define FAMILY_SANDWICHES        5
#define FAMILY_SIDE_ORDERS       6
#define FAMILY_BREAKFAST_ENTREES 7
#define FAMILY_ALACARTE          8
#define FAMILY_BURGERS           10
#define FAMILY_DINNER_ENTREES    11
#define FAMILY_SALADS            12
#define FAMILY_SOUP              13
#define FAMILY_PIZZA             14
#define FAMILY_SPECIALTY         15
#define FAMILY_BEER              16
#define FAMILY_BOTTLED_BEER      17
#define FAMILY_WINE              18
#define FAMILY_BOTTLED_WINE      19
#define FAMILY_COCKTAIL          20
#define FAMILY_BOTTLED_COCKTAIL  21
#define FAMILY_SEAFOOD           22
#define FAMILY_MODIFIER          23
#define FAMILY_LIGHT_DINNER      24
#define FAMILY_REORDER           25
#define FAMILY_MERCHANDISE       26
#define FAMILY_SPECIALTY_ENTREE  27
#define FAMILY_RESERVED_WINE     28
#define FAMILY_BANQUET           29
#define FAMILY_BAKERY            30
#define FAMILY_ROOM              31
#define FAMILY_UNKNOWN           255

// Qualifier Definitions
#define QUALIFIER_NONE      0
#define QUALIFIER_NO        (1<<0)
#define QUALIFIER_SIDE      (1<<1)
#define QUALIFIER_SUB       (1<<2)
#define QUALIFIER_LITE      (1<<3)
#define QUALIFIER_ONLY      (1<<4)
#define QUALIFIER_EXTRA     (1<<5)
#define QUALIFIER_DOUBLE    (1<<6)
#define QUALIFIER_DRY       (1<<7)
#define QUALIFIER_PLAIN     (1<<8)
#define QUALIFIER_TOASTED   (1<<9)
#define QUALIFIER_UNTOASTED (1<<10)
#define QUALIFIER_CRISPY    (1<<11)
#define QUALIFIER_HARD      (1<<12)
#define QUALIFIER_SOFT      (1<<13)
#define QUALIFIER_GRILLED   (1<<14)
#define QUALIFIER_LEFT      (1<<15)
#define QUALIFIER_RIGHT     (1<<16)
#define QUALIFIER_WHOLE     (1<<17)
#define QUALIFIER_CUT2      (1<<18)
#define QUALIFIER_CUT3      (1<<19)
#define QUALIFIER_CUT4      (1<<20)

// Item Definitions
#define ITEM_NORMAL       0  // Regular menu item
#define ITEM_MODIFIER     1  // Modifier
#define ITEM_METHOD       2  // Non-tracking modifier
#define ITEM_SUBSTITUTE   3  // Menu item that can substituted (i.e., used) as a modifier
#define ITEM_COMBO        4  // Item that is only served with another item
#define ITEM_RECIPE       5  // Not for sale - recipe only
#define ITEM_POUND        6  // For sale by the pound
#define ITEM_ADMISSION    7  // Admission type

// Sales Definitions
#define SALES_FOOD          0
#define SALES_ALCOHOL       1
#define SALES_UNTAXED       2
#define SALES_ROOM          4
#define SALES_MERCHANDISE   8
#define SALES_NO_COMP      16
#define SALES_NO_EMPLOYEE  32
#define SALES_NO_DISCOUNT  64
#define SALES_TAKE_OUT    128  // for takeout orders attached to tables

// Sales Group (for families)
// Sales group types - defined in vt_enum_utils.hh for utilities

// Legacy defines for backward compatibility
#define SALESGROUP_NONE        0  // Don't use family
#define SALESGROUP_FOOD        1
#define SALESGROUP_BEVERAGE    2
#define SALESGROUP_BEER        3
#define SALESGROUP_WINE        4
#define SALESGROUP_ALCOHOL     5
#define SALESGROUP_MERCHANDISE 6
#define SALESGROUP_ROOM        7

// Sales Pricing
#define PRICE_NONE     0
#define PRICE_PERITEM  1
#define PRICE_PERHOUR  2
#define PRICE_PERDAY   3


/**** Global Data ****/
extern const char* SalesGroupName[];
extern const char* SalesGroupShortName[];
extern int   SalesGroupValue[];


/**** Types ****/
class Check;
class LayoutForm;
class Report;
class ZoneDB;
class Terminal;
class Settings;
class InputDataFile;
class OutputDataFile;

class Component
{
public:
    Component *next, *fore;
    int item_id;  // recipe or product id

    // Constructor
    Component();
};

class SalesItem
{
    DList<Component> component_list;

public:
    SalesItem *next;
    SalesItem *fore;
    int   id;             // ID value
    Str   item_code;      // exclusively for call center usage
    Str   item_name;      // name of product
    Str   zone_name;      // name shown on zone
    Str   image_path;     // custom image associated with this item
    Str   print_name;     // name printer on customer check
    Str   call_center_name;  // name for call centers (remote, automated order entry)
    Str   location;
    Str   event_time;     // item event time
    Str   total_tickets;  // item total tickets
    Str   available_tickets; // item available tickets
    Str   price_label; //item price label (such as matinee, etc)
    int   cost;           // Cost of product in cents
    int   employee_cost;  // Cost of product in cents for employees
    int   sub_cost;       // Cost of product if used for a substitute
    int   takeout_cost;   // Cost of product when it's a takeout order
    int   delivery_cost;  // Cost of product when it's a delivered order
    int   tax_id;         // revised tax type id (eat-in orders)
    int   takeout_tax_id; // revised tax type id (takeout orders)
    short type;           // menu item type
    short call_order;     // Priority for being printed on check
    short printer_id;     // Printer where this item prints out
    short family;         // general type of food
    short item_class;     // more specific type
    short sales_type;     // tax/discount catagory (soon to be obsolete)
    short stocked;        // boolean - is item stocked or made to order
    short has_zone;       // boolean - is there a zone that can order this item?
    short allow_increase; // whether to show the OrderAddZone button.
    short ignore_split;	  // ignore split kitchen?
    int   period;         // time of day served
    int   prepare_time;   // time to make menu item
    int   quanity;        // Number of item remaining
    int   changed;        // boolean - has this item been altered?
    int   price_type;     // price type (see above)

    // Constructor
    SalesItem(const char* name = NULL);

    // Member Functions
    Component *ComponentList() { return component_list.Head(); }

    int Copy(SalesItem *target);
    int Add(Component *in);
    // Add Component to SalesItem
    int Remove(Component *in);
    // Remove Component from SalesItem
    int Purge();
    // Remove & delete all Components from SalesItem
    int Read(InputDataFile &df, int version);
    // Reads SalesItem object from file
    int Write(OutputDataFile &df, int version);
    // Writes SalesItem object to file
    const char* Family(Terminal *t);
    // Returns string with family name
    const char* Printer(Terminal *t);
    // Returns string with printer destination name
    int Price(Settings *s, int qualifier);
    // Returns eat-in price for item based on qualifier
    const char* ZoneName();
    // Text to be put on item zone for the menu item
    const char* PrintName();
    // Name to be used when printing
    const char* CallCenterName(Terminal *t);
};

class GroupItem
{
public:
    GroupItem *next, *fore;
    Str name;
    int price;

    // Constructor
    GroupItem();

    // Member Functions
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
};

class ItemDB
{
    SalesItem **name_array; // array of items for binary search
    int         array_size;
    int         last_id;    // last id used

    int BuildNameArray();
    // Builds name search array (for binary search)

    DList<SalesItem> item_list;
    DList<GroupItem> group_list;

public:
    Str filename;          // db save filename
    int changed;           // boolean - has menu been changed?
    int merchandise_count; // result from ItemCount()
    int merchandise_sales;
    int other_count;       // result from ItemCount()
    int other_sales;

    // Constructor
    ItemDB();
    // Destructor
    ~ItemDB() { Purge(); }

    // Member Functions
    SalesItem *ItemList()     { return item_list.Head(); }
    SalesItem *ItemListEnd()  { return item_list.Tail(); }
    int        ItemCount()    { return item_list.Count(); }
    GroupItem *GroupList()    { return group_list.Head(); }
    GroupItem *GroupListEnd() { return group_list.Tail(); }
    int        GroupCount()   { return group_list.Count(); }

    int Load(const char* filename);
    // Reads SalesItem records from file into object
    int Save();
    // Writes all SalesItem records from object to file
    int Add(SalesItem *mi);
    // Adds SalesItem to object (sorted by name)
    int Remove(SalesItem *mi);
    // Removes SalesItem from object
    int Purge();
    // Ends the day
    int ResetAdmissionItems();


    // Removes & deletes all SalesItem records from object
    SalesItem *FindByName(const std::string &name);
    // Finds SalesItem by name
    SalesItem *FindByID(int id);
    // Finds SalesItem by ID value
    SalesItem *FindByRecord(int record);
    // Finds SalesItem by record position
    SalesItem *FindByWord(const char* word, int &record);
    SalesItem *FindByCallCenterName(const char* word, int &record);
    SalesItem *FindByItemCode(const char* code, int &record);
    // Finds SalesItem by key word
    int DeleteUnusedItems(ZoneDB *zone_db);
    // Deletes SalesItems that are not in zone_db
    int ItemsInFamily(int family);
};

/**** Functions ****/
int   MergeQualifier(int &flag, int qualifier);
int   PrintItem(char* buffer, int qualifier, const char* item);
std::string FilterName(const std::string &name);

#endif
