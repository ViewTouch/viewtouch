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
 * form_zone.cc - revision 117 (10/13/98)
 * Implementation of FormZone module
 */

#include "form_zone.hh"
#include "locale.hh"
#include "terminal.hh"
#include "manager.hh"
#include "settings.hh"
#include "admission.hh"

#include <cctype>
#include <cstring>
#include <Xm/Xm.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** Types ****/
class LabelField : public FormField
{
public:
    Flt min_width;

    // Constructor
    LabelField();
    LabelField(const genericChar* lbl, Flt width);

    // Member Functions
    int          Init(Terminal *term, FormZone *fzone);
    RenderResult Render(Terminal *term, FormZone *fzone);
};

class SubmitField : public FormField
{
public:
    Flt min_width;

    SubmitField(const genericChar* lbl, Flt width);
    // Member Functions
    int          Init(Terminal *term, FormZone *fzone);
    RenderResult Render(Terminal *term, FormZone *fzone);
    SignalResult Keyboard(Terminal *term, FormZone *fzone, int key, int state);
    SignalResult Mouse(Terminal *term, FormZone *fzone, int action, Flt mx, Flt my);
};

class TextField : public FormField
{
public:
    Str buffer;
    int buffint;  // for FF_MONEY and FF_ONLYDIGITS
    int max_buffer_len;
    Flt label_width;
    Flt min_label_width;
    int cursor;

    // Constructor
    TextField(const genericChar* lbl, int max_entry, int mod, Flt min_label);

    // Member Functions
    int          Init(Terminal *term, FormZone *fzone);
    RenderResult Render(Terminal *term, FormZone *fzone);
    SignalResult Keyboard(Terminal *term, FormZone *fzone, int key, int state);

    int Set(const genericChar* v);
    int Set(Str  &v);
    int Set(int   v);
    int Set(Flt   v);
    int SetNumRange(int lo, int hi);

    int InsertStringAtCursor(genericChar* my_string); // does the append/insert for strings
    int InsertDigits(int digits, int num = 1);  // does append for digits
    int Append(genericChar* my_string);  // these functions determine what to append
    int Append(Str &my_string);   // and whether the append should take place
    int Append(int val);       // they append after cursor, not at the end
    int Append(Flt val);       // of the string (using InsertStringAtCursor()
    int Append(genericChar key); // or InsertDigits())

    int Remove(int num = 1);   // removes the last genericChar (backspace) or num chars
    int Clear();  // sets buffer back to empty string

    int Get(genericChar* v, int len);
    int Get(genericChar* v);
    int Get(Str &v);
    int Get(int &v);
    int Get(Flt &v);
    int GetPrice(int &v)      { v = ParsePrice(buffer.Value()); return 0;}
    void Print(void);         // debug function to print the value to the screen
};

class TimeDateField : public FormField
{
public:
    TimeInfo buffer;
    TimeInfo upper_bounds;
    TimeInfo lower_bounds;
    int      cursor;
    int      can_unset;
    int      show_time;

    // Constructor
    TimeDateField(const genericChar* lbl, int mod, int can_unset);

    // Member Functions
    int          Init(Terminal *term, FormZone *fzone);
    RenderResult Render(Terminal *term, FormZone *fzone);
    SignalResult Keyboard(Terminal *term, FormZone *fzone, int key, int state);
    SignalResult Mouse(Terminal *term, FormZone *fzone, int action, Flt mx, Flt my);

    int Set(TimeInfo *tinfo);
    int Set(TimeInfo &tinfo) { buffer = tinfo; return 0; }
    int Get(TimeInfo &tinfo) { tinfo = buffer; return 0; }
    int IsSet() { return buffer.IsSet(); }
};

class TimeDayField : public FormField
{
public:
    int day;
    int hour;
    int min;
    int cursor;
    int show_day;
    int can_unset;
    int is_unset;

    // Constructor
    TimeDayField(const genericChar* lbl, int mod, int can_unset);

    // Member Functions
    int          Init(Terminal *term, FormZone *fzone);
    RenderResult Render(Terminal *term, FormZone *fzone);
    SignalResult Keyboard(Terminal *term, FormZone *fzone, int key, int state);
    SignalResult Mouse(Terminal *term, FormZone *fzone, int action, Flt mx, Flt my);

    int Set(TimeInfo &tinfo);
    int Set(TimeInfo *tinfo);
    int Set(int minutes);
    int Get(TimeInfo &tinfo);
    int Get(int &minutes);
};

class WeekDayField : public FormField
{
public:
    int days;
    int current;

    WeekDayField(const char* lbl, int mod = 1);
    int          Init(Terminal *term, FormZone *fzone);
    RenderResult Render(Terminal *term, FormZone *fzone);
    SignalResult Keyboard(Terminal *term, FormZone *fzone, int key, int state);
    SignalResult Mouse(Terminal *term, FormZone *fzone, int action, Flt mx, Flt my);

    int Set(int d);
    int Get(int &d);
};

class ListFieldEntry
{
public:
    ListFieldEntry *next;
    ListFieldEntry *fore;
    Str label;
    int value;
    int active;

    // Constructor
    ListFieldEntry(const genericChar* lbl, int val);
};

class ListField : public FormField
{
    DList<ListFieldEntry> entry_list;

public:
    ListFieldEntry *current;
    Flt min_label_width;
    Flt label_width;
    Flt min_entry_width;
    Flt entry_width;
    int light_up;  // boolean - hilight button for nonzero option?

    // Constructor
    ListField( const genericChar* lbl, const genericChar** options, int* values, Flt min_label, Flt min_list );
    // Destructor
    ~ListField();

    // Member Functions
    ListFieldEntry *EntryList()    { return entry_list.Head(); }
    ListFieldEntry *EntryListEnd() { return entry_list.Tail(); }
    int             EntryCount()   { return entry_list.Count(); }

    int          Init(Terminal *term, FormZone *fzone);
    RenderResult Render(Terminal *term, FormZone *fzone);
    SignalResult Keyboard(Terminal *term, FormZone *fzone, int key, int state);
    SignalResult Touch(Terminal *term, FormZone *fzone, Flt tx, Flt ty);
    SignalResult Mouse(Terminal *term, FormZone *fzone, int action, Flt mx, Flt my);

    int NextEntry(int loop = 0);
    int ForeEntry(int loop = 0);

    int Set(int  v);
    int Get(int &v);
    int SetName(Str &set_name);
    int GetName(Str &get_name);
    int SetList(const genericChar** option_list, int* value_list );
    int SetActiveList(int *list);
    int Add(ListFieldEntry *lfe);
    int ClearEntries();

    int AddEntry(const genericChar* name, int val) {
        return Add(new ListFieldEntry(name, val)); }
    void Print(void);
};

class ButtonField : public FormField
{
public:
    Flt label_width;
    Str message;
    int lit;

    // Constructor
    ButtonField(const genericChar* lbl, const genericChar* msg);

    // Member Functions
    int          Init(Terminal *term, FormZone *fzone);
    RenderResult Render(Terminal *term, FormZone *fzone);
    SignalResult Touch(Terminal *term, FormZone *fzone, Flt tx, Flt ty);
    SignalResult Keyboard(Terminal *term, FormZone *fzone, int key, int state);
};

class TemplateField : public FormField
{
public:
    Flt label_width;
    Flt min_label_width;
    Str buffer;
    Str temp;
    int cursor;

    // Constructor
    TemplateField(const genericChar* lbl, const genericChar* tem, Flt min_label);

    // Member Functions
    int          Init(Terminal *term, FormZone *fzone);
    RenderResult Render(Terminal *term, FormZone *fzone);
    SignalResult Keyboard(Terminal *term, FormZone *fzone, int key, int state);

    int Set(const genericChar* v) { return buffer.Set(v); }
    int Set(Str  &v)        { return buffer.Set(v); }
    int Get(Str &v)         { v.Set(buffer); return 0; }
    int Get(genericChar* v) { strcpy(v, buffer.Value()); return 0; }
};


/**** Functions ****/
int TemplateBlanks(const genericChar* temp)
{
    int blanks = 0;
    const genericChar* term = temp;
    while (*term)
    {
        if (*term == '_')
            ++blanks;
        ++term;
    }
    return blanks;
}

genericChar* FillTemplate(genericChar* temp, const genericChar* str)
{
    static genericChar buffer[STRLENGTH];

    genericChar* term = temp;
    genericChar* b = buffer;
    const genericChar* s = str;
    while (*term)
    {
        if (*term != '_')
            *b = *term;
        else if (*s)
            *b = *s++;
        else
            *b = '_';
        ++b;
        ++term;
    }

    *b = '\0';
    return buffer;
}

int TemplatePos(const genericChar* temp, int cursor)
{
    const genericChar* term = temp;
    int pos = 0;
    while (*term)
    {
        if (*term == '_')
        {
            --cursor;
            if (cursor < 0)
                return pos;
        }
        ++term;
        ++pos;
    }
    return pos;
}


/**** FormZone Class ****/
// Constructor
FormZone::FormZone()
{
    keyboard_focus = nullptr;
    record_no      = 0;
    keep_focus     = 1;
    wrap           = 1;
    records        = 1;
    form_header    = 1;
    form_spacing   = 1.0;
    no_line        = 0;
    current_align  = ALIGN_LEFT;
    current_color  = COLOR_DEFAULT;
}

// Member Functions
RenderResult FormZone::Render(Terminal *term, int update_flag)
{
    FnTrace("FormZone::Render()");

    records = RecordCount(term);
    if (update_flag == RENDER_NEW)
    {
        record_no = 0;
        if (records > 0)
            LoadRecord(term, 0);
    }

    if (update_flag || keep_focus == 0)
        keyboard_focus = nullptr;
	

    LayoutZone::Render(term, update_flag);

    if (!no_line)
    {
        Flt tl = form_header;
        if (tl < 0)
            tl += size_y;

        if (tl > 0)
            Line(term, tl + .1, color[0]);
    }

    if (records <= 0)
        return RENDER_OKAY;

    LayoutForm(term);

    for (FormField *f = FieldList(); f != nullptr; f = f->next)
    {
        f->selected = (keyboard_focus == f);
        if (f->active)
            f->Render(term, this);
    }
    return RENDER_OKAY;
}

SignalResult FormZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("FormZone::Signal()");
    static const genericChar* commands[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "00", ".",
        "backspace", "clear", "new",  "search", "nextsearch ", "restore",
         "next", "prior", "save", "delete", "print", "unfocus", nullptr};
    Employee *e = term->user;
    Printer  *p = term->FindPrinter(PRINTER_REPORT);
    int idx = CompareListN(commands, message);

    // initial decisions; put these here so we don't have to duplicate them for
    // various cases below
    if (keyboard_focus == nullptr && idx < 14)
    {
        // don't handle numeric keypad if we don't have any fields selected
        return SIGNAL_IGNORED;
    }
    else if (idx != 14 && records <= 0)
    {
        // only handle "new" messages if we have no records
        return SIGNAL_IGNORED;
    }

    switch (idx)
    {
    case 0: case 1: case 2: case 3: case 4:  // numbers from a keypad
    case 5: case 6: case 7: case 8: case 9:
        keyboard_focus->Append(message[0]);
        break;
    case 10: // 00
        keyboard_focus->Append(message);
        break;
    case 11: // .
        keyboard_focus->Append(message);
        break;
    case 12: // backspace
        keyboard_focus->Remove(1);
        break;
    case 13: // clear
        keyboard_focus->Clear();
        break;
    case 14:  // New
        if (records > 0)
            SaveRecord(term, record_no, 0);
        record_no = records;
        if (NewRecord(term))
            return SIGNAL_IGNORED;
        records = RecordCount(term);
        if (record_no >= records)
            record_no = records - 1;
        LoadRecord(term, record_no);
        FirstField();
        break;
    case 15:  // Search
        if (Search(term, -1, &message[7]) <= 0)
            return SIGNAL_IGNORED;
        break;
    case 16:  // Next Search
        if (Search(term, record_no, &message[11]) <= 0)
            return SIGNAL_IGNORED;
        break;
    case 17:  // Restore
        if (records > 0)
            LoadRecord(term, record_no);
        break;
    case 18:  // Next
        SaveRecord(term, record_no, 0);
        ++record_no;
        if (record_no >= records)
            record_no = 0;
        if (records >= 0)
            LoadRecord(term, record_no);
        break;
    case 19:  // Prior
        SaveRecord(term, record_no, 0);
        --record_no;
        if (record_no < 0)
            record_no = records - 1;
        if (record_no < 0)
            record_no = 0;
        else
            LoadRecord(term, record_no);
        break;
    case 20:  // Save
        SaveRecord(term, record_no, 1);
        break;
    case 21:  // Delete
        if (KillRecord(term, record_no))
            return SIGNAL_IGNORED;
        records = RecordCount(term);
        if (record_no >= records)
            record_no = records - 1;
        if (record_no < 0)
            record_no = 0;
        else
            LoadRecord(term, record_no);
        break;
    case 22:  // Print
        if (p == nullptr || e == nullptr)
            return SIGNAL_IGNORED;
        SaveRecord(term, record_no, 0);
        if (PrintRecord(term, record_no))
            return SIGNAL_IGNORED;
        return SIGNAL_OKAY;
    case 23:  // Unfocus
        break;
    default:
        if (strlen(message) == 1)
        {  // adding a letter
            keyboard_focus->Append(message);
            idx = 0;  // Make sure focus doesn't get lost
        }
        else
            return SIGNAL_IGNORED;

        if (idx > 0 && records > 0)
            LoadRecord(term, record_no);
        break;
    }

    if (idx < 14) // if we have keyboard input, we don't want to unfocus the field
        keyboard_focus->Draw(term, this);
    else
        Draw(term, 1);  // also removes keyboard focus
    return SIGNAL_OKAY;
}

SignalResult FormZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("FormZone::Touch()");
    if (records <= 0)
        return SIGNAL_IGNORED;

    LayoutZone::Touch(term, tx, ty);

    keyboard_focus = Find(selected_x, selected_y);
    if (keyboard_focus &&
        keyboard_focus->Touch(term, this, selected_x, selected_y) == SIGNAL_OKAY)
    {
        UpdateForm(term, record_no);
    }

    Draw(term, 0);
    return SIGNAL_OKAY;
}

SignalResult FormZone::Mouse(Terminal *term, int action, int mx, int my)
{
    FnTrace("FormZone::Mouse()");
    if (records <= 0 || !(action & MOUSE_PRESS))
        return SIGNAL_IGNORED;

    LayoutZone::Touch(term, mx, my);

    keyboard_focus = Find(selected_x, selected_y);
    if (keyboard_focus)
    {
        if (keyboard_focus->Mouse(term, this, action, selected_x, selected_y) == SIGNAL_OKAY)
        {
            UpdateForm(term, record_no);
        }
        Draw(term, 0);
        return SIGNAL_OKAY;
    }
    return Touch(term, mx, my);
}

SignalResult FormZone::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("FormZone::Keyboard()");
    if (records <= 0)
        return SIGNAL_IGNORED;

    if (this != term->previous_zone)
        return SIGNAL_IGNORED;

    switch (my_key)
    {
    case 9:  // Tab
        if (state & ShiftMask)
            ForeField();
        else
            NextField();
        Draw(term, 0);
        return SIGNAL_OKAY;
    case 16:  // ^P - page up
        return Signal(term, "prior");
    case 13: // Enter
        NextField();
        Draw(term, 0);
        return SIGNAL_OKAY;
    case 14:  // ^N - page down
        return Signal(term, "next");
    }

    if (keyboard_focus == nullptr)
        return SIGNAL_IGNORED;

    switch (my_key)
    {
    case 27: // Escape
        Draw(term, 1);
        return SIGNAL_OKAY;
    default:
        if (keyboard_focus->Keyboard(term, this, my_key, state) == SIGNAL_OKAY)
        {
            if (update)
                Draw(term, 0);
            else if (UpdateForm(term, record_no))
                keyboard_focus->Draw(term, this);
            else
                Draw(term, 0);
        }
        return SIGNAL_OKAY;
    }
}

int FormZone::Add(FormField *fe)
{
    FnTrace("FormZone::Add()");
    if (fe == nullptr)
        return 1; // Error

    field_list.AddToTail(fe);
    fe->align = current_align;
    fe->color = current_color;
    return 0;
}

int FormZone::AddLabel(const genericChar* label, Flt min_width)
{
    FnTrace("FormZone::AddLabel()");
    return Add(new LabelField(label, min_width));
}

int FormZone::AddSubmit(const genericChar* label, Flt min_width)
{
    FnTrace("FormZone::AddSubmit()");
    return Add(new SubmitField(label, min_width));
}

int FormZone::AddTextField(const genericChar* label, int max_len, int mod, Flt min_label)
{
    FnTrace("FormZone::AddTextField()");
    return Add(new TextField(label, max_len, mod, min_label));
}

int FormZone::AddTimeDateField(const genericChar* label, int mod, int can_unset)
{
    FnTrace("FormZone::AddTimeDateField()");
    return Add(new TimeDateField(label, mod, can_unset));
}

int FormZone::AddDateField(const genericChar* label, int mod, int can_unset)
{
    FnTrace("FormZone::AddDateField()");
    TimeDateField *tf = new TimeDateField(label, mod, can_unset);
    tf->show_time = 0;
    return Add(tf);
}

int FormZone::AddTimeField(const genericChar* label, int mod, int can_unset)
{
    FnTrace("FormZone::AddTimeField()");
    TimeDayField *wf = new TimeDayField(label, mod, can_unset);
    wf->show_day = 0;
    return Add(wf);
}

int FormZone::AddTimeDayField(const genericChar* label, int mod, int can_unset)
{
    FnTrace("FormZone::AddTimeDayField()");
    return Add(new TimeDayField(label, mod, can_unset));
}

int FormZone::AddWeekDayField(const genericChar* label, int mod)
{
    FnTrace("FormZone::AddWeekDayField()");
    return Add(new WeekDayField(label, mod));
}

int FormZone::AddListField(const genericChar* label, const genericChar* *item_array,
                           int *value_array, Flt min1, Flt min2)
{
    FnTrace("FormZone::AddListField()");
    return Add(new ListField(label, item_array, value_array, min1, min2));
}

int FormZone::AddButtonField(const genericChar* label, const genericChar* message)
{
    FnTrace("FormZone::AddButtonField()");
    return Add(new ButtonField(label, message));
}

int FormZone::AddTemplateField(const genericChar* label, const genericChar* temp, Flt min_label)
{
    FnTrace("FormZone::AddTemplateField()");
    return Add(new TemplateField(label, temp, min_label));
}

int FormZone::AddNewLine(int lines)
{
    FnTrace("FormZone::AddNewLine()");
    if (FieldListEnd())
        FieldListEnd()->new_line += lines;
    return 0;
}

int FormZone::Color(int c)
{
    FnTrace("FormZone::Color()");
    current_color = c;
    return 0;
}

int FormZone::AddSpace(Flt s)
{
    FnTrace("FormZone::AddSpace()");
    if (FieldListEnd())
        FieldListEnd()->pad += s;
    return 0;
}

int FormZone::SetFlag(int flag)
{
    FnTrace("FormZone::SetFlag()");
    if (FieldListEnd())
        FieldListEnd()->flag = flag;
    return 0;
}

int FormZone::SetNumRange(int lo, int hi)
{
    FnTrace("FormZone::SetNumRange()");
    int retval = 1;

    if (FieldListEnd())
        retval = FieldListEnd()->SetNumRange(lo, hi);
    return retval;
}

int FormZone::Center()
{
    FnTrace("FormZone::Center()");
    current_align = ALIGN_CENTER;
    return 0;
}

int FormZone::LeftAlign()
{
    FnTrace("FormZone::LeftAlign()");
    current_align = ALIGN_LEFT;
    return 0;
}

int FormZone::RightAlign()
{
    FnTrace("FormZone::RightAlign()");
    current_align = ALIGN_RIGHT;
    return 0;
}

int FormZone::Remove(FormField *f)
{
    FnTrace("FormZone::Remove()");
    if (f == nullptr)
        return 1;

    return field_list.Remove(f);
}

void FormZone::Purge()
{
    FnTrace("FormZone::Purge()");
    field_list.Purge();
}

int FormZone::LayoutForm(Terminal *term)
{
    FnTrace("FormZone::LayoutForm()");
    Flt tl = form_header;
    if (tl < 0)
        tl += size_y;

    if (tl > 0 && !no_line)
        tl += 1.0;

    FormField *f = FieldList();
    Flt fx = 0, fy = tl;
    while (f)
    {
        f->Init(term, this);
        if (f->active)
        {
            Flt center = (size_x - f->w) / 2;
            if (center < 0)
                center = 0;
            if ((fx + f->w) > size_x && fx > 0 && wrap)
            {
                fx = 0;
                if (f->align == ALIGN_CENTER)
                    fx = center;
                fy += f->h;
            }
            else if (f->align == ALIGN_CENTER && fx < center)
                fx = center;

            f->x = fx;
            f->y = fy;
        }

        if ((f->new_line > 0) &&
            (f->active || (f->fore && f->fore->active) ||
             (f->next && f->next->active)))
        {
            fx  = 0;
            fy += f->new_line + form_spacing;
        }
        else if (f->active)
            fx += f->w + 1 + f->pad;

        f = f->next;
    }
    return 0;
}

FormField *FormZone::Find(Flt px, Flt py)
{
    FnTrace("FormZone::Find()");
    for (FormField *fe = FieldList(); fe != nullptr; fe = fe->next)
    {
        if (fe->active && fe->modify &&
            py >= (fe->y -.5) && py <= (fe->y + fe->h -.5) &&
            px >= (fe->x - 1) && px <= (fe->x + fe->w + 1))
        {
            return fe;
        }
    }
    return nullptr;
}

int FormZone::NextField()
{
    FnTrace("FormZone::NextField()");
    if (keyboard_focus == nullptr)
        return FirstField();

    do
    {
        keyboard_focus = keyboard_focus->next;
        if (keyboard_focus == nullptr)
            return FirstField();
    }
    while (keyboard_focus->modify == 0 || keyboard_focus->active == 0);
    return 0;
}

int FormZone::ForeField()
{
    FnTrace("FormZone::ForeField()");
    if (keyboard_focus == nullptr)
        return LastField();

    do
    {
        keyboard_focus = keyboard_focus->fore;
        if (keyboard_focus == nullptr)
            return LastField();
    }
    while (keyboard_focus->modify == 0 || keyboard_focus->active == 0);
    return 0;
}

int FormZone::FirstField()
{
    FnTrace("FormZone::FirstField()");
    keyboard_focus = FieldList();
    while (keyboard_focus &&
           (keyboard_focus->active == 0 || keyboard_focus->modify == 0))
        keyboard_focus = keyboard_focus->next;
    return 0;
}

int FormZone::LastField()
{
    FnTrace("FormZone::LastField()");
    keyboard_focus = FieldListEnd();
    while (keyboard_focus &&
           (keyboard_focus->active == 0 || keyboard_focus->modify == 0))
        keyboard_focus = keyboard_focus->fore;
    return 0;
}

/**** ListFormZone Class ****/
// Constructor
ListFormZone::ListFormZone()
{
    show_list    = 1;
    list_page    = 0;
    list_header  = 1;
    list_footer  = 0;
    list_spacing = 1;
}

// Member Functions
RenderResult ListFormZone::Render(Terminal *term, int update_flag)
{
    FnTrace("ListFormZone::Render()");
    records = RecordCount(term);
    if (update_flag == RENDER_NEW)
    {
        record_no = 0;
        if (records > 0)
            LoadRecord(term, 0);
        show_list = 1;
        list_page = 0;
    }

    if (records <= 0)
        show_list = 1;
    if (update_flag && show_list)
    {
        list_report.Clear();
        ListReport(term, &list_report);
    }

    if (update_flag || keep_focus == 0)
        keyboard_focus = nullptr;

    LayoutZone::Render(term, update_flag);

    if (show_list)
    {
        if (records > 0)
            list_report.selected_line = record_no;
        else
            list_report.selected_line = -1;
        if (update_flag)
            list_page = -1; // force update
        list_report.Render(term, this, list_header, list_footer,
                           list_page, 0, list_spacing);
    }
    else
    {
        if (!no_line)
        {
            Flt tl = form_header;
            if (tl < 0)
                tl += size_y;
            if (tl > 0)
                Line(term, tl + .1, color[0]);
        }

        if (records > 0)
        {
            LayoutForm(term);
            for (FormField *f = FieldList(); f != nullptr; f = f->next)
            {
                f->selected = (keyboard_focus == f);
                if (f->active)
                    f->Render(term, this);
            }
        }
    }
    return RENDER_OKAY;
}

SignalResult ListFormZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("ListFormZone::Signal()");
    static const genericChar* commands[] = {
        "new", "next", "prior", "save", "restore",
        "delete", "print", "unfocus", "change view", nullptr};
    int idx = CompareListN(commands, message);

	if (idx == -1)
    {
        // let parent class give the message a try
        return FormZone::Signal(term, message);
    }

    if (idx == 0) // New
    {
        if (records > 0)
            SaveRecord(term, record_no, 0);
        record_no = records;
        if (NewRecord(term))
            return SIGNAL_IGNORED;
        records = RecordCount(term);
        if (record_no >= records)
            record_no = records - 1;
        LoadRecord(term, record_no);
        FirstField();
        show_list = 0;
        Draw(term, 0);
        return SIGNAL_OKAY;
    }

    switch (idx)
    {
    case 3:  // Save
        SaveRecord(term, record_no, 1);
        break;
    }

    if (records <= 0)
        return SIGNAL_IGNORED;

    Employee *e = term->user;
    Printer  *p = term->FindPrinter(PRINTER_REPORT);

    switch (idx)
    {
    case 1:  // Next
        SaveRecord(term, record_no, 0);
        ++record_no;
        if (record_no >= records)
            record_no = 0;
        if (records > 0)
            LoadRecord(term, record_no);
        break;
    case 2:  // Prior
        SaveRecord(term, record_no, 0);
        --record_no;
        if (record_no < 0)
            record_no = records - 1;
        if (record_no < 0)
            record_no = 0;
        else
            LoadRecord(term, record_no);
        break;
    case 4:  // Restore
        LoadRecord(term, record_no);
        break;
    case 5:  // Delete
        if (KillRecord(term, record_no))
            return SIGNAL_IGNORED;
        records = RecordCount(term);
        if (record_no >= records)
            record_no = records - 1;
        if (record_no < 0)
            record_no = 0;
        else
            LoadRecord(term, record_no);
        break;
    case 6:  // Print
        if (p == nullptr || e == nullptr)
            return SIGNAL_IGNORED;
        if (show_list)
        {
            list_report.CreateHeader(term, p, e);
            list_report.FormalPrint(p);
        }
        else
        {
            SaveRecord(term, record_no, 0);
            if (PrintRecord(term, record_no))
                return SIGNAL_IGNORED;
        }
        return SIGNAL_OKAY;
    case 7:  // Unfocus
        break;
    case 8:  // change view
        show_list ^= 1;
        if (show_list)
            SaveRecord(term, record_no, 0);
        break;
    default:
        if (strncmp(message, "search ", 7) == 0)
        {
            if (Search(term, -1, &message[7]) <= 0)
                return SIGNAL_IGNORED;
        }
        else if (strncmp(message, "nextsearch ", 11) == 0)
        {
            if (Search(term, record_no, &message[11]) <= 0)
                return SIGNAL_IGNORED;
        }
        else
            return SIGNAL_IGNORED;

        if (records > 0)
            LoadRecord(term, record_no);
        break;
    }

    Draw(term, 1);  // also removes keyboard focus
    return SIGNAL_OKAY;
}

SignalResult ListFormZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("ListFormZone::Touch()");
    if (records <= 0)
        return SIGNAL_IGNORED;

    LayoutZone::Touch(term, tx, ty);
    if (show_list)
    {
        int new_page = list_page;
        int max_page = list_report.max_pages;
        int row = list_report.TouchLine(list_spacing, selected_y);
        if (row == -1)      // header touched
        {
            --new_page;
            if (new_page < 0)
                new_page = max_page - 1;
        }
        else if (row == -2) // footer touched
        {
            ++new_page;
            if (new_page >= max_page)
                new_page = 0;
        }
        else if (row != record_no && row >= 0 && row < records)
        {
            SaveRecord(term, record_no, 0);
            record_no = row;
            LoadRecord(term, record_no);
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        if (list_page != new_page)
        {
            list_page = new_page;
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        return SIGNAL_IGNORED;
    }

    keyboard_focus = Find(selected_x, selected_y);
    if (keyboard_focus &&
        keyboard_focus->Touch(term, this, selected_x, selected_y) == SIGNAL_OKAY)
        UpdateForm(term, record_no);
    Draw(term, 0);
    return SIGNAL_OKAY;
}

SignalResult ListFormZone::Mouse(Terminal *term, int action, int mx, int my)
{
    FnTrace("ListFormZone::Mouse()");
    if (records <= 0 || !(action & MOUSE_PRESS))
        return SIGNAL_IGNORED;

    if (action & MOUSE_MIDDLE)
    {
        SignalResult sig = Signal(term, "change view");
        if (sig != SIGNAL_IGNORED)
            return sig;
    }

    LayoutZone::Touch(term, mx, my);
    if (show_list)
    {
        int new_page = list_page;
        int max_page = list_report.max_pages;
        if (selected_y < (list_header + 1) && (max_page > 1))
        {
            if (action & MOUSE_LEFT)
                --new_page;
            else if (action & MOUSE_RIGHT)
                ++new_page;
        }
        else if (selected_y > (size_y - 2.0) && (max_page > 1))
        {
            if (action & MOUSE_LEFT)
                ++new_page;
            else if (action & MOUSE_RIGHT)
                --new_page;
        }
        else
            return Touch(term, mx, my);

        if (new_page < 0)
            new_page = max_page - 1;
        if (new_page >= max_page)
            new_page = 0;
        if (list_page != new_page)
        {
            list_page = new_page;
            Draw(term, 0);
            return SIGNAL_OKAY;
        }
        return SIGNAL_IGNORED;
    }
    else
    {
        keyboard_focus = Find(selected_x, selected_y);
        if (keyboard_focus)
        {
            if (keyboard_focus->Mouse(term, this, action,
                                      selected_x, selected_y) == SIGNAL_OKAY)
                UpdateForm(term, record_no);
            Draw(term, update);
            return SIGNAL_OKAY;
        }
    }
    return Touch(term, mx, my);
}

SignalResult ListFormZone::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("ListFormZone::Keyboard()");
    if (records <= 0)
        return SIGNAL_IGNORED;

    switch (my_key)
    {
    case 16:  // ^P - page up
        return Signal(term, "prior");
    case 14:  // ^N - page down
        return Signal(term, "next");
    }
    if (show_list)
        return SIGNAL_IGNORED;

    switch (my_key)
    {
    case 9:  // Tab
        if (state & ShiftMask)
            ForeField();
        else
            NextField();
        Draw(term, 0);
        return SIGNAL_OKAY;
    case 13: // Enter
        if (state && ShiftMask)
        {
            show_list ^= 1;
            if (show_list)
                SaveRecord(term, record_no, 0);
        }
        else
        {
            NextField();
            Draw(term, 0);
        }
        return SIGNAL_OKAY;
    }

    if (keyboard_focus == nullptr)
        return SIGNAL_IGNORED;

    switch (my_key)
    {
    case 27: // Escape
        Draw(term, 1);
        return SIGNAL_OKAY;
    default:
        if (keyboard_focus->Keyboard(term, this, my_key, state) == SIGNAL_OKAY)
        {
            if (update)
                Draw(term, update);
            else if (UpdateForm(term, record_no))
                keyboard_focus->Draw(term, this);
            else
                Draw(term, update);
        }
        return SIGNAL_OKAY;
    }
}

int ListFormZone::Update(Terminal *term, int update_message, const genericChar* value)
{
    FnTrace("ListFormZone::Update()");
    if (show_list && list_report.update_flag & update_message)
        return Draw(term, 1);
    else
        return 0;
}

/**** FormField Class ****/
// Constructor
FormField::FormField()
{
    next     = nullptr;
    fore     = nullptr;
    new_line = 0;
    x        = 0;
    y        = 0;
    w        = 0;
    h        = 0;
    pad      = 0;
    selected = 0;
    active   = 1;
    align    = ALIGN_LEFT;
    color    = COLOR_DEFAULT;
    flag     = 0;
}

// Member Functions
SignalResult FormField::Touch(Terminal *term, FormZone *fzone, Flt tx, Flt ty)
{
    FnTrace("FormField::Touch()");
    return SIGNAL_IGNORED;
}

SignalResult FormField::Keyboard(Terminal *term, FormZone *fzone, int key, int state)
{
    FnTrace("FormField::Keyboard()");
    return SIGNAL_IGNORED;
}

SignalResult FormField::Mouse(Terminal *term, FormZone *fzone,
                              int action, Flt mx, Flt my)
{
    FnTrace("FormField::Mouse()");
    if ((action & MOUSE_PRESS))
        return Touch(term, fzone, mx, my);
    else
        return SIGNAL_IGNORED;
}

int FormField::Draw(Terminal *term, FormZone *fzone)
{
    FnTrace("FormField::Draw()");
    if (active == 0)
        return 1;

    Render(term, fzone);
    // should be more selective about area updated
    term->UpdateArea(fzone->x, fzone->y, fzone->w, fzone->h);
    return 0;
}

void FormField::Print(void)
{
    printf("Unspecified %s\n", label.Value());
}

/**** LabelField Class ****/
// Constructor
LabelField::LabelField()
{
    label.Set("");
    min_width = 0;
    modify = 0;
}

LabelField::LabelField(const genericChar* lbl, Flt width)
{
    label.Set(lbl);
    min_width = width;
    modify = 0;
}

// Member Functions
int LabelField::Init(Terminal *term, FormZone *fzone)
{
    FnTrace("LabelField::Init()");
    Flt lw = fzone->TextWidth(term, label.Value());
    if (lw < min_width)
        w = min_width + 1;
    else
        w = lw + 1;
    h = 2;
    return 0;
}

RenderResult LabelField::Render(Terminal *term, FormZone *fzone)
{
    FnTrace("LabelField::Render()");
    fzone->TextPosL(term, x, y, label.Value(), color);
    return RENDER_OKAY;
}

/**** SubmitField Class ****/
SubmitField::SubmitField(const genericChar* lbl, Flt width)
{
    label.Set(lbl);
    min_width = width;
    modify = 1;
}

int SubmitField::Init(Terminal *term, FormZone *fzone)
{
    FnTrace("SubmitField::Init()");
    Flt label_width = fzone->TextWidth(term, label.Value());
    if (label_width < min_width)
        w = min_width + 1;
    else
        w = label_width + 1;
    h = 2;
    return 0;
}

RenderResult SubmitField::Render(Terminal *term, FormZone *fzone)
{
    FnTrace("SubmitField::Render()");
    RenderResult retval = RENDER_OKAY;

    fzone->Button(term, x, y, w, selected);
    if (label.size() > 0)
    {
        int c = color;
        int m = 0;
        if (selected)
        {
            c = COLOR_LT_BLUE;
            m = PRINT_UNDERLINE;
        }
        fzone->TextPosC(term, x + (min_width / 2), y, label.Value(), c, m);
    }
    return retval;
}

SignalResult SubmitField::Keyboard(Terminal *term, FormZone *fzone, int key, int state)
{
    FnTrace("SubmitField::Keyboard()");
    SignalResult retval = SIGNAL_IGNORED;

    switch (key)
    {
    case 13:  //Enter
    case 32:  //Space
        fzone->SaveRecord(term, -1, 1);
        fzone->show_list = 1;
        fzone->update = 1;
        retval = SIGNAL_OKAY;
        break;
    default:
        retval = FormField::Keyboard(term, fzone, key, state);
        break;
    }
    return retval;
}

SignalResult SubmitField::Mouse(Terminal *term, FormZone *fzone, int action, Flt mx, Flt my)
{
    FnTrace("SubmitField::Mouse()");
    SignalResult retval = SIGNAL_OKAY;

    fzone->SaveRecord(term, -1, 1);
    fzone->show_list = 1;
    fzone->update = 1;
    return retval;
}


/**** TextField Class ****/
// Constructor
TextField::TextField(const genericChar* lbl, int max_entry, int mod, Flt min_label)
{
    label.Set(lbl);
    buffint = 0;
    max_buffer_len  = max_entry;
    min_label_width = min_label;
    modify          = mod;
    cursor = 0;
    lo_value = 0;
    hi_value = 0;
}

int TextField::Set(const genericChar* v)
{
    FnTrace("TextField::Set()");
    if ((flag & FF_MONEY) || (flag & FF_ONLYDIGITS))
        buffint = atoi(v);
    else
        buffint = 0;
    return buffer.Set(v);
}

int TextField::Set(Str &v)
{
    FnTrace("TextField::Set()");
    if ((flag & FF_MONEY) || (flag & FF_ONLYDIGITS))
        buffint = v.IntValue();
    else
        buffint = 0;
    return buffer.Set(v.Value());
}

int TextField::Set(int v)
{
    FnTrace("TextField::Set()");
    buffint = v;
    return buffer.Set(v);
}

int TextField::Set(Flt v)
{
    FnTrace("TextField::Set()");
    buffint = 0;
    return buffer.Set(v);
}


int TextField::SetNumRange(int lo, int hi)
{
    FnTrace("TextField::SetNumRange()");
    lo_value = lo;
    hi_value = hi;
    return 0;
}


int TextField::Get(genericChar* v, int len)
{
    FnTrace("TextField::Get(const char* , int)");

    if ((flag & FF_MONEY) || (flag & FF_ONLYDIGITS))
        buffer.Set(buffint);

    strncpy(v, buffer.Value(), len);
    return 0;
}

int TextField::Get(genericChar* v)
{
    FnTrace("TextField::Get(const char* )");

    if ((flag & FF_MONEY) || (flag & FF_ONLYDIGITS))
        buffer.Set(buffint);

    strcpy(v, buffer.Value());
    return 0;
}

int TextField::Get(Str &v)
{
    FnTrace("TextField::Get(Str)");

    if ((flag & FF_MONEY) || (flag & FF_ONLYDIGITS))
        buffer.Set(buffint);

    v.Set(buffer);
    return 0;
}

int TextField::Get(int &v)
{
    FnTrace("TextField::Get()");
    int retval = 0;

    if ((flag & FF_MONEY) || (flag & FF_ONLYDIGITS))
        v = buffint;
    else
        v = buffer.IntValue();
    return retval;
}

int TextField::Get(Flt &v)
{
    FnTrace("TextField::Get()");
    int retval = 0;

    if ((flag & FF_MONEY) || (flag & FF_ONLYDIGITS))
        v = (Flt) buffint;
    else
        v = buffer.FltValue();
    return retval;
}

// Member Functions

/****
 * InsertStringAtCursor:  get the part of the string before the cursor and the
 *  part after, and put the string together as
 *  "<before><string><after>"
 ****/
int TextField::InsertStringAtCursor(genericChar* my_string)
{
    FnTrace("TextField::InsertStringAtCursor()");
    const genericChar* bbuff = buffer.Value();
    genericChar first[STRLENGTH] = "";
    genericChar last[STRLENGTH] = "";
    genericChar newstr[STRLENGTH];
    int bufidx = 0;
    int stridx = 0;
    int tidx = 0;  // index for first and last

    // convert the string to upper case if necessary
    // either way, set stridx to the length of string for use later
    while (my_string[stridx] != '\0')
    {
        if (flag & FF_ALLCAPS)
            my_string[stridx] = toupper(my_string[stridx]);
        stridx += 1;
    }

    while (bbuff[bufidx] != '\0')
    {
        if (bufidx < cursor)
        {  // copy into first
            first[tidx] = bbuff[bufidx];
            tidx += 1;
        }
        else if (bufidx == cursor)
        {
            tidx = 0;
            last[tidx] = bbuff[bufidx];
            tidx += 1;
        }
        else
        {  // copy into last
            last[tidx] = bbuff[bufidx];
            tidx += 1;
        }
        bufidx += 1;
    }
    snprintf(newstr, STRLENGTH, "%s%s%s", first, my_string, last);
    buffer.Set(newstr);
    cursor += stridx;
    return 0;
}

int TextField::InsertDigits(int digits, int num)
{
    FnTrace("TextField::InsertDigits()");
    int newval = buffint;
    int digit;
    int retval = 0;

    if ((digits == 0) || (num > 1))
    { //NOTE BAK-->assume only one digit in digits; this is for "00"
        while (num > 0)
        {
            newval = newval * 10 + digits;
            num = num - 1;
        }
    }
    else if (digits > 0)
    {
        while (digits)
        {
            digit = digits % 10;
            newval = (newval * 10) + digit;
            digits = (digits - digit) / 10;
        }
    }
    if ((hi_value == 0) || (newval <= hi_value))
        buffint = newval;
    else
        retval = 1;
    return retval;
}

/****
 * Append:  adds the given string after the cursor
 *   There are cases where the string will not be added:
 *     o  if it's a letter and the field is numeric
 *     o  if it's 00 and there is no period (this should be extended to
 *        include cases such as 00 before the period)
 *     o  if it's . and there's already a decimal point
 *     o  if the buffer is already full
 * Returns 0 for success.
 ****/
int TextField::Append(genericChar* my_string)
{
    FnTrace("TextField::Append()");
    int retval = 0;
    int numdigits = 1;

    if (buffer.size() >= max_buffer_len)
        retval = 1;
    else if (flag & FF_MONEY || flag & FF_ONLYDIGITS)
    {
        if (strcmp(my_string, "00") == 0)
            numdigits = 2;
        int alldigits = 1;
        int idx = 0;
        while (my_string[idx] != '\0')
        {
            if (! isdigit(my_string[idx]))
                alldigits = 0;
            idx += 1;
        }
        if (alldigits)
            retval = InsertDigits(atoi(my_string), numdigits);
        else
            retval = 1;
    }
    else
        retval = InsertStringAtCursor(my_string);
    return retval;
}

int TextField::Append(Str &my_string)
{
    FnTrace("TextField::Append()");
    genericChar* tbuff = nullptr;
    int retval = 0;
    int numdigits = 1;

    if (buffer.size() >= max_buffer_len)
        retval = 1;
    else if (flag & FF_MONEY || flag & FF_ONLYDIGITS)
    {  // AVOID this in favor of Append(int)
        tbuff = (genericChar*)my_string.Value();
        if (strcmp(tbuff, "00") == 0)
            numdigits = 2;
        int alldigits = 1;
        int idx = 0;
        while (tbuff[idx] != '\0')
        {
            if (! isdigit(tbuff[idx]))
                alldigits = 0;
            idx += 1;
        }
        if (alldigits)
            retval = InsertDigits(atoi(tbuff), numdigits);
        else
            retval = 1;
    }
    else
        retval = InsertStringAtCursor(tbuff);
    return retval;
}

int TextField::Append(int val)
{
    FnTrace("TextField::Append()");
    int retval = 0;
    genericChar my_string[STRLENGTH];

    if (buffer.size() >= max_buffer_len)
        retval = 1;
    else if ((flag & FF_MONEY) || (flag & FF_ONLYDIGITS))
    {
        retval = InsertDigits(val);
    }
    else
    {
        snprintf(my_string, STRLENGTH, "%d", val);
        retval = InsertStringAtCursor(my_string);
    }
    return retval;
}

/****
 * Append:  Should this ever be called?  Probably not.  But it's here in
 *   case we need it sometime in the future.
 ****/
int TextField::Append(Flt val)
{
    FnTrace("TextField::Append()");
    return 1;
}

int TextField::Append(genericChar key)
{
    FnTrace("TextField::Append()");
    int retval = 0;
    genericChar str[2];

    if (buffer.size() >= max_buffer_len)
        retval = 1;
    else if ((flag & FF_MONEY) || (flag & FF_ONLYDIGITS))
    {
        retval = InsertDigits(key - 48);
    }
    else
    {
        str[0] = key;
        str[1] = '\0';
        retval = InsertStringAtCursor(str);
    }
    return retval;
}

int TextField::Remove(int num)
{
    FnTrace("TextField::Remove()");
    genericChar bbuff[STRLENGTH];
    int bufflen;

    if (buffer.size() < 1)
        return 1;

    strcpy(bbuff, buffer.Value());
    bufflen = strlen(bbuff);

    if (cursor == bufflen)
    {
        if ((flag & FF_MONEY) || (flag & FF_ONLYDIGITS))
        {
            while (num > 0)
            {
                buffint = buffint / 10;
                if (cursor > 0)
                    cursor -= 1;
                num -= 1;
            }
        }
        else
        {
            bbuff[bufflen - 1] = '\0';
            buffer.Set(bbuff);
            if (cursor > 0)
                cursor -= 1;
        }
    }
    else
    {
        int idx = cursor + num;
        while (idx <= bufflen)
        {
            bbuff[idx - num] = bbuff[idx];
            idx += 1;
        }
        buffer.Set(bbuff);
        if ((flag & FF_MONEY) || (flag & FF_ONLYDIGITS))
            buffint = buffer.IntValue();
    }
    return 0;
}

int TextField::Clear()
{
    FnTrace("TextField::Clear()");
    buffer.Set("");
    buffint = 0;
    cursor = 0;
    return 0;
}

int TextField::Init(Terminal *term, FormZone *fzone)
{
    FnTrace("TextField::Init()");
    label_width = fzone->TextWidth(term, label.Value());
    if (label_width < min_label_width)
        label_width = min_label_width;
  
    w = label_width + max_buffer_len + 3;
    h = 2;
    return 0;
}

RenderResult TextField::Render(Terminal *term, FormZone *fzone)
{
    FnTrace("TextFields::Render()");
    int c = color, m = 0;
    const genericChar* buff;

    if (selected)
    {
        c = COLOR_LT_BLUE;
        m = PRINT_UNDERLINE;
    }
    fzone->TextPosL(term, x, y, label.Value(), c, m);

    Flt xx = x + label_width + 1;
    if (modify)
        fzone->Entry(term, xx, y, max_buffer_len + 1);

    if (flag & FF_MONEY)
    {
        buffer.Set(term->FormatPrice(buffint));
        cursor = buffer.size();
    }
    else if (flag & FF_ONLYDIGITS)
    {
        buffer.Set(buffint);
        cursor = buffer.size();
    }
    buff = admission_filteredname(buffer.str());   // REFACTOR: Added .str() to convert Str to std::string for new signature
    fzone->TextPosL(term, xx, y, buff, COLOR_WHITE);
    if (selected)
    {
        if (cursor > buffer.size())
            cursor = buffer.size();

        Flt pos = 0;
        if (cursor > 0)
            pos = fzone->TextWidth(term, buff, cursor);
        fzone->Underline(term, xx + pos, y, 1.0, COLOR_YELLOW);
    }
    else
        cursor = buffer.size();
    return RENDER_OKAY;
}

SignalResult TextField::Keyboard(Terminal *term, FormZone *fzone, int key, int state)
{
    FnTrace("TextField::Keyboard()");

    switch (key)
    {
    case 12:  // ^L - left
        if (cursor <= 0)
            return SIGNAL_IGNORED;
        --cursor;
        return SIGNAL_OKAY;
    case 17:  // ^R - right
        if (cursor >= buffer.size())
            return SIGNAL_IGNORED;
        ++cursor;
        return SIGNAL_OKAY;
    case 8:  // Backspace
        if (Remove(1))
            return SIGNAL_IGNORED;
        else
            return SIGNAL_OKAY;
    }

    Append((genericChar)key);
    return SIGNAL_OKAY;
}

/****
 * Print:  This function is for debugging only and should not be
 *   used in live code (as it serves no real purpose).
 ****/
void TextField::Print(void)
{
    if (flag & FF_ONLYDIGITS)
        printf("Text %s:  %d\n", label.Value(), buffint);
    else
        printf("Text %s:  %s\n", label.Value(), buffer.Value());
}


/**** TimeDateField Class ****/
static Flt TDF_Seg[] = {5, 9, 12, 17, 20};
static Flt TDF_Len[] = {3, 2, 4, 2, 2};

// Constructor
TimeDateField::TimeDateField(const genericChar* lbl, int mod, int cu)
{
    label.Set(lbl);
    cursor    = 0;
    show_time = 1;
    modify    = mod;
    can_unset = cu;
}

// Member Functions
int TimeDateField::Init(Terminal *term, FormZone *fzone)
{
    FnTrace("TimeDateField::Init()");
    w = fzone->TextWidth(term, label.Value());
    if (show_time)
        w += 26.5;
    else
        w += 17.5;
    h = 2;
    return 0;
}

RenderResult TimeDateField::Render(Terminal *term, FormZone *fzone)
{
    FnTrace("TimeDateField::Render()");
    int c = color;
    int m = 0;
    buffer.Floor<std::chrono::minutes>();
    if (selected)
    {
        c = COLOR_LT_BLUE;
        m = PRINT_UNDERLINE;
    }
    fzone->TextPosL(term, x, y, label.Value(), c, m);

    Flt xx = x + fzone->TextWidth(term, label.Value()) + 1;
    if (modify)
    {
        if (show_time)
            fzone->Entry(term, xx, y, 25);
        else
            fzone->Entry(term, xx, y, 16);
    }

    if (!buffer.IsSet())
    {
        if (show_time)
            fzone->TextPosC(term, xx + 12.5, y, "Time/Date Not Set", COLOR_WHITE);
        else
            fzone->TextPosC(term, xx + 8, y, "Date Not Set", COLOR_WHITE);
        return RENDER_OKAY;
    }

    if (selected)
        fzone->Underline(term, xx + TDF_Seg[cursor], y, TDF_Len[cursor], COLOR_YELLOW);

    int val;
    genericChar str[STRLENGTH];

    val = buffer.WeekDay();
    if (!buffer.IsSet() || val < 0 || val > 6)
        snprintf(str, STRLENGTH, "---");
    else
        snprintf(str, STRLENGTH, "%s",term->Translate(ShortDayName[val]));
    fzone->TextPosL(term, xx,        y, str, COLOR_WHITE);

    val = buffer.Month() - 1;
    if (!buffer.IsSet() || val < 0 || val > 11)
        snprintf(str, STRLENGTH, "---");
    else
        snprintf(str, STRLENGTH, "%s",term->Translate(ShortMonthName[val]));
    fzone->TextPosC(term, xx + 6.5,  y, str, COLOR_WHITE);

    snprintf(str, STRLENGTH, "%d", buffer.Day());
    fzone->TextPosC(term, xx + 10,   y, str, COLOR_WHITE);
    fzone->TextPosC(term, xx + 11.5, y, ",", COLOR_WHITE);
    snprintf(str, STRLENGTH, "%d", buffer.Year());
    fzone->TextPosC(term, xx + 14,   y, str, COLOR_WHITE);

    if (show_time)
    {
        int hour = buffer.Hour() % 12;
        if (hour == 0)
            hour = 12;
        snprintf(str, STRLENGTH, "%d", hour);
        fzone->TextPosC(term, xx + 18,   y, str, COLOR_WHITE);
        fzone->TextPosC(term, xx + 19.5, y, ":", COLOR_WHITE);
        snprintf(str, STRLENGTH, "%02d", buffer.Min());
        fzone->TextPosC(term, xx + 21,   y, str, COLOR_WHITE);
        if (buffer.Hour() >= 12)
            strcpy(str, "pm");
        else
            strcpy(str, "am");
        fzone->TextPosL(term, xx + 22.3, y, str, COLOR_WHITE);
    }
    return RENDER_OKAY;
}

SignalResult TimeDateField::Keyboard(Terminal *term, FormZone *fzone, int key, int state)
{
    FnTrace("TimeDateField::Keyboard()");
    switch (key)
    {
    case 12:  // ^L - left
        --cursor;
        if (cursor < 0)
        {
            if (show_time)
                cursor = 4;
            else
                cursor = 2;
        }
        return SIGNAL_OKAY;

    case 17:  // ^R - right
        ++cursor;
        if ((show_time && cursor > 4) || (show_time == 0 && cursor > 2))
            cursor = 0;
        return SIGNAL_OKAY;

    case 21:  // ^U - up
    case '+':
    case '=':
        if (!buffer.IsSet())
        {
            buffer = SystemTime;
            return SIGNAL_OKAY;
        }
        switch (cursor)
        {
        case 4: buffer += std::chrono::minutes(1); break;
        case 3: buffer += std::chrono::minutes(60); break;
        case 2: buffer += date::years(1); break;
        case 1: buffer += date::days(1); break;
        case 0: buffer += date::months(1); break;
        }
        return SIGNAL_OKAY;

    case 4:   // ^D - down
    case '-':
    case '_':
        if (!buffer.IsSet())
        {
            buffer = SystemTime;
            buffer.Floor<std::chrono::minutes>();
            return SIGNAL_OKAY;
        }
        switch (cursor)
        {
        case 4: buffer += std::chrono::minutes(-1); break;
        case 3: buffer += std::chrono::minutes(-60); break;
        case 2: buffer -= date::years(1); break;
        case 1: buffer -= date::days(1); break;
        case 0: buffer -= date::months(1); break;
        }
        return SIGNAL_OKAY;

    case 8:  // backspace
        if (can_unset)
        {
            buffer.Clear();
            return SIGNAL_OKAY;
        }
    }

    return SIGNAL_IGNORED;
}

SignalResult TimeDateField::Mouse(Terminal *term, FormZone *fzone,
                                  int action, Flt mx, Flt my)
{
    FnTrace("TimeDateField::Mouse()");
    if ((action & MOUSE_PRESS) == 0)
        return SIGNAL_IGNORED;

    mx -= x + fzone->TextWidth(term, label.Value()) + 1;

    for (int i = 0; i <= 4; ++i)
    {
        if (mx >= TDF_Seg[i] && mx < (TDF_Seg[i] + TDF_Len[i]))
        {
            cursor = i;
            if (action & MOUSE_LEFT)
                Keyboard(term, fzone, '+', 0);
            else if (action & MOUSE_RIGHT)
                Keyboard(term, fzone, '-', 0);
            return SIGNAL_OKAY;
        }
    }

    return SIGNAL_IGNORED;
}

int TimeDateField::Set(TimeInfo *timevar)
{
    FnTrace("TimeDateField::Set()");

    if (timevar)
        buffer = *timevar;
    else
        buffer.Clear();

    return 0;
}


/**** TimeDayField Class ****/
// Constructor
TimeDayField::TimeDayField(const genericChar* lbl, int mod, int unset)
{
    day = 0;
    hour = 0;
    min = 0;
    label.Set(lbl);
    modify = mod;
    cursor = 0;
    show_day = 1;
    can_unset = unset;
    is_unset = 1;
}

// Member Functions
int TimeDayField::Init(Terminal *term, FormZone *fzone)
{
    FnTrace("TimeDayField::Init()");
    w = fzone->TextWidth(term, label.Value()) + 9;
    h = 2;
    if (show_day)
        w += 5;
    return 0;
}

RenderResult TimeDayField::Render(Terminal *term, FormZone *fzone)
{
    FnTrace("TimeDayField::Render()");
    int c = color;
    int m = 0;

    if (hour < 0 || min < 0)
        is_unset = 1;

    if (selected)
    {
        c = COLOR_LT_BLUE;
        m = PRINT_UNDERLINE;
    }
    fzone->TextPosL(term, x, y, label.Value(), c, m);

    Flt xx = x + fzone->TextWidth(term, label.Value()) + 1;
    if (modify)
    {
        if (show_day)
            fzone->Entry(term, xx, y, 13);
        else
            fzone->Entry(term, xx, y, 8);
    }

    genericChar str[STRLENGTH];
    if (selected)
    {
        if (!show_day && cursor < 1)
            cursor = 1;

        Flt pos, len;
        if (cursor == 0)
        {
            pos = 0;
            len = 4;
        }
        else if (cursor == 1)
        {
            if (show_day)
                pos = 5;
            else
                pos = 0;
            len = 2;
        }
        else
        {
            if (show_day)
                pos = 8;
            else
                pos = 3;
            len = 2;
        }
        fzone->Underline(term, xx + pos, y, len, COLOR_YELLOW);
    }

    // Day Of Week
    if (show_day)
    {
        if (is_unset)
            strcpy(str, "---");
        else
            strcpy(str, term->Translate(ShortDayName[day]));
        fzone->TextPosC(term, xx + 2.1,  y, str, COLOR_WHITE);
    }

    // Time Of Day
    if (!show_day)
        xx -= 5;
    int my_hour = hour % 12;
    if (my_hour == 0)
        my_hour = 12;
    if (is_unset)
        strcpy(str, "--");
    else
        snprintf(str, STRLENGTH, "%d", my_hour);
    fzone->TextPosC(term, xx + 6,   y, str, COLOR_WHITE);
    fzone->TextPosC(term, xx + 7.5, y, ":", COLOR_WHITE);
    if (is_unset)
        strcpy(str, "--");
    else
        snprintf(str, STRLENGTH, "%02d", min);
    fzone->TextPosC(term, xx + 9,   y, str, COLOR_WHITE);
    if (!is_unset)
    {
        if (hour >= 12)
            strcpy(str, "pm");
        else
            strcpy(str, "am");
        fzone->TextPosL(term, xx + 10.3, y, str, COLOR_WHITE);
    }
    return RENDER_OKAY;
}

SignalResult TimeDayField::Keyboard(Terminal *term, FormZone *fzone, int key, int state)
{
    FnTrace("TimeDayField::Keyboard()");
    switch (key)
    {
    case 12:  // ^L - left
        --cursor;
        if (cursor < 0 || (cursor < 1 && !show_day))
            cursor = 2;
        return SIGNAL_OKAY;

    case 17:  // ^R - right
        ++cursor;
        if (cursor > 2)
        {
            if (show_day)
                cursor = 0;
            else
                cursor = 1;
        }
        return SIGNAL_OKAY;

    case 21:  // ^U - up
    case '+':
    case '=':
        if (is_unset)
        {
            is_unset = 0;
            return SIGNAL_OKAY;
        }
        switch (cursor)
        {
        case 0:
            ++day;
            if (day > 6)
                day = 0;
            break;
        case 1:
            ++hour;
            if (hour > 23)
            {
                hour = 0;
                ++day;
                if (day > 6)
                    day = 0;
            }
            break;
        case 2:
            ++min;
            if (min > 59)
            {
                min = 0;
                ++hour;
                if (hour > 23)
                {
                    hour = 0;
                    ++day;
                    if (day > 6)
                        day = 0;
                }
            }	
            break;
        }
        return SIGNAL_OKAY;

    case 4:   // ^D - down
    case '-':
    case '_':
        if (is_unset)
        {
            is_unset = 0;
            return SIGNAL_OKAY;
        }
        switch (cursor)
        {
        case 0:
            --day;
            if (day < 0)
                day = 6;
            break;
        case 1:
            --hour;
            if (hour < 0)
            {
                hour = 23;
                --day;
                if (day < 0)
                    day = 6;
            }
            break;
        case 2:
            --min;
            if (min < 0)
            {
                min = 59;
                --hour;
                if (hour < 0)
                {
                    hour = 23;
                    --day;
                    if (day < 0)
                        day = 6;
                }
            }	
            break;
        }
        return SIGNAL_OKAY;

    case ' ':
        if (can_unset)
            is_unset ^= 1;
        else
            is_unset = 0;
        return SIGNAL_OKAY;
    }
    return SIGNAL_IGNORED;
}

SignalResult TimeDayField::Mouse(Terminal *term, FormZone *fzone, int action,
                                 Flt mx, Flt my)
{
    FnTrace("TimeDayField::Mouse()");
    if ((action & MOUSE_PRESS) == 0)
        return SIGNAL_IGNORED;

    mx -= x + fzone->TextWidth(term, label.Value()) + 1;

    if (show_day)
    {
        if (mx >= 0 && mx < 4)
            cursor = 0;
        else if (mx >= 5 && mx < 7)
            cursor = 1;
        else if (mx >= 8 && mx < 10)
            cursor = 2;
        else
            return SIGNAL_IGNORED;
    }
    else
    {
        if (mx >= 0 && mx < 2)
            cursor = 1;
        else if (mx >= 3 && mx < 5)
            cursor = 2;
        else
            return SIGNAL_IGNORED;
    }

    if (action & MOUSE_LEFT)
        Keyboard(term, fzone, '+', 0);
    else if (action & MOUSE_RIGHT)
        Keyboard(term, fzone, '-', 0);
    return SIGNAL_OKAY;
}

int TimeDayField::Set(TimeInfo &tinfo)
{
    FnTrace("TimeDayField::Set(TimeInfo &)");

    if (tinfo.IsSet())
        is_unset = 0;
    else
        is_unset = 1;

    day  = tinfo.WeekDay();
    min  = tinfo.Min();
    hour = tinfo.Hour();
    
    if (day < 0 || day > 6)
        day = 0;
    if (min < 0 || min > 59)
        min = 0;
    if (hour < 0 || hour > 23)
        hour = 0;

    return 0;
}

int TimeDayField::Set(TimeInfo *tinfo)
{
    FnTrace("TimeDayField::Set(TimeInfo *)");

    if (tinfo != nullptr && tinfo->IsSet())
    {
        is_unset = 0;
        day = tinfo->WeekDay();
        min = tinfo->Min();
        hour = tinfo->Hour();

        if (day < 0 || day > 6)
            day = 0;
        if (min < 0 || min > 59)
            min = 0;
        if (hour < 0 || hour > 23)
            hour = 0;
    }
    else
    {
        is_unset = 1;
        Clear();
    }

    return 0;
}

int TimeDayField::Set(int minutes)
{
    FnTrace("TimeDayField::Set(int)");

    if (minutes < 0)
    {
        is_unset = 1;
        day  = 0;
        hour = 0;
        min  = 0;
    }
    else
    {
        is_unset = 0;
        day  = (minutes / 1440) % 7;
        hour = (minutes / 60) % 24;
        min  = minutes % 60;
    }

    return 0;
}

int TimeDayField::Get(TimeInfo &tinfo)
{
    FnTrace("TimeDayField::Get(TimeInfo &)");

    if (!tinfo.IsSet())
    {
        // initialize tinfo to current time if not set
        tinfo.Set();
    }
    tinfo.Floor<date::days>(); // reset time of tinfo object

    tinfo += (std::chrono::hours(hour) + std::chrono::minutes(min));

    // TODO: handle is_unset case
    //if (is_unset == 0)
    //    tinfo.Year(1);

    return 0;
}

int TimeDayField::Get(int &minutes)
{
    FnTrace("TimeDayField::Get(int)");

    if (is_unset)
        minutes = -1;
    else if (show_day)
        minutes = (day * 1440) + (hour * 60) + min;
    else
        minutes = (hour * 60) + min;

    return 0;
}


/*********************************************************************
 * WeekDayField Class
 ********************************************************************/

WeekDayField::WeekDayField(const char* lbl, int mod)
{
    FnTrace("WeekDayField::WeekDayField()");
    label.Set(lbl);
    modify = mod;
    current = 0;
}

int WeekDayField::Init(Terminal *term, FormZone *fzone)
{
    FnTrace("WeekDayField::Init()");
    int retval = 0;

    w = fzone->TextWidth(term, label.Value()) + 35.5;
    h = 2;

    return retval;
}

Flt daypos[] = {1, 5, 10, 14, 19, 23.5, 27, -1};
RenderResult WeekDayField::Render(Terminal *term, FormZone *fzone)
{
    FnTrace("WeekDayField::Render()");
    RenderResult retval = RENDER_OKAY;
    int  c = color;
    int  m = 0;
    Flt  xx;
    char buffer[STRLENGTH];
    const char* daystr[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    int  day;
    int  didx;
    Flt  len;

    if (selected)
    {
        c = COLOR_LT_BLUE;
        m = PRINT_UNDERLINE;
    }
    else
        current = 0;

    fzone->TextPosL(term, x, y, label.Value(), c, m);
    xx = x + fzone->TextWidth(term, label.Value()) + 1;

    fzone->Entry(term, xx, y, 31);

    // figure out the contents of the buffer
    day = WEEKDAY_SUNDAY;
    didx = 0;
    while (day <= WEEKDAY_SATURDAY)
    {
        if (days & day)
            strcpy(buffer, term->Translate(daystr[didx]));
        else
            strcpy(buffer, "---");
        fzone->TextPosL(term, xx + daypos[didx], y, buffer, COLOR_WHITE);
        if (current & day)
        {
            len = fzone->TextWidth(term, buffer);
            fzone->Underline(term, xx + daypos[didx], y, len, COLOR_YELLOW);
        }
        didx += 1;
        day <<= 1;
    }

    return retval;
}

SignalResult WeekDayField::Keyboard(Terminal *term, FormZone *fzone, int key, int state)
{
    FnTrace("WeekDayField::Keyboard()");
    SignalResult retval = SIGNAL_OKAY;

    switch (key)
    {
    case 12:  // left
        if (current == WEEKDAY_SUNDAY)
            current = WEEKDAY_SATURDAY;
        else
            current >>= 1;
        break;
    case 17:  // right
        if (current == WEEKDAY_SATURDAY)
            current = WEEKDAY_SUNDAY;
        else
            current <<= 1;
        break;
    case 4:  // down
    case 21:  // up
    case '+':
    case '-':
        if (current == 0)
            current = WEEKDAY_SUNDAY;
        else if (days & current)
            days ^= current;
        else
            days |= current;
        break;
    }

    return retval;
}

SignalResult WeekDayField::Mouse(Terminal *term, FormZone *fzone, int action, Flt mx, Flt my)
{
    FnTrace("WeekDayField::Mouse()");
    SignalResult retval = SIGNAL_OKAY;
    int didx = 0;
    int day = WEEKDAY_SUNDAY;
    Flt dpos;

    current = 0;
    mx -= x + fzone->TextWidth(term, label.Value()) + 1;
    while (daypos[didx] > -1)
    {
        dpos = daypos[didx];
        if (mx >= dpos && mx <= (dpos + 3))
            current = day;
        didx += 1;
        day <<= 1;
    }

    if (action & MOUSE_LEFT)
        Keyboard(term, fzone, '+', 0);
    else if (action & MOUSE_RIGHT)
        Keyboard(term, fzone, '-', 0);

    return retval;
}

int WeekDayField::Set(int d)
{
    FnTrace("WeekDayField::Set()");
    days = d;

    return days;
}

int WeekDayField::Get(int &d)
{
    FnTrace("WeekDayField::Get()");

    d = days;
    return days;
}


/**** ListFieldEntry Class ****/
// Constructor
ListFieldEntry::ListFieldEntry(const genericChar* lbl, int val)
{
    next = nullptr;
    fore = nullptr;
    label.Set(lbl);
    value  = val;
    active = 1;
}

/**** ListField Class ****/
// Constructor
ListField::ListField(const genericChar* lbl, const genericChar* *options, int *values,
                     Flt min_label, Flt min_list)
{
    current         = nullptr;
    label.Set(lbl);
    SetList(options, values);
    modify          = 1;
    min_label_width = min_label;
    min_entry_width = min_list;
    light_up        = 0;
}

// Destructor
ListField::~ListField()
{
    ClearEntries();
}

// Member Functions
int ListField::Init(Terminal *term, FormZone *fzone)
{
    FnTrace("ListField::Init()");
    entry_width = min_entry_width;
    for (ListFieldEntry *lfe = EntryList(); lfe != nullptr; lfe = lfe->next)
    {
        Flt len = fzone->TextWidth(term, lfe->label.Value());
        if (len > entry_width)
            entry_width = len;
    }

    label_width = fzone->TextWidth(term, label.Value());
    if (min_label_width > label_width)
        label_width = min_label_width;

    if (label.size() <= 0)
        w = entry_width + 1;
    else
        w = label_width + entry_width + 2.5;
    h = 2;
    return 0;
}

RenderResult ListField::Render(Terminal *term, FormZone *fzone)
{
    FnTrace("ListField::Render()");
    Flt xx = x;
    if (label.size() > 0)
    {
        int c = color, m = 0;
        if (selected)
        {
            c = COLOR_LT_BLUE;
            m = PRINT_UNDERLINE;
        }
        fzone->TextPosL(term, x, y, label.Value(), c, m);
        xx += label_width + 1;
    }

    if (current == nullptr)
        current = EntryList();

    fzone->Button(term, xx, y, entry_width, selected);
    if (current)
    {
        fzone->TextPosC(term, xx + (entry_width / 2), y, current->label.Value(),
                    COLOR_BLACK);
    }
    return RENDER_OKAY;
}

SignalResult ListField::Keyboard(Terminal *term, FormZone *fzone, int key, int state)
{
    FnTrace("ListField::Keyboard()");
    switch (key)
    {
    case 4:    // ^D - down
    case ' ':  // space
        NextEntry();
        return SIGNAL_OKAY;

    case 21:   // ^U - up
    case 8:    // backspace
        ForeEntry();
        return SIGNAL_OKAY;

    default:
        return SIGNAL_IGNORED;
    }
}

SignalResult ListField::Touch(Terminal *term, FormZone *fzone, Flt tx, Flt ty)
{
    FnTrace("ListField::Touch()");
    Flt xx = x;
    if (label.size() > 0)
        xx += label_width + .6;

    if (tx >= xx && tx <= (xx + entry_width + 1))
        NextEntry();
    return SIGNAL_OKAY;
}

SignalResult ListField::Mouse(Terminal *term, FormZone *fzone,
                              int action, Flt mx, Flt my)
{
    FnTrace("ListField::Mouse()");
    Flt xx = x;
    if (label.size() > 0)
        xx += label_width + 1;

    if (!(action & MOUSE_PRESS))
        return SIGNAL_IGNORED;

    if (mx >= xx && mx <= (xx + entry_width))
    {
        if (action & MOUSE_LEFT)
            NextEntry();
        else if (action & MOUSE_RIGHT)
            ForeEntry();
        else if (action & MOUSE_MIDDLE)
            current = nullptr;
    }
    return SIGNAL_OKAY;
}

int ListField::NextEntry(int loop)
{
    FnTrace("ListField::NextEntry()");
    if (current)
        current = current->next;
    if (current == nullptr)
    {
        current = EntryList();
        ++loop;
    }

    if (current == nullptr || loop > 1)
        return 1;
    if (current->active == 0)
        return NextEntry(loop);

    return 0;
}

int ListField::ForeEntry(int loop)
{
    FnTrace("ListField::ForeEntry()");
    if (current)
        current = current->fore;
    if (current == nullptr)
    {
        current = EntryListEnd();
        ++loop;
    }

    if (current == nullptr || loop > 1)
        return 1;
    if (current->active == 0)
        return ForeEntry(loop);

    return 0;
}

int ListField::Set(int v)
{
    FnTrace("ListField::Set()");
    for (ListFieldEntry *lfe = EntryList(); lfe != nullptr; lfe = lfe->next)
    {
        if (lfe->value == v)
        {
            current = lfe;
            return 0;
        }
    }
    return 1;
}

int ListField::Get(int &v)
{
    FnTrace("ListField::Get()");
    if (current)
    {
        v = current->value;
        return 0;
    }
    return 1;
}

int ListField::SetName(Str &set_name)
{
    FnTrace("ListField::SetName()");
    int retval = 1;
    ListFieldEntry *lfe = EntryList();

    while (lfe != nullptr)
    {
        if (strcmp(set_name.Value(), lfe->label.Value()) == 0)
        {
            current = lfe;
            retval = 0;
            lfe = nullptr;
        }
        else
            lfe = lfe->next;
    }

    return retval;
}

int ListField::GetName(Str &get_name)
{
    FnTrace("ListField::GetName()");
    int retval = 1;

    if (current != nullptr)
    {
        get_name.Set(current->label.Value());
        retval = 0;
    }

    return retval;
}

int ListField::SetList(const genericChar* *option_list, int *value_list)
{
    FnTrace("ListField::SetList()");
    ClearEntries();
    if (option_list == nullptr)
        return 0;

    int i = 0;
    while (option_list[i] != nullptr)
    {
        int val = i;
        if (value_list)
            val = value_list[i];
        ListFieldEntry *lfe = new ListFieldEntry(MasterLocale->Translate(option_list[i]), val);
        Add(lfe);
        ++i;
    }
    return 0;
}

int ListField::SetActiveList(int *active_list)
{
    FnTrace("ListField::()SetActiveList");
    ListFieldEntry *lfe = EntryList();
    int i = 0;
    while (lfe)
    {
        lfe->active = active_list[i++];
        lfe = lfe->next;
    }
    return 0;
}

int ListField::Add(ListFieldEntry *lfe)
{
    FnTrace("ListField::Add()");
    return entry_list.AddToTail(lfe);
}

int ListField::ClearEntries()
{
    FnTrace("ListField::ClearEntries()");
    entry_list.Purge();
    current = nullptr;
    return 0;
}

void ListField::Print(void)
{
    printf("List %s:  %s\n", label.Value(), current->label.Value());
}


/**** ButtonField Class ****/
// Constructor
ButtonField::ButtonField(const genericChar* lbl, const genericChar* msg)
{
    label.Set(lbl);
    message.Set(msg);
    lit = 0;
    modify = 1;
}

// Member Functions
int ButtonField::Init(Terminal *term, FormZone *fzone)
{
    FnTrace("ButtonField::()");
    w = fzone->TextWidth(term, label.Value()) + 2;
    h = 2;
    return 0;
}

RenderResult ButtonField::Render(Terminal *term, FormZone *fzone)
{
    FnTrace("ButtonField::Render()");
    int c = color, m = 0;
    if (selected)
        m = PRINT_UNDERLINE;
    if (lit)
        c = COLOR_LT_BLUE;

    fzone->Button(term, x, y, w, selected);
    fzone->TextPosC(term, x + (w / 2), y, label.Value(), c, m);
    return RENDER_OKAY;
}

SignalResult ButtonField::Touch(Terminal *term, FormZone *fzone, Flt tx, Flt ty)
{
    FnTrace("ButtonField::Touch()");
    lit = 1;
    fzone->Draw(term, 0);
    lit = 0;
    fzone->Signal(term, message.Value());
    fzone->Draw(term, 0);
    return SIGNAL_OKAY;
}

SignalResult ButtonField::Keyboard(Terminal *term, FormZone *fzone, int key, int state)
{
    FnTrace("ButtonField::Keyboard()");
    if (key == ' ')
        return Touch(term, fzone, 0, 0);
    return SIGNAL_IGNORED;
}

/**** TemplateField Class ****/
// Constructor
TemplateField::TemplateField(const genericChar* lbl, const genericChar* tmp, Flt min_label)
{
    label.Set(lbl);
    temp.Set(tmp);
    min_label_width = min_label;
    modify = 1;
    cursor = 0;
}

// Member Functions
int TemplateField::Init(Terminal *term, FormZone *fzone)
{
    FnTrace("TemplateField::Init()");
    label_width = fzone->TextWidth(term, label.Value());
    if (label_width < min_label_width)
        label_width = min_label_width;

    h = 2;
    w = label_width + temp.size() + 3.5;
    return 0;
}

RenderResult TemplateField::Render(Terminal *term, FormZone *fzone)
{
    FnTrace("TemplateField::Render()");
    int c = color, m = 0;
    if (selected)
    {
        c = COLOR_LT_BLUE;
        m = PRINT_UNDERLINE;
    }
    fzone->TextPosL(term, x, y, label.Value(), c, m);

    Flt xx = x + label_width + 1;
    if (modify)
        fzone->Entry(term, xx, y, temp.size() + 1.5);

    const genericChar* b = FillTemplate((genericChar*)temp.Value(), buffer.Value());
    fzone->TextPosL(term, xx, y, b, COLOR_WHITE);
    if (selected)
    {
        if (cursor > buffer.size())
            cursor = buffer.size();

        int tp = TemplatePos(temp.Value(), cursor);
        Flt pos = 0;
        if (tp > 0)
            pos = fzone->TextWidth(term, b, tp);
        fzone->Underline(term, xx + pos, y, 1.0, COLOR_YELLOW);
    }
    else
        cursor = buffer.size();
    return RENDER_OKAY;
}

SignalResult TemplateField::Keyboard(Terminal *term, FormZone *fzone, int key, int state)
{
    FnTrace("TemplateField::Keyboard()");
    genericChar str[STRLENGTH], *b = (genericChar*)buffer.Value();

    switch (key)
    {
    case 12:  // ^L - left
        if (cursor <= 0)
            return SIGNAL_IGNORED;
        --cursor;
        return SIGNAL_OKAY;

    case 17:  // ^R - right
        if (cursor >= buffer.size())
            return SIGNAL_IGNORED;
        ++cursor;
        return SIGNAL_OKAY;

    case 8:  // Backspace
        if (cursor > 0)
        {
            --cursor;
            strncpy(str, b, cursor);
            strcpy(&str[cursor], &b[cursor+1]);
            buffer.Set(str);
            return SIGNAL_OKAY;
        }
        return SIGNAL_IGNORED;
    }

    genericChar k = (genericChar) key;
    if ((flag & FF_ONLYDIGITS) && !isdigit(k))
        return SIGNAL_IGNORED;
    if (flag & FF_ALLCAPS)
        k = toupper(k);

    if (isprint(k) && buffer.size() < TemplateBlanks(temp.Value()))
    {
        strncpy(str, b, cursor);
        str[cursor] = k;
        strcpy(&str[cursor+1], &b[cursor]);
        buffer.Set(str);
        ++cursor;
        return SIGNAL_OKAY;
    }
    else
        return SIGNAL_IGNORED;
}
