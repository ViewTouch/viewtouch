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
 * settings_zone.hh - revision 50 (8/27/98)
 * Zone forms for system settings
 */

#ifndef _SETTINGS_ZONE_H
#define SETTINGS_ZONE_H

#include "form_zone.hh"

#define ALL_ITEMS_STRING "All items in family"
#define NO_ITEMS_STRING  "No items in family"


/**** Types ****/
// Settings Switch
class SwitchZone : public PosZone
{
    int type;

public:
    // Constructor
    SwitchZone();

    // Member Functions
    int          Type() override { return ZONE_SWITCH; }
    std::unique_ptr<Zone> Copy() override;
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          Update(Terminal *term, int update_message, const genericChar* value) override;
    const genericChar* TranslateString(Terminal *term) override;
    int         *SwitchType() override { return &type; }
};



// General Settings
class SettingsZone : public FormZone
{
    int section;  // Current section being displayed
    FormField *business_start;
    FormField *logon_start;
    FormField *ledger_start;
    FormField *drawer_start;
    FormField *smtp_start;
    FormField *misc_start;
    FormField *restart_start;
    FormField *kitchen_start;

public:
    // Constructor
    SettingsZone();

    // Member Functions
    int          Type() override { return ZONE_SETTINGS; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    int          LoadRecord(Terminal *term, int record) override;
    int          SaveRecord(Terminal *term, int record, int write_file) override;
};

// Receipt Settings
class ReceiptSettingsZone : public FormZone
{
public:
    // Constructor
    ReceiptSettingsZone();

    // Member Functions
    int          Type() override { return ZONE_RECEIPTS; }
    RenderResult Render(Terminal *term, int update_flag) override;
    int          LoadRecord(Terminal *term, int record) override;
    int          SaveRecord(Terminal *term, int record, int write_file) override;
};

// Tax Settings Zone
class TaxSettingsZone : public FormZone
{
public:
    // Constructor
    TaxSettingsZone();

    // Member Functions
    int          Type() override { return ZONE_TAX_SETTINGS; }
    RenderResult Render(Terminal *term, int update_flag) override;
    int          LoadRecord(Terminal *term, int record) override;
    int          SaveRecord(Terminal *term, int record, int write_file) override;
};

// Credit/Charge Card Settings
class CCSettingsZone : public FormZone
{
    FormField *debit_field;
    FormField *gift_field;
    FormField *use_field;
    FormField *save_field;
    FormField *show_field;
    FormField *custinfo_field;

public:
    // Constructor
    CCSettingsZone();

    // Member Functions
    int          Type() override { return ZONE_CC_SETTINGS; }
    RenderResult Render(Terminal *term, int update_flag) override;
    int          LoadRecord(Terminal *term, int record) override;
    int          SaveRecord(Terminal *term, int record, int write_file) override;
    int          UpdateForm(Terminal *term, int record) override;
};

// Credit/Charge Card Message Settings
class CCMessageSettingsZone : public FormZone
{
public:
    CCMessageSettingsZone();

    int          Type() override { return ZONE_CC_MSG_SETTINGS; }
    RenderResult Render(Terminal *term, int update_flag) override;
    int          LoadRecord(Terminal *term, int record) override;
    int          SaveRecord(Terminal *term, int record, int write_file) override;
};

// Low level application settings
class DeveloperZone : public FormZone
{
    int clear_flag;
    unsigned long phrases_changed;

public:
    // Constructor
    DeveloperZone();

    // Member Functions
    int          Type() override { return ZONE_DEVELOPER; }
    int          AddFields();
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    int          LoadRecord(Terminal *term, int record) override;
    int          SaveRecord(Terminal *term, int record, int write_file) override;
};

// Calculation Settings (Multiply, Add/Subtract)
class CalculationSettingsZone : public FormZone
{
    unsigned long phrases_changed;

public:
    // Constructor
    CalculationSettingsZone();

    // Member Functions
    int          Type() override { return ZONE_CALCULATION_SETTINGS; }
    int          AddFields();
    RenderResult Render(Terminal *term, int update_flag) override;
    int          LoadRecord(Terminal *term, int record) override;
    int          SaveRecord(Terminal *term, int record, int write_file) override;
};

/* Revenue Groups Settings */
class RevenueGroupsZone : public FormZone
{
    unsigned long phrases_changed;

public:
    // Constructor
    RevenueGroupsZone();

    // Member Functions
    int          Type() override { return ZONE_REVENUE_GROUPS; }
    int          AddFields();
    RenderResult Render(Terminal *term, int update_flag) override;
    int          LoadRecord(Terminal *term, int record) override;
    int          SaveRecord(Terminal *term, int record, int write_file) override;
};

// Tender Specification & Settings
class TenderSetZone : public ListFormZone
{
    FormField *discount_start;
    FormField *coupon_start;
    FormField *coupon_type;
    FormField *coupon_time_start;
    FormField *coupon_time_end;
    FormField *coupon_date_start;
    FormField *coupon_date_end;
    FormField *coupon_weekdays;
    FormField *coupon_item_specific;
    FormField *coupon_family;
    FormField *creditcard_start;
    FormField *comp_start;
    FormField *meal_start;
    int page;
    int section;
    int display_id;

public:
    // Constructor
    TenderSetZone();

    // Member Functions
    int          Type() override { return ZONE_TENDER_SET; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    Flt         *Spacing() override { return &list_spacing; }

    int LoadRecord(Terminal *term, int record) override;
    int SaveRecord(Terminal *term, int record, int write_file) override;
    int NewRecord(Terminal *term) override;
    int KillRecord(Terminal *term, int record) override;
    int ListReport(Terminal *term, Report *r) override;
    int RecordCount(Terminal *term) override;
    int UpdateForm(Terminal *term, int record) override;
    int ItemList(FormField *itemfield, int family, int item_id);
};

// Currency Selection & Definition
class MoneySetZone : public ListFormZone
{
public:
    // Constructor
    MoneySetZone();

    // Member Functions
    int Type() override { return ZONE_MONEY_SET; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    Flt         *Spacing() override { return &list_spacing; }

    int LoadRecord(Terminal *term, int record) override;
    int SaveRecord(Terminal *term, int record, int write_file) override;
    int NewRecord(Terminal *term) override;
    int KillRecord(Terminal *term, int record) override;
    int ListReport(Terminal *term, Report *r) override;
    int RecordCount(Terminal *term) override;
};

// Tax Definition
class TaxSetZone : public ListFormZone
{
public:
    // Constructor
    TaxSetZone();

    // Member Functions
    int Type() override { return ZONE_TAX_SET; }
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    Flt *Spacing() override { return &list_spacing; }

    int LoadRecord(Terminal *term, int record) override;
    int SaveRecord(Terminal *term, int record, int write_file) override;
    int NewRecord(Terminal *term) override;
    int KillRecord(Terminal *term, int record) override;
    int ListReport(Terminal *term, Report *r) override;
    int RecordCount(Terminal *term) override; 
};

// System time settings
class TimeSettingsZone : public FormZone
{
    int shifts;
    int shift_start[16];
    int meal_used[16];
    int meal_start[16];

public:
    // Constructor
    TimeSettingsZone();

    // Member Functions
    int          Type() override { return ZONE_TIME_SETTINGS; }
    RenderResult Render(Terminal *term, int update_flag) override;
    int          Update(Terminal *term, int update_message, const genericChar* value) override;

    int LoadRecord(Terminal *term, int record) override;
    int SaveRecord(Terminal *term, int record, int write_file) override;
    int UpdateForm(Terminal *term, int record) override;
};

// Expire Message Settings
class ExpireSettingsZone : public FormZone
{
public:
    // Constructor
    ExpireSettingsZone();

    // Member Functions
    int          Type() override { return ZONE_EXPIRE_MSG; }
    RenderResult Render(Terminal *term, int update_flag) override;
    int          LoadRecord(Terminal *term, int record) override;
    int          SaveRecord(Terminal *term, int record, int write_file) override;
};

#endif
