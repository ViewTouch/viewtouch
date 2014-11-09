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
 * printer_zone.hh - revision 13 (11/6/97)
 * Printer target & destination selection zones
 */

#ifndef _PRINTER_ZONE_HH
#define _PRINTER_ZONE_HH

#include "form_zone.hh"
#include "zone_object.hh"


/**** Types ****/
class PrintTargetZone : public FormZone
{
    unsigned long phrases_changed;

public:
    // Constructor
    PrintTargetZone();

    // Member Functions
    int          Type() { return ZONE_PRINT_TARGET; }
    int          AddFields();
    RenderResult Render(Terminal *t, int update_flag);

    int LoadRecord(Terminal *t, int record);
    int SaveRecord(Terminal *t, int record, int write_file);
};

class SplitKitchenZone : public PosZone
{
    ZoneObjectList kitchens;

public:
    // Member Functions
    int          Type() { return ZONE_SPLIT_KITCHEN; }
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Signal(Terminal *t, genericChar *message);
    SignalResult Touch(Terminal *t, int tx, int ty);
    int          ZoneStates() { return 1; }

    int MoveTerms(Terminal *t, int no);
};

class ReceiptSetZone : public FormZone
{
public:
    // Constructor
    ReceiptSetZone();

    // Member Functions
    int          Type() { return ZONE_RECEIPT_SET; }
    RenderResult Render(Terminal *t, int update_flag);

    int LoadRecord(Terminal *t, int record);
    int SaveRecord(Terminal *t, int record, int write_file);
};

#endif
