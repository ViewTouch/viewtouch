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
 * payment_zone.cc - revision 170 (10/13/98)
 * Implementation of PaymentZone module
 */

#include "payment_zone.hh"
#include "terminal.hh"
#include "check.hh"
#include "printer.hh"
#include "drawer.hh"
#include "employee.hh"
#include "dialog_zone.hh"
#include "locale.hh"
#include "settings.hh"
#include "main/data/settings_enums.hh"
#include "system.hh"
#include "credit.hh"
#include "archive.hh"
#include "manager.hh"
#include "customer.hh"
#include "image_data.hh"
#include "utility.hh"
#include "safe_string_utils.hh"
#include "src/utils/cpp23_utils.hh"
#include <cstring>
#include <sys/stat.h>

#ifdef DMALLOC
#include <dmalloc.h>
#include "src/utils/cpp23_utils.hh"
#endif


/***********************************************************************
 * PaymentZone Class
 **********************************************************************/

PaymentZone::PaymentZone()
{
    FnTrace("PaymentZone::PaymentZone()");
    min_size_x       = 22;
    min_size_y       = 17;
    current_payment  = nullptr;
    voided           = 0;
    spacing          = 2;
    drawer_open      = 0;
    input_line       = 0;
    amount           = 0;
}

RenderResult PaymentZone::Render(Terminal *term, int update_flag)
{
    FnTrace("PaymentZone::Render()");
    genericChar cdustring[STRLONG];
    int preauth_amount = 0;
    int has_payments = 0;

    LayoutZone::Render(term, update_flag);
    int text = color[0];

    Employee *employee = term->user;
    Check    *currCheck = term->check;
    if (employee == nullptr || currCheck == nullptr)
        return RENDER_OKAY;

    Settings *settings = term->GetSettings();
    SubCheck *subCheck = &work_sub;
    if (currCheck->current_sub == nullptr)
    {
        currCheck->current_sub = currCheck->FirstOpenSubCheck();
        if (currCheck->current_sub == nullptr)
            return RENDER_OKAY;
        subCheck->Copy(currCheck->current_sub, settings);
        current_payment = nullptr;
        amount = 0;
    }
    else if (update_flag == RENDER_NEW)
    {
        if (term->credit != nullptr)
        {
            delete term->credit;
            term->credit = nullptr;
        }
        current_payment = nullptr;
        subCheck->Copy(currCheck->current_sub, settings);
        amount = 0;
        if (term->is_bar_tab)
            amount = settings->default_tab_amount;
    }


    subCheck->FigureTotals(settings);
    voided            = (subCheck->status == CHECK_VOIDED);
    int change_value  = subCheck->TotalPayment(TENDER_CHANGE);
    Payment *gratuity = subCheck->FindPayment(TENDER_GRATUITY);
    Payment *pennies  = subCheck->FindPayment(TENDER_MONEY_LOST);
    int total_cost    = subCheck->total_cost;
    mark = static_cast<float>(size_x * 0.62);
    Flt min_spacing = 1.0;
    if (spacing < min_spacing)
        min_spacing = spacing;

    genericChar str[256];
    // Header
    Flt line = 0.0;
    if (currCheck->IsTakeOut())
        vt_safe_string::safe_copy(str, 256, MasterLocale->Translate("Take Out"));
    else if(currCheck->IsFastFood())
        vt_safe_string::safe_copy(str, 256, MasterLocale->Translate("Fast Food"));
    else if(currCheck->IsToGo())
        vt_safe_string::safe_copy(str, 256, MasterLocale->Translate("To Go"));
    else if(currCheck->IsForHere())
        vt_safe_string::safe_copy(str, 256, MasterLocale->Translate("For Here"));
    else
        vt_safe_string::safe_format(str, 256, "Table %s", currCheck->Table());

    TextL(term, line, str, text);
    if (currCheck->SubCount() > 1)
    {
        const char* status_str = "";
        if (subCheck->status == CHECK_OPEN)
            status_str = " - Open";
        else if (subCheck->status == CHECK_CLOSED)
            status_str = " - Closed";
        else if (subCheck->status == CHECK_VOIDED)
            status_str = " - Voided";
        
        vt_safe_string::safe_format(str, 256, "Check #%d%s", subCheck->number, status_str);
        
        // Highlight the current subcheck
        Background(term, line - ((spacing - 1)/2), 1.0, IMAGE_LIT_SAND);
        TextC(term, line, str, COLOR_DK_BLUE);
    }
    int guests = currCheck->Guests();
    if (guests > 0)
        vt_safe_string::safe_format(str, 256, "Guests %d", guests);
    else
        vt_safe_string::safe_copy(str, 256, GlobalTranslate("No Guests"));
    TextR(term, line, str, text);
    line += min_spacing * 1.5;

    int nameadd = 0;
    if (strlen(currCheck->FirstName()) > 0)
    {
        if (strlen(currCheck->LastName()) > 0)
            vt_safe_string::safe_format(str, 256, "%s %s", currCheck->FirstName(), currCheck->LastName());
        else
            vt_safe_string::safe_copy(str, 256, currCheck->FirstName());
        TextL(term, line, str, text);
        nameadd = 1;
    }
    else if (strlen(currCheck->LastName()) > 0)
    {
        TextL(term, line, currCheck->LastName(), text);
        nameadd = 1;
    }
    if (strlen(currCheck->PhoneNumber()) > 0)
    {
        TextR(term, line, currCheck->PhoneNumber(), text);
        nameadd = 1;
    }
    if (nameadd > 0)
        line += min_spacing * 1.5;

    TextPosR(term, mark, line, "Sale Total");
    TextPosR(term, mark + 9, line, term->FormatPrice(subCheck->total_sales));
    line += min_spacing;

    if (settings->tax_alcohol && settings->tax_PST)
    {
        int alcohol_tax = subCheck->total_tax_alcohol;
        TextPosR(term, mark, line, "Alcohol Tax");
        TextPosR(term, mark + 9, line, term->FormatPrice(alcohol_tax));
        line += min_spacing;
    }
    else if(settings->tax_food ||
            settings->tax_alcohol ||
            settings->tax_merchandise ||
            settings->tax_room)
    {
    	// if takouts are not taxed, zero out the subcheck food tax
        if (currCheck->IsToGo() && (settings->tax_takeout_food == 0))
           subCheck->total_tax_food = 0;
        int sales_tax = subCheck->total_tax_food;
        sales_tax += subCheck->total_tax_alcohol;
        sales_tax += subCheck->total_tax_merchandise;
        sales_tax += subCheck->total_tax_room;
        if (sales_tax > 0)
        {
            TextPosR(term, mark, line, "Sales Tax");
            TextPosR(term, mark + 9, line, term->FormatPrice(sales_tax));
            line += min_spacing;
        }
    }

	if(settings->tax_GST > 0)
    {
		TextPosR(term, mark, line, "GST"); // Canada - Goods and Services Tax
		TextPosR(term, mark + 9, line, term->FormatPrice(subCheck->total_tax_GST));
		line += min_spacing;
	}

	if(settings->tax_HST > 0)
	{
		TextPosR(term, mark, line, "HST"); // Canada - Harmonized Sales Tax
		TextPosR(term, mark + 9, line, term->FormatPrice(subCheck->total_tax_HST));
		line += min_spacing;
	}

    if (settings->tax_PST > 0)
    {
        TextPosR(term, mark, line, "PST"); // Canada - Provincial Sales Tax - BC, Ontario?
        TextPosR(term, mark + 9, line, term->FormatPrice(subCheck->total_tax_PST));
        line += min_spacing;
    }

    if (settings->tax_QST > 0)
    {
        TextPosR(term, mark, line, "QST"); // Canada - Quebec Sales Tax
        TextPosR(term, mark + 9, line, term->FormatPrice(subCheck->total_tax_QST));
        line += min_spacing;
    }

    if (settings->tax_VAT > 0 && subCheck->total_tax_VAT != 0)
    {
        TextPosR(term, mark, line, "VAT");
        TextPosR(term, mark + 9, line, term->FormatPrice(subCheck->total_tax_VAT));
        line += min_spacing;
    }

    if (subCheck->IsTaxExempt())
    {
        TextPosR(term, mark, line, term->Translate("Tax Exempt"));
        TextPosR(term, mark + 9, line, term->FormatPrice(-subCheck->TotalTax()));
        line += min_spacing;
        vt_safe_string::safe_format(str, 256, "Tax ID:  %s", subCheck->tax_exempt.Value());
        TextL(term, line, str);
        line += min_spacing;
    }
    
    if (gratuity)
    {
        vt_safe_string::safe_format(str, 256, "%g%% Gratuity", (Flt) gratuity->amount / 100.0);
        TextPosR(term, mark, line, str);
        TextPosR(term, mark + 9, line, term->FormatPrice(-gratuity->value));
        line += min_spacing;
    }

    if (pennies)
    {
        TextPosR(term, mark, line, "Money Lost");
        TextPosR(term, mark + 9, line, term->FormatPrice(pennies->value));
        total_cost -= pennies->value;
        line += min_spacing;
    }

    line += min_spacing * .5;
    TextPosR(term, mark, line, "Total");
    TextPosR(term, mark + 9, line, term->FormatPrice(total_cost), COLOR_DK_RED);
    line += min_spacing;

    if (term->cdu != nullptr)
    {
        term->cdu->Refresh(-1);  // make sure the screen doesn't clear until we're done
        vt::cpp23::format_to_buffer(cdustring, STRLONG, "Total  {}{}", settings->money_symbol.Value(),
                 term->FormatPrice(total_cost));
        term->cdu->Clear();
        term->cdu->Write(cdustring);
    }

    header_size = line + 1;
    Line(term, line, color[0]);
    line += min_spacing;

    // Display Payments
    if (subCheck->status == CHECK_VOIDED || voided)
    {
        TextC(term, line + min_spacing, term->Translate("Check Voided"));
        return RENDER_OKAY;
    }

    int c1;
    int c2;
    Flt bg_half = ((spacing - 1)/2);
    Flt bg_start;
    Flt bg_lines;
    Payment *payment = subCheck->PaymentList();
    while (payment != nullptr)
    {
        has_payments = 1;
        if (payment->Suppress() == 0)
        {
            if (payment->flags & TF_FINAL)
            {
                c1 = COLOR_BLUE;
                c2 = COLOR_DK_BLUE;
            }
            else
            {
                c1 = COLOR_DEFAULT;
                c2 = COLOR_DEFAULT;
            }
            if (payment == current_payment)
            {
                bg_start = line - bg_half;
                bg_lines = 1;
                if (payment->credit != nullptr)
                    bg_lines = 4;
                bg_lines += (bg_half * 2);
                if (payment->credit != nullptr &&
                    strlen(payment->credit->Name()) > 0)
                {
                    bg_lines += 1;
                }
                Background(term, bg_start, bg_lines, IMAGE_LIT_SAND);
            }
            // We want to display the Preauthed, amount or the Authed,
            // here, so we call payment->FigureTotals(1) to make sure
            // we have preauthed and then payment->FigureTotals(0) to
            // reset the total value to just the authed for the
            // subCheck->balance.
            payment->FigureTotals(1);
            TextL(term, line, payment->Description(settings), c1);
            TextR(term, line, term->FormatPrice(payment->value), c1);
            payment->FigureTotals(0);
            Credit *cr = payment->credit;
            if (cr != nullptr)
            {
                preauth_amount += cr->TotalPreauth();
                line += min_spacing;
                TextPosL(term, 2, line, "Acct No", c2);
                TextPosL(term, 10, line, cr->PAN(settings->show_entire_cc_num), c1);
                line += min_spacing;
                if (strlen(cr->Name()) > 0)
                {
                    TextPosL(term, 2, line, "Name", c2);
                    TextPosL(term, 10, line, cr->Name(), c1);
                    line += min_spacing;
                    have_name = 1;
                }
                else
                    have_name = 0;
                TextPosL(term, 2, line, "Expires", c2);
                TextPosL(term, 10, line, cr->ExpireDate(), c1);
                line += min_spacing;
                if (cr->IsVoiced())
                {
                    TextPosL(term, 2, line, "Auth", c2);
                    vt::cpp23::format_to_buffer(str, 256, "Voice ({})", cr->Approval());  // str is 256 bytes, not STRLENGTH
                    TextPosL(term, 10, line, str, COLOR_GREEN);
                }
                else if (cr->IsVoided())
                {
                    TextPosL(term, 2, line, term->Translate("Transaction Voided"), COLOR_RED);
                }
                else if (cr->IsPreauthed())
                {
                    TextPosL(term, 2, line, "PreAuth", c2);
                    TextPosL(term, 10, line, cr->Approval(), COLOR_BLUE);
                }
                else if (cr->IsAuthed())
                {
                    TextPosL(term, 2, line, "Auth No", c2);
                    TextPosL(term, 10, line, cr->Approval(), COLOR_GREEN);
                }
                else
                {
                    TextPosL(term, 2, line, "Message", c2);
                    TextPosL(term, 10, line, cr->Code(), COLOR_RED);
                }
            }
            line += spacing;
        }
        payment = payment->next;
    }
    if (has_payments != term->has_payments)
    {
        term->has_payments = has_payments;
        term->Draw(1);
    }

    // Display check status
    if (subCheck->status == CHECK_CLOSED)
    {
        TextC(term, line, term->Translate("Check Closed"), COLOR_BLUE);
        if (employee->CanRebuild(settings))
        {
            TextC(term, line + min_spacing, term->Translate("Select 'Clear All Entries'"), text);
            TextC(term, line + (min_spacing * 2), term->Translate("To Rebuild"), text);
        }
    }
    else if (subCheck->OrderList() == nullptr)
        TextC(term, line, term->Translate("Check Blank"), COLOR_YELLOW);
    else if (subCheck->balance <= 0)
        TextC(term, line, term->Translate("Balance Covered"), COLOR_DK_GREEN);
    else
        line -= min_spacing * 2;

    // Display Input
    input_line = line + (min_spacing * 2);
    if (subCheck->status == CHECK_OPEN)
        RenderPaymentEntry(term);

    // Footer
    int add_space = 3;
    if (preauth_amount > 0)
        add_space += 1;
    if (subCheck->TabRemain() > 0)
        add_space += 1;
    line = size_y - (min_spacing * add_space);
    Line(term, line, text);
    line += min_spacing * .8;

    TextPosR(term, mark, line, "Amount Tendered", text);
    TextPosR(term, mark + 9, line, term->FormatPrice(subCheck->payment), COLOR_BLUE);
    line += min_spacing;
    if (preauth_amount > 0)
    {
        TextPosR(term, mark, line, "Amount Preauthed", text);
        TextPosR(term, mark +9, line, term->FormatPrice(preauth_amount), COLOR_BLUE);
        line += min_spacing;
    }
    if (subCheck->TabRemain() > 0)
    {
        TextPosR(term, mark, line, "Tab Remaining", text);
        TextPosR(term, mark + 9, line, term->FormatPrice(subCheck->TabRemain()), COLOR_BLUE);
        line += min_spacing;
    }
    if (subCheck->IsBalanced() == 0)
    {
        if (term->cdu != nullptr)
        {
            vt::cpp23::format_to_buffer(cdustring, STRLONG, "Due:  {}{}", settings->money_symbol.Value(),
                     term->FormatPrice(subCheck->balance));
            term->cdu->NewLine();
            term->cdu->Write(cdustring);
        }
        TextPosR(term, mark, line, "Balance Due", text);
        TextPosR(term, mark + 9, line, term->FormatPrice(subCheck->balance), COLOR_DK_RED);
    }
    else
    {
        TextPosR(term, mark, line, "Change", text);
        term->FormatPrice(str, change_value);
        TextPosR(term, mark + 9, line, str, COLOR_DK_GREEN);

        if (change_value != 0 && 
            drawer_open == 0 && 
            subCheck->PaymentList() && 
            subCheck->status == CHECK_OPEN)
        {
            OpenDrawer(term);
        }
        if (term->cdu != nullptr)
        {
            vt::cpp23::format_to_buffer(cdustring, STRLONG, "Change  {}{}", settings->money_symbol.Value(),
                     term->FormatPrice(change_value));
            term->cdu->NewLine();
            term->cdu->Write(cdustring);
        }
    }
    return RENDER_OKAY;
}

SignalResult PaymentZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("PaymentZone::Signal()");

    static const genericChar* commands[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "00",
        "cancel", "finalize", "finalize2", "finalize3",
        "print ", "backspace", "clear", "nextcheck", "nextcheckforce",
        "void", "undo", "undoconfirmed", "merchandise", "done",
        "10", "20", "taxexempt", "settaxexempt ", "ccamountchanged",
        "ccrefund", "save", nullptr};

    Employee      *employee = term->user;
    Check         *thisCheck = term->check;
    GetTextDialog *textdiag = nullptr;
    DialogZone    *confirm = nullptr;
    char           buffer[STRLONG];

    if (employee == nullptr || thisCheck == nullptr)
        return SIGNAL_IGNORED;

    Settings *settings = term->GetSettings();
    SubCheck *sc = &work_sub;

    if (strncmp(message, "tender ", 7) == 0)
    {
        int ptype = TENDER_CASH;
        int pid = 0;
        int pflags = 0;
        int pamount = 0;
        sscanf(&message[7], "%d%d%d%d", &ptype, &pid, &pflags, &pamount);
        AddPayment(term, ptype, pid, pflags, pamount);
        amount = 0;
        return SIGNAL_OKAY;
    }
    else if (strncmp(message, "amount ", 7) == 0)
    {
        if (sc && current_payment &&
            current_payment->tender_type == TENDER_CHARGE_ROOM)
        {
            int room = atoi(&message[7]);
            if (room == 0)
            {
                sc->Remove(current_payment);
                delete current_payment;
                current_payment = nullptr;
            }
            else
                current_payment->tender_id = room;
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        return SIGNAL_IGNORED;
    }
    else if (strncmp(message, "swipe ", 6) == 0)
    {
        if (term->dialog != nullptr && term->dialog->Type() == ZONE_DLG_CREDIT)
        {
            return SIGNAL_IGNORED;
        }
        else
        {
            if (term->dialog != nullptr)
            {
                ReportError("PaymentZone Signal Swipe dumping previous dialog!");
                vt::cpp23::format_to_buffer(buffer, STRLONG, "    Named:  {}\n", term->dialog->name.Value());
                ReportError(buffer);
            }
            AddPayment(term, TENDER_CREDIT_CARD, &message[6]);
            return SIGNAL_OKAY;
        }
    }

    int idx = CompareListN(commands, message);
    switch (idx)
    {
    case 10:  // 00
        if (sc && sc->status == CHECK_OPEN && amount < 100000)
        {
            amount = (amount * 100);
            DrawPaymentEntry(term);
            return SIGNAL_OKAY;
        }
        break;
    case 11:  // cancel
        if (sc)
        {
            if (settings->allow_cc_cancels == 0 && sc->HasAuthedCreditCards())
            {
                confirm = new SimpleDialog(term->Translate("You cannot cancel with an authorized card."));
                confirm->Button(term->Translate("Okay"));
                term->OpenDialog(confirm);
                return SIGNAL_ERROR;
            }
            else
            {
                drawer_open = 0;
                voided = 0;
                amount = 0;
                term->is_bar_tab = 0;
                if (thisCheck->current_sub)
                    sc->Copy(thisCheck->current_sub, settings, 1); // should restore original
                if (sc->IsBalanced())
                    term->check_balanced = 1;
                else
                    term->check_balanced = 0;
                term->Draw(1);
                return SIGNAL_OKAY;
            }
        }
        break;
    case 12:  // finalize
        CloseCheck(term, 0);
        return SIGNAL_OKAY;
    case 13:  // finalize2
        CloseCheck(term, 1);
        return SIGNAL_OKAY;
    case 14:  // finalize3
        CloseCheck(term, 2);
        return SIGNAL_OKAY;
    case 15:  // print
        if (strcmp("subcheck", &message[6]) == 0 && sc != nullptr)
        {
            Printer *printer = term->FindPrinter(PRINTER_RECEIPT);
            if (sc->status == CHECK_OPEN && sc->balance <= 0)
                sc->PrintReceipt(term, thisCheck, printer, term->FindDrawer());
            else
                sc->PrintReceipt(term, thisCheck, printer);
            return SIGNAL_OKAY;
        }
        else if (strcmp("credit", &message[6]) == 0 &&
                 current_payment != nullptr &&
                 current_payment->credit != nullptr)
        {
            Printer *printer = term->FindPrinter(PRINTER_RECEIPT);
            int pamount = (amount > 0 ? amount : -1);
            if (pamount == -1 && sc != nullptr && sc->total_cost > 0)
                pamount = sc->total_cost;
            current_payment->credit->PrintReceipt(term, RECEIPT_PICK, printer, pamount);
            return SIGNAL_OKAY;
        }
        break;
    case 16:  // Backspace
        if (sc && amount > 0)
        {
            amount /= 10;
            DrawPaymentEntry(term);
            return SIGNAL_OKAY;
        }
        break;
    case 17:  // Clear
        if (sc && amount > 0)
        {
            sc->tax_exempt.Clear();
            amount = 0;
            DrawPaymentEntry(term);
            voided = 0;
            return SIGNAL_OKAY;
        }
        break;
    case 18:  // nextcheck
        NextCheck(term, 0);
        return SIGNAL_OKAY;
    case 19:  // nextcheckforce
        NextCheck(term, 1);
        return SIGNAL_OKAY;
    case 20:  // Void
        if (sc && sc->PaymentList() == nullptr)
        {
            voided = 1 - voided;
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        break;
    case 21:  // undo
        if (settings->allow_cc_cancels == 0)
        {
            if (current_payment &&
                current_payment->credit &&
                current_payment->credit->IsAuthed(1))
            {
                confirm = new SimpleDialog(term->Translate("You cannot clear an authorized card."));
                confirm->Button(term->Translate("Okay"));
                term->OpenDialog(confirm);
                return SIGNAL_TERMINATE;
            }
            else if (current_payment == nullptr &&
                     sc && sc->HasAuthedCreditCards())
            {
                confirm = new SimpleDialog(term->Translate("You cannot clear all entries with authorized cards."));
                confirm->Button(term->Translate("Okay"));
                term->OpenDialog(confirm);
                return SIGNAL_TERMINATE;
            }
        }
        // Undo and UndoCofirmed allow us to have a confirmation dialog.  For
        // now, just prevent undoing with authed credit cards.  This should
        // also be the case for the cancel message, but I haven't figured out
        // how to do that yet.
    /* fallthrough */
    case 22:  // undoconfirmed
        if (current_payment != nullptr)
        {
            //FIX->BAK verify this code does not cause other problems!
            if (current_payment->tender_type == TENDER_CHARGED_TIP)
            {
                Payment *currpay = sc->PaymentList();
                while (currpay != nullptr)
                {
                    if (currpay->credit != nullptr)
                    {
                        currpay->credit->Tip(0);
                        currpay = nullptr;
                    }
                    else
                        currpay = currpay->next;
                }
            }
            sc->Remove(current_payment);
            if (current_payment->credit != nullptr)
            {
                if (!current_payment->credit->IsVoided() &&
                    !current_payment->credit->IsRefunded())
                {
                    MasterSystem->cc_exception_db->Add(term, current_payment->credit->Copy());
                }
            }
            delete current_payment;
            current_payment = nullptr;
            sc->FigureTotals(settings);
            if (sc->IsBalanced())
            {
                term->check_balanced = 1;
                term->Draw(1);
            }
            else
            {
                term->check_balanced = 0;
                term->Draw(1);
            }
            // Notify other terminals that check data has changed
            term->UpdateOtherTerms(UPDATE_CHECKS, nullptr);
            return SIGNAL_OKAY;
        }
        else if (sc)
        {
            drawer_open = 0;
            voided = 0;
            sc->UndoPayments(term, employee);
            amount = 0;
            current_payment = nullptr;
            if (sc->IsBalanced())
                term->check_balanced = 1;
            else
                term->check_balanced = 0;
            term->Draw(1);
            // Notify other terminals that check data has changed
            term->UpdateOtherTerms(UPDATE_CHECKS, nullptr);
            return SIGNAL_OKAY;
        }
        break;
    case 23:  // merchandise
        Merchandise(term, sc);
        return SIGNAL_OKAY;
    case 24:  // done
        DoneWithCheck(term);
        return SIGNAL_OKAY;
    case 25:  // 10
    case 26:  // 20
        if (sc && sc->status == CHECK_OPEN && amount < 1000000)
        {
            amount = amount + (atoi(message) * 100);
            DrawPaymentEntry(term);
            return SIGNAL_OKAY;
        }
        return SIGNAL_OKAY;
    case 27:  // taxexempt
        textdiag = new GetTextDialog(GlobalTranslate("Tax Exempt Qualification"), "settaxexempt", 50);
        term->OpenDialog(textdiag);
        return SIGNAL_OKAY;
    case 28:  // settaxexempt
        if (sc)
        {
            idx = 0;
            while (message[idx] != ' ')
                idx += 1;
            sc->tax_exempt.Set(&message[idx + 1]);
            sc->FigureTotals(settings);
            return SIGNAL_OKAY;
        }
        else
            return SIGNAL_IGNORED;
    case 29:  // ccamountchanged
        if (sc)
        {
            if (current_payment != nullptr && current_payment->credit != nullptr)
            {
                if (current_payment->credit->IsVoided() ||
                    current_payment->credit->IsRefunded())
                {
                    if (current_payment->flags & TF_FINAL)
                        current_payment->flags -= TF_FINAL;
                    Payment *currpay = sc->PaymentList();
                    while (currpay != nullptr)
                    {
                        if (currpay->tender_type == TENDER_CHARGED_TIP)
                        {
                            sc->Remove(currpay);
                            delete currpay;
                            currpay = nullptr;
                        }
                        else
                            currpay = currpay->next;
                    }
                    sc->status = CHECK_OPEN;
                }
            }
            current_payment = nullptr;
            sc->FigureTotals(settings);
            if (sc->IsBalanced())
                term->check_balanced = 1;
            else
                term->check_balanced = 0;
            return SIGNAL_OKAY;
        }
        else
            return SIGNAL_IGNORED;
    case 30:  // ccrefund
        AddPayment(term, 0, nullptr);
        return SIGNAL_OKAY;
    case 31:  // save
        SaveCheck(term);
        return SIGNAL_OKAY;
    default:
        if (sc && idx >= 0 && idx <= 9 &&
            sc->status == CHECK_OPEN && amount < 1000000)
        {
            amount = (amount * 10) + idx;
            DrawPaymentEntry(term);
            return SIGNAL_OKAY;
        }
        break;
    }
    return SIGNAL_IGNORED;
}

SignalResult PaymentZone::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("PaymentZone::Keyboard()");
    if (my_key == 8)
        return Signal(term, "backspace");

    genericChar str[] = {(char) my_key, '\0'};
    return Signal(term, str);
}

SignalResult PaymentZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("PaymentZone::Touch()");
    Check *check = term->check;
    Flt min_line;
    Flt max_line;
    int name_len = 0;

    if (check == nullptr)
        return SIGNAL_IGNORED;

    SubCheck *sc = &work_sub;
    if (sc == nullptr)
        return SIGNAL_IGNORED;

    LayoutZone::Touch(term, tx, ty);

    Flt line = header_size;
    Flt buffer = ((spacing - 1) / 2);
    for (Payment *payment = sc->PaymentList(); payment != nullptr; payment = payment->next)
    {
        name_len = 0;
        min_line = line - buffer;
        if (payment->credit != nullptr)
        {
            max_line = line + (spacing * 3);
            if (strlen(payment->credit->Name()) > 0)
            {
                max_line += 1;
                name_len = 1;
            }
        }
        else
            max_line = line + spacing;
        max_line -= buffer;
        if (selected_y > min_line && selected_y < max_line)
        {
            if (current_payment == payment)
                current_payment = nullptr;
            else
                current_payment = payment;
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        else
        {
            if (payment->credit != nullptr)
            {
                line += (spacing * 3);
                if (name_len)
                    line += 1;
            }
            else
                line += spacing;
        }
    }
    if (current_payment != nullptr && current_payment->credit != nullptr)
    {
        char str[STRLENGTH] = "";
        vt_safe_string::safe_concat(str, STRLENGTH, "Would you like to print the receipt for\\");
        vt_safe_string::safe_concat(str, STRLENGTH, "the credit card or the subcheck?");
        SimpleDialog *sd = new SimpleDialog(str);
        sd->Button("Credit Card", "print credit");
        sd->Button("SubCheck", "print subcheck");
        sd->Button("Cancel", "noprint");
        term->OpenDialog(sd);
        return SIGNAL_OKAY;
    }
    else
        return Signal(term, "print subcheck");

    return SIGNAL_IGNORED;
}

int PaymentZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    FnTrace("PaymentZone::Update()");

    if (update_message & UPDATE_AUTHORIZE)
    {
        Settings *settings = term->GetSettings();
        if (term->check)
            term->check->Update(settings);
        term->KillDialog();
        Draw(term, 1);
        return 0;
    }
    else if (update_message & UPDATE_ORDERS)
        return Draw(term, 1);
    else
        return 0;
}

int PaymentZone::RenderPaymentEntry(Terminal *term)
{
    FnTrace("PaymentZone::RenderPaymentEntry()");

    TextPosL(term, 2, input_line, "Input Amount:");
    Entry(term, 17, input_line, 8.5);
    TextPosR(term, 25.5, input_line, term->FormatPrice(amount), COLOR_YELLOW);
    return 0;
}

int PaymentZone::DrawPaymentEntry(Terminal *term)
{
    FnTrace("PaymentZone::DrawPaymentEntry()");

    RenderPaymentEntry(term);
    term->UpdateArea(x, y, w, h);

    return 0;
}

int PaymentZone::SaveCheck(Terminal *term)
{
    FnTrace("PaymentZone::SaveCheck()");
    int retval = 0;
    Employee *employee = term->user;
    Check    *currCheck = term->check;
    System *sys = term->system_data;
    Settings *settings = &(sys->settings);
    SubCheck *subCheck = &work_sub;

    if (employee == nullptr || currCheck == nullptr || currCheck->current_sub == nullptr)
        return 1;

    currCheck->current_sub->Copy(subCheck, settings);
    currCheck->Update(settings);

    return retval;
}

int PaymentZone::CloseCheck(Terminal *term, int force)
{
    FnTrace("PaymentZone::CloseCheck()");
    Employee *employee = term->user;
    Check    *currCheck = term->check;

    if (force == 0)
    {
        if (work_sub.balance != 0)
        {
            SimpleDialog *sd = new SimpleDialog(term->Translate("The check still has a balance which should be paid in full prior to leaving this page."));
            sd->Button("Cancel", "nevermind");
            sd->Button("Hold Anyway", "finalize2");
            return term->OpenDialog(sd);
        }
        else if (term->is_bar_tab == 0)
        {  // do we have any pre-auths left?
            Payment *currpay = work_sub.PaymentList();
            int has_preauths = 0;
            while (currpay != nullptr && has_preauths == 0)
            {
                if (currpay->credit && currpay->credit->IsPreauthed())
                    has_preauths = 1;
                currpay = currpay->next;
            }
            if (has_preauths)
            {
                SimpleDialog *sd = new SimpleDialog(term->Translate("There are one or more Pre-Authorizations which should be processed prior to leaving this page."));
                sd->Button("Cancel", "nevermind");
                sd->Button("Finalize Anyway", "finalize2");
                return term->OpenDialog(sd);
            }
        }
    }
    
    if (employee == nullptr || currCheck == nullptr || currCheck->current_sub == nullptr)
        return 1;

    if (term->cdu != nullptr)
    {
        term->cdu->Clear();
        term->cdu->ShowString(&(term->system_data->cdustrings));
        term->cdu->Refresh(15);
    }

    currCheck->termname = term->name;

    System *sys = term->system_data;
    Settings *settings = &(sys->settings);
    SubCheck *subCheck = &work_sub;
    amount = 0;

    if (voided)
    {
        subCheck->Void();
        voided = 0;
    }

    // Make changes final in check
    int old_status = currCheck->current_sub->status;
    currCheck->current_sub->Copy(subCheck, settings);
    currCheck->Update(settings);

    // Leave hotel checks open
    if (currCheck->CustomerType() == CHECK_HOTEL)
    {
        if (currCheck->Settle(term))
            return 0;
        return DoneWithCheck(term);
    }

    // Try to close check
    int close_error = currCheck->Close(term);
    if (! close_error)
    {
        Drawer *drawer = term->FindDrawer();
        if (drawer == nullptr && !currCheck->IsTraining() &&
            !(subCheck->OnlyCredit() == 1 && term->is_bar_tab == 1))
        {
            // Get descriptive reason for drawer unavailability
            Settings *sett = term->GetSettings();  // Renamed to avoid shadowing outer 'settings'
            const char* reason = nullptr;
            if (term->user && term->user->CanSettle(sett))
            {
                int dm = sett->drawer_mode;
                Drawer *d = term->system_data->FirstDrawer();
                
                switch (dm)
                {
                case DRAWER_NORMAL:
                {
                    bool found_terminal_drawer = false;
                    while (d)
                    {
                        if (d->IsOpen() && d->term == term)
                        {
                            found_terminal_drawer = true;
                            break;
                        }
                        d = d->next;
                    }
                    if (!found_terminal_drawer)
                        reason = "No drawer available: No drawer is attached to this terminal in Trusted mode";
                    break;
                }
                case DRAWER_SERVER:
                {
                    bool any_drawers = false;
                    d = term->system_data->FirstDrawer();
                    while (d)
                    {
                        if (d->IsOpen())
                        {
                            any_drawers = true;
                            break;
                        }
                        d = d->next;
                    }
                    if (!any_drawers)
                        reason = "No drawer available: No drawers are configured in Server Bank mode";
                    else
                        reason = "No drawer available: Unable to create Server Bank drawer for this user";
                    break;
                }
                case DRAWER_ASSIGNED:
                {
                    bool found_assigned = false;
                    bool found_available = false;
                    d = term->system_data->FirstDrawer();
                    while (d)
                    {
                        if (d->IsOpen())
                        {
                            if (d->owner_id == term->user->id)
                            {
                                found_assigned = true;
                                break;
                            }
                            if (d->term == term && d->owner_id == 0 && d->IsEmpty())
                                found_available = true;
                        }
                        d = d->next;
                    }
                    if (!found_assigned && !found_available)
                    {
                        if (term->drawer_count == 0)
                            reason = "No drawer available: No drawers are attached to this terminal in Assigned mode";
                        else
                            reason = "No drawer available: No drawers are assigned to this user or available for assignment";
                    }
                    break;
                }
                default:
                    reason = "No drawer available: Unknown drawer mode";
                }
            }
            
            if (reason == nullptr)
                reason = "No drawer available for payments";
            
            DialogZone *diag = new SimpleDialog(GlobalTranslate(reason));
            diag->Button(GlobalTranslate("Okay"));
            return term->OpenDialog(diag);
        }

        // If there is any cash and drawer is still closed, open the drawer
        int open_drawer = 0;
        if (drawer && drawer_open == 0)
        {
            Payment *pmnt = subCheck->FindPayment(TENDER_CASH);
            if (pmnt)
                open_drawer = 1;
        }

        // Update drawer record
        drawer_open = 0;
        Printer *pr = term->FindPrinter(PRINTER_RECEIPT);
        if (pr)
        {
            if (auto receipt_mode = vt::IntToEnum<ReceiptPrintType>(settings->receipt_print)) {
                if (*receipt_mode == ReceiptPrintType::OnFinalize || *receipt_mode == ReceiptPrintType::OnBoth) {
                    if (settings->cash_receipt || subCheck->OnlyCredit() == 0)
                    {
                        subCheck->PrintReceipt(term, currCheck, pr, drawer, open_drawer);
                    }
                }
            }
        }
    }

    // Mark rebuild exception if needed
    int rebuild = 0;
    if (old_status == CHECK_CLOSED && employee->CanRebuild(settings))
    {
        rebuild = 1;
        term->system_data->exception_db.AddRebuildException(term, currCheck);
    }

    if (currCheck->GetStatus() == CHECK_OPEN)
    {
        if (currCheck->SubCount() <= 1)
        {
            if (term->is_bar_tab == 0)
                return DoneWithCheck(term);
            else
                return DoneWithCheck(term, 0);
        }

        // Find next open subcheck in check
        SubCheck *nextsub = currCheck->NextOpenSubCheck();
        if (nextsub != nullptr)
        {
            if (nextsub->IsBalanced())
                term->check_balanced = 1;
            else
                term->check_balanced = 0;
            work_sub.Copy(nextsub, settings);
            current_payment = nullptr;
            term->Draw(1);
            return 0;
        }
        else
            return DoneWithCheck(term);
    }
    else
    {
        if (rebuild == 0 && currCheck->archive == nullptr)
        {
            // Move check to end of list
            sys->Remove(currCheck);
            sys->Add(currCheck);
        }
        return DoneWithCheck(term);
    }
}

/****
 * DoneWithCheck:  I can't think of any reason at this point why store_check
 *  should not be called.  It also serves the purpose of destroying the
 *  check if that is called for, so it seems to me that in all cases it
 *  handles the check properly (instead of leaving blank fastfood checks
 *  in the queue that need to be cleared manually for End of Day).
 ****/
int PaymentZone::DoneWithCheck(Terminal *term, int store_check)
{
    FnTrace("PaymentZone::DoneWithCheck()");
	Check *currCheck = term->check;
	if (currCheck == nullptr)
		return 1;

	Settings *settings = term->GetSettings();
	if (store_check)
	{
		term->StoreCheck(0);
		term->UpdateOtherTerms(UPDATE_CHECKS, nullptr);
	}

	// Check if payment was made through Server Bank drawer by Customer user on SelfOrder terminal
	Drawer *drawer = term->FindDrawer();
	if (drawer && drawer->IsServerBank() && 
	    term->type == TERMINAL_SELFORDER && 
	    term->user && term->user->id == 999) // Customer user has ID 999
	{
		// Server Bank payments by Customer user on SelfOrder should return to Page -2 (PAGEID_LOGIN2)
		term->timeout = settings->delay_time2;
		term->Jump(JUMP_STEALTH, -2); // jump to page -2
		return 0;
	}

	switch (term->type)
	{
    case TERMINAL_BAR:
    case TERMINAL_BAR2:
        if (term->is_bar_tab)
            term->JumpToIndex(INDEX_BAR);
        else
            term->Jump(JUMP_HOME, 0);
        break;
    case TERMINAL_FASTFOOD:
        if (term->is_bar_tab)
            term->JumpToIndex(INDEX_BAR);
        else
        {
            term->timeout = settings->delay_time2;
            term->Jump(JUMP_STEALTH, -1); // jump to login page (not yet implemented)
        }
        break;
    default:
        term->timeout = settings->delay_time2;  // super short timeout
        term->Jump(JUMP_HOME);
        break;
	}
	return 0;
}

/****
 * AddPayment:  Sooner or later, all payments come here.  Credit Cards, et al, will
 *  start with the following method, but that only spawns a CreditCardDialog to
 *  gather the details (swipe the card, authorize, and so on).  CreditCardDialog
 *  will then signal this method to finish the job and do the "Add".  So until
 *  this method is called, the real SubCheck (not work_sub) should not change.
 ****/
int PaymentZone::AddPayment(Terminal *term, int ptype, int pid, int pflags, int pamount)
{
    FnTrace("PaymentZone::AddPayment()");
    Employee  *employee = term->user;
    Check     *currCheck = term->check;
    Settings  *settings = term->GetSettings();
    SubCheck  *subCheck = nullptr;

    if (employee == nullptr || currCheck == nullptr)
        return 1;

    subCheck = &work_sub;
    if (subCheck->status != CHECK_OPEN)
        return 1;

    if (subCheck->OrderList() == nullptr && term->is_bar_tab == 0)
    {
        amount = 0;
        Draw(term, 0);
        return 1;
    }

    if (amount == 0 && pamount == 0)
    {
        if (ptype == TENDER_CAPTURED_TIP || ptype == TENDER_CHARGED_TIP)
        {
            int change_value = subCheck->TotalPayment(TENDER_CHANGE);
            if (change_value == 0)
                return 1;
            pamount = change_value;
        }
        else if (subCheck->balance > 0)
            pamount = subCheck->balance;
        else if (subCheck->TabRemain() > 0)
            pamount = subCheck->SettleTab(term, ptype, pid, pflags);
        else
            return 1;
    }

    if (pamount == 0)
    {
        pamount = amount;
        if (pamount == 0)
            return 1;
    }

    Drawer *drawer = term->FindDrawer();
    if (drawer == nullptr && !currCheck->IsTraining() &&
        !(subCheck->OnlyCredit() == 1 && term->is_bar_tab == 1))
    {
        // Get descriptive reason for drawer unavailability
        Settings *sett = term->GetSettings();  // Renamed to avoid shadowing outer 'settings'
        const char* reason = nullptr;
        if (term->user && term->user->CanSettle(sett))
        {
            int dm = sett->drawer_mode;
            Drawer *d = term->system_data->FirstDrawer();
            
            switch (dm)
            {
            case DRAWER_NORMAL:
            {
                bool found_terminal_drawer = false;
                while (d)
                {
                    if (d->IsOpen() && d->term == term)
                    {
                        found_terminal_drawer = true;
                        break;
                    }
                    d = d->next;
                }
                if (!found_terminal_drawer)
                    reason = "No drawer available: No drawer is attached to this terminal in Trusted mode";
                break;
            }
            case DRAWER_SERVER:
            {
                bool any_drawers = false;
                d = term->system_data->FirstDrawer();
                while (d)
                {
                    if (d->IsOpen())
                    {
                        any_drawers = true;
                        break;
                    }
                    d = d->next;
                }
                if (!any_drawers)
                    reason = "No drawer available: No drawers are configured in Server Bank mode";
                else
                    reason = "No drawer available: Unable to create Server Bank drawer for this user";
                break;
            }
            case DRAWER_ASSIGNED:
            {
                bool found_assigned = false;
                bool found_available = false;
                d = term->system_data->FirstDrawer();
                while (d)
                {
                    if (d->IsOpen())
                    {
                        if (d->owner_id == term->user->id)
                        {
                            found_assigned = true;
                            break;
                        }
                        if (d->term == term && d->owner_id == 0 && d->IsEmpty())
                            found_available = true;
                    }
                    d = d->next;
                }
                if (!found_assigned && !found_available)
                {
                    if (term->drawer_count == 0)
                        reason = "No drawer available: No drawers are attached to this terminal in Assigned mode";
                    else
                        reason = "No drawer available: No drawers are assigned to this user or available for assignment";
                }
                break;
            }
            default:
                reason = "No drawer available: Unknown drawer mode";
            }
        }
        
        if (reason == nullptr)
            reason = "No drawer available for payments";
        
        DialogZone *my_drawer = new SimpleDialog(GlobalTranslate(reason));
        my_drawer->Button(GlobalTranslate("Okay"));
        return term->OpenDialog(my_drawer);
    }

    if (term->is_bar_tab)
        pflags |= TF_IS_TAB;

    Payment *paymnt = subCheck->NewPayment(ptype, pid, pflags, pamount);
    if (paymnt == nullptr)
        return 1;

    if (paymnt->tender_type == TENDER_CREDIT_CARD ||
        paymnt->tender_type == TENDER_DEBIT_CARD)
    {
        paymnt->credit = term->credit;
        paymnt->credit->check_id = currCheck->serial_number;
        term->credit = nullptr;
    }

    amount = 0;
    paymnt->user_id = employee->id;
    if (drawer)
        paymnt->drawer_id = drawer->serial_number;
    subCheck->ConsolidatePayments(settings);
    if (subCheck->IsBalanced())
        term->check_balanced = 1;
    else
        term->check_balanced = 0;
    term->Draw(1);

    current_payment = nullptr;
    paymnt = nullptr;

    if (ptype == TENDER_CASH)
        OpenDrawer(term);
    else if (ptype == TENDER_CHARGE_ROOM)
    {
        TenKeyDialog *room_num = new TenKeyDialog(GlobalTranslate("Enter a Room Number"), 0, 0);
        room_num->target_zone = this;
        term->OpenDialog(room_num);
    }

    return 0;
}

/****
 * AddPayment:  Most of the credit card stuff will actually be processed by
 *  the CreditCardDialogZone, to try to make it easier to manage authorizations,
 *  voids, etc. all on the payment page.  Here, we simply trap appropriate signals
 *  and pop up the CreditCardDialogZone if we get a swipe.  This way, the user
 *  can touch the TenderZone for Credit Cards, or the user can simply swipe.
 ****/
int PaymentZone::AddPayment(Terminal *term, int ptype, const genericChar* swipe_value)
{
    FnTrace("PaymentZone::AddPayment(credit card)");
    int retval = 0;
    CreditCardDialog *ccd = nullptr;
    SimpleDialog *sd = nullptr;
    Settings *settings = term->GetSettings();
    Payment *currpay = work_sub.PaymentList();
    int count = 0;
    char str[STRLENGTH];
    char str1[STRLENGTH];

    if (work_sub.status == CHECK_CLOSED && current_payment == nullptr)
    {
        sd = new SimpleDialog(term->Translate("You cannot add a charge card to a closed check."));
        sd->Button("Okay");
        retval = term->OpenDialog(sd);
    }
    else
    {
        if (term->credit != nullptr)
            ReportError("Possibly losing a credit card in PaymentZone::AddPayment()");
        term->credit = nullptr;
        int len = (swipe_value != nullptr) ? static_cast<int>(strlen(swipe_value)) : 0;
        // Arbitrary.  We'll assume there will never be more than 99 credit
        // cards added to a ticket.  Really, len == 1 should be a valid
        // assumption, but we'll go higher than that (we're trying to
        // distinguish between an index into the payment list and a credit
        // card number).
        if (len == 1 || len == 2)
        {
            count = atoi(swipe_value);
            currpay = work_sub.PaymentList();
            while (currpay != nullptr && count > 0)
            {
                if (currpay->credit != nullptr)
                    count -= 1;
                if (count == 0)
                {
                    current_payment = currpay;
                    term->credit = currpay->credit;
                }
                else
                    currpay = currpay->next;
            }
        }
        else if (len == 0 &&
                 current_payment != nullptr &&
                 current_payment->credit != nullptr)
        {
            term->credit = current_payment->credit;
        }
        else if (len == 0)
        {
            while (currpay != nullptr)
            {
                if (currpay->credit != nullptr)
                    count += 1;
                currpay = currpay->next;
            }
            if (count > 0)
            {
                sd = new SimpleDialog(term->Translate("Please select a card to process."), 1);
                sd->Button("New Card", "swipe newcard");
                currpay = work_sub.PaymentList();
                count = 0;
                while (currpay != nullptr)
                {
                    if (currpay->credit != nullptr)
                    {
                        count += 1;
                        vt::cpp23::format_to_buffer(str, STRLENGTH, "{}\\{}",
                                 currpay->credit->LastFour(),
                                 currpay->credit->Approval());
                        vt::cpp23::format_to_buffer(str1, STRLENGTH, "swipe {}", count);
                        sd->Button(str, str1);
                    }
                    currpay = currpay->next;
                }
                sd->Button("Cancel", "nocard");
                sd->target_zone = this;
                term->OpenDialog(sd);
                return retval;
            }
        }
        
        if (term->check->current_sub != nullptr)
            term->check->current_sub->FigureTotals(term->GetSettings());
        if (amount == 0 && term->credit != nullptr)
        {
            if (term->credit->IsPreauthed() &&
                settings->cc_bar_mode == 1 &&
                term->check->current_sub != nullptr)
            {
                term->auth_amount = term->check->current_sub->balance;
            }
            else if (term->check->current_sub != nullptr && term->check->current_sub->TabRemain() > 0)
                term->auth_amount = term->check->current_sub->balance;
            else
                term->auth_amount = term->credit->Total(1);
            term->void_amount = term->credit->Total(1);
        }
        else
        {
            term->auth_amount = amount;
            term->void_amount = amount;
        }
        if (strlen(swipe_value) > 2 && strcmp(swipe_value, "newcard") != 0)
            ccd = new CreditCardDialog(term, &work_sub, swipe_value);
        else
            ccd = new CreditCardDialog(term, &work_sub, nullptr);
        retval = term->OpenDialog(ccd);
    }

    return retval;
}

int PaymentZone::NextCheck(Terminal *term, int force)
{
    FnTrace("PaymentZone::NextCheck()");

    Check *check = term->check;
    if (check == nullptr)
        return 1;

    SubCheck *sc = check->current_sub;
    if (sc == nullptr)
        return 1;

    SubCheck *sc_next = sc->next;
    if (sc_next == nullptr)
        sc_next = check->SubList();
    if (sc == sc_next)
        return 1;

    if (! sc->IsEqual(&work_sub) && force == 0)
    {
        SimpleDialog *dialog =
            new SimpleDialog(term->Translate("You will lose your changes if you go to the next check now.\\Are you sure you want to do this?"));
        dialog->Button("Discard my changes\\Go to the next check", "nextcheckforce");
        dialog->Button("No, wait!\\I want to keep my changes");
        term->OpenDialog(dialog);
        return 0;
    }

    Settings *settings = term->GetSettings();
    drawer_open    = 0;
    amount         = 0;
    voided         = 0;
    check->current_sub = sc_next;
    work_sub.Copy(sc_next, settings);
    // we have to do a term->Draw(1) to make sure any conditionals get
    // retested and redrawn (see has_payments and check_balanced in
    // ConditionalZone).
    term->Draw(1);

    return 0;
}

int PaymentZone::Merchandise(Terminal *term, SubCheck *sc)
{
    FnTrace("PaymentZone::Merchandise()");

    if (sc == nullptr)
        return 1;

    int price = amount;
    amount = 0;
    if (price <= 0)
        return 1;

    Order *o = new Order("Merchandise", price);
    if (sc->Add(o))
    {
        delete o;
        return 1;
    }

    Settings *settings = term->GetSettings();
    sc->FigureTotals(settings);
    Draw(term, 1);
    return 0;
}

int PaymentZone::OpenDrawer(Terminal *term)
{
    FnTrace("PaymentZone::OpenDrawer()");
    int retval = 1;
    Drawer *drawer = term->FindDrawer();
    if (drawer == nullptr)
        return retval;

    if (drawer_open == 0)
    {
        drawer_open = 1;
        retval = drawer->Open();
    }

    return retval;
}


/**** TenderZone Class ****/
// Constructor
TenderZone::TenderZone()
{
    FnTrace("TenderZone::TenderZone()");

    tender_type = TENDER_CASH;
    amount      = 0;
}

// Member Functions
int TenderZone::RenderInit(Terminal *term, int update_flag)
{
    FnTrace("TenderZone::RenderInit()");
    Employee *employee = term->user;
    Settings *settings = term->GetSettings();

    active = 1;
    switch (tender_type)
    {
    case TENDER_EMPLOYEE_MEAL:
        active = (settings->MealCount(ALL_MEDIA, ACTIVE_MEDIA) > 0); break;
    case TENDER_DISCOUNT:
        active = (settings->DiscountCount(ALL_MEDIA, ACTIVE_MEDIA) > 0); break;
    case TENDER_COMP:
        active = (settings->CompCount(ALL_MEDIA, ACTIVE_MEDIA) > 0);
        if (employee == nullptr || !employee->CanCompOrder(settings))
            active = 0;
        break;
    case TENDER_CHARGE_CARD:
        active = 0;
        if (settings->authorize_method != CCAUTH_NONE ||
            settings->CreditCardCount(ALL_MEDIA, ACTIVE_MEDIA) > 0)
        {
            active = 1;
        }
        break;
    case TENDER_COUPON:
        active = (settings->CouponCount(ALL_MEDIA, ACTIVE_MEDIA) > 0); break;
    }
    return 0;
}

SignalResult TenderZone::Touch(Terminal *term, int tx, int ty)
{
	FnTrace("TenderZone::Touch()");
    SignalResult retval = SIGNAL_OKAY;
	Employee *employee = term->user;
	char str[256];
	Settings *settings = term->GetSettings();
	int count = 0;
    Drawer *drawer = nullptr;

	if (employee == nullptr)
		return SIGNAL_IGNORED;

	switch (tender_type)
	{
    case TENDER_CAPTURED_TIP:
    {
        int tt = tender_type;
        if (term->check && term->check->current_sub)
        {
            Credit *cr = term->check->current_sub->CurrentCredit();
            if (cr && cr->GetStatus() == 1)
            {
                tt = TENDER_CHARGED_TIP;
                cr->Tip(amount);
            }
        }
        vt_safe_string::safe_format(str, 256, "tender %d 0 0 %d", tt, amount);
        retval = term->Signal(str, group_id);
    }
    break;
    case TENDER_EMPLOYEE_MEAL:
    {
        MealInfo *mi = settings->MealList(), *ptr = nullptr;
        while (mi)
        {
            if (!(mi->flags & TF_MANAGER) || employee->IsManager(settings))
            {
                ptr = mi;
                ++count;
            }
            mi = mi->next;
        }

        if (count == 1 && ptr)
        {
            vt_safe_string::safe_format(str, 256, "tender %d %d %d %d", TENDER_EMPLOYEE_MEAL,
                    ptr->id, ptr->flags, ptr->amount);
            retval = term->Signal(str, group_id);
        }

        SimpleDialog *dialog = new SimpleDialog(term->Translate("Select An Employee Meal"), 1);
        mi = settings->MealList();
        while (mi)
        {
            if (mi->active)
            {
                if (!(mi->flags & TF_MANAGER) || employee->IsManager(settings))
                {
                    vt_safe_string::safe_format(str, 256, "tender %d %d %d %d", TENDER_EMPLOYEE_MEAL,
                            mi->id, mi->flags, mi->amount);
                    dialog->Button(mi->name.Value(), str);
                }
            }
            mi = mi->next;
        }
        term->OpenDialog(dialog);
    }
    break;
    case TENDER_CHARGE_CARD:
    {
        drawer = term->FindDrawer();
        if (drawer == nullptr && term->is_bar_tab == 0)
        {
            // Get descriptive reason for drawer unavailability
            Settings *sett = term->GetSettings();  // Renamed to avoid shadowing outer 'settings'
            const char* reason = nullptr;
            if (term->user && term->user->CanSettle(sett))
            {
                int dm = sett->drawer_mode;
                Drawer *d = term->system_data->FirstDrawer();
                
                switch (dm)
                {
                case DRAWER_NORMAL:
                {
                    bool found_terminal_drawer = false;
                    while (d)
                    {
                        if (d->IsOpen() && d->term == term)
                        {
                            found_terminal_drawer = true;
                            break;
                        }
                        d = d->next;
                    }
                    if (!found_terminal_drawer)
                        reason = "No drawer available: No drawer is attached to this terminal in Trusted mode";
                    break;
                }
                case DRAWER_SERVER:
                {
                    bool any_drawers = false;
                    d = term->system_data->FirstDrawer();
                    while (d)
                    {
                        if (d->IsOpen())
                        {
                            any_drawers = true;
                            break;
                        }
                        d = d->next;
                    }
                    if (!any_drawers)
                        reason = "No drawer available: No drawers are configured in Server Bank mode";
                    else
                        reason = "No drawer available: Unable to create Server Bank drawer for this user";
                    break;
                }
                case DRAWER_ASSIGNED:
                {
                    bool found_assigned = false;
                    bool found_available = false;
                    d = term->system_data->FirstDrawer();
                    while (d)
                    {
                        if (d->IsOpen())
                        {
                            if (d->owner_id == term->user->id)
                            {
                                found_assigned = true;
                                break;
                            }
                            if (d->term == term && d->owner_id == 0 && d->IsEmpty())
                                found_available = true;
                        }
                        d = d->next;
                    }
                    if (!found_assigned && !found_available)
                    {
                        if (term->drawer_count == 0)
                            reason = "No drawer available: No drawers are attached to this terminal in Assigned mode";
                        else
                            reason = "No drawer available: No drawers are assigned to this user or available for assignment";
                    }
                    break;
                }
                default:
                    reason = "No drawer available: Unknown drawer mode";
                }
            }
            
            if (reason == nullptr)
                reason = "No drawer available for payments";
            
            DialogZone *diag = new SimpleDialog(GlobalTranslate(reason));
            diag->Button(GlobalTranslate("Okay"));
            term->OpenDialog(diag);
            return SIGNAL_OKAY;
        }
        else if ((term->user->training == 0) &&
                   (settings->authorize_method == CCAUTH_MAINSTREET ||
                    settings->authorize_method == CCAUTH_CREDITCHEQ))
        {
            term->Signal("swipe ", group_id);
        }
        else
        {
            SimpleDialog *dialog = new SimpleDialog(term->Translate("Select A Credit Card"), 1);
            CreditCardInfo *cc = settings->CreditCardList();
            while (cc)
            {
                if (cc->active)
                {
                    vt_safe_string::safe_format(str, 256, "tender %d %d", TENDER_CHARGE_CARD, cc->id);
                    dialog->Button(cc->name.Value(), str);
                }
                cc = cc->next;
            }
            term->OpenDialog(dialog);
        }
    }
    break;
    case TENDER_DISCOUNT:
    {
        SimpleDialog *dialog = new SimpleDialog(term->Translate("Select A Discount"), 1);
        DiscountInfo *ds = settings->DiscountList();
        while (ds)
        {
            if (ds->active)
            {
                vt_safe_string::safe_format(str, 256, "tender %d %d %d %d", TENDER_DISCOUNT,
                        ds->id, ds->flags, ds->amount);
                dialog->Button(ds->name.Value(), str);
            }
            ds = ds->next;
        }
        term->OpenDialog(dialog);
    }
    break;
    case TENDER_COUPON:
    {
        SimpleDialog *dialog = new SimpleDialog(term->Translate("Select A Coupon"), 1);
        CouponInfo *cp = settings->CouponList();
        int applies = 0;
        int cp_count = 0;
        while (cp)
        {
            applies = 0;
            if (cp->active && term->check && term->check->current_sub)
                applies = cp->Applies(term->check->current_sub);
            if (applies)
            {
                cp_count += 1;
                vt_safe_string::safe_format(str, 256, "tender %d %d %d %d", TENDER_COUPON,
                        cp->id, cp->flags, cp->amount);
                dialog->Button(cp->name.Value(), str);
            }
            cp = cp->next;
        }
        if (cp_count > 0)
            term->OpenDialog(dialog);
        else
            delete dialog;
    }
    break;
    case TENDER_COMP:
    {
        SimpleDialog *dialog = new SimpleDialog(term->Translate("Select A Meal Comp"), 1);
        CompInfo *cm = settings->CompList();
        while (cm)
        {
            if (cm->active)
            {
                vt_safe_string::safe_format(str, 256, "tender %d %d %d %d", TENDER_COMP,
                        cm->id, cm->flags | TF_IS_PERCENT, 10000);
                dialog->Button(cm->name.Value(), str);
            }
            cm = cm->next;
        }
        term->OpenDialog(dialog);
    }
    break;
    default:
    {
        int flags = 0;
        if (tender_type == TENDER_GRATUITY)
            flags |= TF_IS_PERCENT;
        else if (tender_type == TENDER_CREDIT_CARD_FEE_PERCENT ||
                 tender_type == TENDER_DEBIT_CARD_FEE_PERCENT)
        {
            // Credit/Debit Card Fee (Percentage) always uses percentage
            flags |= TF_IS_PERCENT;
        }
        // Credit Card Fee (Dollar) uses default flags = 0 (dollar amount)
        vt_safe_string::safe_format(str, 256, "tender %d 0 %d %d", tender_type, flags, amount);
        retval = term->Signal(str, group_id);
    }
	} // end switch

	return retval;
}
