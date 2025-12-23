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
 * merchant_zone.cc - revision 4 (10/13/98)
 * Implementation of merchant zone module
 */

#include "merchant_zone.hh"
#include "terminal.hh"
#include "settings.hh"
#include "system.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** Module Data ****/
static const genericChar* TimeZoneName[] = {
    "Eastern", "Central", "Mountain", "Pacific", NULL};
static int TimeZoneValue[] = {
    705, 706, 707, 708, -1};

static const genericChar* LanguageName[] = {
    "U.S. English", "Spanish", "Portuguese", NULL};
static int LanguageValue[] = {
    0, 1, 2, -1};

/**** MerchantZone Class ****/
// Constructor
MerchantZone::MerchantZone()
{
    AddTextField(GlobalTranslate("Acquirer BIN"), 6);
    AddTextField(GlobalTranslate("Merchant Number"), 12);
    AddTextField(GlobalTranslate("Store Number"), 4);
    AddTextField(GlobalTranslate("Terminal Number"), 4);
    AddTextField(GlobalTranslate("Currency Code"), 3);
    AddTextField(GlobalTranslate("Country Code"), 3);
    AddTextField(GlobalTranslate("City Code (Zip)"), 9);
    AddListField(GlobalTranslate("Language"), LanguageName, LanguageValue);
    AddListField(GlobalTranslate("Time Zone"), TimeZoneName, TimeZoneValue);
}

// Member Functions
RenderResult MerchantZone::Render(Terminal *t, int update_flag)
{
    form_header = 0;
    if (name.size() > 0)
        form_header = 1;

    FormZone::Render(t, update_flag);
    if (name.size() > 0)
        TextC(t, 0, name.Value(), color[0]);
    return RENDER_OKAY;
}

int MerchantZone::LoadRecord(Terminal *t, int my_record_no)
{
    Settings *s = t->GetSettings();
    FormField *f = FieldList();
    f->Set(s->visanet_acquirer_bin); f = f->next;
    f->Set(s->visanet_merchant); f = f->next;
    f->Set(s->visanet_store); f = f->next;
    f->Set(s->visanet_terminal); f = f->next;
    f->Set(s->visanet_currency); f = f->next;
    f->Set(s->visanet_country); f = f->next;
    f->Set(s->visanet_city); f = f->next;
    f->Set(s->visanet_language); f = f->next;
    f->Set(s->visanet_timezone); f = f->next;
    return 0;
}

int MerchantZone::SaveRecord(Terminal *t, int my_record_no, int write_file)
{
    Settings *s = t->GetSettings();
    FormField *f = FieldList();
    f->Get(s->visanet_acquirer_bin); f = f->next;
    f->Get(s->visanet_merchant); f = f->next;
    f->Get(s->visanet_store); f = f->next;
    f->Get(s->visanet_terminal); f = f->next;
    f->Get(s->visanet_currency); f = f->next;
    f->Get(s->visanet_country); f = f->next;
    f->Get(s->visanet_city); f = f->next;
    f->Get(s->visanet_language); f = f->next;
    f->Get(s->visanet_timezone); f = f->next;
    return 0;
}
