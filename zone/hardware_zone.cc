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
 * hardware_zone.cc - revision 3 (10/13/98)
 * Implementation of HardwareZone class (taken from settings_zone.cc)
 */

#include "hardware_zone.hh"
#include "printer.hh"
#include "settings.hh"
#include "labels.hh"
#include "manager.hh"
#include "system.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int IsKitchenPrinter(int type)
{
    int retval = 0;

    if (type == PRINTER_KITCHEN1 ||
        type == PRINTER_KITCHEN2 ||
        type == PRINTER_KITCHEN3 ||
        type == PRINTER_KITCHEN4)
    {
        retval = 1;
    }

    return retval;
}

/**** HardwareZone class ****/
static const genericChar* DrawerCountName[] = {
    "None", "One", "Two", NULL};
static int DrawerCountValue[] = {
    0, 1, 2, -1};

static const genericChar* DrawerPulseName[] = {
    "Pulse 1", "Pulse 2", NULL };
static int          DrawerPulseValue[] = {
    0, 1, -1 };

// Constructor
HardwareZone::HardwareZone()
{
    FnTrace("HardwareZone::HardwareZone()");
    list_header = 3;
    page        = 0;
    section     = 0;

    // Term Fields
    AddTextField("This Display's Name is", 32);
    term_start = FieldListEnd();
    AddListField("This Display's Function is", TermTypeName, TermTypeValue);
    AddListField("If This Display is a Kitchen Video Then It Sorts By", CheckDisplayOrderName, CheckDisplayOrderValue);
    AddListField("The Requisition Ticket Heading is", WOHeadingName, WOHeadingValue);
    AddListField("Does This Display Print Requisition Tickets?", NoYesName, NoYesValue);
    AddTextField("This Display's IP Address is", 18);
    display_host_field = FieldListEnd();
    AddTextField("This Display's Receipt Printer Queue Name is", 20);
    printer_host_field = FieldListEnd();
    //AddListField("Connection Interface", PortName, PortValue);
    AddListField("This Receipt Printer's Output is Formatted For ", ReceiptPrinterModelName,
                 ReceiptPrinterModelValue);
    AddListField("How Many Cash Drawers Does This Display Have?", DrawerCountName, DrawerCountValue);
    AddListField("The Drawer Opens On", DrawerPulseName, DrawerPulseValue);
    drawer_pulse_field = FieldListEnd();
    AddListField("Is A Card Reader Attached To This Display?", NoYesName, NoYesValue);
    AddNewLine();
    AddListField("If This Is A Customer Display Then Its Type Is", CustDispUnitName, CustDispUnitValue);
    AddTextField("If This Is A Customer Display Then Its Customer Display Device Path is", 20);

    AddNewLine();
    AddListField("Do Food prices include tax?", NoYesGlobalName, NoYesGlobalValue);
    AddListField("Do Alcohol prices include tax?", NoYesGlobalName, NoYesGlobalValue);
    AddListField("Do Room prices include tax?", NoYesGlobalName, NoYesGlobalValue);
    AddListField("Do Merchandise prices include tax?", NoYesGlobalName, NoYesGlobalValue);
    
    AddNewLine();
    AddListField("Default Page Variant", PageVariantName, PageVariantValue);

    Center();
    AddNewLine();
    AddLabel("Credit Card Settings");
    AddNewLine();
    LeftAlign();
    AddTextField("Credit Terminal ID", 20);
    AddTextField("Debit Terminal ID", 20);

    AddNewLine();
    Center();
    Color(color[0]);
    AddLabel("Terminal Hardware Testing");
    Color(COLOR_DEFAULT);
    AddNewLine();
    LeftAlign();
    AddSpace(4);
    AddButtonField("Test This Receipt Printer",  "printertest");
    AddSpace(4);
    AddButtonField("Test Drawer 1", "opendrawer1");
    AddSpace(4);
    AddButtonField("Test Drawer 2", "opendrawer2");

    // Printer Fields
    AddTextField("This Printer Is Identified As", 20);
    printer_start = FieldListEnd();
    AddListField("This Printer's Performance Assignment Is", PrinterTypeName, PrinterTypeValue);
    AddTextField("This Printer's Queue Name Is", 20);
    //AddListField("Connection Interface", PortName, PortValue);
    AddListField("This Printer's Output Is Formatted For", PrinterModelName, PrinterModelValue);
    AddListField("Kitchen Print Mode", PrintModeName, PrintModeValue);
    kitchen_mode_field = FieldListEnd();
    AddTextField("Requisition Ticket Header Margin", 4);
}

// Member Functions
RenderResult HardwareZone::Render(Terminal *term, int update_flag)
{
    FnTrace("HardwareZone::Render()");
    if (update_flag == RENDER_NEW)
    {
        page = 0;
        section = 0;
    }

    int col = color[0];
    ListFormZone::Render(term, update_flag);
    if (show_list)
    {
        switch (section)
        {
        default:
            TextC(term, 0, term->Translate("Configure Your Host Computer and Your Display Terminals, Then Assign Receipt Printer Queues To Them"), col);
            TextL(term, 2.3, term->Translate("Display's Name"), col);
            TextPosL(term, 22, 2.3, term->Translate("IP Address"), col);
            TextPosL(term, 38, 2.3, term->Translate("Current User"), col);
            TextPosL(term, 58, 2.3, term->Translate("Status"), col);
            break;
        case 1:
            TextC(term, 0, term->Translate("Configure Your Requisition Printers in Kitchens, Bars, Expediting Areas & Your Office Printer"), col);
            TextL(term, 2.3, term->Translate("Printer's Name"), col);
            TextPosL(term, 18, 2.3, term->Translate("lpd:queuename"), col);
            TextPosL(term, 38, 2.3, term->Translate("Assigned To"), col);
            TextPosL(term, 52, 2.3, term->Translate("Format"), col);
            TextPosL(term, 64, 2.3, term->Translate("Status"), col);
            break;
        }
    }
    else
    {
        switch (section)
        {
        default:
            TextC(term, 0, term->Translate("You May Now Edit This Display Terminal's Configuration"), col);
            break;
        case 1:
            TextC(term, 0, term->Translate("You May Now Edit This Printer's Configuration"), col);
            break;
        }
    }
    return RENDER_OKAY;
}

int HardwareZone::UpdateForm(Terminal *term, int record)
{
    FnTrace("HardwareZone::UpdateForm()");
    int retval = 1;
    FormField *field = printer_start;
    int type;
    int model;
    int drawers;

    if (section == 1)  // printer hardware
    {
        field = field->next;  // skip name
        field->Get(type); field = field->next;
        field = field->next;  // skip host
        //field = field->next;  // skip port
        field->Get(model); field = field->next;
        if (IsKitchenPrinter(type) && (model == MODEL_EPSON))
            kitchen_mode_field->active = 1;
        else
            kitchen_mode_field->active = 0;
    }
    else  // terminal hardware
    {
        field = drawer_pulse_field->fore;  // number of drawers field
        field->Get(drawers);
        if (drawers == 1)
            drawer_pulse_field->active = 1;
        else
            drawer_pulse_field->active = 0;
    }

    return retval;
}

SignalResult HardwareZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("HardwareZone::Signal()");
    static const genericChar* commands[] = {
        "section0", "section1", "changestatus", "calibrate",
        "testreceipt", "testreport",
        "printertest", "opendrawer1", "opendrawer2", NULL};

    Printer *printer = NULL;
    int idx = CompareList(message, commands);
    switch (idx)
    {
    case 0:
        ChangeSection(term, 0);
        return SIGNAL_OKAY;
    case 1:
        ChangeSection(term, 1);
        return SIGNAL_OKAY;
    case 2: // changestatus
        ChangeStatus(term);
        return SIGNAL_OKAY;
    case 3: // calibrate
        Calibrate(term);
        return SIGNAL_OKAY;
    case 4: // testreceipt
        term->parent->TestPrinters(term, 0);
        return SIGNAL_OKAY;
    case 5: // testreport
        term->parent->TestPrinters(term, 1);
        return SIGNAL_OKAY;
    case 6: // printertest
        printer = FindPrinter(term);
        if (printer)
            printer->TestPrint(term);
        return SIGNAL_OKAY;
    case 7: // opendrawer1
        printer = FindPrinter(term);
        if (printer)
            printer->OpenDrawer(0);
        return SIGNAL_OKAY;
    case 8: // opendrawer2
        printer = FindPrinter(term);
        if (printer)
            printer->OpenDrawer(1);
        return SIGNAL_OKAY;
    default:
        return ListFormZone::Signal(term, message);
    }
}

int HardwareZone::LoadRecord(Terminal *term, int record)
{
	FnTrace("HardwareZone::LoadRecord()");
    PrinterInfo *pi = NULL;
    TermInfo    *ti = NULL;
    FormField   *thisForm = NULL;

	for (FormField *field = FieldList(); field != NULL; field = field->next)
		field->active = 0;

	Settings *settings = term->GetSettings();
	switch (section)
	{
    case 1:  // remote printer
        pi = settings->FindPrinterByRecord(record);
        if (pi == NULL)
            break;

        thisForm = printer_start;
        thisForm->Set(pi->name); thisForm->active = 1; thisForm = thisForm->next;
        thisForm->Set(pi->type); thisForm->active = 1; thisForm = thisForm->next;
        thisForm->Set(pi->host); thisForm->active = 1; thisForm = thisForm->next;
        //thisForm->Set(pi->port); thisForm->active = 1; thisForm = thisForm->next;
        thisForm->Set(pi->model); thisForm->active = 1; thisForm = thisForm->next;
        thisForm->Set(pi->kitchen_mode);
        if (IsKitchenPrinter(pi->type) && (pi->model == MODEL_EPSON))
        {
            thisForm->active = 1;
        }
        thisForm = thisForm->next;
	thisForm->Set(pi->order_margin); thisForm->active = 1;

        break;
    default:  // terminal
        ti = settings->FindTermByRecord(record);
        if (ti == NULL)
            break;

        thisForm = term_start;
        thisForm->Set(ti->name); thisForm->active = 1; thisForm = thisForm->next;
        thisForm->Set(ti->type); thisForm->active = 1; thisForm = thisForm->next;
        thisForm->Set(ti->sortorder); thisForm->active = 1; thisForm = thisForm->next;
        thisForm->Set(ti->workorder_heading); thisForm->active = 1; thisForm = thisForm->next;
        thisForm->Set(ti->print_workorder); thisForm->active = 1; thisForm = thisForm->next;
        thisForm->Set(ti->display_host); thisForm->active = !ti->IsServer(); thisForm = thisForm->next;
        thisForm->Set(ti->printer_host); thisForm->active = 1; thisForm = thisForm->next;
        //thisForm->Set(ti->printer_port); thisForm->active = 1; thisForm = thisForm->next;
        thisForm->Set(ti->printer_model); thisForm->active = 1; thisForm = thisForm->next;
        thisForm->Set(ti->drawers); thisForm->active = 1; thisForm = thisForm->next;
        thisForm->Set(ti->dpulse);
        if (ti->drawers == 1)
            thisForm->active = 1;
        thisForm = thisForm->next;
        thisForm->Set(ti->stripe_reader); thisForm->active = 1; thisForm = thisForm->next;
        thisForm->Set(ti->cdu_type); thisForm->active = 1; thisForm = thisForm->next;
        thisForm->Set(ti->cdu_path); thisForm->active = 1; thisForm = thisForm->next;

    	thisForm->Set(ti->tax_inclusive[0]); thisForm->active = 1; thisForm = thisForm->next;
    	thisForm->Set(ti->tax_inclusive[2]); thisForm->active = 1; thisForm = thisForm->next;
    	thisForm->Set(ti->tax_inclusive[1]); thisForm->active = 1; thisForm = thisForm->next;
    	thisForm->Set(ti->tax_inclusive[3]); thisForm->active = 1; thisForm = thisForm->next;
    	thisForm->Set(ti->page_variant); thisForm->active = 1; thisForm = thisForm->next;

        if (MasterSystem->settings.authorize_method == CCAUTH_CREDITCHEQ)
        {
            thisForm->active = 1;  thisForm = thisForm->next;  // skip label
            thisForm->Set(ti->cc_credit_termid);  thisForm->active = 1; thisForm = thisForm->next;
            thisForm->Set(ti->cc_debit_termid);  thisForm->active = 1; thisForm = thisForm->next;
        }
        else
        {
            thisForm = thisForm->next;  // skip label
            thisForm = thisForm->next;  // skip Credit Terminal ID
            thisForm = thisForm->next;  // skip Debit Terminal ID
        }
        thisForm->active = 1; thisForm = thisForm->next; // label
        thisForm->active = 1; thisForm = thisForm->next; // button 1
        thisForm->active = 1; thisForm = thisForm->next; // button 2
        thisForm->active = 1;
        break;
	}
	return 0;
}

int HardwareZone::SaveRecord(Terminal *term, int record, int write_file)
{
	FnTrace("HardwareZone::SaveRecord()");
    TermInfo    *ti = NULL;
    PrinterInfo *pi = NULL;
	Settings    *settings = term->GetSettings();
	FormField   *field = NULL;

	switch (section)
	{
    default:  // terminal hardware
        ti = settings->FindTermByRecord(record);
        if (ti)
        {
            Str tmp;
            field = term_start;
            field->Get(ti->name); field = field->next;
            int old_type = ti->type;
            field->Get(ti->type); field = field->next;
            // Automatically set page_variant to 1 (Page -2) when Self Order Mode is selected
            if (ti->type == TERMINAL_SELFORDER && old_type != TERMINAL_SELFORDER)
            {
                ti->page_variant = 1;  // Page -2 for Self Order Mode
            }
            field->Get(ti->sortorder); field = field->next;
            field->Get(ti->workorder_heading); field = field->next;
            field->Get(ti->print_workorder); field = field->next;
            field->Get(tmp); field = field->next;
            if (tmp.size() <= 0)
                tmp.Set("unset");
            ti->display_host.Set(tmp);
            field->Get(ti->printer_host); field = field->next;
            //field->Get(ti->printer_port); field = field->next;
	    	ti->printer_port = PORT_VT_DAEMON;
            field->Get(ti->printer_model); field = field->next;
            field->Get(ti->drawers); field = field->next;
            field->Get(ti->dpulse); field = field->next;
            field->Get(ti->stripe_reader); field = field->next;
            field->Get(ti->cdu_type); field = field->next;
            field->Get(ti->cdu_path); field = field->next;

			field->Get(ti->tax_inclusive[0]); field = field->next;
			field->Get(ti->tax_inclusive[2]); field = field->next;
			field->Get(ti->tax_inclusive[1]); field = field->next;
			field->Get(ti->tax_inclusive[3]); field = field->next;
			field->Get(ti->page_variant); field = field->next;

            if (MasterSystem->settings.authorize_method == CCAUTH_CREDITCHEQ)
            {
                field = field->next;  // skip label
                field->Get(ti->cc_credit_termid); field = field->next;
                field->Get(ti->cc_debit_termid); field = field->next;
            }
            // Removed automatic printer_host clearing logic that was causing duplication
            // Users can now have independent display and printer host settings
        }
        break;
    case 1:  // remote printer
        pi = settings->FindPrinterByRecord(record);
        if (pi)
        {
            field = printer_start;
            field->Get(pi->name); field = field->next;
            field->Get(pi->type); field = field->next;
            field->Get(pi->host); field = field->next;
            //field->Get(pi->port); field = field->next;
	    pi->port = PORT_VT_DAEMON;
            field->Get(pi->model); field = field->next;
            field->Get(pi->kitchen_mode); field = field->next;
	    field->Get(pi->order_margin);
        }
        break;
	}

	if (write_file)
		settings->Save();
	return 0;
}

int HardwareZone::NewRecord(Terminal *term)
{
    FnTrace("HardwareZone::NewRecord()");
    Settings *settings = term->GetSettings();
    switch (section)
    {
    default: settings->Add(new TermInfo); break;
    case 1:  settings->Add(new PrinterInfo); break;
    }
    return 0;
}

int HardwareZone::KillRecord(Terminal *term, int record)
{
    FnTrace("HardwareZone::KillRecord()");
    Control *db = term->parent;
    Settings *settings = term->GetSettings();
    switch (section)
    {
    default:
    {
        TermInfo *ti = settings->FindTermByRecord(record);
        if (ti && !ti->IsServer())
        {
            settings->Remove(ti);
            Printer *printer = ti->FindPrinter(db);
            if (printer)
                db->KillPrinter(printer);
            Terminal *tmp = ti->FindTerm(db);
            if (tmp)
                tmp->kill_me = 1;
            delete ti;
        }
    }
    break;
    case 1:
    {
        PrinterInfo *pi = settings->FindPrinterByRecord(record);
        if (pi)
        {
            Printer *printer = pi->FindPrinter(db);
            if (printer)
                db->KillPrinter(printer);
            settings->Remove(pi);
            delete pi;
        }
        break;
    }
    }
    return 0;
}

int HardwareZone::ListReport(Terminal *term, Report *r)
{
    FnTrace("HardwareZone::ListRecord()");
    Settings *settings = term->GetSettings();
    switch (section)
    {
    default: return settings->TermReport(term, r);
    case 1:  return settings->PrinterReport(term, r);
    }
}

int HardwareZone::RecordCount(Terminal *term)
{
    FnTrace("HardwareZone::RecordCount()");
    Settings *settings = term->GetSettings();
    switch (section)
    {
    default: return settings->TermCount();
    case 1:  return settings->PrinterCount();
    }
}

int HardwareZone::ChangeSection(Terminal *term, int no)
{
    FnTrace("HardwareZone::ChangeSection()");
    if (no == section)
        return 0;

    SaveRecord(term, record_no, 0);
    record_no = 0;
    show_list = 1;
    section = no;
    if (section > 1)
        section = 0;
    LoadRecord(term, 0);
    records = RecordCount(term);
    Draw(term, 1);
    return 0;
}

int HardwareZone::ChangeStatus(Terminal *term)
{
    FnTrace("HardwareZone::ChangeStatus()");
    Settings    *settings = term->GetSettings();
    TermInfo    *ti = NULL;
    Terminal    *terminal = NULL;
    PrinterInfo *pi = NULL;
    Printer     *printer = NULL;

    switch (section)
    {
    default:
        ti = settings->FindTermByRecord(record_no);
        if (ti == NULL || ti->IsServer())
            return 0;

        terminal = ti->FindTerm(term->parent);
        if (terminal)
        {
            // disable term
            terminal->kill_me = 1;
            printer = ti->FindPrinter(term->parent);
            term->parent->KillPrinter(printer, 1);
        }
        else
        {
            // enable term
            term->OpenDialog("Starting The Display Terminal\\Please Wait");
            ti->OpenTerm(term->parent, 1);
            term->KillDialog();
            term->UpdateAllTerms(UPDATE_TERMINALS | UPDATE_PRINTERS, NULL);

            // enable any associated printers
            pi = settings->PrinterList();
            while (pi)
            {
                printer = pi->FindPrinter(term->parent);
                if (printer == NULL &&
                    (pi->host == ti->printer_host || pi->host == ti->display_host))
                    pi->OpenPrinter(term->parent, 1);
                pi = pi->next;
            }

	    // connect/create drawer for terminal
	    MasterSystem->CreateFixedDrawers();
        }
        break;
    case 1:
        pi = settings->FindPrinterByRecord(record_no);
        if (pi == NULL)
            return 0;

        printer = pi->FindPrinter(term->parent);
        if (printer)
        {
            // disable printer
            term->parent->KillPrinter(printer, 1);
        }
        else
        {
            // enable printer
            term->OpenDialog("Starting The Printer\\Please Wait");
            pi->OpenPrinter(term->parent, 1);
            term->KillDialog();
            term->UpdateAllTerms(UPDATE_PRINTERS, NULL);
        }
        break;
    }
    return 0;
}

int HardwareZone::Calibrate(Terminal *term)
{
    FnTrace("HardwareZone::Calibrate()");
    if (section != 0)
        return 1;

    Settings *settings = term->GetSettings();
    TermInfo *ti = settings->FindTermByRecord(record_no);
    if (ti == NULL)
        return 0;

    Terminal *parent_term = ti->FindTerm(term->parent);
    if (parent_term == NULL)
        return 0;
    else
        return parent_term->CalibrateTS();
}

Printer *HardwareZone::FindPrinter(Terminal *term)
{
    FnTrace("HardwareZone::FindPrinter()");
    Settings *settings = term->GetSettings();
    TermInfo *ti = settings->FindTermByRecord(record_no);
    if (ti)
        return ti->FindPrinter(MasterControl);
    else
        return NULL;
}
