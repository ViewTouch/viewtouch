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
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <array>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <cstring>

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
namespace Constants {
    constexpr int UPDATE_TIME = 500;
    constexpr const char* XWD = "/usr/X11R6/bin/xwd";          /* screen capture utility */
    constexpr const char* SCREEN_DIR = "/usr/viewtouch/screenshots";  /* where to save screenshots */
    constexpr int TERM_RELOAD_FONTS = 0xA5;
    constexpr int MAX_TRIES = 8;
    constexpr int MAX_XPM_SIZE = 4194304;
    constexpr const char* SCREENSAVER_DIR = VIEWTOUCH_PATH "/dat/screensaver";
    constexpr int EXTRA_ICON_WIDTH = 35;
    constexpr int MAX_EVENTS_PER_SECOND = 1000;
    constexpr int MAX_CONSECUTIVE_ERRORS = 10;
    constexpr int SLEEP_TIME_MS = 10000;  // 10ms
    constexpr int RETRY_DELAY_MS = 100000;  // 100ms
    constexpr int RECONNECT_ATTEMPTS = 20;
    constexpr int RECONNECT_DELAY_SEC = 2;
}

// Dynamic scaling system global variables
float ScaleFactorX = 1.0f;
float ScaleFactorY = 1.0f;
float ScaleFactor = 1.0f;

/*********************************************************************
 * Dynamic Scaling Functions
 ********************************************************************/

void CalculateScaleFactors(int screen_width, int screen_height)
{
    // Universal 1920x1080 reference for all displays
    ScaleFactorX = static_cast<float>(screen_width) / static_cast<float>(REFERENCE_WIDTH);
    ScaleFactorY = static_cast<float>(screen_height) / static_cast<float>(REFERENCE_HEIGHT);
    
    // Use uniform scaling to maintain aspect ratios
    ScaleFactor = std::min(ScaleFactorX, ScaleFactorY);
    
    // For exact 1920x1080 displays, use 1.0 scale factor
    if (screen_width == REFERENCE_WIDTH && screen_height == REFERENCE_HEIGHT) {
        ScaleFactor = 1.0f;
    }
    
    // Ensure reasonable scale factor bounds for usability
    ScaleFactor = std::max(0.75f, std::min(3.0f, ScaleFactor));
    ScaleFactorX = ScaleFactor;
    ScaleFactorY = ScaleFactor;
}

int ScaleX(int x)
{
    return static_cast<int>(x * ScaleFactorX + 0.5f);  // Round to nearest pixel
}

int ScaleY(int y)
{
    return static_cast<int>(y * ScaleFactorY + 0.5f);  // Round to nearest pixel
}

int ScaleW(int w)
{
    return static_cast<int>(w * ScaleFactorX + 0.5f);  // Round to nearest pixel
}

int ScaleH(int h)
{
    return static_cast<int>(h * ScaleFactorY + 0.5f);  // Round to nearest pixel
}

int ScaleFontSize(int original_size)
{
    // Scale font size but ensure it's at least 8 pixels for readability
    int scaled_size = static_cast<int>(original_size * ScaleFactor + 0.5f);
    return std::max(8, scaled_size);
}

float GetScaleFactor()
{
    return ScaleFactor;
}

// Custom exception classes for better error handling
class ViewTouchException : public std::runtime_error {
public:
    explicit ViewTouchException(const std::string& message) : std::runtime_error(message) {}
};

class DisplayException : public ViewTouchException {
public:
    explicit DisplayException(const std::string& message) : ViewTouchException("Display Error: " + message) {}
};

class FontException : public ViewTouchException {
public:
    explicit FontException(const std::string& message) : ViewTouchException("Font Error: " + message) {}
};

class SocketException : public ViewTouchException {
public:
    explicit SocketException(const std::string& message) : ViewTouchException("Socket Error: " + message) {}
};

// RAII wrapper for file descriptors
class FileDescriptor {
private:
    int fd_;
    
public:
    explicit FileDescriptor(int fd = -1) noexcept : fd_(fd) {}
    
    // Move constructor
    FileDescriptor(FileDescriptor&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }
    
    // Move assignment operator
    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        if (this != &other) {
            if (fd_ > 0) close(fd_);
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }
    
    // Delete copy constructor and assignment operator
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    
    // Destructor
    ~FileDescriptor() noexcept {
        if (fd_ > 0) close(fd_);
    }
    
    // Accessors
    int get() const noexcept { return fd_; }
    bool is_valid() const noexcept { return fd_ > 0; }
    
    // Release ownership
    int release() noexcept {
        int temp = fd_;
        fd_ = -1;
        return temp;
    }
    
    // Reset with new fd
    void reset(int new_fd = -1) noexcept {
        if (fd_ > 0) close(fd_);
        fd_ = new_fd;
    }
    
    // Implicit conversion to int for compatibility
    operator int() const noexcept { return fd_; }
};

// RAII wrapper for X11 resources
class X11ResourceManager {
public:
    static void cleanup();
    
    ~X11ResourceManager() {
        cleanup();
    }
};

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
    {COLOR_WHITE,       {255, 255, 255}, { 64,  64,  64}, {117,  97,  78}},
    {COLOR_RED,         {235,   0,   0}, { 47,   0,   0}, {242, 200, 200}},
    {COLOR_GREEN,       {  0, 128,   0}, {  0,  42,   0}, {140, 236, 140}},
    {COLOR_BLUE,        {  0,   0, 230}, {  0,   0,  47}, {200, 200, 240}},
    {COLOR_YELLOW,      {255, 255,   0}, { 96,  64,   0}, {127, 127,  78}},
    {COLOR_BROWN,       {132,  76,  38}, { 47,   0,   0}, {224, 212, 200}},
    {COLOR_ORANGE,      {255,  84,   0}, { 47,  23,   0}, {255, 222, 195}},
    {COLOR_PURPLE,      {100,   0, 200}, {  0,   0,  47}, {240, 200, 240}},
    {COLOR_TEAL,        {  0, 132, 168}, {  0,  16,  39}, {176, 216, 255}},
    {COLOR_GRAY,        { 96,  96,  96}, { 32,  32,  32}, {222, 222, 222}},
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

class FontNameClass
{
private:
    std::string foundry;
    std::string family;
    std::string weight;
    std::string slant;
    std::string width;
    std::string adstyl;  // for future reference
    std::string pixels;
    std::string points;
    std::string horres;
    std::string vertres;
    std::string spacing;
    std::string avgwidth;
    std::string charset;

    int parsed;

    void  MakeGeneric();
    int   SetItem(const std::string& word);

public:
    FontNameClass();
    FontNameClass(const char* fontname);
    FontNameClass(const FontNameClass& other) = default;
    FontNameClass(FontNameClass&& other) noexcept = default;
    FontNameClass& operator=(const FontNameClass& other) = default;
    FontNameClass& operator=(FontNameClass&& other) noexcept = default;

    void Clear();
    int  Parse(const char* fontname);
    const char* ToString();

    const char* Foundry() const { return foundry.c_str(); }
    const char* Family() const { return family.c_str(); }
    const char* Weight() const { return weight.c_str(); }
    const char* Slant() const { return slant.c_str(); }
    const char* Width() const { return width.c_str(); }
    const char* Pixels() const { return pixels.c_str(); }
    const char* Points() const { return points.c_str(); }
    const char* HorRes() const { return horres.c_str(); }
    const char* VertRes() const { return vertres.c_str(); }
    const char* Spacing() const { return spacing.c_str(); }
    const char* AvgWidth() const { return avgwidth.c_str(); }
    const char* CharSet() const { return charset.c_str(); }

    void ClearFoundry() { foundry = "*"; }
    void ClearFamily() { family = "*"; }
    void ClearWeight() { weight = "*"; }
    void ClearSlant() { slant = "*"; }
    void ClearWidth() { width = "*"; }
    void ClearPixels() { pixels = "*"; }
    void ClearPoints() { points = "*"; }
    void ClearHorRes() { horres = "*"; }
    void ClearVertRes() { vertres = "*"; }
    void ClearSpacing() { spacing = "*"; }
    void ClearAvgWidth() { avgwidth = "*"; }
    void ClearCharSet() { charset = "*"; }

    void SetFoundry(const char* set) { foundry = set; }
    void SetFamily(const char* set) { family = set; }
    void SetWeight(const char* set) { weight = set; }
    void SetSlant(const char* set) { slant = set; }
    void SetWidth(const char* set) { width = set; }
    void SetPixels(const char* set) { pixels = set; }
    void SetPoints(const char* set) { points = set; }
    void SetHorRes(const char* set) { horres = set; }
    void SetVertRes(const char* set) { vertres = set; }
    void SetSpacing(const char* set) { spacing = set; }
    void SetAvgWidth(const char* set) { avgwidth = set; }
    void SetCharSet(const char* set) { charset = set; }
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

    foundry.clear();
    family.clear();
    weight.clear();
    slant.clear();
    width.clear();
    adstyl.clear();
    pixels.clear();
    points.clear();
    horres.clear();
    vertres.clear();
    spacing.clear();
    avgwidth.clear();
    charset.clear();

    parsed = 0;
}

int FontNameClass::SetItem(const std::string& word)
{
    FnTrace("FontNameClass::SetItem()");
    int retval = 0;

    if (foundry.empty())
        foundry = word;
    else if (family.empty())
        family = word;
    else if (weight.empty())
        weight = word;
    else if (slant.empty())
        slant = word;
    else if (width.empty())
        width = word;
    else if (pixels.empty())
        pixels = word;
    else if (points.empty())
        points = word;
    else if (horres.empty())
        horres = word;
    else if (vertres.empty())
        vertres = word;
    else if (spacing.empty())
        spacing = word;
    else if (avgwidth.empty())
        avgwidth = word;
    else if (charset.empty())
        charset = word;
    else
    {
        charset += "-";
        charset += word;
    }
    
    return retval;
}

int FontNameClass::Parse(const char* fontname)
{
    FnTrace("FontNameClass::Parse()");
    int retval = 0;
    auto len = strlen(fontname);
    int idx = 0;
    std::string word;

    Clear();

    // require the fontname to start with a dash
    if (fontname[idx] != '-')
        return 1;

    idx += 1;  // skip past the first dash
    word.clear();
    while (idx < len)
    {
        if (fontname[idx] == '-' || fontname[idx] == '\0')
        {
            SetItem(word);
            word.clear();
        }
        else
        {
            word += fontname[idx];
        }
        idx += 1;
    }
    if (!word.empty())
    {
        SetItem(word);
    }

    if (idx == len)
        parsed = 1;

    return retval;
}

void FontNameClass::MakeGeneric()
{
    FnTrace("FontNameClass::MakeGeneric()");

    foundry = "*";
    family = "*";
    weight = "*";
    slant = "*";
    width = "*";
    pixels = "*";
    points = "*";
    horres = "*";
    vertres = "*";
    spacing = "*";
    avgwidth = "*";
    charset = "*";

    parsed = 1;  // close enough
}

const char* FontNameClass::ToString()
{
    FnTrace("FontNameClass::ToString()");
    static std::string namestring;

    if (foundry.empty())
        MakeGeneric();

    namestring = "-" + foundry + "-" + family + "-" + weight + "-" + slant + "-" + width + 
                 "-" + adstyl + "-" + pixels + "-" + points + "-" + horres + "-" + vertres + 
                 "-" + spacing + "-" + avgwidth + "-" + charset;

    return namestring.c_str();
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

std::array<XFontStruct*, FONT_SPACE> FontInfo{};
std::array<XftFont*, FONT_SPACE> XftFontsArr{};
std::array<int, FONT_SPACE> FontBaseline{};
std::array<int, FONT_SPACE> FontHeight{};


std::array<int, TEXT_COLORS> ColorTextT{};
std::array<int, TEXT_COLORS> ColorTextH{};
std::array<int, TEXT_COLORS> ColorTextS{};
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

// Implementation of X11ResourceManager::cleanup
void X11ResourceManager::cleanup() {
    extern GC Gfx;
    extern Display* Dis;
    extern Colormap ScrCol;
    
    if (Gfx) {
        XFreeGC(Dis, Gfx);
        Gfx = nullptr;
    }
    
    // Clean up fonts
    for (int i = 0; i < FONT_SPACE; ++i) {
        if (FontInfo[i]) {
            XFreeFont(Dis, FontInfo[i]);
            FontInfo[i] = nullptr;
        }
        if (XftFontsArr[i]) {
            XftFontClose(Dis, XftFontsArr[i]);
            XftFontsArr[i] = nullptr;
        }
    }
    
    if (ScrCol) {
        XFreeColormap(Dis, ScrCol);
        ScrCol = 0;
    }
    
    if (Dis) {
        XtCloseDisplay(Dis);
        Dis = nullptr;
    }
    
    if (App) {
        XtDestroyApplicationContext(App);
        App = nullptr;
    }
}

static Widget       MainShell = NULL;
static int          ScrNo     = 0;
static Screen      *ScrPtr    = NULL;
static int          ScrHeight = 0;
static int          ScrWidth  = 0;
static Window       RootWin;
static int          Colors    = 0;
static int          MaxColors = 0;
static std::array<Ulong, 256> Palette{};
static int          ScreenBlankTime = 60;
static int          UpdateTimerID = 0;
static int          TouchInputID  = 0;
static std::unique_ptr<TouchScreen> TScreen = nullptr;
static int          ResetTime = 20;
static TimeInfo     TimeOut, LastInput;
static int          CalibrateStage = 0;
static int          SocketInputID = 0;
static Cursor       CursorPointer = 0;
static Cursor       CursorBlank = 0;
static Cursor       CursorWait = 0;

#ifndef NO_MOTIF
static std::unique_ptr<PageDialog>      PDialog = nullptr;
static std::unique_ptr<ZoneDialog>      ZDialog = nullptr;
static std::unique_ptr<MultiZoneDialog> MDialog = nullptr;
static std::unique_ptr<TranslateDialog> TDialog = nullptr;
static std::unique_ptr<ListDialog>      LDialog = nullptr;
static std::unique_ptr<DefaultDialog>   DDialog = nullptr;
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

std::unique_ptr<CCard> creditcard = nullptr;
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

int  SendNow() noexcept { return BufferOut.Write(SocketNo); }
int  WInt8(int val) noexcept  { return BufferOut.Put8(val); }
int  RInt8() noexcept         { return BufferIn.Get8(); }
int  WInt16(int val) noexcept { return BufferOut.Put16(val); }
int  RInt16() noexcept        { return BufferIn.Get16(); }
int  WInt32(int val) noexcept { return BufferOut.Put32(val); }
int  RInt32() noexcept        { return BufferIn.Get32(); }
long WLong(long val) noexcept { return BufferOut.PutLong(val); }
long RLong() noexcept         { return BufferIn.GetLong(); }
long long WLLong(long long val) noexcept { return BufferOut.PutLLong(val); }
long long RLLong() noexcept   { return BufferIn.GetLLong(); }
int  WFlt(Flt val) noexcept   { return BufferOut.Put32((int) (val * 100.0)); }
Flt  RFlt() noexcept          { return (Flt) BufferIn.Get32() / 100.0; }

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

    next = nullptr;
    fore = nullptr;
}

Translation::Translation(const char* new_key, const char* new_value)
{
    FnTrace("Translation::Translation()");

    next = nullptr;
    fore = nullptr;
    key = new_key;
    value = new_value;
}

int Translation::Match(const char* check_key)
{
    FnTrace("Translation::Match()");

    return (key == check_key) ? 1 : 0;
}

int Translation::GetKey(char* store, int maxlen)
{
    FnTrace("Translation::GetKey()");

    strncpy(store, key.c_str(), maxlen);
    return 1;
}

int Translation::GetValue(char* store, int maxlen)
{
    FnTrace("Translation::GetValue()");

    strncpy(store, value.c_str(), maxlen);
    return 1;
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

    if (event == NULL)
    {
        fprintf(stderr, "ExposeCB: event is NULL, skipping expose processing\n");
        return;
    }

    static RegionInfo area;

    XExposeEvent *e = reinterpret_cast<XExposeEvent*>(event);
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
    int update_time = Constants::UPDATE_TIME;

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
            if (TScreen) // Critical fix: Check TScreen again after EndCalibrate
                TScreen->Reset();
        }
    }
    
    // Critical fix: Clear the old timer ID before setting new one to prevent race condition
    UpdateTimerID = 0;
    UpdateTimerID = XtAppAddTimeOut(App, update_time, (XtTimerCallbackProc) UpdateCB, NULL);
}

void TouchScreenCB(XtPointer client_data, int *fid, XtInputId *id)
{
    FnTrace("TouchScreenCB()");

    if (TScreen == nullptr)
    {
        if (silent_mode > 0)
            return;
        fprintf(stderr, "TouchScreenCB: TScreen is NULL, skipping touch processing\n");
        return;
    }

    int tx = -1;
    int ty = -1;
    int mode = 0;
    int status = 0;

    status = TScreen->ReadTouch(tx, ty, mode);
    if (status == 1 && mode == TOUCH_DOWN && UserInput() == 0)
    {
        int x = (tx * ScrWidth)  / TScreen->x_res;
        int y = ((TScreen->y_res - 1 - ty) * ScrHeight) / TScreen->y_res;
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

    if (event == NULL)
    {
        fprintf(stderr, "KeyPressCB: event is NULL, skipping key press processing\n");
        return;
    }

    static std::array<char, 1024> swipe_buffer;
    static char last_char    = '\0';
    static int  swipe_char   = 0;
    static int  swipe_stage  = 0;
    static int  swipe_time   = 0;
    static int  swipe_track2 = 0;
    static int  fake_cc      = 0;
    if (UserInput())
        return;

    XKeyEvent *e = reinterpret_cast<XKeyEvent*>(event);
    KeySym key = 0;
    std::array<genericChar, 32> buffer;

    int len = XLookupString(e, buffer.data(), 31, &key, NULL);
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
                WStr(swipe_buffer.data());
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
                // Use safe string concatenation with bounds checking
                std::string test_card_data = "%B5186900000000121^TEST CARD/MONERIS^;??";
                if (test_card_data.length() < swipe_buffer.size()) {
                    std::strcpy(swipe_buffer.data(), test_card_data.c_str());
                }
            }
            else if (randcc == 1 || randcc == 3 || randcc == 5)
            {  // correct data, tracks 1 and 2
                // Use safe string concatenation with bounds checking
                std::string test_card_data = "%B5186900000000121^TEST CARD/MONERIS";
                test_card_data += "^08051011234567890131674486261606288842611?";
                test_card_data += ";5186900000000121=";
                test_card_data += "08051015877400050041?";
                
                // Copy to buffer with bounds checking
                if (test_card_data.length() < swipe_buffer.size()) {
                    std::strcpy(swipe_buffer.data(), test_card_data.c_str());
                }
            }
            else if (randcc == 2)
            {
                // Use safe string concatenation with bounds checking
                std::string test_card_data = "%B5186900000000121^TEST CARD/MONERIS";
                test_card_data += "%B5186900000000121^TEST CARD/MONERIS";
                test_card_data += "%B5186900000000121^TEST CARD/MONERIS";
                test_card_data += "%B5186900000000121^TEST CARD/MONERIS";
                test_card_data += "%B5186900000000121^TEST CARD/MONERIS";
                test_card_data += "%B5186900000000121^TEST CARD/MONERIS";
                test_card_data += "%B5186900000000121^TEST CARD/MONERIS";
                
                // Copy to buffer with bounds checking
                if (test_card_data.length() < swipe_buffer.size()) {
                    std::strcpy(swipe_buffer.data(), test_card_data.c_str());
                }
            }
            else if (randcc == 4)
            {
                // Use safe string concatenation with bounds checking
                std::string test_card_data = "%B5186900000000121^TEST CARD/MONERIS";
                test_card_data += "^08051011234567890131674486261606288842611?";
                test_card_data += ";5186900000000121=";
                test_card_data += "08051015877400050041?";
                
                // Copy to buffer with bounds checking
                if (test_card_data.length() < swipe_buffer.size()) {
                    std::strcpy(swipe_buffer.data(), test_card_data.c_str());
                }
            }
            else if (randcc == 6)
            {
                // Use safe string concatenation with bounds checking
                std::string test_card_data = "%B5186900000000121^TEST CARD/MONERIS";
                test_card_data += "08051015877400050041?";
                
                // Copy to buffer with bounds checking
                if (test_card_data.length() < swipe_buffer.size()) {
                    std::strcpy(swipe_buffer.data(), test_card_data.c_str());
                }
            }
            else if (randcc == 7)
            {
                // Use safe string concatenation with bounds checking
                std::string test_card_data = "%B5186900000000121^TEST CARD/MONERIS";
                test_card_data += "^08051011234567890131674486261606288842611?";
                test_card_data += "%B5186900000000121^TEST CARD/MONERIS";
                test_card_data += "^08051011234567890131674486261606288842611?";
                
                // Copy to buffer with bounds checking
                if (test_card_data.length() < swipe_buffer.size()) {
                    std::strcpy(swipe_buffer.data(), test_card_data.c_str());
                }
            }
            else if (randcc == 8)
            {
                // Use safe string concatenation with bounds checking
                std::string test_card_data = "%B5186900000000121^TEST CARD/MONERIS";
                
                // Copy to buffer with bounds checking
                if (test_card_data.length() < swipe_buffer.size()) {
                    std::strcpy(swipe_buffer.data(), test_card_data.c_str());
                }
            }
            else if (randcc == 9)
            {
                // Use safe string concatenation with bounds checking
                std::string test_card_data = "%B\n\n";
                
                // Copy to buffer with bounds checking
                if (test_card_data.length() < swipe_buffer.size()) {
                    std::strcpy(swipe_buffer.data(), test_card_data.c_str());
                }
            }
            fake_cc = 0;
            printf("Sending Fake Credit Card:  '%s'\n", swipe_buffer.data());
            WInt8(SERVER_SWIPE);
            WStr(swipe_buffer.data());
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

    if (event == NULL)
    {
        fprintf(stderr, "MouseClickCB: event is NULL, skipping mouse click processing\n");
        return;
    }

    if (CalibrateStage)
        return;
    if (UserInput())
        return;
    if (silent_mode > 0)
        return;

    XButtonEvent *btnevent = reinterpret_cast<XButtonEvent*>(event);
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

    if (event == NULL)
    {
        fprintf(stderr, "MouseReleaseCB: event is NULL, skipping mouse release processing\n");
        return;
    }

    if (UserInput())
        return;
    if (silent_mode > 0)
        return;

    XButtonEvent *b = reinterpret_cast<XButtonEvent*>(event);
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

    if (event == NULL)
    {
        fprintf(stderr, "MouseMoveCB: event is NULL, skipping mouse move processing\n");
        return;
    }

    XPointerMovedEvent *e = reinterpret_cast<XPointerMovedEvent*>(event);
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

    if (TScreen == nullptr)
    {
        fprintf(stderr, "CalibrateCB: TScreen is NULL, skipping calibration\n");
        return;
    }

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

        // Critical fix: Remove invalid input handler to prevent infinite ppoll loop
        fprintf(stderr, "SocketInputCB: Connection lost after %d failures, removing input handler\n", failure);
        
        // Remove the invalid input handler immediately to prevent hang
        if (SocketInputID != 0)
        {
            // Try to remove the input handler, but don't crash if Xt context is invalid
            try {
                XtRemoveInput(SocketInputID);
            } catch (...) {
                fprintf(stderr, "SocketInputCB: Exception during XtRemoveInput, continuing\n");
            }
            SocketInputID = 0;
        }
        
        // Close the invalid socket
        if (SocketNo > 0)
        {
            close(SocketNo);
            SocketNo = -1;
        }
        
        // Try to notify the user
        ReportError("Connection to server lost. Attempting to reconnect...");
        
        // Attempt to reconnect to the server
        if (ReconnectToServer() == 0)
        {
            failure = 0; // Reset failure counter on successful reconnection
            ReportError("Successfully reconnected to server.");
            return;
        }
        
        // If reconnection fails, wait before trying again
        if (failure < Constants::RECONNECT_ATTEMPTS) // Try up to 20 times before giving up
        {
            sleep(Constants::RECONNECT_DELAY_SEC); // Wait 2 seconds before next attempt
            return;
        }
        
        // After many failed attempts, restart the terminal gracefully
        ReportError("Unable to reconnect to server. Restarting terminal...");
        RestartTerminal();
        return;
    }

    Layer *l = MainLayer;
    if (l == NULL)
    {
        fprintf(stderr, "SocketInputCB: MainLayer is NULL, skipping processing\n");
        return;
    }
    
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
        // Critical fix: Add bounds checking to prevent infinite loops
        if (BufferIn.size < 1)
        {
            fprintf(stderr, "SocketInputCB: Buffer size too small for command code\n");
            break;
        }
        
        int code = RInt8();
        if (code < 0)
        {
            fprintf(stderr, "SocketInputCB: Failed to read command code\n");
            break;
        }
        
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
            if (creditcard == nullptr)
                creditcard = std::make_unique<CCard>();
            if (creditcard != nullptr)
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
            if (creditcard == nullptr)
                creditcard = std::make_unique<CCard>();
            if (creditcard != nullptr)
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
            if (creditcard == nullptr)
                creditcard = std::make_unique<CCard>();
            if (creditcard != nullptr)
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
            if (creditcard == nullptr)
                creditcard = std::make_unique<CCard>();
            if (creditcard != nullptr)
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
            if (creditcard == nullptr)
                creditcard = std::make_unique<CCard>();
            if (creditcard != nullptr)
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
            if (creditcard == nullptr)
                creditcard = std::make_unique<CCard>();
            if (creditcard != nullptr)
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
            if (creditcard == nullptr)
                creditcard = std::make_unique<CCard>();
            if (creditcard != nullptr)
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
            if (creditcard == nullptr)
                creditcard = std::make_unique<CCard>();
            if (creditcard != nullptr)
            {
                creditcard->BatchSettle();
                creditcard->Clear();
            }
            break;
        case TERM_CC_INIT:
            if (creditcard == nullptr)
                creditcard = std::make_unique<CCard>();
            if (creditcard != nullptr)
            {
                creditcard->CCInit();
                creditcard->Clear();
            }
            break;
        case TERM_CC_TOTALS:
            if (creditcard == nullptr)
                creditcard = std::make_unique<CCard>();
            if (creditcard != nullptr)
            {
                creditcard->Totals();
                creditcard->Clear();
            }
            break;
        case TERM_CC_DETAILS:
            if (creditcard == nullptr)
                creditcard = std::make_unique<CCard>();
            if (creditcard != nullptr)
            {
                creditcard->Details();
                creditcard->Clear();
            }
            break;
        case TERM_CC_CLEARSAF:
            if (creditcard == nullptr)
                creditcard = std::make_unique<CCard>();
            if (creditcard != nullptr)
            {
                creditcard->ClearSAF();
                creditcard->Clear();
            }
            break;
        case TERM_CC_SAFDETAILS:
            if (creditcard == nullptr)
                creditcard = std::make_unique<CCard>();
            if (creditcard != nullptr)
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
        case Constants::TERM_RELOAD_FONTS:
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

    std::array<genericChar, 256> filename;
    int no = 0;

    // Verify directory exists
    if (!DoesFileExist(Constants::SCREEN_DIR))
    {
        mkdir(Constants::SCREEN_DIR, 0);
        chmod(Constants::SCREEN_DIR, 0777);
    }
    
    // Find first unused filename (starting with 0)
    while (1)
    {
        sprintf(filename.data(), "%s/vtscreen%d.wd", Constants::SCREEN_DIR, no++);
        if (!DoesFileExist(filename.data()))
            break;
    }

    // Log the action
    std::array<genericChar, 256> str;
    sprintf(str.data(), "Saving screen image to file '%s'", filename.data());
    ReportError(str.data());

    // Generate the screenshot
    char command[STRLONG];
    snprintf(command, STRLONG, "%s -root -display %s >%s",
             Constants::XWD, DisplayString(Dis), filename.data());
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

int AddColor(XColor &c) noexcept
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

int AddColor(const int red, const int green, const int blue) noexcept
{
    FnTrace("AddColor()");

    const auto r = red   % 256;
    const auto g = green % 256;
    const auto b = blue  % 256;

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
    
    status = XpmCreatePixmapFromData(Dis, MainWin, const_cast<char**>(image_data), &retxpm, NULL, NULL);
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
        UpdateTimerID = XtAppAddTimeOut(App, Constants::UPDATE_TIME,
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
        // Try to remove the input handler, but don't crash if Xt context is invalid
        try {
            XtRemoveInput(TouchInputID);
        } catch (...) {
            fprintf(stderr, "StopTouches: Exception during XtRemoveInput, continuing\n");
        }
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
    Texture.fill(0);
    FontInfo.fill(nullptr);

    // Start Display
    genericChar str[STRLENGTH];
    int argc = 1;
    const genericChar* argv[] = {"vt_term"};
    IsTermLocal = is_term_local;
    Dis = XtOpenDisplay(App, display, NULL, NULL, NULL, 0, &argc, const_cast<char**>(argv));
    if (Dis == nullptr)
    {
        std::string error_msg = "Can't open display '" + std::string(display) + "'";
        ReportError(error_msg);
        throw DisplayException(error_msg);
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
    // Dynamic scaling: Use full screen dimensions with reasonable limits for X11 compatibility
    // This ensures XServer XSDL and other remote displays work properly
    WinWidth   = std::min(ScrWidth, 4096);   // Max 4K width for X11 compatibility
    WinHeight  = std::min(ScrHeight, 4096);  // Max 4K height for X11 compatibility
    
    // Initialize dynamic scaling system based on actual screen dimensions
    CalculateScaleFactors(WinWidth, WinHeight);
    
    MaxColors  = 13 + (TEXT_COLORS * 3) + ImageColorsUsed();
    TScreen.reset(ts);
    RootWin    = RootWindow(Dis, ScrNo);

    // Load Fonts
    for (const auto& fontData : FontData)
    {
        int f = fontData.id;

        
        FontInfo[f] = GetFont(Dis, display, fontData.font);
        if (FontInfo[f] == nullptr)
            throw FontException("Failed to load font: " + std::string(fontData.font));
        
        // Load Xft font using the correct specification
        const char* xft_font_name = fontData.font;
        XftFontsArr[f] = XftFontOpenName(Dis, ScrNo, xft_font_name);
        
        // If Xft font loading failed, try a simple fallback
        if (XftFontsArr[f] == nullptr) {
            printf("Failed to load Xft font: %s, trying fallback\n", xft_font_name);
            XftFontsArr[f] = XftFontOpenName(Dis, ScrNo, "DejaVu Serif:size=24:style=Book");
            if (XftFontsArr[f] == nullptr) {
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
    std::array<Arg, 16> args;
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
                                 applicationShellWidgetClass, Dis, args.data(), n);

    XtRealizeWidget(MainShell);
    MainWin = XtWindow(MainShell);

        if (ScrDepth <= 8)
        {
            if (IsTermLocal ||
                XAllocColorCells(Dis, ScrCol, False, static_cast<Ulong*>(nullptr), 0,
                                 Palette.data(), MaxColors) == 0)
            {
                // Private colormap
                ScrCol = XCreateColormap(Dis, MainWin, ScrVis, AllocNone);
                XAllocColorCells(Dis, ScrCol, False, static_cast<Ulong*>(nullptr), 0,
                                 Palette.data(), MaxColors);
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

        std::array<genericChar, 256> tmp;
        if (term_hardware == 1)
            strcpy(tmp.data(), "NCD Explora");
        else if (term_hardware == 2)
            strcpy(tmp.data(), "NeoStation");
        else
            strcpy(tmp.data(), "Server");
        l->ZoneText(tmp.data(), 0, WinHeight - 30, WinWidth - 20, 30,
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
    PDialog = std::make_unique<PageDialog>(MainShell);
    DDialog = std::make_unique<DefaultDialog>(MainShell);
    ZDialog = std::make_unique<ZoneDialog>(MainShell);
    MDialog = std::make_unique<MultiZoneDialog>(MainShell);
    TDialog = std::make_unique<TranslateDialog>(MainShell);
    LDialog = std::make_unique<ListDialog>(MainShell);
#endif

    // Start terminal
    StartTimers();
    SystemTime.Set();
    LastInput = SystemTime;

    SocketInputID = XtAppAddInput(App, SocketNo, (XtPointer) XtInputReadMask,
                                  (XtInputCallbackProc) SocketInputCB, NULL);

    // Universal 1920x1080 template for ALL displays with dynamic scaling
    int screen_size = PAGE_SIZE_1920x1080;  // Universal base template
    WInt8(SERVER_TERMINFO);
    WInt8(screen_size);
    WInt16(1920);  // Always report 1920x1080 to server
    WInt16(1080);
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
    int event_count = 0;
    int max_events_per_second = Constants::MAX_EVENTS_PER_SECOND; // Prevent infinite loops
    auto last_time = std::chrono::steady_clock::now();
    int consecutive_errors = 0;
    const int max_consecutive_errors = Constants::MAX_CONSECUTIVE_ERRORS;
    
    for (;;)
    {
        // Critical fix: Add error handling for XtAppNextEvent
        if (XtAppPending(App) == 0)
        {
            // No events pending, check if we should exit
            if (SocketNo <= 0 && SocketInputID == 0)
            {
                fprintf(stderr, "No socket connection and no input handler, exiting gracefully\n");
                break;
            }
            usleep(Constants::SLEEP_TIME_MS); // Sleep 10ms to prevent busy waiting
            continue;
        }
        
        try
        {
            XtAppNextEvent(App, &event);
            XtDispatchEvent(&event);
            consecutive_errors = 0; // Reset error counter on success
        }
        catch (...)
        {
            consecutive_errors++;
            fprintf(stderr, "Error in event processing (attempt %d/%d)\n", consecutive_errors, max_consecutive_errors);
            
            if (consecutive_errors >= max_consecutive_errors)
            {
                fprintf(stderr, "Too many consecutive errors, exiting gracefully\n");
                break;
            }
            
            // Small delay before retrying
            usleep(Constants::RETRY_DELAY_MS); // 100ms
            continue;
        }
        
        // Critical fix: Prevent infinite event loops
        event_count++;
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_time);
        
        if (elapsed.count() >= 1) // Reset counter every second
        {
            if (event_count > max_events_per_second)
            {
                fprintf(stderr, "Warning: High event rate detected (%d events/second), possible infinite loop\n", event_count);
            }
            event_count = 0;
            last_time = current_time;
        }
    }
    return 0;
}

// Critical fix: Add reconnection and restart functions
int ReconnectToServer()
{
    FnTrace("ReconnectToServer()");
    
    // Try to reconnect to the server
    struct sockaddr_un server_adr;
    server_adr.sun_family = AF_UNIX;
    strcpy(server_adr.sun_path, "/tmp/vt_term"); // Default socket path
    
    SocketNo = socket(AF_UNIX, SOCK_STREAM, 0);
    if (SocketNo <= 0)
    {
        fprintf(stderr, "ReconnectToServer: Failed to create socket\n");
        return 1;
    }
    
    if (connect(SocketNo, reinterpret_cast<struct sockaddr*>(&server_adr), SUN_LEN(&server_adr)) < 0)
    {
        fprintf(stderr, "ReconnectToServer: Can't connect to server (error %d)\n", errno);
        close(SocketNo);
        SocketNo = -1;
        return 1;
    }
    
    // Re-add the input handler
    SocketInputID = XtAppAddInput(App, SocketNo, (XtPointer) XtInputReadMask,
                                  (XtInputCallbackProc) SocketInputCB, NULL);
    
    return 0;
}

void RestartTerminal()
{
    FnTrace("RestartTerminal()");
    
    // Clean up resources
    StopTouches();
    StopUpdates();
    
    if (SocketInputID != 0)
    {
        // Try to remove the input handler, but don't crash if Xt context is invalid
        try {
            XtRemoveInput(SocketInputID);
        } catch (...) {
            fprintf(stderr, "Cleanup: Exception during XtRemoveInput, continuing\n");
        }
        SocketInputID = 0;
    }
    
    if (SocketNo > 0)
    {
        close(SocketNo);
        SocketNo = -1;
    }
    
    // Exit the terminal gracefully
    exit(0);
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
    ZDialog.reset();
    MDialog.reset();
    PDialog.reset();
    TDialog.reset();
    LDialog.reset();
    DDialog.reset();
#endif
    if (ShadowPix)
    {
        XmuReleaseStippledPixmap(ScrPtr, ShadowPix);
        ShadowPix = 0;
    }
    Layers.Purge();

    for (auto& texture : Texture)
        if (texture)
        {
            XFreePixmap(Dis, texture);
            texture = 0;
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

    for (auto& font : FontInfo)
        if (font)
        {
            XFreeFont(Dis, font);
            font = nullptr;
        }

    // Clean up Xft fonts
    for (auto& xftFont : XftFontsArr)
        if (xftFont)
        {
            XftFontClose(Dis, xftFont);
            xftFont = nullptr;
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

XFontStruct *GetFontInfo(const int font_id) noexcept
{
    FnTrace("GetFontInfo()");

    if (font_id >= 0 && font_id < FONT_SPACE && FontInfo[font_id])
        return FontInfo[font_id];
    else
        return FontInfo[FONT_DEFAULT];
}

int GetFontBaseline(const int font_id) noexcept
{
    FnTrace("GetFontBaseline()");

    if (font_id >= 0 && font_id < FONT_SPACE && XftFontsArr[font_id])
        return XftFontsArr[font_id]->ascent;
    else if (font_id >= 0 && font_id < FONT_SPACE && FontInfo[font_id])
        return FontBaseline[font_id];
    else
        return FontBaseline[FONT_DEFAULT];
}

int GetFontHeight(const int font_id) noexcept
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

Pixmap GetTexture(const int texture) noexcept
{
    FnTrace("GetTexture()");

    if (texture >= 0 && texture < IMAGE_COUNT && Texture[texture])
        return Texture[texture];
    else
        return Texture[0]; // default texture
}

XftFont *GetXftFontInfo(const int font_id) noexcept
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
    for (const auto& fontData : FontData) {
        int f = fontData.id;
        if (XftFontsArr[f]) {
            XftFontClose(Dis, XftFontsArr[f]);
            XftFontsArr[f] = nullptr;
        }
    }
    // Reload fonts
    for (const auto& fontData : FontData) {
        int f = fontData.id;
        const char* xft_font_name = fontData.font;
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
    while (layer != nullptr) {
        LayerObject *obj = layer->buttons.Head();
        while (obj != nullptr) {
            // Try to cast to LO_PushButton to check if it's a button
            LO_PushButton *button = dynamic_cast<LO_PushButton*>(obj);
            if (button != nullptr) {
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

