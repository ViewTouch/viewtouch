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
 * printer_zone.cc - revision 36 (10/13/98)
 * Implementation of printer_zone module
 */

#include "printer_zone.hh"
#include "terminal.hh"
#include "printer.hh"
#include "sales.hh"
#include "settings.hh"
#include "labels.hh"
#include "manager.hh"
#include "system.hh"
#include "image_data.hh"
#include "locale.hh"
#include <string.h>
#include "safe_string_utils.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** PrintTargetZone Class ****/
// Constructor
PrintTargetZone::PrintTargetZone()
{
    phrases_changed = 0;

    AddFields();
}

// Member Functions
int PrintTargetZone::AddFields()
{
    FnTrace("PrintTargetZone::AddFields()");

    int z = 0;
    
    // Ensure we don't go beyond the array bounds and handle NULL entries
    while (z < MAX_FAMILIES && FamilyName[z])
    {
        AddListField(MasterLocale->Translate(FamilyName[z++]),
                     PrinterName, PrinterValue);
    }

    return 0;
}

RenderResult PrintTargetZone::Render(Terminal *t, int update_flag)
{
    if (phrases_changed < t->system_data->phrases_changed)
    {
        Purge();
        AddFields();
        phrases_changed = t->system_data->phrases_changed;
    }

    FormZone::Render(t, update_flag);
    TextC(t, 0, "Video & Printer Targets by Family", color[0]);
    return RENDER_OKAY;
}

int PrintTargetZone::LoadRecord(Terminal *t, int record)
{
    Settings *s = t->GetSettings();
    if (s == nullptr)
        return 1;
    FormField *f = FieldList();
    int z = 0;
    
    // Ensure we don't go beyond the array bounds and handle NULL entries
    while (z < MAX_FAMILIES && FamilyName[z] && f)
    {
        f->active = (s->family_group[FamilyValue[z]] != SALESGROUP_NONE);
        f->Set(s->family_printer[FamilyValue[z]]);  // Fixed: use FamilyValue[z] for correct indexing
        f = f->next; ++z;
    }
    return 0;
}

int PrintTargetZone::SaveRecord(Terminal *t, int record, int write_file)
{
    Settings *s = t->GetSettings();
    if (s == nullptr)
        return 1;
    FormField *f = FieldList();
    int z = 0;
    
    // Ensure we don't go beyond the array bounds and handle NULL entries
    while (z < MAX_FAMILIES && FamilyName[z] && f)
    {
        int value;
        f->Get(value);
        
        // Save to both arrays to ensure they match
        s->family_printer[FamilyValue[z]] = value;    // Fixed: use FamilyValue[z] for correct indexing
        s->video_target[FamilyValue[z]] = value;      // Fixed: use FamilyValue[z] for correct indexing
        
        f = f->next; ++z;
    }
    if (write_file)
        s->Save();
    return 0;
}


/**** TermObj Class (for split kitchen) ****/
class TermObj : public ZoneObject
{
public:
    Terminal *term;

    // Constructor
    TermObj(Terminal *t);

    // Member Functions
    bool IsValid(Terminal *t);
    int Render(Terminal *t) override;
};

// Constructor
TermObj::TermObj(Terminal *t)
{
    term = t;
    w = 80;
    h = 80;
}

// check if this terminal is still active (hasn't been deleted)
// t = active terminal list
bool TermObj::IsValid(Terminal *t)
{
    while (t)
    {
    	if (t == term)
	    return true;
	t = t->next;
    }
    return false;		// no longer in the active list
}

// Member Functions
int TermObj::Render(Terminal *t)
{
    if (!IsValid(t->parent->TermList()))
    	return 0;		// inactive, skip it

    int color;
    if (selected)
    {
        t->RenderButton(x, y, w, h, ZF_RAISED, IMAGE_LIT_SAND);
        color = COLOR_BLACK;
    }
    else
    {
        t->RenderButton(x, y, w, h, ZF_RAISED, IMAGE_WOOD);
        color = COLOR_WHITE;
    }

    const std::string str = AdjustCase(term->name.str());
    t->RenderZoneText(str.c_str(), x, y, w, h, color, FONT_TIMES_24B);
    return 0;
}


/**** KitchenObj Class ****/
class KitchenObj : public ZoneObject
{
public:
    ZoneObjectList terms;
    int number;

    // Constructor
    KitchenObj(Control *db, int no);

    // Member Functions
    int Layout(Terminal *t, int lx, int ly, int lw, int lh) override;
    int Render(Terminal *t) override;
};

// Constructor
KitchenObj::KitchenObj(Control *db, int no)
{
    number = no;

    for (Terminal *t = db->TermList(); t != nullptr; t = t->next)
        if (t->kitchen == no)
            terms.Add(new TermObj(t));
}

// Member Functions
int KitchenObj::Layout(Terminal *t, int lx, int ly, int lw, int lh)
{
    SetRegion(lx, ly, lw, lh);

    int width_left  = lw - 10;
    int height_left = lh - 42;
    int width  = width_left  / 2;
    int height = height_left / 4;

    // lay terms out left to right, top to bottom
    ZoneObject *zo = terms.List();
    while (zo)
    {
        if (width > width_left)
        {
            width_left   = w - 10;
            height_left -= height;
            if (height_left <= 0)
                return 1;  // Ran out of room
        }

        zo->Layout(t, x + w - width_left - 4, y + h - height_left, width, height);
        width_left -= width;
        zo = zo->next;
    }
    return 0;
}

int KitchenObj::Render(Terminal *t)
{
    genericChar str[256];
    int color;

    if (number <= 0)
    {
        vt_safe_string::safe_copy(str, 256, GlobalTranslate("Standard Target"));
        color = COLOR_RED;
    }
    else
    {
        vt_safe_string::safe_format(str, 256, "Kitchen #%d", number);
        color = COLOR_BLACK;
    }

    t->RenderButton(x, y, w, h, ZF_RAISED, IMAGE_SAND);
    t->RenderText(str, x + (w/2), y + 12, color, FONT_TIMES_20B,
                  ALIGN_CENTER, w - 8);
    terms.Render(t);
    return 0;
}


/**** SplitKitchenZone Class ****/
// Member Functions
RenderResult SplitKitchenZone::Render(Terminal *t, int update_flag)
{
    RenderZone(t, nullptr, update_flag);
    if (update_flag)
    {
        kitchens.Purge();
        kitchens.Add(new KitchenObj(t->parent, 1)); // Kitchen 1
        kitchens.Add(new KitchenObj(t->parent, 0)); // Unassigned
        kitchens.Add(new KitchenObj(t->parent, 2)); // Kitchen 2
    }

    kitchens.LayoutColumns(t, x + border, y + border,
                           w - (border * 2), h - (border * 2));
    kitchens.Render(t);
    return RENDER_OKAY;
}

SignalResult SplitKitchenZone::Signal(Terminal *t, const genericChar* message)
{
    static const genericChar* commands[] = {"cancel", nullptr};

    int idx = CompareList(message, commands);
    switch (idx)
    {
    case 0: // cancel
    {
        Terminal *term = t->parent->TermList();
        while (term)
        {
            term->kitchen = 0;
            term = term->next;
        }
        Draw(t, 1);
    }
    return SIGNAL_OKAY;
    }
    return SIGNAL_IGNORED;
}

SignalResult SplitKitchenZone::Touch(Terminal *t, int tx, int ty)
{
    FnTrace("SplitKitchenZone::Touch()");
    ZoneObject *ko = kitchens.Find(tx, ty);
    if (ko)
    {
        ZoneObject *to = ((KitchenObj *)ko)->terms.Find(tx, ty);
        if (to)
            to->Touch(t, tx, ty);
        else
            MoveTerms(t, ((KitchenObj *)ko)->number);
        return SIGNAL_OKAY;
    }
    return SIGNAL_IGNORED;
}

// assign all selected (and still active) terminals to kitchen "no"
int SplitKitchenZone::MoveTerms(Terminal *t, int no)
{
    Terminal *tlist = t->parent->TermList();	// active terminals
    ZoneObject *list = kitchens.List();
    while (list)
    {
        ZoneObject *zo = ((KitchenObj *)list)->terms.List();
        while (zo)
        {
            if (zo->selected)
            {
                zo->selected = 0;
		TermObj *to = (TermObj *)zo;
		if (to->IsValid(tlist))
		    to->term->kitchen = no;
            }
            zo = zo->next;
        }
        list = list->next;
    }
    Draw(t, 1);
    return 0;
}

/**** ReceiptSetZone Class ****/
// Constructor
ReceiptSetZone::ReceiptSetZone()
{
    Center();
    AddLabel(GlobalTranslate("Receipt Header"));
    AddNewLine();
    LeftAlign();
    AddTextField("Line 1", 32);
    AddNewLine();
    AddTextField("Line 2", 32);
    AddNewLine();
    AddTextField("Line 3", 32);
    AddNewLine();
    AddTextField("Line 4", 32);
    AddNewLine(2);
    Center();
    AddLabel(GlobalTranslate("Receipt Footer"));
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
}

// Member Functions
RenderResult ReceiptSetZone::Render(Terminal *t, int update_flag)
{
    return FormZone::Render(t, update_flag);
}

int ReceiptSetZone::LoadRecord(Terminal *t, int my_record_no)
{
    Settings *s = t->GetSettings();
    if (s == nullptr)
        return 1;
    FormField *f = FieldList();
    f->Set(s->receipt_header[0]); f = f->next;
    f->Set(s->receipt_header[1]); f = f->next;
    f->Set(s->receipt_header[2]); f = f->next;
    f->Set(s->receipt_header[3]); f = f->next;
    f = f->next;
    f->Set(s->receipt_footer[0]); f = f->next;
    f->Set(s->receipt_footer[1]); f = f->next;
    f->Set(s->receipt_footer[2]); f = f->next;
    f->Set(s->receipt_footer[3]); f = f->next;
    return 0;
}

int ReceiptSetZone::SaveRecord(Terminal *t, int my_record_no, int write_file)
{
    Settings *s = t->GetSettings();
    if (s == nullptr)
        return 1;
    FormField *f = FieldList();
    f->Get(s->receipt_header[0]); f = f->next;
    f->Get(s->receipt_header[1]); f = f->next;
    f->Get(s->receipt_header[2]); f = f->next;
    f->Get(s->receipt_header[3]); f = f->next;
    f = f->next;
    f->Get(s->receipt_footer[0]); f = f->next;
    f->Get(s->receipt_footer[1]); f = f->next;
    f->Get(s->receipt_footer[2]); f = f->next;
    f->Get(s->receipt_footer[3]); f = f->next;

    if (write_file)
        s->Save();
    return 0;
}
