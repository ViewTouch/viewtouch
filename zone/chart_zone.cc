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
 * chart_zone.cc - revision 1 (10/20/98)
 * Implementation of chart module
 */

#include "chart_zone.hh"
#include "terminal.hh"
#include "chart.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** ChartZone class ****/
// Constructor
ChartZone::ChartZone()
{
    chart = NULL;
}

// Destructor
ChartZone::~ChartZone()
{
    if (chart)
        delete chart;
}

// Member Functions
Zone *ChartZone::Copy()
{
    return NULL;
}

int ChartZone::RenderInit(Terminal *t, int update_flag)
{
    return 0;
}

RenderResult ChartZone::Render(Terminal *t, int update_flag)
{
    return PosZone::Render(t, update_flag);
}

SignalResult ChartZone::Signal(Terminal *t, const genericChar *message)
{
    return PosZone::Signal(t, message);
}

SignalResult ChartZone::Touch(Terminal *t, int tx, int ty)
{
    return PosZone::Touch(t, tx, ty);
}

int ChartZone::Update(Terminal *t, int update_message)
{
    return 0;
}
