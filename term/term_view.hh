/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998  *  All Rights Reserved
 * Confidential and Proprietary Information
 *
 * term_view.hh - revision 38 (10/7/98)
 * Terminal Display module
 */

#ifndef _TERM_VIEW_HH
#define _TERM_VIEW_HH

#include "list_utility.hh"
#include "utility.hh"
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <array>
#include <string>
#include <optional>

// Constants
#define TEXT_COLORS 21

/**** Globals ****/
extern int SocketNo;
extern int TermNo;
extern int IsTermLocal;
extern int WinWidth;
extern int WinHeight;

extern std::array<int, TEXT_COLORS> ColorTextT;
extern std::array<int, TEXT_COLORS> ColorTextH;
extern std::array<int, TEXT_COLORS> ColorTextS;
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
extern Str TermStoreName;
extern Str Message;

extern short new_page_translations;
extern short new_zone_translations;

extern int   ConnectionTimeOut;  // for credit cards

extern int   allow_iconify;
extern int   use_embossed_text;  // whether to use embossed text effects
extern int   use_text_antialiasing;  // whether to use text anti-aliasing
extern int   use_drop_shadows;  // whether to use drop shadows on text
extern int   shadow_offset_x;  // shadow offset in X direction
extern int   shadow_offset_y;  // shadow offset in Y direction
extern int   shadow_blur_radius;  // shadow blur radius
extern int   silent_mode;        // to disable clone input

// Performance optimization: Cache for XRenderColor lookups
// Avoids expensive XQueryColor calls for every text render
struct ColorCache {
    std::array<XRenderColor, TEXT_COLORS> cached_colors;
    bool initialized;
    
    ColorCache() : initialized(false) {
        for (auto& c : cached_colors) {
            c.red = c.green = c.blue = c.alpha = 0;
        }
    }
    
    XRenderColor GetColor(Display* dis, int screen_no, int color_index) {
        if (!initialized || color_index < 0 || color_index >= TEXT_COLORS) {
            // Initialize cache on first use
            if (!initialized) {
                XColor xcolor;
                Colormap cmap = DefaultColormap(dis, screen_no);
                for (int i = 0; i < TEXT_COLORS; ++i) {
                    xcolor.pixel = ColorTextT[i];
                    XQueryColor(dis, cmap, &xcolor);
                    cached_colors[i].red = xcolor.red;
                    cached_colors[i].green = xcolor.green;
                    cached_colors[i].blue = xcolor.blue;
                    cached_colors[i].alpha = 0xFFFF;
                }
                initialized = true;
            }
        }
        
        if (color_index >= 0 && color_index < TEXT_COLORS) {
            return cached_colors[color_index];
        }
        
        // Fallback for invalid index
        XRenderColor fallback = {0, 0, 0, 0xFFFF};
        return fallback;
    }
};

extern ColorCache g_color_cache;


/*********************************************************************
 * Translations Classes
 ********************************************************************/

class Translation
{
    std::string key;
    std::string value;
public:
    Translation *next;
    Translation *fore;

    Translation();
    Translation(const char* new_key, const char* new_value);
    Translation(const Translation& other) = default;
    Translation(Translation&& other) noexcept = default;
    Translation& operator=(const Translation& other) = default;
    Translation& operator=(Translation&& other) noexcept = default;
    
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
  PAGE_SIZE_640x480 =  1,
  PAGE_SIZE_768x1024,
  PAGE_SIZE_800x480,
  PAGE_SIZE_800x600,
  PAGE_SIZE_1024x600,
  PAGE_SIZE_1024x768,
  PAGE_SIZE_1280x800,
  PAGE_SIZE_1280x1024,
  PAGE_SIZE_1366x768,
  PAGE_SIZE_1440x900,
  PAGE_SIZE_1600x900,
  PAGE_SIZE_1600x1200,
  PAGE_SIZE_1680x1050,
  PAGE_SIZE_1920x1080,
  PAGE_SIZE_1920x1200,
  PAGE_SIZE_2560x1440,
  PAGE_SIZE_2560x1600
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
#define SHAPE_TRIANGLE  6
#define FRAME_LIT       8   // alternate palette for frame
#define FRAME_DARK      16  // (use 1)
#define FRAME_INSET     32  // top-bottom, left-right colors switched
#define FRAME_2COLOR    64  // 2 colors used instead of 4

// Fonts
#define FONT_DEFAULT     0
#define FONT_TIMES_48    1
#define FONT_TIMES_48B   2
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
    Pixmap mask;  // Optional mask pixmap for transparency
    int width;
    int height;

    Xpm();
    Xpm(Pixmap pm);
    Xpm(Pixmap pm, int w, int h);
    Xpm(Pixmap pm, Pixmap m, int w, int h);  // Constructor with mask
    constexpr int Width() const noexcept { return width; }
    constexpr int Height() const noexcept { return height; }
    constexpr int PixmapID() const noexcept { return pixmap; }
    constexpr Pixmap MaskID() const noexcept { return mask; }
};

class Pixmaps {
    DList<Xpm> pixmaps;
    int count;
public:
    Pixmaps();
    int Add(Xpm *pixmap);
    Xpm *Get(int idx);
    Xpm *GetRandom();
    constexpr int Count() const noexcept { return count; }
};

extern Pixmaps PixmapList;


/*** Functions ****/
extern int OpenTerm(const char* display, TouchScreen *ts, int is_term_local, int term_hardware,
                    int set_width = -1, int set_height = -1);
extern int KillTerm();
extern int ShowCursor(int type);
extern int BlankScreen();
extern int DrawScreenSaver();
extern void ResetScreenSaver();
extern int ReconnectToServer();
extern void RestartTerminal();

extern XFontStruct *GetFontInfo(int font_id) noexcept;
extern XftFont *GetXftFontInfo(int font_id) noexcept;
extern int          GetFontBaseline(int font_id) noexcept;
extern int          GetFontHeight(int font_id) noexcept;
extern Pixmap       GetTexture(int texture) noexcept;

// Image loading functions
extern Pixmap LoadPixmap(const char** image_data);
extern Xpm *LoadPixmapFile(char* file_name);

#ifdef HAVE_PNG
extern Xpm *LoadPNGFile(const char* file_name);
#endif

#ifdef HAVE_JPEG
extern Xpm *LoadJPEGFile(const char* file_name);
#endif

#ifdef HAVE_GIF
extern Xpm *LoadGIFFile(const char* file_name);
#endif

extern int   WInt8(int val) noexcept;
extern int   RInt8() noexcept;
extern int   WInt16(int val) noexcept;
extern int   RInt16() noexcept;
extern int   WInt32(int val) noexcept;
extern int   RInt32() noexcept;
extern long  WLong(long val) noexcept;
extern long  RLong() noexcept;
extern long long WLLong(long long val) noexcept;
extern long long RLLong() noexcept;
extern int   WFlt(Flt val) noexcept;
extern Flt   RFlt() noexcept;
extern int   WStr(const char* s, int len = 0);
extern genericChar* RStr(genericChar* s = NULL);
extern int   SendNow() noexcept;
extern int   ReloadTermFonts();  // Reload fonts when global defaults change
void TerminalReloadFonts();

// Reconnection functions
extern int   ReconnectToServer();
extern void  RestartTerminal();

#endif

