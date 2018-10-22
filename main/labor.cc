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
 * labor.cc - revision 174 (10/13/98)
 * Implementation of labor module
 */

#include "labor.hh"
#include "employee.hh"
#include "report.hh"
#include "data_file.hh"
#include "settings.hh"
#include "system.hh"
#include "archive.hh"

#include <dirent.h>
#include <sys/types.h>
#include <sys/file.h>
#include <cstring>
#include <string>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** WorkEntry Class ****/
// Constructors
WorkEntry::WorkEntry()
{
    next       = NULL;
    fore       = NULL;
    user_id    = 0;
    job        = 0;
    pay_rate   = PERIOD_HOUR;
    pay_amount = 0;
    tips       = 0;
    edit_id    = 0;
    original   = NULL;
    end_shift  = 0;
    overtime   = 0;
}

WorkEntry::WorkEntry(Employee *e, int j)
{
    next       = NULL;
    fore       = NULL;
    user_id    = e->id;
    start      = SystemTime;
    start.Floor<std::chrono::minutes>();
    tips       = 0;
    edit_id    = 0;
    original   = NULL;
    end_shift  = 0;
    job        = j;
    overtime   = 0;

    JobInfo *ji = e->FindJobByType(j);
    if (ji)
    {
        pay_rate   = ji->pay_rate;
        pay_amount = ji->pay_amount;
    }
    else
    {
        pay_rate   = PERIOD_HOUR;
        pay_amount = 0;
    }
}

// Destructor
WorkEntry::~WorkEntry()
{
    WorkEntry *w = original;
    while (w)
    {
        WorkEntry *tmp = w;
        w = w->original;
        delete tmp;
    }
}

// Member Functions
WorkEntry *WorkEntry::Copy()
{
    WorkEntry *w = new WorkEntry;
    if (w == NULL)
        return NULL;

    w->user_id    = user_id;
    w->job        = job;
    w->pay_rate   = pay_rate;
    w->pay_amount = pay_amount;
    w->tips       = tips;
    w->edit_id    = edit_id;
    w->start      = start;
    w->end        = end;
    w->edit_id    = edit_id;
    return w;
}

int WorkEntry::IsWorkDone()
{
    return end.IsSet();
}

int WorkEntry::LaborCost()
{
    FnTrace("WorkEntry::LaborCost()");
    if (pay_rate == PERIOD_HOUR)
        return FltToPrice(PriceToFlt(MinutesWorked() * pay_amount) / 60.0);
    else
        return 0;
}

int WorkEntry::MinutesWorked()
{
    FnTrace("WorkEntry::MinutesWorked()");
    int minute = 0;
    if (end.IsSet())
        minute = MinutesElapsed(end, start);
    else
        minute = MinutesElapsed(SystemTime, start);
    if (minute < 0)
        minute = 0;
    return minute;
}

int WorkEntry::MinutesWorked(TimeInfo &w_e)
{
    FnTrace("WorkEntry::MinutesWorked(TimeInfo)");
    TimeInfo e;
    if (end.IsSet())
        e = end;
    else
        e = SystemTime;
    if (w_e.IsSet() && e > w_e)
        e = w_e;

    int minute = MinutesElapsed(e, start);
    if (minute < 0)
        minute = 0;
    return minute;
}

int WorkEntry::MinutesOvertime(Settings *s, TimeInfo &overtime_end)
{
    FnTrace("WorkEntry::MinutesOvertime()");
    int shift_over = 0;
    int week_over = 0;
    int minute = 0;

    TimeInfo ot_e = overtime_end;
    if (!ot_e.IsSet())
        ot_e = SystemTime;
    if (end.IsSet() && ot_e > end)
        ot_e = end;
    int amount = MinutesWorked(ot_e);

    if (s->overtime_shift > 0)
    {
        int total = amount;
        WorkEntry *work_entry = fore;
        while (work_entry)
        {
            if (work_entry->user_id == user_id)
            {
                if (work_entry->end_shift)
                    break;
                total += work_entry->MinutesWorked();
            }
            work_entry = work_entry->fore;
        }

        minute = s->overtime_shift * 60;
        if (total > minute)
            shift_over = total - minute;
    }

    if (s->overtime_week > 0)
    {
        int total = 0;
        TimeInfo work_start, we;

        s->OvertimeWeek(start, work_start, we);
        if (ot_e > we)
            total = MinutesElapsed(we, start);
        else
            total = MinutesElapsed(ot_e, start);

        WorkEntry *work_entry = fore;
        while (work_entry)
        {
            if (work_entry->user_id == user_id)
            {
                if (work_entry->start >= work_start)
                    total += work_entry->MinutesWorked();
                else if (work_entry->end > work_start)
                    total += MinutesElapsed(work_entry->end, work_start);
                else
                    break; // entry not part of week - end search
            }
            work_entry = work_entry->fore;
        }

        minute = s->overtime_week * 60;
        if (total > minute)
            week_over = total - minute;
    }

    // return hightest of both tests, but never more than entry length
    minute = Max(shift_over, week_over);
    return Min(minute, amount);
}

int WorkEntry::Read(InputDataFile &df, int version)
{
    FnTrace("WorkEntry::Read()");
    // VERSION NOTES
    // 3 (1/31/97) earliest supported version
    // 4 (2/26/97) added pay_rate (assumed hourly before)

    int error = 0;
    error += df.Read(user_id);
    error += df.Read(job);
    if (version >= 4)
        error += df.Read(pay_rate);
    error += df.Read(pay_amount);
    error += df.Read(tips);
    error += df.Read(start);
    error += df.Read(end);
    error += df.Read(end_shift);

    start.Floor<std::chrono::minutes>();
    end.Floor<std::chrono::minutes>();

    error += df.Read(edit_id);
    if (edit_id > 0)
    {
        original = new WorkEntry;
        error += original->Read(df, version);
    }
    return error;
}

int WorkEntry::Write(OutputDataFile &df, int version)
{
    FnTrace("WorkEntry::Write()");
    // Write version 4
    int error = 0;
    error += df.Write(user_id);
    error += df.Write(job);
    error += df.Write(pay_rate);
    error += df.Write(pay_amount);
    error += df.Write(tips);
    error += df.Write(start);
    error += df.Write(end);
    error += df.Write(end_shift);
    error += df.Write(edit_id, 1);
    if (edit_id > 0)
        error += original->Write(df, version);
    return error;
}

int WorkEntry::Edit(int my_user_id)
{
    FnTrace("WorkEntry::Edit()");
    if (original)
    {
        edit_id = my_user_id;
        return 0;
    }

    WorkEntry *work_entry = Copy();
    if (work_entry == NULL)
        return 1;

    original = work_entry;
    edit_id  = my_user_id;
    return 0;
}

int WorkEntry::Update(LaborPeriod *lp)
{
    FnTrace("WorkEntry::Update()");
    WorkEntry *work_entry = original;
    if (work_entry == NULL)
        return 0;

    if (lp->fore && start < lp->fore->end_time)
        start = lp->fore->end_time;
    if (lp->end_time.IsSet())
    {
        if (start > lp->end_time)
            start = lp->end_time;
        if (end.IsSet() && end > lp->end_time)
            end = lp->end_time;
    }
    else
    {
        if (start > SystemTime)
            start = SystemTime;
    }

    if (end.IsSet() && end < start)
        end = start;

    if (start == work_entry->start && end == work_entry->end && job == work_entry->job && tips == work_entry->tips &&
        pay_amount == work_entry->pay_amount && pay_rate == work_entry->pay_rate)
    {
        delete original;
        original = NULL;
        edit_id = 0;
    }
    return 0;
}

int WorkEntry::UndoEdit()
{
    FnTrace("WorkEntry::UndoEdit()");
    WorkEntry *work_entry = original;
    if (work_entry == NULL)
        return 0;

    start      = work_entry->start;
    end        = work_entry->end;
    job        = work_entry->job;
    pay_rate   = work_entry->pay_rate;
    pay_amount = work_entry->pay_amount;
    tips       = work_entry->tips;

    delete original;
    original = NULL;
    edit_id = 0;
    return 0;
}

int WorkEntry::EndEntry(TimeInfo &timevar)
{
    FnTrace("WorkEntry::EndEntry()");
    end = timevar;
    end.Floor<std::chrono::minutes>();

    WorkEntry *work_entry = original;
    while (work_entry)
    {
        work_entry->end = end;
        work_entry = work_entry->original;
    }
    return 0;
}

int WorkEntry::Overlap(TimeInfo &st, TimeInfo &et)
{
    FnTrace("WorkEntry::Overlap()");
    TimeInfo s, e;
    if (start.IsSet() && start > st)
        s = start;
    else
        s = st;

    if (end.IsSet() && end < et)
        e = end;
    else
        e = et;

    if (s >= e)
        return 0;  // no overlap

    return MinutesElapsed(e, s);
}



/**** LaborPeriod ****/
// Constructor
LaborPeriod::LaborPeriod()
{
    next          = NULL;
    fore          = NULL;
    serial_number = 0;
    loaded        = 0;
}

// Member Functions
int LaborPeriod::Add(WorkEntry *work_entry)
{
    FnTrace("LaborPeriod::Add()");
    if (work_entry == NULL)
        return 1;

    // Start at end of list & work backwards
    WorkEntry *ptr = WorkListEnd();
    while (ptr && work_entry->user_id < ptr->user_id)
        ptr = ptr->fore;

    // Insert work_entry after ptr
    return work_list.AddAfterNode(ptr, work_entry);
}

int LaborPeriod::Remove(WorkEntry *work_entry)
{
    FnTrace("LaborPeriod::Remove()");
    return work_list.Remove(work_entry);
}

int LaborPeriod::Purge()
{
    FnTrace("LaborPeriod::Purge()");
    work_list.Purge();
    return 0;
}

int LaborPeriod::Scan(const char* filename)
{
    FnTrace("LaborPeriod::Scan()");
    if (filename == NULL)
        return 1;

    file_name.Set(filename);
    Unload();

    int version = 0;
    InputDataFile df;
    if (df.Open(filename, version))
        return 1;

    char str[256];
    if (version < 3 || version > 4)
    {
        sprintf(str, "Unknown labor file version %d", version);
        ReportError(str);
        return 1;
    }

    int error = 0;
    error += df.Read(serial_number);
    error += df.Read(end_time);
    return error;
}

int LaborPeriod::Load()
{
    FnTrace("LaborPeriod::Load()");
    Unload();
    int version = 0;
    InputDataFile df;
    if (df.Open(file_name.Value(), version))
        return 1;

    char str[256];
    if (version < 2 || version > 4)
    {
        sprintf(str, "Unknown labor file version %d", version);
        ReportError(str);
        return 1;
    }

    int error = 0;
    error += df.Read(serial_number);
    error += df.Read(end_time);

    int n;
    error += df.Read(n);
    for (int i = 0; i < n; ++i)
    {
        if (df.end_of_file)
        {
            ReportError("Unexpected end of LaborPeriod file");
            return 1;
        }

        WorkEntry *work_entry = new WorkEntry;
        error += work_entry->Read(df, version);
        Add(work_entry);
        work_entry->Update(this);
    }

    loaded = 1;
    return error;
}

int LaborPeriod::Unload()
{
    FnTrace("LaborPeriod::Unload()");
    if (loaded == 0)
        return 0;

    loaded = 0;
    Save();
    Purge();
    return 0;
}

#define LABOR_VERSION 4
int LaborPeriod::Save()
{
    FnTrace("LaborPeriod::Save()");
    if (loaded == 0)
        return 1;

    // Save version 4
    OutputDataFile df;
    if (df.Open(file_name.Value(), LABOR_VERSION))
        return 1;

    int error = 0;
    error += df.Write(serial_number);
    error += df.Write(end_time);
    error += df.Write(WorkCount());
    for (WorkEntry *work_entry = WorkList(); work_entry != NULL; work_entry = work_entry->next)
    {
        work_entry->Update(this);
        error += work_entry->Write(df, LABOR_VERSION);
    }
    return error;
}

int LaborPeriod::ShiftReport(Terminal *t, WorkEntry *work_entry, Report *r)
{
    FnTrace("LaborPeriod::ShiftReport()");
    if (work_entry == NULL || r == NULL)
        return 1;

    r->TextC("Work Summary Report");
    r->NewLine(2);

    r->TextPosR(6, "Start:");
    r->TextPosL(7, t->TimeDate(work_entry->start, TD1));
    r->NewLine();

    r->TextPosR(6, "End:");
    r->TextPosL(7, t->TimeDate(work_entry->end, TD1));
    r->NewLine();
    return 0;
}

int LaborPeriod::WorkReport(Terminal *t, Employee *user, TimeInfo &tm_s,
                            TimeInfo &tm_e, Report *r)
{
    FnTrace("LaborPeriod::WorkReport()");
    if (r == NULL)
        return 1;

    Settings *s = t->GetSettings();
    TimeInfo start;
    TimeInfo end;
    TimeInfo now;
    start = tm_s;
    start.Floor<std::chrono::minutes>();
    end = tm_e;
    end.Floor<std::chrono::minutes>();
    now = SystemTime;
    now.Floor<std::chrono::minutes>();

    r->min_width = 60;
    char str[256];
    int total_work = 0;
    int total_tips = 0;
    int total_wages = 0;
    int last_id = -1;
    int empty = 1;
    WorkEntry *work_entry = WorkList();
    while (work_entry)
    {
        work_entry->Update(this);
        TimeInfo ts, te;
        ts = work_entry->start;
        te = work_entry->end;
        int wid = work_entry->user_id;
        if (!te.IsSet())
        {
            te = now;
            te.Floor<std::chrono::minutes>();
        }
        if ((user == NULL || user->id == wid) && ts <= end && te >= start &&
            ((1 << work_entry->job) & t->job_filter) == 0)
        {
            if (!work_entry->end.IsSet())
                r->update_flag |= UPDATE_MINUTE;

            empty = 0;
            if (wid != last_id)
                r->TextPosL(0, t->UserName(wid));

            if (ts < start)
                ts = start;
            WorkEntry *w2 = work_entry->original;

            int c = COLOR_DEFAULT;
            if (w2 && w2->start != work_entry->start)
                c = COLOR_DK_BLUE;
            r->TextPosR(-38, t->TimeDate(ts, TD_DATEPAD), c);
            r->TextPosR(-31, t->TimeDate(ts, TD_TIMEPAD), c);

            if (te > end)
                te = end;

            c = COLOR_DEFAULT;
            if (w2 && w2->end != work_entry->end)
                c = COLOR_DK_BLUE;
            if (work_entry->end.IsSet() || now > end)
                r->TextPosR(-24, t->TimeDate(te, TD_TIMEPAD), c);
            else
                r->TextPosR(-24, "--:-- ", c);

            int work = MinutesElapsed(te, ts);
            if (work < 0)
                work = 0;

            total_work += work;
            sprintf(str, "%d:%02d", work / 60, work % 60);
            r->TextPosR(-16, str);

            c = COLOR_DEFAULT;
            if (w2 && (work_entry->pay_rate != w2->pay_rate ||
                       work_entry->pay_amount != w2->pay_amount))
                c = COLOR_DK_BLUE;
            int wage = 0;
            if (work_entry->pay_rate == PERIOD_HOUR)
                wage = FltToPrice(PriceToFlt(work * work_entry->pay_amount) / 60.0);
            total_wages += wage;
            r->TextPosR(-7, t->FormatPrice(wage, 1), c);

            c = COLOR_DEFAULT;
            if (w2 && work_entry->tips != w2->tips)
                c = COLOR_DK_BLUE;
            total_tips += work_entry->tips;
            r->TextR(t->FormatPrice(work_entry->tips, 1), c);
            if (work_entry->end_shift)
                r->UnderlinePosR(0, 46);
            r->NewLine();
            last_id = wid;

            int ot = work_entry->MinutesOvertime(s, work_entry->end);
            if (ot > work)
                ot = work;
            work_entry->overtime = ot;

            if (ot > 0)
            {
                r->Mode(PRINT_BOLD);
                r->TextPosR(-25, "Overtime", COLOR_DK_RED);
                r->Mode(0);
                sprintf(str, "%d:%02d", ot / 60, ot % 60);
                r->TextPosR(-16, str, COLOR_DK_RED);
                int ot_wage = FltToPrice(PriceToFlt(ot * work_entry->pay_amount) / 120.0);
                r->TextPosR(-7, t->FormatPrice(ot_wage, 1), COLOR_DK_RED);
                r->NewLine();
                total_wages += ot_wage;
            }
        }

        work_entry = work_entry->next;
        if (last_id == wid && (work_entry == NULL || work_entry->user_id != wid))
        {
            r->Mode(PRINT_BOLD);
//            r->TextPosR(-25, "Total", COLOR_DK_GREEN);
            r->TextPosR(-35, "Total", COLOR_DK_GREEN);
            r->Mode(0);
            sprintf(str, "%.2f", (Flt) total_work / 60.0);
//            r->TextPosR(-16, str, COLOR_DK_GREEN);
            r->TextPosR(-26, str, COLOR_DK_GREEN);
            sprintf(str, "(%d:%02d)", total_work / 60, total_work % 60);
            r->TextPosR(-16, str, COLOR_DK_RED);
            r->TextPosR(-7, t->FormatPrice(total_wages, 1), COLOR_DK_GREEN);
            r->TextR(t->FormatPrice(total_tips, 1), COLOR_DK_GREEN);
            r->NewLine(2);
            total_tips  = 0;
            total_work  = 0;
            total_wages = 0;
        }
    }

    if (empty && user)
    {
        r->TextPosL(0, user->system_name.Value());
        r->Mode(PRINT_BOLD);
        r->TextPosL(-30, "No hours for this period");
    }
    return 0;
}

WorkEntry *LaborPeriod::WorkReportEntry(Terminal *t, int line, Employee *user,
                                        TimeInfo &tm_s, TimeInfo &tm_e)
{
    FnTrace("LaborPeriod::WorkReportEntry()");
    TimeInfo start, end;
    start = tm_s;
    start.Floor<std::chrono::minutes>();
    end   = tm_e;
    end.Floor<std::chrono::minutes>();

    int last_id = -1, l = 0;
    WorkEntry *work_entry = WorkList();
    while (work_entry)
    {
        int wid = work_entry->user_id;
        TimeInfo te = work_entry->end;
        if (!te.IsSet())
            te = SystemTime;

        if ((user == NULL || user->id == wid) && work_entry->start <= end && te >= start &&
            ((1 << work_entry->job) & t->job_filter) == 0)
        {
            if (line == l)
                return work_entry;
            else if (l > line)
                return NULL;
            ++l;
            last_id = wid;

            if (work_entry->overtime > 0)
                ++l;
        }

        work_entry = work_entry->next;
        if (last_id == wid && (work_entry == NULL || work_entry->user_id != wid))
            l+= 2;
    }
    return NULL;
}

int LaborPeriod::WorkReportLine(Terminal *t, WorkEntry *work, Employee *user,
                                TimeInfo &tm_s, TimeInfo &tm_e)
{
    FnTrace("LaborPeriod::WorkReportLine()");
    TimeInfo start, end;
    start = tm_s;
    start.Floor<std::chrono::minutes>();
    end   = tm_e;
    end.Floor<std::chrono::minutes>();

    int last_id = -1, l = 0;
    WorkEntry *work_entry = WorkList();
    while (work_entry)
    {
        int wid = work_entry->user_id;
        TimeInfo te = work_entry->end;
        if (!te.IsSet())
            te = SystemTime;

        if ((user == NULL || user->id == wid) && work_entry->start <= end && te >= start &&
            ((1 << work_entry->job) & t->job_filter) == 0)
        {
            if (work == work_entry)
                return l;
            ++l;
            last_id = wid;

            if (work_entry->overtime > 0)
                ++l;
        }
        work_entry = work_entry->next;
        if (last_id == wid && (work_entry == NULL || work_entry->user_id != wid))
            l+= 2;
    }
    return -1;
}

/**** LaborDB ****/
// Constructor
LaborDB::LaborDB()
{
    last_serial = 0;
}

// Member Functions
int LaborDB::Load(const char* path)
{
    FnTrace("LaborDB::Load()");
    if (path)
    {
        pathname.Set(path);
    }

    DIR *dp = opendir(pathname.Value());
    if (dp == NULL)
        return 1;

    char str[256];
    struct dirent *record = NULL;
    do
	{
		record = readdir(dp);
		if (record)
		{
			const char* name = record->d_name;
            int len = strlen(name);
            if (strcmp(&name[len-4], ".fmt") == 0)
                continue;
			if (strncmp(name, "labor_", 6) == 0)
			{
				sprintf(str, "%s/%s", pathname.Value(), name);
				LaborPeriod *lp = new LaborPeriod;
				if (lp->Scan(str))
				{
					ReportError("Couldn't load labor period");
					delete lp;
				}
				else
					Add(lp);
			}
		}
	}while (record);

    closedir(dp);
    return 0;
}

int LaborDB::Add(LaborPeriod *lp)
{
    FnTrace("LaborDB::Add()");
    if (lp == NULL)
        return 1;  // Add failed

    // start at end of list and work backwords
    LaborPeriod *ptr = PeriodListEnd();
    if (lp->end_time.IsSet())
        while (ptr && lp->end_time < ptr->end_time)
            ptr = ptr->fore;

    if (lp->serial_number <= 0)
        lp->serial_number = ++last_serial;
    else if (lp->serial_number > last_serial)
        last_serial = lp->serial_number;

    // Insert 'lp' after 'ptr'
    return period_list.AddAfterNode(ptr, lp);
}

int LaborDB::Remove(LaborPeriod *lp)
{
    FnTrace("LaborDB::Remove()");
    return period_list.Remove(lp);
}

int LaborDB::Purge()
{
    FnTrace("LaborDB::Purge()");
    period_list.Purge();
    return 0;
}

int LaborDB::NewLaborPeriod()
{
    FnTrace("LaborDB::NewLaborPerion()");
    LaborPeriod *lp = new LaborPeriod;
    if (lp == NULL)
        return 1;

    LaborPeriod *end = PeriodListEnd();
    if (end)
    {
        WorkEntry *work_entry = end->WorkList();
        while (work_entry)
        {
            if (!work_entry->IsWorkDone())
            {
                work_entry->EndEntry(SystemTime);
                WorkEntry *nw = new WorkEntry;
                nw->user_id    = work_entry->user_id;
                nw->start      = SystemTime;
                nw->job        = work_entry->job;
                nw->pay_rate   = work_entry->pay_rate;
                nw->pay_amount = work_entry->pay_amount;
                lp->Add(nw);
            }
            work_entry = work_entry->next;
        }

        end->end_time = SystemTime;
        end->Save();
    }

    Add(lp);
    char str[256];
    sprintf(str, "%s/labor_%09d", pathname.Value(), lp->serial_number);
    lp->file_name.Set(str);
    lp->loaded = 1;
    lp->Save();
    return 0;
}

LaborPeriod *LaborDB::CurrentPeriod()
{
    FnTrace("LaborDB::CurrentPeriod()");
    LaborPeriod *lp = NULL;
    if (PeriodListEnd() == NULL)
        NewLaborPeriod();

    lp = PeriodListEnd();
    if (lp == NULL)
        return NULL;

    if (lp->loaded == 0)
        lp->Load();
    return lp;
}

WorkEntry *LaborDB::CurrentWorkEntry(Employee *e)
{
    FnTrace("LaborDB::CurrentWorkEntry()");
    if (e == NULL)
        return NULL;

    LaborPeriod *lp = CurrentPeriod();
    if (lp == NULL)
        return NULL;

    WorkEntry *work_entry = lp->WorkListEnd();
    while (work_entry)
    {
        if (work_entry->user_id == e->id && !work_entry->end.IsSet())
            return work_entry;
        work_entry = work_entry->fore;
    }
    return NULL;
}

int LaborDB::IsUserOnClock(Employee *e)
{
    FnTrace("LaborDB::IsUserOnClock()");
    if (e == NULL)
        return 0;
    if (e->UseClock() == 0)
        return 1;

    WorkEntry *work_entry = CurrentWorkEntry(e);
    return (work_entry != NULL);
}

int LaborDB::IsUserOnBreak(Employee *e)
{
    FnTrace("LaborDB::IsUserOnBreak()");
    if (e == NULL)
        return 0;
    if (e->UseClock() == 0)
        return 0;
    if (IsUserOnClock(e) == 1)
        return 0;

    LaborPeriod *lp = PeriodListEnd();
    while (lp)
    {
        WorkEntry *work_entry = lp->WorkListEnd();
        while (work_entry)
        {
            if (work_entry->user_id == e->id)
            {
            	if (work_entry->end.IsSet() && work_entry->end_shift)
                    return 0;
                else
                    return 1;
            }
            work_entry = work_entry->fore;
        }
        lp = lp->fore;
    }
    return 0;
}

int LaborDB::CurrentJob(Employee *e)
{
    FnTrace("LaborDB::CurrentJob()");
    if (e == NULL)
        return 0;

    if (e->id == 1)
        return JOB_SUPERUSER;
    else if (e->id == 2)
        return JOB_DEVELOPER;

    WorkEntry *we = CurrentWorkEntry(e);
    if (we)
        return we->job;
    else
        return 0;
}

WorkEntry *LaborDB::NewWorkEntry(Employee *e, int job)
{
    FnTrace("LaborDB::NewWorkEntry()");
    if (e == NULL || IsUserOnClock(e) || !e->UseClock())
        return NULL;

    LaborPeriod *lp = CurrentPeriod();
    if (lp == NULL)
        return NULL;

    WorkEntry *work_entry = new WorkEntry(e, job);
    lp->Add(work_entry);
    lp->Save();
    return work_entry;
}

int LaborDB::EndWorkEntry(Employee *e, int end_shift)
{
    FnTrace("LaborDB::EndWorkEntry()");
    if (e == NULL || e->UseClock() == 0)
        return 1;

    WorkEntry *work_entry = CurrentWorkEntry(e);
    if (work_entry == NULL)
        return 1;

    LaborPeriod *lp = CurrentPeriod();
    if (lp == NULL)
        return 1;

    work_entry->EndEntry(SystemTime);
    work_entry->end_shift = end_shift;
    lp->Save();
    return 0;
}

int LaborDB::ServerLaborReport(Terminal *t, Employee *e, TimeInfo &start,
                               TimeInfo &end, Report *r)
{
    FnTrace("LaborDB::ServerLaborReport()");
    if (e == NULL || r == NULL)
        return 1;

    TimeInfo ps;
    TimeInfo pe;
    char str[256];
    int total_work = 0;
    int total_wages = 0;
    int total_tips = 0;

//    r->TextC("Hour/Wage Report", COLOR_DK_BLUE);           Let the Button's Name Field provide the Title for this report
//    r->NewLine();

    char tm1[32];
    char tm2[32];
    if (start.IsSet())
        t->TimeDate(tm1, start, TD5);
    else
        strcpy(tm1, t->Translate("System Start"));
    if (end.IsSet())
        t->TimeDate(tm2, end, TD5);
    else
        strcpy(tm2, "Now");
    sprintf(str, "(%s to %s)", tm1, tm2);
    r->TextC(str, COLOR_DK_BLUE);
    r->NewLine(2);

    r->Mode(PRINT_UNDERLINE);
    r->TextPosL(1,  "Date", COLOR_DK_BLUE);
    r->TextPosL(10, "Start", COLOR_DK_BLUE);
    r->TextPosL(17, "End", COLOR_DK_BLUE);
    r->TextPosL(24, "Elapsed", COLOR_DK_BLUE);
    r->TextPosL(32, "Wages", COLOR_DK_BLUE);
    r->TextPosL(40, "Tips", COLOR_DK_BLUE);
    r->Mode(0);
    r->NewLine();

    LaborPeriod *p = PeriodList();
    while (p)
    {
        pe = p->end_time;
        if (!pe.IsSet())
            pe = SystemTime;

        if (start <= pe && end >= ps)
        {
            WorkEntry *work_entry = p->WorkList();
            while (work_entry)
            {
                if (work_entry->user_id == e->id &&
                    work_entry->start >= start &&
                    ((work_entry->end.IsSet() && work_entry->end < end) ||
                     SystemTime < end))
                {
                    TimeInfo work_start = work_entry->start;
                    TimeInfo we = work_entry->end;

                    if (work_start < start)
                        work_start = start;
                    if (we > end)
                        we = end;

                    r->TextPosL(0,  t->TimeDate(work_start, TD_DATEPAD));
                    r->TextPosL(9,  t->TimeDate(work_start, TD_TIMEPAD));
                    if (we.IsSet())
                        r->TextPosL(16, t->TimeDate(we, TD_TIMEPAD));
                    else
                        r->TextPosL(16, "--:--");

                    int work = MinutesElapsed(we, work_start);
                    if (work < 0)
                        work = 0;

                    total_work += work;
                    sprintf(str, "%d:%02d", work / 60, work % 60);
                    r->TextPosL(24, str);

                    int wage = 0;
                    if (work_entry->pay_rate == PERIOD_HOUR)
                        wage = FltToPrice(PriceToFlt(work * work_entry->pay_amount) / 60.0);
                    total_wages += wage;
                    r->TextPosR(38, t->FormatPrice(wage));

                    total_tips += work_entry->tips;
                    r->TextPosR(45, t->FormatPrice(work_entry->tips));
                    r->NewLine();
                }
                work_entry = work_entry->next;
            }
        }
        ps = pe;
        p = p->next;
    }

    // Print Totals
    r->NewLine();
    r->Mode(PRINT_BOLD);
    r->TextPosL(8, "Total");
    sprintf(str, "%d:%02d", total_work / 60, total_work % 60);
    r->TextPosL(24, str);
    r->TextPosR(38, t->FormatPrice(total_wages));
    r->TextPosR(45, t->FormatPrice(total_tips));
    return 0;
}

WorkEntry *LaborDB::StartOfShift(Employee *e)
{
    FnTrace("LaborDB::StartOfShift()");
    if (e == NULL)
        return NULL;

    LaborPeriod *lp = PeriodListEnd();
    WorkEntry *first = NULL;
    while (lp)
    {
        WorkEntry *work_entry = lp->WorkListEnd();
        while (work_entry)
        {
            if (work_entry->user_id == e->id && work_entry->end.IsSet())
            {
                if (first && work_entry->end_shift)
                    return first;
                first = work_entry;
            }
            work_entry = work_entry->fore;
        }
        lp = lp->fore;
    }
    return first;
}

WorkEntry *LaborDB::NextEntry(WorkEntry *work_entry)
{
    FnTrace("LaborDB::NextEntry()");
    if (work_entry == NULL)
        return NULL;

    int user_id = work_entry->user_id;
    work_entry = work_entry->next;
    while (work_entry)
    {
        if (work_entry->user_id == user_id)
            return work_entry;
        work_entry = work_entry->next;
    }
    return NULL;
}

#define WORKRECEIPT_TITLE  "Attendance Receipt"
int LaborDB::WorkReceipt(Terminal *t, Employee *e, Report *r)
{
    FnTrace("LaborDB::WorkReceipt()");
    if (r == NULL)
        return 1;

    //r->max_width = 40;
    WorkEntry *we = StartOfShift(e);
    if (we == NULL)
    {
        r->TextC("No work entries found");
        return 0;
    }

    Settings *settings = t->GetSettings();
    Str *header = &settings->receipt_header[0];
    char buffer[STRLONG];
    char buff2[STRLONG];
    int  buffidx = 0;
    int  idx = 0;

    r->Mode(PRINT_LARGE | PRINT_NARROW);
    if (header->size() > 0)
    {
        // ugly hack: get rid of any initial spaces so we can center it properly
        strcpy(buff2, header->Value());
        while (buff2[idx] == ' ' || buff2[idx] == '\t')
            idx += 1;
        while (buff2[idx] != ' ' && buff2[idx] != '\n' && buff2[idx] != '\t')
        {
            buffer[buffidx] = buff2[idx];
            buffidx += 1;
            idx += 1;
        }
        r->TextC(buffer);
        r->NewLine(2);
    }
    r->SetTitle(WORKRECEIPT_TITLE);
    char str[256];
    r->TextC(t->Translate(WORKRECEIPT_TITLE), COLOR_DK_BLUE);
    r->NewLine();
    r->Mode(0);

    sprintf(str, "      User: %s", e->system_name.Value());
    r->TextL(str);
    r->NewLine();

    TimeInfo ts = we->start, te, bs;
    int minute = 0;
    int tips = 0;
    int break_min = 0;
    while (we)
    {
        if (bs.IsSet())
        {
            sprintf(str, " Off Break: %s", t->TimeDate(we->start, TD2));
            break_min += MinutesElapsed(we->start, bs);
        }
        else
            sprintf(str, "   Time On: %s", t->TimeDate(we->start, TD2));

        r->TextL(str);
        r->NewLine();

        if (!we->end.IsSet())
            r->TextL("  Time Off: (still on clock)");
        else
        {
            if (we->end_shift)
                sprintf(str, "  Time Off: %s", t->TimeDate(we->end, TD2));
            else
            {
                sprintf(str, "  On Break: %s", t->TimeDate(we->end, TD2));
                bs = we->end;
            }
            r->TextL(str);
            r->NewLine();
        }
        te = we->end;

        minute  += we->MinutesWorked();
        tips += we->tips;

        if (we->end_shift)
            break;
        we = NextEntry(we);
    }

    r->NewLine();
    sprintf(str, "  Time Worked: %.2f hours", (Flt) minute / 60);
    r->TextL(str);
    if (break_min > 0)
    {
        r->NewLine();
        sprintf(str, "Time on Break: %.2f hours", (Flt) break_min / 80);
        r->TextL(str);
    }

    r->NewLine(2);
    sprintf(str, "%s: %s", t->Translate("Tips Declared"), t->FormatPrice(tips));
    r->TextL(str);
    r->NewLine();

    t->system_data->ServerReport(t, ts, te, e, r);
    return 0;
}

int LaborDB::FigureLabor(Settings *s, TimeInfo &start, TimeInfo &end_time,
                         int job, int &mins, int &cost, int &otmins, int &otcost)
{
    FnTrace("LaborDB::FigureLabor()");
    mins   = 0;
    cost   = 0;
    otmins = 0;
    otcost = 0;

    TimeInfo end;
    if (!end_time.IsSet() || end_time > SystemTime)
        end = SystemTime; // end time can't be ahead of actual time
    else
        end = end_time;

    LaborPeriod *lp = PeriodList();
    while (lp)
    {
        WorkEntry *work_entry = lp->WorkList();
        while (work_entry)
        {
            if ((job <= 0 || work_entry->job == job) && work_entry->pay_rate == PERIOD_HOUR)
            {
                int m = work_entry->Overlap(start, end);
                if (m > 0)
                {
                    int ot = work_entry->MinutesOvertime(s, end);
                    if (ot > m)
                        ot = m;
                    m -= ot;
                    mins += m;
                    cost += (m * work_entry->pay_amount) / 60;
                    otmins += ot;
                    otcost += (ot * work_entry->pay_amount) / 40;
                }
            }
            work_entry = work_entry->next;
        }
        lp = lp->next;
    }
    return 0;
}

/**** WorkDB Class ****/
// Constructor
WorkDB::WorkDB()
{
    archive = NULL;
}

// Member Functions
int WorkDB::Add(WorkEntry *we)
{
    FnTrace("WorkDB::Add()");
    if (we == NULL)
        return 1;

    // Start at end of list & work backwards
    WorkEntry *ptr = WorkListEnd();
    while (ptr && we->user_id < ptr->user_id)
        ptr = ptr->fore;

    // Insert we after ptr
    return work_list.AddAfterNode(ptr, we);
}

int WorkDB::Remove(WorkEntry *we)
{
    FnTrace("WorkDB::Remove()");
    return work_list.Remove(we);
}

int WorkDB::Purge()
{
    FnTrace("WorkDB::Purge()");
    work_list.Purge();
    return 0;
}

int WorkDB::Load(const char* file)
{
    FnTrace("WorkDB::Load()");
    if (file)
        filename.Set(file);

    int version = 0;
    InputDataFile df;
    if (df.Open(filename.Value(), version))
        return 1;
    else
        return Read(df, version);
}

int WorkDB::Save()
{
    FnTrace("WorkDB::Save()");
    if (archive)
    {
        archive->changed = 1;
        return 0;
    }

    OutputDataFile df;
    if (df.Open(filename.Value(), WORK_VERSION))
        return 1;

    int error = Write(df, WORK_VERSION);
    return error;
}

int WorkDB::Read(InputDataFile &df, int version)
{
    FnTrace("WorkDB::Read()");
    int count = 0;
    df.Read(count);
    for (int i = 0; i < count; ++i)
    {
        WorkEntry *we = new WorkEntry;
        we->Read(df, version);
        Add(we);
    }
    return 0;
}

int WorkDB::Write(OutputDataFile &df, int version)
{
    FnTrace("WorkDB::Write()");
    df.Write(WorkCount());
    for (WorkEntry *we = WorkList(); we != NULL; we = we->next)
        we->Write(df, version);
    return 0;
}
