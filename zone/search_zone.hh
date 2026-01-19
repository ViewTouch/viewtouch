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
 * search_zone.hh - revision 1 (10/13/98)
 */

#ifndef _SEARCH_ZONE_HH
#define SEARCH_ZONE_HH

#include "layout_zone.hh"


/**** SearchZone Class ****/
class SearchZone : public LayoutZone
{
    int  search;
    genericChar buffer[STRLENGTH];

public:
    // Constructor
    SearchZone();

    // Member Functions
    int          Type() override { return ZONE_SEARCH; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Keyboard(Terminal *term, int key, int state) override;
    int          LoseFocus(Terminal *term, Zone *newfocus) override;
    int          ZoneStates() override { return 2; }
};

#endif
