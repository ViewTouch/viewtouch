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
 * chart.hh - revision 1 (10/20/98)
 * Report container
 */

#ifndef _CHART_HH
#define _CHART_HH

#include "list_utility.hh"
#include "utility.hh"


/**** Types ****/
class ChartCell
{
public:
    ChartCell *next, *fore;
    Str text;
    int align, color;

    // Constructor
    ChartCell();
};

class ChartRow
{
public:
    ChartRow *next, *fore;

    DList<ChartCell> cell_list;
    int id;

    // Construtor
    ChartRow();
};

class Chart
{
public:
    DList<ChartCell> header_list;
    DList<ChartRow>  row_list;
    ChartRow *current_row;

    // Constructor
    Chart();

    // Member Functions
    int Clear();
    int AddColumn(const char* name);
    int SortByColumn(int id);
    int AddRowCell(const char* text);
    int EndRow();
};

#endif
