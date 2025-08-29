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
 * remote_link.hh - revision 48 (10/6/98)
 * Functions/Protocols for server/terminal/printer communication
 */

#ifndef _REMOTE_LINK_HH
#define _REMOTE_LINK_HH

#include <string.h>
#include "basic.hh"

#include <string>
#include <vector>
#include <array>

#define QUEUE_SIZE 2097152

/**** Types ****/
class CharQueue
{
    std::vector<Uchar> buffer;
    int  start;
    int  end;
    int  code;
    std::string name;

    void ReadError(int wanted, int got);
    int Send8(int val);
    int Read8();

public:
    int buffer_size; 
    int send_size;
	int size;

    CharQueue(int max_size);
    ~CharQueue();

    void SetCode(const char* new_name, int new_code)
    {
        name = new_name;
        code = new_code;
    }
    void Clear() { size = 0, start = 0; end = 0; }

    int  Put8(int val);
    int  Get8();

    int  Put16(int val);
    int  Get16();

    int  Put32(int val);
    int  Get32();

    long PutLong(long val);
    long GetLong();

    long long PutLLong(long long val);
    long long GetLLong();

    int  PutString(const std::string &str, int len);
    int  GetString(char* str);

    int  Read(int device_no);
    int  Write(int device_no, int do_clear = 1);

    int  BuffSize() { return buffer_size; }  // for debugging only
    int  SendSize() { return send_size; }    // ibid
    int  CurrSize() { return size; }         // ibid
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

/**** Terminal Protocols ****/
// any updates should be applied to debug.cc too
#define TERM_UPDATEALL        1   // no args
#define TERM_UPDATEAREA       2   // <x, y, w, h>
#define TERM_SETCLIP          3   // <x, y, w, h>
#define TERM_BLANKPAGE        4   // <m, t, c, sz, s, ts>
#define TERM_BACKGROUND       5   // no args
#define TERM_TITLEBAR         6   // <ts>
#define TERM_ZONE             7   // <x, y, w, h, ap, t, sh>
#define TERM_TEXTL            8   // <s, x, y, c, f, p2>
#define TERM_TEXTC            9   // <s, x, y, c, f, p2>
#define TERM_TEXTR            10  // <s, x, y, c, f, p2>
#define TERM_ZONETEXTL        11  // <s, x, y, w, h, c, f>
#define TERM_ZONETEXTC        12  // <s, x, y, w, h, c, f>
#define TERM_ZONETEXTR        13  // <s, x, y, w, h, c, f>
#define TERM_SHADOW           14  // <x, y, w, h, p2, sh>
#define TERM_RECTANGLE        15  // <x, y, w, h, t>
#define TERM_HLINE            16  // <x, y, l, c, p1>
#define TERM_VLINE            17  // <x, y, l, c, p1>
#define TERM_FRAME            18  // <x, y, w, h, p, m>
#define TERM_FILLEDFRAME      19  // <x, y, w, h, p, t, m>
#define TERM_STATUSBAR        20  // <x, y, w, h, c, s, f, c>
#define TERM_EDITCURSOR       21  // <x, y, w, h>
#define TERM_CURSOR           22  // <I2> - sets displayed cursor
#define TERM_SOLID_RECTANGLE  23  // <x, y, w, h, color>
#define TERM_FLUSH            24  // flush commands to X server

#define TERM_FLUSH_TS         30  // no args
#define TERM_CALIBRATE_TS     31  // no args
#define TERM_USERINPUT        32  // no args
#define TERM_BLANKSCREEN      33  // no args
#define TERM_SETMESSAGE       34  // <str>
#define TERM_CLEARMESSAGE     35  // no args
#define TERM_BLANKTIME        36  // <sec>
#define TERM_STORENAME        37  // <str>

#define TERM_SELECTOFF        40  // no args
#define TERM_SELECTUPDATE     41  // <x, y>
#define TERM_EDITPAGE         42  // see terminal.cc & term_dialog.cc
#define TERM_EDITZONE         43  // see terminal.cc & term_dialog.cc
#define TERM_EDITMULTIZONE    44  // see terminal.cc & term_dialog.cc
#define TERM_TRANSLATE        45  // <str, str>
#define TERM_LISTSTART        46  // see terminal.cc & term_dialog.cc
#define TERM_LISTITEM         47  // see terminal.cc
#define TERM_LISTEND          48
#define TERM_DEFPAGE	      49  // see terminal.cc & term_dialog.cc

#define TERM_NEWWINDOW        50  // <id, x, y, w, h, win_frame, title>
#define TERM_SHOWWINDOW       51  // <id>
#define TERM_KILLWINDOW       52  // <id>
#define TERM_TARGETWINDOW     53  // <id>

#define TERM_PUSHBUTTON       60  // <id, x, y, w, h, str, font, c1, c2>
#define TERM_ITEMLIST         61  // <id, x, y, w, h, label, font, c1, c2>
#define TERM_ITEMMENU         62  // <id, x, y, w, h, label, font, c1, c2>
#define TERM_TEXTENTRY        63  // <id, x, y, w, h, label, font, c1, c2>
#define TERM_CONSOLE          64  // <id, x, y, w, h, c1, c2>
#define TERM_PAGEINDEX        65  // <id, x, y, w, h>

#define TERM_ICONIFY          70  // no args - Iconify display
#define TERM_SOUND            71  // <I2:sound id>
#define TERM_BELL             72  // <I2:volume -100 to 100>
#define TERM_DIE              99  // no args - kills terminal
#define TERM_TRANSLATIONS     100 // see Terminal::SendTranslations()

#define TERM_CC_AUTH          150
#define TERM_CC_PREAUTH       151
#define TERM_CC_FINALAUTH     152
#define TERM_CC_VOID          153
#define TERM_CC_VOID_CANCEL   154
#define TERM_CC_REFUND        155
#define TERM_CC_REFUND_CANCEL 156
#define TERM_CC_SETTLE        157
#define TERM_CC_INIT          158
#define TERM_CC_TOTALS        159
#define TERM_CC_DETAILS       160
#define TERM_CC_CLEARSAF      161
#define TERM_CC_SAFDETAILS    162
#define TERM_CONNTIMEOUT      163

#define TERM_SET_ICONIFY      180
#define TERM_SET_EMBOSSED     181  // <I1> - set embossed text mode (0=off, 1=on)
#define TERM_SET_ANTIALIAS    182  // <I1> - set text anti-aliasing mode (0=off, 1=on)
#define TERM_SET_DROP_SHADOW  183  // <I1> - set drop shadow mode (0=off, 1=on)
#define TERM_SET_SHADOW_OFFSET 184 // <I2> - set shadow offset (x, y)
#define TERM_SET_SHADOW_BLUR  185  // <I1> - set shadow blur radius (0-10)


/**** Server Protocols ****/
// any updates should be applied to debug.cc too
#define SERVER_ERROR              1  // <s>
#define SERVER_TERMINFO           2  // <sz, width, height, depth>
#define SERVER_TOUCH              3  // <I2, x, y>
#define SERVER_KEY                4  // <I2, k, kc>
#define SERVER_MOUSE              5  // <I2, mc, x, y>
#define SERVER_PAGEDATA           6  // see term_dialog.cc
#define SERVER_ZONEDATA           7  // see term_dialog.cc
#define SERVER_ZONECHANGES        8  // see term_dialog.cc
#define SERVER_KILLPAGE           9  // no args
#define SERVER_KILLZONE          10  // no args
#define SERVER_KILLZONES         11  // no args
#define SERVER_TRANSLATE         12  // <str, str>
#define SERVER_LISTSELECT        13  // see term_dialog.cc
#define SERVER_SWIPE             14  // <str>   - card swiped in card reader
#define SERVER_BUTTONPRESS       15  // <I2, I2>      - layer id, button id
#define SERVER_ITEMSELECT        16  // <I2, I2, I2>  - layer, menu/list, item
#define SERVER_TEXTENTRY         17  // <I2, I2, str> - layer, entry, value
#define SERVER_SHUTDOWN          18  // no args

#define SERVER_PRINTER_DONE      20  // <str>   - printer done printing file <str>
#define SERVER_BADFILE           21  // <str>   - invalid file given
#define SERVER_DEFPAGE           22  // see term_dialog.cc

#define SERVER_CC_PROCESSED      30  // see Terminal::ReadCreditCard()
#define SERVER_CC_SETTLED        31
#define SERVER_CC_INIT           32
#define SERVER_CC_TOTALS         33
#define SERVER_CC_DETAILS        34
#define SERVER_CC_SAFCLEARED     35
#define SERVER_CC_SAFDETAILS     36
#define SERVER_CC_SETTLEFAILED   37
#define SERVER_CC_SAFCLEARFAILED 38


/**** Printer Protcols ****/
#define PRINTER_FILE       1  // <str>   - specify file to print
#define PRINTER_CANCEL     2  // no args - cancel current printing task
#define PRINTER_OPENDRAWER 3  // <I1>    - open drawer <I1>
#define PRINTER_DIE        99 // no args - kills printer process

/**** Other Definitions ****/
#define MODE_NONE      0  // normal operation mode
#define MODE_TRAINING  1  // current user is in training
#define MODE_TRANSLATE 2  // edit mode - button translation
#define MODE_EDIT      3  // edit mode - application building
//#define MODE_EXPIRED   4  // license for software has expired // deprecated
#define MODE_MACRO     5  // record a macro

#define WINFRAME_BORDER 1  // regular border for window
#define WINFRAME_TITLE  2  // title bar on window
#define WINFRAME_MOVE   4  // window can be moved by titlebar
#define WINFRAME_RESIZE 8  // resize window handles on border
#define WINFRAME_CLOSE  16 // close button on window border

#endif
