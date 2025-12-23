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
 * credit.hh - revision 18 (9/10/98)
 * Credit/charge card verification/authorization
 */

#ifndef CREDIT_HH
#define CREDIT_HH

#include "list_utility.hh"
#include "printer.hh"
#include "utility.hh"

class Archive;
class ReportZone;
class LayoutZone;

#define CREDIT_CARD_VERSION  1
#define CCBINFO_VERSION      2

extern const char* CardTypeName[];
extern const char* CardTypeShortName[];
extern int   CardTypeValue[];
extern const char* CreditCardName[];
extern const char* CreditCardShortName[];
extern int   CreditCardValue[];

#define CC_STATUS_NONE        (-2)
// these are taken from MCVE
#define CC_STATUS_ERROR       (-1)
#define CC_STATUS_FAIL         0
#define CC_STATUS_SUCCESS      1
#define CC_STATUS_AUTH         2
#define CC_STATUS_DENY         3
#define CC_STATUS_CALL         4
#define CC_STATUS_DUPL         5
#define CC_STATUS_PKUP         6
#define CC_STATUS_RETRY        7
#define CC_STATUS_SETUP        8
#define CC_STATUS_TIMEOUT      9
#define CC_STATUS_SETTLED     10
#define CC_STATUS_VOICE       11

// these are not from MCVE
#define CC_STATUS_NOCONNECT   20
#define CC_STATUS_WRITEFAIL   21

// actions
#define CCAUTH_FIND           (-1)
#define CCAUTH_NOACTION        0
#define CCAUTH_PREAUTH         1
#define CCAUTH_AUTHORIZE       2
#define CCAUTH_COMPLETE        3
#define CCAUTH_VOID            4
#define CCAUTH_VOID_CANCEL     5
#define CCAUTH_REFUND          6
#define CCAUTH_REFUND_CANCEL   7
#define CCAUTH_ADVICE          8

// processors
#define CCAUTH_NONE            0
#define CCAUTH_VISANET         1
#define CCAUTH_MAINSTREET      2
#define CCAUTH_CREDITCHEQ      3
#define CCAUTH_MAX             3  // set to the highest possible CCAUTH_ value

#define CARD_TYPE_NONE         0
#define CARD_TYPE_CREDIT       1
#define CARD_TYPE_DEBIT        2
#define CARD_TYPE_GIFT         4

#define CREDIT_TYPE_UNKNOWN    0
#define CREDIT_TYPE_VISA       1
#define CREDIT_TYPE_MASTERCARD 2
#define CREDIT_TYPE_AMEX       3
#define CREDIT_TYPE_DISCOVER   4
#define CREDIT_TYPE_DINERSCLUB 5
#define CREDIT_TYPE_JCB        6
#define CREDIT_TYPE_DEBIT      7 // ONLY for authorize_method == CCAUTH_NONE

#define CC_INFO_NONE           0
#define CC_INFO_DEBIT          1
#define CC_INFO_PURCHASE       2
#define CC_INFO_REFUND         3
#define CC_INFO_VOID           4
#define CC_INFO_VISA           5
#define CC_INFO_MASTERCARD     6
#define CC_INFO_AMEX           7
#define CC_INFO_DISCOVER       8
#define CC_INFO_DINERSCLUB     9
#define CC_INFO_JCB           10

#define CC_DBTYPE_NONE         0
#define CC_DBTYPE_VOID         1
#define CC_DBTYPE_REFUND       2
#define CC_DBTYPE_EXCEPT       3

#define DEBIT_ACCT_NONE        0
#define DEBIT_ACCT_CHECKING    1
#define DEBIT_ACCT_SAVINGS     2

// These definitions are specifically for use with the
// CreditCardDialog
#define CC_AMOUNT  1
#define CC_TIP     2
#define CC_REFUND  3

#define PREAUTH_MSG         "PreAuthorizing"
#define COMPLETE_MSG        "Completing PreAuth"
#define AUTHORIZE_MSG       "Authorizing"
#define ADVICE_MSG          "PreAuth Advice"
#define VOID_MSG            "Voiding"
#define REFUND_MSG          "Refunding"
#define REFUND_CANCEL_MSG   "Cancelling Refund"

#define SWIPE_MSG           "Please Swipe Card"
#define WAIT_MSG            "Please Wait"

#define AUTH_DEFAULT        (-1)
#define AUTH_NONE            0
#define AUTH_IN_PROGRESS     1
#define AUTH_PREAUTH         2
#define AUTH_AUTHORIZE       4
#define AUTH_VOID            8
#define AUTH_REFUND         16
#define AUTH_REFUND_CORRECT 32
#define AUTH_PICK           64
#define AUTH_COMPLETE      128
#define AUTH_ADVICE        256

// Receipt copy selection (converted from macros to enum constants)
enum ReceiptCopy {
    RECEIPT_PICK     = 0, // auto-select receipt types based on settings
    RECEIPT_CUSTOMER = 1,
    RECEIPT_MERCHANT = 2
};

#define MASTER_CC_EXCEPT     VIEWTOUCH_PATH "/dat/current/cc_exceptions.dat"
#define MASTER_CC_REFUND     VIEWTOUCH_PATH "/dat/current/cc_refunds.dat"
#define MASTER_CC_VOID       VIEWTOUCH_PATH "/dat/current/cc_voids.dat"
#define MASTER_CC_SETTLE     VIEWTOUCH_PATH "/dat/current/cc_settle.dat"
#define MASTER_CC_INIT       VIEWTOUCH_PATH "/dat/current/cc_init.dat"
#define MASTER_CC_SAF        VIEWTOUCH_PATH "/dat/current/cc_saf.dat"


/**** Functions ****/
int CC_CreditCardType(const char* account_no);
int CC_IsValidAccountNumber(const char* account_no);
int CC_IsValidExpiry(const char* expiry);


/**** Types ****/
class Check;
class Report;
class Terminal;
class InputDataFile;
class OutputDataFile;

class Credit
{
    DList<Credit> errors_list;

    int  card_id;      // only used by CreditDB
    int  db_type;      // ditto

    Str  swipe;
    Str  approval;
    Str  number;
    Str  name;
    Str  expire;
    Str  country;
    int  card_type;    // debit, credit, gift card, etc.
    int  credit_type;  // Visa, MasterCard, etc.
    int  debit_acct;   // Savings, Checking, etc.

    Str  code;
    int  intcode;
    Str  isocode;
    Str  b24code;
    Str  verb;
    Str  auth;
    int64_t batch;
    int64_t item;
    int64_t ttid;
    Str  AVS;
    Str  CV;
    int  last_action;
    int  state;
    int  auth_state;
    int  trans_success;
    int  processor;   // CreditCheq, MCVE, etc.

    // specific to CreditCheq
    Str term_id;
    Str batch_term_id;
    Str reference;
    Str sequence;
    Str server_date;
    Str server_time;
    Str receipt_line;
    Str display_line;

    // for tracking purposes
    int auth_user_id;
    int void_user_id;
    int refund_user_id;
    int except_user_id;

    //NOTE-->BAK  I've increased all indices by 10 arbitrarily.  I had one of them set
    // too low (didn't leave room for the null byte), which caused segfaults, and
    // because memory is not a priority I would rather waste the space than worry
    // about more oopses.
    int   read_t1;             // boolean flag to determine whether we've read track 1
    char  t1_fc;               // format code
    char  t1_pan[30];          // primary account number
    char  t1_country[14];
    char  t1_name[37];
    char  t1_expiry[15];
    char  t1_sc[14];            // service code
    char  t1_pvv[16];           // pin verification value
    char  t1_disc[STRLENGTH];  // discretionary data

    int   read_t2;             // boolean flag to determine whether we've read track 2
    char  t2_pan[30];
    char  t2_country[14];
    char  t2_expiry[15];
    char  t2_sc[14];
    char  t2_pvv[16];
    char  t2_disc[STRLENGTH];

    int   read_t3;
    char  t3_fc[13];
    char  t3_pan[30];
    char  t3_country[14];
    char  t3_currency[14];
    char  t3_ce;                // currency exchange
    char  t3_aa[15];             // amount authorized per cycle
    char  t3_ar[15];             // amount remaining in this cycle
    char  t3_cb[15];             // cycle begin date
    char  t3_cl[13];             // cycle length
    char  t3_rc;                // retry count
    char  t3_pincp[17];          // pin control parameters
    char  t3_ic;                // interchange control
    char  t3_pansr[13];          // pan service restriction
    char  t3_fsansr[13];         // fsan service restriction
    char  t3_ssansr[13];         // ssan service restriction
    char  t3_expiry[15];
    char  t3_csn;               // card sequence number
    char  t3_cscn[20];          // card security number
    char  t3_fsan[STRLENGTH];   // first subsidiary account number
    char  t3_ssan[STRLENGTH];   // second subsidiary account number
    char  t3_rm;                // relay marker
    char  t3_ccd[17];            // crypto check digits
    // if t3_fc == 02, the following variables are required
    char  t3_td[15];             // transaction date
    char  t3_avv[19];            // additional verification value
    char  t3_acsn[14];           // alternative card sequence number
    char  t3_inic[14];           // international network identification
    // discretionary data if fc == 01 or fc == 02
    char  t3_disc[STRLENGTH];

    int   read_manual;
    char  mn_pan[30];
    char  mn_expiry[15];

    int   valid;

    int amount;
    int tip;
    int preauth_amount;
    int auth_amount;
    int refund_amount;
    int void_amount;

    int     SetCreditType();
    int     GetTrack( char* dest, const char* source, int maxlen );
    int     ParseTrack1(const char* swipe_value);
    int     ParseTrack2(const char* swipe_value);
    int     ParseTrack3(const char* swipe_value);
    int     ParseManual(const char* swipe_value);
    char* ReverseExpiry( char* expiry );
    int     ValidateCardInfo();
    int     CanPrintSignature();
    int     ReceiptPrint(Terminal *term, int receipt_type, Printer *pprinter = nullptr,
                         int print_amount = -1);

public:
    Credit *fore;
    Credit *next;
    int forced;
    TimeInfo preauth_time;
    TimeInfo auth_time;
    TimeInfo void_time;
    TimeInfo refund_time;
    TimeInfo refund_cancel_time;
    TimeInfo settle_time;
    int check_id;   // set when the Credit is first attached to a check

    // Constructors
    Credit();
    Credit(const char* swipe_value);
    ~Credit();

    // Member Functions
    int            Clear(int safe_clear = 0);
    int            Read(InputDataFile &df, int version);
    int            Write(OutputDataFile &df, int version);
    int            AddError(Credit *ecredit);
    Credit        *Copy();
    int            Copy(Credit *credit);
    int            ParseSwipe(const char* swipe_value);
    int            ParseApproval(const char* value);
    int            CardType() { return card_type; }
    char* CreditTypeName( char* str=nullptr, int shortname = 0 );
    int            CreditType();
    int            GetApproval(Terminal *term);
    int            GetPreApproval(Terminal *term);
    int            GetFinalApproval(Terminal *term);
    int            GetVoid(Terminal *term);
    int            GetVoidCancel(Terminal *term);
    int            GetRefund(Terminal *term);
    int            GetRefundCancel(Terminal *term);
    int            PrintReceipt(Terminal *term, int receipt_type = RECEIPT_PICK,
                                Printer *pprinter = nullptr, int print_amount = -1);
    int            IsEmpty();
    int            IsValid();
    int            IsVoiced();
    int            IsPreauthed();
    int            IsAuthed(int also_preauth = 0);
    int            IsVoided(int any_value = 0);
    int            IsRefunded(int any_value = 0);
    int            IsSettled();
    int            IsDeclined();
    int            IsErrored();
    int            RequireSwipe();
    int            PrintAuth();
    int            ClearAuth();
    int            Finalize(Terminal *term);
    int            GetStatus();
    const char* Code() { return code.Value(); }
    const char* Approval();
    const char* Auth() { return auth.Value(); }
    const char* PAN(int all = 0);
    char* LastFour( char* dest = nullptr);
    const char* ExpireDate(); // nicely formats expiration date
    const char* Name();
    const char* Verb() { return verb.Value(); }
    long long      Batch() { return batch; }
    const char* TermID() { return term_id.Value(); }
    int            Country();
    int            LastAction(int last = -1);

    int            MaskCardNumber();
    int            ClearCardNumber() { number.Set(""); expire.Set(""); return 1; }
    int            SetCardType(int newtype) { return card_type = newtype; }
    int            SetApproval(const char* set) { approval.Set(set); return 0; }
    int            SetCode(const char* set) { code.Set(set); return 0; }
    int            SetVerb(const char* set) { verb.Set(set); return 0; }
    int            SetAuth(const char* set) { auth.Set(set); return 0; }
    int            SetBatch(long long set, const char* btermid = nullptr);
    int            SetItem(long set) { item = set; return 0; }
    int            SetTTID(long set) { ttid = set; return 0; }
    int            SetAVS(const char* set) { AVS.Set(set); return 0; }
    int            SetCV(const char* set) { CV.Set(set); return 0; }
    int            SetState(int newstate = CCAUTH_FIND);
    int            SetStatus(int newstat) { return intcode = newstat; }

    int            Amount(int newamount = -1);
    int            Tip(int newtip = -1);
    int            PreauthAmt() { return preauth_amount; }
    int            AuthAmt() { return auth_amount; }
    int            RefundAmt() { return refund_amount; }
    int            VoidAmt() { return void_amount; }
    int            FullAmount();
    int            Total(int also_preauth = 0);
    int            TotalPreauth();

    int   operator == (Credit *c);

    friend class Terminal;
    friend class CreditDB;
};

class CreditDB
{
    DList<Credit> credit_list;
    char fullpath[STRLONG];
    int  db_type;
    int  last_card_id;

public:

    CreditDB(int dbtype);
    ~CreditDB();

    Credit  *CreditList() { return credit_list.Head(); }
    Credit  *CreditListEnd() { return credit_list.Tail(); }
    int      Count() { return credit_list.Count(); }
    void     Purge() { credit_list.Purge(); }
    CreditDB *Copy();

    int      Read(InputDataFile &infile);
    int      Write(OutputDataFile &outfile);
    int      Save();
    int      Load(const char* path);
    int      Add(Credit *credit);
    int      Add(Terminal *term, Credit *credit);
    int      Remove(int id);
    int      MakeReport(Terminal *term, Report *report, LayoutZone *rzone);
    Credit  *FindByRecord(Terminal *term, int record);
    int      HaveOpenCards();
    void     SetDBType(int type) { db_type = type; }
};

class CCBInfo
{
    int version;
    int type;
    Str name;
    int numhost;
    int amthost;
    int numtr;
    int amttr;
    int numvt;
    int amtvt;

    friend class CCSettle;

public:

    CCBInfo();
    CCBInfo(const char* newname);
    CCBInfo(const char* newname, int type);
    int  Type() { return type; }
    int  Add(Credit *credit);
    int  Add(int amount);
    int  IsZero();
    void Copy(CCBInfo *info);
    void SetName(const char* newname);
    void Clear();
    int  Read(InputDataFile &df);
    int  Write(OutputDataFile &df);
    int  ReadResults(Terminal *term);
    int  MakeReport(Terminal *term, Report *report, int start, int spacing, int doubled = 0);
    void DebugPrint();
};

/****
 * CCSettle: this is a tad awkward.  I decided to use the class as its
 * own container class and this may not have worked out well.  But for
 * now, I don't have time to rework the whole thing.
 ****/
class CCSettle
{

    Str result;
    Str settle;
    Str termid;
    Str op;
    Str merchid;
    Str seqnum;
    Str shift;
    Str batch;
    Str bdate;
    Str btime;
    Str receipt;
    Str display;
    Str iso;
    Str b24;
    Str errormsg;

    CCBInfo visa;
    CCBInfo mastercard;
    CCBInfo amex;
    CCBInfo diners;
    CCBInfo debit;
    CCBInfo discover;
    CCBInfo jcb;
    CCBInfo purchase;
    CCBInfo refund;
    CCBInfo voids;
    char filepath[STRLONG];

    int GenerateReport(Terminal *term, Report *report, ReportZone *rzone, Archive *reparc);

public:
    CCSettle *next;
    CCSettle *fore;

    // These really shouldn't be public at all.  But System::EndDay() needs
    // them to make sure we get to see the latest report even after they've
    // been archived.
    CCSettle *current;
    Archive  *archive;

    TimeInfo settle_date;

    CCSettle();
    CCSettle(const char* fullpath);
    ~CCSettle();

    int       Next(Terminal *term);
    int       Fore(Terminal *term);
    CCSettle *Last();
    int       Add(Terminal *term, const char* message = nullptr);
    int       Add(Check *check);
    CCSettle *Copy();
    void      Clear();
    int       Read(InputDataFile &df);
    int       Write(OutputDataFile &df);
    int       Load(const char* filename);
    int       Save();
    int       ReadResults(Terminal *term);
    int       IsSettled();
    int       MakeReport(Terminal *term, Report *report, ReportZone *rzone);
    void      DebugPrint();
    const char* Batch()  { return batch.Value(); }
    const char* TermID() { return termid.Value(); }
};

class CCInit
{
    SList<Str> init_list;
    char filepath[STRLONG];

    CCInit *current;
    Archive  *archive;

    int GenerateReport(Terminal *term, Report *report, ReportZone *rzone);

public:
    CCInit *next;
    CCInit *fore;

    CCInit();
    CCInit(const char* fullpath);
    ~CCInit();

    int  Next(Terminal *term);
    int  Fore(Terminal *term);
    void Clear() { init_list.Purge(); }
    int  Count() { return init_list.Count(); }
    int  Add(const char* termid, const char* result);
    int  MakeReport(Terminal *term, Report *report, ReportZone *rzone);
    int  Read(InputDataFile &df);
    int  Write(OutputDataFile &df);
    int  Load(const char* filename);
    int  Save();
};

class CCDetails
{
    DList<Str> mcve_list;
public:
    CCDetails *next;
    CCDetails *fore;

    CCDetails();
    ~CCDetails();
    void Clear();
    int  Count();
    int  Add();
    int  Add(const char* );
    int  MakeReport(Terminal *term, Report *report, ReportZone *rzone);
};

class CCSAFDetails
{
    Str  terminal;
    Str  batch;
    Str  op;
    Str  merchid;
    Str  safdate;
    Str  saftime;
    Str  display;
    Str  safnum;
    int  numrecords;
    int  notproc;
    int  completed;
    int  declined;
    int  errors;
    int  voided;
    int  expired;
    int  last;
    char filepath[STRLONG];

    int ReadResults(Terminal *term);
    int GenerateReport(Terminal *term, Report *report, ReportZone *rzone);

public:
    CCSAFDetails *next;
    CCSAFDetails *fore;
 
    CCSAFDetails *current;
    Archive  *archive;

    CCSAFDetails();
    CCSAFDetails(const char* fullpath);
    ~CCSAFDetails();

    int           Next(Terminal *term);
    int           Fore(Terminal *term);
    CCSAFDetails *Last();
    int           Read(InputDataFile &df);
    int           Write(OutputDataFile &df);
    int           Load(const char* filename);
    int           Save();
    bool          IsEmpty() { return terminal.empty(); }
    void          Clear();
    int           Count();
    int           Add(Terminal *term);
    int           MakeReport(Terminal *term, Report *report, ReportZone *rzone);
};

#endif
