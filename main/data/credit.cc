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
 * credit.cc - revision 28 (9/10/98)
 * Implementation of credit module
 */

#include "check.hh"
#include "data_file.hh"
#include "labels.hh"
#include "layout_zone.hh"
#include "manager.hh"
#include "printer.hh"
#include "report.hh"
#include "report_zone.hh"
#include "system.hh"
#include "terminal.hh"
#include "credit.hh"
#include "utility.hh"
#include "src/utils/vt_logger.hh"
#include "safe_string_utils.hh"
#include "src/utils/cpp23_utils.hh"
#include <unistd.h>
#include <cctype>
#include <ctime>
#include <cstring>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define MAX_PAN_LEN  20  // really 19, but let's give one extra for kicks
#define MAX_NAM_LEN  27
#define COUNTRY_LEN  3
#define EXPIRE_LEN   4
#define SC_LEN       3
#define PVV_LEN      5
#define FC3_LEN      2
#define CURRENCY_LEN 3
#define AA_LEN       4
#define AR_LEN       4
#define CB_LEN       4
#define CL_LEN       2
#define PINCP_LEN    6
#define PANSR_LEN    2
#define FSANSR_LEN   2
#define SSANSR_LEN   2
#define CSCN_LEN     9
#define CCD_LEN      6
#define TD_LEN       4
#define AVV_LEN      8
#define ACSN_LEN     3
#define INIC_LEN     3

#define MAX_LOOPS  3


/**** Exported Varibles ****/
const char* CardTypeName[] = {
    "Credit Card", "Debit Card", "Gift Card", nullptr};
const char* CardTypeShortName[] = {
    "Credit", "Debit", "Gift", nullptr};
int CardTypeValue[] = {
    CARD_TYPE_CREDIT, CARD_TYPE_DEBIT, CARD_TYPE_GIFT, -1};
const char* CreditCardName[] = {
    "Visa", "MasterCard", "American Express", "Discover Card", "Diners Club",
    "JCB Card", nullptr};
const char* CreditCardShortName[] = {
    "Visa", "MC", "AMEX", "Discover", "Diners", "JCB", nullptr};
int CreditCardValue[] = {
    CREDIT_TYPE_VISA, CREDIT_TYPE_MASTERCARD, CREDIT_TYPE_AMEX, CREDIT_TYPE_DISCOVER,
    CREDIT_TYPE_DINERSCLUB, CREDIT_TYPE_JCB, -1};
const char* DebitAcctName[] = {
    "Checking", "Savings", nullptr};
int   DebitAcctValue[] = {
    DEBIT_ACCT_CHECKING, DEBIT_ACCT_SAVINGS, -1};

const char* CreditCardStateName[] = {
    "Pre-Authorized", "Authorized", "Pre-Auth Completed", "Voided", "Refunded",
    "Refund Cancelled", "Pre-Auth Adviced", nullptr };
int   CreditCardStateValue[] = {
    CCAUTH_PREAUTH, CCAUTH_AUTHORIZE, CCAUTH_COMPLETE, CCAUTH_VOID, CCAUTH_REFUND,
    CCAUTH_REFUND_CANCEL, CCAUTH_ADVICE, -1 };


/*********************************************************************
 * Credit Class
 ********************************************************************/

Credit::Credit()
{
    FnTrace("Credit::Credit()");

    fore = nullptr;
    next = nullptr;

    Clear();
}

Credit::Credit(const char* value)
{
    FnTrace("Credit::Credit(const char* )");

    fore = nullptr;
    next = nullptr;

    Clear();

    if (value != nullptr)
    {
        swipe.Set(value);
        valid = ParseSwipe(value);
    }
}

Credit::~Credit()
{
    FnTrace("Credit::~Credit()");
    errors_list.Purge();
}

int Credit::Clear(int safe_clear)
{
    FnTrace("Credit::Clear()");

    // There are certain things that should not be cleared if we're in
    // process.  Mostly, we want to clear everything.
    if (safe_clear == 0)
        card_type  = CARD_TYPE_NONE;

    card_id        = 0;
    db_type        = CC_DBTYPE_NONE;

    swipe.Set("");
    approval.Set("");
    number.Set("");
    name.Set("");
    country.Set("");
    expire.Set("");
    credit_type    = CREDIT_TYPE_UNKNOWN;
    debit_acct     = DEBIT_ACCT_NONE;

    forced         = 0;

    code.Set("");
    intcode = CC_STATUS_NONE;
    isocode.Set("");
    verb.Set("");
    auth.Set("");
    AVS.Set("");
    CV.Set("");
    batch = -1;
    item = -1;
    ttid = -1;
    trans_success  = 0;
    last_action    = CCAUTH_NOACTION;
    state          = CCAUTH_NOACTION;
    auth_state     = CCAUTH_NOACTION;
    processor      = MasterSystem->settings.authorize_method;

    read_manual    = 0;
    mn_pan[0]      = '\0';
    mn_expiry[0]   = '\0';

    read_t1        = 0;
    t1_fc          = '\0';
    t1_country[0]  = '\0';
    t1_pan[0]      = '\0';
    t1_name[0]     = '\0';
    t1_expiry[0]   = '\0';
    t1_sc[0]       = '\0';
    t1_pvv[0]      = '\0';

    read_t2        = 0;
    t2_pan[0]      = '\0';
    t2_country[0]  = '\0';
    t2_expiry[0]   = '\0';
    t2_sc[0]       = '\0';
    t2_pvv[0]      = '\0';
    t2_disc[0]     = '\0';

    read_t3        = 0;
    t3_fc[0]       = '\0';
    t3_pan[0]      = '\0';
    t3_country[0]  = '\0';
    t3_currency[0] = '\0';
    t3_ce          = '\0';
    t3_aa[0]       = '\0';
    t3_ar[0]       = '\0';
    t3_cb[0]       = '\0';
    t3_cl[0]       = '\0';
    t3_rc          = '\0';
    t3_pincp[0]    = '\0';
    t3_ic          = '\0';
    t3_pansr[0]    = '\0';
    t3_fsansr[0]   = '\0';
    t3_ssansr[0]   = '\0';
    t3_expiry[0]   = '\0';
    t3_csn         = '\0';
    t3_cscn[0]     = '\0';
    t3_fsan[0]     = '\0';
    t3_ssan[0]     = '\0';
    t3_rm          = '\0';
    t3_ccd[0]      = '\0';
    t3_td[0]       = '\0';
    t3_avv[0]      = '\0';
    t3_acsn[0]     = '\0';
    t3_inic[0]     = '\0';
    t3_disc[0]     = '\0';

    // specific to CreditCheq
    term_id.Set("");
    batch_term_id.Set("");
    reference.Set("");
    sequence.Set("");
    server_date.Set("");
    server_time.Set("");
    receipt_line.Set("");
    display_line.Set("");

    preauth_time.Clear();
    auth_time.Clear();
    void_time.Clear();
    refund_time.Clear();
    refund_cancel_time.Clear();
    settle_time.Clear();

    amount         = 0;
    tip            = 0;
    preauth_amount = 0;
    auth_amount    = 0;
    refund_amount  = 0;
    void_amount    = 0;

    auth_user_id   = 0;
    void_user_id   = 0;
    refund_user_id = 0;
    except_user_id = 0;
    check_id       = 0;

    valid          = 0;

    return 0;
}

int Credit::Read(InputDataFile &df, int version)
{
    FnTrace("Credit::Read()");
    int error = 0;
    int count;
    int idx;
    Credit *ecredit;

    error += df.Read(number);
    error += df.Read(expire);
    error += df.Read(name);
    error += df.Read(country);
    error += df.Read(approval);
    error += df.Read(forced);
    error += df.Read(code);
    error += df.Read(intcode);
    error += df.Read(isocode);
    error += df.Read(b24code);
    error += df.Read(verb);
    error += df.Read(auth);
    error += df.Read(AVS);
    error += df.Read(CV);
    error += df.Read(batch);
    error += df.Read(item);
    error += df.Read(ttid);
    error += df.Read(trans_success);
    error += df.Read(last_action);
    error += df.Read(state);
    error += df.Read(auth_state);
    error += df.Read(card_type);
    error += df.Read(credit_type);
    error += df.Read(debit_acct);

    // specific to CreditCheq
    error += df.Read(term_id);
    error += df.Read(batch_term_id);
    error += df.Read(reference);
    error += df.Read(sequence);
    error += df.Read(server_date);
    error += df.Read(server_time);
    error += df.Read(receipt_line);
    error += df.Read(display_line);

    error += df.Read(auth_user_id);
    error += df.Read(void_user_id);
    error += df.Read(refund_user_id);
    error += df.Read(except_user_id);
    error += df.Read(check_id);

    error += df.Read(amount);
    error += df.Read(tip);
    error += df.Read(preauth_amount);
    error += df.Read(auth_amount);
    error += df.Read(refund_amount);
    error += df.Read(void_amount);

    error += df.Read(processor);

    error += df.Read(preauth_time);
    error += df.Read(auth_time);
    error += df.Read(void_time);
    error += df.Read(refund_time);
    error += df.Read(refund_cancel_time);
    error += df.Read(settle_time);

    error += df.Read(count);
    if (count < 10000 && error == 0)
    {
        for (idx = 0; idx < count; idx += 1)
        {
            ecredit = new Credit();
            error += ecredit->Read(df, version);
            if (error)
            {
                delete ecredit;
                return error;
            }
            errors_list.AddToTail(ecredit);
        }
    }

    if (IsSettled() == 0)
        MasterSystem->AddBatch(batch);

    return error;
}

int Credit::Write(OutputDataFile &df, int version)
{
    FnTrace("Credit::Write()");
    int error = 0;
    char tmpnumber[STRLENGTH];
    Credit *ecredit = errors_list.Head();

    if (credit_type == CREDIT_TYPE_UNKNOWN)
        SetCreditType();

    if (IsPreauthed())
        vt_safe_string::safe_copy(tmpnumber, STRLENGTH, number.Value());
    else
        vt_safe_string::safe_copy(tmpnumber, STRLENGTH, PAN(MasterSystem->settings.save_entire_cc_num));
    error += df.Write(tmpnumber);
    error += df.Write(expire);
    error += df.Write(name);
    error += df.Write(country);
    error += df.Write(approval);
    error += df.Write(forced);
    error += df.Write(code);
    error += df.Write(intcode);
    error += df.Write(isocode);
    error += df.Write(b24code);
    error += df.Write(verb);
    error += df.Write(auth);
    error += df.Write(AVS);
    error += df.Write(CV);
    error += df.Write(batch);
    error += df.Write(item);
    error += df.Write(ttid);
    error += df.Write(trans_success);
    error += df.Write(last_action);
    error += df.Write(state);
    error += df.Write(auth_state);
    error += df.Write(card_type);
    error += df.Write(credit_type);
    error += df.Write(debit_acct);

    // specific to CreditCheq
    error += df.Write(term_id);
    error += df.Write(batch_term_id);
    error += df.Write(reference);
    error += df.Write(sequence);
    error += df.Write(server_date);
    error += df.Write(server_time);
    error += df.Write(receipt_line);
    error += df.Write(display_line);

    error += df.Write(auth_user_id);
    error += df.Write(void_user_id);
    error += df.Write(refund_user_id);
    error += df.Write(except_user_id);
    error += df.Write(check_id);

    error += df.Write(amount);
    error += df.Write(tip);
    error += df.Write(preauth_amount);
    error += df.Write(auth_amount);
    error += df.Write(refund_amount);
    error += df.Write(void_amount);

    error += df.Write(processor);

    error += df.Write(preauth_time);
    error += df.Write(auth_time);
    error += df.Write(void_time);
    error += df.Write(refund_time);
    error += df.Write(refund_cancel_time);
    error += df.Write(settle_time);

    error += df.Write(errors_list.Count());
    while (ecredit != nullptr)
    {
        ecredit->Write(df, version);
        ecredit = ecredit->next;
    }

    return error;
}

/****
 * AddError:  I want to keep a record of all errors so that we can find out,
 *  for instance, whether someone tried too many times to process the same
 *  credit card.  There may be no reason to do this, but if we keep the
 *  record we may be able to, in the future, track potential criminal
 *  activity (and we don't necessarily need to save ANY personal information
 *  from the customer to do this.
 ****/
int Credit::AddError(Credit *ecredit)
{
    FnTrace("Credit::AddError()");
    int retval = 0;

    if (this != ecredit)
        errors_list.AddToTail(ecredit);
    else if (debug_mode)
        printf("AddError:  %s is trying to add me to me\n", FnReturnLast());

    return retval;
}

Credit *Credit::Copy()
{
    FnTrace("Credit::Copy()");
    auto *newcredit = new Credit();
    Credit *ecredit;

    if (newcredit != nullptr)
    {
        newcredit->card_id = card_id;
        newcredit->db_type   = db_type;
        newcredit->number.Set(number);
        newcredit->expire.Set(expire);
        newcredit->name.Set(name);
        newcredit->country.Set(country);
        newcredit->approval.Set(approval);
        newcredit->amount = amount;
        newcredit->tip    = tip;
        newcredit->forced = forced;
        newcredit->code.Set(code);
        newcredit->intcode = intcode;
        newcredit->isocode.Set(isocode);
        newcredit->b24code.Set(b24code);
        newcredit->verb.Set(verb);
        newcredit->auth.Set(auth);
        newcredit->AVS.Set(AVS);
        newcredit->CV.Set(CV);
        newcredit->batch = batch;
        newcredit->item = item;
        newcredit->ttid = ttid;
        newcredit->trans_success = trans_success;
        newcredit->last_action = last_action;
        newcredit->state = state;
        newcredit->auth_state = auth_state;
        newcredit->card_type = card_type;
        newcredit->credit_type = credit_type;
        newcredit->debit_acct = debit_acct;

        // specific to CreditCheq
        newcredit->term_id.Set(term_id);
        newcredit->batch_term_id.Set(batch_term_id);
        newcredit->reference.Set(reference);
        newcredit->sequence.Set(sequence);
        newcredit->server_date.Set(server_date);
        newcredit->server_time.Set(server_time);
        newcredit->receipt_line.Set(receipt_line);
        newcredit->display_line.Set(display_line);

        newcredit->auth_user_id = auth_user_id;
        newcredit->void_user_id = void_user_id;
        newcredit->refund_user_id = refund_user_id;
        newcredit->except_user_id = except_user_id;
        newcredit->check_id = check_id;

        newcredit->amount = amount;
        newcredit->tip = tip;
        newcredit->preauth_amount = preauth_amount;
        newcredit->auth_amount = auth_amount;
        newcredit->refund_amount = refund_amount;
        newcredit->void_amount = void_amount;

        newcredit->processor = processor;

        newcredit->preauth_time.Set(preauth_time);
        newcredit->auth_time.Set(auth_time);
        newcredit->void_time.Set(void_time);
        newcredit->refund_time.Set(refund_time);
        newcredit->refund_cancel_time.Set(refund_cancel_time);
        newcredit->settle_time.Set(settle_time);

        newcredit->read_manual = read_manual;

        ecredit = errors_list.Head();
        while (ecredit != nullptr)
        {
            newcredit->AddError(ecredit);
            ecredit = ecredit->next;
        }

        if (newcredit->swipe.size() > 0)
            newcredit->ParseSwipe(newcredit->swipe.Value());
    }

    return newcredit;
}

int Credit::Copy(Credit *credit)
{
    FnTrace("Credit::Copy(Credit *)");
    Credit *ecredit;
    int retval = 1;

    if (credit != nullptr)
    {
        card_id = credit->card_id;
        db_type   = credit->db_type;
        number.Set(credit->number);
        expire.Set(credit->expire);
        name.Set(credit->name);
        country.Set(credit->country);
        approval.Set(credit->approval);
        forced = credit->forced;
        code.Set(credit->code);
        intcode = credit->intcode;
        isocode.Set(credit->isocode);
        b24code.Set(credit->b24code);
        verb.Set(credit->verb);
        auth.Set(credit->auth);
        AVS.Set(credit->AVS);
        CV.Set(credit->CV);
        batch = credit->batch;
        item = credit->item;
        ttid = credit->ttid;
        trans_success = credit->trans_success;
        last_action = credit->last_action;
        state = credit->state;
        auth_state = credit->auth_state;
        card_type = credit->card_type;
        credit_type = credit->credit_type;
        debit_acct = credit->debit_acct;

        // specific to CreditCheq
        term_id.Set(credit->term_id);
        batch_term_id.Set(credit->batch_term_id);
        reference.Set(credit->reference);
        sequence.Set(credit->sequence);
        server_date.Set(credit->server_date);
        server_time.Set(credit->server_time);
        receipt_line.Set(credit->receipt_line);
        display_line.Set(credit->display_line);

        auth_user_id = credit->auth_user_id;
        void_user_id = credit->void_user_id;
        refund_user_id = credit->refund_user_id;
        except_user_id = credit->except_user_id;
        check_id = credit->check_id;

        amount = credit->amount;
        tip = credit->tip;
        preauth_amount = credit->preauth_amount;
        auth_amount = credit->auth_amount;
        refund_amount = credit->refund_amount;
        void_amount = credit->void_amount;

        processor = credit->processor;

        preauth_time.Set(credit->preauth_time);
        auth_time.Set(credit->auth_time);
        void_time.Set(credit->void_time);
        refund_time.Set(credit->refund_time);
        refund_cancel_time.Set(credit->refund_cancel_time);
        settle_time.Set(credit->settle_time);

        read_manual = credit->read_manual;

        ecredit = credit->errors_list.Head();
        while (ecredit != nullptr)
        {
            AddError(ecredit);
            ecredit = ecredit->next;
        }

        if (swipe.size() > 0)
            ParseSwipe(swipe.Value());

        retval = 1;
    }

    return retval;
}

char* Credit::ReverseExpiry(char* expiry)
{
    char hold;

    hold = expiry[0];
    expiry[0] = expiry[2];
    expiry[2] = hold;

    hold = expiry[1];
    expiry[1] = expiry[3];
    expiry[3] = hold;

    return expiry;
}

int Credit::ValidateCardInfo()
{
    FnTrace("Credit::ValidateCardInfo()");
    int retval = 0;
    int valid_card = 0;
    int valid_expiry = 0;

    if (read_t1)
    {
        number.Set(t1_pan);
        name.Set(t1_name);
        expire.Set(ReverseExpiry(t1_expiry));
        if (strlen(t1_country) > 0)
            country.Set(t1_country);
    }
    else if (read_t2)
    {
        number.Set(t2_pan);
        name.Set("");
        expire.Set(ReverseExpiry(t2_expiry));
        if (strlen(t2_country) > 0)
            country.Set(t2_country);
    }
    else if (read_t3)
    {
        number.Set(t3_pan);
        name.Set("");
        expire.Set(ReverseExpiry(t3_expiry));
        if (strlen(t3_country) > 0)
            country.Set(t3_country);
    }
    else if (read_manual)
    {
        number.Set(mn_pan);
        name.Set("");
        expire.Set(mn_expiry);
        country.Set("");
    }

    if (number.size() > 0 && expire.size() > 0)
    {
        valid_card = CC_IsValidAccountNumber(number.Value());
        valid_expiry = CC_IsValidExpiry(expire.Value());
        if (valid_card && valid_expiry)
            retval = 1;
        else
        {
            ReportError(GlobalTranslate("Got a bad card for validation."));
            read_manual = 0;
            read_t1 = 0;
            read_t2 = 0;
            read_t3 = 0;
            verb.Set("Invalid Card Number");
        }
    }

    return retval;
}

int Credit::CanPrintSignature()
{
    FnTrace("Credit::CanPrintSignature()");
    int retval = 0;

    if (card_type == CARD_TYPE_CREDIT)
    {
        // Want last_action because we also print receipts for
        // declines.
        if (last_action == CCAUTH_PREAUTH ||
            last_action == CCAUTH_AUTHORIZE ||
            last_action == CCAUTH_REFUND_CANCEL ||
            last_action == CCAUTH_COMPLETE)
        {
            if (intcode == CC_STATUS_SUCCESS ||
                intcode == CC_STATUS_AUTH)
            {
                retval = 1;
            }
        }
    }

    return retval;
}

/****
 * GetTrack:  Returns -1 on error, otherwise the last index of the source
 *   copied into the destination.
 ****/
int Credit::GetTrack(char* dest, const char* source, int maxlen)
{
    FnTrace("Credit::GetTrack()");
    int retval = -1;
    int srcidx = 0;
    int srclen = static_cast<int>(strlen(source));
    int dstidx = 0;

    while ((srcidx < srclen) && (source[srcidx] != '?') &&
           (source[srcidx] != '\0') && (dstidx < maxlen))
    {
        dest[dstidx] = source[srcidx];
        srcidx += 1;
        dstidx += 1;
    }
    if (source[srcidx] == '?')
    {
        dest[dstidx] = source[srcidx];
        dstidx += 1;
        retval = srcidx + 1;
    }
    dest[dstidx] = '\0';

    return retval;
}

int Credit::ParseTrack1(const char* swipe_value)
{
    FnTrace("Credit::ParseTrack1()");
    int   retval            = 0;
    char  field_sep         = '^';
    int   idx;
    int   cidx;
    int   len;

    len = static_cast<int>(strlen(swipe_value));
    cidx = 1;
    t1_fc = swipe_value[cidx];
    cidx++;
    idx = 0;
    while (swipe_value[cidx] != field_sep && cidx < len && cidx < MAX_PAN_LEN)
    {
        // we need to skip over spaces in the card number
        if (swipe_value[cidx] != ' ')
        {
            t1_pan[idx] = swipe_value[cidx];
            idx++;
        }
        cidx++;
    }
    if (cidx >= len || swipe_value[cidx] != field_sep)
        return 1;
    t1_pan[idx] = '\0';
    cidx++;
    idx = 0;
    if (t1_pan[0] == '5' && t1_pan[1] == '9')
    {  // read country code
        while (idx < COUNTRY_LEN && cidx < len)
        {
            t1_country[idx] = swipe_value[cidx];
            cidx++;
            idx++;
        }
    }
    t1_country[idx] = '\0';
    idx = 0;
    while (swipe_value[cidx] != field_sep && cidx < len && idx < MAX_NAM_LEN)
    {
        t1_name[idx] = swipe_value[cidx];
        idx++;
        cidx++;
    }
    t1_name[idx] = '\0';
    if (cidx >= len || swipe_value[cidx] != field_sep)
        return 1;
    cidx++;
    idx = 0;
    while (idx < EXPIRE_LEN && cidx < len)
    {
        t1_expiry[idx] = swipe_value[cidx];
        cidx++;
        idx++;
    }
    t1_expiry[idx] = '\0';
    idx = 0;
    while (idx < SC_LEN && cidx < len)
    {
        t1_sc[idx] = swipe_value[cidx];
        cidx++;
        idx++;
    }
    t1_sc[idx] = '\0';
    idx = 0;
    while (idx < PVV_LEN && cidx < len)
    {
        t1_pvv[idx] = swipe_value[cidx];
        cidx++;
        idx++;
    }
    t1_pvv[idx] = '\0';
    idx = 0;
    while (swipe_value[cidx] != '?' && swipe_value[cidx] != '\0' && cidx < len)
    {
        t1_disc[idx] = swipe_value[cidx];
        idx++;
        cidx++;
    }
    t1_disc[idx] = '\0';

    read_t1 = 1;

    return retval;
}

int Credit::ParseTrack2(const char* swipe_value)
{
    FnTrace("Credit::ParseTrack2()");
    int retval = 0;
    char  field_sep         = '=';
    int   idx;
    int   cidx = 0;
    int   len = static_cast<int>(strlen(swipe_value));

    cidx = 1;  // skip past start sentinel
    idx = 0;
    while (swipe_value[cidx] != field_sep && cidx < len && cidx < MAX_PAN_LEN)
    {
        t2_pan[idx] = swipe_value[cidx];
        cidx++;
        idx++;
    }
    t2_pan[idx] = '\0';
    if (cidx >= len || swipe_value[cidx] != field_sep)
        return 1;
    cidx++;  // skip past the field separator
    idx = 0;
    if (t2_pan[0] == '5' && t2_pan[1] == '9')
    {  // read country code
        while (idx < COUNTRY_LEN && cidx < len)
        {
            t2_country[idx] = swipe_value[cidx];
            cidx++;
            idx++;
        }
    }
    t2_country[idx] = '\0';
    idx = 0;
    while (idx < EXPIRE_LEN && cidx < len)
    {
        t2_expiry[idx] = swipe_value[cidx];
        cidx++;
        idx++;
    }
    t2_expiry[idx] = '\0';
    idx = 0;
    while (idx < SC_LEN && cidx < len)
    {
        t2_sc[idx] = swipe_value[cidx];
        cidx++;
        idx++;
    }
    t2_sc[idx] = '\0';
    idx = 0;
    while (idx < PVV_LEN && cidx < len)
    {
        t2_pvv[idx] = swipe_value[cidx];
        cidx++;
        idx++;
    }
    t2_pvv[idx] = '\0';
    idx = 0;
    while (swipe_value[cidx] != '?' && swipe_value[cidx] != '\0' && cidx < len)
    {
        t2_disc[idx] = swipe_value[cidx];
        idx ++;
        cidx++;
    }
    t2_disc[idx] = '\0';

    read_t2 = 1;

    return retval;
}

int Credit::ParseTrack3(const char* swipe_value)
{
    FnTrace("Credit::ParseTrack3()");
    int retval = 0;
    const char* curr              = swipe_value;
    char  field_sep         = '=';
    int   idx;

    curr++;
    for (idx = 0; idx < FC3_LEN; idx++)
    {
        t3_fc[idx] = *curr;
        curr++;
    }
    t3_fc[idx] = '\0';
    idx = 0;
    while (*curr != field_sep)
    {
        t3_pan[idx] = *curr;
        curr++;
        idx++;
    }
    t3_pan[idx] = '\0';
    curr++;
    t3_country[0] = '\0';
    if (*curr != field_sep)
    {
        for (idx = 0; idx < COUNTRY_LEN; idx++)
        {
            t3_country[idx] = *curr;
            curr++;
        }
        t3_country[idx] = '\0';
    }
    for (idx = 0; idx < CURRENCY_LEN; idx++)
    {
        t3_currency[idx] = *curr;
        curr++;
    }
    t3_currency[idx] = '\0';
    t3_ce = *curr;
    curr++;
    for (idx = 0; idx < AA_LEN; idx++)
    {
        t3_aa[idx] = *curr;
        curr++;
    }
    t3_aa[idx] = '\0';
    for (idx = 0; idx < AR_LEN; idx++)
    {
        t3_ar[idx] = *curr;
        curr++;
    }
    t3_ar[idx] = '\0';
    for (idx = 0; idx < CB_LEN; idx++)
    {
        t3_cb[idx] = *curr;
        curr++;
    }
    t3_cb[idx] = '\0';
    for (idx = 0; idx < CL_LEN; idx++)
    {
        t3_cl[idx] = *curr;
        curr++;
    }
    t3_cl[idx] = '\0';
    t3_rc = *curr;
    curr++;
    if (*curr != field_sep)
    {
        for (idx = 0; idx < PINCP_LEN; idx++)
        {
            t3_pincp[idx] = *curr;
            curr++;
        }
        t3_pincp[idx] = '\0';
    }
    t3_ic = *curr;
    curr++;
    for (idx = 0; idx < PANSR_LEN; idx++)
    {
        t3_pansr[idx] = *curr;
        curr++;
    }
    t3_pansr[idx] = '\0';
    for (idx = 0; idx < FSANSR_LEN; idx++)
    {
        t3_fsansr[idx] = *curr;
        curr++;
    }
    t3_fsansr[idx] = '\0';
    for (idx = 0; idx < SSANSR_LEN; idx++)
    {
        t3_ssansr[idx] = *curr;
        curr++;
    }
    t3_ssansr[idx] = '\0';
    for (idx = 0; idx < EXPIRE_LEN; idx++)
    {
        t3_expiry[idx] = *curr;
        curr++;
    }
    t3_expiry[idx] = '\0';
    t3_csn = *curr;
    curr++;
    if (*curr != field_sep)
    {
        for (idx = 0; idx < CSCN_LEN; idx++)
        {
            t3_cscn[idx] = *curr;
            curr++;
        }
        t3_cscn[idx] = '\0';
    }
    idx = 0;
    while (*curr != field_sep)
    {
        t3_fsan[idx] = *curr;
        curr++;
        idx++;
    }
    t3_fsan[idx] = '\0';
    curr++;
    idx = 0;
    while (*curr != field_sep)
    {
        t3_ssan[idx] = *curr;
        curr++;
        idx++;
    }
    t3_ssan[idx] = '\0';
    curr++;
    t3_rm = *curr;
    curr++;
    if (*curr != field_sep)
    {
        for (idx = 0; idx < CCD_LEN; idx++)
        {
            t3_ccd[idx] = *curr;
            curr++;
        }
        t3_ccd[idx] = '\0';
    }
    if (t3_fc[0] == '0' && t3_fc[1] == '1')
    {
        if (*curr != field_sep)
        {
            for (idx = 0; idx < TD_LEN; idx++)
            {
                t3_td[idx] = *curr;
                curr++;
            }
            t3_td[idx] = '\0';
        }
        if (*curr != field_sep)
        {
            for (idx = 0; idx < AVV_LEN; idx++)
            {
                t3_avv[idx] = *curr;
                curr++;
            }
            t3_avv[idx] = '\0';
        }
        if (*curr != field_sep)
        {
            for (idx = 0; idx < ACSN_LEN; idx++)
            {
                t3_acsn[idx] = *curr;
                curr++;
            }
            t3_acsn[idx] = '\0';
        }
        if (*curr != field_sep)
        {
            for (idx = 0; idx < INIC_LEN; idx++)
            {
                t3_inic[idx] = *curr;
                curr++;
            }
            t3_inic[idx] = '\0';
        }
    }
    idx = 0;
    while (*curr != '?')
    {
        t3_disc[idx] = *curr;
        curr++;
        idx++;
    }
    t3_disc[idx] = '\0';
    
    read_t3 = 1;

    return retval;
}

/****
 * ParseManual:  returns -1 on error, 1 on success
 ****/
int Credit::ParseManual(const char* swipe_value)
{
    FnTrace("Credit::ParseManual()");
    int retval = -1;
    const char* curr = swipe_value;
    int idx;

    mn_pan[0] = '\0';
    mn_expiry[0] = '\0';

    idx = 0;
    while (*curr != '=' && *curr != '\0')
    {
        mn_pan[idx] = *curr;
        curr++;
        idx += 1;
    }
    mn_pan[idx] = '\0';

    curr++;
    idx = 0;
    while (*curr != '\0')
    {
        mn_expiry[idx] = *curr;
        curr++;
        idx += 1;
    }
    mn_expiry[idx] = '\0';

    if (mn_pan[0] != '\0' && mn_expiry[0] != '\0')
    {
        read_manual = 1;
        retval = 1;
    }

    return retval;
}

/****
 * ParseSwipe:  returns 1 for success or 0 for failure, as it returns
 *  the result of ValidateCardInfo() and the result is supposed to
 *  be used to determine whether the card is valid.  AKA, the return
 *  value of this function ends up also being returned by the
 *  IsValid() method.
 ****/
int Credit::ParseSwipe(const char* value)
{
    FnTrace("Credit::ParseSwipe()");
    int   retval = 0;
    char  track1[STRLONG] = "";
    char  track2[STRLONG] = "";
    char  track3[STRLONG] = "";
    int   validx = 0;
    int   result = 0;

    read_t1     = 0;
    read_t2     = 0;
    read_t3     = 0;
    read_manual = 0;

    if (value[validx] == '%')
    {  // parsing track 1
        result = GetTrack(track1, value, STRLONG);
        if (result > -1)
        {
            ParseTrack1(track1);
            validx = result;
            swipe.Set(track1);
        }
    }
    if (validx > -1 && value[validx] == ';')
    {  // parsing track 2
        result = GetTrack(track2, &value[validx], STRLONG);
        if (result > -1)
        {
            ParseTrack2(track2);
            validx += result;
            swipe.Set(track2);
        }
    }
    if (validx > -1 && value[validx] == ';')
    {  // parsing track 3
        result = GetTrack(track3, &value[validx], STRLONG);
        if (result > -1)
        {
            ParseTrack3(track3);
            validx += result;
        }
    }
    if (validx <= 0)
    {
        if (strncmp("manual ", value, 7) == 0)
            result = ParseManual(&value[7]);
    }

    if (read_t1 || read_t2 || read_t3 || read_manual)
        retval = ValidateCardInfo();

    return retval;
}

int Credit::ParseApproval(const char* value)
{
    FnTrace("Credit::ParseApproval()");
    if (value == nullptr)
        return 1;

    char  str[256];
    const char* s = value;
    char* d = str;
    int quote = 0;
    while (*s)
    {
        if (*s == '\"')
        {
            if (quote)
                break;
            quote = 1;
        }
        else
            *d++ = *s;
        ++s;
    }
    *d = '\0';
    approval.Set(str);
    return 0;
}

int Credit::SetCreditType()
{
    FnTrace("Credit::SetCreditType()");
    const char* num = number.Value();
    int len = static_cast<int>(strlen(num));
    int v;

    if (len >= 13 && len <= 16)
    {
        v = (num[0]-'0')*1000 + (num[1]-'0')*100 + (num[2]-'0')*10 + (num[3]-'0');
        
        if ((len == 13 || len == 16) && v >= 4000 && v <= 4999)
        {
            credit_type = CREDIT_TYPE_VISA;
        }
        else if (len == 16 && v >= 5100 && v <= 5599)
        {
            credit_type = CREDIT_TYPE_MASTERCARD;
        }
        else if (len == 15 && ((v >= 3400 && v <= 3499) || (v >= 3700 && v <= 3799)))
        {
            credit_type = CREDIT_TYPE_AMEX;
        }
        else if (len == 16 && v == 6011)
        {
            credit_type = CREDIT_TYPE_DISCOVER;
        }
        else if ((len == 14 || len == 16) && ((v >= 3000 && v <= 3059) ||
                                         (v >= 3600 && v <= 3699) || (v >= 3800 && v <= 3899)))
        {
            credit_type = CREDIT_TYPE_DINERSCLUB;
        }
        else if (len == 16 && v >= 3528 && v <= 3589)
        {
            credit_type = CREDIT_TYPE_JCB;
        }
    }

    return credit_type;
}

int Credit::CreditType()
{
    FnTrace("Credit::CreditType()");

    if (credit_type == CREDIT_TYPE_UNKNOWN && number.size() > 0)
        SetCreditType();

    return credit_type;
}

char* Credit::CreditTypeName(char* str, int shortname)
{
    FnTrace("Credit::CreditTypeName()");
    static char buffer[32];
    const char* hold = nullptr;

    if (str == nullptr)
        str = buffer;

    vt_safe_string::safe_copy(str, 32, UnknownStr);

    if (card_type == CARD_TYPE_DEBIT)
    {
        hold = FindStringByValue(debit_acct, DebitAcctValue, DebitAcctName);
    }
    else if (card_type == CARD_TYPE_GIFT)
    {
        vt_safe_string::safe_copy(str, 32, "Gift Card");
    }
    else
    {
        if (credit_type == CREDIT_TYPE_UNKNOWN)
            SetCreditType();
        
        if (credit_type != CREDIT_TYPE_UNKNOWN)
        {
            if (shortname)
                hold = FindStringByValue(credit_type, CreditCardValue, CreditCardShortName);
            else
                hold = FindStringByValue(credit_type, CreditCardValue, CreditCardName);
        }
    }

    if (hold != nullptr)
        vt_safe_string::safe_copy(str, 32, hold);

    return str;
}

int Credit::IsEmpty()
{
    FnTrace("Credit::IsEmpty()");
    int retval = 1;

    if (swipe.size() > 0)
        return 0;
    if (number.size() > 0)
        return 0;
    if (name.size() > 0)
        return 0;
    if (expire.size() > 0)
        return 0;
    if (country.size() > 0)
        return 0;

    return retval;
}

int Credit::IsValid()
{
    FnTrace("Credit::IsValid()");
    int retval = 0;

    if (number.size() > 0 && expire.size() > 0)
        retval = 1;
    
    return retval;
}

int Credit::IsVoiced()
{
    FnTrace("Credit::IsVoiced()");
    int retval = 0;

    if (intcode == CC_STATUS_VOICE)
        retval = 1;

    return retval;
}

int Credit::IsPreauthed()
{
    FnTrace("Credit::IsPreauthed()");
    int retval = 0;

    if (auth_state == CCAUTH_PREAUTH && IsVoided() == 0)
        retval = 1;

    return retval;
}

int Credit::IsAuthed(int also_preauth)
{
    FnTrace("Credit::IsAuthed()");
    int retval = 0;

    if (auth_state == CCAUTH_COMPLETE || auth_state == CCAUTH_AUTHORIZE)
    {
        retval = 1;
    }
    else if (also_preauth && IsPreauthed())
    {
        retval = 1;
    }
    else if (intcode == CC_STATUS_VOICE)
        retval = 1;

    return retval;
}

int Credit::IsVoided(int any_value)
{
    FnTrace("Credit::IsVoided()");
    int retval = 0;

    if (state == CCAUTH_VOID)
    {
        if (any_value || Total(1) <= 0)
            retval = 1;
    }

    return retval;
}

int Credit::IsRefunded(int any_value)
{
    FnTrace("Credit::IsRefunded()");
    int retval = 0;

    if (state == CCAUTH_REFUND)
    {
        if (any_value || Total(1) <= 0)
            retval = 1;
    }

    return retval;
}

int Credit::IsSettled()
{
    FnTrace("Credit::IsSettled()");
    int retval = 0;

    if (intcode == CC_STATUS_SETTLED)
        retval = 1;

    return retval;
}

int Credit::IsDeclined()
{
    FnTrace("Credit::IsDeclined()");
    int retval = 0;

    if (intcode == CC_STATUS_DENY)
        retval = 1;

    return retval;
}

int Credit::IsErrored()
{
    FnTrace("Credit::IsErrored()");
    int retval = 0;

    if (intcode == CC_STATUS_ERROR)
        retval = 1;

    return retval;
}

/****
 * RequireSwipe: A more accurate and complete name would be
 *   "RequireSwipeOrManualEntry()", but that seemed too long.  This
 *   method determines if we have a "saved" number (xxxx xxxx xxxx 9999
 *   for credits).
 * Returns 1 if so, in which case the number must be re-entered,
 *   either manually or via swipe.
 * Returns 0 if the number is fine for further transactions.
 ****/
int Credit::RequireSwipe()
{
    FnTrace("Credit::RequireSwipe()");
    int retval = 0;
    int len = static_cast<int>(number.size());
    const char* numval = number.Value();

    if (len < 1)
        retval = 1;
    else if (numval[0] == 'x' || numval[len - 1] == 'x')
        retval = 1;

    return retval;
}

/****
 * PrintAuth:  Debugging function.  Should not call this in live code.
 ****/
int Credit::PrintAuth()
{
    FnTrace("Credit::PrintAuth()");
    int retval = 0;

    printf("    Code:         %s\n", code.Value());
    printf("    Verbiage:     %s\n", verb.Value());
    printf("    Auth:         %s\n", auth.Value());
    printf("    ISO:          %s\n", isocode.Value());
    printf("    B24:          %s\n", b24code.Value());
    printf("    Batch:        %ld\n", static_cast<long>(batch));
    printf("    Item:         %ld\n", static_cast<long>(item));
    printf("    TTID:         %ld\n", static_cast<long>(ttid));
    printf("    AVS:          %s\n", AVS.Value());
    printf("    CV:           %s\n", CV.Value());
    printf("    Reference:    %s\n", reference.Value());
    printf("    Sequence:     %s\n", sequence.Value());
    printf("    Date:         %s\n", server_date.Value());
    printf("    Time:         %s\n", server_time.Value());
    printf("    Receipt:      %s\n", receipt_line.Value());
    printf("    Display:      %s\n", display_line.Value());
    printf("    Status:       %d\n", intcode);
    printf("    Last Action:  %d\n", last_action);
    printf("    State:        %d\n", state);
    printf("    Auth State:   %d\n", auth_state);

    return retval;
}

int Credit::ClearAuth()
{
    FnTrace("Credit::ClearAuth()");
    int retval = 0;

    code.Clear();
    verb.Clear();
    auth.Clear();
    batch = 0;
    item = 0;
    ttid = 0;
    AVS.Clear();
    CV.Clear();
    sequence.Clear();
    server_date.Clear();
    server_time.Clear();
    receipt_line.Clear();
    display_line.Clear();

    return retval;
}

int Credit::Finalize(Terminal *term)
{
    FnTrace("Credit::Finalize()");
    
    vt::Logger::debug("Finalizing credit transaction - Amount: ${:.2f}, Card: ****{}",
                      amount / 100.0, number.Value() + (number.size() > 4 ? number.size() - 4 : 0));

    batch_term_id.Set(term->cc_debit_termid.Value());
    if (IsPreauthed() && !preauth_time.IsSet())
    {
        preauth_amount = FullAmount();
        preauth_time.Set();
        approval.Set(auth.Value());
        vt::Logger::info("Credit preauth finalized - Approval: {}, Amount: ${:.2f}",
                        auth.Value(), preauth_amount / 100.0);
    }
    else if (IsAuthed() && !auth_time.IsSet())
    {
        auth_amount = FullAmount();
        preauth_amount = 0;
        void_amount = 0;
        refund_amount = 0;
        auth_time.Set();
        approval.Set(auth.Value());
        vt::Logger::info("Credit auth finalized - Approval: {}, Amount: ${:.2f}",
                        auth.Value(), auth_amount / 100.0);
    }
    else if (IsVoided(1) && !void_time.IsSet())
    {
        void_amount = amount;
        void_time.Set();
        MasterSystem->cc_void_db->Add(term, Copy());
        approval.Set(auth.Value());
        vt::Logger::info("Credit void finalized - Amount: ${:.2f}", void_amount / 100.0);
    }
    else if (IsRefunded(1) && !refund_time.IsSet())
    {
        refund_amount = amount;
        refund_time.Set();
        vt::Logger::info("Credit refund finalized - Amount: ${:.2f}", refund_amount / 100.0);
        MasterSystem->cc_refund_db->Add(term, Copy());
        approval.Set(auth.Value());
    }
    else if (IsAuthed(1) &&
             state == CCAUTH_REFUND_CANCEL &&
             !refund_cancel_time.IsSet())
    {
        refund_amount -= amount;
        approval.Set(auth.Value());
        refund_cancel_time.Set();
    }

    return 0;
}

/****
 * Status:  Best not to use this externally.  Instead, use IsAuthed(),
 *   IsVoided(), IsRefunded(), etc.  The method of determining status
 *   may change frequently.  Those functions will always tell you what
 *   you want to know (regardless of what processor we're using as well
 *   as what method).
 ****/
int Credit::GetStatus()
{
    FnTrace("Credit::GetStatus()");

    return intcode;
}

/****
 * Approval:  Returns the approval/authorization code.
 ****/
const char* Credit::Approval()
{
    FnTrace("Credit::Approval()");
    static char str[256];

    str[0] = '\0';
    if (processor == CCAUTH_CREDITCHEQ)
        vt_safe_string::safe_format(str, 256, "%s", code.Value());
    else if (approval.empty())
        vt_safe_string::safe_format(str, 256, "PENDING");
    else if (approval.size() > 0)
        vt_safe_string::safe_copy(str, 256, approval.Value());

    return str;
}

/****
 * PAN: Masks the card number (unless all argument is non-negative)
 * and returns it as a string.  The masking is:
 *    For Credit Cards, mask all but the last 4 digits
 *    For Debit Cards, mask the last 5 digits.
 * So:
 *    Visa  xxxx xxxx xxxx 9999
 *    Debit 9999 9999 999x xxxx
 * However, the Debit method is only used for CreditCHEQ.  For all
 * other processing APIs, we mask all but the last four digits of
 * every card, including debit.  This is true as of June 30, 2005.
 ****/
const char* Credit::PAN(int all)
{
    FnTrace("Credit::PAN()");
    static char str[STRSHORT];
    char str2[STRSHORT];
    int idx;
    int len;

    str[0] = '\0';
    if (number.size() > 0)
    {
        vt_safe_string::safe_copy(str, STRSHORT, number.Value());
        
        if (all == 0)
        {
            if (card_type == CARD_TYPE_DEBIT && processor == CCAUTH_CREDITCHEQ)
            {
                len = static_cast<int>(strlen(str));
                idx = len - 5;
                while (idx < len)
                {
                    str[idx] = 'x';
                    idx += 1;
                }
            }
            else
            {
                idx = static_cast<int>(strlen(str)) - 4;
                while (idx > 0)
                {
                    idx -= 1;
                    str[idx] = 'x';
                }
            }
        }
        else if (all == 2) // remove spaces
        {
            vt_safe_string::safe_copy(str2, STRSHORT, str);
            int didx = 0;
            idx = 0;
            while (idx < static_cast<int>(STRSHORT) && str2[idx] != '\0')
            {
                if (str2[idx] != ' ')
                {
                    str[didx] = str2[idx];
                    didx += 1;
                }
                idx += 1;
            }
            str[didx] = '\0';
        }
    }

    return str;
}

char* Credit::LastFour(char* dest)
{
    FnTrace("Credit::LastFour()");
    char buffer[STRLENGTH];
    static char str[10];
    int sidx = 0;
    int didx = 0;
    int len = 0;
    if (dest == nullptr)
        dest = str;

    if (number.size() > 0)
    {
        memset(dest, '\0', 10);
        vt_safe_string::safe_copy(buffer, STRLENGTH, number.Value());
        len = static_cast<int>(strlen(buffer));
        sidx = len - 4;
        while (sidx < len)
        {
            dest[didx] = buffer[sidx];
            didx += 1;
            sidx += 1;
        }
    }

    return dest;
}

const char* Credit::ExpireDate()
{
    FnTrace("Credit::ExpireDate()");
    static char str[16];

    str[0] = '\0';
    if (expire.size() > 0)
    {
        const char* s = expire.Value();
        if (expire.size() < 4)
            vt_safe_string::safe_format(str, 16, "%s/%s", "??", "??");  // to get rid of compiler warnings
        else
            vt_safe_string::safe_format(str, 16, "%c%c/%c%c", s[0], s[1], s[2], s[3]);
    }

    return str;
}

const char* Credit::Name()
{
    FnTrace("Credit::Name()");
    static char str[STRLENGTH];
    char buffer[STRLENGTH] = "";
    char first[STRLENGTH] = "";
    char last[STRLENGTH] = "";
    char init = '\0';
    int bidx;
    int idx;

    vt_safe_string::safe_copy(buffer, STRLENGTH, name.Value());
    if (strlen(buffer) > 0 && strcmp(buffer, " /") != 0)
    {
        bidx = 0;

        // get last name
        idx = 0;
        while (buffer[bidx] != '/' && buffer[bidx] != '\0')
        {
            if (idx == 0 && buffer[bidx] > 96)
                last[idx] = buffer[bidx] - 32;
            else if (idx == 0)
                last[idx] = buffer[bidx];
            else if (buffer[bidx] < 97)
                last[idx] = buffer[bidx] + 32;
            else
                last[idx] = buffer[bidx];
            idx += 1;
            bidx += 1;
        }
        bidx += 1;  // skip slash
        last[idx] = '\0';

        while (buffer[bidx] == ' ' && buffer[bidx] != '\0')
            bidx += 1;

        // get first name
        idx = 0;
        while (buffer[bidx] != ' ' && buffer[bidx] != '\0')
        {
            if (idx == 0 && buffer[bidx] > 96)
                first[idx] = buffer[bidx] - 32;
            else if (idx == 0)
                first[idx] = buffer[bidx];
            else if (buffer[bidx] < 97)
                first[idx] = buffer[bidx] + 32;
            else
                first[idx] = buffer[bidx];
            idx += 1;
            bidx += 1;
        }
        first[idx] = '\0';

        while (buffer[bidx] == ' ' && buffer[bidx] != '\0')
            bidx += 1;

        // get initial
        if (buffer[bidx] != '\0')
        {
            if (buffer[bidx] < 97)
                init = buffer[bidx];
            else
                init = buffer[bidx] - 32;
        }
    }

    str[0] = '\0';
    if (init != '\0')
        vt::cpp23::format_to_buffer(str, STRLENGTH, "{} {}. {}", first, init, last);
    else if (first[0] != '\0')
        vt::cpp23::format_to_buffer(str, STRLENGTH, "{} {}", first, last);
    else if (last[0] != '\0')
        vt_safe_string::safe_copy(str, STRLENGTH, last);

    return str;
}

/****
 * Country:  Returns LANG_ENGLISH or the proper LANG_ define for the country
 *  as swiped from the card.
 ****/
int Credit::Country()
{
    FnTrace("Credit::Country()");
    // Language support removed - always return default language
    return LANG_PHRASE;
}

int Credit::LastAction(int last)
{
    FnTrace("Credit::LastAction()");

    if (last >= 0)
        last_action = last;

    return last_action;
}

/****
 * MakeCardNumber: Used to mask the card number in memory.  Should
 * only be used after the action (Auth, PreAuth, Void, etc.) has been
 * completed.
 ****/
int Credit::MaskCardNumber()
{
    FnTrace("Credit::MaskCardNumber()");
    int retval = 0;
    const char* cardnum = nullptr;

    cardnum = PAN(0);
    number.Set(cardnum);

    return retval;
}

int Credit::Amount(int newamount)
{
    FnTrace("Credit::Amount()");
    int retval = amount;

    if (newamount > -1)
        amount = newamount;

    return retval;
}

int Credit::Tip(int newtip)
{
    FnTrace("Credit::Tip()");
    int retval = tip;

    if (newtip > -1)
        tip = newtip;

    return retval;
}

int Credit::Total(int also_preauth)
{
    FnTrace("Credit::Total()");
    int retval = 0;

    if (also_preauth == 0 || auth_amount > 0)
        retval = auth_amount;
    else
        retval = preauth_amount;

    retval -= (refund_amount + void_amount);

    return retval;
}
int Credit::FullAmount()
{
    FnTrace("Credit::FullAmount()");

    return (amount + tip);
}

int Credit::TotalPreauth()
{
    FnTrace("Credit::TotalPreauth()");
    int retval = 0;

    retval = preauth_amount - (refund_amount + void_amount);

    return retval;
}

int Credit::SetBatch(long long batchnum, const char* btermid)
{
    FnTrace("Credit::SetBatch()");
    int retval = 1;

    if (btermid != nullptr)
    {
        if (batch <= 0 && strcmp(batch_term_id.Value(), btermid) == 0)
        {
            settle_time.Set();
            batch = batchnum;
            retval = 0;
        }
    }
    else
    {
        settle_time.Set();
        batch = batchnum;
        retval = 0;
    }

    return retval;
}

int Credit::SetState(int newstate)
{
    FnTrace("Credit::SetState()");

    if (newstate == CCAUTH_FIND)
    {
        if (intcode == CC_STATUS_SUCCESS ||
            intcode == CC_STATUS_AUTH)
        {
            state = last_action;
        }
    }
    else
    {
        state = newstate;
    }

    if (state == CCAUTH_PREAUTH ||
        state == CCAUTH_AUTHORIZE ||
        state == CCAUTH_COMPLETE)
    {
        auth_state = state;
    }

    return state;
}

int Credit::operator == (Credit *c)
{
    FnTrace("Credit::==()");
    int retval = 0;

    if (read_t1 && (strcmp(t1_pan, c->t1_pan) == 0) && (strcmp(t1_expiry, c->t1_expiry) == 0))
        retval = 1;
    else if (read_t2 && (strcmp(t2_pan, c->t2_pan) == 0) && (strcmp(t2_expiry, c->t2_expiry) == 0))
        retval = 1;
    else if (read_t3 && (strcmp(t3_pan, c->t3_pan) == 0) && (strcmp(t3_expiry, c->t3_expiry) == 0))
        retval = 1;
    else if (read_manual && (strcmp(mn_pan, c->mn_pan) == 0) && (strcmp(mn_expiry, c->mn_expiry) == 0))
        retval = 1;

    return retval;
}

int Credit::GetApproval(Terminal *term)
{
    FnTrace("Credit::GetApproval()");
    int retval = 1;

    if (term->credit != nullptr && term->credit != this)
    {
        if (debug_mode) printf("Have stale card in GetApproval...\n");
    }
    term->credit = this;
    retval = term->CC_GetApproval();
    LastAction(CCAUTH_AUTHORIZE);

    return retval;
}

int Credit::GetPreApproval(Terminal *term)
{
    FnTrace("Credit::GetPreApproval()");
    int retval = 1;
    
    if (term->credit != nullptr && term->credit != this)
    {
        if (debug_mode) printf("Have stale card in GetPreApproval...\n");
    }
    term->credit = this;
    retval = term->CC_GetPreApproval();
    LastAction(CCAUTH_PREAUTH);

    return retval;
}

int Credit::GetFinalApproval(Terminal *term)
{
    FnTrace("Credit::GetFinalApproval()");
    int retval = 1;

    if (term->credit != nullptr && term->credit != this)
    {
        if (debug_mode) printf("Have stale card in GetFinalApproval...\n");
    }
    term->credit = this;
    retval = term->CC_GetFinalApproval();
    LastAction(CCAUTH_COMPLETE);

    return retval;
}

int Credit::GetVoid(Terminal *term)
{
    FnTrace("Credit::GetVoid()");
    int retval = 1;

    if (term->credit != nullptr && term->credit != this)
    {
        if (debug_mode) printf("Have stale card in GetVoid...\n");
    }
    term->credit = this;
    retval = term->CC_GetVoid();
    LastAction(CCAUTH_VOID);

    return retval;
}

int Credit::GetVoidCancel(Terminal *term)
{
    FnTrace("Credit::GetVoidCancel()");
    int retval = 1;

    if (term->credit != nullptr && term->credit != this)
    {
        if (debug_mode) printf("Have stale card in GetVoidCancel...\n");
    }
    term->credit = this;
    retval = term->CC_GetVoidCancel();
    LastAction(CCAUTH_VOID_CANCEL);

    return retval;
}

int Credit::GetRefund(Terminal *term)
{
    FnTrace("Credit::GetRefund()");
    int retval = 1;

    if (term->credit != nullptr && term->credit != this)
    {
        if (debug_mode) printf("Have stale card in GetRefund...\n");
    }
    term->credit = this;
    retval = term->CC_GetRefund();
    LastAction(CCAUTH_REFUND);

    return retval;
}

int Credit::GetRefundCancel(Terminal *term)
{
    FnTrace("Credit::GetRefundCancel()");
    int retval = 1;

    if (term->credit != nullptr && term->credit != this)
    {
        if (debug_mode) printf("Have stale card in GetRefundCancel...\n");
    }
    term->credit = this;
    retval = term->CC_GetRefundCancel();
    LastAction(CCAUTH_REFUND_CANCEL);

    return retval;
}

/****
 * ReceiptPrint: This is the private member which does the actual
 * printing.  Normally, the public PrintReceipt() member should be
 * called, with the types of receipts required (merchant, customer,
 * etc.) specified.
 ****/
int Credit::ReceiptPrint(Terminal *term, int receipt_type, Printer *pprinter, int print_amount)
{
    FnTrace("Credit::ReceiptPrint()");
    int retval = 0;
    int lang = Country();
    char buffer[STRLENGTH];
    char buffer2[STRLENGTH];
    char buffer3[STRLENGTH];
    char line[STRLENGTH];
    Settings *settings = term->GetSettings();
    Printer *printer = pprinter;
    int pwidth;
    int width;
    int len;
    int idx;
    static int count = 0;  // for saving receipts for Moneris testing
    Check *parent = term->system_data->FindCheckByID(check_id);

    if (printer == nullptr)
        printer = term->FindPrinter(PRINTER_RECEIPT);
    if (printer != nullptr)
    {
        pwidth = printer->MaxWidth();
        vt::cpp23::format_to_buffer(line, STRLENGTH, "{:>{}}", "________________", pwidth);
        if (debug_mode)
        {
            while (1)
            {
                vt::cpp23::format_to_buffer(buffer, STRLENGTH, "CreditCardReceipt-{:02d}\n", count);
                printer->SetTitle(buffer);
                printer->GetFilePath(buffer);
                if (!DoesFileExist(buffer))
                    break;
                count += 1;
            }
        }
        else
        {
            printer->SetTitle(term->Translate("CreditCardReceipt"));
        }
        printer->Start();
        vt_safe_string::safe_copy(buffer, STRLENGTH, term->Translate("==== TRANSACTION RECORD ====", lang));
        len = strlen(buffer);
        width = ((pwidth - len) / 2) + len;
        vt::cpp23::format_to_buffer(buffer2, STRLENGTH, "{:>{}}", buffer, width);
        printer->Write(buffer2);
        printer->NewLine();
        
        // store name and address
        printer->Write(settings->store_name.Value());
        printer->Write(settings->store_address.Value());
        printer->Write(settings->store_address2.Value());
        printer->LineFeed(2);
        
        // type of transaction
        if (settings->authorize_method == CCAUTH_CREDITCHEQ)
        {
            if (last_action == CCAUTH_AUTHORIZE)
                vt_safe_string::safe_copy(buffer, STRLENGTH, term->Translate("Purchase", lang));
            else if (last_action == CCAUTH_PREAUTH)
                vt_safe_string::safe_copy(buffer, STRLENGTH, term->Translate("Pre-Authorization", lang));
            else if (last_action == CCAUTH_COMPLETE && auth.size() > 0)
                vt_safe_string::safe_copy(buffer, STRLENGTH, term->Translate("Pre-Auth Completion", lang));
            else if (last_action == CCAUTH_COMPLETE)
                vt_safe_string::safe_copy(buffer, STRLENGTH, term->Translate("Pre-Auth Advice", lang));
            else if (last_action == CCAUTH_REFUND)
                vt_safe_string::safe_copy(buffer, STRLENGTH, term->Translate("Refund", lang));
            else if (last_action == CCAUTH_REFUND_CANCEL)
                vt_safe_string::safe_copy(buffer, STRLENGTH, term->Translate("Refund Cancel", lang));
            else if (last_action == CCAUTH_VOID)
                vt_safe_string::safe_copy(buffer, STRLENGTH, term->Translate("Purchase Correction", lang));
            else if (last_action == CCAUTH_VOID_CANCEL)
                vt_safe_string::safe_copy(buffer, STRLENGTH, term->Translate("Void Cancel", lang));
            else
                vt_safe_string::safe_copy(buffer, STRLENGTH, term->Translate("Unknown Transaction", lang));
            vt::cpp23::format_to_buffer(buffer2, STRLENGTH, "{}: {}", term->Translate("Transaction Type", lang), buffer);
            printer->Write(buffer2);
            printer->LineFeed();
        }

        // amount of transaction, with tip and total
        if (IsPreauthed() && print_amount > -1)
            vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{}", term->FormatPrice(print_amount, 1));
        else
            vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{}", term->FormatPrice(FullAmount(), 1));
        width = pwidth - strlen(buffer) - 1;
        vt::cpp23::format_to_buffer(buffer2, STRLENGTH, "{}:", term->Translate("Amount", lang));
        vt::cpp23::format_to_buffer(buffer3, STRLENGTH, "{:<{}} {}", buffer2, -width, buffer);
        printer->Write(buffer3);
        if (IsPreauthed())
        {
            vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{}:", term->Translate("Tip", lang));
            printer->Write(buffer);
            printer->Write(line);
            vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{}:", term->Translate("Total", lang));
            printer->Write(buffer);
            printer->Write(line);
            printer->NewLine();
        }
        printer->LineFeed(2);

        // card and date information
        if (settings->cc_print_custinfo)
        {
            if (parent != nullptr)
            {
                int did_print = 0;
                vt_safe_string::safe_copy(buffer2, STRLENGTH, parent->FullName());
                if (strlen(buffer2) > 0)
                {
                    vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{}: {}",
                             term->Translate("Customer Name", lang), buffer2);
                    printer->Write(buffer);
                    did_print = 1;
                }
                vt_safe_string::safe_copy(buffer2, STRLENGTH, parent->Table());
                if (strlen(buffer2) > 0)
                {
                    vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{}: {}",
                             term->Translate("Table", lang), buffer2);
                    printer->Write(buffer);
                    did_print = 1;
                }
                if (did_print)
                    printer->Write("");
            }
            if (name.size() > 0)
            {
                vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{}: {}",
                         term->Translate("Card Owner", lang), Name());
                printer->Write(buffer);
            }
        }
        if (card_type == CARD_TYPE_DEBIT)
        {
            vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{}: {}",
                     term->Translate("Debit Card Number", lang),
                     PAN(settings->show_entire_cc_num));
        }
        else
        {
            vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{}: {}",
                     term->Translate("Card Number", lang),
                     PAN(settings->show_entire_cc_num));
        }
        printer->Write(buffer);
        vt_safe_string::safe_copy(buffer3, STRLONG, term->Translate(CreditTypeName(buffer2), lang));
        vt::cpp23::format_to_buffer(buffer, STRLONG, "{}: {}", term->Translate("Account Type", lang),
                 buffer3);
        printer->Write(buffer);
        if (server_date.size() > 0 && server_time.size() > 0)
        {
            vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{}: {} {}", term->Translate("Date/Time", lang),
                     server_date.Value(), server_time.Value());
            printer->Write(buffer);
        }
        else
        {
            vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{}:  {}", term->Translate("Date/Time", lang),
                     term->TimeDate(SystemTime, TD3));
            printer->Write(buffer);
            
        }
        if (settings->authorize_method == CCAUTH_CREDITCHEQ)
        {
            if (read_manual || b24code.empty())
                vt_safe_string::safe_format(buffer, STRLENGTH, "%s: %s %s M",
                         term->Translate("Reference Number", lang),
                         term_id.Value(), sequence.Value());
            else
                vt_safe_string::safe_format(buffer, STRLENGTH, "%s: %s %s S",
                         term->Translate("Reference Number", lang),
                         term_id.Value(), sequence.Value());
            printer->Write(buffer);
        }
        if (auth.size() > 0)
        {
            vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{}: {}", term->Translate("Authorization Number", lang),
                     auth.Value());
            printer->Write(buffer);
        }

        // customer message, signature line, and customer agreement
        printer->LineFeed();
        if (settings->authorize_method == CCAUTH_CREDITCHEQ)
        {
            if ((last_action == CCAUTH_REFUND_CANCEL ||
                 last_action == CCAUTH_VOID) &&
                strcmp("AUTHORIZED", receipt_line.Value()) == 0)
            {
                
                receipt_line.Set(term->Translate("APPROVED - THANK YOU", lang));
            }
            printer->LineFeed();
            width = pwidth - (receipt_line.size());
            width = (width / 2) + receipt_line.size();
            vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{:>{}}", receipt_line.Value(), width);

            printer->Write(buffer);
        }
        else
        {
            width = ((pwidth - verb.size()) / 2) + verb.size();
            vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{:>{}}", verb.Value(), width);
            printer->Write(buffer);
        }

        if (CanPrintSignature())
        {
            printer->LineFeed(3);
            for (idx = 0; idx < pwidth; idx += 1)
                buffer[idx] = '_';
            buffer[idx] = '\0';
            printer->Write(buffer);
            printer->Write(term->Translate("Cardholder Signature", lang));
            
            printer->LineFeed(2);
            for (idx = 1; idx < 5; idx += 1)
            {
                vt::cpp23::format_to_buffer(buffer, STRLENGTH, "Customer Agreement {}", idx);
                vt_safe_string::safe_copy(buffer2, STRLENGTH, term->Translate(buffer, lang, 1));
                len = strlen(buffer2);
                if (len > 0)
                {
                    width = ((pwidth - len) / 2) + len;
                    vt::cpp23::format_to_buffer(buffer, STRLENGTH, "{:>{}}", buffer2, len);
                    printer->Write(buffer);
                }
            }
        }

        printer->LineFeed(3);
        printer->End();
    }

    return retval;
}

int Credit::PrintReceipt(Terminal *term, int receipt_type, Printer *pprinter, int print_amount)
{
    FnTrace("Credit::PrintReceipt()");
    int retval = 0;

    if (receipt_type == RECEIPT_PICK)
    {
        retval = ReceiptPrint(term, RECEIPT_CUSTOMER, pprinter, print_amount);
        if (retval == 0 && term->GetSettings()->merchant_receipt)
        {
            sleep(2);
            retval = ReceiptPrint(term, RECEIPT_MERCHANT, pprinter, print_amount);
        }
    }
    else
    {
        retval = ReceiptPrint(term, receipt_type, pprinter, print_amount);
    }

    return retval;
}


/*********************************************************************
 * CreditDB Class -- Just a container class to hold credit card lists
 *  and to allow for easy saving and loading.
 ********************************************************************/

CreditDB::CreditDB(int dbtype)
{
    FnTrace("CreditDB::CreditDB()");

    fullpath[0] = '\0';
    db_type = dbtype;
    last_card_id = 0;
}

CreditDB::~CreditDB()
{
    FnTrace("CreditDB::~CreditDB()");
}

CreditDB *CreditDB::Copy()
{
    FnTrace("CreditDB::Copy()");
    CreditDB *newdb;
    Credit   *credit = credit_list.Head();
    Credit   *crednext = nullptr;

    newdb = new CreditDB(db_type);
    if (newdb != nullptr)
    {
        vt_safe_string::safe_copy(newdb->fullpath, STRLONG, fullpath);
        newdb->last_card_id = last_card_id;

        while (credit != nullptr)
        {
            crednext = credit->next;
            newdb->credit_list.AddToTail(credit);
            credit_list.Remove(credit);
            credit = crednext;
        }
    }

    return newdb;
}

int CreditDB::Read(InputDataFile &infile)
{
    FnTrace("CreditDB:Read()");
    int retval = 0;
    int version;
    int count;
    int idx;
    Credit *credit;
    
    infile.Read(version);
    infile.Read(count);

    for (idx = 0; idx < count; idx ++)
    {
        credit = new Credit();
        credit->Read(infile, version);
        if (credit->IsEmpty() == 0)
            Add(credit);
    }

    return retval;
}

int CreditDB::Write(OutputDataFile &outfile)
{
    FnTrace("CreditDB:Write()");
    int retval = 0;
    int count = credit_list.Count();
    Credit *credit = credit_list.Head();

    outfile.Write(CREDIT_CARD_VERSION);
    outfile.Write(count);
    while (credit != nullptr)
    {
        if (credit->IsEmpty() == 0)
            credit->Write(outfile, CREDIT_CARD_VERSION);
        credit = credit->next;
    }

    return retval;
}

int CreditDB::Save()
{
    FnTrace("CreditDB::Save()");
    int retval = 0;
    OutputDataFile outfile;

    if (fullpath[0] == '\0')
    {
        if (db_type == CC_DBTYPE_VOID)
            vt_safe_string::safe_copy(fullpath, STRLONG, MASTER_CC_VOID);
        else if (db_type == CC_DBTYPE_REFUND)
            vt_safe_string::safe_copy(fullpath, STRLONG, MASTER_CC_REFUND);
        else
            vt_safe_string::safe_copy(fullpath, STRLONG, MASTER_CC_EXCEPT);
    }

    if (fullpath[0] != '\0' &&
        outfile.Open(fullpath, CREDIT_CARD_VERSION) == 0)
    {
        Write(outfile);
    }

    return retval;
}

int CreditDB::Load(const char* path)
{
    FnTrace("CreditDB::Load()");
    int retval = 0;
    InputDataFile infile;
    int version;  // a throwaway for infile; we save version manually so that
                  // Read() and Write() don't have to wonder.

    if (path != nullptr)
        vt_safe_string::safe_copy(fullpath, STRLONG, path);

    if (fullpath[0] != '\0')
    {
      if (infile.Open(fullpath, version) == 0) 
        Read(infile);
    }
    

    return retval;
}

/****
 * Add:  Add(Terminal, Credit) should be used in all cases where it is possible
 *  as it records the user.
 ****/
int CreditDB::Add(Credit *credit)
{
    FnTrace("CreditDB::Add(Credit)");
    int retval = 0;

    if (credit != nullptr)
    {
        if (credit->card_id == 0)
        {
            last_card_id += 1;
            credit->card_id = last_card_id;
            credit->db_type = db_type;
        }
        else if (credit->db_type != db_type)
        {
            // need to transfer a card from another CreditDB
            if (credit->db_type == CC_DBTYPE_EXCEPT)
                MasterSystem->cc_exception_db->Remove(credit->card_id);
            else if (credit->db_type == CC_DBTYPE_REFUND)
                MasterSystem->cc_refund_db->Remove(credit->card_id);
            else if (credit->db_type == CC_DBTYPE_EXCEPT)
                MasterSystem->cc_void_db->Remove(credit->card_id);
        }
        credit_list.AddToTail(credit);
    }

    return retval;
}

int CreditDB::Add(Terminal *term, Credit *credit)
{
    FnTrace("CreditDB::Add(Terminal, Credit)");
    int retval = 0;
    int user_id = term->user->id;
    
    if (db_type == CC_DBTYPE_VOID)
        credit->void_user_id = user_id;
    else if (db_type == CC_DBTYPE_REFUND)
        credit->refund_user_id = user_id;
    else if (db_type == CC_DBTYPE_EXCEPT)
        credit->except_user_id = user_id;

    if (credit->card_id == 0)
    {
        last_card_id += 1;
        credit->card_id = last_card_id;
        credit->db_type = db_type;
    }
    else if (credit->db_type != db_type)
    {
        // need to transfer a card from another CreditDB
        if (credit->db_type == CC_DBTYPE_EXCEPT)
            term->system_data->cc_exception_db->Remove(credit->card_id);
        else if (credit->db_type == CC_DBTYPE_REFUND)
            term->system_data->cc_refund_db->Remove(credit->card_id);
        else if (credit->db_type == CC_DBTYPE_EXCEPT)
            term->system_data->cc_void_db->Remove(credit->card_id);
    }
    credit_list.AddToTail(credit);

    return retval;
}

int CreditDB::Remove(int id)
{
    FnTrace("CreditDB::Remove()");
    int retval = 0;
    Credit *credit = credit_list.Head();

    while (credit != nullptr && credit->card_id != id)
        credit = credit->next;

    if (credit != nullptr && credit->card_id == id)
    {
        credit_list.Remove(credit);
    }

    return retval;
}

int CreditDB::MakeReport(Terminal *term, Report *report, LayoutZone *rzone)
{
    FnTrace("CreditDB::MakeReport()");
    int retval = 0;
    Credit *credit = nullptr;
    int color = COLOR_DEFAULT;
    int spacing = rzone->ColumnSpacing(term, 4);
    int indent = 0;
    const char* state;
    Settings *settings = term->GetSettings();

    if (credit_list.Count() < 1)
    {
        report->TextL("No transactions...");
    }
    else
    {
        credit = credit_list.Head();
        while (credit != nullptr)
        {
            indent = 0;
            report->TextPosL(indent, credit->PAN(settings->show_entire_cc_num));
            indent += spacing + 5;
            report->TextPosL(indent, credit->ExpireDate(), color);
            indent += spacing;
            report->TextPosL(indent, term->FormatPrice(credit->Total()), color);
            indent += spacing;
            state = FindStringByValue(credit->state, CreditCardStateValue,
                                      CreditCardStateName);
            report->TextPosL(indent, term->Translate(state), color);
            report->NewLine();
            credit = credit->next;
        }
    }

    report->is_complete = 1;

    return retval;
}

Credit *CreditDB::FindByRecord(Terminal *term, int record)
{
    FnTrace("CreditDB::FindByRecord()");
    Credit *retval = nullptr;
    Credit *curr = credit_list.Head();
    int count = 0;
    int done = 0;
    
    while (curr != nullptr && done == 0)
    {
        if (count == record)
        {
            retval = curr;
            done = 1;
        }
        else
        {
            count += 1;
            curr = curr->next;
        }

    }

    return retval;
}

int CreditDB::HaveOpenCards()
{
    FnTrace("CreditDB::HaveOpenCards()");
    int retval = 0;
    Credit *curr = credit_list.Head();

    while (curr != nullptr && retval == 0)
    {
        if (curr->GetStatus() != CCAUTH_VOID && curr->GetStatus() != CCAUTH_REFUND)
            retval = 1;
        else
            curr = curr->next;
    }

    return retval;
}


/*********************************************************************
 * CCBInfo Class
 ********************************************************************/

CCBInfo::CCBInfo()
{
    FnTrace("CCBInfo::CCBInfo()");

    type    = CC_INFO_NONE;
    name.Clear();
    Clear();
}

CCBInfo::CCBInfo(const char* newname)
{
    FnTrace("CCBInfo::CCBInfo(const char* )");

    name.Set(newname);
    type = CC_INFO_NONE;
    Clear();
}

CCBInfo::CCBInfo(const char* newname, int settype)
{
    FnTrace("CCBInfo::CCBInfo(const char* )");

    name.Set(newname);
    type = settype; // assign provided type to member
    Clear();
}

int CCBInfo::Add(Credit *credit)
{
    FnTrace("CCBInfo::Add(Credit)");
    int retval = 0;

    if (credit->Total() > 0)
    {
        numvt += 1;
        amtvt += credit->Total();
    }

    return retval;
}

int CCBInfo::Add(int amount)
{
    FnTrace("CCBInfo::Add(int)");
    int retval = 0;

    if (amount > 0)
    {
        numvt += 1;
        amtvt += amount;
    }

    return retval;
}

int CCBInfo::IsZero()
{
    FnTrace("CCBInfo::IsZero()");
    int retval = 0;

    if (numhost == 0 && numtr == 0)
        retval = 1;

    return retval;
}

void CCBInfo::Copy(CCBInfo *info)
{
    FnTrace("CCBInfo::Copy()");

    version = info->version;
    type = info->type;
    name.Set(info->name);
    numhost = info->numhost;
    amthost = info->amthost;
    numtr   = info->numtr;
    amttr   = info->amttr;
    numvt   = info->numvt;
    amtvt   = info->amtvt;
}

void CCBInfo::SetName(const char* newname)
{
    FnTrace("CCBInfo::SetName()");

    name.Set(newname);
}

void CCBInfo::Clear()
{
    FnTrace("CCBInfo::Clear()");

    numhost = 0;
    amthost = 0;
    numtr = 0;
    amttr = 0;
    numvt = 0;
    amtvt = 0;
}

int CCBInfo::CCBInfo::Read(InputDataFile &df)
{
    FnTrace("CCBInfo::Read()");
    int retval = 0;

    df.Read(version);
    df.Read(name);
    df.Read(numhost);
    df.Read(amthost);
    df.Read(numtr);
    df.Read(amttr);
    df.Read(numvt);
    df.Read(amtvt);

    return retval;
}

int CCBInfo::Write(OutputDataFile &df)
{
    FnTrace("CCBInfo::Write()");
    int retval = 0;

    df.Write(CCBINFO_VERSION);
    df.Write(name);
    df.Write(numhost);
    df.Write(amthost);
    df.Write(numtr);
    df.Write(amttr);
    df.Write(numvt);
    df.Write(amtvt);

    return retval;
}

int CCBInfo::ReadResults(Terminal *term)
{
    FnTrace("CCBInfo::ReadResults()");
    int retval = 0;

    name.Set(term->RStr());
    numhost = term->RInt8();
    amthost = term->RInt32();
    numtr   = term->RInt8();
    amttr   = term->RInt32();

    return retval;
}

int CCBInfo::MakeReport(Terminal *term, Report *report, int start, int spacing, int doubled)
{
    FnTrace("CCBInfo::MakeReport()");
    int retval = 0;
    int col6 = start;
    int col5 = col6 - spacing;
    int col4 = col5 - spacing;
    int col3 = col4 - spacing;
    int col2 = col3 - spacing;
    int col1 = col2 - spacing;
    int vt_num = numvt;
    int vt_amt = amtvt;

    if (term->hide_zeros == 0 || IsZero() == 0)
    {
        report->TextL(term->Translate(name.Value()));
        report->NumberPosR(col1, numhost);
        report->TextPosR(col2, term->FormatPrice(amthost));
        report->NumberPosR(col3, numtr);
        report->TextPosR(col4, term->FormatPrice(amttr));
        if (doubled)
        {
            if (vt_num)
                vt_num = vt_num / 2;
            if (vt_amt)
                vt_amt = vt_amt / 2;
        }
        report->NumberPosR(col5, vt_num);
        report->TextPosR(col6, term->FormatPrice(vt_amt));
        report->NewLine();
    }

    return retval;
}

void CCBInfo::DebugPrint()
{
    FnTrace("CCBInfo::DebugPrint()");

    printf("\t%-20s", name.Value());
    printf("\t\t%d\t%d\t%d\t%d\t%d\t%d\n",
           numhost, amthost, numtr, amttr, numvt, amtvt);
}


/*********************************************************************
 * CCSettle Class
 ********************************************************************/

CCSettle::CCSettle()
{
    FnTrace("CCSettle::CCSettle()");

    next = nullptr;
    fore = nullptr;
    filepath[0] = '\0';
    current = nullptr;
    archive = nullptr;
    Clear();
}

CCSettle::CCSettle(const char* fullpath)
{
    FnTrace("CCSettle::CCSettle()");

    next = nullptr;
    fore = nullptr;
    vt_safe_string::safe_copy(filepath, STRLONG, fullpath);
    current = nullptr;
    archive = nullptr;
    Clear();
}

CCSettle::~CCSettle()
{
    FnTrace("CCSettle::~CCSettle()");
    if (next != nullptr)
        delete next;
}

int CCSettle::Next(Terminal *term)
{
    FnTrace("CCSettle::Next()");
    int retval = 0;
    int loops = 0;
    Settings *settings = term->GetSettings();

    if (current == nullptr)
    {
        current = this;
    }
    else
    {
        // Need to find the next item.  Search both this current list
        // and archived lists.  We'll do multiple loops to try to
        // avoid grabbing NULLs.
        while (loops < MAX_LOOPS)
        {
            if (current != nullptr && current->next != nullptr)
                current = current->next;
            else
            { // search archives
                if (archive == nullptr)
                {
                    archive = MasterSystem->ArchiveList();
                    if (archive && archive->loaded == 0)
                        archive->LoadPacked(settings);
                }
                else
                {
                    do
                    {
                        archive = archive->next;
                        if (archive && archive->loaded == 0)
                            archive->LoadPacked(settings);
                    } while (archive != nullptr && archive->cc_settle_results == nullptr);
                }

                if (archive != nullptr)
                    current = archive->cc_settle_results;
                else
                    current = this;
            }
            loops += ((current != nullptr) ? MAX_LOOPS : 1);
        }
    }

    return retval;
}

int CCSettle::Fore(Terminal *term)
{
    FnTrace("CCSettle::Fore()");
    int retval = 0;
    int loops = 0;
    Settings *settings = term->GetSettings();

    if (current == nullptr)
    {
        current = this;
    }
    else
    {
        // Need to find the previous item.  Search both this current
        // list and archived lists.  We'll do multiple loops to try to
        // avoid grabbing NULLs.
        while (loops < MAX_LOOPS)
        {
            if (current != nullptr && current->fore != nullptr)
                current = current->fore;
            else
            { // search archives
                if (archive == nullptr)
                {
                    archive = MasterSystem->ArchiveListEnd();
                    if (archive && archive->loaded == 0)
                        archive->LoadPacked(settings);
                }
                else
                {
                    do
                    {
                        archive = archive->fore;
                        if (archive && archive->loaded == 0)
                            archive->LoadPacked(settings);
                    } while (archive != nullptr && archive->cc_settle_results == nullptr);
                }

                if (archive != nullptr)
                    current = archive->cc_settle_results;
                else
                    current = this;

                while (current!= nullptr && current->next != nullptr)
                    current = current->next;
            }
            loops += ((current != nullptr) ? MAX_LOOPS : 1);
        }
    }

    return retval;
}

CCSettle *CCSettle::Last()
{
    FnTrace("CCSettle:Last()");
    CCSettle *retval = this;

    while (retval->next != nullptr)
        retval = retval->next;

    return retval;
}

int CCSettle::Add(Terminal *term, const char* message)
{
    FnTrace("CCSettle::Add(Terminal)");
    int retval = 0;
    CCSettle *newsettle = nullptr;
    CCSettle *curr = nullptr;

    if (result.empty())
    {
        if (message != nullptr)
        {
            result.Set("Batch Settle Failed");
            errormsg.Set(message);
        }
        else
            ReadResults(term);
        current = this;
    }
    else
    {
        newsettle = new CCSettle();
        if (message != nullptr)
        {
            newsettle->result.Set("Batch Settle Failed");
            newsettle->errormsg.Set(message);
        }
        else
            newsettle->ReadResults(term);
        // Add to tail
        curr = this;
        while (curr->next != nullptr)
            curr = curr->next;
        curr->next = newsettle;
        newsettle->fore = curr;
        current = newsettle;
    }
    if (term->GetSettings()->authorize_method == CCAUTH_MAINSTREET)
    {
        current->bdate.Set(term->TimeDate(SystemTime, TD_DATETIMEY));
    }

    return retval;
}

int CCSettle::Add(Check *check)
{
    FnTrace("CCSettle::Add(Check)");
    int retval = 1;
    SubCheck *subcheck = check->SubList();
    Payment *payment = nullptr;
    Credit *credit = nullptr;

    while (subcheck != nullptr)
    {
        payment = subcheck->PaymentList();
        while (payment != nullptr)
        {
            if (payment->credit != nullptr)
            {
                credit = payment->credit;
                if (credit->CardType() == CARD_TYPE_DEBIT)
                {
                    debit.Add(credit);
                }
                else if (credit->CardType() == CARD_TYPE_CREDIT)
                {
                    switch (credit->CreditType())
                    {
                    case CREDIT_TYPE_VISA:
                        visa.Add(credit);
                        break;
                    case CREDIT_TYPE_MASTERCARD:
                        mastercard.Add(credit);
                        break;
                    case CREDIT_TYPE_AMEX:
                        amex.Add(credit);
                        break;
                    case CREDIT_TYPE_DISCOVER:
                        discover.Add(credit);
                        break;
                    case CREDIT_TYPE_DINERSCLUB:
                        diners.Add(credit);
                        break;
                    case CREDIT_TYPE_JCB:
                        jcb.Add(credit);
                        break;
                    }
                }
                purchase.Add(credit->AuthAmt());
                refund.Add(credit->RefundAmt());
                voids.Add(credit->VoidAmt());
            }
            payment = payment->next;
        }
        subcheck = subcheck->next;
    }

    return retval;
}

CCSettle *CCSettle::Copy()
{
    FnTrace("CCSettle::Copy()");
    auto *newsettle = new CCSettle();

    newsettle->result.Set(result);
    newsettle->settle.Set(settle);
    newsettle->termid.Set(termid);
    newsettle->op.Set(op);
    newsettle->merchid.Set(merchid);
    newsettle->seqnum.Set(seqnum);
    newsettle->shift.Set(shift);
    newsettle->batch.Set(batch);
    newsettle->bdate.Set(bdate);
    newsettle->btime.Set(btime);
    newsettle->receipt.Set(receipt);
    newsettle->display.Set(display);
    newsettle->iso.Set(iso);
    newsettle->b24.Set(b24);

    newsettle->visa.Copy(&visa);
    newsettle->mastercard.Copy(&mastercard);
    newsettle->amex.Copy(&amex);
    newsettle->diners.Copy(&diners);
    newsettle->debit.Copy(&debit);
    newsettle->discover.Copy(&discover);
    newsettle->jcb.Copy(&jcb);
    newsettle->purchase.Copy(&purchase);
    newsettle->refund.Copy(&refund);
    newsettle->voids.Copy(&voids);

    newsettle->settle_date.Set(settle_date);

    if (next != nullptr)
        newsettle->next = next->Copy();

    return newsettle;
}

void CCSettle::Clear()
{
    FnTrace("CCSettle::Clear()");

    if (next != nullptr)
        delete next;
    next = nullptr;

    result.Clear();
    settle.Clear();
    termid.Clear();
    op.Clear();
    merchid.Clear();
    seqnum.Clear();
    shift.Clear();
    batch.Clear();
    bdate.Clear();
    btime.Clear();
    receipt.Clear();
    display.Clear();
    iso.Clear();
    b24.Clear();

    visa.Clear();
    mastercard.Clear();
    amex.Clear();
    diners.Clear();
    debit.Clear();
    discover.Clear();
    jcb.Clear();
    purchase.Clear();
    refund.Clear();
    voids.Clear();

    settle_date.Clear();
}

/****
 * Read: Assumes we're starting the read at the head of the list, so
 * does not backtrack.
 ****/
int CCSettle::Read(InputDataFile &df)
{
    FnTrace("CCSettle::Read()");
    int retval = 0;
    int count = 0;
    int idx = 0;
    CCSettle *curr = this;
    CCSettle *node = nullptr;
    int version = 0;

    df.Read(version);
    df.Read(count);
    while (idx < count)
    {
        df.Read(curr->result);
        df.Read(curr->settle);
        df.Read(curr->termid);
        df.Read(curr->op);
        df.Read(curr->merchid);
        df.Read(curr->seqnum);
        df.Read(curr->shift);
        df.Read(curr->batch);
        df.Read(curr->bdate);
        df.Read(curr->btime);
        df.Read(curr->receipt);
        df.Read(curr->display);
        df.Read(curr->iso);
        df.Read(curr->b24);
        
        curr->visa.Read(df);
        curr->mastercard.Read(df);
        curr->amex.Read(df);
        curr->diners.Read(df);
        curr->debit.Read(df);
        curr->discover.Read(df);
        curr->jcb.Read(df);
        curr->purchase.Read(df);
        curr->refund.Read(df);
        curr->voids.Read(df);

        idx += 1;
        if (idx < count)
        {
            node = new CCSettle;
            curr->next = node;
            node->fore = curr;
            curr = curr->next;
        }
    }

    return retval;
}

int CCSettle::Write(OutputDataFile &df)
{
    FnTrace("CCSettle::Write()");
    int retval = 0;
    CCSettle *head = this;
    CCSettle *curr;
    int count = 0;
    int version = CREDIT_CARD_VERSION;

    while (head->fore != nullptr)
        head = head->fore;
    curr = head;
    while (curr != nullptr)
    {
        count += 1;
        curr = curr->next;
    }

    df.Write(version);
    df.Write(count);
    curr = head;
    while (curr != nullptr)
    {
        df.Write(curr->result);
        df.Write(curr->settle);
        df.Write(curr->termid);
        df.Write(curr->op);
        df.Write(curr->merchid);
        df.Write(curr->seqnum);
        df.Write(curr->shift);
        df.Write(curr->batch);
        df.Write(curr->bdate);
        df.Write(curr->btime);
        df.Write(curr->receipt);
        df.Write(curr->display);
        df.Write(curr->iso);
        df.Write(curr->b24);
        
        curr->visa.Write(df);
        curr->mastercard.Write(df);
        curr->amex.Write(df);
        curr->diners.Write(df);
        curr->debit.Write(df);
        curr->discover.Write(df);
        curr->jcb.Write(df);
        curr->purchase.Write(df);
        curr->refund.Write(df);
        curr->voids.Write(df);

        curr = curr->next;
    }

    return retval;
}

int CCSettle::Load(const char* filename)
{
    FnTrace("CCSettle::Load()");
    int retval = 0;
    InputDataFile infile;
    int version = 0;

    if (filename != nullptr && strlen(filename) > 0)
    {
        vt_safe_string::safe_copy(filepath, STRLONG, filename);
        
        if (infile.Open(filepath, version) == 0)
            Read(infile);
    }

    return retval;
}

int CCSettle::Save()
{
    FnTrace("CCSettle::Save()");
    int retval = 0;
    OutputDataFile outfile;
    int version = CREDIT_CARD_VERSION;

    if (filepath[0] == '\0')
        vt_safe_string::safe_copy(filepath, STRLONG, MASTER_CC_SETTLE);

    if (filepath[0] != '\0')
    {
        if (outfile.Open(filepath , version) == 0)
            Write(outfile);
    }

    return retval;
}

int CCSettle::ReadResults(Terminal *term)
{
    FnTrace("CCSettle::ReadResults()");
    int retval = 0;

    result.Set(term->RStr());
    settle.Set(term->RStr());
    termid.Set(term->RStr());
    op.Set(term->RStr());
    merchid.Set(term->RStr());
    seqnum.Set(term->RStr());
    shift.Set(term->RStr());
    batch.Set(term->RStr());
    bdate.Set(term->RStr());
    btime.Set(term->RStr());
    receipt.Set(term->RStr());
    display.Set(term->RStr());
    iso.Set(term->RStr());
    b24.Set(term->RStr());

    visa.ReadResults(term);
    mastercard.ReadResults(term);
    amex.ReadResults(term);
    diners.ReadResults(term);
    debit.ReadResults(term);
    discover.ReadResults(term);
    jcb.ReadResults(term);
    purchase.ReadResults(term);
    refund.ReadResults(term);
    voids.ReadResults(term);

    return retval;
}

int CCSettle::IsSettled()
{
    FnTrace("CCSettle::IsSettled()");
    int retval = 0;
    int authmethod = MasterSystem->settings.authorize_method;

    if (authmethod == CCAUTH_CREDITCHEQ && termid.size())
        retval = 1;
    else if (authmethod == CCAUTH_MAINSTREET && batch.size())
        retval = 1;

    return retval;
}

int CCSettle::GenerateReport(Terminal *term, Report *report, ReportZone *rzone, Archive *reparc)
{
    FnTrace("CCSettle::GenerateReport()");
    int retval = 0;
    char str[STRLENGTH];
    int column_spacing = rzone->ColumnSpacing(term, 8);
    int col6 = (rzone->x / rzone->font_width) - 2;
    int col5 = col6 - column_spacing;
    int col4 = col5 - column_spacing;
    int col3 = col4 - column_spacing;
    int col2 = col3 - column_spacing;
    int col1 = col2 - column_spacing;
    Settings *settings = term->GetSettings();

    if (IsSettled() || errormsg.size() > 0)
    {
        // Cache translated strings to avoid repeated translation calls
        const char* merchant_label = term->Translate("Merchant ID");
        const char* terminal_label = term->Translate("Terminal");

        if (merchid.size() > 0)
            vt_safe_string::safe_format(str, STRLENGTH, "%s: %s", merchant_label, merchid.Value());
        else
            vt_safe_string::safe_format(str, STRLENGTH, "%s: %s", merchant_label, settings->cc_merchant_id.Value());
        report->TextL(str);
        if (termid.size() > 0)
        {
            vt_safe_string::safe_format(str, STRLENGTH, "%s: %s", terminal_label, termid.Value());
            report->TextR(str);
        }
        report->NewLine();
        
        vt::cpp23::format_to_buffer(str, STRLENGTH, "{}: {} {}", term->Translate("Date/Time"),
                 bdate.Value(), btime.Value());
        report->TextL(str);
        vt::cpp23::format_to_buffer(str, STRLENGTH, "{}: {}", term->Translate("Batch"), batch.Value());
        report->TextR(str);
        report->NewLine(2);
        
        if (display.size() > 0)
        {
            report->Mode(PRINT_BOLD | PRINT_LARGE);
            report->TextC(display.Value());
            report->Mode(0);
            report->NewLine(2);
        }
        
        if (errormsg.size() > 0)
        {
            report->TextL("There was a problem closing the batch:");
            report->NewLine();
            report->TextL(errormsg.Value());
            report->NewLine();
        }
        else
        {
            report->Mode(PRINT_BOLD | PRINT_LARGE);
            report->TextPosR(col1, term->Translate("Host"));
            report->TextPosR(col2, term->Translate("Host"));
            report->TextPosR(col3, term->Translate("TRS"));
            report->TextPosR(col4, term->Translate("TRS"));
            report->TextPosR(col5, term->Translate("VT"));
            report->TextPosR(col6, term->Translate("VT"));
            report->NewLine();
            report->TextPosR(col1, term->Translate("Count"));
            report->TextPosR(col2, term->Translate("Amt"));
            report->TextPosR(col3, term->Translate("Count"));
            report->TextPosR(col4, term->Translate("Amt"));
            report->TextPosR(col5, term->Translate("Count"));
            report->TextPosR(col6, term->Translate("Amt"));
            report->NewLine();
            report->Mode(0);
            
            // Check for doubled batch counts
            int doubled = 0;
            if ((purchase.numhost > 0 && (purchase.numhost * 2) == purchase.numvt) ||
                (refund.numhost > 0 && (refund.numhost * 2) == refund.numvt) ||
                (voids.numhost > 0 && (voids.numhost * 2) == voids.numvt))
            {
                doubled = 1;
            }
            
            visa.MakeReport(term, report, col6, column_spacing, doubled);
            mastercard.MakeReport(term, report, col6, column_spacing, doubled);
            amex.MakeReport(term, report, col6, column_spacing, doubled);
            diners.MakeReport(term, report, col6, column_spacing, doubled);
            debit.MakeReport(term, report, col6, column_spacing, doubled);
            discover.MakeReport(term, report, col6, column_spacing, doubled);
            jcb.MakeReport(term, report, col6, column_spacing, doubled);
            purchase.MakeReport(term, report, col6, column_spacing, doubled);
            refund.MakeReport(term, report, col6, column_spacing, doubled);
            voids.MakeReport(term, report, col6, column_spacing, doubled);
        }
    }
    
    report->is_complete = 1;
    
    return retval;
}

int CCSettle::MakeReport(Terminal *term, Report *report, ReportZone *rzone)
{
    FnTrace("CCSettle::MakeReport()");
    int retval = 1;

    if (current == nullptr)
    {
        // Go to the last CCSettle in the linked list
        current = this;
        while (current->next != nullptr)
            current = current->next;
    }

    if (current != nullptr)
    {
        current->GenerateReport(term, report, rzone, archive);
        retval = 0;
    }

    return retval;
}

void CCSettle::DebugPrint()
{
    FnTrace("CCSettle::DebugPrint()");
    
    printf("CCSettle:\n");
    printf("\tResult:  %s\n", result.Value());
    printf("\tSettle:  %s\n", settle.Value());
    printf("\tTermID:  %s\n", termid.Value());
    printf("\tOperator:  %s\n", op.Value());
    printf("\tMerchant ID:  %s\n", merchid.Value());
    printf("\tSequence Number:  %s\n", seqnum.Value());
    printf("\tShift:  %s\n", shift.Value());
    printf("\tBatch:  %s\n", batch.Value());
    printf("\tDate:  %s\n", bdate.Value());
    printf("\tTime:  %s\n", btime.Value());
    printf("\tReceipt:  %s\n", receipt.Value());
    printf("\tDisplay:  %s\n", display.Value());
    printf("\tISO:  %s\n", iso.Value());
    printf("\tB24:  %s\n", b24.Value());

    visa.DebugPrint();
    mastercard.DebugPrint();
    amex.DebugPrint();
    diners.DebugPrint();
    debit.DebugPrint();
    discover.DebugPrint();
    jcb.DebugPrint();
    purchase.DebugPrint();
    refund.DebugPrint();
    voids.DebugPrint();
}


/*********************************************************************
 * CCInit Class -- stores terminal initialization results
 ********************************************************************/

CCInit::CCInit()
{
    FnTrace("CCInit::CCInit()");

    filepath[0] = '\0';
    next = nullptr;
    fore = nullptr;
    current = nullptr;
    archive = nullptr;
}

CCInit::CCInit(const char* fullpath)
{
    FnTrace("CCInit::CCInit()");

    vt_safe_string::safe_copy(filepath, STRLONG, fullpath);
    next = nullptr;
    fore = nullptr;
    current = nullptr;
    archive = nullptr;
}

CCInit::~CCInit()
{
    FnTrace("CCInit::~CCInit()");

    if (next != nullptr)
        delete next;
}

int CCInit::Next(Terminal *term)
{
    FnTrace("CCInit::Next()");
    int retval = 0;
    int loops = 0;
    Settings *settings = term->GetSettings();
    
    if (current == nullptr)
    {
        current = this;
    }
    else
    {
        // Need to find the next item.  Search both this current list
        // and archived lists.  We'll do multiple loops to try to
        // avoid grabbing NULLs.
        while (loops < MAX_LOOPS)
        {
            if (current != nullptr && current->next != nullptr)
                current = current->next;
            else
            { // search archives
                if (archive == nullptr)
                {
                    archive = MasterSystem->ArchiveList();
                    if (archive && archive->loaded == 0)
                        archive->LoadPacked(settings);
                }
                else
                {
                    do
                    {
                        archive = archive->next;
                        if (archive && archive->loaded == 0)
                            archive->LoadPacked(settings);
                    } while (archive != nullptr && archive->cc_init_results == nullptr);
                }

                if (archive != nullptr)
                    current = archive->cc_init_results;
                else
                    current = this;
            }
            loops += ((current != nullptr) ? MAX_LOOPS : 1);
        }
    }

    return retval;
}

int CCInit::Fore(Terminal *term)
{
    FnTrace("CCInit::Fore()");
    int retval = 0;
    int loops = 0;
    Settings *settings = term->GetSettings();

    if (current == nullptr)
    {
        current = this;
    }
    else
    {
        // Need to find the previous item.  Search both this current
        // list and archived lists.  We'll do multiple loops to try to
        // avoid grabbing NULLs.
        while (loops < MAX_LOOPS)
        {
            if (current != nullptr && current->fore != nullptr)
                current = current->fore;
            else
            { // search archives
                if (archive == nullptr)
                {
                    archive = MasterSystem->ArchiveListEnd();
                    if (archive && archive->loaded == 0)
                        archive->LoadPacked(settings);
                }
                else
                {
                    do
                    {
                        archive = archive->fore;
                        if (archive && archive->loaded == 0)
                            archive->LoadPacked(settings);
                    } while (archive != nullptr && archive->cc_init_results == nullptr);
                }

                if (archive != nullptr)
                    current = archive->cc_init_results;
                else
                {
                    current = this;
                    while (current->next != nullptr)
                        current = current->next;
                }
            }
            loops += ((current != nullptr) ? MAX_LOOPS : 1);
        }
    }

    return retval;
}

int CCInit::Read(InputDataFile &df)
{
    FnTrace("CCInit::Read()");
    int retval = 0;
    int count;
    Str *currstr;
    int version = 0;

    df.Read(version);
    df.Read(count);
    while (count > 0)
    {
        currstr = new Str();
        df.Read(currstr);
        init_list.AddToTail(currstr);
        count -= 1;
    }

    return retval;
}

int CCInit::Write(OutputDataFile &df)
{
    FnTrace("CCInit::Write()");
    int retval = 0;
    int count = init_list.Count();
    Str *currstr = init_list.Head();
    int version = CREDIT_CARD_VERSION;

    df.Write(version);
    df.Write(count);

    while (currstr != nullptr)
    {
        df.Write(currstr);
        currstr = currstr->next;
    }

    return retval;
}

int CCInit::Load(const char* filename)
{
    FnTrace("CCInit::Load()");
    int retval = 0;
    InputDataFile infile;
    int version = 0;

    if (filename != nullptr)
    {
        vt_safe_string::safe_copy(filepath, STRLONG, filename);
        if (infile.Open(filepath, version) == 0)
            Read(infile);
    }

    return retval;
}

int CCInit::Save()
{
    FnTrace("CCInit::Save()");
    int retval = 0;
    OutputDataFile outfile;
    int version = CREDIT_CARD_VERSION;

    if (filepath[0] == '\0')
        vt_safe_string::safe_copy(filepath, STRLONG, MASTER_CC_INIT);

    if (filepath[0] != '\0')
    {
        if (outfile.Open(filepath, version) == 0)
            Write(outfile);
    }

    return retval;
}

int CCInit::Add(const char* termid, const char* result)
{
    FnTrace("CCInit::Add()");
    int retval = 0;
    Str *newstr = new Str();
    char buffer[STRLONG];
    Terminal *term = MasterControl->TermList();
    TimeInfo now;
    int datefmt = TD_SHORT_MONTH | TD_NO_DAY | TD_PAD | TD_SHORT_TIME;

    now.Set();
    vt::cpp23::format_to_buffer(buffer, STRLONG, "{}  {}: {}", term->TimeDate(now, datefmt),
             termid, result);
    newstr->Set(buffer);
    init_list.AddToTail(newstr);

    return retval;
}

int CCInit::MakeReport(Terminal *term, Report *report, ReportZone *rzone)
{
    FnTrace("CCInit::MakeReport()");
    int retval = 0;

    if (current == nullptr)
    {
        current = this;
        while (current->next != nullptr)
            current = current->next;
    }

    if (current != nullptr)
    {
        current->GenerateReport(term, report, rzone);
        retval = 0;
    }

    return retval;
}

int CCInit::GenerateReport(Terminal *term, Report *report, ReportZone *rzone)
{
    FnTrace("CCInit::GenerateReport()");
    int retval = 0;
    Str *currstr = init_list.Head();

    while (currstr != nullptr)
    {
        report->TextL(currstr->Value(), COLOR_DEFAULT);
        report->NewLine();
        currstr = currstr->next;
    }

    report->NewLine();
    report->is_complete = 1;

    return retval;
}


/*********************************************************************
 * CCDetails Class -- stores results of a details query
 ********************************************************************/

CCDetails::CCDetails()
{
    FnTrace("CCDetails::CCDetails()");
    next = nullptr;
    fore = nullptr;
}

CCDetails::~CCDetails()
{
    FnTrace("CCDetails::~CCDetails()");

    if (next != nullptr)
        delete next;
}

void CCDetails::Clear()
{
    FnTrace("CCDetails::Clear()");

    if (MasterSystem->settings.authorize_method == CCAUTH_MAINSTREET)
        mcve_list.Purge();
}

int CCDetails::Count()
{
    FnTrace("CCDetails::Count");
    int retval = 0;

    if (MasterSystem->settings.authorize_method == CCAUTH_MAINSTREET)
        retval = mcve_list.Count();

    return retval;
}

int CCDetails::Add()
{
    FnTrace("CCDetails::Add()");
    int retval = 0;
    
    return retval;
}

int CCDetails::Add(const char* line)
{
    FnTrace("CCDetails::Add()");
    int retval = 0;
    Str *mcve_line;

    if (MasterSystem->settings.authorize_method == CCAUTH_MAINSTREET)
    {
        mcve_line = new Str();
        mcve_line->Set(line);
        mcve_list.AddToTail(mcve_line);
    }
    
    return retval;
}

int CCDetails::MakeReport(Terminal *term, Report *report, ReportZone *rzone)
{
    FnTrace("CCDetails::MakeReport()");
    int retval = 0;
    Str *currstr = nullptr;

    if (MasterSystem->settings.authorize_method == CCAUTH_MAINSTREET &&
        report != nullptr)
    {
        currstr = mcve_list.Head();
        report->Mode(0);
        report->NewLine();

        while (currstr != nullptr)
        {
            report->TextL(currstr->Value());
            report->NewLine();
            currstr = currstr->next;
        }
    }

    return retval;
}

/*********************************************************************
 * CCSAFDetails Class -- stores results of a details query
 ********************************************************************/

CCSAFDetails::CCSAFDetails()
{
    FnTrace("CCSAFDetails::CCSAFDetails()");

    filepath[0] = '\0';
    next = nullptr;
    fore = nullptr;
    current = nullptr;
    archive = nullptr;
    Clear();
}

CCSAFDetails::CCSAFDetails(const char* fullpath)
{
    FnTrace("CCSAFDetails::CCSAFDetails()");

    vt_safe_string::safe_copy(filepath, STRLONG, fullpath);
    next = nullptr;
    fore = nullptr;
    current = nullptr;
    archive = nullptr;
    Clear();
}

CCSAFDetails::~CCSAFDetails()
{
    FnTrace("CCSAFDetails::~CCSAFDetails()");

    if (next != nullptr)
        delete next;
}

void CCSAFDetails::Clear()
{
    FnTrace("CCSAFDetails::Clear()");

    terminal.Clear();
    batch.Clear();
    op.Clear();
    merchid.Clear();
    safdate.Clear();
    saftime.Clear();
    display.Clear();
    safnum.Clear();
    numrecords = 0;
    notproc = 0;
    completed = 0;
    declined = 0;
    errors = 0;
    voided = 0;
    expired = 0;
    last = 0;

    if (next != nullptr)
        delete next;
    next = nullptr;
    fore = nullptr;
}

int CCSAFDetails::Next(Terminal *term)
{
    FnTrace("CCSAFDetails::Next()");
    int retval = 0;
    int loops = 0;
    Settings *settings = term->GetSettings();

    if (current == nullptr)
    {
        current = this;
    }
    else
    {
        // Need to find the next item.  Search both this current list
        // and archived lists.  We'll do multiple loops to try to
        // avoid grabbing NULLs.
        while (loops < MAX_LOOPS)
        {
            if (current != nullptr && current->next != nullptr)
                current = current->next;
            else
            { // search archives
                if (archive == nullptr)
                {
                    archive = MasterSystem->ArchiveList();
                    if (archive && archive->loaded == 0)
                        archive->LoadPacked(settings);
                }
                else
                {
                    do
                    {
                        archive = archive->next;
                        if (archive && archive->loaded == 0)
                            archive->LoadPacked(settings);
                    } while (archive != nullptr && archive->cc_saf_details_results == nullptr);
                }

                if (archive != nullptr)
                    current = archive->cc_saf_details_results;
                else
                    current = this;
            }
            loops += ((current != nullptr) ? MAX_LOOPS : 1);
        }
    }

    return retval;
}

int CCSAFDetails::Fore(Terminal *term)
{
    FnTrace("CCSAFDetails::Fore()");
    int retval = 0;
    int loops = 0;
    Settings *settings = term->GetSettings();

    if (current == nullptr)
    {
        current = this;
    }
    else
    {
        // Need to find the previous item.  Search both this current
        // list and archived lists.  We'll do multiple loops to try to
        // avoid grabbing NULLs.
        while (loops < MAX_LOOPS)
        {
            if (current != nullptr && current->fore != nullptr)
                current = current->fore;
            else
            { // search archives
                if (archive == nullptr)
                {
                    archive = MasterSystem->ArchiveListEnd();
                    if (archive && archive->loaded == 0)
                        archive->LoadPacked(settings);
                }
                else
                {
                    do
                    {
                        archive = archive->fore;
                        if (archive && archive->loaded == 0)
                            archive->LoadPacked(settings);
                    } while (archive != nullptr && archive->cc_saf_details_results == nullptr);
                }

                if (archive != nullptr)
                    current = archive->cc_saf_details_results;
                else
                {
                    current = this;
                    while (current->next != nullptr)
                        current = current->next;
                }
            }
            loops += ((current != nullptr) ? MAX_LOOPS : 1);
        }
    }

    return retval;
}

CCSAFDetails *CCSAFDetails::Last()
{
    FnTrace("CCSAFDetails::Last()");
    CCSAFDetails *retval = this;

    while (retval->next != nullptr)
        retval = retval->next;

    return retval;
}

int CCSAFDetails::Read(InputDataFile &df)
{
    FnTrace("CCSAFDetails::Read()");
    int retval = 0;
    CCSAFDetails *curr = this;
    CCSAFDetails *node = nullptr;
    int count = 0;
    int idx = 0;
    int version = 0;

    df.Read(version);
    df.Read(count);
    while (idx < count)
    { 
        df.Read(curr->terminal);
        df.Read(curr->batch);
        df.Read(curr->op);
        df.Read(curr->merchid);
        df.Read(curr->safdate);
        df.Read(curr->saftime);
        df.Read(curr->display);
        df.Read(curr->safnum);
        df.Read(curr->numrecords);
        df.Read(curr->notproc);
        df.Read(curr->completed);
        df.Read(curr->declined);
        df.Read(curr->errors);
        df.Read(curr->voided);
        df.Read(curr->expired);
        df.Read(curr->last);

        idx += 1;
        if (idx < count)
        {
            node = new CCSAFDetails();
            curr->next = node;
            node->fore = curr;
            curr = curr->next;
        }
    }

    return retval;
}

int CCSAFDetails::Write(OutputDataFile &df)
{
    FnTrace("CCSAFDetails::Write()");
    int retval = 0;
    CCSAFDetails *head = this;
    CCSAFDetails *curr;
    int count = 0;
    int version = CREDIT_CARD_VERSION;

    while (head->fore != nullptr)
        head = head->fore;
    curr = head;
    while (curr != nullptr)
    {
        count += 1;
        curr = curr->next;
    }

    df.Write(version);
    df.Write(count);
    curr = head;
    while (curr != nullptr)
    {
        df.Write(curr->terminal);
        df.Write(curr->batch);
        df.Write(curr->op);
        df.Write(curr->merchid);
        df.Write(curr->safdate);
        df.Write(curr->saftime);
        df.Write(curr->display);
        df.Write(curr->safnum);
        df.Write(curr->numrecords);
        df.Write(curr->notproc);
        df.Write(curr->completed);
        df.Write(curr->declined);
        df.Write(curr->errors);
        df.Write(curr->voided);
        df.Write(curr->expired);
        df.Write(curr->last);

        curr = curr->next;
    }

    return retval;
}

int CCSAFDetails::Load(const char* filename)
{
    FnTrace("CCSAFDetails::Load()");
    int retval = 0;
    InputDataFile infile;
    int version;

    if (filename != nullptr)
    {
        vt_safe_string::safe_copy(filepath, STRLONG, filename);
        if (infile.Open(filepath, version) == 0)
            Read(infile);
    }

    return retval;
}

int CCSAFDetails::Save()
{
    FnTrace("CCSAFDetails::Save()");
    int retval = 0;
    OutputDataFile outfile;
    int version = CREDIT_CARD_VERSION;

    if (filepath[0] == '\0')
        vt_safe_string::safe_copy(filepath, STRLONG, MASTER_CC_SAF);

    if (filepath[0] != '\0')
    {
        if (outfile.Open(filepath, version) == 0)
            Write(outfile);
    }

    return retval;
}

int CCSAFDetails::Count()
{
    FnTrace("CCSAFDetails::Count()");
    int retval = 0;
    CCSAFDetails *node = next;

    if (terminal.size() > 0)
    {
        retval = 1;
        while (node != nullptr)
        {
            retval += 1;
            node = node->next;
        }
    }

    return retval;
}

int CCSAFDetails::ReadResults(Terminal *term)
{
    FnTrace("CCSAFDetails::ReadResults()");
    int error = 0;

    terminal.Set(term->RStr());
    batch.Set(term->RStr());
    op.Set(term->RStr());
    merchid.Set(term->RStr());
    safdate.Set(term->RStr());
    saftime.Set(term->RStr());
    display.Set(term->RStr());
    safnum.Set(term->RStr());
    numrecords = atoi(term->RStr());
    notproc = atoi(term->RStr());
    completed = atoi(term->RStr());
    declined = atoi(term->RStr());
    errors = atoi(term->RStr());
    voided = atoi(term->RStr());
    expired = atoi(term->RStr());
    last = atoi(term->RStr());

    return error;
}

int CCSAFDetails::Add(Terminal *term)
{
    FnTrace("CCSAFDetails::Add()");
    int retval = 0;
    CCSAFDetails *newsaf = nullptr;
    CCSAFDetails *node = this;

    if (terminal.empty())
    {
        ReadResults(term);
    }
    else
    {
        while (node->next != nullptr)
            node = node->next;
        newsaf = new CCSAFDetails;
        newsaf->ReadResults(term);
        node->next = newsaf;
        newsaf->fore = node;
        current = newsaf;
    }

    return retval;
}

int CCSAFDetails::MakeReport(Terminal *term, Report *report, ReportZone *rzone)
{
    FnTrace("CCSAFDetails::MakeReport()");
    int retval = 0;

    if (current == nullptr)
    {
        current = this;
        while (current->next != nullptr)
            current = current->next;
    }

    if (current != nullptr)
    {
        current->GenerateReport(term, report, rzone);
        retval = 0;
    }

    return retval;
}

int CCSAFDetails::GenerateReport(Terminal *term, Report *report, ReportZone *rzone)
{
    FnTrace("CCSAFDetails::GenerateReport()");
    int retval = 0;
    char str[STRLENGTH];

    if (!IsEmpty())
    {
        report->Mode(PRINT_BOLD | PRINT_LARGE);
        report->TextC(term->Translate(display.Value()));
        report->Mode(0);
        report->NewLine();
        
        vt::cpp23::format_to_buffer(str, STRLENGTH, "{}: {}", term->Translate("Terminal"), terminal.Value());
        report->TextL(str);
        vt::cpp23::format_to_buffer(str, STRLENGTH, "{}: {} {}", term->Translate("Date/Time"),
                 safdate.Value(), saftime.Value());
        report->TextR(str);
        report->NewLine();
        
        vt::cpp23::format_to_buffer(str, STRLENGTH, "{}: {}", term->Translate("Batch"), batch.Value());
        report->TextL(str);
        vt::cpp23::format_to_buffer(str, STRLENGTH, "{}: {}", term->Translate("SAF Number"), safnum.Value());
        report->TextR(str);
        report->NewLine();
        
        vt::cpp23::format_to_buffer(str, STRLENGTH, "{}: {}", term->Translate("Merchant ID"), merchid.Value());
        report->TextL(str);
        report->NewLine(2);
        
        report->TextL(term->Translate("Number of Records"));
        report->NumberR(numrecords);
        report->NewLine();
        report->TextL(term->Translate("Last Processed Record"));
        report->NumberR(last);
        report->NewLine();
        report->TextL(term->Translate("New Records"));
        report->NumberR(notproc);
        report->NewLine();
        report->TextL(term->Translate("Completed Records"));
        report->NumberR(completed);
        report->NewLine();
        report->TextL(term->Translate("Declined Records"));
        report->NumberR(declined);
        report->NewLine();
        report->TextL(term->Translate("Error Records"));
        report->NumberR(errors);
        report->NewLine();
        report->TextL(term->Translate("Voided Records"));
        report->NumberR(voided);
        report->NewLine();
        report->TextL(term->Translate("Expired Records"));
        report->NumberR(expired);
        report->NewLine(3);
    }
        
    report->is_complete = 1;
        
    return retval;
}


/*********************************************************************
 * Functions
 ********************************************************************/

/****
 * CC_CreditType:  returns credit card type based on account number
 ****/
int CC_CreditType(const char* a)
{
    FnTrace("CC_CreditType()");
    if (a == nullptr)
        return CREDIT_TYPE_UNKNOWN;

    int len = strlen(a);
    if (len < 13 || len > 16)
        return CREDIT_TYPE_UNKNOWN;

    int v = (a[0]-'0')*1000 + (a[1]-'0')*100 + (a[2]-'0')*10 + (a[3]-'0');

    if ((len == 13 || len == 16) && v >= 4000 && v <= 4999)
        return CREDIT_TYPE_VISA;
    if (len == 16 && v >= 5100 && v <= 5599)
        return CREDIT_TYPE_MASTERCARD;
    if (len == 15 && ((v >= 3400 && v <= 3499) || (v >= 3700 && v <= 3799)))
        return CREDIT_TYPE_AMEX;
    if (len == 16 && v == 6011)
        return CREDIT_TYPE_DISCOVER;
    if ((len == 14 || len == 16) && ((v >= 3000 && v <= 3059) ||
                                     (v >= 3600 && v <= 3699) || (v >= 3800 && v <= 3899)))
        return CREDIT_TYPE_DINERSCLUB;
    if (len == 16 && v >= 3528 && v <= 3589)
        return CREDIT_TYPE_JCB;
    return CREDIT_TYPE_UNKNOWN;
}

/****
 * CC_IsValidAccountNumber: checks account number with Luhn check-digit
 *   algorithm - returns boolean
 ****/
int CC_IsValidAccountNumber(const char* account_no)
{
    FnTrace("CC_IsValidAccountNumber()");
    int retval = 0;
    int idx = 0;
    int flag = 1;
    int num;
    int total = 0;
    int checksum;
    int checkdigit;

    while (account_no[idx] != '\0' &&
           account_no[idx] >= '0' &&
           account_no[idx] <= '9' &&
           idx < MAX_PAN_LEN)
    {
        idx += 1;
    }
    idx -= 1;

    checksum = account_no[idx] - '0';
    idx -= 1;

    while (idx >= 0)
    {
        num = account_no[idx] - '0';
        if (flag)
        {
            num *= 2;
            if (num >= 10)
            {
                total += 1;
                num -= 10;
            }
            flag = 0;
        }
        else
        {
            flag = 1;
        }
        total += num;
        idx -= 1;
    }
    if (total % 10)
    {
        checkdigit = ((total / 10) * 10) + 10;
        checkdigit = checkdigit - total;
    }
    else
        checkdigit = 0;

    retval = (checkdigit == checksum);
    
    return retval;
}

int CC_IsValidExpiry(const char* expiry)
{
    FnTrace("CC_IsValidExpiry()");
    int retval = 1;
    time_t    now;
    struct tm *current;
    int month;
    int curr_month;
    int year;
    int curr_year;
    char buff[3];
    
    // get expiry month
    buff[0] = expiry[0];
    buff[1] = expiry[1];
    buff[2] = '\0';
    month = atoi(buff);

    // get expiry year
    buff[0] = expiry[2];
    buff[1] = expiry[3];
    buff[2] = '\0';
    year = atoi(buff) + 2000;

    // get current month and year
    now = time(nullptr);
    current    = localtime(&now);
    curr_month = current->tm_mon + 1;
    curr_year  = current->tm_year + 1900;

    if (month < 1 || month > 12)
        retval = 0;
    else if (year < curr_year || month > (curr_year + 10))
        retval = 0;
    else if (year == curr_year && month < curr_month)
        retval = 0;

    return retval;
}
