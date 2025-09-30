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
 * cdu.cc  Classes and functions for handling CDU communications with various
 *   devices.  Allows for an arbitrary number of closing messages, different
 *   display styles (slide in from left, fade in, etc.), and that sort of thing.
 */

/* bkk -- fbs6 build */
#ifdef BSD
#include "inttypes.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include "cdu.hh"
#include "cdu_att.hh"
#include "utility.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/*********************************************************************
 * class CDUString:  This will hold one CDU message of two or
 *  more lines (the initial version supports only two lines).
 ********************************************************************/
CDUString::CDUString()
{
    int idx;

    id = -1;
    for (idx = 0; idx < MAX_CDU_LINES; idx++)
        lines[idx].Set("");
}

int CDUString::Read(InputDataFile &infile, int version)
{
    FnTrace("CDUString::Read()");
    int idx = 0;

    infile.Read(id);
    for (idx = 0; idx < MAX_CDU_LINES; idx++)
        infile.Read(lines[idx]);
    
    return 0;
}

int CDUString::Write(OutputDataFile &outfile, int version)
{
    FnTrace("CDUString::Write()");
    int idx;

    outfile.Write(id);
    for (idx = 0; idx < MAX_CDU_LINES; idx++)
        outfile.Write(lines[idx]);

    return 0;
}

int CDUString::GetLine(genericChar* dest, int line)
{
    FnTrace("CDUString::GetLine()");
    if (line < 0)
        line = 0;
    else if (line >= MAX_CDU_LINES)
        line = MAX_CDU_LINES - 1;

    strcpy(dest, lines[line].Value());
    return 0;
}

int CDUString::GetLine(Str &dest, int line)
{
    FnTrace("CDUString::GetLine()");
    if (line < 0)
        line = 0;
    else if (line >= MAX_CDU_LINES)
        line = MAX_CDU_LINES - 1;

    dest.Set(lines[line]);
    return 0;
}

int CDUString::SetLine(Str &source, int line)
{
    if (line < 0)
        line = 0;
    else if (line >= MAX_CDU_LINES)
        line = MAX_CDU_LINES - 1;

    lines[line].Set(source);
    return 0;
}

int CDUString::Copy(CDUString *source)
{
    FnTrace("CDUString::Copy()");
    int idx;

    id = source->id;
    for (idx = 0; idx < MAX_CDU_LINES; idx++)
        lines[idx].Set(source->lines[idx]);

    return 0;
}

int CDUString::IsBlank()
{
    FnTrace("CDUString::IsBlank()");
    int retval = 0;
    int idx = 0;

    if (id < 0)
    {
        retval = 1;
    }
    else
    {
        while (idx < MAX_CDU_LINES && retval == 0)
        {
            if (lines[idx].empty())
                retval = 1;
            idx += 1;
        }
    }
    return retval;
}

/*********************************************************************
 * class CDUStrings
 ********************************************************************/
CDUStrings::CDUStrings()
{
}

int CDUStrings::Read(InputDataFile &infile, int version)
{
    FnTrace("CDUStrings::Read()");
    int errval = 0;
    int count;
    int idx;
    CDUString *currString;

    infile.Read(count);
    for (idx = 0; idx < count; idx++)
    {
        currString = NewString();
        currString->Read(infile, version);
    }
    return errval;
}

int CDUStrings::Write(OutputDataFile &outfile, int version)
{
    FnTrace("CDUStrings::Write()");
    int errval = 0;
    CDUString *currString = strings.Head();

    outfile.Write(strings.Count());
    while (currString != NULL)
    {
        currString->Write(outfile, version);
        currString = currString->next;
    }

    return errval;
}

int CDUStrings::Load(const char* path)
{
    FnTrace("CDUStrings::Load()");
    InputDataFile infile;
    int version = 0;
    int result = 0;

    if (path != NULL)
        strncpy(filename, path, STRLONG);

    result = infile.Open(filename, version);
    if (result == 0)
        Read(infile, version);

    return result;
}

int CDUStrings::Save(const char* path)
{
    FnTrace("CDUStrings::Save()");
    OutputDataFile outfile;
    int result = 0;

    if (path != NULL)
        strncpy(filename, path, STRLONG);
    RemoveBlank();

    result = outfile.Open(filename, CDU_VERSION, 0);
    if (result == 0)
        Write(outfile, CDU_VERSION);

    return result;
}

int CDUStrings::RemoveBlank()
{
    FnTrace("CDUStrings::RemoveBlank()");
    CDUString *currString = strings.Head();
    CDUString *prevString = NULL;

    while (currString != NULL)
    {
        if (currString->IsBlank())
        {
            Remove(currString);
            if (prevString != NULL)
                currString = prevString->next;
            else
                currString = strings.Head();
        }
        else
        {
            prevString = currString;
            currString = currString->next;
        }
    }
    
    return 0;
}

int CDUStrings::Remove(CDUString *cdustring)
{
    FnTrace("CDUStrings::Remove()");
    if (cdustring == NULL)
        return 1;

    strings.RemoveSafe(cdustring);
    return 0;
}

CDUString *CDUStrings::GetString(int idx)
{
    FnTrace("CDUStrings::GetString()");
    CDUString *retString = NULL;
    CDUString *currString = strings.Head();
    int record = 0;
    int count = strings.Count();
    int randnum;

    if (idx < 0)
    {
        // generate random index
        randnum = (int)((((double)rand())/RAND_MAX) * count);
        if (randnum < 0)
            randnum = 0;
        else if (randnum > count)
            randnum = count - 1;
        idx = randnum;
    }
    while ((currString != NULL) && (record <= idx))
    {
        if (record == idx)
            retString = currString;
        currString = currString->next;
        record++;
    }
    return retString;
}

CDUString *CDUStrings::FindByRecord(int record)
{
    FnTrace("CDUStrings::FindByRecord()");
    return GetString(record);
}

CDUString *CDUStrings::FindByID(int id)
{
    FnTrace("CDUStrings::FindByID()");
    CDUString *retString = NULL;
    CDUString *currString = strings.Head();

    while (currString != NULL && retString == NULL)
    {
        if (id == currString->id)
            retString = currString;
        currString = currString->next;
    }
    return retString;
}

int CDUStrings::FindRecordByWord(const genericChar* word, int record)
{
    FnTrace("CDUStrings::FindRecordByWord()");
    return -1;
}

CDUString *CDUStrings::NewString()
{
    FnTrace("CDUStrings::NewString()");
    CDUString *newstring = new CDUString();
    CDUString *laststring = strings.Tail();

    if (laststring == NULL)
        newstring->id = 1;
    else
        newstring->id = laststring->id + 1;
    strings.AddToTail(newstring);
    return newstring;
}


/*********************************************************************
 * class CustDispUnit
 ********************************************************************/
CustDispUnit::CustDispUnit()
{
    FnTrace("CustDispUnit::CustDispUnit()");
    port_open   = 0;
    filedes     = -1;
    report      = 0;
    delay       = 0;
    width       = CDU_WIDTH;
    height      = CDU_HEIGHT;
    file_parsed = 0;
    filetype    = 0;
    port        = 0;
    filepath[0] = '\0';
    target[0]   = '\0';
}

CustDispUnit::CustDispUnit(const char* filename)
{
    FnTrace("CustDispUnit::CustDispUnit(const char* )");
    strncpy(filepath, filename, 256);
    port_open   = 0;
    filedes     = -1;
    report      = 0;
    delay       = 0;
    width       = CDU_WIDTH;
    height      = CDU_HEIGHT;
    file_parsed = 0;
    filetype    = 0;
    port        = 0;
    filepath[0] = '\0';
    target[0]   = '\0';
}

CustDispUnit::CustDispUnit(const char* filename, int verbose)
{
    FnTrace("CustDispUnit::CustDispUnit(const char* , int)");
    strncpy(filepath, filename, 256);
    port_open   = 0;
    filedes     = -1;
    report      = verbose;
    delay       = 0;
    width       = CDU_WIDTH;
    height      = CDU_HEIGHT;
    file_parsed = 0;
    filetype    = 0;
    port        = 0;
    filepath[0] = '\0';
    target[0]   = '\0';
}

CustDispUnit::CustDispUnit(const char* filename, int verbose, int allow_delay)
{
    FnTrace("CustDispUnit::CustDispUnit(const char* , int, int)");
    strncpy(filepath, filename, 256);
    port_open   = 0;
    filedes     = -1;
    report      = verbose;
    delay       = allow_delay;
    width       = CDU_WIDTH;
    height      = CDU_HEIGHT;
    file_parsed = 0;
    filetype    = 0;
    port        = 0;
    filepath[0] = '\0';
    target[0]   = '\0';
}

// Destructor
CustDispUnit::~CustDispUnit()
{
}

int CustDispUnit::ParseFileName()
{
    FnTrace("CustDispUnit::ParseFileName()");
    int retval = 0;
    int idx = 0;
    int tidx = 0;
    genericChar buffer[STRLONG];

    while (filepath[idx] != '\0' && filepath[idx] != ':')
        idx += 1;

    if (filepath[idx] == ':')
    {
        strncpy(buffer, filepath, idx);
        buffer[idx] = '\0';
        if (strcmp(buffer, "socket") == 0)
            filetype = CDU_DEV_SOCKET;
        else if (strcmp(buffer, "serial") == 0)
            filetype = CDU_DEV_SERIAL;
        else
        {
            printf("Unknown file type for CDU:  %s\n", buffer);
        }
        idx += 1;
        while (filepath[idx] != '\0' && filepath[idx] != ',')
        {
            target[tidx] = filepath[idx];
            idx += 1;
            tidx += 1;
        }
        target[tidx] = '\0';
        if (filepath[idx] == ',')
            port = atoi(&filepath[idx + 1]);
        else
            port = CDU_PORT;
    }
    else
    { // serial port
        strncpy(target, filepath, 256);
        filetype = CDU_DEV_SERIAL;
        port     = 0;
    }
    
    return retval;
}

int CustDispUnit::SocketOpen()
{
    FnTrace("CustDispUnit::SocketOpen()");
    int sockfd;
    struct hostent *he;
    struct sockaddr_in their_addr; // connector's address information

    he = gethostbyname(target);  // get the host info
    if (he == NULL) {
        perror("gethostbyname");
        return -1;
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }
    
    their_addr.sin_family = AF_INET;    // host byte order
    their_addr.sin_port = htons(port);  // short, network byte order
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(their_addr.sin_zero), '\0', 8);  // zero the rest of the struct
    
    if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1) {
        perror("connect");
        fprintf(stderr, "Is vt_cdu running?\n");
        return -1;
    }
    
    return sockfd;
}

int CustDispUnit::Open()
{
    FnTrace("CustDispUnit::Open()");
    char status[256];
    int retval = -1;
    
    if (file_parsed == 0)
        ParseFileName();

    filedes = -1;
    if (filetype == CDU_DEV_SOCKET)
        filedes = SocketOpen();
    else if (filetype == CDU_DEV_SERIAL)
        filedes = open(target, O_RDWR | O_NOCTTY | O_NDELAY);
    if (filedes < 0)
    {
        snprintf(status, 256, "open_port Error %d opening %s", errno, target);
        ReportError(status);
        port_open = 0;
    }
    else
    { // port is open; set the various attributes
        if (filetype == CDU_DEV_SERIAL)
            SetAttributes(filedes);
        retval = 1;
        port_open = 1;
    }
    return retval;
}

int CustDispUnit::Close()
{
    FnTrace("CustDispUnit::Close()");
    close(filedes);
    filedes = -1;
    port_open = 0;
    return 0;
}

int CustDispUnit::Write(const char* buffer, int len)
{
    FnTrace("CustDispUnit::Write()");
    if (len <= 0)
        len = strlen(buffer);
    int written = 0;

    if (port_open == 0)
        Open();
    if (port_open)
    {
        written = write(filedes, buffer, len);
        if (written < 0)
        {
            perror("Cannot write to CDU");
            port_open = 0;
        }
    }

    return written;
}

int CustDispUnit::Read(char* buffer, int len)
{
    FnTrace("CustDispUnit::Read()");
    int retval = 0;
    char status[256];

    if (port_open == 0)
        Open();

    if (port_open)
    {
        retval = read(filedes, buffer, len);
        if (retval < 0)
        {
            snprintf(status, 256, "Only read %d bytes", retval);
            perror(status);
        }
        else if (report)
        {
            printf("Read %d bytes\n", retval);
        }
    }
    return retval;
}

/****
 * CDURefresh:  Want a method for periodically clearing and/or refreshing the
 *   CDU to prevent burn-in.  When cycles is given, we'll set counter.  As long
 *   as counter is above zero, we'll simply decrement it.  When it hits zero,
 *   we'll clear the CDU.  When it's below zero, we'll ignore it.
 *   The reason for this is that there is a timer function run every half
 *   second or so in manager.cc:  UpdateSystemCB.  We can use that, by having
 *   it call this method, to clear the CDU screen after a period of time to
 *   prevent burn-in.  So we have three cases:
 *   cycles > 0    We'll count down to screen clear
 *   cycles == 0   Clear the screen now
 *   cycles < 0    Never clear the screen
 *
 *   One caveat:  Setting cycles < 0 will not prevent the screen from being cleared.
 *   We still need to prevent burn-in, so while we don't want to clear the screen
 *   while a customer is slowly counting out change, we still want to make sure the
 *   screen gets cleared if the cashier leaves things as they are while the
 *   customer goes home to get a checkbook, stops by a movie theater, etc.
 *   So when cycles < 0, we'll clear the screen when cycles reaches -10000.
 ****/
int CustDispUnit::Refresh(int cycles)
{
    static int counter = 0;
    if (cycles)
        counter = cycles;

    if (counter == 0)
    {
        Clear();
    }
    else if (counter < -10000)
    {
        Clear();
        counter = -1;
    }

    counter -= 1;
    return 0;
}

int CustDispUnit::ShowString(CDUStrings *stringlist, int idx, int style)
{
    FnTrace("CustDispUnit::ShowString()");
    CDUString *cdustring = NULL;
    int retval = 0;
    int randnum;

    cdustring = stringlist->GetString(idx);
    if (cdustring == NULL)
        return 1;
    
    if (style == CDU_STYLE_RANDOM)
    {
        randnum = (int)((((double)rand())/RAND_MAX) * CDU_STYLE_MAX);
        if (randnum < CDU_STYLE_MIN)
            randnum = CDU_STYLE_MIN;
        else if (randnum > CDU_STYLE_MAX)
            randnum = CDU_STYLE_MAX;
        style = randnum;
    }

    // now draw it
    switch (style)
    {
    case CDU_STYLE_SIMPLE:
        retval = Simple(cdustring);
        break;
    case CDU_STYLE_FADEIN:
        retval = FadeIn(cdustring);
        break;
    case CDU_STYLE_LEFTIN:
        retval = SlideLeft(cdustring);
        break;
    case CDU_STYLE_RIGHTIN:
        retval = SlideRight(cdustring);
        break;
    }
    return retval;
}

int CustDispUnit::SlideLeft(CDUString *cdustring)
{
    FnTrace("CustDispUnit::SlideLeft()");
    Simple(cdustring);

    return 0;
}

int CustDispUnit::SlideRight(CDUString *cdustring)
{
    FnTrace("CustDispUnit::SlideRight()");
    Simple(cdustring);

    return 0;
}

int CustDispUnit::FadeIn(CDUString *cdustring)
{
    FnTrace("CustDispUnit::FadeIn()");
    Simple(cdustring);

    return 0;
}

int CustDispUnit::Simple(CDUString *cdustring)
{
    FnTrace("CustDispUnit::Simple()");
    int idx = 0;
    Str line;

    for (idx = 0; idx < MAX_CDU_LINES; idx++)
    {
        cdustring->GetLine(line, idx);
        if (line.size() > 0)
        {
            Write(line.Value());
            if ((line.size() < width) && (idx < (MAX_CDU_LINES - 1)))
                NewLine();
        }
    }
    return 0;
}


/*********************************************************************
 * The Epson Customer Display Unit
 ********************************************************************/

//****  CONTROL CODES  ****
const char BS[]         = { 0x08, 0x00 };
const char ADVANCE[]    = { 0x09, 0x00 };
const char HOME[]       = { 0x0B, 0x00 };
const char CLS[]        = { 0x0C, 0x00 };
const char SELF_TEST[]  = { 0x1F, 0x40, 0x00 };
const char TIMER[]      = { 0x1F, 0x55, 0x00 };
const char CURSOR_ON[]  = { 0x1F, 0x43, 0x01, 0x00 };
const char CURSOR_OFF[] = { 0x1F, 0x43, 0x00, 0x00 };

//****  CONTROL CODES NEEDING REPLACEMENTS  ****
//The following control codes will need all 0xFF entries replaced with
//parameters.
const unsigned char CURSORLOC[]  = { 0x1F, 0x24, 0xFF, 0xFF, 0x00 };
#define CURSOR_COL_POS  2
#define CURSOR_ROW_POS  3
const unsigned char SET_TIMER[]  = { 0x1F, 0x54, 0xFF, 0xFF, 0x00 };
#define TIMER_HR_POS    2
#define TIMER_MN_POS    3
const unsigned char BRIGHTNESS[] = { 0x1F, 0x58, 0xFF, 0x00 };
#define BRIGHT_POS      2

EpsonDispUnit::EpsonDispUnit(const char* filename)
{
    FnTrace("EpsonDispUnit::EpsonDispUnit()");
    strncpy(filepath, filename, 256);
    port_open = 0;
    filedes = -1;
    report = 0;
    delay = 0;
}

EpsonDispUnit::EpsonDispUnit(const char* filename, int verbose)
{
    FnTrace("EpsonDispUnit::EpsonDispUnit(const char* , int)");
    strncpy(filepath, filename, 256);
    port_open = 0;
    filedes = -1;
    report = verbose;
    delay = 0;
}

EpsonDispUnit::EpsonDispUnit(const char* filename, int verbose, int allow_delay)
{
    FnTrace("EpsonDispUnit::EpsonDispUnit(const char* , int, int)");
    strncpy(filepath, filename, 256);
    port_open = 0;
    filedes = -1;
    report = verbose;
    delay = allow_delay;
}

EpsonDispUnit::~EpsonDispUnit()
{
}

int EpsonDispUnit::SetAttributes(int fd)
{
    return EpsonSetAttributes(fd);
}

int EpsonDispUnit::NewLine()
{
    FnTrace("EpsonDispUnit::NewLine()");
    return Write("\r\n");
}

int EpsonDispUnit::Home()
{
    FnTrace("EpsonDispUnit::Home()");
    return Write(HOME);
}

/****
 * ToPos:  Moves the cursor to the specified screen position, assuming
 *   1,1 top left corner.
 ****/
int EpsonDispUnit::ToPos(int x, int y)
{
    FnTrace("EpsonDispUnit::ToPos()");
    char cursorstr[256];

    strncpy(cursorstr, (const char*)CURSORLOC, 256);
    if (x > width)
        x = width;
    else if (x < 0)
        x = width + x;  // subtract x from width (x is negative)
    if (y > height)
        y = height;
    else if (y < 1)
        y = 1;
    cursorstr[CURSOR_COL_POS] = (char)x;
    cursorstr[CURSOR_ROW_POS] = (char)y;
    return Write(cursorstr);
}

int EpsonDispUnit::Clear()
{
    FnTrace("EpsonDispUnit::Clear()");
    Write(CURSOR_OFF, 3);
    return Write(CLS);
}

int EpsonDispUnit::Test()
{
    FnTrace("EpsonDispUnit::Test()");
    return Write(SELF_TEST);
}

int EpsonDispUnit::SetTimer(int hour, int minute)
{
    FnTrace("EpsonDispUnit::SetTimer()");
    char timestr[256];

    if (hour < 0)
    {
        // set to current hour
    }
    if (minute < 0)
    {
        // set to current minute
    }

    strncpy(timestr, (const char*)SET_TIMER, 256);
    while (hour > 23)
        hour -= 24;
    while (minute > 60)
        minute -= 60;
    
    timestr[TIMER_HR_POS] = (char) hour;
    timestr[TIMER_MN_POS] = (char) minute;
    return Write(timestr);
}

int EpsonDispUnit::Timer()
{
    FnTrace("EpsonDispUnit::Timer()");
    return Write(TIMER);
}

int EpsonDispUnit::Brightness(int level)
{
    FnTrace("EpsonDispUnit::Brightness()");
    char brightstr[256];

    strncpy(brightstr, (const char*)BRIGHTNESS, 256);
    if (level > 4)
        level = 4;
    else if (level < 1)
        level = 1;
    brightstr[BRIGHT_POS] = (char)level;
    return Write(brightstr);
}

/*********************************************************************
 * The BA63 Customer Display Unit
 ********************************************************************/

//****  CONTROL CODES  ****
const unsigned char BA63_CLEAR[]    = { 0x1B, 0x5B, 0x32, 0x4A, 0x00 };
const unsigned char BA63_IDENT[]    = { 0x1B, 0x5B, 0x30, 0x63, 0x00 };
const unsigned char BA63_COUNTRY[]  = { 0x1B, 0x52, 0xFF, 0x00 };
#define BA63_COUNTRY_LOC   2
const unsigned char BA63_PLACE0[]   = { 0x1B, 0x5B, 0x48, 0x00 };


BA63DispUnit::BA63DispUnit(const char* filename)
{
    FnTrace("BA63DispUnit::BA63DispUnit()");
    strncpy(filepath, filename, 256);
    port_open = 0;
    filedes = -1;
    report = 0;
    delay = 0;
}

BA63DispUnit::BA63DispUnit(const char* filename, int verbose)
{
    FnTrace("BA63DispUnit::BA63DispUnit(const char* , int)");
    strncpy(filepath, filename, 256);
    port_open = 0;
    filedes = -1;
    report = verbose;
    delay = 0;
}

BA63DispUnit::BA63DispUnit(const char* filename, int verbose, int allow_delay)
{
    FnTrace("BA63DispUnit::BA63DispUnit(const char* , int, int)");
    strncpy(filepath, filename, 256);
    port_open = 0;
    filedes = -1;
    report = verbose;
    delay = allow_delay;
}

BA63DispUnit::~BA63DispUnit()
{
}

int BA63DispUnit::SetAttributes(int fd)
{
    return BA63SetAttributes(fd);
}

int BA63DispUnit::NewLine()
{
    FnTrace("BA63DispUnit::NewLine()");
    return Write("\n");
}

int BA63DispUnit::Home()
{
    FnTrace("BA63DispUnit::Home()");
    return Write((const char*)BA63_PLACE0);
}

/****
 * ToPos:  Moves the cursor to the specified screen position, assuming
 *   1,1 top left corner.
 ****/
int BA63DispUnit::ToPos(int x, int y)
{
    FnTrace("BA63DispUnit::ToPos()");
    char cursorstr[256];

    if (x < 0)
        x = width + x;

    if (x < 2 && y < 2)
        strncpy(cursorstr,(const char*) BA63_PLACE0, 256);
    else
        snprintf(cursorstr, 256, "%c[%d;%dH", 0x1B, y, x);  // ESC[Py;PxH
    return Write(cursorstr);
}

int BA63DispUnit::Clear()
{
    FnTrace("BA63DispUnit::Clear()");
    genericChar buffer[STRLENGTH];

    // We're going to set country code here
    strcpy(buffer, (const char*)BA63_COUNTRY);
    buffer[BA63_COUNTRY_LOC] = 0x00;
    Write(buffer, 3);
    // Now clear the screen and home the cursor
    Write((const char*)BA63_CLEAR);
    Write((const char*)BA63_PLACE0);
    return 0;
}

int BA63DispUnit::Test()
{
    FnTrace("BA63DispUnit::Test()");
    return 1;
}

int BA63DispUnit::SetTimer(int hour, int minute)
{
    FnTrace("BA63DispUnit::SetTimer()");
    return 1;
}

int BA63DispUnit::Timer()
{
    FnTrace("BA63DispUnit::Timer()");
    return 1;
}

int BA63DispUnit::Brightness(int level)
{
    FnTrace("BA63DispUnit::Brightness()");
    return 1;
}

/*********************************************************************
 * GENERAL FUNCTIONS
 ********************************************************************/

CustDispUnit *NewCDUObject(const char* filename, int type)
{
    FnTrace("NewCDUObject()");
    CustDispUnit *CDURetval = NULL;
    if (type == CDU_TYPE_EPSON)
        CDURetval = new EpsonDispUnit(filename);
    if (type == CDU_TYPE_BA63)
        CDURetval = new BA63DispUnit(filename);
    
    return CDURetval;
}
