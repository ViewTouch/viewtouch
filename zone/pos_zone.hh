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
 * pos_zone.hh - revision 30 (8/20/98)
 * Definition of zone types & other zone data
 */

#ifndef POS_ZONE_HH
#define POS_ZONE_HH

#include "zone.hh"


/**** Definitions ****/
#define ZONE_VERSION 29

// Zone Types (converted from macros to enum constants)
enum ZoneType : std::uint8_t {
    ZONE_UNDEFINED       = 0,   // type not defined
    ZONE_STANDARD        = 1,   // button with message & jump
    ZONE_ITEM            = 2,   // order menu item
    ZONE_CONDITIONAL     = 3,   // work if conditions are met
    ZONE_TENDER          = 4,   // tender payment type button
    ZONE_TABLE           = 5,   // table status/selection
    ZONE_COMMENT         = 6,   // only seen by superuser
    ZONE_QUALIFIER       = 7,   // qualifier: no, extra, lite
    ZONE_TOGGLE          = 8,   // button with toggleing text/message
    ZONE_SIMPLE          = 9,   // button with only jump
    ZONE_SWITCH          = 10,  // settings selection button

    ZONE_LOGIN           = 20,  // takes user id for login
    ZONE_COMMAND         = 21,  // system command/status
    ZONE_GUEST_COUNT     = 23,  // enter the number of guests
    ZONE_LOGOUT          = 24,  // user logout stuff
    ZONE_CHECK_LIST      = 31,  // show open checks
    ZONE_ORDER_ENTRY     = 30,  // show current menu order
    ZONE_ORDER_PAGE      = 51,  // page change on order entry window
    ZONE_ORDER_FLOW      = 64,  // order start/index/continue
    ZONE_ORDER_ADD       = 83,  // increase order button on order entry page
    ZONE_ORDER_DELETE    = 84,  // delete/rebuild button on order entry page
    ZONE_ORDER_DISPLAY   = 85,  // kitchen work order display
    ZONE_ORDER_COMMENT   = 104, // add comment button on order entry page
    ZONE_PAYMENT_ENTRY   = 32,  // show/allow payments for check
    ZONE_USER_EDIT       = 33,  // show/edit users
    ZONE_SETTINGS        = 34,  // edit general system variables
    ZONE_TAX_SETTINGS    = 35,  // tax and royalty settings
    ZONE_DEVELOPER       = 36,  // developer application settings
    ZONE_TENDER_SET      = 37,  // tender selection & settings
    ZONE_TAX_SET         = 38,  // tax specifications
    ZONE_MONEY_SET       = 39,  // currency specifications
    ZONE_CC_SETTINGS     = 40,  // credit/charge card settings
    ZONE_CC_MSG_SETTINGS = 41,  // credit/charge card messages
    ZONE_REPORT          = 50,  // super report zone
    ZONE_SCHEDULE        = 52,  // employee scheduling
    ZONE_PRINT_TARGET    = 53,  // family printer destinations
    ZONE_SPLIT_CHECK     = 54,  // check splitting zone
    ZONE_DRAWER_MANAGE   = 55,  // Drawer pulling/balancing
    ZONE_HARDWARE        = 56,  // terminal & printer setup/settings/status
    ZONE_TIME_SETTINGS   = 57,  // store hours/shifts
    ZONE_TABLE_ASSIGN    = 58,  // transfer tables/checks between servers
    ZONE_CHECK_DISPLAY   = 59,  // display multiple checks on the screen
    ZONE_KILL_SYSTEM     = 61,  // system termination
    ZONE_PAYOUT          = 62,  // cash payout system
    ZONE_DRAWER_ASSIGN   = 63,  // drawer assignment
    ZONE_SEARCH          = 66,  // search for word though records
    ZONE_SPLIT_KITCHEN   = 67,  // split kitchen terminal assignment
    ZONE_END_DAY         = 68,  // end of day management
    ZONE_READ            = 69,  // reading & displaying text files
    ZONE_JOB_SECURITY    = 70,  // job secuity settings
    ZONE_INVENTORY       = 71,  // raw product inventory
    ZONE_RECIPE          = 72,  // recipes using raw products
    ZONE_VENDOR          = 73,  // raw product suppliers
    ZONE_LABOR           = 74,  // labor management
    ZONE_ITEM_LIST       = 75,  // list all sales items
    ZONE_INVOICE         = 76,  // invoice entry/listing
    ZONE_PHRASE          = 77,  // phrase translation/replacement
    ZONE_ITEM_TARGET     = 78,  // item printer target
    ZONE_RECEIPT_SET     = 79,  // printed receipt settings
    ZONE_MERCHANT        = 80,  // merchant info for credit authorize
    ZONE_LICENSE         = 81,  // viewtouch pos license setup
    ZONE_ACCOUNT         = 82,  // chart of accounts list/edit
    ZONE_CHART           = 86,  // spreadsheet like data display
    ZONE_VIDEO_TARGET    = 87,  // for Kitchen Video, which food types get displayed
    ZONE_EXPENSE         = 88,  // paying expense from revenue
    ZONE_STATUS_BUTTON   = 89,  // for error messages and other things
    ZONE_CDU             = 90,  // CDU string entry and modification
    ZONE_RECEIPTS        = 91,  // Strings for receipt headers and footers
    ZONE_CUSTOMER_INFO   = 92,  // For editing customer info (name, address, et al)
    ZONE_CHECK_EDIT      = 93,  // For editing check info like Delivery Date.
    ZONE_CREDITCARD_LIST = 94,  // For managing exceptions, refunds, and voids
    ZONE_EXPIRE_MSG      = 95,  // For setting the expiration message
    ZONE_REVENUE_GROUPS  = 96,  // Revenue group settings for menu families
    ZONE_IMAGE_BUTTON    = 97,  // button with user-selectable image
    ZONE_CALCULATION_SETTINGS = 110,  // calculation settings (multiply, add/subtract)
    ZONE_ITEM_NORMAL     = 98,  // menu item button
    ZONE_ITEM_MODIFIER   = 99,  // modifier button
    ZONE_ITEM_METHOD     = 100, // non-tracking modifier button
    ZONE_ITEM_SUBSTITUTE = 101, // menu item + substitute button
    ZONE_ITEM_POUND      = 102, // priced by weight button
    ZONE_ITEM_ADMISSION  = 103, // event admission button
    ZONE_INDEX_TAB       = 108, // index tab button (only on Index pages, inherited by Menu Item pages)
    ZONE_LANGUAGE_BUTTON = 109, // language selection button
    ZONE_CLEAR_SYSTEM    = 107  // clear system with countdown
};

/**** Types ****/
class PosZone : public Zone
{
    // Optional image for zone background (used by button-like zones)
    Str image_path;

public:
    // Member Functions
    int   Type() override { return ZONE_UNDEFINED; }
    std::unique_ptr<Zone> Copy() override;

    int CanSelect(Terminal *t) override;
    int CanEdit(Terminal *t) override;
    int CanCopy(Terminal *t) override;
    int SetSize(Terminal *t, int width, int height) override;
    int SetPosition(Terminal *t, int pos_x, int pos_y) override;
    int Read(InputDataFile &df, int version) override;
    int Write(OutputDataFile &df, int version) override;

    Str *ImagePath() { return &image_path; }
};

class PosPage : public Page
{
public:
    // Member Functions
    std::unique_ptr<Page> Copy() override;
    int   Read(InputDataFile &df, int version) override;
    int   Write(OutputDataFile &df, int version) override;
};


/**** Functions ****/
Zone *NewPosZone(int type);
// Calls the proper function for creating a particular zone
Page *NewPosPage();
// returns a new POS page

#endif
