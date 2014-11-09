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
 * drawer.cc - revision 112 (9/17/98)
 * Implementation of drawer module
 */

#include "drawer.hh"
#include "report.hh"
#include "check.hh"
#include "employee.hh"
#include "system.hh"
#include "data_file.hh"
#include "settings.hh"
#include "manager.hh"
#include "archive.hh"
#include <sys/types.h>
#include <sys/file.h>
#include <dirent.h>
#include <cstring>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** DrawerPayment Class ****/
// Constructors
DrawerPayment::DrawerPayment()
{
    next        = NULL;
    fore        = NULL;
    tender_type = TENDER_CASH;
    amount      = 0;
    user_id     = 0;
    target_id   = 0;
}

DrawerPayment::DrawerPayment(int tender, int amt, int user, TimeInfo &timevar, int target)
{
    next        = NULL;
    fore        = NULL;
    tender_type = tender;
    amount      = amt;
    user_id     = user;
    target_id   = target;
    time        = timevar;
}

// Member Functions
int DrawerPayment::Read(InputDataFile &df, int version)
{
    int error = 0;
    error += df.Read(tender_type);
    error += df.Read(amount);
    error += df.Read(user_id);
    error += df.Read(target_id);
    error += df.Read(time);

    if (error)
    {
        genericChar str[256];
        sprintf(str, "Error in reading drawer payment version %d data", version);
        ReportError(str);
    }
    return error;
}

int DrawerPayment::Write(OutputDataFile &df, int version)
{
    int error = 0;
    error += df.Write(tender_type);
    error += df.Write(amount);
    error += df.Write(user_id);
    error += df.Write(target_id);
    error += df.Write(time, 1);
    return error;
}


/**** DrawerBalance Class ****/
// Contructor
DrawerBalance::DrawerBalance()
{
    next = NULL;
    fore = NULL;
    tender_type = TENDER_CHECK;
    tender_id   = 0;
    amount      = 0;
    count       = 0;
    entered     = 0;
}

DrawerBalance::DrawerBalance(int type, int id)
{
    next = NULL;
    fore = NULL;
    tender_type = type;
    tender_id   = id;
    amount      = 0;
    count       = 0;
    entered     = 0;
}

// Member Functions
int DrawerBalance::Read(InputDataFile &df, int version)
{
    int error = 0;
    error += df.Read(tender_type);
    error += df.Read(tender_id);
    error += df.Read(entered);
    return error;
}

int DrawerBalance::Write(OutputDataFile &df, int version)
{
    int error = 0;
    error += df.Write(tender_type);
    error += df.Write(tender_id);
    error += df.Write(entered, 1);
    return error;
}

const char *DrawerBalance::Description(Settings *s, const genericChar *str)
{
    return s->TenderName(tender_type, tender_id, str);
}


/**** Drawer Class ****/
// Constructors
Drawer::Drawer()
{
    FnTrace("Drawer::Drawer()");

    owner_id         = 0;
    puller_id        = 0;
    term             = NULL;
    serial_number    = 0;
    number           = 0;
    total_difference = 0;
    total_checks     = 0;
    total_payments   = 0;
    position         = 0;
    media_balanced   = 0;
    archive = NULL;
}

Drawer::Drawer(TimeInfo &timevar)
{
    FnTrace("Drawer::Drawer(TimeInfo)");

    start_time       = timevar;
    owner_id         = 0;
    puller_id        = 0;
    term             = NULL;
    serial_number    = 0;
    number           = 0;
    total_difference = 0;
    total_checks     = 0;
    total_payments   = 0;
    position         = 0;
    media_balanced   = 0;
    archive = NULL;
}

// Member Functions
int Drawer::Read(InputDataFile &df, int version)
{
    FnTrace("Drawer::Read()");
    // VERSION NOTES
    // 5 (3/3/97) earliest supported version
    int i;

    genericChar str[256];
    if (version < 5 || version > 5)
    {
        sprintf(str, "Unknown drawer version %d", version);
        ReportError(str);
        return 1;
    }

    int n, error = 0;
    error += df.Read(serial_number);
    error += df.Read(host);
    error += df.Read(position);
    error += df.Read(start_time);
    error += df.Read(pull_time);
    error += df.Read(balance_time);
    error += df.Read(owner_id);
    error += df.Read(puller_id);
    error += df.Read(number);
    error += df.Read(media_balanced);

    error += df.Read(n);
    if (n > 100)
    {
        sprintf(str, "Unusually high DrawerBalance count (%d)", n);
        ReportError(str);
    }
    for (i = 0; i < n; ++i)
    {
        if (df.end_of_file)
        {
            sprintf(str, "Unexpected end of DrawerBalances (%d of %d so far)",
                    i+1, n);
            ReportError(str);
            return 1;
        }

        DrawerBalance *db = new DrawerBalance;
        error += db->Read(df, version);
        if (error)
        {
            delete db;
            sprintf(str, "Error reading DrawerBalance %d of %d", i+1, n);
            ReportError(str);
            return 1;
        }
        else
            Add(db);
    }

    error += df.Read(n);
    for (i = 0; i < n; ++i)
    {
        if (df.end_of_file)
        {
            sprintf(str, "Unexpected end of DrawerPayments (%d of %d so far)",
                    i+1, n);
            ReportError(str);
            return 1;
        }

        DrawerPayment *dp = new DrawerPayment;
        error += dp->Read(df, version);
        if (error)
        {
            delete dp;
            sprintf(str, "Error reading DrawerPayment %d of %d", i+1, n);
            ReportError(str);
            return 1;
        }
        else
            Add(dp);
    }
    return error;
}

int Drawer::Write(OutputDataFile &df, int version)
{
    FnTrace("Drawer::Write()");
    if (version < 5 || version > 5)
    {
        genericChar str[256];
        sprintf(str, "Invalid drawer version '%d' for writing", version);
        ReportError(str);
        return 1;
    }
    DrawerBalance *balance;
    DrawerPayment *dpaymnt;

    // Save version 5
    int error = 0;
    error += df.Write(serial_number);
    error += df.Write(host);
    error += df.Write(position);
    error += df.Write(start_time);
    error += df.Write(pull_time);
    error += df.Write(balance_time);
    error += df.Write(owner_id);
    error += df.Write(puller_id);
    error += df.Write(number);
    error += df.Write(media_balanced, 1);

    // save drawer balances
    int count = 0;
    for (balance = balance_list.Head(); balance != NULL; balance = balance->next)
    {
        if (balance->entered)
            ++count;
    }
    error += df.Write(count, 1);
    for (balance = balance_list.Head(); balance != NULL; balance = balance->next)
    {
        if (balance->entered)
            error += balance->Write(df, version);
    }

    // save drawer payments
    error += df.Write(payment_list.Count(), 1);
    for (dpaymnt = payment_list.Head(); dpaymnt != NULL; dpaymnt = dpaymnt->next)
    {
        error += dpaymnt->Write(df, version);
    }

    return error;
}

int Drawer::Status()
{
    FnTrace("Drawer::Status()");
    if (balance_time.IsSet())
        return DRAWER_BALANCED;
    else if (pull_time.IsSet())
        return DRAWER_PULLED;
    else
        return DRAWER_OPEN;
}

int Drawer::Load(const char *file)
{
    FnTrace("Drawer::Load()");
    filename.Set(file);

    int version = 0;
    InputDataFile df;
    if (df.Open(file, version))
        return 1;

    if (version < 0)
    {
        df.Close();
        ReportError("Invalid drawer found & removed");
        DeleteFile(file);
        return 1;
    }

    int error = Read(df, version);
    return error;
}

int Drawer::Save()
{
    FnTrace("Drawer::Save()");
    int retval = 0;

    if (archive)
        archive->changed = 1;
    else
        retval = MasterSystem->SaveDrawer(this);

    return retval;
}

int Drawer::DestroyFile()
{
    FnTrace("Drawer::DestroyFile()");
    if (filename.length <= 0)
        return 0;

    int result = DeleteFile(filename.Value());
    if (result)
        ReportError("Error In Deleting Drawer");
    filename.Clear();
    return result;
}

int Drawer::Add(DrawerPayment *dp)
{
    FnTrace("Drawer::Add()");
    return payment_list.AddToTail(dp);
}

int Drawer::Add(DrawerBalance *db)
{
    FnTrace("Drawer::Add()");
    return balance_list.AddToTail(db);
}

int Drawer::Remove(DrawerPayment *dp)
{
    FnTrace("Drawer::Remove()");
    return payment_list.Remove(dp);
}

int Drawer::Remove(DrawerBalance *db)
{
    FnTrace("Drawer::Remove()");
    return balance_list.Remove(db);
}

int Drawer::Purge()
{
    FnTrace("Drawer::Purge()");
    payment_list.Purge();
    balance_list.Purge();

    total_difference = 0;
    total_checks     = 0;
    return 0;
}

int Drawer::Count()
{
    FnTrace("Drawer::Count()");
    int count = 1;
    for (Drawer *d = next; d != NULL; d = d->next)
        ++count;
    return count;
}

#define COL -9
int Drawer::MakeReport(Terminal *my_term, Check *check_list, Report *r)
{
    FnTrace("Drawer::MakeReport()");
    if (r == NULL)
        return 1;

    r->update_flag = UPDATE_ARCHIVE | UPDATE_CHECKS | UPDATE_SERVER;
    if (term == NULL)
        term = my_term;
    System *sys = term->system_data;
    Settings *s = &(sys->settings);
    Total(check_list);
    genericChar str[256];
    //r->max_width = 40;
    int status = Status();
    int balanced = (status == DRAWER_BALANCED);
    if (IsServerBank())
        strcpy(str, term->Translate("Server Bank Report"));
    else if (number > 0)
        sprintf(str, "Drawer #%d Balance Report", number);
    else
        strcpy(str, term->Translate("Cashier Balance Report"));

    r->SetTitle(str);
    r->TextC(str);
    r->NewLine();
    r->TextC(term->Translate("(Gross Sales & Tax Collected)"));
    r->NewLine(2);

    // write hostname
    Terminal *termlist = my_term->parent->TermList();
    while (termlist != NULL)
    {
        if (termlist->host == host)
        {
            snprintf(str, 256, "Host:  %s", termlist->name.Value());
            r->TextL(str);
            r->NewLine();
            termlist = NULL;
        }
        else
        {
            termlist = termlist->next;
        }
    }

    // Header
    if (serial_number != -1)
    {
        Employee *e = sys->user_db.FindByID(owner_id);
        if (e)
            sprintf(str, "%s: %s", term->Translate("Drawer Assignment"), e->system_name.Value());
        else
            sprintf(str, term->Translate("Drawer Hasn't Been Assigned"));
        r->TextL(str);
        r->NewLine();
        
        r->TextPosR(7, term->Translate("Opened:"));
        r->TextPosL(8, term->TimeDate(start_time, TD2));
        
        if (pull_time.IsSet())
        {
            r->NewLine();
            r->TextPosR(7, term->Translate("Pulled:"));
            r->TextPosL(8, term->TimeDate(pull_time, TD2));
        }
    }
    else
    {
        r->TextL(term->Translate("All Drawers"));
    }
    r->NewLine();
    r->TextPosR(9, term->Translate("Checks:"));
    r->NumberPosL(10, total_checks);
    r->NewLine();
    r->TextPosR(9, term->Translate("Payments:"));
    r->NumberPosL(10, total_payments);
    r->NewLine();

    int cash_float = term->GetSettings()->drawer_day_float;
    int total_count = 0;
    int total_amount = cash_float;
    int total_entered = cash_float;
    int dis_count = 0;
    int dis_amount = 0;
    int dis_entered = 0;
    int credit_count = 0;
    int credit_amount = 0;
    int credit_entered = 0;
    int total_deposit = 0;
    DrawerBalance *db;

    if (balanced)
    {
        r->Mode(PRINT_UNDERLINE);
        r->TextPosR(COL, term->Translate("Actual"));
        r->TextR(term->Translate("System"));
        r->Mode(0);
    }
    r->NewLine();

    // Cash totals
    int tip_a = TotalBalance(TENDER_PAID_TIP);
    int cash_amount = 0;  // amount
    int cash_count = 0;  // count
    int cash_entered = 0;  // entered
    db = FindBalance(TENDER_CASH_AVAIL, 0);
    if (db)
    {
        cash_amount    = db->amount;
        cash_count     = db->count;
        cash_entered   = db->entered;
        total_amount  += cash_amount;
        total_count   += cash_count;
        total_entered += cash_entered;
    }

    r->TextL(term->Translate("Starting Balance"));
    if (balanced)
        r->TextPosR(COL, term->FormatPrice(cash_float));
    r->TextR(term->FormatPrice(cash_float));
    r->NewLine();

    if (tip_a)
    {
        r->TextL(term->Translate("Cash Before Tip Payout"));
        if (balanced)
            r->TextPosR(COL, term->FormatPrice(cash_entered + tip_a));
        else
            r->TextPosR(COL, term->FormatPrice(cash_count));
        r->TextR(term->FormatPrice(cash_amount + tip_a));
        r->NewLine();

        r->TextL(term->Translate("Tips Paid out"));
        if (balanced)
            r->TextPosR(COL, term->FormatPrice(tip_a));
        r->TextR(term->FormatPrice(tip_a));
        r->NewLine();

    }

    r->TextL(term->Translate("Cash"));
    if (balanced)
        r->TextPosR(COL, term->FormatPrice(cash_entered));
    else
        r->NumberPosR(COL, cash_count);
    r->TextR(term->FormatPrice(cash_amount));
    r->NewLine();

    // Check
    int check_amount = 0;
    int check_count = 0;
    int check_entered = 0;
    db = FindBalance(TENDER_CHECK, 0);
    if (db && (db->amount != 0 || db->entered != 0))
    {
        check_amount   = db->amount;
        check_count    = db->count;
        check_entered  = db->entered;
        total_amount  += check_amount;
        total_count   += check_count;
        total_entered += check_entered;
        r->TextL(term->Translate("Check"));
        if (balanced)
            r->TextPosR(COL, term->FormatPrice(check_entered));
        else
            r->NumberPosR(COL, check_count);
        r->TextR(term->FormatPrice(check_amount));
        r->NewLine();
    }

    // Gift Cert.
    db = FindBalance(TENDER_GIFT, 0);
    if (db && (db->amount != 0 || db->entered != 0))
    {
        total_amount  += db->amount;
        total_count   += db->count;
        total_entered += db->entered;
        r->TextL(term->Translate("Gift Certificate"));
        if (balanced)
            r->TextPosR(COL, term->FormatPrice(db->entered));
        else
            r->NumberPosR(COL, db->count);
        r->TextR(term->FormatPrice(db->amount));
        r->NewLine();
    }

    // Expense Payments
    int pay_amount = 0;
    int pay_count = 0;
    int pay_entered = 0;
    db = FindBalance(TENDER_EXPENSE, 0);
    if (db &&(db->amount != 0 || db->entered != 0))
    {
        pay_amount = db->amount;
        pay_count = db->count;
        pay_entered = db->entered;
    }

    // SubTotal
    r->TextR("--------");
    r->NewLine();
    r->TextL(term->Translate("SubTotal"));
    if (balanced)
        r->TextPosR(COL, term->FormatPrice(total_entered));
    else
        r->NumberPosR(COL, total_count);
    r->TextR(term->FormatPrice(total_amount));
    r->NewLine(2);

    //NOTE->BAK Support both the original credit card method as well
    //as the new method simultaneously.  The assumption is that there
    //will be overlap only during a transition stage.
    // Credit Cards, original
    for (CreditCardInfo *cc = s->CreditCardList(); cc != NULL; cc = cc->next)
    {
        db = FindBalance(TENDER_CHARGE_CARD, cc->id);
        if (db && (db->amount != 0 || db->entered != 0))
        {
            credit_amount  += db->amount;
            credit_count   += db->count;
            credit_entered += db->entered;
            r->TextL(cc->name.Value());
            if (balanced)
                r->TextPosR(COL, term->FormatPrice(db->entered));
            else
                r->NumberPosR(COL, db->count);
            r->TextR(term->FormatPrice(db->amount));
            r->NewLine();
        }
    }

    // Credit Cards, new (AKA, integral)
    int idx = 0;
    while (CreditCardValue[idx] > -1)
    {
        db = FindBalance(TENDER_CREDIT_CARD, CreditCardValue[idx]);
        if (db && (db->amount != 0 || db->entered != 0))
        {
            credit_amount  += db->amount;
            credit_count   += db->count;
            credit_entered += db->entered;
            r->TextL(CreditCardName[idx]);
            if (balanced)
                r->TextPosR(COL, term->FormatPrice(db->entered));
            else
                r->NumberPosR(COL, db->count);
            r->TextR(term->FormatPrice(db->amount));
            r->NewLine();
        }
        idx += 1;
    }

    // Debit Cards, new
    db = FindBalance(TENDER_DEBIT_CARD, 1);
    if (db && (db->amount != 0 || db->entered != 0))
    {
        credit_amount += db->amount;
        credit_count += db->count;
        credit_entered += db->entered;
        r->TextL(term->Translate(FindStringByValue(CARD_TYPE_DEBIT, CardTypeValue, CardTypeName)));
        if (balanced)
            r->TextPosR(COL, term->FormatPrice(db->entered));
        else
            r->NumberPosR(COL, db->count);
        r->TextR(term->FormatPrice(db->amount));
        r->NewLine();
    }

    // Credit Card total
    if (credit_amount != 0 || credit_count != 0 || credit_entered != 0)
    {
        total_amount  += credit_amount;
        total_count   += credit_count;
        total_entered += credit_entered;
        r->TextR("--------");
        r->NewLine();
        r->TextL(term->Translate("Total C.Cards"));
        if (balanced)
            r->TextPosR(COL, term->FormatPrice(credit_entered));
        else
            r->NumberPosR(COL, credit_count);
        r->TextR(term->FormatPrice(credit_amount));
        r->NewLine();
    }

    // Drawer Balance
    r->TextR("--------");
    r->NewLine();
    r->TextL(term->Translate("Drawer Balance"));
    if (balanced)
        r->TextPosR(COL, term->FormatPrice(total_entered));
    else
        r->NumberPosR(COL, total_count);
    r->TextR(term->FormatPrice(total_amount));
    r->NewLine(2);

    // Discounts
    r->Mode(PRINT_RED);
    for (CouponInfo *cp = s->CouponList(); cp != NULL; cp = cp->next)
    {
        db = FindBalance(TENDER_COUPON, cp->id);
        if (db && (db->amount != 0 || db->entered != 0))
        {
            dis_amount  += db->amount;
            dis_count   += db->count;
            dis_entered += db->entered;
            r->TextL(cp->name.Value());
            if (balanced)
                r->TextPosR(COL, term->FormatPrice(db->entered));
            else
                r->NumberPosR(COL, db->count);
            r->TextR(term->FormatPrice(db->amount));
            r->NewLine();
        }
    }

    for (CompInfo *cm = s->CompList(); cm != NULL; cm = cm->next)
    {
        db = FindBalance(TENDER_COMP, cm->id);
        if (db && (db->amount != 0 || db->entered != 0))
        {
            dis_amount += db->amount;
            dis_count  += db->count;
            r->TextL(cm->name.Value());
            if (media_balanced & (1<<TENDER_COMP))
            {
                dis_entered += db->entered;
                if (balanced)
                    r->TextPosR(COL, term->FormatPrice(db->entered));
                else
                    r->NumberPosR(COL, db->count);
            }
            else
            {
                dis_entered += db->amount;
                if (!balanced)
                    r->NumberPosR(COL, db->count);
            }
            r->TextR(term->FormatPrice(db->amount));
            r->NewLine();
        }
    }

    db = FindBalance(TENDER_ITEM_COMP, 0);
    if (db && (db->amount != 0 || db->entered != 0))
    {
        dis_amount += db->amount;
        dis_count  += db->count;
        r->TextL(term->Translate("Item Comps"));
        if (media_balanced & (1<<TENDER_ITEM_COMP))
        {
            dis_entered += db->entered;
            if (balanced)
                r->TextPosR(COL, term->FormatPrice(db->entered));
            else
                r->NumberPosR(COL, db->count);
        }
        else
        {
            dis_entered += db->amount;
            if (!balanced)
                r->NumberPosR(COL, db->count);
        }
        r->TextR(term->FormatPrice(db->amount));
        r->NewLine();
    }

    for (DiscountInfo *ds = s->DiscountList(); ds != NULL; ds = ds->next)
    {
        db = FindBalance(TENDER_DISCOUNT, ds->id);
        if (db && (db->amount != 0 || db->entered != 0))
        {
            dis_amount += db->amount;
            dis_count  += db->count;
            r->TextL(ds->name.Value());
            if (media_balanced & (1<<TENDER_DISCOUNT))
            {
                dis_entered += db->entered;
                if (balanced)
                    r->TextPosR(COL, term->FormatPrice(db->entered));
                else
                    r->NumberPosR(COL, db->count);
            }
            else
            {
                dis_entered += db->amount;
                if (!balanced)
                    r->NumberPosR(COL, db->count);
            }
            r->TextR(term->FormatPrice(db->amount));
            r->NewLine();
        }
    }

    for (MealInfo *mi = s->MealList(); mi != NULL; mi = mi->next)
    {
        db = FindBalance(TENDER_EMPLOYEE_MEAL, mi->id);
        if (db && (db->amount != 0 || db->entered != 0))
        {
            dis_amount += db->amount;
            dis_count  += db->count;
            r->TextL(mi->name.Value());
            if (media_balanced & (1<<TENDER_EMPLOYEE_MEAL))
            {
                dis_entered += db->entered;
                if (balanced)
                    r->TextPosR(COL, term->FormatPrice(db->entered));
                else
                    r->NumberPosR(COL, db->count);
            }
            else
            {
                dis_entered += db->amount;
                if (!balanced)
                    r->NumberPosR(COL, db->count);
            }
            r->TextR(term->FormatPrice(db->amount));
            r->NewLine();
        }
    }

    db = FindBalance(TENDER_MONEY_LOST, 0);
    if (db && (db->amount != 0 || db->entered != 0))
    {
        dis_amount  += db->amount;
        dis_count   += db->count;
        dis_entered += db->amount;
        r->TextL(term->Translate("Money Lost"));
        if (!balanced)
            r->NumberPosR(COL, db->count);
        r->TextR(term->FormatPrice(db->amount));
        r->NewLine();
    }

    if (dis_amount != 0 || dis_count != 0 || dis_entered != 0)
    {
        r->TextR("--------");
        r->NewLine();
        r->TextL(term->Translate("Total Discounts"));
        if (balanced)
            r->TextPosR(COL, term->FormatPrice(dis_entered));
        else
            r->NumberPosR(COL, dis_count);
        r->TextR(term->FormatPrice(dis_amount));
        r->NewLine(2);
    }
    r->Mode(0);

    int add_line = 0;

    // Room Charges
    int amount = TotalBalance(TENDER_CHARGE_ROOM);
    if (amount != 0)
    {
        r->TextL(term->Translate("Room Charges"));
        r->TextR(term->FormatPrice(amount));
        r->NewLine();
        add_line = 1;
    }

    // House Account
    db = FindBalance(TENDER_ACCOUNT, 0);
    if (db && (db->amount != 0 || db->entered != 0))
    {
        r->TextL(term->Translate("House Accounts"));
        if (!balanced)
            r->NumberPosR(COL, db->count);
        r->TextR(term->FormatPrice(db->amount));
        r->NewLine();
        add_line = 1;
    }

    if (add_line)
        r->NewLine();

    // Deposit Amounts
    r->TextL(term->Translate("Cash"));
    if (balanced)
        r->TextPosR(COL, term->FormatPrice(cash_entered));
    r->TextR(term->FormatPrice(cash_amount));
    r->NewLine();

    r->TextL(term->Translate("Check"));
    if (balanced)
        r->TextPosR(COL, term->FormatPrice(check_entered));
    r->TextR(term->FormatPrice(check_amount));
    r->NewLine();

    r->TextL(term->Translate("Expenses"));
    if (balanced)
        r->TextPosR(COL, term->FormatPrice(-pay_entered));
    r->TextR(term->FormatPrice(-pay_amount));
    r->NewLine();

    total_deposit = cash_amount + check_amount;
    r->NewLine();
    r->TextL(term->Translate("Total Deposit"));
    if (balanced)
    {
        total_entered -= cash_float;
        r->TextPosR(COL, term->FormatPrice(total_entered));
    }
    r->TextR(term->FormatPrice(total_deposit));

    if (balanced)
    {
        r->NewLine(2);
        r->Mode(PRINT_BOLD);
        r->TextL(term->Translate("Over/Short"));
        r->TextR(term->FormatPrice(total_difference));
        r->Mode(0);
    }
    return 0;
}

int Drawer::Open()
{
    FnTrace("Drawer::Open()");
    if (IsServerBank())
        return 1;

    if (term == NULL)
        term = MasterControl->FindTermByHost(host.Value());

    if (term)
        return term->OpenDrawer(position);
    else
        return 1;
}

int Drawer::Total(Check *check_list, int force)
{
    FnTrace("Drawer::Total()");
    if (check_list == NULL && force != 1)
        return 1;

    int i;
    DrawerBalance *balance;
    DrawerPayment *dpaymnt;
    Check *check;
    SubCheck *subcheck;
    Payment *payment;

    // Clear Balances
    total_checks = 0;
    total_payments = 0;
    total_difference = 0;

    for (balance = balance_list.Head(); balance != NULL; balance = balance->next)
    {
        //FIX BAK-->This may or may not need to be fixed:
        // TENDER_EXPENSE totals are calculated by
        // ExpenseDB::AddDrawerPayments() when expenses are read
        // in from archives or from current data.  Should expense
        // total be calculated here?  The downside is that
        // expense data would have to be passed in to this function.
        // The upside would be that totals would all be calculated
        // here, and so there would be more consistency.
        if (balance->tender_type != TENDER_EXPENSE)
        {
            balance->amount = 0;
            balance->count  = 0;
        }
    }

    // Set up counters
    int amount[NUMBER_OF_TENDERS];
    int count[NUMBER_OF_TENDERS];
    for (i = 0; i < NUMBER_OF_TENDERS; ++i)
    {
        amount[i] = 0;
        count[i]  = 0;
    }

    // Go through checks
    for (check = check_list; check != NULL; check = check->next)
    {
        if (check->IsTraining() > 0)
            continue;

        for (subcheck = check->SubList(); subcheck != NULL; subcheck = subcheck->next)
        {
            if (subcheck->drawer_id != serial_number)
                continue;

            ++total_checks;
            if (subcheck->item_comps > 0)
            {
                amount[TENDER_ITEM_COMP] += subcheck->item_comps;
                ++count[TENDER_ITEM_COMP];
            }

            for (payment = subcheck->PaymentList(); payment != NULL; payment = payment->next)
            {
                int idx = payment->tender_type;
                int pid = payment->tender_id;
                if (pid > 0 || idx >= NUMBER_OF_TENDERS)
                {
                    DrawerBalance *bal = FindBalance(idx, pid, 1);
                    if (bal)
                    {
                        bal->amount += payment->value;
                        ++bal->count;
                    }
                }
                else
                {
                    amount[idx] += payment->value;
                    ++count[idx];
                }
                switch (idx)
                {
                case TENDER_CHANGE:
                case TENDER_PAID_TIP:
                    amount[TENDER_CASH] -= payment->value;
                    break;
                }
            }
        }
    }

    // Go through payments
    for (dpaymnt = payment_list.Head(); dpaymnt != NULL; dpaymnt = dpaymnt->next)
    {
        ++total_payments;
        int idx = dpaymnt->tender_type;
        amount[idx] += dpaymnt->amount;
        ++count[idx];
        switch (idx)
        {
        case TENDER_CHANGE:
        case TENDER_PAID_TIP:
            amount[TENDER_CASH] -= dpaymnt->amount;
            break;
        }
    }

    // Add in counters
    for (i = 0; i < NUMBER_OF_TENDERS; ++i)
    {
        if (amount[i] == 0)
            continue;

        DrawerBalance *bal = FindBalance(i, 0, 1);
        if (bal)
        {
            bal->amount = amount[i];
            bal->count  = count[i];
        }
    }

    // Calculate TENDER_CASH_AVAIL
    //NOTE BAK-->For balancing purposes, TENDER_CASH is cash receipts.  It does
    // not reflect the actual amount of cash in a drawer as it does not include
    // calculations for TENDER_EXPENSE.  TENDER_CASH_AVAIL is TENDER_CASH minus
    // TENDER_EXPENSE and is used ONLY for balancing purposes (you should see it
    // on the drawer balancing page while the drawer is pulled and also on the
    // Final Balance Report.
    DrawerBalance *cash_avail = FindBalance(TENDER_CASH_AVAIL, 0, 1);
    if (cash_avail != NULL)
    {
        int cashamount = 0;
        int cashentered = 0;
        int expenseamount = 0;
        int expenseentered = 0;
        DrawerBalance *cash     = FindBalance(TENDER_CASH, 0);
        if (cash != NULL)
        {
            cashamount = cash->amount;
            cashentered = cash->entered;
        }
        DrawerBalance *expenses = FindBalance(TENDER_EXPENSE, 0);
        if (expenses != NULL)
        {
            expenseamount = expenses->amount;
            expenseentered = expenses->entered;
        }
        if (cash_avail->amount == 0)
            cash_avail->amount = cashamount - expenseamount;
        if (cash_avail->entered == 0)
        {
            cash_avail->entered = cashentered - expenseentered;
            if (cash_avail->entered)
            {
                media_balanced = (media_balanced &~ (1 << TENDER_CASH));
                media_balanced = (media_balanced | (1 << TENDER_CASH_AVAIL));
            }
        }
        cash_avail->count  = 1;
    }

    // Get total_payments value
    balance = FindBalance(TENDER_EXPENSE, 0);
    if (balance)
        total_payments += balance->count;

    // Calculate total difference
    for (balance = balance_list.Head(); balance != NULL; balance = balance->next)
    {
        if (media_balanced & (1 << balance->tender_type))
        {
            total_difference += (balance->entered - balance->amount);
        }
    }
    return 0;
}

int Drawer::ChangeOwner(int user_id)
{
    FnTrace("Drawer::ChangeOwner()");
    if (owner_id > 0 && !IsEmpty())
        return 1;  // can't reassign

    owner_id = user_id;
    return 0;
}

int Drawer::RecordSale(SubCheck *sc)
{
    FnTrace("Drawer::RecordSale()");
    if (sc == NULL)
        return 1;

    if (sc->drawer_id > 0)
        return 0;  // money for this check already in a drawer

    ++total_checks;
    sc->drawer_id = serial_number;
    return 0;
}

int Drawer::RecordPayment(int tender, int amount, int user, TimeInfo &timevar, int target)
{
    FnTrace("Drawer::RecordPayment()");
    if (tender != TENDER_PAID_TIP)
        return 1;

    DrawerPayment *dp = new DrawerPayment(tender, amount, user, timevar, target);
    Add(dp);
    return 0;
}

int Drawer::TotalPaymentAmount(int tender_type)
{
    DrawerPayment *currPayment = payment_list.Head();
    int retval = 0;

    while (currPayment != NULL)
    {
        if (currPayment->tender_type == tender_type)
            retval += currPayment->amount;
    }
    return retval;
}

int Drawer::IsOpen()
{
    FnTrace("Drawer::IsOpen()");
    if (balance_time.IsSet())
        return 0;
    else if (pull_time.IsSet())
        return 0;
    else
        return 1;
}

int Drawer::IsBalanced()
{
    FnTrace("Drawer::IsBalanced()");

    if (IsEmpty() || balance_time.IsSet())
        return 1;

    return 0;
}

int Drawer::IsServerBank()
{
    FnTrace("Drawer::IsServerBank()");
    return (number < 0);
}

int Drawer::IsEmpty()
{
    FnTrace("Drawer::IsEmpty()");
    int no_checks = (total_checks <= 0);
    int empty_payment_list = (payment_list.Head() == NULL);
    int no_payments = (total_payments <= 0);

    return (no_checks && empty_payment_list && no_payments);
}

DrawerBalance *Drawer::FindBalance(int tender, int id, int make_new)
{
    FnTrace("Drawer::FindBalance()");
    DrawerBalance *balance;

    for (balance = balance_list.Head(); balance != NULL; balance = balance->next)
    {
        if (balance->tender_type == tender && balance->tender_id == id)
        {
            return balance;
        }
    }

    if (make_new)
    {
        balance = new DrawerBalance(tender, id);
        Add(balance);
        return balance;
    }
    return NULL;
}

/****
 * ListBalances:  This is exclusively for debugging.
 ****/
void Drawer::ListBalances()
{
    FnTrace("Drawer::ListBalances()");
    DrawerBalance *balance = balance_list.Head();

    printf("Listing Balances for Drawer %d\n", serial_number);
    while (balance != NULL)
    {
        printf("    Type:     %d\n", balance->tender_type);
        printf("    ID:       %d\n", balance->tender_id);
        printf("        Amount:   %d\n", balance->amount);
        printf("        Entered:  %d\n", balance->entered);
        balance = balance->next;
    }
    
}

int Drawer::Balance(int tender, int id)
{
    FnTrace("Drawer::Balance(int, int)");
    for (DrawerBalance *b = balance_list.Head(); b != NULL; b = b->next)
    {
        if (b->tender_type == tender && b->tender_id == id)
        {
            if (media_balanced & (1<<tender))
                return b->entered;
            else
                return b->amount;
        }
    }
    return 0;
}

int Drawer::TotalBalance(int tender)
{
    FnTrace("Drawer::TotalBalance()");
    int entered_total = 0;
    int amount_total = 0;
    int retval = 0;

    for (DrawerBalance *b = balance_list.Head(); b != NULL; b = b->next)
    {
        if (b->tender_type == tender)
        {
            entered_total += b->entered;
            amount_total += b->amount;
        }
    }

    if (media_balanced & (1 << tender))
        retval = entered_total;
    else
        retval = amount_total;
    return retval;
}

Drawer *Drawer::FindBySerial(int serial)
{
    FnTrace("Drawer::FindBySerial()");
    for (Drawer *d = this; d != NULL; d = d->next)
    {
        if (d->serial_number == serial)
            return d;
    }
    return NULL;
}

Drawer *Drawer::FindByNumber(int no, int status)
{
    FnTrace("Drawer::FindByNumber()");
    for (Drawer *d = this; d != NULL; d = d->next)
        if (d->number == no && d->Status() == status)
            return d;
    return NULL;
}

Drawer *Drawer::FindByOwner(Employee *e, int status)
{
    FnTrace("Drawer::FindByOwner()");
    if (e->training)
        return NULL;

    for (Drawer *d = this; d != NULL; d = d->next)
        if (d->owner_id == e->id && d->Status() == status)
            return d;
    return NULL;
}

int Drawer::Balance(int user_id)
{
    FnTrace("Drawer::Balance(int)");
    if (Status() != DRAWER_PULLED)
        return 1;

    balance_time = SystemTime;
    Save();
    return 0;
}

int Drawer::Pull(int user_id)
{
    FnTrace("Drawer::Pull()");
    if (Status() != DRAWER_OPEN || IsEmpty() || archive)
        return 1;

    puller_id = user_id;
    pull_time = SystemTime;
    Save();

    if (IsServerBank() || number <= 0)
        return 0;

    // FIX - should confirm physical drawer exists in system
    Drawer *d = new Drawer(SystemTime);
    d->host     = host;
    d->owner_id = owner_id;
    d->term     = term;
    d->number   = number;
    d->position = position;
    MasterSystem->Add(d);
    d->Save();
    return 0;
}

int Drawer::MergeServerBanks()
{
    FnTrace("Drawer::MergeServerBanks()");
    if (!IsServerBank() || Status() != DRAWER_BALANCED)
        return 1;

    System *sys = MasterSystem;
    Check *check_list;
    Drawer *drawer_list;
    if (archive)
    {
        check_list  = archive->CheckList();
        drawer_list = archive->DrawerList();
    }
    else
    {
        check_list  = sys->CheckList();
        drawer_list = sys->DrawerList();
    }

    int check_change = 0;
    int drawer_change = 0;
    Drawer *d = drawer_list;
    while (d)
    {
        Drawer *d_next = d->next;
        if (d->owner_id == owner_id && d->IsServerBank() &&
            d->Status() == DRAWER_BALANCED && d != this)
        {
            drawer_change = 1;
            // Reassign drawer_id in checks
            for (Check *c = check_list; c != NULL; c = c->next)
            {
                check_change = 0;
                for (SubCheck *sc = c->SubList(); sc != NULL; sc = sc->next)
                    if (sc->drawer_id == d->serial_number)
                    {
                        check_change = 1;
                        sc->drawer_id = serial_number;
                    }

                if (check_change)
                    c->Save();
            }

            // Move payments
            DrawerPayment *dp = d->payment_list.Head();
            while (dp)
            {
                DrawerPayment *dp_next = dp->next;
                d->Remove(dp);
                Add(dp);
                dp = dp_next;
            }
            // Move balances
            for (DrawerBalance *b = d->balance_list.Head(); b != NULL; b = b->next)
            {
                DrawerBalance *mdb = FindBalance(b->tender_type, b->tender_id, 1);
                mdb->entered += b->entered;
            }

            // Merge times
            if (d->start_time < start_time)
                start_time = d->start_time;
            if (d->pull_time > pull_time)
                pull_time = d->pull_time;
            if (d->balance_time > balance_time)
                balance_time = d->balance_time;

            // kill drawer
            if (archive)
            {
                archive->changed = 1;
                archive->Remove(d);
            }
            else
            {
                sys->Remove(d);
                d->DestroyFile();
            }
            delete d;
        }
        d = d_next;
    }

    if (drawer_change)
    {
        Total(check_list);
        Save();
    }
    return 0;
}

/****
 * MergeSystems:  Merge all drawers from one system into this drawer.
 ****/
int Drawer::MergeSystems(int mergeall)
{
    FnTrace("Drawer::MergeSystems()");

    System *sys = MasterSystem;
    Check *check_list;
    Drawer *drawer_list;
    if (archive)
    {
        check_list  = archive->CheckList();
        drawer_list = archive->DrawerList();
    }
    else
    {
        check_list  = sys->CheckList();
        drawer_list = sys->DrawerList();
    }

    int check_change = 0;
    int drawer_change = 0;
    Drawer *d = drawer_list;
    while (d)
    {
        Drawer *d_next = d->next;
        if (d != this && (mergeall || (d->host == host)))
        {
            drawer_change = 1;
            // Reassign drawer_id in checks
            for (Check *c = check_list; c != NULL; c = c->next)
            {
                check_change = 0;
                for (SubCheck *sc = c->SubList(); sc != NULL; sc = sc->next)
                    if (sc->drawer_id == d->serial_number)
                    {
                        check_change = 1;
                        sc->drawer_id = serial_number;
                    }

                if (check_change)
                    c->Save();
            }

            // Move payments
            DrawerPayment *dp = d->payment_list.Head();
            while (dp)
            {
                DrawerPayment *dp_next = dp->next;
                d->Remove(dp);
                Add(dp);
                dp = dp_next;
            }
            // Move balances
            for (DrawerBalance *b = d->balance_list.Head(); b != NULL; b = b->next)
            {
                DrawerBalance *mdb = FindBalance(b->tender_type, b->tender_id, 1);
                mdb->entered += b->entered;
            }

            // Merge times
            if (d->start_time < start_time)
                start_time = d->start_time;
            if (d->pull_time > pull_time)
                pull_time = d->pull_time;
            if (d->balance_time > balance_time)
                balance_time = d->balance_time;

            // kill drawer
            if (archive)
            {
                archive->changed = 1;
                archive->Remove(d);
            }
            else
            {
                sys->Remove(d);
                d->DestroyFile();
            }
            delete d;
        }
        d = d_next;
    }

    if (drawer_change)
    {
        Total(check_list);
        Save();
    }
    return 0;
}


/*********************************************************************
 * General Drawer Functions
 ********************************************************************/

/****
 * MergeSystems:  Walk through the terminals and merge all drawers for
 *   all terminals into one drawer per terminal.
 ****/
int MergeSystems(Terminal *term, int mergeall)
{
    FnTrace("MergeSystems()");
    int retval = 1;

    Drawer *firstdrawer = term->system_data->DrawerList();
    if (mergeall)
    {
        firstdrawer->MergeSystems(mergeall);
        retval = 0;
    }
    else
    {
        Drawer *currdrawer;
        Drawer *mergedrawer;
        int count = 0;
        
        // gather drawers attached to terminals
        Terminal *termlist = term->parent->TermList();
        while (termlist != NULL)
        {
            // count the drawers for this hostname
            count = 0;
            currdrawer = firstdrawer;
            mergedrawer = NULL;
            while (currdrawer != NULL)
            {
                if (currdrawer->host == termlist->host)
                {
                    if (mergedrawer == NULL)
                        mergedrawer = currdrawer;
                    count += 1;
                }
                currdrawer = currdrawer->next;
            }
            if (count > 1 && mergedrawer != NULL)
            {
                mergedrawer->MergeSystems();
                retval = 1;
            }
            termlist = termlist->next;
        }
        
        // now gather the drawers that have no assigned host
        count = 0;
        currdrawer = firstdrawer;
        mergedrawer = NULL;
        while (currdrawer != NULL)
        {
            if (currdrawer->host.length == 0)
            {
                if (mergedrawer == NULL)
                    mergedrawer = currdrawer;
                count += 1;
            }
            currdrawer = currdrawer->next;
        }
        if (count > 1 && mergedrawer != NULL)
        {
            mergedrawer->MergeSystems();
            retval = 0;
        }
    }
    return retval;
}
