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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "credit.hh"
#include "remote_link.hh"
#include "term_credit_mcve.hh"
#include "term_view.hh"
#include "utility.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/***********************************************************************
 * GLOBAL VARIABLES
 **********************************************************************/

// We'll have arrays to keep track of the proper field indices for
// the different reports we want.  Monetra changes the column order, but
// never the column headers.  So we have an initialization method that
// will process through the header names and look for the columns we
// want, initializing these arrays appropriately.  When that's done,
// we want to indicate that on future passes it doesn't need to be done
// to save time.
// The Unsettled fields are:
//   ttid, type, card, acct, expdate, amount, auth, batch, item
int GUTFIELDS[15];
#define GUT_TTID      0
#define GUT_TYPE      1
#define GUT_CARD      2
#define GUT_ACCOUNT   3
#define GUT_EXPDATE   4
#define GUT_AMOUNT    5
#define GUT_AUTHNUM   6
#define GUT_BATCH     7
#define GUT_ITEM      8
#define GUT_TIMESTAMP 9

// The Settled fields are:
//   ttid, type, card, acct, expdate, amount, auth, batch, timestamp, comments
// The last two are for settlement lines only
int GLFIELDS[15];
#define GL_TTID      0
#define GL_TYPE      1
#define GL_CARD      2
#define GL_ACCOUNT   3
#define GL_EXPDATE   4
#define GL_AMOUNT    5
#define GL_AUTHNUM   6
#define GL_BATCH     7
#define GL_TIMESTAMP 8
#define GL_COMMENTS  9

int report_fields_set = 0;


int AppendString(char* dest, int fwidth, const char* source)
{
    FnTrace("AppendString()");
    int retval = 0;
    char buffer[STRLONG];

    snprintf(buffer, STRLONG, "%-*s", fwidth, source);
    strncat(dest, buffer, sizeof(dest) - strlen(dest) - 1);

    return retval;
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

    strcpy(name, newname);
    Clear();
}

void CCInfo::SetName(const char* newname)
{
    FnTrace("CCInfo::SetName()");

    strcpy(name, newname);
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
    int GetAmt(char* dest, const char* value);

public:
    BatchInfo();
    void Clear();
    int ParseResults(MCVE_CONN *conn, long identifier);
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

    visa.SetName("Visa");
    mastercard.SetName("MasterCard");
    amex.SetName("American Express");
    diners.SetName("Diners");
    debit.SetName("Debit");
    discover.SetName("Discover");
    jcb.SetName("JCB");
    purchase.SetName("Purchase");
    refund.SetName("Refund");
    voids.SetName("Corrections");
}

int BatchInfo::ParseResults(MCVE_CONN *conn, long id)
{
    FnTrace("BatchInfo::ParseResults()");
    int   retval = 1;
    int   row;
    int   rows;
    int   columns;
    int   is_sale = 0;
    const char* trans;
    const char* card;
    int   amount;

    if (MCVE_ParseCommaDelimited(conn, id))
    {
        rows = MCVE_NumRows(conn, id);
        columns = MCVE_NumColumns(conn, id);

        for (row = 0; row < rows; row++)
        {
            is_sale = 0;
            trans = MCVE_GetCellByNum(conn, id, 1, row);
            card = MCVE_GetCellByNum(conn, id, 2, row);
            amount = GetNum(MCVE_GetCellByNum(conn, id, 6, row));

            if (strcmp("SALE", trans) == 0)
            {
                is_sale = 1;
                purchase.numhost += 1;
                purchase.amthost += amount;
            }
            else if (strcmp("RETURN", trans) == 0)
            {
                refund.numhost += 1;
                refund.amthost += amount;
            }
            else if (strcmp("SETTLE", trans) == 0)
            {
                GetAmt(batch, MCVE_GetCellByNum(conn, id, 9, row));
            }

            if (strcmp("VISA", card) == 0)
            {
                if (is_sale)
                {
                    visa.numhost += 1;
                    visa.amthost += amount;
                }
                else
                {
                    visa.numhost += 1;
                    visa.amthost -= amount;
                }
            }
            else if (strcmp("MC", card) == 0)
            {
                if (is_sale)
                {
                    mastercard.numhost += 1;
                    mastercard.amthost += amount;
                }
                else
                {
                    mastercard.numhost += 1;
                    mastercard.amthost -= amount;
                }
            }
            else if (strcmp("AMEX", card) == 0)
            {
                if (is_sale)
                {
                    amex.numhost += 1;
                    amex.amthost += amount;
                }
                else
                {
                    amex.numhost += 1;
                    amex.amthost -= amount;
                }
            }
        }
    }

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
    int  didx = 0;
    int  vidx = 0;

    while (value[vidx] != '\0')
    {
        if (ISDIGIT(value[vidx]))
        {
            dest[didx] = value[vidx];
            didx += 1;
        }
        vidx += 1;
    }
    dest[didx] = '\0';

    retval = atoi(dest);

    return retval;
}

int BatchInfo::GetAmt(char* dest, const char* value)
{
    FnTrace("BatchInfo::GetAmt()");
    int retval = 0;

    snprintf(dest, sizeof(dest), "%d", GetNum(value));

    return retval;
}


/*********************************************************************
 * CCard Class
 ********************************************************************/
CCard::CCard()
{
    conn = NULL;
    trans_success = 0;
    intcode = CC_STATUS_NONE;

    Clear();
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
    RStr(swipe);
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
    Close();

    server[0] = '\0';
    port[0] = '\0';
    user[0] = '\0';
    password[0] = '\0';
    termid[0] = '\0';
    approval[0] = '\0';
    swipe[0] = '\0';
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
    reference[0] = '0';
    sequence[0] = '0';
    server_date[0] = '0';
    server_time[0] = '0';
    receipt_line[0] = '0';
    display_line[0] = '0';

    return 0;
}

int CCard::Connect()
{
    FnTrace("CCard::Connect()");
    int retval = 1;

    if (server[0] != '\0' && port[0] != '\0')
    {
        if (conn == NULL)
            conn = new MCVE_CONN;
        MCVE_InitEngine(NULL);
        MCVE_InitConn(conn);
        if (MCVE_SetIP(conn, server, atoi(port)))
        {
            MCVE_SetBlocking(conn, 1);
            MCVE_SetTimeout(conn, 30);
            if (MCVE_Connect(conn))
            {
                retval = 0;
            }
            else
            {
                strcpy(verb, "No Connection");
                intcode = CC_STATUS_NOCONNECT;
                MCVE_DestroyConn(conn);
                MCVE_DestroyEngine();
                conn = NULL;
            }
        }
    }

    return retval;
}

int CCard::Close()
{
    FnTrace("CCard::Close()");
    int retval = 1;

    if (conn != NULL)
    {
        strcpy(code, "NOCONN");
        intcode = CC_STATUS_NOCONNECT;
        MCVE_DestroyConn(conn);
        MCVE_DestroyEngine();
        conn = NULL;
        retval = 0;
    }

    return retval;
}

int CCard::SetValue(char* dest, const char* source)
{
    FnTrace("CCard::SetValue()");
    int retval = 0;

    dest[0] = '\0';
    if (source != NULL)
        strcpy(dest, source);

    return retval;
}

int CCard::TransSend(long identifier)
{
    FnTrace("CCard::TransSend()");
    int retval = 1;
    char buffer[STRLONG];

    TransSendSimple(identifier);
    intcode = MCVE_ReturnCode(conn, identifier);
    SetValue(buffer, MCVE_TEXT_Code(intcode));
    if (strlen(buffer) > 0)
        strcpy(code, buffer);
    SetValue(buffer, MCVE_TransactionText(conn, identifier));
    if (strlen(buffer) > 0)
        strcpy(verb, buffer);
    if (intcode == MCVE_SUCCESS || intcode == MCVE_AUTH)
    {
        SetValue(auth, MCVE_TransactionAuth(conn, identifier));
        batch = MCVE_TransactionBatch(conn, identifier);
        item  = MCVE_TransactionItem(conn, identifier);
        ttid  = MCVE_TransactionID(conn, identifier);
        SetValue(AVS, MCVE_TEXT_AVS(MCVE_TransactionAVS(conn, identifier)));
        SetValue(CV, MCVE_TEXT_CV(MCVE_TransactionCV(conn, identifier)));
        trans_success = 1;
        retval = 0;
    }
    
    return retval;
}

int CCard::TransSendSimple(long identifier)
{
    FnTrace("CCard::TransSendSimple()");
    int retval = 1;
    if (MCVE_TransSend(conn, identifier))
    {
        while (MCVE_CheckStatus(conn, identifier) != MCVE_DONE)
        {
            MCVE_Monitor(conn);
            MCVE_uwait(10000);
        }
        if (MCVE_ReturnStatus(conn, identifier) == MCVE_SUCCESS)
            retval = 0;
        else if (MCVE_ReturnStatus(conn, identifier) == MCVE_FAIL)
            ReportError("M transaction failed");
    }

    return retval;
}

int CCard::SetFields(int gut, long identifier)
{
    FnTrace("CCard::SetFields()");
    int retval = 0;
    char header[STRLENGTH];
    int columns = 0;
    int column = 0;
    
    if (report_fields_set)
        return retval;

    if (Connect() == 0)
    {
        // Get fields for GUT
        identifier = MCVE_TransNew(conn);
        MCVE_TransParam(conn, identifier, MC_USERNAME, user);
        MCVE_TransParam(conn, identifier, MC_PASSWORD, password);
        MCVE_TransParam(conn, identifier, MC_TRANTYPE, MC_TRAN_ADMIN);
        MCVE_TransParam(conn, identifier, MC_BATCH, "1");
        MCVE_TransParam(conn, identifier, MC_ADMIN, MC_ADMIN_GUT);
        if (TransSendSimple(identifier) == 0)
        {
            if (MCVE_ParseCommaDelimited(conn, identifier))
            {
                columns = MCVE_NumColumns(conn, identifier);
                for (column = 0; column < columns; column += 1)
                {
                    strcpy(header, MCVE_GetHeader(conn, identifier, column));
                    if (strcmp(header, "ttid") == 0)
                        GUTFIELDS[GUT_TTID] = column;
                    else if (strcmp(header, "type") == 0)
                        GUTFIELDS[GUT_TYPE] = column;
                    else if (strcmp(header, "card") == 0)
                        GUTFIELDS[GUT_CARD] = column;
                    else if (strcmp(header, "account") == 0)
                        GUTFIELDS[GUT_ACCOUNT] = column;
                    else if (strcmp(header, "expdate") == 0)
                        GUTFIELDS[GUT_EXPDATE] = column;
                    else if (strcmp(header, "amount") == 0)
                        GUTFIELDS[GUT_AMOUNT] = column;
                    else if (strcmp(header, "authnum") == 0)
                        GUTFIELDS[GUT_AUTHNUM] = column;
                    else if (strcmp(header, "batch") == 0)
                        GUTFIELDS[GUT_BATCH] = column;
                    else if (strcmp(header, "item") == 0)
                        GUTFIELDS[GUT_ITEM] = column;
                    else if (strcmp(header, "timestamp") == 0)
                        GUTFIELDS[GUT_TIMESTAMP] = column;
                }
            }

            // Get fields for GL (only if getting fields for GUT succeeded)
            identifier = MCVE_TransNew(conn);
            MCVE_TransParam(conn, identifier, MC_USERNAME, user);
            MCVE_TransParam(conn, identifier, MC_PASSWORD, password);
            MCVE_TransParam(conn, identifier, MC_TRANTYPE, MC_TRAN_ADMIN);
            MCVE_TransParam(conn, identifier, MC_BATCH, "1");
            MCVE_TransParam(conn, identifier, MC_ADMIN, MC_ADMIN_GL);
            if (TransSendSimple(identifier) == 0)
            {
                if (MCVE_ParseCommaDelimited(conn, identifier))
                {
                    columns = MCVE_NumColumns(conn, identifier);
                    for (column = 0; column < columns; column += 1)
                    {
                        strcpy(header, MCVE_GetHeader(conn, identifier, column));
                        if (strcmp(header, "ttid") == 0)
                            GLFIELDS[GL_TTID] = column;
                        else if (strcmp(header, "type") == 0)
                            GLFIELDS[GL_TYPE] = column;
                        else if (strcmp(header, "card") == 0)
                            GLFIELDS[GL_CARD] = column;
                        else if (strcmp(header, "account") == 0)
                            GLFIELDS[GL_ACCOUNT] = column;
                        else if (strcmp(header, "expdate") == 0)
                            GLFIELDS[GL_EXPDATE] = column;
                        else if (strcmp(header, "amount") == 0)
                            GLFIELDS[GL_AMOUNT] = column;
                        else if (strcmp(header, "authnum") == 0)
                            GLFIELDS[GL_AUTHNUM] = column;
                        else if (strcmp(header, "batnum") == 0)
                            GLFIELDS[GL_BATCH] = column;
                        else if (strcmp(header, "timestamp") == 0)
                            GLFIELDS[GL_TIMESTAMP] = column;
                        else if (strcmp(header, "comments") == 0)
                            GLFIELDS[GL_COMMENTS] = column;
                    }
                }
            }
            else
                retval = 1;
        }
        else
            retval = 1;


        MCVE_DestroyConn(conn);
        conn = NULL;
    }
    else
        retval = 1;

    if (retval == 0)
        report_fields_set = 1;

    return retval;
}

int CCard::GetBatchNumber(char* dest)
{
    FnTrace("CCard::GetBatchNumber()");
    int retval = 0;
    long identifier = 0;
    int  row;
    int  rows;
    int  columns;
    int  batchnum = 0;
    int  tempnum  = 0;

    SetFields(1, identifier);
    if (Connect() == 0)
    {
        identifier = MCVE_TransNew(conn);
        MCVE_TransParam(conn, identifier, MC_USERNAME, user);
        MCVE_TransParam(conn, identifier, MC_PASSWORD, password);
        MCVE_TransParam(conn, identifier, MC_TRANTYPE, MC_TRAN_ADMIN);
        MCVE_TransParam(conn, identifier, MC_ADMIN, MC_ADMIN_GUT);
        if (TransSendSimple(identifier) == 0)
        {
            if (MCVE_ParseCommaDelimited(conn, identifier))
            {
                rows = MCVE_NumRows(conn, identifier);
                columns = MCVE_NumColumns(conn, identifier);
                for (row = 0; row < rows; row++)
                {
                    tempnum = atoi(MCVE_GetCellByNum(conn, identifier, GUTFIELDS[7], row));
                    if (tempnum > batchnum)
                        batchnum = tempnum;
                }
            }
        }
        else
        {
        }
    }
    snprintf(dest, sizeof(dest), "%d", batchnum);

    return retval;
}

int CCard::Sale()
{
    FnTrace("CCard::Sale()");
    int retval = 1;  // 1 == failure, 0 == success
    long identifier;

    if (Connect() == 0)
    {
        identifier = MCVE_TransNew(conn);
        MCVE_TransParam(conn, identifier, MC_USERNAME, user);
        MCVE_TransParam(conn, identifier, MC_PASSWORD, password);
        MCVE_TransParam(conn, identifier, MC_TRANTYPE, MC_TRAN_SALE);
        if (strlen(swipe) > 0)
        {
            MCVE_TransParam(conn, identifier, MC_TRACKDATA, swipe);
        }
        else
        {
            MCVE_TransParam(conn, identifier, MC_ACCOUNT, number);
            MCVE_TransParam(conn, identifier, MC_EXPDATE, expire);
        }
        MCVE_TransParam(conn, identifier, MC_AMOUNT, amount / 100.00);
        if (TransSend(identifier) == 0)
            retval = 0;
    }

    return retval;
}

int CCard::PreAuth()
{
    FnTrace("CCard::PreAuth()");
    int retval = 1;
    long identifier = 0;

    if (Connect() == 0)
    {
        identifier = MCVE_TransNew(conn);
        MCVE_TransParam(conn, identifier, MC_USERNAME, user);
        MCVE_TransParam(conn, identifier, MC_PASSWORD, password);
        MCVE_TransParam(conn, identifier, MC_TRANTYPE, MC_TRAN_PREAUTH);
        if (strlen(swipe) > 0)
        {
            MCVE_TransParam(conn, identifier, MC_TRACKDATA, swipe);
        }
        else
        {
            MCVE_TransParam(conn, identifier, MC_ACCOUNT, number);
            MCVE_TransParam(conn, identifier, MC_EXPDATE, expire);
        }
        MCVE_TransParam(conn, identifier, MC_AMOUNT, amount / 100.00);
        if (TransSend(identifier) == 0)
            retval = 0;
    }

    return retval;
}

int CCard::FinishAuth()
{
    FnTrace("CCard::FinishAuth()");
    int retval = 1;
    long identifier = 0;

    if (Connect() == 0)
    {
        identifier = MCVE_TransNew(conn);
        MCVE_TransParam(conn, identifier, MC_USERNAME, user);
        MCVE_TransParam(conn, identifier, MC_PASSWORD, password);
        MCVE_TransParam(conn, identifier, MC_TRANTYPE, MC_TRAN_PREAUTHCOMPLETE);
        MCVE_TransParam(conn, identifier, MC_TTID, ttid);
        MCVE_TransParam(conn, identifier, MC_AMOUNT, fullamount / 100.00);
        if (TransSend(identifier) == 0)
            retval = 0;
    }

    return retval;
}

int CCard::Void()
{
    FnTrace("CCard::Void()");
    int retval = 1;
    long identifier = 0;

    if (Connect() == 0)
    {
        identifier = MCVE_TransNew(conn);
        MCVE_TransParam(conn, identifier, MC_USERNAME, user);
        MCVE_TransParam(conn, identifier, MC_PASSWORD, password);
        MCVE_TransParam(conn, identifier, MC_TRANTYPE, MC_TRAN_VOID);
        MCVE_TransParam(conn, identifier, MC_TTID, ttid);
        if (TransSend(identifier) == 0)
            retval = 0;
    }

    return retval;
}

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
    long identifier = 0;

    if (Connect() == 0)
    {
        identifier = MCVE_TransNew(conn);
        MCVE_TransParam(conn, identifier, MC_USERNAME, user);
        MCVE_TransParam(conn, identifier, MC_PASSWORD, password);
        MCVE_TransParam(conn, identifier, MC_TRANTYPE, MC_TRAN_RETURN);
        if (strlen(swipe) > 0)
        {
            MCVE_TransParam(conn, identifier, MC_TRACKDATA, swipe);
        }
        else
        {
            MCVE_TransParam(conn, identifier, MC_ACCOUNT, number);
            MCVE_TransParam(conn, identifier, MC_EXPDATE, expire);
        }
        MCVE_TransParam(conn, identifier, MC_AMOUNT, fullamount / 100.00);
        if (TransSend(identifier) == 0)
            retval = 0;
    }

    return retval;
}


int CCard::RefundCancel()
{
    FnTrace("CCard::RefundCancel()");
    int retval = 1;

    return retval;
}

int CCard::BatchSettle()
{
    FnTrace("CCard::BatchSettle()");
    int retval = 1;
    char batchnum[STRLENGTH];
    char errbuff[STRLENGTH];
    char msgbuff[STRLENGTH];
    long identifier = 0;
    BatchInfo binfo;

    RStr(server);
    RStr(port);
    RStr(batchnum);
    RStr(user);
    RStr(password);

    if (Connect() == 0)
    {
        if (strcmp(batchnum, "find") == 0)
            GetBatchNumber(batchnum);
        identifier = MCVE_TransNew(conn);
        MCVE_TransParam(conn, identifier, MC_USERNAME, user);
        MCVE_TransParam(conn, identifier, MC_PASSWORD, password);
        MCVE_TransParam(conn, identifier, MC_TRANTYPE, MC_TRAN_SETTLE);
        MCVE_TransParam(conn, identifier, MC_BATCH, batchnum);
        if (TransSend(identifier) == 0)
        {
            identifier = MCVE_TransNew(conn);
            MCVE_TransParam(conn, identifier, MC_USERNAME, user);
            MCVE_TransParam(conn, identifier, MC_PASSWORD, password);
            MCVE_TransParam(conn, identifier, MC_TRANTYPE, MC_TRAN_ADMIN);
            MCVE_TransParam(conn, identifier, MC_ADMIN, MC_ADMIN_GL);
            MCVE_TransParam(conn, identifier, MC_BATCH, batchnum);
            if (TransSendSimple(identifier) == 0)
            {
                binfo.ParseResults(conn, identifier);
                WInt8(SERVER_CC_SETTLED);
                binfo.Write();
                retval = 0;
            }
        }
    }

    if (retval)
    {
        WInt8(SERVER_CC_SETTLEFAILED);
        if (conn != NULL)
        {
            SetValue(msgbuff, MCVE_TransactionText(conn, identifier));
            if (strlen(msgbuff) < 1)
                strcpy(msgbuff, "Unknown");
        }
        else
            strcpy(msgbuff, "Connect error");
        WStr(msgbuff);
        snprintf(errbuff, STRLENGTH, "Failed to close batch '%s'", batchnum);
        ReportError(errbuff);
        ReportError(msgbuff);
    }
    SendNow();

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
    char batchnum[STRLENGTH];
    long identifier = 0;
    int  row;
    int  rows;
    int  columns;
    char buffer[STRLONG];
    char ttype[STRLENGTH];

    RStr(server);
    RStr(port);
    RStr(batchnum);
    RStr(user);
    RStr(password);

    SetFields(0, identifier);
    if (Connect() == 0)
    {
        identifier = MCVE_TransNew(conn);
        MCVE_TransParam(conn, identifier, MC_USERNAME, user);
        MCVE_TransParam(conn, identifier, MC_PASSWORD, password);
        MCVE_TransParam(conn, identifier, MC_TRANTYPE, MC_TRAN_ADMIN);
        MCVE_TransParam(conn, identifier, MC_ADMIN, MC_ADMIN_GL);
        if (strcmp(batchnum, "all"))
            MCVE_TransParam(conn, identifier, MC_BATCH, batchnum);
        if (TransSendSimple(identifier) == 0)
        {
            if (MCVE_ParseCommaDelimited(conn, identifier))
            {
                rows = MCVE_NumRows(conn, identifier);
                WInt8(SERVER_CC_TOTALS);
                WInt16(rows + 1);
                columns = MCVE_NumColumns(conn, identifier);
                buffer[0] = '\0';
                AppendString(buffer, 8, "TTID");
                AppendString(buffer, 10, "Type");
                AppendString(buffer, 7, "Card");
                AppendString(buffer, 20, "Account");
                AppendString(buffer, 7, "Exp");
                AppendString(buffer, 7, "Amt");
                AppendString(buffer, 18, "Time Stamp");
                AppendString(buffer, 8, "Auth");
                AppendString(buffer, 6, "Batch");
                WStr(buffer);
                for (row = 0; row < rows; row++)
                {
                    strcpy(ttype, MCVE_GetCellByNum(conn, identifier, GLFIELDS[GL_TYPE], row));
                    buffer[0] = '\0';
                    if (strcmp(ttype, "SETTLE") == 0)
                    {
                        AppendString(buffer, 8, MCVE_GetCellByNum(conn, identifier, GLFIELDS[GL_TTID], row));
                        AppendString(buffer, 10, ttype);
                        AppendString(buffer, 41, MCVE_GetCellByNum(conn, identifier, GLFIELDS[GL_COMMENTS], row));
                        AppendString(buffer, 18, MCVE_GetCellByNum(conn, identifier, GLFIELDS[GL_TIMESTAMP], row));
                    }
                    else
                    {
                        // This is just an effort to clean the the report visually.  The
                        // VOID_PREAUTHVISA and VOID_PREAUTHMC values can mess up the columns.
                        if (strncmp(ttype, "VOID_PREAUTH", 12) == 0)
                            strcpy(ttype, "VOID_PRE");
                        AppendString(buffer, 8, MCVE_GetCellByNum(conn, identifier, GLFIELDS[GL_TTID], row));
                        AppendString(buffer, 10, ttype);
                        AppendString(buffer, 7, MCVE_GetCellByNum(conn, identifier, GLFIELDS[GL_CARD], row));
                        AppendString(buffer, 20, MCVE_GetCellByNum(conn, identifier, GLFIELDS[GL_ACCOUNT], row));
                        AppendString(buffer, 7, MCVE_GetCellByNum(conn, identifier, GLFIELDS[GL_EXPDATE], row));
                        AppendString(buffer, 7, MCVE_GetCellByNum(conn, identifier, GLFIELDS[GL_AMOUNT], row));
                        AppendString(buffer, 18, MCVE_GetCellByNum(conn, identifier, GLFIELDS[GL_TIMESTAMP], row));
                        AppendString(buffer, 8, MCVE_GetCellByNum(conn, identifier, GLFIELDS[GL_AUTHNUM], row));
                        AppendString(buffer, 6, MCVE_GetCellByNum(conn, identifier, GLFIELDS[GL_BATCH], row));
                    }
                    WStr(buffer);
                }
                SendNow();
            }
        }
        else
        {
        }
    }

    return retval;
}


int CCard::Details()
{
    FnTrace("CCard::Details()");
    int retval = 0;
    char batchnum[STRLENGTH];
    long identifier = 0;
    int  row;
    int  rows;
    int  columns;
    char buffer[STRLONG];
    char buff2[STRLENGTH];
    int  len;

    RStr(server);
    RStr(port);
    RStr(batchnum);
    RStr(user);
    RStr(password);

    SetFields(1, identifier);
    if (Connect() == 0)
    {
        identifier = MCVE_TransNew(conn);
        MCVE_TransParam(conn, identifier, MC_USERNAME, user);
        MCVE_TransParam(conn, identifier, MC_PASSWORD, password);
        MCVE_TransParam(conn, identifier, MC_TRANTYPE, MC_TRAN_ADMIN);
        MCVE_TransParam(conn, identifier, MC_ADMIN, MC_ADMIN_GUT);
        if (strcmp(batchnum, "all"))
            MCVE_TransParam(conn, identifier, MC_BATCH, batchnum);
        if (TransSendSimple(identifier) == 0)
        {
            if (MCVE_ParseCommaDelimited(conn, identifier))
            {
                rows = MCVE_NumRows(conn, identifier);
                columns = MCVE_NumColumns(conn, identifier);
                WInt8(SERVER_CC_DETAILS);
                WInt16(rows + 1);
                buffer[0] = '\0';
                AppendString(buffer, 8, "TTID");
                AppendString(buffer, 10, "Type");
                AppendString(buffer, 7, "Card");
                AppendString(buffer, 7, "Acct");
                AppendString(buffer, 7, "Exp");
                AppendString(buffer, 7, "Amt");
                AppendString(buffer, 8, "Auth");
                AppendString(buffer, 18, "Time Stamp");
                AppendString(buffer, 6, "Batch");
                AppendString(buffer, 5, "Item");
                WStr(buffer);
                for (row = 0; row < rows; row++)
                {
                    buffer[0] = '\0';
                    AppendString(buffer, 8, MCVE_GetCellByNum(conn, identifier, GUTFIELDS[GUT_TTID], row));
                    AppendString(buffer, 10, MCVE_GetCellByNum(conn, identifier, GUTFIELDS[GUT_TYPE], row));
                    AppendString(buffer, 7, MCVE_GetCellByNum(conn, identifier, GUTFIELDS[GUT_CARD], row));
                    strcpy(buff2, MCVE_GetCellByNum(conn, identifier, GUTFIELDS[GUT_ACCOUNT], row));
                    len = strlen(buff2);
                    AppendString(buffer, 7, &buff2[len - 4]);
                    AppendString(buffer, 7, MCVE_GetCellByNum(conn, identifier, GUTFIELDS[GUT_EXPDATE], row));
                    AppendString(buffer, 7, MCVE_GetCellByNum(conn, identifier, GUTFIELDS[GUT_AMOUNT], row));
                    AppendString(buffer, 8, MCVE_GetCellByNum(conn, identifier, GUTFIELDS[GUT_AUTHNUM], row));
                    AppendString(buffer, 18, MCVE_GetCellByNum(conn, identifier, GUTFIELDS[GUT_TIMESTAMP], row));
                    AppendString(buffer, 6, MCVE_GetCellByNum(conn, identifier, GUTFIELDS[GUT_BATCH], row));
                    AppendString(buffer, 5, MCVE_GetCellByNum(conn, identifier, GUTFIELDS[GUT_ITEM], row));
                    WStr(buffer);
                }
                SendNow();
            }
        }
        else
        {
        }
    }

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
