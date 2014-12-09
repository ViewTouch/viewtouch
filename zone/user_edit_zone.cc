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
 * user_edit_zone.cc - revision 118 (10/13/98)
 * Implementation of UserEditZone module
 */

#include "user_edit_zone.hh"
#include "terminal.hh"
#include "employee.hh"
#include "labels.hh"
#include "report.hh"
#include "dialog_zone.hh"
#include "system.hh"
#include "manager.hh"
#include <string.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/**** UserEditZone Class ****/
// Constructor
UserEditZone::UserEditZone()
{
    list_header = 2;
    AddTextField("User ID", 9); SetFlag(FF_ONLYDIGITS);
    AddTextField("Nickname", 10);
    AddListField("Training", NoYesName);
    AddNewLine(2);
    AddTextField("Last Name", 16);
    AddTextField("First Name", 16);
    AddTextField("Address", 40);
    AddTextField("City", 16);
    AddTextField("State", 3); SetFlag(FF_ALLCAPS);
    AddTemplateField("Phone", "(___) ___-____"); SetFlag(FF_ONLYDIGITS);
    AddTemplateField("SSN", "___-__-____");  SetFlag(FF_ONLYDIGITS);
    AddTextField("Job Info", 24);
    AddTextField("Employee #", 8);
    AddNewLine(2);

    Center(); Color(COLOR_WHITE);
    AddLabel("Primary Job");
    AddNewLine();
    LeftAlign(); Color(COLOR_DEFAULT);
    AddListField("Job", JobName, JobValue);
    AddListField("Pay Rate", PayRateName, PayRateValue);
    AddTextField("Amount", 7);
    AddListField("Start Page", NULL);
    AddTextField("Department", 8);
    Color(COLOR_RED);
    AddButtonField("Remove This Job", "killjob1");
    AddNewLine(2);

    Center(); Color(COLOR_WHITE);
    AddLabel("2nd Job");
    AddNewLine();
    LeftAlign(); Color(COLOR_DEFAULT);
    AddListField("Job", JobName, JobValue);
    AddListField("Pay Rate", PayRateName, PayRateValue);
    AddTextField("Amount", 7);
    AddListField("Start Page", NULL);
    AddTextField("Department", 8);
    Color(COLOR_RED);
    AddButtonField("Remove This Job", "killjob2");
    AddNewLine(2);

    Center(); Color(COLOR_WHITE);
    AddLabel("3rd Job");
    AddNewLine();
    LeftAlign(); Color(COLOR_DEFAULT);
    AddListField("Job", JobName, JobValue);
    AddListField("Pay Rate", PayRateName, PayRateValue);
    AddTextField("Amount", 7);
    AddListField("Start Page", NULL);
    AddTextField("Department", 8);
    Color(COLOR_RED);
    AddButtonField("Remove This Job", "killjob3");
    AddNewLine(2);

    Center(); Color(COLOR_DK_GREEN);
    AddButtonField("* Add Another Job *", "addjob");
    view_active = 1;
}

// Member Functions
RenderResult UserEditZone::Render(Terminal *term, int update_flag)
{
    FnTrace("UserEditZone::Render()");
    if (update_flag == RENDER_NEW)
        view_active = 1;

    char str[256];
    ListFormZone::Render(term, update_flag);
    int col = color[0];
    if (show_list)
    {
        if (term->job_filter)
        {
            if (view_active)
                strcpy(str, "Filtered Active Employees");
            else
                strcpy(str, "Filtered Inactive Employees");
        }
        else
        {
            if (view_active)
                strcpy(str, "All Active Employees");
            else
                strcpy(str, "All Inactive Employees");
        }

        TextC(term, 0, term->Translate(str), col);
        TextL(term, 1.3, term->Translate("Employee Name"), col);
        TextC(term, 1.3, term->Translate("Job Title"), col);
        TextR(term, 1.3, term->Translate("Phone Number"), col);
    }
    else
    {
        if (records == 1)
            strcpy(str, "Employee Record");
        else
            sprintf(str, "Employee Record %d of %d", record_no + 1, records);
        TextC(term, 0, str, col);
    }
    return RENDER_OKAY;
}

SignalResult UserEditZone::Signal(Terminal *term, const char* message)
{
    FnTrace("UserEditZone::Signal()");
    static const char* commands[] = {
        "active", "inactive", "clear password", "remove", "activate",
        "addjob", "killjob1", "killjob2", "killjob3", NULL};
    int idx = CompareList(message, commands);

    if (idx < 0)
        return ListFormZone::Signal(term, message);

    switch (idx)
    {
    case 0:  // active
    case 1:  // inactive
        if (records > 0)
            SaveRecord(term, record_no, 0);
        show_list = 1;
        view_active ^= 1;
        record_no = 0;
        records = RecordCount(term);
        if (records > 0)
            LoadRecord(term, record_no);
        Draw(term, 1);
        return SIGNAL_OKAY;
    }

    if (user == NULL)
        return SIGNAL_IGNORED;

    switch (idx)
    {
    case 2:  // clear password
        user->password.Clear();
        SaveRecord(term, record_no, 0);
        Draw(term, 1);
        return SIGNAL_OKAY;

    case 3:  // remove
        if (user->active)
        {
            if (user->last_job == 0)
            {
                user->active = 0;
                records = RecordCount(term);
                if (record_no >= records)
                    record_no = records - 1;
                if (records > 0)
                    LoadRecord(term, record_no);
                Draw(term, 1);
            }
            else
            {
                char str[STRLENGTH] = "";
                strcat(str, "This employee is clocked in.  You cannot\\");
                strcat(str, "change the employee's status until he or\\");
                strcat(str, "she is clocked out of the system.");
                SimpleDialog *d = new SimpleDialog(str);
                d->force_width = 600;
                d->Button("Okay");
                term->OpenDialog(d);
            }
        }
        else
        {
            char str[STRLENGTH];
            sprintf(str, "Employee '%s' is inactive.  What do you want to do?",
                    user->system_name.Value());
            SimpleDialog *d = new SimpleDialog(str);
            d->Button("Reactivate this employee", "activate");
            d->Button("Completely remove employee", "delete");
            d->Button("Oops!\\Leave as is");
            d->target_zone = this;
            term->OpenDialog(d);
        }
        return SIGNAL_OKAY;

    case 4:  // activate
        user->active = 1;
        show_list = 1;
        records = RecordCount(term);
        if (record_no >= records)
            record_no = records - 1;
        if (records > 0)
            LoadRecord(term, record_no);
        Draw(term, 1);
        return SIGNAL_OKAY;

    case 5:  // addjob
        if (user->JobCount() < 3)
        {
            SaveRecord(term, record_no, 0);
            JobInfo *j = new JobInfo;
            user->Add(j);
            LoadRecord(term, record_no);
            keyboard_focus = NULL;
            Draw(term, 0);
        }
        break;
    case 6:  // killjob1
        if (user->JobCount() >= 1)
        {
            SaveRecord(term, record_no, 0);
            JobInfo *j = user->JobList();
            user->Remove(j);
            delete j;
            LoadRecord(term, record_no);
            keyboard_focus = NULL;
            Draw(term, 0);
        }
        break;
    case 7:  // killjob2
        if (user->JobCount() >= 2)
        {
            SaveRecord(term, record_no, 0);
            JobInfo *j = user->JobList()->next;
            user->Remove(j);
            delete j;
            LoadRecord(term, record_no);
            keyboard_focus = NULL;
            Draw(term, 0);
        }
        break;
    case 8:  // killjob3
        if (user->JobCount() >= 3)
        {
            SaveRecord(term, record_no, 0);
            JobInfo *j = user->JobList()->next->next;
            user->Remove(j);
            delete j;
            LoadRecord(term, record_no);
            keyboard_focus = NULL;
            Draw(term, 0);
        }
        break;
    }
    return SIGNAL_IGNORED;
}

int UserEditZone::Update(Terminal *term, int update_message, const char* value)
{
    if (update_message & UPDATE_JOB_FILTER)
    {
        SaveRecord(term, record_no, 0);
        record_no = 0;
        show_list = 1;
        Draw(term, 1);
        return 0;
    }
    else
    {
        return ListFormZone::Update(term, update_message, value);
    }
}

int UserEditZone::AddStartPages(Terminal *term, FormField *field)
{
    FnTrace("UserEditZone::AddStartPages()");
    int retval = 0;

    int last_page = 0;
    field->ClearEntries();
    for (Page *p = term->zone_db->PageList(); p != NULL; p = p->next)
    {
        if (p->IsStartPage() && p->id != last_page)
        {
            last_page = p->id;
            field->AddEntry(p->name.Value(), p->id);
        }
    }
    //NOTE BAK-->Check List Page is not a specific page, but a page type.
    //NOTE What happens when there are two Check List Page entries?
    field->AddEntry("Check List Page", 0);
    return retval;
}

int UserEditZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("UserEditZone::LoadRecord()");
    System *sys = term->system_data;
    Employee *e = sys->user_db.FindByRecord(term, record, view_active);
    if (e == NULL)
        return 1;

    Settings *s = &(sys->settings);
    user = e;

    int job_active[MAX_JOBS];
    int i;
    for (i = 0; i < MAX_JOBS; ++i)
        job_active[i] = 0;

    int list = 0;
    while (JobName[list])
    {
        int j = JobValue[list];
        job_active[list] = s->job_active[j];
        ++list;
    }

    FormField *f = FieldList();
    f->Set(e->key); f = f->next;
    f->Set(e->system_name); f = f->next;

    f->Set(e->training); f = f->next;
    f->Set(e->last_name); f = f->next;
    f->Set(e->first_name); f = f->next;
    f->Set(e->address); f = f->next;
    f->Set(e->city); f = f->next;
    f->Set(e->state); f = f->next;
    f->Set(e->phone); f = f->next;
    f->Set(e->ssn); f = f->next;
    f->Set(e->description); f = f->next;
    f->Set(e->employee_no); f = f->next;

    JobInfo *j = e->JobList();
    for (i = 0; i < 3; ++i)
    {
        if (j)
        {
            f->active = 1; f = f->next;
            f->active = 1; f->Set(j->job); f->SetActiveList(job_active); f = f->next;
            f->active = 1; f->Set(j->pay_rate); f = f->next;
            f->active = 1; f->Set(term->SimpleFormatPrice(j->pay_amount)); f = f->next;
            f->active = 1; AddStartPages(term, f); f->Set(j->starting_page); f = f->next;
            f->active = 1; f->Set(j->dept_code); f = f->next;
            f->active = (e->JobCount() > 1); f = f->next;
            j = j->next;
        }
        else
        {
            for (int k = 0; k < 7; ++k)
            {
                f->active = 0; f = f->next;
            }
        }
    }
    f->active = (e->JobCount() < 3);
    return 0;
}

int UserEditZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("UserEditZone::SaveRecord()");
    Employee *e = user;
    if (e)
    {
        FormField *f = FieldList();
        f->Get(e->key); f = f->next;
        f->Get(e->system_name); f = f->next;
        AdjustCase((genericChar*)e->system_name.Value());
        f->Get(e->training); f = f->next;
        f->Get(e->last_name); f = f->next;
        AdjustCase(e->last_name.Value());
        f->Get(e->first_name); f = f->next;
        AdjustCase(e->first_name.Value());
        f->Get(e->address); f = f->next;
        AdjustCase(e->address.Value());
        f->Get(e->city); f = f->next;
        AdjustCase(e->city.Value());
        f->Get(e->state); f = f->next;
        StringToUpper(e->state.Value());
        f->Get(e->phone); f = f->next;
        f->Get(e->ssn); f = f->next;
        f->Get(e->description); f = f->next;
        f->Get(e->employee_no); f = f->next;

        for (JobInfo *j = e->JobList(); j != NULL; j = j->next)
        {
            f = f->next;
            f->Get(j->job); f = f->next;
            f->Get(j->pay_rate); f = f->next;
            f->GetPrice(j->pay_amount); f = f->next;
            f->Get(j->starting_page); f = f->next;
            f->Get(j->dept_code); f = f->next;
            f = f->next;
        }
    }

    if ((e->system_name.length <= 0) &&
        ((e->first_name.length > 0) && (e->last_name.length > 0)))
    {
        char tempname[STRLONG];
        snprintf(tempname, STRLONG, "%s %s", e->first_name.Value(), e->last_name.Value());
        e->system_name.Set(tempname);
    }
    

    if (write_file)
        term->system_data->user_db.Save();
    return 0;
}

int UserEditZone::NewRecord(Terminal *term)
{
    FnTrace("UserEditZone::NewRecord()");
    term->job_filter = 0; // make sure new user is shown on list
    user = term->system_data->user_db.NewUser();
    record_no = 0;
    view_active = 1;
    return 0;
}

int UserEditZone::KillRecord(Terminal *term, int record)
{
    FnTrace("UserEditZone::KillRecord()");
    if (user == NULL || term->IsUserOnline(user))
        return 1;
    term->system_data->user_db.Remove(user);
    delete user;
    user = NULL;
    return 0;
}

int UserEditZone::PrintRecord(Terminal *term, int record)
{
    FnTrace("UserEditZone::PrintRecord()");
    // FIX - finish UserEditZone::PrintRecord()
    return 1;
}

int UserEditZone::Search(Terminal *term, int record, const char* word)
{
    FnTrace("UserEditZone::Search()");
    int r = term->system_data->user_db.FindRecordByWord(term, word, view_active, record);
    if (r < 0)
        return 0;  // no matches
    record_no = r;
    return 1;  // one match (only 1 for now)
}

int UserEditZone::ListReport(Terminal *term, Report *r)
{
    return term->system_data->user_db.ListReport(term, view_active, r);
}

int UserEditZone::RecordCount(Terminal *term)
{
    return term->system_data->user_db.UserCount(term, view_active);
}


/**** JobSecurityZone Class ****/
// Constructor
JobSecurityZone::JobSecurityZone()
{
    wrap        = 0;
    keep_focus  = 0;
    form_header = 2;
    columns     = 11;
    int i;

    for (i = 1; JobName[i] != NULL; ++i)
    {
        AddLabel(JobName[i], 17);
        AddListField("", MarkName, NULL, 0, 4);
        AddSpace(1);
        AddListField("", MarkName, NULL, 0, 7);
        AddListField("", MarkName, NULL, 0, 7);
        AddListField("", MarkName, NULL, 0, 7);
        AddListField("", MarkName, NULL, 0, 7);
        AddListField("", MarkName, NULL, 0, 7);
        AddListField("", MarkName, NULL, 0, 7);
        AddListField("", MarkName, NULL, 0, 7);
        AddListField("", MarkName, NULL, 0, 7);
        AddListField("", MarkName, NULL, 0, 7);
        AddNewLine();
    }
}

// Member Functions
RenderResult JobSecurityZone::Render(Terminal *term, int update_flag)
{
    FnTrace("JobSecurityZone::Render()");

    int col = color[0];
    FormZone::Render(term, update_flag);
    TextPosC(term,   6,   .5, "Job", col);
    TextPosC(term,  21,   .5, "Active", col);
    TextPosC(term,  29.5,  0, "Enter", col);
    TextPosC(term,  29.5,  1, "System", col);
    TextPosC(term,  38.5, .5, "Order", col);
    TextPosC(term,  47.5, .5, "Settle", col);
    TextPosC(term,  56.5,  0, "Move", col);
    TextPosC(term,  56.5,  1, "Table", col);
    TextPosC(term,  65.5,  0, "Rebuild", col);
    TextPosC(term,  65.5,  1, "Edit", col);
    TextPosC(term,  74.5, .5, "Comp", col);
    TextPosC(term,  83.5,  0, "Supervisor", col);
    TextPosC(term,  83.5,  1, "Functions", col);
    TextPosC(term,  92.5,  0, "Manager", col);
    TextPosC(term,  92.5,  1, "Functions", col);
    TextPosC(term, 101.5,  0, "Employee", col);
    TextPosC(term, 101.5,  1, "Records", col);
    return RENDER_OKAY;
}

/****
 * IsActiveField:  What do I name this?  The idea is that there
 *  is one field per job category, second field from the left.
 *  We need to determine if that field is the current field,
 *  and whether it is being disabled.  If both of those are
 *  true, return 1.  Otherwise, return 0.
 ****/
int JobSecurityZone::DisablingCategory()
{
    FnTrace("JobSecurityZone::DisablingCategory()");
    int retval       = 0;
    FormField *field = FieldList();
    int counter      = 0;
    int is_active    = 0;  // container for the field's value

    while (field != NULL && retval == 0)
    {
        // Every 'columns' columns, we have the label.  The next
        // field over is the "active" field for the current job,
        // which is the one we want.
        if ((counter % columns) == 0)
        {
            // We're on the right row.  Now skip to the field we want.
            field = field->next;
            counter += 1;
            field->Get(is_active);
            if (field == keyboard_focus && is_active == 1)
            {
                // We have a match.  We'll give the index of the job category,
                // which is slightly convoluted.
                retval = ((counter - 1) / columns) + 1;
            }
        }
        if (field != NULL)
            field = field->next;
        counter += 1;
    }

    // retval is an index into JobValue[]
    retval = JobValue[retval];

    return retval;
}

int JobSecurityZone::EmployeeIsUsing(Terminal *term, int active_job)
{
    FnTrace("JobSecurityZone::EmployeeIsUsing()");
    int retval = 0;
    Employee *employee   = NULL;
    JobInfo  *jobinfo    = NULL;

    employee = MasterSystem->user_db.UserList();
    while (employee != NULL && retval == 0)
    {
        jobinfo = employee->JobList();
        while (jobinfo != NULL && retval == 0)
        {
            if (jobinfo->job == active_job)
                retval = 1;

            jobinfo = jobinfo->next;
        }
        employee = employee->next;
    }

    return retval;
}

SignalResult JobSecurityZone::Signal(Terminal *term, const char* message)
{
    FnTrace("JobSecurityZone::Signal()");
    SignalResult retval = SIGNAL_IGNORED;
    static const genericChar* commands[] = { "jsz_no", "jsz_yes", NULL};
    int idx = CompareListN(commands, message);

    switch (idx)
    {
    case 0:
        last_focus = NULL;
        break;
    case 1:
        if (last_focus != NULL)
        {
            keyboard_focus = last_focus;
            last_focus = NULL;
            keyboard_focus->Touch(term, this, keyboard_focus->x + 1, keyboard_focus->y + 1);
            UpdateForm(term, 0);
            Draw(term, 0);
        }
        break;
    default:
        retval = FormZone::Signal(term, message);
        break;
    }

    return retval;
}

SignalResult JobSecurityZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("JobSecurityZone::Touch()");
    int active_cat        = 0;
    int is_used           = 0;
    SimpleDialog *sdialog = NULL;
    if (records <= 0)
        return SIGNAL_IGNORED;
    
    LayoutZone::Touch(term, tx, ty);

    // It's bad to disable a job category when there are employee's configured
    // for that job.  We'll allow it, but only after a confirmation.
    keyboard_focus = Find(selected_x, selected_y);
    active_cat = DisablingCategory();
    if (active_cat > 0)
        is_used = EmployeeIsUsing(term, active_cat);
    if (is_used)
    {
        last_focus = keyboard_focus;
        sdialog = new SimpleDialog("This category is in use.  Are \
                                    you sure you want to disable it?");
        sdialog->Button("Yes", "jsz_yes");
        sdialog->Button("No", "jsz_no");
        term->OpenDialog(sdialog);
    }
    else
        FormZone::Touch(term, tx, ty);

    Draw(term, 0);
    return SIGNAL_OKAY;
}

SignalResult JobSecurityZone::Mouse(Terminal *term, int action, int mx, int my)
{
    FnTrace("JobSecurityZone::Mouse()");
    SignalResult retval = SIGNAL_IGNORED;
    if (records <= 0 || !(action & MOUSE_PRESS))
        return SIGNAL_IGNORED;

    // mouse touches are just touches here
    if (action & MOUSE_PRESS)
        retval = Touch(term, mx, my);

    return retval;
}

int JobSecurityZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("JobSecurityZone::LoadRecord()");
    Settings  *s = term->GetSettings();
    FormField *f = FieldList();
    int i = 1;
    while (JobName[i])
    {
        int j    = JobValue[i];
        int a    = s->job_active[j];
        int flag = s->job_flags[j];

        // job title
        f->label.Set(term->Translate(JobName[i]));
        f = f->next;

        // Active switch
        f->Set(a); f = f->next;

        // tables
        f->active = a;
        if (flag & SECURITY_TABLES)
            f->Set(1);
        else
            f->Set(0);
        f = f->next;

        // order
        f->active = a;
        if (flag & SECURITY_ORDER)
            f->Set(1);
        else
            f->Set(0);
        f = f->next;

        // settle
        f->active = a;
        if (flag & SECURITY_SETTLE)
            f->Set(1);
        else
            f->Set(0);
        f = f->next;

        // transfer
        f->active = a;
        if (flag & SECURITY_TRANSFER)
            f->Set(1);
        else
            f->Set(0);
        f = f->next;

        // rebuild
        f->active = a;
        if (flag & SECURITY_REBUILD)
            f->Set(1);
        else
            f->Set(0);
        f = f->next;

        // comp
        f->active = a;
        if (flag & SECURITY_COMP)
            f->Set(1);
        else
            f->Set(0);
        f = f->next;

        // supervisor
        f->active = a;
        if (flag & SECURITY_SUPERVISOR)
            f->Set(1);
        else
            f->Set(0);
        f = f->next;

        // manager
        f->active = a;
        if (flag & SECURITY_MANAGER)
            f->Set(1);
        else
            f->Set(0);
        f = f->next;

        // employees
        f->active = a;
        if (flag & SECURITY_EMPLOYEES)
            f->Set(1);
        else
            f->Set(0);
        f = f->next;
        ++i;
    }
    return 0;
}

int JobSecurityZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("JobSecurityZone::SaveRecord()");
    Settings  *s = term->GetSettings();
    FormField *f = FieldList();
    int i = 1;
    while (JobName[i])
    {
        int flag = 0;
        int val[9];
        int jv = JobValue[i];

        // job title (skip)
        f = f->next;
        // job active
        f->Get(s->job_active[jv]); f = f->next;

        int j;
        for (j = 0; j < 9; ++j)
        {
            f->Get(val[j]);
            f = f->next;
        }

        if (val[0] > 0) flag |= SECURITY_TABLES;
        if (val[1] > 0) flag |= SECURITY_ORDER;
        if (val[2] > 0) flag |= SECURITY_SETTLE;
        if (val[3] > 0) flag |= SECURITY_TRANSFER;
        if (val[4] > 0) flag |= SECURITY_REBUILD;
        if (val[5] > 0) flag |= SECURITY_COMP;
        if (val[6] > 0) flag |= SECURITY_SUPERVISOR;
        if (val[7] > 0) flag |= SECURITY_MANAGER;
        if (val[8] > 0) flag |= SECURITY_EMPLOYEES;

        s->job_flags[jv] = flag;
        ++i;
    }

    if (write_file)
        s->Save();
    return 0;
}

int JobSecurityZone::UpdateForm(Terminal *term, int record)
{
    FnTrace("JobSecurityZone::UpdateForm()");
    FormField *f = FieldList();
    int a;
    int i = 1;
    int j;

    while (JobName[i])
    {
        f = f->next;
        f->Get(a);
        f = f->next;
        for (j = 0; j < 9; ++j)
        {
            f->active = a; f = f->next;
        }
        ++i;
    }
    return 0;
}
