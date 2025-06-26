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
 * exception.cc - revision 12 (8/25/98)
 * Implementation of exception module
 */

#include "exception.hh"
#include "check.hh"
#include "sales.hh"
#include "employee.hh"
#include "data_file.hh"
#include "report.hh"
#include "terminal.hh"
#include "archive.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** ItemException Class ****/
// Constructors
ItemException::ItemException()
{
    next = nullptr;
    fore = nullptr;
    user_id = 0;
    exception_type = 0;
    reason = -1;
    item_cost = 0;
    item_type = 0;
    item_family = 0;
}

ItemException::ItemException(Check *c, Order *o)
{
    next = nullptr;
    fore = nullptr;
    user_id = 0;
    exception_type = 0;
    reason = -1;
    item_name   = o->item_name;
    item_cost   = o->item_cost;
    item_type   = o->item_type;
    item_family = o->item_family;
    check_serial = c->serial_number;
}

// Member Functions
int ItemException::Read(InputDataFile &df, int version)
{
    int error = 0;

    error += df.Read(time);
    error += df.Read(user_id);
    error += df.Read(exception_type);
    error += df.Read(reason);
    error += df.Read(check_serial);
    error += df.Read(item_name);
    error += df.Read(item_cost);
    error += df.Read(item_family);

    return error;
}

int ItemException::Write(OutputDataFile &df, int version)
{
    int error = 0;
    error += df.Write(time);
    error += df.Write(user_id);
    error += df.Write(exception_type);
    error += df.Write(reason);
    error += df.Write(check_serial);
    error += df.Write(item_name);
    error += df.Write(item_cost);
    error += df.Write(item_family, 1);
    return error;
}


/**** TableException Class ****/
// Constructors
TableException::TableException()
{
    next = nullptr;
    fore = nullptr;
    user_id = 0;
    source_id = 0;
    target_id = 0;
    check_serial = 0;
}

TableException::TableException(Check *c)
{
    next = nullptr;
    fore = nullptr;
    user_id = 0;
    source_id = 0;
    target_id = 0;
    check_serial = c->serial_number;
    table = c->Table();
}

// Member Functions
int TableException::Read(InputDataFile &df, int version)
{
    int error = 0;
    error += df.Read(time);
    error += df.Read(user_id);
    error += df.Read(source_id);
    error += df.Read(target_id);
    error += df.Read(table);
    error += df.Read(check_serial);

    return error;
}

int TableException::Write(OutputDataFile &df, int version)
{
    int error = 0;
    error += df.Write(time);
    error += df.Write(user_id);
    error += df.Write(source_id);
    error += df.Write(target_id);
    error += df.Write(table);
    error += df.Write(check_serial, 1);
    return error;
}

/**** RebuildException Class ****/
// Constructor
RebuildException::RebuildException()
{
    next = nullptr;
    fore = nullptr;
    user_id = 0;
    check_serial = 0;
}

RebuildException::RebuildException(Check *c)
{
    next = nullptr;
    fore = nullptr;
    user_id = 0;
    check_serial = c->serial_number;
}

// Member Functions
int RebuildException::Read(InputDataFile &df, int version)
{
    int error = 0;

    error += df.Read(time);
    error += df.Read(user_id);
    error += df.Read(check_serial);

    return error;
}

int RebuildException::Write(OutputDataFile &df, int version)
{
    int error = 0;
    error += df.Write(time);
    error += df.Write(user_id);
    error += df.Write(check_serial, 1);
    return error;
}


/**** ExceptionDB Class ****/
// Constructor
ExceptionDB::ExceptionDB()
{
    archive = nullptr;
}

// Member Functions
int ExceptionDB::Load(const char* file)
{
    FnTrace("ExceptionDB::Load()");
    if (file)
        filename.Set(file);

    int version = 0;
    InputDataFile df;
    if (df.Open(filename.Value(), version))
        return 1;
    else
        return Read(df, version);
}

int ExceptionDB::Save()
{
    FnTrace("ExceptionDB::Save()");
    if (archive)
    {
        archive->changed = 1;
        return 0;
    }

    if (filename.empty())
        return 1;

    BackupFile(filename.Value());
    OutputDataFile df;
    if (df.Open(filename.Value(), EXCEPTION_VERSION))
        return 1;
    else
        return Write(df, EXCEPTION_VERSION);
}

int ExceptionDB::Read(InputDataFile &df, int version)
{
    // VERSION NOTES
    // 3 (8/22/97) earliest supported version

    if (version < 3 || version > 3)
        return 1;

    int count = 0;
    int error = df.Read(count);
    int i;

    for (i = 0; i < count; ++i)
    {
        ItemException *ie = new ItemException;
        ie->Read(df, version);
        Add(ie);
    }

    error += df.Read(count);
    for (i = 0; i < count; ++i)
    {
        TableException *te = new TableException;
        te->Read(df, version);
        Add(te);
    }

    error += df.Read(count);
    for (i = 0; i < count; ++i)
    {
        RebuildException *re = new RebuildException;
        re->Read(df, version);
        Add(re);
    }
    return error;
}

int ExceptionDB::Write(OutputDataFile &df, int version)
{
    if (version < 3)
        return 1;

    // Write Item Exceptions
    int error = df.Write(ItemCount(), 1);
    for (ItemException *ie = ItemList(); ie != nullptr; ie = ie->next)
        error += ie->Write(df, version);

    // Write Table Exceptions
    error += df.Write(TableCount(), 1);
    for (TableException *te = TableList(); te != nullptr; te = te->next)
        error += te->Write(df, version);

    // Write Rebuild Exceptions
    error += df.Write(RebuildCount(), 1);
    for (RebuildException *re = RebuildList(); re != nullptr; re = re->next)
        error += re->Write(df, version);
    return error;
}

int ExceptionDB::Add(ItemException *ie)
{
    return item_list.AddToTail(ie);
}

int ExceptionDB::Add(TableException *te)
{
    return table_list.AddToTail(te);
}

int ExceptionDB::Add(RebuildException *re)
{
    return rebuild_list.AddToTail(re);
}

int ExceptionDB::Remove(ItemException *ie)
{
    return item_list.Remove(ie);
}

int ExceptionDB::Remove(TableException *te)
{
    return table_list.Remove(te);
}

int ExceptionDB::Remove(RebuildException *re)
{
    return rebuild_list.Remove(re);
}

int ExceptionDB::Purge()
{
    item_list.Purge();
    table_list.Purge();
    rebuild_list.Purge();
    return 0;
}

int ExceptionDB::MoveTo(ExceptionDB *db)
{
    FnTrace("ExceptionDB::MoveTo()");

    while (item_list.Head())
    {
        ItemException *ie = item_list.Head();
        item_list.Remove(ie);
        db->Add(ie);
    }

    while (table_list.Head())
    {
        TableException *te = table_list.Head();
        table_list.Remove(te);
        db->Add(te);
    }

    while (rebuild_list.Head())
    {
        RebuildException *re = rebuild_list.Head();
        rebuild_list.Remove(re);
        db->Add(re);
    }
    return 0;
}

int ExceptionDB::AddItemException(Terminal *term, Check *thisCheck, Order *thisOrder, int type, int reason)
{
    FnTrace("ExceptionDB::AddItemException()");

    Employee *thisEmployee = term->user;
    if (thisOrder == nullptr || !(thisOrder->status & ORDER_FINAL) || thisEmployee == nullptr)
	{
        return 1; // exception ignored
	}

	// allocate space on the heap for exception structure
    ItemException *ie = new ItemException(thisCheck, thisOrder);
	// NOTE: need to implement check for failed allocation

	// set relevant properties for this exception
    ie->user_id = thisEmployee->id;
    ie->time = SystemTime;
    ie->exception_type = type;
    ie->reason = reason;

    Add(ie);  // Add pointer to item_list
    Save();		// dump changes to disk

    return 0;
}

int ExceptionDB::AddTableException(Terminal *t, Check *c, int target_id)
{
    FnTrace("ExceptionDB::AddTableException()");
    Employee *e = t->user;
    if (c == nullptr || c->IsEmpty() || c->IsTraining() || e == nullptr)
        return 1;  // exception ignored

    // Add table exception
    TableException *te = new TableException(c);
    te->user_id = e->id;
    te->time = SystemTime;
    te->source_id = c->user_owner;
    te->target_id = target_id;
    Add(te);
    Save();
    return 0;
}

int ExceptionDB::AddRebuildException(Terminal *t, Check *c)
{
    FnTrace("ExceptionDB::AddRebuildException()");
    Employee *e = t->user;
    if (c == nullptr || c->IsTraining() || e == nullptr)
        return 1;  // exception ignored

    // Add rebuild exception
    RebuildException *re = new RebuildException(c);
    re->user_id = e->id;
    re->time = SystemTime;
    Add(re);
    Save();
    return 0;
}
