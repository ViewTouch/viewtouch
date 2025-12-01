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

#ifdef HAVE_PNG
#include <png.h>
#endif

#ifdef HAVE_JPEG
#include <jpeglib.h>
#endif

#ifdef HAVE_GIF
#include <gif_lib.h>
#endif
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
#include <cstdlib>

#include "debug.hh"
#include "safe_string_utils.hh"
#include "term_view.hh"
#include "term_dialog.hh"
#include "remote_link.hh"
#include "image_data.hh"
#include "touch_screen.hh"
#include "layer.hh"
#include "generic_char.hh"

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

// Connection state management
enum ConnectionState {
    CONNECTION_CONNECTED,
    CONNECTION_DISCONNECTED,
    CONNECTION_RECONNECTING,
    CONNECTION_FAILED
};

class ConnectionMonitor {
private:
    ConnectionState state_;
    time_t last_heartbeat_;
    time_t last_reconnect_attempt_;
    int reconnect_attempts_;
    int max_reconnect_attempts_;
    int reconnect_delay_;
    bool keep_alive_enabled_;

public:
    ConnectionMonitor() :
        state_(CONNECTION_DISCONNECTED),
        last_heartbeat_(0),
        last_reconnect_attempt_(0),
        reconnect_attempts_(0),
        max_reconnect_attempts_(10),
        reconnect_delay_(2),
        keep_alive_enabled_(true) {}

    void set_connected() {
        state_ = CONNECTION_CONNECTED;
        last_heartbeat_ = time(NULL);
        reconnect_attempts_ = 0;
        ReportError("Connection established");
    }

    void set_disconnected() {
        if (state_ != CONNECTION_DISCONNECTED) {
            state_ = CONNECTION_DISCONNECTED;
            ReportError("Connection lost - attempting to reconnect");
        }
    }

    void set_reconnecting() {
        state_ = CONNECTION_RECONNECTING;
        last_reconnect_attempt_ = time(NULL);
        reconnect_attempts_++;
    }

    void set_failed() {
        state_ = CONNECTION_FAILED;
        ReportError("Connection failed permanently");
    }

    ConnectionState get_state() const { return state_; }

    bool should_attempt_reconnect() {
        if (state_ != CONNECTION_DISCONNECTED) return false;
        if (reconnect_attempts_ >= max_reconnect_attempts_) return false;

        time_t now = time(NULL);
        int delay = reconnect_delay_ * (1 << (reconnect_attempts_ - 1)); // Exponential backoff
        if (delay > 60) delay = 60; // Cap at 60 seconds

        return (now - last_reconnect_attempt_) >= delay;
    }

    bool is_healthy() const {
        if (state_ != CONNECTION_CONNECTED) return false;
        if (!keep_alive_enabled_) return true;

        time_t now = time(NULL);
        return (now - last_heartbeat_) < 30; // 30 second timeout
    }

    void send_heartbeat() {
        last_heartbeat_ = time(NULL);
    }

    void reset_reconnect_attempts() {
        reconnect_attempts_ = 0;
    }

    int get_reconnect_attempts() const { return reconnect_attempts_; }
    int get_max_reconnect_attempts() const { return max_reconnect_attempts_; }
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
    {FONT_TIMES_48,     "DejaVu Serif:size=28:style=Book"},
    {FONT_TIMES_20B,    "DejaVu Serif:size=12:style=Bold"},
    {FONT_TIMES_24B,    "DejaVu Serif:size=14:style=Bold"},
    {FONT_TIMES_34B,    "DejaVu Serif:size=18:style=Bold"},
    {FONT_TIMES_48B,    "DejaVu Serif:size=28:style=Bold"},
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
    {COLOR_LT_BLUE,     { 80,  45,  25}, { 20,  10,   5}, {180, 160, 140}},  // Dark brown (replaced light blue)
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
    pixmap = 0;
    mask = 0;
}

Xpm::Xpm(Pixmap pm)
{
    next = NULL;
    fore = NULL;
    width = 0;
    height = 0;
    pixmap = pm;
    mask = 0;
}

Xpm::Xpm(Pixmap pm, int w, int h)
{
    next = NULL;
    fore = NULL;
    width = w;
    height = h;
    pixmap = pm;
    mask = 0;
}

Xpm::Xpm(Pixmap pm, Pixmap m, int w, int h)
{
    next = NULL;
    fore = NULL;
    width = w;
    height = h;
    pixmap = pm;
    mask = m;
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

// Global connection monitor for robust connection handling
static ConnectionMonitor connection_monitor;

// Forward declarations for reconnection UI functions
void ShowReconnectingMessage();
void HideReconnectingMessage();

// UI State preservation for disconnections
struct SavedUIState {
    int current_page;
    int cursor_x, cursor_y;
    bool input_active;
    std::string last_message;

    SavedUIState() : current_page(0), cursor_x(0), cursor_y(0), input_active(false) {}

    void save() {
        if (MainLayer != NULL) {
            current_page = MainLayer->page_x;  // Use page_x instead of page
            cursor_x = MainLayer->cursor;       // Use cursor instead of cursor_x
            cursor_y = 0;                       // No cursor_y, set to 0
        }
        input_active = false;                    // TODO: Check touchscreen status when available
        last_message = Message.Value();
    }

    void restore() {
        if (MainLayer != NULL) {
            // Request the server to switch to the saved page
            // This will be sent when connection is restored
            fprintf(stderr, "UI State: Requesting page %d restore\n", current_page);
        }
        Message.Set(last_message.c_str());
    }
};

static SavedUIState saved_ui_state;

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

// Performance optimization: Color cache to avoid expensive XQueryColor calls
ColorCache g_color_cache;


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
int InitializeTouchScreen();
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
    else
    {
        // Screen is blanked - continuously update screensaver for animation
        Layers.UpdateAll();
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

    // Enhanced touch event handling
    TouchEvent event;
    int status = TScreen->ReadTouchEvent(event);
    
    if (status == 1 && UserInput() == 0)
    {
        // Process touch events for gestures
        TScreen->ProcessTouchEvents();
        
        // Handle different touch modes
        switch (event.mode)
        {
            case TOUCH_DOWN:
            {
                int x = (event.x * ScrWidth) / TScreen->x_res;
                int y = ((TScreen->y_res - 1 - event.y) * ScrHeight) / TScreen->y_res;
                
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
                {
                    Layers.Touch(x, y);
                }
                break;
            }
            
            case TOUCH_UP:
                // Handle touch release if needed
                break;
                
            case TOUCH_MOVE:
                // Handle touch movement if needed
                break;
                
            default:
                // Handle other touch modes
                break;
        }
    }
    else if (status == -1)
    {
        // Handle touch errors - could implement recovery strategies
        if (silent_mode == 0)
        {
            fprintf(stderr, "TouchScreenCB: Touch read error, status: %d\n", status);
        }
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
                vt_safe_string::safe_copy(swipe_buffer.data(), swipe_buffer.size(), test_card_data);
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
                    vt_safe_string::safe_copy(swipe_buffer.data(), swipe_buffer.size(), test_card_data);
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
                    vt_safe_string::safe_copy(swipe_buffer.data(), swipe_buffer.size(), test_card_data);
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
                    vt_safe_string::safe_copy(swipe_buffer.data(), swipe_buffer.size(), test_card_data);
                }
            }
            else if (randcc == 6)
            {
                // Use safe string concatenation with bounds checking
                std::string test_card_data = "%B5186900000000121^TEST CARD/MONERIS";
                test_card_data += "08051015877400050041?";
                
                // Copy to buffer with bounds checking
                if (test_card_data.length() < swipe_buffer.size()) {
                    vt_safe_string::safe_copy(swipe_buffer.data(), swipe_buffer.size(), test_card_data);
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
                    vt_safe_string::safe_copy(swipe_buffer.data(), swipe_buffer.size(), test_card_data);
                }
            }
            else if (randcc == 8)
            {
                // Use safe string concatenation with bounds checking
                std::string test_card_data = "%B5186900000000121^TEST CARD/MONERIS";
                
                // Copy to buffer with bounds checking
                if (test_card_data.length() < swipe_buffer.size()) {
                    vt_safe_string::safe_copy(swipe_buffer.data(), swipe_buffer.size(), test_card_data);
                }
            }
            else if (randcc == 9)
            {
                // Use safe string concatenation with bounds checking
                std::string test_card_data = "%B\n\n";
                
                // Copy to buffer with bounds checking
                if (test_card_data.length() < swipe_buffer.size()) {
                    vt_safe_string::safe_copy(swipe_buffer.data(), swipe_buffer.size(), test_card_data);
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

    static int consecutive_failures = 0;
    int val = BufferIn.Read(SocketNo);

    if (val <= 0)
    {
        consecutive_failures++;

        // Update connection monitor state
        if (consecutive_failures >= 3) {  // More aggressive failure detection
            connection_monitor.set_disconnected();

            // Save UI state before showing reconnecting message
            saved_ui_state.save();

            // Show "Reconnecting..." message on screen instead of just logging
            if (MainLayer != NULL) {
                ShowReconnectingMessage();
            }
        }

        // Check if we should attempt reconnection
        if (connection_monitor.should_attempt_reconnect()) {
            connection_monitor.set_reconnecting();

            fprintf(stderr, "SocketInputCB: Attempting reconnection (attempt %d/%d)\n",
                    connection_monitor.get_reconnect_attempts(),
                    connection_monitor.get_max_reconnect_attempts());

            // Remove the invalid input handler
            if (SocketInputID != 0) {
                try {
                    XtRemoveInput(SocketInputID);
                } catch (...) {
                    fprintf(stderr, "SocketInputCB: Exception during XtRemoveInput, continuing\n");
                }
                SocketInputID = 0;
            }

            // Close the invalid socket
            if (SocketNo > 0) {
                close(SocketNo);
                SocketNo = -1;
            }

            // Attempt to reconnect
            if (ReconnectToServer() == 0) {
                connection_monitor.set_connected();
                consecutive_failures = 0;

                // Hide reconnecting message and restore normal operation
                if (MainLayer != NULL) {
                    HideReconnectingMessage();
                }

                ReportError("Successfully reconnected to server.");
                return;
            } else {
                // Reconnection failed, will try again later
                fprintf(stderr, "SocketInputCB: Reconnection attempt failed\n");
            }
        }

        // If we've exceeded max reconnection attempts, fail permanently
        if (connection_monitor.get_reconnect_attempts() >= connection_monitor.get_max_reconnect_attempts()) {
            connection_monitor.set_failed();
            ReportError("Unable to reconnect to server after maximum attempts. Terminal will continue in offline mode.");
            consecutive_failures = 0;

            // Keep the terminal running in offline mode instead of restarting
            return;
        }

        return;
    }

    // Successful read - update connection health
    consecutive_failures = 0;
    if (connection_monitor.get_state() != CONNECTION_CONNECTED) {
        connection_monitor.set_connected();
        if (MainLayer != NULL) {
            HideReconnectingMessage();
            // Restore UI state after reconnection
            saved_ui_state.restore();
        }
    }
    connection_monitor.send_heartbeat();

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

    int failure = 0;
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
        case TERM_PIXMAP:
            n1 = RInt16();  // x
            n2 = RInt16();  // y
            n3 = RInt16();  // w
            n4 = RInt16();  // h
            s1 = RStr();    // filename
            l->DrawPixmap(n1, n2, n3, n4, s1);
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
        vt_safe_string::safe_format(filename.data(), filename.size(), "%s/vtscreen%d.wd", Constants::SCREEN_DIR, no++);
        if (!DoesFileExist(filename.data()))
            break;
    }

    // Log the action
    std::array<genericChar, 256> str;
    vt_safe_string::safe_format(str.data(), str.size(), "Saving screen image to file '%s'", filename.data());
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

#ifdef HAVE_PNG
/****
 * RemoveCheckeredBackground: Detect and remove checkered/checked backgrounds
 * from PNG images. Common checkered patterns use alternating light gray (#C0C0C0)
 * and white (#FFFFFF) squares, or similar light colors. This function detects
 * these patterns and converts matching pixels to transparent.
 * 
 * Returns true if checkered background was detected and removed, false otherwise.
 * If checkered background is detected, the image will be converted to RGBA format
 * with alpha channel added.
 ****/
static bool RemoveCheckeredBackground(png_bytep** row_pointers_ptr, int width, int height, 
                                       int& channels, int& rowbytes)
{
    FnTrace("RemoveCheckeredBackground()");
    
    fprintf(stderr, "RemoveCheckeredBackground: Processing image %dx%d, channels=%d\n", 
            width, height, channels);
    
    // Common checkered background colors (light gray and white variations)
    // Typical patterns: #C0C0C0/#FFFFFF, #D0D0D0/#FFFFFF, #E0E0E0/#FFFFFF
    const int CHECKERED_COLOR_THRESHOLD = 40;  // Tolerance for color matching
    const int MIN_LIGHTNESS = 180;  // Minimum RGB average for checkered colors (light colors only)
    
    // Checkered pattern typically uses 8x8 or 16x16 pixel squares
    const int CHECKER_SIZE_MIN = 4;
    const int CHECKER_SIZE_MAX = 32;
    
    png_bytep* row_pointers = *row_pointers_ptr;
    
    // We need RGB data to detect checkered patterns
    if (channels < 3) {
        return false;  // Can't detect checkered pattern in grayscale
    }
    
    // First pass: detect if checkered pattern exists by sampling
    int total_pixels = width * height;
    int checkered_threshold = total_pixels / 50;  // At least 2% of pixels should match
    
    // Try different checker sizes to detect pattern
    int detected_checker_size = 0;
    int best_match_score = 0;
    
    for (int checker_size = CHECKER_SIZE_MIN; checker_size <= CHECKER_SIZE_MAX; checker_size *= 2) {
        int pattern_matches = 0;
        int samples = 0;
        
        // Sample the image to detect checkered pattern
        // Check multiple positions to find the pattern
        for (int offset_y = 0; offset_y < checker_size && offset_y < height; offset_y++) {
            for (int offset_x = 0; offset_x < checker_size && offset_x < width; offset_x++) {
                for (int y = offset_y; y < height - checker_size; y += checker_size * 2) {
                    for (int x = offset_x; x < width - checker_size; x += checker_size * 2) {
                        samples++;
                        
                        // Get colors from two adjacent squares in the checker pattern
                        if (y + checker_size / 2 < height && x + checker_size / 2 < width) {
                            png_bytep row1 = row_pointers[y];
                            png_bytep row2 = row_pointers[y + checker_size / 2];
                            
                            unsigned char r1 = row1[x * channels + 0];
                            unsigned char g1 = row1[x * channels + 1];
                            unsigned char b1 = row1[x * channels + 2];
                            
                            unsigned char r2 = row2[(x + checker_size / 2) * channels + 0];
                            unsigned char g2 = row2[(x + checker_size / 2) * channels + 1];
                            unsigned char b2 = row2[(x + checker_size / 2) * channels + 2];
                            
                            // Check if colors are light (potential checkered background)
                            int lightness1 = (r1 + g1 + b1) / 3;
                            int lightness2 = (r2 + g2 + b2) / 3;
                            
                            if (lightness1 >= MIN_LIGHTNESS && lightness2 >= MIN_LIGHTNESS) {
                                // Check if colors are different (alternating pattern)
                                int diff_r = abs((int)r1 - (int)r2);
                                int diff_g = abs((int)g1 - (int)g2);
                                int diff_b = abs((int)b1 - (int)b2);
                                
                                if (diff_r + diff_g + diff_b > CHECKERED_COLOR_THRESHOLD) {
                                    pattern_matches++;
                                }
                            }
                        }
                    }
                }
                
                // Check if this offset gives us a good pattern match
                if (samples > 0) {
                    int match_score = pattern_matches * 100 / samples;
                    if (match_score > best_match_score) {
                        best_match_score = match_score;
                        detected_checker_size = checker_size;
                    }
                }
            }
        }
    }
    
    // Also try a simpler approach: just count light pixels that could be checkered
    int light_pixel_count = 0;
    for (int y = 0; y < height; y++) {
        png_bytep row = row_pointers[y];
        for (int x = 0; x < width; x++) {
            unsigned char r = row[x * channels + 0];
            unsigned char g = row[x * channels + 1];
            unsigned char b = row[x * channels + 2];
            int lightness = (r + g + b) / 3;
            
            // Count very light pixels (white or light gray)
            if (lightness >= 240 || (lightness >= 190 && lightness <= 230)) {
                int color_variance = abs((int)r - (int)g) + abs((int)g - (int)b) + abs((int)r - (int)b);
                if (color_variance < 30) {  // Grayish or white
                    light_pixel_count++;
                }
            }
        }
    }
    
    // If we have a lot of light pixels, likely has checkered background
    bool has_many_light_pixels = (light_pixel_count * 100 / total_pixels) > 10;  // More than 10% light pixels
    
    if (detected_checker_size == 0 && !has_many_light_pixels) {
        fprintf(stderr, "RemoveCheckeredBackground: No checkered pattern detected\n");
        return false;  // No checkered pattern detected
    }
    
    if (detected_checker_size > 0) {
        fprintf(stderr, "RemoveCheckeredBackground: Detected checkered pattern (checker size ~%d, %d%% match)\n", 
                detected_checker_size, best_match_score);
    } else if (has_many_light_pixels) {
        fprintf(stderr, "RemoveCheckeredBackground: Detected many light pixels (%.1f%%), treating as checkered background\n",
                light_pixel_count * 100.0 / total_pixels);
        // Use a default checker size for removal
        detected_checker_size = 8;
    }
    
    // Convert to RGBA if not already (needed to add transparency)
    bool needs_alpha = (channels == 3);
    png_bytep* new_row_pointers = nullptr;
    
    if (needs_alpha) {
        // Allocate new row pointers with alpha channel
        int new_rowbytes = width * 4;
        new_row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
        if (!new_row_pointers) {
            fprintf(stderr, "RemoveCheckeredBackground: Cannot allocate new row pointers\n");
            return false;
        }
        
        for (int y = 0; y < height; y++) {
            new_row_pointers[y] = (png_byte*)malloc(new_rowbytes);
            if (!new_row_pointers[y]) {
                fprintf(stderr, "RemoveCheckeredBackground: Cannot allocate new row data\n");
                for (int i = 0; i < y; i++) free(new_row_pointers[i]);
                free(new_row_pointers);
                return false;
            }
            
            // Copy RGB data and add alpha channel (fully opaque initially)
            for (int x = 0; x < width; x++) {
                new_row_pointers[y][x * 4 + 0] = row_pointers[y][x * 3 + 0];
                new_row_pointers[y][x * 4 + 1] = row_pointers[y][x * 3 + 1];
                new_row_pointers[y][x * 4 + 2] = row_pointers[y][x * 3 + 2];
                new_row_pointers[y][x * 4 + 3] = 255;  // Fully opaque initially
            }
        }
        
        // Update to use new row pointers
        *row_pointers_ptr = new_row_pointers;
        row_pointers = new_row_pointers;
        channels = 4;
        rowbytes = new_rowbytes;
    } else if (channels == 4) {
        // Image already has alpha channel - we'll modify it
        // Make sure existing transparent pixels stay transparent
        fprintf(stderr, "RemoveCheckeredBackground: Image already has alpha channel, will modify existing transparency\n");
    }
    
    // Second pass: remove checkered background pixels
    int checkered_pixel_count = 0;
    for (int y = 0; y < height; y++) {
        png_bytep row = row_pointers[y];
        for (int x = 0; x < width; x++) {
            unsigned char r = row[x * channels + 0];
            unsigned char g = row[x * channels + 1];
            unsigned char b = row[x * channels + 2];
            
            int lightness = (r + g + b) / 3;
            int color_variance = abs((int)r - (int)g) + abs((int)g - (int)b) + abs((int)r - (int)b);
            
            bool should_remove = false;
            
            // Simple and aggressive: remove any pixel that looks like checkered background
            // Check for white pixels (very common in checkered backgrounds: #FFFFFF, #FEFEFE, etc.)
            if (r >= 240 && g >= 240 && b >= 240) {
                should_remove = true;
            }
            // Check for light gray pixels - common checkered background colors:
            // #C0C0C0 (192), #D0D0D0 (208), #E0E0E0 (224), #F0F0F0 (240)
            else if (lightness >= 180 && lightness <= 245) {
                // Check if it's a uniform grayish color (low color variance = gray, not colored)
                if (color_variance < 45) {
                    // This is a light grayish color - very likely checkered background
                    should_remove = true;
                }
            }
            
            // Note: We don't require perfect pattern matching - if it's a light uniform color,
            // it's likely a checkered background pixel, so remove it.
            
            if (should_remove) {
                // Only remove if pixel isn't already transparent (preserve existing transparency)
                unsigned char current_alpha = (channels == 4) ? row[x * channels + 3] : 255;
                if (current_alpha > 0) {  // Not already transparent
                    row[x * channels + 3] = 0;  // Make transparent
                    checkered_pixel_count++;
                }
            }
        }
    }
    
    fprintf(stderr, "RemoveCheckeredBackground: Removed %d checkered background pixels (%.1f%%)\n", 
            checkered_pixel_count, checkered_pixel_count * 100.0 / total_pixels);
    
    return checkered_pixel_count > checkered_threshold;
}

/****
 * LoadPNGFile: Load a PNG file and convert it to an Xpm object
 ****/
Xpm *LoadPNGFile(const char* file_name)
{
    FnTrace("LoadPNGFile()");

    if (!file_name) {
        fprintf(stderr, "LoadPNGFile: No filename provided\n");
        return nullptr;
    }

    FILE *fp = fopen(file_name, "rb");
    if (!fp) {
        fprintf(stderr, "LoadPNGFile: Cannot open file %s\n", file_name);
        return nullptr;
    }

    // Read PNG header to verify format
    png_byte header[8];
    if (fread(header, 1, 8, fp) != 8) {
        fprintf(stderr, "LoadPNGFile: Cannot read PNG header from %s\n", file_name);
        fclose(fp);
        return nullptr;
    }

    if (png_sig_cmp(header, 0, 8)) {
        fprintf(stderr, "LoadPNGFile: File %s is not a valid PNG\n", file_name);
        fclose(fp);
        return nullptr;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fprintf(stderr, "LoadPNGFile: Cannot create PNG read struct\n");
        fclose(fp);
        return nullptr;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        fprintf(stderr, "LoadPNGFile: Cannot create PNG info struct\n");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return nullptr;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "LoadPNGFile: PNG read error in %s\n", file_name);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return nullptr;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    int width = png_get_image_width(png_ptr, info_ptr);
    int height = png_get_image_height(png_ptr, info_ptr);
    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    fprintf(stderr, "LoadPNGFile: Loading %s - %dx%d, color_type=%d, bit_depth=%d\n",
            file_name, width, height, color_type, bit_depth);

    // Convert palette images to RGB
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);

    // Ensure we have at least 8 bits per channel
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    // Convert grayscale to RGB
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    // Add alpha channel if missing for consistency
    if (color_type != PNG_COLOR_TYPE_RGBA && png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);

    // Ensure 8-bit depth
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    int rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    int channels = png_get_channels(png_ptr, info_ptr);
    fprintf(stderr, "LoadPNGFile: Row bytes = %d, channels = %d\n", rowbytes, channels);

    // Allocate image data
    png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
    if (!row_pointers) {
        fprintf(stderr, "LoadPNGFile: Cannot allocate row pointers\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return nullptr;
    }

    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte*)malloc(rowbytes);
        if (!row_pointers[y]) {
            fprintf(stderr, "LoadPNGFile: Cannot allocate row data\n");
            for (int i = 0; i < y; i++) free(row_pointers[i]);
            free(row_pointers);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
            fclose(fp);
            return nullptr;
        }
    }

    png_read_image(png_ptr, row_pointers);

    // Remove checkered backgrounds if detected
    png_bytep* original_row_pointers = row_pointers;
    bool checkered_removed = RemoveCheckeredBackground(&row_pointers, width, height, channels, rowbytes);
    if (checkered_removed && row_pointers != original_row_pointers) {
        // Free original row pointers if they were replaced
        for (int y = 0; y < height; y++) {
            free(original_row_pointers[y]);
        }
        free(original_row_pointers);
        fprintf(stderr, "LoadPNGFile: Checkered background removed, image converted to RGBA\n");
    }

    // Convert to X11 Pixmap with transparency mask
    Pixmap pixmap = 0;
    Pixmap mask = 0;
    bool has_transparency = (channels == 4);  // RGBA format
    
    if (width <= WinWidth && height <= WinHeight) {
        Visual *visual = DefaultVisual(Dis, DefaultScreen(Dis));
        int depth = DefaultDepth(Dis, DefaultScreen(Dis));

        fprintf(stderr, "LoadPNGFile: Creating pixmap %dx%d, depth=%d, has_transparency=%d\n", 
                width, height, depth, has_transparency);

        pixmap = XCreatePixmap(Dis, MainWin, width, height, depth);
        if (!pixmap) {
            fprintf(stderr, "LoadPNGFile: Cannot create pixmap\n");
        } else {
            // Create mask pixmap if we have transparency
            if (has_transparency) {
                mask = XCreatePixmap(Dis, MainWin, width, height, 1);  // 1-bit depth for mask
                if (!mask) {
                    fprintf(stderr, "LoadPNGFile: Cannot create mask pixmap\n");
                }
            }
            
            // Create XImage with proper format for the visual
            XImage *ximage = XCreateImage(Dis, visual, depth, ZPixmap, 0,
                                         (char*)malloc(width * height * 4), width, height, 32, 0);
            if (!ximage) {
                fprintf(stderr, "LoadPNGFile: Cannot create XImage\n");
                XFreePixmap(Dis, pixmap);
                if (mask) XFreePixmap(Dis, mask);
                pixmap = 0;
                mask = 0;
            } else {
                // Create mask image if needed
                XImage *mask_image = nullptr;
                if (mask) {
                    mask_image = XCreateImage(Dis, visual, 1, XYBitmap, 0,
                                            (char*)malloc((width + 7) / 8 * height), 
                                            width, height, 8, 0);
                    if (mask_image) {
                        // Initialize mask to all transparent
                        memset(mask_image->data, 0, (width + 7) / 8 * height);
                    }
                }
                
                // Copy PNG data to XImage pixels
                for (int y = 0; y < height; y++) {
                    png_bytep row = row_pointers[y];
                    for (int x = 0; x < width; x++) {
                        unsigned long pixel;
                        bool is_opaque = true;
                        
                        if (channels >= 3) {
                            // RGB or RGBA format
                            unsigned char r = row[x*channels + 0];
                            unsigned char g = row[x*channels + 1];
                            unsigned char b = row[x*channels + 2];

                            if (channels == 4) {
                                // RGBA format - handle alpha transparency
                                unsigned char a = row[x*channels + 3];
                                is_opaque = (a >= 128);  // Threshold for transparency
                                
                                if (!is_opaque) {
                                    // Set pixel to black for transparent areas (will be masked)
                                    pixel = 0;
                                } else {
                                    pixel = (r << 16) | (g << 8) | b;
                                }
                            } else {
                                // RGB format - fully opaque
                                pixel = (r << 16) | (g << 8) | b;
                            }
                        } else if (channels == 1) {
                            // Grayscale - fully opaque
                            unsigned char gray = row[x];
                            pixel = (gray << 16) | (gray << 8) | gray;
                        } else {
                            // Unknown format, use black
                            pixel = 0;
                        }
                        XPutPixel(ximage, x, y, pixel);
                        
                        // Update mask if we have one
                        if (mask_image && is_opaque) {
                            XPutPixel(mask_image, x, y, 1);  // 1 = opaque, 0 = transparent
                        }
                    }
                }

                // Put image to pixmap
                XPutImage(Dis, pixmap, Gfx, ximage, 0, 0, 0, 0, width, height);
                
                // Put mask to mask pixmap
                if (mask && mask_image) {
                    GC mask_gc = XCreateGC(Dis, mask, 0, NULL);
                    XPutImage(Dis, mask, mask_gc, mask_image, 0, 0, 0, 0, width, height);
                    XFreeGC(Dis, mask_gc);
                    XDestroyImage(mask_image);
                }
                
                XDestroyImage(ximage);
                fprintf(stderr, "LoadPNGFile: Successfully loaded PNG with%s transparency\n",
                        mask ? "" : "out");
            }
        }
    } else {
        fprintf(stderr, "LoadPNGFile: Image too large (%dx%d > %dx%d)\n",
                width, height, WinWidth, WinHeight);
    }

    // Cleanup
    for (int y = 0; y < height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    if (pixmap) {
        return new Xpm(pixmap, mask, width, height);
    }

    fprintf(stderr, "LoadPNGFile: Failed to load PNG %s\n", file_name);
    return nullptr;
}
#endif

#ifdef HAVE_JPEG
/****
 * LoadJPEGFile: Load a JPEG file and convert it to an Xpm object
 ****/
Xpm *LoadJPEGFile(const char* file_name)
{
    FnTrace("LoadJPEGFile()");

    if (!file_name)
        return nullptr;

    FILE *fp = fopen(file_name, "rb");
    if (!fp)
        return nullptr;

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int num_components = cinfo.output_components;

    // Allocate image data
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr) &cinfo, JPOOL_IMAGE, width * num_components, 1);

    Pixmap pixmap = 0;
    if (width <= WinWidth && height <= WinHeight) {
        pixmap = XCreatePixmap(Dis, MainWin, width, height, DefaultDepth(Dis, DefaultScreen(Dis)));

        XImage *ximage = XCreateImage(Dis, DefaultVisual(Dis, DefaultScreen(Dis)),
                                     DefaultDepth(Dis, DefaultScreen(Dis)), ZPixmap, 0,
                                     (char*)malloc(width * height * 4), width, height, 32, 0);

        // Read and convert JPEG data
        while (cinfo.output_scanline < height) {
            jpeg_read_scanlines(&cinfo, buffer, 1);
            int y = cinfo.output_scanline - 1;

            for (int x = 0; x < width; x++) {
                unsigned long pixel = 0;
                if (num_components >= 3) {
                    pixel = (buffer[0][x*num_components+0] << 16) |
                           (buffer[0][x*num_components+1] << 8) |
                           buffer[0][x*num_components+2];
                }
                XPutPixel(ximage, x, y, pixel);
            }
        }

        XPutImage(Dis, pixmap, Gfx, ximage, 0, 0, 0, 0, width, height);
        XDestroyImage(ximage);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);

    if (pixmap) {
        return new Xpm(pixmap, width, height);
    }

    return nullptr;
}
#endif

#ifdef HAVE_GIF
/****
 * LoadGIFFile: Load a GIF file and convert it to an Xpm object
 ****/
Xpm *LoadGIFFile(const char* file_name)
{
    FnTrace("LoadGIFFile()");

    if (!file_name)
        return nullptr;

    // For simplicity, we'll use libgif to load the first frame
    GifFileType *gif = DGifOpenFileName(file_name, NULL);
    if (!gif) {
        return nullptr;
    }

    if (DGifSlurp(gif) != GIF_OK) {
        DGifCloseFile(gif, NULL);
        return nullptr;
    }

    if (gif->ImageCount == 0) {
        DGifCloseFile(gif, NULL);
        return nullptr;
    }

    SavedImage *image = &gif->SavedImages[0];
    GifImageDesc *desc = &image->ImageDesc;
    ColorMapObject *color_map = image->ImageDesc.ColorMap ?
                               image->ImageDesc.ColorMap : gif->SColorMap;

    if (!color_map) {
        DGifCloseFile(gif, NULL);
        return nullptr;
    }

    int width = desc->Width;
    int height = desc->Height;

    Pixmap pixmap = 0;
    if (width <= WinWidth && height <= WinHeight) {
        pixmap = XCreatePixmap(Dis, MainWin, width, height, DefaultDepth(Dis, DefaultScreen(Dis)));

        XImage *ximage = XCreateImage(Dis, DefaultVisual(Dis, DefaultScreen(Dis)),
                                     DefaultDepth(Dis, DefaultScreen(Dis)), ZPixmap, 0,
                                     (char*)malloc(width * height * 4), width, height, 32, 0);

        // Convert GIF pixels to X11 pixels
        GifPixelType *gif_pixels = image->RasterBits;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int pixel_index = y * width + x;
                GifPixelType color_index = gif_pixels[pixel_index];

                if (color_index < color_map->ColorCount) {
                    GifColorType *color = &color_map->Colors[color_index];
                    unsigned long pixel = (color->Red << 16) | (color->Green << 8) | color->Blue;
                    XPutPixel(ximage, x, y, pixel);
                }
            }
        }

        XPutImage(Dis, pixmap, Gfx, ximage, 0, 0, 0, 0, width, height);
        XDestroyImage(ximage);
    }

    DGifCloseFile(gif, NULL);

    if (pixmap) {
        return new Xpm(pixmap, width, height);
    }

    return nullptr;
}
#endif

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

// Global flag to reset screensaver position
bool g_reset_screensaver = false;

int DrawScreenSaver()
{
    FnTrace("DrawScreenSaver()");
    
    // Static variables for DVD-style bouncing animation
    static float text_x = -1.0f;  // Current X position (-1 = uninitialized)
    static float text_y = -1.0f;  // Current Y position
    static float vel_x = 4.0f;    // X velocity (pixels per frame)
    static float vel_y = 3.0f;    // Y velocity (pixels per frame)
    
    // Check if reset was requested
    if (g_reset_screensaver)
    {
        text_x = -1.0f;  // Mark for re-initialization
        text_y = -1.0f;
        g_reset_screensaver = false;
    }

    ShowCursor(CURSOR_BLANK);
    Layers.SetScreenBlanker(1);
    Layers.SetScreenImage(1);  // Enable continuous animation
    XSetTSOrigin(Dis, Gfx, 0, 0);
    XSetForeground(Dis, Gfx, ColorBlack);
    XSetFillStyle(Dis, Gfx, FillSolid);
    XFillRectangle(Dis, MainWin, Gfx, 0, 0, WinWidth, WinHeight);
    
    // Draw "ViewTouch 35 Years In Point Of Sales" bouncing around screen
    const char* text = "ViewTouch 35 Years In Point Of Sales";
    int text_len = strlen(text);
    
    // Use a large, elegant font for the screensaver text
    XftFont *font = GetXftFontInfo(FONT_TIMES_34B);
    if (font)
    {
        // Get text extents for collision detection
        XGlyphInfo extents;
        XftTextExtentsUtf8(Dis, font, reinterpret_cast<const FcChar8*>(text), text_len, &extents);
        
        int text_width = extents.width;
        int text_height = font->ascent + font->descent;
        
        // Initialize position on first call (center of screen)
        if (text_x < 0)
        {
            text_x = (WinWidth - text_width) / 2.0f;
            text_y = (WinHeight - text_height) / 2.0f;
        }
        
        // Update position
        text_x += vel_x;
        text_y += vel_y;
        
        // Bounce off edges (like DVD logo)
        if (text_x <= 0 || text_x + text_width >= WinWidth)
        {
            vel_x = -vel_x;
            // Clamp position to stay within bounds
            if (text_x < 0) text_x = 0;
            if (text_x + text_width > WinWidth) text_x = WinWidth - text_width;
        }
        
        if (text_y <= 0 || text_y + text_height >= WinHeight)
        {
            vel_y = -vel_y;
            // Clamp position to stay within bounds
            if (text_y < 0) text_y = 0;
            if (text_y + text_height > WinHeight) text_y = WinHeight - text_height;
        }
        
        // Create XftDraw context for MainWin
        XftDraw *xftdraw = XftDrawCreate(Dis, MainWin, 
                                          DefaultVisual(Dis, ScrNo), 
                                          DefaultColormap(Dis, ScrNo));
        if (xftdraw)
        {
            // Set up white color for text
            XRenderColor render_color;
            render_color.red   = 0xFFFF;
            render_color.green = 0xFFFF;
            render_color.blue  = 0xFFFF;
            render_color.alpha = 0xFFFF;
            
            // Draw the text at current bouncing position
            int draw_x = static_cast<int>(text_x);
            int draw_y = static_cast<int>(text_y) + font->ascent;
            
            GenericDrawStringXftAntialiased(Dis, MainWin, xftdraw, font, &render_color, 
                                           draw_x, draw_y, text, text_len, ScrNo);
            
            XftDrawDestroy(xftdraw);
        }
    }
    
    return 0;
}

/****
 *  ResetScreenSaver: Reset bouncing text position to center of screen
 ****/
void ResetScreenSaver()
{
    FnTrace("ResetScreenSaver()");
    // Access the static variables in DrawScreenSaver by calling it with a special flag
    // Actually, we'll just use a global variable to signal reset
    extern bool g_reset_screensaver;
    g_reset_screensaver = true;
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
        ResetScreenSaver();  // Reset position for next time
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
        // Initialize enhanced touchscreen features
        InitializeTouchScreen();
        
        TouchInputID = XtAppAddInput(App, 
                                     TScreen->device_no, 
                                     (XtPointer) XtInputReadMask, 
                                     (XtInputCallbackProc) TouchScreenCB, 
                                     NULL);
	}

    return 0;
}

int InitializeTouchScreen()
{
    FnTrace("InitializeTouchScreen()");
    
    if (TScreen == nullptr)
        return -1;
    
    // Load calibration data if available
    TScreen->LoadCalibration("/tmp/viewtouch_touch_calibration.dat");
    
    // Enable enhanced features
    TScreen->SetGesturesEnabled(true);
    TScreen->SetTouchTimeout(500); // 500ms timeout for touch events
    
    // Try to auto-calibrate if no calibration exists
    TouchCalibration cal;
    TScreen->GetCalibration(cal);
    if (!cal.calibrated)
    {
        TScreen->AutoCalibrate();
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

// Detect if running on Raspberry Pi or ARM architecture for performance optimizations
static bool IsRaspberryPi()
{
    static bool checked = false;
    static bool is_pi = false;
    
    if (!checked) {
        // Check /proc/cpuinfo for Raspberry Pi
        FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
        if (cpuinfo) {
            char line[256];
            while (fgets(line, sizeof(line), cpuinfo)) {
                if (strstr(line, "Raspberry Pi") || strstr(line, "BCM") || strstr(line, "Model")) {
                    is_pi = true;
                    break;
                }
            }
            fclose(cpuinfo);
        }
        
        // Also check architecture
        #if defined(__aarch64__) || defined(__arm__)
        is_pi = true;  // Assume Pi for ARM architectures
        #endif
        
        checked = true;
    }
    
    return is_pi;
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
    WinWidth   = Min(MAX_SCREEN_WIDTH, ScrWidth);
    WinHeight  = Min(MAX_SCREEN_HEIGHT, ScrHeight);
    MaxColors  = 13 + (TEXT_COLORS * 3) + ImageColorsUsed();
    TScreen.reset(ts);
    RootWin    = RootWindow(Dis, ScrNo);

    // Load Fonts
    // Use fixed DPI (96) for consistent font rendering across all displays
    // This ensures fonts render at the same size regardless of display DPI
    static char font_spec_with_dpi[256];
    for (const auto& fontData : FontData)
    {
        int f = fontData.id;

        
        FontInfo[f] = GetFont(Dis, display, fontData.font);
        if (FontInfo[f] == nullptr)
            throw FontException("Failed to load font: " + std::string(fontData.font));
        
        // Load Xft font with fixed DPI (96) for consistency across displays
        // Append :dpi=96 to font specification if not already present
        const char* xft_font_name = fontData.font;
        if (strstr(xft_font_name, ":dpi=") == nullptr) {
            snprintf(font_spec_with_dpi, sizeof(font_spec_with_dpi), "%s:dpi=96", xft_font_name);
            xft_font_name = font_spec_with_dpi;
        }
        XftFontsArr[f] = XftFontOpenName(Dis, ScrNo, xft_font_name);
        
        // If Xft font loading failed, try a simple fallback with fixed DPI
        if (XftFontsArr[f] == nullptr) {
            printf("Failed to load Xft font: %s, trying fallback\n", xft_font_name);
            XftFontsArr[f] = XftFontOpenName(Dis, ScrNo, "DejaVu Serif:size=24:style=Book:dpi=96");
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
    
    // Initialize color cache for performance optimization
    // This avoids expensive XQueryColor calls for every text render
    g_color_cache.initialized = false;  // Will be initialized on first use
    
    // Performance optimization: Disable expensive rendering features on Raspberry Pi
    if (IsRaspberryPi()) {
        use_drop_shadows = 0;  // Disable drop shadows on Pi for better performance
        use_embossed_text = 0;  // Disable embossed text on Pi
        shadow_blur_radius = 0;  // Disable blur on Pi
        fprintf(stderr, "Raspberry Pi detected: Disabling expensive rendering features for better performance\n");
    }

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
            vt_safe_string::safe_copy(tmp.data(), tmp.size(), "NCD Explora");
        else if (term_hardware == 2)
            vt_safe_string::safe_copy(tmp.data(), tmp.size(), "NeoStation");
        else
            vt_safe_string::safe_copy(tmp.data(), tmp.size(), "Server");
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
            vt_safe_string::safe_format(str, STRLENGTH, "Can't Create Pixmap #%d On Display '%s'",
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

// Graceful degradation UI functions
static bool reconnect_message_visible = false;
static Window reconnect_window = 0;

void ShowReconnectingMessage()
{
    FnTrace("ShowReconnectingMessage()");

    if (reconnect_message_visible || Dis == NULL) {
        return;
    }

    // Create a semi-transparent overlay window
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.background_pixel = BlackPixel(Dis, ScrNo);
    attrs.border_pixel = BlackPixel(Dis, ScrNo);

    reconnect_window = XCreateWindow(Dis, RootWin,
                                   0, 0, WinWidth, WinHeight, 0,
                                   ScrDepth, InputOutput, ScrVis,
                                   CWOverrideRedirect | CWBackPixel | CWBorderPixel,
                                   &attrs);

    // Make it semi-transparent if possible
    if (ScrVis->c_class == TrueColor) {  // Use c_class instead of class (C++ keyword)
        // Create a semi-transparent background
        XSetForeground(Dis, Gfx, BlackPixel(Dis, ScrNo));
        XFillRectangle(Dis, reconnect_window, Gfx, 0, 0, WinWidth, WinHeight);
    }

    // Draw reconnecting message
    const char* message = "RECONNECTING TO SERVER...";
    int message_len = strlen(message);

    // Calculate text width using XftTextExtentsUtf8
    XGlyphInfo extents;
    XftTextExtentsUtf8(Dis, XftFontsArr[FONT_TIMES_24], reinterpret_cast<const FcChar8*>(message), message_len, &extents);
    int text_width = extents.width;
    int text_height = FontHeight[FONT_TIMES_24];

    int x = (WinWidth - text_width) / 2;
    int y = (WinHeight - text_height) / 2;

    XftColor color;
    color.color.red = 65535;   // Full red
    color.color.green = 65535; // Full green
    color.color.blue = 65535;  // Full blue
    color.color.alpha = 65535; // Opaque

    XftDraw* draw = XftDrawCreate(Dis, reconnect_window, ScrVis, ScrCol);
    if (draw) {
        XftDrawStringUtf8(draw, &color, XftFontsArr[FONT_TIMES_24], x, y + FontBaseline[FONT_TIMES_24],
                         reinterpret_cast<const FcChar8*>(message), message_len);
        XftDrawDestroy(draw);
    }

    XMapRaised(Dis, reconnect_window);
    XFlush(Dis);

    reconnect_message_visible = true;
}

void HideReconnectingMessage()
{
    FnTrace("HideReconnectingMessage()");

    if (!reconnect_message_visible || Dis == NULL) {
        return;
    }

    if (reconnect_window != 0) {
        XDestroyWindow(Dis, reconnect_window);
        reconnect_window = 0;
    }

    // Force a redraw of the main window to restore normal display
    if (MainLayer != NULL) {
        MainLayer->DrawAll();
        XFlush(Dis);
    }

    reconnect_message_visible = false;
}

// Connection state checking functions
bool IsConnectionHealthy()
{
    return connection_monitor.is_healthy();
}

bool IsOfflineMode()
{
    return connection_monitor.get_state() == CONNECTION_FAILED;
}

// Critical fix: Add reconnection and restart functions
int ReconnectToServer()
{
    FnTrace("ReconnectToServer()");

    // Check if we're already trying to reconnect
    if (connection_monitor.get_state() == CONNECTION_RECONNECTING) {
        fprintf(stderr, "ReconnectToServer: Already attempting reconnection\n");
        return 1;
    }

    // Try to reconnect to the server
    struct sockaddr_un server_adr;
    server_adr.sun_family = AF_UNIX;
    vt_safe_string::safe_copy(server_adr.sun_path, sizeof(server_adr.sun_path), "/tmp/vt_term"); // Default socket path

    int new_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (new_socket <= 0)
    {
        fprintf(stderr, "ReconnectToServer: Failed to create socket\n");
        return 1;
    }

    // Set a timeout on the connection attempt
    struct timeval timeout;
    timeout.tv_sec = 10;  // 10 second timeout
    timeout.tv_usec = 0;

    if (setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(new_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        fprintf(stderr, "ReconnectToServer: Failed to set socket timeouts\n");
        close(new_socket);
        return 1;
    }

    if (connect(new_socket, reinterpret_cast<struct sockaddr*>(&server_adr), SUN_LEN(&server_adr)) < 0)
    {
        fprintf(stderr, "ReconnectToServer: Can't connect to server (error %d)\n", errno);
        close(new_socket);
        return 1;
    }

    // Close the old socket if it exists
    if (SocketNo > 0) {
        close(SocketNo);
    }

    // Update the global socket
    SocketNo = new_socket;

    // Clear and reset buffers
    BufferIn.Clear();
    BufferOut.Clear();

    // Re-add the input handler
    SocketInputID = XtAppAddInput(App, SocketNo, (XtPointer) XtInputReadMask,
                                  (XtInputCallbackProc) SocketInputCB, NULL);

    fprintf(stderr, "ReconnectToServer: Successfully reconnected\n");
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
    // Reload fonts with fixed DPI (96) for consistency
    static char font_spec_with_dpi[256];
    for (const auto& fontData : FontData) {
        int f = fontData.id;
        const char* xft_font_name = fontData.font;
        // Append :dpi=96 to font specification if not already present
        if (strstr(xft_font_name, ":dpi=") == nullptr) {
            snprintf(font_spec_with_dpi, sizeof(font_spec_with_dpi), "%s:dpi=96", xft_font_name);
            xft_font_name = font_spec_with_dpi;
        }
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

