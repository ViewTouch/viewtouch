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
 * debug.cc
 * Some debug functions that may or may not help in the debugging process.
 * If they don't help, don't use them.  Heh.
 * Created By:  Bruce Alon King, Tue Apr  2 08:45:15 2002
 */

#include "debug.hh"
#include "labels.hh"
#include <string.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef DEBUG

// As long as DEBUG is defined, everything gets defined normally.  Otherwise,
// the header file will revert things to empty definitions that won't take up
// any processor time or memory; they simply get stripped from the executable.

const genericChar *pos_data_filename = VIEWTOUCH_PATH "/dat/tmp/posdata.txt";

const genericChar *event_names[] = {
    "Protocol Error",
    "Protocol Reply",
    "KeyPress",
    "KeyRelease",
    "ButtonPress",
    "ButtonRelease",
    "MotionNotify",
    "EnterNotify",
    "LeaveNotify",
    "FocusIn",
    "FocusOut",
    "KeymapNotify",
    "Expose",
    "GraphicsExpose",
    "NoExpose",
    "VisibilityNotify",
    "CreateNotify",
    "DestroyNotify",
    "UnmapNotify",
    "MapNotify",
    "MapRequest",
    "ReparentNotify",
    "ConfigureNotify",
    "ConfigureRequest",
    "GravityNotify",
    "ResizeRequest",
    "CirculateNotify",
    "CirculateRequest",
    "PropertyNotify",
    "SelectionClear",
    "SelectionRequest",
    "SelectionNotify",
    "ColormapNotify",
    "ClientMessage",
    "MappingNotify",
    "LASTEvent"
};
int num_events = sizeof(event_names) / sizeof(const char *);
/****
 *  GetXEventName:  a simple routine to pull the message name out of event_name
 *  and return it.
 ****/
const genericChar *GetXEventName( XEvent event )
{
    if ( event.type < num_events )
    {
        return( event_names[event.type] );
    }
    else
    {
        return( "" );
    }
}

/****
 * PrintXEventName:  print the names of all events other than a few that we
 *  don't want (it's boring to read a bunch of mouse movements).
 ****/
void PrintXEventName( XEvent event, const genericChar *function, FILE *stream )
{
    const genericChar *name = GetXEventName( event );
    if ( ( strcmp( name, "MotionNotify" ) == 0 ) ||
         ( strcmp( name, "NoExpose" ) == 0 ) )
    {
        // NULL BLOCK -- we want to skip these events
    }
    else
    {
        fprintf( stream, "%s XEvent:  %s\n", function, name );
    }
}

const char *term_codes[] = {
    "TERM_UPDATEALL",
    "TERM_UPDATEAREA",
    "TERM_SETCLIP",
    "TERM_BLANKPAGE",
    "TERM_BACKGROUND",
    "TERM_TITLEBAR",
    "TERM_ZONE",
    "TERM_TEXTL",
    "TERM_TEXTC",
    "TERM_TEXTR",
    "TERM_ZONETEXTL",
    "TERM_ZONETEXTC",
    "TERM_ZONETEXTR",
    "TERM_SHADOW",
    "TERM_RECTANGLE",
    "TERM_HLINE",
    "TERM_VLINE",
    "TERM_FRAME",
    "TERM_FILLEDFRAME",
    "TERM_STATUSBAR",
    "TERM_EDITCURSOR",
    "TERM_CURSOR",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "TERM_FLUSH_TS",
    "TERM_CALIBRATE_TS",
    "TERM_USERINPUT",
    "TERM_BLANKSCREEN",
    "TERM_SETMESSAGE",
    "TERM_CLEARMESSAGE",
    "TERM_BLANKTIME",
    "TERM_STORENAME",
    "",
    "",
    "TERM_SELECTOFF",
    "TERM_SELECTUPDATE",
    "TERM_EDITPAGE",
    "TERM_EDITZONE",
    "TERM_EDITMULTIZONE",
    "TERM_TRANSLATE",
    "TERM_LISTSTART",
    "TERM_LISTITEM",
    "TERM_LISTEND",
    "",
    "TERM_NEWWINDOW",
    "TERM_SHOWWINDOW",
    "TERM_KILLWINDOW",
    "TERM_TARGETWINDOW",
    "",
    "",
    "",
    "",
    "",
    "TERM_PUSHBUTTON",
    "TERM_ITEMLIST",
    "TERM_ITEMMENU",
    "TERM_TEXTENTRY",
    "TERM_CONSOLE",
    "TERM_PAGEINDEX",
    "",
    "",
    "",
    "",
    "TERM_ICONIFY",
    "TERM_SOUND",
    "TERM_BELL",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "TERM_DIE"
};
int num_term_codes = sizeof( term_codes ) / sizeof( const genericChar * );
void PrintTermCode( int code )
{
    if ( code < num_term_codes )
    {
        printf( "Term Code:  %s\n", term_codes[code] );
    }
}

const char *server_codes[] = {
    "",
    "SERVER_ERROR",
    "SERVER_TERMINFO",
    "SERVER_TOUCH",
    "SERVER_KEY",
    "SERVER_MOUSE",
    "SERVER_PAGEDATA",
    "SERVER_ZONEDATA",
    "SERVER_ZONECHANGES",
    "SERVER_KILLPAGE",
    "SERVER_KILLZONE",
    "SERVER_KILLZONES",
    "SERVER_TRANSLATE",
    "SERVER_LISTSELECT",
    "SERVER_SWIPE",
    "SERVER_BUTTONPRESS",
    "SERVER_ITEMSELECT",
    "SERVER_TEXTENTRY",
    "",
    "",
    "SERVER_PRINTER_DONE",
    "SERVER_BADFILE"
};
int num_server_codes = sizeof( server_codes ) / sizeof( const genericChar * );
void PrintServerCode( int code )
{
    if ( code < num_server_codes )
    {
        printf( "Server Code:  %d %s\n", code, server_codes[code] );
    }
}

void PrintFamilyCode( int code )
{
    int idx = 0;
    int famvalue = FamilyValue[idx];
    while (famvalue >= 0 )
    {
        if (famvalue == code)
        {
            printf( "Family Name for %d is %s\n", code, FamilyName[idx] );
            famvalue = -1;  // End the while loop
        }
        else
        {
            idx += 1;
            famvalue = FamilyValue[idx];
        }
    }
}

//FIX BAK --> Should get the zone type names out of labels.cc or otherwise
//prevent the need to link everything with labels.o.  This solution, and really
//the entire labels.cc file, just seems kludgy.
const genericChar *GetZoneTypeName( int type )
{
    int idx = 0;
    int val = FullZoneTypeValue[idx];
    while ( ( val > -1 ) && ( val != type ) )
    {
        idx += 1;
        val = FullZoneTypeValue[idx];
    }
    if ( val != -1 )
    {
        return( FullZoneTypeName[idx] );
    }
    else
    {
        return( "" );
    }
}

#endif
