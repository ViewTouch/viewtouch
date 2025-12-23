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
 * zone_object.hh - revision 3 (8/10/98)
 * User Interfact componet objects
 */

#ifndef ZONE_OBJECT_HH
#define ZONE_OBJECT_HH

#include "utility.hh"
#include "list_utility.hh"


/**** Types ****/
class Terminal;

class ZoneObject : public RegionInfo
{
public:
    ZoneObject *fore;
    ZoneObject *next;
    int active;
    int selected;
    int font;

    // Constructors
    ZoneObject();
    // Destructor
    ~ZoneObject() override = default;

    // Member Functions
    int Draw(Terminal *t);
    int Draw(Terminal *t, int set_selected);

    virtual int Render(Terminal *t) = 0;
    virtual int Touch(Terminal *t, int tx, int ty);
    virtual int Layout(Terminal *t, int lx, int ly, int lw, int lh);
};

class ZoneObjectList
{
    DList<ZoneObject> list;

public:
    // Member Functions
    ZoneObject *List()    { return list.Head(); }
    ZoneObject *ListEnd() { return list.Tail(); }
    int         Count()   { return list.Count(); }

    int         Add(ZoneObject *zo);
    int         AddToHead(ZoneObject *zo);
    int         Remove(ZoneObject *zo);
    int         CountSelected();
    int         Purge();
    ZoneObject *Find(int fx, int fy);
    int         Render(Terminal *t);
    int         SetActive(int val);
    int         SetSelected(int val);

    int LayoutRows(Terminal *t, int x, int y, int w, int h, int gap = 0);
    int LayoutColumns(Terminal *t, int x, int y, int w, int h, int gap = 0);
    int LayoutGrid(Terminal *t, int x, int y, int w, int h, int gap = 0);
};

#endif
