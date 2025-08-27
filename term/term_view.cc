/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998  *  All Rights Reserved
 * Confidential and Proprietary Information
 *
 * term_view.cc - revision 103 (10/8/98)
 * Terminal Display module
 */

#include <Xm/Xm.h>
#include <Xm/MwmUtil.h>
#include <X11/keysym.h>
#include <X11/Xmu/Drawing.h>
#include <X11/cursorfont.h>
#include <X11/xpm.h>
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
#include <string>

#include "debug.hh"
#include "term_view.hh"
#include "term_dialog.hh"
#include "remote_link.hh"
#include "image_data.hh"
#include "touch_screen.hh"
#include "layer.hh"

#ifdef CREDITMCVE
#include "term_credit_mcve.hh"
#elif defined CREDITCHEQ
#include "term_credit_cheq.hh"
#else
#include "term_credit.hh"
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include <filesystem>       // generic filesystem functions available since C++17

namespace fs = std::filesystem;

/*********************************************************************
 * Definitions
 ********************************************************************/
#define UPDATE_TIME 500
#define XWD         "/usr/X11R6/bin/xwd"          /* screen capture utility */
#define SCREEN_DIR  "/usr/viewtouch/screenshots"  /* where to save screenshots */
// Font directory will be detected at runtime

#define TERM_RELOAD_FONTS 0xA5

/*********************************************************************
 * Data
 ********************************************************************/

struct FontDataType
{
    int id;
    const genericChar* font;
};

static FontDataType FontData[] =
{
    {FONT_TIMES_20,     "DejaVu Serif:size=12:style=Book"},
    {FONT_TIMES_24,     "DejaVu Serif:size=14:style=Book"},
    {FONT_TIMES_34,     "DejaVu Serif:size=18:style=Book"},
    {FONT_TIMES_20B,    "DejaVu Serif:size=12:style=Bold"},
    {FONT_TIMES_24B,    "DejaVu Serif:size=14:style=Bold"},
    {FONT_TIMES_34B,    "DejaVu Serif:size=18:style=Bold"},
    {FONT_TIMES_14,     "DejaVu Serif:size=10:style=Book"},
    {FONT_TIMES_14B,    "DejaVu Serif:size=10:style=Bold"},
    {FONT_TIMES_18,     "DejaVu Serif:size=11:style=Book"},
    {FONT_TIMES_18B,    "DejaVu Serif:size=11:style=Bold"},
    {FONT_COURIER_18,   "Liberation Serif:size=11:style=Regular"},
    {FONT_COURIER_18B,  "Liberation Serif:size=11:style=Bold"},
    {FONT_COURIER_20,   "Liberation Serif:size=12:style=Regular"},
    {FONT_COURIER_20B,  "Liberation Serif:size=12:style=Bold"}
};

struct PenDataType
{
    int id, t[3], s[3], h[3];
};

static PenDataType PenData[] =
{
// ColorID             Text Color       Shadow Color     HiLight Color
    {COLOR_BLACK,       {  0,   0,   0}, {249, 230, 210}, {148, 113,  78}},
    {COLOR_WHITE,       {255, 255, 255}, {  0,   0,   0}, {117,  97,  78}},
    {COLOR_RED,         {235,   0,   0}, { 47,   0,   0}, {242, 200, 200}},
    {COLOR_GREEN,       {  0, 128,   0}, {  0,  42,   0}, {140, 236, 140}},
    {COLOR_BLUE,        {  0,   0, 230}, {  0,   0,  47}, {200, 200, 240}},
    {COLOR_YELLOW,      {255, 255,   0}, {  0,   0,   0}, {127, 127,  78}},
    {COLOR_BROWN,       {132,  76,  38}, { 47,   0,   0}, {224, 212, 200}},
    {COLOR_ORANGE,      {255,  84,   0}, { 47,  23,   0}, {255, 222, 195}},
    {COLOR_PURPLE,      {100,   0, 200}, {  0,   0,  47}, {240, 200, 240}},
    {COLOR_TEAL,        {  0, 132, 168}, {  0,  16,  39}, {176, 216, 255}},
    {COLOR_GRAY,        { 96,  96,  96}, {  0,   0,   0}, {222, 222, 222}},
    {COLOR_MAGENTA,     {192,  48, 136}, { 47,   0,  24}, {232, 188, 210}},
    {COLOR_REDORANGE,   {255,  56,   0}, { 39,  19,   0}, {255, 218, 202}},
    {COLOR_SEAGREEN,    {  0, 128,  96}, {  0,  42,  21}, {127, 228, 200}},
    {COLOR_LT_BLUE,     {  0, 120, 255}, {  0,   0,  47}, {218, 218, 240}},
    {COLOR_DK_RED,      {165,   0,   0}, { 32,   0,   0}, {240, 200, 200}},
    {COLOR_DK_GREEN,    {  0,  90,   0}, {  0,  32,   0}, {140, 235, 140}},
    {COLOR_DK_BLUE,     {  0,   0, 145}, {  0,   0,  45}, {205, 205, 245}},
    {COLOR_DK_TEAL,     {  0,  92, 130}, {  0,  12,  30}, {176, 216, 255}},
    {COLOR_DK_MAGENTA,  {160,  32, 110}, { 32,   0,  16}, {232, 188, 210}},
    {COLOR_DK_SEAGREEN, {  0,  98,  72}, {  0,  32,  16}, {127, 228, 200}},
};

#define FONTS         (int)(sizeof(FontData)/sizeof(FontDataType))
#define FONT_SPACE    (FONTS+4)
#define TEXT_COLORS   (int)(sizeof(PenData)/sizeof(PenDataType))

class FontNameClass
{
private:
    char foundry[STRSHORT];
    char family[STRSHORT];
    char weight[STRSHORT];
    char slant[STRSHORT];
    char width[STRSHORT];
    char adstyl[STRSHORT];  // for future reference
    char pixels[STRSHORT];
    char points[STRSHORT];
    char horres[STRSHORT];
    char vertres[STRSHORT];
    char spacing[STRSHORT];
    char avgwidth[STRSHORT];
    char charset[STRSHORT];

    int parsed;

    void  MakeGeneric();
    int   SetItem(const char* word);

public:
    FontNameClass();
    FontNameClass(const char* fontname);

    void Clear();
    int  Parse(const char* fontname);
    const char* ToString();

    const char* Foundry() { return foundry; }
    const char* Family() { return family; }
    const char* Weight() { return weight; }
    const char* Slant() { return slant; }
    const char* Width() { return width; }
    const char* Pixels() { return pixels; }
    const char* Points() { return points; }
    const char* HorRes() { return horres; }
    const char* VertRes() { return vertres; }
    const char* Spacing() { return spacing; }
    const char* AvgWidth() { return avgwidth; }
    const char* CharSet() { return charset; }

    void ClearFoundry() { strcpy(foundry, "*"); }
    void ClearFamily() { strcpy(family, "*"); }
    void ClearWeight() { strcpy(weight, "*"); }
    void ClearSlant() { strcpy(slant, "*"); }
    void ClearWidth() { strcpy(width, "*"); }
    void ClearPixels() { strcpy(pixels, "*"); }
    void ClearPoints() { strcpy(points, "*"); }
    void ClearHorRes() { strcpy(horres, "*"); }
    void ClearVertRes() { strcpy(vertres, "*"); }
    void ClearSpacing() { strcpy(spacing, "*"); }
    void ClearAvgWidth() { strcpy(avgwidth, "*"); }
    void ClearCharSet() { strcpy(charset, "*"); }

    void SetFoundry(const char* set) { strcpy(foundry, set); }
    void SetFamily(const char* set) { strcpy(family, set); }
    void SetWeight(const char* set) { strcpy(weight, set); }
    void SetSlant(const char* set) { strcpy(slant, set); }
    void SetWidth(const char* set) { strcpy(width, set); }
    void SetPixels(const char* set) { strcpy(pixels, set); }
    void SetPoints(const char* set) { strcpy(points, set); }
    void SetHorRes(const char* set) { strcpy(horres, set); }
    void SetVertRes(const char* set) { strcpy(vertres, set); }
    void SetSpacing(const char* set) { strcpy(spacing, set); }
    void SetAvgWidth(const char* set) { strcpy(avgwidth, set); }
    void SetCharSet(const char* set) { strcpy(charset, set); }
};

FontNameClass::FontNameClass()
{
    FnTrace("FontNameClass::FontNameClass()");
    Clear();
}

FontNameClass::FontNameClass(const char* fontname)
{
    FnTrace("FontNameClass::FontNameClass(const char* )");
    parsed = Parse(fontname);
}

void FontNameClass::Clear()
{
    FnTrace("FontNameClass::Clear()");

    foundry[0] = '\0';
    family[0] = '\0';
    weight[0] = '\0';
    slant[0] = '\0';
    width[0] = '\0';
    adstyl[0] = '\0';
    pixels[0] = '\0';
    points[0] = '\0';
    horres[0] = '\0';
    vertres[0] = '\0';
    spacing[0] = '\0';
    avgwidth[0] = '\0';
    charset[0] = '\0';

    parsed = 0;
}

int FontNameClass::SetItem(const char* word)
{
    FnTrace("FontNameClass::SetItem()");
    int retval = 0;

    if (foundry[0] == '\0')
        strcpy(foundry, word);
    else if (family[0] == '\0')
        strcpy(family, word);
    else if (weight[0] == '\0')
        strcpy(weight, word);
    else if (slant[0] == '\0')
        strcpy(slant, word);
    else if (width[0] == '\0')
        strcpy(width, word);
    else if (pixels[0] == '\0')
        strcpy(pixels, word);
    else if (points[0] == '\0')
        strcpy(points, word);
    else if (horres[0] == '\0')
        strcpy(horres, word);
    else if (vertres[0] == '\0')
        strcpy(vertres, word);
    else if (spacing[0] == '\0')
        strcpy(spacing, word);
    else if (avgwidth[0] == '\0')
        strcpy(avgwidth, word);
    else if (charset[0] == '\0')
        strcpy(charset, word);
    else
    {
        strcat(charset, "-");
        strcat(charset, word);
    }
    
    return retval;
}

int FontNameClass::Parse(const char* fontname)
{
    FnTrace("FontNameClass::Parse()");
    int retval = 0;
    int len = strlen(fontname);
    int idx = 0;
    char word[STRLENGTH];
    int widx = 0;

    Clear();

    // require the fontname to start with a dash
    if (fontname[idx] != '-')
        return 1;

    idx += 1;  // skip past the first dash
    word[0] = '\0';
    widx = 0;
    while (idx < len)
    {
        if (fontname[idx] == '-' || fontname[idx] == '\0')
        {
            word[widx] = '\0';
            SetItem(word);
            word[0] = '\0';
            widx = 0;
        }
        else
        {
            word[widx] = fontname[idx];
            widx += 1;
        }
        idx += 1;
    }
    if (word[0] != '\0')
    {
        word[widx] = '\0';
        SetItem(word);
    }

    if (idx == len)
        parsed = 1;

    return retval;
}

void FontNameClass::MakeGeneric()
{
    FnTrace("FontNameClass::MakeGeneric()");

    strcpy(foundry, "*");
    strcpy(family, "*");
    strcpy(weight, "*");
    strcpy(slant, "*");
    strcpy(width, "*");
    strcpy(pixels, "*");
    strcpy(points, "*");
    strcpy(horres, "*");
    strcpy(vertres, "*");
    strcpy(spacing, "*");
    strcpy(avgwidth, "*");
    strcpy(charset, "*");

    parsed = 1;  // close enough
}

const char* FontNameClass::ToString()
{
    FnTrace("FontNameClass::ToString()");
    static char namestring[STRLONG];

    if (foundry[0] == '\0')
        MakeGeneric();

    snprintf(namestring, STRLONG, "-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s-%s",
             foundry, family, weight, slant, width, adstyl, pixels,
             points, horres, vertres, spacing, avgwidth, charset);

    return namestring;
}


/*--------------------------------------------------------------------
 * Xpm Class for screensaver images.
 *------------------------------------------------------------------*/
Xpm::Xpm()
{
    next = NULL;
    fore = NULL;
    width = 0;
    height = 0;
}

Xpm::Xpm(Pixmap pm)
{
    next = NULL;
    fore = NULL;
    width = 0;
    height = 0;
    pixmap = pm;
}

Xpm::Xpm(Pixmap pm, int w, int h)
{
    next = NULL;
    fore = NULL;
    width = w;
    height = h;
    pixmap = pm;
}

/*--------------------------------------------------------------------
 * Pixmaps Container Class for screensaver images.
 *------------------------------------------------------------------*/
Pixmaps::Pixmaps()
{
    count = 0;
}

int Pixmaps::Add(Xpm *pixmap)
{
    pixmaps.AddToTail(pixmap);
    count += 1;
    
    return 0;
}

Xpm *Pixmaps::Get(int idx)
{
    int curridx = 0;
    Xpm *currXpm = pixmaps.Head();
    Xpm *retval = NULL;
    
    if (pixmaps.Count() < 1)
        return retval;

    while (currXpm != NULL && curridx < count)
    {
        if (curridx == idx)
        {
            retval = currXpm;
            currXpm = NULL;
        }
        else
        {
            currXpm = currXpm->next;
            curridx += 1;
        }
    }
    
    return retval;
}

Xpm *Pixmaps::GetRandom()
{
    Xpm *retval = NULL;

    if (pixmaps.Count() < 2)
        return retval;

    int j = rand() % count;
    retval = Get(j);

    return retval;
}

#define SCREENSAVER_DIR  VIEWTOUCH_PATH "/dat/screensaver"
#define MAX_XPM_SIZE     4194304
Pixmaps PixmapList;


/*********************************************************************
 * Globals
 ********************************************************************/

LayerList Layers;
Layer *MainLayer = NULL;

int SocketNo = 0;

Display *Dis = NULL;
GC       Gfx = 0;
Window   MainWin;
std::array<Pixmap, IMAGE_COUNT> Texture;
Pixmap   ShadowPix;
int      ScrDepth = 0;
Visual  *ScrVis = NULL;
Colormap ScrCol = 0;
int      WinWidth  = 0;
int      WinHeight = 0;
int      IsTermLocal = 0;
int      Connection = 0;

XFontStruct *FontInfo[FONT_SPACE];
XftFont *XftFontsArr[FONT_SPACE];
int FontBaseline[FONT_SPACE];
int FontHeight[FONT_SPACE];

int ColorTextT[TEXT_COLORS];
int ColorTextH[TEXT_COLORS];
int ColorTextS[TEXT_COLORS];
int ColorBE;    // Bottom edge
int ColorLE;    // Left edge
int ColorRE;    // Right edge
int ColorTE;    // Top edge
int ColorLBE;   // Lit bottom edge
int ColorLLE;   // Lit left edge
int ColorLRE;   // Lit right edge
int ColorLTE;   // Lit top edge
int ColorDBE;   // Dark bottom edge
int ColorDLE;   // Dark left edge
int ColorDRE;   // Dark right edge
int ColorDTE;   // Dark top edge
int ColorBlack;
int ColorWhite;

Str TimeString;
Str TermStoreName;
Str Message;

static XtAppContext App;
static Widget       MainShell = NULL;
static int          ScrNo     = 0;
static Screen      *ScrPtr    = NULL;
static int          ScrHeight = 0;
static int          ScrWidth  = 0;
static Window       RootWin;
static int          Colors    = 0;
static int          MaxColors = 0;
static Ulong        Palette[256];
static int          ScreenBlankTime = 60;
static int          UpdateTimerID = 0;
static int          TouchInputID  = 0;
static TouchScreen *TScreen = NULL;
static int          ResetTime = 20;
static TimeInfo     TimeOut, LastInput;
static int          CalibrateStage = 0;
static int          SocketInputID = 0;
static Cursor       CursorPointer = 0;
static Cursor       CursorBlank = 0;
static Cursor       CursorWait = 0;

#ifndef NO_MOTIF
static PageDialog      *PDialog = NULL;
static ZoneDialog      *ZDialog = NULL;
static MultiZoneDialog *MDialog = NULL;
static TranslateDialog *TDialog = NULL;
static ListDialog      *LDialog = NULL;
static DefaultDialog   *DDialog = NULL;
#endif

// So that translations aren't done every time a dialog is opened, we'll
// only do translations when TERM_TRANSLATIONS is received.
short new_page_translations;
short new_zone_translations;

// Track mouse movements so that we can distinguish between mouse
// clicks and touches.  XFree86 sends a touch as a left mouse click.
// It always sends one motion event prior to the click.  It's
// unlikely that a real mouse click will follow only one real mouse
// motion, so we'll assume that clicks following moves != 1 are
// mouse clicks while clicks following moves == 1 are touches.
// There's also a situation where the user moves the mouse quite a
// bit and then touches without clicking.  For that we reset
// moves_count if too much time has passed between mouse movements,
// but only if there is also too much distance between the two
// points.
int moves_count = 0;
struct timeval last_mouse_time;
int last_x_pos = 0;
int last_y_pos = 0;

CCard *creditcard = NULL;
int ConnectionTimeOut = 30;

int allow_iconify = 1;
int use_embossed_text = 0;  // Default to disabled; users can enable in Settings
int use_text_antialiasing = 1;  // Default to enabled for better text quality
int use_drop_shadows = 0;  // Default to disabled (can be enabled per preference)
int shadow_offset_x = 2;  // Default shadow offset
int shadow_offset_y = 2;  // Default shadow offset
int shadow_blur_radius = 1;  // Default blur radius
int silent_mode   = 0;


/*********************************************************************
 * Socket Communication Functions
 ********************************************************************/

CharQueue BufferOut(QUEUE_SIZE);  // 1 meg buffers
CharQueue BufferIn(QUEUE_SIZE);

int  SendNow() { return BufferOut.Write(SocketNo); }
int  WInt8(int val)  { return BufferOut.Put8(val); }
int  RInt8()         { return BufferIn.Get8(); }
int  WInt16(int val) { return BufferOut.Put16(val); }
int  RInt16()        { return BufferIn.Get16(); }
int  WInt32(int val) { return BufferOut.Put32(val); }
int  RInt32()        { return BufferIn.Get32(); }
long WLong(long val) { return BufferOut.PutLong(val); }
long RLong()         { return BufferIn.GetLong(); }
long long WLLong(long long val) { return BufferOut.PutLLong(val); }
long long RLLong()   { return BufferIn.GetLLong(); }
int  WFlt(Flt val)   { return BufferOut.Put32((int) (val * 100.0)); }
Flt  RFlt()          { return (Flt) BufferIn.Get32() / 100.0; }

int WStr(const char* s, int len)
{
    FnTrace("WStr()");
    return BufferOut.PutString(s, len);
}

genericChar* RStr(genericChar* s)
{
    FnTrace("RStr()");

    static genericChar buffer[1024] = "";
    if (s == NULL)
        s = buffer;
    BufferIn.GetString(s);
    return s;
}

int ReportError(const std::string &message)
{
    FnTrace("ReportError()");
    if (SocketNo)
    {
        WInt8(SERVER_ERROR);
        WStr(message.c_str(), 0);
        return SendNow();
    }
    return 0;
}


/*********************************************************************
 * Translation Class
 ********************************************************************/

Translation::Translation()
{
    FnTrace("Translation::Translation()");

    next = NULL;
    fore = NULL;
    key[0] = '\0';
    value[0] = '\0';
}

Translation::Translation(const char* new_key, const char* new_value)
{
    FnTrace("Translation::Translation()");

    next = NULL;
    fore = NULL;
    key[0] = '\0';
    strncpy(key, new_key, STRLONG);
    value[0] = '\0';
    strncpy(value, new_value, STRLONG);
}

int Translation::Match(const char* check_key)
{
    FnTrace("Translation::Match()");

    int retval = 0;

    if (strncmp(key, check_key, STRLONG) == 0)
        retval = 1;

    return retval;
}

int Translation::GetKey(char* store, int maxlen)
{
    FnTrace("Translation::GetKey()");

    int retval = 1;
    strncpy(store, key, maxlen);
    return retval;
}

int Translation::GetValue(char* store, int maxlen)
{
    FnTrace("Translation::GetValue()");

    int retval = 1;
    strncpy(store, value, maxlen);
    return retval;
}

/*********************************************************************
 * Translations Class
 ********************************************************************/

Translations::Translations()
{
    FnTrace("Translations::Translations()");
}

int Translations::AddTranslation(const char* key, const char* value)
{
    FnTrace("Translations::AddTranslation()");

    int retval = 0;
    Translation *trans = new Translation(key, value);
    trans_list.AddToTail(trans);
    
    return retval;
}

const char* Translations::GetTranslation(const char* key)
{
    FnTrace("Translations::GetTranslation()");

    Translation *trans = trans_list.Head();
    while (trans != NULL)
    {
        if (trans->Match(key))
        {
            static char buffer[STRLONG];
            trans->GetValue(buffer, STRLONG);
            return buffer;
        }
        trans = trans->next;
    }
    return key;
}

void Translations::PrintTranslations()
{
    FnTrace("Translations::PrintTranslations()");

    Translation *trans = trans_list.Head();
    char key[STRLONG];
    char value[STRLONG];

    while (trans != NULL)
    {
        trans->GetKey(key, STRLONG);
        trans->GetValue(value, STRLONG);
        printf("%s = %s\n", key, value);
        trans = trans->next;
    }
}

Translations MasterTranslations;


/*********************************************************************
 * IconifyButton Class
 ********************************************************************/

#define EXTRA_ICON_WIDTH 35
class IconifyButton : public LO_PushButton
{
public:
    // Constructor
    IconifyButton(const char* str, int c1, int c2) : LO_PushButton(str, c1, c2) { }

    /****
     * IsPointIn:  The problem is that the size of the iconize icon is very
     *  very small.  This works fine for mouse usage, but not so well if
     *  you're trying to steer your users toward touches.  So we'll use
     *  IsPointIn() to increase the size of the click area without
     *  increasing the size of the visible area.  In other words, we
     *  add in a fudge factor to accomodate large fingers.
     *NOTE:  This code assumes the iconize button is in the top right
     *corner.  If you move the button, you'll need to modify this
     *code accordingly.  In that case, you should also make the code
     *more dynamic, though it should still be quite fast.
     ****/
    int IsPointIn(int px, int py)
        {
            return (px >= (x - EXTRA_ICON_WIDTH)) &&
                (py >= y) &&
                (px < (x + w)) &&
                (py < (y + h + EXTRA_ICON_WIDTH));
        }

    int Command(Layer *l)
        {
            if (allow_iconify)
            {
                ReportError("Minimizing...\n");
                XIconifyWindow(Dis, MainWin, ScrNo);
            }
            return 0;
        }
    int Render(Layer *l)
        {
            if (allow_iconify)
                return LO_PushButton::Render(l);
            else
                return 0;
        }
};


/*********************************************************************
 * Prototypes
 ********************************************************************/

int UserInput();
int BlankScreen();
int Calibrate(int status);
int EndCalibrate();
int StartTimers();
int StopTouches();
int StopUpdates();
int ResetView();
int SaveToPPM();
int OpenLayer(int id, int x, int y, int w, int h, int win_frame, const genericChar* title);
int ShowLayer(int id);
int KillLayer(int id);
int SetTargetLayer(int id);
int NewPushButton(int id, int x, int y, int w, int h,
                  const genericChar* text, int font, int c1, int c2);
int NewTextEntry(int id, int x, int y, int w, int h,
                 const genericChar* text, int font, int c1, int c2);
int NewItemList(int id, int x, int y, int w, int h,
                const genericChar* text, int font, int c1, int c2);
int NewItemMenu(int id, int x, int y, int w, int h,
                const genericChar* text, int font, int c1, int c2);
XFontStruct *GetFont(Display *display, const char* displayname, const char* fontname);
XFontStruct *GetAlternateFont(Display *display, const char* displayname, const char* fontname);


/*********************************************************************
 * Inline Functions
 ********************************************************************/

inline int SetTitleBar(const char* my_time)
{
    FnTrace("SetTitleBar()");

    if (my_time && strlen(my_time) > 0)
        TimeString.Set(my_time);
    return 0;
}


/*********************************************************************
 * Callback Functions
 ********************************************************************/

void ExposeCB(Widget widget, XtPointer client_data, XEvent *event,
              Boolean *okay)
{
    FnTrace("ExposeCB()");

    static RegionInfo area;

    XExposeEvent *e = (XExposeEvent *) event;
    if (CalibrateStage)
        return;

    area.Fit(e->x, e->y, e->width, e->height);
    if (e->count <= 0)
    {
        if (area.w > 0 && area.h > 0)
        {
            Layers.UpdateArea(area.x, area.y, area.w, area.h);
            XFlush(Dis);
        }
        area.SetRegion(0, 0, 0, 0);
    }
    // FIX - should redraw calibrate screen properly
}

void UpdateCB(XtPointer client_data, XtIntervalId *timer_id)
{
    FnTrace("UpdateCB()");
    int update_time = UPDATE_TIME;

    SystemTime.Set();
    if (Layers.screen_blanked == 0)
    {
        // Blank screen if no user input for a while
        int sec = SecondsElapsed(SystemTime, LastInput);
        if (ScreenBlankTime > 0 && sec > ScreenBlankTime)
        {
            BlankScreen();
        }
    }

    if (TScreen)
    {
        // Reset TouchScreen every 'x' secs if no user input
        int sec = SecondsElapsed(SystemTime, TScreen->last_reset);
        if (sec > ResetTime)
        {
            EndCalibrate();
            TScreen->Reset();
        }
    }
    UpdateTimerID = XtAppAddTimeOut(App, update_time, (XtTimerCallbackProc) UpdateCB, NULL);
}

void TouchScreenCB(XtPointer client_data, int *fid, XtInputId *id)
{
    FnTrace("TouchScreenCB()");

    TouchScreen *ts = TScreen;
    if (ts == NULL && silent_mode > 0)
        return;

    int tx = -1;
    int ty = -1;
    int mode = 0;
    int status = 0;

    status = ts->ReadTouch(tx, ty, mode);
    if (status == 1 && mode == TOUCH_DOWN && UserInput() == 0)
    {
        int x = (tx * ScrWidth)  / ts->x_res;
        int y = ((ts->y_res - 1 - ty) * ScrHeight) / ts->y_res;
        if (IsTermLocal)
        {
            // XTranslateCoordinates() is a bit slow - only used for local terminal
            Window w;
            int new_x, new_y;
            XTranslateCoordinates(Dis, 
                                  RootWin, 
                                  MainWin, 
                                  x, y, 
                                  &new_x, &new_y, &w);
            Layers.Touch(new_x, new_y);
        }
        else
            Layers.Touch(x, y);
    }
}

/****
 * ChangeKey:  New keyboards have new keys.  For now, we're not going
 *  to alter the keys much, just translate them to what they would be
 *  without the "F Lock" key enabled.  For example, on this Microsoft
 *  keyboard I have now, F1 without F Lock is Help.  ChangeKey() will
 *  set it up so that the user may enter Edit Mode with F Lock enabled
 *  or not.
 *
 *  In the future, we may want to use these additional keys, at which
 *  point they can be removed from this function.
 ****/
KeySym ChangeKey(KeySym key, unsigned int keycode)
{
    FnTrace("ChangeKey()");
    KeySym retval = key;

    switch (keycode)
    {
    case 187:  retval = XK_F1;  break;
    case 136:  retval = XK_F2;  break;
    case 135:  retval = XK_F3;  break;
    case 119:  retval = XK_F4;  break;
    case 120:  retval = XK_F5;  break;
    case 121:  retval = XK_F6;  break;
    case 122:  retval = XK_F7;  break;
    case 194:  retval = XK_F8;  break;
    case 195:  retval = XK_F9;  break;
    case 163:  retval = XK_F10; break;
    case 215:  retval = XK_F11; break;
    case 216:  retval = XK_F12; break;
    }

    return retval;
}

void KeyPressCB(Widget widget, XtPointer client_data,
                XEvent *event, Boolean *okay)
{
    FnTrace("KeyPressCB()");

    static char swipe_buffer[1024];
    static char last_char    = '\0';
    static int  swipe_char   = 0;
    static int  swipe_stage  = 0;
    static int  swipe_time   = 0;
    static int  swipe_track2 = 0;
    static int  fake_cc      = 0;
    if (UserInput())
        return;

    XKeyEvent *e = (XKeyEvent *) event;
    KeySym key = 0;
    genericChar buffer[32];

    int len = XLookupString(e, buffer, 31, &key, NULL);
    if (len < 0)
        len = 0;
    buffer[len] = '\0';
    key = ChangeKey(key, e->keycode);

    if (silent_mode > 0 && key != XK_F12)
        return;


    switch (key)
	{
    case XK_Print:
        if (e->state & ControlMask)
            SaveToPPM();
        return;

    case XK_Escape:
        if (EndCalibrate() == 0)
            return;
        break;
    case XK_KP_Enter:
    case XK_End:
        if (e->state & ControlMask && e->state & Mod1Mask)
        {
            WInt8(SERVER_SHUTDOWN);
            SendNow();
        }
        break;
    case XK_F12:
        if (e->state & ControlMask)
            silent_mode = !silent_mode;
        break;
#ifdef USE_TOUCHSCREEN
    case XK_F11:
        Calibrate(0);
        return;
    case XK_F10:
        TScreen->SetMode("POINT");
        return;
#endif
	}

    // Prevent extra post-swipe CRs.
    if (last_char == 13 && buffer[0] == 13)
    {
        ReportError("Got an extra carriage return post card swipe...");
        return;
    }
    else if (swipe_stage == 0)
        last_char = '\0';

    // Check For Card Swipe
    int clock_time = clock() / CLOCKS_PER_SEC;
    int dif = clock_time - swipe_time;
    if (debug_mode && (dif > 10000))
        swipe_stage = 0;
    else if (dif > 1000)
        swipe_stage = 0;  // FIX - timeout only sort of works
    swipe_time = clock_time;

    switch (swipe_stage)
    {
    case 0:
        // stage 0:  We're just looking for the initial % symbol.
        if (buffer[0] == '%')
        {
            swipe_char = 0;
            swipe_buffer[swipe_char] = buffer[0];
            swipe_char += 1;
            swipe_stage = 1;
        }
        break;
    case 1:
        // stage 1:  Now we're looking for either b or B. Anything
        // else stops the whole show.
        if (buffer[0] == 'b' || buffer[0] == 'B')
        {
            swipe_buffer[swipe_char] = buffer[0];
            swipe_char += 1;
            swipe_stage = 2;
            return;
        }
        else if (buffer[0] != '\0')
        {
            swipe_stage = 0;
            swipe_char = 0;
            swipe_track2 = 0;
        }
        break;
    case 2:
        if (buffer[0] == 13)
        {
            if (last_char == 13)
            {
                // Need to skip multiple CRs.  I'm not sure why they
                // happen.  It appears to be caused either by a bad
                // card or a bad card reader.  Whatever the case, it
                // can cause problems further on.
                ReportError("Got an extra carriage return in card swipe...");
            }
            else if (swipe_track2)
            {
                swipe_buffer[swipe_char] = '\0';
                swipe_stage = 0;
                swipe_char = 0;
                swipe_track2 = 0;
                WInt8(SERVER_SWIPE);
                WStr(swipe_buffer);
                SendNow();
            }
            else
            {
                swipe_track2 = 1;
            }
        }
        else if (buffer[0] != '\0' && swipe_char < 1023)
        {
            swipe_buffer[swipe_char++] = buffer[0];
        }
        last_char = buffer[0];
        return;
    }

    if (debug_mode && buffer[0] == 'c')
    {
        // if the user presses 'c' three times, in debug mode, let's send some
        // credit card data as if we'd received a swipe.  Pseudo-randomly
        // determine whether to send good or garbage data.
        fake_cc += 1;
        if (fake_cc >= 3)
        {
            int randcc = (int)(10.0 * rand() / (RAND_MAX + 1.0));
            swipe_buffer[0] = '\0';
            if (randcc == 0)
            {
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS^;??");
            }
            else if (randcc == 1 || randcc == 3 || randcc == 5)
            {  // correct data, tracks 1 and 2
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS");
                strcat(swipe_buffer, "^08051011234567890131674486261606288842611?");
                strcat(swipe_buffer, ";5186900000000121=");
                strcat(swipe_buffer, "08051015877400050041?");
            }
            else if (randcc == 2)
            {
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS");
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS");
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS");
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS");
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS");
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS");
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS");
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS");
                strcat(swipe_buffer, "^08051011234567890131674486261606288842611?");
            }
            else if (randcc == 4)
            {
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS");
                strcat(swipe_buffer, "^08051011234567890131674486261606288842611?");
                strcat(swipe_buffer, ";5186900000000121=");
                strcat(swipe_buffer, "08051015877400050041?");
            }
            else if (randcc == 6)
            {
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS");
                strcat(swipe_buffer, "08051015877400050041?");
            }
            else if (randcc == 7)
            {
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS");
                strcat(swipe_buffer, "^08051011234567890131674486261606288842611?");
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS");
                strcat(swipe_buffer, "^08051011234567890131674486261606288842611?");
            }
            else if (randcc == 8)
            {
                strcat(swipe_buffer, "%B5186900000000121^TEST CARD/MONERIS");
            }
            else if (randcc == 9)
            {
                strcat(swipe_buffer, "%B\n\n");
            }
            fake_cc = 0;
            printf("Sending Fake Credit Card:  '%s'\n", swipe_buffer);
            WInt8(SERVER_SWIPE);
            WStr(swipe_buffer);
            SendNow();
        }
    }

    // Convert special keys to control characters
    switch (key)
    {
    case XK_Delete:    buffer[0] =  8; len = 1; break;  // backspace
    case XK_Page_Up:   buffer[0] = 16; len = 1; break;  // ^P
    case XK_Page_Down: buffer[0] = 14; len = 1; break;  // ^N
    case XK_Up:        buffer[0] = 21; len = 1; break;  // ^U
    case XK_Down:      buffer[0] =  4; len = 1; break;  // ^D
    case XK_Left:      buffer[0] = 12; len = 1; break;  // ^L
    case XK_Right:     buffer[0] = 17; len = 1; break;  // ^R
    }

    genericChar k;
    if (len <= 0)
        k = 0;
    else
        k = buffer[0];
    Layers.Keyboard(k, key, (int)e->state);
}

void MouseClickCB(Widget widget, XtPointer client_data, XEvent *event,
                  Boolean *okay)
{
    FnTrace("MouseClickCB()");

    if (CalibrateStage)
        return;
    if (UserInput())
        return;
    if (silent_mode > 0)
        return;

    XButtonEvent *btnevent = (XButtonEvent *) event;
    int code = MOUSE_PRESS;
    int touch = 0;

    switch (btnevent->button)
    {
    case Button1:
        code |= MOUSE_LEFT;
        if (moves_count == 1)
            touch = 1;
        break;
    case Button2:
        code |= MOUSE_MIDDLE;
        break;
    case Button3:
        code |= MOUSE_RIGHT;
        break;
    }
    if (btnevent->state & ShiftMask)
        code |= MOUSE_SHIFT;

    moves_count = 0;
    if (touch)
        Layers.Touch(btnevent->x, btnevent->y);
    else
        Layers.MouseAction(btnevent->x, btnevent->y, code);
}

void MouseReleaseCB(Widget widget, XtPointer client_data, XEvent *event,
                    Boolean *okay)
{
    FnTrace("MouseReleaseCB()");

    if (UserInput())
        return;
    if (silent_mode > 0)
        return;

    XButtonEvent *b = (XButtonEvent *) event;
    Layers.RubberBandOff();

    int code = MOUSE_RELEASE;
    switch (b->button)
    {
    case Button1: code |= MOUSE_LEFT;   break;
    case Button2: code |= MOUSE_MIDDLE; break;
    case Button3: code |= MOUSE_RIGHT;  break;
    }
    if (b->state & ShiftMask)
        code |= MOUSE_SHIFT;

    Layers.MouseAction(b->x, b->y, code);
}

void MouseMoveCB(Widget widget, XtPointer client_data, XEvent *event,
                 Boolean *okay)
{
    FnTrace("MouseMoveCB()");

    XPointerMovedEvent *e = (XPointerMovedEvent *) event;
    if (UserInput())
        return;
    if (silent_mode > 0)
        return;
    struct timeval now;

    // try to intelligently determine whether this might be
    // a touch
    gettimeofday(&now, NULL);
    if ((now.tv_sec - last_mouse_time.tv_sec) > 1 ||
        (now.tv_usec - last_mouse_time.tv_usec) > 100000)
    {
        int x_diff = (e->x > last_x_pos) ? (e->x - last_x_pos) : (last_x_pos - e->x);
        int y_diff = (e->y > last_y_pos) ? (e->y - last_y_pos) : (last_y_pos - e->y);
        if (x_diff > 5 || y_diff > 5)
            moves_count = 0;
    }

    int code = 0;
    if (e->state & Button1Mask)
        code |= MOUSE_LEFT | MOUSE_DRAG;
    if (e->state & Button2Mask)
        code |= MOUSE_MIDDLE | MOUSE_DRAG;
    if (e->state & Button3Mask)
        code |= MOUSE_RIGHT | MOUSE_DRAG;
    if (code && (e->state & ShiftMask))
        code |= MOUSE_SHIFT | MOUSE_DRAG;

    moves_count += 1;
    last_x_pos = e->x;
    last_y_pos = e->y;
    last_mouse_time.tv_sec = now.tv_sec;
    last_mouse_time.tv_usec = now.tv_usec;

    Layers.MouseAction(e->x, e->y, code);
}

void CalibrateCB(XtPointer client_data, int *fid, XtInputId *id)
{
    FnTrace("CalibrateCB()");

    int status = TScreen->ReadStatus();
    if (status >= 0)
        Calibrate(status);
}

void SocketInputCB(XtPointer client_data, int *fid, XtInputId *id)
{
    FnTrace("SocketInputCB()");

    static int failure = 0;
    int val = BufferIn.Read(SocketNo);
    if (val <= 0)
    {
        ++failure;
        if (failure < 8)
            return;

        // Server must be dead - go ahead and quit
        exit(1);
    }

    Layer *l = MainLayer;
    int px = l->page_x;
    int py = l->page_y;
    int offset_x = l->x + px;
    int offset_y = l->y + py;

    int n1, n2, n3, n4, n5, n6, n7, n8;
    const genericChar* s1, *s2;

    failure = 0;
    genericChar s[STRLONG];
    genericChar key[STRLENGTH];
    genericChar value[STRLENGTH];

    while (BufferIn.size > 0)
    {
        int code = RInt8();
        BufferIn.SetCode("vt_term", code);
        switch (code)
        {
        case TERM_FLUSH:
            ResetView();
            break;
        case TERM_UPDATEALL:
            l->buttons.Render(l);
            if (CalibrateStage == 0)
            {
                if (l->use_clip)
                {
                    Layers.UpdateArea(offset_x + l->clip.x, offset_y + l->clip.y,
                                      l->clip.w, l->clip.h);
                }
                else
                {
                    l->update = 1;
                    Layers.UpdateAll(0);
                }
                XFlush(Dis);
            }
            l->ClearClip();
            break;
        case TERM_UPDATEAREA:
            l->buttons.Render(l);
            if (CalibrateStage == 0)
            {
                // FIX - should clip area given
                n1 = RInt16();
                n2 = RInt16();
                n3 = RInt16();
                n4 = RInt16();
                Layers.UpdateArea(offset_x + n1, offset_y + n2, n3, n4);
                XFlush(Dis);
            }
            l->ClearClip();
            break;
        case TERM_BLANKPAGE:
            n1 = RInt8();
            n2 = RInt8();
            n3 = RInt8();
            n4 = RInt8();
            n5 = RInt16();
            n6 = RInt8();
            s1 = RStr(s);
            s2 = RStr();
            if (TScreen)
                TScreen->Flush();
            l->BlankPage(n1, n2, n3, n4, n5, n6, s1, s2);
            break;
        case TERM_BACKGROUND:
            if (l->use_clip)
                l->Background(l->clip.x, l->clip.y, l->clip.w, l->clip.h);
            else
                l->Background(0, 0, l->page_w, l->page_h);
            break;
        case TERM_TITLEBAR:
            SetTitleBar(RStr());
            break;
        case TERM_TEXTL:
            RStr(s);
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt8();
            n4 = RInt8();
            n5 = RInt16();
            l->Text(s, strlen(s), n1, n2, n3, n4, ALIGN_LEFT, n5, use_embossed_text);
            break;
        case TERM_TEXTC:
            RStr(s);
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt8();
            n4 = RInt8();
            n5 = RInt16();
            l->Text(s, strlen(s), n1, n2, n3, n4, ALIGN_CENTER, n5, use_embossed_text);
            break;
        case TERM_TEXTR:
            RStr(s);
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt8();
            n4 = RInt8();
            n5 = RInt16();
            l->Text(s, strlen(s), n1, n2, n3, n4, ALIGN_RIGHT, n5, use_embossed_text);
            break;
        case TERM_ZONETEXTL:
            RStr(s);
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt16();
            n5 = RInt8();
            n6 = RInt8();
            l->ZoneText(s, n1, n2, n3, n4, n5, n6, ALIGN_LEFT, use_embossed_text);
            break;
        case TERM_ZONETEXTC:
            RStr(s);
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt16();
            n5 = RInt8();
            n6 = RInt8();
            l->ZoneText(s, n1, n2, n3, n4, n5, n6, ALIGN_CENTER, use_embossed_text);
            break;
        case TERM_ZONETEXTR:
            RStr(s);
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt16();
            n5 = RInt8();
            n6 = RInt8();
            l->ZoneText(s, n1, n2, n3, n4, n5, n6, ALIGN_RIGHT, use_embossed_text);
            break;
        case TERM_ZONE:
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt16();
            n5 = RInt8();
            n6 = RInt8();
            n7 = RInt8();
            l->Zone(n1, n2, n3, n4, n5, n6, n7);
            break;
        case TERM_EDITCURSOR:
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt16();
            l->EditCursor(n1, n2, n3, n4);
            break;
        case TERM_SHADOW:
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt16();
            n5 = RInt8();
            n6 = RInt8();
            l->Shadow(n1, n2, n3, n4, n5, n6);
            break;
        case TERM_RECTANGLE:
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt16();
            n5 = RInt8();
            l->Rectangle(n1, n2, n3, n4, n5);
            break;
        case TERM_SOLID_RECTANGLE:
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt16();
            n5 = RInt16();
            l->SolidRectangle(n1, n2, n3, n4, n5);
            break;
        case TERM_HLINE:
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt8();
            n5 = RInt8();
            l->HLine(n1, n2, n3, n4, n5);
            break;
        case TERM_VLINE:
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt8();
            n5 = RInt8();
            l->VLine(n1, n2, n3, n4, n5);
            break;
        case TERM_FRAME:
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt16();
            n5 = RInt8();
            n6 = RInt8();
            l->Frame(n1, n2, n3, n4, n5, n6);
            break;
        case TERM_FILLEDFRAME:
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt16();
            n5 = RInt8();
            n6 = RInt8();
            n7 = RInt8();
            l->FilledFrame(n1, n2, n3, n4, n5, n6, n7);
            break;
        case TERM_STATUSBAR:
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt16();
            n5 = RInt8();
            RStr(s);
            n6 = RInt8();
            n7 = RInt8();
            l->StatusBar(n1, n2, n3, n4, n5, s, n6, n7);
            break;
        case TERM_FLUSH_TS:
            if (TScreen)
                TScreen->Flush();
            break;
        case TERM_CALIBRATE_TS:
            Calibrate(0); break;
        case TERM_USERINPUT:
            UserInput(); break;
        case TERM_BLANKSCREEN:
            BlankScreen(); break;
        case TERM_SETMESSAGE:
            Message.Set(RStr()); break;
        case TERM_CLEARMESSAGE:
            Message.Clear(); break;
        case TERM_BLANKTIME:
            ScreenBlankTime = RInt16();
            UserInput();
            break;
        case TERM_STORENAME:
            TermStoreName.Set(RStr()); break;
        case TERM_CONNTIMEOUT:
            ConnectionTimeOut = RInt16();
            break;
        case TERM_SELECTOFF:
            Layers.RubberBandOff(); break;
        case TERM_SELECTUPDATE:
            n1 = RInt16();
            n2 = RInt16();
            Layers.RubberBandUpdate(n1 + l->x + l->page_x, n2 + l->y + l->page_y);
            break;
        case TERM_EDITPAGE:
#ifndef NO_MOTIF
            if (MDialog)
                MDialog->Close();
            if (ZDialog)
                ZDialog->Close();
            if (DDialog)
                DDialog->Close();
            if (PDialog)
                PDialog->Open();
#endif
            break;
        case TERM_EDITZONE:
#ifndef NO_MOTIF
            if (PDialog)
                PDialog->Close();
            if (MDialog)
                MDialog->Close();
            if (DDialog)
                DDialog->Close();
            if (ZDialog)
                ZDialog->Open();
#endif
            break;
        case TERM_EDITMULTIZONE:
#ifndef NO_MOTIF
            if (PDialog)
                PDialog->Close();
            if (ZDialog)
                ZDialog->Close();
            if (DDialog)
                DDialog->Close();
            if (MDialog)
                MDialog->Open();
#endif
            break;
        case TERM_DEFPAGE:
#ifndef NO_MOTIF
            if (PDialog)
                PDialog->Close();
            if (ZDialog)
                ZDialog->Close();
            if (MDialog)
                MDialog->Close();
            if (DDialog)
                DDialog->Open();
#endif
            break;
        case TERM_TRANSLATE:
#ifndef NO_MOTIF
            if (TDialog)
                TDialog->Open();
#endif
            break;
        case TERM_LISTSTART:
#ifndef NO_MOTIF
            if (LDialog)
                LDialog->Start();
#endif
            break;
        case TERM_LISTITEM:
#ifndef NO_MOTIF
            if (LDialog)
                LDialog->ReadItem();
#endif
            break;
        case TERM_LISTEND:
#ifndef NO_MOTIF
            if (LDialog)
                LDialog->End();
#endif
            break;
        case TERM_SETCLIP:
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt16();
            l->SetClip(n1, n2, n3, n4);
            break;
        case TERM_CURSOR:
            Layers.SetCursor(l, RInt16());
            break;
        case TERM_DIE:
            KillTerm();
            exit(0);
            break;
        case TERM_NEWWINDOW:
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt16();
            n5 = RInt16();
            n6 = RInt8();
            RStr(s);
            OpenLayer(n1, n2, n3, n4, n5, n6, s);
            break;
        case TERM_SHOWWINDOW:
            ShowLayer(RInt16());
            break;
        case TERM_KILLWINDOW:
            KillLayer(RInt16());
            break;
        case TERM_TARGETWINDOW:
            SetTargetLayer(RInt16());
            break;
        case TERM_PUSHBUTTON:
            n1 = RInt16();
            n2 = RInt16();
            n3 = RInt16();
            n4 = RInt16();
            n5 = RInt16();
            RStr(s);
            n6 = RInt8();
            n7 = RInt8();
            n8 = RInt8();
            NewPushButton(n1, n2, n3, n4, n5, s, n6, n7, n8);
            break;
        case TERM_ICONIFY:
            ResetView();
            XIconifyWindow(Dis, MainWin, ScrNo);
            break;
        case TERM_BELL:
            XBell(Dis, RInt16());
            break;
        case TERM_TRANSLATIONS:
            MasterTranslations.Clear();
            n1 = RInt8();
            for (n2 = 0; n2 < n1; n2++)
            {
                RStr(key);
                RStr(value);
                MasterTranslations.AddTranslation(key, value);
            }
            new_page_translations = 1;
            new_zone_translations = 1;
            break;
        case TERM_CC_AUTH:
            if (creditcard == NULL)
                creditcard = new CCard;
            if (creditcard != NULL)
            {
                creditcard->Read();
                creditcard->Sale();
                WInt8(SERVER_CC_PROCESSED);
                creditcard->Write();
                SendNow();
                creditcard->Clear();
            }
            break;
        case TERM_CC_PREAUTH:
            if (creditcard == NULL)
                creditcard = new CCard;
            if (creditcard != NULL)
            {
                creditcard->Read();
                creditcard->PreAuth();
                WInt8(SERVER_CC_PROCESSED);
                creditcard->Write();
                SendNow();
                creditcard->Clear();
            }
            break;
        case TERM_CC_FINALAUTH:
            if (creditcard == NULL)
                creditcard = new CCard;
            if (creditcard != NULL)
            {
                creditcard->Read();
                creditcard->FinishAuth();
                WInt8(SERVER_CC_PROCESSED);
                creditcard->Write();
                SendNow();
                creditcard->Clear();
            }
            break;
        case TERM_CC_VOID:
            if (creditcard == NULL)
                creditcard = new CCard;
            if (creditcard != NULL)
            {
                creditcard->Read();
                creditcard->Void();
                WInt8(SERVER_CC_PROCESSED);
                creditcard->Write();
                SendNow();
                creditcard->Clear();
            }
            break;
        case TERM_CC_VOID_CANCEL:
            if (creditcard == NULL)
                creditcard = new CCard;
            if (creditcard != NULL)
            {
                creditcard->Read();
                creditcard->VoidCancel();
                WInt8(SERVER_CC_PROCESSED);
                creditcard->Write();
                SendNow();
                creditcard->Clear();
            }
            break;
        case TERM_CC_REFUND:
            if (creditcard == NULL)
                creditcard = new CCard;
            if (creditcard != NULL)
            {
                creditcard->Read();
                creditcard->Refund();
                WInt8(SERVER_CC_PROCESSED);
                creditcard->Write();
                SendNow();
                creditcard->Clear();
            }
            break;
        case TERM_CC_REFUND_CANCEL:
            if (creditcard == NULL)
                creditcard = new CCard;
            if (creditcard != NULL)
            {
                creditcard->Read();
                creditcard->RefundCancel();
                WInt8(SERVER_CC_PROCESSED);
                creditcard->Write();
                SendNow();
                creditcard->Clear();
            }
            break;
        case TERM_CC_SETTLE:
            // BatchSettle() should also write the response to vt_main
            if (creditcard == NULL)
                creditcard = new CCard;
            if (creditcard != NULL)
            {
                creditcard->BatchSettle();
                creditcard->Clear();
            }
            break;
        case TERM_CC_INIT:
            if (creditcard == NULL)
                creditcard = new CCard;
            if (creditcard != NULL)
            {
                creditcard->CCInit();
                creditcard->Clear();
            }
            break;
        case TERM_CC_TOTALS:
            if (creditcard == NULL)
                creditcard = new CCard;
            if (creditcard != NULL)
            {
                creditcard->Totals();
                creditcard->Clear();
            }
            break;
        case TERM_CC_DETAILS:
            if (creditcard == NULL)
                creditcard = new CCard;
            if (creditcard != NULL)
            {
                creditcard->Details();
                creditcard->Clear();
            }
            break;
        case TERM_CC_CLEARSAF:
            if (creditcard == NULL)
                creditcard = new CCard;
            if (creditcard != NULL)
            {
                creditcard->ClearSAF();
                creditcard->Clear();
            }
            break;
        case TERM_CC_SAFDETAILS:
            if (creditcard == NULL)
                creditcard = new CCard;
            if (creditcard != NULL)
            {
                creditcard->SAFDetails();
                creditcard->Clear();
            }
            break;
        case TERM_SET_ICONIFY:
            allow_iconify = RInt8();
            break;
        case TERM_SET_EMBOSSED:
            use_embossed_text = RInt8();
            break;
        case TERM_SET_ANTIALIAS:
            use_text_antialiasing = RInt8();
            break;
        case TERM_SET_DROP_SHADOW:
            use_drop_shadows = RInt8();
            break;
        case TERM_SET_SHADOW_OFFSET:
            shadow_offset_x = RInt16();
            shadow_offset_y = RInt16();
            break;
        case TERM_SET_SHADOW_BLUR:
            shadow_blur_radius = RInt8();
            break;
        case TERM_RELOAD_FONTS:
            TerminalReloadFonts();
            break;
        }
	}
}

/*********************************************************************
 * General Functions
 ********************************************************************/

Layer       *TargetLayer = NULL;
LayerObject *TargetObject = NULL;

int OpenLayer(int id, int x, int y, int w, int h, int win_frame, const genericChar* title)
{
    FnTrace("OpenLayer()");

    if (win_frame)
    {
        w += 14;
        h += 37;
    }

    KillLayer(id);
    Layer *l = new Layer(Dis, Gfx, MainWin, w, h);
    if (l == NULL)
        return 1;

    if (l->pix == 0)
    {
        delete l;
        return 1;
    }

    l->id = id;
    l->window_frame = win_frame;
    l->window_title.Set(title);
    l->x = x;
    l->y = y;
    if (win_frame)
    {
        l->offset_x = 7;
        l->offset_y = 30;
    }
    Layers.AddInactive(l);
    TargetLayer = l;
    return 0;
}

int ShowLayer(int id)
{
    FnTrace("ShowLayer()");

    Layer *l = Layers.FindByID(id);
    if (l == NULL)
        return 1;

    l->buttons.Render(l);
    Layers.Remove(l);
    Layers.Add(l);
    TargetLayer = l;
    return 0;
}

int KillLayer(int id)
{
    FnTrace("KillLayer()");

    Layer *l;
    do
    {
        l = Layers.FindByID(id);
        if (l)
        {
            Layers.Remove(l);
            delete l;
        }
    }
    while (l);
    return 0;
}

int SetTargetLayer(int id)
{
    FnTrace("SetTargetLayer()");

    Layer *l = Layers.FindByID(id);
    if (l == NULL)
        return 1;

    TargetLayer = l;
    return 0;
}

int NewPushButton(int id, int x, int y, int w, int h, const genericChar* text,
                  int font, int c1, int c2)
{
    FnTrace("NewPushButton()");

    Layer *l = TargetLayer;
    if (l == NULL)
        return 1;
    LO_PushButton *b = new LO_PushButton(text, c1, c2);
    b->SetRegion(x + l->offset_x, y + l->offset_y, w, h);
    b->font = font;
    b->id = id;
    l->buttons.Add(b);
    TargetObject = b;
    return 0;
}

int NewTextEntry(int id, int x, int y, int w, int h,
                 const genericChar* text, int font, int c1, int c2)
{
    FnTrace("NewTextEntry()");

    return 1;
}

int NewItemList(int id, int x, int y, int w, int h,
                const genericChar* text, int font, int c1, int c2)
{
    FnTrace("NewItemList()");

    return 1;
}

int NewItemMenu(int id, int x, int y, int w, int h,
                const genericChar* text, int font, int c1, int c2)
{
    FnTrace("NewItemMenu()");

    return 1;
}

XFontStruct *GetFont(Display *display, const char* displayname, const char* fontname)
{
    FnTrace("GetFont()");
    XFontStruct *retfont = NULL;
    char str[STRLONG];
    
    // First try to load as scalable font using Xft
    XftFont *xftfont = XftFontOpenName(Dis, ScrNo, fontname);
    if (xftfont != NULL)
    {
        // For now, we'll still need to return an XFontStruct for compatibility
        // We'll store the XftFont separately and use it for rendering
        retfont = XLoadQueryFont(Dis, "fixed"); // fallback font
        if (retfont == NULL)
        {
            // If even the fallback fails, try to get any available font
            retfont = XLoadQueryFont(Dis, "*");
        }
    }
    else
    {
        // Try traditional X11 font loading
        retfont = XLoadQueryFont(Dis, fontname);
        if (retfont == NULL)
        {
            snprintf(str, STRLENGTH, "Can't load font '%s' on display '%s'",
                     fontname, displayname);
            ReportError(str);
            retfont = GetAlternateFont(Dis, displayname, fontname);
        }
    }

    return retfont;
}

XFontStruct *GetAlternateFont(Display *display, const char* displayname, const char* fontname)
{
    FnTrace("GetAlternateFont()");
    XFontStruct *retfont = NULL;
    FontNameClass font;
    char str[STRLENGTH];

    ReportError("  Looking for alternative font...");
    font.Parse(fontname);

    // see if we have the same font in a different foundry
    font.ClearFoundry();
    retfont = XLoadQueryFont(Dis, font.ToString());
    if (retfont != NULL)
        goto done;

    // try switching families
    if (strcmp(font.Family(), "courier") == 0)
        font.SetFamily("fixed");
    XLoadQueryFont(Dis, font.ToString());
    if (retfont != NULL)
        goto done;

    font.ClearCharSet();
    retfont = XLoadQueryFont(Dis, font.ToString());
    if (retfont != NULL)
        goto done;

    font.ClearWeight();
    retfont = XLoadQueryFont(Dis, font.ToString());
    if (retfont != NULL)
        goto done;

    font.ClearPixels();
    retfont = XLoadQueryFont(Dis, font.ToString());
    
done:
    if (retfont == NULL)
        ReportError("  Unable to find alternative!!");
    else
    {
        snprintf(str, STRLENGTH, "  Got one:  %s", font.ToString());
        ReportError(str);
    }

    return retfont;
}

int ShowCursor(int type)
{
    FnTrace("ShowCursor()");

    Cursor c = 0;
    switch (type)
	{
    case CURSOR_POINTER:
        c = CursorPointer;
        break;
    case CURSOR_WAIT:
        c = CursorWait;
        break;
    case CURSOR_BLANK:
        c = CursorBlank;
        break;
    default:
        break;
	}

    if (c)
        XDefineCursor(Dis, MainWin, c);

    return 0;
}

int SaveToPPM()
{
    FnTrace("SaveToPPM()");

    genericChar filename[256];
    int no = 0;

    // Verify directory exists
    if (!DoesFileExist(SCREEN_DIR))
    {
        mkdir(SCREEN_DIR, 0);
        chmod(SCREEN_DIR, 0777);
    }
    
    // Find first unused filename (starting with 0)
    while (1)
    {
        sprintf(filename, "%s/vtscreen%d.wd", SCREEN_DIR, no++);
        if (!DoesFileExist(filename))
            break;
    }

    // Log the action
    genericChar str[256];
    sprintf(str, "Saving screen image to file '%s'", filename);
    ReportError(str);

    // Generate the screenshot
    char command[STRLONG];
    snprintf(command, STRLONG, "%s -root -display %s >%s",
             XWD, DisplayString(Dis), filename);
    system(command);

    return 0;
}

int ResetView()
{
    FnTrace("ResetView()");

    XMoveResizeWindow(Dis, MainWin, 0, 0, WinWidth, WinHeight);
    Layers.HideCursor();
    if (CalibrateStage == 0)
    {
        Layers.UpdateAll(1);
        XFlush(Dis);
    }
    return 0;
}

int AddColor(XColor &c)
{
    FnTrace("AddColor()");

    if (Colors >= MaxColors)
        return -1;

    ++Colors;
    if (ScrDepth <= 8)
    {
        c.pixel = Palette[Colors];
        XStoreColor(Dis, ScrCol, &c);
    }
    else
        XAllocColor(Dis, ScrCol, &c);
    return c.pixel;
}

int AddColor(int red, int green, int blue)
{
    FnTrace("AddColor()");

    int r = red   % 256;
    int g = green % 256;
    int b = blue  % 256;

    XColor c;
    c.flags = DoRed | DoGreen | DoBlue;
    c.red   = (r * 256) + r;
    c.green = (g * 256) + g;
    c.blue  = (b * 256) + b;
    return AddColor(c);
}

Pixmap LoadPixmap(const char**image_data)
{
    FnTrace("LoadPixmap()");
    
    Pixmap retxpm = 0;
    int status;
    
    status = XpmCreatePixmapFromData(Dis, MainWin, (char**)image_data, &retxpm, NULL, NULL);
    if (status != XpmSuccess)
        fprintf(stderr, "XpmError:  %s\n", XpmGetErrorString(status));
    
    return retxpm;
}

/****
 * LoadPixmapFile:  Use the XPM library to read a .xpm file into memory.  Set
 *   upper limits for the file: the file cannot be larger than MAX_XPM_SIZE and
 *   the image cannot be wider or taller than the screen.
 ****/
Xpm *LoadPixmapFile(char* file_name)
{
    FnTrace("LoadPixmapFile()");
    
    Xpm *retxpm = NULL;
    Pixmap xpm;
    XpmAttributes attributes;
    int status;
    struct stat sb;
    
    if (stat(file_name, &sb) == 0)
    {
        if (sb.st_size <= MAX_XPM_SIZE)
        {
            attributes.valuemask = 0;
            status = XpmReadFileToPixmap(Dis, MainWin, file_name, &xpm, NULL, &attributes);
            if (status != XpmSuccess)
            {
                fprintf(stderr, "XpmError %s for %s\n", XpmGetErrorString(status), file_name);
            }
            else if (attributes.width  <= (unsigned int)WinWidth &&
                     attributes.height <= (unsigned int)WinHeight)
            {
                retxpm = new Xpm(xpm, attributes.width, attributes.height);
            }
            else
            {
                printf("Image %s too large for screen, skipping\n", file_name);
            }
            XpmFreeAttributes(&attributes);
        }
        else
        {
            printf("Xpm file %s too large, skipping\n", file_name);
        }
    }

    return retxpm;
}

int ReadScreenSaverPix()
{
    FnTrace("ReadScreenSaverPix()");
    struct dirent *record = NULL;
    DIR *dp;
    Xpm *newpm;
    genericChar fullpath[STRLONG];
    int len;
    if (!fs::is_directory(SCREENSAVER_DIR))
    {
        std::cerr << "Screen saver directory does not exist: '"
            << SCREENSAVER_DIR << "' creating it" << std::endl;
        fs::create_directory(SCREENSAVER_DIR);
        fs::permissions(SCREENSAVER_DIR, fs::perms::all); // be sure read/write/execute flags are set
    }
    dp = opendir(SCREENSAVER_DIR);
    if (dp == NULL)
    {
        ReportError("Can't find screen saver directory");
        return 1;
    }
    
    do
    {
        record = readdir(dp);
        if (record)
        {
            const genericChar* name = record->d_name;
            len = strlen(name);
            if ((strcmp(&name[len-4], ".xpm") == 0) ||
                (strcmp(&name[len-4], ".XPM") == 0))
            {
                snprintf(fullpath, STRLONG, "%s/%s", SCREENSAVER_DIR, name);
                newpm = LoadPixmapFile(fullpath);
                if (newpm != NULL)
                    PixmapList.Add(newpm);
            }
        }
    }
    while (record);
    
    return 0;
}

int BlankScreen()
{
    FnTrace("BlankScreen()");
    
    if (CalibrateStage)
    {
        StopTouches();
        CalibrateStage = 0;
        if (TScreen)
            TScreen->Reset();
        StartTimers();
        Layers.UpdateAll();
    }

    // Blank the screen and try to draw an image
    DrawScreenSaver();

    return 0;
}

int DrawScreenSaver()
{
    FnTrace("DrawScreenSaver()");
    static Xpm *lastimage = NULL;

    ShowCursor(CURSOR_BLANK);
    Layers.SetScreenBlanker(1);
    XSetTSOrigin(Dis, Gfx, 0, 0);
    XSetForeground(Dis, Gfx, ColorBlack);
    XSetFillStyle(Dis, Gfx, FillSolid);
    XFillRectangle(Dis, MainWin, Gfx, 0, 0, WinWidth, WinHeight);
    
    Xpm *image = PixmapList.GetRandom(); 
    if (image != NULL && image != lastimage)
    {
        Layers.SetScreenImage(1);
        XSetClipMask(Dis, Gfx, None);
        int img_width = image->Width();
        int img_height = image->Height();
        int x = (ScrWidth - img_width) / 2;
        int y = (ScrHeight - img_height) / 2;
        XSetTSOrigin(Dis, Gfx, x, y);
        XSetTile(Dis, Gfx, image->PixmapID());
        XSetFillStyle(Dis, Gfx, FillTiled);
        XFillRectangle(Dis, MainWin, Gfx, x, y, img_width, img_height);
        XSetTSOrigin(Dis, Gfx, 0, 0);
        XSetFillStyle(Dis, Gfx, FillSolid);
        lastimage = image;
    }
    
    return 0;
}

/****
 *  UserInput:  Apparently just reacts to input, updating the system so that,
 *    for instance, the screen saver doesn't come on when it's not supposed to.
 ****/
int UserInput()
{
    FnTrace("UserInput()");
    
    // doesn't seem to work...
    XResetScreenSaver(Dis);
    XForceScreenSaver(Dis, ScreenSaverReset);
    
    TimeOut   = SystemTime;
    LastInput = SystemTime;
    if (TScreen)
        TScreen->last_reset = SystemTime;
    if (Layers.screen_blanked)
    {
        Layers.SetScreenBlanker(0);
        Layers.SetScreenImage(0);
        return 1;  // ignore input
    }
    return 0;    // accept input
}

int Calibrate(int status)
{
    FnTrace("Calibrate()");

    if (TScreen == NULL)
        return 1;

    ResetView();
    XSetFillStyle(Dis, Gfx, FillTiled);
    XSetTile(Dis, Gfx, Texture[IMAGE_DARK_SAND]);
    XFillRectangle(Dis, MainWin, Gfx, 0, 0, WinWidth, WinHeight);
    XFlush(Dis);

    switch (CalibrateStage)
    {
    case 0:   // 1st stage - setup
        StopTouches();
        sleep(1);
        TScreen->Calibrate();
        TouchInputID =
            XtAppAddInput(App, TScreen->device_no, (XtPointer)XtInputReadMask,
                          (XtInputCallbackProc)CalibrateCB, NULL);
        break;
    case 1:   // 2nd stage - get lower left touch
        XSetTile(Dis, Gfx, Texture[IMAGE_LIT_SAND]);
        XFillRectangle(Dis, MainWin, Gfx, 0, WinHeight - 40, 40, 40);
        break;
    case 2:   // 3rd stage - get upper right touch
        XSetTile(Dis, Gfx, Texture[IMAGE_LIT_SAND]);
        XFillRectangle(Dis, MainWin, Gfx, WinWidth - 40, 0, 40, 40);
        break;
    }

    XSetFillStyle(Dis, Gfx, FillSolid);
    UserInput();

    if (CalibrateStage < 3)
        ++CalibrateStage;
    else
    {
        // 4th stage - calibration done
        EndCalibrate();
    }
    return 0;
}

int EndCalibrate()
{
    FnTrace("EndCalibrate()");

    if (CalibrateStage == 0)
        return 1;

    StopTouches();
    CalibrateStage = 0;
    TScreen->Reset();
    StartTimers();
    Layers.UpdateAll();
    return 0;
}


/*********************************************************************
 * Terminal Init Function
 ********************************************************************/

int StartTimers()
{
    FnTrace("StartTimers()");

    if (UpdateTimerID == 0)
	{
        UpdateTimerID = XtAppAddTimeOut(App, UPDATE_TIME,
                                        (XtTimerCallbackProc) UpdateCB, NULL);
	}

    if (TouchInputID == 0 && TScreen && TScreen->device_no > 0)
	{
        TouchInputID = XtAppAddInput(App, 
                                     TScreen->device_no, 
                                     (XtPointer) XtInputReadMask, 
                                     (XtInputCallbackProc) TouchScreenCB, 
                                     NULL);
	}

    return 0;
}

int StopTouches()
{
    FnTrace("StopTouches()");

    if (TouchInputID)
    {
        XtRemoveInput(TouchInputID);
        TouchInputID = 0;
    }
    return 0;
}

int StopUpdates()
{
    FnTrace("StopUpdates()");

    if (UpdateTimerID)
    {
        XtRemoveTimeOut(UpdateTimerID);
        UpdateTimerID = 0;
    }
    return 0;
}

int OpenTerm(const char* display, TouchScreen *ts, int is_term_local, int term_hardware,
             int set_width, int set_height)
{
    FnTrace("OpenTerm()");

    int i;

    srand(time(NULL));

    // Init Xt & Create Application Context
    App = XtCreateApplicationContext();

    // Clear Structures
    for (i = 0; i < IMAGE_COUNT; ++i)
        Texture[i] = 0;
    for (i = 0; i < FONT_SPACE; ++i)
        FontInfo[i] = NULL;

    // Start Display
    genericChar str[STRLENGTH];
    int argc = 1;
    const genericChar* argv[] = {"vt_term"};
    IsTermLocal = is_term_local;
    Dis = XtOpenDisplay(App, display, NULL, NULL, NULL, 0, &argc,(char**) argv);
    if (Dis == NULL)
    {
        sprintf(str, "Can't open display '%s'", display);
        ReportError(str);
        return 1;
    }

    Connection = ConnectionNumber(Dis);
    ScrNo      = DefaultScreen(Dis);
    ScrPtr     = ScreenOfDisplay(Dis, ScrNo);
    ScrVis     = DefaultVisual(Dis, ScrNo);
    ScrCol     = DefaultColormap(Dis, ScrNo);
    ScrDepth   = DefaultDepth(Dis, ScrNo);
    if (set_width > -1)
        ScrWidth = set_width;
    else
        ScrWidth   = DisplayWidth(Dis, ScrNo);
    if (set_height > -1)
        ScrHeight = set_height;
    else
        ScrHeight  = DisplayHeight(Dis, ScrNo);
    WinWidth   = Min(MAX_SCREEN_WIDTH, ScrWidth);
    WinHeight  = Min(MAX_SCREEN_HEIGHT, ScrHeight);
    MaxColors  = 13 + (TEXT_COLORS * 3) + ImageColorsUsed();
    TScreen    = ts;
    RootWin    = RootWindow(Dis, ScrNo);

    // Load Fonts
    for (i = 0; i < FONTS; ++i)
    {
        int f = FontData[i].id;

        
        FontInfo[f] = GetFont(Dis, display, FontData[i].font);
        if (FontInfo[f] == NULL)
            return 1;
        
        // Load Xft font using the correct specification
        const char* xft_font_name = FontData[i].font;
        XftFontsArr[f] = XftFontOpenName(Dis, ScrNo, xft_font_name);
        
        // If Xft font loading failed, try a simple fallback
        if (XftFontsArr[f] == NULL) {
            printf("Failed to load Xft font: %s, trying fallback\n", xft_font_name);
            XftFontsArr[f] = XftFontOpenName(Dis, ScrNo, "DejaVu Serif:size=24:style=Book");
            if (XftFontsArr[f] == NULL) {
                printf("Failed to load fallback font too!\n");
            }
        }
        
        // Calculate font metrics from Xft font
        if (XftFontsArr[f]) {
            FontHeight[f] = XftFontsArr[f]->ascent + XftFontsArr[f]->descent;
            FontBaseline[f] = XftFontsArr[f]->ascent;
        } else {
            // Fallback values if font loading fails
            FontHeight[f] = 28; // Default height
            FontBaseline[f] = 20; // Default baseline
        }
    }

    // Set Default Font
    FontInfo[FONT_DEFAULT]     = FontInfo[FONT_TIMES_24];
    XftFontsArr[FONT_DEFAULT]  = XftFontsArr[FONT_TIMES_24];
    FontHeight[FONT_DEFAULT]   = FontHeight[FONT_TIMES_24];
    FontBaseline[FONT_DEFAULT] = FontBaseline[FONT_TIMES_24];

    // Create Window
    int n = 0;
    Arg args[16];
    XtSetArg(args[n], "visual",       ScrVis); ++n;
    XtSetArg(args[n], XtNdepth,       ScrDepth); ++n;
    //XtSetArg(args[n], "mappedWhenManaged", False); ++n;
    XtSetArg(args[n], XtNx,           0); ++n;
    XtSetArg(args[n], XtNy,           0); ++n;
    XtSetArg(args[n], XtNwidth,       WinWidth); ++n;
    XtSetArg(args[n], XtNheight,      WinHeight); ++n;
    XtSetArg(args[n], XtNborderWidth, 0); ++n;
    XtSetArg(args[n], "minWidth",     WinWidth); ++n;
    XtSetArg(args[n], "minHeight",    WinHeight); ++n;
    XtSetArg(args[n], "maxWidth",     WinWidth); ++n;
    XtSetArg(args[n], "maxHeight"   , WinHeight); ++n;
    XtSetArg(args[n], "mwmDecorations", 0); ++n;

    MainShell = XtAppCreateShell("POS", "viewtouch",
                                 applicationShellWidgetClass, Dis, args, n);

    XtRealizeWidget(MainShell);
    MainWin = XtWindow(MainShell);

    if (ScrDepth <= 8)
    {
        if (IsTermLocal ||
            XAllocColorCells(Dis, ScrCol, False, (Ulong *) NULL, 0,
                             Palette, MaxColors) == 0)
        {
            // Private colormap
            ScrCol = XCreateColormap(Dis, MainWin, ScrVis, AllocNone);
            XAllocColorCells(Dis, ScrCol, False, (Ulong *) NULL, 0,
                             Palette, MaxColors);
            XSetWindowColormap(Dis, MainWin, ScrCol);
        }
    }

    // General Colors
    ColorTE  = AddColor(240, 225, 205);  // Top Edge
    ColorBE  = AddColor( 90,  80,  50);  // Bottom Edge
    ColorLE  = AddColor(210, 195, 180);  // Left Edge
    ColorRE  = AddColor(120, 100,  70);  // Right Edge

    ColorLTE = AddColor(255, 255, 220);  // Lit Top Edge
    ColorLBE = AddColor(100,  85,  60);  // Lit Bottom Edge
    ColorLLE = AddColor(245, 240, 195);  // Lit Left Edge
    ColorLRE = AddColor(130, 105,  80);  // Lit Right Edge

    ColorDTE = AddColor(185, 140, 120);  // Dark Top Edge
    ColorDBE = AddColor( 55,  40,  10);  // Dark Bottom Edge
    ColorDLE = AddColor(165, 130, 110);  // Dark Left Edge
    ColorDRE = AddColor( 80,  60,  15);  // Dark Right Edge

    // Text Colors
    for (i = 0; i < TEXT_COLORS; ++i)
    {
        int pendata = PenData[i].id;
        ColorTextT[pendata] =
            AddColor(PenData[i].t[0], PenData[i].t[1], PenData[i].t[2]);
        ColorTextS[pendata] =
            AddColor(PenData[i].s[0], PenData[i].s[1], PenData[i].s[2]);
        ColorTextH[pendata] =
            AddColor(PenData[i].h[0], PenData[i].h[1], PenData[i].h[2]);
    }

    ColorBlack = ColorTextT[0];
    ColorWhite = ColorTextT[1];

    Gfx       = XCreateGC(Dis, MainWin, 0, NULL);
    ShadowPix = XmuCreateStippledPixmap(ScrPtr, 0, 1, 1);
    XSetStipple(Dis, Gfx, ShadowPix);

    // Setup Cursors
    CursorPointer = XCreateFontCursor(Dis, XC_left_ptr);
    CursorWait = XCreateFontCursor(Dis, XC_watch);
    // Setup Blank Cursor
    Pixmap p   = XCreatePixmap(Dis, MainWin, 16, 16, 1);
    GC     pgc = XCreateGC(Dis, p, 0, NULL);
    XSetForeground(Dis, pgc, BlackPixel(Dis, ScrNo));
    XSetFillStyle(Dis, pgc, FillSolid);
    XFillRectangle(Dis, p, pgc, 0, 0, 16, 16);
    XColor c;
    c.pixel = 0; c.red = 0; c.green = 0; c.blue = 0;
    CursorBlank = XCreatePixmapCursor(Dis, p, p, &c, &c, 0, 0);
    XFreePixmap(Dis, p);

    // Show Display
    ShowCursor(CURSOR_POINTER);
    XtMapWidget(MainShell);

    // Setup layers
    Layers.XWindowInit(Dis, Gfx, MainWin);
    Layer *l = new Layer(Dis, Gfx, MainWin, WinWidth, WinHeight);
    if (l)
    {
        l->id = 1;
        l->SolidRectangle(0, 0, WinWidth, WinHeight, ColorBlack);
        l->ZoneText("Please Wait", 0, 0, WinWidth, WinHeight,
                    COLOR_WHITE, FONT_TIMES_34, ALIGN_CENTER, use_embossed_text);

        genericChar tmp[256];
        if (term_hardware == 1)
            strcpy(tmp, "NCD Explora");
        else if (term_hardware == 2)
            strcpy(tmp, "NeoStation");
        else
            strcpy(tmp, "Server");
        l->ZoneText(tmp, 0, WinHeight - 30, WinWidth - 20, 30,
                    COLOR_WHITE, FONT_TIMES_20, ALIGN_RIGHT, use_embossed_text);
        Layers.Add(l);
    }
    MainLayer = l;
    ResetView();

    // Set up textures
    for (int image = 0; image < IMAGE_COUNT; image++)
    {
        Pixmap pixmap = LoadPixmap(ImageData[image]);
        if (pixmap)
            Texture[image] = pixmap;
        else
        {
            sprintf(str, "Can't Create Pixmap #%d On Display '%s'",
                    image, display);
            ReportError(str);
            return 1;
        }
    }
    ReadScreenSaverPix();

    // Add Iconify Button
    if (l && IsTermLocal)
    {
        IconifyButton *b = new IconifyButton("I", COLOR_GRAY, COLOR_LT_BLUE);
        // b->SetRegion(WinWidth - l->title_height + 4, 4,
        //              l->title_height - 8, l->title_height - 8);
        b->SetRegion(WinWidth - l->title_height + 8, 8,
                     l->title_height - 4, l->title_height - 4);
        b->font = FONT_TIMES_34;
        l->buttons.Add(b);
    }

#ifndef NO_MOTIF
    // Setup Dialogs (obsolete)
    PDialog = new PageDialog(MainShell);
    DDialog = new DefaultDialog(MainShell);
    ZDialog = new ZoneDialog(MainShell);
    MDialog = new MultiZoneDialog(MainShell);
    TDialog = new TranslateDialog(MainShell);
    LDialog = new ListDialog(MainShell);
#endif

    // Start terminal
    StartTimers();
    SystemTime.Set();
    LastInput = SystemTime;

    SocketInputID = XtAppAddInput(App, SocketNo, (XtPointer) XtInputReadMask,
                                  (XtInputCallbackProc) SocketInputCB, NULL);

    // Send server term size
    int screen_size = PAGE_SIZE_640x480;

    if (WinWidth >= 2560) 		// 16:10
        screen_size = PAGE_SIZE_2560x1600;
    else if (WinWidth >= 2560 && WinHeight < 1600) 		// 16:9
        screen_size = PAGE_SIZE_2560x1440;
    else if (WinWidth >= 1920 && WinHeight >= 1200) 		// 16:10
        screen_size = PAGE_SIZE_1920x1200;
    else if (WinWidth >= 1920 && WinHeight >= 1080) 		// 16:9
        screen_size = PAGE_SIZE_1920x1080;
    else if (WinWidth >= 1680 && WinHeight >= 1050)		// 16:10
        screen_size = PAGE_SIZE_1680x1050;
    else if (WinWidth >= 1600 && WinHeight >= 1200)
      screen_size = PAGE_SIZE_1600x1200;
    else if (WinWidth >= 1600 && WinHeight >= 900)		// 16:9
        screen_size = PAGE_SIZE_1600x900;
    else if (WinWidth >= 1440 && WinHeight >= 900)		// 16:10
        screen_size = PAGE_SIZE_1440x900; 
    else if (WinWidth >= 1366 && WinHeight >= 768)		// 16:9
        screen_size = PAGE_SIZE_1366x768;
    else if (WinWidth >= 1280 && WinHeight >= 1024) 		// 5:4
        screen_size = PAGE_SIZE_1280x1024;
    else if  (WinWidth >= 1280 && WinHeight >= 800) 		// 16:10
        screen_size = PAGE_SIZE_1280x800;
    else if (WinWidth >= 1024 && WinHeight >= 768)		// 4:3
        screen_size = PAGE_SIZE_1024x768;
    else if (WinWidth >= 1024 && WinHeight >= 600)		// 128:75
        screen_size = PAGE_SIZE_1024x600;
    else if (WinWidth >= 800 && WinHeight >= 600)			// 4:3
      screen_size = PAGE_SIZE_800x600;
    else if (WinWidth >= 800 && WinHeight >= 480)
        screen_size = PAGE_SIZE_800x480;
    else if (WinWidth >= 768 && WinHeight >= 1024)
        screen_size = PAGE_SIZE_768x1024;

    WInt8(SERVER_TERMINFO);
    WInt8(screen_size);
    WInt16(WinWidth);
    WInt16(WinHeight);
    WInt16(ScrDepth);
    SendNow();
    if (TScreen)
        TScreen->Flush();

    XtAddEventHandler(MainShell, KeyPressMask, FALSE, KeyPressCB, NULL);
    XtAddEventHandler(MainShell, ExposureMask, FALSE, ExposeCB, NULL);
    XtAddEventHandler(MainShell, ButtonPressMask, FALSE, MouseClickCB, NULL);
    XtAddEventHandler(MainShell, ButtonReleaseMask, FALSE, MouseReleaseCB, NULL);
    XtAddEventHandler(MainShell, PointerMotionMask, FALSE, MouseMoveCB, NULL);

    //Boolean okay;
    XEvent event;
    for (;;)
    {
        XtAppNextEvent(App, &event);
        XtDispatchEvent(&event);
    }
    return 0;
}

int KillTerm()
{
    FnTrace("KillTerm()");

    int i;

    StopTouches();
    StopUpdates();

    XUndefineCursor(Dis, MainWin);
    if (MainShell)
    {
        XtUnmapWidget(MainShell);
        XtDestroyWidget(MainShell);
    }
#ifndef NO_MOTIF
    if (ZDialog)
    {
        delete ZDialog;
        ZDialog = NULL;
    }
    if (MDialog)
    {
        delete MDialog;
        MDialog = NULL;
    }
    if (PDialog)
    {
        delete PDialog;
        PDialog = NULL;
    }
    if (TDialog)
    {
        delete TDialog;
        TDialog = NULL;
    }
    if (LDialog)
    {
        delete LDialog;
        LDialog = NULL;
    }
    delete DDialog;
    DDialog = NULL;
#endif
    if (ShadowPix)
    {
        XmuReleaseStippledPixmap(ScrPtr, ShadowPix);
        ShadowPix = 0;
    }
    Layers.Purge();

    for (i = 0; i < IMAGE_COUNT; ++i)
        if (Texture[i])
        {
            XFreePixmap(Dis, Texture[i]);
            Texture[i] = 0;
        }

    if (CursorPointer)
    {
        XFreeCursor(Dis, CursorPointer);
        CursorPointer = 0;
    }
    if (CursorBlank)
    {
        XFreeCursor(Dis, CursorBlank);
        CursorBlank = 0;
    }
    if (CursorWait)
    {
        XFreeCursor(Dis, CursorWait);
        CursorWait = 0;
    }

    if (Gfx)
    {
        XFreeGC(Dis, Gfx);
        Gfx = NULL;
    }

    for (i = 1; i < FONT_SPACE; ++i)
        if (FontInfo[i])
        {
            XFreeFont(Dis, FontInfo[i]);
            FontInfo[i] = NULL;
        }

    // Clean up Xft fonts
    for (i = 1; i < FONT_SPACE; ++i)
        if (XftFontsArr[i])
        {
            XftFontClose(Dis, XftFontsArr[i]);
            XftFontsArr[i] = NULL;
        }

    if (ScrCol)
    {
        XFreeColormap(Dis, ScrCol);
        ScrCol = 0;
    }
    if (Dis)
    {
        XtCloseDisplay(Dis);
        Dis = NULL;
    }
    if (App)
    {
        XtDestroyApplicationContext(App);
        App = NULL;
    }
    return 0;
}

/*********************************************************************
 * External Data Functions
 ********************************************************************/

XFontStruct *GetFontInfo(int font_id)
{
    FnTrace("GetFontInfo()");

    if (font_id >= 0 && font_id < FONT_SPACE && FontInfo[font_id])
        return FontInfo[font_id];
    else
        return FontInfo[FONT_DEFAULT];
}

int GetFontBaseline(int font_id)
{
    FnTrace("GetFontBaseline()");

    if (font_id >= 0 && font_id < FONT_SPACE && XftFontsArr[font_id])
        return XftFontsArr[font_id]->ascent;
    else if (font_id >= 0 && font_id < FONT_SPACE && FontInfo[font_id])
        return FontBaseline[font_id];
    else
        return FontBaseline[FONT_DEFAULT];
}

int GetFontHeight(int font_id)
{
    FnTrace("GetFontHeight()");

    int retval = 0;

    if (font_id >= 0 && font_id < FONT_SPACE && XftFontsArr[font_id])
        retval = XftFontsArr[font_id]->ascent + XftFontsArr[font_id]->descent;
    else if (font_id >= 0 && font_id < FONT_SPACE && FontInfo[font_id])
        retval = FontHeight[font_id];
    else
        retval = FontHeight[FONT_DEFAULT];
    return retval;
}

Pixmap GetTexture(int texture)
{
    FnTrace("GetTexture()");

    if (texture >= 0 && texture < IMAGE_COUNT && Texture[texture])
        return Texture[texture];
    else
        return Texture[0]; // default texture
}

XftFont *GetXftFontInfo(int font_id)
{
    FnTrace("GetXftFontInfo()");

    if (font_id >= 0 && font_id < FONT_SPACE && XftFontsArr[font_id])
        return XftFontsArr[font_id];
    else
        return XftFontsArr[FONT_DEFAULT];
}

// Reload all Xft fonts and update font metrics
void TerminalReloadFonts()
{
    // Free existing Xft fonts
    for (int i = 0; i < FONTS; ++i) {
        int f = FontData[i].id;
        if (XftFontsArr[f]) {
            XftFontClose(Dis, XftFontsArr[f]);
            XftFontsArr[f] = NULL;
        }
    }
    // Reload fonts
    for (int i = 0; i < FONTS; ++i) {
        int f = FontData[i].id;
        const char* xft_font_name = FontData[i].font;
        XftFontsArr[f] = XftFontOpenName(Dis, ScrNo, xft_font_name);
        if (XftFontsArr[f]) {
            FontHeight[f] = XftFontsArr[f]->ascent + XftFontsArr[f]->descent;
            FontBaseline[f] = XftFontsArr[f]->ascent;
        } else {
            FontHeight[f] = 0;
            FontBaseline[f] = 0;
        }
    }
    
    // Update all layer objects (buttons) to use the new fonts
    // This ensures toolbar buttons and other layer objects get updated fonts
    Layer *layer = Layers.Head();
    while (layer != NULL) {
        LayerObject *obj = layer->buttons.Head();
        while (obj != NULL) {
            // Try to cast to LO_PushButton to check if it's a button
            LO_PushButton *button = dynamic_cast<LO_PushButton*>(obj);
            if (button != NULL) {
                // Update button font to use the current font family
                // The font family will change while keeping the same size
                // Force redraw of this button
                layer->update = 1;
            }
            obj = obj->next;
        }
        layer = layer->next;
    }
    
    // Force redraw of all layers to show updated fonts
    Layers.UpdateAll();
}

