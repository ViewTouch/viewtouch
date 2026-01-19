/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025, 2026
  
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
 * merchant_zone.hh - revision 2 (12/18/97)
 * Credit/Debit Card merchant authorization infomation entry zone
 */

#ifndef _MERCHANT_ZONE_HH
#define MERCHANT_ZONE_HH

#include "form_zone.hh"


/**** Types ****/
class MerchantZone : public FormZone
{
public:
    // Constructor
    MerchantZone();

    // Member Functions
    int          Type() override { return ZONE_MERCHANT; }
    RenderResult Render(Terminal *t, int update_flag) override;
    Flt         *Spacing() override { return &form_spacing; }

    int LoadRecord(Terminal *t, int record_no) override;
    int SaveRecord(Terminal *t, int record_no, int write_file) override;
};

#endif
