/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025
  
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
 * table_zone.cc - revision 76 (10/13/98)
 * implementation of table_zone module
 */

#include "table_zone.hh"
#include "terminal.hh"
#include "check.hh"
#include "employee.hh"
#include "system.hh"
#include "labor.hh"
#include "exception.hh"
#include "drawer.hh"
#include "dialog_zone.hh"
#include "manager.hh"
#include "image_data.hh"
#include "customer.hh"
#include "zone_object.hh"
#include "form_zone.hh"
#include "safe_string_utils.hh"
#include <string.h>

#include <cctype>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/*********************************************************************
 * RoomDialog class
 ********************************************************************/
class RoomDialog : public FormZone
{
    FormField *check_in, *check_out;
    ZoneObjectList button_list;
    ZoneObject *cancel, *update, *order;
    ZoneObject *checkout;

public:
    // Constructor
    RoomDialog();
    // Destructor
    ~RoomDialog();

    // Member Functions
    int          RenderInit(Terminal *term, int update_flag);
    RenderResult Render(Terminal *term, int update_flag);
    SignalResult Signal(Terminal *term, const genericChar* message);
    SignalResult Touch(Terminal *term, int tx, int ty);

    int LoadRecord(Terminal *term, int record);
    int SaveRecord(Terminal *term, int record, int write_file);
    int UpdateForm(Terminal *term, int record);

    int ParseSwipe(Terminal *term, const genericChar* swipe);
};

// Constructor
RoomDialog::RoomDialog()
{
    FnTrace("RoomDialog::RoomDialog()");

    font = FONT_TIMES_24;
    AddTextField(GlobalTranslate("Last Name"), 32);
    AddTextField(GlobalTranslate("First Name"), 32);
    AddTextField(GlobalTranslate("Company"), 32);
    AddNewLine(1);
    AddTextField(GlobalTranslate("Address"), 48);
    AddTextField(GlobalTranslate("City"), 24);
    AddTextField(GlobalTranslate("State"), 3); SetFlag(FF_ALLCAPS);
    AddTextField(GlobalTranslate("Zip"), 10);
    AddNewLine(1);
    AddTextField(GlobalTranslate("Phone"), 14); //SetFlag(FF_ONLYDIGITS);
    AddTextField("Driver's License", 12);
    AddTextField(GlobalTranslate("License Plate"), 12); SetFlag(FF_ALLCAPS);
    AddTextField("Credit Card #", 18); SetFlag(FF_ONLYDIGITS);
    AddTemplateField("Expires (M/Y)", "__/____"); SetFlag(FF_ONLYDIGITS);
    AddTextField(GlobalTranslate("Guest Count"), 3); SetFlag(FF_ONLYDIGITS);
    AddDateField(GlobalTranslate("Check In"));
    check_in = FieldListEnd();
    AddDateField(GlobalTranslate("Check Out"));
    check_out = FieldListEnd();

    cancel = new ButtonObj(GlobalTranslate("Cancel"));
    button_list.Add(cancel);

    update = new ButtonObj(GlobalTranslate("Update"));
    button_list.Add(update);

    order = new ButtonObj(GlobalTranslate("Order"));
    button_list.Add(order);

    checkout = new ButtonObj(GlobalTranslate("Checkout Guest"));
}

// Destructor
RoomDialog::~RoomDialog()
{
    if (checkout)
        delete checkout;
}

// Member Functions
int RoomDialog::RenderInit(Terminal */*term*/, int /*update_flag*/)
{
    FnTrace("RoomDialog::RenderInit()");
    w = 800;
    h = 600;
    return 0;
}

RenderResult RoomDialog::Render(Terminal *term, int update_flag)
{
    FnTrace("RoomDialog::Render()");
    FormZone::Render(term, update_flag);

    int total = 0;
    int balance = 0;
    genericChar str[256];
    int no = 0;
    Check *check = term->check;
    if (check)
    {
        for (SubCheck *sc = check->SubList(); sc != NULL; sc = sc->next)
        {
            total   += sc->total_cost + sc->payment;
            balance += sc->balance;
        }
        vt_safe_string::safe_format(str, 256, "%s %s", term->Translate("Room"), check->Table());
        no = check->serial_number;
    }
    else
        vt_safe_string::safe_format(str, 256, "%s ???", term->Translate("Room"));

    if (no <= 0)
        TextC(term, 0, str);
    else
    {
        TextL(term, 0, str);
        vt_safe_string::safe_format(str, 256, "%s %06d", term->Translate("Folio No"), no);
        TextR(term, 0, str);
    }

    (void)checkout->SetRegion(x + w - border - 140, y + border + 46, 140, 110);  // Suppress nodiscard warning

    button_list.LayoutColumns(term, x + border, y + h - 80 - border,
                              w - (border * 2), 80, 4);
    button_list.Render(term);
    checkout->Render(term);

    if (total > 0)
    {
        if (balance == 0)
            vt_safe_string::safe_format(str, 256, "Account Total:  %s      Account Paid",
                    term->FormatPrice(total, 1));
        else
            vt_safe_string::safe_format(str, 256, "Account Total:  %s      Unpaid Balance:  %s",
                    term->FormatPrice(total, 1), term->FormatPrice(balance, 1));
        TextC(term, max_size_y - 4.5, str);
    }
    return RENDER_OKAY;
}

SignalResult RoomDialog::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("RoomDialog::Touch()");
    Check *check = term->check;
    if (check == NULL)
        return SIGNAL_TERMINATE;

    if (checkout->IsPointIn(tx, ty))
    {
        checkout->Draw(term, 1);
        if (check->Close(term))
        {
            // close failed
            checkout->Draw(term, 0);
            return SIGNAL_IGNORED;
        }

        term->StoreCheck();
        return SIGNAL_TERMINATE;
    }

    ZoneObject *zoneObj = button_list.Find(tx, ty);
    if (zoneObj)
	{
		zoneObj->Draw(term, 1);
		if (zoneObj == update)
		{
			SaveRecord(term, 0, 0);
			term->StoreCheck();
		}
		else if (zoneObj == order)
		{
			SaveRecord(term, 0, 0);
			if (check->Guests() <= 0)
				check->Guests(1);
			term->JumpToIndex(INDEX_ROOM);
		}
		else if (zoneObj == cancel)
			term->StoreCheck();
		return SIGNAL_TERMINATE;
	}

    return FormZone::Touch(term, tx, ty);
}

SignalResult RoomDialog::Signal(Terminal *term, const genericChar* message)
{
    if (StringCompare(message, "swipe ", 6) == 0)
    {
        const genericChar* swipe = &message[6];
        if (ParseSwipe(term, swipe) == 0)
            Draw(term, 0);
        return SIGNAL_OKAY;
    }
  
    return FormZone::Signal(term, message);
}

int RoomDialog::LoadRecord(Terminal *term, int /*record*/)
{
    FnTrace("RoomDialog::LoadRecord()");
    Check *check = term->check;
    if (check == NULL)
        return 0;

    FormField *f = FieldList();
    f->Set(check->LastName()); f = f->next;
    f->Set(check->FirstName()); f = f->next;
    f->Set(check->Company()); f = f->next;
    f->Set(check->Address()); f = f->next;
    f->Set(check->City()); f = f->next;
    f->Set(check->State()); f = f->next;
    f->Set(check->Postal()); f = f->next;
    f->Set(check->PhoneNumber()); f = f->next;
    f->Set(check->License()); f = f->next;
    f->Set(check->Vehicle()); f = f->next;
    f->Set(check->CCNumber()); f = f->next;
    f->Set(check->CCExpire()); f = f->next;
    if (check->Guests() <= 0)
        f->Set("");
    else
        f->Set(check->Guests());
    f = f->next;
    f->Set(check->CheckIn()); f = f->next;
    f->Set(check->CheckOut()); f = f->next;
    return 0;
}

int RoomDialog::SaveRecord(Terminal *term, int /*record*/, int /*write_file*/)
{
    FnTrace("RoomDialog::SaveRecord()");
    Check *check = term->check;
    if (check == NULL)
        return 0;

    Str tmp;

    FormField *f = FieldList();
    f->Get(tmp); f = f->next; check->LastName(tmp.Value());
    f->Get(tmp); f = f->next; check->FirstName(tmp.Value());
    f->Get(tmp); f = f->next; check->Company(tmp.Value());
    f->Get(tmp); f = f->next; check->Address(tmp.Value());
    f->Get(tmp); f = f->next; check->City(tmp.Value());
    f->Get(tmp); f = f->next; check->State(tmp.Value());
    f->Get(tmp); f = f->next; check->Postal(tmp.Value());
    f->Get(tmp); f = f->next; check->PhoneNumber(tmp.Value());
    f->Get(tmp); f = f->next; check->License(tmp.Value());
    f->Get(tmp); f = f->next; check->Vehicle(tmp.Value());
    f->Get(tmp); f = f->next; check->CCNumber(tmp.Value());
    f->Get(tmp); f = f->next; check->CCExpire(tmp.Value());
    f->Get(tmp); f = f->next; check->Guests(tmp.IntValue());

    TimeInfo timevar;
    f->Get(timevar); f = f->next; check->CheckIn(&timevar);
    f->Get(timevar); f = f->next; check->CheckOut(&timevar);

    if (check->Guests() < 0)
        check->Guests(0);
    return 0;
}

int RoomDialog::UpdateForm(Terminal */*term*/, int /*record*/)
{
    TimeInfo start, end;
    check_in->Get(start);
    check_out->Get(end);
    int status = 1;

    if (end < start || end == start)
    {
        start += date::days(1);
        check_out->Set(start);
        status = 0;
    }
    return status;
}

int RoomDialog::ParseSwipe(Terminal */*term*/, const genericChar* value)
{
    genericChar tmp[256];
    const genericChar* s = value;
    genericChar* d;
    int i;

    d = tmp;
    while (*s != '^' && *s != '\0')
        *d++ = *s++;
    *d = '\0';

    genericChar number[256];
    vt_safe_string::safe_copy(number, 256, tmp);

    if (*s == '\0')
        return 1;
    ++s;

    d = tmp;
    while (*s != '^' && *s != '\0')
        *d++ = *s++;
    *d = '\0';
    --d;

    genericChar first[256], last[256];
    genericChar* fp = first, *lp = last;

    int flag = 0;
    d = tmp;
    while (*d)
    {
        if (*d == '/')
            flag = 1;
        else if (flag)
            *fp++ = *d;
        else
            *lp++ = *d;
        ++d;
    }
    *fp = '\0';
    *lp = '\0';

    AdjustCaseAndSpacing(first);
    AdjustCaseAndSpacing(last);

    if (*s == '\0')
        return 1;
    ++s;
    for (i = 0; i < 4; ++i)
    {
        if (*s == '\0')
            return 1;
        else
            tmp[i] = *s++;
    }
    tmp[4] = '\0';

    genericChar expire[256];
    vt_safe_string::safe_copy(expire, 256, tmp);
  
    tmp[0] = expire[0];
    tmp[1] = expire[1];
    tmp[2] = '\0';
    int year = atoi(tmp);

    // Y2K fix for credit year expire
    if (year >= 70)
        year += 1900;
    else
        year += 2000;

    tmp[0] = expire[2];
    tmp[1] = expire[3];
    tmp[2] = '\0';
    int month = atoi(tmp);

    vt_safe_string::safe_format(expire, 256, "%02d%04d", month, year);

    //sprintf(tmp, "expire: '%s'", expire);
    //ReportError(tmp);

    // Load Fields
    FormField *f = FieldList();
    f->Set(last); f = f->next;
    f->Set(first); f = f->next;
    f = f->next;
    f = f->next;
    f = f->next;
    f = f->next;
    f = f->next;
    f = f->next;
    f = f->next;
    f->Set(number); f = f->next;
    f->Set(expire); f = f->next;
    return 0;
}


/*********************************************************************
 * CustomerInfoZone Class
 ********************************************************************/
CustomerInfoZone::CustomerInfoZone()
{
    font        = FONT_TIMES_24;
    customer    = NULL;
    my_update   = 1;
    form_header = 0.65;

    AddTextField(GlobalTranslate("First Name"), 32);
    AddNewLine(1);
    AddTextField(GlobalTranslate("Last Name"), 32);
    AddNewLine(1);
    AddTextField(GlobalTranslate("Company"), 32);
    AddNewLine(1);
    AddTextField(GlobalTranslate("Address"), 48);
    AddNewLine(1);
    AddTextField(GlobalTranslate("City"), 24);
    AddTextField(GlobalTranslate("State"), 3); SetFlag(FF_ALLCAPS);
    AddTextField(GlobalTranslate("Zip"), 5);
    AddNewLine(1);
    AddTextField(GlobalTranslate("Phone"), 14);
    AddTextField("Driver's License", 12);
    AddNewLine(1);
    AddTextField("Credit Card #", 18);
    AddTemplateField("Expires (M/Y)", "__/____"); SetFlag(FF_ONLYDIGITS);
    AddTextField(GlobalTranslate("Comment"), 50);
}

CustomerInfoZone::~CustomerInfoZone()
{
}

int CustomerInfoZone::RenderInit(Terminal *term, int update_flag)
{
    FnTrace("CustomerInfoZone::RenderInit()");
    int retval = 0;

    FormZone::RenderInit(term, update_flag);
    return retval;
}

RenderResult CustomerInfoZone::Render(Terminal *term, int update_flag)
{
    FnTrace("CustomerInfoZone::Render()");
    RenderResult retval = RENDER_OKAY;
    int col = COLOR_DEFAULT;
    int fields_active = 1;

    if (customer != term->customer)
        my_update = 1;

    if (update_flag || my_update)
    {
        if (customer != NULL)
            customer->Save();
        if (term->customer == NULL)
            customer = NULL;
        else
            customer = term->customer;
        LoadRecord(term, 0);
        my_update = 0;
    }

    if (customer == NULL)
        fields_active = 0;
    FormField *field = FieldList();
    while (field != NULL)
    {
        field->active = static_cast<short>(fields_active);
        field = field->next;
    }

    FormZone::Render(term, update_flag);
    TextC(term, 0, term->Translate(name.Value()), col);

    if (customer == NULL)
    {
        TextL(term, 3, term->Translate("No customer available"), col);
    }

    return retval;
}

SignalResult CustomerInfoZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("CustomerInfoZone::Signal()");
    SignalResult retval = SIGNAL_OKAY;
    static const genericChar* commands[] = {
        "next", "prior", "search", "nextsearch ", "new", NULL};
    int idx = CompareListN(commands, message);
    int draw = 0;
    int customer_type = CHECK_TAKEOUT;  // for new records
    if (term && term->check)
       customer_type = term->check->CustomerType();

    switch (idx)
    {
    case 0:  //next
        if (customer != NULL && customer->next != NULL)
        {
            if (customer != NULL)
                SaveRecord(term, 0, 1);
            customer = customer->next;
            if (term != NULL)
                term->customer = customer;
            draw = 1;
        }
        break;
    case 1:  //prior
        if (customer != NULL && customer->fore != NULL)
        {
            if (customer != NULL)
                SaveRecord(term, 0, 1);
            customer = customer->fore;
            if (term != NULL)
                term->customer = customer;
            draw = 1;
        }
        break;
    case 2:  // search
        if (Search(term, -1, &message[7]) <= 0)
            retval = SIGNAL_IGNORED;
        else
            draw = 1;
        break;
    case 3:  // nextsearch
        if (Search(term, record_no, &message[11]) <= 0)
            retval = SIGNAL_IGNORED;
        else
            draw = 1;
        break;
    case 4:  // new
        if (customer != NULL)
            SaveRecord(term, 0, 1);
        if (term != NULL && term->check != NULL)
            customer_type = term->check->CustomerType();
        if (term != NULL)
            term->customer = NewCustomerInfo(customer_type);
        keyboard_focus = FieldList();
        draw = 1;
        break;
    default:
        retval = FormZone::Signal(term, message);
        break;
    }

    if (draw && term != NULL)
    {
        // Normally we'd only draw our own zone, but we changed the current customer
        // term knows about, so other zones may be affected.
        my_update = 1;
        term->Draw(0);
    }

    return retval;
}

SignalResult CustomerInfoZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("CustomerInfoZone::Touch()");
    SignalResult retval = SIGNAL_OKAY;

    retval = FormZone::Touch(term, tx, ty);
    return retval;
}

int CustomerInfoZone::LoseFocus(Terminal *term, Zone */*newfocus*/)
{
    FnTrace("CustomerInfoZone::LoseFocus()");
    int retval = 0;

    keyboard_focus = NULL;
    Draw(term, 0);

    return retval;
}

int CustomerInfoZone::LoadRecord(Terminal */*term*/, int /*record*/)
{
    FnTrace("CustomerInfoZone::LoadRecord()");
    int retval = 0;
    FormField *fields = FieldList();
    const genericChar* buffer;

    if (customer != NULL)
    {
        buffer = customer->FirstName();
        fields->Set(buffer);
        fields = fields->next;

        buffer = customer->LastName();
        fields->Set(buffer);
        fields = fields->next;

        buffer = customer->Company();
        fields->Set(buffer);
        fields = fields->next;

        buffer = customer->Address();
        fields->Set(buffer);
        fields = fields->next;

        buffer = customer->City();
        fields->Set(buffer);
        fields = fields->next;

        buffer = customer->State();
        fields->Set(buffer);
        fields = fields->next;

        buffer = customer->Postal();
        fields->Set(buffer);
        fields = fields->next;

        buffer = customer->PhoneNumber();
        fields->Set(buffer);
        fields = fields->next;

        buffer = customer->License();
        fields->Set(buffer);
        fields = fields->next;

        buffer = customer->CCNumber();
        fields->Set(buffer);
        fields = fields->next;

        buffer = customer->CCExpire();
        fields->Set(buffer);
        fields = fields->next;

        buffer = customer->Comment();
        fields->Set(buffer);
        fields = fields->next;
    }

    return retval;
}

int CustomerInfoZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("CustomerInfoZone::SaveRecord()");
    int retval = 0;
    FormField *fields = FieldList();
    genericChar buffer[STRLONG];
    
    if (customer != NULL)
    {
        fields->Get(buffer);
        customer->FirstName(buffer);
        fields = fields->next;

        fields->Get(buffer);
        customer->LastName(buffer);
        fields = fields->next;

        fields->Get(buffer);
        customer->Company(buffer);
        fields = fields->next;

        fields->Get(buffer);
        customer->Address(buffer);
        fields = fields->next;

        fields->Get(buffer);
        customer->City(buffer);
        fields = fields->next;

        fields->Get(buffer);
        customer->State(buffer);
        fields = fields->next;

        fields->Get(buffer);
        customer->Postal(buffer);
        fields = fields->next;

        fields->Get(buffer);
        customer->PhoneNumber(buffer);
        fields = fields->next;

        fields->Get(buffer);
        customer->License(buffer);
        fields = fields->next;

        fields->Get(buffer);
        customer->CCNumber(buffer);
        fields = fields->next;

        fields->Get(buffer);
        customer->CCExpire(buffer);
        fields = fields->next;

        fields->Get(buffer);
        customer->Comment(buffer);
        /* fields = fields->next; */  // fields is not used after this, dead store removed

        customer->Save();
    }

    return retval;
}

int CustomerInfoZone::UpdateForm(Terminal *term, int record)
{
    FnTrace("CustomerInfoZone::UpdateForm()");
    int retval = 0;

    return retval;
}

int CustomerInfoZone::RecordCount(Terminal *term)
{
    FnTrace("CustomerInfoZone::RecordCount()");
    int retval = 0;

    retval = term->system_data->customer_db.Count();

    // CustomerInfoDB will only count records if they are not empty.
    // However, the current record may be the very first record, and it
    // may still be empty.  So we'll make sure the current record still
    // counts even if no data has yet been entered.
    if (retval < 1 && term->customer != NULL)
        retval = 1;

    return retval;
}

int CustomerInfoZone::Search(Terminal *term, int record, const genericChar* word)
{
    FnTrace("CustomerInfoZone::Search()");

    int retval = 1;
    CustomerInfo *found = NULL;

    if (record > -1)
    {
        if (customer != NULL)
            record = customer->CustomerID();
        else
            record = -1;
    }

    found = (term != NULL && term->system_data != NULL) ? term->system_data->customer_db.FindByString(word, record) : NULL;
    if (found != NULL && term != NULL)
        term->customer = found;
    else if (term != NULL && term->system_data != NULL)
        term->customer = term->system_data->customer_db.FindBlank();

    return retval;
}


/*********************************************************************
 * CommandZone Class
 ********************************************************************/
static const genericChar* ManagerStr = "Manager's Gateway";

CommandZone::CommandZone()
{
    buffer[0] = '\0';
}

// Member Functions
RenderResult CommandZone::Render(Terminal *term, int update_flag)
{
    FnTrace("CommandZone::Render()");
    if (update_flag == RENDER_NEW)
        buffer[0] = '\0';

    LayoutZone::Render(term, update_flag);
    Employee *employee = term->user;
    if (employee == NULL)
        return RENDER_OKAY;

    Settings *settings = term->GetSettings();
    Check *check = term->check;
    Drawer *drawer = term->FindDrawer();
    int col = color[0];
    genericChar str[256];

    if (check && term->move_check)
    {
        const genericChar* temp_str;
        if (check->CustomerType() == CHECK_HOTEL)
            temp_str = term->Translate("Select Target Room");
        else
            temp_str = term->Translate("Select Target Table");
        TextC(term, .3, temp_str, col);
    }
    else
    {
        vt_safe_string::safe_format(str, 256, "%s %s", term->Translate("Hello"), employee->system_name.Value());
        TextC(term, .3, str, col);
    }

    if (buffer[0])
    {
        vt_safe_string::safe_format(str, 256, "%s_", buffer);
        TextC(term, 1.3, str);
    }
    else if (check)
    {
        if (check->IsTakeOut())
            vt_safe_string::safe_copy(str, 256, GlobalTranslate("Takeout Order Selected"));
        else if (check->IsFastFood())
            vt_safe_string::safe_copy(str, 256, GlobalTranslate("Fast Food Order Selected"));
		else
            vt_safe_string::safe_format(str, 256, "Table Selected: %s", check->Table());

        TextC(term, 1.3, str);
    }

    if (!term->move_check && drawer && check)
    {
        if (drawer->IsServerBank())
            vt_safe_string::safe_copy(str, 256, GlobalTranslate("You May Settle Here"));
        else
            vt_safe_string::safe_format(str, 256, "Drawer Available: #%d", drawer->number);
        TextC(term, 2.3, str, col);
    }
    else if (employee->IsSupervisor(settings))
        term->RenderText(ManagerStr, x + (w/2), y + h - 12 - border,
                         col, FONT_TIMES_14B, ALIGN_CENTER);
    return RENDER_OKAY;
}

SignalResult CommandZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("CommandZone::Signal()");
    static const genericChar* commands[] = {
        "takeout", "printreceipt", "stack", "move", "more tables",
        "tablenext", "tableprior", "faststart", NULL};

    int idx = CompareList(message, commands);

    if (idx < 0)
        return SIGNAL_IGNORED;

    Check *check = term->check;
    switch (idx)
	{
    case 0:  // Create Take Out order
        TakeOut(term);
        return SIGNAL_OKAY;

    case 1:  // printreceipt
        if (check)
        {
            Printer *p = term->FindPrinter(PRINTER_RECEIPT);
            for (SubCheck *sc = check->SubList(); sc != NULL; sc = sc->next)
            {
                if (sc->status == CHECK_OPEN)
                    sc->PrintReceipt(term, check, p);
            }
            return SIGNAL_OKAY;
        }
        break;

    case 2:  // Stack
        term->StackCheck(CHECK_RESTAURANT);
        return SIGNAL_OKAY;

    case 3:  // Move
        if (check)
        {
            term->move_check ^= 1;
            term->Update(UPDATE_ALL_TABLES, NULL);
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        break;

    case 4:  // Next Table
    case 5:  // tablenext
        term->NextTablePage();
        return SIGNAL_OKAY;

    case 6:  // tableprior
        term->PriorTablePage();
        return SIGNAL_OKAY;

    case 7: //fastserve
        FastFood(term);
        return SIGNAL_OKAY;
	}
    return SIGNAL_IGNORED;
}

SignalResult CommandZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("CommandZone::Touch()");
    Employee *e = term->user;
    Settings *s = term->GetSettings();
    if (e == NULL || !e->IsSupervisor(s))
        return SIGNAL_IGNORED;

    term->Jump(JUMP_NORMAL, PAGEID_MANAGER);

    return SIGNAL_OKAY;
}

SignalResult CommandZone::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("CommandZone::Keyboard()");
    Check *check = term->check;
    int len = strlen(buffer);

    switch (my_key)
    {
    case 12: // cursor_left
        term->PriorTablePage();
        return SIGNAL_OKAY;
    case 17: // cursor_right
        term->NextTablePage();
        return SIGNAL_OKAY;
    case 21: // cursor_up
        break;
    case 4:  // cursor_down
        break;
    case 8:  // backspace
        if (len > 0)
        {
            buffer[len - 1] = '\0';
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        break;
    case 13: // enter
    {
        Zone *z = NULL;
        if (check && len <= 0)
            z = FindTableZone(term, check->Table());
        else
        {
            z = FindTableZone(term, buffer);
            buffer[0] = '\0';
            Draw(term, 0);
        }
        if (z)
            z->Touch(term, 0, 0);
        return SIGNAL_OKAY;
    }
    default:
    {
        char k = (char) my_key;
        if (isprint(k) && !isspace(k) && len < (int)(sizeof(buffer) - 1) &&
            term->TextWidth(buffer, len, font) < (w - (border * 2) - 32))
        {
            buffer[len] = k;
            buffer[len + 1] = '\0';
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        break;
    }
    }
    return SIGNAL_IGNORED;
}

int CommandZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    FnTrace("CommandZone::Update()");
    if (update_message & UPDATE_TIMEOUT)
        return term->LogoutUser();
    else if (update_message & UPDATE_CHECKS)
        return Draw(term, 1);
    else
        return 0;
}

const char* CommandZone::TranslateString(Terminal *term)
{
    return ManagerStr;
}

int CommandZone::TakeOut(Terminal *term)
{
    FnTrace("CommandZone::TakeOut()");

    if (term->move_check == 0)
        return term->QuickMode(CHECK_TAKEOUT);

    Check *check = term->check;
    if (check == NULL || check->IsTakeOut())
        return 1;

    Str tbl(check->Table());
    check->Table("");  // clear table - make takeout
    term->move_check = 0;
    term->UpdateAllTerms(UPDATE_TABLE, tbl.Value());
    Draw(term, 0);

    return 0;
}

int CommandZone::FastFood(Terminal *term)
{
    FnTrace("CommandZone::FastFood()");
    if (term->move_check == 0)
        return term->QuickMode(CHECK_FASTFOOD);

    Check *check = term->check;
    if (check == NULL || check->IsFastFood())
        return 1;

    Str tbl(check->Table());
    check->Table("");  // clear table - like takeout
    term->move_check = 0;
    term->UpdateAllTerms(UPDATE_TABLE, tbl.Value());
    Draw(term, 0);

    return 0;
}

Zone *CommandZone::FindTableZone(Terminal *term, const genericChar* table)
{
    FnTrace("CommandZone::FindTableZone()");
    if (table == NULL)
        return NULL;

    int len = strlen(table);
    Page *my_page;

    // pass 1
    for (my_page = term->zone_db->PageList(); my_page != NULL; my_page = my_page->next)
    {
        if (!my_page->IsTable() || my_page->size > term->size)
            continue;

        for (Zone *z = my_page->ZoneList(); z != NULL; z = z->next)
            if (z->Type() == ZONE_TABLE &&
                StringCompare(z->name.Value(), table, len) == 0)
            {
                term->ChangePage(my_page);
                return z;
            }
    }

    // pass 2
    for (my_page = term->zone_db->PageList(); my_page != NULL; my_page = my_page->next)
    {
        if (!my_page->IsTable() || my_page->size > term->size)
            continue;

        for (Zone *z = my_page->ZoneList(); z != NULL; z = z->next){
            if (z->Type() == ZONE_TABLE &&
                StringCompare(z->name.Value(), table) == 0)
            {
				term->ChangePage(my_page);
				return z;
            }
		}
    }
    return NULL;
}


/*********************************************************************
 * TableZone Class
 ********************************************************************/
// Constructor
TableZone::TableZone()
{
    check         = NULL;
    stack_depth   = 0;
    blink         = 0;
    current       = 0;
    footer        = 12;
    customer_type = CHECK_RESTAURANT;
}

// Member Functions
std::unique_ptr<Zone> TableZone::Copy()
{
    int i;

    auto z = std::make_unique<TableZone>();
    z->SetRegion(this);
    z->name.Set(name);
    z->key      = key;
    z->behave   = behave;
    z->font     = font;
    z->shape    = shape;
    z->group_id = group_id;
    z->customer_type = customer_type;
    for (i = 0; i < 3; ++i)
    {
        z->color[i]   = color[i];
        z->image[i]   = image[i];
        z->frame[i]  = frame[i];
        z->texture[i] = texture[i];
    }
    return z;
}

RenderResult TableZone::Render(Terminal *term, int update_flag)
{
    FnTrace("TableZone::Render()");

    // Check for custom image
    if (ImagePath() && ImagePath()->size() > 0 && term->show_button_images)
    {
        // Render image as the main visual element
        term->RenderPixmap(x, y, w, h, ImagePath()->Value());

        // Render text on top of image
        int state = State(term);
        if (frame[state] != ZF_HIDDEN)
        {
            int bx = Max(border - 2, 0);
            int by = Max(border - 4, 0);
            const genericChar* text = name.Value();
            const genericChar* b = term->ReplaceSymbols(text);
            if (b)
            {
                int c = color[state];
                if (c == COLOR_PAGE_DEFAULT || c == COLOR_DEFAULT)
                    c = term->page->default_color[state];
                if (c != COLOR_CLEAR)
                    term->RenderZoneText(b, x + bx, y + by + header, w - (bx*2),
                                         h - (by*2) - header - footer, c, font);
            }
        }
        return RENDER_OKAY;
    }
    else
    {
        // Normal rendering without image
        RenderZone(term, name.Value(), update_flag);
    }

    // Continue with normal TableZone rendering logic
    Employee *employee = term->user;

    if (employee == NULL)
        return RENDER_OKAY;

    System *sys = term->system_data;
    Settings *settings = &(sys->settings);
    if (update_flag)
    {
        blink = 0;
        if (name.size() > 0)
            check = sys->FindOpenCheck(name.Value(), employee);
        else
            check = NULL;
        stack_depth = sys->NumberStacked(name.Value(), employee);
    }

    current = 0;
    if (check)
    {
        genericChar str[32];
        int subs = check->SubCount();
        if (subs > 1)
            vt_safe_string::safe_format(str, 32, "%d/%d", check->Guests(), subs);
        else
            vt_safe_string::safe_format(str, 32, "%d", check->Guests());

        int bar_color, text_color, off = 0;
        if (check->user_current > 0 && check->user_current != employee->id)
        {
            // someone elses selected check (can't touch it)
            bar_color  = COLOR_PURPLE;
            text_color = COLOR_GRAY;
        }
        else if (check->user_owner != employee->id && !employee->IsSupervisor(settings))
        {
            // someone elses check
            bar_color  = COLOR_BLACK;
            text_color = COLOR_GRAY;
        }
        else
        {
            // my check
            bar_color  = COLOR_YELLOW;
            text_color = COLOR_BLACK;
        }

        current = (check == term->check);
        if (current)
        {
            // selected check
            if (blink)
                off       = term->move_check;
            else
                bar_color = COLOR_ORANGE;
        }

        if (!off)
            term->RenderStatusBar(this, bar_color, str, text_color);
        if (stack_depth > 1)
            term->RenderText("*", x + w - border, y + border - 6, COLOR_BLUE,
                             FONT_TIMES_34, ALIGN_RIGHT);
    }
    return RENDER_OKAY;
}

SignalResult TableZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("TableZone::Signal()");
    static const genericChar* commands[] = {"mergetables", NULL};
    
    int idx = CompareList(message, commands);
    switch (idx)
    {
    case 0:  // mergetables
        if (term->move_check && term->check && check)
        {
            // Merge the moving check into the target table's check
            Check *moving_check = term->check;
            Check *target_check = check;
            Str source_table(moving_check->Table());  // Save source table name for cleanup
            
            // Combine guest counts
            int total_guests = moving_check->Guests() + target_check->Guests();
            target_check->Guests(total_guests);
            
            // Move all subchecks from moving check to target check
            while (moving_check->SubList())
            {
                SubCheck *sc = moving_check->SubList();
                moving_check->Remove(sc);
                target_check->Add(sc);
            }
            
            // Update the target check
            Settings *settings = term->GetSettings();
            target_check->Update(settings);
            
            // Clear the moving check's table assignment and guest count
            // This ensures the source table is properly cleared even if StoreCheck doesn't destroy the check
            moving_check->Table("");  // Clear table assignment
            moving_check->Guests(0);  // Clear guest count
            
            // Store and remove the now-empty moving check
            term->StoreCheck(0);
            
            // Clear move mode and update display
            term->move_check = 0;
            term->UpdateAllTerms(UPDATE_ALL_TABLES | UPDATE_CHECKS, NULL);
        }
        return SIGNAL_OKAY;
    }
    return SIGNAL_IGNORED;
}

SignalResult TableZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("TableZone::Touch()");
    Employee *e = term->user;

    if (e == NULL)
        return SIGNAL_IGNORED;

    if (term->move_check && term->check)
    {
        if (check == term->check)
        {
            // Move canceled
            term->move_check = 0;
            return SIGNAL_OKAY;
        }

        if (check)
        {
            // Target table has a check - offer to merge
            SimpleDialog *d = new SimpleDialog(term->Translate("Table already has a check.\\Merge all orders from both tables?"));
            d->Button(GlobalTranslate("Yes, Merge Tables"), "mergetables");
            d->Button("No, Cancel Move");
            d->target_zone = this;
            term->OpenDialog(d);
            return SIGNAL_OKAY;
        }

        // move check to this table & update
        term->check->Table(name.Value());
        term->move_check = 0;
        term->UpdateAllTerms(UPDATE_ALL_TABLES | UPDATE_CHECKS, NULL);
        return SIGNAL_OKAY;
    }

    Check *tmp_check = term->check;
    if (tmp_check == NULL || tmp_check != check)
    {
        if (check)
        {
            if (term->type == TERMINAL_FASTFOOD)
                term->type = TERMINAL_NORMAL;
            term->SetCheck(check);
            return SIGNAL_OKAY;
        }
        else
            term->GetCheck(name.Value(), customer_type);
    }

    if (term->check)
    {
        int ct = term->check->CustomerType();
        if (ct == CHECK_HOTEL)
        {
            RoomDialog *d = new RoomDialog();
            d->frame[0] = frame[0];
            d->texture[0] = texture[0];
            d->color[0] = color[0];
            d->LoadRecord(term, 0);
            d->FirstField();      
            term->OpenDialog(d);
        }
        else
        {
            if (term->type == TERMINAL_FASTFOOD)
                term->type = TERMINAL_NORMAL;
            term->Jump(JUMP_NORMAL, PAGEID_GUESTCOUNT);
        }
    }
    return SIGNAL_OKAY;
}

int TableZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    FnTrace("TableZone::Update()");
    Employee *e = term->user;
    if (e == NULL)
        return 0;

    System *sys = term->system_data;
    if (current && check != term->check)
    {
        Draw(term, 1);
    }
    else if ((update_message & UPDATE_BLINK) && current)
    {
        blink ^= 1;
        Draw(term, 0);
    }
    else if ((update_message & UPDATE_ALL_TABLES) ||
             ((update_message & UPDATE_TABLE) && (StringCompare(name.Value(), value) == 0)))
    {
        Check *tmp_check = sys->FindOpenCheck(name.Value(), e);
        if (tmp_check == NULL && check == NULL)
            return 0;
        check = tmp_check;
        stack_depth = sys->NumberStacked(name.Value(), e);
        Draw(term, 0);
    }
    return 0;
}


/*********************************************************************
 * GuestCountZone Class
 ********************************************************************/
#define MAX_ENTRY (int)(sizeof(GuestCountZone::buffer)-2)

// Constructor
GuestCountZone::GuestCountZone()
{
    min_guests = 0;
    okay       = 0;
}

// Member Functions
RenderResult GuestCountZone::Render(Terminal *term, int update_flag)
{
    FnTrace("GuestCountZone::Render()");
    LayoutZone::Render(term, update_flag);
    Check *check = term->check;
    if (check == NULL)
        return RENDER_OKAY;

    if (update_flag)
    {
        count      = 0;
        okay       = (check->Guests() > 9);
        min_guests = check->SeatsUsed();
        if ((update_flag == RENDER_NEW) &&
            (term->type != TERMINAL_BAR) && 
            (term->type != TERMINAL_BAR2) &&
            (term->type != TERMINAL_FASTFOOD))
        {
            count = check->Guests();
            term->guests = check->Guests();
        }
    }

    genericChar str[256];
    vt_safe_string::safe_format(str, 256, "Guest Count for Table %s", check->Table());
    TextC(term, 0, str, color[0]);
    Entry(term, 3, 2, size_x - 6);
    if (term->guests <= 0)
        vt_safe_string::safe_format(str, 256, "_");
    else
        vt_safe_string::safe_format(str, 256, "%d_", term->guests);

    TextC(term, 2, str, COLOR_WHITE);
    return RENDER_OKAY;
}

SignalResult GuestCountZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("GuestCountZone::Signal()");
    static const genericChar* commands[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "00",
        "backspace", "clear", "increase", "decrease",
        "done", "ordering", "okay", "cancel", NULL};

    Check *check = term->check;
    int idx = CompareList(message, commands);
    switch (idx)
    {
    case 10:  // double zero
        if (term->guests < 10)
            count = (term->guests * 100);
        break;
    case 11:  // backspace
        count /= 10;
        break;
    case 12:  // clear
        count = 0;
        break;
    case 13:  // increase
        if (term->guests < 999)
            count = term->guests + 1;
        break;
    case 14:  // decrease
        if (term->guests > min_guests)
            count = term->guests - 1;
        break;
    case 15:  // done
    case 16:  // ordering
        if (term->guests < min_guests)
            term->guests = min_guests;
        if (check)
        {
            check->Guests(term->guests);
            term->UpdateAllTerms(UPDATE_CHECKS | UPDATE_TABLE, check->Table());
        }
        if (idx == 15)  // done
        {
            // prepare to delete the check if there aren't any orders yet.
            if (check && check->IsEmpty())
                check->Guests(0);
            if (check && check->Guests() <= 0)
                term->StoreCheck();
            term->Jump(JUMP_RETURN);
        }
        return SIGNAL_OKAY;
    case 17:  // okay  (from dialog)
        okay = 1;
        break;
    case 18:  // cancel
        if (check)
        {
            term->guests = check->Guests();
            if (check->Guests() <= 0)
                term->StoreCheck();
        }
        term->Jump(JUMP_RETURN);
        return SIGNAL_OKAY;
    default:
        if (idx >= 0 && idx <= 9 && term->guests < 100) // max of 999
            count = (term->guests * 10) + idx;
        else
            return SIGNAL_IGNORED;
    }

    if (count > 9 && okay == 0)
    {
        SimpleDialog *d = new SimpleDialog(term->Translate("Do You Have More Than 9 Guests?"));
        d->Button(GlobalTranslate("Yes"), "okay");
        d->Button("No, that was a mistake");
        d->target_zone = this;
        term->OpenDialog(d);
        return SIGNAL_OKAY;
    }

    if (term->guests == count)
        return SIGNAL_IGNORED;

    term->guests = count;
    Draw(term, 0);
    term->Update(UPDATE_GUESTS, NULL);
    return SIGNAL_OKAY;
}

SignalResult GuestCountZone::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("GuestCoundZone::Keyboard()");
    switch (my_key)
    {
    case 13:
        return Signal(term, "done");
    case 8:
        return Signal(term, "backspace");
    case '+':
        return Signal(term, "increase");
    case '-':
        return Signal(term, "decrease");
    }

    genericChar str[] = {(char) my_key, '\0'};
    return Signal(term, str);
}

int GuestCountZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    FnTrace("GuestCountZone::Update()");
    if (update_message & UPDATE_TIMEOUT)
        term->LogoutUser();
    return 0;
}


/*********************************************************************
 * TableObj class
 ********************************************************************/
class TableObj : public ZoneObject
{
public:
    Check *check;
    int selected;

    // Constructor
    TableObj(Check *thisCheck);

    // Member Functions
    int Render(Terminal *term);
    int Draw(Terminal *term);
};

// Constructor
TableObj::TableObj(Check *thisCheck)
{
    check    = thisCheck;
    selected = 0;
}

// Member Functions
int TableObj::Render(Terminal *term)
{
    FnTrace("TableObj::Render()");

    if (w <= 0 || h <= 0)
        return 1;

    int col;
    if (selected)
    {
        term->RenderButton(x, y, w, h, ZF_RAISED, IMAGE_LIT_SAND);
        col = COLOR_BLACK;
    }
    else
    {
        term->RenderButton(x, y, w, h, ZF_RAISED, IMAGE_WOOD);
        col = COLOR_WHITE;
    }

    int xx = x + 2, ww = w - 4;
    if (check->IsTakeOut() || check->IsFastFood())
	{
        term->RenderZoneText(GlobalTranslate("To Go"), xx, y, ww, h, col, FONT_TIMES_24B);
	}
    else
        term->RenderZoneText(check->Table(), xx, y, ww, h, col, FONT_TIMES_24B);
    return 0;
}

int TableObj::Draw(Terminal *term)
{
    FnTrace("TableObj::Draw()");
    Render(term);
    term->UpdateArea(x, y, w, h);
    return 0;
}


/*********************************************************************
 * ServerTableObj Class
 ********************************************************************/
class ServerTableObj : public ZoneObject
{
public:
    ZoneObjectList tables;
    Employee *user;

    // Constructor
    ServerTableObj(Terminal *term, Employee *e);

    // Member Functions
    int Render(Terminal *term);
    int Layout(Terminal *term, int lx, int ly, int lw, int lh);
};

// Constructor
ServerTableObj::ServerTableObj(Terminal *term, Employee *e)
{
    FnTrace("ServerTableObj::ServerTableObj()");
    user = e;
    Check *check = term->system_data->CheckList();
    while (check)
    {
        if (check->IsTraining() == e->training && check->GetStatus() == CHECK_OPEN &&
            check->user_owner == e->id)
            tables.Add(new TableObj(check));
        check = check->next;
    }
}

// Member Functions
int ServerTableObj::Render(Terminal *term)
{
    FnTrace("ServerTableObj::Render()");
    term->RenderButton(x, y, w, h, ZF_RAISED, IMAGE_SAND);

    int xx = x + (w / 2);
    term->RenderText(user->system_name.Value(), xx, y + 6, COLOR_BLACK,
                     FONT_TIMES_20B, ALIGN_CENTER, w - 8);

    const genericChar* j = user->JobTitle(term);
    term->RenderText(j, xx, y + 26, COLOR_BLUE, FONT_TIMES_20B,
                     ALIGN_CENTER, w - 8);

    tables.Render(term);
    return 0;
}

int ServerTableObj::Layout(Terminal *term, int lx, int ly, int lw, int lh)
{
    FnTrace("ServerTableObj::Layout()");
    SetRegion(lx, ly, lw, lh);

    int width_left  = lw - 10;
    int height_left = lh - 46;
    int width  = 80;
    int height = 80;

    if (width > width_left)
        width = width_left;
    if (height > height_left)
        height = height_left;

    if (tables.Count() > ((width_left / width) * (height_left / height)))
    {
        // Full sized tables don't fit - make them smaller (at least fit 2x2)
        height = Min(60, height_left / 2);
        width  = Min(60, width_left / 2);
    }

    // lay tables out left to right, top to bottom
    ZoneObject *zoneObj = tables.List();
    while (zoneObj)
    {
        if (width > width_left)
        {
            width_left   = lw - 10;
            height_left -= height;
            if (height_left < 8)
                return 1;  // Ran out of room
            if (height > height_left)
                height = height_left;
        }
        zoneObj->Layout(term, lx + lw - width_left - 4, ly + lh - height_left,
                        width, height);
        width_left -= width;
        zoneObj = zoneObj->next;
    }
    return 0;
}


/*********************************************************************
 * TableAssignZone Class
 ********************************************************************/
// Member Functions
RenderResult TableAssignZone::Render(Terminal *term, int update_flag)
{
    FnTrace("TableAssignZone::Render()");
    RenderZone(term, NULL, update_flag);

    if (term->user == NULL)
        return RENDER_OKAY;

    System *sys = term->system_data;
    Settings *s = &(sys->settings);
    if (update_flag)
    {
        servers.Purge();
        for (Employee *e = sys->user_db.UserList(); e != NULL; e = e->next)
            if ((sys->labor_db.IsUserOnClock(e) && e->CanOrder(s) && !e->training)
                || sys->CountOpenChecks(e) > 0)
                servers.Add(new ServerTableObj(term, e));
    }

    servers.LayoutGrid(term, x + border, y + border,
                       w - (border * 2), h - (border * 2));
    servers.Render(term);  
    return RENDER_OKAY;
}

SignalResult TableAssignZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("TableAssignZone::Signal()");
    return SIGNAL_IGNORED;
}

SignalResult TableAssignZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("TableAssignZone::Touch()");
    ZoneObject *so = servers.Find(tx, ty);
    if (so)
    {
        ZoneObject *to = ((ServerTableObj *)so)->tables.Find(tx, ty);
        if (to)
            to->Touch(term, tx, ty);
        else
            MoveTables(term, (ServerTableObj *) so);
        return SIGNAL_OKAY;
    }
    return SIGNAL_IGNORED;
}

int TableAssignZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    if (update_message & (UPDATE_ALL_TABLES | UPDATE_USERS))
        Draw(term, 1);
    return 0;
}

int TableAssignZone::MoveTables(Terminal *term, ServerTableObj *sto)
{
    FnTrace("TableAssignZone::MoveTables()");
    sto->tables.SetSelected(0);
    ZoneObject *list;

    // Count selected tables
    int count = 0;
    for (list = servers.List(); list != NULL; list = list->next)
        count += ((ServerTableObj *)list)->tables.CountSelected();

    if (count <= 0)
    {
        sto->Draw(term);
        return 1;  // No tables to transfer
    }

    // Move tables
    for (list = servers.List(); list != NULL; list = list->next)
    {
        ZoneObject *zoneObj = ((ServerTableObj *)list)->tables.List();
        while (zoneObj)
        {
            if (zoneObj->selected)
            {
                Check *check = ((TableObj *)zoneObj)->check;
                int id = sto->user->id;
                term->system_data->exception_db.AddTableException(term, check, id);
                if (check)
                {
                    // Release the check from the previous user so the target owner
                    // can access it immediately, then persist the ownership change.
                    check->user_owner = id;
                    check->user_current = 0;
                    zoneObj->selected = 0;
                    check->Save();
                }
            }
            zoneObj = zoneObj->next;
        }
    }

    Draw(term, 1);
    term->UpdateOtherTerms(UPDATE_ALL_TABLES | UPDATE_CHECKS, NULL);
    return 0;
}
