 /*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025, 2026
  
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
 * expense_zone.cc
 * Functionality for paying expenses from revenue.
 */

#include "expense_zone.hh"
#include "utility.hh"
#include "account.hh"
#include "employee.hh"
#include "system.hh"
#include "src/utils/cpp23_utils.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


ExpenseZone::ExpenseZone()
{
    expense = nullptr;
    saved_expense = nullptr;
    show_expense = 0;
    // form_header defines the top of the space where the form fields will be drawn
    form_header = -11;
    list_footer = 12;
    form_spacing = 0.65;
    AddTextField(GlobalTranslate("Expense ID"), 5, 0);
    AddListField(GlobalTranslate("Payer"), nullptr);
    AddNewLine();

    AddTextField(GlobalTranslate("Expense Amount"), 10);  SetFlag(FF_MONEY);
    AddListField(GlobalTranslate("Expense Drawer"), nullptr);
    AddLabel("  or  ");
    AddListField(GlobalTranslate("Account"), nullptr);
    AddNewLine();

    AddListField(GlobalTranslate("Destination Account"), nullptr);
    AddNewLine();

    AddTextField(GlobalTranslate("Tax Amount"), 10);  SetFlag(FF_MONEY);
    AddListField(GlobalTranslate("Tax Account"), nullptr);
    AddNewLine();

    AddTextField(GlobalTranslate("Document"), 25);
    AddTextField(GlobalTranslate("Explanation"), 25);
    AddNewLine(2);
    AddSubmit(GlobalTranslate("Submit"), 10);

    record_no = -1;
    report = nullptr;
    page = 0;
    no_line = 1;
    lines_shown = 0;
    allow_balanced = 0;
}

ExpenseZone::~ExpenseZone()
{
    if (saved_expense != nullptr)
        delete saved_expense;
}

RenderResult ExpenseZone::Render(Terminal *term, int update_flag)
{
    FnTrace("ExpenseZone::Render()");
    genericChar buff[STRLENGTH];
    int col = color[0];
    int indent = 0;
    Flt header_line = 1.3;
    int total_expenses = term->system_data->expense_db.TotalExpenses();
    num_spaces = FormZone::ColumnSpacing(term, EXPENSE_ZONE_COLUMNS);
    list_spacing = 1.0;

    if (show_expense)
        ShowFields();
    else
        HideFields();
    FormZone::Render(term, update_flag);

    // FormZone::Render sets the initial record to 0.  But we want it to start
    // at -1, so we'll reset it here.
    if (update_flag == RENDER_NEW)
        record_no = -1;

    TextC(term, 0, term->Translate("Pay Expenses"), col);
    TextPosL(term, indent, header_line, term->Translate("Date"), col);
    indent += num_spaces;
    TextPosL(term, indent, header_line, term->Translate("Payer"), col);
    indent += num_spaces;
    TextPosL(term, indent, header_line, term->Translate("Source"), col);
    indent += num_spaces;
    TextPosL(term, indent, header_line, term->Translate("Amount"), col);
    indent += num_spaces;
    TextPosL(term, indent, header_line, term->Translate("Document"), col);
    // C++23: Use std::format
    vt::cpp23::format_to_buffer(buff, STRLENGTH, "Total Expenses: {}", term->FormatPrice(total_expenses));
    TextC(term, size_y - 1, buff, col);

    // generate and display the list of expenses
    if (update || update_flag || (report == nullptr))
    {
        report = std::make_unique<Report>();
        ListReport(term, report.get());
    }
    if (show_expense)
        report->selected_line = record_no;
    else
        report->selected_line = -1;
    // end the report two lines above the top of the form field area so that
    // there is plenty of room for the "Page x of y" display.
    if (lines_shown == 0)
        page = -1;
    else if (show_expense)
        page = record_no / lines_shown;
    report->Render(term, this, 2, list_footer, page, 0, list_spacing);
    page = report->page;
    lines_shown = report->lines_shown;
    return RENDER_OKAY;
}

SignalResult ExpenseZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("ExpenseZone::Signal()");
    SignalResult signal = SIGNAL_IGNORED;
    static const genericChar* commands[] = {"next", "prior", "change view",
                                      "restore", "test", "new", nullptr};
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
            show_expense = 1;
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
            show_expense = 1;
            LoadRecord(term, record_no);
        }
        draw = 1;
        break;
    case 2:  // change view
        if (show_expense == 1)
            show_expense = 0;
        else if (record_no > -1)
            show_expense = 1;
        draw = 1;
        break;
    case 3:  // restore
        RestoreRecord(term);
        draw = 1;
        break;
#ifdef DEBUG
    case 4:  // test  -- this could mess everything up.  Don't do it in live code
    {
        ExpenseDB *exp_db = &(term->system_data->expense_db);
        exp_db->MoveAll(nullptr);
        draw = 1;
        break;
    }
#endif
    case 5:  // new
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

SignalResult ExpenseZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("ExpenseZone::Touch()");
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
            Expense *exp = term->system_data->expense_db.FindByRecord(term, yy);
            if (exp != expense)
                SaveRecord(term, record_no, 1);
            if (exp)
                show_expense = 1;
            else
                show_expense = 0;
            LoadRecord(term, yy);
            Draw(term, 1);
            retval = SIGNAL_OKAY;
        }
        
        if (page != new_page)
        {
            page = new_page;
            show_expense = 0;
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
int ExpenseZone::UpdateForm(Terminal *term, int record)
{
    FnTrace("ExpenseZone::UpdateForm()");
    int changed = 0;
    int employee_id;
    int amount;
    int drawer_id;
    int account_id;
    Str doc;
    FormField *field = FieldList();

    if (expense == nullptr || show_expense == 0)
        return 1;

    field = field->next;  // skip expense id label
    field->Get(employee_id);
    if (employee_id != expense->employee_id)
    {
        changed = 1;
        expense->employee_id = employee_id;
    }
    field = field->next;  // pass payer

    field->Get(amount);
    if (amount != expense->amount)
    {
        changed = 1;
        expense->amount = amount;
    }
    field = field->next;  // pass amount

    field->Get(drawer_id); 
    if (drawer_id != expense->drawer_id)
    {
        changed = 1;
        expense->drawer_id = drawer_id;
    }
    field = field->next;  // pass drawer
    field = field->next;  // skip "or" label
    field->Get(account_id);
    if (account_id != expense->account_id)
    {
        changed = 1;
        expense->account_id = account_id;
    }
    field = field->next;  // pass account_id
    field = field->next;  // skip destination account
    field = field->next;  // skip tax
    field = field->next;  // skip tax account

    field->Get(doc);
    if (doc != expense->document)
    {
        changed = 1;
        expense->document = doc;
    }

    if (changed)
    {
        report.reset();
        update = 1;
    }
    return 0;
}

int ExpenseZone::HideFields()
{
    FnTrace("ExpenseZone::HideFields()");
    FormField *field = FieldList();

    while (field != nullptr)
    {
        field->active = 0;
        field = field->next;
    }
    return 0;
}

int ExpenseZone::ShowFields()
{
    FnTrace("ExpenseZone::ShowFields()");
    FormField *field = FieldList();

    while (field != nullptr)
    {
        field->active = 1;
        field = field->next;
    }
    return 0;
}

int ExpenseZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("ExpenseZone::LoadRecord()");
    FormField *field = FieldList();
    UserDB *employees = &(term->system_data->user_db);
    Employee *employee = nullptr;
    AccountDB *accounts = &(term->system_data->account_db);
    Account *account = nullptr;
    genericChar buffer[STRLENGTH];

    if (show_expense)
        expense = term->system_data->expense_db.FindByRecord(term, record);
    else
        expense = nullptr;

    if (expense != nullptr)
    {
        record_no = record;
        // save off the expense for Undo
        if (saved_expense == nullptr)
            saved_expense = new Expense;
        saved_expense->Copy(expense);
        //  AddTextField("Expense ID", 5, 0);
        field->Set(expense->eid);
        field = field->next;

        //  AddListField("Payer", NULL);
        field->ClearEntries();
        if (term->GetSettings()->allow_user_select)
        {
            employee = employees->UserList();
            while (employee != nullptr)
            {
                if (employee->active)
                    field->AddEntry(employee->system_name.Value(), employee->id);
                employee = employee->next;
            }
        }
        else
        {
            employee = employees->FindByID(expense->employee_id);
            if (employee)
                field->AddEntry(employee->system_name.Value(), employee->id);
        }
        field->Set(expense->employee_id);
        field = field->next;

        //  AddTextField("Expense Amount", 10);  SetFlag(FF_MONEY);
        field->Set(expense->amount);
        field = field->next;

        //  AddListField("Expense Drawer", NULL);
        field->ClearEntries();
        Drawer *drawer = term->system_data->DrawerList();
        field->AddEntry(GlobalTranslate("Not Set"), -1);
        while (drawer != nullptr)
        {
            if (drawer->IsOpen())
            {
                Employee *e = employees->FindByID(drawer->owner_id);
                if (e)
                {
                    field->AddEntry(e->system_name.Value(), drawer->serial_number);
                }
                else
                {
                    // C++23: Use std::format
                    vt::cpp23::format_to_buffer(buffer, STRLENGTH, "Drawer {}", drawer->number);
                    field->AddEntry(buffer, drawer->serial_number);
                }
            }
            drawer = drawer->next;
        }
        if (expense->drawer_id)
            field->Set(expense->drawer_id);
        else
        {
            Drawer *default_drawer = term->FindDrawer();
            if (default_drawer)
                field->Set(default_drawer->serial_number);
            else
                field->Set(-1);
        }
        field = field->next;

        //  AddLabel("  or  ");
        field = field->next;

        //  AddListField("Account", NULL);
        field->ClearEntries();
        account = accounts->AccountList();
        field->AddEntry(GlobalTranslate("Not Set"), -1);
        while (account != nullptr)
        {
            if (IsValidAccountNumber(term, account->number))
                field->AddEntry(account->name.Value(), account->number);
            account = account->next;
        }
        field->Set(expense->account_id);
        field = field->next;

        //  AddTextField("Destination Account", 10);
        field->ClearEntries();
        account = accounts->AccountList();
        field->AddEntry(GlobalTranslate("Not Set"), -1);
        while (account != nullptr)
        {
            if (IsValidAccountNumber(term, account->number))
                field->AddEntry(account->name.Value(), account->number);
            account = account->next;
        }
        field->Set(expense->dest_account_id);
        field = field->next;

        //  AddTextField("Tax Amount", 10);  SetFlag(FF_MONEY);
        field->Set(expense->tax);
        field = field->next;

        //  AddListField("Tax Account", NULL);
        field->ClearEntries();
        account = accounts->AccountList();
        while (account != nullptr)
        {
            field->AddEntry(account->name.Value(), account->number);
            account = account->next;
        }
        field->Set(expense->tax_account_id);
        field = field->next;

        //  AddTextField("Document", 25);
        field->Set(expense->document.Value());
        field = field->next;

        //  AddTextField("Explanation", 25);
        field->Set(expense->explanation);
        field = field->next;
        show_expense = 1;
        return 0;
    }
    else
    {
        show_expense = 0;
    }
    return 1;
}

/****
 * SaveRecord:  the last two arguments are not used.  They're only here
 *   because an ancestor class (FormZone) requires them.
 ****/
int ExpenseZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("ExpenseZone::SaveRecord()");
    Drawer *dlist = term->system_data->DrawerList();

    if (expense != nullptr)
    {
        FormField *field = FieldList();
        field->Get(expense->eid);  field = field->next;
        field->Get(expense->employee_id);  field = field->next;
        field->Get(expense->amount);  field = field->next;
        field->Get(expense->drawer_id);  field = field->next;
        field = field->next;  // label
        field->Get(expense->account_id);  field = field->next;
        field->Get(expense->dest_account_id);  field = field->next;
        field->Get(expense->tax);  field = field->next;
        field->Get(expense->tax_account_id);  field = field->next;
        field->Get(expense->document);  field = field->next;
        field->Get(expense->explanation);
    }
    //FIX BAK-->The rest of this block really needs to be atomic.
    //Otherwise, we could save without creating the balance, which...
    //...coming back to this page should reset everything, but who knows...
    if (record == -1)
        term->system_data->expense_db.Save();
    else if (expense != nullptr)
        term->system_data->expense_db.Save(expense->eid);
    records = RecordCount(term);
    if (record_no >= records)
        record_no = records - 1;
    // Update the drawer balance entry
    term->system_data->expense_db.AddDrawerPayments(dlist);
    report.reset();
    expense = nullptr;
    show_expense = 0;
    update = 1;
    return 0;
}

int ExpenseZone::RestoreRecord(Terminal *term)
{
    FnTrace("ExpenseZone::RestoreRecord()");

    if (expense != nullptr && saved_expense != nullptr)
    {
        expense->Copy(saved_expense);
        LoadRecord(term, record_no);
    }
    return 0;
}

int ExpenseZone::NewRecord(Terminal *term)
{
    FnTrace("ExpenseZone::NewRecord()");
    expense = term->system_data->expense_db.NewExpense();
    
    // Critical fix: Check if term->user exists before accessing it
    if (term->user != nullptr)
    {
        if (term->GetSettings()->allow_user_select == 0)
            expense->employee_id = term->user->id;
        
        if (term->user->training)
            expense->SetFlag(EF_TRAINING);
        expense->employee_id = term->user->id;
    }
    else
    {
        // No user logged in - set default values
        expense->employee_id = 0;
        fprintf(stderr, "WARNING: ExpenseZone::NewRecord() called with no user logged in\n");
    }
    
    show_expense = 1;
    records = RecordCount(term);
    record_no = records;
    return 0;
}

int ExpenseZone::KillRecord(Terminal *term, int record)
{
    FnTrace("ExpenseZone::KillRecord()");
    Drawer *dlist = term->system_data->DrawerList();
    int retval = 1;

    if (show_expense && expense)
    {
        Expense *delexp = term->system_data->expense_db.FindByID(expense->eid);
        if (delexp)
        {
            term->system_data->expense_db.Remove(delexp);
            delete delexp;
            expense = nullptr;
            records = RecordCount(term);
            if (record_no > records)
                record_no = records - 1;
            term->system_data->expense_db.AddDrawerPayments(dlist);
            show_expense = 0;
            retval = 0;
        }
    }
    else
    {
        term->Signal("status No record selected", group_id);
    }
    return retval;
}

int ExpenseZone::PrintRecord(Terminal *term, int record)
{
    FnTrace("ExpenseZone::PrintRecord()");

    //FIX BAK-->Not needed now (May 7, 2002) but should be completed
    return 0;
}

int ExpenseZone::Search(Terminal *term, int record, const genericChar* word)
{
    FnTrace("ExpenseZone::Search()");
    int search_rec;

    search_rec = term->system_data->expense_db.FindRecordByWord(term, word, record);
    if (search_rec >= 0)
    {
        record_no = search_rec;
        show_expense = 1;
        LoadRecord(term, record_no);
        return 1;
    }
    else if (show_expense != 0)
    {
        record_no = -1;
        show_expense = 0;
        report.reset();
    }
    return 1;
}

int ExpenseZone::ListReport(Terminal *term, Report *my_report)
{
    FnTrace("ExpenseZone::ListReport()");
    num_spaces = FormZone::ColumnSpacing(term, EXPENSE_ZONE_COLUMNS);
    Expense *currExpense = term->system_data->expense_db.ExpenseList();
    Drawer *dlist = term->system_data->DrawerList();
    Drawer *drawer;
    const genericChar* datestring;
    genericChar employee_name[STRLENGTH] = "";
    genericChar drawer_name[STRLENGTH] = "";
    genericChar account_name[STRLENGTH] = "";
    int indent = 0;
    int my_color = COLOR_DEFAULT;

    records = RecordCount(term);
    if (records < 1)
        my_report->TextC("No Expenses Entered", my_color);

    while (currExpense != nullptr)
    {
        drawer = dlist->FindBySerial(currExpense->drawer_id);
        if ((drawer == nullptr) || (drawer->GetStatus() == DRAWER_OPEN))
        {
            if (currExpense->IsTraining())
                my_color = COLOR_BLUE;
            else
                my_color = COLOR_DEFAULT;
            indent = 0;
            // get the person responsible
            currExpense->Author(term, employee_name);
            // get the drawer from which the cash was taken
            currExpense->DrawerOwner(term, drawer_name);
            // and get the account name, if set
            currExpense->AccountName(term, account_name);
            datestring = term->TimeDate(currExpense->exp_date, TD_DATE);
            
            // now print everything
            my_report->TextPosL(indent, datestring, my_color);
            indent += num_spaces;
            my_report->TextPosL(indent, employee_name, my_color);
            indent += num_spaces;
            // print either drawer or account, depending on which was selected
            if (currExpense->drawer_id > -1)
            {
                my_report->TextPosL(indent, drawer_name, my_color);
            }
            else if (currExpense->account_id > -1)
            {
                my_report->TextPosL(indent, account_name, my_color);
            }
            else
            {
                my_report->TextPosL(indent, "No Source!", my_color);
            }
            indent += num_spaces;
            my_report->TextPosL(indent, term->FormatPrice(currExpense->amount), my_color);
            indent += num_spaces;
            my_report->TextPosL(indent, currExpense->document.Value(), my_color);
            my_report->NewLine();
        }
        currExpense = currExpense->next;
    }
    return 0;
}

int ExpenseZone::RecordCount(Terminal *term)
{
    FnTrace("ExpenseZone::RecordCount()");
    int my_records = 0;
    int drawer_type = DRAWER_OPEN;

    if (allow_balanced)
        drawer_type = DRAWER_ANY;
    my_records = term->system_data->expense_db.ExpenseCount(term, drawer_type);

    return my_records;
}
