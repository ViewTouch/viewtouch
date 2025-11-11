/*************************************************************
 *
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,`
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * settings_zone.cc - revision 169 (10/13/98)
 * Implementation of SettingsZone module
 *************************************************************/

#include "settings_zone.hh"
#include "sales.hh"
#include "employee.hh"
#include "settings.hh"
#include "report.hh"
#include "labels.hh"
#include "system.hh"
#include "dialog_zone.hh"
#include "credit.hh"
#include "manager.hh"
#include "image_data.hh"
#include "locale.hh"
#include "main/data/settings_enums.hh"
#include "src/utils/vt_enum_utils.hh"
#include "src/utils/vt_logger.hh"

#include <iostream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/*********************************************************************
 * SwitchZone Class
 ********************************************************************/

static const genericChar* PasswordName[] = {"No", "Managers Only", "Everyone", NULL};
static int PasswordValue[] = {PW_NONE, PW_MANAGERS, PW_ALL, -1};

// octal values for ISO-8859-15:
// \244	= Euro 			("€")
// \243 = British pound ("£")
// " "  = no symbol
static const genericChar* MoneySymbolName[] = { "$", "\244", "\243", " ", NULL };

static const genericChar* ButtonTextPosName[] = { "Over Image", "Above Image", "Below Image", NULL };
static int ButtonTextPosValue[] = { 0, 1, 2, -1 };

namespace {

std::string FormatMultiplierDisplay(Flt value)
{
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::fixed << std::setprecision(3) << value;
    std::string text = oss.str();

    const auto last_significant = text.find_last_not_of('0');
    if (last_significant != std::string::npos)
    {
        text.erase(last_significant + 1);
        if (!text.empty() && text.back() == '.')
        {
            text.pop_back();
        }
    }
    else
    {
        text = "0";
    }

    if (text.empty())
    {
        text = "0";
    }

    return text;
}

} // namespace


SwitchZone::SwitchZone()
{
    FnTrace("SwitchZone::SwitchZone()");
    type   = SWITCH_SEATS;
    footer = 16;
}

std::unique_ptr<Zone> SwitchZone::Copy()
{
    FnTrace("SwitchZone::Copy()");
    auto swZone = std::make_unique<SwitchZone>();
    swZone->SetRegion(this);
    swZone->name.Set(name);
    swZone->key      = key;
    swZone->behave   = behave;
    swZone->font     = font;
    swZone->shape    = shape;
    swZone->group_id = group_id;
    swZone->type     = type;
    int i;

    for (i = 0; i < 3; ++i)
    {
        swZone->color[i]   = color[i];
        swZone->image[i]   = image[i];
        swZone->frame[i]  = frame[i];
        swZone->texture[i] = texture[i];
    }

    return swZone;
}

RenderResult SwitchZone::Render(Terminal *term, int update_flag)
{
    FnTrace("SwitchZone::Render()");
	int idx = CompareList(type, SwitchValue);
	if (idx < 0)
		return Zone::Render(term, update_flag);

	RenderZone(term, SwitchName[idx], update_flag);
	Settings *settings = term->GetSettings();

	int   col = COLOR_BLUE;
    int   onoff = -1;
	const char* str = NULL;
	switch (type)
	{
    case SWITCH_SEATS:
        onoff = settings->use_seats;
        break;
    case SWITCH_DRAWER_MODE:
        // Modern enum-based approach
        if (auto mode = vt::IntToEnum<DrawerModeType>(settings->drawer_mode)) {
            str = vt::GetDrawerModeDisplayName(*mode);
            vt::Logger::debug("Drawer mode: {}", str);
        } else {
            str = FindStringByValue(settings->drawer_mode, DrawerModeValue, DrawerModeName);
        }
        break;
    case SWITCH_PASSWORDS:
        str = FindStringByValue(settings->password_mode, PasswordValue, PasswordName);
        break;
    case SWITCH_SALE_CREDIT:
        str = FindStringByValue(settings->sale_credit, SaleCreditValue, SaleCreditName);
        break;
    case SWITCH_DISCOUNT_ALCOHOL:
        onoff = settings->discount_alcohol;
        break;
    case SWITCH_CHANGE_FOR_CHECKS:
        onoff = settings->change_for_checks;
        break;
    case SWITCH_CHANGE_FOR_CREDIT:
        onoff = settings->change_for_credit;
        break;
    case SWITCH_CHANGE_FOR_GIFT:
        onoff = settings->change_for_gift;
        break;
    case SWITCH_CHANGE_FOR_ROOM:
        onoff = settings->change_for_roomcharge;
        break;
    case SWITCH_COMPANY:
        str = FindStringByValue(settings->store, StoreValue, StoreName);
        break;
    case SWITCH_ROUNDING:
        str = FindStringByValue(settings->price_rounding, RoundingValue, RoundingName);
        break;
    case SWITCH_RECEIPT_PRINT:
        // Modern enum-based approach
        if (auto type = vt::IntToEnum<ReceiptPrintType>(settings->receipt_print)) {
            str = vt::GetReceiptPrintDisplayName(*type);
        } else {
            str = FindStringByValue(settings->receipt_print, ReceiptPrintValue, ReceiptPrintName);
        }
        break;
    case SWITCH_DATE_FORMAT:
        // Modern enum-based approach
        if (auto format = vt::IntToEnum<DateFormatType>(settings->date_format)) {
            str = vt::GetDateFormatDisplayName(*format);
        } else {
            str = FindStringByValue(settings->date_format, DateFormatValue, DateFormatName);
        }
        break;
    case SWITCH_NUMBER_FORMAT:
        // Modern enum-based approach
        if (auto format = vt::IntToEnum<NumberFormatType>(settings->number_format)) {
            str = vt::GetNumberFormatDisplayName(*format);
        } else {
            str = FindStringByValue(settings->number_format, NumberFormatValue, NumberFormatName);
        }
        break;
    case SWITCH_MEASUREMENTS:
        str = FindStringByValue(settings->measure_system, MeasureSystemValue, MeasureSystemName);
        break;
    case SWITCH_TIME_FORMAT:
        // Modern enum-based approach  
        if (auto format = vt::IntToEnum<TimeFormatType>(settings->time_format)) {
            str = vt::GetTimeFormatDisplayName(*format);
        } else {
            str = FindStringByValue(settings->time_format, TimeFormatValue, TimeFormatName);
        }
        break;
    case SWITCH_AUTHORIZE_METHOD:
        str = FindStringByValue(settings->authorize_method, AuthorizeValue, AuthorizeName);
        break;
    case SWITCH_24HOURS:
        onoff = settings->always_open;
        break;
    case SWITCH_ITEM_TARGET:
        onoff = settings->use_item_target;
        break;
    case SWITCH_EXPAND_LABOR:
        onoff = term->expand_labor;
        break;
    case SWITCH_HIDE_ZEROS:
        onoff = term->hide_zeros;
        break;
    case SWITCH_SHOW_FAMILY:
        onoff = term->show_family;
        break;
    case SWITCH_GOODWILL:
        onoff = term->expand_goodwill;
        break;
    case SWITCH_MONEY_SYMBOL:
        str = settings->money_symbol.Value();
        break;
    case SWITCH_SHOW_MODIFIERS:
        onoff = settings->show_modifiers;
        break;
    case SWITCH_ALLOW_MULT_COUPON:
        onoff = settings->allow_multi_coupons;
        break;
    case SWITCH_RECEIPT_ALL_MODS:
        onoff = settings->receipt_all_modifiers;
        break;
    case SWITCH_DRAWER_PRINT:
        str = FindStringByValue(settings->drawer_print, DrawerPrintValue, DrawerPrintName);
        break;
    case SWITCH_BALANCE_AUTO_CPNS:
        onoff = settings->balance_auto_coupons;
        break;
    case SWITCH_F3_F4_RECORDING:
        onoff = settings->enable_f3_f4_recording;
        // Set the main button text for F3/F4 recording (very short version)
        str = term->Translate("F3/F4");
        break;
    case SWITCH_AUTO_UPDATE_VT_DATA:
        onoff = settings->auto_update_vt_data;
        break;
    case SWITCH_BUTTON_IMAGES:
        onoff = settings->show_button_images_default;
        break;
    default:
        return RENDER_OKAY;
	}

	// For switches that don't have custom text, set On/Off text
	if (onoff >= 0 && str == NULL)
	{
		if (onoff == 0)
		{
			str = term->Translate("Off");
			col = COLOR_RED;
		}
		else if (onoff == 1)
		{
			str = term->Translate("On");
			col = COLOR_GREEN;
		}
	}
	
	// Special handling for F3/F4 recording switch - ensure onoff is valid
	if (type == SWITCH_F3_F4_RECORDING)
	{
		// Always get the current setting value to ensure it's up to date
		onoff = settings->enable_f3_f4_recording;
		if (onoff < 0)
		{
			onoff = 0; // Default to Off if setting is invalid
		}
	}
	
	// Debug: Log the values for F3/F4 recording switch (commented out for cleaner UI)
	/*
	if (type == SWITCH_F3_F4_RECORDING)
	{
		char debug_str[256];
		snprintf(debug_str, sizeof(debug_str), "F3F4: onoff=%d, setting=%d", 
		         onoff, settings->enable_f3_f4_recording);
		term->RenderText(debug_str, x + 5, y + 5, COLOR_BLUE, FONT_TIMES_14, ALIGN_LEFT);
	}
	*/

	// Render the button text
	if (str)
	{
		// For F3/F4 recording switch, render main text centered and On/Off at bottom
		if (type == SWITCH_F3_F4_RECORDING)
		{
			// Render main text centered (higher up to avoid overlap)
			term->RenderText(str, x + (w / 2), y + (h / 2) - 15, COLOR_BLACK, FONT_TIMES_20B, ALIGN_CENTER);
			
			// Render On/Off status at bottom (lower to avoid overlap)
			if (onoff == 0)
			{
				term->RenderText(term->Translate("Off"), x + (w / 2), y + h - border - 25, COLOR_RED, FONT_TIMES_20B, ALIGN_CENTER);
			}
			else
			{
				// Any non-zero value means "On"
				term->RenderText(term->Translate("On"), x + (w / 2), y + h - border - 25, COLOR_GREEN, FONT_TIMES_20B, ALIGN_CENTER);
			}
		}
		else
		{
			// For other switches, render text at bottom with appropriate color
			term->RenderText(str, x + (w / 2), y + h - border - 18, col, FONT_TIMES_20B, ALIGN_CENTER);
		}
	}

	return RENDER_OKAY;
}

SignalResult SwitchZone::Touch(Terminal *term, int tx, int ty)
{
	FnTrace("SwitchZone::Touch()");
	int no_update = 0;
	Settings *settings = term->GetSettings();
	switch (type)
	{
    case SWITCH_SEATS:
        settings->use_seats ^= 1;
        break;
    case SWITCH_DRAWER_MODE:
        settings->drawer_mode = NextValue(settings->drawer_mode, DrawerModeValue);
        break;
    case SWITCH_PASSWORDS:
        settings->password_mode = NextValue(settings->password_mode, PasswordValue);
        break;
    case SWITCH_SALE_CREDIT:
        settings->sale_credit ^= 1;
        break;
    case SWITCH_DISCOUNT_ALCOHOL:
        settings->discount_alcohol ^= 1;
        break;
    case SWITCH_CHANGE_FOR_CHECKS:
        settings->change_for_checks ^= 1;
        break;
    case SWITCH_CHANGE_FOR_CREDIT:
        settings->change_for_credit ^= 1;
        break;
    case SWITCH_CHANGE_FOR_GIFT:
        settings->change_for_gift ^= 1;
        break;
    case SWITCH_CHANGE_FOR_ROOM:
        settings->change_for_roomcharge ^= 1;
        break;
    case SWITCH_COMPANY:
        settings->store = NextValue(settings->store, StoreValue);
        break;
    case SWITCH_ROUNDING:
        settings->price_rounding = NextValue(settings->price_rounding, RoundingValue);
        break;
    case SWITCH_RECEIPT_PRINT:
        settings->receipt_print = NextValue(settings->receipt_print, ReceiptPrintValue);
        break;
    case SWITCH_DATE_FORMAT:
        settings->date_format = NextValue(settings->date_format, DateFormatValue);
        break;
    case SWITCH_NUMBER_FORMAT:
        settings->number_format = NextValue(settings->number_format, NumberFormatValue);
        break;
    case SWITCH_MEASUREMENTS:
        settings->measure_system = NextValue(settings->measure_system, MeasureSystemValue);
        break;
    case SWITCH_TIME_FORMAT:
        settings->time_format = NextValue(settings->time_format, TimeFormatValue);
        break;
    case SWITCH_AUTHORIZE_METHOD:
        settings->authorize_method = NextValue(settings->authorize_method, AuthorizeValue);
        break;
    case SWITCH_24HOURS:
        settings->always_open ^= 1;
        break;
    case SWITCH_ITEM_TARGET:
        settings->use_item_target ^= 1;
        break;
    case SWITCH_EXPAND_LABOR:
        term->expand_labor ^= 1;
        no_update = 1;
        break;
    case SWITCH_HIDE_ZEROS:
        term->hide_zeros ^= 1;
        no_update = 1;
        break;
    case SWITCH_SHOW_FAMILY:
        term->show_family ^= 1;
        no_update = 1;
        break;
    case SWITCH_GOODWILL:
        term->expand_goodwill ^= 1;
        no_update = 1;
        break;
    case SWITCH_MONEY_SYMBOL:
        settings->money_symbol.Set(NextName(settings->money_symbol.Value(), MoneySymbolName));
        break;
    case SWITCH_SHOW_MODIFIERS:
        settings->show_modifiers ^= 1;
        break;
    case SWITCH_ALLOW_MULT_COUPON:
        settings->allow_multi_coupons ^= 1;
        break;
    case SWITCH_RECEIPT_ALL_MODS:
        settings->receipt_all_modifiers ^= 1;
        break;
    case SWITCH_DRAWER_PRINT:
        settings->drawer_print = NextValue(settings->drawer_print, DrawerPrintValue);
        break;
    case SWITCH_BALANCE_AUTO_CPNS:
        settings->balance_auto_coupons ^= 1;
        break;
    case SWITCH_F3_F4_RECORDING:
        {
            // Debug: Log the setting value before and after toggle
            int old_value = settings->enable_f3_f4_recording;
            
            // Ensure the setting only toggles between 0 and 1
            if (old_value == 0)
                settings->enable_f3_f4_recording = 1;
            else
                settings->enable_f3_f4_recording = 0;
                
            int new_value = settings->enable_f3_f4_recording;
            
            // Force immediate save of the setting
            settings->changed = 1;
            
            // Debug: Print to console (this will show in terminal output)
            printf("F3/F4 Recording: %d -> %d (fixed toggle)\n", old_value, new_value);
        }
        break;
    case SWITCH_AUTO_UPDATE_VT_DATA:
        settings->auto_update_vt_data ^= 1;
        break;
    case SWITCH_BUTTON_IMAGES:
        settings->show_button_images_default ^= 1;
        break;
    default:
        return SIGNAL_IGNORED;
	}

	settings->changed = 1;
	char str[16];
	sprintf(str, "%d", type);
	if (no_update)
		term->Update(UPDATE_SETTINGS, str);
	else
		term->UpdateAllTerms(UPDATE_SETTINGS, str);
	return SIGNAL_OKAY;
}

int SwitchZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    FnTrace("SwitchZone::Update()");
    if (update_message & UPDATE_SETTINGS)
    {
        if (value && atoi(value) != type)
            return 0;
        Draw(term, 1);
    }
    return 0;
}

const char* SwitchZone::TranslateString(Terminal *term)
{
    FnTrace("SwitchZone::TranslateString()");
    int idx = CompareList(type, SwitchValue);
    if (idx < 0)
        return NULL;

    return SwitchName[idx];
}


/*********************************************************************
 * SettingsZone Class
 ********************************************************************/

SettingsZone::SettingsZone()
{
    FnTrace("SettingsZone::SettingsZone()");

    form_header = 0;
    AddNewLine();
    AddTextField("Your Business Name", 32);
    AddTextField("Address", 64);
    AddTextField("City State Zip Code", 64);
    AddTextField("Country Code", 8);
    AddTextField("Location Code", 8);
    AddNewLine();
    Center();
    AddLabel("Set the Life of the Logon ID (Up to 999 seconds)");
    AddNewLine();
    LeftAlign();
    AddTextField("Screen Saver", 5); SetFlag(FF_ONLYDIGITS);
    AddTextField("On the Table Page", 5); SetFlag(FF_ONLYDIGITS);
    AddTextField("After Settlement", 5); SetFlag(FF_ONLYDIGITS);
    AddTextField("On Page One", 5); SetFlag(FF_ONLYDIGITS);
    AddNewLine();
    Center();
    AddLabel("Ledger Accounts");
    AddNewLine();
    LeftAlign();
    AddTextField("Lowest Account Number", 10);  SetFlag(FF_ONLYDIGITS);
    AddTextField("Highest Account Number", 10);  SetFlag(FF_ONLYDIGITS);
    AddListField("Account for expenses paid from drawers", NULL);
    AddNewLine();
    Center();
    AddLabel("Drawer Settings");
    AddNewLine();
    LeftAlign();
    AddListField("Require user to balance drawer in ServerBank mode?", YesNoName, YesNoValue);
    AddNewLine();
    AddTextField("Default Tab Amount", 10);  SetFlag(FF_MONEY);
    AddNewLine();
    Center();
    AddLabel("SMTP Settings");
    AddNewLine();
    LeftAlign();
    AddTextField("SMTP Server for Sending", 50);
    AddNewLine();
    AddTextField("SMTP Reply To Address", 50);
    AddNewLine();
    Center();
    AddLabel("Miscellaneous Settings");
    AddNewLine();
    LeftAlign();
    AddListField("Can select user for expenses?", YesNoName, YesNoValue);
    AddNewLine();
    AddTextField("Minimum Day Length (hours)", 8);  SetFlag(FF_ONLYDIGITS);
    AddNewLine();
    AddListField("FastFood mode for TakeOut orders?", YesNoName, YesNoValue);
    AddNewLine();
    AddListField("Set Your Preferred Report Time Frame", ReportPeriodName, ReportPeriodValue);
    AddListField("Print A Header on Reports?", YesNoName, YesNoValue);
    AddNewLine();
    AddListField("Split Check View", SplitCheckName, SplitCheckValue);
    AddListField("Modifier Separation", ModSeparatorName, ModSeparatorValue);
    AddListField("Start reports at Midnight?", YesNoName, YesNoValue);
    AddListField("Allow Background Icon?", YesNoName, YesNoValue);
    AddNewLine();
    AddListField("Use Embossed Text Effects?", YesNoName, YesNoValue);
    AddListField("Use Text Anti-aliasing?", YesNoName, YesNoValue);
    AddListField("Use Drop Shadows?", YesNoName, YesNoValue);
    AddTextField("Shadow Offset X (pixels)", 5); SetFlag(FF_ONLYDIGITS);
    AddTextField("Shadow Offset Y (pixels)", 5); SetFlag(FF_ONLYDIGITS);
    AddTextField("Shadow Blur Radius (0-10)", 5); SetFlag(FF_ONLYDIGITS);
    AddNewLine();
    AddListField("Button Text Position", ButtonTextPosName, ButtonTextPosValue);
    AddNewLine();
    Center();
    AddLabel("Scheduled Restart Settings");
    AddNewLine();
    LeftAlign();
    AddTextField("Restart Hour (0-23, -1=disabled)", 5); SetFlag(FF_ONLYDIGITS);
    AddTextField("Restart Minute (0-59)", 5); SetFlag(FF_ONLYDIGITS);
    AddNewLine();
    Center();
    AddLabel("Kitchen Video Order Alert Settings");
    AddNewLine();
    LeftAlign();
    AddTextField("Warning Time (minutes)", 5); SetFlag(FF_ONLYDIGITS);
    AddTextField("Alert Time (minutes)", 5); SetFlag(FF_ONLYDIGITS);
    AddTextField("Flash Time (minutes)", 5); SetFlag(FF_ONLYDIGITS);
    AddNewLine();
    AddListField("Warning Color", ColorName, ColorValue);
    AddListField("Alert Color", ColorName, ColorValue);
    AddListField("Flash Color", ColorName, ColorValue);
}

RenderResult SettingsZone::Render(Terminal *term, int update_flag)
{
    FnTrace("SettingsZone::Render()");
    form_header = 0;
    if (name.size() > 0)
        form_header = 1;

    FormZone::Render(term, update_flag);
    TextC(term, 0, name.Value(), color[0]);
    return RENDER_OKAY;
}

int SettingsZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("SettingsZone::LoadRecord()");
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();
    int day_length_hrs = settings->min_day_length / 60 / 60;

    f->Set(settings->store_name); f = f->next;
    f->Set(settings->store_address); f = f->next;
    f->Set(settings->store_address2); f = f->next;
    f->Set(settings->country_code); f = f->next;
    f->Set(settings->store_code); f = f->next;

    f = f->next;  // skip past label
    f->Set(settings->screen_blank_time); f = f->next;
    f->Set(settings->delay_time1); f = f->next;
    f->Set(settings->delay_time2); f = f->next;
    f->Set(settings->start_page_timeout); f = f->next;

    f = f->next;  // skip past label
    f->Set(settings->low_acct_num); f = f->next;
    f->Set(settings->high_acct_num); f = f->next;
    // need to get the list of accounts for this
    Account *acct = term->system_data->account_db.AccountList();
    while (acct != NULL)
    {
        f->AddEntry(acct->name.Value(), acct->number);
        acct = acct->next;
    }
    f->Set(settings->drawer_account); f = f->next;

    f = f->next;  // skip past label
    f->Set(settings->require_drawer_balance); f = f->next;
    f->Set(settings->default_tab_amount); f = f->next;

    f = f->next;  // skip past label
    f->Set(settings->email_send_server); f = f->next;
    f->Set(settings->email_replyto); f = f->next;

    f = f->next;  // skip past label
    f->Set(settings->allow_user_select); f = f->next;
    f->Set(day_length_hrs); f = f->next;
    f->Set(settings->fast_takeouts); f = f->next;
    f->Set(settings->default_report_period); f = f->next;
    f->Set(settings->print_report_header); f = f->next;
    f->Set(settings->split_check_view); f = f->next;
    f->Set(settings->mod_separator); f = f->next;
    f->Set(settings->report_start_midnight); f = f->next;
    f->Set(settings->allow_iconify); f = f->next;
    f->Set(settings->use_embossed_text); f = f->next;
    f->Set(settings->use_text_antialiasing); f = f->next;
    f->Set(settings->use_drop_shadows); f = f->next;
    f->Set(settings->shadow_offset_x); f = f->next;
    f->Set(settings->shadow_offset_y); f = f->next;
    f->Set(settings->shadow_blur_radius); f = f->next;
    f->Set(settings->button_text_position); f = f->next;
    
    f = f->next;  // skip past label
    f->Set(settings->scheduled_restart_hour); f = f->next;
    f->Set(settings->scheduled_restart_min); f = f->next;

    f = f->next;  // skip past label
    f->Set(settings->kv_order_warn_time); f = f->next;
    f->Set(settings->kv_order_alert_time); f = f->next;
    f->Set(settings->kv_order_flash_time); f = f->next;
    f->Set(settings->kv_warn_color); f = f->next;
    f->Set(settings->kv_alert_color); f = f->next;
    f->Set(settings->kv_flash_color); f = f->next;

    return 0;
}

int SettingsZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("SettingsZone::SaveRecord()");
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();
    int day_length_hrs = 0;

    f->Get(settings->store_name); f = f->next;
    f->Get(settings->store_address); f = f->next;
    f->Get(settings->store_address2); f = f->next;
    f->Get(settings->country_code);  f = f->next;
    f->Get(settings->store_code);  f = f->next;

    f = f->next;  // skip past label
    f->Get(settings->screen_blank_time); f = f->next;
    f->Get(settings->delay_time1); f = f->next;
    f->Get(settings->delay_time2); f = f->next;
    f->Get(settings->start_page_timeout); f = f->next;

    f = f->next;  // skip past label
    f->Get(settings->low_acct_num);  f = f->next;
    f->Get(settings->high_acct_num);  f = f->next;
    f->Get(settings->drawer_account);  f = f->next;

    f = f->next;  // skip past label
    f->Get(settings->require_drawer_balance);  f = f->next;
    f->Get(settings->default_tab_amount);  f = f->next;

    f = f->next;  // skip past label
    f->Get(settings->email_send_server);  f = f->next;
    f->Get(settings->email_replyto); f = f->next;

    f = f->next;  // skip past label
    f->Get(settings->allow_user_select);  f = f->next;
    f->Get(day_length_hrs);  f = f->next;
    f->Get(settings->fast_takeouts); f = f->next;
    f->Get(settings->default_report_period); f = f->next;
    f->Get(settings->print_report_header); f = f->next;
    f->Get(settings->split_check_view); f = f->next;
    f->Get(settings->mod_separator); f = f->next;
    f->Get(settings->report_start_midnight); f = f->next;
    f->Get(settings->allow_iconify); f = f->next;
    f->Get(settings->use_embossed_text); f = f->next;
    f->Get(settings->use_text_antialiasing); f = f->next;
    f->Get(settings->use_drop_shadows); f = f->next;
    f->Get(settings->shadow_offset_x); f = f->next;
    f->Get(settings->shadow_offset_y); f = f->next;
    f->Get(settings->shadow_blur_radius); f = f->next;
    f->Get(settings->button_text_position); f = f->next;
    
    f = f->next;  // skip past label
    f->Get(settings->scheduled_restart_hour); f = f->next;
    f->Get(settings->scheduled_restart_min); f = f->next;

    f = f->next;  // skip past label
    f->Get(settings->kv_order_warn_time); f = f->next;
    f->Get(settings->kv_order_alert_time); f = f->next;
    f->Get(settings->kv_order_flash_time); f = f->next;
    f->Get(settings->kv_warn_color); f = f->next;
    f->Get(settings->kv_alert_color); f = f->next;
    f->Get(settings->kv_flash_color); f = f->next;

    settings->min_day_length = day_length_hrs * 60 * 60;  // convert from hours to seconds

    // set the global settings here
    term->system_data->account_db.low_acct_num = settings->low_acct_num;
    term->system_data->account_db.high_acct_num = settings->high_acct_num;

    // argument checking
    int fixed = 0;
    if (settings->screen_blank_time < 0)
        settings->screen_blank_time = 0, fixed = 1;
    if (settings->start_page_timeout < 0)
        settings->start_page_timeout = 0, fixed = 1;
    if (settings->delay_time1 < 0)
        settings->delay_time1 = 0, fixed = 1;
    if (settings->delay_time1 == 1 || settings->delay_time1 == 2)
        settings->delay_time1 = 3, fixed = 1;

    if (settings->delay_time2 < 0)
        settings->delay_time2 = 0, fixed = 1;

    if (fixed)
        Draw(term, 1);
    if (write_file)
        settings->Save();

    MasterControl->SetAllIconify(settings->allow_iconify);

    return 0;
}


/*********************************************************************
 * ReceiptSettingsZone Class
 ********************************************************************/

ReceiptSettingsZone::ReceiptSettingsZone()
{
    FnTrace("ReceiptSettingsZone::ReceiptSettingsZone()");
    form_header = 0;
    AddNewLine();
    Center();
    AddLabel("Receipt Header");
    AddNewLine();
    LeftAlign();
    AddTextField("Line 1", 32);
    AddNewLine();
    AddTextField("Line 2", 32);
    AddNewLine();
    AddTextField("Line 3", 32);
    AddNewLine();
    AddTextField("Line 4", 32);
    AddNewLine();
    AddTextField("Header Length", 5);
    AddListField("Order Number Style", PrintModeName, PrintModeValue);
    AddListField("Table Number Style", PrintModeName, PrintModeValue);
    AddNewLine(2);
    Center();
    AddLabel("Receipt Footer");
    AddNewLine();
    LeftAlign();
    AddTextField("Line 1", 32);
    AddNewLine();
    AddTextField("Line 2", 32);
    AddNewLine();
    AddTextField("Line 3", 32);
    AddNewLine();
    AddTextField("Line 4", 32);
    AddNewLine();
    Center();
    AddLabel("Kitchen Video/Printouts");
    AddNewLine();
    LeftAlign();
    AddListField("Kitchen Video Print Method", KVPrintMethodName, KVPrintMethodValue);
    AddListField("Kitchen Video Show User", YesNoName, YesNoValue);
}

// Member Functions
RenderResult ReceiptSettingsZone::Render(Terminal *term, int update_flag)
{
    FnTrace("ReceiptSettingsZone::Render()");
    form_header = 0;
    if (name.size() > 0)
        form_header = 1;

    FormZone::Render(term, update_flag);
    TextC(term, 0, name.Value(), color[0]);
    return RENDER_OKAY;
}

int ReceiptSettingsZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("ReceiptSettingsZone::LoadRecord()");
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();

    f = f->next;
    f->Set(settings->receipt_header[0]); f = f->next;
    f->Set(settings->receipt_header[1]); f = f->next;
    f->Set(settings->receipt_header[2]); f = f->next;
    f->Set(settings->receipt_header[3]); f = f->next;
    f->Set(settings->receipt_header_length); f = f->next;
    f->Set(settings->order_num_style); f = f->next;
    f->Set(settings->table_num_style); f = f->next;

    f = f->next;  // skip past a label
    f->Set(settings->receipt_footer[0]); f = f->next;
    f->Set(settings->receipt_footer[1]); f = f->next;
    f->Set(settings->receipt_footer[2]); f = f->next;
    f->Set(settings->receipt_footer[3]); f = f->next;

    f = f->next;  // skip past a label
    f->Set(settings->kv_print_method); f = f->next;
    f->Set(settings->kv_show_user); f = f->next;
    return 0;
}

int ReceiptSettingsZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("ReceiptSettingsZone::SaveRecord()");
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();

    f = f->next;
    f->Get(settings->receipt_header[0]); f = f->next;
    f->Get(settings->receipt_header[1]); f = f->next;
    f->Get(settings->receipt_header[2]); f = f->next;
    f->Get(settings->receipt_header[3]); f = f->next;
    f->Get(settings->receipt_header_length); f = f->next;
    f->Get(settings->order_num_style); f = f->next;
    f->Get(settings->table_num_style); f = f->next;

    f = f->next;  // skip past label
    f->Get(settings->receipt_footer[0]); f = f->next;
    f->Get(settings->receipt_footer[1]); f = f->next;
    f->Get(settings->receipt_footer[2]); f = f->next;
    f->Get(settings->receipt_footer[3]); f = f->next;

    f = f->next;  // skip past a label
    f->Get(settings->kv_print_method); f = f->next;
    f->Get(settings->kv_show_user); f = f->next;

    if (write_file)
        settings->Save();
    return 0;
}


/*********************************************************************
 * TaxSettingsZone Class
 ********************************************************************/

TaxSettingsZone::TaxSettingsZone()
{
    FnTrace("TaxSettingsZone::TaxSettingsZone()");
    form_header = 0;

    AddNewLine();
    Center();
    AddLabel("United States Tax Settings");
    LeftAlign();
    AddNewLine();
    AddTextField("Food Sales Tax %", 6);
    AddListField("Prices include tax?", YesNoName, YesNoValue);
    AddNewLine();
    AddTextField("Alcohol Sales Tax %", 6);
    AddListField("Prices include tax?", YesNoName, YesNoValue);
    AddNewLine();
    AddTextField("Room Sales Tax %", 6);
    AddListField("Prices include tax?", YesNoName, YesNoValue);
    AddNewLine();
    AddTextField("Merchandise Sales Tax %", 6);
    AddListField("Prices inc tax?", YesNoName, YesNoValue);
    AddNewLine(2);
    Center();
    AddLabel("Canadian Tax Settings");
    LeftAlign();
    AddNewLine();
    AddTextField("GST %", 6);
    AddTextField("PST %", 6);
    AddTextField("HST %", 6);
    AddTextField("QST %", 6);
    AddNewLine(2);
    Center();
    AddLabel("European Tax Settings");
    LeftAlign();
    AddNewLine();
    AddTextField("VAT %", 6);
    AddNewLine(2);
    Center();
    AddLabel("General Rate Settings");
    LeftAlign();
    AddNewLine();
    AddTextField("Royalty Rate %", 6);
    AddNewLine();
    AddTextField("Advertising Fund %", 6);
}

RenderResult TaxSettingsZone::Render(Terminal *term, int update_flag)
{
    FnTrace("TaxSettingsZone::Render()");
    form_header = 0;
    if (name.size() > 0)
        form_header = 1;

    FormZone::Render(term, update_flag);
    TextC(term, 0, name.Value(), color[0]);
    return RENDER_OKAY;
}

int TaxSettingsZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("TaxSettingsZone::LoadRecord()");
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();

    f = f->next;  // skip US Tax label
    f->Set(settings->tax_food * 100.0); f = f->next;
    f->Set(settings->food_inclusive); f = f->next;
    f->Set(settings->tax_alcohol * 100.0); f = f->next;
    f->Set(settings->alcohol_inclusive); f = f->next;

    f->Set(settings->tax_room * 100.0); f = f->next;
    f->Set(settings->room_inclusive); f = f->next;
    f->Set(settings->tax_merchandise * 100.0); f = f->next;
    f->Set(settings->merchandise_inclusive); f = f->next;

    f = f->next;  // skip Canadian Tax label
    f->Set(settings->tax_GST * 100.0); f = f->next;
    f->Set(settings->tax_PST * 100.0); f = f->next;
    f->Set(settings->tax_HST * 100.0); f = f->next;
    f->Set(settings->tax_QST * 100.0); f = f->next;

    f = f->next;  // skip Euro Tax label
    f->Set(settings->tax_VAT * 100.0); f = f->next;

    f = f->next;  // skip General Rates label
    f->Set(settings->royalty_rate * 100.0); f = f->next;
    f->Set(settings->advertise_fund * 100.0); f = f->next;
    return 0;
}

int TaxSettingsZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("TaxSettingsZone::SaveRecord()");
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();

    f = f->next;  // skip US Tax label
    f->Get(settings->tax_food); f = f->next;        settings->tax_food    *= 0.01;
    f->Get(settings->food_inclusive); f = f->next;
    f->Get(settings->tax_alcohol); f = f->next;     settings->tax_alcohol *= 0.01;
    f->Get(settings->alcohol_inclusive); f = f->next;
    f->Get(settings->tax_room); f = f->next;        settings->tax_room    *= 0.01;
    f->Get(settings->room_inclusive); f = f->next;
    f->Get(settings->tax_merchandise); f = f->next; settings->tax_merchandise *= 0.01;
    f->Get(settings->merchandise_inclusive); f = f->next;

    f = f->next;  // skip Canadian Tax label
    f->Get(settings->tax_GST); f = f->next;         settings->tax_GST *= 0.01;
    f->Get(settings->tax_PST); f = f->next;         settings->tax_PST *= 0.01;
    f->Get(settings->tax_HST); f = f->next;         settings->tax_HST *= 0.01;
    f->Get(settings->tax_QST); f = f->next;         settings->tax_QST *= 0.01;

    f = f->next;  // skip Euro Tax label
    f->Get(settings->tax_VAT); f = f->next;         settings->tax_VAT *= 0.01;

    f = f->next;  // skip General Rates label
    f->Get(settings->royalty_rate); f = f->next;    settings->royalty_rate *= 0.01;
    f->Get(settings->advertise_fund); f = f->next;  settings->advertise_fund *= 0.01;

    // argument checking and validation
    int fixed = 0;
    if (settings->tax_food < 0)
        settings->tax_food = 0, fixed = 1;
    if (settings->tax_alcohol < 0)
        settings->tax_alcohol = 0, fixed = 1;
    if (settings->tax_room < 0)
        settings->tax_room = 0, fixed = 1;
    if (settings->tax_merchandise < 0)
        settings->tax_merchandise = 0, fixed = 1;
    if (settings->tax_GST < 0)
        settings->tax_GST = 0, fixed = 1;
    if (settings->tax_PST < 0)
        settings->tax_PST = 0, fixed = 1;
    if (settings->tax_HST < 0)
        settings->tax_HST = 0, fixed = 1;
    if (settings->tax_QST < 0)
        settings->tax_QST = 0, fixed = 1;
    if (settings->tax_VAT < 0)
        settings->tax_VAT = 0, fixed = 1;
    if (settings->royalty_rate < 0)
        settings->royalty_rate = 0, fixed = 1;
    if (settings->advertise_fund < 0)
        settings->advertise_fund = 0, fixed = 1;

    if (fixed)
        Draw(term, 1);
    if (write_file)
        settings->Save();
    return 0;
}


/*********************************************************************
 * CCSettingsZone Class
 ********************************************************************/

const genericChar* CCNumName[] = { "1234 5678 9012 3456", "xxxx xxxx xxxx 3456", NULL };

CCSettingsZone::CCSettingsZone()
{
    FnTrace("CCSettingsZone::CCSettingsZone()");

    form_header = 0;

    AddNewLine();
    AddTextField("Processing Server", 32);
    AddTextField("Processing Port", 32);
    AddNewLine();
    AddTextField("Merchant ID", 32);
    AddNewLine();
    AddTextField("User name", 15);
    AddTextField("Password", 15);
    AddNewLine();
    AddTextField("Connect Timeout", 10); SetFlag(FF_ONLYDIGITS);
    AddTextField("Amount to Add for PreAuth", 10);  SetFlag(FF_ONLYDIGITS);
    AddNewLine();
    AddListField("Support Credit Cards?", YesNoName, YesNoValue);
    AddListField("Support Debit Cards?", YesNoName, YesNoValue);
    debit_field = FieldListEnd();
    AddListField("Support Gift Cards?", YesNoName, YesNoValue);
    gift_field = FieldListEnd();
    AddNewLine();
    AddListField("Allow PreAuthorizations?", YesNoName, YesNoValue);
    AddListField("Allow auths to be cancelled?", YesNoName, YesNoValue);
    AddNewLine();
    AddListField("Also print a merchant receipt?", YesNoName, YesNoValue);
    AddListField("Also print a cash receipt?", YesNoName, YesNoValue);
    AddListField("Print receipt for PreAuth Complete?", YesNoName, YesNoValue);
    AddListField("Print receipt for Voids?", YesNoName, YesNoValue);
    AddNewLine();
    AddListField("Print Customer Information?", YesNoName, YesNoValue);
    custinfo_field = FieldListEnd();
    AddNewLine();
    AddListField("Automatically authorize on scan?", YesNoName, YesNoValue);
    AddListField("Use Bar mode for PreAuth Completes?", YesNoName, YesNoValue);
    AddListField("How to use card number in memory?", CCNumName, YesNoValue);
    use_field = FieldListEnd();
    AddListField("How to save card number?", CCNumName, YesNoValue);
    save_field = FieldListEnd();
    AddListField("How to display card number?", CCNumName, YesNoValue);
    show_field = FieldListEnd();
}

RenderResult CCSettingsZone::Render(Terminal *term, int update_flag)
{
    FnTrace("CCSettingsZone::Render()");
    RenderResult retval = RENDER_OKAY;

    form_header = 0;
    if (name.size() > 0)
        form_header = 1;

    FormZone::Render(term, update_flag);
    if (name.size() > 0)
        TextC(term, 0, name.Value(), color[0]);

    return retval;
}

int CCSettingsZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("CCSettingsZone::LoadRecord()");
    int retval = 0;
    Settings *settings = term->GetSettings();
    FormField *field = FieldList();

    field->Set(settings->cc_server);           field = field->next;
    field->Set(settings->cc_port);             field = field->next;
    field->Set(settings->cc_merchant_id);      field = field->next;
    field->Set(settings->cc_user);             field = field->next;
    field->Set(settings->cc_password);         field = field->next;
    field->Set(settings->cc_connect_timeout);  field = field->next;
    field->Set(settings->cc_preauth_add);      field = field->next;
    field->Set(settings->CanDoCredit());       field = field->next;
    field->Set(settings->CanDoDebit());        field = field->next;
    field->Set(settings->CanDoGift());         field = field->next;
    field->Set(settings->allow_cc_preauth);    field = field->next;
    field->Set(settings->allow_cc_cancels);    field = field->next;
    field->Set(settings->merchant_receipt);    field = field->next;
    field->Set(settings->cash_receipt);        field = field->next;
    field->Set(settings->finalauth_receipt);   field = field->next;
    field->Set(settings->void_receipt);        field = field->next;
    field->Set(settings->cc_print_custinfo);   field = field->next;
    field->Set(settings->auto_authorize);      field = field->next;
    field->Set(settings->cc_bar_mode);         field = field->next;
    field->Set(settings->use_entire_cc_num);   field = field->next;
    field->Set(settings->save_entire_cc_num);  field = field->next;
    field->Set(settings->show_entire_cc_num);  field = field->next;

    if (settings->use_entire_cc_num)
    {
        save_field->active = 1;
        show_field->active = settings->save_entire_cc_num;
    }
    else
    {
        show_field->active = 0;
        save_field->active = 0;
    }
    if (settings->authorize_method == CCAUTH_CREDITCHEQ)
    {
        debit_field->active = 1;
        gift_field->active = 0;
        custinfo_field->active = 0;
    }
    else
    {
        debit_field->active = 0;
        gift_field->active = 0;
        custinfo_field->active = 1;
    }

    return retval;
}

int CCSettingsZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("CCSettingsZone::SaveRecord()");
    int retval = 0;
    Settings *settings = term->GetSettings();
    FormField *field = FieldList();
    int cancredit;
    int candebit;
    int cangift;

    field->Get(settings->cc_server);           field = field->next;
    field->Get(settings->cc_port);             field = field->next;
    field->Get(settings->cc_merchant_id);      field = field->next;
    field->Get(settings->cc_user);             field = field->next;
    field->Get(settings->cc_password);         field = field->next;
    field->Get(settings->cc_connect_timeout);  field = field->next;
    field->Get(settings->cc_preauth_add);      field = field->next;
    field->Get(cancredit);                     field = field->next;
    field->Get(candebit);                      field = field->next;
    field->Get(cangift);                       field = field->next;
    field->Get(settings->allow_cc_preauth);    field = field->next;
    field->Get(settings->allow_cc_cancels);    field = field->next;
    field->Get(settings->merchant_receipt);    field = field->next;
    field->Get(settings->cash_receipt);        field = field->next;
    field->Get(settings->finalauth_receipt);   field = field->next;
    field->Get(settings->void_receipt);        field = field->next;
    field->Get(settings->cc_print_custinfo);   field = field->next;
    field->Get(settings->auto_authorize);      field = field->next;
    field->Get(settings->cc_bar_mode);         field = field->next;
    field->Get(settings->use_entire_cc_num);   field = field->next;
    field->Get(settings->save_entire_cc_num);  field = field->next;
    field->Get(settings->show_entire_cc_num);  field = field->next;

    if (settings->use_entire_cc_num == 0)
        settings->save_entire_cc_num = 0;
    if (settings->save_entire_cc_num == 0)
        settings->show_entire_cc_num = 0;

    settings->card_types = 0;
    if (cancredit)
        settings->card_types |= CARD_TYPE_CREDIT;
    if (candebit)
        settings->card_types |= CARD_TYPE_DEBIT;
    if (cangift)
        settings->card_types |= CARD_TYPE_GIFT;

    settings->Save();
    MasterControl->SetAllTimeouts(settings->cc_connect_timeout);

    return retval;
}

int CCSettingsZone::UpdateForm(Terminal *term, int record)
{
    FnTrace("CCSettingsZone::UpdateForm()");
    Settings *settings = term->GetSettings();
    int retval = 0;
    int save = 0;

    if (keyboard_focus == use_field)
    {
        use_field->Get(save);
        if (save != settings->use_entire_cc_num)
        {
            settings->use_entire_cc_num = save;
            if (save == 0)
            {
                show_field->active = 0;
                save_field->active = 0;
            }
            else
            {
                save_field->active = 1;
                show_field->active = settings->save_entire_cc_num;
            }
            Draw(term, 1);
        }
    }
    else if (keyboard_focus == save_field)
    {
        save_field->Get(save);
        if (save != settings->save_entire_cc_num)
        {
            settings->save_entire_cc_num = save;
            show_field->active = save;
            Draw(term, 1);
        }
    }

    return retval;
}

/*********************************************************************
 * CCMessageSettingsZone Class
 ********************************************************************/
CCMessageSettingsZone::CCMessageSettingsZone()
{
    FnTrace("CCMessageSettingsZone::CCMessageSettingsZone()");
    int strlength = 50;

    Center();
    AddLabel("Credit Card No Connection Messages");
    AddNewLine();
    LeftAlign();
    AddTextField("Line 1", strlength);
    AddNewLine();
    AddTextField("Line 2", strlength);
    AddNewLine();
    AddTextField("Line 3", strlength);
    AddNewLine();
    AddTextField("Line 4", strlength);
    AddNewLine(2);

    Center();
    AddLabel("Credit Card Voice Authorization Messages");
    AddNewLine();
    LeftAlign();
    AddTextField("Line 1", strlength);
    AddNewLine();
    AddTextField("Line 2", strlength);
    AddNewLine();
    AddTextField("Line 3", strlength);
    AddNewLine();
    AddTextField("Line 4", strlength);
}

RenderResult CCMessageSettingsZone::Render(Terminal *term, int update_flag)
{
    FnTrace("CCMessageSettingsZone::Render()");
    RenderResult retval = RENDER_OKAY;

    form_header = 0;
    if (name.size() > 0)
        form_header = 1;

    FormZone::Render(term, update_flag);
    TextC(term, 0, name.Value(), color[0]);

    return retval;
}

int CCMessageSettingsZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("CCMessageSettingsZone::LoadRecord()");
    int retval = 0;
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();

    f = f->next;
    f->Set(settings->cc_noconn_message1); f = f->next;
    f->Set(settings->cc_noconn_message2); f = f->next;
    f->Set(settings->cc_noconn_message3); f = f->next;
    f->Set(settings->cc_noconn_message4); f = f->next;
    f = f->next;  // skip label
    f->Set(settings->cc_voice_message1); f = f->next;
    f->Set(settings->cc_voice_message2); f = f->next;
    f->Set(settings->cc_voice_message3); f = f->next;
    f->Set(settings->cc_voice_message4); f = f->next;

    return retval;
}

int CCMessageSettingsZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("CCMessageSettingsZone::SaveRecord()");
    int retval = 0;
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();

    f = f->next;
    f->Get(settings->cc_noconn_message1); f = f->next;
    f->Get(settings->cc_noconn_message2); f = f->next;
    f->Get(settings->cc_noconn_message3); f = f->next;
    f->Get(settings->cc_noconn_message4); f = f->next;
    f = f->next;  // skip label
    f->Get(settings->cc_voice_message1); f = f->next;
    f->Get(settings->cc_voice_message2); f = f->next;
    f->Get(settings->cc_voice_message3); f = f->next;
    f->Get(settings->cc_voice_message4); f = f->next;

    if (write_file)
        settings->Save();

    return retval;
}


/*********************************************************************
 * DeveloperZone Class
 ********************************************************************/
DeveloperZone::DeveloperZone()
{
    FnTrace("DeveloperZone::DeveloperZone()");

    phrases_changed = 0;
    AddFields();
}

// Member Functions
int DeveloperZone::AddFields()
{
    FnTrace("DeveloperZone::AddFields()");

    AddTextField("Editor's Password", 9); SetFlag(FF_ONLYDIGITS);
    AddTextField("Minimum Password Length", 2); SetFlag(FF_ONLYDIGITS);
    AddTextField("Multiply", 8);
    AddTextField("Add or Subtract", 5);

    return 0;
}

RenderResult DeveloperZone::Render(Terminal *term, int update_flag)
{
    FnTrace("DeveloperZone::Render()");

    if (phrases_changed < term->system_data->phrases_changed)
    {
        Purge();
        AddFields();
        phrases_changed = term->system_data->phrases_changed;
    }

    if (update_flag)
        clear_flag = 0;

    form_header = 0;
    if (name.size() > 0)
        form_header = 1;

    FormZone::Render(term, update_flag);
    if (name.size() > 0)
        TextC(term, 0, name.Value(), color[0]);
    return RENDER_OKAY;
}

int DeveloperZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("DeveloperZone::LoadRecord()");
    Settings  *settings = term->GetSettings();
    FormField *f = FieldList();

    f->Set(settings->developer_key); f = f->next;
    f->Set(settings->min_pw_len); f = f->next;
    const std::string multiplier_text = FormatMultiplierDisplay(settings->double_mult);
    f->Set(multiplier_text.c_str()); f = f->next;
    f->Set(term->SimpleFormatPrice(settings->double_add)); f = f->next;

    return 0;
}

int DeveloperZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("DeveloperZone::SaveRecord()");
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();

    f->Get(settings->developer_key); f = f->next;
    f->Get(settings->min_pw_len); f = f->next;
    f->Get(settings->double_mult); f = f->next;
    f->GetPrice(settings->double_add); f = f->next;

    int fixed = 0;
    if (settings->shifts_used < 1)
        settings->shifts_used = 1, fixed = 1;
    if (settings->shifts_used > MAX_SHIFTS)
        settings->shifts_used = MAX_SHIFTS, fixed = 1;
    if (settings->double_mult <= 0.0)
        settings->double_mult = 1.0, fixed = 1;

    if (fixed)
        Draw(term, 1);
    if (write_file)
        settings->Save();

    term->system_data->user_db.developer->key = settings->developer_key;
    return 0;
}

SignalResult DeveloperZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("DeveloperZone::Signal()");
    static const genericChar* commands[] = {"clearsystem", "clear system", "clearsystemall",
                                      "clearsystemsome", NULL};
    SimpleDialog *sd = NULL;

    int idx = CompareList(message, commands);
    switch (idx)
    {
    case 0:  // clearsystem
    case 1:  // clear system
        ++clear_flag;
        if (clear_flag >= 10)
        {
            sd = new SimpleDialog(term->Translate("Also clear labor data?"));
            sd->Button("Yes", "clearsystemall");
            sd->Button("No", "clearsystemsome");
            sd->target_zone = this;
            term->OpenDialog(sd);
        }
        return SIGNAL_OKAY;
    case 2:  // clearsystemall
        term->system_data->ClearSystem(1);
        return SIGNAL_OKAY;
    case 3:  // clearsystemsome
        term->system_data->ClearSystem(0);
        return SIGNAL_OKAY;
    default:
        clear_flag = 0;
        return FormZone::Signal(term, message);
    }
}

/* RevenueGroupsZone Class */
RevenueGroupsZone::RevenueGroupsZone()
{
    FnTrace("RevenueGroupsZone::RevenueGroupsZone()");

    phrases_changed = 0;
    AddFields();
}

// Member Functions
int RevenueGroupsZone::AddFields()
{
    FnTrace("RevenueGroupsZone::AddFields()");

    int i = 0;
    for (i = 0; FamilyName[i] != NULL; i++)
    {
        AddListField(MasterLocale->Translate(FamilyName[i]),
                     SalesGroupName, SalesGroupValue);
    }

    return 0;
}

RenderResult RevenueGroupsZone::Render(Terminal *term, int update_flag)
{
    FnTrace("RevenueGroupsZone::Render()");

    if (phrases_changed < term->system_data->phrases_changed)
    {
        Purge();
        AddFields();
        phrases_changed = term->system_data->phrases_changed;
    }

    if (update_flag)
        ; // No clear_flag equivalent needed for this zone

    form_header = 0;
    if (name.size() > 0)
        form_header = 1;

    FormZone::Render(term, update_flag);
    return RENDER_OKAY;
}

int RevenueGroupsZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("RevenueGroupsZone::LoadRecord()");
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();

    int i = 0;
    while (FamilyName[i])
    {
        f->Set(settings->family_group[FamilyValue[i]]);
        f = f->next; ++i;
    }
    return 0;
}

int RevenueGroupsZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("RevenueGroupsZone::SaveRecord()");
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();

    int i = 0;
    while (FamilyName[i])
    {
        f->Get(settings->family_group[FamilyValue[i]]);
        f = f->next; ++i;
    }

    if (write_file)
        settings->Save();

    return 0;
}

/*********************************************************************
 * TenderSetZone Class
 ********************************************************************/
#define CP_TYPE_DOLLAR   0
#define CP_TYPE_PERCENT  1
#define CP_TYPE_SUBST    2

static const char* TS_TypeName[] = {"dollar value", "percent of price", NULL};
static const char* CP_TypeName[] = {"dollar value", "percent of price",
                              "substitute price", NULL};
static int CP_TypeValue[] = { CP_TYPE_DOLLAR, CP_TYPE_PERCENT,
                              CP_TYPE_SUBST, -1 };

TenderSetZone::TenderSetZone()
{
    FnTrace("TenderSetZone::TenderSetZone()");
    list_header = 3;
    page        = 0;
    section     = 0;
    display_id  = 0;

    // Discount Fields
    AddTextField("Customer Discount Name", 20);
    discount_start = FieldListEnd();
    AddListField("Type", TS_TypeName);
    AddTextField("Amount", 7);
    AddNewLine();
    AddListField("Is this discount valid revenue?", YesNoName, YesNoValue);
    AddListField("Can this revenue be taxed?", YesNoName, YesNoValue);
    AddListField("Is this discount exclusive to this store?", YesNoName, YesNoValue);

    // Coupon Fields
    AddTextField("Coupon Name", 20);
    coupon_start = FieldListEnd();
    AddListField("Type", CP_TypeName, CP_TypeValue);
    coupon_type = FieldListEnd();
    AddTextField("Amount", 7);
    AddNewLine();
    AddListField("Is this coupon valid revenue?", YesNoName, YesNoValue);
    AddListField("Can this revenue be taxed?", YesNoName, YesNoValue);
    AddListField("Should this coupon count for royalty payments?", YesNoName, YesNoValue);
    AddListField("Is this coupon exclusive to this store?", YesNoName, YesNoValue);
    AddListField("How to apply coupon to items", CouponApplyName, CouponApplyValue);
    AddNewLine();
    AddListField("Is this coupon automatic?", YesNoName, YesNoValue);
    AddNewLine();
    AddTimeField("Start Time");
    coupon_time_start = FieldListEnd();
    AddDateField("Start Date");
    coupon_date_start = FieldListEnd();
    AddButtonField("Clear", "clearstart");
    AddNewLine();
    AddTimeField("End Time");
    coupon_time_end = FieldListEnd();
    AddDateField("End Date");
    coupon_date_end = FieldListEnd();
    AddWeekDayField("Days of the Week");
    coupon_weekdays = FieldListEnd();
    AddButtonField("Clear", "clearweekday");
    AddNewLine();
    AddListField("Is this coupon item specific?", YesNoName, YesNoValue);
    coupon_item_specific = FieldListEnd();
    AddNewLine();
    AddListField("Item Family", FamilyName, FamilyValue);
    coupon_family = FieldListEnd();
    AddListField("Item", YesNoName, YesNoValue);

    // CreditCard Fields
    AddListField("CreditCard Name", CCTypeName, CCTypeValue);
    creditcard_start = FieldListEnd();
    AddListField("Is this credit card exclusive to this store?", YesNoName, YesNoValue);

    // Comp Fields
    AddTextField("WholeComp Description", 26);
    comp_start = FieldListEnd();
    AddNewLine();
    AddListField("Is this comp valid revenue?", YesNoName, YesNoValue);
    AddListField("Can this revenue be taxed?", YesNoName, YesNoValue);
    AddListField("Will tax be paid from store revenue?", NoYesName, NoYesValue);
    AddListField("Override all comp restrictions?", NoYesName, NoYesValue);
    AddListField("Allow only managers to use this comp?", NoYesName, NoYesValue);
    AddListField("Is this comp exclusive to this store?", YesNoName, YesNoValue);

    // Employee Meal Fields
    AddTextField("Employee Discount Name", 20);
    meal_start = FieldListEnd();
    AddListField("Type", TS_TypeName);
    AddTextField("Amount", 7);
    AddNewLine();
    AddListField("Is this discount valid revenue?", YesNoName, YesNoValue);
    AddListField("Can this revenue be taxed?", YesNoName, YesNoValue);
    AddListField("Override all employee discount restrictions?",
                 NoYesName, NoYesValue);
    AddListField("Allow only managers to use this discount?",
                 NoYesName, NoYesValue);
}

RenderResult TenderSetZone::Render(Terminal *term, int update_flag)
{
    FnTrace("TenderSetZone::Render()");
	if (update_flag == RENDER_NEW)
	{
		page    = 0;
		section = 0;
	}

	int col = color[0];
	ListFormZone::Render(term, update_flag);
	if (show_list)
	{
		switch (section)
		{
        default:
            TextC(term, 0, term->Translate("Customer Discounts"), col);
            TextL(term, 2.3, term->Translate("Name"), col);
            TextR(term, 2.3, term->Translate("Amount"), col);
            break;
        case 1:
            TextC(term, 0, term->Translate("Coupons"), col);
            TextL(term, 2.3, term->Translate("Name"), col);
            TextR(term, 2.3, term->Translate("Amount"), col);
            break;
        case 2:
            TextC(term, 0, term->Translate("Credit/Charge Cards"), col);
            TextL(term, 2.3, term->Translate("Name"), col);
            break;
        case 3:
            TextC(term, 0, term->Translate("Whole Check Comps"), col);
            TextL(term, 2.3, term->Translate("Description"), col);
            break;
        case 4:
            TextC(term, 0, term->Translate("Employee Discounts"), col);
            TextL(term, 2.3, term->Translate("Name"), col);
            TextR(term, 2.3, term->Translate("Amount"), col);
            break;
		}
	}
	else
	{
		switch (section)
		{
        default:
            TextC(term, 0, term->Translate("Edit Customer Discount"), col);
            break;
        case 1:
            TextC(term, 0, term->Translate("Edit Coupon"), col);
            break;
        case 2:
            TextC(term, 0, term->Translate("Edit Credit Card"), col);
            break;
        case 3:
            TextC(term, 0, term->Translate("Edit Comp"), col);
            break;
        case 4:
            TextC(term, 0, term->Translate("Edit Employee Discount"), col);
            break;
		}
	}
	return RENDER_OKAY;
}

SignalResult TenderSetZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("TenderSetZone::Signal()");
	static const genericChar* commands[] = {"section", "clearstart",
                                      "clearend", "clearweekday", NULL};
    SignalResult retval = SIGNAL_OKAY;
    int draw = 1;

	int idx = CompareList(message, commands);
	switch (idx)
	{
    case 0:  // section
        SaveRecord(term, record_no, 0);
        display_id = 0;
        record_no = 0;
        show_list = 1;
        ++section;
        if (section > 4)
            section = 0;
        LoadRecord(term, 0);
        records = RecordCount(term);
        break;
    case 1:  // clearstart
        coupon_time_start->Set((TimeInfo *) NULL);
        coupon_time_end->Set((TimeInfo *) NULL);
        coupon_date_start->Set((TimeInfo *) NULL);
        coupon_date_end->Set((TimeInfo *) NULL);
        break;
    case 2:  // clearend
        coupon_time_start->Set((TimeInfo *) NULL);
        coupon_time_end->Set((TimeInfo *) NULL);
        coupon_date_start->Set((TimeInfo *) NULL);
        coupon_date_end->Set((TimeInfo *) NULL);
        break;
    case 3:  // clearweekday
        coupon_weekdays->Set(0);
        break;

    default:
        retval = ListFormZone::Signal(term, message);
        draw = 0;
        break;
	}

    if (draw)
        Draw(term, 1);

    return retval;
}

int TenderSetZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("TenderSetZone::LoadRecord()");
	Settings  *settings = term->GetSettings();
	FormField *thisForm = FieldList();

	while (thisForm)
	{
		thisForm->active = 0;
		thisForm = thisForm->next;
	}

	switch (section)
	{
    default:  // discounts
    {
        // BAK--> When I create a new record (Discount or whatever) it
        // won't necessarily append at the end of the list.  However,
        // FormZone::NewRecord() (or Signal()) always assumes that the
        // new record will be the last in the list.  So we could
        // either ignore record and do something else, or we can
        // update record any time we add a record.  I chose the latter
        // method here in the hopes that the code will be more
        // consistent and so that I don't miss a bunch of code that
        // expects record to be accurate.
        DiscountInfo *ds;
        if (display_id > 0)
        {
            record = 0;
            ds = settings->DiscountList();
            while (ds != NULL)
            {
                if (display_id == ds->id)
                    break;
                else if (ds->active)
                    record += 1;
                ds = ds->next;
            }
            display_id = 0;
        }
        record_no = record;
        ds = settings->FindDiscountByRecord(record);
        if (ds)
        {
            thisForm = discount_start;
            thisForm->Set(ds->name); thisForm->active = 1; thisForm = thisForm->next;
            if (ds->flags & TF_IS_PERCENT)
                thisForm->Set(1);
            else
                thisForm->Set(0);
            thisForm->active = 1; thisForm = thisForm->next;
            thisForm->Set(term->SimpleFormatPrice(ds->amount)); thisForm->active = 1; thisForm = thisForm->next;
            if (ds->flags & TF_NO_REVENUE)
                thisForm->Set(0);
            else
                thisForm->Set(1);
            thisForm->active = 1; thisForm = thisForm->next;
            if (ds->flags & TF_NO_TAX)
                thisForm->Set(0);
            else
                thisForm->Set(1);
            thisForm->active = 1; thisForm = thisForm->next;
            thisForm->Set(ds->IsLocal());
            thisForm->active = 1;
        }
        return 0;
    }
    case 1:  // coupons
    {
        // see note for discounts
        CouponInfo *cp;
        if (display_id > 0)
        {
            record = 0;
            cp = settings->CouponList();
            while (cp != NULL)
            {
                if (display_id == cp->id)
                    break;
                else if (cp->active)
                    record += 1;
                cp = cp->next;
            }
            display_id = 0;
        }
        record_no = record;
        cp = settings->FindCouponByRecord(record);
        if (cp)
        {
            thisForm = coupon_start;
            thisForm->Set(cp->name); thisForm->active = 1; thisForm = thisForm->next;
            if (cp->flags & TF_IS_PERCENT)
                thisForm->Set(CP_TYPE_PERCENT);
            else if (cp->flags & TF_SUBSTITUTE)
                thisForm->Set(CP_TYPE_SUBST);
            else
                thisForm->Set(CP_TYPE_DOLLAR);
            thisForm->active = 1; thisForm = thisForm->next;
            thisForm->Set(term->SimpleFormatPrice(cp->amount)); thisForm->active = 1; thisForm = thisForm->next;
            if (cp->flags & TF_NO_REVENUE)
                thisForm->Set(0);
            else
                thisForm->Set(1);
            thisForm->active = 1; thisForm = thisForm->next;
            if (cp->flags & TF_NO_TAX)
                thisForm->Set(0);
            else
                thisForm->Set(1);
            thisForm->active = 1; thisForm = thisForm->next;
            if (cp->flags & TF_ROYALTY)
                thisForm->Set(1);
            else
                thisForm->Set(0);
            thisForm->active = 1; thisForm = thisForm->next;
            thisForm->Set(cp->IsLocal());
            thisForm->active = 1; thisForm = thisForm->next;
            if (cp->flags & TF_APPLY_EACH)
                thisForm->Set(COUPON_APPLY_EACH);
            else
                thisForm->Set(COUPON_APPLY_ONCE);
            thisForm->active = 1; thisForm = thisForm->next;
            thisForm->Set(cp->automatic);
            thisForm->active = 1; thisForm = thisForm->next;
            thisForm->Set(cp->start_time);
            thisForm->active = 1; thisForm = thisForm->next;
            thisForm->Set(cp->start_date);
            thisForm->active = 1; thisForm = thisForm->next;
            thisForm->active = 1; thisForm = thisForm->next;  // skip button
            thisForm->Set(cp->end_time);
            thisForm->active = 1; thisForm = thisForm->next;
            thisForm->Set(cp->end_date);
            thisForm->active = 1; thisForm = thisForm->next;
            thisForm->Set(cp->days);
            thisForm->active = 1; thisForm = thisForm->next;
            thisForm->active = 1; thisForm = thisForm->next;  // skip button
            if (cp->flags & TF_ITEM_SPECIFIC)
                thisForm->Set(1);
            else
                thisForm->Set(0);
            thisForm->active = 1; thisForm = thisForm->next;
            // these fields always get set, but will only be displayed
            // (active = 1) if item_specific is true
            // set family and populate the item list
            thisForm->Set(cp->family);
            if (cp->flags & TF_ITEM_SPECIFIC)
                thisForm->active = 1;
            thisForm = thisForm->next;
            // set item id
            ItemList(thisForm, cp->family, cp->item_id);
            if (cp->item_name.empty())
                thisForm->Set(cp->item_id);
            else
                thisForm->SetName(cp->item_name);
            if (cp->flags & TF_ITEM_SPECIFIC)
                thisForm->active = 1;
            thisForm = thisForm->next;
        }
        return 0;
    }
    case 2:  // credit cards
    {
        // see note for discounts
        CreditCardInfo *cc;
        if (display_id > 0)
        {
            record = 0;
            cc = settings->CreditCardList();
            while (cc != NULL)
            {
                if (display_id == cc->id)
                    break;
                else if (cc->active)
                    record += 1;
                cc = cc->next;
            }
            display_id = 0;
        }
        record_no = record;
        cc = settings->FindCreditCardByRecord(record);
        int hold;
        if (cc)
        {
            thisForm = creditcard_start;
            hold = FindValueByString(cc->name.Value(), CCTypeValue, CCTypeName);
            thisForm->Set(hold); thisForm->active = 1; thisForm = thisForm->next;
            thisForm->Set(cc->IsLocal());
            thisForm->active = 1;
        }
        return 0;
    }
    case 3:  // comps
    {
        // see note for discounts
        CompInfo *thisComp;
        if (display_id > 0)
        {
            record = 0;
            thisComp = settings->CompList();
            while (thisComp != NULL)
            {
                if (display_id == thisComp->id)
                    break;
                else if (thisComp->active)
                    record += 1;
                thisComp = thisComp->next;
            }
            display_id = 0;
        }
        record_no = record;
        thisComp = settings->FindCompByRecord(record);
        if (thisComp)
        {
            thisForm = comp_start;
            thisForm->Set(thisComp->name); thisForm->active = 1; thisForm = thisForm->next;
            if (thisComp->flags & TF_NO_REVENUE)
                thisForm->Set(0);
            else
                thisForm->Set(1);
            thisForm->active = 1; thisForm = thisForm->next;
            if (thisComp->flags & TF_NO_TAX)
                thisForm->Set(0);
            else
                thisForm->Set(1);
            thisForm->active = 1; thisForm = thisForm->next;
            if (thisComp->flags & TF_COVER_TAX)
                thisForm->Set(1);
            else
                thisForm->Set(0);
            thisForm->active = 1; thisForm = thisForm->next;
            if (thisComp->flags & TF_NO_RESTRICTIONS)
                thisForm->Set(1);
            else
                thisForm->Set(0);
            thisForm->active = 1; thisForm = thisForm->next;
            if (thisComp->flags & TF_MANAGER)
                thisForm->Set(1);
            else
                thisForm->Set(0);
            thisForm->active = 1; thisForm = thisForm->next;
            thisForm->Set(thisComp->IsLocal());
            thisForm->active = 1;
        }
        return 0;
    }
    case 4:  // Employee Meals
    {
        // see note for discounts
        MealInfo *mi;
        if (display_id > 0)
        {
            record = 0;
            mi = settings->MealList();
            while (mi != NULL)
            {
                if (display_id == mi->id)
                    break;
                else if (mi->active)
                    record += 1;
                mi = mi->next;
            }
            display_id = 0;
        }
        record_no = record;
        mi = settings->FindMealByRecord(record);
        if (mi)
        {
            thisForm = meal_start;
            thisForm->Set(mi->name); thisForm->active = 1; thisForm = thisForm->next;
            if (mi->flags & TF_IS_PERCENT)
                thisForm->Set(1);
            else
                thisForm->Set(0);
            thisForm->active = 1; thisForm = thisForm->next;
            thisForm->Set(term->SimpleFormatPrice(mi->amount)); thisForm->active = 1; thisForm = thisForm->next;
            if (mi->flags & TF_NO_REVENUE)
                thisForm->Set(0);
            else
                thisForm->Set(1);
            thisForm->active = 1; thisForm = thisForm->next;
            if (mi->flags & TF_NO_TAX)
                thisForm->Set(0);
            else
                thisForm->Set(1);
            thisForm->active = 1; thisForm = thisForm->next;
            if (mi->flags & TF_NO_RESTRICTIONS)
                thisForm->Set(1);
            else
                thisForm->Set(0);
            thisForm->active = 1; thisForm = thisForm->next;
            if (mi->flags & TF_MANAGER)
                thisForm->Set(1);
            else
                thisForm->Set(0);
            thisForm->active = 1; thisForm = thisForm->next;
        }
        return 0;
    }
	}
}

int TenderSetZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("TenderSetZone::SaveRecord()");
    Settings *settings = term->GetSettings();
    FormField *f;
    int tmp;
    DiscountInfo *dslist = settings->DiscountList();
    CouponInfo *cplist = settings->CouponList();
    CreditCardInfo *cclist = settings->CreditCardList();
    CompInfo *cmlist = settings->CompList();

    switch (section)
    {
    default: // discounts
    {
        DiscountInfo *ds = settings->FindDiscountByRecord(record);
        if (ds)
        {
            ds->flags = 0;
            f = discount_start;
            f->Get(ds->name); f = f->next;
            f->Get(tmp); f = f->next;
            if (tmp)
                ds->flags |= TF_IS_PERCENT;
            f->GetPrice(ds->amount); f = f->next;
            f->Get(tmp); f = f->next;
            if (!tmp)
                ds->flags |= TF_NO_REVENUE;
            f->Get(tmp); f = f->next;
            if (!tmp)
                ds->flags |= TF_NO_TAX;
            f->Get(ds->local);
            if (ds->name.empty())
            {
                settings->Remove(ds);
            }
            else if (ds->local &&
                     (ds->id >= GLOBAL_MEDIA_ID ||
                      settings->MediaIsDupe(dslist, ds->id, 1)))
            {
                settings->Remove(ds);
                dslist = settings->DiscountList();  // refresh
                if (dslist != NULL)
                    ds->id = settings->MediaFirstID(dslist, 1);
                else
                    ds->id = 1;
                settings->Add(ds);
                record_no = -1;
            }
            else if (!ds->local &&
                     (ds->id < GLOBAL_MEDIA_ID ||
                      settings->MediaIsDupe(dslist, ds->id, 1)))
            {
                settings->Remove(ds);
                dslist = settings->DiscountList();  // refresh
                if (dslist != NULL)
                    ds->id = settings->MediaFirstID(dslist, GLOBAL_MEDIA_ID);
                else
                    ds->id = GLOBAL_MEDIA_ID;
                settings->Add(ds);
                record_no = -1;
            }
        }
        break;
    }
    case 1: // coupons
    {
        CouponInfo *cp = settings->FindCouponByRecord(record);
        if (cp)
        {
            cp->flags = 0;
            f = coupon_start;
            f->Get(cp->name); f = f->next;
            f->Get(tmp); f = f->next;
            if (tmp == CP_TYPE_PERCENT)
                cp->flags |= TF_IS_PERCENT;
            else if (tmp == CP_TYPE_SUBST)
                cp->flags |= TF_SUBSTITUTE;
            f->GetPrice(cp->amount); f = f->next;
            f->Get(tmp); f = f->next;
            if (!tmp)
                cp->flags |= TF_NO_REVENUE;
            f->Get(tmp); f = f->next;
            if (!tmp)
                cp->flags |= TF_NO_TAX;
            f->Get(tmp); f = f->next;
            if (tmp)
                cp->flags |= TF_ROYALTY;
            f->Get(cp->local); f = f->next;
            f->Get(tmp); f = f->next;
            if (tmp == COUPON_APPLY_EACH)
                cp->flags |= TF_APPLY_EACH;
            f->Get(cp->automatic); f = f->next;
            f->Get(cp->start_time); f = f->next;
            f->Get(cp->start_date); f = f->next;
            f = f->next;  // skip button
            f->Get(cp->end_time); f = f->next;
            f->Get(cp->end_date); f = f->next;
            f->Get(cp->days); f = f->next;
            f = f->next;  // skip button
            f->Get(tmp); f = f->next;
            if (tmp == 1)
                cp->flags |= TF_ITEM_SPECIFIC;
            f->Get(cp->family); f = f->next;
            f->GetName(cp->item_name); f = f->next;

            if (cp->name.empty())
            {
                settings->Remove(cp);
            }
            else if (cp->local &&
                     (cp->id >= GLOBAL_MEDIA_ID ||
                      settings->MediaIsDupe(cplist, cp->id, 1)))
            {
                settings->Remove(cp);
                cplist = settings->CouponList();  // refresh
                if (cplist != NULL)
                    cp->id = settings->MediaFirstID(cplist, 1);
                else
                    cp->id = 1;
                settings->Add(cp);
                record_no = -1;
            }
            else if (!cp->local &&
                     (cp->id < GLOBAL_MEDIA_ID ||
                      settings->MediaIsDupe(cplist, cp->id, 1)))
            {
                settings->Remove(cp);
                cplist = settings->CouponList();  // refresh
                if (cplist != NULL)
                    cp->id = settings->MediaFirstID(cplist, GLOBAL_MEDIA_ID);
                else
                    cp->id = 1;
                settings->Add(cp);
                record_no = -1;
            }
        }
        break;
    }
    case 2: // credit cards
    {
        int hold;
        CreditCardInfo *cc = settings->FindCreditCardByRecord(record);
        if (cc)
        {
            f = creditcard_start;
            f->Get(hold);
            cc->name.Set(FindStringByValue(hold, CCTypeValue, CCTypeName));
            f = f->next;
            f->Get(cc->local);
            if (cc->name.empty())
            {
                settings->Remove(cc);
            }
            else if (cc->local &&
                     (cc->id >= GLOBAL_MEDIA_ID ||
                      settings->MediaIsDupe(cclist, cc->id, 1)))
            {
                settings->Remove(cc);
                cclist = settings->CreditCardList();  // refresh
                if (cclist != NULL)
                    cc->id = settings->MediaFirstID(cclist, 1);
                else
                    cc->id = 1;
                settings->Add(cc);
                record_no = -1;
            }
            else if (!cc->local &&
                     (cc->id < GLOBAL_MEDIA_ID ||
                      settings->MediaIsDupe(cclist, cc->id, 1)))
            {
                settings->Remove(cc);
                cclist = settings->CreditCardList();  // refresh
                if (cclist != NULL)
                    cc->id = settings->MediaFirstID(cclist, GLOBAL_MEDIA_ID);
                else
                    cc->id = GLOBAL_MEDIA_ID;
                settings->Add(cc);
                record_no = -1;
            }
        }
        break;
    }
    case 3: // comps
    {
        CompInfo *cm = settings->FindCompByRecord(record);
        if (cm)
        {
            cm->flags = 0;
            f = comp_start;
            f->Get(cm->name); f = f->next;
            f->Get(tmp); f = f->next;
            if (!tmp)
                cm->flags |= TF_NO_REVENUE;
            f->Get(tmp); f = f->next;
            if (!tmp)
                cm->flags |= TF_NO_TAX;
            f->Get(tmp); f = f->next;
            if (tmp)
                cm->flags |= TF_COVER_TAX;
            f->Get(tmp); f = f->next;
            if (tmp)
                cm->flags |= TF_NO_RESTRICTIONS;
            f->Get(tmp); f = f->next;
            if (tmp)
                cm->flags |= TF_MANAGER;
            f->Get(cm->local);
            if (cm->name.empty())
            {
                settings->Remove(cm);
            }
            else if (cm->local &&
                     (cm->id >= GLOBAL_MEDIA_ID ||
                      settings->MediaIsDupe(cmlist, cm->id, 1)))
            {
                settings->Remove(cm);
                cmlist = settings->CompList();  // refresh
                if (cmlist != NULL)
                    cm->id = settings->MediaFirstID(cmlist, 1);
                else
                    cm->id = 1;
                settings->Add(cm);
                record_no = -1;
            }
            else if (!cm->local &&
                     (cm->id < GLOBAL_MEDIA_ID ||
                      settings->MediaIsDupe(cmlist, cm->id)))
            {
                settings->Remove(cm);
                cmlist = settings->CompList();  // refresh
                if (cmlist != NULL)
                    cm->id = settings->MediaFirstID(cmlist, GLOBAL_MEDIA_ID);
                else
                    cm->id = GLOBAL_MEDIA_ID;
                settings->Add(cm);
                record_no = -1;
            }
        }
        break;
    }
    case 4: // employee meals
    {
        MealInfo *mi = settings->FindMealByRecord(record);
        if (mi)
        {
            mi->flags = 0;
            f = meal_start;
            f->Get(mi->name); f = f->next;
            f->Get(tmp); f = f->next;
            if (tmp)
                mi->flags |= TF_IS_PERCENT;
            f->GetPrice(mi->amount); f = f->next;
            f->Get(tmp); f = f->next;
            if (!tmp)
                mi->flags |= TF_NO_REVENUE;
            f->Get(tmp); f = f->next;
            if (!tmp)
                mi->flags |= TF_NO_TAX;
            f->Get(tmp); f = f->next;
            if (tmp)
                mi->flags |= TF_NO_RESTRICTIONS;
            f->Get(tmp); f = f->next;
            if (tmp)
                mi->flags |= TF_MANAGER;
        }
        break;
    }
    }

    if (write_file)
        settings->Save();

    return 0;
}

int TenderSetZone::NewRecord(Terminal *term)
{
    FnTrace("TenderSetZone::NewRecord()");
    Settings *settings = term->GetSettings();
    switch (section)
    {
    default:
    {
        DiscountInfo *newdiscount = new DiscountInfo;
        settings->Add(newdiscount);
        display_id = newdiscount->id;
    }
        break;
    case 1:
    {
        CouponInfo *newcoupon = new CouponInfo;
        settings->Add(newcoupon);
        display_id = newcoupon->id;
    }
        break;
    case 2:
    {
        CreditCardInfo *newcreditcard = new CreditCardInfo;
        settings->Add(newcreditcard);
        display_id = newcreditcard->id;
    }
        break;
    case 3:
    {
        CompInfo *newcomp = new CompInfo;
        settings->Add(newcomp);
        display_id = newcomp->id;
    }
        break;
    case 4:
    {
        MealInfo *newmeal = new MealInfo;
        settings->Add(newmeal);
        display_id = newmeal->id;
    }
        break;
    }
    return 0;
}

int TenderSetZone::KillRecord(Terminal *term, int record)
{
    FnTrace("TenderSetZone::KillRecord()");
    Settings *settings = term->GetSettings();
    switch (section)
    {
    default:
    {
        DiscountInfo *ds = settings->FindDiscountByRecord(record);
        ds->active = 0;
        if (ds->next != NULL)
            display_id = ds->next->id;
        else
            record_no = -1;
        return 0;
    }
    case 1:
    {
        CouponInfo *cp = settings->FindCouponByRecord(record);
        cp->active = 0;
        return 0;
    }
    case 2:
    {
        CreditCardInfo *cc = settings->FindCreditCardByRecord(record);
        cc->active = 0;
        return 0;
    }
    case 3:
    {
        CompInfo *cm = settings->FindCompByRecord(record);
        cm->active = 0;
        return 0;
    }
    case 4:
    {
        MealInfo *mi = settings->FindMealByRecord(record);
        mi->active = 0;
        return 0;
    }
    }
}

int TenderSetZone::ListReport(Terminal *term, Report *r)
{
    FnTrace("TenderSetZone::ListReport()");
    Settings *settings = term->GetSettings();
    switch (section)
    {
    case 1: return settings->CouponReport(term, r);
    case 2: return settings->CreditCardReport(term, r);
    case 3: return settings->CompReport(term, r);
    case 4: return settings->MealReport(term, r);
    }
    return settings->DiscountReport(term, r);
}

int TenderSetZone::RecordCount(Terminal *term)
{
    FnTrace("TenderSetZone::RecordCount()");
    Settings *settings = term->GetSettings();
    switch (section)
    {
    case 1: return settings->CouponCount(ALL_MEDIA, ACTIVE_MEDIA);
    case 2: return settings->CreditCardCount(ALL_MEDIA, ACTIVE_MEDIA);
    case 3: return settings->CompCount(ALL_MEDIA, ACTIVE_MEDIA);
    case 4: return settings->MealCount(ALL_MEDIA, ACTIVE_MEDIA);
    }
    return settings->DiscountCount(ALL_MEDIA, ACTIVE_MEDIA);
}

int TenderSetZone::UpdateForm(Terminal *term, int record)
{
    FnTrace("TenderSetZone::UpdateForm()");
    int retval = 0;
    FormField *field = NULL;
    int is_item_specific = 0;
    int is_active = 0;
    static int last_family = -1;
    int family = 0;
    int item = 0;
    int percent = 0;
    TimeInfo start_time;
    TimeInfo end_time;
    TimeInfo start_date;
    TimeInfo end_date;

    if (keyboard_focus == coupon_item_specific)
    {
        field = coupon_type;
        field->Get(percent);
        field = coupon_item_specific;
        field->Get(is_item_specific);
        field = field->next;
        if (is_item_specific)
            is_active = 1;
        while (field != NULL && field != creditcard_start)
        {
            field->active = is_active;
            field = field->next;
        }
    }
    else if (keyboard_focus == coupon_family)
    {
        field = coupon_family;
        field->Get(family);
        if (last_family != family)
        {
            last_family = family;
            field = field->next;
            field->Get(item);
            ItemList(field, family, item);
        }
    }
    else if (keyboard_focus == coupon_time_start)
    {
        coupon_time_start->Get(start_time);
        coupon_time_end->Get(end_time);
        if (end_time.IsSet() == 0 ||
            end_time <= start_time)
        {
            end_time.Set(start_time);
            end_time += std::chrono::minutes(60);
            coupon_time_end->Set(end_time);
        }
    }
    else if (keyboard_focus == coupon_time_end)
    {
        coupon_time_start->Get(start_time);
        coupon_time_end->Get(end_time);
        if (start_time.IsSet() == 0 ||
            start_time >= end_time)
        {
            start_time.Set(end_time);
            start_time -= std::chrono::minutes(60);
            coupon_time_start->Set(start_time);
        }
    }
    else if (keyboard_focus == coupon_date_start)
    {
        coupon_date_start->Get(start_date);
        coupon_date_end->Get(end_date);
        if (end_date.IsSet() == 0 ||
            end_date < start_date)
        {
            end_date.Set(start_date);
            end_date += date::days(1);
            coupon_date_end->Set(end_date);
        }
    }
    else if (keyboard_focus == coupon_date_end)
    {
        coupon_date_start->Get(start_date);
        coupon_date_end->Get(end_date);
        if (start_date.IsSet() == 0 ||
            start_date > end_date)
        {
            start_date.Set(end_date);
            start_date -= date::days(1);
            coupon_date_start->Set(start_date);
        }
    }

    return retval;
}

int TenderSetZone::ItemList(FormField *itemfield, int family, int item_id)
{
    FnTrace("TenderSetZone::ItemList()");
    int retval = 0;
    ItemDB *items = &MasterSystem->menu;
    SalesItem *item = items->ItemList();

    itemfield->ClearEntries();
    if (items->ItemsInFamily(family) > 0)
    {
        itemfield->AddEntry(ALL_ITEMS_STRING, -1);
        while (item != NULL)
        {
            if (item->family == family)
            {
                itemfield->AddEntry(item->item_name.Value(), item->id);
            }
            item = item->next;
        }
    }
    else
    {
        itemfield->AddEntry(NO_ITEMS_STRING, -1);
    }

    return retval;
}


/*********************************************************************
 * TimeSettingsZone Class
 ********************************************************************/

TimeSettingsZone::TimeSettingsZone()
{
    FnTrace("TimeSettingsZone::TimeSettingsZone()");
    form_header = 10;
    int i;

    for (i = 0; i < 16; ++i)
    {
        shift_start[i] = -1;
        meal_used[i]   =  0;
        meal_start[i]  = -1;
    }

    Color(COLOR_DK_BLUE);
    AddTextField("Set the Number of Customer Activity Time Slices to Analyze", 2);
    AddNewLine();

    genericChar str[256];
    for (i = 0; i < MAX_SHIFTS; ++i)
    {
        sprintf(str, "Start Slice %d at", i+1);
        AddTimeField(str, 1, 0);
    }
    AddNewLine(6);
    Color(COLOR_DK_GREEN);
    int m = 0;
    while (MealStartName[m])
        AddListField(MealStartName[m++], MarkName);
    AddNewLine();

    m = 0;
    while (MealStartName[m])
    {
        sprintf(str, "%s Start", MealStartName[m++]);
        AddTimeField(str, 1, 0);
    }

    Color(COLOR_DEFAULT);
    AddNewLine(2);
    AddListField("Sales Period", SalesPeriodName, SalesPeriodValue, 11.5);
    AddTimeDateField("Start", 1, 0);
    AddNewLine();
    AddListField("Labor Period", SalesPeriodName, SalesPeriodValue, 11.5);
    AddTimeDateField("Start", 1, 0);

    AddNewLine(2);
    AddListField("Overtime After 8 Hours In A Shift?", NoYesName, NoYesValue);
    AddListField("Overtime After 40 Hours In A Week?", NoYesName, NoYesValue);
    AddTimeDayField("Start Of Week For Overtime Calculation", 1, 0);
}

RenderResult TimeSettingsZone::Render(Terminal *term, int update_flag)
{
    FnTrace("TimeSettingsZone::Render()");
    FormZone::Render(term, update_flag);

    // Render schedule bar
    int bx = x + border + 10;
    int by = y + border + 24;
    int bw = w - border*2 - 20;
    int bh = font_height * 6;
    int lx, c = term->TextureTextColor(texture[0]);
    int i;

    Settings *settings = term->GetSettings();
    term->RenderButton(bx, by, bw, bh, ZF_RAISED, IMAGE_SAND);
    for (i = 0; i <= 24; ++i)
    {
        lx = bx + 8 + (((bw - 16) * i) / 24);
        term->RenderText(HourName[i], lx, y + border, c, FONT_TIMES_20, ALIGN_CENTER);
        term->RenderVLine(lx, by + 8, bh - 16, COLOR_BLACK);
    }

    // Show current time
    int minute = (SystemTime.Hour() * 60 + SystemTime.Min());
    lx = bx + 8 + (((bw - 16) * minute) / 1440);
    term->RenderVLine(lx, by, bh, COLOR_DK_RED);
    TextC(term, 8, term->Translate("Current Time"), COLOR_DK_RED);

    // Show shift info
    genericChar str[256];
    for (i = 0; i < 16; ++i)
        if (shift_start[i] >= 0)
        {
            lx = bx + 8 + (((bw - 16) * shift_start[i]) / 1440);
            term->RenderVLine(lx, by + 3, 31, COLOR_DK_BLUE, 3);
            sprintf(str, "%d", i + 1);
            term->RenderText(str, lx, by + (bh / 2) - font_height,
                             COLOR_DK_BLUE, FONT_TIMES_34, ALIGN_CENTER);
        }
    int shift = settings->ShiftNumber(SystemTime);
    if (shift >= 0)
    {
        sprintf(str, "%s: %d", term->Translate("Current Slice"), shift + 1);
        TextPosL(term, 10, 8, str, COLOR_DK_BLUE);
    }

    // Show meal info
    int m = 0;
    while (MealStartName[m])
    {
        if (meal_start[m] >= 0)
        {
            lx = bx + 8 + (((bw - 16) * meal_start[m]) / 1440);
            term->RenderVLine(lx, by + bh - 36, 33, COLOR_DK_GREEN, 3);
            term->RenderText(MealStartName[m], lx + 6, by + bh - 24,
                             COLOR_DK_GREEN, FONT_TIMES_20);
        }
        ++m;
    }

    int meal = settings->MealPeriod(SystemTime);
	//cout << "meal: " << meal << endl;

    if (meal >= 0)
    {
		const char* strMealLabel = FindStringByValue(meal, IndexValue, IndexName, UnknownStr);
        sprintf(str, "%s: %s", term->Translate("Current Index"), term->Translate(strMealLabel));
        TextPosR(term, size_x-10, 8, str, COLOR_DK_GREEN);
    }
    return RENDER_OKAY;
}

int TimeSettingsZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    FnTrace("TimeSettingsZone::Update()");
    if (update_message & UPDATE_MINUTE)
        return Draw(term, 0);
    else
        return 0;
}

int TimeSettingsZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("TimeSettingsZone::LoadRecord()");
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();
    shifts = settings->shifts_used;
    f->Set(shifts); f = f->next;
    int i;

    for (i = 0; i < MAX_SHIFTS; ++i)
    {
        f->active = (i < settings->shifts_used);
        if (f->active)
            shift_start[i] = settings->shift_start[i];
        else
            shift_start[i] = -1;
        f->Set(shift_start[i]);
        f = f->next;
    }

    int m = 0;
    while (MealStartName[m])
    {
        meal_used[m] = settings->meal_active[MealStartValue[m]];
        f->Set(meal_used[m]);
        f = f->next; ++m;
    }

    m = 0;
    while (MealStartName[m])
    {
        f->active = meal_used[m];
        if (f->active)
            meal_start[m] = settings->meal_start[MealStartValue[m]];
        else
            meal_start[m] = -1;
        f->Set(meal_start[m]);
        f = f->next; ++m;
    }

    f->Set(settings->sales_period); f = f->next;
    f->Set(settings->sales_start); f = f->next;
    f->Set(settings->labor_period); f = f->next;
    f->Set(settings->labor_start); f = f->next;

    if (settings->overtime_shift > 0)
        f->Set(1);
    else
        f->Set(0);
    f = f->next;

    if (settings->overtime_week > 0)
        f->Set(1);
    else
        f->Set(0);
    f = f->next;

    f->Set(settings->wage_week_start);
    f = f->next;
    return 0;
}

int TimeSettingsZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("TimeSettingsZone::SaveRecord()");
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();
    f->Get(settings->shifts_used); f = f->next;
    if (settings->shifts_used > MAX_SHIFTS)
        settings->shifts_used = MAX_SHIFTS;
    int i;

    for (i = 0; i < MAX_SHIFTS; ++i)
    {
        f->Get(shift_start[i]);
        if (f->active)
            settings->shift_start[i] = shift_start[i];
        f = f->next;
    }

    int m = 0;
    while (MealStartName[m])
    {
        f->Get(settings->meal_active[MealStartValue[m]]);
        f = f->next; ++m;
    }

    m = 0;
    while (MealStartName[m])
    {
        f->Get(meal_start[m]);
        if (f->active)
            settings->meal_start[MealStartValue[m]] = meal_start[m];
        f = f->next; ++m;
    }

    f->Get(settings->sales_period); f = f->next;
    f->Get(settings->sales_start); f = f->next;
    f->Get(settings->labor_period); f = f->next;
    f->Get(settings->labor_start); f = f->next;

    int tmp = 0;
    f->Get(tmp);
    if (tmp)
        settings->overtime_shift = 8;
    else
        settings->overtime_shift = 0;
    f = f->next;

    f->Get(tmp);
    if (tmp)
        settings->overtime_week = 40;
    else
        settings->overtime_week = 0;
    f = f->next;
    f->Get(settings->wage_week_start);
    f = f->next;

    term->UpdateOtherTerms(UPDATE_MEAL_PERIOD, NULL);
    if (write_file)
        settings->Save();
    return 0;
}

int TimeSettingsZone::UpdateForm(Terminal *term, int record)
{
    FnTrace("TimeSettingsZone::UpdateForm()");
    FormField *f = FieldList();
    f->Get(shifts); f = f->next;
    int i;

    for (i = 0; i < MAX_SHIFTS; ++i)
    {
        f->active = (i < shifts);
        f->Get(shift_start[i]);
        if (f->active)
            shift_start[i] = shift_start[i] % 1440;
        else
            shift_start[i] = -1;
        f = f->next;
    }

    int m = 0;
    while (MealStartName[m])
    {
        f->Get(meal_used[m]);
        f = f->next; ++m;
    }

    m = 0;
    while (MealStartName[m])
    {
        f->active = meal_used[m];
        f->Get(meal_start[m]);
        if (f->active)
            meal_start[m] = meal_start[m] % 1440;
        else
            meal_start[m] = -1;
        f = f->next; ++m;
    }
    return 0;
}

/*********************************************************************
 * TaxSetZone Class
 ********************************************************************/

TaxSetZone::TaxSetZone()
{
    FnTrace("TaxSetZone::TaxSetZone()");
}

RenderResult TaxSetZone::Render(Terminal *term, int update_flag)
{
    FnTrace("TaxSetZone::Render()");
    return ListFormZone::Render(term, update_flag);
}

SignalResult TaxSetZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("TaxSetZone::Signal()");
    return ListFormZone::Signal(term, message);
}

int TaxSetZone::LoadRecord(Terminal *term, int my_record_no)
{
    FnTrace("TaxSetZone::LoadRecord()");
    return 1;
}

int TaxSetZone::SaveRecord(Terminal *term, int my_record_no, int write_file)
{
    FnTrace("TaxSetZone::SaveRecord()");
    return 1;
}

int TaxSetZone::NewRecord(Terminal *term)
{
    FnTrace("TaxSetZone::NewRecord()");
    return 1;
}

int TaxSetZone::KillRecord(Terminal *term, int my_record_no)
{
    FnTrace("TaxSetZone::KillRecord()");
    return 1;
}

int TaxSetZone::ListReport(Terminal *term, Report *r)
{
    FnTrace("TaxSetZone::ListReport()");
    return 1;
}

int TaxSetZone::RecordCount(Terminal *term)
{
    FnTrace("TaxSetZone::RecordCount()");
    return 0;
}

/*********************************************************************
 * MoneySetZone Class
 ********************************************************************/

MoneySetZone::MoneySetZone()
{
    FnTrace("MoneySetZone::MoneySetZone()");
}

RenderResult MoneySetZone::Render(Terminal *term, int update_flag)
{
    FnTrace("MoneySetZone::Render()");
    return ListFormZone::Render(term, update_flag);
}

SignalResult MoneySetZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("MoneySetZone::Signal()");
    return ListFormZone::Signal(term, message);
}

int MoneySetZone::LoadRecord(Terminal *term, int my_record_no)
{
    FnTrace("MoneySetZone::LoadRecord()");
    return 1;
}

int MoneySetZone::SaveRecord(Terminal *term, int my_record_no, int write_file)
{
    FnTrace("MoneySetZone::SaveRecord()");
    return 1;
}

int MoneySetZone::NewRecord(Terminal *term)
{
    FnTrace("MoneySetZone::NewRecord()");
    return 1;
}

int MoneySetZone::KillRecord(Terminal *term, int my_record_no)
{
    FnTrace("MoneySetZone::KillRecord()");
    return 1;
}

int MoneySetZone::ListReport(Terminal *term, Report *r)
{
    FnTrace("MoneySetZone::ListReport()");
    return 1;
}

int MoneySetZone::RecordCount(Terminal *term)
{
    FnTrace("MoneySetZone::RecordCount()");
    return 0;
}


/*********************************************************************
 * ExpireSettingsZone Class
 ********************************************************************/

ExpireSettingsZone::ExpireSettingsZone()
{
    FnTrace("ExpireSettingsZone::ExpireSettingsZone()");
    form_header = 0;
    AddNewLine();
    Center();
    AddLabel("Expire Header");
    AddNewLine();
    LeftAlign();
    AddTextField("Line 1", 32);
    AddNewLine();
    AddTextField("Line 2", 32);
    AddNewLine();
    AddTextField("Line 3", 32);
    AddNewLine();
    AddTextField("Line 4", 32);
}

// Member Functions
RenderResult ExpireSettingsZone::Render(Terminal *term, int update_flag)
{
    FnTrace("ExpireSettingsZone::Render()");
    form_header = 0;
    if (name.size() > 0)
        form_header = 1;

    FormZone::Render(term, update_flag);
    TextC(term, 0, name.Value(), color[0]);
    return RENDER_OKAY;
}

int ExpireSettingsZone::LoadRecord(Terminal *term, int record)
{
    FnTrace("ExpireSettingsZone::LoadRecord()");
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();

    f = f->next;
    f->Set(settings->expire_message1); f = f->next;
    f->Set(settings->expire_message2); f = f->next;
    f->Set(settings->expire_message3); f = f->next;
    f->Set(settings->expire_message4); f = f->next;

    return 0;
}

int ExpireSettingsZone::SaveRecord(Terminal *term, int record, int write_file)
{
    FnTrace("ExpireSettingsZone::SaveRecord()");
    Settings *settings = term->GetSettings();
    FormField *f = FieldList();

    f = f->next;
    f->Get(settings->expire_message1); f = f->next;
    f->Get(settings->expire_message2); f = f->next;
    f->Get(settings->expire_message3); f = f->next;
    f->Get(settings->expire_message4); f = f->next;

    if (write_file)
        settings->Save();
    return 0;
}



