/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025
  
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
 * report.hh - revision 62 (10/6/98)
 * layout & formating of report information
 */

#ifndef REPORT_HH
#define REPORT_HH

#include "utility.hh"
#include "terminal.hh"
#include "list_utility.hh"

#include <string>
#include <vector>


/**** Definitions ****/
// Report Types
#define REPORT_DRAWER           1
#define REPORT_CLOSEDCHECK      2
#define REPORT_SERVERLABOR      3
#define REPORT_CHECK            7
#define REPORT_SERVER           8
#define REPORT_SALES            9
#define REPORT_BALANCE          10
#define REPORT_DEPOSIT          11
#define REPORT_COMPEXCEPTION    13
#define REPORT_VOIDEXCEPTION    14
#define REPORT_TABLEEXCEPTION   15
#define REPORT_REBUILDEXCEPTION 16
#define REPORT_CUSTOMERDETAIL   17
#define REPORT_EXPENSES         18
#define REPORT_ROYALTY          19
#define REPORT_AUDITING         20
#define REPORT_CREDITCARD       21

// Check Report sorting order (for Kitchen Video)
#define CHECK_ORDER_NEWOLD      0
#define CHECK_ORDER_OLDNEW      1

// Report Printing Options
#define RP_NO_PRINT         0  // don't print this report
#define RP_PRINT_LOCAL      1  // print at local printer
#define RP_PRINT_REPORT     2  // print at report printer
#define RP_ASK              3  // ask user for print destination

// Report Destination Options
#define RP_DEST_EITHER      0
#define RP_DEST_SCREEN      1
#define RP_DEST_PRINTER     2

//FIX BAK->GET THIS OUT OF HERE!!!  It should only be in printer.hh.
// Print Modes (should match printer.hh)
#define PRINT_RED         1   // use red ink (if available)
#define PRINT_BOLD        2   // bold text
#define PRINT_UNDERLINE   4   // underline text
#define PRINT_NARROW      8   // condensed width text
#define PRINT_WIDE        16  // print double width only
#define PRINT_TALL        32  // print double height only
#define PRINT_LARGE       48  // print double width & height
#define PRINT_BLUE        64  // blue text (if available)

// Other
#define MAX_REPORT_COLUMNS  16


/**** Types ****/
class Terminal;
class LayoutZone;
class Report;
class Printer;

class ReportEntry
{
public:
    std::string text;
    float pos = 0.0;
    short max_len = 256;
    short new_lines = 0;
    Uchar color;
    Uchar align;
    Uchar edge;
    Uchar mode;
    bool draw_a_line=false; // draw a line if text char ptr is a nullptr

    // Constructor
    ReportEntry(const char *t, int c, int a, int m);
    ReportEntry(const std::string &t, int c, int a, int m);
    // Destructor
    ~ReportEntry() = default;
};


class Report
{
    std::vector<ReportEntry> header_list;
    std::vector<ReportEntry> body_list;
    std::string report_title;
    int have_title;

public:
    int   destination;
    int   update_flag;
    int   lines_shown;     // caculated page info
    short page;
    short max_pages;
    short current_mode;
    short word_wrap;       // boolean - should word wrap be used when printing?
    short max_width;       // max printing width
    short min_width;       // min width needed for report;
    float header;  // last header used in rendering
    float footer;  // last footer used in rendering
    int   selected_line;
    short add_where;
    short is_complete;     // is report finished?
    // I do not believe page_width is redundant just because of max_width, but
    // I'm not going to crawl through the code completely enough to verify that.
    // I'm just going to add page_width and use it specifically for
    // Text2Col() and its children.
    int   page_width;
    char  div_char;

    // Constructor
    Report();

    // Member Functions

    int  SetTitle(const std::string &title);
    int  SetPageWidth(int pwidth)
    { int retval = pwidth; page_width = pwidth; return retval; }
    char SetDividerChar(char divc = '-')
    { char retval = div_char; div_char = divc; return retval; }
    int  Clear();                     // erases report
    int  Load(const std::string &textfile, int color = COLOR_DEFAULT);        // make report out of text file
    int  Mode(int flags);             // printing mode to use for next entries
    int  Text(const char *t, int c, int a, float indent); // Adds text entry
    int  Text(const std::string &t, int c, int a, float indent); // Adds text entry
    int  Text2Col(const std::string &text, int color, int align, float indent);
    int  Number(int n, int c, int a, float indent); // Adds number entry
    int  Line(int c = COLOR_DEFAULT); // Adds line entry
    int  Underline(int len, int c, int a, float indent);
    int  NewLine(int nl = 1);         // Adds new line
    int  NewPage();                   // Adds page break

    int  Purge();                     // Deletes all entries
    int  PurgeHeader();               // Deletes report header only
    int  Add(const ReportEntry &re);        // Adds entry to report
    int  Append(const Report &r);           // Adds report to end of this one
    int  Render(Terminal *t, LayoutZone *lz, Flt header_size, Flt footer_size,
                int page, int print, Flt spacing = 1.0);
    // Renders all or part of report
    int  CreateHeader(Terminal *t, Printer *p, const Employee *e);
    // Creates standard header
    int  Print(Printer *p);
    // Prints report (1 column, no page breaks)
    int  FormalPrint(Printer *p, int columns = 1);
    // Report printing with multi-columns, etc
    int  PrintEntry(const ReportEntry &re, int start, int width, int max_width,
                    genericChar text[], int mode[]);
    int  TouchLine(Flt spacing, Flt line);
    // returns report line where touch happened
    // also: -1 header or above, -2 footer or below

    int  Divider(char divc = '\0', int dwidth = -1);
    int  Divider2Col(char divc = '\0', int dwidth = -1);

    int Header() { add_where = 1; return 0; }
    int Body()   { add_where = 0; return 0; }
    int Footer() { add_where = 2; return 0; }

    int TextL(const std::string &t, int c = COLOR_DEFAULT)
    { return Text(t, c, ALIGN_LEFT, 0); }
    int TextC(const std::string &t, int c = COLOR_DEFAULT)
    { return Text(t, c, ALIGN_CENTER, 0); }
    int TextR(const std::string &t, int c = COLOR_DEFAULT)
    { return Text(t, c, ALIGN_RIGHT, 0); }

    int TextL2Col(const std::string &t, int c = COLOR_DEFAULT)
    { return Text2Col(t, c, ALIGN_LEFT, 0); }
    int TextC2Col(const std::string &t, int c = COLOR_DEFAULT)
    { return Text2Col(t, c, ALIGN_CENTER, 0); }
    int TextR2Col(const std::string &t, int c = COLOR_DEFAULT)
    { return Text2Col(t, c, ALIGN_RIGHT, 0); }

    int NumberL(int n, int c = COLOR_DEFAULT)
    { return Number(n, c, ALIGN_LEFT, 0); }
    int NumberC(int n, int c = COLOR_DEFAULT)
    { return Number(n, c, ALIGN_CENTER, 0); }
    int NumberR(int n, int c = COLOR_DEFAULT)
    { return Number(n, c, ALIGN_RIGHT, 0); }

    int TextPosL(int pos, const std::string &t, int c = COLOR_DEFAULT)
    { return Text(t, c, ALIGN_LEFT, static_cast<float>(pos)); }
    int TextPosR(int pos, const std::string &t, int c = COLOR_DEFAULT)
    { return Text(t, c, ALIGN_RIGHT, static_cast<float>(pos)); }

    int TextPosL2Col(int pos, const std::string &t, int c = COLOR_DEFAULT)
    { return Text2Col(t, c, ALIGN_LEFT, static_cast<float>(pos)); }
    int TextPosR2Col(int pos, const std::string &t, int c = COLOR_DEFAULT)
    { return Text2Col(t, c, ALIGN_RIGHT, static_cast<float>(pos)); }

    int NumberPosL(int pos, int n, int c = COLOR_DEFAULT)
    { return Number(n, c, ALIGN_LEFT, static_cast<float>(pos)); }
    int NumberPosR(int pos, int n, int c = COLOR_DEFAULT)
    { return Number(n, c, ALIGN_RIGHT, static_cast<float>(pos)); }

    int UnderlinePosL(int pos, int len = 0, int c = COLOR_DEFAULT)
    { return Underline(len, c, ALIGN_LEFT, static_cast<float>(pos)); }
    int UnderlinePosR(int pos, int len = 0, int c = COLOR_DEFAULT)
    { return Underline(len, c, ALIGN_RIGHT, static_cast<float>(pos)); }

};

#endif
