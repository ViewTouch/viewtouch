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
 * cdu.hh  Classes and functions for handling CDU communications with various
 *   devices.  Allows for an arbitrary number of closing messages, different
 *   display styles (slide in from left, fade in, etc.), and that sort of thing.
 */

#ifndef CDU_HH
#define CDU_HH

#include "list_utility.hh"
#include "data_file.hh"

#define CDU_VERSION        1

#define CDU_TYPE_NONE      0
#define CDU_TYPE_EPSON     1
#define CDU_TYPE_BA63      2

#define CDU_DEV_SERIAL     1
#define CDU_DEV_SOCKET     2

#define CDU_STYLE_RANDOM   0
#define CDU_STYLE_SIMPLE   1
#define CDU_STYLE_FADEIN   2
#define CDU_STYLE_LEFTIN   3
#define CDU_STYLE_RIGHTIN  4
#define CDU_STYLE_MIN      1  /* cdu style cannot be lower than this (random doesn't count) */
#define CDU_STYLE_MAX      4  /* cdu style cannot be higher than this */

#define CDU_WIDTH         20
#define CDU_HEIGHT         2
#define MAX_CDU_LINES      CDU_HEIGHT

class CustDispUnit;
class Terminal;

/*--------------------------------------------------------------------
 * CDUString:  Holds one string (actually a 2 line message,
 *   or maybe more lines with some CDUs) along with any information
 *   about how the strings should be displayed.
 *------------------------------------------------------------------*/
class CDUString
{
protected:
    Str             lines[MAX_CDU_LINES];
public:
    CDUString *next;
    CDUString *fore;
    int             id;

    CDUString();
    int             Read(InputDataFile &infile, int version);
    int             Write(OutputDataFile &outfile, int version);
    int             GetLine( genericChar* dest, int line );
    int             GetLine(Str &dest, int line);
    int             SetLine(Str &dest, int line);
    int             Copy(CDUString *source);
    int             IsBlank();
};

/*--------------------------------------------------------------------
 * CDUStrings:  A class for holding any number of messages that
 *   should be displayed to a CDU.  Each item should contain two
 *   short lines (or however many lines the CDU has) and any
 *   information on how the lines should be displayed (e.g. scroll in,
 *   fade in, random in, etc.).
 *------------------------------------------------------------------*/
class CDUStrings
{
protected:
    DList<CDUString> strings;
    genericChar     filename[STRLONG];
public:
    CDUStrings();
    CDUString      *StringList()    { return strings.Head(); }
    CDUString      *StringListEnd() { return strings.Tail(); }
    int             StringCount()   { return strings.Count(); }
    int             Read(InputDataFile &infile, int version);
    int             Write(OutputDataFile &outfile, int version);
    int             Load(const char* path);
    int             Save(const char* path = NULL);
    int             RemoveBlank();
    int             Remove(CDUString *cdustr);
    CDUString      *GetString(int idx = -1);
    CDUString      *FindByRecord(int record);
    CDUString      *FindByID(int id);
    int             FindRecordByWord(const genericChar* word, int record = -1);
    CDUString      *NewString();
};


/*--------------------------------------------------------------------
 * CustDispUnit:  base class for Customer Display Units.
 *------------------------------------------------------------------*/
class CustDispUnit
{
protected:
    int  port_open;
    char file_parsed;
    int  filedes;
    int  filetype;   // serial or socket (1 or 2)
    int  port;       // for socket communications
    char target[256];
    char filepath[256];
    int  report;
    int  delay;
    int  width;
    int  height;
    virtual int SocketOpen();
    virtual int Open();
    virtual int Close();

public:
    CustDispUnit();
    virtual ~CustDispUnit();
    CustDispUnit(const char* filename);
    CustDispUnit(const char* filename, int verbose);
    CustDispUnit(const char* filename, int verbose, int allow_delay);
    virtual int ParseFileName();
    virtual int Write(const char* buffer, int len = -1);
    virtual int Read( char* buffer, int len );
    virtual int Refresh(int cycles = 0);
    virtual int ShowString(CDUStrings *stringlist, int idx = -1, int style = CDU_STYLE_RANDOM);
    virtual int SlideLeft(CDUString *cdustring);
    virtual int SlideRight(CDUString *cdustring);
    virtual int FadeIn(CDUString *cdustring);
    virtual int Simple(CDUString *cdustring);
    virtual int Width()  { return width; }
    virtual int Height() { return height; }

    virtual int Type() = 0;
    virtual int SetAttributes(int fd) = 0;
    virtual int NewLine() = 0;
    virtual int Home() = 0;
    virtual int ToPos(int x, int y) = 0;
    virtual int Clear() = 0;
    virtual int Test() = 0;
    virtual int SetTimer(int hour = -1, int minute = -1) = 0;
    virtual int Timer() = 0;
    virtual int Brightness(int level) = 0;
};

/*--------------------------------------------------------------------
 * EpsonDispUnit:  class for units using the Epson protocol.  This
 *  works, for example, with the IEE devices (or at least the one
 *  we have now).
 *------------------------------------------------------------------*/
class EpsonDispUnit : public CustDispUnit
{
public:
    EpsonDispUnit(const char* filename);
    EpsonDispUnit(const char* filename, int verbose);
    EpsonDispUnit(const char* filename, int verbose, int allow_delay);

    virtual ~EpsonDispUnit();
    
    virtual int Type() { return CDU_TYPE_EPSON; }
    virtual int SetAttributes(int fd);
    virtual int NewLine();
    virtual int Home();
    virtual int ToPos(int x, int y);
    virtual int Clear();
    virtual int Test();
    virtual int SetTimer(int hour = -1, int minute = -1);
    virtual int Timer();
    virtual int Brightness(int level);
};

/*--------------------------------------------------------------------
 * BA63DispUnit:  The devices we have for the Wincor Nixdorf systems.
 *------------------------------------------------------------------*/
class BA63DispUnit : public CustDispUnit
{
public:
    BA63DispUnit(const char* filename);
    BA63DispUnit(const char* filename, int verbose);
    BA63DispUnit(const char* filename, int verbose, int allow_delay);
    virtual ~BA63DispUnit();

    virtual int Type() { return CDU_TYPE_EPSON; }
    virtual int SetAttributes(int fd);
    virtual int NewLine();
    virtual int Home();
    virtual int ToPos(int x, int y);
    virtual int Clear();
    virtual int Test();
    virtual int SetTimer(int hour = -1, int minute = -1);
    virtual int Timer();
    virtual int Brightness(int level);
};

/*--------------------------------------------------------------------
 * Functions
 *------------------------------------------------------------------*/

CustDispUnit *NewCDUObject(const char* filename, int type = 0);

#endif
