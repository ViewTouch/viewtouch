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
 * chart_zone.hh - revision 1 (10/20/98)
 * Report display & layout zone
 */

#ifndef _CHART_ZONE_HH
#define _CHART_ZONE_HH

#include "pos_zone.hh"
#include "list_utility.hh"


/**** Types ****/
class Chart;

class ChartZone : public PosZone
{
public:
    Chart *chart;

    // Constructor
    ChartZone();
    // Destructor
    ~ChartZone();

    // Member Functions
    int   Type() { return ZONE_CHART; }
    Zone *Copy();

    int          RenderInit(Terminal *t, int update_flag);
    RenderResult Render(Terminal *t, int update_flag);
    SignalResult Signal(Terminal *t, genericChar *message);
    SignalResult Touch(Terminal *t, int tx, int ty);
    int          Update(Terminal *t, int update_message);
};

#endif
