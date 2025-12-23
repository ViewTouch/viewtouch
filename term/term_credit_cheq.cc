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
 * term_credit.cc -- terminal specific credit card processing functions.
 *   Many/most of these functions can lock the process during connection, so
 *   keeping them in vt_term ensures that only the local terminal will be locked.
 */

#include <cerrno>
#include <fcntl.h>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <cctype>
#include "credit.hh"
#include "locale.hh"
#include "remote_link.hh"
#include "term_credit_cheq.hh"
#include "term_view.hh"
#include "utility.hh"
#include "safe_string_utils.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define STRHUGE   16384

#define NUMBER_MANUAL    1
#define NUMBER_SWIPED    0

int CCQ_AddString(char* dest, const char* src, int length, char pad = ' ');
int CCQ_GetStringDelim(char* dest, const char* src, int start, char edelim, char sdelim = 0x00);
int CCQ_GetStringCount(char* dest, const char* src, int start, int count);
int CCQ_GetKeyValue(const char* destkey, const char* destval, const char* src, int start);
int CCQ_GetDebitAccount(const char* account_string);
int CCQ_GetSwiped(const char* swiped_string);


/*********************************************************************
 * Functions for connect timeouts and non-blocking sockets (or
 *  rather, easy conversion between blocking and non-blocking).
 ********************************************************************/

#ifndef LINUX
/****
 * connect_alarm:  For connect() timeouts in FreeBSD (doesn't work in Linux).
 *  Just needs to return; the alarm itself generates the EINTR for connect().
 ****/
static void connect_alarm(int signo)
{
    return;
}
#endif

/****
 * my_connect:  Does a timeout, the mechanism of which depends on the OS.
 *  Returns 0 on successful connect, 1 otherwise.
 ****/
int my_connect(int sockfd, struct sockaddr *serv_addr, socklen_t addrlen, int timeout)
{
    FnTrace("my_connect()");
    int retval = 1;

#ifdef LINUX
    int flags = fcntl(sockfd, F_GETFL, 0);
    fd_set readset;
    fd_set writeset;
    struct timeval timev;
    int result;
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    result = connect(sockfd, serv_addr, addrlen);
    if (result < 0 && errno == EINPROGRESS)
    {
        FD_ZERO(&readset);
        FD_SET(sockfd, &readset);
        writeset = readset;
        timev.tv_sec = timeout;
        timev.tv_usec = 0;
        result = select(sockfd + 1, &readset, &writeset, nullptr, &timev);
        if (FD_ISSET(sockfd, &readset) || FD_ISSET(sockfd, &writeset))
            retval = 0;
        else
            perror("Select");
    }
    else if (result == 0)
        retval = 0;
    else
        perror("Connect");

    fcntl(sockfd, F_SETFL, flags);
#else
    sig_t sigfunc;
    sigfunc = signal(SIGALRM, connect_alarm);
    alarm(timeout);
    if (connect(sockfd, serv_addr, addrlen) == 0)
        retval = 0;
    alarm(0);
    signal(SIGALRM, sigfunc);
#endif

    return retval;
}


/*********************************************************************
 * Classes for Parsing
 ********************************************************************/

class SAFClear {
    char terminal[STRLENGTH];
    char batch[STRLENGTH];
    char op[STRLENGTH];
    char merchid[STRLENGTH];
    char safdate[STRLENGTH];
    char saftime[STRLENGTH];
    char display[STRLENGTH];
    char safnum[STRLENGTH];
    char numrecords[STRLENGTH];
    char notproc[STRLENGTH];
    char completed[STRLENGTH];
    char declined[STRLENGTH];
    char errors[STRLENGTH];
    char voided[STRLENGTH];
    char expired[STRLENGTH];
    char last[STRLENGTH];

public:
    SAFClear();
    int Write();
    int ParseSAF(const char* results);
    void DebugPrint();
};

SAFClear::SAFClear()
{
    FnTrace("SAFClear::SAFClear()");
    terminal[0]   = '\0';
    batch[0]      = '\0';
    op[0]         = '\0';
    merchid[0]    = '\0';
    safdate[0]    = '\0';
    saftime[0]    = '\0';
    display[0]    = '\0';
    safnum[0]     = '\0';
    numrecords[0] = '\0';
    notproc[0]    = '\0';
    completed[0]  = '\0';
    declined[0]   = '\0';
    errors[0]     = '\0';
    voided[0]     = '\0';
    expired[0]    = '\0';
    last[0]       = '\0';
}

int SAFClear::Write()
{
    FnTrace("SAFClear::Write()");
    int error = 0;

    error += WStr(terminal);
    error += WStr(batch);
    error += WStr(op);
    error += WStr(merchid);
    error += WStr(safdate);
    error += WStr(saftime);
    error += WStr(display);
    error += WStr(safnum);
    error += WStr(numrecords);
    error += WStr(notproc);
    error += WStr(completed);
    error += WStr(declined);
    error += WStr(errors);
    error += WStr(voided);
    error += WStr(expired);
    error += WStr(last);

    return error;
}

int SAFClear::ParseSAF(const char* results)
{
    FnTrace("SAFClear::ParseSAF()");
    int retval = 1;
    int idx = 0;
    char key[STRLENGTH];
    char value[STRLENGTH];
    int len = strlen(results);

    while (idx < len)
    {
        idx = CCQ_GetKeyValue(key, value, results, idx);
        if (strncmp(key, "TRM", 3) == 0)
            vt_safe_string::safe_copy(terminal, STRLENGTH, value);
        else if (strncmp(key, "OPR", 3) == 0)
            vt_safe_string::safe_copy(op, STRLENGTH, value);
        else if (strncmp(key, "MRC", 3) == 0)
            vt_safe_string::safe_copy(merchid, STRLENGTH, value);
        else if (strncmp(key, "BTC", 3) == 0)
            vt_safe_string::safe_copy(batch, STRLENGTH, value);
        else if (strncmp(key, "DAT", 3) == 0)
            vt_safe_string::safe_copy(safdate, STRLENGTH, value);
        else if (strncmp(key, "TIM", 3) == 0)
            vt_safe_string::safe_copy(saftime, STRLENGTH, value);
        else if (strncmp(key, "DSP", 3) == 0)
            vt_safe_string::safe_copy(display, STRLENGTH, value);
        else if (strncmp(key, "SFN", 3) == 0)
            vt_safe_string::safe_copy(safnum, STRLENGTH, value);
        else if (strncmp(key, "NOR", 3) == 0)
            vt_safe_string::safe_copy(numrecords, STRLENGTH, value);
        else if (strncmp(key, "NEW", 3) == 0)
            vt_safe_string::safe_copy(notproc, STRLENGTH, value);
        else if (strncmp(key, "CMP", 3) == 0)
            vt_safe_string::safe_copy(completed, STRLENGTH, value);
        else if (strncmp(key, "DEC", 3) == 0)
            vt_safe_string::safe_copy(declined, STRLENGTH, value);
        else if (strncmp(key, "ERR", 3) == 0)
            vt_safe_string::safe_copy(errors, STRLENGTH, value);
        else if (strncmp(key, "VOI", 3) == 0)
            vt_safe_string::safe_copy(voided, STRLENGTH, value);
        else if (strncmp(key, "OLD", 3) == 0)
            vt_safe_string::safe_copy(expired, STRLENGTH, value);
        else if (strncmp(key, "LST", 3) == 0)
            vt_safe_string::safe_copy(last, STRLENGTH, value);
    }

    if (idx >= len)
        retval = 0;
    
    return retval;
}

void SAFClear::DebugPrint()
{
    FnTrace("SAFClear::DebugPrint()");

    printf("Debug Printout:\n");
    printf("\tTerminal:   %s\n", terminal);
    printf("\tBatch:      %s\n", batch);
    printf("\tOperator:   %s\n", op);
    printf("\tMerchant:   %s\n", merchid);
    printf("\tDate:       %s\n", safdate);
    printf("\tTime:       %s\n", saftime);
    printf("\tDisplay:    %s\n", display);
    printf("\tSAF Number: %s\n", safnum);
    printf("\tRecords:    %s\n", numrecords);
    printf("\tUnproced:   %s\n", notproc);
    printf("\tCompleted:  %s\n", completed);
    printf("\tDeclined:   %s\n", declined);
    printf("\tErrors:     %s\n", errors);
    printf("\tVoided:     %s\n", voided);
    printf("\tExpired:    %s\n", expired);
    printf("\tLast:       %s\n", last);
}


/*********************************************************************
 * CCInfo Class -- for use by the BatchInfo class.  Stores:
 *   o  number of transactions from the host
 *   o  amount from the host
 *   o  number of transactions from TR
 *   o  amount from TR
 ********************************************************************/

class CCInfo
{
public:
    char name[STRLENGTH];
    int  numhost;
    int  amthost;
    int  numtr;
    int  amttr;

    CCInfo();
    CCInfo(const char* newname);
    void SetName(const char* newname);
    void Clear();
    int  Write();
    void DebugPrint();
};

CCInfo::CCInfo()
{
    FnTrace("CCInfo::CCInfo()");

    Clear();
}

CCInfo::CCInfo(const char* newname)
{
    FnTrace("CCInfo::CCInfo(const char* )");

    vt_safe_string::safe_copy(name, STRLENGTH, newname);
    Clear();
}

void CCInfo::SetName(const char* newname)
{
    FnTrace("CCInfo::SetName()");

    vt_safe_string::safe_copy(name, STRLENGTH, newname);
}

void CCInfo::Clear()
{
    FnTrace("CCInfo::Clear()");
    numhost = 0;
    amthost = 0;
    numtr = 0;
    amttr = 0;
}

int CCInfo::Write()
{
    FnTrace("CCInfo::Write()");
    int retval = 0;

    WStr(name);
    WInt8(numhost);
    WInt32(amthost);
    WInt8(numtr);
    WInt32(amttr);

    return retval;
}

void CCInfo::DebugPrint()
{
    FnTrace("CCInfo::DebugPrint()");

    printf("\t%-20s", name);
    printf("\t\t%d\t%d\t%d\t%d\n", numhost, amthost, numtr, amttr);
}


/*********************************************************************
 * BatchInfo Class -- for Batch Settle
 ********************************************************************/

class BatchInfo
{
    char result[STRLENGTH];
    char settle[STRLENGTH];
    char termid[STRLENGTH];
    char op[STRLENGTH];
    char merchid[STRLENGTH];
    char seqnum[STRLENGTH];
    char shift[STRLENGTH];
    char batch[STRLENGTH];
    char bdate[STRLENGTH];
    char btime[STRLENGTH];
    char receipt[STRLENGTH];
    char display[STRLENGTH];
    char iso[STRLENGTH];
    char b24[STRLENGTH];

    CCInfo visa;
    CCInfo mastercard;
    CCInfo amex;
    CCInfo diners;
    CCInfo debit;
    CCInfo discover;
    CCInfo jcb;
    CCInfo purchase;
    CCInfo refund;
    CCInfo voids;

    int GetNum(const char* value);
    int GetAmt(const char* dest, const char* value);

public:
    BatchInfo();
    void Clear();
    int ParseResults(const char* results);
    int Write();
    void DebugPrint();
};

BatchInfo::BatchInfo()
{
    FnTrace("BatchInfo::BatchInfo()");

    Clear();
}

void BatchInfo::Clear()
{
    FnTrace("BatchInfo::Clear()");

    result[0]  = '\0';
    settle[0]  = '\0';
    termid[0]  = '\0';
    op[0]      = '\0';
    merchid[0] = '\0';
    seqnum[0]  = '\0';
    shift[0]   = '\0';
    batch[0]   = '\0';
    bdate[0]   = '\0';
    btime[0]   = '\0';
    receipt[0] = '\0';
    display[0] = '\0';
    iso[0]     = '\0';
    b24[0]     = '\0';

    visa.SetName(GlobalTranslate("Visa"));
    mastercard.SetName(GlobalTranslate("MasterCard"));
    amex.SetName(GlobalTranslate("American Express"));
    diners.SetName(GlobalTranslate("Diners"));
    debit.SetName(GlobalTranslate("Debit"));
    discover.SetName(GlobalTranslate("Discover"));
    jcb.SetName(GlobalTranslate("JCB"));
    purchase.SetName(GlobalTranslate("Purchase"));
    refund.SetName(GlobalTranslate("Refund"));
    voids.SetName(GlobalTranslate("Corrections"));
}

int BatchInfo::ParseResults(const char* results)
{
    FnTrace("BatchInfo::ParseResults()");
    int retval = 1;
    int idx = 0;
    char key[STRLENGTH];
    char value[STRLENGTH];
    int len = strlen(results);

    while (idx < len)
    {
        idx = CCQ_GetKeyValue(key, value, results, idx);
        if (strncmp(key, "RES", 3) == 0)
            vt_safe_string::safe_copy(result, STRLENGTH, value);
        else if (strncmp(key, "STL", 3) == 0)
            vt_safe_string::safe_copy(settle, STRLENGTH, value);
        else if (strncmp(key, "TRM", 3) == 0)
            vt_safe_string::safe_copy(termid, STRLENGTH, value);
        else if (strncmp(key, "OPR", 3) == 0)
            vt_safe_string::safe_copy(op, STRLENGTH, value);
        else if (strncmp(key, "MRC", 3) == 0)
            vt_safe_string::safe_copy(merchid, STRLENGTH, value);
        else if (strncmp(key, "SEQ", 3) == 0)
            vt_safe_string::safe_copy(seqnum, STRLENGTH, value);
        else if (strncmp(key, "SHF", 3) == 0)
            vt_safe_string::safe_copy(shift, STRLENGTH, value);
        else if (strncmp(key, "BTC", 3) == 0)
            vt_safe_string::safe_copy(batch, STRLENGTH, value);
        else if (strncmp(key, "DAT", 3) == 0)
            vt_safe_string::safe_copy(bdate, STRLENGTH, value);
        else if (strncmp(key, "TIM", 3) == 0)
            vt_safe_string::safe_copy(btime, STRLENGTH, value);
        else if (strncmp(key, "RCP", 3) == 0)
            vt_safe_string::safe_copy(receipt, STRLENGTH, value);
        else if (strncmp(key, "DSP", 3) == 0)
            vt_safe_string::safe_copy(display, STRLENGTH, value);
        else if (strncmp(key, "ISO", 3) == 0)
            vt_safe_string::safe_copy(iso, STRLENGTH, value);
        else if (strncmp(key, "B24", 3) == 0)
            vt_safe_string::safe_copy(b24, STRLENGTH, value);
        else if (strncmp(key, "C01OUR", 6) == 0)
            visa.numtr = GetNum(value);
        else if (strncmp(key, "A01OUR", 6) == 0)
            visa.amttr = GetNum(value);
        else if (strncmp(key, "C01", 3) == 0)
            visa.numhost = GetNum(value);
        else if (strncmp(key, "A01", 3) == 0)
            visa.amthost = GetNum(value);
        else if (strncmp(key, "C02OUR", 6) == 0)
            mastercard.numtr = GetNum(value);
        else if (strncmp(key, "A02OUR", 6) == 0)
            mastercard.amttr = GetNum(value);
        else if (strncmp(key, "C02", 3) == 0)
            mastercard.numhost = GetNum(value);
        else if (strncmp(key, "A02", 3) == 0)
            mastercard.amthost = GetNum(value);
        else if (strncmp(key, "C03OUR", 6) == 0)
            amex.numtr = GetNum(value);
        else if (strncmp(key, "A03OUR", 6) == 0)
            amex.amttr = GetNum(value);
        else if (strncmp(key, "C03", 3) == 0)
            amex.numhost = GetNum(value);
        else if (strncmp(key, "A03", 3) == 0)
            amex.amthost = GetNum(value);
        else if (strncmp(key, "C04OUR", 6) == 0)
            diners.numtr = GetNum(value);
        else if (strncmp(key, "A04OUR", 6) == 0)
            diners.amttr = GetNum(value);
        else if (strncmp(key, "C04", 3) == 0)
            diners.numhost = GetNum(value);
        else if (strncmp(key, "A04", 3) == 0)
            diners.amthost = GetNum(value);
        else if (strncmp(key, "C05OUR", 6) == 0)
            debit.numtr = GetNum(value);
        else if (strncmp(key, "A05OUR", 6) == 0)
            debit.amttr = GetNum(value);
        else if (strncmp(key, "C05", 3) == 0)
            debit.numhost = GetNum(value);
        else if (strncmp(key, "A05", 3) == 0)
            debit.amthost = GetNum(value);
        else if (strncmp(key, "C06OUR", 6) == 0)
            discover.numtr = GetNum(value);
        else if (strncmp(key, "A06OUR", 6) == 0)
            discover.amttr = GetNum(value);
        else if (strncmp(key, "C06", 3) == 0)
            discover.numhost = GetNum(value);
        else if (strncmp(key, "A06", 3) == 0)
            discover.amthost = GetNum(value);
        else if (strncmp(key, "C08OUR", 6) == 0)
            jcb.numtr = GetNum(value);
        else if (strncmp(key, "A08OUR", 6) == 0)
            jcb.amttr = GetNum(value);
        else if (strncmp(key, "C08", 3) == 0)
            jcb.numhost = GetNum(value);
        else if (strncmp(key, "A08", 3) == 0)
            jcb.amthost = GetNum(value);
        else if (strncmp(key, "C13OUR", 6) == 0)
            purchase.numtr = GetNum(value);
        else if (strncmp(key, "A13OUR", 6) == 0)
            purchase.amttr = GetNum(value);
        else if (strncmp(key, "C13", 3) == 0)
            purchase.numhost = GetNum(value);
        else if (strncmp(key, "A13", 3) == 0)
            purchase.amthost = GetNum(value);
        else if (strncmp(key, "C14OUR", 6) == 0)
            refund.numtr = GetNum(value);
        else if (strncmp(key, "A14OUR", 6) == 0)
            refund.amttr = GetNum(value);
        else if (strncmp(key, "C14", 3) == 0)
            refund.numhost = GetNum(value);
        else if (strncmp(key, "A14", 3) == 0)
            refund.amthost = GetNum(value);
        else if (strncmp(key, "C15OUR", 6) == 0)
            voids.numtr = GetNum(value);
        else if (strncmp(key, "A15OUR", 6) == 0)
            voids.amttr = GetNum(value);
        else if (strncmp(key, "C15", 3) == 0)
            voids.numhost = GetNum(value);
        else if (strncmp(key, "A15", 3) == 0)
            voids.amthost = GetNum(value);
    }

    if (idx >= len)
        retval = 0;

    return retval;
}

int BatchInfo::Write()
{
    FnTrace("BatchInfo::Write()");
    int retval = 0;

    WStr(result);
    WStr(settle);
    WStr(termid);
    WStr(op);
    WStr(merchid);
    WStr(seqnum);
    WStr(shift);
    WStr(batch);
    WStr(bdate);
    WStr(btime);
    WStr(receipt);
    WStr(display);
    WStr(iso);
    WStr(b24);   
    
    visa.Write();
    mastercard.Write();
    amex.Write();
    diners.Write();
    debit.Write();
    discover.Write();
    jcb.Write();
    purchase.Write();
    refund.Write();
    voids.Write();

    return retval;
}

void BatchInfo::DebugPrint()
{
    FnTrace("BatchInfo::DebugPrint()");

    printf("Batch Results:\n");
    printf("\tResult:  %s\n", result);
    printf("\tSettle:  %s\n", settle);
    printf("\tTermID:  %s\n", termid);
    printf("\tOP:  %s\n", op);
    printf("\tMerchant ID:  %s\n", merchid);
    printf("\tSeq Number:  %s\n", seqnum);
    printf("\tBatch:  %s\n", batch);
    printf("\tShift:  %s\n", shift);
    printf("\tDate/Time:  %s %s\n", bdate, btime);
    printf("\tReceipt:  %s\n", receipt);
    printf("\tDisplay:  %s\n", display);
    printf("\tISO:  %s\n", iso);
    printf("\tB24:  %s\n", b24);

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

int BatchInfo::GetNum(const char* value)
{
    FnTrace("BatchInfo::GetNum()");
    int  retval = 0;
    char dest[STRLENGTH];
    int  idx = 0;

    while (std::isdigit(static_cast<unsigned char>(value[idx])))
    {
        dest[idx] = value[idx];
        idx += 1;
    }
    dest[idx] = '\0';

    retval = atoi(dest);

    return retval;
}

int BatchInfo::GetAmt(const char* dest, const char* value)
{
    FnTrace("BatchInfo::GetAmt()");
    int retval = 0;
    char temp_buffer[STRLENGTH];

    if (vt_safe_string::safe_format(temp_buffer, STRLENGTH, "%d", GetNum(value)))
    {
        strncpy(const_cast<char*>(dest), temp_buffer, STRLENGTH - 1);
        const_cast<char*>(dest)[STRLENGTH - 1] = '\0';
    }
    else
    {
        retval = 1;
    }

    return retval;
}


/*********************************************************************
 * CCard Class
 ********************************************************************/
CCard::CCard()
{
    FnTrace("CCard::CCard()");
    ipconn = -1;
    trans_success = 0;
    intcode = CC_STATUS_NONE;

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
    isocode[0] = '\0';
    b24code[0] = '\0';
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
    reference[0] = '0';
    sequence[0] = '0';
    server_date[0] = '0';
    server_time[0] = '0';
    receipt_line[0] = '0';
    display_line[0] = '0';
    manual = 0;
}

CCard::CCard(const char* serv, const char* prt, const char* id)
{
    FnTrace("CCard::CCard(const char* , const char* , const char* )");
    ipconn = -1;
    trans_success = 0;
    intcode = CC_STATUS_NONE;

    vt_safe_string::safe_copy(server, STRLENGTH, serv);
    vt_safe_string::safe_copy(port, STRLENGTH, prt);
    user[0] = '\0';
    password[0] = '\0';
    vt_safe_string::safe_copy(termid, STRLENGTH, id);
    approval[0] = '\0';
    number[0] = '\0';
    expire[0] = '\0';
    name[0] = '\0';
    country[0] = '\0';
    debit_acct = DEBIT_ACCT_NONE;
    code[0] = '\0';
    intcode = 0;
    isocode[0] = '\0';
    b24code[0] = '\0';
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
    reference[0] = '0';
    sequence[0] = '0';
    server_date[0] = '0';
    server_time[0] = '0';
    receipt_line[0] = '0';
    display_line[0] = '0';
    manual = 0;
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
    WStr(isocode);
    WStr(b24code);
    WInt8(manual);
    WStr(verb);
    WStr(auth);
    WLLong(batch);
    WLLong(item);
    WLLong(ttid);
    WStr(AVS);
    WStr(CV);
    WInt8(trans_success);

    // specific to CreditCheq
    WStr(termid);
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
    FnTrace("CCard::Clear()");
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
    isocode[0] = '\0';
    b24code[0] = '\0';
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
    reference[0] = '0';
    sequence[0] = '0';
    server_date[0] = '0';
    server_time[0] = '0';
    receipt_line[0] = '0';
    display_line[0] = '0';

    return 0;
}

int CCard::ParseResponse(const char* response)
{
    FnTrace("CCard::ParseResponse()");
    int retval = 1;
    int idx = 0;
    char unused[STRLONG];  // for discarding unimportant sections

    if (response[0] == 0x02 && strncmp(&response[1], "ANS", 3) == 0)
    {
        idx += 6;  // STX, 'ANS', STX, Space
        idx = CCQ_GetStringCount(AVS, response, idx, 1);
        idx = CCQ_GetStringCount(code, response, idx, 40);
        idx = CCQ_GetStringCount(isocode, response, idx, 2);
        idx = CCQ_GetStringCount(unused, response, idx, 40);  // *UNUSED* according to Tender Retail
        
        idx = CCQ_GetStringDelim(unused, response, idx, 0x1C, 0x1C);  // throw FS RCP FS away
        idx = CCQ_GetStringDelim(receipt, response, idx, 0x1C, 0x1C);
        idx = CCQ_GetStringDelim(unused, response, idx, 0x1C, 0x1C);  // throw FS RCT FS away
        idx = CCQ_GetStringDelim(formatted, response, idx, 0x1C, 0x1C);

        ParseReceipt();

        if (AVS[0] == 'A')
            intcode = CC_STATUS_AUTH;
        else if (AVS[0] == 'F')
            intcode = CC_STATUS_AUTH;
        else if (AVS[0] == 'I')
            intcode = CC_STATUS_AUTH;
        else if (AVS[0] == 'D')
            intcode = CC_STATUS_DENY;
        else if (AVS[0] == 'E')
            intcode = CC_STATUS_ERROR;
        else if (AVS[0] == 'Y')
            intcode = CC_STATUS_RETRY;
        else if (AVS[0] == 'Z')
            intcode = CC_STATUS_RETRY;
    }
    else
    {
        intcode = CC_STATUS_ERROR;
        vt_safe_string::safe_copy(verb, STRLENGTH, &response[1]);
    }

    return retval;
}

int CCard::ParseReceipt()
{
    FnTrace("CCard::ParseReceipt()");
    char key[STRLENGTH];
    char value[STRLENGTH];
    int idx = 0;
    int len = strlen(receipt);

    while (idx < len)
    {
        idx = CCQ_GetKeyValue(key, value, receipt, idx);
        if (strncmp(key, "CRN", 3) == 0 && number[0] == 0)
            vt_safe_string::safe_copy(number, STRLENGTH, value);
        else if (strncmp(key, "EXP", 3) == 0 && expire[0] == 0)
            vt_safe_string::safe_copy(expire, STRLENGTH, value);
        else if (strncmp(key, "AUT", 3) == 0)
            vt_safe_string::safe_copy(auth, STRLENGTH, value);
        else if (strncmp(key, "REF", 3) == 0)
            vt_safe_string::safe_copy(reference, STRLENGTH, value);
        else if (strncmp(key, "SEQ", 3) == 0)
            vt_safe_string::safe_copy(sequence, STRLENGTH, value);
        else if (strncmp(key, "DAT", 3)  == 0)
            vt_safe_string::safe_copy(server_date, STRLENGTH, value);
        else if (strncmp(key, "TIM", 3) == 0)
            vt_safe_string::safe_copy(server_time, STRLENGTH, value);
        else if (strncmp(key, "RCP", 3) == 0)
            vt_safe_string::safe_copy(receipt_line, STRLENGTH, value);
        else if (strncmp(key, "DSP", 3) == 0)
            vt_safe_string::safe_copy(verb, STRLENGTH, value);
        else if (strncmp(key, "LNG", 3) == 0)
            vt_safe_string::safe_copy(country, STRLENGTH, value);
        else if (strncmp(key, "TRM", 3) == 0)
            vt_safe_string::safe_copy(termid, STRLENGTH, value);
        else if (strncmp(key, "B24", 3) == 0)
            vt_safe_string::safe_copy(b24code, STRLENGTH, value);
        else if (strncmp(key, "DBA", 3) == 0)
            debit_acct = CCQ_GetDebitAccount(value);
        else if (strncmp(key, "SWP", 3) == 0)
            manual = CCQ_GetSwiped(value);
    }

    return 1;
}

int CCard::Command(const char* trans_type, const char* sub_type)
{
    FnTrace("CCard::Command()");
    int retval = 1;
    char buffer[STRHUGE];
    char outbuff[STRLENGTH];
    int writedone = 0;
    int writelen;
    int done = 0;

    while (done == 0)
    {
        done = 1;
        if (SendCheq(trans_type, sub_type) > 0)
        {
            if (ReadCheq(buffer, STRHUGE) == 0)
            {
                snprintf(outbuff, STRLENGTH, "%c", 0x06);
                while (writedone == 0)
                {
                    writelen = write(ipconn, outbuff, 1);
                    if (writelen >= 0 || errno != EAGAIN)
                        writedone = 1;
                }
                ParseResponse(buffer);
            }
            retval = 0;
        }
        else
        {
            vt_safe_string::safe_format(code, STRLENGTH, "NOCONN");
            intcode = CC_STATUS_NOCONNECT;
        }
    }

    return retval;
}

/****
 * SendCheq:  Return value of system call write().  AKA:
 *   -1 on error
 *   number of bytes read otherwise
 ****/
int CCard::SendCheq(const char* trans_type, const char* sub_type)
{
    FnTrace("CCard::SendCheq()");
    int retval = -1;
    char authstring[STRLENGTH] = "";
    char tmpstring[STRLENGTH];
    int len = 0;

    if (Connect() == 0 && ipconn > 0)
    {
        len += CCQ_AddString(authstring, trans_type, 2);   // transaction type
        len += CCQ_AddString(authstring, sub_type, 1);     // sub type
        if (card_type == CARD_TYPE_DEBIT)
        {
            len += CCQ_AddString(authstring, "", 40);      // card number
            len += CCQ_AddString(authstring, "", 4);       // expiration date
        }
        else
        {
            len += CCQ_AddString(authstring, number, 40);      // card number
            len += CCQ_AddString(authstring, expire, 4);       // expiration date
        }
        if (trans_type[0] == 'T')
            tmpstring[0] = '\0';
        else
            snprintf(tmpstring, STRLENGTH, "%.2f", (fullamount / 100.0));
        len += CCQ_AddString(authstring, tmpstring, 10);         // amount, with decimal point
        if (trans_type[0] == 'T')
            reference[0] = '\0';
        len += CCQ_AddString(authstring, reference, 12);   // transaction number
        len += CCQ_AddString(authstring, termid, 12);      // terminal id
        len += CCQ_AddString(authstring, auth, 10);        // authorization number

        retval = write(ipconn, authstring, strlen(authstring));
    }

    return retval;
}

/****
 * SendSAF:  Return value of system call write().  AKA:
 *   -1 on error
 *   number of bytes read otherwise
 ****/
int CCard::SendSAF(const char* trans_type, const char* sub_type)
{
    FnTrace("CCard::SendSAF()");
    int retval = -1;
    char authstring[STRLENGTH] = "";
    int len = 0;

    if (Connect() == 0 && ipconn > 0)
    {
        memset(authstring, 0x00, STRLENGTH);
        // most of these fields are RESERVED
        len += CCQ_AddString(authstring, trans_type, 2);   // transaction type
        len += CCQ_AddString(authstring, sub_type, 1);     // sub type
        len += CCQ_AddString(authstring, "", 40);      // card number
        len += CCQ_AddString(authstring, "", 4);
        len += CCQ_AddString(authstring, "", 10);
        len += CCQ_AddString(authstring, "", 12);
        len += CCQ_AddString(authstring, termid, 12);
        len += CCQ_AddString(authstring, "", 10);
        len += CCQ_AddString(authstring, "", 3);
        len += CCQ_AddString(authstring, "", 30);
        len += CCQ_AddString(authstring, "", 30);
        len += CCQ_AddString(authstring, "", 30);
        len += CCQ_AddString(authstring, "", 1);

        retval = write(ipconn, authstring, strlen(authstring));
    }

    return retval;
}

/****
 * ReadCheq:  Read what the server (Multi.exe or mlt_serv) has for us.
 ****/
int CCard::ReadCheq(const char* buffer_param, int buffsize)
{
    FnTrace("CCard::ReadCheq()");
    int retval = 1;
    int readlen;
    int done = 0;
    int idx = 0;
    int nfds = 0;
    fd_set readfd;
    struct timeval timeout;
    int selresult;
    int counter = 0;
    char* buffer = const_cast<char*>(buffer_param);  // Need non-const for read() and assignment

    if (ipconn > 0)
    {
        // Do a select to find out if we have something to read.  If
        // we've already read something, we'll only try five times and
        // give up (almost certainly nothing else to read at that
        // point).
        while (!done)
        {
            nfds = ipconn + 1;
            FD_ZERO(&readfd);
            FD_SET(ipconn, &readfd);
            timeout.tv_sec = 0;
            timeout.tv_usec = 50;
            selresult = select(nfds, &readfd, nullptr, nullptr, &timeout);
            if (selresult > 0)
            {
                counter = 0;
                readlen = static_cast<int>(read(ipconn, &buffer[idx], static_cast<size_t>(buffsize - idx)));
                if (readlen >= 0)
                {
                    if (readlen > 0)
                    {
                        // Don't read "<ACK>Wait, request sent".
                        if (buffer[0] != 0x06 && strncmp(&buffer[1], "Wait", 4) != 0)
                        {
                            idx += readlen;
                            retval = 0;
                        }
                    }
                    else
                    {
                        vt_safe_string::safe_copy(verb, STRLENGTH, "Failed to get response");
                        done = 1;
                    }
                }
                else
                {
                    perror("ReadCheq read");
                    done = 1;
                }
            }
            else if (selresult < 0)
                perror("ReadCheq select");
            else if (idx > 0 && counter > 5)
                done = 1;
            else
                counter += 1;
        }
    }
    buffer[idx] = '\0';

    return retval;
}

int CCard::Connect()
{
    FnTrace("CCard::Connect()");
    int retval = 1;
    int sockfd;
    struct sockaddr_in servaddr;
    struct sockaddr *addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd > 0)
    {
        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port   = htons(atoi(port));
        inet_pton(AF_INET, server, &servaddr.sin_addr);
        addr = reinterpret_cast<struct sockaddr*>(&servaddr);  // sockaddr_in* to sockaddr* requires reinterpret_cast
        if (my_connect(sockfd, addr, sizeof(servaddr), ConnectionTimeOut) == 0)
        {
            ipconn = sockfd;
            retval = 0;
        }
        else
        {
            perror("Connect");
            vt_safe_string::safe_copy(verb, STRLENGTH, "No Connection");
            intcode = CC_STATUS_NOCONNECT;
        }
    }

    return retval;
}

int CCard::Close()
{
    FnTrace("CCard::Close()");
    int retval = 1;

    if (ipconn > 0)
    {
        close(ipconn);
        retval = 0;
    }

    return retval;
}

int CCard::Sale()
{
    FnTrace("CCard::Sale()");
    int retval = 1;

    if (card_type == CARD_TYPE_DEBIT)
        retval = Command("01", "1");
    else
        retval = Command("00", "1");

    return retval;
}

int CCard::PreAuth()
{
    FnTrace("CCard::PreAuth()");
    int retval = 1;

    if (card_type == CARD_TYPE_DEBIT)
        retval = Command("01", "1");
    else
        retval = Command("10", "1");

    return retval;
}

int CCard::FinishAuth()
{
    FnTrace("CCard::FinishAuth()");
    int retval = 1;

    if (card_type != CARD_TYPE_DEBIT)
        retval = Command("60", "1");

    return retval;
}

int CCard::Void()
{
    FnTrace("CCard::Void()");
    int retval = 1;

    if (card_type == CARD_TYPE_DEBIT)
        retval = Command("31", "1");
    else
        retval = Command("30", "1");

    return retval;
}

/****
 * VoidCancel:  Not supported.  Just re-run the transaction.
 *  BAK->In truth, it was a goof, but I'm not going to remove the
 *  code because it may be required later.  So consider it a stub
 *  for now and don't use it unless you finish it.
 ****/
int CCard::VoidCancel()
{
    FnTrace("CCard::VoidCancel()");
    int retval = 1;

    return retval;
}

int CCard::Refund()
{
    FnTrace("CCard::Refund()");
    int retval = 1;

    if (card_type == CARD_TYPE_DEBIT)
        retval = Command("41", "1");
    else
        retval = Command("40", "1");

    return retval;
}

int CCard::RefundCancel()
{
    FnTrace("CCard::RefundCancel()");
    int retval = 1;

    if (card_type == CARD_TYPE_DEBIT)
        retval = Command("51", "1");
    else
        retval = Command("50", "1");

    return retval;
}

int CCard::BatchSettle()
{
    FnTrace("CCard::BatchSettle()");
    int retval = 1;
    char buffer[STRHUGE];
    BatchInfo binfo;

    RStr(server);
    RStr(port);
    RStr(termid);

    if (SendCheq("TS", "1") > 0)
    {
        if (ReadCheq(buffer, STRHUGE) == 0)
        {
            ParseResponse(buffer);
            binfo.ParseResults(receipt);
            WInt8(SERVER_CC_SETTLED);
            binfo.Write();
            retval = 0;
        }
    }

    if (retval)
        WInt8(SERVER_CC_SETTLEFAILED);
    SendNow();

    return retval;
}

int CCard::CCInit()
{
    FnTrace("CCard::CCInit()");
    int retval = 0;
    char buffer[STRHUGE];

    RStr(server);
    RStr(port);
    RStr(termid);

    if (SendCheq("TI", "1") > 0)
    {
        if (ReadCheq(buffer, STRHUGE) == 0)
        {
            ParseResponse(buffer);
            WInt8(SERVER_CC_INIT);
            WStr(termid);
            WStr(code);
            WInt8(intcode);
            SendNow();
        }
    }
    

    return retval;
}

int CCard::Totals()
{
    FnTrace("CCard::Totals()");
    int retval = 0;
    char buffer[STRHUGE];
    BatchInfo binfo;

    RStr(server);
    RStr(port);
    RStr(termid);

    if (SendCheq("TT", "1") > 0)
    {
        if (ReadCheq(buffer, STRHUGE) == 0)
        {
            ParseResponse(buffer);
            binfo.ParseResults(receipt);
            WInt8(SERVER_CC_TOTALS);
            binfo.Write();
            SendNow();
        }
    }

    return retval;
}

int CCard::Details()
{
    FnTrace("CCard::Details()");
    int retval = 0;
    char buffer[STRHUGE];

    RStr(server);
    RStr(port);
    RStr(termid);

    if (SendCheq("TD", "1") > 0)
    {
        if (ReadCheq(buffer, STRHUGE) == 0)
        {
            ParseResponse(buffer);
            WInt8(SERVER_CC_DETAILS);
            WStr(termid);
            WStr(code);
            WInt8(intcode);
            SendNow();
        }
    }

    return retval;
}

int CCard::ClearSAF()
{
    FnTrace("CCard::ClearSAF()");
    int retval = 1;
    char buffer[STRHUGE];
    SAFClear safclear;

    RStr(server);
    RStr(port);
    RStr(termid);

    if (SendSAF("SC", "1") > 0)
    {
        if (ReadCheq(buffer, STRHUGE) == 0)
        {
            if (safclear.ParseSAF(buffer) == 0)
            {
                WInt8(SERVER_CC_SAFCLEARED);
                safclear.Write();
                retval = 0;
            }
        }
    }

    if (retval)
        WInt8(SERVER_CC_SAFCLEARFAILED);

    SendNow();

    return retval;
}

int CCard::SAFDetails()
{
    FnTrace("CCard::SAFDetails()");
    int retval = 0;
    char buffer[STRHUGE];
    SAFClear safclear;

    RStr(server);
    RStr(port);
    RStr(termid);

    if (SendSAF("SH", "1") > 0)
    {
        if (ReadCheq(buffer, STRHUGE) == 0)
        {
            if (safclear.ParseSAF(buffer) == 0)
            {
                WInt8(SERVER_CC_SAFDETAILS);
                safclear.Write();
                SendNow();
            }
        }
    }

    return retval;
}


/*********************************************************************
 * Functions
 ********************************************************************/

/****
 * CCQ_AddString:  
 ****/
int CCQ_AddString(char* dest, const char* src, int length, char pad)
{
    FnTrace("CCQ_AddString()");
    int retval = 0;
    int sidx = 0;
    int didx = strlen(dest);

    while (src[sidx] != '\0' && sidx < length)
    {
        dest[didx] = src[sidx];
        didx += 1;
        sidx += 1;
    }
    while (sidx < length)
    {
        dest[didx] = pad;
        didx += 1;
        sidx += 1;
    }
    dest[didx] = '\0';
    retval = sidx;

    return retval;
}

int CCQ_GetStringDelim(char* dest, const char* src, int start, char edelim, char sdelim)
{
    FnTrace("CCQ_GetStringDelim()");
    int didx = 0;
    int sidx = start;
    int slen = strlen(src);

    if (sdelim > 0)
    {
        while (src[sidx] != sdelim && src[sidx] != '\0' && sidx < slen)
        {
            sidx += 1;
        }
        sidx += 1;
    }

    while (src[sidx] != edelim && src[sidx] != '\0' && sidx < slen)
    {
        dest[didx] = src[sidx];
        didx += 1;
        sidx += 1;
    }
    dest[didx] = '\0';

    return sidx;
}

int CCQ_GetStringCount(char* dest, const char* src, int start, int count)
{
    FnTrace("CCQ_GetStringCount()");
    int didx = 0;
    int sidx = start;
    int slen = strlen(src);

    while (sidx > -1 && didx < count && src[sidx] != '\0' && sidx < slen)
    {
        dest[didx] = src[sidx];
        didx += 1;
        sidx += 1;
    }
    dest[didx] = '\0';

    return sidx;
}

int CCQ_GetKeyValue(char* destkey, char* destval, const char* src, int start)
{
    FnTrace("CCQ_GetKeyValue()");
    int didx = 0;
    int sidx = start;

    // get the key (to the first colon)
    while (src[sidx] != ':' && src[sidx] != '\0')
    {
        destkey[didx] = src[sidx];
        didx += 1;
        sidx += 1;
    }
    destkey[didx] = '\0';

    // skip past colon and whitespace
    if (src[sidx] != '\0')
    {
        sidx += 1;
        while (src[sidx] == ' ' && src[sidx] != '\0')
            sidx += 1;
    }

    // gather the value until the newline
    if (src[sidx] != '\0')
    {
        didx = 0;
        while (src[sidx] != '\r' && src[sidx] != '\n' && src[sidx] != '\0')
        {
            destval[didx] = src[sidx];
            didx += 1;
            sidx += 1;
        }
        destval[didx] = '\0';
        while (src[sidx] == '\r' || src[sidx] == '\n')
            sidx += 1;
    }

    return sidx;
}

int CCQ_GetDebitAccount(const char* account_string)
{
    FnTrace("CCQ_GetDebitAcct()");
    int retval = DEBIT_ACCT_NONE;

    if (strncmp(account_string, "SAVING", 6) == 0)
        retval = DEBIT_ACCT_SAVINGS;
    else if (strncmp(account_string, "CHECKING", 8) == 0)
        retval = DEBIT_ACCT_CHECKING;
    else if (strncmp(account_string, "CHEQUING", 8) == 0)
        retval = DEBIT_ACCT_CHECKING;

    return retval;
}

int CCQ_GetSwiped(const char* swiped_string)
{
    FnTrace("CCQ_GetSwiped()");
    int retval = NUMBER_MANUAL;

    if (strncmp(swiped_string, "MANUAL", 6) == 0)
        retval = NUMBER_MANUAL;
    else if (strncmp(swiped_string, "SWIPED", 6) == 0)
        retval = NUMBER_SWIPED;

    return retval;
}
