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
 * layout_zone.hh - revision 40 (2/26/98)
 * base class zone object for display layout
 */

#ifndef LAYOUT_ZONE_HH
#define LAYOUT_ZONE_HH

#include "pos_zone.hh"
#include "terminal.hh"


/**** Types ****/
class LayoutZone : public PosZone
{
public:
    Flt size_x;
    Flt size_y;
    Flt min_size_x;
    Flt min_size_y;
    Flt max_size_x;
    Flt max_size_y;
    Flt selected_x;
    Flt selected_y;
    int font_width;
    int font_height;
    int left_margin;
    int right_margin;
    int num_spaces;  // number of spaces (characters) in a column (for reports)

    // Constructor
    LayoutZone();

    // Member Functions
    virtual RenderResult Render(Terminal *t, int update_flag);
    virtual SignalResult Touch(Terminal *t, int tx, int ty);
    virtual int          SetSize(Terminal *t, int width, int height);
    virtual int          ZoneStates() { return 1; }

    Flt TextWidth(Terminal *t, const genericChar* str, int len = 0);
    int SetMargins(int left, int right);
    int TextL(Terminal *t, Flt line, const genericChar* text,
              int color = COLOR_DEFAULT, int mode = 0);
    int TextC(Terminal *t, Flt line, const genericChar* text,
              int color = COLOR_DEFAULT, int mode = 0);
    int TextR(Terminal *t, Flt line, const genericChar* text,
              int color = COLOR_DEFAULT, int mode = 0);

    int TextPosL(Terminal *t, Flt row, Flt line, const genericChar* text,
                 int color = COLOR_DEFAULT, int mode = 0);
    int TextPosC(Terminal *t, Flt row, Flt line, const genericChar* text,
                 int color = COLOR_DEFAULT, int mode = 0);
    int TextPosR(Terminal *t, Flt row, Flt line, const genericChar* text,
                 int color = COLOR_DEFAULT, int mode = 0);

    int TextLR(Terminal *t, Flt line,
               const genericChar* l_text, int l_color, const genericChar* r_text, int r_color);

    int Line(Terminal *t, Flt line, int color = COLOR_DEFAULT);
    int LinePos(Terminal *t, Flt row, Flt line, Flt len,
                int color = COLOR_DEFAULT);

    int Entry(Terminal *t, Flt px, Flt py, Flt len, RegionInfo *place = nullptr);
    int Button(Terminal *t, Flt px, Flt py, Flt len, int lit = 0);
    int Background(Terminal *t, Flt line, Flt h, int texture);
    int Raised(Terminal *t, Flt line, Flt h);
    int Underline(Terminal *t, Flt px, Flt py, Flt len,
                  int color = COLOR_DEFAULT);
    int ColumnSpacing(Terminal *term, int num_columns);
    int Width(Terminal *term);
};

#endif
