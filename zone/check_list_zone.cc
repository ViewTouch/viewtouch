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
 * check_list_zone.cc - revision 92 (10/13/98)
 * Implementation of CheckListZone module
 */

#include "check_list_zone.hh"
#include "terminal.hh"
#include "employee.hh"
#include "check.hh"
#include "system.hh"
#include "image_data.hh"
#include "archive.hh"
#include "customer.hh"

#include <cstring>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/*********************************************************************
 * Definitions & Data
 ********************************************************************/
#define HEADER_SIZE 3.8
#define FOOTER_SIZE 3.8

enum checkList_types {
	CL_ALL,
	CL_OPEN,
	CL_TAKEOUT,
	CL_CLOSED,
	CL_FASTFOOD
};

const char* CLName[] = {
    "All", "Open", "Take Out", "Closed", "Fast Food", nullptr};
int CLValue[] = {
    CL_ALL, CL_OPEN, CL_TAKEOUT, CL_CLOSED, CL_FASTFOOD, -1};


/*********************************************************************
 * CheckListZone class
 ********************************************************************/

CheckListZone::CheckListZone()
{
    array_max_size = 0;
    array_size     = 0;
    page_no    = 0;
    min_size_y = HEADER_SIZE + FOOTER_SIZE + 1;
    spacing    = 2;
    status     = CL_OPEN;
}

RenderResult CheckListZone::Render(Terminal *term, int update_flag)
{
	FnTrace("CheckListZone::Render()");
	LayoutZone::Render(term, update_flag);

	Employee *e = term->user;
	if (e == nullptr)
		return RENDER_OKAY;

	// Set up display check list
	int n = (int) ((size_y - FOOTER_SIZE - HEADER_SIZE - 1) / spacing);
	if (n > 32)
		n = 32;
	if (array_max_size != n)
	{
		page_no        = 0;
		array_max_size = n;
        if (update_flag < 1)
            update_flag    = 1;
	}

	if (update_flag)
	{
		if (update_flag == RENDER_NEW)
		{
            status = CL_OPEN;
			Check *c = term->check;
			if (c)
			{
				switch (c->Status())
				{
                case CHECK_OPEN:
                    if (c->IsTakeOut())
                        status = CL_TAKEOUT;
                    else if(c->IsFastFood())
                        status = CL_FASTFOOD;
                    else
                        status = CL_OPEN;
                    break;
						
                case CHECK_CLOSED:
                    status = CL_CLOSED; 
                    break;

                default:
                    status = CL_ALL;
                    break;
				}
			}
			else if (term->archive)
				status = CL_ALL;
			else // NOT c (invalid check)
				status = CL_OPEN;

            if (term->GetSettings()->drawer_mode == DRAWER_SERVER)
                term->server = nullptr;
		}
		MakeList(term);
	}
    if (array_max_size > 0)
	    max_pages = ((possible_size - 1) / array_max_size) + 1;
    else
        {max_pages = 1;}

		int col = color[0];
	// Header
		char str[128];
		char str2[128];

	if (term->server)
		snprintf(str2, STRLENGTH, "%s's", term->server->system_name.Value());
	else
		strcpy(str2, "System");

	if (status != CL_ALL)
		snprintf(str, STRLENGTH, "%s %s Checks", str2, term->Translate(CLName[status]));
	else
		snprintf(str, STRLENGTH, "%s Checks", str2);
	TextC(term, 1, str, col);

	if (term->archive == nullptr)
	{
		if ((term->server == nullptr && e->training) ||
            (term->server && term->server->training))
			strcpy(str, term->Translate("Current Training Checks"));
		else
			strcpy(str, term->Translate("Current Checks"));
	}
	else
	{
		if (term->archive->fore)
			term->TimeDate(str2, term->archive->fore->end_time, TD5);
		else
			strcpy(str2, term->Translate("System Start"));
		if (term->archive->start_time.IsSet())
			strcpy(str2, term->TimeDate(term->archive->start_time, TD5));
		else
			strcpy(str2, term->Translate("System Start"));
		snprintf(str, STRLENGTH, "%s  to  %s", str2, term->TimeDate(term->archive->end_time, TD5));
	}
	TextC(term, 0, str, COLOR_BLUE);

	TextPosL(term,            0, 2.2, "Table", col);
	TextPosL(term, size_x *  .2, 2.2, "#Gst", col);
	TextPosC(term, size_x * .56, 2.2, "Time", col);
	TextPosL(term, size_x *  .8, 2.2, "Status", col);
	Flt x0 = size_x * .02, x1 = size_x * .22;
	Flt x2 = size_x * .56, x3 = size_x * .80;

	// Footer
	if (possible_size > 0)
	{
		snprintf(str, STRLENGTH, "%s: %d", term->Translate("Number of checks"), possible_size);
		TextC(term, size_y - 3, str, col);
	}
	else
		snprintf(str, STRLENGTH, "No %s checks", term->Translate(CLName[status]));
	// TextC(term, line, str, COLOR_RED);  // line not defined yet

	if (max_pages > 1)
		TextL(term, size_y - 1, term->PageNo(page_no + 1, max_pages), col);

	Flt line = HEADER_SIZE;
	if (array_size <= 0)
	{
		if (status == CL_ALL)
			strcpy(str, term->Translate("No checks of any kind"));
		else
			snprintf(str, STRLENGTH, "No %s checks", term->Translate(CLName[status]));
		TextC(term, line, str, COLOR_RED);
	}

	for (int i = 0; i < array_size; ++i)
	{
		int tc = COLOR_BLACK;
		Check *c = check_array[i];
		if (c->user_current > 0)
		{
			if (c->user_current == e->id)
			{
				Background(term, line - ((spacing - 1)/2), spacing, IMAGE_LIT_SAND);
				tc = COLOR_GRAY;
			}
			else
				tc = COLOR_PURPLE;
		}

        switch (c->CustomerType())
        {
        case CHECK_CATERING:
            strcpy(str, term->Translate("CATR"));
            break;
        case CHECK_DELIVERY:
            strcpy(str, term->Translate("DLVR"));
            break;
        case CHECK_TAKEOUT:
            strcpy(str, term->Translate("Take Out"));
            break;
        default:
            if (c->IsFastFood())
                strcpy(str, term->Translate("Fast Food"));
            else if (c->CustomerType() == CHECK_BAR)
                strcpy(str, term->Translate("Bar"));
            else
                snprintf(str, STRLENGTH, "%.4s", c->Table());
            break;
        }

		TextPosL(term, x0, line, str, tc);
		if (c->IsTakeOut() || c->IsFastFood() || c->CustomerType() == CHECK_BAR)
		{
			// for now we are using last 4 digits of phone number.
			if (c->customer) {
				const genericChar* tmp = c->customer->PhoneNumber();
				Str *tStr = new Str(tmp);
                strncpy(str, tmp += (tStr->size() - 4), 4);
				str[4] = '\0';
			}
//			sprintf(str, "%d", c->Guests());
//			str[0] = '-'; 
//			str[1] = '\0';
		}
		else
			snprintf(str, STRLENGTH, "%d", c->Guests());
		TextPosL(term, x1, line, str, tc);

		if (status == CL_OPEN || status == CL_TAKEOUT || status == CL_FASTFOOD)
			term->TimeDate(str, c->time_open, TD_TIME);
		else
		{
			TimeInfo time_close;
			time_close.Set(c->TimeClosed());
			if (time_close.IsSet())
				snprintf(str, STRLENGTH, "%s %s", term->TimeDate(str2, c->time_open, TD_TIME),
						term->TimeDate(time_close, TD_TIME));
			else
				term->TimeDate(str, c->time_open, TD_TIME);
		}
		TextPosC(term, x2, line, str, tc);
		TextPosL(term, x3, line, c->StatusString(term), tc);
		line += spacing;
	}
	Line(term, HEADER_SIZE - 1, col);
	Line(term, size_y - FOOTER_SIZE, col);
	return RENDER_OKAY;
}

SignalResult CheckListZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("CheckListZone::Signal()");
    static const genericChar* commands[] = {
        "status", "resend", nullptr};

    Employee *e = term->user;
    if (e == nullptr)
        return SIGNAL_IGNORED;

    Check *c = term->check;
    int idx = CompareList(message, commands);
    switch (idx)
    {
    case 0:  // status
        page_no = 0;
        status = NextValue(status, CLValue);
        break;
    case 1:  // resend
        if (c && c->Status() == CHECK_OPEN)
            c->FinalizeOrders(term, 1);
        break;
    default:
        if (strncmp(message, "search ", 7) == 0)
        {
            if (Search(term, &message[7], nullptr) <= 0)
                return SIGNAL_IGNORED;
        }
        else if (strncmp(message, "nextsearch ", 11) == 0)
        {
            if (Search(term, &message[11], term->server) <= 0)
                return SIGNAL_IGNORED;
        }
        else
            return SIGNAL_IGNORED;
    }

    Draw(term, 1);
    return SIGNAL_OKAY;
}

SignalResult CheckListZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("CheckListZone::Touch()");
    LayoutZone::Touch(term, tx, ty);

    if (selected_y < HEADER_SIZE)
    {
        // Pageup
        if (max_pages <= 1)
            return SIGNAL_IGNORED;

        --page_no;
        if (page_no < 0)
            page_no = max_pages - 1;
        Draw(term, 1);
        return SIGNAL_OKAY;
    }
    else if (selected_y >= (size_y - FOOTER_SIZE))
    {
        // Pagedown
        if (max_pages <= 1)
            return SIGNAL_IGNORED;
    
        ++page_no;
        if (page_no >= max_pages)
            page_no = 0;
        Draw(term, 1);
        return SIGNAL_OKAY;
    }

    int line = (int) ((selected_y - HEADER_SIZE + ((spacing-1)/2)) / spacing);
    if (line >= array_size || line < 0)
        return SIGNAL_IGNORED;

    if (check_array[line] == term->check)
        term->StoreCheck();
    else
        term->SetCheck(check_array[line]);
    return SIGNAL_OKAY;
}

int CheckListZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    FnTrace("CheckListZone::Update()");
    int retval = 0;

    if (update_message & (UPDATE_CHECKS | UPDATE_ARCHIVE | UPDATE_SERVER))
        Draw(term, 1);

    return retval;
}

int CheckListZone::MakeList(Terminal *term)
{
    FnTrace("CheckListZone::MakeList()");
    possible_size = 0;
    array_size    = 0;

    Employee *e = term->user;
    if (e == nullptr)
        return 1;

    Employee *server = term->server;
    Archive *a = term->archive;
    Check *c = term->system_data->FirstCheck(a);
    int offset = page_no * array_max_size;
    while (c)
    {
        int okay = 0;
        if ((server == nullptr && (e->training == c->IsTraining() || a)) ||
            (server && server->training == c->IsTraining() &&
             server->id == c->user_owner))
        {
            switch (status)
			{
            case CL_ALL:
                okay = 1; 
                break;
            case CL_OPEN:
                okay = (c->Status() == CHECK_OPEN);
                break;
            case CL_TAKEOUT:
                okay = (c->Status() == CHECK_OPEN) && c->IsTakeOut(); 
                break;
            case CL_FASTFOOD:
                okay = (c->Status() == CHECK_OPEN) && c->IsFastFood(); 
                break;
            case CL_CLOSED:
                okay = (c->Status() == CHECK_CLOSED); 
                break;
            default:
                break;
			} // end switch
        }

        if (okay)
        {
            ++possible_size;
            if (offset > 0)
                --offset;
            else if (array_size <= array_max_size)
                check_array[array_size++] = c;
        }
        c = c->next;
    }
    return 0;
}

int CheckListZone::Search(Terminal *term, const genericChar* emp_name, Employee *start)
{
    FnTrace("CheckListZone::Search()");
    Employee *e = term->system_data->user_db.NameSearch(emp_name, start);
    if (e)
    {
        term->server = e;
        Draw(term, 1);
    }
    return 0;
}

/*********************************************************************
 * CheckEditZone class
 ********************************************************************/

const genericChar* CHECK_TypesChar[] = { "Take Out", "Delivery", "Catering", nullptr };
int          CHECK_TypesInt[]  = { CHECK_TAKEOUT, CHECK_DELIVERY, CHECK_CATERING, -1 };
CheckEditZone::CheckEditZone()
{
    FnTrace("CheckEditZone::CheckEditZone()");

    form_header  = 0.65;
    form_spacing = 0.65;

    list_header  = 0.65;
    list_footer  = 10;
    list_spacing = 1.0;

    lines_shown  = 5;
    page         = 1;

    check        = nullptr;
    my_update    = 1;
    report       = nullptr;
    view         = -1;  // -1 represents Show All

    AddTimeDateField("TakeOut/Delivery Date");
    AddListField("Type", CHECK_TypesChar, CHECK_TypesInt);
    AddTextField("Comment", 50);
}

CheckEditZone::~CheckEditZone()
{
}

RenderResult CheckEditZone::Render(Terminal *term, int update_flag)
{
    FnTrace("CheckEditZone::Render()");
    RenderResult retval = RENDER_OKAY;
    int fields_active = 1;
    int col = COLOR_DEFAULT;

    if (update_flag || my_update)
    {
        check = term->check;
        LoadRecord(term, 0);
        if (report != nullptr)
            free(report);
        report = nullptr;
        my_update = 0;
    }

    if (check == nullptr)
        fields_active = 0;
    FormField *field = FieldList();
    while (field != nullptr)
    {
        field->active = fields_active;
        field = field->next;
    }
    FormZone::Render(term, update_flag);

    TextC(term, 0, term->Translate(name.Value()), col);
    

    return retval;
}

/****
 * Keyboard:  We'll prevent all modifications to the check unless the check
 *  is open.
 ****/
SignalResult CheckEditZone::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("CheckEditZone::Keyboard()");
    SignalResult retval = SIGNAL_OKAY;

    if ((check != nullptr) && (check->Status() == CHECK_OPEN))
        retval = FormZone::Keyboard(term, my_key, state);
    else
        retval = SIGNAL_IGNORED;

    return retval;
}

SignalResult CheckEditZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("CheckEditZone::Touch()");
    SignalResult retval = SIGNAL_OKAY;

    if ((check != nullptr) && (check->Status() == CHECK_OPEN))
        retval = FormZone::Touch(term, tx, ty);
    else
        retval = SIGNAL_IGNORED;

    return retval;
}

SignalResult CheckEditZone::Mouse(Terminal *term, int action, int mx, int my)
{
    FnTrace("CheckEditZone::Mouse()");
    SignalResult retval = SIGNAL_OKAY;

    if ((check != nullptr) && (check->Status() == CHECK_OPEN))
        retval = FormZone::Mouse(term, action, mx, my);
    else
        retval = SIGNAL_IGNORED;

    return retval;
}

/****
 * GetNextCheck:
 ****/
Check *GetNextCheck(Check *current)
{
    FnTrace("GetNextCheck()");
    Check *retval = nullptr;

    if (current != nullptr)
        current = current->next;
    else
        current = MasterSystem->CheckList();

    while (current != nullptr && retval == nullptr)
    {
    	// Don't want to limit this to only takeouts
        // if (current->Status() == CHECK_OPEN && current->IsTakeOut())
        if (current->Status() == CHECK_OPEN)
            retval = current;
        current = current->next;
    }

    return retval;
}

/****
 * GetPriorCheck:
 ****/
Check *GetPriorCheck(Check *current)
{
    FnTrace("GetPriorCheck()");
    Check *retval = nullptr;

    if (current != nullptr)
        current = current->fore;
    else
        current = MasterSystem->CheckListEnd();

    while (current != nullptr && retval == nullptr)
    {
    	// Don't want to limit this to only takeouts
        // if (current->Status() == CHECK_OPEN && current->IsTakeOut())
        if (current->Status() == CHECK_OPEN)
            retval = current;
        current = current->fore;
    }

    return retval;
}

SignalResult CheckEditZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("CheckEditZone::Signal()");
    SignalResult retval = SIGNAL_OKAY;
    static const genericChar* commands[] = { "next", "prior", "change view", "search",
                                       "nextsearch", "save", nullptr};
    int idx = CompareListN(commands, message);
    int draw = 0;

    switch (idx)
    {
    case 0:  //next
        if (check != nullptr)
            SaveRecord(term, 0, 1);
        term->check = GetNextCheck(term->check);
        if (term->check != nullptr && term->check->customer != nullptr)
            term->customer = term->check->customer;
        else
            term->customer = nullptr;
        draw = 2;
        check = nullptr;
        break;
    case 1:  //prior
        if (check != nullptr)
            SaveRecord(term, 0, 1);
        term->check = GetPriorCheck(term->check);
        if (term->check != nullptr && term->check->customer != nullptr)
            term->customer = term->check->customer;
        else
            term->customer = nullptr;
        draw = 2;
        check = nullptr;
        break;
    case 2:  //change view
        break;
    case 3:  //search
        if (Search(term, -1, &message[7]) <= 0)
            retval = SIGNAL_IGNORED;
        else
            draw = 1;
        break;
    case 4:  // nextsearch
        if (Search(term, record_no, &message[11]) <= 0)
            retval = SIGNAL_IGNORED;
        else
            draw = 1;
        break;
    case 5:  // save
        // We also need to save the customer info.  We'll assume CustomerInfoZone
        // is at group 1, but in the future this should not be necessary.
        //NOTE->BAK add button type similar to BEHAVE_MISS but activates the
        //button as well as passing the command through.  This could get tricky
        //because two stacked return buttons could cause problems.  So perhaps
        //certain conditions break the pass-through chain.
        term->Signal("save", 1);
        retval = FormZone::Signal(term, message);
        break;
    default:
        retval = FormZone::Signal(term, message);
        break;
    }

    if (draw > 0)
    {
        my_update = 1;
        term->Draw(draw - 1);
    }

    return retval;
}

int CheckEditZone::LoseFocus(Terminal *term, Zone *newfocus)
{
    FnTrace("CheckEditZone::LoseFocus()");
    int retval = 0;

    keyboard_focus = nullptr;
    Draw(term, 0);

    return retval;
}

int CheckEditZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("CheckEditZone::LoadRecord()");
    int retval = 0;
    FormField *fields = FieldList();

    // this prevented the user being able to add customer information to a check that was not
    // a takeout order. Some customers want to do name/number all the time.
    // if (check != nullptr && check->Status() == CHECK_OPEN && check->IsTakeOut())
    if (check != nullptr && check->Status() == CHECK_OPEN)
    {
        fields->Set(check->Date());
        fields = fields->next;

        fields->Set(check->CustomerType());
        fields = fields->next;

        fields->Set(check->Comment());
    }
    else if (term->check != nullptr)
    {
        // We have a non-takeout check.  Clear it and redraw everything.  We have
        // to redraw because there are likely other check displays (check report)
        // on the screen and we don't know if they've already been drawn.  To
        // keep everything synchronized, we make sure all zones displays the
        // term->check = nullptr situation.
        term->check = nullptr;
        term->Draw(1);
    }

    return retval;
}

int CheckEditZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("CheckEditZone::SaveRecord()");
    int retval = 0;
    FormField *fields = FieldList();
    TimeInfo date;
    int type;
    genericChar buffer[STRLONG];

    // Verify we only save open takeout checks (no tables)
    if ((check != nullptr) && (check->Status() == CHECK_OPEN) && check->IsTakeOut())
    {
        if (term->customer != nullptr)
            term->customer->Save();

        fields->Get(date);
        check->Date(&date);
        fields = fields->next;

        fields->Get(type);
        check->CustomerType(type);
        fields = fields->next;

        fields->Get(buffer);
        check->Comment(buffer);

        if (check->customer == nullptr || check->customer->IsBlank() || !term->customer->IsBlank())
            check->customer = term->customer;
        if (check->customer != nullptr)
            check->customer_id = check->customer->CustomerID();
        else
            check->customer_id = -1;

        check->Save();
    }

    return retval;
}

int CheckEditZone::Search(Terminal *term, int record, const genericChar* word)
{
    FnTrace("CheckEditZone::Search()");
    int retval = 0;

    return retval;
}

int CheckEditZone::ListReport(Terminal *term, Report *lreport)
{
    FnTrace("CheckEditZone::ListReport()");
    int retval = 0;

    return retval;
}

int CheckEditZone::RecordCount(Terminal *term)
{
    FnTrace("CheckEditZone::RecordCount()");
    int retval = 0;

    if (check != nullptr)
        retval = 1;

    return retval;
}
