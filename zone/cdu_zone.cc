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
 * cdu_zone.hh
 * For Customer Display Units, we need to allow for various configuration details.
 * May allow for multiple advertisements, different text display effects, et al.
 */

#include "cdu_zone.hh"
#include "system.hh"
#include <string.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

CDUZone::CDUZone()
{
    font         = FONT_TIMES_24B;  // Set appropriate font for CDU UI
    // form_header defines the top of the space where the form fields will be drawn
    form_header = -11;
    list_footer = 12;
    form_spacing = 0.65;

    AddTextField("Line 1", 20);
    AddNewLine();
    AddTextField("Line 2", 20);
    AddNewLine();
    AddSubmit("Submit", 10);

    record_no = -1;
    report = nullptr;
    page = 0;
    no_line = 1;
    lines_shown = 0;
    show_item = 0;
    cdustring = nullptr;
}

CDUZone::~CDUZone()
{
    if (report != nullptr)
        delete report;
}

RenderResult CDUZone::Render(Terminal *term, int update_flag)
{
    FnTrace("CDUZone::Render()");
    num_spaces = FormZone::ColumnSpacing(term, CDU_ZONE_COLUMNS);
    Flt header_line = 1.3;
    list_spacing = 1.0;
    int col = color[0];

    if (show_item)
        ShowFields();
    else
        HideFields();
    FormZone::Render(term, update_flag);

    // FormZone::Render sets the initial record to 0.  But we want it to start
    // at -1, so we'll reset it here.
    if (update_flag == RENDER_NEW)
        record_no = -1;

    TextC(term, 0, term->Translate("Customer Display Unit Messages"), col); 
    TextL(term, header_line, term->Translate("First Line"), col);
    TextPosL(term, num_spaces, header_line, term->Translate("Second Line"), col);

    if (update || update_flag || (report == nullptr))
    {
        if (report != nullptr)
            delete report;
        report = new Report;
        ListReport(term, report);
    }
    if (show_item)
        report->selected_line = record_no;
    else
        report->selected_line = -1;

    // end the report two lines above the top of the form field area so that
    // there is plenty of room for the "Page x of y" display.
    if (lines_shown == 0)
        page = -1;
    else if (show_item)
        page = record_no / lines_shown;
    report->Render(term, this, 2, list_footer, page, 0, list_spacing);
    page = report->page;
    lines_shown = report->lines_shown;

    return RENDER_OKAY;
}

SignalResult CDUZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("CDUZone::Signal()");
    SignalResult signal = SIGNAL_IGNORED;
    static const genericChar* commands[] = {"next", "prior", "change view",
                                      "restore", "new", nullptr};
    int idx = CompareListN(commands, message);
    int draw = 0;

    switch (idx)
    {
    case 0:  // next
        SaveRecord(term, record_no, 1);
        record_no += 1;
        if (record_no >= records)
            record_no = 0;
        if (records >= 0)
        {
            show_item = 1;
            LoadRecord(term, record_no);
        }
        draw = 1;
        break;
    case 1:  // prior
        SaveRecord(term, record_no, 1);
        record_no -= 1;
        if (record_no < 0)
            record_no = records - 1;
        if (records >= 0)
        {
            show_item = 1;
            LoadRecord(term, record_no);
        }
        draw = 1;
        break;
    case 2:  // change view
        if (show_item == 1)
            show_item = 0;
        else if (record_no > -1)
            show_item = 1;
        draw = 1;
        break;
    case 3:  // restore
        RestoreRecord(term);
        draw = 1;
        break;
    case 4:  // new
        if (records > 0)
            SaveRecord(term, record_no, 0);
        record_no = records;
        if (NewRecord(term))
            return SIGNAL_IGNORED;
        records = RecordCount(term);
        if (record_no >= records)
            record_no = records - 1;
        LoadRecord(term, record_no);
        FirstField();
        show_list = 0;
        draw = 1;
        break;
    default:
        signal = FormZone::Signal(term, message);
    }

    if (draw)
    {
        if (UpdateForm(term, -1) == 0)
            Draw(term, 0);
        else
            Draw(term, 1);
        signal = SIGNAL_OKAY;
    }
    return signal;
}

SignalResult CDUZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("CDUZone::Touch()");
    SignalResult retval = SIGNAL_IGNORED;

    if (report != nullptr)
    {
        FormZone::Touch(term, tx, ty);
        int yy = report->TouchLine(list_spacing, selected_y);
        int max_page = report->max_pages;
        int new_page = page;
        if (yy == -1)
        {  // top of form:  page up
            --new_page;
            if (new_page < 0)
                new_page = max_page - 1;
        }
        else if (yy == -2)
        {  // bottom of form:  page down
            if (selected_y > (size_y - 2.0))
                return FormZone::Touch(term, tx, ty);
            
            ++new_page;
            if (new_page >= max_page)
                new_page = 0;
        }
        else
        {
            CDUString *cdustr = term->system_data->cdustrings.FindByRecord(yy);
            if (cdustr != cdustring)
                SaveRecord(term, record_no, 1);
            if (cdustr)
                show_item = 1;
            else
                show_item = 0;
            LoadRecord(term, yy);
            Draw(term, 1);
            retval = SIGNAL_OKAY;
        }
        
        if (page != new_page)
        {
            page = new_page;
            show_item = 0;
            Draw(term, 1);
            retval = SIGNAL_OKAY;
        }
    }
    return retval;
}

/****
 * UpdateForm:  This function isn't essential, but it makes for a better user
 *   experience by keeping the record list updated as an individual record
 *   is edited.
 ****/
int CDUZone::UpdateForm(Terminal *term, int record)
{
    FnTrace("CDUZone::UpdateForm()");
    int changed = 0;
    int idx = 0;
    Str formline;
    genericChar cduline[STRLENGTH];
    FormField *field = FieldList();

    if (cdustring == nullptr || show_item == 0)
        return 1;

    while (idx < MAX_CDU_LINES && changed == 0)
    {
        field->Get(formline);
        cdustring->GetLine(cduline, idx);
        if (strcmp(formline.Value(), cduline))
        {
            changed = 1;
            cdustring->SetLine(formline, idx);
        }
        field = field->next;
        idx += 1;
    }
    if (changed)
    {
        if (report != nullptr)
        {
            delete report;
            report = nullptr;
        }
        update = 1;
    }
    return 0;
}

int CDUZone::HideFields()
{
    FnTrace("CDUZone::HideFields()");
    FormField *field = FieldList();

    while (field != nullptr)
    {
        field->active = 0;
        field = field->next;
    }
    return 0;
}

int CDUZone::ShowFields()
{
    FnTrace("CDUZone::ShowFields()");
    FormField *field = FieldList();

    while (field != nullptr)
    {
        field->active = 1;
        field = field->next;
    }
    return 0;
}

int CDUZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("CDUZone::LoadRecord()");
    FormField *field = FieldList();
    Str buffer;

    if (show_item)
        cdustring = term->system_data->cdustrings.FindByRecord(record);
    else
        cdustring = nullptr;

    if (cdustring != nullptr)
    {
        record_no = record;
        // save off the cdustring for Undo
        if (saved_cdustring == nullptr)
            saved_cdustring = new CDUString;
        saved_cdustring->Copy(cdustring);

        cdustring->GetLine(buffer, 0);
        field->Set(buffer);
        field = field->next;
        cdustring->GetLine(buffer, 1);
        field->Set(buffer);
        show_item = 1;
        return 0;
    }
    else
    {
        show_item = 0;
    }
    return 1;
}

/****
 * SaveRecord:  the last two arguments are not used.  They're only here
 *   because an ancestor class (FormZone) requires them.
 ****/
int CDUZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("CDUZone::SaveRecord()");
    FormField *field = FieldList();
    Str buffer;

    if (cdustring != nullptr)
    {
        field->Get(buffer);
        cdustring->SetLine(buffer, 0);
        field = field->next;
        field->Get(buffer);
        cdustring->SetLine(buffer, 1);
    }

    term->system_data->cdustrings.Save();
    records = RecordCount(term);
    if (record_no >= records)
        record_no = records - 1;
    if (report != nullptr)
        delete report;
    report = nullptr;
    cdustring = nullptr;
    show_item = 0;
    update = 1;
    return 0;
}

int CDUZone::RestoreRecord(Terminal *term)
{
    FnTrace("CDUZone::RestoreRecord()");

    if (cdustring != nullptr && saved_cdustring != nullptr)
    {
        cdustring->Copy(saved_cdustring);
        LoadRecord(term, record_no);
    }
    return 0;
}

int CDUZone::NewRecord(Terminal *term)
{
    FnTrace("CDUZone::NewRecord()");
    cdustring = term->system_data->cdustrings.NewString();
    show_item = 1;
    records = RecordCount(term);
    record_no = records;
    return 0;
}

int CDUZone::KillRecord(Terminal *term, int record)
{
    FnTrace("CDUZone::KillRecord()");
    int retval = 1;

    if (show_item && cdustring)
    {
        CDUString *delstr = term->system_data->cdustrings.FindByID(cdustring->id);
        if (delstr)
        {
            term->system_data->cdustrings.Remove(delstr);
            delete delstr;
            cdustring = nullptr;
            records = RecordCount(term);
            if (record_no > records)
                record_no = records - 1;
            show_item = 0;
            retval = 0;
        }
    }
    else
    {
        term->Signal("status No record selected", group_id);
    }
    return retval;
}

int CDUZone::PrintRecord(Terminal *term, int record)
{
    FnTrace("CDUZone::PrintRecord()");

    //FIX BAK-->Not needed now (May 7, 2002) but should be completed
    return 0;
}

int CDUZone::Search(Terminal *term, int record, const genericChar* word)
{
    FnTrace("CDUZone::Search()");
    int search_rec;

    search_rec = term->system_data->cdustrings.FindRecordByWord(word, record);
    if (search_rec >= 0)
    {
        record_no = search_rec;
        show_item = 1;
        LoadRecord(term, record_no);
        return 1;
    }
    else if (show_item != 0)
    {
        record_no = -1;
        show_item = 0;
        delete report;
        report = nullptr;
    }
    return 1;
}

int CDUZone::ListReport(Terminal *term, Report *my_report)
{
    FnTrace("CDUZone::ListReport()");
    int col = COLOR_DEFAULT;
    int idx = 0;
    num_spaces = FormZone::ColumnSpacing(term, CDU_ZONE_COLUMNS);
    Str cduline;
    CDUString *currString = term->system_data->cdustrings.StringList();

    records = RecordCount(term);
    if (records < 1)
        my_report->TextC("No Messages Entered", col);

    while (currString != nullptr)
    {
        for (idx = 0; idx < 2; idx++)
        {
            currString->GetLine(cduline, idx);
            report->TextPosL(idx * num_spaces, cduline.Value(), col);
        }
        report->NewLine();
        currString = currString->next;
    }

    return 0;
}

int CDUZone::RecordCount(Terminal *term)
{
    FnTrace("CDUZone::RecordCount()");
    int my_records = 0;

    my_records = term->system_data->cdustrings.StringCount();

    return my_records;
}
