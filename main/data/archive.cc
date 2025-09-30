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
 * archive.cc - revision 3 (8/25/98)
 * Implementation of archive module
 */

#include "archive.hh"
#include "data_file.hh"
#include "check.hh"
#include "credit.hh"
#include "drawer.hh"
#include "utility.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** Archive Class ****/
// Constructors
Archive::Archive(TimeInfo &end)
{
    FnTrace("Archive::Archive(TimeInfo)");
    next               = NULL;
    fore               = NULL;
    end_time           = end;
    id                 = 0;
    loaded             = 1;
    changed            = 1;
    corrupt            = 0;
    last_serial_number = 0;
    file_version       = 0;
    altmedia.Set("");
    from_disk          = 0;

    drawer_version = DRAWER_VERSION;
    check_version  = CHECK_VERSION;
    media_version  = 0;  //SETTINGS_VERSION;

    tip_version    = TIP_VERSION;
    tip_db.archive = this;

    work_version    = WORK_VERSION;
    work_db.archive = this;

    exception_version    = EXCEPTION_VERSION;
    exception_db.archive = this;

    tax_food               = 0;
    tax_alcohol            = 0;
    tax_room               = 0;
    tax_merchandise        = 0;
    tax_GST                = 0;
    tax_PST                = 0;
    tax_HST                = 0;
    tax_QST                = 0;
    tax_VAT                = 0;
    royalty_rate           = 0;
    advertise_fund         = 0;
    change_for_checks      = 0;
    change_for_credit      = 0;
    change_for_gift        = 0;
    change_for_roomcharge  = 0;
    discount_alcohol       = 0;
    price_rounding         = 0;

    cc_exception_db        = NULL;
    cc_refund_db           = NULL;
    cc_void_db             = NULL;
    cc_init_results        = NULL;
    cc_saf_details_results = NULL;
    cc_settle_results      = NULL;
}

Archive::Archive(Settings *settings, const char* file)
{
    FnTrace("Archive::Archive(Settings, const char* )");
    filename.Set(file);
    next               = NULL;
    fore               = NULL;
    loaded             = 0;
    changed            = 0;
    id                 = 0;
    corrupt            = 0;
    last_serial_number = 0;
    altmedia.Set("");
    from_disk          = 0;

    drawer_version = DRAWER_VERSION;
    check_version  = CHECK_VERSION;
    media_version  = 0;  //SETTINGS_VERSION;

    tip_version    = TIP_VERSION;
    tip_db.archive = this;

    work_version    = WORK_VERSION;
    work_db.archive = this;

    exception_version    = EXCEPTION_VERSION;
    exception_db.archive = this;

    tax_food               = settings->tax_food;
    tax_alcohol            = settings->tax_alcohol;
    tax_room               = settings->tax_room;
    tax_merchandise        = settings->tax_merchandise;
    tax_GST                = settings->tax_GST;
    tax_PST                = settings->tax_PST;
    tax_HST                = settings->tax_HST;
    tax_QST                = settings->tax_QST;
    tax_VAT                = settings->tax_VAT;
    royalty_rate           = settings->royalty_rate;
    advertise_fund         = settings->advertise_fund;
    change_for_checks      = settings->change_for_checks;
    change_for_credit      = settings->change_for_credit;
    change_for_gift        = settings->change_for_gift;
    change_for_roomcharge  = settings->change_for_roomcharge;
    discount_alcohol       = settings->discount_alcohol;
    price_rounding         = settings->price_rounding;

    cc_exception_db        = NULL;
    cc_refund_db           = NULL;
    cc_void_db             = NULL;
    cc_init_results        = NULL;
    cc_saf_details_results = NULL;
    cc_settle_results      = NULL;

    // Read in header of archive
    file_version = 0;
    InputDataFile df;
    if (df.Open(file, file_version))
        return;

    int error = 0;
    error += df.Read(id);
    if (file_version >= 6)
        error += df.Read(start_time);
    error += df.Read(end_time);
}

// Member Functions
int Archive::LoadUnpacked(Settings *, const char* path)
{
    FnTrace("Archive::LoadUnpacked()");
    return 1; // FIX - finish Archive::LoadUnpacked()
}

int Archive::LoadPacked(Settings *settings, const char* file)
{
    FnTrace("Archive::LoadPacked()");
    char str[STRLENGTH];
    int version = 0;
    int count = 0;
    InputDataFile df;
    int error = 0;
    int i = 0;

    Unload();
    if (file)
        filename.Set(file);

    if (df.Open(filename.Value(), version))
        return 1;

    // Put these initializations at the top.  If an archive is
    // corrupted, it will fail to load, but saving it again won't help
    // with that (and might just cause further corruption).  It was
    // tempting to put this stuff at the very end, but I think that
    // didn't work so well.  AKA, if there was a momentary bug in
    // ViewTouch, a good archive might fail to load and then get
    // zeroed before the next bugfix.
    changed = 0;
    from_disk = 1;

    // VERSION NOTES
    // 2  (2/27/97)  earliest supported version
    // 3  (3/3/97)   drawer sales record removed - drawer_id in subcheck now
    // 4  (3/5/97)   new version for checks
    // 5  (3/17/97)  check version updated
    // 6  (8/20/97)  archive start saved; exception records added
    // 7  (2/6/98)   new file encoding
    // 8  (5/17/02)  expenses added to archive
    // 9  (7/23/02)  no changes; just need to force an update for the check entries
    // 10 (8/26/02)  save media data into archive
    // 11            added tax settings
    // 12 (2/14/03)  added tax_VAT
    // 13 (10/14/03) added Credit Cards
    // 14 (05/25/05) added advertise_fund

    if (version < 2 || version > ARCHIVE_VERSION)
    {
        sprintf(str, "Unknown archive file version %d", version);
        ReportError(str);
        return 1;
    }

    loaded = 1;
    df.Read(id);
    if (version >= 6)
    {
        TimeInfo timevar;
        df.Read(timevar);
        if (timevar.IsSet())
            start_time = timevar;
    }
    df.Read(end_time);

    // Read Drawers
    Drawer *drawer;
    df.Read(drawer_version);
    df.Read(count);
    if (count < 10000 && error == 0)
    {
        for (i = 0; i < count; ++i)
        {
            if (df.end_of_file)
            {
                sprintf(str, "Unexpected end of drawers in archive (%d of %d so far)",
                        i + 1, count);
                ReportError(str);
                goto archive_read_error;
            }
            drawer = new Drawer;
            error = drawer->Read(df, drawer_version);
            if (error)
            {
                delete drawer;
                sprintf(str, "Error %d in reading drawer %d of %d", error, i + 1, count);
                ReportError(str);
                goto archive_read_error;
            }
            Add(drawer);
        }
    }
    else
        goto archive_read_error;

    // Read Checks
    df.Read(check_version);
    df.Read(count);
    if (count < 10000 && error == 0)
    {
        for (i = 0; i < count; ++i)
        {
            if (df.end_of_file)
            {
                ReportError("Unexpected end of Check data");
                goto archive_read_error;
            }
            Check *check = new Check;
            error = check->Read(settings, df, check_version);
            if (error)
            {
                delete check;
                sprintf(str, "Error %d in check %d of %d", error, i + 1, count);
                ReportError(str);
                goto archive_read_error;
            }

            Add(check);
        }
    }
    else
        goto archive_read_error;

    // Read Tips
    df.Read(tip_version);
    df.Read(count);
    if (count < 10000 && error == 0)
    {
        for (i = 0; i < count; ++i)
        {
            if (df.end_of_file)
            {
                ReportError("Unexpected end of TipDB");
                goto archive_read_error;
            }
            TipEntry *te = new TipEntry;
            error = te->Read(df, tip_version);
            if (error)
            {
                delete te;
                sprintf(str, "Error %d in tip %d of %d", error, i + 1, count);
                ReportError(str);
                goto archive_read_error;
            }
            tip_db.Add(te);
        }
    }
    else
        goto archive_read_error;

    if (version >= 6)
    {  // Read Exceptions
        df.Read(exception_version);
        error = exception_db.Read(df, exception_version);
        if (error)
        {
            sprintf(str, "Error %d loading exception data (version %d) from archive",
                    error, exception_version);
            ReportError(str);
            goto archive_read_error;
        }
    }

    if (version >= 8)
    {  // Read Expenses
        expense_db.Purge();  // Need to clear out expenses before we read any in
        df.Read(expense_version);
        error = expense_db.Read(df, expense_version);
        if (error)
        {
            snprintf(str, STRLENGTH,
                    "Error %d loading expense data (version %d) from archive",
                    error, expense_version);
            ReportError(str);
            goto archive_read_error;
        }
        expense_db.AddDrawerPayments(DrawerList());
    }

    if (version >= 10)
    {  // Read media data
        df.Read(media_version);
        // Read Discounts
        df.Read(count);  // using count for all media types
        if (count < 1000 && error == 0)
        {
            for (i = 0; i < count; i++)
            {
                DiscountInfo *discinfo = new DiscountInfo;
                discinfo->Read(df, media_version);
                Add(discinfo);
            }
        }
        else
            goto archive_read_error;
        // Read Coupons
        df.Read(count);  // using count for all media types
        if (count < 1000 && error == 0)
        {
            for (i = 0; i < count; i++)
            {
                CouponInfo *coupinfo = new CouponInfo;
                coupinfo->Read(df, media_version);
                Add(coupinfo);
            }
        }
        else
            goto archive_read_error;
        // Read CreditCards
        df.Read(count);  // using count for all media types
        if (count < 1000 && error == 0)
        {
            for (i = 0; i < count; i++)
            {
                CreditCardInfo *credinfo = new CreditCardInfo;
                credinfo->Read(df, media_version);
                Add(credinfo);
            }
        }
        else
            goto archive_read_error;
        // Read Comps
        df.Read(count);  // using count for all media types
        if (count < 1000 && error == 0)
        {
            for (i = 0; i < count; i++)
            {
                CompInfo *compinfo = new CompInfo;
                compinfo->Read(df, media_version);
                Add(compinfo);
            }
        }
        else
            goto archive_read_error;
        // Read Meals
        df.Read(count);  // using count for all media types
        if (count < 1000 && error == 0)
        {
            for (i = 0; i < count; i++)
            {
                MealInfo *mealinfo = new MealInfo;
                mealinfo->Read(df, media_version);
                Add(mealinfo);
            }
        }
        else
            goto archive_read_error;
    }
    else
    {
        // For older archives.  We don't want to rewrite the archive and we don't
        // want reports changing every time a discount is added or deleted.
        // So we load the media from an alternate file that is a static snapshot
        // at a point in time.  It may not be accurate for all archives, but
        // there isn't any way to be accurate for all archives and at least reports
        // won't change any more.
        altmedia = settings->altdiscount_filename;
        LoadAlternateMedia();
    }

    if (version >= 11)
    {
        df.Read(tax_food);
        df.Read(tax_alcohol);
        df.Read(tax_room);
        df.Read(tax_merchandise);
        df.Read(tax_GST);
        df.Read(tax_PST);
        df.Read(tax_HST);
        df.Read(tax_QST);
        df.Read(royalty_rate);
        df.Read(price_rounding);
        df.Read(change_for_credit);
        df.Read(change_for_roomcharge);
        df.Read(change_for_checks);
        df.Read(change_for_gift);
        df.Read(discount_alcohol);
    }
    else
    {
        altsettings.Set(settings->altsettings_filename);
        LoadAlternateSettings();
    }

    if (version >= 12)
        df.Read(tax_VAT);

    if (version >= 13)
    {
        cc_exception_db = new CreditDB(CC_DBTYPE_EXCEPT);
        cc_exception_db->Read(df);
        cc_refund_db = new CreditDB(CC_DBTYPE_REFUND);
        cc_refund_db->Read(df);
        cc_void_db = new CreditDB(CC_DBTYPE_VOID);
        cc_void_db->Read(df);

        cc_init_results = new CCInit();
        cc_init_results->Read(df);
        cc_saf_details_results = new CCSAFDetails();
        cc_saf_details_results->Read(df);
        cc_settle_results = new CCSettle();
        cc_settle_results->Read(df);
    }
    if (version >= 14)
        df.Read(advertise_fund);

    // Initialize Data
    for (drawer = DrawerList(); drawer != NULL; drawer = drawer->next)
    {
        drawer->Total(CheckList());
    }

    {
        Check *check = CheckList();
        SubCheck *subcheck;
        while (check != NULL)
        {
            subcheck = check->SubList();
            while (subcheck != NULL)
            {
                subcheck->archive = this;
                subcheck->FigureTotals(settings);
                subcheck = subcheck->next;
            }
            check = check->next;
        }
    }

    return 0;

archive_read_error:
    df.Close();
    loaded = 1;
    corrupt = 1;
    sprintf(str, "Archive '%s' (version %d) invalid", filename.Value(), version);
    ReportError(str);
    changed = 0;
    Unload();
    return 1;
}

int Archive::LoadAlternateMedia()
{
    FnTrace("Archive::LoadAlternateMedia()");
    int retval = 1;
    int count;
    int i;
    int openfile = 0;
    InputDataFile mf;

    if (altmedia.size())
    {
        openfile = mf.Open(altmedia.Value(), media_version);
        if (openfile == 0)
        {
            // for older versions of archives, we still want to get the  media information
            // so that we don't use the EverChanging current data.  If the media-archive.dat
            // file exists, read that into this archive.

            // Read Discounts
            mf.Read(count);  // using count for all media types
            for (i = 0; i < count; i++)
            {
                DiscountInfo *discinfo = new DiscountInfo;
                discinfo->Read(mf, media_version);
                Add(discinfo);
            }
            // Read Coupons
            mf.Read(count);  // using count for all media types
            for (i = 0; i < count; i++)
            {
                CouponInfo *coupinfo = new CouponInfo;
                coupinfo->Read(mf, media_version);
                Add(coupinfo);
            }
            // Read CreditCards
            mf.Read(count);  // using count for all media types
            for (i = 0; i < count; i++)
            {
                CreditCardInfo *credinfo = new CreditCardInfo;
                credinfo->Read(mf, media_version);
                Add(credinfo);
            }
            // Read Comps
            mf.Read(count);  // using count for all media types
            for (i = 0; i < count; i++)
            {
                CompInfo *compinfo = new CompInfo;
                compinfo->Read(mf, media_version);
                Add(compinfo);
            }
            // Read Meals
            mf.Read(count);  // using count for all media types
            for (i = 0; i < count; i++)
            {
                MealInfo *mealinfo = new MealInfo;
                mealinfo->Read(mf, media_version);
                Add(mealinfo);
            }
            mf.Close();
            retval = 0;
        }
    }

    return retval;
}

int Archive::LoadAlternateSettings()
{
    FnTrace("Archive::LoadAlternateSettings()");
    int retval = 1;
    InputDataFile infile;
    int openfile = 0;

    if (altsettings.size())
    {
        openfile = infile.Open(altsettings.Value(), settings_version);
        if (openfile == 0)
        {
            infile.Read(tax_food);
            infile.Read(tax_alcohol);
            infile.Read(tax_room);
            infile.Read(tax_merchandise);
            infile.Read(tax_GST);
            infile.Read(tax_PST);
            infile.Read(tax_HST);
            infile.Read(tax_QST);
            infile.Read(royalty_rate);
            infile.Read(price_rounding);
            infile.Read(change_for_credit);
            infile.Read(change_for_roomcharge);
            infile.Read(change_for_checks);
            infile.Read(change_for_gift);
            infile.Read(discount_alcohol);
            if (settings_version >= 52)
                infile.Read(tax_VAT);
            if (settings_version >= 89)
                infile.Read(advertise_fund);
            retval = 0;
        }
    }
    return retval;
}

int Archive::SavePacked()
{
    FnTrace("Archive::SavePacked()");
    if (loaded == 0 || corrupt)
        return 1;  // archive not loaded or is corrupt
    if (from_disk)
    {
        if (debug_mode)
            printf("Tried to save archive, but won't...\n");
        return 1;  // cannot rewrite archive
    }

    file_version = ARCHIVE_VERSION;
    OutputDataFile df;
    if (df.Open(filename.Value(), ARCHIVE_VERSION, 1))
        return 1;

    int count;
    df.Write(id);
    df.Write(start_time);
    df.Write(end_time);

    // Save Drawers
    count = 0;
    Drawer *drawer = DrawerList();
    if (drawer)
        count = drawer->Count();

    drawer_version = DRAWER_VERSION;
    df.Write(DRAWER_VERSION);
    df.Write(count, 1);
    while (drawer)
    {
        drawer->Write(df, DRAWER_VERSION);
        drawer = drawer->next;
    }

    // Save Checks
    count = 0;
    Check *c = CheckList();
    if (c)
        count = c->Count();

    check_version = CHECK_VERSION;
    df.Write(CHECK_VERSION);
    df.Write(count, 1);
    while (c)
    {
        c->Write(df, CHECK_VERSION);
        c = c->next;
    }

    // Save Tips
    count = 0;
    TipEntry *te = tip_db.TipList();
    if (te)
        count = te->Count();

    tip_version = TIP_VERSION;
    df.Write(TIP_VERSION);
    df.Write(count, 1);
    while (te)
    {
        te->Write(df, TIP_VERSION);
        te = te->next;
    }

    // Save Exceptions
    exception_version = EXCEPTION_VERSION;
    df.Write(EXCEPTION_VERSION);
    if (exception_db.Write(df, EXCEPTION_VERSION))
    {
        ReportError("Error saving archive exception data");
        return 1;
    }

    // Save Expenses (version 8)
    expense_version = EXPENSE_VERSION;
    df.Write(EXPENSE_VERSION);
    if (expense_db.Write(df, EXPENSE_VERSION))
    {
        ReportError("Error saving archive expense data");
        return 1;
    }

    // Save Media (version 10)
    media_version = SETTINGS_VERSION;
    df.Write(media_version);
    df.Write(DiscountCount());
    DiscountInfo *discount = DiscountList();
    while (discount != NULL)
    {
        discount->Write(df, media_version);
        discount = discount->next;
    }

    df.Write(CouponCount());
    CouponInfo *coupon = CouponList();
    while (coupon != NULL)
    {
        coupon->Write(df, media_version);
        coupon = coupon->next;
    }

    df.Write(CreditCardCount());
    CreditCardInfo *creditcard = CreditCardList();
    while (creditcard != NULL)
    {
        creditcard->Write(df, media_version);
        creditcard = creditcard->next;
    }

    df.Write(CompCount());
    CompInfo *comp = CompList();
    while (comp != NULL)
    {
        comp->Write(df, media_version);
        comp = comp->next;
    }

    df.Write(MealCount());
    MealInfo *meal = MealList();
    while (meal != NULL)
    {
        meal->Write(df, media_version);
        meal = meal->next;
    }

    df.Write(tax_food);
    df.Write(tax_alcohol);
    df.Write(tax_room);
    df.Write(tax_merchandise);
    df.Write(tax_GST);
    df.Write(tax_PST);
    df.Write(tax_HST);
    df.Write(tax_QST);
    df.Write(royalty_rate);
    df.Write(price_rounding);
    df.Write(change_for_credit);
    df.Write(change_for_roomcharge);
    df.Write(change_for_checks);
    df.Write(change_for_gift);
    df.Write(discount_alcohol);
    df.Write(tax_VAT);

    if (cc_exception_db == NULL)
        cc_exception_db = new CreditDB(CC_DBTYPE_EXCEPT);
    cc_exception_db->Write(df);
    if (cc_refund_db == NULL)
        cc_refund_db = new CreditDB(CC_DBTYPE_REFUND);
    cc_refund_db->Write(df);
    if (cc_void_db == NULL)
        cc_void_db = new CreditDB(CC_DBTYPE_VOID);
    cc_void_db->Write(df);

    if (cc_init_results == NULL)
        cc_init_results = new CCInit();
    cc_init_results->Write(df);
    if (cc_saf_details_results == NULL)
        cc_saf_details_results = new CCSAFDetails();
    cc_saf_details_results->Write(df);
    if (cc_settle_results == NULL)
        cc_settle_results = new CCSettle();
    cc_settle_results->Write(df);

    df.Write(advertise_fund);

    changed = 0;  // can't have changed:  we just saved it
    from_disk = 1;  // it is now on disc, no need for system restart

    return 0;
}

int Archive::Unload()
{
    FnTrace("Archive::Unload()");

    if (loaded == 0)
        return 1;

    if (changed)
        SavePacked();

    check_list.Purge();
    drawer_list.Purge();
    discount_list.Purge();
    coupon_list.Purge();
    creditcard_list.Purge();
    comp_list.Purge();
    meal_list.Purge();
    tip_db.Purge();
    work_db.Purge();
    expense_db.Purge();

    delete cc_exception_db;
    cc_exception_db = NULL;

    delete cc_refund_db;
    cc_refund_db = NULL;

    delete cc_void_db;
    cc_void_db = NULL;

    delete cc_init_results;
    cc_init_results = NULL;

    delete cc_saf_details_results;
    cc_saf_details_results = NULL;

    delete cc_settle_results;
    cc_settle_results = NULL;

    loaded = 0;
    return 0;
}

int Archive::Add(Check *c)
{
    FnTrace("Archive::Add(Check)");
    if (loaded == 0 || c == NULL || c->Status() == CHECK_OPEN)
        return 1; // can't archive open check

    c->archive = this;
    check_list.AddToTail(c);
    if (c->serial_number > last_serial_number)
        last_serial_number = c->serial_number;

    changed = 1;
    return 0;
}

int Archive::Remove(Check *c)
{
    FnTrace("Archive::Remove(Check)");
    if (c == NULL || c->archive != this)
        return 1;

    c->archive = NULL;
    check_list.Remove(c);

    changed = 1;
    return 0;
}

int Archive::Add(Drawer *drawer)
{
    FnTrace("Archive::Add(Drawer)");
    if (drawer == NULL || loaded == 0)
        return 1;

    drawer->archive = this;
    drawer_list.AddToTail(drawer);

    if (drawer->serial_number > last_serial_number)
        last_serial_number = drawer->serial_number;

    changed = 1;
    return 0;
}

int Archive::Remove(Drawer *drawer)
{
    FnTrace("Archive::Remove(Drawer)");
    if (drawer == NULL || drawer->archive != this)
        return 1;

    drawer->archive = NULL;
    drawer_list.Remove(drawer);

    changed = 1;
    return 0;
}

int Archive::Add(WorkEntry *we)
{
    FnTrace("Archive::Add(WorkEntry)");
    if (we == NULL || loaded == 0)
        return 1;

    work_db.Add(we);
    changed = 1;
    return 0;
}

int Archive::Remove(WorkEntry *we)
{
    FnTrace("Archive::Remove(WorkEntry)");
    return work_db.Remove(we);
}

int Archive::Add(DiscountInfo *discount)
{
    FnTrace("Archive::Add(Discount)");
    if (discount == NULL)
        return 1;
    return discount_list.AddToTail(discount);
}

int Archive::Add(CouponInfo *coupon)
{
    FnTrace("Archive::Add(Coupon)");
    if (coupon == NULL)
        return 1;
    return coupon_list.AddToTail(coupon);
}

int Archive::Add(CreditCardInfo *creditcard)
{
    FnTrace("Archive::Add(CreditCard)");
    if (creditcard == NULL)
        return 1;
    return creditcard_list.AddToTail(creditcard);
}

int Archive::Add(CompInfo *comp)
{
    FnTrace("Archive::Add(Comp)");
    if (comp == NULL)
        return 1;
    return comp_list.AddToTail(comp);
}

int Archive::Add(MealInfo *meal)
{
    FnTrace("Archive::Add(Meal)");
    if (meal == NULL)
        return 1;
    return meal_list.AddToTail(meal);
}

int Archive::DiscountCount()
{
    FnTrace("Archive::DiscountCount()");
    int count = 0;
    DiscountInfo *discount = DiscountList();
    while (discount != NULL)
    {
        count += 1;
        discount = discount->next;
    }
    return count;
}

int Archive::CouponCount()
{
    FnTrace("Archive::CouponCount()");
    int count = 0;
    CouponInfo *coupon = CouponList();
    while (coupon != NULL)
    {
        count += 1;
        coupon = coupon->next;
    }
    return count;
}

int Archive::CreditCardCount()
{
    FnTrace("Archive::CreditCardCount()");
    int count = 0;
    CreditCardInfo *creditcard = CreditCardList();
    while (creditcard != NULL)
    {
        count += 1;
        creditcard = creditcard->next;
    }
    return count;
}

int Archive::CompCount()
{
    FnTrace("Archive::CompCount()");
    int count = 0;
    CompInfo *comp = CompList();
    while (comp != NULL)
    {
        count += 1;
        comp = comp->next;
    }
    return count;
}

int Archive::MealCount()
{
    FnTrace("Archive::MealCount()");
    int count = 0;
    MealInfo *meal = MealList();
    while (meal != NULL)
    {
        count += 1;
        meal = meal->next;
    }
    return count;
}

DiscountInfo *Archive::FindDiscountByID(int discount_id)
{
    FnTrace("Archive::FindDiscountByID()");
    for (DiscountInfo *ds = discount_list.Head(); ds != NULL; ds = ds->next)
    {
        if (ds->id == discount_id)
            return ds;
    }
    return NULL;
}

CouponInfo *Archive::FindCouponByID(int coupon_id)
{
    FnTrace("Archive::FindCouponByID()");
    for (CouponInfo *cp = coupon_list.Head(); cp != NULL; cp = cp->next)
    {
        if (cp->id == coupon_id)
            return cp;
    }
    return NULL;
}

CompInfo *Archive::FindCompByID(int comp_id)
{
    FnTrace("Archive::FindCompByID()");
    for (CompInfo *cm = comp_list.Head(); cm != NULL; cm = cm->next)
    {
        if (cm->id == comp_id)
            return cm;
    }
    return NULL;
}

CreditCardInfo *Archive::FindCreditCardByID(int creditcard_id)
{
    FnTrace("Archive::FindCreditCardByID()");
    for (CreditCardInfo *cc = creditcard_list.Head(); cc != NULL; cc = cc->next)
    {
        if (cc->id == creditcard_id)
            return cc;
    }
    return NULL;
}

MealInfo *Archive::FindMealByID(int meal_id)
{
    FnTrace("Archive::FindMealByID()");
    for (MealInfo *mi = meal_list.Head(); mi != NULL; mi = mi->next)
    {
        if (mi->id == meal_id)
            return mi;
    }
    return NULL;
}
