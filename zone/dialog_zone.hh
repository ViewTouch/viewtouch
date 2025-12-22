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
 * dialog_zone.hh - revision 33 (2/3/98)
 * Zone dialog box classes
 */

#ifndef _DIALOG_ZONE_HH
#define _DIALOG_ZONE_HH

#include "check.hh"
#include "layout_zone.hh"
#include "zone_object.hh"


/***********************************************************************
 * Definitions
 ***********************************************************************/

#define ACTION_CANCEL      0
#define ACTION_SUCCESS     1

#define ACTION_DEFAULT     0
#define ACTION_AUTH        1
#define ACTION_JUMPINDEX   2
#define ACTION_SIGNAL      3

#define ZONE_DLG_UNKNOWN   0
#define ZONE_DLG_CREDIT    1

/***********************************************************************
 * DialogAction Struct
 ***********************************************************************/
struct DialogAction {
    int type;
    int arg;
    char msg[STRLENGTH];
    DialogAction() { type = ACTION_DEFAULT; arg = 0; msg[0] = '\0'; }
};


class UnitAmount;

class MessageDialog : public PosZone
{
public:
    MessageDialog(const char* text);
};

class ButtonObj : public ZoneObject
{
public:
    Str label;
    Str message;
    int color;

    ButtonObj(const char* text, const genericChar* message = nullptr);

    int Render(Terminal *term) override;
    int SetLabel(const char* newlabel) { return label.Set(newlabel); }
    int SetMessage(const char* newmessage) { return message.Set(newmessage); }
};

class DialogZone : public LayoutZone
{
protected:
    DialogAction cancel_action;
    DialogAction success_action;

public:
    ZoneObjectList buttons;
    Zone *target_zone;
    // target_index is a method by which a dialog can change pages.  Normally,
    // just calling term->JumpToIndex(...) from within the dialog seems to
    // cause the dialog to be called again after it is deleted, causing
    // problems.  target_index allows the dialog to tell term what to do
    // after it has killed the dialog ("kill me, then go there").
    int   target_index;
    char  target_signal[STRLENGTH];

    DialogZone();

    std::unique_ptr<Zone> Copy() override
    {
        printf("Error:  No DialogZone::Copy() method defined for subclass!\n");
        return nullptr;
    }
    int Type() override { return ZONE_DLG_UNKNOWN; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Mouse(Terminal *term, int action, int mx, int my) override;

    ButtonObj *Button(const char* text, const genericChar* message = nullptr);
    int ClosingAction(int action_type, int action, int arg);
    int ClosingAction(int action_type, int action, const char* message);
    int SetAllActions(DialogZone *dest);
    int PrepareForClose(int action_type);
};

class SimpleDialog : public DialogZone
{
    int format;
    int gap;           // spacing between buttons
    int zofont;        // font size for buttons (ZoneObject Font)
    // the following are only used when format == 1
    int head_height;   // height of header
    int btn_height;    // height of individual buttons
public:

    int force_width;

    SimpleDialog();
    SimpleDialog(const char* title, int format = 0);

    void         SetTitle(const char* new_title) { name.Set(new_title); }
    int          RenderInit(Terminal *term, int update_flag) override;
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
};

class UnitAmountDialog : public DialogZone
{
    ButtonObj *key[14];
    ButtonObj *unit[8];
    ButtonObj *lit;
    int  units;
    int  ut[8];
    char buffer[STRLENGTH + 2];
    int  unit_type;

public:
    UnitAmountDialog(const char* title, UnitAmount &ua);

    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Keyboard(Terminal *term, int key, int state) override;

    int RenderEntry(Terminal *term) override;
};

class TenKeyDialog : public DialogZone
{
    ButtonObj *key[14];
    ButtonObj *lit;
    char return_message[STRLENGTH];

protected:
    int first_row;
    int first_row_y;
    int buffer;
    int decimal;

public:
    int max_amount;

    TenKeyDialog();
    TenKeyDialog(const char* title, int amount, int cancel = 1, int dp = 0);
    TenKeyDialog(const char* title, const char* retmsg, int amount, int dp = 0);

    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Keyboard(Terminal *term, int key, int state) override;

    int RenderEntry(Terminal *term) override;
};

class GetTextDialog : public DialogZone
{
protected:
    ButtonObj *key[37];  // 0-9, A-Z
    ButtonObj *cancelkey;
    ButtonObj *clearkey;
    ButtonObj *spacekey;
    ButtonObj *bskey;
    ButtonObj *enterkey;

    ButtonObj *lit;
    genericChar buffer[STRLENGTH];
    genericChar display_string[STRLENGTH];
    genericChar return_message[STRLENGTH];
    int buffidx;
    int max_len;
    int hh;
    int first_row;
    int first_row_y;

public:
    GetTextDialog();
    GetTextDialog(const char* msg, const char* retmsg, int mlen = 20);

    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Keyboard(Terminal *term, int key, int state) override;

    int RenderEntry(Terminal *term) override;
    int DrawEntry(Terminal *term) override;
    int AddChar(Terminal *term, genericChar val) override;
    int Backspace(Terminal *term) override;
};

class PasswordDialog : public GetTextDialog
{
protected:
    ButtonObj *changekey;
    genericChar password[32];
    genericChar new_password[32];
    int  stage;
    int  min_len;
    int  force_change;

public:
    PasswordDialog(const char* password);

    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;

    int RenderEntry(Terminal *term) override;
    int PasswordOkay(Terminal *term);
    int PasswordFailed(Terminal *term);
};

class CreditCardAmountDialog : public TenKeyDialog
{
    int save_buffer;
    int cct_type;

public:
    CreditCardAmountDialog();
    CreditCardAmountDialog(Terminal *term, const char* title, int type);

    SignalResult Signal(Terminal *term, const genericChar* message);
};

class CreditCardEntryDialog : public TenKeyDialog
{
    char        cc_num[STRLENGTH];
    int         max_num;              // max length, not max value
    char        cc_expire[STRLENGTH];
    int         max_expire;           // max length
    char* current;              // tracks whether we're pointing to cc_num or cc_expire
    char* last_current;
    int         max_current;          // set to either max_num or max_expire
    RegionInfo  entry_pos[2];         // contains the geometry and measurements of the edit regions
    RegionInfo *curr_entry;           // points to either entry_pos[0] or entry_pos[1]

    int FormatCCInfo(char* dest, const char* number, const char* expire);
    int SetCurrent(Terminal *term, const char* set);

public:
    CreditCardEntryDialog();

    int ZoneStates() override { return 1; }

    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Keyboard(Terminal *term, int key, int state) override;

    int RenderEntry(Terminal *term) override;
};

class CreditCardVoiceDialog : public GetTextDialog
{
public:
    CreditCardVoiceDialog();
    CreditCardVoiceDialog(const char* msg, const char* retmsg, int mlen = 20);
    ~CreditCardVoiceDialog() override;

    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
};

class CreditCardDialog : public DialogZone
{
    ButtonObj *preauth_key;
    ButtonObj *complete_key;
    ButtonObj *auth_key;
    ButtonObj *advice_key;
    ButtonObj *tip_key;
    ButtonObj *cancel_key;
    ButtonObj *void_key;
    ButtonObj *refund_key;
    ButtonObj *undorefund_key;
    ButtonObj *manual_key;
    ButtonObj *done_key;
    ButtonObj *credit_key;
    ButtonObj *debit_key;
    ButtonObj *swipe_key;
    ButtonObj *clear_key;
    ButtonObj *voice_key;

    ButtonObj *lit;

    int        authorizing;
    char       message_str[STRLENGTH];
    char       last_message[STRLENGTH];
    int        message_line;
    int        declined;
    int        finalizing;
    int        from_swipe;

    Credit    *saved_credit;

    void  Init(Terminal *term, SubCheck *subch, const char* swipe_value);
    const char* SetMessage(Terminal *term, const char* msg1, const char* msg2 = nullptr);

public:
    CreditCardDialog();
    CreditCardDialog(Terminal *term, const char* swipe_value = nullptr);
    CreditCardDialog(Terminal *term, SubCheck *subch, const char* swipe_value = nullptr);
    CreditCardDialog(Terminal *term, int action, const char* message);

    int          Type() override { return ZONE_DLG_CREDIT; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          SetAction(Terminal *term, int action, const char* msg1, const char* msg2 = nullptr);
    int          ClearAction(Terminal *term, int all = 0);
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Keyboard(Terminal *term, int my_key, int state) override;
    int          ProcessSwipe(Terminal *term, const char* swipe_value);
    int          DialogDone(Terminal *term);
    int          FinishCreditCard(Terminal *term);
    SignalResult ProcessCreditCard(Terminal *term);
};

class JobFilterDialog : public DialogZone
{
    ButtonObj *job[32], *key[4];
    int filter, jobs;

public:
    JobFilterDialog();

    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Keyboard(Terminal *term, int key, int state) override;
};

class SwipeDialog : public DialogZone
{
    ButtonObj *key[2];

public:
    SwipeDialog();

    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Keyboard(Terminal *term, int key, int state) override;
};

class OpenTabDialog : public GetTextDialog
{
    int main_font;
    int entry_font;

    CustomerInfo *customer;
    char customer_name[STRLENGTH];
    int  max_name;
    char customer_phone[STRLENGTH];
    int  max_phone;
    char customer_comment[STRLENGTH];
    int  max_comment;
    char* current;
    char* last_current;
    int  max_current;
    RegionInfo  entry_pos[3];    // contains the geometry and measurements of the edit regions
    RegionInfo *curr_entry;      // points to either entry_pos[0] or entry_pos[1]

    int SetCurrent(Terminal *term, const char* set);

public:
    OpenTabDialog(CustomerInfo *custinfo);
    
    int ZoneStates() override { return 1; }

    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Keyboard(Terminal *term, int kb_key, int state) override;

    int RenderEntry(Terminal *term) override;
};

class OrderCommentDialog : public GetTextDialog
{
public:
    OrderCommentDialog();
    OrderCommentDialog(const char* msg, const char* retmsg, int mlen = 100);

    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Keyboard(Terminal *term, int kb_key, int state) override;

    int RenderEntry(Terminal *term) override;
};




/**** Functions ****/
DialogZone *NewPrintDialog(int no_report = 1);

#endif
