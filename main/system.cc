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
 * system.cc - revision 115 (9/8/98)
 * Implementation of system module
 * (see system_report.cc for report functions)
 */

#include "manager.hh"
#include "system.hh"
#include "data_file.hh"
#include "terminal.hh"
#include "manager.hh"
#include "locale.hh"
#include "zone.hh"
#include "drawer.hh"
#include "check.hh"
#include "archive.hh"
#include "customer.hh"
#include "utility.hh"
#include <dirent.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <cstring>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** Globals ****/
System *MasterSystem = NULL;


/**** System Class ****/
// Constructor
System::System()
{
    last_archive_id        = 0;
    last_serial_number     = 0;
    report_sort_method     = 0;
    phrases_changed        = 0;
    data_path.Set(VIEWTOUCH_PATH "/dat");
    temp_path.Set("/tmp");
    non_eod_settle         = 0;
    eod_term               = NULL;

    cc_void_db             = new CreditDB(CC_DBTYPE_VOID);
    cc_exception_db        = new CreditDB(CC_DBTYPE_EXCEPT);
    cc_refund_db           = new CreditDB(CC_DBTYPE_REFUND);

    cc_init_results        = new CCInit();
    cc_totals_results      = new CCDetails();
    cc_details_results     = new CCDetails();
    cc_saf_details_results = new CCSAFDetails();
    cc_settle_results      = new CCSettle();
    cc_finish              = NULL;

    cc_report_type         = CC_REPORT_BATCH;
}

System::~System()
{
    if (cc_init_results != NULL)
        delete cc_init_results;
    if (cc_totals_results != NULL)
        delete cc_totals_results;
    if (cc_details_results != NULL)
        delete cc_details_results;
    if (cc_saf_details_results != NULL)
        delete cc_saf_details_results;
    if (cc_settle_results != NULL)
        delete cc_settle_results;

    if (cc_void_db != NULL)
        delete cc_void_db;
    if (cc_exception_db != NULL)
        delete cc_exception_db;
    if (cc_refund_db != NULL)
        delete cc_refund_db;
}

// Member Functions
int System::LicenseExpired()
{
    return (expire.IsSet() && SystemTime >= expire);
}

int System::InitCurrentDay()
{
    FnTrace("System::InitCurrentDay()");
    for (Drawer *drawer = DrawerList(); drawer != NULL; drawer = drawer->next)
        drawer->Total(CheckList());

    CreateFixedDrawers();
    user_db.Init(&labor_db);
    return 0;
}

int System::LoadCurrentData(const char* path)
{
	FnTrace("System::LoadCurrentData()");
	if (path == NULL)
		return 1;

	DIR *dp = opendir(path);
	if (dp == NULL)
	{
		ReportError("Can't find current data directory");
		return 1;
	}

	current_path.Set(path);
	char str[256];
    const char* name;
	struct dirent *record = NULL;
	do
	{
		record = readdir(dp);
		if (record)
		{
			name = record->d_name;
            int len = strlen(name);
            if (strcmp(&name[len-4], ".fmt") == 0)
                continue;
			if (strncmp(name, "check_", 6) == 0)
			{
				sprintf(str, "%s/%s", path, name);
				Check *check = new Check;
				if (check == NULL)
					ReportError("Couldn't create check");
				else
				{
					if (check->Load(&settings, str))
					{
						ReportError("Error in loading check");
						delete check;
					}
					else
						Add(check);
				}
			}
			else if (strncmp(name, "drawer_", 7) == 0)
			{
				sprintf(str, "%s/%s", path, name);
				Drawer *drawer = new Drawer;
				if (drawer == NULL)
					ReportError("Couldn't Create Drawer");
				else
				{
					if (drawer->Load(str))
					{
						ReportError("Error in loading drawer");
						delete drawer;
					}
					else
						Add(drawer);
				}
			}
            else if (strcmp(name, "ccvoiddb") == 0)
            {
                sprintf(str, "%s/%s", path, name);
                cc_void_db->Load(str);
            }
            else if (strcmp(name, "ccrefunddb") == 0)
            {
                sprintf(str, "%s/%s", path, name);
                cc_refund_db->Load(str);
            }
            else if (strcmp(name, "ccexceptiondb") == 0)
            {
                sprintf(str, "%s/%s", path, name);
                cc_exception_db->Load(str);
            }
		}
	}
	while (record);
	closedir(dp);
	return 0;
}

/****
 * BackupCurrentData:  This method should really only be called
 *  from System::EndDay().  It will copy all data in current
 *  to a directory the name of which will contain the current
 *  date.
 *  Fails if current_path has not been set.  Returns 1 on failure.
 ****/
int System::BackupCurrentData()
{
    FnTrace("System::BackupCurrentData()");
    int retval = 0;
    char bakname[STRLONG];
    char command[STRLONG];

    if (current_path.length < 1)
        retval = 1;
    else
    {
        snprintf(bakname, STRLONG, "%s/current_%04d%02d%02d%02d%02d.tar.gz",
                 backup_path.Value(), SystemTime.Year(),
                 SystemTime.Month(), SystemTime.Day(),
                 SystemTime.Hour(), SystemTime.Min());
        snprintf(command, STRLONG, "tar czf %s %s",
                 bakname, current_path.Value());
        system(command);
    }

    return retval;
}

int System::ScanArchives(const char* path, const char* altmedia)
{
    FnTrace("System::ScanArchives()");
    if (path)
        archive_path.Set(path);

    DIR *dp = opendir(archive_path.Value());
    if (dp == NULL)
    {
        ReportError("Can't find archive directory");
        return 1;
    }

    struct dirent *record = NULL;
    do
    {
        record = readdir(dp);
        if (record)
        {
            const genericChar* name = record->d_name;
            if (strncmp(name, "archive_", 8) == 0)
            {
                int len = strlen(name);
                if (strcmp(&name[len-4], ".bak") == 0)
                    continue;
                if (strcmp(&name[len-4], ".fmt") == 0)
                    continue;

                genericChar str[256];
                sprintf(str, "%s/%s", archive_path.Value(), name);
                Archive *archive = new Archive(&settings, str);
                archive->altmedia.Set(altmedia);
                if (archive == NULL)
                    ReportError("Couldn't create archive");
                else
                {
                    if (archive->id > last_archive_id)
                        last_archive_id = archive->id;
                    Add(archive);
                }
            }
        }
    }
    while (record);

    Archive *archive;

    // set start time on all archives
    archive = ArchiveList();
    while (archive)
    {
        if (archive->fore && !archive->start_time.IsSet())
            archive->start_time = archive->fore->end_time;
        archive = archive->next;
    }

    // load last archive with checks
    archive = ArchiveListEnd();
    while (archive)
    {
        archive->LoadPacked(&settings);
        if (archive->last_serial_number > 0)
        {
            last_serial_number = archive->last_serial_number;
            break;
        }
        archive = archive->fore;
    }

    closedir(dp);
    return 0;
}

int System::UnloadArchives()
{
    FnTrace("System::UnloadArchives()");
    Archive *archive = ArchiveList();

    while (archive != NULL)
    {
        archive->Unload();
        archive = archive->next;
    }
    return 0;
}

int System::Add(Archive *archive)
{
    FnTrace("System::Add(Archive)");
    if (archive == NULL)
        return 1;  // Add failed

    // start at end of list and work backwords
    Archive *ptr = ArchiveListEnd();
    while (ptr && archive->end_time < ptr->end_time)
        ptr = ptr->fore;

    // Insert 'archive' after 'ptr'
    return archive_list.AddAfterNode(ptr, archive);
}

int System::Remove(Archive *archive)
{
    return archive_list.Remove(archive);
}

Archive *System::NewArchive()
{
    FnTrace("System::NewArchive()");
    Archive *archive = new Archive(SystemTime);
    if (archive == NULL)
        return NULL;

    genericChar str[256];
    archive->id = ++last_archive_id;
    sprintf(str, "%s/archive_%06d", archive_path.Value(), archive->id);
    archive->filename.Set(str);

    if (ArchiveListEnd())
        archive->start_time = ArchiveListEnd()->end_time;
    Add(archive);
    return archive;
}

Archive *System::FindByTime(TimeInfo &timevar)
{
    FnTrace("System::FindByTime()");
    Archive *archive = ArchiveListEnd();
    Archive *last = NULL;
    while (archive)
    {
        if (timevar >= archive->end_time)
            break;
        last = archive;
        archive = archive->fore;
    }
    return last;
}

Archive *System::FindByStart(TimeInfo &timevar)
{
    FnTrace("System::FindByStart()");
    if (!timevar.IsSet())
        return ArchiveList();

    Archive *archive    = ArchiveListEnd();
    Archive *last = NULL;
    while (archive)
    {
        if (timevar > archive->end_time)
            break;
        last = archive;
        archive = archive->fore;
    }
    if (last)
        return last->next;
    else
        return NULL;
}

int System::SaveChanged()
{
    FnTrace("System::SaveChanged()");

    for (Archive *archive = ArchiveList(); archive != NULL; archive = archive->next)
    {
        if (archive->changed)
            archive->SavePacked();
    }

    return 0;
}

int System::Add(Check *check)
{
    FnTrace("System::Add(Check)");
    int retval = 0;
    int done = 0;
    Check *currcheck = CheckListEnd();
    if (check == NULL)
        return 1;

    if (check->serial_number <= 0)
        check->serial_number = NewSerialNumber();
    if (check->serial_number > last_serial_number)
        last_serial_number = check->serial_number;

    check->archive = NULL; 
    if (currcheck == NULL)
        retval = check_list.AddToTail(check);
    else
    {
        while (currcheck != NULL && !done)
        {
            if (check->serial_number > currcheck->serial_number)
            {
                check_list.AddAfterNode(currcheck, check);
                done = 1;
            }
            else
                currcheck = currcheck->fore;
        }
        if (done == 0)
            check_list.AddToHead(check);
    }

    return retval;
}

int System::Remove(Check *check)
{
    if (check == NULL || check->archive)
        return 1;

    return check_list.Remove(check);
}

int System::Add(Drawer *drawer)
{
    FnTrace("System::Add(Drawer)");
    if (drawer == NULL)
        return 1;

    drawer->archive = NULL;
    if (drawer->serial_number <= 0)
        drawer->serial_number = NewSerialNumber();
    else if (drawer->serial_number > last_serial_number)
        last_serial_number = drawer->serial_number;

    // start at end of list and work backwords
    Drawer *ptr = DrawerListEnd();
    while (ptr && (drawer->owner_id < ptr->owner_id))
        ptr = ptr->fore;

    return drawer_list.AddAfterNode(ptr, drawer);
}

int System::Remove(Drawer *drawer)
{
    if (drawer == NULL || drawer->archive)
        return 1;

    return drawer_list.Remove(drawer);
}

int System::Add(WorkEntry *we)
{
    FnTrace("System::Add(WorkEntry)");
    if (we == NULL)
        return 1;

    Archive *archive = FindByTime(we->start);
    if (archive == NULL)
    {
        work_db.Add(we);
        return 0;
    }
    else if (we->end <= archive->end_time)
    {
        archive->Add(we);
        return 0;
    }

    while (archive)
    {
        if (we->end.IsSet() && we->end <= archive->end_time)
        {
            archive->Add(we);
            return 0;
        }
        WorkEntry *w = we->Copy();
        w->end.Clear();
        archive->Add(w);

        we->start.Clear();
        archive = archive->next;
    }
    return 0;
}

int System::Remove(WorkEntry *we)
{
    return work_db.Remove(we);
}

/****
 * EndDay:  Copies all of today's data into a new archive and adds the archive
 *  to the list.
 *  NOTE:  As part of the process, all other archives are forced to reload.
 *   So if you're seeing duplication of data after EndDay(), make sure you
 *   purge all archive data in the Archive::Unload() method (do not delete
 *   any files, just get rid of in-memory data).
 ****/
int System::EndDay()
{
    FnTrace("System::EndDay()");

    if (!AllDrawersPulled())
        return 1;  // all drawers must be pulled at once to end day

    UnloadArchives();
    BackupCurrentData();

    // delete training checks
    Check *check_next;
    Check *check = CheckList();
    while (check)
    {
        check_next = check->next;
        if (check->IsTraining())
        {
            Remove(check);
            delete check;
        }
        check = check_next;
    }

    tip_db.Update(this);
    Archive *archive = NewArchive();
    if (archive == NULL)
        return 1;

    archive->cc_exception_db = cc_exception_db->Copy();
    cc_exception_db->Purge();

    archive->cc_refund_db = cc_refund_db->Copy();
    cc_refund_db->Purge();

    archive->cc_void_db = cc_void_db->Copy();
    cc_void_db->Purge();

    archive->cc_init_results = cc_init_results;
    cc_init_results = new CCInit();

    // For SAF Details and Settlement, we run those actions and then
    // move them into an archive.  If a user then goes directly to the
    // credit card reports, he or she should see these reports.  AKA,
    // we need to set archive and current in each.
    archive->cc_saf_details_results = cc_saf_details_results;
    cc_saf_details_results          = new CCSAFDetails();
    cc_saf_details_results->archive = archive;
    cc_saf_details_results->current = archive->cc_saf_details_results->Last();

    archive->cc_settle_results      = cc_settle_results;
    cc_settle_results               = new CCSettle();
    cc_settle_results->archive      = archive;
    cc_settle_results->current      = archive->cc_settle_results->Last();

    // Archive Drawers
    Drawer *drawer = DrawerList();
    while (drawer)
    {
        Drawer *d_next = drawer->next;
        drawer->Total(CheckList());
        Remove(drawer);
        drawer->DestroyFile();
        if (drawer->IsEmpty())
            delete drawer;
        else
            archive->Add(drawer);
        drawer = d_next;
    }

    // Archive Tips
    archive->tip_db.Copy(&tip_db);

    // Move all open checks to temp check list
    DList<Check> tmp_list;
    Check *tmp = NULL;
    check = CheckList();
    while (check)
    {
        check_next = check->next;
        tmp = ExtractOpenCheck(check);
        check = check_next;

        tmp_list.AddToTail(tmp);
    }

    // Archive all remaining closed checks
    check = CheckList();
    while (check)
    {
        check_next = check->next;
        Remove(check);
        check->DestroyFile();
        if (archive->Add(check))
        {
            ReportError("Error in adding check to archive");
            delete check;
        }
        check = check_next;
    }

    // Move open checks back to todays checks
    while (tmp_list.Head())
    {
        Check *check_tmp = tmp_list.Head();
        tmp_list.Remove(check_tmp);
        Add(check_tmp);
        check_tmp->Save();
    }

    // Move exceptions to archive
    exception_db.MoveTo(&archive->exception_db);
    exception_db.Save();

    // Move expenses to archive
    expense_db.MoveTo(&(archive->expense_db), archive->DrawerList());
    expense_db.Save();
    // recalculate DrawerBalances
    expense_db.AddDrawerPayments(DrawerList());
    archive->expense_db.AddDrawerPayments(archive->DrawerList());

    // Copy media data into the archive
    DiscountInfo *discount = settings.DiscountList();
    while (discount != NULL)
    {
        archive->Add(discount->Copy());
        discount = discount->next;
    }
    CouponInfo *coupon = settings.CouponList();
    while (coupon != NULL)
    {
        archive->Add(coupon->Copy());
        coupon = coupon->next;
    }
    CreditCardInfo *creditcard = settings.CreditCardList();
    while (creditcard != NULL)
    {
        archive->Add(creditcard->Copy());
        creditcard = creditcard->next;
    }
    CompInfo *comp = settings.CompList();
    while (comp != NULL)
    {
        archive->Add(comp->Copy());
        comp = comp->next;
    }
    MealInfo *meal = settings.MealList();
    while (meal != NULL)
    {
        archive->Add(meal->Copy());
        meal = meal->next;
    }

    archive->tax_food              = settings.tax_food;
    archive->tax_alcohol           = settings.tax_alcohol;
    archive->tax_room              = settings.tax_room;
    archive->tax_merchandise       = settings.tax_merchandise;
    archive->tax_GST               = settings.tax_GST;
    archive->tax_PST               = settings.tax_PST;
    archive->tax_HST               = settings.tax_HST;
    archive->tax_QST               = settings.tax_QST;
    archive->royalty_rate          = settings.royalty_rate;
    archive->change_for_checks     = settings.change_for_checks;
    archive->change_for_credit     = settings.change_for_credit;
    archive->change_for_gift       = settings.change_for_gift;
    archive->change_for_roomcharge = settings.change_for_roomcharge;
    archive->discount_alcohol      = settings.discount_alcohol;
    archive->price_rounding        = settings.price_rounding;

    // Save Archive
    archive->SavePacked();

    // Prepare for new day
    CreateFixedDrawers();
    tip_db.Update(this);
    settings.RemoveInactiveMedia();

    return 0;
}

/****
 * LastEndDay:  Returns the number of hours since the last time the End Day
 *   procedure was run.  NOTE:  for this to produce any meaningful result,
 *   EndDay() has to have been run at least once because it checks against
 *   the most recent archive; if EndDay() has never been run, there are no
 *   archives.
 ****/
int System::LastEndDay()
{
    FnTrace("System::LastEndDay()");

    Archive *lastarc = ArchiveListEnd();
    int hours = 0;
    int minutes = 0;

    if (lastarc)
    {
        minutes = MinutesElapsedToNow(lastarc->end_time);
        hours = minutes / 60;
    }
    return hours;
}

/****
 * CheckEndDay:  Do we even need to bother running EndDay?  If there are no
 *  open checks and no open drawers, then no.  So return 0.  Otherwise,
 *  return 1 indicating that running EndDay is certainly a viable option.
 ****/
int System::CheckEndDay(Terminal *term)
{
    FnTrace("System::CheckEndDay()");
    int retval = 0;
    Drawer *drawer = DrawerList();
    Check  *check  = CheckList();

    while (drawer != NULL && retval == 0)
    {
        if (!drawer->IsEmpty())
            retval = 1;
        drawer = drawer->next;
    }

    while (check != NULL)
    {
        retval += 1;
        check = check->next;
    }

    return retval;
}

int System::SetDataPath(const char* path)
{
    FnTrace("System::SetDataPath()");
    if (path == NULL)
        return 1;

    genericChar str[256] = "";
    genericChar tmp[256] = "";
    if (DoesFileExist(path) == 0)
    {
        sprintf(str, "Can't find path '%s'", path);
        ReportError(str);
        return 1;
    }

    unsigned int len = strlen(path);
    if (len >= sizeof(str))
    	len = sizeof(str)-1;
    while (len > 1)
    {
        if (path[len - 1] != '/')
            break;
        //path[len - 1] = '\0';
        --len;
    }
    memcpy(str,path,len);
    str[len] = 0;
    data_path.Set(str);
    //memset(str,256,0);

    // Make sure all data directories in path are set up
    chmod(path, DIR_PERMISSIONS);
    sprintf(str, "%s/current", path);
    EnsureFileExists(str);

    // consolidate checks & drawers
    sprintf(tmp, "%s/checks", path);
    if (DoesFileExist(tmp))
    {
        genericChar cmd[256];
        sprintf(cmd, "/bin/mv %s/* %s", tmp, str);
        system(cmd);
        sprintf(cmd, "/bin/rmdir %s", tmp);
        system(cmd);
    }

    sprintf(tmp, "%s/drawers", path);
    if (DoesFileExist(tmp))
    {
        genericChar cmd[256];
        sprintf(cmd, "/bin/mv %s/* %s", tmp, str);
        system(cmd);
        sprintf(cmd, "/bin/rmdir %s", tmp);
        system(cmd);
    }

    sprintf(str, "%s/%s", path, ARCHIVE_DATA_DIR);
    EnsureFileExists(str);

    sprintf(str, "%s/%s", path, LABOR_DATA_DIR);
    EnsureFileExists(str);

    sprintf(str, "%s/%s", path, STOCK_DATA_DIR);
    EnsureFileExists(str);

    sprintf(str, "%s/%s", path, LANGUAGE_DATA_DIR);
    EnsureFileExists(str);

    sprintf(str, "%s/%s", path, ACCOUNTS_DATA_DIR);
    EnsureFileExists(str);

    sprintf(str, "%s/%s", path, EXPENSE_DATA_DIR);
    EnsureFileExists(str);

    sprintf(str, "%s/%s", path, CUSTOMER_DATA_DIR);
    EnsureFileExists(str);

    sprintf(str, "%s/%s", path, HTML_DATA_DIR);
    EnsureFileExists(str);

    sprintf(str, "%s/%s", path, TEXT_DATA_DIR);
    EnsureFileExists(str);

    sprintf(str, "%s/%s", path, PAGEEXPORTS_DIR);
    EnsureFileExists(str);

    sprintf(str, "%s/%s", path, PAGEIMPORTS_DIR);
    EnsureFileExists(str);

    sprintf(str, "%s/%s", path, UPDATES_DATA_DIR);
    EnsureFileExists(str);

    sprintf(str, "%s/%s", path, BACKUP_DATA_DIR);
    backup_path.Set(str);
    EnsureFileExists(str);

    return 0;
}

/****
 * CheckFileUpdate:  If an update exists (".../dat/updates/<file>"), moves
 *  the file to the dat directory.
 * Returns 1 if the file exists and is successfully moved, 0 otherwise.
 ****/
int System::CheckFileUpdate(const char* file)
{
    FnTrace("System::CheckFileUpdate()");
    int retval = 0;
    char update[STRLENGTH];
    char buffer[STRLENGTH];
    char newfile[STRLENGTH];
    char backup[STRLENGTH];

    snprintf(update, STRLENGTH, "%s/%s/%s", data_path.Value(), UPDATES_DATA_DIR, file);
    if (DoesFileExist(update))
    {
        snprintf(buffer, STRLENGTH, "Updating %s", update);
        ReportError(buffer);
        snprintf(newfile, STRLENGTH, "%s/%s", data_path.Value(), file);
        if (DoesFileExist(newfile))
        {
            snprintf(backup, STRLENGTH, "%s.%04d%02d%02d%02d%02d", newfile,
                     SystemTime.Year(), SystemTime.Month(), SystemTime.Day(),
                     SystemTime.Hour(), SystemTime.Min());
            snprintf(buffer, STRLENGTH, "  Saving original as %s", backup);
            ReportError(buffer);
            rename(newfile, backup);
        }
        if (rename(update, newfile) == 0)
            retval = 1;
    }

    return retval;
}

int System::CheckFileUpdates()
{
    FnTrace("System::CheckFileUpdates()");
    int retval = 0;

    retval += CheckFileUpdate(MASTER_MENU_DB);
    retval += CheckFileUpdate(MASTER_SETTINGS);
    retval += CheckFileUpdate(MASTER_DISCOUNTS);
    retval += CheckFileUpdate(MASTER_LOCALE);
    retval += CheckFileUpdate(MASTER_ZONE_DB1);
    retval += CheckFileUpdate(MASTER_ZONE_DB2);
    retval += CheckFileUpdate(MASTER_ZONE_DB3);

    return retval;
}

char* System::FullPath(const char* filename, genericChar* buffer)
{
    FnTrace("System::FullPath()");
    static genericChar str[256];

    if (buffer == NULL)
        buffer = str;

    sprintf(buffer, "%s/%s", data_path.Value(), filename);
    return buffer;
}

int System::ClearSystem(int all)
{
    FnTrace("System::ClearSystem()");
    // Ouch! this kills all recorded data & takes down system
    genericChar  str[STRLONG];
    const genericChar* p = data_path.Value();
    sprintf(str, "%s/error_log.txt", p);
    DeleteFile(str);
    sprintf(str, "%s/exception.dat", p);
    DeleteFile(str);
    snprintf(str, STRLONG, "/bin/rm -r %s/%s %s/%s %s/%s",
            p, ARCHIVE_DATA_DIR, p, CURRENT_DATA_DIR,
            p, STOCK_DATA_DIR);
    system(str);
    if (all)
    {
        snprintf(str, STRLONG, "/bin/rm -r %s/%s", p, LABOR_DATA_DIR);
        system(str);
    }
    return EndSystem();
}

int System::NewSerialNumber()
{
    FnTrace("System::NewSerialNumber()");
    ++last_serial_number;
    return last_serial_number;
}

char* System::NewPrintFile(char* str)
{
    FnTrace("System::NewPrintFile()");
    static int counter = 0;
    static genericChar buffer[256];
    if (str == NULL)
        str = buffer;

    ++counter;
    sprintf(str, "%s/printqueue/%06d", data_path.Value(), counter);
    return str;
}

Check *System::FirstCheck(Archive *archive)
{
    FnTrace("System::FirstCheck()");
    if (archive)
    {
        if (archive->loaded == 0)
            archive->LoadPacked(&settings);
        return archive->CheckList();
    }
    else
        return CheckList();
}

Drawer *System::FirstDrawer(Archive *archive)
{
    FnTrace("System::FirstDrawer()");
    if (archive)
    {
        if (archive->loaded == 0)
            archive->LoadPacked(&settings);
        return archive->DrawerList();
    }
    else
        return DrawerList();
}

ItemException *System::FirstItemException(Archive *archive)
{
    FnTrace("System::FirstItemException()");
    if (archive)
    {
        if (archive->loaded == 0)
            archive->LoadPacked(&settings);
        return archive->exception_db.ItemList();
    }
    else
        return exception_db.ItemList();
}

TableException *System::FirstTableException(Archive *archive)
{
    FnTrace("System::FirstTableException()");
    if (archive)
    {
        if (archive->loaded == 0)
            archive->LoadPacked(&settings);
        return archive->exception_db.TableList();
    }
    else
        return exception_db.TableList();
}

RebuildException *System::FirstRebuildException(Archive *archive)
{
    FnTrace("System::FirstRebuildException()");
    if (archive)
    {
        if (archive->loaded == 0)
            archive->LoadPacked(&settings);
        return archive->exception_db.RebuildList();
    }
    else
        return exception_db.RebuildList();
}

int System::CountOpenChecks(Employee *e)
{
    FnTrace("System::CountOpenChecks()");
    int id = 0;
    if (e)
        id = e->id;
    
    int count = 0;
    int ctype;
    TimeInfo now;
    now.Set();
    now.AdjustMinutes(60);

    for (Check *check = CheckList(); check != NULL; check = check->next)
    {
        if (check->IsTraining())
            continue;
        if (id > 0 && check->user_owner != id)
            continue;
        if (check->Status() != CHECK_OPEN)
            continue;
        
        ctype = check->CustomerType();
        if (ctype == CHECK_HOTEL)
            continue;
        
        // Take Out, Delivery, and Catering orders are only counted as open
        // if they are past due.  Otherwise, they may need to be open because
        // they have a delivery/pickup date sometime in the future.  This is
        // especially true of catering, but even deliveries may be scheduled
        // in advance.
        if ((ctype == CHECK_TAKEOUT || ctype == CHECK_DELIVERY || ctype == CHECK_CATERING) &&
            check->date > now)
        {
            continue;
        }
        ++count;
    }
    
    return count;
}

int System::NumberStacked(const char* table, Employee *e)
{
    FnTrace("System::NumberStacked()");
    if (e == NULL)
        return 0;

    int count = 0;
    for (Check *check = CheckList(); check != NULL; check = check->next)
        if (check->IsTraining() == e->training && check->Status() == CHECK_OPEN &&
            strcmp(check->Table(), table) == 0)
            ++count;
    return count;
}

Check *System::FindOpenCheck(const char* table, Employee *e)
{
    FnTrace("System::FindOpenCheck()");
    if (e == NULL)
        return NULL;

    for (Check *check = CheckListEnd(); check != NULL; check = check->fore)
    {
        if (check->IsTraining() == e->training && strcmp(check->Table(), table) == 0 &&
            check->Status() == CHECK_OPEN)
        {
            return check;
        }
    }
    return NULL;
}

Check *System::FindCheckByID(int check_id)
{
    FnTrace("System::FindCheckByID()");
    Check *retval = NULL;
    Check *currcheck = CheckList();

    while (currcheck != NULL)
    {
        if (currcheck->serial_number == check_id)
        {
            retval = currcheck;
            currcheck = NULL;
        }
        else
            currcheck = currcheck->next;
    }

    return retval;
}

Check *System::ExtractOpenCheck(Check *check)
{
    FnTrace("System::ExtractOpenCheck()");
    if (check == NULL || check->IsTraining())
        return NULL;
    SubCheck *sc;

    int count = 0;
    for (sc = check->SubList(); sc != NULL; sc = sc->next)
    {
        if (sc->status == CHECK_OPEN)
            ++count;
    }

    if (count >= check->SubCount())
    {
        // Entire check is extracted
        Remove(check);
        return check;
    }
    else if (count <= 0)
        return NULL; // no closed sub-checks

    Check *oc = new Check;
    oc->Table(check->Table());
    oc->time_open     = check->time_open;
    oc->user_open     = check->user_open;
    oc->user_owner    = check->user_owner;
    oc->serial_number = NewSerialNumber();

    sc = check->SubList();
    while (sc)
    {
        SubCheck *tmp = sc->next;
        if (sc->status == CHECK_OPEN)
        {
            check->Remove(sc);
            int g = Max(check->Guests() - 1, 0);
            check->Guests(g);
            oc->Add(sc);
            g = oc->Guests() + 1;
            oc->Guests(g);
        }
        sc = tmp;
    }

    // guest count will be off but oh well...
    if (check->Guests() <= 0 && ! (check->IsTakeOut() || check->IsFastFood()))
        check->Guests(1);
    return oc;
}

int System::SaveCheck(Check *check)
{
    FnTrace("System::SaveCheck()");
    if (check == NULL || check->IsTraining() || check->archive)
        return 1;

    if (check->serial_number <= 0)
        check->serial_number = NewSerialNumber();

    if (check->filename.length <= 0)
    {
        genericChar str[256];
        sprintf(str, "%s/check_%d", current_path.Value(), check->serial_number);
        check->filename.Set(str);
    }

    OutputDataFile df;
    if (df.Open(check->filename.Value(), CHECK_VERSION))
        return 1;
    else
        return check->Write(df, CHECK_VERSION);
}

int System::DestroyCheck(Check *check)
{
    FnTrace("System::DestroyCheck()");
    Archive *archive = check->archive;
    if (archive)
    {
        if (archive->Remove(check))
            return 1;
    }
    else
    {
        if (Remove(check))
            return 1;
        check->DestroyFile();
    }
    check->customer = NULL;
    delete check;
    return 0;
}

Drawer *System::GetServerBank(Employee *e)
{
    FnTrace("System::GetServerBank()");
    if (e == NULL || e->training)
        return NULL;

    Drawer *drawer = NULL;
    if (DrawerList())
        drawer = DrawerList()->FindByOwner(e, DRAWER_OPEN);
    if (drawer)
        return drawer;  // already have drawer

    drawer = new Drawer(SystemTime);
    drawer->owner_id =  e->id;
    drawer->number   = -e->id;
    Add(drawer);
    drawer->Save();
    return drawer;
}

int System::CreateFixedDrawers()
{
    FnTrace("System::CreateFixedDrawers()");

    // Scan System for drawers that need to be created
    int drawer_no = 1;
    for (Terminal *term = MasterControl->TermList(); term != NULL; term = term->next)
    {
        for (int i = 0; i < term->drawer_count; ++i)
        {
            Drawer *drawer = NULL;
            if (DrawerList())
                drawer = DrawerList()->FindByNumber(drawer_no);
            if (drawer == NULL)
            {
                drawer = new Drawer(SystemTime);
                Add(drawer);
                drawer->number = drawer_no;
            }
            drawer->host     = term->host;
            drawer->position = i;
            drawer->term     = term;
            drawer->Save();
            ++drawer_no;
        }
    }
    return 0;
}

int System::SaveDrawer(Drawer *drawer)
{
    FnTrace("System::SaveDrawer()");
    if (drawer->serial_number <= 0 || drawer->archive)
        return 1;

    if (drawer->filename.length <= 0)
    {
        genericChar str[256];
        sprintf(str, "%s/drawer_%d", current_path.Value(), drawer->serial_number);
        drawer->filename.Set(str);
    }

    OutputDataFile df;
    if (df.Open(drawer->filename.Value(), DRAWER_VERSION))
        return 1;
    else
        return drawer->Write(df, DRAWER_VERSION);
}

int System::CountDrawersOwned(int user_id)
{
    FnTrace("System::CoundDrawersOwned()");
    int count = 0;
    for (Drawer *drawer = DrawerList(); drawer != NULL; drawer = drawer->next)
        if (drawer->owner_id == user_id && drawer->Status() == DRAWER_OPEN)
            ++count;
    return count;
}

int System::AllDrawersPulled()
{
    FnTrace("System::AllDrawersPulled()");
    for (Drawer *drawer = DrawerList(); drawer != NULL; drawer = drawer->next)
        if (drawer->Status() == DRAWER_OPEN && !drawer->IsEmpty())
            return 0; // false
    return 1;     // true
}

int System::AddBatch(long long batchnum)
{
    FnTrace("System::AddBatch()");
    int retval = 0;
    BatchItem *newbatch = NULL;
    BatchItem *currbatch = BatchList.Head();
    int done = 1;

    if (batchnum > 0)
    {
        if (currbatch == NULL)
        {
            newbatch = new BatchItem(batchnum);
            BatchList.AddToHead(newbatch);
        }
        else
        {
            while (!done)
            {
                if (currbatch == NULL)
                {  // add to tail
                    newbatch = new BatchItem(batchnum);
                    BatchList.AddToTail(newbatch);
                    done = 1;
                    retval = 1;
                }
                else if (currbatch->batch == batchnum)
                {
                    done = 1;
                }
                else if (currbatch->batch < batchnum)
                {
                    currbatch = currbatch->next;
                }
                else
                {  // add to head
                    newbatch = new BatchItem(batchnum);
                    BatchList.AddToHead(newbatch);
                    done = 1;
                    retval = 1;
                }
            }
        }
    }

    return retval;
}

