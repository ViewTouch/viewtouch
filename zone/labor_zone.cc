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
 * labor_zone.cc - revision 54 (8/10/98)
 * Implementation of labor_zone module
 */

#include "labor_zone.hh"
#include "terminal.hh"
#include "report.hh"
#include "employee.hh"
#include "labor.hh"
#include "system.hh"
#include "settings.hh"
#include "dialog_zone.hh"
#include "labels.hh"
#include "archive.hh"
#include "manager.hh"
#include <string.h>
extern int AdjustPeriod(TimeInfo &ref, int period, int adjust);

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** LaborZone Class ****/
// Constructor
LaborZone::LaborZone()
{
    report   = NULL;
    period   = NULL;
    work     = NULL;
    page     = 0;
    day_view = 1;
    spacing  = 1.0;

    no_line      = 1;
    form_header  = -3;
    form_spacing = .65;
    AddTimeDateField("Start", 1, 0);
    AddTimeDateField("End", 1, 1);
    Color(COLOR_DK_GREEN);
    AddButtonField("Clock Out", "clockout");
    AddButtonField("Start Break", "break");
    Color(COLOR_DEFAULT);
    AddNewLine();
    AddListField("Job", NULL);
    AddTextField("Pay", 7);
    AddListField("Rate", PayRateName, PayRateValue);
    AddTextField("Tips", 7);
}

// Destructor
LaborZone::~LaborZone()
{
    if (report)
        delete report;
}

// Member Functions
RenderResult LaborZone::Render(Terminal *term, int update_flag)
{
	//printf("LaborZone::Render  update_flag=%d; period=%d RENDER_NEW=%d\n",update_flag, period, RENDER_NEW);
    FnTrace("LaborZone::Render()");
    System *sys = term->system_data;
    if (update_flag || period == NULL)
    {
        if (report)
        {
            delete report;
            report = NULL;
        }

        if (update_flag == RENDER_NEW || period == NULL)
        {
            ref    = SystemTime;
            work   = NULL;
            page   = 0;
            period = sys->labor_db.CurrentPeriod();
            day_view = 1;
        }
    }

    if (day_view)
    {
        Archive *archive = term->archive;
        if (archive)
        {
            start = archive->start_time;
            end   = archive->end_time;
        }
        else
        {
            if (sys->ArchiveListEnd())
                start = sys->ArchiveListEnd()->end_time;
            else
                start.Clear();
            end = SystemTime;
        }
    }
    else {
        sys->settings.LaborPeriod(ref, start, end);
       	//printf("Render after settings:LaborPeriod: ref=%d/%d/%d : start=%d/%d/%d: end=%d/%d/%d : System=%d/%d/%d\n", ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year(), SystemTime.Month(), SystemTime.Day(), SystemTime.Year());
    }

    if (report == NULL && period)
    {
        report = new Report;
        report->SetTitle("Time Clock Summary");
        period->WorkReport(term, term->server, start, end, report);
    }
    FormZone::Render(term, update_flag);

    genericChar str[256], str2[256];
    int c = color[0];
    if (day_view)
        strcpy(str2, term->Translate("Business Day Time Clock View"));
    else
        strcpy(str2, term->Translate("Labor Period Time Clock View"));

    if (term->server)
        sprintf(str, "%s for %s", str2, term->server->system_name.Value());
    else
        sprintf(str, "%s for Everyone", str2);
    TextC(term, 0, str, c);

    genericChar tm1[32];
    if (start.IsSet())
        term->TimeDate(tm1, start, TD_SHORT_DAY | TD_SHORT_DATE | TD_SHORT_TIME);
    else
        strcpy(tm1, term->Translate("System Start"));
    sprintf(str, "%s  to  %s", tm1,
            term->TimeDate(end, TD_SHORT_DAY | TD_SHORT_DATE | TD_SHORT_TIME));
    TextC(term, 1, str, c);

    TextPosL(term, 0,         2.3, "Name", c);
    TextPosL(term, size_x-44, 2.3, "Date", c);
    TextPosL(term, size_x-36, 2.3, "Start", c);
    TextPosL(term, size_x-29, 2.3, "End", c);
    TextPosL(term, size_x-22, 2.3, "Elapsed", c);
    TextPosL(term, size_x-13, 2.3, "Wages", c);
    TextPosL(term, size_x-5,  2.3, "Tips", c);

    if (report)
    {
        report->selected_line =
            period->WorkReportLine(term, work, term->server, start, end);
        report->Render(term, this, 3, 5, page, 0, spacing);
    }
    //printf("LaborZone::Render(): ref=%d/%d/%d\nstart=%d/%d/%d\nend=%d/%d/%d\n",
    //ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year());
    return RENDER_OKAY;
}

SignalResult LaborZone::Signal(Terminal *term, const genericChar *message)
{
    FnTrace("LaborZone::Signal()");
    static const genericChar *commands[] = {
        "clockout", "break", "undo edit", "change view",
        "next server", "prior server", "next", "prior",
        "change period", "day", "period",
        "print", "localprint", "reportprint", NULL};

    Employee *employee = term->user;
    if (employee == NULL || period == NULL)
        return SIGNAL_IGNORED;

    System *sys = term->system_data;
    Settings *s = &(sys->settings);    int idx = CompareList(message, commands);
    switch (idx)
    {
    case 0:  // clockout
        if (work)
        {
            SaveRecord(term, 0, 0);
            work->Edit(employee->id);
            work->end = SystemTime;
            work->end.Sec(0);
            work->end_shift = 1;
            LoadRecord(term, 0);
            Draw(term, 1);
            return SIGNAL_OKAY;
        }
        break;
    case 1:  // break;
        if (work)
        {
            SaveRecord(term, 0, 0);
            work->Edit(employee->id);
            work->end = SystemTime;
            work->end.Sec(0);
            work->end_shift = 0;
            LoadRecord(term, 0);
            Draw(term, 1);
            return SIGNAL_OKAY;
        }
        break;
    case 2:  // undo edit
        if (work)
        {
            work->UndoEdit();
            LoadRecord(term, 0);
            Draw(term, 1);
            return SIGNAL_OKAY;
        }
        break;
    case 3:  // change view
        if (term->server)
            term->server = NULL;
        else if (work)
            term->server = sys->user_db.FindByID(work->user_id);
        else
            term->server = sys->user_db.UserList();
        term->Update(UPDATE_SERVER, NULL);
        return SIGNAL_OKAY;
    case 4:  // next server
        if (term->server)
        {
            if (work)
            {
                work = NULL;
                LoadRecord(term, 0);
            }
            term->server = sys->user_db.NextUser(term, term->server);
        }
        else if (work)
            term->server = sys->user_db.FindByID(work->user_id);
        else
            term->server = sys->user_db.UserList();
        term->Update(UPDATE_SERVER, NULL);
        return SIGNAL_OKAY;
    case 5:  // prior server
        if (term->server)
        {
            if (work)
            {
                work = NULL;
                LoadRecord(term, 0);
            }
            term->server = sys->user_db.ForeUser(term, term->server);
        }
        else if (work)
            term->server = sys->user_db.FindByID(work->user_id);
        else
            term->server = sys->user_db.UserList();
        term->Update(UPDATE_SERVER, NULL);
        return SIGNAL_OKAY;
    case 6:  // next
      	//printf("\n\nlabor_zone -> next(): ref=%d/%d/%d : start=%d/%d/%d: end=%d/%d/%d : System=%d/%d/%d\n", ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year(), SystemTime.Month(), SystemTime.Day(), SystemTime.Year());
        if (day_view)
        {
            if (term->archive == NULL)
                return SIGNAL_IGNORED;
            term->archive = term->archive->next;
            term->Update(UPDATE_ARCHIVE, NULL);
        } else {
        	ref = end;
         	//printf("labor_zone->next prior to AdjustPeriod ref=%d/%d/%d : start=%d/%d/%d: end=%d/%d/%d : System=%d/%d/%d\n", ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year(), SystemTime.Month(), SystemTime.Day(), SystemTime.Year());
            AdjustPeriod(ref, s->labor_period, 1);
         	//printf("AfterAdjustPeriod: ref=%d/%d/%d : start=%d/%d/%d: end=%d/%d/%d : System=%d/%d/%d\n", ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year(), SystemTime.Month(), SystemTime.Day(), SystemTime.Year());
            s->SetPeriod(ref, start, end, s->labor_period, 0);
         	//printf("AfterSetPeriod: ref=%d/%d/%d : start=%d/%d/%d: end=%d/%d/%d : System=%d/%d/%d\n", ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year(), SystemTime.Month(), SystemTime.Day(), SystemTime.Year());
            Draw(term, 1);
        }
        return SIGNAL_OKAY;
        break;
    case 7:  // prior
      	//printf("\n\nlabor_zone -> prior(): ref=%d/%d/%d : start=%d/%d/%d: end=%d/%d/%d : System=%d/%d/%d\n", ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year(), SystemTime.Month(), SystemTime.Day(), SystemTime.Year());
        if (day_view)
        {
            if (term->archive == NULL)
                term->archive = sys->ArchiveListEnd();
            else if (term->archive->fore)
                term->archive = term->archive->fore;
            else
                return SIGNAL_IGNORED;
            term->Update(UPDATE_ARCHIVE, NULL);
        } else {
        	ref = start;
        	//printf("labor_zone->prior before Adjust ref=%d/%d/%d : start=%d/%d/%d: end=%d/%d/%d : System=%d/%d/%d\n", ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year(), SystemTime.Month(), SystemTime.Day(), SystemTime.Year());
            AdjustPeriod(ref, s->labor_period, -1);
        	//printf("labor_zone->prior after Adjust ref=%d/%d/%d : start=%d/%d/%d: end=%d/%d/%d : System=%d/%d/%d\n", ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year(), SystemTime.Month(), SystemTime.Day(), SystemTime.Year());
            s->SetPeriod(ref, start, end, s->labor_period, 0);
        	//printf("labor_zone->prior after SetPeriod ref=%d/%d/%d : start=%d/%d/%d: end=%d/%d/%d : System=%d/%d/%d\n", ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year(), SystemTime.Month(), SystemTime.Day(), SystemTime.Year());
            Draw(term, 1);
        	//printf("Drawing Term ref=%d/%d/%d : start=%d/%d/%d: end=%d/%d/%d : System=%d/%d/%d\n", ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year(), SystemTime.Month(), SystemTime.Day(), SystemTime.Year());
        }
        return SIGNAL_OKAY;
        break;
    case 8:  // change period
        //printf("labor_zone -> change period()\n");
        // need to implement this still
        return SIGNAL_OKAY;
        break;
    case 9: // day
        //printf("labor_zone -> day()\n");
//        	printf("ref=%d/%d/%d : end=%d/%d/%d : System=%d/%d/%d\n", ref.Month(), ref.Day(), ref.Year(), end.Month(), end.Day(), end.Year(), SystemTime.Month(), SystemTime.Day(), SystemTime.Year());
        ref = SystemTime;
        day_view = 1 - day_view;
        Draw(term, 1);
        return SIGNAL_OKAY;
        break;
    case 10: // period
        //printf("labor_zone -> period()\n");
        	//printf("ref=%d/%d/%d : start=%d/%d/%d: end=%d/%d/%d : System=%d/%d/%d\n", ref.Month(), ref.Day(), ref.Year(), start.Month(), start.Day(), start.Year(), end.Month(), end.Day(), end.Year(), SystemTime.Month(), SystemTime.Day(), SystemTime.Year());
        ref = SystemTime;
        day_view = 1 - day_view;
        Draw(term, 1);
        return SIGNAL_OKAY;
        break;
    case 11: // print
        Print(term, RP_ASK);
        break;
    case 12: // localprint
        Print(term, RP_PRINT_LOCAL);
        break;
    case 13: // reportprint
        Print(term, RP_PRINT_REPORT);
        break;
    default:
        return FormZone::Signal(term, message);
    }
    return SIGNAL_IGNORED;
}

SignalResult LaborZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("LaborZone::Touch()");
    if (period == NULL || report == NULL)
        return SIGNAL_IGNORED;

    LayoutZone::Touch(term, tx, ty);
    int yy = report->TouchLine(spacing, selected_y);

    int max_page = report->max_pages;
    int new_page = page;
    if (yy == -1)
    {
        --new_page;
        if (new_page < 0)
            new_page = max_page - 1;
    }
    else if (yy == -2)
    {
        if (selected_y > (size_y - 2.0))
            return FormZone::Touch(term, tx, ty);

        ++new_page;
        if (new_page >= max_page)
            new_page = 0;
    }
    else
    {
        WorkEntry *work_entry = period->WorkReportEntry(term, yy, term->server, start, end);
        if (work != work_entry)
        {
            SaveRecord(term, 0, 0);
            work = work_entry;
            keyboard_focus = FieldList();
        }

        LoadRecord(term, 0);
        Draw(term, 1);
        return SIGNAL_OKAY;
    }

    if (page != new_page)
    {
        page = new_page;
        Draw(term, 0);
        return SIGNAL_OKAY;
    }
    return SIGNAL_IGNORED;
}

int LaborZone::Update(Terminal *term, int update_message, const genericChar *value)
{
    FnTrace("LaborZone::Update()");
    Report *r = report;
    if (r == NULL || r->update_flag & update_message)
        return Draw(term, 1);

    // FIX - obsolete - replace with value in r->update_flag
    if (update_message & (UPDATE_ARCHIVE | UPDATE_JOB_FILTER | UPDATE_SERVER))
        Draw(term, 1);
    return 0;
}

int LaborZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("LaborZone::LoadRecord()");
    FormField *f = FieldList();
    if (work == NULL)
    {
        while (f)
        {
            f->active = 0;
            f = f->next;
        }
        return 0;
    }

    f->active = 1;
    f->Set(work->start); f = f->next;

    f->Set(work->end);
    if (work->end.IsSet())
    {
        f->active = 1; f = f->next;
        f->active = 0; f = f->next;
        f->active = 0; f = f->next;
    }
    else
    {
        f->active = 0; f = f->next;
        f->active = 1; f = f->next;
        f->active = 1; f = f->next;
    }

    f->active = 1;
    f->ClearEntries();
    Employee *employee = term->system_data->user_db.FindByID(work->user_id);
    if (employee)
    {
        for (JobInfo *j = employee->JobList(); j != NULL; j = j->next)
            f->AddEntry(j->Title(term), j->job);
    }
    else
    {
        const genericChar *s = FindStringByValue(work->job, JobValue, JobName, UnknownStr);
        f->AddEntry(s, work->job);
    }
    f->Set(work->job);
    f = f->next;

    f->Set(term->SimpleFormatPrice(work->pay_amount));
    f->active = 1; f = f->next;

    f->Set(work->pay_rate);
    f->active = 1; f = f->next;

    f->Set(term->SimpleFormatPrice(work->tips));
    f->active = 1; f = f->next;
    return 0;
}

int LaborZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("LaborZone::SaveRecord()");
    Employee *employee = term->user;
    if (employee == NULL)
        return 1;

    if (work)
    {
        TimeInfo work_start;
        TimeInfo we;
        int job, tips, pay, rate;
        FormField *f = FieldList();
        f->Get(work_start); f = f->next;
        f->Get(we); f = f->next;
        f = f->next;
        f = f->next;
        f->Get(job); f = f->next;
        f->GetPrice(pay); f = f->next;
        f->Get(rate); f = f->next;
        f->GetPrice(tips); f = f->next;
    
        if (work->start != work_start || work->end != we || work->job != job ||
            work->pay_rate != rate || work->pay_amount != pay ||
            work->tips != tips)
        {
            work->Edit(employee->id);
            work->start      = work_start;
            work->end        = we;
            work->job        = job;
            work->pay_rate   = rate;
            work->pay_amount = pay;
            work->tips       = tips;
        }
    }

    if (write_file)
        period->Save();
    return 0;
}

int LaborZone::UpdateForm(Terminal *term, int record)
{
    FnTrace("LaborZone::UpdateForm()");
    Employee *employee = term->user;
    if (employee == NULL || work == NULL)
        return 1;

    TimeInfo work_start;
    TimeInfo we;
    FormField *f = FieldList();
    f->Get(work_start); f = f->next;
    f->Get(we); f = f->next;

    if (work->start != work_start || work->end != we)
    {
        work->Edit(employee->id);
        work->start = work_start;
        work->end   = we;
        if (report)
        {
            delete report;
            report = NULL;
        }
    }
    return 0;
}

int LaborZone::Print(Terminal *term, int print_mode)
{
    FnTrace("LaborZone::Print()");
    if (print_mode == RP_NO_PRINT)
        return 0;

    Employee *employee = term->user;
    if (employee == NULL || report == NULL)
        return 1;

    Printer *p1 = term->FindPrinter(PRINTER_RECEIPT);
    Printer *p2 = term->FindPrinter(PRINTER_REPORT);
    if (p1 == NULL && p2 == NULL)
        return 1;

    if (print_mode == RP_ASK)
    {
        DialogZone *d = NewPrintDialog(p1 == p2);
        d->target_zone = this;
        term->OpenDialog(d);
        return 0;
    }

    Printer *printer = p1;
    if ((print_mode == RP_PRINT_REPORT && p2) || p1 == NULL)
        printer = p2;
    if (printer == NULL)
        return 1;

    report->CreateHeader(term, printer, employee);
    report->FormalPrint(printer);

    return 0;
}


/**** ScheduleZone Class ****/
// Constructor
ScheduleZone::ScheduleZone()
{
    name_len = 0;
}

// Member Functions
RenderResult ScheduleZone::Render(Terminal *term, int update_flag)
{
    FnTrace("ScheduleZone::Render()");
    Employee *employee;

    RenderZone(term, NULL, update_flag);

    int users = 0;
    name_len = 0;
    System *sys = term->system_data;
    Settings *s = &(sys->settings);
    for (employee = sys->user_db.UserList(); employee != NULL; employee = employee->next)
    {
        if (employee->active && employee->system_name.length > name_len)
        {
            name_len = employee->system_name.length;
            ++users;
        }
    }

    int font_height, font_width;
    term->FontSize(font, font_width, font_height);

    int top_margin = font_height + border*2;
    int side_margin = (name_len + 2) * font_width + border*2;
    int cw = w - border*2 - side_margin;
    int ch = h - border*2 - top_margin;
    int cx, cy, hour = s->day_start;
    int day_hours = s->day_end - s->day_start;
    if (day_hours <= 0)
        day_hours += 24;

    for (int i = 0; i <= day_hours; ++i)
    {
        cx = side_margin + x - 1 + ((cw * i) / day_hours);
        term->RenderVLine(cx, y + border + font_height, h - border*2 - font_height,
                       COLOR_BLACK);
        term->RenderText(HourName[hour], cx, y + border, COLOR_BLACK,
                      FONT_TIMES_20, ALIGN_CENTER);
        ++hour;
        if (hour >= 24)
            hour = 0;
    }

    int no = 0;
    for (employee = sys->user_db.UserList(); employee != NULL; employee = employee->next)
    {
        if (employee->active)
        {
            cy = top_margin + y + ((ch * no) / users) + (ch / (users *2));
            term->RenderText(employee->system_name.Value(), x + border*2, cy,
                          color[0], FONT_TIMES_20);
            ++no;
        }
    }
    return RENDER_OKAY;
}

SignalResult ScheduleZone::Signal(Terminal *term, const genericChar *message)
{
    FnTrace("ScheduleZone::Signal()");
    return SIGNAL_IGNORED;
}

SignalResult ScheduleZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("ScheduleZone::Touch()");
    return SIGNAL_IGNORED;
}

SignalResult ScheduleZone::Mouse(Terminal *term, int action, int mx, int my)
{
    FnTrace("ScheduleZone::Mouse()");
    return SIGNAL_IGNORED;
}
