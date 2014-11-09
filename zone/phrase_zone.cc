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
 * phrase_zone.cc - revision 15 (8/31/98)
 * Implementation of PhaseZone class
 */

#include "labels.hh"
#include "locale.hh"
#include "phrase_zone.hh"
#include "system.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** Data & Definitions ****/
#define PAGES 16

static genericChar *PageName[] = {
    "Days of Week", "Abrv. Days of Week", "Months", "Abrv. Months",
    "General", "Greetings", "Statements", "Commands", "Errors",
    "Meal Period Index Names", "Jobs", "Families 1", "Families 2",
    "Card Terms","Card Expressions 1", "Card Expressions 2", NULL };


/**** PhraseZone Class ****/
// Constructor
PhraseZone::PhraseZone()
{
    form_header = 1;
    for (int i = 0; i < 31; ++i)
        AddTextField("", 40, 1, 40);
}

// Member Functions
RenderResult PhraseZone::Render(Terminal *t, int update_flag)
{
    FnTrace("PhraseZone::Render()");

    FormZone::Render(t, update_flag);
    TextL(t, 0, PageName[record_no], color[0]);
    TextR(t, 0, t->PageNo(record_no + 1, PAGES), color[0]);
    return RENDER_OKAY;
}

int PhraseZone::LoadRecord(Terminal *t, int record)
{
    FnTrace("PhraseZone::LoadRecord()");
    Locale *l = MasterLocale;
    FormField *f = FieldList();
    int idx = 0;

    while (f)
    {
        if (PhraseData[idx].page == record)
        {
            f->active = 1;
            f->label.Set(PhraseData[idx].text);
            genericChar *v = l->Translate(PhraseData[idx].text);
            if (v == PhraseData[idx].text)
                f->Set("");
            else
                f->Set(v);
            f = f->next;
        }
        else if (PhraseData[idx].page < 0)
            break;
        ++idx;
    }

    while (f)
    {
        f->active = 0;
        f = f->next;
    }
    return 0;
}

int PhraseZone::SaveRecord(Terminal *t, int record, int write_file)
{
    FnTrace("PhraseZone::SaveRecord()");
    Str tmp;
    Locale *l = MasterLocale;
    FormField *f = FieldList();
    int idx = 0;
    while (f)
    {
        if (PhraseData[idx].page == record)
        {
            f->Get(tmp);
            l->NewTranslation(PhraseData[idx].text, tmp.Value());
            f = f->next;
        }
        else if (PhraseData[idx].page < 0)
            break;
        ++idx;
    }

    if (write_file)
        l->Save();

    if (record == 11 || record == 12)
    {
        t->SendTranslations(FamilyName);
        t->system_data->phrases_changed = time(NULL);
    }

    return 0;
}

int PhraseZone::RecordCount(Terminal *t)
{
    return PAGES;
}
