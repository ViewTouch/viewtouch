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
 * payment_zone.hh - revision 60 (2/3/98)
 * touch zone objects for showing/recieving payments for current check
 */

#ifndef _PAYMENT_ZONE_HH
#define _PAYMENT_ZONE_HH

#include "layout_zone.hh"
#include "check.hh"


/**** Types ****/
class PaymentZone : public LayoutZone
{
    Flt      spacing;
    Payment *current_payment;
    int      amount;
    int      voided;
    int      drawer_open;
    float    input_line;
    float    header_size;
    float    mark;
    SubCheck work_sub;
    int      have_name;

public:
    // Constructor
    PaymentZone();

    // Memeber Functions
    int          Type() { return ZONE_PAYMENT_ENTRY; }
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Signal(Terminal *term, const genericChar* message);
    SignalResult Keyboard(Terminal *term, int key, int state);
    SignalResult Touch(Terminal *term, int tx, int ty);
    int          Update(Terminal *term, int update_message, const genericChar* value);
    int          AddPayment(Terminal *term, int type, int id,
                            int amount, int flags);
    int          AddPayment(Terminal *term, int type, const genericChar* swipe_value);
    Flt         *Spacing() { return &spacing; }

    int  RenderPaymentEntry(Terminal *term);
    int  DrawPaymentEntry(Terminal *term);
    int  SaveCheck(Terminal *term);
    int  CloseCheck(Terminal *term, int force);
    int  DoneWithCheck(Terminal *term, int store_check = 1);
    int  NextCheck(Terminal *term, int force);
    int  Merchandise(Terminal *term, SubCheck *sc);
    int  OpenDrawer(Terminal *term);
};

class TenderZone : public PosZone
{
    int tender_type;
    int amount;

public:
    // Constructor
    TenderZone();

    // Member Functions
    int          Type() { return ZONE_TENDER; }
    int          RenderInit(Terminal *term, int update_flag);
    SignalResult Touch(Terminal *term, int tx, int ty);
    int          ZoneStates() { return 3; }

    int *TenderType()   { return &tender_type; }
    int *TenderAmount() { return &amount; }
};

#endif
