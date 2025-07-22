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
 * term_dialog.hh - revision 21 (8/25/98)
 * Implementation of edit mode dialogs
 */

#ifndef _TERM_DIALOG_HH
#define _TERM_DIALOG_HH

#include <Xm/Xm.h>
#include "basic.hh"


/**** Types ****/
// Dialog input items
class DialogEntry
{
public:
    Widget container, entry;

    // Constructor
    DialogEntry();

    // Member Functions
    int   Init(Widget parent, const genericChar* label);
    int   Show(int flag);
    int   Set(const char* val);
    int   Set(int val);
    int   Set(Flt val);
    const genericChar* Value();
    int   Get(const char* result);
    int   Get(int &result);
    int   Get(Flt &result);
};

class DialogMenu
{
public:
    Widget  container;
    Widget  option;
    Widget  mlabel; 
    Widget  menu;
    Widget *choice_list;
    int     choice_count;
    int    *value_list;
    Widget  no_change_widget;
    int     no_change_value;

    // Constructor
    DialogMenu();
    // Destructor
    ~DialogMenu();

    // Member Functions
    int Clear();
    int Init(Widget parent, const genericChar* label, const genericChar* *option_name, int *option_value,
             void *option_cb = NULL, void *client_data = NULL);
    int Show(int flag);
    int Set(int val);
    int SetLabel(const char* label);
    int Value();
};

class DialogDoubleMenu
{
public:
    Widget  container;
    Widget  option1;
    Widget  option2;
    Widget *choice1_list;
    Widget *choice2_list;
    int     choice1_count;
    int     choice2_count;
    int    *value1_list;
    int    *value2_list;
    Widget  no_change_widget1;
    Widget  no_change_widget2;
    int     no_change_value;

    // Constructor
    DialogDoubleMenu();
    // Destructor
    ~DialogDoubleMenu();

    int Init(Widget parget, const genericChar* label, const genericChar* *option1_name, int *option1_value,
             const genericChar* *option2_name, int *option2_value);
    int Show(int flag);
    int Set(int o1, int o2);
    int Value(int &v1, int &v2);
};

// Dialog Classes

class PageDialog
{
public:
    Widget dialog;
    int full_edit;
    int page_type;
    int open;

    DialogEntry name;
    DialogEntry id;
    DialogEntry default_spacing;
    DialogEntry parent_page;
    DialogMenu  size;
    DialogMenu  type;
    DialogMenu  type2;
    DialogMenu  title_color;
    DialogMenu  texture;
    DialogMenu  index;
    DialogMenu  default_font;
    DialogMenu  default_color1;
    DialogMenu  default_color2;
    DialogMenu  default_shadow;
    DialogDoubleMenu default_appear1;
    DialogDoubleMenu default_appear2;

    // Constructor
    PageDialog(Widget parent);

    // Member Functions
    int Open();
    int Correct();
    int Close();
    int Send();
};

class ZoneDialog
{
public:
    Widget dialog;
    Widget container;
    int full_edit;
    int ztype;
    int itype;
    int jtype;
    int open;
    int states;

    DialogMenu  type;
    DialogMenu  type2;
    DialogMenu  behave;
    DialogMenu  font;
    DialogMenu  color1;
    DialogMenu  color2;
    DialogMenu  color3;
    DialogMenu  shape;
    DialogMenu  shadow;
    DialogMenu  item_type;
    DialogMenu  item_family;
    DialogMenu  item_sales;
    DialogMenu  item_printer;
    DialogMenu  item_order;
    DialogMenu  tender_type;
    DialogMenu  report_type;
    DialogMenu  report_print;
    DialogMenu  qualifier;
    DialogMenu  switch_type;
    DialogMenu  jump_type;
    DialogMenu  jump_type2;
    DialogMenu  customer_type;
    DialogMenu  video_target;
    DialogMenu  drawer_zone_type;
    DialogMenu  confirm;
    DialogEntry confirm_msg;
    DialogEntry name;
    DialogEntry page;
    DialogEntry group;
    DialogEntry expression;
    DialogEntry message;
    DialogEntry filename;
    DialogEntry item_name;
    DialogEntry item_print_name;
    DialogEntry item_zone_name;
    DialogEntry item_location;
    DialogEntry item_event_time;
    DialogEntry item_total_tickets;
    DialogEntry item_available_tickets;
    DialogEntry item_price_label;
    DialogEntry item_price;
    DialogEntry item_subprice;
    DialogEntry item_employee_price;
    DialogEntry tender_amount;
    DialogEntry page_list;
    DialogEntry spacing;
    DialogEntry amount;
    DialogEntry jump_id;
    DialogEntry key;
    DialogEntry check_disp_num;
    DialogDoubleMenu appear1;
    DialogDoubleMenu appear2;
    DialogDoubleMenu appear3;

    // Constructor
    ZoneDialog(Widget parent);

    // Member Functions
    int Open();
    int Correct();
    int Close();
    int Send();
};

class DefaultDialog
{
public:
    Widget dialog;
    int full_edit;
    int open;

    DialogEntry default_spacing;
    DialogMenu  size;
    DialogMenu  title_color;
    DialogMenu  texture;
    DialogMenu  default_font;
    DialogMenu  default_color1;
    DialogMenu  default_color2;
    DialogMenu  default_shadow;
    DialogDoubleMenu default_appear1;
    DialogDoubleMenu default_appear2;

    // Constructor
    DefaultDialog(Widget parent);

    // Member Functions
    int Open();
    int Close();
    int Send();
};


class MultiZoneDialog
{
public:
    Widget dialog;
    int open;
    DialogMenu behave;
    DialogMenu font;
    DialogMenu color1;
    DialogMenu color2;
    DialogMenu shape;
    DialogMenu shadow;
    DialogDoubleMenu appear1;
    DialogDoubleMenu appear2;

    // Constructor
    MultiZoneDialog(Widget parent);

    // Member functions
    int Open();
    int Close();
    int Send();
};

class TranslateDialog
{
public:
    Widget dialog;
    int open;
    int count;
    DialogEntry original;
    DialogEntry translation;

    // Constructor
    TranslateDialog(Widget parent);

    // Member Functions
    int Open();
    int Close();
    int Send();
};

class ListDialog
{
public:
    Widget dialog;
    Widget list;
    int open;
    int selected;
    int items;

    // Constructor
    ListDialog(Widget parent);

    // Member Functions
    int Start();
    int ReadItem();
    int End();
    int Close();
    int Send();
};

#endif
