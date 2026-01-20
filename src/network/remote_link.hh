/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025, 2026
  
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
 * remote_link.hh - revision 48 (10/6/98)
 * Functions/Protocols for server/terminal/printer communication
 */

#ifndef REMOTE_LINK_HH
#define REMOTE_LINK_HH

#include "basic.hh"

#include <array>
#include <cstring>
#include <string>
#include <vector>

// Reduced from 2MB to 256KB for better performance on memory-constrained 
// systems like Raspberry Pi CM5 with 2GB RAM
inline constexpr size_t QUEUE_SIZE = 262144;

/**** Types ****/
class CharQueue
{
    std::vector<Uchar> buffer;
    int start{0};
    int end{0};
    int code{0};
    std::string name;

    void ReadError(int wanted, int got);
    int Send8(int val);
    int Read8();

public:
    int buffer_size{0}; 
    int send_size{0};
    int size{0};

    explicit CharQueue(int max_size);
    ~CharQueue();
    
    // Delete copy operations
    CharQueue(const CharQueue&) = delete;
    CharQueue& operator=(const CharQueue&) = delete;
    
    // Move operations
    CharQueue(CharQueue&&) noexcept = default;
    CharQueue& operator=(CharQueue&&) noexcept = default;

    void SetCode(const char* new_name, int new_code) noexcept
    {
        if (new_name)
            name = new_name;
        code = new_code;
    }
    
    void Clear() noexcept { size = 0; start = 0; end = 0; }

    int Put8(int val);
    int Get8();

    int Put16(int val);
    int Get16();

    int Put32(int val);
    int Get32();

    long PutLong(long val);
    long GetLong();

    long long PutLLong(long long val);
    long long GetLLong();

    int PutString(const std::string &str, int len);
    int GetString(char* str, size_t max_len);

    int Read(int device_no);
    int Write(int device_no, int do_clear = 1);

    [[nodiscard]] int BuffSize() const noexcept { return buffer_size; }
    [[nodiscard]] int SendSize() const noexcept { return send_size; }
    [[nodiscard]] int CurrSize() const noexcept { return size; }
};


/**** Protocol Formats ****/
// I1  - integer 1 byte  (8 bits)
// I2  - integer 2 bytes (16 bits)
// I4  - integer 4 bytes (32 bits)
// STR - I2 for string length, then string contents

// x, y - coordinate positions (I2, I2)
// w, h - width, height        (I2, I2)
// ap   - appearence type      (I1)
// b    - mouse button code    (I1)
// c    - color                (I1)
// f    - font                 (I2)
// k    - keyboard character   (I1)
// kc   - X key code           (I2)
// l    - length               (I2)
// m    - mode/flags           (I1)
// mc   - mouse code           (I1)
// p1   - pixels               (I1)
// p2   - pixels               (I2)
// pg   - page number          (I2)
// s    - string               (STR)
// sec  - time in seconds      (I2)
// sh   - shape                (I1)
// sz   - size                 (I1)
// t    - texture              (I1)
// ts   - time string          (STR)

/**** Terminal Protocol Constants ****/
// Note: any updates should be applied to debug.cc too
namespace TerminalProtocol {
    inline constexpr int UPDATEALL       = 1;   // no args
    inline constexpr int UPDATEAREA      = 2;   // <x, y, w, h>
    inline constexpr int SETCLIP         = 3;   // <x, y, w, h>
    inline constexpr int BLANKPAGE       = 4;   // <m, t, c, sz, s, ts>
    inline constexpr int BACKGROUND      = 5;   // no args
    inline constexpr int TITLEBAR        = 6;   // <ts>
    inline constexpr int ZONE            = 7;   // <x, y, w, h, ap, t, sh>
    inline constexpr int TEXTL           = 8;   // <s, x, y, c, f, p2>
    inline constexpr int TEXTC           = 9;   // <s, x, y, c, f, p2>
    inline constexpr int TEXTR           = 10;  // <s, x, y, c, f, p2>
    inline constexpr int ZONETEXTL       = 11;  // <s, x, y, w, h, c, f>
    inline constexpr int ZONETEXTC       = 12;  // <s, x, y, w, h, c, f>
    inline constexpr int ZONETEXTR       = 13;  // <s, x, y, w, h, c, f>
    inline constexpr int SHADOW          = 14;  // <x, y, w, h, p2, sh>
    inline constexpr int RECTANGLE       = 15;  // <x, y, w, h, t>
    inline constexpr int HLINE           = 16;  // <x, y, l, c, p1>
    inline constexpr int VLINE           = 17;  // <x, y, l, c, p1>
    inline constexpr int FRAME           = 18;  // <x, y, w, h, p, m>
    inline constexpr int FILLEDFRAME     = 19;  // <x, y, w, h, p, t, m>
    inline constexpr int STATUSBAR       = 20;  // <x, y, w, h, c, s, f, c>
    inline constexpr int EDITCURSOR      = 21;  // <x, y, w, h>
    inline constexpr int CURSOR          = 22;  // <I2> - sets displayed cursor
    inline constexpr int SOLID_RECTANGLE = 23;  // <x, y, w, h, color>
    inline constexpr int PIXMAP          = 25;  // <x, y, w, h, s> - draw pixmap from file path
    inline constexpr int FLUSH           = 26;  // flush commands to X server
    
    inline constexpr int FLUSH_TS        = 30;  // no args
    inline constexpr int CALIBRATE_TS    = 31;  // no args
    inline constexpr int USERINPUT       = 32;  // no args
    inline constexpr int BLANKSCREEN     = 33;  // no args
    inline constexpr int SETMESSAGE      = 34;  // <str>
    inline constexpr int CLEARMESSAGE    = 35;  // no args
    inline constexpr int BLANKTIME       = 36;  // <sec>
    inline constexpr int STORENAME       = 37;  // <str>
    
    inline constexpr int SELECTOFF       = 40;  // no args
    inline constexpr int SELECTUPDATE    = 41;  // <x, y>
    inline constexpr int EDITPAGE        = 42;  // see terminal.cc & term_dialog.cc
    inline constexpr int EDITZONE        = 43;  // see terminal.cc & term_dialog.cc
    inline constexpr int EDITMULTIZONE   = 44;  // see terminal.cc & term_dialog.cc
    inline constexpr int TRANSLATE       = 45;  // <str, str>
    inline constexpr int LISTSTART       = 46;  // see terminal.cc & term_dialog.cc
    inline constexpr int LISTITEM        = 47;  // see terminal.cc
    inline constexpr int LISTEND         = 48;
    inline constexpr int DEFPAGE         = 49;  // see terminal.cc & term_dialog.cc
    
    inline constexpr int NEWWINDOW       = 50;  // <id, x, y, w, h, win_frame, title>
    inline constexpr int SHOWWINDOW      = 51;  // <id>
    inline constexpr int KILLWINDOW      = 52;  // <id>
    inline constexpr int TARGETWINDOW    = 53;  // <id>
    
    inline constexpr int PUSHBUTTON      = 60;  // <id, x, y, w, h, str, font, c1, c2>
    inline constexpr int ITEMLIST        = 61;  // <id, x, y, w, h, label, font, c1, c2>
    inline constexpr int ITEMMENU        = 62;  // <id, x, y, w, h, label, font, c1, c2>
    inline constexpr int TEXTENTRY       = 63;  // <id, x, y, w, h, label, font, c1, c2>
    inline constexpr int CONSOLE         = 64;  // <id, x, y, w, h, c1, c2>
    inline constexpr int PAGEINDEX       = 65;  // <id, x, y, w, h>
    
    inline constexpr int ICONIFY         = 70;  // no args - Iconify display
    inline constexpr int SOUND           = 71;  // <I2:sound id>
    inline constexpr int BELL            = 72;  // <I2:volume -100 to 100>
    inline constexpr int DIE             = 99;  // no args - kills terminal
    inline constexpr int TRANSLATIONS    = 100; // see Terminal::SendTranslations()
    
    inline constexpr int CC_AUTH_CMD     = 150;
    inline constexpr int CC_PREAUTH_CMD  = 151;
    inline constexpr int CC_FINALAUTH_CMD = 152;
    inline constexpr int CC_VOID_CMD     = 153;
    inline constexpr int CC_VOID_CANCEL_CMD = 154;
    inline constexpr int CC_REFUND_CMD   = 155;
    inline constexpr int CC_REFUND_CANCEL_CMD = 156;
    inline constexpr int CC_SETTLE_CMD   = 157;
    inline constexpr int CC_INIT_CMD     = 158;
    inline constexpr int CC_TOTALS_CMD   = 159;
    inline constexpr int CC_DETAILS_CMD  = 160;
    inline constexpr int CC_CLEARSAF_CMD = 161;
    inline constexpr int CC_SAFDETAILS_CMD = 162;
    inline constexpr int CONNTIMEOUT     = 163;
    
    inline constexpr int SET_ICONIFY     = 180;
    inline constexpr int SET_EMBOSSED    = 181;  // <I1> - set embossed text mode (0=off, 1=on)
    inline constexpr int SET_ANTIALIAS   = 182;  // <I1> - set text anti-aliasing mode (0=off, 1=on)
    inline constexpr int SET_DROP_SHADOW = 183;  // <I1> - set drop shadow mode (0=off, 1=on)
    inline constexpr int SET_SHADOW_OFFSET = 184; // <I2> - set shadow offset (x, y)
    inline constexpr int SET_SHADOW_BLUR = 185;  // <I1> - set shadow blur radius (0-10)
}

// Maintain backward compatibility with legacy #define names
#define TERM_UPDATEALL        TerminalProtocol::UPDATEALL
#define TERM_UPDATEAREA       TerminalProtocol::UPDATEAREA
#define TERM_SETCLIP          TerminalProtocol::SETCLIP
#define TERM_BLANKPAGE        TerminalProtocol::BLANKPAGE
#define TERM_BACKGROUND       TerminalProtocol::BACKGROUND
#define TERM_TITLEBAR         TerminalProtocol::TITLEBAR
#define TERM_ZONE             TerminalProtocol::ZONE
#define TERM_TEXTL            TerminalProtocol::TEXTL
#define TERM_TEXTC            TerminalProtocol::TEXTC
#define TERM_TEXTR            TerminalProtocol::TEXTR
#define TERM_ZONETEXTL        TerminalProtocol::ZONETEXTL
#define TERM_ZONETEXTC        TerminalProtocol::ZONETEXTC
#define TERM_ZONETEXTR        TerminalProtocol::ZONETEXTR
#define TERM_SHADOW           TerminalProtocol::SHADOW
#define TERM_RECTANGLE        TerminalProtocol::RECTANGLE
#define TERM_HLINE            TerminalProtocol::HLINE
#define TERM_VLINE            TerminalProtocol::VLINE
#define TERM_FRAME            TerminalProtocol::FRAME
#define TERM_FILLEDFRAME      TerminalProtocol::FILLEDFRAME
#define TERM_STATUSBAR        TerminalProtocol::STATUSBAR
#define TERM_EDITCURSOR       TerminalProtocol::EDITCURSOR
#define TERM_CURSOR           TerminalProtocol::CURSOR
#define TERM_SOLID_RECTANGLE  TerminalProtocol::SOLID_RECTANGLE
#define TERM_PIXMAP           TerminalProtocol::PIXMAP
#define TERM_FLUSH            TerminalProtocol::FLUSH
#define TERM_FLUSH_TS         TerminalProtocol::FLUSH_TS
#define TERM_CALIBRATE_TS     TerminalProtocol::CALIBRATE_TS
#define TERM_USERINPUT        TerminalProtocol::USERINPUT
#define TERM_BLANKSCREEN      TerminalProtocol::BLANKSCREEN
#define TERM_SETMESSAGE       TerminalProtocol::SETMESSAGE
#define TERM_CLEARMESSAGE     TerminalProtocol::CLEARMESSAGE
#define TERM_BLANKTIME        TerminalProtocol::BLANKTIME
#define TERM_STORENAME        TerminalProtocol::STORENAME
#define TERM_SELECTOFF        TerminalProtocol::SELECTOFF
#define TERM_SELECTUPDATE     TerminalProtocol::SELECTUPDATE
#define TERM_EDITPAGE         TerminalProtocol::EDITPAGE
#define TERM_EDITZONE         TerminalProtocol::EDITZONE
#define TERM_EDITMULTIZONE    TerminalProtocol::EDITMULTIZONE
#define TERM_TRANSLATE        TerminalProtocol::TRANSLATE
#define TERM_LISTSTART        TerminalProtocol::LISTSTART
#define TERM_LISTITEM         TerminalProtocol::LISTITEM
#define TERM_LISTEND          TerminalProtocol::LISTEND
#define TERM_DEFPAGE          TerminalProtocol::DEFPAGE
#define TERM_NEWWINDOW        TerminalProtocol::NEWWINDOW
#define TERM_SHOWWINDOW       TerminalProtocol::SHOWWINDOW
#define TERM_KILLWINDOW       TerminalProtocol::KILLWINDOW
#define TERM_TARGETWINDOW     TerminalProtocol::TARGETWINDOW
#define TERM_PUSHBUTTON       TerminalProtocol::PUSHBUTTON
#define TERM_ITEMLIST         TerminalProtocol::ITEMLIST
#define TERM_ITEMMENU         TerminalProtocol::ITEMMENU
#define TERM_TEXTENTRY        TerminalProtocol::TEXTENTRY
#define TERM_CONSOLE          TerminalProtocol::CONSOLE
#define TERM_PAGEINDEX        TerminalProtocol::PAGEINDEX
#define TERM_ICONIFY          TerminalProtocol::ICONIFY
#define TERM_SOUND            TerminalProtocol::SOUND
#define TERM_BELL             TerminalProtocol::BELL
#define TERM_DIE              TerminalProtocol::DIE
#define TERM_TRANSLATIONS     TerminalProtocol::TRANSLATIONS
// Note: TERM_CC_* macros maintained for protocol constants
// (credit.hh defines different CC_* constants for dialog fields - those are separate!)
#define TERM_CC_AUTH          TerminalProtocol::CC_AUTH_CMD
#define TERM_CC_PREAUTH       TerminalProtocol::CC_PREAUTH_CMD
#define TERM_CC_FINALAUTH     TerminalProtocol::CC_FINALAUTH_CMD
#define TERM_CC_VOID          TerminalProtocol::CC_VOID_CMD
#define TERM_CC_VOID_CANCEL   TerminalProtocol::CC_VOID_CANCEL_CMD
#define TERM_CC_REFUND        TerminalProtocol::CC_REFUND_CMD
#define TERM_CC_REFUND_CANCEL TerminalProtocol::CC_REFUND_CANCEL_CMD
#define TERM_CC_SETTLE        TerminalProtocol::CC_SETTLE_CMD
#define TERM_CC_INIT          TerminalProtocol::CC_INIT_CMD
#define TERM_CC_TOTALS        TerminalProtocol::CC_TOTALS_CMD
#define TERM_CC_DETAILS       TerminalProtocol::CC_DETAILS_CMD
#define TERM_CC_CLEARSAF      TerminalProtocol::CC_CLEARSAF_CMD
#define TERM_CC_SAFDETAILS    TerminalProtocol::CC_SAFDETAILS_CMD
#define TERM_CONNTIMEOUT      TerminalProtocol::CONNTIMEOUT
#define TERM_SET_ICONIFY      TerminalProtocol::SET_ICONIFY
#define TERM_SET_EMBOSSED     TerminalProtocol::SET_EMBOSSED
#define TERM_SET_ANTIALIAS    TerminalProtocol::SET_ANTIALIAS
#define TERM_SET_DROP_SHADOW  TerminalProtocol::SET_DROP_SHADOW
#define TERM_SET_SHADOW_OFFSET TerminalProtocol::SET_SHADOW_OFFSET
#define TERM_SET_SHADOW_BLUR  TerminalProtocol::SET_SHADOW_BLUR


/**** Server Protocol Constants ****/
// Note: any updates should be applied to debug.cc too
enum class ServerProtocol : int {
    SrvError           = 1,  // <s>
    SrvTermInfo        = 2,  // <sz, width, height, depth>
    SrvTouch           = 3,  // <I2, x, y>
    SrvKey             = 4,  // <I2, k, kc>
    SrvMouse           = 5,  // <I2, mc, x, y>
    SrvPageData        = 6,  // see term_dialog.cc
    SrvZoneData        = 7,  // see term_dialog.cc
    SrvZoneChanges     = 8,  // see term_dialog.cc
    SrvKillPage        = 9,  // no args
    SrvKillZone        = 10, // no args
    SrvKillZones       = 11, // no args
    SrvTranslate       = 12, // <str, str>
    SrvListSelect      = 13, // see term_dialog.cc
    SrvSwipe           = 14, // <str> - card swiped in card reader
    SrvButtonPress     = 15, // <I2, I2> - layer id, button id
    SrvItemSelect      = 16, // <I2, I2, I2> - layer, menu/list, item
    SrvTextEntry       = 17, // <I2, I2, str> - layer, entry, value
    SrvShutdown        = 18, // no args
    
    SrvPrinterDone     = 20, // <str> - printer done printing file
    SrvBadFile         = 21, // <str> - invalid file given
    SrvDefPage         = 22, // see term_dialog.cc
    
    SrvCcProcessed     = 30, // see Terminal::ReadCreditCard()
    SrvCcSettled       = 31,
    SrvCcInit          = 32,
    SrvCcTotals        = 33,
    SrvCcDetails       = 34,
    SrvCcSafCleared    = 35,
    SrvCcSafDetails    = 36,
    SrvCcSettleFailed  = 37,
    SrvCcSafClearFailed = 38
};

inline constexpr int ToInt(ServerProtocol code) {
    return static_cast<int>(code);
}

/**** Printer Protocol Constants ****/
enum class PrinterProtocol : int {
    File       = 1,  // <str> - specify file to print
    Cancel     = 2,  // no args - cancel current printing task
    OpenDrawer = 3,  // <I1> - open drawer <I1>
    Die        = 99  // no args - kills printer process
};

inline constexpr int ToInt(PrinterProtocol code) {
    return static_cast<int>(code);
}

/*** Mode Constants ***/
enum class OperationMode : int {
    OpNone      = 0,  // normal operation mode
    OpTraining  = 1,  // current user is in training
    OpTranslate = 2,  // edit mode - button translation
    OpEdit      = 3,  // edit mode - application building
    OpMacro     = 5   // record a macro
};

inline constexpr int ToInt(OperationMode mode) {
    return static_cast<int>(mode);
}

/*** Window Frame Constants ***/
enum class WindowFrame : int {
    FrameBorder = 1,  // regular border for window
    FrameTitle  = 2,  // title bar on window
    FrameMove   = 4,  // window can be moved by titlebar
    FrameResize = 8,  // resize window handles on border
    FrameClose  = 16  // close button on window border
};

inline constexpr int ToInt(WindowFrame frame) {
    return static_cast<int>(frame);
}

inline constexpr int operator|(WindowFrame lhs, WindowFrame rhs) {
    return ToInt(lhs) | ToInt(rhs);
}

#endif
