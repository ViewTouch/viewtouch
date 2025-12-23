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
 * form_zone.hh - revision 72 (10/6/98)
 * base touch zone for data entry and display
 */

#ifndef FORM_ZONE_HH
#define FORM_ZONE_HH

#include "layout_zone.hh"
#include "report.hh"

/**** Definitions ****/
#define FF_ALLCAPS    1
#define FF_ONLYDIGITS 2
#define FF_MONEY      4


/**** Types ****/
class Terminal;
class FormZone;
class FormField;

class FormZone : public LayoutZone
{
    DList<FormField> field_list;

public:
    FormField *keyboard_focus; // current focus - field that reads keyboard
    Flt        form_header;    // header lines given to form
    Flt        form_spacing;   // spacing given to form
    short      keep_focus;     // boolean - use field focus?
    short      wrap;           // boolean - use word wrap?
    short      no_line;        // turn off line at top of form
    short      current_align;
    short      current_color;
    short      pad;
    // the following variables are mostly for ListFormZone, but sometimes
    // need to be callable by member functions.  So we put them here to
    // avoid inheritance problems.
    int        record_no;      // current record number
    int        records;        // current number of records
    int        show_list;      // boolean - is record list being shown?

    // Constructor
    FormZone();

    // Member Functions
    FormField *FieldList()    { return field_list.Head(); }
    FormField *FieldListEnd() { return field_list.Tail(); }

    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Keyboard(Terminal *term, int key, int state) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Mouse(Terminal *term, int action, int mx, int my) override;

    virtual int LoadRecord(Terminal *term, int record) = 0;
    virtual int SaveRecord(Terminal *term, int record, int write_file) = 0;
    virtual int UpdateForm(Terminal *term, int record)         { return 1; }
    virtual int NewRecord(Terminal *term)                      { return 1; }
    virtual int KillRecord(Terminal *term, int record)         { return 1; }
    virtual int PrintRecord(Terminal *term, int record)        { return 1; }
    virtual int Search(Terminal *term, int record, const genericChar* word) { return 0; }
    virtual int RecordCount(Terminal *term)                    { return 1; }

    int AddLabel(const genericChar* label, Flt min_width = 0);
    int AddSubmit(const genericChar* label, Flt min_width = 0);
    int AddTextField(const genericChar* label, int max_len, int mod = 1, Flt min_label = 0);
    int AddTimeField(const genericChar* l, int mod = 1, int can_unset = 1);
    int AddDateField(const genericChar* l, int mod = 1, int can_unset = 1);
    int AddTimeDateField(const genericChar* l, int mod = 1, int can_unset = 1);
    int AddTimeDayField(const genericChar* l, int mod = 1, int can_unset = 1);
    int AddWeekDayField(const genericChar* l, int mod = 1);
    int AddListField( const genericChar* label, const genericChar** item_array, int* value_array = nullptr, Flt min1 = 0, Flt min2 = 0 );
    int AddButtonField(const genericChar* label, const genericChar* message);
    int AddTemplateField(const genericChar* label, const genericChar* temp, Flt min_label = 0);
    int AddNewLine(int lines = 1);
    int AddSpace(Flt s);
    int SetFlag(int value);
    int SetNumRange(int lo, int hi);
    int Color(int c);
    int Center();
    int LeftAlign();
    int RightAlign();
    int Add(FormField *fe);
    int Remove(FormField *fe);
    void Purge();
    int LayoutForm(Terminal *term);
    int NextField();
    int ForeField();
    int FirstField();
    int LastField();
    FormField *Find(Flt px, Flt py);
};

class ListFormZone : public FormZone
{
public:
    Report list_report;    // report of list contents
    int    list_page;      // page of list shown
    Flt    list_header;    // header lines given to list report
    Flt    list_footer;    // footer lines given to list report
    Flt    list_spacing;   // spacing for report list
    // Constructor
    ListFormZone();

    // Member Functions
    RenderResult Render(Terminal *term, int update_flag) override;
    SignalResult Signal(Terminal *term, const genericChar* message) override;
    SignalResult Keyboard(Terminal *term, int key, int state) override;
    SignalResult Touch(Terminal *term, int tx, int ty) override;
    SignalResult Mouse(Terminal *term, int action, int mx, int my) override;
    int          Update(Terminal *term, int update_message, const genericChar* value) override;

    virtual int ListReport(Terminal *term, Report *report) = 0;
};

class FormField
{
public:
    FormField *next, *fore;

    // Properties
    Str   label;
    short align;
    short color;
    short modify;
    short new_line;
    int   flag;
    int   lo_value;  // numeric field cannot have value lower than this
    int   hi_value;  // same for high end

    // State
    Flt   x, y, w, h, pad;
    short selected;
    short active;

    // Constructor
    FormField();
    // Destructor
    virtual ~FormField() = default;

    // Member Functions
    int Draw(Terminal *term, FormZone *z);

    virtual int          Init(Terminal *term, FormZone *z) = 0;
    virtual RenderResult Render(Terminal *term, FormZone *z) = 0;
    virtual SignalResult Keyboard(Terminal *term, FormZone *z, int key, int state);
    virtual SignalResult Touch(Terminal *term, FormZone *z, Flt tx, Flt ty);
    virtual SignalResult Mouse(Terminal *term, FormZone *z,
                               int action, Flt mx, Flt my);

    virtual int Set(const genericChar* v)     { return 1; }
    virtual int Set(Str  &v)     { return 1; }
    virtual int Set(int   v)     { return 1; }
    virtual int Set(Flt   v)     { return 1; }
    virtual int Set(TimeInfo &term) { return 1; }
    virtual int Set(TimeInfo *term) { return 1; }
    virtual int SetList(const genericChar* *options, int *values) { return 1; }
    virtual int SetActiveList(int *list)             { return 1; }
    virtual int SetNumRange(int lo, int hi)          { return 1; }
    virtual int SetName(Str &set_name) { return 1; }
    virtual int IsSet() { return 0; }

    virtual int Append(const genericChar* str)    { return 1; }
    virtual int Append(Str &str)            { return 1; }
    virtual int Append(int val)             { return 1; }
    virtual int Append(Flt val)             { return 1; }
    virtual int Append(genericChar key)     { return 1; }

    virtual int Remove(int num = 1)  { return 1; }
    virtual int Clear()              { return 1; }

    virtual int Get(genericChar* v, int len) { return 1; }
    virtual int Get(genericChar* v)          { return 1; }
    virtual int Get(Str &v)           { return 1; }
    virtual int Get(int &v)           { return 1; }
    virtual int Get(Flt &v)           { return 1; }
    virtual int Get(TimeInfo &term)      { return 1; }
    virtual int GetPrice(int &v)      { return 1; }
    virtual int GetName(Str &get_name) { return 1; }

    virtual int ClearEntries()                  { return 1; }
    virtual int AddEntry(const genericChar* name, int value) { return 1; }

    virtual void Print(void);   // debug function, not for live code
};

#endif // FORM_ZONE_HH
