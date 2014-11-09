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
 * printer.hh - revision 40 (2/9/98)
 * Definition of printer device class
 */

#ifndef _PRINTER_HH
#define _PRINTER_HH

#include "utility.hh"

/**** Definitions ****/
#define PRINT_RED       1   // use red ink (if available)
#define PRINT_BOLD      2   // bold text
#define PRINT_UNDERLINE 4   // underline text
#define PRINT_NARROW    8   // condensed width text
#define PRINT_WIDE      16  // print double width only
#define PRINT_TALL      32  // print double height only
#define PRINT_LARGE     48  // print double width & height
#define PRINT_BLUE      64  // blue text (if available)

#define EPSON_WIDE      32
#define EPSON_TALL      16

#define FORM_FEED_LEN   '1' // number of inches, as a char, for the form feed

/*********************************************************************
 * The new style of printing will not use the ports setting
 ********************************************************************/
#define TARGET_TYPE_PARALLEL  "parallel"
#define TARGET_TYPE_LPD       "lpd"
#define TARGET_TYPE_SOCKET    "socket"
#define TARGET_TYPE_FILE      "file"
#define TARGET_TYPE_EMAIL     "email"

enum targettype {
    TARGET_NONE,
    TARGET_PARALLEL,
    TARGET_LPD,
    TARGET_FILE,
    TARGET_EMAIL,
    TARGET_SOCKET
};

enum printer_models {
	MODEL_NONE,
	MODEL_EPSON,
	MODEL_STAR,
	MODEL_HP,
	MODEL_TOSHIBA,
	MODEL_ITHACA,
    MODEL_HTML,
    MODEL_POSTSCRIPT,
    MODEL_PDF,
    MODEL_RECEIPT_TEXT,
    MODEL_REPORT_TEXT
};


/**** Global Data ****/
extern const genericChar *PrinterModelName[];
extern int          PrinterModelValue[];

extern const genericChar *ReceiptPrinterModelName[];
extern int          ReceiptPrinterModelValue[];

extern const genericChar *ReportPrinterModelName[];
extern int          ReportPrinterModelValue[];

extern const genericChar *PortName[];
extern int          PortValue[];


/**** Types ****/
class Control;
class Terminal;

/****
 * class Printer is a base class, not to be instantiated
 ****/
class Printer
{
protected:
    int last_mode;
    int last_color;
    int last_uni;
    int last_uline;
    int last_large;
    int last_narrow;
    int last_bold;
    int temp_fd;          // descriptor for temp file
    Str temp_name;        // name of temp file
    Str target;           // host where printer is located
    int target_type;
    Str host_name;
    int port_no;          // TCP/IP port number
    int active_flags;     // flags always on (print mode)
    int printer_type;     // receipt, report, etc.
    int have_title;
    Str page_title;
    int kitchen_mode;

    virtual int WriteFlags(int flags) = 0;
    int MakeFileName(const genericChar *buffer, const genericChar *source, const genericChar *ext, int max_len);
    int ValidChar(genericChar c);
    int IsDirectory(const genericChar *path);

public:
    Printer *next;
    Printer *fore;
    Control *parent;
    int pulse;
    Str term_name;

    Printer();
    Printer(const genericChar *host, int port, const genericChar *targetstr, int type);
    virtual ~Printer();

    virtual int MatchHost(const genericChar *host, int port);
    virtual int MatchTerminal(const genericChar *termname);
    virtual int SetKitchenMode(int mode);
    virtual int KitchenMode()   { return kitchen_mode; }
    virtual int SetType(int type);
    virtual int IsType(int type);
    virtual int SetTitle(const genericChar *title);
    virtual int Open();
    virtual int Close();
    virtual int ParallelPrint();
    virtual int LPDPrint();
    virtual int SocketPrint();                           // print to TCP socket
    virtual int FilePrint();                             // print to local file
    virtual int GetFilePath(const char *dest);
    virtual int EmailPrint();                            // mail printout to specified address
    virtual int TestPrint(Terminal *t);                  // print out test text message
    virtual int WriteLR(const genericChar *left, const genericChar *right, int flags = 0);
    virtual int Write(const genericChar *string, int flags = 0);      // Prints string & new line
    virtual int Put(const genericChar *string, int flags = 0);        // just prints string
    virtual int Put(genericChar c, int flags = 0);              // Single character print

    virtual int Model() = 0;                             // returns integer specifying model number
    virtual int Init()  = 0;                             // Initializes printer
    virtual int NewLine() = 0;                           // LF & CR
    virtual int LineFeed(int lines = 1) = 0;                 // Skips # lines
    virtual int FormFeed() = 0;                          // Goes to next page
    virtual int MaxWidth() = 0;                          // max carage width of printer
    virtual int MaxLines() = 0;                          // max lines per page (-1 for continuous)
    virtual int Width(int flags = 0) = 0;                // returns width based on flags
    virtual int StopPrint() = 0;                         // Stops printing imediately
    virtual int Start() = 0;                             // Resets printer head position (opens temp file)
    virtual int End() = 0;                               // line feeds & cuts (prints temp file)
    virtual int OpenDrawer(int drawer) = 0;              // Sends pulse to cash drawer
    virtual int CutPaper(int partial_only = 0) = 0;      // Cuts receipt
    virtual void DebugPrint(int printall = 0);
};

class PrinterIthaca : public Printer
{
    virtual int WriteFlags(int flags);
public:
    PrinterIthaca(const genericChar *host, int port, const genericChar *targetstr, int type);
    virtual int Model() { return MODEL_ITHACA; }
    virtual int Start();
    virtual int End();
    virtual int Init();
    virtual int NewLine();
    virtual int LineFeed(int lines = 1);
    virtual int FormFeed();
    virtual int MaxWidth();
    virtual int MaxLines();
    virtual int Width(int flags = 0);
    virtual int StopPrint();
    virtual int OpenDrawer(int drawer);
    virtual int CutPaper(int partial_only = 0);
};

class PrinterStar : public Printer
{
    virtual int WriteFlags(int flags);
public:
    PrinterStar(const genericChar *host, int port, const genericChar *targetstr, int type);
    virtual int Model() { return MODEL_STAR; }
    virtual int Start();
    virtual int End();
    virtual int Init();
    virtual int NewLine();
    virtual int LineFeed(int lines = 1);
    virtual int FormFeed();
    virtual int MaxWidth();
    virtual int MaxLines();
    virtual int Width(int flags = 0);
    virtual int StopPrint();
    virtual int OpenDrawer(int drawer);
    virtual int CutPaper(int partial_only = 0);
};

class PrinterEpson : public Printer
{
    virtual int WriteFlags(int flags);
public:
    PrinterEpson(const genericChar *host, int port, const genericChar *targetstr, int type);
    virtual int Model() { return MODEL_EPSON; }
    virtual int Start();
    virtual int End();
    virtual int Init();
    virtual int NewLine();
    virtual int LineFeed(int lines = 1);
    virtual int FormFeed();
    virtual int MaxWidth();
    virtual int MaxLines();
    virtual int Width(int flags = 0);
    virtual int StopPrint();
    virtual int OpenDrawer(int drawer);
    virtual int CutPaper(int partial_only = 0);
};

class PrinterHP : public Printer
{
    virtual int WriteFlags(int flags);
public:
    PrinterHP(const genericChar *host, int port, const genericChar *targetstr, int type);
    virtual int Model() { return MODEL_HP; }
    virtual int Start();
    virtual int End();
    virtual int Init();
    virtual int NewLine();
    virtual int LineFeed(int lines = 1);
    virtual int FormFeed();
    virtual int MaxWidth();
    virtual int MaxLines();
    virtual int Width(int flags = 0);
    virtual int StopPrint();
    virtual int OpenDrawer(int drawer);
    virtual int CutPaper(int partial_only = 0);
};

class PrinterHTML : public Printer
{
    virtual int WriteFlags(int flags);
public:
    PrinterHTML(const genericChar *host, int port, const genericChar *targetstr, int type);
    virtual int Model() { return MODEL_HTML; }
    virtual int Start();
    virtual int End();
    virtual int Init();
    virtual int LineFeed(int lines = 1);
    virtual int NewLine() { return LineFeed(1); }
    virtual int FormFeed() { return 0; }
    virtual int MaxWidth() { return 80; }
    virtual int MaxLines() { return -1; }
    virtual int Width(int flags = 0) { return 80; }
    virtual int StopPrint() { return 1; }
    virtual int OpenDrawer(int drawer) { return 1; }
    virtual int CutPaper(int partial_only = 0) { return 1; }
};

class PrinterPostScript : public Printer
{
    virtual int WriteFlags(int flags);
    genericChar putbuffer[STRLONG];
    int putbuffidx;
public:
    PrinterPostScript();
    PrinterPostScript(const genericChar *host, int port, const genericChar *targetstr, int type);
    virtual int Model() { return MODEL_POSTSCRIPT; }
    virtual int Start();
    virtual int End();
    virtual int Init();
    virtual int NewLine();
    virtual int LineFeed(int lines = 1);
    virtual int FormFeed();
    virtual int MaxWidth();
    virtual int MaxLines();
    virtual int Width(int flags = 0);
    virtual int StopPrint();
    virtual int OpenDrawer(int drawer);
    virtual int CutPaper(int partial_only = 0);
    virtual int WriteLR(const genericChar *left, const genericChar *right, int flags = 0);
    virtual int Write(const genericChar *string, int flags);
    virtual int Put(const genericChar *string, int flags);
    virtual int Put(genericChar c, int flags);
};

class PrinterPDF : public PrinterPostScript
{
public:
    PrinterPDF();
    PrinterPDF(const genericChar *host, int port, const genericChar *targetstr, int type);
    virtual int Model() { return MODEL_PDF; }
    virtual int Close();
};

class PrinterReceiptText : public Printer
{
    virtual int WriteFlags(int flags) { return 0; }
public:
    PrinterReceiptText();
    PrinterReceiptText(const genericChar *host, int port, const genericChar *targetstr, int type);
    virtual int Model() { return MODEL_RECEIPT_TEXT; }
    virtual int Start();
    virtual int End();
    virtual int Init();
    virtual int LineFeed(int lines = 1);
    virtual int NewLine() { return LineFeed(1); }
    virtual int FormFeed() { return 0; }
    virtual int MaxWidth() { return 40; }
    virtual int MaxLines() { return -1; }
    virtual int Width(int flags = 0) { return 40; }
    virtual int StopPrint() { return 1; }
    virtual int OpenDrawer(int drawer) { return 1; }
    virtual int CutPaper(int partial_only = 0) { return 1; }
};

class PrinterReportText : public PrinterReceiptText
{
public:
    PrinterReportText(const genericChar *host, int port, const genericChar *targetstr, int type);
    virtual int Model() { return MODEL_REPORT_TEXT; }
    virtual int MaxWidth() { return 80; }
    virtual int Width(int flags = 0) { return 80; }
};


Printer *NewPrinterObj(const genericChar *host, int port, int model, int no = 0);
Printer *NewPrinterFromString(const genericChar *specification);

#endif
