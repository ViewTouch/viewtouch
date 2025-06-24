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
 * term_credit.cc -- terminal specific credit card processing functions.
 *   Many/most of these functions can lock the process during connection, so
 *   keeping them in vt_term ensures that only the local terminal will be locked.
 */

/*********************************************************************
 * NOTE:  This is the empty CCard object stubbed out for compilation
 *  without credit card support.  Eventually I'll rework the credit
 *  card stuff so that we don't have to use stubs, but I don't want
 *  to worry about it right now.
 ********************************************************************/


#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "credit.hh"
#include "term_credit.hh"
#include "term_view.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/*********************************************************************
 * CCard Class
 ********************************************************************/
CCard::CCard()
{
    trans_success = 0;
    intcode = CC_STATUS_NONE;
}

int CCard::Read()
{
    FnTrace("CCard::Read()");
    int retval = 0;

    RStr(server);
    RStr(port);
    RStr(user);
    RStr(password);
    RStr(termid);
    RStr(approval);
    RStr(number);
    RStr(name);
    RStr(expire);
    RStr(code);
    intcode = RInt8();
    RStr(verb);
    RStr(auth);
    batch = RLLong();
    item  = RLLong();
    ttid  = RLLong();
    RStr(AVS);
    RStr(CV);
    amount = RInt32();
    fullamount = RInt32();
    card_type  = RInt8();

    // specific to CreditCheq
    RStr(reference);
    RStr(sequence);
    RStr(server_date);
    RStr(server_time);
    RStr(receipt_line);
    RStr(display_line);

    return retval;
}

int CCard::Write()
{
    FnTrace("CCard::Write()");
    int retval = 0;

    WStr(approval);
    WStr(number);
    WStr(expire);
    WStr(name);
    WStr(country);
    WInt8(debit_acct);
    WStr(code);
    WInt8(intcode);
    WStr(verb);
    WStr(auth);
    WLLong(batch);
    WLLong(item);
    WLLong(ttid);
    WStr(AVS);
    WStr(CV);
    WInt8(trans_success);

    // specific to CreditCheq
    WStr(reference);
    WStr(sequence);
    WStr(server_date);
    WStr(server_time);
    WStr(receipt_line);
    WStr(display_line);

    return retval;
}

int CCard::Clear()
{
    Close();

    server[0] = '\0';
    port[0] = '\0';
    user[0] = '\0';
    password[0] = '\0';
    termid[0] = '\0';
    approval[0] = '\0';
    number[0] = '\0';
    expire[0] = '\0';
    name[0] = '\0';
    country[0] = '\0';
    debit_acct = DEBIT_ACCT_NONE;
    code[0] = '\0';
    intcode = 0;
    verb[0] = '\0';
    auth[0] = '\0';
    AVS[0] = '\0';
    CV[0] = '\0';
    batch = 0;
    item = 0;
    ttid = 0;
    amount = 0;
    fullamount = 0;
    trans_success = 0;
    card_type = 0;

    // specific to CreditCheq
    reference[0] = '\0';
    sequence[0] = '\0';
    server_date[0] = '\0';
    server_time[0] = '\0';
    receipt_line[0] = '\0';
    display_line[0] = '\0';

    return 0;
}

int CCard::Connect()
{
    FnTrace("CCard::Connect()");
    int retval = 1;

    return retval;
}

int CCard::Close()
{
    FnTrace("CCard::Close()");
    int retval = 1;

    return retval;
}

int CCard::Sale()
{
    FnTrace("CCard::Sale()");
    int retval = 1;  // 1 == failure, 0 == success

    return retval;
}

int CCard::PreAuth()
{
    FnTrace("CCard::PreAuth()");
    int retval = 0;

    return retval;
}

int CCard::FinishAuth()
{
    FnTrace("CCard::FinishAuth()");
    int retval = 0;

    return retval;
}

int CCard::Void()
{
    FnTrace("CCard::Void()");
    int retval = 0;

    return retval;
}

int CCard::VoidCancel()
{
    FnTrace("CCard::VoidCancel()");
    int retval = 0;

    return retval;
}

int CCard::Refund()
{
    FnTrace("CCard::Refund()");
    int retval = 0;

    return retval;
}

int CCard::RefundCancel()
{
    FnTrace("CCard::RefundCancel()");
    int retval = 0;

    return retval;
}

int CCard::BatchSettle()
{
    FnTrace("CCard::BatchSettle()");
    int retval = 0;

    return retval;
}

int CCard::CCInit()
{
    FnTrace("CCard::CCInit()");
    int retval = 0;

    return retval;
}

int CCard::Totals()
{
    FnTrace("CCard::Totals()");
    int retval = 0;

    return retval;
}

int CCard::Details()
{
    FnTrace("CCard::Details()");
    int retval = 0;

    return retval;
}

int CCard::ClearSAF()
{
    FnTrace("CCard::ClearSAF()");
    int retval = 0;

    return retval;
}

int CCard::SAFDetails()
{
    FnTrace("CCard::SAFDetails()");
    int retval = 0;

    return retval;
}
