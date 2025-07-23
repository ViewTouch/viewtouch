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
 * pos_zone.cc - revision 105 (10/20/98)
 * Implementation of pos_zone module
 */

#include "pos_zone.hh"
#include "button_zone.hh"
#include "login_zone.hh"
#include "order_zone.hh"
#include "check_list_zone.hh"
#include "creditcard_list_zone.hh"
#include "payment_zone.hh"
#include "user_edit_zone.hh"
#include "settings_zone.hh"
#include "report_zone.hh"
#include "table_zone.hh"
#include "split_check_zone.hh"
#include "drawer_zone.hh"
#include "printer_zone.hh"
#include "payout_zone.hh"
#include "inventory_zone.hh"
#include "labor_zone.hh"
#include "phrase_zone.hh"
#include "merchant_zone.hh"
#include "account_zone.hh"
#include "hardware_zone.hh"
#include "search_zone.hh"
#include "data_file.hh"
#include "image_data.hh"
#include "employee.hh"
#include "chart_zone.hh"
#include "video_zone.hh"

#include "expense_zone.hh"
#include "cdu_zone.hh"

#ifdef DEBUG
#include "labels.hh"
#include "debug.hh"
#include <errno.h>
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif

// ZONE_VERSION NOTES for Read()/Write()
// 17 (1/1/97)    earliest supported version
// 18 (10/3/97)    key shortcut field added
// 19 (11/10/97)   default_appear & default_color fields expanded in page
// 20 (1/7/98)     appear changed to frame & texture (page & zone)
// 21 (8/25/98)    added customer_type
// 22 (4/9/2002)   added display_check_num for ReportZone
// 26 (11/21/2003) added CCMessageSettings zone
// 27 ???
// 28 (1/20/2015)  added global page defaults for zonedb

/**** Functions ****/
// Zone Graphic Appearence (obsolete)
#define APPEAR_DEFAULT      50 // Default appearance
#define APPEAR_HIDDEN       0  // Isn't drawn (text included)
#define APPEAR_NF_SAND      1  // No Frame Sand
#define APPEAR_SAND         2  // Sand (w/ single frame)
#define APPEAR_PARCHMENT    28 // Parchment (w/ single frame)
#define APPEAR_WOOD         10 // Wood (w/ single frame)
#define APPEAR_LT_WOOD      22 // Lite wood (w/ single frame)
#define APPEAR_DK_WOOD      23 // Dark wood (w/ single frame)
#define APPEAR_ISF          3  // Inset single frame
#define APPEAR_IFW          5  // Inset double frame
#define APPEAR_CLEAR        6  // See through
#define APPEAR_FCLEAR       9  // Framed see through
#define APPEAR_DF           8  // Double frame
#define APPEAR_NF_LITSAND   11 // No Frame Lit Sand
#define APPEAR_LITSAND      12 // Lit Raised single frame
#define APPEAR_LRFW         14 // Lit Raised frame window
#define APPEAR_LDF1         17 // Lit Double frame 1
#define APPEAR_LDF2         18 // Lit Double frame 2
#define APPEAR_DRFW         19 // Double raised frame window
#define APPEAR_LDRFW        24 // Lit Double raised frame window
#define APPEAR_RF_WOOD      4  // Wood inlay w/ raised sand frame
#define APPEAR_RF_SAND      7  // Sand inlay w/ raised sand frame
#define APPEAR_RF_MARBLE    25 // Green marble inlay w/ raised sand frame
#define APPEAR_RF_GRAVEL    26 // Gravel inlay w/ raised sand frame
#define APPEAR_RF_PARCHMENT 27 // parchment inlay

int ConvertAppear(int appear, Uchar &f, Uchar &t)
{
    // Convert old (version <= 19) appearences to new frame/texture appearence
    switch (appear)
    {
    default:
    case APPEAR_SAND:         f = ZF_RAISED;        t = IMAGE_SAND; break;
    case APPEAR_DEFAULT:      f = ZF_DEFAULT;       t = IMAGE_DEFAULT; break;
    case APPEAR_HIDDEN:       f = ZF_HIDDEN;        t = IMAGE_DEFAULT; break;
    case APPEAR_NF_SAND:      f = ZF_NONE;          t = IMAGE_SAND; break;
    case APPEAR_PARCHMENT:    f = ZF_RAISED;        t = IMAGE_PARCHMENT; break;
    case APPEAR_WOOD:         f = ZF_RAISED;        t = IMAGE_WOOD; break;
    case APPEAR_LT_WOOD:      f = ZF_RAISED;        t = IMAGE_LITE_WOOD; break;
    case APPEAR_DK_WOOD:      f = ZF_RAISED;        t = IMAGE_DARK_WOOD; break;
    case APPEAR_ISF:          f = ZF_INSET;         t = IMAGE_DARK_SAND; break;
    case APPEAR_IFW:          f = ZF_INSET_BORDER;  t = IMAGE_WOOD; break;
    case APPEAR_CLEAR:        f = ZF_NONE;          t = IMAGE_CLEAR; break;
    case APPEAR_FCLEAR:       f = ZF_RAISED;        t = IMAGE_CLEAR; break;
    case APPEAR_DF:           f = ZF_DOUBLE;        t = IMAGE_SAND; break;
    case APPEAR_NF_LITSAND:   f = ZF_NONE;          t = IMAGE_LIT_SAND; break;
    case APPEAR_LITSAND:      f = ZF_RAISED;        t = IMAGE_LIT_SAND; break;
    case APPEAR_LRFW:         f = ZF_LIT_SAND_BORDER; t = IMAGE_WOOD; break;
    case APPEAR_LDF1:         f = ZF_SAND_BORDER;   t = IMAGE_SAND; break;
    case APPEAR_LDF2:         f = ZF_DOUBLE;        t = IMAGE_SAND; break;
    case APPEAR_DRFW:         f = ZF_DOUBLE_BORDER; t = IMAGE_LITE_WOOD; break;
    case APPEAR_LDRFW:        f = ZF_LIT_DOUBLE_BORDER; t = IMAGE_LITE_WOOD; break;
    case APPEAR_RF_WOOD:      f = ZF_SAND_BORDER;   t = IMAGE_WOOD; break;
    case APPEAR_RF_SAND:      f = ZF_BORDER;        t = IMAGE_SAND; break;
    case APPEAR_RF_MARBLE:    f = ZF_SAND_BORDER;   t = IMAGE_GREEN_MARBLE; break;
    case APPEAR_RF_PARCHMENT: f = ZF_SAND_BORDER;   t = IMAGE_PARCHMENT; break;
    }
    return 0;
}

Zone *NewPosZone(int type)
{
	Zone *pNewZone = NULL;
	switch (type)
	{
		// General Zone Types
    case ZONE_ITEM:
        pNewZone = new ItemZone;
        break;
    case ZONE_QUALIFIER:
        pNewZone = new QualifierZone;
        break;
    case ZONE_SIMPLE:
        pNewZone = new ButtonZone;
        break;
    case ZONE_TABLE:
        pNewZone = new TableZone;
        break;

        // Restricted Zone Types
    case ZONE_ACCOUNT:
        pNewZone = new AccountZone;
        break;
    case ZONE_CC_SETTINGS:
        pNewZone = new CCSettingsZone;
        break;
    case ZONE_CC_MSG_SETTINGS:
        pNewZone = new CCMessageSettingsZone;
        break;
    case ZONE_CDU:
        pNewZone = new CDUZone;
        break;
    case ZONE_CHART:
        pNewZone = new ChartZone;
        break;
    case ZONE_CHECK_EDIT:
        pNewZone = new CheckEditZone();
        break;
    case ZONE_CHECK_LIST:
        pNewZone = new CheckListZone;
        break;
    case ZONE_COMMAND:
        pNewZone = new CommandZone;
        break;
    case ZONE_COMMENT:
        pNewZone = new CommentZone;
        break;
    case ZONE_CONDITIONAL:
        pNewZone = new ConditionalZone;
        break;
    case ZONE_CREDITCARD_LIST:
        pNewZone = new CreditCardListZone;
        break;
    case ZONE_CUSTOMER_INFO:
        pNewZone = new CustomerInfoZone;
        break;
    case ZONE_DEVELOPER:
        pNewZone = new DeveloperZone;
        break;
    case ZONE_DRAWER_ASSIGN:
        pNewZone = new DrawerAssignZone;
        break;
    case ZONE_DRAWER_MANAGE:
        pNewZone = new DrawerManageZone;
        break;
    case ZONE_END_DAY:
        pNewZone = new EndDayZone;
        break;
    case ZONE_EXPENSE:
        pNewZone = new ExpenseZone;
        break;
    case ZONE_EXPIRE_MSG:
        pNewZone = new ExpireSettingsZone;
        break;
    case ZONE_GUEST_COUNT:
        pNewZone = new GuestCountZone;
        break;
    case ZONE_HARDWARE:
        pNewZone = new HardwareZone;
        break;
    case ZONE_INVENTORY:
        pNewZone = new ProductZone;
        break;
    case ZONE_INVOICE:
        pNewZone = new InvoiceZone;
        break;
    case ZONE_ITEM_LIST:
        pNewZone = new ItemListZone;
        break;
    case ZONE_ITEM_TARGET:
        pNewZone = new ItemPrintTargetZone;
        break;
    case ZONE_JOB_SECURITY:
        pNewZone = new JobSecurityZone;
        break;
    case ZONE_KILL_SYSTEM:
        pNewZone = new KillSystemZone;
        break;
    case ZONE_LABOR:
        pNewZone = new LaborZone;
        break;
    case ZONE_LOGIN:
        pNewZone = new LoginZone;
        break;
    case ZONE_LOGOUT:
        pNewZone = new LogoutZone;
        break;
    case ZONE_MERCHANT:
        pNewZone = new MerchantZone;
        break;
    case ZONE_MONEY_SET:
        pNewZone = new MoneySetZone;
        break;
    case ZONE_ORDER_ADD:
        pNewZone = new OrderAddZone;
        break;
    case ZONE_ORDER_DELETE:
        pNewZone = new OrderDeleteZone;
        break;
    case ZONE_ORDER_ENTRY:
        pNewZone = new OrderEntryZone;
        break;
    case ZONE_ORDER_FLOW:
        pNewZone = new OrderFlowZone;
        break;
    case ZONE_ORDER_PAGE:
        pNewZone = new OrderPageZone;
        break;
    case ZONE_PAYMENT_ENTRY:
        pNewZone = new PaymentZone;
        break;
    case ZONE_PAYOUT:
        pNewZone = new PayoutZone;
        break;
    case ZONE_PHRASE:
        pNewZone = new PhraseZone;
        break;
    case ZONE_PRINT_TARGET:
        pNewZone = new PrintTargetZone;
        break;
    case ZONE_READ:
        pNewZone = new ReadZone;
        break;
    case ZONE_RECEIPTS:
        pNewZone = new ReceiptSettingsZone;
        break;
    case ZONE_RECIPE:
        pNewZone = new RecipeZone;
        break;
    case ZONE_REPORT:
        pNewZone = new ReportZone;
        break;
    case ZONE_SCHEDULE:
        pNewZone = new ScheduleZone;
        break;
    case ZONE_SEARCH:
        pNewZone = new SearchZone;
        break;
    case ZONE_SETTINGS:
        pNewZone = new SettingsZone;
        break;
    case ZONE_SPLIT_CHECK:
        pNewZone = new SplitCheckZone;
        break;
    case ZONE_SPLIT_KITCHEN:
        pNewZone = new SplitKitchenZone;
        break;
    case ZONE_STANDARD:
        pNewZone = new MessageButtonZone;
        break;
    case ZONE_STATUS_BUTTON:
        pNewZone = new StatusZone;
        break;
    case ZONE_SWITCH:
        pNewZone = new SwitchZone;
        break;
    case ZONE_TABLE_ASSIGN:
        pNewZone = new TableAssignZone;
        break;
    case ZONE_TAX_SET:
        pNewZone = new TaxSetZone;
        break;
    case ZONE_TAX_SETTINGS:
        pNewZone = new TaxSettingsZone;
        break;
    case ZONE_TENDER:
        pNewZone = new TenderZone;
        break;
    case ZONE_TENDER_SET:
        pNewZone = new TenderSetZone;
        break;
    case ZONE_TIME_SETTINGS:
        pNewZone = new TimeSettingsZone;
        break;
    case ZONE_TOGGLE:
        pNewZone = new ToggleZone;
        break;
    case ZONE_USER_EDIT:
        pNewZone = new UserEditZone;
        break;
    case ZONE_VENDOR:
        pNewZone = new VendorZone;
        break;
    case ZONE_VIDEO_TARGET:
        pNewZone = new VideoTargetZone;
        break;
    default:
        if (debug_mode)
            printf("Unknown zone type:  %d\n", type);
        break;
	}

	if (pNewZone == NULL)
	{
		char str[64];
		sprintf(str, "Creation of PosZone object type %d failed", type);
		ReportError(str);
	}

	return pNewZone;
}

Page *NewPosPage()
{
    PosPage *p = new PosPage;
    if (p == NULL)
        ReportError("Creation of PosPage object failed");

    return p;
}


/**** PosZone Class ****/
// Member Functions
Zone *PosZone::Copy()
{
    Zone *z = NewPosZone(Type());
    CopyZone(z);
    return z;
}

int PosZone::CanSelect(Terminal *t)
{
    if (page == NULL)
        return 1;

    Employee *e = t->user;
    if (e == NULL)
        return 0;

    if (page->id < 0 && !e->CanEditSystem() && Type() != ZONE_ORDER_ENTRY)
        return 0;
    return e->CanEdit();
}

int PosZone::CanEdit(Terminal *t)
{
    if (page == NULL)
        return 1;

    Employee *e = t->user;
    if (e == NULL)
        return 0;

    if (page->id < 0 && !e->CanEditSystem())
        return 0;
    return e->CanEdit();
}

int PosZone::CanCopy(Terminal *t)
{
    Employee *e = t->user;
    if (e == NULL)
        return 0;

    if (page->id < 0 && !e->CanEditSystem())
        return 0;
    return e->CanEdit();
}

int PosZone::SetSize(Terminal *t, int width, int height)
{
    // FINISH! - Add user & page check
    if (width < 16)
        width = 16;
    if (height < 16)
        height = 16;
    w = width;
    h = height;
    return 0;
}

int PosZone::SetPosition(Terminal *t, int pos_x, int pos_y)
{
    // FINISH! - Add user & page check
    x = pos_x;
    y = pos_y;
    return 0;
}

int PosZone::Read(InputDataFile &df, int version)
{
	int tmp;
	df.Read(name);
	df.Read(group_id);
	df.Read(x);
	df.Read(y);
	df.Read(w);
	df.Read(h);
	df.Read(behave);
	df.Read(font);
	if (version <= 19)
	{
		switch (font)
		{
        case FONT_FIXED_14: font = FONT_TIMES_14; break;
        case FONT_FIXED_20: font = FONT_FIXED_20; break;
        case FONT_FIXED_24: font = FONT_FIXED_24; break;
		} //end switch
	}

	for (int i = 0; i < 3; ++i)
	{
		if (version <= 19)
		{
			df.Read(tmp);
			ConvertAppear(tmp, frame[i], texture[i]);
		}
		else
		{
			df.Read(frame[i]);
			df.Read(texture[i]);
		}

		df.Read(color[i]);
		df.Read(image[i]);

		if (version <= 19)
		{
            // There were some comparisons here which are just impossible.  color[i] is an
            // unsigned char, which cannot hold 999, 998, or 1000.  I'm not going to increase
            // the size of the variable.  I'm just going to assume that a) there aren't many
            // (if any) version <= 19 files out there, and b) COLOR_DEFAULT will work fine.
            color[i] = COLOR_DEFAULT;
		}
	}

	df.Read(shadow);
	df.Read(shape);

	df.Read(Amount());
	df.Read(Expression());
	df.Read(FileName());
	df.Read(JumpType());
	df.Read(JumpID());
	df.Read(Message());
	df.Read(ItemName());
	df.Read(Script());
	df.Read(QualifierType());
	df.Read(ReportType());
	df.Read(Spacing());
	df.Read(TenderType());
	df.Read(TenderAmount());
	df.Read(ReportPrint());
	df.Read(Columns());
	df.Read(SwitchType());

	if (version >= 21)
		df.Read(CustomerType());
    if (version >= 22)
        df.Read(CheckDisplayNum());
    if (version >= 23)
        df.Read(VideoTarget());
    if (version >= 24)
        df.Read(DrawerZoneType());
    if (version >= 25)
    {
        df.Read(Confirm());
        df.Read(ConfirmMsg());
    }
	if (version >= 18)
		df.Read(key);

	return 0;
}

int PosZone::Write(OutputDataFile &df, int version)
{
    if (version < 20)
        return 1;

    int error = 0;
    error += df.Write(name);
    error += df.Write(group_id);
    error += df.Write(x);
    error += df.Write(y);
    error += df.Write(w);
    error += df.Write(h);
    error += df.Write(behave);
    error += df.Write(font);
    for (int i = 0; i < 3; ++i)
    {
        error += df.Write(frame[i]);
        error += df.Write(texture[i]);
        error += df.Write(color[i]);
        error += df.Write(image[i]);
    }
    error += df.Write(shadow);
    error += df.Write(shape);

    error += df.Write(Amount());
    error += df.Write(Expression());
    error += df.Write(FileName());
    error += df.Write(JumpType());
    error += df.Write(JumpID());
    error += df.Write(Message());
    error += df.Write(ItemName());
    error += df.Write(Script());
    error += df.Write(QualifierType());
    error += df.Write(ReportType());
    error += df.Write(Spacing());
    error += df.Write(TenderType());
    error += df.Write(TenderAmount());
    error += df.Write(ReportPrint());
    error += df.Write(Columns());
    error += df.Write(SwitchType());
    error += df.Write(CustomerType());
    error += df.Write(CheckDisplayNum());
    error += df.Write(VideoTarget());
    error += df.Write(DrawerZoneType());
    error += df.Write(Confirm());
    error += df.Write(ConfirmMsg());
    error += df.Write(key,1);
    return error;
}

/**** PosPage Class ****/
// Member Functions
Page *PosPage::Copy()
{
    PosPage *p = new PosPage;
    if (p == NULL)
    {
        ReportError("Can't create copy of page");
        return NULL;
    }

    p->name            = name;
    p->id              = id;
    p->parent_id       = parent_id;
    p->image           = image;
    p->title_color     = title_color;
    p->type            = type;
    p->index           = index;
    p->size            = size;
    p->default_font    = default_font;
    p->default_spacing = default_spacing;
    p->default_shadow  = default_shadow;
    for (int i = 0; i < 3; ++i)
    {
        p->default_frame[i]   = default_frame[i];
        p->default_texture[i] = default_texture[i];
        p->default_color[i]   = default_color[i];
    }

    for (Zone *z = ZoneList(); z != NULL; z = z->next)
        p->Add(z->Copy());

    return p;
}

int PosPage::Read(InputDataFile &infile, int version)
{
    infile.Read(name);
    infile.Read(id);
    infile.Read(image);
    infile.Read(title_color);
    infile.Read(parent_id);
    infile.Read(type);
    if (type == PAGE_ITEM2)
        type = PAGE_ITEM;
    infile.Read(index);
    infile.Read(size);

    /*
     * New screen sizes were added and it's necessary to remap the previous page sizes to preserve compatiblity
     */
    if (version <= 26) {
      switch (size) {
      case 2:
	size = 4; // SIZE_800x600
	break;
      case 3:
	size = 6; // SIZE_1024x768
	break;
      case 4:
	size = 8; // SIZE_1280x1024
	break;
      case 5:
	size = 12; // SIZE_1600x1200
	break;
      case 6:
	size = 2; // SIZE_768x1024
	break;
      case 7:
	size = 3; // SIZE_800x480
	break;
      case 8:
	size = 14; // SIZE_1920x1080
	break;
      case 9:
	size = 15; // SIZE_1920x1200
	break;
      case 10: 
	size = 13; // SIZE_1680x1050
	break;
      case 14:
	size = 15;
	break;
      }
    }

    infile.Read(default_font);
    if (version <= 19)
    {
        switch (default_font)
        {
        case FONT_FIXED_14: default_font = FONT_TIMES_14; break;
        case FONT_FIXED_20: default_font = FONT_FIXED_20; break;
        case FONT_FIXED_24: default_font = FONT_FIXED_24; break;
        }

        int appear;
        infile.Read(appear);
        ConvertAppear(appear, default_frame[0], default_texture[0]);
        if (version == 19)
        {
            infile.Read(appear);
            ConvertAppear(appear, default_frame[1], default_texture[1]);
            infile.Read(appear);
            ConvertAppear(appear, default_frame[2], default_texture[2]);
        }
        infile.Read(default_color[0]);
        if (version == 19)
        {
            infile.Read(default_color[1]);
            infile.Read(default_color[2]);
        }

        if (title_color == 999)
            title_color = COLOR_DEFAULT;
        else if (title_color == 998)
            title_color = COLOR_PAGE_DEFAULT;
        else if (title_color == 1000)
            title_color = COLOR_CLEAR;

        // default_color[0] is an unsigned char and thus cannot have values of
        // 999, 998, or 1000.  The comparisons are bogus, so I'm going to hope
        // COLOR_DEFAULT will suffice.
        default_color[0] = COLOR_DEFAULT;
    }
    else
    {
        for (int i = 0; i < 3; ++i)
        {
            infile.Read(default_frame[i]);
            infile.Read(default_texture[i]);
            infile.Read(default_color[i]);
        }
    }
    infile.Read(default_spacing);
    infile.Read(default_shadow);

    int z_count = -1;
    infile.Read(z_count);

    if (z_count < 0)
    {
        ReportError("Couldn't read zone count");
        return 1;
    }

    for (int i = 0; i < z_count; ++i)
    {
        if (infile.end_of_file)
        {
            std::string msg = "Unexpected end of file: '" + infile.FileName() + "'";
            ReportError(msg);
            return 1;
        }

        genericChar str[256];
        int z_type = 0;
        infile.Read(z_type);

        Zone *z = NewPosZone(z_type);
        if (z == NULL)
        {
            sprintf(str, "Error in creating touch zone type %d", type);
            ReportError(str);
            return 1;
        }
        if (z->Read(infile, version))
        {
            sprintf(str, "Error in reading touch zone type %d", type);
            ReportError(str);
            delete z;
            return 1;
        }
        Add(z);
    }
    return 0;
}

int PosPage::Write(OutputDataFile &df, int version)
{
    if (version < 20)
        return 1;

    // Save version 20
    // see Zone::Read() for version notes

    int error = 0;
    error += df.Write(name);
    error += df.Write(id);
    error += df.Write(image);
    error += df.Write(title_color);
    error += df.Write(parent_id);
    error += df.Write(type);
    error += df.Write(index);
    error += df.Write(size);
    error += df.Write(default_font);
    for (int i = 0; i < 3; ++i)
    {
        error += df.Write(default_frame[i]);
        error += df.Write(default_texture[i]);
        error += df.Write(default_color[i]);
    }

    error += df.Write(default_spacing);
    error += df.Write(default_shadow, 1);

    // Write all touch zones
    error += df.Write(ZoneCount(), 1);

    for (Zone *z = ZoneList(); z != NULL; z = z->next)
    {
        error += df.Write(z->Type());
        error += z->Write(df, version);
    }
    return error;
}
