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
 * exception.hh - revision 7 (8/10/98)
 * Record of voids/comps and other system changes
 */

#ifndef _EXCEPTION_HH
#define _EXCEPTION_HH

#include "utility.hh"
#include "list_utility.hh"


/**** Definitions ****/
#define EXCEPTION_VERSION  3

enum exceptions {
	EXCEPTION_COMP = 1,
	EXCEPTION_VOID,
	EXCEPTION_UNCOMP
};

/**** Types ****/
class Archive;
class Check;
class Order;
class Terminal;
class InputDataFile;
class OutputDataFile;

class ItemException
{
public:
    ItemException *next, *fore;
    TimeInfo time;

    Str      item_name;
    int      item_cost;
    int      user_id;
    int      check_serial;
    short    exception_type;
    short    reason;
    short    item_type;
    short    item_family;

    // Constructors
    ItemException();
    ItemException(Check *, Order *o);

    // Member Functions
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
};

class TableException
{
public:
    TableException *next, *fore;
    TimeInfo time;
    int user_id;
    int source_id, target_id;
    Str table;
    int check_serial;

    // Constructors
    TableException();
    TableException(Check *c);

    // Member Functions
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
};

class RebuildException
{
public:
    RebuildException *next, *fore;
    TimeInfo time;
    int user_id;
    int check_serial;

    // Constructor
    RebuildException();
    RebuildException(Check *c);

    // Member Functions
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);  
};

class ExceptionDB
{
    DList<ItemException>    item_list;
    DList<TableException>   table_list;
    DList<RebuildException> rebuild_list;

public:
    Archive *archive;
    Str filename;

    // Constructor
    ExceptionDB();

    // Member Functions
    ItemException *ItemList()        { return item_list.Head(); }
    int            ItemCount()       { return item_list.Count(); }

    TableException *TableList()      { return table_list.Head(); }
    int             TableCount()     { return table_list.Count(); }

    RebuildException *RebuildList()  { return rebuild_list.Head(); }
    int               RebuildCount() { return rebuild_list.Count(); }

    int Load(const char* file);
    int Save();
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
    int Add(ItemException *ie);
    int Add(TableException *te);
    int Add(RebuildException *re);
    int Remove(ItemException *ie);
    int Remove(TableException *te);
    int Remove(RebuildException *re);
    int Purge();
    int MoveTo(ExceptionDB *db);

    int AddItemException(Terminal *t, Check *c, Order *o, int type, int reason);
    int AddTableException(Terminal *t, Check *c, int target_id);
    int AddRebuildException(Terminal *t, Check *c);
};

#endif
