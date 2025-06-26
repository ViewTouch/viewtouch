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
 * search_zone.cc - revision 1 (10/13/98)
 */

#include "search_zone.hh"
#include "terminal.hh"
#include <string.h>
#include <cctype>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** SearchZone Class ****/
// Constructor
SearchZone::SearchZone()
{
    search = 0;
    buffer[0] = '\0';
}

// Member Functions
RenderResult SearchZone::Render(Terminal *term, int update_flag)
{
    FnTrace("SearchZone::Render()");
    if (update_flag)
        search = 0;

    if (search)
    {
        LayoutZone::Render(term, update_flag);
        TextC(term, 0, "Search For...", color[0]);
        Entry(term, 2, 1.5, size_x - 4);
        genericChar str[STRLENGTH];
        snprintf(str, STRLENGTH, "%s_", buffer);
        TextC(term, 1.5, str, COLOR_WHITE);
    }
    else
        RenderZone(term, name.Value(), update_flag);

    return RENDER_OKAY;
}

SignalResult SearchZone::Signal(Terminal *term, const genericChar* message)
{
    FnTrace("SearchZone::Signal()");
    static const genericChar* commands[] =
        {"nextsearch", "backspace", "clear", nullptr};
    unsigned int bufflen = strlen(buffer);
    genericChar tbuff[STRLENGTH];
    SignalResult retval = SIGNAL_IGNORED;

    int idx = CompareList(message, commands);
    switch (idx)
    {
    case 0:
        if (bufflen > 0)
        {
            snprintf(tbuff, STRLENGTH, "nextsearch %s", buffer);
            term->Signal(tbuff, group_id);
            retval =  SIGNAL_OKAY;
        }
        break;
    case 1:  // backspace
        if (bufflen > 0)
            buffer[bufflen - 1] = '\0';
        break;
    case 2:  // clear
        buffer[0] = '\0';
        break;
    default:  // character entered from onscreen keyboard or keypad
        if (strlen(message) == 1)
        {
            snprintf(tbuff, STRLENGTH, "%s%s", buffer, message);
            strncpy(buffer, tbuff, STRLENGTH);
            retval = SIGNAL_OKAY;
        }
        break;
    }
    if (strlen(buffer) != bufflen)
        term->Draw(0);  // buffer contents changed; redraw
    return retval;
}

SignalResult SearchZone::Touch(Terminal *term, int tx, int ty)
{
    FnTrace("SearchZone::Touch()");
    genericChar msgbuffer[STRLENGTH];

    if (search == 0)
    {
        term->Signal("unfocus", group_id);
        search = 1;
        if (strlen(buffer) > 0)
        {
            snprintf(msgbuffer, STRLENGTH, "search %s", buffer);
            term->Signal(msgbuffer, group_id);
        }
        buffer[0] = '\0';
        Draw(term, 0);
    }
    else
        Draw(term, 1);
    return SIGNAL_OKAY;
}

SignalResult SearchZone::Keyboard(Terminal *term, int my_key, int state)
{
    FnTrace("SearchZone::Keyboard()");
    if (search == 0)
        return SIGNAL_IGNORED;

    genericChar str[STRLENGTH];
    int len = strlen(buffer);
    switch (my_key)
    {
    case 8:  // backspace
        if (len <= 0)
            return SIGNAL_IGNORED;
        buffer[len - 1] = '\0';
        break;
    case 9:  // tab
        snprintf(str, STRLENGTH, "nextsearch %s", buffer);
        term->Signal(str, group_id);
        return SIGNAL_END;
    case 13: // return
        Draw(term, 1);
        return SIGNAL_END;
    case 27: // escape (cancel)
        Draw(term, 1);
        return SIGNAL_END;
    default:
        if (!isprint(my_key) || len >= (int)(sizeof(buffer) - 2) ||
            len >= (size_x - 4))
            return SIGNAL_IGNORED;

        // FIX BAK-->Should check buffer length to make sure we don't spread
        // outside the enter field's width.  Also should prevent buffer
        // overflows.
        buffer[len]   = my_key;
        buffer[len+1] = '\0';
        break;
    }

    Draw(term, 0);
    snprintf(str, STRLENGTH, "search %s", buffer);
    term->Signal(str, group_id);
    return SIGNAL_END;
}

/****
 * LoseFocus:  The SearchZone should lose focus unless a message-type button
 *   is pressed.  For example, if the user clicks on a letter on an onscreen
 *   keyboard, that letter should go to this zone and this zone should 
 *   retain focus.  But if the user searches, and then selects a searched for
 *   item, then this zone should relinquish focus to the searched for item.
 ****/
int SearchZone::LoseFocus(Terminal *term, Zone *newfocus)
{
    FnTrace("SearchZone::LoseFocus()");

    Draw(term, 1);
    return 1;
}

