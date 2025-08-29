/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997  
  
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
 * creditcard_list_zone.cc
 * Touch zone for managing credit cards
 */

#include "creditcard_list_zone.hh"
#include "dialog_zone.hh"
#include "system.hh"
#include "terminal.hh"
#include "utility.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define MODE_EXCEPTIONS 1
#define MODE_REFUNDS    2
#define MODE_VOIDS      3

#define MAX_LOOPS       3

#define FROM_ARCHIVE    0
#define FROM_SYSTEM     1


CreditCardListZone::CreditCardListZone()
{
    FnTrace("CreditCardListZone::CreditCardListZone()");

    report      = NULL;
    credit      = NULL;
    mode        = MODE_EXCEPTIONS;
    no_line     = 1;
    creditdb    = MasterSystem->cc_exception_db.get();
    archive     = NULL;
    lines_shown = 0;
    list_footer = 1;
}

CreditCardListZone::~CreditCardListZone()
{
    FnTrace("CreditCardListZone::~CreditCardListZone()");
}

RenderResult CreditCardListZone::Render(Terminal *term, int update_flag)
{
    FnTrace("CreditCardListZone::Render()");
    RenderResult retval = RENDER_OKAY;
    int col = color[0];
    Flt header_line = 1.3;
    num_spaces = ListFormZone::ColumnSpacing(term, 4);
    int indent = 0;
    list_spacing = 1.3;

    if (update_flag == RENDER_NEW)
    {
        credit = NULL;
        archive = NULL;
        mode = MODE_EXCEPTIONS;
        creditdb = MasterSystem->cc_exception_db.get();
    }

    FormZone::Render(term, update_flag);

    if (mode == MODE_EXCEPTIONS)
        name.Set(term->Translate("Exceptions"));
    else if (mode == MODE_REFUNDS)
        name.Set(term->Translate("Refunds"));
    else if (mode == MODE_VOIDS)
        name.Set(term->Translate("Voids"));
    else
        name.Set(term->Translate("Credit Cards"));
    TextC(term, 0, name.Value(), col);

    TextPosL(term, indent, header_line, term->Translate("Card Num."), col);
    indent += num_spaces + 5;
    TextPosL(term, indent, header_line, term->Translate("Expiry"), col);
    indent += num_spaces;
    TextPosL(term, indent, header_line, term->Translate("Amount"), col);
    indent += num_spaces;
    TextPosL(term, indent, header_line, term->Translate("Status"), col);

    if (update || update_flag || (report == NULL))
    {
        if (report != NULL)
            delete report;
        report = new Report;
        ListReport(term, report);
    }

    if (report != NULL)
    {
        if (credit != NULL)
            report->selected_line = record_no;
        else
            report->selected_line = -1;
    
        // end the report two lines above the top of the form field area so that
        // there is plenty of room for the "Page x of y" display.
        if (lines_shown == 0)
            page = -1;
        report->Render(term, this, 2, list_footer, page, 0, list_spacing);
        page = report->page;
        lines_shown = report->lines_shown;
    }

    return retval;
}

SignalResult CreditCardListZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("CreditCardListZone::Signal()");
    SignalResult retval = SIGNAL_OKAY;
    static const char* commands[] = {"ccexceptions", "ccrefunds", "ccvoids",
                               "next", "prev", "nextperiod", "prevperiod",
                               "process", NULL};
    int idx = CompareListN(commands, message);
    CreditCardDialog *ccdialog = NULL;

    switch (idx)
    {
    case 0:  // ccexception
        if (mode != MODE_EXCEPTIONS)
        {
            credit = NULL;
            archive = NULL;
            record_no = -1;
            creditdb = term->system_data->cc_exception_db.get();
            mode = MODE_EXCEPTIONS;
        }
        break;
    case 1:  // ccrefund
        if (mode != MODE_REFUNDS)
        {
            credit = NULL;
            archive = NULL;
            record_no = -1;
            creditdb = term->system_data->cc_refund_db.get();
            mode = MODE_REFUNDS;
        }
        break;
    case 2:  // ccvoid
        if (mode != MODE_VOIDS)
        {
            credit = NULL;
            archive = NULL;
            record_no = -1;
            creditdb = term->system_data->cc_void_db.get();
            mode = MODE_VOIDS;
        }
        break;
    case 3:  // next
        record_no += 1;
        if (record_no >= RecordCount(term))
            record_no = 0;
        LoadRecord(term, record_no);
        break;
    case 4:  // prev
        record_no -= 1;
        if (record_no < 0)
            record_no = RecordCount(term) - 1;
        LoadRecord(term, record_no);
        break;
    case 5:  // nextperiod
        creditdb = NextCreditDB(term);
        credit = NULL;
        record_no = -1;
        break;
    case 6:  // prevperiod
        creditdb = PrevCreditDB(term);
        credit = NULL;
        record_no = -1;
        break;
    case 7:  // process
        if (credit != NULL)
        {
            term->credit = credit;
            ccdialog = new CreditCardDialog(term);
            term->OpenDialog(ccdialog);
        }
        break;
    default:
        retval = SIGNAL_IGNORED;
    }

    if (retval == SIGNAL_OKAY)
        Draw(term, 1);
    
    return retval;
}

SignalResult CreditCardListZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("CreditCardListZone::Touch()");
    SignalResult retval = SIGNAL_IGNORED;

    if (report != NULL)
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
            LoadRecord(term, yy);
            Draw(term, 1);
            retval = SIGNAL_OKAY;
        }
        
        if (page != new_page)
        {
            page = new_page;
            Draw(term, 1);
            retval = SIGNAL_OKAY;
        }
    }
    return retval;
}

int CreditCardListZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("CreditCardListZone::LoadRecord()");
    int retval = 1;

    if (creditdb != NULL)
        credit = creditdb->FindByRecord(term, record);

    if (credit != NULL)
        record_no = record;
    else
        record_no = -1;

    return retval;
}

int CreditCardListZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("CreditCardListZone::SaveRecord()");
    int retval = 1;

    return retval;
}

int CreditCardListZone::RestoreRecord(Terminal *term)
{
    FnTrace("CreditCardListZone::RestoreRecord()");
    int retval = 1;

    return retval;
}

int CreditCardListZone::NewRecord(Terminal *term)
{
    FnTrace("CreditCardListZone::NewRecord()");
    int retval = 1;

    return retval;
}

int CreditCardListZone::KillRecord(Terminal *term, int record)
{
    FnTrace("CreditCardListZone::KillRecord()");
    int retval = 1;

    return retval;
}

int CreditCardListZone::PrintRecord(Terminal *term, int record)
{
    FnTrace("CreditCardListZone::PrintRecord()");
    int retval = 1;

    return retval;
}

int CreditCardListZone::Search(Terminal *term, int record, const genericChar* word)
{
    FnTrace("CreditCardListZone::Search()");

    int retval = 1;

    return retval;
}

int CreditCardListZone::ListReport(Terminal *term, Report *lreport)
{
    FnTrace("CreditCardListZone::ListReport()");
    int retval = 1;

    creditdb->MakeReport(term, lreport, this);

    return retval;
}

int CreditCardListZone::RecordCount(Terminal *term)
{
    FnTrace("CreditCardListZone::RecordCount()");
    int retval = 0;

    retval = creditdb->Count();

    return retval;
}

CreditDB *CreditCardListZone::GetDB(int in_system)
{
    FnTrace("CreditCardListZone::GetDB()");
    CreditDB *retval = NULL;

    if (in_system)
    {
        switch (mode)
        {
        case MODE_EXCEPTIONS:
            retval = MasterSystem->cc_exception_db.get();
            break;
        case MODE_REFUNDS:
            retval = MasterSystem->cc_refund_db.get();
            break;
        case MODE_VOIDS:
            retval = MasterSystem->cc_void_db.get();
            break;
        }
    }
    else if (archive != NULL)
    {
        switch (mode)
        {
        case MODE_EXCEPTIONS:
            retval = archive->cc_exception_db;
            break;
        case MODE_REFUNDS:
            retval = archive->cc_refund_db;
            break;
        case MODE_VOIDS:
            retval = archive->cc_void_db;
            break;
        }
    }

    return retval;
}

CreditDB *CreditCardListZone::NextCreditDB(Terminal *term)
{
    FnTrace("CreditCardListZone::NextCreditDB()");
    CreditDB *retval = NULL;
    int loops = 0;

    if (creditdb == NULL)
        retval = GetDB(FROM_SYSTEM);
    else
    {
        while (loops < MAX_LOOPS)
        {
            if (archive == NULL)
                archive = MasterSystem->ArchiveList();
            else
            {
                do
                {
                    archive = archive->next;
                } while (archive != NULL && GetDB(FROM_ARCHIVE) == NULL);
            }

            if (archive != NULL)
                retval = GetDB(FROM_ARCHIVE);
            else
                retval = GetDB(FROM_SYSTEM);
            loops += ((retval == NULL) ? 1 : MAX_LOOPS);
        }
    }
    
    return retval;
}

CreditDB *CreditCardListZone::PrevCreditDB(Terminal *term)
{
    FnTrace("CreditCardListZone::PrevCreditDB()");
    CreditDB *retval = NULL;
    int loops = 0;

    if (creditdb == NULL)
        retval = GetDB(FROM_SYSTEM);
    else
    {
        while (loops < MAX_LOOPS)
        {
            if (archive == NULL)
                archive = MasterSystem->ArchiveListEnd();
            else
            {
                do
                {
                    archive = archive->fore;
                } while (archive != NULL && GetDB(FROM_ARCHIVE) == NULL);
            }

            if (archive != NULL)
                retval = GetDB(FROM_ARCHIVE);
            else
                retval = GetDB(FROM_SYSTEM);
            loops += ((retval == NULL) ? 1 : MAX_LOOPS);
        }
    }
    
    return retval;
}
