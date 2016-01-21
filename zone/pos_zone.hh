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
 * pos_zone.hh - revision 30 (8/20/98)
 * Definition of zone types & other zone data
 */

#ifndef _POS_ZONE_HH
#define _POS_ZONE_HH

#include "zone.hh"


/**** Definitions ****/
#define ZONE_VERSION 28

// Zone Types
#define ZONE_UNDEFINED       0   // type not defined
#define ZONE_STANDARD        1   // button with message & jump
#define ZONE_ITEM            2   // order menu item
#define ZONE_CONDITIONAL     3   // work if conditions are met
#define ZONE_TENDER          4   // tender payment type button
#define ZONE_TABLE           5   // table status/selection
#define ZONE_COMMENT         6   // only seen by superuser
#define ZONE_QUALIFIER       7   // qualifier: no, extra, lite
#define ZONE_TOGGLE          8   // button with toggleing text/message
#define ZONE_SIMPLE          9   // button with only jump
#define ZONE_SWITCH          10  // settings selection button
#define ZONE_LOGIN           20  // takes user id for login
#define ZONE_COMMAND         21  // system command/status
#define ZONE_GUEST_COUNT     23  // enter the number of guests
#define ZONE_LOGOUT          24  // user logout stuff
#define ZONE_CHECK_LIST      31  // show open checks
#define ZONE_ORDER_ENTRY     30  // show current menu order
#define ZONE_ORDER_PAGE      51  // page change on order entry window
#define ZONE_ORDER_FLOW      64  // order start/index/continue
#define ZONE_ORDER_ADD       83  // increase order button on order entry page
#define ZONE_ORDER_DELETE    84  // delete/rebuild button on order entry page
#define ZONE_ORDER_DISPLAY   85  // kitchen work order display
#define ZONE_PAYMENT_ENTRY   32  // show/allow payments for check
#define ZONE_USER_EDIT       33  // show/edit users
#define ZONE_SETTINGS        34  // edit general system variables
#define ZONE_TAX_SETTINGS    35  // tax and royalty settings
#define ZONE_DEVELOPER       36  // developer application settings
#define ZONE_TENDER_SET      37  // tender selection & settings
#define ZONE_TAX_SET         38  // tax specifications
#define ZONE_MONEY_SET       39  // currency specifications
#define ZONE_CC_SETTINGS     40  // credit/charge card settings
#define ZONE_CC_MSG_SETTINGS 41  // credit/charge card messages
#define ZONE_REPORT          50  // super report zone
#define ZONE_SCHEDULE        52  // employee scheduling
#define ZONE_PRINT_TARGET    53  // family printer destinations
#define ZONE_SPLIT_CHECK     54  // check splitting zone
#define ZONE_DRAWER_MANAGE   55  // Drawer pulling/balancing
#define ZONE_HARDWARE        56  // terminal & printer setup/settings/status
#define ZONE_TIME_SETTINGS   57  // store hours/shifts
#define ZONE_TABLE_ASSIGN    58  // transfer tables/checks between servers
#define ZONE_CHECK_DISPLAY   59  // display multiple checks on the screen
#define ZONE_KILL_SYSTEM     61  // system termination
#define ZONE_PAYOUT          62  // cash payout system
#define ZONE_DRAWER_ASSIGN   63  // drawer assignment
#define ZONE_SEARCH          66  // search for word though records
#define ZONE_SPLIT_KITCHEN   67  // split kitchen terminal assignment
#define ZONE_END_DAY         68  // end of day management
#define ZONE_READ            69  // reading & displaying text files
#define ZONE_JOB_SECURITY    70  // job secuity settings
#define ZONE_INVENTORY       71  // raw product inventory
#define ZONE_RECIPE          72  // recipes using raw products
#define ZONE_VENDOR          73  // raw product suppliers
#define ZONE_LABOR           74  // labor management
#define ZONE_ITEM_LIST       75  // list all sales items
#define ZONE_INVOICE         76  // invoice entry/listing
#define ZONE_PHRASE          77  // phrase translation/replacement
#define ZONE_ITEM_TARGET     78  // item printer target
#define ZONE_RECEIPT_SET     79  // printed receipt settings
#define ZONE_MERCHANT        80  // merchant info for credit authorize
#define ZONE_LICENSE         81  // viewtouch pos license setup
#define ZONE_ACCOUNT         82  // chart of accounts list/edit
#define ZONE_CHART           86  // spreadsheet like data display
#define ZONE_VIDEO_TARGET    87  // for Kitchen Video, which food types get displayed
#define ZONE_EXPENSE         88  // paying expense from revenue
#define ZONE_STATUS_BUTTON   89  // for error messages and other things
#define ZONE_CDU             90  // CDU string entry and modification
#define ZONE_RECEIPTS        91  // Strings for receipt headers and footers
#define ZONE_CUSTOMER_INFO   92  // For editing customer info (name, address, et al)
#define ZONE_CHECK_EDIT      93  // For editing check info like Delivery Date.
#define ZONE_CREDITCARD_LIST 94  // For managing exceptions, refunds, and voids
#define ZONE_EXPIRE_MSG      95  // For setting the expiration message

/**** Types ****/
class PosZone : public Zone
{
    // Member Functions
    virtual int   Type() { return ZONE_UNDEFINED; }
    virtual Zone *Copy();
    
    virtual int CanSelect(Terminal *t);
    virtual int CanEdit(Terminal *t);
    virtual int CanCopy(Terminal *t);
    virtual int SetSize(Terminal *t, int width, int height);
    virtual int SetPosition(Terminal *t, int pos_x, int pos_y);
    virtual int Read(InputDataFile &df, int version);
    virtual int Write(OutputDataFile &df, int version);
};

class PosPage : public Page
{
public:
    // Member Functions
    virtual Page *Copy();
    virtual int   Read(InputDataFile &df, int version);
    virtual int   Write(OutputDataFile &df, int version);
};


/**** Functions ****/
Zone *NewPosZone(int type);
// Calls the proper function for creating a particular zone
Page *NewPosPage();
// returns a new POS page

#endif
