/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997  
  
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
 * split_check_zone.hh - revision 23 (11/6/97)
 * Zone for dividing checks by order or seat
 */

#ifndef _SPLIT_CHECK_HH
#define _SPLIT_CHECK_HH

#include "pos_zone.hh"
#include "zone_object.hh"


/**** Types ****/
class CheckObj;
class ItemObj;
class PrintTargetObj;

class SubCheck;
class Order;
class SplitCheckZone : public PosZone
{
    ZoneObjectList checks;
    int seat_mode;
    int start_check;
    CheckObj *from_check;
    CheckObj *to_check;
    ItemObj  *item_object;

public:
    // Member Functions
    int          Type() { return ZONE_SPLIT_CHECK; }
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Signal(Terminal *t, const genericChar* message);
    SignalResult Touch(Terminal *t, int tx, int ty);
    int          ZoneStates() { return 1; }

    int CreateChecks(Terminal *t);
    int LayoutChecks(Terminal *t);
    int MoveItems(Terminal *t, CheckObj *target, int move_amount = -1);
    // moves selected items to check
    int PrintReceipts(Terminal *t);
};

class ItemPrintTargetZone : public PosZone
{
    ZoneObjectList targets;
    ZoneObjectList empty_targets;

public:
    // Member Functions
    int Type() { return ZONE_ITEM_TARGET; }
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Signal(Terminal *t, const genericChar* message);
    SignalResult Touch(Terminal *t, int tx, int ty);
    int          ZoneStates() { return 1; }

    int MoveItems(Terminal *t, PrintTargetObj *target);
};

#endif
