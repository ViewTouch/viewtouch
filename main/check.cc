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
 * check.cc - revision 286 (10/13/98)
 * Implementation of check management classes
 */

#include "check.hh"
#include "sales.hh"
#include "employee.hh"
#include "terminal.hh"
#include "printer.hh"
#include "report.hh"
#include "drawer.hh"
#include "data_file.hh"
#include "system.hh"
#include "settings.hh"
#include "inventory.hh"
#include "labels.hh"
#include "credit.hh"
#include "archive.hh"
#include "customer.hh"
#include "report_zone.hh"
#include "manager.hh"
#include "admission.hh"

#include <sys/types.h>
#include <dirent.h>
#include <sys/file.h>
#include <cstring>
#include <list>
#include <ctime>

#include <iostream>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** Module Data & Definitions ****/
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/**** Module Variables ****/
int last_check_serial = 0;  // Global serial number counter for checks
static const genericChar* EmptyStr = "";

const genericChar* CheckStatusName[] = { "Open", "Closed", "Voided", nullptr };
int CheckStatusValue[] = { CHECK_OPEN, CHECK_CLOSED, CHECK_VOIDED, -1 };

const genericChar* CheckDisplayName[] = {
    "Normal", "Compact", "Void Check", "Make Table Display", nullptr };
const int CheckDisplayValue[] = {
    CHECK_DISPLAY_ALONE, CHECK_DISPLAY_ORDER, CHECK_DISPLAY_KV1, CHECK_DISPLAY_KV2,
    CHECK_DISPLAY_KVALL, CHECK_DISPLAY_BV1, CHECK_DISPLAY_BV2, CHECK_DISPLAY_BVALL,
    CHECK_DISPLAY_SPLIT, CHECK_DISPLAY_TABLE, -1 };

int tender_order[] = {
    TENDER_CASH_AVAIL,
    TENDER_CHECK,
    TENDER_CREDIT_CARD,
    TENDER_DEBIT_CARD,
    TENDER_CHARGE_CARD,
    TENDER_GIFT,
    TENDER_ACCOUNT,
    TENDER_CHARGE_ROOM,
    TENDER_CAPTURED_TIP,
    TENDER_CHARGED_TIP,
    TENDER_PAID_TIP,
    TENDER_EXPENSE,
    TENDER_OVERAGE,
    TENDER_CHANGE,
    TENDER_COUPON,
    TENDER_DISCOUNT,
    TENDER_COMP,
    TENDER_EMPLOYEE_MEAL,
    -1 };

// ViewTouch used to store the check type in CustomerInfo.  We need to
// hold on to the type, but we want to convert from storing type in
// the customer record (because a customer might be a dine-in one day,
// a takeout the next, and a delivery the day after that, but it's only
// one customer, not three).  These defines are for legacy data files
// and should not be used elsewhere.  In other words, never type these
// in unless you know why you are re-using them.  The CHECK_ defines
// (e.g. CHECK_RESTAURANT) should be used instead.
#define CI_TAKEOUT     2
#define CI_DELIVERY    3
#define CI_HOTEL       4
#define CI_RETAIL      5
#define CI_FASTFOOD    6

#define WAITSTR        "PENDING"


/**** General Functions ****/
genericChar* SeatName(int seat, genericChar* str, int guests)
{
    FnTrace("SeatName()");
    static genericChar buffer[16];
    static constexpr size_t SEAT_NAME_BUFFER_SIZE = 16;
    if (str == nullptr)
        str = buffer;

    if (seat == -1)
    {
        strcpy(str, "ToGo");
    }
    else if (seat < -1)
    {
        snprintf(str, SEAT_NAME_BUFFER_SIZE, "%d", seat);
    }
    else if (seat < 26)
    {
        str[0] = 'A' + seat;
        str[1] = '\0';
    }
    else if (seat < 702)
    {
        str[0] = 'A' + (seat - 26) / 26;
        str[1] = 'A' + (seat % 26);
        str[2] = '\0';
    }
    else
    {
        str[0] = 'A' + ((seat - 702) / 676);
        str[1] = 'A' + (((seat - 26) /  26) % 26);
        str[2] = 'A' + (seat %  26);
        str[3] = '\0';
    }
    return str;
}



/**** Check Class ****/
// Constructors
Check::Check()
{
    FnTrace("Check::Check()");
    next          = nullptr;
    fore          = nullptr;
    serial_number = ++last_check_serial;
    flags         = 0;
    call_center_id = 0;
    current_sub   = nullptr;
    archive       = nullptr;
    customer      = nullptr;
    check_state   = 0;
    checknum      = 0;
    chef_time.Clear();
    made_time.Clear();
    check_in.Clear();
    check_out.Clear();
    date.Set();
    copy = 0;
    termname.Set("");
    comment.Set("");
    guests        = 0;
    has_takeouts  = 0;
    customer_id   = -1;
    undo          = 0;
}

Check::Check(Settings *settings, int customer_type, Employee *employee)
{
    FnTrace("Check::Check(Settings, int, Employee)");
    flags = 0;
    if (employee)
    {
        user_open    = employee->id;
        user_owner   = employee->id;
        user_current = employee->id;
        if (employee->training)
            flags |= CF_TRAINING;
    }
    else
    {
        user_open    = 0;
        user_owner   = 0;
        user_current = 0;
    }

    next          = nullptr;
    fore          = nullptr;
    serial_number = 0;
    call_center_id = 0;
    time_open     = SystemTime;
    type          = customer_type;
    current_sub   = nullptr;
    archive       = nullptr;
    check_state   = 0;
    chef_time.Clear();
    made_time.Clear();
    check_in.Clear();
    check_out.Clear();
    date.Set();
    checknum      = 0;
    copy = 0;
    termname.Set("");
    comment.Set("");
    guests        = 0;
    has_takeouts  = 0;
    undo          = 0;

    customer = NewCustomerInfo(customer_type);
    customer_id = customer->CustomerID();
}

// Destructor
Check::~Check()
{
    FnTrace("Check::~Check()");
    if (customer)
        customer = nullptr;
}

Check *Check::Copy(Settings *settings)
{
    FnTrace("Check::Copy()");

    Check *newcheck = new Check();
    if (newcheck == nullptr)
        return nullptr;

    newcheck->archive = archive;
    newcheck->current_sub = current_sub;
    newcheck->user_current = user_current;
    newcheck->serial_number = serial_number;
    newcheck->call_center_id = call_center_id;
    newcheck->time_open = time_open;
    newcheck->user_open = user_open;
    newcheck->user_owner = user_owner;
    newcheck->flags = flags;
    newcheck->type = type;
    newcheck->filename = filename;
    newcheck->check_state = check_state;
    newcheck->chef_time = chef_time;
    newcheck->made_time = made_time;
    newcheck->checknum = checknum;
    newcheck->copy = 1;
    newcheck->termname = termname;
    newcheck->guests = guests;
    newcheck->has_takeouts = has_takeouts;
    newcheck->undo = undo;  // Iffy.  Probably shouldn't do this.

    SubCheck *scheck = sub_list.Head();
    while (scheck != nullptr)
    {
        newcheck->Add(scheck->Copy(settings));
        scheck = scheck->next;
    }
    return newcheck;
}

// Member Functions
int Check::Load(Settings *settings, const genericChar* file)
{
    FnTrace("Check::Load()");
    filename.Set(file);

    int version = 0;
    InputDataFile df;
    if (df.Open(file, version))
        return 1;

    int error = Read(settings, df, version);
    return error;
}

//FIX BAK-->So Check::Save() calls System::SaveCheck() which calls Check::Write().
//FIX Maybe we can quit looping around like that and keep the process inside
//FIX check.cc
int Check::Save()
{
    FnTrace("Check::Save()");
    if (archive)
    {
        archive->changed = 1;
        return 0;
    }
    else if (copy == 0)
        return MasterSystem->SaveCheck(this);
    else
        return 0;
}

int Check::Read(Settings *settings, InputDataFile &infile, int version)
{
    FnTrace("Check::Read()");
    // VERSION NOTES
    // 7  (3/17/97)  earliest supported version
    // 8  (8/24/98)  drawer_id moved from SubCheck to Payment;
    //                 added customer data for check
    // 9  (10/6/98)  hotel customer data changed slightly
    // 10 (4/17/02)  added kitchen video (and Donato) information
    // 11 (7/23/02)  Fixed a problem with chef_time, made_time
    // 12 (1/7/03)   TakeOutInfo (CustomerInfo) needs to write all data
    // 13 (1/27/03)  Separate CustomerInfo from Check
    // 14 (2/20/03)  Store guest count in check rather than customer
    // 15 (3/14/03)  Use has_takeouts for takeouts attached to tables
    // 16 (3/31/03)  Read and Write label to maintain table views
    // 17 (6/26/03)  tax_exempt added to SubCheck
    // 18 (11/10/03) new_QST_method added to SubCheck
    // 19-23 are apparently subcheck version changes, or order or payment or something
    // 24 (08/26/05) added call_center_id to Check

    if (version < 7 || version > CHECK_VERSION)
    {
        genericChar str[32];
        snprintf(str, sizeof(str), "Unknown check version '%d'", version);
        ReportError(str);
        return 1;
    }

    int error = 0;
    error += infile.Read(serial_number);
    error += infile.Read(time_open);
    Str table;
    if (version <= 7)
        error += infile.Read(table);

    error += infile.Read(user_open);
    error += infile.Read(user_owner);

    if (version <= 7)
        error += infile.Read(guests);

    error += infile.Read(flags);
    if (version <= 7)
    {
        if (customer)
            delete customer;

        if (table.size() > 0)
        {
            customer = NewCustomerInfo(CHECK_RESTAURANT);
            customer->Guests(guests);
            Table(table.Value());
            type = CHECK_RESTAURANT;
        }
        else
        {
            customer = NewCustomerInfo(CHECK_TAKEOUT);
            type = CHECK_TAKEOUT;
        }
    }
    else if (version <= 12)
    {
        int customer_type = 0;
        error += infile.Read(customer_type);

        customer = NewCustomerInfo(customer_type);
        if (customer != nullptr)
            customer->Read(infile, version);

        type = CHECK_RESTAURANT;
        if (customer_type == CI_TAKEOUT)
            type = CHECK_TAKEOUT;
        else if (customer_type == CI_DELIVERY)
            type = CHECK_DELIVERY;
        else if (customer_type == CI_HOTEL)
            type = CHECK_HOTEL;
        else if (customer_type == CI_RETAIL)
            type = CHECK_RETAIL;
        else if (customer_type == CI_FASTFOOD)
            type = CHECK_FASTFOOD;
    }
    else  // version > 12
    {
        error += infile.Read(type);
        error += infile.Read(customer_id);
        customer = MasterSystem->customer_db.FindByID(customer_id);
        if (customer == nullptr)
            customer_id = -1;
    }

    if (version >= 10)
    {
        if ((version >= 11) || (ReadFix(infile, version) == 0))
        {
            error += infile.Read(check_state);
            error += infile.Read(chef_time);
            error += infile.Read(made_time);
            error += infile.Read(checknum);
        }
    }

    if (version >= 13)
    {
        error += infile.Read(check_in);
        error += infile.Read(check_out);
        error += infile.Read(date);
        error += infile.Read(comment);
    }

    if (version >= 14)
        error += infile.Read(guests);
    if (version >= 15)
        error += infile.Read(has_takeouts);
    if (version >= 16)
        error += infile.Read(label);
    if (version >= 24)
        error += infile.Read(call_center_id);

    int numsubchecks = 0;
    error += infile.Read(numsubchecks);
    if (error)
    {
        ReportError("Error in reading general check data");
        printf("Error in reading general check data");
        return error;
    }

    if (numsubchecks < 10000 && error == 0)  // sanity check
    {
        int i;
        for (i = 0; i < numsubchecks; ++i)
        {
            if (infile.end_of_file)
            {
                ReportError("Unexpected end of SubChecks in Check");
                return 1;
            }
            
            SubCheck *sc = new SubCheck();
            error += sc->Read(settings, infile, version);
            if (error)
            {
                delete sc;
                return error;
            }
            
            sc->check_type = type;
            Add(sc);
        }
    }

    return error;
}

/****
 * ReadFix: The first version 10 had chef_time and made_time as ints.
 *  They are now TimeInfos, but nothing was updated to handle the extra
 *  token written, so any archives created between the two changes cannot
 *  be read by the latest code.  So we have to fix that.  We'll peek
 *  ahead and see how many tokens we have before the next newline.
 *  If we only have 5, then we have the old data style.  If we have 7
 *  we have the new data style, so we'll just return 0 and let the
 *  parent function read it.
 ****/
int Check::ReadFix(InputDataFile &datFile, int version)
{
    FnTrace("Check::ReadFix()");
    int retval = 0;
    int tokens;
    int chef;
    int made;

    tokens = datFile.PeekTokens();
    if (tokens == 4)
    {
        // read 4 tokens and convert the integers to TimeInfos
        datFile.Read(check_state);
        datFile.Read(chef);
        datFile.Read(made);
        datFile.Read(checknum);
        chef_time.Set();
        made_time.Set();
        retval = 1;
    }
    return retval;
}

int Check::Write(OutputDataFile &datFile, int version)
{
    FnTrace("Check::Write()");
    if (copy == 1)
        return 1;

    if (version < 7)
    {
        genericChar str[64];
        snprintf(str, sizeof(str), "Invalid check version '%d' for saving", version);
        ReportError(str);
        return 1;
    }

    // Write version 7-9
    int error = 0;
    error += datFile.Write(serial_number);
    error += datFile.Write(time_open);

    if (version <= 7)
        error += datFile.Write(Table());

    error += datFile.Write(user_open);
    error += datFile.Write(user_owner);

    if (version <= 7)
        error += datFile.Write(Guests());

    error += datFile.Write(flags, 1);

    if (version >= 13)
    {
        error += datFile.Write(type);
        // verify customer actually exists in the database and that it is
        // not blank (blanks won't get saved)
        customer = MasterSystem->customer_db.FindByID(customer_id);
        if (customer == nullptr || customer->IsBlank() || customer->IsTraining())
            customer_id = -1;
        else
            MasterSystem->customer_db.Save(customer);
        error += datFile.Write(customer_id);
    }
    else if (version >= 8)
    {
        error += datFile.Write(type);
        error += customer->Write(datFile, version);
    }

    // Write Version 10 stuff
    error += datFile.Write(check_state);
    error += datFile.Write(chef_time);
    error += datFile.Write(made_time);
    error += datFile.Write(checknum);

    // Write Version 13 stuff
    error += datFile.Write(check_in);
    error += datFile.Write(check_out);
    error += datFile.Write(date);
    error += datFile.Write(comment);

    // Write Version 14 stuff
    error += datFile.Write(guests);

    // Write Version 15 stuff
    error += datFile.Write(has_takeouts);

    // Write Version 16 stuff
    error += datFile.Write(label);

    // Write Version 19 stuff
    error += datFile.Write(call_center_id);

    error += datFile.Write(SubCount(), 1);
    for (SubCheck *sc = SubList(); sc != nullptr; sc = sc->next)
    {
        error += sc->Write(datFile, version);
    }

    return error;
}

int Check::Add(SubCheck *sc)
{
    FnTrace("Check::Add(SubCheck)");
    if (sc == nullptr)
        return 1;

    if (SubListEnd())
        sc->number = SubListEnd()->number + 1;
    else
        sc->number = 1;
        
    sc->check_type = type;

    return sub_list.AddToTail(sc);
}

int Check::Remove(SubCheck *sc)
{
    FnTrace("Check::Remove()");
    return sub_list.Remove(sc);
}

int Check::Purge()
{
    FnTrace("Check::Purge()");
    sub_list.Purge();
    return 0;
}

int Check::Count()
{
    FnTrace("Check::Count()");
    int count = 1;
    for (Check *c = next; c != nullptr; c = c->next)
        ++count;
    return count;
}

int Check::DestroyFile()
{
    FnTrace("Check::DestroyFile()");
    if (filename.empty())
        return 1; // no file to destroy

    int result = DeleteFile(filename.Value());
    if (result)
        ReportError("Error in deleting check");

    filename.Clear();
    return result;
}

SubCheck *Check::NewSubCheck()
{
    FnTrace("Check::NewSubCheck()");
    SubCheck *sc = new SubCheck();
    if (sc == nullptr)
    {
        ReportError("Can't create subcheck");
        return nullptr;
    }
    
    Add(sc);
    current_sub = sc;
    return sc;
}

int Check::CancelOrders(Settings *settings)
{
    FnTrace("Check::CancelOrders()");
    for (SubCheck *sc = SubList(); sc != nullptr; sc = sc->next)
        sc->CancelOrders(settings);
    return Update(settings);
}

int Check::SendWorkOrder(Terminal *term, int printer_target, int reprint)
{
    FnTrace("Check::SendWorkOrder()");
    Report *report = new Report();
    if (report == nullptr)
    	return 1;

    Printer *printer = term->FindPrinter(printer_target);
    int retval = PrintWorkOrder(term, report, printer_target, reprint, nullptr, printer);
    if (report->is_complete && printer != nullptr && retval == 0)
        retval = report->FormalPrint(printer);

    // send to all other printers of this type 
    // (e.g. to allow multiple KITCHEN1 printers)
    Settings *settings = term->GetSettings();
    for (PrinterInfo *pi = settings->PrinterList(); pi; pi=pi->next)
    {
    	if (pi->type == printer_target)
	{
	    Printer *altprinter = pi->FindPrinter(term->parent);
	    if (altprinter && altprinter != printer)
	    {
    		retval = PrintWorkOrder(term, report, printer_target, reprint, 
				nullptr, altprinter);
    		if (report->is_complete && retval == 0)
        		retval = report->FormalPrint(altprinter);
	    }
    	}
    }

    return retval;
}

/****
 * SetOrderStatus:  Sets the status for all orders within the subcheck
 *  and returns 1 if a status on any order has changed, 0 otherwise.
 ****/
int Check::SetOrderStatus(SubCheck *subCheck, int set_status)
{
    FnTrace("Check::SetOrderStatus()");
    int change = 0;
    int all_subchecks = 0;

    if (subCheck == nullptr)
    {
        subCheck = SubList();
        all_subchecks = 1;
    }

    while (subCheck != nullptr)
    {
        Order *thisOrder = subCheck->OrderList();
        while (thisOrder != nullptr)
        {
            if (! (thisOrder->status & set_status))
            {
                change = 1;
                thisOrder->status |= set_status;
            }
    
            Order *mod = thisOrder->modifier_list;
            while (mod != nullptr)
            {
                if (!(mod->status & set_status))
                {
                    change = 1;
                    mod->status |= set_status;
                }
                mod = mod->next;
            }
            thisOrder = thisOrder->next;
        }

        if (all_subchecks == 1)
            subCheck = subCheck->next;
        else
            subCheck = nullptr;
    }

    return change;
}

/****
 * ClearOrderStatus:  Clears the status from all orders within the subcheck
 *  and returns 1 if a status on any order has changed, 0 otherwise.
 ****/
int Check::ClearOrderStatus(SubCheck *subCheck, int clear_status)
{
    FnTrace("Check::ClearOrderStatus()");
    int change = 0;
    int all_subchecks = 0;

    if (subCheck == nullptr)
    {
        subCheck = SubList();
        all_subchecks = 1;
    }

    while (subCheck != nullptr)
    {
        Order *thisOrder = subCheck->OrderList();
        while (thisOrder != nullptr)
        {
            if (thisOrder->status & clear_status)
            {
                change = 1;
                thisOrder->status &= ~clear_status;
            }
    
            Order *mod = thisOrder->modifier_list;
            while (mod != nullptr)
            {
                if (mod->status & clear_status)
                {
                    change = 1;
                    mod->status &= ~clear_status;
                }
                mod = mod->next;
            }
            thisOrder = thisOrder->next;
        }

        if (all_subchecks == 1)
            subCheck = subCheck->next;
        else
            subCheck = nullptr;
    }

    return change;
}

int Check::FinalizeOrders(Terminal *term, int reprint)
{
    FnTrace("Check::FinalizeOrders()");
    Settings *settings = &MasterSystem->settings;
    SubCheck *subCheck = SubList();
    Printer *printer = nullptr;
    int change = 0;
    int result = 0;

    while (subCheck != nullptr)
    {
        subCheck->ConsolidateOrders(settings);
        subCheck->FinalizeOrders();
        subCheck = subCheck->next;
    }

    if (term != nullptr)
    {
        // At this point, the report is just a throwaway report.
	if (term->print_workorder)
	{
            result += SendWorkOrder(term, PRINTER_KITCHEN1, reprint);
            result += SendWorkOrder(term, PRINTER_KITCHEN2, reprint);
            result += SendWorkOrder(term, PRINTER_KITCHEN3, reprint);
            result += SendWorkOrder(term, PRINTER_KITCHEN4, reprint);
            result += SendWorkOrder(term, PRINTER_BAR1, reprint);
            result += SendWorkOrder(term, PRINTER_BAR2, reprint);
            result += SendWorkOrder(term, PRINTER_EXPEDITER, reprint);
	}
        if (result < 7)
            flags |= CF_PRINTED;
    
        printer = term->FindPrinter(PRINTER_RECEIPT);
        subCheck = SubList();
        while (subCheck != nullptr)
        {
            change = SetOrderStatus(subCheck, ORDER_SENT);
            subCheck->ConsolidateOrders();
            if (change && (settings->receipt_print & RECEIPT_SEND))
                subCheck->PrintReceipt(term, this, printer);
    
            subCheck = subCheck->next;
        }
    }
    else
    {
        while (subCheck != nullptr)
        {
            SetOrderStatus(subCheck, ORDER_SENT);
            subCheck = subCheck->next;
        }
    }
    check_state = ORDER_FINAL;

    if (!chef_time.IsSet())
        chef_time.Set();  //check is sent to kitchen on close

    return 0;
}

int Check::Settle(Terminal *term)
{
    FnTrace("Check::Settle()");

    Employee *e = term->user;
    if (e == nullptr)
        return 1;

    Drawer *d = nullptr;
    if (! IsTraining())
    {
        d = term->FindDrawer();
        if (d == nullptr)
            return 1;
    }

    int settled = 0;
    for (SubCheck *sc = SubList(); sc != nullptr; sc = sc->next)
	{
        if (sc->Settle(term) == 0)
        {
            ++settled;
            if (d)
                d->RecordSale(sc);
        }
	}

    if (settled == 0)
        return 1;

    return 0;
}

int Check::Close(Terminal *term)
{
    FnTrace("Check::Close()");
    Employee *employee = term->user;
    if (employee == nullptr)
        return 1;

    Drawer *d = nullptr;
    if (!IsTraining())
    {
        d = term->FindDrawer();
        if (d == nullptr)
            return 1;
    }

    int closed = 0;
    for (SubCheck *sc = SubList(); sc != nullptr; sc = sc->next)
    {
        if (sc->Close(term) == 0)
        {
            ++closed;
            if (d)
                d->RecordSale(sc);
        }
    }

    if (closed == 0)
        return 1;

    if (!IsTraining())
    {
        term->system_data->inventory.MakeOrder(this);
        Save();
    }
    if (!chef_time.IsSet())
        chef_time.Set();  //check is sent to kitchen on close
    return 0;
}

int Check::Update(Settings *settings)
{
    FnTrace("Check::Update()");
    int number = 0;
    SubCheck *sc = SubList();
    while (sc)
    {
        SubCheck *ptr = sc->next;
        sc->ConsolidateOrders();
        if (sc->OrderList() == nullptr && sc->PaymentList() == nullptr)
        {
            if (current_sub == sc)
            {
                if (sc->next)
                    current_sub = sc->next;
                else
                    current_sub = sc->fore;
            }
            Remove(sc);
            delete sc;
        }
        else
        {
            sc->number = ++number;
            sc->FigureTotals(settings);
        }
        sc = ptr;
    }
    return 0;
}

int Check::Status()
{
    FnTrace("Check::Status()");
    int total_open = 0;
    int total_closed = 0;
    int total_voided = 0;

    for (SubCheck *sc = SubList(); sc != nullptr; sc = sc->next)
    {
        switch (sc->status)
        {
        case CHECK_OPEN:
            ++total_open;
            break;
        case CHECK_CLOSED:
            ++total_closed;
            break;
        case CHECK_VOIDED:
            ++total_voided;
            break;
        }
    }

    if (total_open <= 0)
    {
        if (total_closed > 0)
            return CHECK_CLOSED;
        else if (total_voided > 0)
            return CHECK_VOIDED;
    }
    return CHECK_OPEN;
}

const genericChar* Check::StatusString(Terminal *term)
{
    FnTrace("Check::StatusString()");
    const genericChar* s =
        FindStringByValue(Status(), CheckStatusValue, CheckStatusName, UnknownStr);
    return term->Translate(s);
}

int Check::MoveOrdersBySeat(SubCheck *sc1, SubCheck *sc2, int seat)
{
    FnTrace("Check::MoveOrdersBySeat()");
    if (sc1 == nullptr || sc2 == nullptr || sc1 == sc2)
        return 1;

    Order *order = sc1->OrderList();
    while (order)
    {
        Order *ptr = order->next;
        if (order->seat == seat)
        {
            sc1->Remove(order);
            sc2->Add(order);
        }
        order = ptr;
    }
    return 0;
}

int Check::MergeOpenChecks(Settings *settings)
{
    FnTrace("Check::MergeOpenChecks()");
    SubCheck *main = FirstOpenSubCheck();
    if (main == nullptr)
        return 1;

    for (SubCheck *sc = main->next; sc != nullptr; sc = sc->next)
    {
        if (sc->status == CHECK_OPEN)
        {
            while (sc->OrderList())
            {
                Order *order = sc->OrderList();
                sc->Remove(order);
                main->Add(order);
            }
        }
    }

    current_sub = FirstOpenSubCheck();
    return Update(settings);
}

int Check::SplitCheckBySeat(Settings *settings)
{
    FnTrace("Check::SplitCheckBySeat()");
    if (MergeOpenChecks(settings))
        return 1;

    SubCheck *main = FirstOpenSubCheck();
    if (main == nullptr)
        return 1;

    for (int i = 1; i < Guests(); ++i)
    {
        SubCheck *sc = NewSubCheck();
        MoveOrdersBySeat(main, sc, i);
    }
    current_sub = FirstOpenSubCheck();
    return Update(settings);
}

int Check::MergeWithCheck(Check *other_check, Settings *settings)
{
    FnTrace("Check::MergeWithCheck()");
    if (other_check == nullptr || other_check == this)
        return 1;

    // Merge all subchecks from the other check into this check
    SubCheck *sc = other_check->SubList();
    while (sc != nullptr)
    {
        SubCheck *next_sc = sc->next;
        
        // Remove the subcheck from the other check
        other_check->Remove(sc);
        
        // Add it to this check
        Add(sc);
        
        sc = next_sc;
    }

    // Update guest count to be the sum of both checks
    int total_guests = Guests() + other_check->Guests();
    Guests(total_guests);

    // Update the check
    Update(settings);
    
    // Set current subcheck to the first open one
    current_sub = FirstOpenSubCheck();
    
    return 0;
}

int Check::PrintCount(Terminal *term, int printer_id, int reprint, int flag_sent)
{
    FnTrace("Check::PrintCount()");
    int count = 0;
    for (SubCheck *sc = SubList(); sc != nullptr; sc = sc->next)
    {
        for (Order *order = sc->OrderList(); order != nullptr; order = order->next)
        {
            if (order->PrintStatus(term, printer_id, reprint, flag_sent))
                ++count;
        }
    }

    return count;
}

/****
 * PrintWorkOrder:  There are different formats for the work order
 *  depending on the value of settings->mod_separator:
 *
 * MOD_SEPARATE_NL:  separate modifiers by newlines
 *  [->][COUNT] ORDER
 *     [MOD1]
 *     ...
 *     [MODn]
 *
 * MOD_SEPARATE_CM:  separate modifiers by commas
 *  [->][GUEST] [COUNT] ORDER [[MOD1] .. [MODn]]
 ****/
int Check::PrintWorkOrder(Terminal *term, Report *report, int printer_id, int reprint,
                          ReportZone *rzone, Printer *printer)
{
    FnTrace("Check::PrintWorkOrder()");
    int flag_sent = ORDER_SENT;
    int flag_printed = CF_PRINTED;
    int color = COLOR_DEFAULT;
    bool full_hdr = true;

    if (rzone != nullptr)				// kitchen video, not printer
    {
        flag_sent = ORDER_SHOWN;
        flag_printed = CF_SHOWN;
        if (rzone == term->active_zone) {	// highlighted for bump bar
	    color = ((Zone *)rzone)->page->default_color[1];
	    if (color == COLOR_DEFAULT)
	        color = term->zone_db->default_color[1];
	}
        if (term->workorder_heading)		// use shorter/simpler heading?
	    full_hdr = false;
    }

    if (report == nullptr && printer == nullptr)
    {
        ReportError("No Printer Available For Work Order and No Report");
        return 1;
    }

    if (PrintCount(term, printer_id, reprint, flag_sent) <= 0)
        return 1;

    report->Clear();
    report->SetTitle("WorkOrder");
    System *sys = term->system_data;
    Settings *settings = &(sys->settings);
    Employee *employee = sys->user_db.FindByID(user_owner);
    genericChar str[STRLENGTH];
    genericChar str1[STRLENGTH];
    genericChar str2[STRLENGTH];
    genericChar cststr[STRLENGTH];
    genericChar ordstr[STRLENGTH];
    genericChar tmpstr[STRLENGTH];
    int pwidth = 80;
    int kitchen_mode = 0;
    if (rzone != nullptr)
    {
        pwidth = rzone->Width(term);
    }
    else if (printer != nullptr)
    {
        kitchen_mode = printer->KitchenMode();
        pwidth = printer->Width(kitchen_mode);
    }
    pwidth = MIN(pwidth, 256);
    TimeInfo now;
    now.Set();
    int firstmod = 0;

    // printer header margin
    if (rzone == nullptr)
    {
        report->Mode(PRINT_LARGE);
        report->TextL(" ");
        if (printer)
        {
            report->NewLine(printer->order_margin);
        } else
        {
            report->NewLine();
        }
    }

    if (employee && employee->training)
    {
        report->TextL(" ", COLOR_RED);
        report->Underline(pwidth, COLOR_DEFAULT, ALIGN_CENTER, 0.0);
        report->NewLine();
        report->Mode(PRINT_LARGE);
        report->TextL(" ** TRAINING **", COLOR_RED);
        report->Mode(0);
        report->NewLine();
        report->TextL("Do NOT Prepare This Order", COLOR_RED);
        report->NewLine();
        report->TextL(" ", COLOR_RED);
        report->Underline(pwidth, COLOR_DEFAULT, ALIGN_CENTER, 0.0);
        report->NewLine();
    }
    else if (full_hdr && (CustomerType() == CHECK_TAKEOUT  ||
             CustomerType() == CHECK_DELIVERY ||
             CustomerType() == CHECK_CATERING ||
             CustomerType() == CHECK_DINEIN   ||
             CustomerType() == CHECK_TOGO     ||
             CustomerType() == CHECK_CALLIN))
    {
        if (rzone == nullptr)
	{
            report->Mode(0);
            report->TextL(" ");
            report->Underline(pwidth, color, ALIGN_CENTER, 0.0);
            report->NewLine();
	}

	// ** order type **
        str[0] = '\0';
        switch(CustomerType())
        {
        case CHECK_TAKEOUT:
            if (!date.IsSet() || (date <= now))
            {
                strcpy(str, term->Translate(WAITSTR));
                strncat(str, " ", sizeof(str) - strlen(str) - 1);
            }
            strncat(str, term->Translate("Take Out"), sizeof(str) - strlen(str) - 1);
            break;
        case CHECK_DELIVERY:
            if (!date.IsSet() || (date <= now))
            {
                strcpy(str, term->Translate(WAITSTR));
                strncat(str, " ", sizeof(str) - strlen(str) - 1);
            }
            strncat(str, term->Translate("Delivery"), sizeof(str) - strlen(str) - 1);
            break;
        case CHECK_CATERING:
            if (!date.IsSet() || (date <= now))
            {
                strcpy(str, term->Translate(WAITSTR));
                strncat(str, " ", sizeof(str) - strlen(str) - 1);
            }
            strncat(str, term->Translate("Catering"), sizeof(str) - strlen(str) - 1);
            break;
        case CHECK_DINEIN:
            if (!date.IsSet() || (date <= now))
            {
                strcpy(str, term->Translate(WAITSTR));
                strncat(str, " ", sizeof(str) - strlen(str) - 1);
            }
            strncat(str, "Here", sizeof(str) - strlen(str) - 1);
            break;
        case CHECK_TOGO:
            if (!date.IsSet() || (date <= now))
            {
                strcpy(str, term->Translate(WAITSTR));
                strncat(str, " ", sizeof(str) - strlen(str) - 1);
            }
            strncat(str, "To Go", sizeof(str) - strlen(str) - 1);
            break;
        case CHECK_CALLIN:
            if (!date.IsSet() || (date <= now))
            {
                strcpy(str, term->Translate(WAITSTR));
                strncat(str, " ", sizeof(str) - strlen(str) - 1);
            }
            strncat(str, "Pick Up", sizeof(str) - strlen(str) - 1);
            break;
        }
        snprintf(str1, pwidth, "%s", str);
        report->Mode(PRINT_LARGE);
        report->TextL(str1, color);
        report->NewLine();

	// order due time
        snprintf(str1, pwidth, "%s", term->TimeDate(date, TD_DATETIME));
        report->TextL(str1, color);
        report->Mode(0);
        report->NewLine();
    }

    // order routing
    switch (CustomerType())
	{
    case CHECK_RESTAURANT:
        snprintf(str1, sizeof(str1), "%s %s-%d", term->Translate("Table"), Table(), Guests());
        break;

    case CHECK_HOTEL:
        snprintf(str1, sizeof(str1), "%s %s", term->Translate("Room"), Table());
        break;

    case CHECK_TAKEOUT:
        strcpy(str1, term->Translate("TO GO"));
        break;

    case CHECK_FASTFOOD:
        strcpy(str1, term->Translate("Fast"));
        break;

    case CHECK_DELIVERY:
        strcpy(str1, term->Translate("Deliver"));
        break;

    case CHECK_RETAIL:
        strcpy(str1, term->Translate("Retail"));
        break;

    case CHECK_DINEIN:
        strcpy(str1, "Here");
        break;

    case CHECK_TOGO:
        strcpy(str1, "To Go");
        break;

    case CHECK_CALLIN:
        strcpy(str1, "Pick Up");
        break;

    default:
	str1[0] = 0;
	}
    if (full_hdr && str1[0])	// order type on separate line
    {
        report->Mode(kitchen_mode);
        report->TextL(str1, color);
        report->NewLine();
    }

    // flags, order number, type, elapsed time
    if (reprint)
    {
        strcpy(str, "REPRINT ");
        flags |= CF_REPRINT & CF_PRINTED;
    }
    else if (flags & flag_printed)
        strcpy(str, "Restored ");
    else
    	str[0] = 0;
    snprintf(str2, sizeof(str2), "#%d ", serial_number % 10000);	// max 4 digits
    strncat(str, str2, sizeof(str) - strlen(str) - 1);
    if (!full_hdr && str1[0])	// combine order type on this line
        strncat(str, str1, sizeof(str) - strlen(str) - 1);
    report->Mode(kitchen_mode);
    // green if paid
    report->TextL(str, Status() == CHECK_CLOSED ? COLOR_DK_GREEN : color);

    // Show when it was made or the elapsed time that it's been in the kitchen
    if (rzone != nullptr)
    {
        if (undo == 0)
        {
            StringElapsedToNow(str, 256, chef_time);
            report->TextR(str);
        }
        else
            report->TextR(term->TimeDate(made_time, TD_TIME));
    }
    report->NewLine();

    // order source and creation timestamp
    term->TimeDate(str, time_open, TD_NO_YEAR | TD_SHORT_MONTH | TD_NO_DAY | TD_SHORT_TIME);
    snprintf(str1, sizeof(str1), "%*s", pwidth, str);	// pad to right justify, with space for underline

    if (employee)
        strcpy(str, employee->system_name.Value());
    else if (call_center_id > 0)
        strcpy(str, "Call Center");
    else
        strcpy(str, UnknownStr);

    if (rzone)		// video
    {
     	report->Underline(pwidth, COLOR_DEFAULT, ALIGN_LEFT, 0.0);
    	report->TextL(str, color);
	report->TextR(str1, COLOR_DK_BLUE);
    }
    else		// printer - timestamp on separate line
    {
	report->Mode(kitchen_mode);
    	report->TextL(str, color);
    	report->NewLine();
	report->Mode(kitchen_mode | PRINT_UNDERLINE);
    	report->TextR(str1, COLOR_DK_BLUE);
    }
    report->NewLine();

    if (settings->kv_show_user)
        PrintCustomerInfoReport(report, kitchen_mode);

    // Now list the orders
    for (SubCheck *sc = SubList(); sc != nullptr; sc = sc->next)
	{
        for (Order *order = sc->OrderList(); order != nullptr; order = order->next)
        {
            ordstr[0] = '\0';
            cststr[0] = '\0';
            int pval = order->PrintStatus(term, printer_id, reprint, flag_sent);
            if (pval > 0)
            {
                // get customer seat if available
                if (CustomerType() == CHECK_RESTAURANT)
                    order->Seat(settings, cststr);
                else
                    cststr[0] = '\0';
                if (settings->mod_separator == MOD_SEPARATE_CM)
                    snprintf(ordstr, sizeof(ordstr), "%s  ", cststr);

                // get the description 
                order->PrintDescription(str2);
                if (pval > 1)
                    strncat(ordstr, "->", sizeof(ordstr) - strlen(ordstr) - 1);
                if (order->item_type == ITEM_POUND)
                    snprintf(str1, sizeof(str1), "%.2f %s", ((Flt) order->count / 100.0), str2);
                else if (order->count >= 1)
                    snprintf(str1, sizeof(str1), "%d %s", order->count, str2);
                else
                    snprintf(str1, sizeof(str1), "%s", str2);
                strncat(ordstr, str1, sizeof(ordstr) - strlen(ordstr) - 1);
                report->Mode(kitchen_mode);
                report->TextL(ordstr, COLOR_DEFAULT);
                report->TextR(cststr, COLOR_DEFAULT);
                report->Mode(0);
                report->NewLine();

                ordstr[0] = '\0';
                str2[0] = '\0';
                firstmod = 1;

                // Original code had two identical for loop declarations which caused:
                // - "warning: declaration of 'mod' shadows a previous local"
                // - "error: qualified-id in declaration before '(' token" for subsequent methods
                // - Parser confusion that prevented proper method boundary recognition
                for (Order *mod = order->modifier_list; mod != nullptr; mod = mod->next)
                {
                    if (mod->PrintStatus(term, printer_id, reprint, flag_sent) > 0)
                    {
                        mod->PrintDescription(str1);
                        if (pval > 1)
                            snprintf(str2, sizeof(str2), "-> %s", str1);
                        else
                            snprintf(str2, sizeof(str2), "  %s", str1);
                        if (settings->mod_separator == MOD_SEPARATE_NL)
                        {
                            // write out the line for newline mode
                            report->Mode(kitchen_mode);
                            report->TextL(str2, COLOR_RED);
                            report->TextR("", COLOR_RED);
                            report->Mode(0);
                            report->NewLine();
                        }
                        else
                        {
                            strcpy(tmpstr, ordstr);  // save a backup copy
                            if (firstmod == 0)
                                strncat(ordstr, ",", sizeof(ordstr) - strlen(ordstr) - 1);
                            firstmod = 0;
                            strncat(ordstr, str2, sizeof(ordstr) - strlen(ordstr) - 1);
                            if (strlen(ordstr) >= ((unsigned int)(pwidth - 1)))
                            {
                                strncat(tmpstr, ",", sizeof(tmpstr) - strlen(tmpstr) - 1);
                                report->Mode(kitchen_mode);
                                report->TextL(tmpstr, COLOR_RED);
                                report->TextR("", COLOR_RED);
                                report->Mode(0);
                                report->NewLine();
                                ordstr[0] = '\0';
                                strncat(ordstr, "  ", sizeof(ordstr) - strlen(ordstr) - 1);
                                strncat(ordstr, str2, sizeof(ordstr) - strlen(ordstr) - 1);
                            }
                        }
                    }
                }
                // write out the line for comma delimited mode
                if (settings->mod_separator == MOD_SEPARATE_CM && ordstr[0] != '\0')
                {
                    report->Mode(kitchen_mode);
                    report->TextL(ordstr, COLOR_RED);
                    report->TextR("", COLOR_RED);
                    report->Mode(0);
                    report->NewLine();
                }
            }
        }
	}

    report->is_complete = 1;

    return 0;
}

int Check::PrintDeliveryOrder(Report *report, int pwidth)
{
    FnTrace("Check::PrintDeliveryOrder()");
    int       retval = 0;
    Settings *settings = &MasterSystem->settings;
    SubCheck *subcheck = nullptr;
    Order    *order = nullptr;
    Employee *employee = nullptr;
    char      ordstr[STRLONG] = "";
    char      str1[STRLONG] = "";
    char      str2[STRLONG] = "";
    int       firstmod = 0;
    int       idx = 0;
    int       sidx = 0;
    int       didx = 0;
    int       indent = 0;
    int       delivery_cost = 0;
    int       total_cost = 0;
    
    report->Clear();
    report->SetTitle("DeliveryOrder");
    report->SetPageWidth(pwidth);
    report->SetDividerChar('-');

    // Header:  store name, footer information (for "Thank You" or whatever)
    report->TextC2Col(settings->store_name.Value());
    report->NewLine();
    for (idx = 0; idx < MAX_HEADER_LINES; idx += 1)
    {
        if (settings->receipt_footer[idx].size() > 0)
        {
            sidx = 0;
            didx = 0;
            strcpy(str1, settings->receipt_footer[idx].Value());
            while (str1[sidx] == ' ')
                sidx += 1;
            while (str1[sidx] != '\0')
            {
                str2[didx] = str1[sidx];
                sidx += 1;
                didx += 1;
            }
            str2[didx] = '\0';
            report->TextC2Col(str2);
            report->NewLine();
        }
    }
    report->Divider2Col();

    // Customer information, including address
    snprintf(str1, STRLONG, "Phone:  %s", PhoneNumber());
    report->TextL2Col(str1);
    if (strlen(Extension()) > 0)
    {
        snprintf(str1, STRLONG, "Ext:  %s", Extension());
        report->TextPosL2Col(25, str1);
    }
    report->NewLine();
    snprintf(str1, STRLONG, "Name:  %s", FullName());
    report->TextL2Col(str1);
    report->NewLine();
    if (strlen(Address()) > 0)
    {
        report->TextL2Col(Address());
        report->NewLine();
    }
    if (strlen(Address2()) > 0)
    {
        report->TextL2Col(Address2());
        report->NewLine();
    }
    indent = 0;
    if (strlen(City()) > 0)
    {
        report->TextL2Col(City());
        indent += strlen(City()) + 1;
    }
    if (strlen(State()) > 0)
    {
        report->TextL2Col(State());
        indent += strlen(State()) + 1;
    }
    if (strlen(Postal()) > 0)
    {
        report->TextL2Col(Postal());
        indent += strlen(Postal()) + 1;
    }
    if (indent > 0)
        report->NewLine();
    report->Divider2Col();

    //Store information
    snprintf(str1, STRLONG, "Store:  %s", settings->StoreNum());
    report->TextL2Col(str1);
    employee = MasterSystem->user_db.FindByID(user_owner);
    if (employee != nullptr)
        snprintf(str1, STRLONG, "Op:  %s", employee->system_name.Value());
    else
        strcpy(str1, "Op:  callcenter");
    report->TextPosL2Col(20, str1);
    report->NewLine();
    snprintf(str1, STRLONG, "Date:  %s", time_open.Date().c_str());
    report->TextL2Col(str1);
    snprintf(str1, STRLONG, "Order #:  %d", call_center_id);
    report->TextPosL2Col(20, str1);
    report->NewLine();
    snprintf(str1, STRLONG, "Order Created:  %s", time_open.Time().c_str());
    report->TextL2Col(str1);
    report->NewLine();
    report->Divider2Col();

    // Now list the orders
    strcpy(str1, "Qty  Description");
    report->TextL2Col(str1);
    report->TextR2Col("Price");
    report->NewLine();
    subcheck = SubList();
    while (subcheck != nullptr)
    {
        order = subcheck->OrderList();
        while (order != nullptr)
        {
            ordstr[0] = '\0';

            // get the description 
            order->PrintDescription(str2, 1);
            if (order->item_type == ITEM_POUND)
                snprintf(str1, sizeof(str1), "%.2f    %s", ((Flt) order->count / 100.0), str2);
            else if (order->count >= 1)
                snprintf(str1, sizeof(str1), "%d    %s", order->count, str2);
            else
                snprintf(str1, sizeof(str1), "%s", str2);
            strncat(ordstr, str1, sizeof(ordstr) - strlen(ordstr) - 1);
            report->TextL2Col(ordstr);
            if (order->cost > 0)
            {
                PriceFormat(settings, order->cost, 0, 0, str1);
                report->TextR2Col(str1);
            }
            report->NewLine();

            ordstr[0] = '\0';
            str2[0] = '\0';
            firstmod = 1;
            for (Order *mod = order->modifier_list; mod != nullptr; mod = mod->next)
            {
                mod->PrintDescription(str1);
                report->TextPosL2Col(5, str1, COLOR_RED);
                report->NewLine();
            }
            order = order->next;
        }
        delivery_cost += subcheck->delivery_charge;
        total_cost += subcheck->total_cost;
        subcheck = subcheck->next;
	}
    report->Divider2Col();
    report->TextL2Col("SubTotal:");
    PriceFormat(settings, total_cost, 0, 0, str1);
    report->TextR2Col(str1);
    report->NewLine();
    report->TextL2Col("Delivery:");
    PriceFormat(settings, delivery_cost, 0, 0, str1);
    report->TextR2Col(str1);
    report->NewLine();

    // Show the absolute total
    total_cost += delivery_cost;
    report->Divider2Col('=');
    report->TextL2Col("Total Including Taxes:");
    PriceFormat(settings, total_cost, 0, 0, str1);
    report->TextR2Col(str1);
    report->NewLine();

    return retval;
}

int Check::PrintCustomerInfo(Printer *printer, int mode)
{
    FnTrace("Check::PrintCustomerInfo()");
    int custinfo = 0;
    char str[STRLONG];
    int retval = 0;

    if (customer)
    {
        // name
        str[0] = '\0';
        if (strlen(FirstName()) > 0)
            snprintf(str, sizeof(str), "%s %s", FirstName(), LastName());
        else if (strlen(LastName()) > 0)
            snprintf(str, sizeof(str), "%s", LastName());
        if (str[0] != '\0')
        {
            custinfo = 1;
            printer->Write(str, mode);
        }
        // phone
        if (strlen(PhoneNumber()) > 0)
        {
            custinfo = 1;
            printer->Write(PhoneNumber(), mode);
        }
        // address
        if (strlen(Address()) > 0)
        {
            custinfo = 1;
            printer->Write(Address(), mode);
        }
        // address2
        if (strlen(Address2()) > 0)
        {
            custinfo = 1;
            printer->Write(Address2(), mode);
        }
        // cross street
        if (strlen(CrossStreet()) > 0)
        {
            custinfo = 1;
            printer->Write(CrossStreet(), mode);
        }
        // city, state
        str[0] = '\0';
        if (strlen(City()) > 0)
            snprintf(str, sizeof(str), "%s  %s", City(), State());
        else if (strlen(State()) > 0)
            strcpy(str, State());
        if (str[0] != '\0')
        {
            custinfo = 1;
            printer->Write(str, mode);
        }
        if (custinfo)
            printer->NewLine();
    }

    return retval;
}

int Check::PrintCustomerInfoReport(Report *report, int mode, int columns, int pwidth)
{
    FnTrace("Check::PrintCustomerInfoReport()");
    int custinfo = 0;
    char str[STRLONG];
    int retval = 0;
    int column1 = 0;
    int column2 = (pwidth / 2) + 1;

    if (customer)
    {
        report->Mode(mode);
        // name
        str[0] = '\0';
        if (strlen(FirstName()) > 0)
            snprintf(str, sizeof(str), "Name:  %s %s", FirstName(), LastName());
        else if (strlen(LastName()) > 0)
            snprintf(str, sizeof(str), "Last Name:  %s", LastName());
        else if (strlen(FullName()) > 0)
            snprintf(str, sizeof(str), "Name:  %s", FullName());
        if (str[0] != '\0')
        {
            custinfo = 1;
            report->TextPosL(column1, str, COLOR_DEFAULT);
            if (columns > 1)
                report->TextPosL(column2, str, COLOR_DEFAULT);
            report->NewLine();
        }
        // phone
        if (strlen(PhoneNumber()) > 0)
        {
            custinfo = 1;
            snprintf(str, sizeof(str), "Phone:  %s", PhoneNumber());
            report->TextPosL(column1, str, COLOR_DEFAULT);
            if (columns > 1)
                report->TextPosL(column2, str, COLOR_DEFAULT);
            report->NewLine();
        }
        // address
        if (strlen(Address()) > 0)
        {
            custinfo = 1;
            snprintf(str, sizeof(str), "Street:  %s", Address());
            report->TextPosL(column1, str, COLOR_DEFAULT);
            if (columns > 1)
                report->TextPosL(column2, str, COLOR_DEFAULT);
            report->NewLine();
        }
        // address2
        if (strlen(Address2()) > 0)
        {
            custinfo = 1;
            snprintf(str, sizeof(str), "Address 2:  %s", Address2());
            report->TextPosL(column1, str, COLOR_DEFAULT);
            if (columns > 1)
                report->TextPosL(column2, str, COLOR_DEFAULT);
            report->NewLine();
        }
        // cross street
        if (strlen(CrossStreet()) > 0)
        {
            custinfo = 1;
            snprintf(str, sizeof(str), "Cross Street:  %s", CrossStreet());
            report->TextPosL(column1, str, COLOR_DEFAULT);
            if (columns > 1)
                report->TextPosL(column2, str, COLOR_DEFAULT);
            report->NewLine();
        }
        // city, state
        str[0] = '\0';
        if (strlen(City()) > 0 && strlen(State()) > 0)
            snprintf(str, sizeof(str), "City and State:  %s  %s", City(), State());
        else if (strlen(City()) > 0)
            snprintf(str, sizeof(str), "City:  %s", City());
        else if (strlen(State()) > 0)
            snprintf(str, sizeof(str), "State:  %s", State());
        if (str[0] != '\0')
        {
            custinfo = 1;
            report->TextPosL(column1, str, COLOR_DEFAULT);
            if (columns > 1)
                report->TextPosL(column2, str, COLOR_DEFAULT);
            report->NewLine();
        }

        if (custinfo)
            report->NewLine();
        report->Mode(0);
    }

    return retval;
}

int Check::MakeReport(Terminal *term, Report *report, int show_what, int video_target,
                      ReportZone *rzone)
{
    FnTrace("Check::MakeReport()");
    if (report == nullptr)
        return 1;

    System *sys = term->system_data;
    Settings *settings = &(sys->settings);
    int use_comma = 0;
    TimeInfo now;
    now.Set();

    if (settings->mod_separator == MOD_SEPARATE_CM &&
        video_target != PRINTER_DEFAULT)
    {
        use_comma = 1;
    }


    char str[STRLONG] = "";
    char str2[STRLONG] = "";
    report->update_flag = UPDATE_CHECKS | UPDATE_PAYMENTS | UPDATE_ORDERS;
    report->Mode(PRINT_BOLD);
    if (video_target == PRINTER_DEFAULT)
    {
        report->TextC(term->UserName(user_owner));
    }
    else
    {
        if ((check_state & ORDER_FINAL || check_state & ORDER_SENT) &&
            !chef_time.IsSet())
        {
            check_state |= ORDER_SENT;
            chef_time.Set();
            Save();
        }
        if (undo == 0)
        {
            StringElapsedToNow(str, 256, chef_time);
            report->TextL(str);
        }
        else
            report->TextL(term->TimeDate(made_time, TD_TIME));
        report->TextC(termname.Value());
        report->TextR(term->UserName(user_owner));
    }
    report->Mode(0);
    report->NewLine();

    switch (CustomerType())
    {
    case CHECK_RESTAURANT:
        snprintf(str, STRLONG, "%s: %d", term->Translate("Guests"), Guests());
        break;
    case CHECK_HOTEL:
        snprintf(str, STRLONG, "%s %s", term->Translate("Room"), Table());
        break;
    case CHECK_TAKEOUT:
	if (date.IsSet() && (date <= now))
	    snprintf(str, STRLONG, "%s %s", term->Translate(WAITSTR), term->Translate("Take Out"));
	else
	    snprintf(str, STRLONG, "%s",term->Translate("Take Out"));
        break;
    case CHECK_FASTFOOD:
        snprintf(str, STRLONG, "%s",term->Translate("Fast Food"));
        break;
    case CHECK_CATERING:
	if (date.IsSet() && (date <= now))
	    snprintf(str, STRLONG, "%s %s", term->Translate(WAITSTR), term->Translate("Catering"));
	else
	    snprintf(str, STRLONG, "%s",term->Translate("Catering"));
        break;
    case CHECK_DELIVERY:
	if (date.IsSet() && (date <= now))
	    snprintf(str, STRLONG, "%s %s", term->Translate(WAITSTR), term->Translate("Delivery"));
	else
	    snprintf(str, STRLONG, "%s",term->Translate("Delivery"));
        break;
    case CHECK_RETAIL:
        snprintf(str, STRLONG,"%s", term->Translate("Retail"));
        break;
    case CHECK_DINEIN:
        snprintf(str, STRLONG, "Here");
        break;
    case CHECK_TOGO:
        snprintf(str, STRLONG, "To Go");
        break;
    case CHECK_CALLIN:
        snprintf(str, STRLONG, "Pick Up");
        break;
    default:
        break;
	}
    report->TextL(str);

    if (IsTraining())
        strcpy(str, term->Translate("Training Check"));
    else if (video_target == PRINTER_DEFAULT)
        snprintf(str, sizeof(str), "#%09d", serial_number);
    else
        snprintf(str, sizeof(str), "#%d", serial_number);

    report->TextR(str);
    report->NewLine();
    if (video_target == PRINTER_DEFAULT)
    {
        snprintf(str, sizeof(str), "%s: %s", term->Translate("Opened"),
                term->TimeDate(time_open, TD_SHORT_DATE | TD_NO_DAY));
        report->TextL(str);
        report->NewLine();
        if (date.IsSet())
        {
            snprintf(str, sizeof(str), "%s: %s", term->Translate("Due"),
                    term->TimeDate(date, TD_SHORT_DATE | TD_NO_DAY));
            report->TextL(str);
            report->NewLine();
        }
        if (user_open != user_owner)
        {
            snprintf(str, sizeof(str), "%s: %s", term->Translate("Original Owner"),
                    term->UserName(user_open));
            report->TextL(str);
            report->NewLine();
        }
        report->NewLine();
    }
    else
    {
        snprintf(str, sizeof(str), "%s: %s", term->Translate("Table"), Table());
        report->TextL(str);
        report->NewLine();
    }

    if (customer != nullptr && settings->kv_show_user)
    {
        int custinfo = 0;

        // print customer name
        str[0] = '\0';
        if (strlen(customer->FirstName()) > 0)
            snprintf(str, sizeof(str), "%s %s", customer->FirstName(), customer->LastName());
        else if (strlen(customer->LastName()) > 0)
            snprintf(str, sizeof(str), "%s", customer->LastName());
        if (strlen(str) > 0)
        {
            custinfo = 1;
            report->TextL(str);
        }
        report->NewLine();

        // print customer street address
        if (strlen(customer->Address()) > 0)
        {
            report->TextL(customer->Address());
            report->NewLine();
            custinfo = 1;
        }

        // print customer city, state, and/or postal
        str[0] = '\0';
        if (strlen(customer->City()) > 0)
        {
            if (strlen(customer->State()) > 0)
                snprintf(str, sizeof(str), "%s %s  %s", customer->City(), customer->State(), customer->Postal());
            else
                snprintf(str, sizeof(str), "%s  %s", customer->City(), customer->Postal());
        }
        else if (strlen(customer->State()) > 0)
            snprintf(str, sizeof(str), "%s  %s", customer->State(), customer->Postal());
        else if (strlen(customer->Postal()) > 0)
            snprintf(str, sizeof(str), "%s", customer->Postal());
        if (strlen(str) > 0)
        {
            report->TextL(str);
            report->NewLine();
            custinfo = 1;
        }

        if (strlen(customer->PhoneNumber()) > 0)
        {
            report->TextL(customer->PhoneNumber());
            report->NewLine();
            custinfo = 1;
        }
        if (custinfo)
            report->NewLine();
    }

    if (SubList() == nullptr)
    {
        report->TextC(term->Translate("No Orders"));
        return 0;
    }

    int i = 1;
    int subs = SubCount();
    for (SubCheck *sc = SubList(); sc != nullptr; sc = sc->next)
    {
        if (subs > 1)
        {
            snprintf(str, sizeof(str), "%s #%d - %s", term->Translate("Check"),
                    i, sc->StatusString(term));
            report->Mode(PRINT_UNDERLINE);
            report->TextC(str);
            report->Mode(0);
            report->NewLine();
        }

        if (video_target == PRINTER_DEFAULT)
        {
            Drawer *d = nullptr;
            if (archive)
                d = archive->DrawerList()->FindBySerial(sc->drawer_id);
            else
                d = sys->DrawerList()->FindBySerial(sc->drawer_id);
            
            if (d)
            {
                if (d->IsServerBank())
                    strcpy(str, term->Translate("Server Bank"));
                else
                    snprintf(str, sizeof(str), "%s %d", term->Translate("Drawer"), d->number);
                report->TextL(str);
                
                snprintf(str, sizeof(str), "%s: %s", term->Translate("Cashier"),
                        term->UserName(sc->settle_user));
                report->TextR(str);
                report->NewLine();
            }
            else if (sc->drawer_id)
            {
                snprintf(str, sizeof(str), "%s #%09d", term->Translate("Drawer"), sc->drawer_id);
                report->TextC(str);
                report->NewLine();
            }
            
            if (sc->settle_time.IsSet())
            {
                snprintf(str, sizeof(str), "%s: %s", term->Translate("Time Settled"),
                        term->TimeDate(sc->settle_time, TD_SHORT_DATE | TD_NO_DAY));
                report->TextL(str);
                report->NewLine();
            }
        }

        report->NewLine();
        Flt pos;
        int first;
        for (Order *order = sc->OrderList(); order != nullptr; order = order->next)
		{
            first = 1;
            int order_target = order->VideoTarget(settings);
            if (order->sales_type & SALES_TAKE_OUT)
                snprintf(str2, sizeof(str2), "%s ", term->Translate("TO"));
            else
                str2[0] = '\0';
            if ((video_target == PRINTER_DEFAULT) ||
                (order_target == video_target))
            {
                if (video_target != PRINTER_DEFAULT)
                {
                    snprintf(str, sizeof(str), "%s%-2d %s", str2, order->count, order->PrintDescription());
                }
                else
                    snprintf(str, sizeof(str), "%s%-2d %s", str2, order->count, order->Description(term));
                report->TextL(str);

                // BAK->I'm not going to worry about fitting this in with the
                // "use comma" stuff because the use comma stuff is only for
                // kitchen video and this is only for non-kitchen video.
                if ( show_what & CHECK_DISPLAY_CASH )
                {
                    if (order->cost != 0.0 || (order->status & ORDER_COMP))
                    {
                        report->TextR(term->FormatPrice(order->cost));
                        if (order->status & ORDER_COMP)
                        {
                            report->NewLine();
                            report->TextPosR(-8, "COMP");
                            report->TextR(term->FormatPrice(-order->cost), COLOR_RED);
                        }
                    }
                }
                report->NewLine();
                
                pos = 0.0;
                for (Order *mod = order->modifier_list; mod != nullptr; mod = mod->next)
                {
                    int mod_target = mod->VideoTarget(settings);
                    if ((video_target == PRINTER_DEFAULT) ||
                        (mod_target == video_target))
                    {
                        if (use_comma && rzone)
                        {
                            if (first)
                            {
                                snprintf(str, sizeof(str), "    %s", mod->PrintDescription());
                            }
                            else
                            {
                                char tmpstr[STRLENGTH];
                                strcpy(tmpstr, mod->PrintDescription());
                                snprintf(str, sizeof(str), ", %s", tmpstr);
                                Flt swidth = rzone->TextWidth(term, str);
                                if ((pos + swidth) >= (rzone->Width(term) - 3))
                                {
                                    report->Text(",", COLOR_DEFAULT, ALIGN_LEFT, pos);
                                    report->NewLine();
                                    pos = 0.0;
                                    snprintf(str, sizeof(str), "    %s", tmpstr);
                                }
                            }
                            report->Text(str, COLOR_DEFAULT, ALIGN_LEFT, pos);
                            pos += ((Flt) term->TextWidth(str) / (Flt) term->curr_font_width);
                        }
                        else
                        {
                            snprintf(str, sizeof(str), "    %s", mod->Description(term));
                            report->Text(str, COLOR_DEFAULT, ALIGN_LEFT, pos);
                        }
                        if (show_what & CHECK_DISPLAY_CASH)
                        {
                            if (mod->cost != 0.0 || (mod->status & ORDER_COMP))
                            {
                                if (mod->status & ORDER_COMP)
                                    report->TextR("COMP");
                                else
                                    report->TextR(term->FormatPrice(mod->cost));
                            }
                        }
                        first = 0;
                        if (use_comma == 0)
                            report->NewLine();
                    }
                }
                if (use_comma)
                    report->NewLine();
            }
        }
        if (show_what & CHECK_DISPLAY_CASH)
        {
            report->TextR("------");
            report->NewLine();
            int tax = sc->TotalTax();
            if (tax)
            {
                report->TextPosR(-8, "Tax");
                report->TextR(term->FormatPrice(tax));
                report->NewLine();
                if (sc->IsTaxExempt())
                {
                    report->TextPosR(-8, "Tax Exempt");
                    report->TextR(term->FormatPrice(-tax));
                    report->NewLine();
                    snprintf(str, sizeof(str), "Tax ID:  %s", sc->tax_exempt.Value());
                    report->Mode(PRINT_BOLD);
                    report->TextL(str);
                    report->NewLine();
                    report->Mode(0);
                    tax = 0;
                }
            }
            report->TextPosR(-8, "Total");
            report->TextR(term->FormatPrice(sc->total_sales + tax - sc->item_comps, 1));
            report->NewLine();
            
            if (sc->PaymentList())
            {
                report->NewLine();
                Payment *payptr = sc->PaymentList();
                while (payptr)
                {
                    report->TextL(payptr->Description(settings));
                    report->TextR(term->FormatPrice(payptr->value));
                    report->NewLine();
                    payptr = payptr->next;
                }
                
                report->TextR("------");
                report->NewLine();
                report->TextPosR(-8, term->Translate("Amount Tendered"));
                report->TextR(term->FormatPrice(sc->payment, 1));
                report->NewLine();
                if (sc->balance > 0)
                {
                    report->TextPosR(-8, term->Translate("Balance Due"));
                    report->TextR(term->FormatPrice(sc->balance, 1));
                    report->NewLine();
                }
            }
        }
        
        if (sc->next)
        {
            report->Line();
            report->NewLine();
        }
        ++i;
    }
    return 0;
}

int Check::EntreeCount(int seat)
{
    FnTrace("Check::EntreeCount()");
    int count = 0;
    // FIX - entree count modified for SunWest

    for (SubCheck *sc = SubList(); sc != nullptr; sc = sc->next)
        for (Order *order = sc->OrderList(); order != nullptr; order = order->next)
            if ((seat < 0 || seat == order->seat) && order->IsEntree() && order->cost > 0)
                ++count;

    return count;
}

// this no longer updates check->current_sub
SubCheck *Check::FirstOpenSubCheck(int seat)
{
    FnTrace("Check::FirstOpenSubCheck()");
    if (SubList() == nullptr)
        return NewSubCheck();
    SubCheck *sc;

    // Tries for open check first with seat
    for (sc = SubList(); sc != nullptr; sc = sc->next)
    {
        if (sc->status == CHECK_OPEN &&
            (seat < 0 || sc->IsSeatOnCheck(seat)))
        {
            return sc;
        }
    }

    // Tries for a closed check with seat
    for (sc = SubList(); sc != nullptr; sc = sc->next)
    {
        if (sc->status == CHECK_CLOSED &&
            (seat < 0 || sc->IsSeatOnCheck(seat)))
        {
            return sc;
        }
    }

    // Failed to find any subchecks with seat
    if (seat >= 0)
        return FirstOpenSubCheck(); // search without seat

    return current_sub;
}

SubCheck *Check::NextOpenSubCheck(SubCheck *sc)
{
    FnTrace("Check::NextOpenSubCheck()");
    if (sc == nullptr)
        sc = current_sub;
    if (sc == nullptr || SubList() == nullptr)
    {
        current_sub = nullptr;
        return nullptr;
    }

    int loop = 0;
    do
    {
        if (sc != nullptr && sc->next == nullptr)
        {
            // For non-seat based ordering mode, if we have a PreAuthed
            // credit card we need to be able to get back to the table
            // page.  If we just keep looping from check to check, always
            // returning to the beginning, then we'll never get back to
            // the table page.  So if we're at the last subcheck in the
            // list, return nullptr;
            return nullptr;
        }
        else
        {
            sc = sc->next;
            if (sc == nullptr)
            {
                ++loop;
                sc = SubList();
            }
            if (sc->status == CHECK_OPEN)
            {
                current_sub = sc;
                return sc;
            }
        }
    }
    while (loop < 2);

    current_sub = nullptr;
    return nullptr;
}

TimeInfo *Check::TimeClosed()
{
    FnTrace("Check::TimeClosed()");
    SubCheck *best = nullptr;
    for (SubCheck *sc = SubList(); sc != nullptr; sc = sc->next)
    {
        if (sc->status == CHECK_OPEN)
            return nullptr;
        if (best == nullptr || best->settle_time < sc->settle_time)
            best = sc;
    }

    if (best == nullptr)
        return nullptr;
    else
        return &best->settle_time;
}

int Check::WhoGetsSale(Settings *settings)
{
    FnTrace("Check::WhoGetsSale()");
    if (settings->sale_credit == 0)
        return user_owner;
    else
        return user_open;
}

int Check::SecondsOpen()
{
    FnTrace("Check::SecondsOpen()");
    TimeInfo *timevar = TimeClosed();
    if (timevar == nullptr)
        timevar = &SystemTime;

    return SecondsElapsed(*timevar, time_open);
}

int Check::SeatsUsed()
{
    FnTrace("Check::SeatsUsed()");
    int s;
    int s1;
    int s2;
    int count = 0;
    int seats[32];
    int i;

    for (i = 0; i < (int)(sizeof(seats)/sizeof(int)); ++i)
        seats[i] = 0;

    for (SubCheck *sc = SubList(); sc != nullptr; sc = sc->next)
        for (Order *order = sc->OrderList(); order != nullptr; order = order->next)
		{
			s = order->seat;
			if (s >= (int)(sizeof(seats)*8))
				continue;

			s1 = s / (sizeof(int)*8);
			s2 = 1 << (s % (sizeof(int)*8));
			if (!(seats[s1] & s2))
			{
				++count;
				seats[s1] |= s2;
			}
		}

    return count;
}

int Check::HasOpenTab()
{
    FnTrace("Check::HasOpenTab()");
    int retval = 0;
    SubCheck *subcheck = SubList();

    while (retval == 0 && subcheck != nullptr)
    {
        if (subcheck->status == CHECK_OPEN)
        {
            if (subcheck->HasOpenTab())
                retval = 1;
        }
        subcheck = subcheck->next;
    }

    return retval;
}

int Check::IsEmpty()
{
    FnTrace("Check::IsEmpty()");
    for (SubCheck *sc = SubList(); sc != nullptr; sc = sc->next)
    {
        if (sc->OrderList() || sc->PaymentList())
            return 0;
    }
    return 1;
}

int Check::IsTraining()
{
    FnTrace("Check::IsTraining()");
    if (flags & CF_TRAINING)
        return 1;
    else
        return 0;
}

const genericChar* Check::PaymentSummary(Terminal *term)
{
    FnTrace("Check::PaymentSummary()");
    static genericChar str[256];
    Settings *settings = term->GetSettings();

    genericChar tmp[32]; 
    int status = Status();
    if (status == CHECK_VOIDED)
        strcpy(str, term->Translate("Voided"));
    else if (status == CHECK_OPEN)
        strcpy(str, term->Translate("Unpaid"));
    else
    {
        int check = 0, comp = 0, cash = 0, gift = 0, room = 0;
        int discount = 0, emeal = 0, coupon = 0, credit = 0, account = 0;

        for (SubCheck *sc = SubList(); sc != nullptr; sc = sc->next)
		{
            for (Payment *payptr = sc->PaymentList(); payptr != nullptr; payptr = payptr->next)
			{
                switch (payptr->tender_type)
				{
                case TENDER_CASH:          cash = 1; break;
                case TENDER_CHECK:         check = 1; break;
                case TENDER_COUPON:        coupon = 1; break;
                case TENDER_DISCOUNT:      discount = 1; break;
                case TENDER_COMP:          comp = 1; break;
                case TENDER_EMPLOYEE_MEAL: emeal = 1; break;
                case TENDER_GIFT:          gift = 1; break;
                case TENDER_CHARGE_CARD:   credit = 1; break;
                case TENDER_CREDIT_CARD:   credit = 1; break;
                case TENDER_DEBIT_CARD:    credit = 1; break;
                case TENDER_ACCOUNT:       account = 1; break;
                case TENDER_CHARGE_ROOM:   room = payptr->tender_id; break;
                default: break;
				}// end switch
			}// end for
		}// end for

        str[0] = '\0';
        if (credit)   strncat(str, "CC,", sizeof(str) - strlen(str) - 1);
        if (gift)     strncat(str, "G,", sizeof(str) - strlen(str) - 1);
        if (coupon)   strncat(str, "Cp,", sizeof(str) - strlen(str) - 1);
        if (comp)     strncat(str, "WC,", sizeof(str) - strlen(str) - 1);
        if (discount) strncat(str, "D,", sizeof(str) - strlen(str) - 1);
        if (emeal)    strncat(str, "E,", sizeof(str) - strlen(str) - 1);
        if (check)    strncat(str, "Ck,", sizeof(str) - strlen(str) - 1);
        if (account)  strncat(str, "A,", sizeof(str) - strlen(str) - 1);
        if (room)
        {
            snprintf(tmp, sizeof(tmp), "R#%d,", room);
            strncat(str, tmp, sizeof(str) - strlen(str) - 1);
        }
        if (settings->money_symbol.size() > 0)
        {
            strncat(str, settings->money_symbol.Value(), sizeof(str) - strlen(str) - 1);
            strncat(str, ",", sizeof(str) - strlen(str) - 1);
        }

        int len = strlen(str);
        if (len > 0)
            str[len-1] = '\0';
    }
    return str;
}

/****
 * Search:  This is primarily for TakeOut, Delivery, and Catering orders so
 *   that employees can find previous orders more easily.  It only searches
 *   the CustomerInfo.
 * Returns:  1 if the word is contained in the check, 0 otherwise.
 ****/
int Check::Search(const genericChar* word)
{
    FnTrace("Check::Search()");
    int retval = 0;

    if (customer != nullptr)
        retval = customer->Search(word);

    return retval;
}

int Check::SetBatch(const char* termid, const char* batch)
{
    FnTrace("Check::SetBatch()");
    int retval = 1;
    SubCheck *currsub = SubList();

    while (currsub != nullptr)
    {
        retval = currsub->SetBatch(termid, batch);
        currsub = currsub->next;
    }

    return retval;
}

int Check::IsBatchSet()
{
    FnTrace("Check::IsBatchSet()");
    int retval = 0;
    SubCheck *currsub = SubList();
    Payment *payment;
    Credit *credit;

    while (currsub != nullptr)
    {
        payment = currsub->PaymentList();
        while (payment != nullptr)
        {
            if (payment->credit != nullptr)
            {
                credit = payment->credit;
                if (credit->Batch() > 0 &&
                    credit->settle_time.IsSet())
                {
                    retval += 1;
                }
            }
            payment = payment->next;
        }
        currsub = currsub->next;
    }

    return retval;
}

int Check::CustomerType(int set)
{
    FnTrace("Check::CustomerType()");

    if (set >= 0)
        type = set;

    return type;
}

int Check::IsTakeOut()
{
    FnTrace("Check::IsTakeOut()");
    int ct = CustomerType();
    return (ct == CHECK_TAKEOUT  || 
            ct == CHECK_DELIVERY || 
            ct == CHECK_RETAIL   ||
            ct == CHECK_CATERING ||
            ct == CHECK_TOGO); 
}

int Check::IsFastFood()
{
    FnTrace("Check::IsFastFood()");
	int ct = CustomerType();
	return (ct == CHECK_FASTFOOD ||
            ct == CHECK_RETAIL   ||
            ct == CHECK_TAKEOUT);
}

int Check::IsToGo()
{
   FnTrace("Check::IsToGo()");
   int ct = CustomerType();
   return ct == CHECK_TOGO || 
          ct == CHECK_TAKEOUT;
}

int Check::IsForHere()
{
   FnTrace("Check::IsForHere()");
   int ct = CustomerType();
   return ct == CHECK_DINEIN;
}

const genericChar* Check::Table(const genericChar* set)
{
    FnTrace("Check::Table()");

    if (set != nullptr)
        label.Set(set);

    return label.Value();
}

const genericChar* Check::Comment(const genericChar* set)
{
    FnTrace("Check::Comment()");

    if (set)
        comment.Set(set);

    return comment.Value();
}

TimeInfo *Check::Date(TimeInfo *timevar)
{
    FnTrace("Check::Date()");

    if (timevar != nullptr)
        date.Set(timevar);

    return &date;
}

TimeInfo *Check::CheckIn(TimeInfo *timevar)
{
    FnTrace("Check::CheckIn()");

    if (timevar != nullptr)
        check_in.Set(timevar);
    
    return &check_in;
}

TimeInfo *Check::CheckOut(TimeInfo *timevar)
{
    FnTrace("Check::CheckOut()");

    if (timevar != nullptr)
        check_out.Set(timevar);

    return &check_out;
}

int Check::Guests(int set)
{
    FnTrace("Check::Guests()");

    if (set > -1)
        guests = set;

    return guests;
}

int Check::CallCenterID(int set)
{
    FnTrace("Check::CallCenterID()");

    if (set >= 0)
        call_center_id = set;

    return call_center_id;
}

int Check::CustomerID(int set)
{
    FnTrace("Check::CustomerID()");

    if (set >= 0)
    {
        customer_id = set;
        customer = MasterSystem->customer_db.FindByID(customer_id);
    }
    if (customer == nullptr)
        customer_id = -1;

    return customer_id;
}

const genericChar* Check::Address(const genericChar* set)
{
    FnTrace("Check::Address()");
    if (customer)
        return customer->Address(set);
    return EmptyStr;
}

const genericChar* Check::Address2(const genericChar* set)
{
    FnTrace("Check::Address2()");
    if (customer)
        return customer->Address2(set);
    return EmptyStr;
}

const genericChar* Check::CrossStreet(const genericChar* set)
{
    FnTrace("Check::CrossStreet()");
    if (customer)
        return customer->CrossStreet(set);
    return EmptyStr;
}

const genericChar* Check::LastName(const genericChar* set)
{
    FnTrace("Check::LastName()");
    if (customer)
        return customer->LastName(set);
    return EmptyStr;
}

const genericChar* Check::FirstName(const genericChar* set)
{
    FnTrace("Check::FirstName()");
    if (customer)
        return customer->FirstName(set);
    return EmptyStr;
}

genericChar* Check::FullName(genericChar* dest)
{
    FnTrace("Check::FullName()");
    static genericChar buffer[STRLENGTH];

    if (dest == nullptr)
        dest = buffer;
    if (customer != nullptr)
    {
        if (strlen(customer->FirstName()) > 0)
            snprintf(dest, STRLENGTH, "%s %s", customer->FirstName(), customer->LastName());
        else if (strlen(customer->LastName()) > 0)
            snprintf(dest, STRLENGTH, "%s", customer->LastName());
    }

    return dest;
}

const genericChar* Check::Company(const genericChar* set)
{
    FnTrace("Check::Company()");
    if (customer)
        return customer->Company(set);
    return EmptyStr;
}

const genericChar* Check::City(const genericChar* set)
{
    FnTrace("Check::City()");
    if (customer)
        return customer->City(set);
    return EmptyStr;
}

const genericChar* Check::State(const genericChar* set)
{
    FnTrace("Check::State()");
    if (customer)
        return customer->State(set);
    return EmptyStr;
}

const genericChar* Check::Postal(const genericChar* set)
{
    FnTrace("Check::Postal()");
    if (customer)
        return customer->Postal(set);
    return EmptyStr;
}

const genericChar* Check::Vehicle(const genericChar* set)
{
    FnTrace("Check::Vehicle()");
    if (customer)
        return customer->Vehicle(set);
    return EmptyStr;
}

const genericChar* Check::CCNumber(const genericChar* set)
{
    FnTrace("Check::CCNumber()");
    const genericChar* retval = EmptyStr;

    if (customer)
        retval = customer->CCNumber(set);
    return retval;
}

const genericChar* Check::CCExpire(const genericChar* set)
{
    FnTrace("Check::CCExpire()");
    const genericChar* retval = EmptyStr;

    if (customer)
        retval = customer->CCExpire(set);
    return retval;
}

const genericChar* Check::License(const genericChar* set)
{
    FnTrace("Check::License()");
    const genericChar* retval = EmptyStr;

    if (customer)
        retval = customer->License(set);
    return retval;
}

const genericChar* Check::PhoneNumber(const genericChar* set)
{
    FnTrace("Check::PhoneNumber()");
    if (customer)
        return customer->PhoneNumber(set);
    return EmptyStr;
}

const genericChar* Check::Extension(const genericChar* set)
{
    FnTrace("Check::Extension()");
    if (customer)
        return customer->Extension(set);
    return EmptyStr;
}


/**** SubCheck Class ****/
// Constructor
SubCheck::SubCheck()
{
    FnTrace("SubCheck::SubCheck()");
    next        = nullptr;
    fore        = nullptr;
    status      = CHECK_OPEN;
    raw_sales   = 0;
    payment     = 0;
    balance     = 0;
    number      = 0;
    id          = 0;
    settle_user = 0;
    item_comps  = 0;
    total_cost  = 0;
    drawer_id   = 0;

    total_sales           = 0;
    total_tax_food        = 0;
    total_tax_alcohol     = 0;
    total_tax_room        = 0;
    total_tax_merchandise = 0;
    total_tax_GST 		  = 0;
    total_tax_PST 		  = 0;
    total_tax_HST 		  = 0;
    total_tax_QST		  = 0;
    new_QST_method        = 1;
    tab_total             = 0;
    total_tax_VAT         = 0;
    delivery_charge       = 0;
    archive     = nullptr;
}

// Member functions
SubCheck *SubCheck::Copy(Settings *settings)
{
    FnTrace("SubCheck::Copy(Settings)");
    SubCheck *sc = new SubCheck();
    if (sc == nullptr)
        return nullptr;

    sc->status         = status;
    sc->number         = number;
    sc->settle_user    = settle_user;
    sc->drawer_id      = drawer_id;
    sc->tax_exempt.Set(tax_exempt);
    sc->new_QST_method = new_QST_method;
    sc->tab_total      = tab_total;
    sc->delivery_charge = delivery_charge;
    sc->check_type     = check_type;

    for (Order *order = OrderList(); order != nullptr; order = order->next)
        sc->Add(order->Copy(), 0);

    for (Payment *payptr = PaymentList(); payptr != nullptr; payptr = payptr->next)
        sc->Add(payptr->Copy(), 0);
        
    sc->FigureTotals(settings);
    return sc;
}

int SubCheck::Copy(SubCheck *sc, Settings *settings, int restore)
{
    FnTrace("SubCheck::Copy(SubCheck,Settings)");
    if (sc == nullptr)
        return 1;

    Purge(restore);
    status         = sc->status;
    number         = sc->number;
    settle_user    = sc->settle_user;
    drawer_id      = sc->drawer_id;
    tax_exempt.Set(sc->tax_exempt);
    new_QST_method = sc->new_QST_method;
    tab_total      = sc->tab_total;
    delivery_charge = sc->delivery_charge;
    check_type     = sc->check_type;

    for (Order *order = sc->OrderList(); order != nullptr; order = order->next)
        Add(order->Copy(), 0);

    for (Payment *payptr = sc->PaymentList(); payptr != nullptr; payptr = payptr->next)
        Add(payptr->Copy(), 0);

    if (settings)
        FigureTotals(settings);
    return 0;
}

int SubCheck::Read(Settings *settings, InputDataFile &infile, int version)
{
    FnTrace("SubCheck::Read()");
    // See Check::Read() for Version Notes
    int count = 0;
    int error = 0;
    int i;

    error += infile.Read(status);
    error += infile.Read(settle_user);
    error += infile.Read(settle_time);
    error += infile.Read(drawer_id);

    error += infile.Read(count);
    if (count < 10000 && error == 0)
    {
        for (i = 0; i < count; ++i)
        {
            if (infile.end_of_file)
            {
                ReportError("Unexpected end of orders in SubCheck");
                return 1; // Error
            }
    
            Order *order = new Order;
            error += order->Read(infile, version);
            if (error)
            {
                delete order;
                genericChar str[64];
                snprintf(str, sizeof(str), "Error reading order %d of %d", i+1, count);
                ReportError(str);
                return error;
            }
            if (Add(order, 0))
            {
                ReportError("Error in adding order");
                delete order;
            }
        }
    }

    count = 0;
    error += infile.Read(count);
    if (count < 10000 && error == 0)
    {
        for (i = 0; i < count; ++i)
        {
            if (infile.end_of_file)
            {
                ReportError("Unexpected end of payments in SubCheck");
                return 1; // Error
            }
    
            Payment *pmnt = new Payment;
            pmnt->drawer_id = drawer_id;  // FIX - clear this up later
            error += pmnt->Read(infile, version);
            if (error)
            {
                delete pmnt;
                return error;
            }
            if (Add(pmnt, 0))
            {
                ReportError("Error in adding payment");
                delete pmnt;
            }
        }
    }

    if (version >= 17)
        error += infile.Read(tax_exempt);
    if (version >= 18)
        error += infile.Read(new_QST_method);
    else
        new_QST_method = 0;
    if (version >= 23)
        error += infile.Read(tab_total);
    if (version >= 25)
        error += infile.Read(delivery_charge);

    if (error == 0)
        FigureTotals(settings);
    else
        ReportError("Error in reading subcheck");
    return error;
}

int SubCheck::Write(OutputDataFile &outfile, int version)
{
    FnTrace("SubCheck::Write()");
    if (version < 7)
        return 1;

    // Write version 7
    int error = 0;
    error += outfile.Write(status);
    error += outfile.Write(settle_user);
    error += outfile.Write(settle_time);
    error += outfile.Write(drawer_id, 1);

    error += outfile.Write(OrderCount(), 1);
    for (Order *order = OrderList(); order != nullptr; order = order->next)
    {
        error += order->Write(outfile, version);
        for (Order *mod = order->modifier_list; mod != nullptr; mod = mod->next)
        {
            error += mod->Write(outfile, version);
        }
    }

    error += outfile.Write(PaymentCount(), 1);
    for (Payment *payptr = PaymentList(); payptr != nullptr; payptr = payptr->next)
    {
        error += payptr->Write(outfile, version);
    }

    error += outfile.Write(tax_exempt);
    error += outfile.Write(new_QST_method);
    error += outfile.Write(tab_total);
    error += outfile.Write(delivery_charge);

    return 0;
}

int SubCheck::Add(Order *order, Settings *settings)
{
    FnTrace("SubCheck::Add(Order, Settings)");
    Order *ptr = nullptr;
    int added = 0;

    if (order == nullptr)
        return 1;

    if (order->IsModifier())
    {
        if (OrderListEnd())
            return OrderListEnd()->Add(order);  // Add modifier to last order
        else
            return 1;  // nothing to modify
    }

    if (order->item_type == ITEM_POUND)
    {
        ptr = OrderList();
        while (ptr != nullptr && added == 0)
        {
            if (ptr->IsEqual(order))
            {
                ptr->count += order->count;
                delete order;
                added = 1;
            }
            else
            {
                ptr = ptr->next;
            }
        }
    }

    if (added == 0)
    {
        ptr = OrderListEnd();

        // start at end of list and work backwords
        while (ptr && order->seat < ptr->seat)
            ptr = ptr->fore;
        
        // Insert order after ptr, possibly at head (if ptr == nullptr)
        order_list.AddAfterNode(ptr, order);
    }

    if (settings)
        FigureTotals(settings);

    return 0;
}

/****
 * Add:  This method can cause deep recursion.  It should generally
 ****/
int SubCheck::Add(Payment *pmnt, Settings *settings)
{
    FnTrace("SubCheck::Add(Payment, Settings)");
    if (pmnt == nullptr)
        return 1;

    // Prevent multiple of discount payments
    // We only allow one global coupon ("15% off entire meal") but we'll
    // allow multiple item specific coupons.
    int tt = pmnt->tender_type;
    if (tt == TENDER_COMP || tt == TENDER_EMPLOYEE_MEAL ||
        tt == TENDER_DISCOUNT || tt == TENDER_COUPON)
    {
        Payment *ptr = PaymentList();
        while (ptr)
        {
            tt = ptr->tender_type;
            Payment *nptr = ptr->next;
            if (tt == TENDER_COMP || tt == TENDER_EMPLOYEE_MEAL ||
                tt == TENDER_DISCOUNT)
            {
                Remove(ptr, 0);
                delete ptr;
            }
            else if (tt == TENDER_COUPON)
            {
                if ((pmnt->flags & TF_APPLY_EACH) == 0 &&
                    (ptr->flags & TF_APPLY_EACH) == 0)
                {
                    Remove(ptr, 0);
                    delete ptr;
                }
            }
            ptr = nptr;
        }
    }
    else if (tt == TENDER_GRATUITY || tt == TENDER_CAPTURED_TIP ||
             tt == TENDER_CHARGED_TIP)
    {
        // Replace previous payment with new one
        Payment *ptr = FindPayment(tt);
        if (ptr)
        {
            Remove(ptr, 0);
            delete ptr;
        }
    }

    // start at end of list and work backwords
    Payment *ptr = PaymentListEnd();
    while (ptr && pmnt->Priority() > ptr->Priority())
        ptr = ptr->fore;

    // Insert payment after ptr
    payment_list.AddAfterNode(ptr, pmnt);

    if (settings)
        ConsolidatePayments(settings);

    return 0;
}

int SubCheck::Remove(Order *order, Settings *settings)
{
    FnTrace("SubCheck::Remove(Order, Settings)");
    if (order == nullptr)
        return 1;

    if (order->parent)
    {
        order->parent->Remove(order);
        if (settings)
            FigureTotals(settings);
        return 0;
    }

    order_list.Remove(order);

    if (settings)
        FigureTotals(settings);
    return 0;
}

int SubCheck::Remove(Payment *pmnt, Settings *settings)
{
    FnTrace("SubCheck::Remove(Payment, Settings)");
    payment_list.Remove(pmnt);
    if (settings)
        FigureTotals(settings);
    return 0;
}

int SubCheck::Purge(int restore)
{
    FnTrace("SubCheck::Purge()");

    if (restore)
    {
        Payment *paymnt = payment_list.Head();
        while (paymnt != nullptr)
        {
            if (paymnt->credit != nullptr &&
                !paymnt->credit->IsVoided() &&
                !paymnt->credit->IsRefunded())
            {
                if (paymnt->flags & TF_FINAL)
                    delete paymnt->credit;
                else
                    MasterSystem->cc_exception_db->Add(paymnt->credit);
                paymnt->credit = nullptr;
            }
            paymnt = paymnt->next;
        }
    }

    order_list.Purge();
    payment_list.Purge();
    return 0;
}

Order *SubCheck::RemoveOne(Order *order)
{
	FnTrace("SubCheck::RemoveOne()");

    return RemoveCount(order, 1);
}

Order *SubCheck::RemoveCount(Order *order, int count)
{
	FnTrace("SubCheck::RemoveCount()");

	if (order == nullptr)
		return nullptr;

	if (order->count > count)
	{
		Order *ptr = order->Copy();
		order->count -= count;
		order->FigureCost();
		ptr->count = count;
		ptr->FigureCost();
		return ptr;
	}
	else
	{
		Remove(order);
		return order;
	}
}

int SubCheck::CancelOrders(Settings *settings)
{
    FnTrace("SubCheck::CancelOrders()");
    int change = 0;
    Order *order = OrderList();
    while (order)
    {
        Order *ptr = order->next;
        if ((order->status & ORDER_FINAL) == 0)
        {
            Remove(order);
            delete order;
            change = 1;
        }
        order = ptr;
    }

    if (change)
        FigureTotals(settings);
    return 0;
}

int SubCheck::CancelPayments(Terminal *term)
{
    FnTrace("SubCheck::CancelPayments()");
    int retval = 0;
    int change = 0;
    Payment *payptr = PaymentList();
    Settings *settings = term->GetSettings();
    Credit *credit = nullptr;  // just to save some typing

    while (payptr)
    {
        Payment *ptr = payptr->next;
        if (!(payptr->flags & TF_FINAL) && !(payptr->flags & TF_IS_TAB))
        {
            if (payptr->credit != nullptr)
            {
                credit = payptr->credit;
                if (credit->IsAuthed())
                    MasterSystem->cc_exception_db->Add(term, credit->Copy());
            }
            Remove(payptr, 0);
            delete payptr;  // delete payptr also deletes credit
            change = 1;
        }
        payptr = ptr;
    }
    if (tax_exempt.size() > 0)
    {
        tax_exempt.Clear();
        change = 1;
    }

    if (change)
        FigureTotals(settings);
    return retval;
}

int SubCheck::UndoPayments(Terminal *term, Employee *employee)
{
    FnTrace("SubCheck::UndoPayments()");
    int retval = 0;
    Settings *settings = term->GetSettings();

    if (employee == nullptr)
        return 1;

    if (employee->CanRebuild(settings))
    {
        // Always ensure check status is open
        status = CHECK_OPEN;

        Payment *payptr = PaymentList();
        while (payptr)
        {
            if (payptr->flags & TF_FINAL)
                payptr->flags &= ~TF_FINAL;
            payptr = payptr->next;
        }
    }
    if (retval == 0)
        retval = CancelPayments(term);

    return retval;
}

int SubCheck::FigureTotals(Settings *settings)
{
    FnTrace("SubCheck::FigureTotals()");
    DList<Payment> coupons;
    Payment *discount = nullptr; // ptr to discount payment
    Payment *gratuity = nullptr; // ptr to auto gratuity amount
    Order *order;
    CouponInfo *coupon;
    int max_change = 0;
    int max_tip    = 0;
    payment = 0;
    balance = 0;
    tab_total = 0;

    int change_for_credit = 0;
    int change_for_roomcharge = 0;
    int change_for_checks = 0;
    int change_for_gift = 0;
    if (archive != nullptr)
    {
        change_for_credit = archive->change_for_credit;
        change_for_roomcharge = archive->change_for_roomcharge;
        change_for_checks = archive->change_for_checks;
        change_for_gift = archive->change_for_gift;
    }
    else
    {
        change_for_credit = settings->change_for_credit;
        change_for_roomcharge = settings->change_for_roomcharge;
        change_for_checks = settings->change_for_checks;
        change_for_gift = settings->change_for_gift;
    }

    // Clear the reduced flag on all orders
    order = OrderList();
    while (order != nullptr)
    {
        if (order->IsReduced() == 1)
            order->IsReduced(0);
        order = order->next;
    }

    // Check Payments
    Payment *payptr = PaymentList();
    while (payptr)
    {
        payptr->FigureTotals();
        Payment *ptr = payptr->next;
        Credit *cr = payptr->credit;

        if (!(payptr->flags & TF_IS_PERCENT))
            payptr->value = payptr->amount;
        if (payptr->flags & TF_IS_TAB)
            tab_total += payptr->TabRemain();

        switch (payptr->tender_type)
        {
        case TENDER_CHANGE:
        case TENDER_OVERAGE:
        case TENDER_MONEY_LOST:
            Remove(payptr, 0);
            delete payptr;
            break;
        case TENDER_GRATUITY:
            gratuity = payptr;
            break;
        case TENDER_COMP:
        case TENDER_EMPLOYEE_MEAL:
        case TENDER_DISCOUNT:
            discount = payptr; // only one discount allowed at a time
            break;
        case TENDER_COUPON:
            if ((payptr->flags & TF_APPLY_EACH) == 0)
                discount = payptr;
            else
            {
                if (archive != nullptr)
                    coupon = archive->FindCouponByID(payptr->tender_id);
                else
                    coupon = settings->FindCouponByID(payptr->tender_id);
                if (coupon != nullptr)
                    coupon->Apply(this, payptr);
                balance -= payptr->value;
            }
            break;
        case TENDER_CASH:
            payment    += payptr->value;
            balance    -= payptr->value;
            max_tip    += payptr->value;
            max_change += payptr->value;
            break;
        case TENDER_CREDIT_CARD:
        case TENDER_DEBIT_CARD:
            if (cr && cr->Total() > 0)
            {
                payment    += payptr->value;
                balance    -= payptr->value;
                max_tip    += payptr->value;
                if (change_for_credit)
                    max_change += payptr->value;
            }
            break;
        case TENDER_CHARGE_CARD:
            payment += payptr->value;
            balance -= payptr->value;
            max_tip += payptr->value;
            if (change_for_credit)
                max_change += payptr->value;
            break;
        case TENDER_CHARGE_ROOM:
            payment += payptr->value;
            balance -= payptr->value;
            max_tip += payptr->value;
            if (change_for_roomcharge)
                max_change += payptr->value;
            break;
        case TENDER_CHECK:
            payment += payptr->value;
            balance -= payptr->value;
            max_tip += payptr->value;
            if (change_for_checks)
                max_change += payptr->value;
            break;
        case TENDER_GIFT:
            payment += payptr->value;
            balance -= payptr->value;
            if (change_for_gift)
                max_change += payptr->value;
            break;
        case TENDER_CAPTURED_TIP:
            balance += payptr->value;
            break;
        case TENDER_CHARGED_TIP:
            balance += payptr->value;
            break;
        default:
            payment += payptr->value;
            balance -= payptr->value;
            max_tip += payptr->value;
            max_change += payptr->value;
            break;
        }
        payptr = ptr;
    }
    balance += delivery_charge;

    raw_sales = 0;                // total of all orders' value
    int untaxed_sales       = 0; // other sales (gift certif)
    int untaxed_comp        = 0; // total of untaxed line item comps

    int food_sales          = 0; // total food sales
    int food_discount       = 0; // regular food
    int food_no_discount    = 0; // food that can't be discounted
    int food_comp           = 0; // total of food line item comps
	
    int alcohol_sales       = 0; // total alcohol sales
    int alcohol_discount    = 0; // regular alcohol
    int alcohol_no_discount = 0; // alcohol that can't be discounted
    int alcohol_comp        = 0; // total of alcohol line item comps

    int room_sales           = 0;
    int room_discount        = 0;
    int room_no_discount     = 0;
    int room_comp            = 0;

    int merchandise_sales       = 0;
    int merchandise_discount    = 0;
    int merchandise_no_discount = 0;
    int merchandise_comp        = 0;

    Flt food_tax         = 0;
    Flt alcohol_tax      = 0;
    Flt GST_tax          = 0;
    Flt PST_tax          = 0;
    Flt HST_tax          = 0;
    Flt QST_tax          = 0;
    Flt room_tax         = 0;
    Flt merchandise_tax  = 0;
    Flt VAT_tax          = 0;
    if (archive != nullptr)
    {
        food_tax = archive->tax_food;
        alcohol_tax = archive->tax_alcohol;
        GST_tax = archive->tax_GST;
        PST_tax = archive->tax_PST;
        HST_tax = archive->tax_HST;
        QST_tax = archive->tax_QST;
        room_tax = archive->tax_room;
        merchandise_tax = archive->tax_merchandise;
        VAT_tax = archive->tax_VAT;
    }
    else
    {
        food_tax = settings->tax_food;
        alcohol_tax = settings->tax_alcohol;
        GST_tax = settings->tax_GST;
        PST_tax = settings->tax_PST;
        HST_tax = settings->tax_HST;
        QST_tax = settings->tax_QST;
        room_tax = settings->tax_room;
        merchandise_tax = settings->tax_merchandise;
        VAT_tax = settings->tax_VAT;
    }
    
    // For some states, takeout food is not taxed. Account for this based on the tax settings
    if ((settings->tax_takeout_food == 0) && ((check_type == CHECK_TAKEOUT) || (check_type == CHECK_TOGO)))
       food_tax = 0;

    // Count up the cost of each order into discounts and no_discounts,
    // depending on whether each item can be discounted.
    // AKA, the food_discount variable will contain the total dollar amount
    // of all food ordered that can be discounted.
    int discount_alcohol = archive ? archive->discount_alcohol : settings->discount_alcohol;
    order = OrderList();
    while (order)
    {
        order->FigureCost();
        raw_sales += order->total_cost;

        if (order->sales_type & SALES_UNTAXED)
        {
            untaxed_sales += order->total_cost;
            untaxed_comp  += order->total_comp;
        }
        else if (order->sales_type & SALES_ALCOHOL)
        {
            alcohol_comp  += order->total_comp;
            if (order->CanDiscount(discount_alcohol, discount))
            {
                alcohol_discount += order->total_cost;
                order->discount = 1;
            }
            else
            {
                alcohol_no_discount += order->total_cost;
                order->discount = 1;
            }
        }
        else if (order->sales_type & SALES_ROOM)
        {
            room_comp += order->total_comp;
            if (order->CanDiscount(discount_alcohol, discount))
            {  // order cost discounted
                room_discount += order->total_cost;
                order->discount = 1;
            }
            else
            {  // order cost not discounted
                room_no_discount += order->total_cost;
                order->discount = 0;
            }
        }
        else if (order->sales_type & SALES_MERCHANDISE)
        {
            merchandise_comp += order->total_comp;
            if (order->CanDiscount(discount_alcohol, discount))
            {  // order cost discounted
                merchandise_discount += order->total_cost;
                order->discount = 1;
            }
            else
            {  // order cost not discounted
                merchandise_no_discount += order->total_cost;
                order->discount = 0;
            }
        }
        else
        {  // food
            food_comp += order->total_comp;
            if (order->CanDiscount(discount_alcohol, discount))
            {  // order cost discounted
                food_discount += order->total_cost;
                order->discount = 1;
            }
            else
            {  // order cost not discounted
                food_no_discount += order->total_cost;
                order->discount = 0;
            }
        }
        order = order->next;
    }

    // totals
    item_comps = food_comp + alcohol_comp + untaxed_comp +
        room_comp + merchandise_comp;

    if (gratuity && gratuity->flags & TF_IS_PERCENT)
    {
        Flt f = PriceToFlt(raw_sales) * PercentToFlt(gratuity->amount);
        gratuity->value = -FltToPrice(f);
    }

    food_sales        = food_no_discount;
    alcohol_sales     = alcohol_no_discount;
    room_sales        = room_no_discount;
    merchandise_sales = merchandise_no_discount;
    if (discount)
    {
        // apply percentage discount
        if (discount->flags & TF_IS_PERCENT)
        {
            int per = discount->amount;
            if (per > 10000)
                per = 10000;

            Flt f = (Flt) food_discount * (1.0 - PercentToFlt(per));
            food_sales += (int) (f + .5);

            f = (Flt) alcohol_discount * (1.0 - PercentToFlt(per));
            alcohol_sales += (int) (f + .5);

            f = (Flt) room_discount * (1.0 - PercentToFlt(per));
            room_sales += (int) (f + .5);

            f = (Flt) merchandise_discount * (1.0 - PercentToFlt(per));
            merchandise_sales += (int) (f + .5);

            discount->value =
				((food_no_discount + food_discount) - food_sales) +
				((alcohol_no_discount + alcohol_discount) - alcohol_sales) +
				((room_no_discount + room_discount) - room_sales) +
				((merchandise_no_discount + merchandise_discount) - merchandise_sales);
        }
        else
        {
            int fd = discount->amount;
            int ad = 0;
            int rd = 0;
            int md = 0;

            if (fd > food_discount)
            {
				ad = fd - food_discount;
				fd = food_discount;
            }
            if (ad > alcohol_discount)
            {
				rd = ad - alcohol_discount;
				ad = alcohol_discount;
            }
            if (rd > room_discount)
            {
				md = rd - room_discount;
				rd = room_discount;
            }

            if (md > merchandise_discount)
				md = merchandise_discount;

			food_sales        += food_discount - fd;
			alcohol_sales     += alcohol_discount - ad;
			room_sales        += room_discount - rd;
			merchandise_sales += merchandise_discount - md;

			discount->value = fd + ad + rd + md;
        }

        if (!(discount->flags & TF_NO_REVENUE))
        {
            // add discount amount to payments & restore food sales total
            //payment += discount->value;
            balance -= discount->value;
            food_sales        = food_discount + food_no_discount;
            alcohol_sales     = alcohol_discount + alcohol_no_discount;
            room_sales        = room_discount + room_no_discount;
            merchandise_sales = merchandise_discount + merchandise_no_discount;
        }
    }
    else
    {
        food_sales        += food_discount;
        alcohol_sales     += alcohol_discount;
        room_sales        += room_discount;
        merchandise_sales += merchandise_discount;
    }

    // Calculate amount to base taxes on
    int food_tax_revenue = (food_sales - food_comp);
    int alcohol_tax_revenue = (alcohol_sales - alcohol_comp);
    int room_tax_revenue = (room_sales - room_comp);
    int merchandise_tax_revenue = (merchandise_sales - merchandise_comp);
    int total_tax_revenue = food_tax_revenue + alcohol_tax_revenue +
        room_tax_revenue + merchandise_tax_revenue;

    if (discount && (discount->flags & TF_NO_TAX))
    {
        food_tax_revenue -= discount->value;
        if (food_tax_revenue < 0)
        {
            alcohol_tax_revenue += food_tax_revenue;
            food_tax_revenue = 0;
        }
        if (alcohol_tax_revenue < 0)
        {
            room_tax_revenue += alcohol_tax_revenue;
            alcohol_tax_revenue = 0;
        }
        if (room_tax_revenue < 0)
        {
            merchandise_tax_revenue += room_tax_revenue;
            room_tax_revenue = 0;
        }
        if (merchandise_tax_revenue < 0)
            merchandise_tax_revenue = 0;
    }

    // Calculate tax amounts
    
    // if takouts are not taxed, zero out the subcheck food tax
    if ((settings->tax_takeout_food == 0) && ((check_type == CHECK_TAKEOUT) || (check_type == CHECK_TOGO))) {
       total_tax_food = 0;
       food_tax_revenue = 0;
    } else {
       total_tax_food    = settings->FigureFoodTax(food_tax_revenue, SystemTime, food_tax);
    }
    total_tax_alcohol = settings->FigureAlcoholTax(alcohol_tax_revenue, SystemTime, alcohol_tax);

    total_tax_GST = settings->FigureGST((food_tax_revenue + alcohol_tax_revenue), SystemTime, GST_tax);

	bool drinksOnly = true;
	int currFamily;
    for (Order *my_order = OrderList(); my_order != nullptr; my_order = my_order->next)
    {
		currFamily = settings->family_group[my_order->item_family];
		if(currFamily != SALESGROUP_BEVERAGE) 
			drinksOnly = false;
	}
    if (alcohol_tax == 0)
    {
        total_tax_PST = settings->FigurePST((food_tax_revenue + alcohol_tax_revenue),
                                            SystemTime, drinksOnly, PST_tax);
    }
    else
    {
        total_tax_PST = settings->FigurePST(food_tax_revenue,
                                            SystemTime, drinksOnly, PST_tax);
    }

    total_tax_HST = settings->FigureHST((food_tax_revenue + alcohol_tax_revenue),
                                        SystemTime, HST_tax);

    if (new_QST_method)
    {
        total_tax_QST = settings->FigureQST(food_tax_revenue + alcohol_tax_revenue,
                                            total_tax_GST, SystemTime, drinksOnly, QST_tax);
    }
    else
    {
        total_tax_QST = settings->FigureQST((food_tax_revenue + alcohol_tax_revenue),
                                            0, SystemTime, drinksOnly, QST_tax);
    }

    total_tax_room    = settings->FigureRoomTax(room_tax_revenue,
                                                SystemTime, room_tax);
    total_tax_merchandise = settings->FigureMerchandiseTax(merchandise_tax_revenue,
                                                           SystemTime, merchandise_tax);
    total_tax_VAT = settings->FigureVAT(total_tax_revenue , SystemTime, VAT_tax);

    if (discount && (discount->flags & TF_COVER_TAX))
    {
        // Extend discount/comp to cover taxes also
        int amount = 0;
        if (discount->flags & TF_IS_PERCENT)
        {
            int per = discount->amount;
            if (per > 10000)
                per = 10000;
            amount = (int) ((Flt) ( total_tax_food + total_tax_room +
                                    total_tax_merchandise + total_tax_GST + total_tax_PST +
                                    total_tax_HST + total_tax_QST + total_tax_VAT ) * PercentToFlt(per));
            if (discount_alcohol)
                amount += (int) ((Flt) total_tax_alcohol * PercentToFlt(per));
        }
        else if (discount->amount > discount->value)
        {
            // amount of tax that can be discounted
            int tax_dis = total_tax_food + total_tax_room +
                total_tax_merchandise + total_tax_GST + 
                total_tax_PST + total_tax_HST + total_tax_QST +
                total_tax_VAT;
            if (discount_alcohol) 
				tax_dis += total_tax_alcohol;

            // amount still left in discount
            int over = discount->amount - discount->value;

            if (tax_dis > over)
				amount = over;    // move tax than discount left
            else
				amount = tax_dis; // discount covers all tax
        }

        if (amount > 0)
        {
            // adjust discount's actual value & check balance
            discount->value += amount;
            balance -= amount;
        }
    }

    // Generate totals
    total_sales = (food_sales + alcohol_sales + untaxed_sales + room_sales + merchandise_sales);

    if (IsTaxExempt())
    {
        total_cost = total_sales - item_comps;
    }
    else
    {
        total_cost  = (total_sales + total_tax_food + 
                       total_tax_alcohol + total_tax_merchandise + 
                       total_tax_room + total_tax_GST + 
                       total_tax_PST + total_tax_HST + total_tax_QST +
                       total_tax_VAT) - item_comps;
    }
        
    if (gratuity)
        total_cost += -gratuity->value;
    balance += total_cost;

    // Price Rounding
    int price_rounding = archive ? archive->price_rounding : settings->price_rounding;
    int dis = 0;
    if (discount && !(discount->flags & TF_NO_REVENUE))
        dis = discount->value;
    int pennies = (total_cost - dis) % 5;
    if (price_rounding == ROUNDING_DROP_PENNIES)
    {
        // Calculate amount to round down by
        if (pennies > 0 && total_cost > 5)
        {
            payptr = NewPayment(TENDER_MONEY_LOST, 0, pennies, 0);
            balance -= pennies;
        }
    }
    else if (price_rounding == ROUNDING_UP_GRATUITY && gratuity)
    {
        // Increase gratuity if there is one
        if (pennies > 0)
        {
            int amt = 5 - pennies;
            gratuity->value -= amt;
            total_cost += amt;
            balance    += amt;
        }
    }

    // Deal with excess payment
    if (balance < 0)
    {
        int tip  = 0;
        int over = -balance - max_change;
        if (over > 0)
        {
            if (max_tip > max_change)
            {
                tip   = Min(max_tip - max_change, over);
                over -= tip;
            }

            if (over > 0)
            {
                NewPayment(TENDER_OVERAGE, 0, 0, over);
                balance += over;
            }
        }

        if (tip > 0)
        {
            NewPayment(TENDER_CAPTURED_TIP, 0, 0, tip);
            balance += tip;
        }

        if (balance < 0)
        {
            NewPayment(TENDER_CHANGE, 0, 0, -balance);
            balance = 0;
        }
    }
    return 0;
}

int SubCheck::TabRemain()
{
    FnTrace("SubCheck::TabRemain()");
    int remain = 0;

    if (tab_total > 0)
        remain = tab_total - total_cost;

    return remain;
}

int SubCheck::SettleTab(Terminal *term, int payment_type, int payment_id, int payment_flags)
{
    FnTrace("SubCheck::SettleTab()");
    int retval = 0;
    Payment *paymnt = nullptr;

    if (payment_type == TENDER_CREDIT_CARD || payment_type == TENDER_DEBIT_CARD)
        return retval;
    FigureTotals(term->GetSettings());
    paymnt = FindPayment(payment_type);
    if (paymnt != nullptr && (paymnt->flags & TF_IS_TAB))
    {
        retval = paymnt->value;
        Remove(paymnt);
        delete(paymnt);
    }

    return retval;
}

int SubCheck::FinalizeOrders()
{
    FnTrace("SubCheck::FinalizeOrders()");
    for (Order *order = OrderList(); order != nullptr; order = order->next)
    {
        order->Finalize();
        for (Order *mod = order->modifier_list; mod != nullptr; mod = mod->next)
            mod->Finalize();
    }
    return 0;
}

int SubCheck::ConsolidateOrders(Settings *settings, int relaxed)
{
    FnTrace("SubCheck::ConsolidateOrders()");

    Order *thisOrder = OrderList();
    while (thisOrder)
    {
        Order *o2 = thisOrder->next;
        while (o2)
        {
            Order *ptr = o2->next;
            if (thisOrder->status == o2->status &&
                (!(thisOrder->status & ORDER_FINAL) || relaxed) &&
                thisOrder->seat == o2->seat &&
                thisOrder->user_id == o2->user_id &&
                thisOrder->item_cost == o2->item_cost &&
                thisOrder->qualifier == o2->qualifier &&
                thisOrder->modifier_list == nullptr &&
                o2->modifier_list == nullptr &&
                strcmp(thisOrder->item_name.Value(), o2->item_name.Value()) == 0)
            {
                Remove(o2, 0);
                thisOrder->count += o2->count;
                delete o2;
            }
            o2 = ptr;
        }
        thisOrder = thisOrder->next;
    }

    if(settings)
        FigureTotals(settings);

    return 0;
}

int SubCheck::ConsolidatePayments(Settings *settings)
{
    FnTrace("SubCheck::ConsolidatePayments()");
    Payment *payptr = PaymentList();
    Payment *p2;
    Payment *ptr;
    int tt;

    while (payptr != nullptr)
    {
        p2 = payptr->next;
        while (p2)
        {
            ptr = p2->next;
            tt = payptr->tender_type;
            if (tt != TENDER_CREDIT_CARD && tt != TENDER_DEBIT_CARD &&
                tt == p2->tender_type && payptr->flags == p2->flags &&
                payptr->drawer_id == p2->drawer_id && payptr->user_id == p2->user_id)
            {
                Remove(p2, 0);
                payptr->amount += p2->amount;
                delete p2;
            }
            p2 = ptr;
        }
        payptr = payptr->next;
    }

    if (settings)
        FigureTotals(settings);

    return 0;
}

int SubCheck::Void()
{
    FnTrace("SubCheck::Void()");

    if (PaymentList())
        return 1;  // can't void a check with payments (use comp instead)

    status = CHECK_VOIDED;
    for (Order *order = OrderList(); order != nullptr; order = order->next)
        order->status |= ORDER_COMP;
    return 0;
}

int SubCheck::SeatsUsed()
{
    FnTrace("SubCheck::SeatsUsed()");
    int seat_count[64];
    int count = 0;
    int i;

    for (i = 0; i < 64; ++i)
        seat_count[i] = 0;

    for (Order *order = OrderList(); order != nullptr; order = order->next)
    {
        if (order->seat < 64 && ++seat_count[order->seat] == 1)
            ++count;
    }
    return count;
}

void spacefill(genericChar* buf,size_t n)
{
	bool zhit=false;
	for(size_t i=0;i<n;i++)
	{
		zhit=zhit || buf[i]==0;
		if(zhit)
		{
			buf[i]=' ';
		}
	}
}

int SubCheck::PrintReceipt(Terminal *term, Check *check, Printer *printer, Drawer *drawer, int open_drawer)
{
    FnTrace("SubCheck::PrintReceipt()");
    if (check == nullptr)
        return 1;

    if (printer == nullptr)
    {
        ReportError("No printer to print receipt");
        return 1;  // no printer
    }

    // FIX - check receipt should be done with a report
    printer->Start();

    if (drawer && open_drawer)
        printer->OpenDrawer(drawer->position);

    System *sys = term->system_data;
    Settings *settings = &(sys->settings);
    ItemDB* items= &(sys->menu);
    Employee *e = sys->user_db.FindByID(check->WhoGetsSale(settings));
    genericChar str[256];
    genericChar str1[64];
    genericChar str2[64];

    ConsolidateOrders(settings, 1);

    if (e && e->training)
    {
        printer->Write("                                 ", PRINT_UNDERLINE | PRINT_RED);
        printer->NewLine();
        printer->Write(" ** TRAINING **", PRINT_LARGE);
        printer->Write("   This Is NOT A Valid Receipt");
        printer->Write("                                 ", PRINT_UNDERLINE | PRINT_RED);
        printer->NewLine();
    }

    int flag = 0;
    int lines = 0;
    int i;
    for (i = 0; i < 4; ++i)
    {
        if (settings->receipt_header[i].size() > 0)
        {
            if (lines > 0)
            {
                printer->LineFeed(lines);
                ++lines;
            }
            flag = 1;
            printer->Write(settings->receipt_header[i].Value());
        }
        else if (flag)
            ++lines;
    }
    if (flag)
        printer->LineFeed(2 + settings->receipt_header_length);

    switch (check->CustomerType())
	{
    case CHECK_RESTAURANT:
        snprintf(str1, sizeof(str1), "%s %s #%d", term->Translate("Table"), check->Table(), number);
        break;
    case CHECK_HOTEL:
        snprintf(str1, sizeof(str1), "%s %s", term->Translate("Room"), check->Table());
        break;
    case CHECK_TAKEOUT:
        snprintf(str1, sizeof(str1), "%s",term->Translate("Take Out"));
        break;
    case CHECK_FASTFOOD:
        snprintf(str1, sizeof(str1), "%s",term->Translate("Fast"));
        break;
    case CHECK_CATERING:
        snprintf(str1, sizeof(str1),"%s", term->Translate("Catering"));
        break;
    case CHECK_DELIVERY:
        snprintf(str1, sizeof(str1), "%s",term->Translate("Deliver"));
        break;
    case CHECK_RETAIL:
        snprintf(str1, sizeof(str1), "%s",term->Translate("Retail"));
        break;
    case CHECK_DINEIN:
        snprintf(str1, sizeof(str1), "Here");
        break;
    case CHECK_TOGO:
        snprintf(str1, sizeof(str1), "To Go");
        break;
    case CHECK_CALLIN:
        snprintf(str1, sizeof(str1), "Pick Up");
        break;
    default:
        str1[0] = '\0';
        break;
	}

    if (e)
        snprintf(str2, sizeof(str2), "%s: %s", term->Translate("Server"), e->system_name.Value());
    else
        snprintf(str2, sizeof(str2), "%s: %s", term->Translate("Server"), term->Translate(UnknownStr));

    snprintf(str, sizeof(str), "%-14s%19s", str1, str2);
    printer->Write(str, settings->table_num_style);

    if (drawer == nullptr)
    {
        if (check->archive)
            drawer = check->archive->DrawerList()->FindBySerial(drawer_id);
        else
            drawer = sys->DrawerList()->FindBySerial(drawer_id);
    }

    if (drawer)
	{
		if (drawer->IsServerBank())
			strcpy(str1, term->Translate("Server Bank"));
		else
			snprintf(str1, sizeof(str1), "%s %d", term->Translate("Drawer"), drawer->number);

		Employee *cashier = nullptr;
		if (settle_user > 0)
			cashier = sys->user_db.FindByID(settle_user);
		else
			cashier = sys->user_db.FindByID(drawer->owner_id);

		if (cashier)
			snprintf(str2, sizeof(str2), "%s: %s", term->Translate("Cashier"),
					cashier->system_name.Value());
		else
			snprintf(str2, sizeof(str2), "%s: %s", term->Translate("Cashier"),
					term->Translate(UnknownStr));

		snprintf(str, sizeof(str), "%-14s%19s", str1, str2);
		printer->Write(str);
	}

    printer->Write(term->TimeDate(SystemTime, TD0));
    if (check->CustomerType() == CHECK_TAKEOUT ||
        check->CustomerType() == CHECK_DELIVERY ||
        check->CustomerType() == CHECK_CATERING)
    {
        snprintf(str, sizeof(str), "Due:  %s", term->TimeDate(check->date, TD_DATETIME));
        printer->Write(str, PRINT_BOLD);
    }
    if (check->serial_number > 0)
    {
        snprintf(str, sizeof(str), "#%04d", check->serial_number % 10000);
        printer->Write(str, settings->order_num_style);
    }
    printer->NewLine();

    check->PrintCustomerInfo(printer, 0);
    
    std::list<Order*> tickets;

    for (Order *order = OrderList(); order != nullptr; order = order->next)
    {
	if(order->item_type == ITEM_ADMISSION)
	{
		tickets.push_back(order);
	}
        if (order->item_type == ITEM_POUND)
        {
            snprintf(str1, sizeof(str1), "%.2f %s                              ",
                    ((Flt)order->count / 100), order->Description(term));
        }
        else
        {
            snprintf(str1, sizeof(str1), "%d %s                              ",
                    order->count, order->Description(term));
        }
        if (order->status & ORDER_COMP)
            strcpy(str2, "COMP");
        else
            term->FormatPrice(str2, order->cost);
        str1[32 - strlen(str2)] = '\0';
        snprintf(str, sizeof(str), "%s %s", str1, str2);
        printer->Write(str);

        for (Order *mod = order->modifier_list; mod != nullptr; mod = mod->next)
		{
            if (settings->receipt_all_modifiers > 0 || mod->cost != 0 || (mod->status & ORDER_COMP))
            {
                if (settings->receipt_all_modifiers > 0 && mod->cost == 0)
                    str2[0] = '\0';
                else if (mod->status & ORDER_COMP)
                    strcpy(str2, "COMP");
                else
                    term->FormatPrice(str2, mod->cost);
                str2[23] = '\0';
                snprintf(str, sizeof(str), "   %-23s %6s", mod->Description(term), str2);
                printer->Write(str);
            }
		}
    }

    int change_value = TotalPayment(TENDER_CHANGE);
    Payment *gratuity = FindPayment(TENDER_GRATUITY);
    Payment *pennies  = FindPayment(TENDER_MONEY_LOST);
    Credit *cr = CurrentCredit();

    printer->Write("                           ------");
    snprintf(str, sizeof(str), "              Sales Total %7s", term->FormatPrice(raw_sales - item_comps));
    printer->Write(str);

    int tc = raw_sales - item_comps;
    if (IsTaxExempt() == 0)
        tc += TotalTax();
    if (pennies)
        tc += -pennies->amount;

    Payment *pay = nullptr;
    // check for and print coupons, discounts, and comps
    if (PaymentList())
    {
        for (pay = PaymentList(); pay != nullptr; pay = pay->next)
		{
            if (!pay->Suppress())
            {
                if (pay->tender_type == TENDER_COUPON ||
                    pay->tender_type == TENDER_DISCOUNT ||
                    pay->tender_type == TENDER_COMP)
                {
                    snprintf(str, sizeof(str), "%25.25s %7s", pay->Description(settings),
                            term->FormatPrice(-pay->value));
                    printer->Write(str);
                    tc -= pay->value;
                }
            }
        }

    }

	//for canada implementation
	if(settings->tax_HST <= 0)
    {
        if (settings->tax_GST > 0)
        {
            snprintf(str, sizeof(str), "                     GST: %7s", term->FormatPrice(total_tax_GST));
            printer->Write(str);
        }

        str[0] = '\0';
        if (settings->tax_QST > 0)
			snprintf(str, sizeof(str), "                     QST: %7s", term->FormatPrice(total_tax_QST));
        else if (settings->tax_PST > 0)
			snprintf(str, sizeof(str), "                     PST: %7s", term->FormatPrice(total_tax_PST));
        if (str[0] != '\0')
            printer->Write(str);
	}
	else
	{
		snprintf(str, sizeof(str), "                      HST: %7s", term->FormatPrice(total_tax_HST));
		printer->Write(str);
	}
    if (settings->tax_VAT > 0)
        snprintf(str, sizeof(str), "                      VAT: %7s", term->FormatPrice(total_tax_VAT));
		
    snprintf(str, sizeof(str), "                Total Tax %7s", term->FormatPrice(TotalTax()));
    printer->Write(str);

    if (IsTaxExempt())
    {
        snprintf(str, sizeof(str), "               Tax Exempt %7s", term->FormatPrice(-TotalTax()));
        printer->Write(str);
        snprintf(str, sizeof(str), "Tax ID:  %s\n", tax_exempt.Value());
        printer->Write(str, PRINT_BOLD);
    }


    if (gratuity)
    {
        snprintf(str, sizeof(str), "%25.25s %7s", gratuity->Description(settings),
                term->FormatPrice(-gratuity->value));
        printer->Write(str);
    }

    printer->Write("                           ------");
    snprintf(str, sizeof(str), "                    Total %7s", term->FormatPrice(tc, 1));
    printer->Write(str);

    if (PaymentList())
    {
        // print all other payments
        for (pay = PaymentList(); pay != nullptr; pay = pay->next)
		{
            if (!pay->Suppress())
            {
                if (pay->tender_type != TENDER_COUPON &&
                    pay->tender_type != TENDER_DISCOUNT &&
                    pay->tender_type != TENDER_COMP)
                {
                    Credit *tmp = pay->credit;
                    str[0] = '\0';
                    if (tmp && settings->authorize_method == CCAUTH_NONE)
                    {
                        snprintf(str, sizeof(str), "%25.25s %7s",
                                tmp->CreditTypeName(), term->FormatPrice(pay->value));
                    }
                    else
                    {
                        snprintf(str, sizeof(str), "%25.25s %7s", pay->Description(settings),
                                term->FormatPrice(pay->value));
                    }
                    printer->Write(str);
                    if (tmp && settings->authorize_method == CCAUTH_NONE)
                    {
                        printer->LineFeed(1);
                        snprintf(str, sizeof(str), "  Account       %s", tmp->PAN(settings->show_entire_cc_num));
                        printer->Write(str);
                        snprintf(str, sizeof(str), "  Card Holder   %s", tmp->Name());
                        printer->Write(str);
                        snprintf(str, sizeof(str), "  Card Expires  %s", tmp->ExpireDate());
                        printer->Write(str);
                        snprintf(str, sizeof(str), "  Authorization %s", tmp->Approval());
                    }
                }
            }
		}

        printer->LineFeed(1);
        if (payment > 0)
        {
            snprintf(str, sizeof(str), "          Amount Tendered %7s", term->FormatPrice(payment, 1));
            printer->Write(str);
        }
        if (balance > 0)
        {
            snprintf(str, sizeof(str), "              Balance Due %7s", term->FormatPrice(balance, 1));
            printer->Write(str);
        }
        else
        {
            snprintf(str, sizeof(str), "                   Change %7s", term->FormatPrice(change_value, 1));
            printer->Write(str);
        }

        if (item_comps > 0)
        {
            printer->LineFeed(1);
            snprintf(str, sizeof(str), "               Total Comp %7s", term->FormatPrice(item_comps, 1));
            printer->Write(str);
        }
    }

    // If we have an authorize method, we assume the tip, total, and
    // signature lines are being printed on the credit card receipt
    // (Credit::PrintReceipt()) so we won't print them here.
    if (cr && status == CHECK_OPEN && settings->authorize_method == CCAUTH_NONE)
    {
        printer->LineFeed(2);
        printer->Put("       TIP ", PRINT_WIDE);
        printer->Write("           ", PRINT_UNDERLINE);
        printer->LineFeed(1);
        printer->Put("     TOTAL ", PRINT_WIDE);
        printer->Write("           ", PRINT_UNDERLINE);
        printer->LineFeed(2);
        printer->Put("SIGNATURE X");
        printer->Write("                      ", PRINT_UNDERLINE);
    }

    flag  = 0;	// found footer text, output initial 2 blank links
    lines = 0;	// blank footer lines skipped
    for (i = 0; i < 4; ++i)
    {
        if (settings->receipt_footer[i].size() > 0)
        {
            if (flag == 0)
            {
                printer->LineFeed(2);
                flag = 1;
            }
            else if (lines > 0)
            {
                printer->LineFeed(lines);
                lines = 0;
            }
            printer->Write(settings->receipt_footer[i].Value());
        }
        else if (flag)
            ++lines;
    }
  
  
    genericChar charbuffer[15];
    genericChar serialnumber[15];
    genericChar blankbuffer[15];
    genericChar datebuffer[15];
    
    memset(charbuffer,0,15);
    memset(serialnumber,0,15);
    memset(blankbuffer,0,15);
    memset(datebuffer,0,15);
    
    
    time_t rawtime;
    struct tm* dateinfo;
    time(&rawtime);
    dateinfo=localtime(&rawtime);
    strftime(datebuffer,14,"%a, %b %e",dateinfo);
    
    int leftflags=PRINT_LARGE;
    int rightflags=PRINT_TALL;
    
    int ticket_count_on_subcheck=0;
    
    if(tickets.size() > 0)
    {
	    printer->LineFeed(8);
    }
    //PRINT TICKETS
    for(std::list<Order*>::iterator loi=tickets.begin();loi!=tickets.end();++loi)
    {
	Order* ord=*loi;
	int count=ord->count;
	SalesItem* si=ord->Item(items);
	for(i=0;i<count;i++)
	{
		//sys->NewSerialNumber()
		
		snprintf(serialnumber,14,"%d-%d",check->serial_number,ticket_count_on_subcheck);
		ticket_count_on_subcheck++;
		//print ticket and stub here.
		printer->CutPaper(1);
		
		Str tname;
		admission_parse_hash_name(tname,si->item_name);
		snprintf(charbuffer,14,"%s",tname.Value());
		spacefill(charbuffer,14);
		printer->Put(charbuffer,leftflags);
		printer->Put(charbuffer,rightflags);
		printer->NewLine();
		
		spacefill(datebuffer,14);
		printer->Put(datebuffer,leftflags);
		printer->Put(datebuffer,rightflags);
		printer->NewLine();
		
		snprintf(charbuffer,14,"%s",si->event_time.Value());
		spacefill(charbuffer,14);
		printer->Put(charbuffer,leftflags);
		printer->Put(charbuffer,rightflags);
		printer->NewLine();
		
		snprintf(charbuffer,14,"%s",si->location.Value());
		spacefill(charbuffer,14);
		printer->Put(charbuffer,leftflags);
		printer->Put(charbuffer,rightflags);
		printer->NewLine();
		
		snprintf(charbuffer,14,"1 %s",si->price_label.Value());
		spacefill(charbuffer,14);
		printer->Put(charbuffer,leftflags);
		printer->Put(charbuffer,rightflags);
		printer->NewLine();
		
		snprintf(charbuffer,14,"%s",term->FormatPrice(ord->cost));//Price
		spacefill(charbuffer,14);
		printer->Put(charbuffer,leftflags);
		printer->Put(serialnumber,rightflags);
		printer->NewLine();
		
		snprintf(charbuffer,14,"%s",settings->store_name.Value());//Store name
		spacefill(charbuffer,14);
		printer->Put(charbuffer,leftflags);
		printer->NewLine();
		
		snprintf(charbuffer,14,"%s",serialnumber);//Store name
		spacefill(charbuffer,14);
		printer->Put(charbuffer,leftflags);
		printer->NewLine();
		
		printer->NewLine();
		printer->NewLine();
	}
	
	Str on,ohsh;
	admission_parse_hash_name(on,ord->item_name);
	admission_parse_hash_ltime_hash(ohsh,ord->item_name);
	
	for (SalesItem *sicheck = items->ItemList(); sicheck != nullptr; sicheck = sicheck->next)
	{
		if(sicheck->type == ITEM_ADMISSION)
		{
			Str ckhsh,ckn;
			admission_parse_hash_name(ckn,sicheck->item_name);
			admission_parse_hash_ltime_hash(ckhsh,sicheck->item_name);
			if(on == ckn && ohsh == ckhsh)
			{
				int a=sicheck->available_tickets.IntValue();
				a-=count;
				if(a < 0)
				{
					a=0;
				}
				sicheck->available_tickets.Set(a);
			}
		}
	}
    }
    if(tickets.size() > 0)
    {
	    printer->CutPaper(1);
    }
    printer->End();

    return 0;
}

int SubCheck::ReceiptReport(Terminal *t, Check *c, Drawer *d, Report *r)
{
    FnTrace("SubCheck::ReceiptReport()");
    // FIX - Finish ReceiptReport()
    return 0;
}

const genericChar* SubCheck::StatusString(Terminal *t)
{
    FnTrace("SubCheck::StatusString()");
    const genericChar* s =
        FindStringByValue(status, CheckStatusValue, CheckStatusName, UnknownStr);
    return t->Translate(s);
}

int SubCheck::IsSeatOnCheck(int seat)
{
    FnTrace("SubCheck::IsSeatOnCheck()");
    for (Order *order = OrderList(); order != nullptr; order = order->next)
        if (order->seat == seat)
            return 1;
    return 0;
}

Order *SubCheck::LastOrder(int seat)
{
    FnTrace("SubCheck::LastOrder()");
    for (Order *order = OrderListEnd(); order != nullptr; order = order->fore)
        if (seat < 0 || order->seat == seat)
        {
            // order found - now return last modifier (otherwise just return order)
            if (order->modifier_list)
            {
                order = order->modifier_list;
                while (order->next)
                    order = order->next;
                return order;
            }
            else
                return order;
        }
    return nullptr;
}

Order *SubCheck::LastParentOrder(int seat)
{
    FnTrace("SubCheck::LastParentOrder()");
    for (Order *order = OrderListEnd(); order != nullptr; order = order->fore)
        if (seat < 0 || order->seat == seat)
        {
            // (parent) order found - no need to search for its last modifier
            return order;
        }
    return nullptr;
}

int SubCheck::TotalTip()
{
    FnTrace("SubCheck::TotalTip()");
    int tip = 0, tt;
    for (Payment *payptr = PaymentList(); payptr != nullptr; payptr = payptr->next)
    {
        tt = payptr->tender_type;
        if (tt == TENDER_CAPTURED_TIP || tt == TENDER_CHARGED_TIP)
            tip += payptr->value;
        else if (tt == TENDER_GRATUITY)
            tip += -payptr->value;
    }
    return tip;
}

void SubCheck::ClearTips()
{
    FnTrace("SubCheck::ClearTips()");
    int tt;
    for (Payment *payptr = PaymentList(); payptr != nullptr; payptr = payptr->next)
    {
        tt = payptr->tender_type;
        if (tt == TENDER_CAPTURED_TIP || tt == TENDER_CHARGED_TIP)
            payptr->value = 0;
    }
}

int SubCheck::GrossSales(Check *check, Settings *settings, int sales_group)
{
    FnTrace("SubCheck::GrossSales()");
    if (status != CHECK_CLOSED && check->CustomerType() != CHECK_HOTEL)
        return 0;

    int family;
    int sales = 0;
    for (Order *order = OrderList(); order != nullptr; order = order->next)
    {
        family = order->item_family;
        if (sales_group == 0 || (family != FAMILY_UNKNOWN && settings->family_group[family] == sales_group))
        {
            order->FigureCost();
            sales += order->total_cost;
        }
    }
    return sales;
}

int SubCheck::Settle(Terminal *term)
{
    FnTrace("SubCheck::Settle()");
    Employee *employee = term->user;

    if (balance != 0 || (TabRemain() > 0 && term->is_bar_tab > 0) || employee == nullptr)
        return 1;  // Check not fully settled

    for (Payment *paymnt = PaymentList(); paymnt != nullptr; paymnt = paymnt->next)
    {
        paymnt->flags |= TF_FINAL;
        if (paymnt->credit)
        {
            paymnt->credit->Finalize(term);
            if (paymnt->credit->IsVoided() || paymnt->credit->IsRefunded())
            {
                Payment *nextpaymnt = paymnt->next;
                Remove(paymnt);
                paymnt = nextpaymnt;
            }
        }
    }

    if (settle_user == 0)
        settle_user = employee->id;
    if (! settle_time.IsSet())
        settle_time = SystemTime;

    return 0;
}

int SubCheck::Close(Terminal *term)
{
    FnTrace("SubCheck::Close()");
    if (Settle(term))
        return 1;

    if (status != CHECK_OPEN)
        return 1;

    status = CHECK_CLOSED;
    return 0;
}

Payment *SubCheck::FindPayment(int ptype, int pid)
{
    FnTrace("SubCheck::FindPayment()");

    for (Payment *paymnt = PaymentList(); paymnt != nullptr; paymnt = paymnt->next)
    {
        if (paymnt->tender_type == ptype &&
            (paymnt->tender_id == pid || pid < 0))
        {
            return paymnt;
        }
    }

    return nullptr;
}

int SubCheck::TotalPayment(int ptype, int pid)
{
    FnTrace("SubCheck::TotalPayment()");
    int total = 0;
    Payment *payptr = PaymentList();

    while (payptr != nullptr)
    {
        if (payptr->tender_type == ptype && (pid < 0 || payptr->tender_id == pid))
        {
            total += payptr->value;
        }
        payptr = payptr->next;
    }

    return total;
}

Order *SubCheck::FindOrder(int order_num, int seat)
{
    FnTrace("SubCheck::FindOrder()");
	for (Order *thisOrder = OrderList(); thisOrder != nullptr; thisOrder = thisOrder->next)
    {
		if (seat < 0 || thisOrder->seat == seat)
		{
			if (order_num <= 0)
				return thisOrder;

			--order_num;
			for (Order *mod = thisOrder->modifier_list; mod != nullptr; mod = mod->next)
			{
				if (order_num <= 0)
					return mod;

				--order_num;
			}
		}
    }
	return nullptr;
}

int SubCheck::CompOrder(Settings *settings, Order *ptrOrder, int comp)
{
	FnTrace("SubCheck::CompOrder()");

	if (ptrOrder == nullptr)
		return 1;

	if (ptrOrder->count > 1 && ptrOrder->item_type != ITEM_POUND)
	{
		Order *o2 = ptrOrder;
		ptrOrder = RemoveOne(o2);

		// insert comp ptrOrder after original
		order_list.AddAfterNode(o2, ptrOrder);
	}

	if (comp)
	{
		ptrOrder->status |= ORDER_COMP;
	}
	else if (ptrOrder->status & ORDER_COMP)
	{
		ptrOrder->status -= ORDER_COMP;
	}

	Order *mod = ptrOrder->modifier_list;
	while (mod)
	{
		if (comp)
			mod->status |= ORDER_COMP;
		else if (mod->status & ORDER_COMP)
			mod->status -= ORDER_COMP;

		mod = mod->next;
	}

	ConsolidateOrders(settings);
	FigureTotals(settings);

	return 0;
}

int SubCheck::OrderCount(int seat)
{
    FnTrace("SubCheck::OrderCount()");
    int count = 0;
    for (Order *order = OrderList(); order != nullptr; order = order->next)
    {
        if (seat < 0 || order->seat == seat)
        {
            ++count;
            Order *mod = order->modifier_list;
            while (mod)
            {
                ++count;
                mod = mod->next;
            }
        }
    }
    return count;
}

int SubCheck::OrderPage(Order *order, int lines_per_page, int seat)
{
    FnTrace("SubCheck::OrderPage()");
    int page = 0, line = 0;
    for (Order *my_order = OrderList(); my_order != nullptr; my_order = my_order->next)
    {
        if (seat < 0 || my_order->seat == seat)
        {
            Order *mod = my_order->modifier_list;
            while (mod)
            {
                if (mod == my_order)
                    return page;
                ++line;
                if (line >= lines_per_page)
                {
                    line = 0;
                    ++page;
                }
                mod = mod->next;
            }
            if (my_order == order)
                return page;
            ++line;
            if (line >= lines_per_page)
            {
                line = 0;
                ++page;
            }
        }
    }
    return -1;  // order not found
}

Payment *SubCheck::NewPayment(int tender, int pid, int pflags, int pamount)
{
    FnTrace("SubCheck::NewPayment()");
    Payment *payptr = new Payment(tender, pid, pflags, pamount);
    Add(payptr, 0);
    return payptr;
}

Credit *SubCheck::CurrentCredit()
{
    FnTrace("SubCheck::CurrentCredit()");
    for (Payment *payptr = PaymentList(); payptr != nullptr; payptr = payptr->next)
        if (payptr->credit)
            return payptr->credit;
    return nullptr;
}

int SubCheck::IsEqual(SubCheck *sc)
{
    FnTrace("SubCheck::IsEqual()");
    Order *order1 = OrderList();
    Order *order2 = sc->OrderList();
    while (order1 && order2)
    {
        if (!order1->IsEqual(order2))
            return 0;
        order1 = order1->next;
        order2 = order2->next;
    }
    if (order1 || order2)
        return 0;

    Payment *payment1 = PaymentList();
    Payment *payment2 = sc->PaymentList();
    while (payment1 && payment2)
	{
		if (! payment1->IsEqual(payment2))
			return 0;

		payment1 = payment1->next;
		payment2 = payment2->next;
	}

    if (payment1 || payment2)
        return 0;

    return 1;
}

int SubCheck::IsTaxExempt()
{
    FnTrace("SubCheck::IsTaxExempt()");
    int retval = 0;

    if (tax_exempt.size() > 0)
        retval = 1;

    return retval;
}

int SubCheck::IsBalanced()
{ 
    FnTrace("SubCheck::IsBalanced()");

    return (balance == 0 && TabRemain() == 0);
}

int SubCheck::HasAuthedCreditCards()
{
    FnTrace("SubCheck::HasAuthedCreditCards()");
    int retval = 0;
    Payment *paymnt = PaymentList();

    while (paymnt != nullptr && retval == 0)
    {
        if (paymnt->credit != nullptr && paymnt->credit->IsAuthed(1))
            retval = 1;
        paymnt = paymnt->next;
    }

    return retval;
}

int SubCheck::HasOpenTab()
{
    FnTrace("SubCheck::HasOpenTab()");
    int retval = 0;
    Payment *paymnt = PaymentList();

    while (retval == 0 && paymnt != nullptr)
    {
        if (paymnt->flags & TF_IS_TAB)
            retval = 1;
        paymnt = paymnt->next;
    }

    return retval;
}

int SubCheck::OnlyCredit()
{
    FnTrace("SubCheck::OnlyCredit()");
    int retval = 1;
    Payment *paymnt = PaymentList();

    while (paymnt != nullptr && retval >= 1)
    {
        if (paymnt->tender_type != TENDER_CREDIT_CARD &&
            paymnt->tender_type != TENDER_DEBIT_CARD &&
            paymnt->tender_type != TENDER_CHANGE)
        {
            retval = 0;
        }
        paymnt = paymnt->next;
    }

    return retval;
}

int SubCheck::SetBatch(const char* termid, const char* batch)
{
    FnTrace("SubCheck::SetBatch()");
    int retval = 1;
    Payment *paymnt = PaymentList();

    if (status == CHECK_CLOSED)
    {
        while (paymnt != nullptr)
        {
            retval = paymnt->SetBatch(termid, batch);
            paymnt = paymnt->next;
        }
    }

    return retval;
}


/**** Order Class ****/
// Constructors
Order::Order()
{
    FnTrace("Order::Order()");
    item_cost       = 0;
    item_type       = ITEM_NORMAL;
    item_family     = FAMILY_UNKNOWN;
    sales_type      = SALES_FOOD;
    call_order      = 1;
    next            = nullptr;
    fore            = nullptr;
    parent          = nullptr;
    count           = 1;
    status          = 0;
    cost            = 0;
    modifier_list   = nullptr;
    qualifier       = QUALIFIER_NONE;
    user_id         = 0;
    page_id         = 0;
    seat            = 0;
    discount        = 0;
    total_cost      = 0;
    total_comp      = 0;
    printer_id      = PRINTER_DEFAULT;
    employee_meal   = 0;
    is_reduced      = 0;
    reduced_cost    = 0;
    auto_coupon_id  = -1;
}

// helper to adjust cost if tax is already included
// scaling minimizes discrepency with final total after tax is added
static int adjust_cost(int cost, Flt tax, int type, Settings *settings, Terminal *term)
{
	int inclusive = -1;
	if (term)
		inclusive = term->tax_inclusive[type];		// per-terminal setting
	if (inclusive < 0)						
		inclusive = settings->tax_inclusive[type]; 	// use global default
    if (inclusive)
        //return int(cost/(1.0+tax) + tax);			// for round-up
        return int(cost/(1.0+tax) + 0.5);			// for rounding
    return cost;
}

Order::Order(Settings *settings, SalesItem *item, Terminal *term, int price)
{
    FnTrace("Order::Order(Settings, SalesItem, int)");
    
    if (term)
    	qualifier   = term->qualifier;
    else
		qualifier   = QUALIFIER_NONE;
    item_name = item->item_name; //item_name;
    if (price >= 0)
        item_cost = price;
    else
        item_cost   = item->Price(settings, qualifier);
    item_type       = item->type;
    item_family     = item->family;
    sales_type      = item->sales_type;
    call_order      = item->call_order;
    allow_increase  = item->allow_increase;
    ignore_split    = item->ignore_split;
    next            = nullptr;
    fore            = nullptr;
    parent          = nullptr;
    count           = 1;
    status          = 0;
    cost            = 0;
    modifier_list   = nullptr;
    user_id         = 0;
    page_id         = 0;
    seat            = 0;
    discount        = 0;
    total_cost      = 0;
    total_comp      = 0;
    printer_id      = PRINTER_DEFAULT;
    employee_meal   = 0;
    is_reduced      = 0;
    reduced_cost    = 0;
    auto_coupon_id  = -1;

    // remove tax if already included in cost
    if (sales_type & SALES_UNTAXED)
		;
    else if (sales_type & SALES_ALCOHOL)
		item_cost = adjust_cost(item_cost, settings->tax_alcohol, 2, settings, term);
    else if (sales_type & SALES_MERCHANDISE)
    	item_cost = adjust_cost(item_cost, settings->tax_merchandise, 3, settings, term);
    else if (sales_type & SALES_ROOM)
        item_cost = adjust_cost(item_cost, settings->tax_room, 1, settings, term);
    else
        item_cost = adjust_cost(item_cost, settings->tax_food, 0, settings, term);
}

Order::Order(const genericChar* name, int price)
{
    FnTrace("Order::Order(const char* , int)");
    item_name.Set(name);
    item_cost       = price;	// Note no tax-inclusive adjustment
    item_type       = ITEM_NORMAL;
    item_family     = FAMILY_MERCHANDISE;
    sales_type      = SALES_FOOD;
    call_order      = 4;
    allow_increase  = 1;
    ignore_split    = 0;
    next            = nullptr;
    fore            = nullptr;
    parent          = nullptr;
    count           = 1;
    status          = 0;
    cost            = 0;
    modifier_list   = nullptr;
    qualifier       = QUALIFIER_NONE;
    user_id         = 0;
    page_id         = 0;
    seat            = 0;
    discount        = 0;
    total_cost      = 0;
    total_comp      = 0;
    printer_id      = PRINTER_DEFAULT;
    employee_meal   = 0;
    is_reduced      = 0;
    reduced_cost    = 0;
    auto_coupon_id  = -1;
}

// Destructor
Order::~Order()
{
    FnTrace("Order::~Order()");
    while (modifier_list)
    {
        Order *order = modifier_list;
        modifier_list = modifier_list->next;
        delete order;
    }
}

// Member Functions
Order *Order::Copy()
{
    FnTrace("Order::Copy()");
    Order *order = new Order;
    if (order == nullptr)
        return nullptr;

    order->item_name       = item_name;
    order->item_cost       = item_cost;
    order->item_type       = item_type;
    order->item_family     = item_family;
    order->sales_type      = sales_type;
    order->call_order      = call_order;
    order->count           = count;
    order->status          = status;
    order->cost            = cost;
    order->user_id         = user_id;
    order->page_id         = page_id;
    order->script          = script;
    order->qualifier       = qualifier;
    order->total_cost      = total_cost;
    order->total_comp      = total_comp;
    order->discount        = discount;
    order->printer_id      = printer_id;
    order->seat            = seat;
    order->checknum        = checknum;
    order->employee_meal   = employee_meal;
    order->is_reduced      = is_reduced;
    order->reduced_cost    = reduced_cost;
    order->auto_coupon_id  = auto_coupon_id;

    Order *list = modifier_list;
    while (list)
    {
        order->Add(list->Copy());
        list = list->next;
    }

    return order;
}

int Order::Read(InputDataFile &infile, int version)
{
    FnTrace("Order::Read()");
    // See Check::Read() for Version Notes
    int error = 0;
    error += infile.Read(item_name);
    error += infile.Read(item_type);
    error += infile.Read(item_cost);

    int fam;
    error += infile.Read(fam);
    if (fam == 999)
        fam = FAMILY_UNKNOWN;
    item_family = fam;

    error += infile.Read(sales_type);
    error += infile.Read(count);
    error += infile.Read(qualifier);
    error += infile.Read(status);
    error += infile.Read(user_id);
    error += infile.Read(seat);
    if (version >= 19)
        error += infile.Read(employee_meal);
    if (version >= 20)
        error += infile.Read(is_reduced);
    if (version >= 21)
        error += infile.Read(reduced_cost);
    if (version >= 22)
        error += infile.Read(auto_coupon_id);

    if (error)
    {
        genericChar str[256];
        snprintf(str, sizeof(str), "Error in reading version %d order data", version);
        ReportError(str);
    }
    return error;
}

int Order::Write(OutputDataFile &outfile, int version)
{
    FnTrace("Order::Write()");
    if (version < 7)
        return 1;

    // Save all versions
    int error = 0;
    error += outfile.Write(item_name);
    error += outfile.Write(item_type);
    error += outfile.Write(item_cost);
    error += outfile.Write(item_family);
    error += outfile.Write(sales_type);
    error += outfile.Write(count);
    error += outfile.Write(qualifier);
    error += outfile.Write(status);
    error += outfile.Write(user_id);
    error += outfile.Write(seat);
    error += outfile.Write(employee_meal);
    error += outfile.Write(is_reduced);
    error += outfile.Write(reduced_cost);
    error += outfile.Write(auto_coupon_id);

    return error;
}

int Order::Add(Order *order)
{
    FnTrace("Order::Add()");
    if (order == nullptr)
        return 1;  // Add failed

    // start at end of list and work backwords
    Order *ptr = modifier_list;
    if (ptr)
    {
        while (ptr->next)
            ptr = ptr->next;
        while (ptr && order->call_order < ptr->call_order)
            ptr = ptr->fore;
    }

    // Insert order after ptr
    order->parent = this;
    order->fore   = ptr;
    if (ptr)
    {
        order->next   = ptr->next;
        ptr->next = order;
    }
    else
    {
        order->next       = modifier_list;
        modifier_list = order;
    }

    if (order->next)
        order->next->fore = order;
    return FigureCost();
}

int Order::Remove(Order *order)
{
    FnTrace("Order::Remove()");
    if (order == nullptr || order->parent != this)
        return 1;

    if (modifier_list == order)
        modifier_list = order->next;
    if (order->next)
        order->next->fore = order->fore;
    if (order->fore)
        order->fore->next = order->next;
    order->next   = nullptr;
    order->fore   = nullptr;
    order->parent = nullptr;

    FigureCost();
    return 0;
}

int Order::FigureCost()
{
    FnTrace("Order::FigureCost()");
    int parent_count = 1;
    if (parent)
        parent_count = parent->count;

    if ((is_reduced && reduced_cost) || employee_meal)
        cost = reduced_cost * count * parent_count;
    else
        cost = item_cost * count * parent_count;

    if (item_type == ITEM_POUND)
        cost = cost / 100;

    total_cost = cost;
    total_comp = 0;

    Order *order = modifier_list;
    while (order)
    {
        order->FigureCost();
        total_cost += order->total_cost;
        total_comp += order->total_comp;
        order = order->next;
    }

    if (qualifier & QUALIFIER_NO)
        cost = total_cost = 0;

    if (status & ORDER_COMP)
        total_comp = total_cost;

    return 0;
}

void PrintItemAdmissionFiltered(char* str,int qual,const genericChar* item_name)
{
	Str in(item_name);
	admission_parse_hash_name(in,in);
	PrintItem(str,qual,in.Value());
}

genericChar* Order::Description(Terminal *t, genericChar* str)
{
    FnTrace("Order::Description()");
    static genericChar buffer[STRLENGTH];

    if (str == nullptr)
        str = buffer;

    PrintItemAdmissionFiltered(str, qualifier, item_name.Value());
    
    return str;
}

genericChar* Order::PrintDescription(genericChar* str, short pshort)
{
    FnTrace("Order::PrintDescription()");
    static genericChar buffer[256];
    if (str == nullptr)
        str = buffer;

    SalesItem *si = Item(&(MasterSystem->menu));
    if (si)
    {
        if (pshort)
            PrintItem(str, qualifier, si->ZoneName());
        else
            PrintItem(str, qualifier, si->PrintName());
    }
    else
        PrintItemAdmissionFiltered(str, qualifier, item_name.Value());
    return str;
}

int Order::IsEntree()
{
    FnTrace("Order::IsEntree()");
	switch (item_family)
	{
    case FAMILY_BREAKFAST_ENTREES:
    case FAMILY_BURGERS:
    case FAMILY_DINNER_ENTREES:
    case FAMILY_LUNCH_ENTREES:
    case FAMILY_PIZZA:
    case FAMILY_SANDWICHES:
    case FAMILY_SPECIALTY:
    case FAMILY_SPECIALTY_ENTREE:
        return 1;
    default:
        return 0;
	}
}

int Order::FindPrinterID(Settings *settings)
{
    FnTrace("Order::FindPrinterID()");
    SalesItem *mi = Item(&MasterSystem->menu);
    if (mi == nullptr)
        return PRINTER_KITCHEN1;

    int pid = PRINTER_KITCHEN1;
    if (mi->printer_id != PRINTER_DEFAULT)
        pid = mi->printer_id;
    else
    {
        int idx = CompareList(mi->family, FamilyValue);
        if (idx < 0)
            return PRINTER_KITCHEN1;
        pid = settings->family_printer[idx];
    }

    if (settings->use_item_target)
    {
        if (pid == PRINTER_KITCHEN1_NOTIFY)
            return PRINTER_KITCHEN1;
        else if (pid == PRINTER_KITCHEN2_NOTIFY)
            return PRINTER_KITCHEN2;
        else if (pid == PRINTER_KITCHEN3_NOTIFY)
            return PRINTER_KITCHEN3;
        else if (pid == PRINTER_KITCHEN4_NOTIFY)
            return PRINTER_KITCHEN4;
    }
    return pid;
}

SalesItem *Order::Item(ItemDB *item_db)
{
    FnTrace("Order::Item()");
    return item_db->FindByName(item_name.Value());
}

int Order::PrintStatus(Terminal *t, int target_printer, int reprint, int flag_sent)
{
    FnTrace("Order::PrintStatus()");
    if ((status & flag_sent) && !reprint)
        return 0; // item has already printed

    if (t->kitchen > 0 && !ignore_split)
    {
        // Split kitchen mode, override with printer assigned to terminal 
	if (t->kitchen == 1 && target_printer == PRINTER_KITCHEN1)
	    return 1;
	if (t->kitchen == 2 && target_printer == PRINTER_KITCHEN2)
	    return 1;
	return 0;
    }

    Settings *settings = t->GetSettings();
    int pid = printer_id;
    if (pid == PRINTER_DEFAULT)
        pid = FindPrinterID(settings);
    if (pid == target_printer)
        return 1; // Print!
    else if (printer_id == PRINTER_KITCHEN1 && pid == PRINTER_KITCHEN1_NOTIFY)
        return 1;
    else if (printer_id == PRINTER_KITCHEN2 && pid == PRINTER_KITCHEN2_NOTIFY)
        return 1;
    else if (printer_id == PRINTER_KITCHEN1 && pid == PRINTER_KITCHEN2_NOTIFY)
        return 2; // Notify only
    else if (printer_id == PRINTER_KITCHEN2 && pid == PRINTER_KITCHEN1_NOTIFY)
        return 2; // Notify only
    else if (printer_id == PRINTER_DEFAULT && item_type == ITEM_MODIFIER)
        return 1;
    else
        return 0; // Don't print
}

genericChar* Order::Seat(Settings *settings, genericChar* str)
{
    FnTrace("Order::Seat()");
    static genericChar buffer[32];
    if (str == nullptr)
        str = buffer;

    if (sales_type & SALES_TAKE_OUT)
        strcpy(str, "ToGo");
    else if (settings->use_seats)
        SeatName(seat, str);
    else
        str[0] = '\0';
    return str;
}

int Order::IsModifier()
{
    FnTrace("Order::IsModifier()");
    return (item_type == ITEM_MODIFIER || item_type == ITEM_METHOD ||
            (item_type == ITEM_SUBSTITUTE && (qualifier & QUALIFIER_SUB)));
}

int Order::CanDiscount(int discount_alcohol, Payment *payment)
{
    FnTrace("Order::CanDiscount()");
    int retval = 0;

    if (payment == nullptr || (status & ORDER_COMP))
    {
        retval = 0;
    }
    else if (IsReduced() > 0)
    {
        retval = 0;
    }
    else if (payment->flags & TF_NO_RESTRICTIONS)
    {
        retval = 1;  // payment overrides any restrictions
    }
    else if (!discount_alcohol && (sales_type & SALES_ALCOHOL))
    {
        retval = 0;  // can't discount/comp alcohol
    }
    else
    {
        switch (payment->tender_type)
        {
        case TENDER_COMP:
            retval = !(sales_type & SALES_NO_COMP);
            break;
        case TENDER_EMPLOYEE_MEAL:
            retval = !(sales_type & SALES_NO_EMPLOYEE);
            break;
        case TENDER_DISCOUNT:
        case TENDER_COUPON:
            retval = !(sales_type & SALES_NO_DISCOUNT);
            break;
        }
    }
    return retval;
}

int Order::Finalize()
{
    FnTrace("Order::Finalize()");
    if (status & ORDER_FINAL)
        return 1; // already finalized

    status |= ORDER_FINAL;
    page_id = 0;
    script.Clear();
    return 0;
}

// FIX - override equality operator (==) to handle this operation
// and update all calls to this method
int Order::IsEqual(Order *order)
{
    FnTrace("Order::IsEqual()");
    if ((item_cost != order->item_cost) || 
        (item_type != order->item_type) ||
        (item_family != order->item_family) || 
        (sales_type != order->sales_type) ||
        (qualifier != order->qualifier))
    {
        return 0;
    }

    if (strcmp(item_name.Value(), order->item_name.Value()))
        return 0;

    // if this is a By the Pound order, then we don't want
    // to compare counts.  Otherwise, do compare counts.
    if (item_type != ITEM_POUND && count != order->count)
        return 0;

    Order *mod = modifier_list;
    Order *mod2 = modifier_list;
    while (mod && mod2)
    {
        if (!mod->IsEqual(mod2))
            return 0;
        mod = mod->next;
        mod2 = mod2->next;
    }

    if (mod || mod2)
        return 0; // one order has more modifiers
    else
        return 1;
}

int Order::IsEmployeeMeal(int set)
{
    FnTrace("Order::IsEmployeeMeal()");
    int retval = employee_meal;

    if (set >= 0)
        employee_meal = set;

    return retval;
}

int Order::IsReduced(int set)
{
    FnTrace("Order::IsReduced()");
    int retval = is_reduced;

    if (set >= 0)
        is_reduced = set;

    return retval;
}

int Order::VideoTarget(Settings *settings)
{
    FnTrace("Order::VideoTarget()");
    int fvalue = FindIndexOfValue(item_family, FamilyValue);
    return settings->video_target[fvalue];
}

int Order::AddQualifier(const char* qualifier_str)
{
    FnTrace("Order::AddQualifier()");
    int retval = 1;
    int initial_qual = qualifier;

    if (strncmp(qualifier_str, "LEFT", 4) == 0)
        qualifier = qualifier | QUALIFIER_LEFT;
    else if (strncmp(qualifier_str, "RIGHT", 5) == 0)
        qualifier = qualifier | QUALIFIER_RIGHT;
    else if (strncmp(qualifier_str, "WHOLE", 5) == 0)
        qualifier = qualifier | QUALIFIER_WHOLE;

    if (qualifier != initial_qual)
        retval = 0;

    return retval;
}

/**** Payment Class ****/
// Constructors
Payment::Payment()
{
    FnTrace("Payment::Payment()");
    next        = nullptr;
    fore        = nullptr;
    value       = 0;
    tender_type = TENDER_CASH;
    tender_id   = 0;
    flags       = 0;
    amount      = 0;
    user_id     = 0;
    drawer_id   = 0;
    credit      = nullptr;
}

Payment::Payment(int tender, int pid, int pflags, int pamount)
{
    FnTrace("Payment::Payment(int, int, int, int)");
    next        = nullptr;
    fore        = nullptr;
    amount      = pamount;
    tender_type = tender;
    tender_id   = pid;
    flags       = pflags;
    user_id     = 0;
    drawer_id   = 0;
    credit      = nullptr;

    value = 0;
    if (!(pflags & TF_IS_PERCENT))
        value = pamount;
}

// Destructor
Payment::~Payment()
{
    FnTrace("Payment::~Payment()");

    if (credit != nullptr)
        delete credit;
}

// Member Functions
Payment *Payment::Copy()
{
    FnTrace("Payment::Copy()");
    Payment *payptr = new Payment;
    if (payptr == nullptr)
    {
        ReportError("Can't copy payment");
        return nullptr;
    }

    payptr->value       = value;
    payptr->tender_type = tender_type;
    payptr->tender_id   = tender_id;
    payptr->flags       = flags;
    payptr->amount      = amount;
    payptr->user_id     = user_id;
    payptr->drawer_id   = drawer_id;
    if (credit)
        payptr->credit = credit->Copy();

    return payptr;
}

int Payment::Read(InputDataFile &infile, int version)
{
    FnTrace("Payment::Read()");
    // See Check::Read() for Version Notes
    int error = 0;
    error += infile.Read(tender_type);
    error += infile.Read(tender_id);
    error += infile.Read(amount);
    error += infile.Read(flags);
    if (version >= 8)
        error += infile.Read(drawer_id);
    error += infile.Read(user_id);
    flags |= TF_FINAL;

    if (tender_type == TENDER_CREDIT_CARD || tender_type == TENDER_DEBIT_CARD)
    {
        credit = new Credit;
        credit->Read(infile, version);
    }
    return error;
}

int Payment::Write(OutputDataFile &outfile, int version)
{
    FnTrace("Payment::Write()");
    if (version < 7)
        return 1;

    // Write version 7-8
    int error = 0;
    error += outfile.Write(tender_type);
    error += outfile.Write(tender_id);
    error += outfile.Write(amount);
    error += outfile.Write(flags);
    if (version >= 8)
        error += outfile.Write(drawer_id);
    error += outfile.Write(user_id, 1);

    if (tender_type == TENDER_CREDIT_CARD || tender_type == TENDER_DEBIT_CARD)
        error += credit->Write(outfile, version);

    return error;
}

genericChar* Payment::Description(Settings *settings, genericChar* str)
{
    FnTrace("Payment::Description()");
    static genericChar buffer[128];
    static constexpr size_t PAYMENT_DESC_BUFFER_SIZE = 128;

    if (str == nullptr)
        str = buffer;

    if (tender_type == TENDER_CREDIT_CARD && credit != nullptr)
    {
        snprintf(str, PAYMENT_DESC_BUFFER_SIZE, "Credit Card (%s)", credit->CreditTypeName(nullptr, 1));
        return str;
    }

    settings->TenderName(tender_type, tender_id, str);
    if (flags & TF_IS_PERCENT)
    {
        genericChar tmp[32];
        snprintf(tmp, sizeof(tmp), " %g%%", (Flt) amount / 100.0);
        strncat(str, tmp, sizeof(str) - strlen(str) - 1);
    }
    return str;
}

int Payment::Priority()
{
    FnTrace("Payment::Priority()");
    // expand later
    return 0;
}

int Payment::Suppress()
{
    FnTrace("Payment::Suppress()");
    return (tender_type == TENDER_CHANGE || 
            tender_type == TENDER_GRATUITY ||
            tender_type == TENDER_MONEY_LOST);
}

int Payment::IsDiscount()
{
    FnTrace("Payment::IsDiscount()");
    return (tender_type == TENDER_DISCOUNT || tender_type == TENDER_COUPON);
}

// FIX - Override the equality operator (==) to handle this 
// type of operation.
int Payment::IsEqual(Payment *payment)
{
    FnTrace("Payment::IsTab()");
    int retval = 0;

    if ((payment->tender_type == tender_type) && 
        (payment->tender_id == tender_id) &&
        (payment->flags == flags) && 
        (payment->amount == amount) &&
        (payment->user_id == user_id) && 
        (payment->drawer_id == drawer_id))
    {
        retval = 1;
    }

    return retval;
}

int Payment::IsTab()
{
    FnTrace("Payment::IsTab()");

    return flags & TF_IS_TAB;
}

int Payment::TabRemain()
{
    FnTrace("Payment::TabRemain()");
    int retval = 0;
    
    if (IsTab())
    {
        if (credit == nullptr)
            retval = value;
        else
            retval = credit->Total(1);
    }

    return retval;
}

int Payment::FigureTotals(int also_preauth)
{
    FnTrace("Payment::FigureTotals()");
    int retval = 0;

    if (credit != nullptr)
    {
        value = credit->Total(also_preauth);
        amount = value;
    }

    return retval;
}

int Payment::SetBatch(const char* termid, const char* batch)
{
    FnTrace("Payment::SetBatch()");
    int retval = 1;

    if (credit != nullptr && strcmp(termid, credit->TermID()))
        retval = credit->SetBatch(atol(batch), termid);

    return retval;
}
