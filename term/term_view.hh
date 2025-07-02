/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998  *  All Rights Reserved
 * Confidential and Proprietary Information
 *
 * term_view.hh - revision 38 (10/7/98)
 * Terminal Display module
 */

#pragma once

#include "list_utility.hh"
#include "utility.hh"
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>


/**** Globals ****/
extern int SocketNo;
extern int TermNo;
extern int IsTermLocal;
extern int WinWidth;
extern int WinHeight;

extern int ColorTextT[];
extern int ColorTextH[];
extern int ColorTextS[];
extern int ColorBE;    // Bottom edge
extern int ColorLE;    // Left edge
extern int ColorRE;    // Right edge
extern int ColorTE;    // Top edge
extern int ColorLBE;   // Lit bottom edge
extern int ColorLLE;   // Lit left edge
extern int ColorLRE;   // Lit right edge
extern int ColorLTE;   // Lit top edge
extern int ColorDBE;   // Dark bottom edge
extern int ColorDLE;   // Dark left edge
extern int ColorDRE;   // Dark right edge
extern int ColorDTE;   // Dark top edge
extern int ColorBlack;
extern int ColorWhite;

extern Str TimeString;
extern Str StoreName;
extern Str Message;

extern short new_page_translations;
extern short new_zone_translations;

extern int   ConnectionTimeOut;  // for credit cards

extern int   allow_iconify;
extern int   silent_mode;        // to disable clone input


/*********************************************************************
 * Translations Classes
 ********************************************************************/

class Translation
{
    char key[STRLONG];
    char value[STRLONG];
public:
    Translation *next;
    Translation *fore;

    Translation();
    Translation(const char* new_key, const char* new_value);
    int Match(const char* check_key);
    int GetKey(char* store, int maxlen);
    int GetValue(char* store, int maxlen);
};

class Translations
{
    DList<Translation> trans_list;
public:
    Translations();
    void Clear() { trans_list.Purge(); }
    int AddTranslation(const char* key, const char* value);
    const char* GetTranslation(const char* key);
    void PrintTranslations();
};

extern Translations MasterTranslations;


/*********************************************************************
 * Definitions
 ********************************************************************/

// Mouse Messages
#define MOUSE_LEFT    1
#define MOUSE_MIDDLE  2
#define MOUSE_RIGHT   4
#define MOUSE_PRESS   8
#define MOUSE_DRAG    16
#define MOUSE_RELEASE 32
#define MOUSE_SHIFT   64

// Page Types
#define PAGE_SYSTEM        0 // Hidden, normally unmodifiable System Page
#define PAGE_TABLE         1 // Table layout page
#define PAGE_INDEX         2 // Top level menu page
#define PAGE_ITEM          3 // Menu Item ordering page
#define PAGE_SCRIPTED      5 // Page in a modifier script
#define PAGE_SCRIPTED2     6 // Alternate modifier page
#define PAGE_SCRIPTED3     4 // Yet another modifier page
#define PAGE_TEMPLATE      7 // Viewable System page
#define PAGE_LIBRARY       8 // user page for storing zones
#define PAGE_CHECKS       12 // Check list system page
#define PAGE_KITCHEN_VID  13 // List of checks for the cooks
#define PAGE_KITCHEN_VID2 14 // Secondary list of checks for cooks
#define PAGE_BAR1         15
#define PAGE_BAR2         16

#define MAX_SCREEN_WIDTH 1920
// #define MAX_SCREEN_WIDTH 1600
#define MAX_SCREEN_HEIGHT 1200

// Page Sizes (resolutions)
enum page_sizes {
  SIZE_640x480 =  1,
  SIZE_768x1024,
  SIZE_800x480,
  SIZE_800x600,
  SIZE_1024x600,
  SIZE_1024x768,
  SIZE_1280x800,
  SIZE_1280x1024,
  SIZE_1366x768,
  SIZE_1440x900,
  SIZE_1600x900,
  SIZE_1600x1200,
  SIZE_1680x1050,
  SIZE_1920x1080,
  SIZE_1920x1200,
  SIZE_2560x1440,
  SIZE_2560x1600
};

// Colors
#define COLOR_DEFAULT      255  // color determined by zone
#define COLOR_PAGE_DEFAULT 254  // color determined by page setting
#define COLOR_CLEAR        253  // text not rendered
#define COLOR_UNCHANGED    252
#define COLOR_BLACK        0
#define COLOR_WHITE        1
#define COLOR_RED          2
#define COLOR_GREEN        3
#define COLOR_BLUE         4
#define COLOR_YELLOW       5
#define COLOR_BROWN        6
#define COLOR_ORANGE       7
#define COLOR_PURPLE       8
#define COLOR_TEAL         9
#define COLOR_GRAY         10
#define COLOR_MAGENTA      11
#define COLOR_REDORANGE    12
#define COLOR_SEAGREEN     13
#define COLOR_LT_BLUE      14
#define COLOR_DK_RED       15
#define COLOR_DK_GREEN     16
#define COLOR_DK_BLUE      17
#define COLOR_DK_TEAL      18
#define COLOR_DK_MAGENTA   19
#define COLOR_DK_SEAGREEN  20

// Text Alignment
#define ALIGN_LEFT      0
#define ALIGN_CENTER    1
#define ALIGN_RIGHT     2

// Frame/Shape Properties
#define SHAPE_RECTANGLE 1   // shape is rectangle by default
#define SHAPE_DIAMOND   2
#define SHAPE_CIRCLE    3
#define SHAPE_HEXAGON   4
#define SHAPE_OCTAGON   5
#define FRAME_LIT       8   // alternate palette for frame
#define FRAME_DARK      16  // (use 1)
#define FRAME_INSET     32  // top-bottom, left-right colors switched
#define FRAME_2COLOR    64  // 2 colors used instead of 4

// Fonts
#define FONT_DEFAULT     0
#define FONT_TIMES_20    4
#define FONT_TIMES_24    5
#define FONT_TIMES_34    6
#define FONT_TIMES_20B   7
#define FONT_TIMES_24B   8
#define FONT_TIMES_34B   9
#define FONT_TIMES_14    10
#define FONT_TIMES_14B   11
#define FONT_TIMES_18    12
#define FONT_TIMES_18B   13
#define FONT_COURIER_18  14
#define FONT_COURIER_18B 15
#define FONT_COURIER_20  16
#define FONT_COURIER_20B 17

// Modern POS Fonts - DejaVu Sans (Superior for POS interfaces)
#define FONT_DEJAVU_14    18
#define FONT_DEJAVU_16    19
#define FONT_DEJAVU_18    20
#define FONT_DEJAVU_20    21
#define FONT_DEJAVU_24    22
#define FONT_DEJAVU_28    23
#define FONT_DEJAVU_14B   24
#define FONT_DEJAVU_16B   25
#define FONT_DEJAVU_18B   26
#define FONT_DEJAVU_20B   27
#define FONT_DEJAVU_24B   28
#define FONT_DEJAVU_28B   29

// Monospace Fonts - Perfect for prices and numbers
#define FONT_MONO_14      30
#define FONT_MONO_16      31
#define FONT_MONO_18      32
#define FONT_MONO_20      33
#define FONT_MONO_24      34
#define FONT_MONO_14B     35
#define FONT_MONO_16B     36
#define FONT_MONO_18B     37
#define FONT_MONO_20B     38
#define FONT_MONO_24B     39

// Classic Serif Fonts - EB Garamond 8, Bookman, Nimbus Roman
#define FONT_GARAMOND_14    40
#define FONT_GARAMOND_16    41
#define FONT_GARAMOND_18    42
#define FONT_GARAMOND_20    43
#define FONT_GARAMOND_24    44
#define FONT_GARAMOND_28    45
#define FONT_GARAMOND_14B   46
#define FONT_GARAMOND_16B   47
#define FONT_GARAMOND_18B   48
#define FONT_GARAMOND_20B   49
#define FONT_GARAMOND_24B   50
#define FONT_GARAMOND_28B   51

#define FONT_BOOKMAN_14     52
#define FONT_BOOKMAN_16     53
#define FONT_BOOKMAN_18     54
#define FONT_BOOKMAN_20     55
#define FONT_BOOKMAN_24     56
#define FONT_BOOKMAN_28     57
#define FONT_BOOKMAN_14B    58
#define FONT_BOOKMAN_16B    59
#define FONT_BOOKMAN_18B    60
#define FONT_BOOKMAN_20B    61
#define FONT_BOOKMAN_24B    62
#define FONT_BOOKMAN_28B    63

#define FONT_NIMBUS_14      64
#define FONT_NIMBUS_16      65
#define FONT_NIMBUS_18      66
#define FONT_NIMBUS_20      67
#define FONT_NIMBUS_24      68
#define FONT_NIMBUS_28      69
#define FONT_NIMBUS_14B     70
#define FONT_NIMBUS_16B     71
#define FONT_NIMBUS_18B     72
#define FONT_NIMBUS_20B     73
#define FONT_NIMBUS_24B     74
#define FONT_NIMBUS_28B     75

#define FONT_UNDERLINE  128

// Zone Frame Appearence
#define ZF_UNCHANGED 0  // no change (or default)
#define ZF_DEFAULT   1
#define ZF_HIDDEN    2  // frame, texture & text all hidden
#define ZF_NONE      3  // no frame
#define ZF_RAISED    10 // raised single frame (auto select best type)
#define ZF_RAISED1   11 // medium raised single frame
#define ZF_RAISED2   12 // lit raised single frame
#define ZF_RAISED3   13 // dark raised single frame
#define ZF_INSET     20 // inset single frame (auto select best type)
#define ZF_INSET1    21 // medium inset single frame
#define ZF_INSET2    22 // lit inset single frame
#define ZF_INSET3    23 // dark inset single frame
#define ZF_DOUBLE    30 // double raised frame (auto select best type)
#define ZF_DOUBLE1   31 // medium double raised frame
#define ZF_DOUBLE2   32 // lit double raised frame
#define ZF_DOUBLE3   33 // dark double raised frame

#define ZF_BORDER            40 // raised & inset frames filled with 'texture'
#define ZF_CLEAR_BORDER      41 // raised & inset frames
#define ZF_SAND_BORDER       42 // raised & inset frames filled with sand
#define ZF_LIT_SAND_BORDER   43 // raised & inset frames filled with lit sand
#define ZF_INSET_BORDER      44 // inset board fill with dark sand
#define ZF_PARCHMENT_BORDER  45 // raised & inset frames filled with parchment
#define ZF_DOUBLE_BORDER     50 // raised twice & inset frames filled with sand
#define ZF_LIT_DOUBLE_BORDER 51 // double_border with lit sand instead

// Cursor Types
#define CURSOR_DEFAULT 0
#define CURSOR_BLANK   1
#define CURSOR_POINTER 2
#define CURSOR_WAIT    3


/**** Types ****/
class TouchScreen;

class Xpm {
public:
    Xpm *next;
    Xpm *fore;
    Pixmap pixmap;
    int width;
    int height;

    Xpm();
    Xpm(Pixmap pm);
    Xpm(Pixmap pm, int w, int h);
    int Width() { return width; }
    int Height() { return height; }
    int PixmapID() { return pixmap; }
};

class Pixmaps {
    DList<Xpm> pixmaps;
    int count;
public:
    Pixmaps();
    int Add(Xpm *pixmap);
    Xpm *Get(int idx);
    Xpm *GetRandom();
    int Count() { return count; }
};

extern Pixmaps PixmapList;


/*** Functions ****/
extern int OpenTerm(const char* display, TouchScreen *ts, int is_term_local, int term_hardware,
                    int set_width = -1, int set_height = -1);
extern int KillTerm();
extern int ShowCursor(int type);
extern int BlankScreen();
extern int DrawScreenSaver();

extern XftFont *GetFontInfo(int font_id);
extern int          GetFontBaseline(int font_id);
extern int          GetFontHeight(int font_id);
extern Pixmap       GetTexture(int texture);
extern const char*  GetScalableFontName(int font_id);

extern int   WInt8(int val);
extern int   RInt8();
extern int   WInt16(int val);
extern int   RInt16();
extern int   WInt32(int val);
extern int   RInt32();
extern long  WLong(long val);
extern long  RLong();
extern long long WLLong(long long val);
extern long long RLLong();
extern int   WFlt(Flt val);
extern Flt   RFlt();
extern int   WStr(const char* s, int len = 0);
extern genericChar* RStr(genericChar* s = nullptr);
extern int   SendNow();

