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
 * term_dialog.cc - revision 44 (9/1/98)
 * Implementation of term_dialog module
 */

#include "report.hh"
#include "utility.hh"
#include "term_dialog.hh"
#include "term_view.hh"
#include "remote_link.hh"
#include <Xm/MwmUtil.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/List.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>
#include <Xm/ToggleB.h>
#include <X11/keysym.h>
#include "labels.hh"
#include "image_data.hh"
#include "sales.hh"
#include "pos_zone.hh"

#include <iostream>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/**** Definitions ****/
// Jump Values
#define JUMP_NONE       0  // Don't jump
#define JUMP_NORMAL     1  // Jump to page, push current page onto stack
#define JUMP_STEALTH    2  // Jump to page (don't put current page on stack)
#define JUMP_RETURN     3  // Pop page off stack, jump to it
#define JUMP_HOME       4  // Jump to employee home page
#define JUMP_SCRIPT     5  // Jump to next page in script
#define JUMP_INDEX      6  // Jump to current page's index
#define JUMP_PASSWORD   7  // Like JUMP_NORMAL but password must be entered

// Other Definitions
#define MARGIN 43

const char* *translations = NULL;

/*********************************************************************
 * Functions
 ********************************************************************/

Widget AddLine(Widget parent)
{
    return XtVaCreateManagedWidget("line", xmSeparatorWidgetClass, parent,
                                   XmNleftAttachment,  XmATTACH_FORM,
                                   XmNrightAttachment, XmATTACH_FORM, NULL);
}

int AddButtons(Widget parent, XtCallbackProc okay_cb, XtCallbackProc delete_cb,
               XtCallbackProc cancel_cb, void *client_data = NULL)
{
    Widget f = XtVaCreateWidget("form", xmFormWidgetClass, parent,
                                XmNleftAttachment,  XmATTACH_WIDGET,
                                XmNleftWidget,      parent,
                                XmNrightAttachment, XmATTACH_WIDGET,
                                XmNrightWidget,     parent, NULL);

    if (okay_cb)
    {
        Widget w = XtVaCreateManagedWidget("Okay", xmPushButtonWidgetClass, f,
                                           XmNleftAttachment,   XmATTACH_FORM,
                                           XmNrightAttachment,  XmATTACH_POSITION,
                                           XmNrightPosition,    32, NULL);
        XtAddCallback(w, XmNactivateCallback, (XtCallbackProc) okay_cb,
                      (XtPointer) client_data);
    }

    if (delete_cb)
    {
        Widget w = XtVaCreateManagedWidget("Delete", xmPushButtonWidgetClass, f,
                                           XmNleftAttachment,   XmATTACH_POSITION,
                                           XmNleftPosition,     34,
                                           XmNrightAttachment,  XmATTACH_POSITION,
                                           XmNrightPosition,    66, NULL);
        XtAddCallback(w, XmNactivateCallback, (XtCallbackProc) delete_cb,
                      (XtPointer) client_data);
    }

    if (cancel_cb)
    {
        Widget w = XtVaCreateManagedWidget("Cancel", xmPushButtonWidgetClass, f,
                                           XmNleftAttachment,   XmATTACH_POSITION,
                                           XmNleftPosition,     68,
                                           XmNrightAttachment,  XmATTACH_FORM, NULL);
        XtAddCallback(w, XmNactivateCallback, (XtCallbackProc) cancel_cb,
                      (XtPointer) client_data);
    }

    XtManageChild(f);
    return 0;
}


/*********************************************************************
 * DialogEntry Class
 ********************************************************************/

DialogEntry::DialogEntry()
{
    container = 0;
    entry     = 0;
}

// Member Functions
int DialogEntry::Init(Widget parent, const char* label)
{
    if (entry || container)
        return 1;

    container = XtVaCreateWidget("form",
                                 xmFormWidgetClass,   parent,
                                 XmNleftAttachment,   XmATTACH_WIDGET,
                                 XmNleftWidget,       parent,
                                 XmNrightAttachment,  XmATTACH_WIDGET,
                                 XmNrightWidget,      parent, NULL);

    XtVaCreateManagedWidget(label, xmLabelWidgetClass, container,
                            XmNtopAttachment,    XmATTACH_FORM,
                            XmNleftAttachment,   XmATTACH_FORM,
                            XmNrightAttachment,  XmATTACH_POSITION,
                            XmNrightPosition,    MARGIN+1,
                            XmNbottomAttachment, XmATTACH_FORM, NULL);

    entry = XtVaCreateManagedWidget("entry", xmTextWidgetClass, container,
                                    XmNtopAttachment,    XmATTACH_FORM,
                                    XmNleftAttachment,   XmATTACH_POSITION,
                                    XmNleftPosition,     MARGIN+1,
                                    XmNrightAttachment,  XmATTACH_FORM,
                                    XmNbottomAttachment, XmATTACH_FORM, NULL);

    XmTextSetString(entry, (char*)"");
    XtManageChild(container);
    return 0;
}

int DialogEntry::Show(int flag)
{
    if (flag)
    {
        if (!XtIsManaged(container))
            XtManageChild(container);
    }
    else
    {
        if (XtIsManaged(container))
            XtUnmanageChild(container);
    }
    return 0;
}

int DialogEntry::Set(const char* val)
{
    XmTextSetString(entry, (char*)val);
    return 0;
}

int DialogEntry::Set(int val)
{
    char str[32];
    sprintf(str, "%d", val);
    XmTextSetString(entry, str);
    return 0;
}

int DialogEntry::Set(Flt val)
{
    char str[32];
    sprintf(str, "%g", val);
    XmTextSetString(entry, str);
    return 0;
}

const char* DialogEntry::Value()
{
    return XmTextGetString(entry);
}

int DialogEntry::Get(const char* str)
{
    strcpy(XmTextGetString(entry), str);
    return 0;
}

int DialogEntry::Get(int &result)
{
    return (sscanf(XmTextGetString(entry), "%d", &result) != 1);
}

int DialogEntry::Get(Flt &result)
{
    return (sscanf(XmTextGetString(entry), "%lf", &result) != 1);
}


/*********************************************************************
 * DialogMenu Class
 ********************************************************************/

DialogMenu::DialogMenu()
{
    choice_list  = NULL;
    choice_count = 0;
    value_list   = NULL;
    no_change_widget = 0;
    no_change_value  = 0;
    container = NULL;
}

DialogMenu::~DialogMenu()
{
    if (choice_list)
        delete [] choice_list;
}

// Member Functions

/****
 * Clear:  Do not call this function unless the program is exiting or
 *  Init will be called immediately after.
 ****/
int DialogMenu::Clear()
{
    XtDestroyWidget(mlabel);
    XtDestroyWidget(menu);
    XtDestroyWidget(option);
    if (choice_list)
    {
        delete [] choice_list;
        choice_list = NULL;
    }
    return 0;
}

int DialogMenu::Init(Widget parent, const char* label, const char* *option_name,
                     int *option_value, void *option_cb, void *client_data)
{
    const char* name;

    if (container == NULL)
    {
        container = XtVaCreateWidget("form",
                                     xmFormWidgetClass,   parent,
                                     XmNleftAttachment,   XmATTACH_POSITION,
                                     XmNleftPosition,     1,
                                     XmNrightAttachment,  XmATTACH_POSITION,
                                     XmNrightPosition,    99,
                                     NULL);
    }
    mlabel = XtVaCreateManagedWidget(label,
                                     xmLabelWidgetClass,  container,
                                     XmNtopAttachment,    XmATTACH_FORM,
                                     XmNbottomAttachment, XmATTACH_FORM,
                                     XmNleftAttachment,   XmATTACH_FORM,
                                     XmNrightAttachment,  XmATTACH_POSITION,
                                     XmNrightPosition,    MARGIN,
                                     NULL);

    Arg args[5];
    menu = XmCreatePulldownMenu(container, (char*)"menu", args, 0);
    XtSetArg(args[0], XmNsubMenuId,        menu);
    XtSetArg(args[1], XmNtopAttachment,    XmATTACH_FORM);
    XtSetArg(args[2], XmNbottomAttachment, XmATTACH_FORM);
    XtSetArg(args[3], XmNleftAttachment,   XmATTACH_POSITION);
    XtSetArg(args[4], XmNleftPosition,     MARGIN);
    option = XmCreateOptionMenu(container, (char*)"option", args, 5);

    int count = 0;
    while (option_name[count])
        ++count;

    choice_count = count;
    choice_list  = new Widget[count];
    value_list   = option_value;

    if (no_change_value)
    {
        no_change_widget = XtVaCreateManagedWidget("** No Change **",
                                                   xmPushButtonWidgetClass, menu, NULL);
        if (option_cb)
            XtAddCallback(no_change_widget, XmNactivateCallback,
                          (XtCallbackProc) option_cb, (XtPointer) client_data);
    }

    for (int i = 0; i < count; ++i)
    {
        name = MasterTranslations.GetTranslation(option_name[i]);
        choice_list[i] = XtVaCreateManagedWidget(name, xmPushButtonWidgetClass, menu, NULL);
        if (option_cb)
            XtAddCallback(choice_list[i], XmNactivateCallback,
                          (XtCallbackProc) option_cb, (XtPointer) client_data);
    }

    XtManageChild(option);
    XtManageChild(container);
    return 0;
}

int DialogMenu::Show(int flag)
{
    if (flag)
    {
        if (!XtIsManaged(container))
            XtManageChild(container);
    }
    else
    {
        if (XtIsManaged(container))
            XtUnmanageChild(container);
    }
    return 0;
}

int DialogMenu::Set(int value)
{
    if (no_change_widget && (value == no_change_value))
        XtVaSetValues(option, XmNmenuHistory, no_change_widget, NULL);
    else
    {
        int idx = CompareList(value, value_list, 0);
        XtVaSetValues(option, XmNmenuHistory, choice_list[idx], NULL);
    }
    return 0;
}

int DialogMenu::SetLabel(const char* label)
{
    XmString label_string;

    label_string = XmStringCreateLtoR((char*)label, XmFONTLIST_DEFAULT_TAG);
    XtVaSetValues(mlabel, XmNlabelString, label_string, NULL);
    XmStringFree(label_string);

    return 0;
}

int DialogMenu::Value()
{
    Widget choice;
    XtVaGetValues(option, XmNmenuHistory, &choice, NULL);
    if (no_change_widget == choice)
        return no_change_value;

    for (int i = 0; i < choice_count; ++i)
        if (choice == choice_list[i])
            return value_list[i];
    return -1;
}

/*********************************************************************
 * DialogDoubleMenu Class
 ********************************************************************/
DialogDoubleMenu::DialogDoubleMenu()
{
    choice1_list      = NULL;
    choice1_count     = 0;
    choice2_list      = NULL;
    choice2_count     = 0;
    value1_list       = NULL;
    value2_list       = NULL;
    no_change_widget1 = 0;
    no_change_widget2 = 0;
    no_change_value   = 0;
}

DialogDoubleMenu::~DialogDoubleMenu()
{
    if (choice1_list)
        delete [] choice1_list;
    if (choice2_list)
        delete [] choice2_list;
}

// Member Functions
int DialogDoubleMenu::Init(Widget parent, const char* label,
                           const char* *op1_name, int *op1_value, const char* *op2_name, int *op2_value)
{
    int i;

    container = XtVaCreateWidget("form", xmFormWidgetClass, parent,
                                 XmNleftAttachment,   XmATTACH_POSITION,
                                 XmNleftPosition,     1,
                                 XmNrightAttachment,  XmATTACH_POSITION,
                                 XmNrightPosition,    99,
                                 NULL);
    XtVaCreateManagedWidget(label, xmLabelWidgetClass, container,
                            XmNtopAttachment,    XmATTACH_FORM,
                            XmNbottomAttachment, XmATTACH_FORM,
                            XmNleftAttachment,   XmATTACH_FORM,
                            XmNrightAttachment,  XmATTACH_POSITION,
                            XmNrightPosition,    MARGIN,
                            NULL);

    Arg args[5];
    Widget menu1 = XmCreatePulldownMenu(container, (char*)"menu1", args, 0);
    XtSetArg(args[0], XmNsubMenuId,        menu1);
    XtSetArg(args[1], XmNtopAttachment,    XmATTACH_FORM);
    XtSetArg(args[2], XmNbottomAttachment, XmATTACH_FORM);
    XtSetArg(args[3], XmNleftAttachment,   XmATTACH_POSITION);
    XtSetArg(args[4], XmNleftPosition,     MARGIN);
    option1 = XmCreateOptionMenu(container, (char*)"option1", args, 5);

    Widget menu2 = XmCreatePulldownMenu(container, (char*)"menu2", args, 0);
    XtSetArg(args[0], XmNsubMenuId,        menu2);
    XtSetArg(args[1], XmNtopAttachment,    XmATTACH_FORM);
    XtSetArg(args[2], XmNbottomAttachment, XmATTACH_FORM);
    XtSetArg(args[3], XmNleftAttachment,   XmATTACH_WIDGET);
    XtSetArg(args[4], XmNleftWidget,       option1);
    option2 = XmCreateOptionMenu(container, (char*)"option2", args, 5);

    int count = 0;
    while (op1_name[count])
        ++count;
    choice1_count = count;
    choice1_list  = new Widget[count];
    value1_list   = op1_value;

    count = 0;
    while (op2_name[count])
        ++count;
    choice2_count = count;
    choice2_list  = new Widget[count];
    value2_list   = op2_value;

    if (no_change_value)
    {
        no_change_widget1 = XtVaCreateManagedWidget("** No Change **",
                                                    xmPushButtonWidgetClass, menu1, NULL);
        no_change_widget2 = XtVaCreateManagedWidget("** No Change **",
                                                    xmPushButtonWidgetClass, menu2, NULL);
    }

    for (i = 0; i < choice1_count; ++i)
        choice1_list[i] = XtVaCreateManagedWidget(op1_name[i],
                                                  xmPushButtonWidgetClass, menu1, NULL);
    for (i = 0; i < choice2_count; ++i)
        choice2_list[i] = XtVaCreateManagedWidget(op2_name[i],
                                                  xmPushButtonWidgetClass, menu2, NULL);

    XtManageChild(option1);
    XtManageChild(option2);
    XtManageChild(container);
    return 0;
}

int DialogDoubleMenu::Show(int flag)
{
    if (flag)
    {
        if (!XtIsManaged(container))
            XtManageChild(container);
    }
    else
    {
        if (XtIsManaged(container))
            XtUnmanageChild(container);
    }
    return 0;
}

int DialogDoubleMenu::Set(int v1, int v2)
{
    int idx;
    if (no_change_widget1 && v1 == no_change_value)
        XtVaSetValues(option1, XmNmenuHistory, no_change_widget1, NULL);
    else
    {
        idx = CompareList(v1, value1_list, 0);
        XtVaSetValues(option1, XmNmenuHistory, choice1_list[idx], NULL);
    }

    if (no_change_widget2 && v2 == no_change_value)
        XtVaSetValues(option2, XmNmenuHistory, no_change_widget2, NULL);
    else
    {
        idx = CompareList(v2, value2_list, 0);
        XtVaSetValues(option2, XmNmenuHistory, choice2_list[idx], NULL);
    }
    return 0;
}

int DialogDoubleMenu::Value(int &v1, int &v2)
{
    Widget choice;
    // Default the args.  If they don't get set later, we'll know
    // there was a problem.
    v1 = -1;
    v2 = -1;

    XtVaGetValues(option1, XmNmenuHistory, &choice, NULL);
    if (no_change_widget1 == choice)
    {
        v1 = no_change_value;
    }
    else
    {
        for (int i = 0; i < choice1_count; ++i)
            if (choice == choice1_list[i])
            {
                v1 = value1_list[i];
                break;
            }
    }

    XtVaGetValues(option2, XmNmenuHistory, &choice, NULL);
    if (no_change_widget2 == choice)
    {
        v2 = no_change_value;
    }
    else
    {
        for (int i = 0; i < choice2_count; ++i)
            if (choice == choice2_list[i])
            {
                v2 = value2_list[i];
                break;
            }
    }
    return 0;
}

/*********************************************************************
 * PageDialog Class
 ********************************************************************/

// Callback Functions
void EP_OkayCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    PageDialog *d = (PageDialog *) client_data;
    d->Close();
    d->Send();
}

void EP_DeleteCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    PageDialog *d = (PageDialog *) client_data;
    d->Close();
    WInt8(SERVER_KILLPAGE);
    SendNow();
}

void EP_CancelCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    PageDialog *d = (PageDialog *) client_data;
    d->Close();
}

void EP_TypeCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    PageDialog *d = (PageDialog *) client_data;
    int new_type = 0;
    if (d->full_edit)
        new_type = d->type.Value();
    else
        new_type = d->type2.Value();

    if (new_type == d->page_type)
        return;

    XtUnmanageChild(d->dialog);
    d->page_type = new_type;
    d->Correct();
    XtManageChild(d->dialog);
}

// Constructor
PageDialog::PageDialog(Widget parent)
{
    full_edit = 0;
    page_type = 0;
    open      = 0;

    Arg args[9];
    XtSetArg(args[0], XmNtitle, "Page Properties");
    XtSetArg(args[1], XmNmwmDecorations, MWM_DECOR_ALL | MWM_DECOR_MENU);
    XtSetArg(args[2], XmNmwmFunctions, MWM_FUNC_ALL | MWM_FUNC_CLOSE);
    dialog = XmCreateFormDialog(parent, (char*)"page dialog", args, 3);

    XtSetArg(args[0], XmNorientation,      XmVERTICAL);
    XtSetArg(args[1], XmNtopAttachment,    XmATTACH_POSITION);
    XtSetArg(args[2], XmNtopPosition,      1);
    XtSetArg(args[3], XmNbottomAttachment, XmATTACH_POSITION);
    XtSetArg(args[4], XmNbottomPosition,   99);
    XtSetArg(args[5], XmNleftAttachment,   XmATTACH_POSITION);
    XtSetArg(args[6], XmNleftPosition,     1);
    XtSetArg(args[7], XmNrightAttachment,  XmATTACH_POSITION);
    XtSetArg(args[8], XmNrightPosition,    99);
    Widget w = XmCreateRowColumn(dialog, (char*)"", args, 9);

    size.Init(w, "Page Size", PageSizeName, PageSizeValue);
    type.Init(w, "Page Type", PageTypeName, PageTypeValue,
              (void *) EP_TypeCB, this);
    type2.Init(w, "Page Type", PageType2Name, PageType2Value,
               (void *) EP_TypeCB, this);
    AddLine(w);

    name.Init(w, "Name");
    id.Init(w, "ID");
    title_color.Init(w, "Title Bar Color", ColorName, ColorValue);
    texture.Init(w, "Background Texture", &TextureName[1], &TextureValue[1]);
    AddLine(w);

    default_font.Init(w, "Default Font", FontName, FontValue);
    default_appear1.Init(w, "Default Appearance",
                         &ZoneFrameName[1], &ZoneFrameValue[1], &TextureName[1], &TextureValue[1]);
    default_color1.Init(w, "Default Color", ColorName, ColorValue);
    default_appear2.Init(w, "Default Selected Appearance",
                         &ZoneFrameName[1], &ZoneFrameValue[1], &TextureName[1], &TextureValue[1]);
    default_color2.Init(w, "Default Selected Color", ColorName, ColorValue);
    default_spacing.Init(w, "Default Spacing");
    default_shadow.Init(w, "Default Shadow", PageShadowName, PageShadowValue);
    AddLine(w);

    parent_page.Init(w, "Parent ID");
    index.Init(w, "Index Type", IndexName, IndexValue);
    AddLine(w);
    AddButtons(w, EP_OkayCB, EP_DeleteCB, EP_CancelCB, this);
    XtManageChild(w);
}

// Member Functions
int PageDialog::Open()
{
    int v1, v2;

    // Load data
    open = 1;
    full_edit = RInt8();
    size.Set(RInt8());
    page_type = RInt8();
    if (full_edit)
        type.Set(page_type);
    else
        type2.Set(page_type);
    name.Set(RStr());
    id.Set(RInt32());
    title_color.Set(RInt8());
    texture.Set(RInt8());
    default_font.Set(RInt8());
    v1 = RInt8();
    v2 = RInt8();
    default_appear1.Set(v1, v2);
    default_color1.Set(RInt8());
    v1 = RInt8();
    v2 = RInt8();
    default_appear2.Set(v1, v2);
    default_color2.Set(RInt8());
    RInt8(); RInt8(); RInt8();
    default_spacing.Set(RInt8());
    default_shadow.Set(RInt16());
    parent_page.Set(RInt32());
    index.Set(RInt8());

    // Show proper fields
    size.Show(1);
    type.Show(full_edit);
    type2.Show(!full_edit);
    name.Show(1);
    id.Show(1);
    title_color.Show(1);
    texture.Show(1);
    default_font.Show(1);
    default_appear1.Show(1);
    default_color1.Show(1);
    default_appear2.Show(1);
    default_color2.Show(1);
    default_spacing.Show(1);
    default_shadow.Show(1);
    Correct();

    // no page translations for now, but we'll reset anyway
    new_page_translations = 0;

    XtManageChild(dialog);
    return 0;
}

int PageDialog::Correct()
{
    parent_page.Show(page_type == PAGE_SYSTEM || page_type == PAGE_CHECKS);
    index.Show(page_type == PAGE_INDEX);
    return 0;
}

int PageDialog::Close()
{
    open = 0;
    if (XtIsManaged(dialog))
        XtUnmanageChild(dialog);
    return 0;
}

int PageDialog::Send()
{
    int tmp, v1, v2;
    WInt8(SERVER_PAGEDATA);
    WInt8(size.Value());
    WInt8(page_type);
    WStr(name.Value());
    id.Get(tmp); WInt32(tmp);
    WInt8(title_color.Value());
    WInt8(texture.Value());
    WInt8(default_font.Value());

    // 0
    default_appear1.Value(v1, v2);
    WInt8(v1); WInt8(v2);
    WInt8(default_color1.Value());
    // 1
    default_appear2.Value(v1, v2);
    WInt8(v1); WInt8(v2);
    WInt8(default_color2.Value());
    // 2
    WInt8(ZF_HIDDEN); WInt8(IMAGE_SAND);
    WInt8(COLOR_DEFAULT);

    default_spacing.Get(tmp); WInt8(tmp);
    WInt16(default_shadow.Value());
    parent_page.Get(tmp); WInt32(tmp);
    WInt8(index.Value());
    return SendNow();
}


/*********************************************************************
 * ZoneDialog Class
 ********************************************************************/

void EZ_OkayCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    ZoneDialog *d = (ZoneDialog *) client_data;
    d->Close();
    d->Send();
}

void EZ_DeleteCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    ZoneDialog *d = (ZoneDialog *) client_data;
    d->Close();
    WInt8(SERVER_KILLZONE);
    SendNow();
}

void EZ_CancelCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    ZoneDialog *d = (ZoneDialog *) client_data;
    d->Close();
}

void EZ_TypeCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    ZoneDialog *d = (ZoneDialog *) client_data;
    int ztype = 0, itype = 0;
    if (d->full_edit)
        ztype = d->type.Value();
    else
        ztype = d->type2.Value();
    itype = d->item_type.Value();

    if (ztype == d->ztype && itype == d->itype)
        return;

    XtUnmanageChild(d->dialog);
    d->ztype = ztype;
    d->itype = itype;
    d->Correct();
    XtManageChild(d->dialog);
}

void EZ_JumpCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    ZoneDialog *d = (ZoneDialog *) client_data;
    int jtype = 0;
    if (d->full_edit)
        jtype = d->jump_type.Value();
    else
        jtype = d->jump_type2.Value();

    int old_show = 0, new_show = 0;
    if (d->jtype == JUMP_NORMAL || d->jtype == JUMP_STEALTH ||
        d->jtype == JUMP_PASSWORD)
        old_show = 1;
    if (jtype == JUMP_NORMAL || jtype == JUMP_STEALTH || jtype == JUMP_PASSWORD)
        new_show = 1;

    d->jtype = jtype;
    if (old_show != new_show)
        d->jump_id.Show(new_show);
}

void EZ_CorrectCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    ZoneDialog *d = (ZoneDialog *) client_data;
    d->Correct();
}

// Constructor
ZoneDialog::ZoneDialog(Widget parent)
{
    full_edit = 0;
    ztype = 0;
    itype = 0;
    jtype = 0;
    open  = 0;
    states = 2;

    Arg args[6];
    XtSetArg(args[0], XmNtitle, "Button Properties");
    XtSetArg(args[1], XmNmwmDecorations, MWM_DECOR_ALL | MWM_DECOR_MENU);
    XtSetArg(args[2], XmNmwmFunctions, MWM_FUNC_ALL | MWM_FUNC_CLOSE);
    dialog = XmCreateFormDialog(parent, (char*)"singlezone dialog", args, 3);

    XtSetArg(args[0], XmNorientation,      XmVERTICAL);
    XtSetArg(args[1], XmNtopAttachment,    XmATTACH_FORM);
    XtSetArg(args[2], XmNbottomAttachment, XmATTACH_FORM);
    XtSetArg(args[3], XmNleftAttachment,   XmATTACH_FORM);
    XtSetArg(args[4], XmNrightAttachment,  XmATTACH_FORM);
    XtSetArg(args[5], XmNpacking,          XmPACK_TIGHT);
    container = XmCreateRowColumn(dialog, (char*)"", args, 6);

    type.Init(container, "Type", FullZoneTypeName, FullZoneTypeValue,
              (void *) EZ_TypeCB, this);
    type2.Init(container, "Type", ZoneTypeName, ZoneTypeValue,
               (void *) EZ_TypeCB, this);
    name.Init(container, "Button Name");
    page.Init(container, "Page Location");
    group.Init(container, "Group ID");
    AddLine(container);

    behave.Init(container, "Behavior", ZoneBehaveName, ZoneBehaveValue);
    confirm.Init(container, "Confirmation", YesNoName, YesNoValue);
    confirm_msg.Init(container, "Confirmation Message");
    font.Init(container, "Font", FontName, FontValue);
    appear1.Init(container, "Appearance", ZoneFrameName, ZoneFrameValue,
                 TextureName, TextureValue);
    color1.Init(container, "Text Color", ColorName, ColorValue);
    appear2.Init(container, "Selected Appearance", ZoneFrameName, ZoneFrameValue,
                 TextureName, TextureValue);
    color2.Init(container, "Selected Text Color", ColorName, ColorValue);
    appear3.Init(container, "Disabled Appearance", ZoneFrameName, ZoneFrameValue,
                 TextureName, TextureValue);
    color3.Init(container, "Disabled Text Color", ColorName, ColorValue);
    shape.Init(container, "Button Shape", ShapeName, ShapeValue);
    shadow.Init(container, "Shadow Thickness", ShadowName, ShadowValue);
    key.Init(container, "Keyboard Shortcut");
    AddLine(container);

    drawer_zone_type.Init(container, "Drawer Button Type", DrawerZoneTypeName, DrawerZoneTypeValue);
    expression.Init(container, "Expression");
    message.Init(container, "Message");
    filename.Init(container, "File Name");
    item_name.Init(container, "Formal Name");
    item_zone_name.Init(container, "Screen Name (if different)");
    item_print_name.Init(container, "Short Name (if different)");
    item_type.Init(container, "Item Type", ItemTypeName, ItemTypeValue,
                   (void *) EZ_TypeCB, this);
    item_price.Init(container, "Price");
    item_subprice.Init(container, "Substitute Price");
    item_employee_price.Init(container, "Employee Price");
    item_family.Init(container, "Family", FamilyName, FamilyValue);
    item_sales.Init(container, "Tax/Discount Category", SalesTypeName, SalesTypeValue);
    item_printer.Init(container, "Printer Destination",
                      PrinterIDName, PrinterIDValue);
    item_order.Init(container, "Call Order", CallOrderName, CallOrderValue);
    tender_type.Init(container, "Tender Type", TenderName, TenderValue);
    tender_amount.Init(container, "Tender Amount");
    report_type.Init(container, "Report Type", ReportTypeName, ReportTypeValue,
                     (void *) EZ_CorrectCB, this);
    check_disp_num.Init(container, "Check to Display");
    video_target.Init(container, "Video Target", PrinterIDName, PrinterIDValue);
    report_print.Init(container, "Touch Print", ReportPrintName, ReportPrintValue);
    page_list.Init(container, "Modifier Page Script");
    spacing.Init(container, "Line Spacing");
    qualifier.Init(container, "Qualifier Type", QualifierName, QualifierValue);
    amount.Init(container, "Amount");
    switch_type.Init(container, "Switch Type", SwitchName, SwitchValue);
    jump_type.Init(container, "Jump Option", FullJumpTypeName, FullJumpTypeValue,
                   (void *) EZ_JumpCB, this);
    jump_type2.Init(container, "Jump Option", JumpTypeName, JumpTypeValue,
                    (void *) EZ_JumpCB, this);
    jump_id.Init(container, "Jump Page ID");
    customer_type.Init(container, "Customer Type", CustomerTypeName, CustomerTypeValue);

    AddLine(container);
    AddButtons(container, EZ_OkayCB, EZ_DeleteCB, EZ_CancelCB, this);
    XtManageChild(container);
}

// Member Functions
int ZoneDialog::Open()
{
    // Perform translations
    if (new_zone_translations)
    {
        item_family.Clear();
        item_family.Init(container, "Family", FamilyName, FamilyValue);
        new_zone_translations = 0;
    }
    
    // Load Data
    int v1;
    int v2;
    open = 1;
    full_edit = RInt8();
    ztype = RInt8();
    if (full_edit)
        type.Set(ztype);
    else
        type2.Set(ztype);
    name.Set(RStr());
    page.Set(RInt32());
    group.Set(RInt8());
    behave.Set(RInt8());
    confirm.Set(RInt8());
    confirm_msg.Set(RStr());
    font.Set(RInt8());
    states = RInt8();
    v1 = RInt8();
    v2 = RInt8();
    appear1.Set(v1, v2);
    color1.Set(RInt8());
    RInt8(); // image[0]
    v1 = RInt8();
    v2 = RInt8();
    appear2.Set(v1, v2);
    color2.Set(RInt8());
    RInt8(); // image[1]
    v1 = RInt8();
    v2 = RInt8();
    appear3.Set(v1, v2);
    color3.Set(RInt8());
    RInt8(); // image[2]
    shape.Set(RInt8());
    shadow.Set(RInt16());
    key.Set(RInt16());

    expression.Set(RStr());
    message.Set(RStr());
    filename.Set(RStr());
    tender_type.Set(RInt8());
    tender_amount.Set(RStr());
    report_type.Set(RInt8());
    check_disp_num.Set(RInt8());
    video_target.Set(RInt8());
    report_print.Set(RInt8());
    page_list.Set(RStr());
    spacing.Set(RFlt());
    qualifier.Set(RInt32());
    amount.Set(RInt32());
    switch_type.Set(RInt8());
    jtype = RInt8();
    if (full_edit)
        jump_type.Set(jtype);
    else
        jump_type2.Set(jtype);
    jump_id.Set(RInt32());
    customer_type.Set(RInt16());
    drawer_zone_type.Set(RInt8());

    item_name.Set(RStr());
    item_print_name.Set(RStr());
    item_zone_name.Set(RStr());
    itype = RInt8();
    item_type.Set(itype);
    item_price.Set(RStr());
    item_subprice.Set(RStr());
    item_employee_price.Set(RStr());
    item_family.Set(RInt8());
    item_sales.Set(RInt8());
    item_printer.Set(RInt8());
    item_order.Set(RInt8());

    // Show Proper fields
    type.Show(full_edit);
    type2.Show(!full_edit);
    page.Show(full_edit);
    group.Show(full_edit);
    font.Show(1);
    appear1.Show(1);
    color1.Show(1);
    shape.Show(full_edit);
    shadow.Show(1);
    Correct();

    XtManageChild(dialog);
    return 0;
}

int ZoneDialog::Correct()
{
    int t = ztype, tmp;

    name.Show(t != ZONE_COMMAND && t != ZONE_GUEST_COUNT &&
              t != ZONE_USER_EDIT && t != ZONE_INVENTORY && t != ZONE_RECIPE &&
              t != ZONE_VENDOR && t != ZONE_ITEM_LIST && t != ZONE_INVOICE &&
              t != ZONE_QUALIFIER && t != ZONE_LABOR && t != ZONE_LOGIN &&
              t != ZONE_LOGOUT && t != ZONE_ORDER_ENTRY && t != ZONE_ORDER_PAGE &&
              t != ZONE_ORDER_FLOW && t != ZONE_PAYMENT_ENTRY && t != ZONE_SWITCH &&
              t != ZONE_JOB_SECURITY && t != ZONE_TENDER_SET && t != ZONE_HARDWARE &&
              t != ZONE_ITEM && t != ZONE_ORDER_ADD && t != ZONE_ORDER_DELETE);
    behave.Show(full_edit && t != ZONE_COMMENT);
    confirm.Show(t == ZONE_STANDARD);
    confirm_msg.Show(t == ZONE_STANDARD);
    appear2.Show(states >= 2);
    color2.Show(states >= 2);
    appear3.Show(states >= 3);
    color3.Show(states >= 3);
    drawer_zone_type.Show(t == ZONE_DRAWER_MANAGE);
    expression.Show(t == ZONE_CONDITIONAL);
    message.Show(t == ZONE_STANDARD || t == ZONE_CONDITIONAL || t == ZONE_TOGGLE);
    filename.Show(t == ZONE_READ);
    item_name.Show(t == ZONE_ITEM);
    item_zone_name.Show(t == ZONE_ITEM);
    item_print_name.Show(t == ZONE_ITEM);
    item_type.Show(t == ZONE_ITEM);
    item_price.Show(t == ZONE_ITEM);
    item_subprice.Show(t == ZONE_ITEM && itype == ITEM_SUBSTITUTE);
    item_employee_price.Show(t == ZONE_ITEM);
    item_family.Show(t == ZONE_ITEM);
    item_sales.Show(t == ZONE_ITEM);
    item_printer.Show(t == ZONE_ITEM &&
                      (itype == ITEM_NORMAL ||
                       itype == ITEM_SUBSTITUTE ||
                       itype == ITEM_POUND));
    item_order.Show(t == ZONE_ITEM &&
                    (itype != ITEM_NORMAL ||
                     itype != ITEM_POUND));
    tender_type.Show(t == ZONE_TENDER);
    tender_amount.Show(t == ZONE_TENDER);
    report_type.Show(t == ZONE_REPORT);
    //FIX BAK--> check_disp_num and video_target:
    //FIX only display when report type is REPORT_CHECK
    int rt = report_type.Value();
    check_disp_num.Show(t == ZONE_REPORT && rt == REPORT_CHECK);
    video_target.Show(t == ZONE_REPORT && rt == REPORT_CHECK);
    report_print.Show(t == ZONE_REPORT);
    page_list.Show(t == ZONE_ITEM);
    spacing.Show(t == ZONE_CHECK_LIST || t == ZONE_DRAWER_MANAGE ||
                 t == ZONE_USER_EDIT || t == ZONE_INVENTORY || t == ZONE_RECIPE ||
                 t == ZONE_VENDOR || t == ZONE_ITEM_LIST || t == ZONE_INVOICE ||
                 t == ZONE_LABOR || t == ZONE_ORDER_ENTRY || t == ZONE_PAYMENT_ENTRY ||
                 t == ZONE_PAYOUT || t == ZONE_REPORT || t == ZONE_HARDWARE ||
                 t == ZONE_TENDER_SET || t == ZONE_MERCHANT);
    qualifier.Show(t == ZONE_QUALIFIER);
    amount.Show(t == ZONE_ORDER_PAGE);
    switch_type.Show(t == ZONE_SWITCH);
    customer_type.Show(t == ZONE_TABLE);

    tmp = (t == ZONE_ITEM || t == ZONE_SIMPLE ||
           t == ZONE_STANDARD || t == ZONE_CONDITIONAL || t == ZONE_QUALIFIER);
    if (full_edit)
    {
        jump_type.Show(tmp);
        jump_type2.Show(0);
    }
    else
    {
        jump_type.Show(0);
        jump_type2.Show(tmp);
    }
    if (jtype != JUMP_NORMAL && jtype != JUMP_STEALTH && jtype != JUMP_PASSWORD)
        tmp = 0;
    jump_id.Show(tmp);
    key.Show(full_edit &&
             (t == ZONE_SIMPLE || t == ZONE_STANDARD || t == ZONE_TOGGLE ||
              t == ZONE_CONDITIONAL));
    return 0;
}

int ZoneDialog::Close()
{
    open = 0;
    if (XtIsManaged(dialog))
        XtUnmanageChild(dialog);
    return 0;
}

int ZoneDialog::Send()
{
    Flt f;
    int tmp;
    int v1;
    int v2;

    WInt8(SERVER_ZONEDATA);
    WInt8(ztype);
    WStr(name.Value());
    page.Get(tmp); WInt32(tmp);
    group.Get(tmp); WInt8(tmp);
    WInt8(behave.Value());
    WInt8(confirm.Value());
    WStr(confirm_msg.Value());
    WInt8(font.Value());
    appear1.Value(v1, v2);
    WInt8(v1);
    WInt8(v2);
    WInt8(color1.Value());
    WInt8(0); // image[0]
    appear2.Value(v1, v2);
    WInt8(v1);
    WInt8(v2);
    WInt8(color2.Value());
    WInt8(0); // image[1]
    appear3.Value(v1, v2);
    WInt8(v1);
    WInt8(v2);
    WInt8(color3.Value());
    WInt8(0);  // image[2]
    WInt8(shape.Value());
    WInt16(shadow.Value());
    key.Get(tmp); WInt16(tmp);

    WStr(expression.Value());
    WStr(message.Value());
    WStr(filename.Value());
    WInt8(tender_type.Value());
    WStr(tender_amount.Value());
    WInt8(report_type.Value());
    check_disp_num.Get(tmp); WInt8(tmp);
    WInt8(video_target.Value());
    WInt8(report_print.Value());
    WStr(page_list.Value());
    spacing.Get(f); WFlt(f);
    WInt32(qualifier.Value());
    amount.Get(tmp); WInt32(tmp);
    WInt8(switch_type.Value());
    WInt8(jtype);
    jump_id.Get(tmp); WInt32(tmp);
    WInt16(customer_type.Value());
    WInt8(drawer_zone_type.Value());

    WStr(item_name.Value());
    WStr(item_print_name.Value());
    WStr(item_zone_name.Value());
    WInt8(item_type.Value());
    WStr(item_price.Value());
    WStr(item_subprice.Value());
    WStr(item_employee_price.Value());
    WInt8(item_family.Value());
    WInt8(item_sales.Value());
    WInt8(item_printer.Value());
    WInt8(item_order.Value());
    return SendNow();
}


/*********************************************************************
 * MultiZoneDialog Class
 ********************************************************************/

void MZ_OkayCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    MultiZoneDialog *d = (MultiZoneDialog *) client_data;
    d->Close();
    d->Send();
}

void MZ_CancelCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    MultiZoneDialog *d = (MultiZoneDialog *) client_data;
    d->Close();
}

// Constructor
MultiZoneDialog::MultiZoneDialog(Widget parent)
{
    open = 0;
    Arg args[6];
    XtSetArg(args[0], XmNtitle, "Multi-Button Properties");
    XtSetArg(args[1], XmNmwmDecorations, MWM_DECOR_ALL | MWM_DECOR_MENU);
    XtSetArg(args[2], XmNmwmFunctions, MWM_FUNC_ALL | MWM_FUNC_CLOSE);
    dialog = XmCreateFormDialog(parent, (char*)"multizone dialog", args, 3);

    XtSetArg(args[0], XmNorientation,      XmVERTICAL);
    XtSetArg(args[1], XmNtopAttachment,    XmATTACH_FORM);
    XtSetArg(args[2], XmNbottomAttachment, XmATTACH_FORM);
    XtSetArg(args[3], XmNleftAttachment,   XmATTACH_FORM);
    XtSetArg(args[4], XmNrightAttachment,  XmATTACH_FORM);
    XtSetArg(args[5], XmNpacking,          XmPACK_TIGHT);
    Widget w = XmCreateRowColumn(dialog, (char*)"", args, 6);

    behave.no_change_value = -1;
    behave.Init(w, "Behavior", ZoneBehaveName, ZoneBehaveValue);
    font.no_change_value = -1;
    font.Init(w, "Font", FontName, FontValue);
    appear1.no_change_value = -1;
    appear1.Init(w, "Appearance", ZoneFrameName, ZoneFrameValue,
                 TextureName, TextureValue);
    color1.no_change_value = -1;
    color1.Init(w, "Text Color", ColorName, ColorValue);
    appear2.no_change_value = -1;
    appear2.Init(w, "Selected Appearance", ZoneFrameName, ZoneFrameValue,
                 TextureName, TextureValue);
    color2.no_change_value = -1;
    color2.Init(w, "Selected Text Color", ColorName, ColorValue);
    shape.no_change_value = -1;
    shape.Init(w, "Button Shape", ShapeName, ShapeValue);
    shadow.no_change_value = -1;
    shadow.Init(w, "Shadow Thickness", ShadowName, ShadowValue);

    AddLine(w);
    AddButtons(w, MZ_OkayCB, NULL, MZ_CancelCB, this);
    XtManageChild(w);
}

// Member Functions
int MultiZoneDialog::Open()
{
    // Read data for fields
    int v1, v2;
    int full_edit = RInt8();
    behave.Set(RInt16());
    font.Set(RInt16());
    v1 = RInt16();
    v2 = RInt16();
    appear1.Set(v1, v2);
    color1.Set(RInt16());
    v1 = RInt16();
    v2 = RInt16();
    appear2.Set(v1, v2);
    color2.Set(RInt16());
    shape.Set(RInt16());
    shadow.Set(RInt16());

    // Show proper fields
    behave.Show(full_edit);
    font.Show(1);
    appear1.Show(1);
    color1.Show(1);
    appear2.Show(1);
    color2.Show(1);
    shape.Show(1);
    shadow.Show(1);
    XtManageChild(dialog);
    open = 1;
    return 0;
}

int MultiZoneDialog::Close()
{
    open = 0;
    if (XtIsManaged(dialog))
        XtUnmanageChild(dialog);
    return 0;
}

int MultiZoneDialog::Send()
{
    int v1, v2;
    WInt8(SERVER_ZONECHANGES);
    WInt16(behave.Value());
    WInt16(font.Value());
    appear1.Value(v1, v2);
    WInt16(v1); WInt16(v2);
    WInt16(color1.Value());
    appear2.Value(v1, v2);
    WInt16(v1); WInt16(v2);
    WInt16(color2.Value());
    WInt16(shape.Value());
    WInt16(shadow.Value());
    return SendNow();
}

/*********************************************************************
 * TranslateDialog Class
 ********************************************************************/

void TD_OkayCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    TranslateDialog *d = (TranslateDialog *) client_data;
    d->Close();
    d->Send();
}

void TD_CancelCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    TranslateDialog *d = (TranslateDialog *) client_data;
    d->Close();
}

// Constructor
TranslateDialog::TranslateDialog(Widget parent)
{
    open = 0;
    count = 0;

    Arg args[6];
    XtSetArg(args[0], XmNtitle, "Button Translation");
    XtSetArg(args[1], XmNmwmDecorations, MWM_DECOR_ALL | MWM_DECOR_MENU);
    XtSetArg(args[2], XmNmwmFunctions, MWM_FUNC_ALL | MWM_FUNC_CLOSE);
    dialog = XmCreateFormDialog(parent, (char*)"translate dialog", args, 3);

    XtSetArg(args[0], XmNorientation,      XmVERTICAL);
    XtSetArg(args[1], XmNtopAttachment,    XmATTACH_FORM);
    XtSetArg(args[2], XmNbottomAttachment, XmATTACH_FORM);
    XtSetArg(args[3], XmNleftAttachment,   XmATTACH_FORM);
    XtSetArg(args[4], XmNrightAttachment,  XmATTACH_FORM);
    XtSetArg(args[5], XmNpacking,          XmPACK_TIGHT);
    Widget w = XmCreateRowColumn(dialog, (char*)"", args, 6);

    original.Init(w, "Original Text");
    translation.Init(w, "Translation");

    AddLine(w);
    AddButtons(w, TD_OkayCB, NULL, TD_CancelCB, this);
    XtManageChild(w);
}

// Member Functions
int TranslateDialog::Open()
{
    open = 1;
    count = RInt8();
    for (int i = 0; i < count; ++i)
    {
        original.Set(RStr());
        translation.Set(RStr());
    }
    XtManageChild(dialog);
    return 0;
}

int TranslateDialog::Close()
{
    open = 0;
    if (XtIsManaged(dialog))
        XtUnmanageChild(dialog);
    return 0;
}

int TranslateDialog::Send()
{
    WInt8(SERVER_TRANSLATE);
    WInt8(count);
    for (int i = 0; i < count; ++i)
    {
        WStr(original.Value());
        WStr(translation.Value());
    }
    return SendNow();
}

/*********************************************************************
 * ListDialog Class
 ********************************************************************/

void ListSelectCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XmListCallbackStruct *data = (XmListCallbackStruct *) call_data;
    ListDialog *d = (ListDialog *) client_data;

    int new_pos = data->item_position;
    if (new_pos != d->selected)
    {
        d->selected = new_pos;
        d->Send();
    }
}

void ListPrintCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    WInt8(SERVER_BUTTONPRESS);
    WInt16(1);  // Main Window
    WInt16(12); // PrintList Button
    SendNow();
}

void ListCloseCB(Widget widget, XtPointer client_data, XtPointer call_data)
{
    ListDialog *d = (ListDialog *) client_data;
    d->Close();
}

// Constructor
ListDialog::ListDialog(Widget parent)
{
    open = 0;
    selected = 0;
    items = 0;

    Arg args[11];
    XtSetArg(args[0], XmNtitle, "Page List");
    dialog = XmCreateFormDialog(parent, (char*)"pagelist dialog", args, 1);

    Widget w, buttons;
    buttons = XtVaCreateWidget("form", xmFormWidgetClass, dialog,
                               XmNleftAttachment,   XmATTACH_POSITION,
                               XmNleftPosition,     1,
                               XmNrightAttachment,  XmATTACH_POSITION,
                               XmNrightPosition,    99,
                               XmNbottomAttachment, XmATTACH_POSITION,
                               XmNbottomPosition,   99, NULL);

    w = XtVaCreateManagedWidget("Print", xmPushButtonWidgetClass, buttons,
                                XmNleftAttachment,   XmATTACH_FORM,
                                XmNrightAttachment,  XmATTACH_POSITION,
                                XmNrightPosition,    32, NULL);
    XtAddCallback(w, XmNactivateCallback, (XtCallbackProc) ListPrintCB,
                  (XtPointer) this);

    w = XtVaCreateManagedWidget("Close", xmPushButtonWidgetClass, buttons,
                                XmNleftAttachment,   XmATTACH_POSITION,
                                XmNleftPosition,     68,
                                XmNrightAttachment,  XmATTACH_FORM, NULL);
    XtAddCallback(w, XmNactivateCallback, (XtCallbackProc) ListCloseCB,
                  (XtPointer) this);
    XtManageChild(buttons);

    XtSetArg(args[0], XmNselectionPolicy,        XmSINGLE_SELECT);
    XtSetArg(args[1], XmNscrollBarDisplayPolicy, XmSTATIC);
    XtSetArg(args[2], XmNtopAttachment,          XmATTACH_POSITION);
    XtSetArg(args[3], XmNtopPosition,            1);
    XtSetArg(args[4], XmNleftAttachment,         XmATTACH_POSITION);
    XtSetArg(args[5], XmNleftPosition,           1);
    XtSetArg(args[6], XmNrightAttachment,        XmATTACH_POSITION);
    XtSetArg(args[7], XmNrightPosition,          99);
    XtSetArg(args[8], XmNbottomAttachment,       XmATTACH_WIDGET);
    XtSetArg(args[9], XmNbottomWidget,           buttons);
    XtSetArg(args[10], XmNvisibleItemCount,      32);

    list = XmCreateScrolledList(dialog, (char*)"list", args, 11);
    XtAddCallback(list, XmNsingleSelectionCallback,
                  (XtCallbackProc) ListSelectCB, (XtPointer) this);
    XtManageChild(list);
}

// Member Functions
int ListDialog::Start()
{
    if (open)
        Close();

    selected = 0;
    XmListDeselectAllItems(list);
    XmListDeleteAllItems(list);
    return 0;
}

int ListDialog::ReadItem()
{
    XmString xs = XmStringCreateSimple(RStr());
    XmListAddItemUnselected(list, xs, 0);
    XmStringFree(xs);
    return 0;
}

int ListDialog::End()
{
    open = 1;
    XtManageChild(dialog);
    return 0;
}

int ListDialog::Close()
{
    open = 0;
    if (XtIsManaged(dialog))
        XtUnmanageChild(dialog);
    return 0;
}

int ListDialog::Send()
{
    WInt8(SERVER_LISTSELECT);
    WInt32(selected);
    return SendNow();
}
