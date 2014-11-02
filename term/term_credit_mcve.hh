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

#ifndef __TERM_CREDIT__

#include <mcve.h>
#include "utility.hh"

class CCard
{
protected:
    MCVE_CONN *conn;
    char server[STRLENGTH];
    char port[STRLENGTH];
    char user[STRLENGTH];
    char password[STRLENGTH];
    char termid[STRLENGTH];
    char approval[STRLENGTH];
    char swipe[STRLENGTH];
    char number[STRLENGTH];
    char expire[STRLENGTH];
    char name[STRLENGTH];
    char country[STRLENGTH];
    int  debit_acct;
    char code[STRLENGTH];
    int  intcode;
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

    int  SetValue(char *dest, const char *source);
    int  TransSend(long identifier);
    int  TransSendSimple(long identifier);
    int  SetFields(int gut, long identifier);
    int  GetBatchNumber(char *dest);

public:
    CCard();
    int  Read();
    int  Write();
    int  Clear();
    int  Connect();
    int  Close();
    int  Sale();
    int  PreAuth();
    int  FinishAuth();
    int  Void();
    int  VoidCancel();
    int  Refund();
    int  RefundCancel();
    int  BatchSettle();
    int  CCInit();
    int  Totals();
    int  Details();
    int  ClearSAF();
    int  SAFDetails();
};

#define __TERM_CREDIT__
#endif  // __TERM_CREDIT__
