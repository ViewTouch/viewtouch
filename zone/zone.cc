/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998  *  All Rights Reserved
 * Confidential and Proprietary Information
 *
 * zone.cc - revision 244 (9/8/98)
 * Implementation of touch zone module
 */

#include "zone.hh"
#include "terminal.hh"
#include "data_file.hh"
#include "report.hh"
#include "image_data.hh" // IMAGE definitions
#include "manager.hh"
#include "settings.hh"   // INDEX definitions
#include "system.hh"
#include "pos_zone.hh"   // NewPosPage()
#include "logger.hh"     // logmsg()

#include <cstring>
#include <errno.h>
#include <dirent.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/***********************************************************************
 * Zone Class
 ***********************************************************************/
Zone::Zone()
{
    next       = nullptr;
    fore       = nullptr;
    page       = nullptr;
    group_id   = 0;
    w          = 140;
    h          = 100;
    behave     = BEHAVE_BLINK;
    font       = FONT_DEFAULT;
    shape      = SHAPE_RECTANGLE;
    frame[0]   = ZF_DEFAULT;
    texture[0] = IMAGE_DEFAULT;
    color[0]   = COLOR_DEFAULT;
    image[0]   = 0;
    frame[1]   = ZF_DEFAULT;
    texture[1] = IMAGE_DEFAULT;
    color[1]   = COLOR_DEFAULT;
    image[1]   = 0;
    frame[2]   = ZF_HIDDEN;
    texture[2] = IMAGE_SAND;
    color[2]   = COLOR_DEFAULT;
    image[2]   = 0;
    edit       = 0;
    active     = 1;
    update     = 0;
    border     = 0;
    header     = 0;
    footer     = 0;
    stay_lit   = 0;
    shadow     = SHADOW_DEFAULT;
    key        = 0;
    iscopy     = 0;
}

int Zone::CopyZone(Zone *target)
{
    if (target == nullptr)
        return 1;

    target->SetRegion(this);
    target->name     = name;
    target->group_id = group_id;
    target->behave   = behave;
    target->font     = font;
    target->shadow   = shadow;
    target->shape    = shape;
    target->key      = key;
    for (int i = 0; i < 3; ++i)
    {
        target->frame[i]   = frame[i];
        target->texture[i] = texture[i];
        target->color[i]   = color[i];
        target->image[i]   = image[i];
    }

    if (Amount() && target->Amount())
        *target->Amount() = *Amount();
    if (Expression() && target->Expression())
        *target->Expression() = *Expression();
    if (FileName() && target->FileName())
        *target->FileName() = *FileName();
    if (ItemName() && target->ItemName())
        *target->ItemName() = *ItemName();
    if (JumpType() && target->JumpType())
        *target->JumpType() = *JumpType();
    if (JumpID() && target->JumpID())
        *target->JumpID() = *JumpID();
    if (Message() && target->Message())
        *target->Message() = *Message();
    if (QualifierType() && target->QualifierType())
        *target->QualifierType() = *QualifierType();
    if (ReportType() && target->ReportType())
        *target->ReportType() = *ReportType();
    if (CheckDisplayNum() && target->CheckDisplayNum())
        *target->CheckDisplayNum() = *CheckDisplayNum();
    if (ReportPrint() && target->ReportPrint())
        *target->ReportPrint() = *ReportPrint();
    if (Script() && target->Script())
        *target->Script() = *Script();
    if (Spacing() && target->Spacing())
        *target->Spacing() = *Spacing();
    if (SwitchType() && target->SwitchType())
        *target->SwitchType() = *SwitchType();
    if (TenderType() && target->TenderType())
        *target->TenderType() = *TenderType();
    if (TenderAmount() && target->TenderAmount())
        *target->TenderAmount() = *TenderAmount();
    if (Columns() && target->Columns())
        *target->Columns() = *Columns();
    if (VideoTarget() && target->VideoTarget())
        *target->VideoTarget() = *VideoTarget();
    if (DrawerZoneType() && target->DrawerZoneType())
        *target->DrawerZoneType() = *DrawerZoneType();
    if (Confirm() && target->Confirm())
        *target->Confirm() = *Confirm();
    if (ConfirmMsg() && target->ConfirmMsg())
        *target->ConfirmMsg() = *ConfirmMsg();

    target->iscopy = 1;

    return 0;
}

int Zone::State(Terminal *t)
{
    FnTrace("Zone::State()");
    if (active == 0)
        return 2;
    else if (t->selected_zone == this)
        return 1;
    else
        return stay_lit;
}

int Zone::Draw(Terminal *term, int update_flag)
{
    FnTrace("Zone::Draw()");
    if (update_flag)
    {
        RenderInit(term, update_flag);  // Method NOT implemented as of 9.14.01
    }

    update = update_flag;
    int currShadow = ShadowVal(term);
    int state = State(term);
    int zoneFrame = frame[state];
    int zoneTexture = texture[state];

    if (shape != SHAPE_RECTANGLE || zoneTexture == IMAGE_CLEAR || zoneFrame == ZF_CLEAR_BORDER)
    {
        // Draw zone with background
        return term->Draw(0, x, y, (w + currShadow), (h + currShadow));
    }
    else if (this == term->dialog)
    {
        // Render Dialog (dialog always on top)
        term->SetClip(x, y, w, h);
        Render(term, update_flag);
    }
    else
    {
        // Draw zone without background
        term->SetClip(x, y, (w + currShadow), (h + currShadow));
        term->page->Render(term, 0, x, y, (w + currShadow), (h + currShadow));
    }

    term->UpdateAll();
    return 0;
}

SignalResult Zone::Touch(Terminal *t, int touch_x, int touch_y)
{
    FnTrace("Zone::Touch()");
    // Don't respond to touches
    return SIGNAL_IGNORED;
}

SignalResult Zone::Signal(Terminal *t, const genericChar* message)
{
    FnTrace("Zone::Signal()");
    // Don't respond to messages
    return SIGNAL_IGNORED;
}

SignalResult Zone::Keyboard(Terminal *t, int k, int state)
{
    FnTrace("Zone::Keyboard()");
    if (key > 0 && k == key)
        return Touch(t, 0, 0);

    return SIGNAL_IGNORED;
}

SignalResult Zone::Mouse(Terminal *t, int action, int mx, int my)
{
    FnTrace("Zone::Mouse()");
    if ((action & MOUSE_PRESS))
    {
        return Touch(t, mx, my);
    }
    else
    {
        return SIGNAL_IGNORED;
    }
}

int Zone::RenderZone(Terminal *term, const genericChar* text, int update_flag)
{
    FnTrace("Zone::RenderZone()");
    if (update_flag)
    {
        border = term->FrameBorder(frame[0], shape);
        if (ZoneStates() > 1)
        {
            int b = term->FrameBorder(frame[1], shape);
            if (b > border)
                border = b;
            if (ZoneStates() > 2)
            {
                b = term->FrameBorder(frame[2], shape);
                if (b > border)
                    border = b;
            }
        }
    }
    update = 0;
    term->RenderZone(this);

    genericChar str[256];
    int state = State(term);
    if (frame[state] != ZF_HIDDEN)
    {
        int bx = Max(border - 2, 0);
        int by = Max(border - 4, 0);
        const genericChar* b = term->ReplaceSymbols(text);
        if (b)
        {
            if (behave == BEHAVE_DOUBLE)
            {
                sprintf(str, "%s\\( 2X )", b);
                b = str;
            }
            int c = color[state];
            if (c == COLOR_PAGE_DEFAULT || c == COLOR_DEFAULT)
                c = term->page->default_color[state];
            if (c != COLOR_CLEAR)
                term->RenderZoneText(b, x + bx, y + by + header, w - (bx*2),
                                     h - (by*2) - header - footer, c, font);
        }
    }
    if (term->show_info)
    {
        RenderInfo(term);
    }

    return 0;
}

int Zone::RenderInfo(Terminal *term)
{
    FnTrace("Zone::RenderInfo()");
    genericChar str[32];
    if (group_id != 0)
    {
        sprintf(str, "ID %d", group_id);
        term->RenderText(str, x + border, y + 16, COLOR_BLACK, FONT_TIMES_14);
    }

    if (JumpType() && JumpID())
    {
        switch (*JumpType())
		{
        case JUMP_NORMAL:
            sprintf(str, "J %d", *JumpID());
            break;
        case JUMP_STEALTH:
            sprintf(str, "J %d*", *JumpID());
            break;
        case JUMP_RETURN:
            strcpy(str, GlobalTranslate("J back"));
            break;
        case JUMP_HOME:
            strcpy(str, GlobalTranslate("J home"));
            break;
        case JUMP_SCRIPT:
            strcpy(str, GlobalTranslate("J continue"));
            break;
        case JUMP_INDEX:
            strcpy(str, GlobalTranslate("J index"));
            break;
        default:
            return 0;
		}

        term->RenderText(str, x + border, y + h - border - 12,
                         COLOR_BLACK, FONT_TIMES_14);
    }
    return 0;
}

int Zone::RenderInit(Terminal *t, int update_flag)
{
    FnTrace("Zone::RenderInit()");
    return 0;
}

RenderResult Zone::Render(Terminal *t, int update_flag)
{
    FnTrace("Zone::Render()");
    RenderZone(t, name.Value(), update_flag);
    return RENDER_OKAY;
}

int Zone::Update(Terminal *t, int update_message, const genericChar* value)
{
    FnTrace("Zone::Update()");
    // No update by default
    return 0;
}

int Zone::ShadowVal(Terminal *t)
{
    FnTrace("Zone::ShadowVal()");
    if (shadow != SHADOW_DEFAULT)
    	return shadow;
    int s = t->page->default_shadow;
    if (s != SHADOW_DEFAULT)
    	return s;
    return t->zone_db->default_shadow;
}

const char* Zone::TranslateString(Terminal *t)
{
    FnTrace("Zone::TranslateString()");
    return name.Value();
}

int Zone::SetSize(Terminal *t, int width, int height)
{
    FnTrace("Zone::SetSize()");
    w = width;
    h = height;
    return 0;
}

int Zone::SetPosition(Terminal *t, int pos_x, int pos_y)
{
    FnTrace("Zone::SetPosition()");
    x = pos_x;
    y = pos_y;

    return 0;
}

int Zone::AlterSize(Terminal *t, int wchange, int hchange,
                    int move_x, int move_y)
{
    FnTrace("Zone::AlterSize()");
    int old_w = w;
    int old_h = h;
    SetSize(t, w + wchange, h + hchange);

    wchange = w - old_w;
    if (move_x == 0)
        wchange = 0;

    hchange = h - old_h;
    if (move_y == 0)
        hchange = 0;

    if (wchange == 0 && hchange == 0)
        return 0;

    AlterPosition(t, -wchange, -hchange);
    return 0;
}

int Zone::AlterPosition(Terminal *t, int xchange, int ychange)
{
    FnTrace("Zone::AlterPosition()");
    if (page == NULL)
        return 1;

    int grid_x = t->grid_x;
    int new_x = x + xchange;
    if ((new_x + w) <= 0)
        new_x = -w + grid_x;
    else if (new_x >= page->width)
        new_x = page->width - grid_x;

    int grid_y = t->grid_y;
    int new_y = y + ychange;
    if ((new_y + h) <= 0)
        new_y = -h + grid_y;
    else if (new_y >= page->height)
        new_y = page->height - grid_y;

    if (new_x != x || new_y != y)
        SetPosition(t, new_x, new_y);
    return 0;
}

int Zone::ChangeJumpID(int old_id, int new_id)
{
    FnTrace("Zone::ChangeJumpID()");
    if (old_id == 0)
        return 1;

    if (old_id == new_id)
        return 0;

    if (JumpID())
    {
        if (*JumpID() == old_id)
            *JumpID() = new_id;
    }

    if (Script())
    {
        int j[16];
        int jump_count = sscanf(Script()->Value(),
                                "%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d",
                                &j[0], &j[1], &j[2], &j[3], &j[4], &j[5], &j[6], &j[7], &j[8],
                                &j[9], &j[10], &j[11], &j[12], &j[13], &j[14], &j[15]);

        if (jump_count > 0)
        {
            int change_flag = 0;
            int i;
            for (i = 0; i < jump_count; ++i)
                if (j[i] == old_id)
                {
                    j[i] = new_id;
                    change_flag = 1;
                }

            if (change_flag)
            {
                genericChar str[256] = "";
                for (i = 0; i < jump_count; ++i)
                {
                    genericChar num[16];
                    if (i == 0)
                        sprintf(num, "%d", j[0]);
                    else
                        sprintf(num, " %d", j[i]);
                    strcat(str, num);
                }
                Script()->Set(str);
            }
        }
    }
    return 0;
}

int Zone::RenderShadow(Terminal *t)
{
    FnTrace("Zone::RenderShadow()");
    int s = ShadowVal(t);
    if (s <= 0)
        return 1;

    int state = State(t);
    int zf = frame[state], zt = texture[state];
    if (zf == ZF_HIDDEN || (zf == ZF_NONE && zt == IMAGE_CLEAR))
        return 0;

    t->RenderShadow(x, y, w, h, shadow, shape);
    return 0;
}


/***********************************************************************
 * Page Class
 ***********************************************************************/
Page::Page()
{
    next        = NULL;
    fore        = NULL;
    parent_page = NULL;
    id          = 0;
    parent_id   = 0;
    image       = IMAGE_DEFAULT;
    title_color = COLOR_DEFAULT;
    type        = PAGE_ITEM;
    index       = INDEX_GENERAL;
    size        = SIZE_1024x768;
    changed     = 0;

    // Defaults
    default_font       = FONT_DEFAULT;
    default_frame[0]   = ZF_DEFAULT;
    default_texture[0] = IMAGE_DEFAULT;
    default_color[0]   = COLOR_DEFAULT;
    default_frame[1]   = ZF_DEFAULT;
    default_texture[1] = IMAGE_DEFAULT;
    default_color[1]   = COLOR_DEFAULT;
    default_frame[2]   = ZF_HIDDEN;
    default_texture[2] = IMAGE_DEFAULT;
    default_color[2]   = COLOR_DEFAULT;
    default_spacing    = 0;
    default_shadow     = SHADOW_DEFAULT;
}

int Page::Init(ZoneDB *zone_db)
{
    FnTrace("Page::Init()");
	// Get page width
	switch (size)
	{
    case SIZE_640x480:   width = 640;  height = 480;  break;
    case SIZE_800x600:   width = 800;  height = 600;  break;
    case SIZE_1024x600:   width = 1024;  height = 600;  break;
    case SIZE_1024x768:  width = 1024; height = 768;  break;
    case SIZE_1280x800: width = 1280; height = 800; break;   
    case SIZE_1280x1024: width = 1280; height = 1024; break;
    case SIZE_1366x768:  width = 1366;  height = 768; break;
    case SIZE_1440x900:  width = 1440;  height = 900; break;
    case SIZE_1600x900: width = 1600; height = 900; break;
    case SIZE_1680x1050: width = 1680; height = 1050; break;
    case SIZE_1920x1080: width = 1920; height = 1080; break;
    case SIZE_1920x1200: width = 1920; height = 1200; break;
    case SIZE_2560x1440: width = 2560; height = 1440; break;	
    case SIZE_2560x1600: width = 2560; height = 1600; break;
	}

	// FIX - should look up these values instead of having hardcoded values
	switch (type)
	{
    case PAGE_INDEX:     parent_id = -99; break;
    case PAGE_ITEM:      parent_id = -98; break;
    case PAGE_ITEM2:     parent_id = -98; break;
    case PAGE_SCRIPTED:  parent_id = -98; break;
    case PAGE_SCRIPTED2: parent_id = -99; break;
    case PAGE_SCRIPTED3: parent_id = -97; break;
    case PAGE_TABLE:     parent_id = PAGEID_TABLE; break;
    case PAGE_TABLE2:    parent_id = PAGEID_TABLE2; break;
    case PAGE_LIBRARY:   parent_id = 0; break;
	}

	if (zone_db)
		parent_page = zone_db->FindByID(parent_id, size);
	else
		parent_page = NULL;

	// Check for circular parent pointers
	int count = 0;
	Page *p = parent_page;
	while (p)
	{
		if (p == this || count > 16)
		{
			// loop detected - kill parent pointer
			parent_id   = 0;
			parent_page = NULL;
			break;
		}
		++count;
		p = p->parent_page;
	}
	return 0;
}

int Page::Add(Zone *z)
{
    FnTrace("Page::Add()");
    if (z == NULL)
        return 1;

    z->page = this;
    zone_list.AddToTail(z);

    // Bit of error checking
    if (z->JumpType() && z->JumpID())
    {
        int t = *z->JumpType();
        if (t == JUMP_NONE || t == JUMP_RETURN || t == JUMP_HOME ||
            t == JUMP_SCRIPT || t == JUMP_INDEX)
            *z->JumpID() = 0;
        else if (*z->JumpID() == 0)
            *z->JumpType() = JUMP_NONE;
    }
    return 0;
}

int Page::AddFront(Zone *z)
{
    FnTrace("Page::AddFront()");
    if (z == NULL)
        return 1;

    z->page = this;
    zone_list.AddToHead(z);

    // Bit of error checking
    if (z->JumpType() && z->JumpID())
    {
        int t = *z->JumpType();
        if (t == JUMP_NONE || t == JUMP_RETURN || t == JUMP_HOME ||
            t == JUMP_SCRIPT || t == JUMP_INDEX)
            *z->JumpID() = 0;
        else if (*z->JumpID() == 0)
            *z->JumpType() = JUMP_NONE;
    }
    return 0;
}

int Page::Remove(Zone *z)
{
    FnTrace("Page::Remove()");
    if (z == NULL)
        return 1;

    zone_list.Remove(z);
    z->page = NULL;
    return 0;
}

int Page::Purge()
{
    zone_list.Purge();
    return 0;
}

RenderResult Page::Render(Terminal *term, int update_flag, int no_parent)
{
    FnTrace("Page::Render()");
    Zone *currZone;

    // Init & Render Shadows
    Page *currPage = this;
    while (currPage)
	{
		currZone = currPage->ZoneList(); //grab the first zone in the list
		while (currZone)
		{
			if (update_flag)
				currZone->RenderInit(term, update_flag);

			if (currZone->shadow > 0)
				currZone->RenderShadow(term);

			currZone = currZone->next;
		}

		if (no_parent)
			break;

		currPage = currPage->parent_page;
	}

    // Render Zones
    currPage = this;
    while (currPage)
    {
        currZone = currPage->ZoneListEnd();  // grab last zone in list
        while (currZone)
        {
            if (currZone->update)
                currZone->Render(term, currZone->update);
            else
                currZone->Render(term, update_flag);

            currZone = currZone->fore;
        }

        if (no_parent)
            break;

        currPage = currPage->parent_page;
    }

    // Render Edit Cursors
    currPage = this;
    while (currPage)
    {
        currZone = currPage->ZoneListEnd();
        while (currZone)
        {
            if (currZone->edit)
                term->RenderEditCursor(currZone->x, currZone->y, currZone->w, currZone->h);
            currZone = currZone->fore;
        }
        currPage = currPage->parent_page;
    }

    // Render Dialog
    currZone = term->dialog;
    if (currZone)
    {
        currZone->RenderShadow(term);
        if (currZone->update)
            currZone->Render(term, currZone->update);
        else
            currZone->Render(term, update_flag);
    }
    return RENDER_OKAY;
}

RenderResult Page::Render(Terminal *t, int update_flag,
                          int rx, int ry, int rw, int rh)
{
    FnTrace("Page::Render(x,y,w,h)");
    Zone *z;
    // Init & Render Shadows
    Page *p = this;
    while (p)
    {
        z = p->ZoneList();
        while (z)
        {
            if (update_flag)
                z->RenderInit(t, update_flag);
            int s = z->ShadowVal(t);
            if (s > 0)
            {
                RegionInfo sr(z->x + s, z->y + s, z->w, z->h);
                if (sr.Overlap(rx, ry, rw, rh))
                    z->RenderShadow(t);
            }
            z = z->next;
        }
        p = p->parent_page;
    }

    // Render Zones
    p = this;
    while (p)
    {
        z = p->ZoneListEnd();
        while (z)
        {
            if (z->Overlap(rx, ry, rw, rh))
            {
                if (z->update)
                    z->Render(t, z->update);
                else
                    z->Render(t, update_flag);
            }
            z = z->fore;
        }
        p = p->parent_page;
    }

    // Render Edit Cursors
    p = this;
    while (p)
    {
        z = p->ZoneListEnd();
        while (z)
        {
            if (z->edit && z->Overlap(rx, ry, rw, rh))
                t->RenderEditCursor(z->x, z->y, z->w, z->h);
            z = z->fore;
        }
        p = p->parent_page;
    }

    z = t->dialog;
    if (z)
    {
        RegionInfo dr(z);
        dr.x += z->shadow;
        dr.y += z->shadow;
        if (dr.Overlap(rx, ry, rw, rh))
            z->RenderShadow(t);
        if (z->Overlap(rx, ry, rw, rh))
        {
            if (z->update)
                z->Render(t, z->update);
            else
                z->Render(t, update_flag);
        }
    }
    return RENDER_OKAY;
}

SignalResult Page::Signal(Terminal *t, const genericChar* message, int group_id)
{
    FnTrace("Page::Signal()");
    SignalResult sig = SIGNAL_IGNORED;
    SignalResult res;

    Page *startpage = t->page;
    for (Page *p = this; p != NULL; p = p->parent_page)
    {
        for (Zone *z = p->ZoneList(); z != NULL; z = z->next)
        {
            if (z->AcceptSignals() &&
                z->active &&
                (group_id == 0 || z->group_id == group_id))
            {
                res = z->Signal(t, message);
                switch (res)
                {
                case SIGNAL_ERROR:
                    return SIGNAL_ERROR;
                case SIGNAL_END:
                    return SIGNAL_END;
                case SIGNAL_OKAY:
                    sig = SIGNAL_OKAY;
                    break;
                default:
                    break;
                }
                if (t->page != startpage)
                    return sig;  // stop giving the signal if page changes
            }
        }
    }
    return sig;
}

SignalResult Page::Keyboard(Terminal *t, int key, int state)
{
    FnTrace("Page::Keyboard()");
    SignalResult sig = SIGNAL_IGNORED;

    Page *startpage = t->page;
    for (Page *p = this; p != NULL; p = p->parent_page)
    {
        for (Zone *z = p->ZoneList(); z != NULL; z = z->next)
        {
            if (z->active)
            {
                switch (z->Keyboard(t, key, state))
                {
                case SIGNAL_ERROR:
                    return SIGNAL_ERROR;
                case SIGNAL_END:
                    return SIGNAL_END;
                case SIGNAL_OKAY:
                    sig = SIGNAL_OKAY;
                    break;
                default:
                    break;
                }
            }
            if (t->page != startpage)
                return sig;  // stop giving the keyboard input if page changes
        }
    }
    return sig;
}

Zone *Page::FindZone(Terminal *t, int x, int y)
{
    FnTrace("Page::FindZone()");
    Zone *z;
    if (parent_page)
    {
        z = parent_page->FindZone(t, x, y);
        if (z)
            return z;
    }

    z = zone_list.Head();
    while (z)
    {
        if (z->behave != BEHAVE_MISS && z->active && z->IsPointIn(x, y))
            return z;
        z = z->next;
    }
    return NULL;
}

Zone *Page::FindEditZone(Terminal *t, int x, int y)
{
    FnTrace("Page::FindEditZone()");
    Zone *z;
    if (parent_page)
    {
        z = parent_page->FindEditZone(t, x, y);
        if (z)
            return z;
    }

    z = zone_list.Head();
    while (z)
    {
        if (z->IsPointIn(x, y) && z->CanSelect(t))
            return z;
        z = z->next;
    }
    return NULL;
}

Zone *Page::FindTranslateZone(Terminal *t, int x, int y)
{
    FnTrace("Page::FindTranslateZone()");
    Zone *z;
    if (parent_page)
    {
        z = parent_page->FindEditZone(t, x, y);
        if (z)
            return z;
    }

    z = zone_list.Head();
    while (z)
    {
        if (z->IsPointIn(x, y))
            return z;
        z = z->next;
    }
    return NULL;
}

int Page::IsZoneOnPage(Zone *z)
{
    FnTrace("Page::IsZoneOnPage()");
    Page *p = this;
    while (p)
    {
        Zone *zz = p->ZoneList();
        while (zz)
        {
            if (zz == z)
                return 1;  // True
            zz = zz->next;
        }
        p = p->parent_page;
    }
    return 0;  // False
}

int Page::Update(Terminal *t, int update_message, const genericChar* value)
{
    FnTrace("Page::Update()");
    Page *p = this;
    while (p)
    {
        Zone *z = p->ZoneList();
        while (z)
        {
            z->Update(t, update_message, value);
            z = z->next;
        }
        p = p->parent_page;
    }
    return 0;
}

int Page::Class()
{
    FnTrace("Page::Class()");
    if (id < 0)
        return PAGECLASS_SYSTEM;
    else if (IsTable())
        return PAGECLASS_TABLE;
    else
        return PAGECLASS_MENU;
}

int Page::IsStartPage()
{
    FnTrace("Page::IsStartPage()");
    return (IsTable() || IsKitchen() || IsBar());
}

int Page::IsTable()
{
    FnTrace("Page::IsTable()");
    return (type == PAGE_TABLE || type == PAGE_TABLE2);
}

int Page::IsKitchen()
{
    FnTrace("Page::IsKitchen()");
    return (type == PAGE_KITCHEN_VID || type == PAGE_KITCHEN_VID2);
}

int Page::IsBar()
{
    FnTrace("Page::IsBar()");
    return (type == PAGE_BAR1 || type == PAGE_BAR2);
}


/***********************************************************************
 * ZoneDB Class
 ***********************************************************************/
ZoneDB::ZoneDB()
{
    table_pages = 0;

    // Defaults
    default_font       = FONT_TIMES_24;
    default_frame[0]   = ZF_RAISED;
    default_texture[0] = IMAGE_SAND;
    default_color[0]   = COLOR_BLACK;
    default_frame[1]   = ZF_RAISED;
    default_texture[1] = IMAGE_LIT_SAND;
    default_color[1]   = COLOR_BLACK;
    default_frame[2]   = ZF_HIDDEN;
    default_texture[2] = IMAGE_SAND;
    default_color[2]   = COLOR_BLACK;
    default_spacing    = 2;
    default_shadow     = 0;
    default_image      = IMAGE_GRAY_MARBLE;
    default_title_color = COLOR_BLUE;
    default_size        = SIZE_1024x768;
}

// Member Functions
int ZoneDB::Init()
{
    FnTrace("ZoneDB::Init()");
    table_pages = 0;
    int last_page = 0;

    Page *p = page_list.Head();
    while (p)
    {
        if (p->IsTable() && (p->id != 0) && (p->id != last_page))
        {
            last_page = p->id;
            ++table_pages;
        }

        int error = p->Init(this);
        if (error)
            return error;
        p = p->next;
    }
    return 0;
}

int ZoneDB::Load(const char* filename)
{
    FnTrace("ZoneDB::Load()");

    // Load Pages
    int version = 0;
    InputDataFile infile;
    if (infile.Open(filename, version))
        return 1;

    genericChar str[STRLENGTH];
    if (version < 17 || version > ZONE_VERSION)
    {
        sprintf(str, "Unknown ZoneDB file version %d", version);
        ReportError(str);
        return 1;  // Error
    }

    int p_count = -1;
    infile.Read(p_count);

    for (int i = 0; i < p_count; ++i)
    {
        if (infile.end_of_file)
        {
            std::string msg = "Unexpected end of file: '" + infile.FileName() + "'";
            ReportError(msg);
            return 1;  // Error
        }

        Page *currPage = NewPosPage();
        if (currPage)
		{
			if (currPage->Read(infile, version))
			{
				sprintf(str, "Error in page %d '%s' of file '%s'",
						currPage->id, currPage->name.Value(), filename);

				ReportError(str);
				delete currPage;
				return 1;  // Error;
			}
            if (currPage->id > 100000)
            {
                snprintf(str, STRLENGTH, "Bad Page ID:  %d", currPage->id);
                ReportError(str);
                delete currPage;
            }
			else if (Add(currPage))
			{
				ReportError("Error adding page to ZoneDB");
				delete currPage;
				return 1;  // Error;
			}
		}
    }

	// read global default properties
	// note ZoneDB::Load() isn't used for system pages
    if (version >= 28) {
		infile.Read(default_font);
		infile.Read(default_shadow);
		infile.Read(default_spacing);
		for (int i=0; i<3; i++) {
			infile.Read(default_frame[i]);
			infile.Read(default_texture[i]);
			infile.Read(default_color[i]);
		}
    	infile.Read(default_image);
    	infile.Read(default_title_color);
    	infile.Read(default_size);
	}
	
    return 0;
}

int ZoneDB::Save(const char* filename, int page_class)
{
    FnTrace("ZoneDB::Save()");
    if (filename == NULL)
        return 1;

    // Count pages to save
    Page *p = page_list.Head();
    int save_pages = 0;
    while (p)
    {
        if (p->Class() & page_class)
            ++save_pages;
        p = p->next;
    }

    // see Zone::Read() (PosZone; pos_zone.cc) for version notes
    OutputDataFile df;
    if (df.Open(filename, ZONE_VERSION, 1))
        return 1;

    int error = 0;
    error += df.Write(save_pages, 1);
    p = page_list.Head();
    while (p)
    {
        if (p->Class() & page_class)
            error += p->Write(df, ZONE_VERSION);
        p = p->next;
    }

	// save global default properties
	df.Write(default_font);
	df.Write(default_shadow);
	df.Write(default_spacing);
	for (int i=0; i<3; i++) {
		df.Write(default_frame[i]);
		df.Write(default_texture[i]);
		df.Write(default_color[i]);
	}
	df.Write(default_image);
	df.Write(default_title_color);
	df.Write(default_size, 1);

    return error;
}

int ZoneDB::LoadPages(const char* path)
{
    FnTrace("ZoneDB::LoadPages()");
    return 0;
}

int ZoneDB::SaveChangedPages()
{
    FnTrace("ZoneDB::SaveChangedPages()");
    return 0;
}

int ZoneDB::ImportPage(const char* filename)
{
    FnTrace("ZoneDB::ImportPage()");
    int retval = 0;
    int pagenum;
    int idx;
    int len = strlen(filename);
    Page *newpage = NULL;
    InputDataFile infile;
    int version = 0;
    char str[STRLONG];
    int count = 0;
    SalesItem *salesitem = NULL;
    SalesItem *olditem = NULL;

    idx = len - 1;
    while (idx > 0 && filename[idx - 1] != '_')
        idx -= 1;
    pagenum = atoi(&filename[idx]);

    snprintf(str, STRLONG, "Importing page %d", pagenum);
    ReportError(str);

    // load the file into a Page object
    if (infile.Open(filename, version))
        return retval;
    if (version < 17 || version > ZONE_VERSION)
    {
        sprintf(str, "Unknown ZoneDB file version %d", version);
        ReportError(str);
        return 1;  // Error
    }
    newpage = NewPosPage();
    if (newpage != NULL)
    {
        newpage->Read(infile, version);
        newpage->id = pagenum;
        AddUnique(newpage);
    }
    // now read the SalesItems
    infile.Read(count);
    while (count > 0)
    {
        salesitem = new SalesItem();
        salesitem->Read(infile, SALES_ITEM_VERSION);
        olditem = MasterSystem->menu.FindByName(salesitem->item_name.Value());
        if (olditem != NULL)
            MasterSystem->menu.Remove(olditem);
        MasterSystem->menu.Add(salesitem);
        count -= 1;
    }

    return retval;
}

/****
 * ImportPages:  Returns the number of pages imported.
 ****/
int ZoneDB::ImportPages()
{
    FnTrace("ZoneDB::ImportPages()");
    char importdir[STRLONG];
    DIR *dir = NULL;
    struct dirent *record = NULL;
    const char* name = NULL;
    char fullpath[STRLONG];
    int count = 0;

    MasterSystem->FullPath(PAGEIMPORTS_DIR, importdir);
    dir = opendir(importdir);
    if (dir != NULL)
    {
        record = readdir(dir);
        while (record != NULL)
        {
            name = record->d_name;
            if (strncmp(name, "page_", 5) == 0)
            {
                snprintf(fullpath, STRLONG, "%s/%s", importdir, name);
                if (ImportPage(fullpath) == 0)
                {
                    unlink(fullpath);
                    count += 1;
                }
            }
            record = readdir(dir);
        }
        closedir(dir);
    }

    return count;
}

int ZoneDB::ExportPage(Page *page)
{
    FnTrace("ZoneDB::ExportPage()");
    int retval = 1;
    char fullpath[STRLONG];
    char filepath[STRLONG];
    OutputDataFile outfile;
    Zone *zone = NULL;
    SalesItem *salesitem = NULL;
    int count = 0;

    if (page == NULL)
        return retval;

    MasterSystem->FullPath(PAGEEXPORTS_DIR, fullpath);
    EnsureFileExists(fullpath);
    snprintf(filepath, STRLONG, "/page_%d", page->id);
    strcat(fullpath, filepath);
    if (outfile.Open(fullpath, ZONE_VERSION) == 0)
    {
        page->Write(outfile, ZONE_VERSION);
        zone = page->ZoneList();
        while (zone != NULL)
        {
            if (zone->ItemName())
                count += 1;
            zone = zone->next;
        }
        outfile.Write(count);
        zone = page->ZoneList();
        while (zone != NULL)
        {
            if (zone->ItemName())
            {
                salesitem = MasterSystem->menu.FindByName(zone->ItemName()->Value());
                if (salesitem != NULL)
                    salesitem->Write(outfile, SALES_ITEM_VERSION);
            }
            zone = zone->next;
        }
        zone = page->ZoneList();
        outfile.Close();
    }

    return retval;
}

int ZoneDB::Add(Page *p)
{
    FnTrace("ZoneDB::Add()");
    if (p == NULL)
        return 1;

    // start at end of list and work backwords
    Page *ptr = page_list.Tail();
    while (ptr && (p->id < ptr->id || (p->id == ptr->id && p->size > ptr->size)))
        ptr = ptr->fore;

    // Insert p after ptr
    return page_list.AddAfterNode(ptr, p);
}

int ZoneDB::AddUnique(Page *page)
{
    FnTrace("ZoneDB::AddUnique()");
    int retval = 0;
    Page *oldpage = NULL;
    int pagenum = page->id;
    char str[STRLENGTH];

    // remove a page if there is already one at this ID (of the same size)
    oldpage = FindByID(pagenum, page->size);
    if (oldpage != NULL && oldpage->size == page->size)
    {
        if (Remove(oldpage))
        {
            sprintf(str, "Error removing page %d", pagenum);
            ReportError(str);
            return 1;  // Error
        }   
    }

    // add the new page in
    if (Add(page))
    {
        sprintf(str, "Error adding page %d", pagenum);
        ReportError(str);
        return 1;  // Error
    }

    return retval;
}

int ZoneDB::Remove(Page *p)
{
    FnTrace("ZoneDB::Remove()");
    return page_list.Remove(p);
}

int ZoneDB::Purge()
{
    FnTrace("ZoneDB::Purge()");
    page_list.Purge();
    return 0;
}

Page *ZoneDB::FindByID(int id, int max_size)
{
    FnTrace("ZoneDB::FindByID()");
	if (id == 0)
		return NULL;

    Page *retPage = NULL;
	Page *currPage = page_list.Head();
	while ((retPage == NULL) && (currPage != NULL))
	{
		if (currPage->id == id && currPage->size <= max_size)
			retPage = currPage;
        else
            currPage = currPage->next;
	}
	return retPage;
}

Page *ZoneDB::FindByType(int type, int period, int max_size)
{
    FnTrace("ZoneDB::FindByType()");
    Page *thisPage = page_list.Head();
    while (thisPage)
    {
        if ((thisPage->type == type) && 
            (thisPage->index == period || period == INDEX_ANY) &&
            (thisPage->size <= max_size) && 
            (thisPage->id != 0))
        {
            return thisPage;
        }
        if (thisPage == page_list.Tail()) {
        	return NULL;
        } else {
            thisPage = thisPage->next;
        }
    }
    return NULL;
}

Page *ZoneDB::FindByTerminal(int term_type, int period, int max_size)
{
    FnTrace("ZoneDB::FindByTerminal()");
    int type = 0;
    switch (term_type)
    {
        case TERMINAL_BAR: type = PAGE_BAR1; break;
        case TERMINAL_BAR2: type = PAGE_BAR2; break;
        case TERMINAL_KITCHEN_VIDEO: type = PAGE_KITCHEN_VID; break;
        case TERMINAL_KITCHEN_VIDEO2: type = PAGE_KITCHEN_VID2; break;
    }
    if (type != 0)
        return FindByType(type, period, max_size);
    else
        return NULL;
}

Page *ZoneDB::FindByTerminalWithVariant(int term_type, int page_variant, int period, int max_size)
{
    FnTrace("ZoneDB::FindByTerminalWithVariant()");
    int type = 0;
    switch (term_type)
    {
        case TERMINAL_BAR: 
        case TERMINAL_BAR2: 
            type = (page_variant == 1) ? PAGE_BAR2 : PAGE_BAR1; 
            break;
        case TERMINAL_KITCHEN_VIDEO: 
        case TERMINAL_KITCHEN_VIDEO2: 
            type = (page_variant == 1) ? PAGE_KITCHEN_VID2 : PAGE_KITCHEN_VID; 
            break;
        case TERMINAL_NORMAL:
        case TERMINAL_FASTFOOD:
            // For normal terminals, choose between PAGE_TABLE and page -2
            if (page_variant == 1)
                return FindByID(-2, max_size); // Page -2
            else
                type = PAGE_TABLE; // Page -1
            break;
    }
    if (type != 0)
        return FindByType(type, period, max_size);
    else
        return NULL;
}

Page *ZoneDB::FirstTablePage(int max_size)
{
    FnTrace("ZoneDB::FirstTablePage()");
    Page *p = page_list.Head();
    while (p)
    {
        if (p->IsTable() && p->size <= max_size && p->id != 0)
            return p;
        p = p->next;
    }
    return NULL;
}

int ZoneDB::ChangePageID(Page *target, int new_id)
{
    FnTrace("ZoneDB::ChangePageID()");
    int old_id = target->id;
    if (old_id == new_id)
        return 0;

    if (old_id != 0)
    {
        Page *p = page_list.Head(), *pnext;
        while (p)
        {
            if (p->parent_id == old_id)
                p->parent_id = new_id;
            for (Zone *z = p->ZoneList(); z != NULL; z = z->next)
                z->ChangeJumpID(old_id, new_id);
            p = p->next;
        }

        p = page_list.Head();
        while (p)
        {
            pnext = p->next;
            if (p->id == old_id)
            {
                Remove(p);
                p->id = new_id;
                Add(p);
            }
            p = pnext;
        }
    }
    else
    {
        Remove(target);
        target->id = new_id;
        Add(target);
    }
    return 0;
}

int ZoneDB::IsPageDefined(int my_page_id, int size)
{
    FnTrace("ZoneDB::IsPageDefined()");
    if (my_page_id == 0)
        return 0;   // FALSE

    Page *p = page_list.Head();
    while (p)
    {
        if (p->id == my_page_id && p->size == size)
            return 1; // TRUE
        p = p->next;
    }
    return 0;     // FALSE
}

int ZoneDB::ClearEdit(Terminal *t)
{
    FnTrace("ZoneDB::ClearEdit()");
    RegionInfo r;
    int count = 0;

    Page *p = page_list.Head();;
    while (p)
    {
        Zone *z = p->ZoneList();
        while (z)
        {
            if (z->edit)
            {
                z->edit = 0;
                r.Fit(z->x, z->y, z->w, z->h);
                ++count;
            }
            z = z->next;
        }
        p = p->next;
    }

    if (count)
        t->Draw(0, r.x, r.y, r.w, r.h);
    return 0;
}

int ZoneDB::SizeEdit(Terminal *t, int wchange, int hchange,
                     int move_x, int move_y)
{
    FnTrace("ZoneDB::SizeEdit()");
    RegionInfo r;
    int count = 0, s;

    Zone *z;
    Page *p = t->page;
    while (p)
    {
        z = p->ZoneList();
        while (z)
        {
            if (z->edit)
            {
                s = z->ShadowVal(t);
                r.Fit(z->x, z->y, z->w + s, z->h + s);
                z->AlterSize(t, wchange, hchange, move_x, move_y);
                r.Fit(z->x, z->y, z->w + s, z->h + s);
                ++count;
            }
            z = z->next;
        }
        p = p->parent_page;
    }

    if (count > 0)
        t->Draw(0, r.x, r.y, r.w, r.h);
    return 0;
}

int ZoneDB::PositionEdit(Terminal *t, int xchange, int ychange)
{
    FnTrace("ZoneDB::PositionEdit()");
    RegionInfo r;
    int count = 0, s;

    Zone *z;
    Page *p = t->page;
    while (p)
    {
        z = p->ZoneList();
        while (z)
        {
            if (z->edit)
            {
                s = z->ShadowVal(t);
                r.Fit(z->x, z->y, z->w + s, z->h + s);
                z->AlterPosition(t, xchange, ychange);
                r.Fit(z->x, z->y, z->w + s, z->h + s);
                ++count;
            }
            z = z->next;
        }
        p = p->parent_page;
    }

    if (count > 0)
        t->Draw(0, r.x, r.y, r.w, r.h);
    return 0;
}

#define DEFAULT_MOVE  16
int ZoneDB::CopyEdit(Terminal *t, int modify_x, int modify_y)
{
    FnTrace("ZoneDB::CopyEdit()");
    RegionInfo r;
    int count = 0;

    Zone *list = NULL;
    Page *p = page_list.Head();
    while (p)
    {
        Zone *z = p->ZoneList();
        while (z)
        {
            if (z->edit && z->CanEdit(t))
            {
                z->edit = 0;
                auto zone_copy = z->Copy();
                Zone *ptr = zone_copy.get();
                if (ptr)
                {
                    zone_copy.release(); // Transfer ownership to the linked list
                    ptr->edit = 1;
                    int s = ptr->ShadowVal(t);
                    r.Fit(ptr->x, ptr->y, ptr->w + s, ptr->h + s);

                    if (t->page->IsZoneOnPage(z))
                    {
                        if (modify_x || modify_y)
                        {
                            if (modify_x > 0)
                                ptr->x = z->x + z->w + modify_x;
                            else if (modify_x < 0)
                                ptr->x = z->x - z->w + modify_x; // it's negative, so add to subtract
                            else
                                ptr->x = z->x;
                            if (modify_y > 0)
                                ptr->y = z->y + z->h + modify_y;
                            else if (modify_y < 0)
                                ptr->y = z->y - z->h + modify_y; // ibid
                            else
                                ptr->y = z->y;
                        }
                        else
                        {
                            ptr->x = z->x + DEFAULT_MOVE;
                            ptr->y = z->y + DEFAULT_MOVE;
                        }
                        r.Fit(ptr->x, ptr->y, ptr->w + s, ptr->h + s);
                    }
                    ++count;
                    ptr->next = list;
                    list = ptr;
                }
            }
            z = z->next;
        }
        p = p->next;
    }

    while (list)
    {
        Zone *z = list;
        list = list->next;
        t->page->AddFront(z);
    }

    if (count)
        t->Draw(1, r.x, r.y, r.w, r.h);
    return 0;
}

int ZoneDB::RelocateEdit(Terminal *t)
{
    FnTrace("ZoneDB::RelocateEdit()");
    Zone *list = NULL;
    Page *p = page_list.Head();
    while (p)
    {
        Zone *z = p->ZoneList();
        while (z)
        {
            Zone *ptr = z->next;
            if (z->edit && z->CanEdit(t))
            {
                p->Remove(z);
                z->next = list;
                list = z;
            }
            z = ptr;
        }
        p = p->next;
    }

    RegionInfo r;
    int count = 0;
    while (list)
    {
        Zone *z = list;
        list = list->next;
        t->page->AddFront(z);

        int s = z->ShadowVal(t);
        r.Fit(z->x, z->y, z->w + s, z->h + s);
        ++count;
    }

    if (count)
        t->Draw(0, r.x, r.y, r.w, r.h);
    return 0;
}

int ZoneDB::DeleteEdit(Terminal *term)
{
    FnTrace("ZoneDB::DeleteEdit()");
    int count = 0;
    int retval = 0;
    Zone *del_zone = term->page->ZoneList();
    while (del_zone)
    {
        Zone *next_zone = del_zone->next;
        if (del_zone->edit && del_zone->CanEdit(term))
        {
            term->page->Remove(del_zone);
            count++;
        }
        del_zone = next_zone;
    }
    if (count)
        term->Draw(1);
    else
        retval = 1;
    return retval;
}

int ZoneDB::ToggleEdit(Terminal *t, int toggle)
{
    FnTrace("ZoneDB::ToggleEdit()");
    RegionInfo r;
    int count = 0;

    Zone *z;
    Page *p = t->page;
    while (p)
    {
        z = p->ZoneList();
        while (z)
        {
            if ((z->edit == 0 || toggle) && z->CanSelect(t))
            {
                z->edit ^= 1;
                r.Fit(z->x, z->y, z->w, z->h);
                ++count;
            }
            z = z->next;
        }
        p = p->parent_page;
    }

    if (count)
        t->Draw(0, r.x, r.y, r.w, r.h);
    return 0;
}

int ZoneDB::ToggleEdit(Terminal *t, int toggle, int rx, int ry, int rw, int rh)
{
    FnTrace("ZoneDB::ToggleEdit(x,y,w,h)");
    RegionInfo r;
    int count = 0;

    Zone *z;
    Page *p = t->page;
    while (p)
    {
        z = p->ZoneList();
        while (z)
        {
            if ((z->edit == 0 || toggle) && z->Overlap(rx, ry, rw, rh) &&
                z->CanSelect(t))
            {
                z->edit ^= 1;
                r.Fit(z->x, z->y, z->w, z->h);
                ++count;
            }
            z = z->next;
        }
        p = p->parent_page;
    }

    if (count)
        t->Draw(0, r.x, r.y, r.w, r.h);
    return 0;
}

std::unique_ptr<ZoneDB> ZoneDB::Copy()
{
    FnTrace("ZoneDB::Copy()");
    auto new_db = std::make_unique<ZoneDB>();
    if (!new_db)
    {
        ReportError("Couldn't create copy of ZoneDB");
        return nullptr;
    }

    Page *p = page_list.Head();
    while (p)
    {
        auto page_copy = std::unique_ptr<Page>(p->Copy());
        new_db->Add(page_copy.release());
        p = p->next;
    }

    new_db->table_pages = table_pages;
    new_db->default_font = default_font;
    new_db->default_shadow = default_shadow;
    new_db->default_spacing = default_spacing;
    new_db->default_image = default_image;
    new_db->default_title_color = default_title_color;
    new_db->default_size = default_size;
    for (int i=0; i<3; i++) {
    	new_db->default_frame[i] = default_frame[i];
    	new_db->default_texture[i] = default_texture[i];
    	new_db->default_color[i] = default_color[i];
    }

    return new_db;
}

int ZoneDB::References(Page *page, int *list, int my_max, int &count)
{
    FnTrace("ZoneDB::References()");
    int id = page->id;
    if (id == 0)
        return 0;

    count = 0;
    int ref = 0, last = 0;
    Page *thisPage = page_list.Head();
    while (thisPage)
	{
		if (thisPage != page)
		{
			if (thisPage->parent_id == id)
			{
				if (last != thisPage->id)
				{
					last = thisPage->id;
					if (ref < my_max)
						list[ref] = last;
					++ref;
				}
				++count;
			}

			Zone *thisZone = thisPage->ZoneList();
			while (thisZone)
			{
				if (thisZone->JumpID() && *thisZone->JumpID() == id)
				{
					if (last != thisPage->id)
					{
						last = thisPage->id;
						if (ref < my_max)
							list[ref] = last;
						++ref;
					}
					++count;
				}
				if (thisZone->Script())
				{
					int a[16], n;
					n = sscanf(thisZone->Script()->Value(), "%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d",
                               &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7],
                               &a[8], &a[9], &a[10], &a[11], &a[12], &a[13], &a[14], &a[15]);
					if (n > 0)
						for (int i = 0; i < n; ++i)
							if (a[i] == id)
							{
								if (last != thisPage->id)
								{
									last = thisPage->id;
									if (ref < my_max)
										list[ref] = last;
									++ref;
								}
								++count;
							}
				}
				thisZone = thisZone->next;
			}
		}
		thisPage = thisPage->next;
	}
    return ref;
}

int ZoneDB::PageListReport(Terminal *t, int show_system, Report *r)
{
    FnTrace("ZoneDB::PageListReport()");
    if (r == NULL)
        return 1;

    r->TextC("Page List", PRINT_UNDERLINE);
    r->NewLine();

    int count = 0;
    Page *p = page_list.Head();
    while (p)
    {
        if (show_system || (p->type != PAGE_SYSTEM && p->type != PAGE_CHECKS))
        {
            r->TextPosL(6, p->name.Value());
            if (p->id == 0)
                r->TextL("---");
            else
                r->NumberL(p->id);
            r->NewLine();
            ++count;
        }
        p = p->next;
    }

    r->NewLine();
    genericChar str[32];
    sprintf(str, "Total Pages: %d", count);
    r->TextC(str);
    return 0;
}

int ZoneDB::ChangeItemName(const char* old_name, const genericChar* new_name)
{
	FnTrace("ZoneDB::ChangeItemName()");
	int changed = 0;
	Page *thisPage = page_list.Head();
	while (thisPage)
	{
		for (Zone *z = thisPage->ZoneList(); z != NULL; z = z->next)
		{
			if (z->ItemName() &&
                StringCompare(z->ItemName()->Value(), old_name) == 0)
			{
				z->ItemName()->Set(new_name);
				++changed;
			}
		}
		thisPage = thisPage->next;
	}
	return changed;
}

/****
 * PrintZoneDB:  This is for debugging purposes.  It just prints a list of
 *   pages and zones either to the given file or to STDOUT.
 ****/
int ZoneDB::PrintZoneDB(const char* dest, int brief)
{
    int retval = 0;
    int outfd = STDOUT_FILENO;
    genericChar buffer[STRLONG];
    Zone *currZone;
    Page *currPage = page_list.Head();
    int pcount = 0;
    int zcount = 0;

    if (dest != NULL)
    {
        outfd = open(dest, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (outfd <= 0)
        {
            snprintf(buffer, STRLONG, "PrintZoneDB Error %d opening %s",
                     errno, dest);
            ReportError(buffer);
            outfd = STDOUT_FILENO;
        }
    }
    while (currPage != NULL)
    {
        if (brief == 0)
        {
            snprintf(buffer, 75, "Page (%d, %d):  %s",
                     currPage->id, currPage->size, currPage->name.Value());
            write(outfd, buffer, strlen(buffer));
            write(outfd, "\n", 1);
        }
        currZone = currPage->ZoneList();
        pcount += 1;
        while (currZone != NULL)
        {
            if (brief == 0)
            {
                snprintf(buffer, 75, "    Zone (%d):  %s",
                         currZone->Type(), currZone->name.Value());
                write(outfd, buffer, strlen(buffer));
                write(outfd, "\n", 1);
            }
            zcount += 1;
            currZone = currZone->next;
        }
        currPage = currPage->next;
    }
    snprintf(buffer, STRLONG, "There are %d pages and %d zones\n", pcount, zcount);
    write(outfd, buffer, strlen(buffer));
    if (outfd != STDOUT_FILENO)
        close(outfd);
    return retval;
}

/*
 * ValidateSystemPages - Check for System Pages with invalid parent_id values
 * System Pages (type PAGE_SYSTEM = 0) should never have parent_id > 0
 * This can happen if data files are corrupted or saved incorrectly
 *
 * Returns: Number of invalid System Pages found (0 = all valid)
 * Logs details about any invalid pages found
 */
int ZoneDB::ValidateSystemPages()
{
    FnTrace("ZoneDB::ValidateSystemPages()");
    int invalid_count = 0;

    Page *currPage = page_list.Head();
    while (currPage != NULL)
    {
        if (currPage->type == PAGE_SYSTEM && currPage->parent_id > 0)
        {
            invalid_count++;
            char buffer[STRLONG];
            snprintf(buffer, STRLONG,
                     "INVALID: System Page id=%d name='%s' has parent_id=%d (should be 0)",
                     currPage->id, currPage->name.Value(), currPage->parent_id);
            ReportError(buffer);
            logmsg(LOG_DEBUG, buffer);
        }
        currPage = currPage->next;
    }

    if (invalid_count > 0)
    {
        char buffer[STRLONG];
        snprintf(buffer, STRLONG,
                 "Found %d System Page(s) with invalid parent_id > 0. "
                 "This can occur due to corrupted data files or incorrect saving.",
                 invalid_count);
        ReportError(buffer);
        logmsg(LOG_WARNING, buffer);
    }

    return invalid_count;
}

