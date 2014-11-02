/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997,1998  
  
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
 * zone_object.cc - revision 4 (8/27/98)
 * Implementation of zone_object module
 */

#include "zone_object.hh"
#include "terminal.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

    
/**** ZoneObject Class ****/
// Constructors
ZoneObject::ZoneObject()
{
    fore = NULL;
    next = NULL;
    selected = 0;
    active = 1;
    font = FONT_DEFAULT;
}

// Member Functions
int ZoneObject::Draw(Terminal *t)
{
    if (active)
    {
        Render(t);
        t->UpdateArea(x, y, w, h);
    }
    return 0;
}

int ZoneObject::Draw(Terminal *t, int set_selected)
{
    selected = set_selected;
    if (active)
    {
        Render(t);
        t->UpdateArea(x, y, w, h);
    }
    return 0;
}

int ZoneObject::Touch(Terminal *t, int tx, int ty)
{
    if (active)
    {
        selected ^= 1;
        Draw(t);
    }
    return 0;
}

int ZoneObject::Layout(Terminal *t, int lx, int ly, int lw, int lh)
{
    SetRegion(lx, ly, lw, lh);
    return 0;
}

/**** ZoneObjectList Class ****/
// Member Functions
int ZoneObjectList::Add(ZoneObject *zo)
{
    return list.AddToTail(zo);
}

int ZoneObjectList::AddToHead(ZoneObject *zo)
{
    return list.AddToHead(zo);
}

int ZoneObjectList::Remove(ZoneObject *zo)
{
    return list.Remove(zo);
}

int ZoneObjectList::CountSelected()
{
    int count = 0;
    for (ZoneObject *zo = List(); zo != NULL; zo = zo->next)
        if (zo->selected && zo->active)
            ++count;
    return count;
}

int ZoneObjectList::Purge()
{
    list.Purge();
    return 0;
}

ZoneObject *ZoneObjectList::Find(int x, int y)
{
    for (ZoneObject *zo = List(); zo != NULL; zo = zo->next)
        if (zo->IsPointIn(x, y) && zo->active)
            return zo;
    return NULL;
}

int ZoneObjectList::Render(Terminal *t)
{
    for (ZoneObject *zo = List(); zo != NULL; zo = zo->next)
        if (zo->active && zo->w > 0 && zo->h > 0)
            zo->Render(t);
    return 0;
}

int ZoneObjectList::SetActive(int val)
{
    for (ZoneObject *zo = List(); zo != NULL; zo = zo->next)
        zo->active = val;
    return 0;
}

int ZoneObjectList::SetSelected(int val)
{
    for (ZoneObject *zo = List(); zo != NULL; zo = zo->next)
        zo->selected = val;
    return 0;
}

int ZoneObjectList::LayoutRows(Terminal *t, int x, int y, int w, int h,
                               int gap)
{
    FnTrace("ZoneObjectList::LayoutRows()");
    int no = Count();
    if (no <= 0)
        return 0;  // nothing to layout

    int pos = 0, ly;
    for (ZoneObject *zo = List(); zo != NULL; zo = zo->next)
    {
        ly = y + ((h * pos) / no);
        zo->Layout(t, x, ly, w, (y + ((h * (pos + 1)) / no)) - ly);
        ++pos;
    }
    return 0;
}

int ZoneObjectList::LayoutColumns(Terminal *t, int x, int y, int w, int h,
                                  int gap)
{
    FnTrace("ZoneObjectList::LayoutColumns()");
    int no = Count();
    if (no <= 0)
        return 0;  // nothing to layout

    int i = 0, s, e;
    int ww = w - (gap * (no - 1));
    for (ZoneObject *zo = List(); zo != NULL; zo = zo->next)
    {
        s = ((ww * i) / no) + (gap * i);
        e = ((ww * (i + 1)) / no) + (gap * i);
        zo->Layout(t, x + s, y, e - s, h);
        ++i;
    }
    return 0;
}

int ZoneObjectList::LayoutGrid(Terminal *t, int x, int y, int w, int h,
                               int gap)
{
    FnTrace("ZoneObjectList::LayoutGrid()");
    int no = Count();
    if (no <= 0)
        return 0;  // nothing to layout

    // Find rows & cols that are about square
    // (height never greater that 5/3 width)
    int r = 0, c = 0;
    do
    {
        ++r;
        c = (no + r - 1) / r;
    }
    while ((h * c * 3) > (w * r * 5));

    int sw = w / c;
    int sh = h / r;
    int cols = c;

    // Layout servers left to right, top to bottom
    r = 0; c = 0;
    for (ZoneObject *zo = List(); zo != NULL; zo = zo->next)
    {
        // FIX - height/width should be calculated each time to remove error
        zo->Layout(t, x + (c * sw), y + (r * sh), sw, sh);
        ++c;
        if (c >= cols)
        {
            c = 0;
            ++r;
        }
    }
    return 0;
}
