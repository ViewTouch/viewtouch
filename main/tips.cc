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
 * tips.cc - revision 54 (10/13/98)
 * Implementation of tip module
 */

#include "tips.hh"
#include "check.hh"
#include "drawer.hh"
#include "report.hh"
#include "data_file.hh"
#include "employee.hh"
#include "system.hh"
#include "settings.hh"
#include "archive.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** TipEntry Class ****/
// Constructor
TipEntry::TipEntry()
{
    FnTrace("TipEntry::TipEntry()");
    next            = nullptr;
    fore            = nullptr;
    user_id         = 0;
    amount          = 0;
    previous_amount = 0;
    paid            = 0;
}

// Member Functions
int TipEntry::Read(InputDataFile &df, int version)
{
    FnTrace("TipEntry::Read()");
    int error = 0;
    error += df.Read(user_id);
    error += df.Read(amount);
    error += df.Read(paid);
    return error;
}

int TipEntry::Write(OutputDataFile &df, int version)
{
    FnTrace("TipEntry::Write()");
    int error = 0;
    error += df.Write(user_id);
    error += df.Write(amount);
    error += df.Write(paid);
    return error;
}

TipEntry *TipEntry::Copy()
{
    FnTrace("TipEntry::Copy()");
    TipEntry *te = new TipEntry;
    if (te == nullptr)
        return nullptr;

    te->user_id = user_id;
    te->amount  = amount;
    te->paid    = paid;
    return te;
}

int TipEntry::Count()
{
    FnTrace("TipEntry::Count()");
    int count = 1;
    for (TipEntry *te = next; te != nullptr; te = te->next)
        ++count;
    return count;
}

/**** TipDB Class ****/
// Constructor
TipDB::TipDB()
{
    FnTrace("TipDB::TipDB()");
    archive        = nullptr;
    total_paid     = 0;
    total_held     = 0;
    total_previous = 0;
}

// Member Functions
int TipDB::Add(TipEntry *te)
{
    FnTrace("TipDB::Add()");
    if (te == nullptr)
        return 1;

    // search for previous entry for user
    TipEntry *ptr = TipList();
    while (ptr)
    {
        if (ptr->user_id == te->user_id)
        {
            Remove(ptr);
            te->amount += ptr->amount;
            te->paid   += ptr->paid;
            Add(te);
            delete ptr;
            return 0;
        }
        ptr = ptr->next;
    }

    // add entry
    return tip_list.AddToTail(te);
}

int TipDB::Remove(TipEntry *te)
{
    FnTrace("TipDB::Remove()");
    return tip_list.Remove(te);
}

int TipDB::Purge()
{
    FnTrace("TipDB::Purge()");
    tip_list.Purge();
    return 0;
}

TipEntry *TipDB::FindByUser(int id)
{
    FnTrace("TipDB::FindByUser()");
    for (TipEntry *te = TipList(); te != nullptr; te = te->next)
    {
        if (te->user_id == id)
            return te;
    }
    return nullptr;
}

TipEntry *TipDB::FindByRecord(int record, Employee *e)
{
    FnTrace("TipDB::FindByRecord()");
    if (record < 0)
        return nullptr;

    for (TipEntry *te = TipList(); te != nullptr; te = te->next)
    {
        if (e == nullptr || te->user_id == e->id)
        {
            if (record <= 0)
                return te;
            --record;
        }
    }
    return nullptr;
}

int TipDB::CaptureTip(int user_id, int amount)
{
    FnTrace("TipDB::CaptureTip()");
    TipEntry *te = FindByUser(user_id);
    if (te)
    {
        te->amount += amount;
        if (te->amount == 0 && te->paid == 0)
        {
            Remove(te);
            delete te;
        }
    }
    else
    {
        te = new TipEntry;
        te->user_id = user_id;
        te->amount  = amount;
        Add(te);
    }
    return 0;
}

int TipDB::TransferTip(int user_id, int amount)
{
    FnTrace("TipDB::TransferTip()");
    TipEntry *te = FindByUser(user_id);
    if (te)
    {
        te->amount          += amount;
        te->previous_amount += amount;
        if (te->amount == 0 && te->paid == 0)
        {
            Remove(te);
            delete te;
        }
    }
    else
    {
        te = new TipEntry;
        te->user_id         = user_id;
        te->amount          = amount;
        te->previous_amount = amount;
        Add(te);
    }
    return 0;
}

int TipDB::PayoutTip(int user_id, int amount)
{
    FnTrace("TipDB::PayoutTip()");
    TipEntry *te = FindByUser(user_id);
    if (te == nullptr || te->amount <= 0)
        return 1;  // no captured tip to payout

    te->amount -= amount;
    te->paid   += amount;
    return 0;
}

int TipDB::Calculate(Settings *s, TipDB *previous,
                     Check *check_list, Drawer *drawer_list)
{
    FnTrace("TipDB::Calculate()");
    Purge();
    if (previous)
    {
        // base todays captured tip amounts on previous day
        TipEntry *te = previous->TipList();
        while (te)
        {
            TransferTip(te->user_id, te->amount);
            te = te->next;
        }
    }

    // figure today's tips
    for (Check *c = check_list; c != nullptr; c = c->next)
    {
        for (SubCheck *sc = c->SubList(); sc != nullptr; sc = sc->next)
        {
            int tips = sc->TotalTip();
            if (tips != 0)
                CaptureTip(c->WhoGetsSale(s), tips);
        }
    }

    // subract amount paid out
    for (Drawer *d = drawer_list; d != nullptr; d = d->next)
    {
        for (DrawerPayment *dp = d->PaymentList(); dp != nullptr; dp = dp->next)
        {
            if (dp->tender_type == TENDER_PAID_TIP)
                PayoutTip(dp->target_id, dp->amount);
        }
    }

    return 0;
}

int TipDB::Copy(TipDB *db)
{
    FnTrace("TipDB::Copy()");
    Purge();

    for (TipEntry *te = db->TipList(); te != nullptr; te = te->next)
        Add(te->Copy());
    return 0;
}

int TipDB::Total()
{
    FnTrace("TipDB::Total()");
    total_paid     = 0;
    total_held     = 0;
    total_previous = 0;

    for (TipEntry *te = TipList(); te != nullptr; te = te->next)
    {
        if (te->paid > 0)
            total_paid += te->paid;
        if (te->amount > 0)
            total_held += te->amount;
        if (te->previous_amount > 0)
            total_previous += te->previous_amount;
    }
    return 0;
}

void TipDB::ClearHeld()
{
    FnTrace("TipDB::ClearHeld()");
    total_held     = 0;

    for (TipEntry *te = TipList(); te != nullptr; te = te->next)
        te->amount = 0;
}

int TipDB::PaidReport(Terminal *t, Report *r)
{
    FnTrace("TipDB::PaidReport()");
    if (r == nullptr)
        return 1;

    r->TextC("Tips Paid Report");
    r->NewLine(2);

    int total = 0;
    for (TipEntry *te = TipList(); te != nullptr; te = te->next)
    {
        if (te->paid > 0)
        {
            r->TextL(t->UserName(te->user_id));
            r->TextR(t->FormatPrice(te->paid));
            r->NewLine();
            total += te->paid;
        }
    }

    // Total
    r->TextR("--------");
    r->NewLine();
    r->TextL("Total Tips Paid");
    r->TextR(t->FormatPrice(total));
    return 0;
}

int TipDB::PayoutReceipt(Terminal *t, Employee *e, int amount, Report *r)
{
    FnTrace("TipDB::PayoutReceipt()");
    if (r == nullptr || e == nullptr || amount <= 0)
        return 1;

    genericChar str[256];
    r->Mode(PRINT_LARGE | PRINT_NARROW);
    r->TextC("Tip Payout Receipt");
    r->NewLine(2);
    r->Mode(0);

    snprintf(str, STRLENGTH, "     Server: %s", e->system_name.Value());
    r->TextL(str);
    r->NewLine();
    snprintf(str, STRLENGTH, "       Time: %s", t->TimeDate(SystemTime, TD2));
    r->TextL(str);
    r->NewLine();
    snprintf(str, STRLENGTH, "Amount Paid: %s", t->FormatPrice(amount, 1));
    r->TextL(str);
    r->NewLine(3);
    r->Mode(PRINT_UNDERLINE);
    r->TextL("X                               ");
    return 0;
}

int TipDB::ListReport(Terminal *t, Employee *e, Report *r)
{
    FnTrace("TipDB::ListReport()");
    if (r == nullptr || e == nullptr)
        return 1;

    Settings *s = t->GetSettings();
    int flag = e->IsSupervisor(s);
    int count = 0;

    for (TipEntry *te = TipList(); te != nullptr; te = te->next)
    {
        if (te->user_id == e->id || flag)
        {
            r->TextL(t->UserName(te->user_id));
            r->TextC(t->FormatPrice(te->paid));
            genericChar str[256];
            if (te->previous_amount != 0 && te->amount != 0)
                snprintf(str, STRLENGTH, "(!) %s", t->FormatPrice(te->amount));
            else
                t->FormatPrice(str, te->amount);
            int c = COLOR_RED;
            if (te->amount == 0)
                c = COLOR_BLACK;
            r->TextR(str, c);
            r->NewLine();
            ++count;
        }
    }

    if (count == 0)
    {
        if (flag == 0)
            r->TextC("You Have No Captured Tips", COLOR_RED);
        else
            r->TextC("None", COLOR_RED);
    }
    return 0;
}

int TipDB::Update(System *sys)
{
    FnTrace("TipDB::Update()");
    Settings *s = &sys->settings;
    Archive *a = sys->ArchiveListEnd();
    if (a)
    {
        if (a->loaded == 0)
            a->LoadPacked(s);
        Calculate(s, &a->tip_db, sys->CheckList(), sys->DrawerList());
    }
    else
        Calculate(s, nullptr, sys->CheckList(), sys->DrawerList());
    return 0;
}
