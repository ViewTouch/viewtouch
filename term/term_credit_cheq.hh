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
 * term_credit.hh -- terminal specific credit card processing functions.
 *   Many/most of these functions can lock the process during connection, so
 *   keeping them in vt_term ensures that only the local terminal will be locked.
 */

#pragma once  // REFACTOR: Replaced #ifndef __TERM_CREDIT__ guard with modern pragma once

#include "utility.hh"

class CCard
{
protected:
    int  ipconn;
    char server[STRLENGTH];
    char port[STRLENGTH];
    char user[STRLENGTH];
    char password[STRLENGTH];
    char termid[STRLENGTH];
    char approval[STRLENGTH];
    char number[STRLENGTH];
    char expire[STRLENGTH];
    char name[STRLENGTH];
    char country[STRLENGTH];
    int  debit_acct;
    char code[STRLENGTH];
    int  intcode;
    char isocode[STRLENGTH];
    char b24code[STRLENGTH];
    char verb[STRLENGTH];
    char auth[STRLENGTH];
    char AVS[STRLENGTH];
    char CV[STRLENGTH];
    long long batch;
    long long item;
    long long ttid;
    int  amount;
    int  fullamount;
    int  trans_success;
    int  card_type;

    // specific to CreditCheq
    char reference[STRLENGTH];  // CCQ transaction  number
    char sequence[STRLENGTH];   // possibly equivalent to batch
    char server_date[STRLENGTH];
    char server_time[STRLENGTH];
    char receipt_line[STRLENGTH];
    char display_line[STRLENGTH];
    int  manual;

    char receipt[STRLONG];
    char formatted[STRLONG];

    int ParseResponse(const char* response);
    int ParseReceipt();
    int Command(const char* trans_type, const char* sub_type);

public:
    CCard();
    CCard(const char* serv, const char* prt, const char* id);
    int SendCheq(const char* trans_type, const char* sub_type);
    int SendSAF(const char* trans_type, const char* sub_type);
    int ReadCheq(const char* buffer, int buffsize);
    int Read();
    int Write();
    int Clear();
    int Connect();
    int Close();
    int Sale();
    int PreAuth();
    int FinishAuth();
    int Void();
    int VoidCancel();
    int Refund();
    int RefundCancel();
    int BatchSettle();
    int CCInit();
    int Totals();
    int Details();
    int ClearSAF();
    int SAFDetails();
};
