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
 * printer.cc - revision 66 (10/13/98)
 * Implementation of printer device class
 */

#include "manager.hh"
#include "printer.hh"
#include "socket.hh"
#include "settings.hh"
#include "terminal.hh"

#include <errno.h>
#include <iostream>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <cctype>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef BSD
#define PRINTER_PORT "lpt0"
#endif

#ifdef LINUX
#define PRINTER_PORT "lp0"
#endif

#define END_PAGE  8

/**** Global Data ****/
const genericChar* PrinterModelName[] = {
    "No Printer", "Epson", "Star", "HP", "Toshiba", "Ithaca",
    "HTML", "PostScript", "PDF", "Receipt Text",
    "Report Text", NULL};
int          PrinterModelValue[] = {
    MODEL_NONE, MODEL_EPSON, MODEL_STAR, MODEL_HP, MODEL_TOSHIBA, MODEL_ITHACA,
    MODEL_HTML, MODEL_POSTSCRIPT, MODEL_PDF, MODEL_RECEIPT_TEXT,
    MODEL_REPORT_TEXT, -1};

// const genericChar* ReceiptPrinterModelName[] = {
    // "No Printer", "Epson", "Star", "Ithaca", "Text", NULL};
// int          ReceiptPrinterModelValue[] = {
    // MODEL_NONE, MODEL_EPSON, MODEL_STAR, MODEL_ITHACA, MODEL_RECEIPT_TEXT, -1};

const genericChar* ReceiptPrinterModelName[] = {
    "No Printer", "Epson", "Star", "HP", "Toshiba", "Ithaca",
    "HTML", "PostScript", "PDF", "Receipt Text",
    "Report Text", NULL};
int          ReceiptPrinterModelValue[] = {
    MODEL_NONE, MODEL_EPSON, MODEL_STAR, MODEL_HP, MODEL_TOSHIBA, MODEL_ITHACA,
    MODEL_HTML, MODEL_POSTSCRIPT, MODEL_PDF, MODEL_RECEIPT_TEXT,
    MODEL_REPORT_TEXT, -1};

const genericChar* ReportPrinterModelName[] = {
    "No Printer", "HP", "Toshiba", "HTML", "PostScript",
    "PDF", "Text", NULL};
int          ReportPrinterModelValue[] = {
    MODEL_NONE, MODEL_HP, MODEL_TOSHIBA, MODEL_HTML, MODEL_POSTSCRIPT,
    MODEL_PDF, MODEL_REPORT_TEXT, -1};

#define GENERIC_TITLE   "ViewTouch POS Report"  // page title when not otherwise specified

const genericChar* PortName[] = {
    "XCD Parallel", "XCD Serial", "Explora Parallel", "Explora Serial",
    "VT Daemon", "Device On Server", NULL};
int          PortValue[] = {
    PORT_XCD_PARALLEL, PORT_XCD_SERIAL, PORT_EXPLORA_PARALLEL, PORT_EXPLORA_SERIAL,
    PORT_VT_DAEMON, PORT_SERVER_DEVICE, -1};


/*********************************************************************
 * THE NEW PRINTER OBJECT -- These methods should never need to be
 *   subclassed.
 ********************************************************************/

Printer::Printer()
{
    next         = NULL;
    fore         = NULL;
    parent       = NULL;
    last_mode    = 0;
    last_color   = 0;
    last_uni     = 0;
    last_uline   = 0;
    last_large   = 0;
    last_narrow  = 0;
    last_bold    = 0;
    port_no      = 0;
    active_flags = 0;
    temp_fd      = -1;
    temp_name.Set("");
    target.Set("");
    target_type  = TARGET_NONE;
    host_name.Set("");
    port_no      = 0;
    have_title   = 0;
    page_title.Set("");
    kitchen_mode = PRINT_LARGE | PRINT_NARROW;
    pulse        = -1;
    term_name.Set("");
}

Printer::Printer(const genericChar* host, int port, const genericChar* targetstr, int type)
{
    next         = NULL;
    fore         = NULL;
    parent       = NULL;
    last_mode    = 0;
    last_color   = 0;
    last_uni     = 0;
    last_uline   = 0;
    last_large   = 0;
    last_narrow  = 0;
    last_bold    = 0;
    port_no      = 0;
    active_flags = 0;
    temp_fd      = -1;
    temp_name.Set("");
    target.Set(targetstr);
    target_type  = type;
    host_name.Set(host);
    port_no      = port;
    have_title   = 0;
    page_title.Set("");
    kitchen_mode = PRINT_LARGE | PRINT_NARROW;
    pulse        = -1;
    term_name.Set("");
}

Printer::~Printer()
{
}

/****
 * MatchHost:  This is fairly rudimentary.  It only checks the hostname.  I can only
 *  think of one situation in which this should matter:  If there are multiple vt_print
 *  instances on the same server, accessing different printers, listening on different
 *  ports.  But this should not happen as the new code allows for lpd, which will avoid
 *  the problem while also providing more versatility.
 ****/
int Printer::MatchHost(const genericChar* host, int port)
{
    FnTrace("Printer::MatchHost()");
    int retval = 1;

    if (strcmp(host_name.Value(), host))
        retval = 0;

    return retval;
}

int Printer::MatchTerminal(const genericChar* termname)
{
    FnTrace("Printer::MatchTerminal()");
    int retval = 1;

    if (strcmp(term_name.Value(), termname))
        retval = 0;

    return retval;
}

/****
 * SetKitchenMode:
 ****/
int Printer::SetKitchenMode(int mode)
{
    FnTrace("Printer::SetKitchenMode()");
    int retval = kitchen_mode;

    kitchen_mode = mode;
    return retval;
}

/****
 * SetType:  Returns the previous type.
 ****/
int Printer::SetType(int type)
{
    FnTrace("Printer::SetType()");
    int retval = printer_type;

    printer_type = type;
    return retval;
}

int Printer::IsType(int type)
{
    FnTrace("Printer::IsType()");
    int retval = 0;

    if (printer_type == type)
        retval = 1;
    return retval;
}

int Printer::SetTitle(const genericChar* title)
{
    page_title.Set(title);
    have_title = 1;
    return 0;
}

int Printer::Open()
{
    FnTrace("Printer::Open()");
    int retval = 0;
    genericChar filename[STRLENGTH] = "/tmp/viewtouchXXXXXX";

    temp_fd = mkstemp(filename);
    if (filename[0] != '\0')
    {
        temp_name.Set(filename);
    }
    else
    {
        temp_name.Set("");
        temp_fd = -1;
        retval = 1;
    }
    return retval;
}

/****
 * Close:  Need to close out the resulting file and send it off to the
 *   destination.
 ****/
int Printer::Close()
{
    FnTrace("Printer::Close()");

    // assume we're going to do something.  But only close if we're pretty
    // sure we have a valid file handle
    if (temp_fd > 0)
        close(temp_fd);
    switch (target_type)
    {
    case TARGET_PARALLEL:
        ParallelPrint();
        break;
    case TARGET_LPD:
        LPDPrint();
        break;
    case TARGET_SOCKET:
        SocketPrint();
        break;
    case TARGET_FILE:
        FilePrint();
        break;
    case TARGET_EMAIL:
        EmailPrint();
        break;
    }
    // delete the temp file unless printing to the parallel port might still need it
    if (target_type != TARGET_PARALLEL)
        unlink(temp_name.Value());
    temp_name.Set("");
    temp_fd = -1;
    return 0;
}

/****
 * ParallelPrint:  I've been experiencing severe slowdowns with parallel printing.
 *  Apparently, we have to wait until the whole job is done.  So I'm spawning
 *  a child process that will do the printing.  The parent should be able to continue
 *  as if nothing else is going on.  //NOTE this may create timing issues if
 *  multiple print jobs are spooled, so we'll have to handle that situation
 *  (I don't know that I'll worry about order so much as making sure every job
 *  gets through).
 ****/
int Printer::ParallelPrint()
{
    pid_t pid;
    genericChar buffer[STRLENGTH];
    int lockfd;
    int infd;
    int outfd;
    int bytes;
    unsigned char buff[STRLENGTH];
    struct timeval timeout;

    snprintf(buffer, STRLENGTH, "cat %s >>%s", temp_name.Value(), target.Value());

    if (debug_mode)
        printf("Forking for ParallelPrint\n");
    pid = fork();
    if (pid == 0)
    {  // child process, handle the print job
        // Do a loop with a select() call for a brief pause between
        // each write to the parallel port.  On my FreeBSD system,
        // at least, writing to the parallel port seems to lock the
        // entire system (I'm guessing the printer has a very small buffer,
        // and possibly I have something misconfigured).
        lockfd = LockDevice(target.Value());
        if (lockfd > 0)
        {
            infd = open(temp_name.Value(), O_RDONLY);
            outfd = open(target.Value(), O_WRONLY);
            if (infd > 0 && outfd > 0)
            {
                timeout.tv_sec = 0;
                timeout.tv_usec = 100;
                bytes = read(infd, buff, STRLENGTH);
                while (bytes > 0)
                {
                    bytes = write(outfd, buff, bytes);
                    select(0, NULL, NULL, NULL, &timeout);
                    if (bytes > 0)
                        bytes = read(infd, buff, STRLENGTH);
                }
                if (infd > 0)
                    close(infd);
                if (outfd > 0)
                    close(outfd);
            }
            else
            {
                if (infd < 0)
                {
                    snprintf(buffer, STRLENGTH, "ParallelPrint Error %d opening %s for read",
                             errno, temp_name.Value());
                    ReportError(buffer);
                }
                if (outfd < 0)
                {
                    snprintf(buffer, STRLENGTH, "ParallelPrint Error %d opening %s for write",
                             errno, target.Value());
                    ReportError(buffer);
                }
            }
            UnlockDevice(lockfd);
        }
        else
        { // couldn't open one of the files, try cat
            system(buffer);
        }
        unlink(temp_name.Value());
        exit(0);
    }
    else if (pid < 0)
    {  // error, just copy the temp file to the printer without forking
        system(buffer);
        unlink(temp_name.Value());
    }
    temp_name.Set("");
    temp_fd = -1;
    return 0;
}

int Printer::LPDPrint()
{
    FnTrace("Printer::LPDPrint()");
    genericChar buffer[STRLONG];

    snprintf(buffer, STRLONG, "cat %s | /usr/bin/lpr -P%s", temp_name.Value(), target.Value());
    system(buffer);
    return 0;
}

int Printer::SocketPrint()
{
    FnTrace("Printer::SocketPrint()");
    int bytesread;
    int byteswritten = 1; // set it to one to ensure one while loop
    genericChar buffer[STRLENGTH];
    int sockfd;
    struct hostent *he;
    struct sockaddr_in their_addr; // connector's address information

    he = gethostbyname(target.Value());  // get the host info
    if (he == NULL)
    {
        perror("gethostbyname");
        return 1;
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        perror("socket");
        return 1;
    }
    
    their_addr.sin_family = AF_INET;    // host byte order
    their_addr.sin_port = htons(port_no);  // short, network byte order
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(their_addr.sin_zero), '\0', 8);  // zero the rest of the struct
    
    if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("connect");
        fprintf(stderr, "Is vt_print running?\n");
        return 1;
    }
    
    temp_fd = open(temp_name.Value(), O_RDONLY, 0);
    if (temp_fd <= 0)
    {
        close(sockfd);
        snprintf(buffer, STRLENGTH, "SocketPrint Error %d opening %s",
                 errno, temp_name.Value());
        ReportError(buffer);
        return 1;
    }
    
    bytesread = read(temp_fd, buffer, STRLENGTH);
    while (bytesread > 0 && byteswritten > 0)
    {
        byteswritten = write(sockfd, buffer, bytesread);
        bytesread = read(temp_fd, buffer, STRLENGTH);
    }
    close(sockfd);
    return 0;
}

/****
 * ValidChar:  For filenames, we'll only allow 0-9, a-z, and A-Z.
 ****/
int Printer::ValidChar(genericChar c)
{
    FnTrace("Printer::ValidChar()");
    int valid = 0;
    int ch = (int) c;
    
    if (ch >= 48 && ch <= 57)       // 0-9
        valid = 1;
    else if (ch >= 65 && ch <= 90)  // A-Z
        valid = 1;
    else if (ch >= 97 && ch <= 122) // a-z
        valid = 1;
    else if (ch >= 45 && ch <= 46)  // - and .
        valid = 1;
    return valid;
}

/****
 * MakeFileName:  Remove spaces and other characters that could cause problems.
 *   Most modern OSs will handle just about any character fine, but that
 *   doesn't mean we need to beg for trouble.
 ****/
int Printer::MakeFileName(genericChar* buffer, const genericChar* source, const genericChar* ext, int max_len)
{
    FnTrace("Printer::MakeFileName()");
    int len;
    int idx = 0;
    int buffidx = 0;
    TimeInfo now;
    genericChar timestr[STRLENGTH];
    genericChar title[STRLENGTH];

    if (source != NULL)
        strncpy(title, source, STRLENGTH);
    else if (have_title)
        strncpy(title, page_title.Value(), STRLENGTH);
    else
        strncpy(title, GENERIC_TITLE, STRLENGTH);
    len = strlen(title);

    while (idx < len)
    {
        if (ValidChar(title[idx]))
        {
            buffer[buffidx] = title[idx];
            buffidx += 1;
        }
        idx += 1;
    }
    buffer[buffidx] = '\0';

    if (buffidx < max_len)
    {
        // append the date
        now.Set();
        snprintf(timestr, STRLENGTH, "-%02d-%02d-%d", now.Day(), now.Month(), now.Year());
        strcat(buffer, timestr);

        if (ext == NULL)
        {
            // append an extension
            switch (Model())
            {
            case MODEL_HTML:
                strcat(buffer, ".html");
                break;
            case MODEL_POSTSCRIPT:
                strcat(buffer, ".ps");
                break;
            case MODEL_PDF:
                strcat(buffer, ".pdf");
                break;
            case MODEL_RECEIPT_TEXT:
            case MODEL_REPORT_TEXT:
                strcat(buffer, ".txt");
                break;
            default:
                strcat(buffer, ".prn");
                break;
            }
        }
        else
        {
            strcat(buffer, ext);
        }
    }
    return 0;
}

int Printer::IsDirectory(const genericChar* path)
{
    FnTrace("Printer::IsDirectory()");
    struct stat sb;
    int isdir = 0;

    if (lstat(path, &sb) >= 0)
    {
        if (sb.st_mode && S_IFDIR)
            isdir = 1;
    }
    return isdir;
}

/****
 * FilePrint:  If the target specifies a directory, create a filename out of the
 *   page_title, or GENERIC_TITLE if page_title has not been set, and add that
 *   to the target for the full path.
 ****/
int Printer::FilePrint()
{
    FnTrace("Printer::FilePrint()");
    int retval = 0;
    int renval = 0;
    genericChar fullpath[STRLENGTH];
    genericChar command[STRLONG];

    GetFilePath(fullpath);
    snprintf(command, STRLONG, "mv %s %s", temp_name.Value(), fullpath);
    renval = system(command);
    if (renval < 0)
        retval = 1;  // return error
    else
        chmod(fullpath, 0644);
    return retval;
}

int Printer::GetFilePath(char* dest)
{
    FnTrace("Printer::GetFilePath()");
    char filename[STRLENGTH];

    if (IsDirectory(target.Value()))
    {
        MakeFileName(filename, NULL, NULL, STRLENGTH);
        snprintf(dest, STRLENGTH, "%s/%s", target.Value(), filename);
    }
    else
    {
        strncpy(dest, target.Value(), STRLENGTH);
    }

    return 0;
}

/****
 * EmailPrint:  Need to package the printed file into an email using MIME.
 ****/
int Printer::EmailPrint()
{
    FnTrace("Printer::EmailPrint()");
    genericChar buffer[STRLONG];
    int buffidx;
    int bytesread;
    genericChar line[STRLONG];
    int lineidx;
    Email email;
    int fd;
    int sockfd;
    Settings *settings = MasterControl->TermList()->GetSettings();

    if (settings->email_replyto.empty())
    {
        ReportError("No ReplyTo address specified for emails");
        return 1;
    }
    email.AddTo(target.Value());
    email.AddFrom(settings->email_replyto.Value());
    email.AddSubject(page_title.Value());

    // now read in the file and add it to the email
    fd = open(temp_name.Value(), O_RDONLY);
    if (fd > 0)
    {
        lineidx = 0;
        bytesread = read(fd, buffer, STRLONG);
        while (bytesread > 0)
        {
            buffidx = 0;
            while (buffidx < bytesread)
            {
                if (buffer[buffidx] == '\n' && lineidx > 0)
                {
                    line[lineidx] = '\0';
                    email.AddBody(line);
                    lineidx = 0;
                    buffidx += 1;
                }
                else
                {
                    line[lineidx] = buffer[buffidx];
                    buffidx += 1;
                    lineidx += 1;
                }
            }
            bytesread = read(fd, buffer, STRLONG);
        }
        close(fd);
    }
    else if (fd < 0)
    {
        snprintf(buffer, STRLONG, "EmailPrint Error %d opening %s",
                 errno, temp_name.Value());
        ReportError(buffer);
    }

    // Email the file
    sockfd = Connect(settings->email_send_server.Value(), "smtp");
    if (sockfd > 0)
    {
        if (SMTP(sockfd, &email))
            ReportError("Failed to send email");
        close(sockfd);
    }
    return 0;
}

/****
 * TestPrint:  Just print something that will let us know the printer
 *   is working.
 ****/
int Printer::TestPrint(Terminal *t)
{
    FnTrace("Printer::TestPrint()");
    if (Start())
        return 1;

    genericChar str[256];
    NewLine();
    sprintf(str, "\r** %s **\r", t->Translate("Printer Test"));
    Write(str, PRINT_RED | PRINT_BOLD | PRINT_UNDERLINE | PRINT_LARGE | PRINT_NARROW);
    sprintf(str, "Host: %s\r", target.Value());
    Write(str);
    t->TimeDate(str, SystemTime, TD0);
    Write(str);
    return End();
}

int Printer::WriteLR(const genericChar* left, const genericChar* right, int flags)
{
    FnTrace("Printer::WriteLR()");
    if (WriteFlags(flags))
        return 1;

    int  llen  = strlen(left);
    int  rlen  = strlen(right);
    int  width = Width(flags);
    genericChar str[256];

    // FIX - stupid truncate-word wrap for now
    int pos = 0;
    while ((llen + rlen + 1 - pos) > width)
    {
        strncpy(str, &left[pos], width);
        if (write(temp_fd, str, width) < 0)
        {
            genericChar errmsg[STRLONG];
            snprintf(errmsg, STRLONG, "Printer::WriteLR failed while loop printing Left '%s' and Right '%s'", left, right);
            ReportError(errmsg);
            break;
        }
        NewLine();
        pos += width;
    }

    for (int i = 0; i <= width; ++i)
        str[i] = ' ';
    if (pos < llen)
        strncpy(str, &left[pos], llen - pos);
    strcpy(&str[width - rlen], right);

    write(temp_fd, str, width);
    NewLine();
    return 0;
}

int Printer::Write(const genericChar* my_string, int flags)
{
    FnTrace("Printer::Write()");
    if (temp_fd <= 0)
        return 1;

    WriteFlags(flags);
    write(temp_fd, my_string, strlen(my_string));
    NewLine();
    return 0;
}

int Printer::Put(const genericChar* my_string, int flags)
{
    FnTrace("Printer::Put()");
    if (WriteFlags(flags))
        return 1;

    write(temp_fd, my_string, strlen(my_string));
    return 0;
}

int Printer::Put(genericChar c, int flags)
{
    FnTrace("Printer::Put()");
    if (WriteFlags(flags))
        return 1;

    genericChar str[] = {c};
    write(temp_fd, str, sizeof(str));
    return 0;
}

/****
 * DebugPrint:  Exclusively for debugging.
 ****/
void Printer::DebugPrint(int printall)
{
    FnTrace("Printer::DebugPrint()");

    printf("Printer:\n");
    printf("    Temp Name:  %s\n", temp_name.Value());
    printf("    Target:  %s\n", target.Value());
    printf("    Host Name:  %s\n", host_name.Value());
    printf("    Page Title:  %s\n", page_title.Value());
    printf("    Last Mode:  %d\n", last_mode);
    printf("    Last Color:  %d\n", last_color);
    printf("    Last Uni:  %d\n", last_uni);
    printf("    Last Uline:  %d\n", last_uline);
    printf("    Last Large:  %d\n", last_large);
    printf("    Last Narrow:  %d\n", last_narrow);
    printf("    Last Bold:  %d\n", last_bold);
    printf("    Temp FD:  %d\n", temp_fd);
    printf("    Target Type:  %d\n", target_type);
    printf("    Port No:  %d\n", port_no);
    printf("    Active Flags:  %d\n", active_flags);
    printf("    Printer Type:  %d\n", printer_type);
    printf("    Have Title:  %d\n", have_title);
    printf("    Kitchen Mode:  %d\n", kitchen_mode);

    if (printall && next != NULL)
        next->DebugPrint(printall);
}


/*********************************************************************
 * ITHACA PRINTERS
 ********************************************************************/
PrinterIthaca::PrinterIthaca(const genericChar* host, int port, const genericChar* targetstr, int type)
{
    next         = NULL;
    fore         = NULL;
    parent       = NULL;
    last_mode    = 0;
    last_color   = 0;
    last_uni     = 0;
    last_uline   = 0;
    last_large   = 0;
    last_narrow  = 0;
    last_bold    = 0;
    port_no      = 0;
    active_flags = 0;
    temp_fd      = -1;
    temp_name.Set("");
    target.Set(targetstr);
    target_type  = type;
    host_name.Set(host);
    port_no      = port;
    have_title   = 0;
    page_title.Set("");
    pulse        = -1;
    term_name.Set("");
}

int PrinterIthaca::WriteFlags(int flags)
{
    FnTrace("PrinterIthaca::WriteFlags()");
    if (temp_fd <= 0)
        return 1;

    active_flags = flags ^ active_flags;
	genericChar quality_draft[] = { 0x1b, 0x49, 0 }; // 12 x 12
	genericChar quality_high[] = { 0x1b, 0x49, 2 }; // 24x16
	genericChar underline_start[] = { 0x1b, 0x2d, 1};
	genericChar underline_end[] = { 0x1b, 0x2d, 0};
    int mode = 0;

    if (flags & PRINT_UNDERLINE)
		write(temp_fd, underline_start, sizeof(underline_start));
	else  //turn underline off (in case it was turned on previously)
		write(temp_fd, underline_end, sizeof(underline_end));

	if(flags & PRINT_LARGE)
		write(temp_fd, quality_high, sizeof(quality_high));
	else
		write(temp_fd, quality_draft, sizeof(quality_draft));
	last_mode = mode;

    return 0;
}

int PrinterIthaca::Start()
{
    FnTrace("PrinterIthaca::Start()");

    Open();
    if (temp_fd <= 0)
        return 1;
    Init();
    return 0;
}

int PrinterIthaca::End()
{
    FnTrace("PrinterIthaca::End()");
	if (temp_fd <= 0)
		return 1;

    FormFeed();
	Close();
    have_title = 0;
    page_title.Set("");  // reset the title for a new print job
	return 0;
}

int PrinterIthaca::Init()
{
    FnTrace("PrinterIthaca::Init()");

    genericChar init[]          = { 0x1b, 0x40 };
    genericChar clear_buffer[]  = { 0x18 };
    genericChar justify_left[]  = { 0x1b, 0x61, 0 };
    genericChar quality_draft[] = { 0x1b, 0x49, 0 }; // 12 x 12
    genericChar CR[]            = { 0x0d };
    
    //define form feed length as 2 inches
    genericChar formfeed_length[] = { 0x1b, 0x43, FORM_FEED_LEN }; 
    
    write(temp_fd, init, sizeof(init));
    write(temp_fd, clear_buffer, sizeof(clear_buffer));
    write(temp_fd, quality_draft, sizeof(quality_draft));
    write(temp_fd, justify_left, sizeof(justify_left));
    write(temp_fd, formfeed_length, sizeof(formfeed_length));
    write(temp_fd, CR, sizeof(CR));
    last_mode   = 99;
    last_color  = 99;
    last_uni    = 99;
    last_uline  = 99;
    last_large  = 99;
    last_narrow = 99;
    last_bold   = 99;
    return WriteFlags(0);
}

int PrinterIthaca::NewLine()
{
    FnTrace("PrinterIthaca::NewLine()");

    LineFeed(1);
    return 1;
}

int PrinterIthaca::LineFeed(int lines)
{
    FnTrace("PrinterIthaca::LineFeed()");
    if (temp_fd <= 0)
        return 1;

    genericChar endline[] = { 0x0a, 0x0d };
    for(int i=0; i < lines; ++i)
        write(temp_fd, endline, sizeof(endline));
    return 0;
}

int PrinterIthaca::FormFeed()
{
    FnTrace("PrinterIthaca::FormFeed()");
    if (temp_fd <= 0)
        return 1;

    LineFeed(8);
    return 0;
}

int PrinterIthaca::Width(int flags)
{
    FnTrace("PrinterIthaca::Width()");
    return 40;
}

int PrinterIthaca::MaxWidth()
{
    FnTrace("PrinterIthaca::MaxWidth()");
    return 40;
}

int PrinterIthaca::MaxLines()
{
    FnTrace("PrinterIthaca::MaxLines()");
    return 60;
}

int PrinterIthaca::StopPrint()
{
    FnTrace("PrinterIthaca::StopPrint()");
    return 1;
}

int PrinterIthaca::OpenDrawer(int drawer)
{
    FnTrace("PrinterIthaca::OpenDrawer()");
    int close_when_done = 0;
    if (temp_fd <= 0)
    {
        Open();
        close_when_done = 1;
    }
    if (temp_fd <= 0)
        return 1;

    genericChar str[] = { 0x1b, 0x78, '1' };
    write(temp_fd, str, sizeof(str));
    if (close_when_done)
        Close();
    return 0;
}

int PrinterIthaca::CutPaper(int partial_only)
{
    FnTrace("PrinterIthaca::CutPaper()");
    if (temp_fd <= 0)
        return 1;

    genericChar str[] = { 0x0c, 0x1b, 0x76 };
    write(temp_fd, str, sizeof(str));
    return 0;
}


/*********************************************************************
 * STAR PRINTERS
 ********************************************************************/
PrinterStar::PrinterStar(const genericChar* host, int port, const genericChar* targetstr, int type)
{
    next         = NULL;
    fore         = NULL;
    parent       = NULL;
    last_mode    = 0;
    last_color   = 0;
    last_uni     = 0;
    last_uline   = 0;
    last_large   = 0;
    last_narrow  = 0;
    last_bold    = 0;
    port_no      = 0;
    active_flags = 0;
    temp_fd      = -1;
    temp_name.Set("");
    target.Set(targetstr);
    target_type  = type;
    host_name.Set(host);
    port_no      = port;
    have_title   = 0;
    page_title.Set("");
    pulse        = -1;
    term_name.Set("");
}

int PrinterStar::WriteFlags(int flags)
{
    FnTrace("PrinterStar::WriteFlags()");
    active_flags = flags ^ active_flags;
    int uline  = (flags & PRINT_UNDERLINE);
    int large  = (flags & PRINT_LARGE);
    int narrow = (flags & PRINT_NARROW);
    int bold   = (flags & PRINT_BOLD) || (flags & PRINT_LARGE);

    if (uline != last_uline)
    {
        last_uline = uline;
        genericChar str[] = { 0x1b, 0x2d, 0x30 };
        if (uline)
            str[2] = 0x31;
        write(temp_fd, str, sizeof(str));
    }

    if (large != last_large)
    {
        last_large = large;
        if (large)
        {
            genericChar str[] = { 0x0e, 0x1b, 0x68, 0x01, 0x1b, 0x55, 0x01 };
            write(temp_fd, str, sizeof(str));
        }
        else
        {
            genericChar str[] = { 0x14, 0x1b, 0x68, 0x00, 0x1b, 0x55, 0x00 };
            write(temp_fd, str, sizeof(str));
        }
    }

    if (narrow != last_narrow)
    {
        last_narrow = narrow;
        genericChar str[] = { 0x1b, 0x50 };
        if (narrow)
            str[1] = 0x4d;
        write(temp_fd, str, sizeof(str));
    }

    if (bold != last_bold)
    {
        last_bold = bold;
        genericChar str[] = { 0x1b, 0x46 };
        if (bold)
            str[1] = 0x45;
        write(temp_fd, str, sizeof(str));
    }
    return 0;
}

int PrinterStar::Start()
{
    FnTrace("PrinterStar::Start()");
    Open();
    if (temp_fd <= 0)
        return 1;

    Init();
    LineFeed(2);
    return 0;
}

int PrinterStar::End()
{
    FnTrace("PrinterStar::End()");
	if (temp_fd <= 0)
		return 1;

    LineFeed(END_PAGE);
    CutPaper();
	Close();
    have_title = 0;
    page_title.Set("");  // reset the title for a new print job
    return 0;
}

int PrinterStar::Init()
{
    FnTrace("PrinterStar::Init()");
    if (temp_fd <= 0)
        return 1;

    genericChar str[] = { 0x1b, 0x40 };
    write(temp_fd, str, sizeof(str));
    last_mode  = 0;
    last_color = 0;
    last_uni = 0;
    last_uline = 0;
    last_large = 0;
    last_narrow = 0;
    last_bold = 0;
    return WriteFlags(0);
}

int PrinterStar::NewLine()
{
    FnTrace("PrinterStar::NewLine()");

    genericChar str[] = { 0x0a };
    write(temp_fd, str, sizeof(str));
    return 0;
}

int PrinterStar::LineFeed(int lines)
{
    FnTrace("PrinterStar::LineFeed()");
    if (temp_fd <= 0)
        return 1;

    if (lines <= 0)
        return 0;

    for (int i = 0; i < lines; ++i)
        write(temp_fd, "\n", 1);
    return 0;
}

int PrinterStar::FormFeed()
{
    FnTrace("PrinterStar::FormFeed()");
    if (temp_fd <= 0)
        return 1;

    genericChar str[] = { 0x0c };
    write(temp_fd, str, 1);
    return 0;
}

int PrinterStar::Width(int flags)
{
    FnTrace("PrinterStar::Width()");
    int retval = 36;

    if (flags & PRINT_LARGE)
        retval = 23;

    return retval;
}

int PrinterStar::MaxWidth()
{
    FnTrace("PrinterStar::MaxWidth()");
    return 40;
}

int PrinterStar::MaxLines()
{
    FnTrace("PrinterStar::MaxLines()");
    return -1;
}

int PrinterStar::StopPrint()
{
    FnTrace("PrinterStar::StopPrint()");
    return 1;
}

int PrinterStar::OpenDrawer(int drawer)
{
    FnTrace("PrinterStar::OpenDrawer()");
    int close_when_done = 0;
    int d;

    if (temp_fd <= 0)
    {
        Open();
        close_when_done = 1;
    }
    if (temp_fd <= 0)
        return 1;

    if (pulse >= 0)
        d = (pulse % 2);
    else
        d = (drawer % 2);

    genericChar str[] = {0x1c};
    if (d == 1)
        str[0] = 0x1a;
    write(temp_fd, str, sizeof(str));

    if (close_when_done)
        Close();

    return 0;
}

int PrinterStar::CutPaper(int partial_only)
{
    FnTrace("PrinterStar::CutPaper()");
    if (temp_fd <= 0)
        return 1;

    genericChar str[] = { 0x1b, 0x64, 0x0 };
    if (partial_only)
        str[2] = 0x01;
    write(temp_fd, str, sizeof(str));
    return 0;
}


/*********************************************************************
 * EPSON PRINTERS
 ********************************************************************/
PrinterEpson::PrinterEpson(const genericChar* host, int port, const genericChar* targetstr, int type)
{
    next         = NULL;
    fore         = NULL;
    parent       = NULL;
    last_mode    = 0;
    last_color   = 0;
    last_uni     = 0;
    last_uline   = 0;
    last_large   = 0;
    last_narrow  = 0;
    last_bold    = 0;
    port_no      = 0;
    active_flags = 0;
    temp_fd      = -1;
    temp_name.Set("");
    target.Set(targetstr);
    target_type  = type;
    host_name.Set(host);
    port_no      = port;
    have_title   = 0;
    page_title.Set("");
    pulse        = -1;
    term_name.Set("");
}

int PrinterEpson::WriteFlags(int flags)
{
    FnTrace("PrinterEpson::WriteFlags()");
    active_flags = flags ^ active_flags;
    int mode = 0;
    int color = 0;
    int uni = 0;

    if (flags & PRINT_RED)
        color = 1;

    if (flags & PRINT_UNDERLINE)
        mode |= 128;

    if (flags & PRINT_TALL)
        mode |= EPSON_TALL;

    if (flags & PRINT_WIDE)
        mode |= EPSON_WIDE;

    if (flags & PRINT_NARROW)  // subtract wide if it was specified
        mode ^= EPSON_WIDE;

    if (mode != last_mode)
    {
        unsigned char str[] = { 0x1b, 0x21, (unsigned char)mode };
        last_mode = mode;
        write(temp_fd, str, sizeof(str));
    }

    if (color != last_color)
    {
        unsigned char str[] = { 0x1b, 0x72, (unsigned char)color };
        last_color = color;
        write(temp_fd, str, sizeof(str));
    }

    if (uni != last_uni)
    {
        unsigned char str[] = { 0x1b, 0x55, (unsigned char)uni };
        last_uni = uni;
        write(temp_fd, str, sizeof(str));
    }

    return 0;
}

int PrinterEpson::Start()
{
    FnTrace("PrinterEpson::Start()");
    Open();
    if (temp_fd <= 0)
        return 1;

    Init();
    // reset printer head
    genericChar str[] = { 0x1b, 0x3c };
    write(temp_fd, str, sizeof(str));
    return 0;
}

int PrinterEpson::End()
{
    FnTrace("PrinterEpson::End()");
	if (temp_fd <= 0)
		return 1;

    LineFeed(END_PAGE);
    CutPaper();
	Close();
    have_title = 0;
    page_title.Set("");  // reset the title for a new print job
    return 0;
}

int PrinterEpson::Init()
{
    FnTrace("PrinterEpson::Init()");
    if (temp_fd <= 0)
        return 1;

    genericChar str[] = { 0x1b, 0x40, 0x1b, 0x21, 0 };
    write(temp_fd, str, sizeof(str));
    last_mode  = 99;
    last_color = 99;
    last_uni = 99;
    last_uline = 99;
    last_large = 99;
    last_narrow = 99;
    last_bold = 99;
    return WriteFlags(0);
}

int PrinterEpson::NewLine()
{
    FnTrace("PrinterEpson::NewLine()");
    genericChar str[] = { 0x0a };
    write(temp_fd, str, sizeof(str));
    return 0;
}

int PrinterEpson::LineFeed(int lines)
{
    FnTrace("PrinterEpson::LineFeed()");
    if (temp_fd <= 0)
        return 1;

    if (lines <= 0)
        return 0;

    genericChar str[] = { 0x1b, 0x64, (genericChar) (lines & 255) };
    write(temp_fd, str, sizeof(str));
    return 0;
}

int PrinterEpson::FormFeed()
{
    FnTrace("PrinterEpson::FormFeed()");
    if (temp_fd <= 0)
        return 1;

    LineFeed(2);
    return 0;
}

int PrinterEpson::MaxWidth()
{
    FnTrace("PrinterEpson::MaxWidth()");
    return 40;
}

int PrinterEpson::MaxLines()
{
    FnTrace("PrinterEpson::MaxLines()");
    return -1;
}

int PrinterEpson::Width(int flags)
{
    FnTrace("PrinterEpson::Width()");

    if (flags & PRINT_WIDE)
        return 16;
    else
        return 33;

    return 0;
}

int PrinterEpson::StopPrint()
{
    FnTrace("PrinterEpson::StopPrint()");
    return 1;
}

int PrinterEpson::OpenDrawer(int drawer)
{
    FnTrace("PrinterEpson::OpenDrawer()");
    int close_when_done = 0;
    if (temp_fd <= 0)
    {
        Open();
        close_when_done = 1;
    }
    if (temp_fd <= 0)
        return 1;

    genericChar d = 0;
    if (pulse >= 0)
        d = (pulse % 2);
    else
        d = (drawer % 2);
    unsigned char str[] = { 0x1b, 0x70, (unsigned char)d, 100, 255 };
    write(temp_fd, str, sizeof(str));
    if (close_when_done)
        Close();
    return 0;
}

int PrinterEpson::CutPaper(int partial_only)
{
    FnTrace("PrinterEpson::CutPaper()");
    if (temp_fd <= 0)
        return 1;

    genericChar str[] = { 0x1b, 0x69 };
    if (partial_only)
        str[1] = 0x6d;
    write(temp_fd, str, sizeof(str));
    return 0;
}


/*********************************************************************
 * HP PRINTERS
 ********************************************************************/

PrinterHP::PrinterHP(const genericChar* host, int port, const genericChar* targetstr, int type)
{
    next         = NULL;
    fore         = NULL;
    parent       = NULL;
    last_mode    = 0;
    last_color   = 0;
    last_uni     = 0;
    last_uline   = 0;
    last_large   = 0;
    last_narrow  = 0;
    last_bold    = 0;
    port_no      = 0;
    active_flags = 0;
    temp_fd      = -1;
    temp_name.Set("");
    target.Set(targetstr);
    target_type  = type;
    host_name.Set(host);
    port_no      = port;
    have_title   = 0;
    page_title.Set("");
    pulse        = -1;
    term_name.Set("");
}

int PrinterHP::WriteFlags(int flags)
{
    FnTrace("PrinterHP::WriteFlags()");
    active_flags = flags ^ active_flags;
    int uline = (flags & PRINT_UNDERLINE);
    int bold  = (flags & PRINT_BOLD);

    if (uline != last_uline)
    {
        last_uline = uline;
        if (uline)
        {
            genericChar str[] = { 0x1b, 0x26, 0x64, 0x33, 0x44 };
            write(temp_fd, str, sizeof(str));
        }
        else
        {
            genericChar str[] = { 0x1b, 0x26, 0x64, 0x40 };
            write(temp_fd, str, sizeof(str));
        }
    }

    if (bold != last_bold)
    {
        last_bold = bold;
        genericChar str[] = { 0x1b, 0x28, 0x73, 0x30, 0x42 };
        if (bold)
            str[3] = 0x33;
        write(temp_fd, str, sizeof(str));
    }
    return 0;
}

int PrinterHP::Start()
{
    FnTrace("PrinterHP::Start()");
    Open();
    if (temp_fd <= 0)
        return 1;

    Init();
    return 0;
}

int PrinterHP::End()
{
    FnTrace("PrinterHP::End()");
	if (temp_fd <= 0)
		return 1;

    FormFeed();
	Close();
    have_title = 0;
    page_title.Set("");  // reset the title for a new print job
    return 0;
}

int PrinterHP::Init()
{
    FnTrace("PrinterHP::Init()");
    if (temp_fd <= 0)
        return 1;

    genericChar str[] = {
        0x1b, 0x45, 27, 40, 115, '6', 't', '1', '2', 'v', '1', '2', 'H',
        27, 38, 97, '1', '2', 'L', 27, 38, 107, 49, 71};
    write(temp_fd, str, sizeof(str));

    last_mode  = 99;
    last_color = 99;
    last_uni = 99;
    last_uline = 99;
    last_large = 99;
    last_narrow = 99;
    last_bold = 99;
    return WriteFlags(0);
}

int PrinterHP::NewLine()
{
    FnTrace("PrinterHP::NewLine()");
    return LineFeed(1);
}

int PrinterHP::LineFeed(int lines)
{
    FnTrace("PrinterHP::LineFeed()");
    if (temp_fd <= 0)
        return 1;

    if (lines <= 0)
        return 0;

    for (int i = 0; i < lines; ++i)
        write(temp_fd, "\r\n", 2);
    return 0;
}

int PrinterHP::FormFeed()
{
    FnTrace("PrinterHP::FormFeed()");
    LineFeed(2);
    return 0;
}

int PrinterHP::MaxWidth()
{
    FnTrace("PrinterHP::MaxWidth()");
    return 80;
}

int PrinterHP::MaxLines()
{
    FnTrace("PrinterHP::MaxLines()");
    return 60;
}

int PrinterHP::Width(int flags)
{
    FnTrace("PrinterHP::Width()");
    return 80;
}

int PrinterHP::StopPrint()
{
    FnTrace("PrinterHP::StopPrint()");
    return 1;
}

int PrinterHP::OpenDrawer(int drawer)
{
    FnTrace("PrinterHP::OpenDrawer()");
    return 1;
}

int PrinterHP::CutPaper(int partial_only)
{
    FnTrace("PrinterHP::CutPaper()");
    if (temp_fd <= 0)
        return 1;

    // eject paper (no cut available)
    FormFeed();
    return 0;
}


/*********************************************************************
 * HTML
 *
 *  prints to HTML for onscreen viewing as well as email
 *  delivery of reports
 ********************************************************************/
PrinterHTML::PrinterHTML(const genericChar* host, int port, const genericChar* targetstr, int type)
{
    next         = NULL;
    fore         = NULL;
    parent       = NULL;
    last_mode    = 0;
    last_color   = 0;
    last_uni     = 0;
    last_uline   = 0;
    last_large   = 0;
    last_narrow  = 0;
    last_bold    = 0;
    port_no      = 0;
    active_flags = 0;
    temp_fd      = -1;
    temp_name.Set("");
    target.Set(targetstr);
    target_type  = type;
    host_name.Set(host);
    port_no      = port;
    have_title   = 0;
    page_title.Set("");
    pulse        = -1;
    term_name.Set("");
}

int PrinterHTML::WriteFlags(int flags)
{
    FnTrace("PrinterHTML::WriteFlags()");
    static int last_red       = 0;
    static int last_blue      = 0;
    static int last_underline = 0;
    static int last_wide      = 0;
    int large     = (flags & PRINT_LARGE);
    int wide      = (flags & PRINT_WIDE);
    int red       = (flags & PRINT_RED);
    int blue      = (flags & PRINT_BLUE);
    int bold      = (flags & PRINT_BOLD);
    int underline = (flags & PRINT_UNDERLINE);
    genericChar flagstr[STRLENGTH] = "";

    if (last_red != red)
    {
        if (red)
            strcat(flagstr, "<font color=\"red\">");
        else
            strcat(flagstr, "</font>");
    }
    if (last_blue != blue)
    {
        if (blue)
            strcat(flagstr, "<font color=\"blue\">");
        else
            strcat(flagstr, "</font>");
    }
    if (last_bold != bold)
    {
        if (bold)
            strcat(flagstr, "<b>");
        else
            strcat(flagstr, "</b>");
    }
    if (last_underline != underline)
    {
        if (underline)
            strcat(flagstr, "<u>");
        else
            strcat(flagstr, "</u>");
    }
    if (last_large != large || last_wide != wide)
    {
        if (large || wide)
            strcat(flagstr, "<font size=\"+2\">");
        else
            strcat(flagstr, "</font>");
    }
    if (flagstr[0] != '\0')
        write(temp_fd, flagstr, strlen(flagstr));
    last_large = large;
    last_red = red;
    last_blue = blue;
    last_bold = bold;
    last_underline = underline;
    return 0;
}

int PrinterHTML::Start()
{
    FnTrace("PrinterHTML::Start()");
    Open();
    if (temp_fd <= 0)
        return 1;

    Init();
    return 0;
}

int PrinterHTML::End()
{
    FnTrace("PrinterHTML::End()");
    if (temp_fd <= 0)
        return 1;

    Write("</pre>\n</body>\n</html>");
    Close();
    have_title = 0;
    page_title.Set("");  // reset the title for a new print job
    return 0;
}

int PrinterHTML::Init()
{
    FnTrace("PrinterHTML::Init()");
    genericChar buffer[STRLENGTH];

    Write("<html>\n<head>", 0);
    if (have_title == 0)
        page_title.Set(GENERIC_TITLE);
    snprintf(buffer, STRLENGTH, "<title>%s</title>", page_title.Value());
    Write(buffer, 0);
    Write("</head>", 0);
    Write("<body>", 0);
    Write("<pre>", 0);
    return 0;
}

int PrinterHTML::LineFeed(int lines)
{
    FnTrace("PrinterHTML::LineFeed()");
    if (temp_fd <= 0)
        return 1;

    genericChar linefeed[] = "\n";

    while (lines > 0)
    {
        if (write(temp_fd, linefeed, strlen(linefeed)) < 0)
            break;
        lines -= 1;
    }
    return 0;
}


/*********************************************************************
 * POSTSCRIPT
 *
 *  an object that prints to postscript files for onscreen or
 *  offscreen viewing or conversion to PDF
 ********************************************************************/
PrinterPostScript::PrinterPostScript()
{
    next         = NULL;
    fore         = NULL;
    parent       = NULL;
    last_mode    = 0;
    last_color   = 0;
    last_uni     = 0;
    last_uline   = 0;
    last_large   = 0;
    last_narrow  = 0;
    last_bold    = 0;
    port_no      = 0;
    active_flags = 0;
    temp_fd      = -1;
    temp_name.Set("");
    target.Set("");
    target_type  = TARGET_NONE;
    host_name.Set("");
    port_no      = 0;
    have_title   = 0;
    page_title.Set("");
    putbuffer[0] = '\0';
    putbuffidx   = 0;
    pulse        = -1;
    term_name.Set("");
}

PrinterPostScript::PrinterPostScript(const genericChar* host, int port, const genericChar* targetstr, int type)
{
    next         = NULL;
    fore         = NULL;
    parent       = NULL;
    last_mode    = 0;
    last_color   = 0;
    last_uni     = 0;
    last_uline   = 0;
    last_large   = 0;
    last_narrow  = 0;
    last_bold    = 0;
    port_no      = 0;
    active_flags = 0;
    temp_fd      = -1;
    temp_name.Set("");
    target.Set(targetstr);
    target_type  = type;
    host_name.Set(host);
    port_no      = port;
    have_title   = 0;
    page_title.Set("");
    putbuffer[0] = '\0';
    putbuffidx   = 0;
    pulse        = -1;
    term_name.Set("");
}

int PrinterPostScript::WriteFlags(int flags)
{
    FnTrace("PrinterPostScript::WriteFlags()");
    static int last_red       = 0;
    static int last_underline = 0;
    int large     = (flags & PRINT_LARGE);
    int red       = (flags & PRINT_RED);
    int bold      = (flags & PRINT_BOLD);
    int underline = (flags & PRINT_UNDERLINE);
    genericChar flagstr[STRLENGTH] = "";

    if (last_large != large)
    {
    }
    if (last_red != red)
    {
    }
    if (last_bold != bold)
    {
    }
    if (last_underline != underline)
    {
    }

    if (flagstr[0] != '\0')
        write(temp_fd, flagstr, strlen(flagstr));

    last_large = large;
    last_red = red;
    last_bold = bold;
    last_underline = underline;
    return 0;
}

int PrinterPostScript::Start()
{
    FnTrace("PrinterPostScript::Start()");
    Open();
    if (temp_fd <= 0)
        return 1;

    Init();
    return 0;
}

int PrinterPostScript::End()
{
    FnTrace("PrinterPostScript::End()");
    if (temp_fd <= 0)
        return 1;

    genericChar showstring[] = "showpage";
    write(temp_fd, showstring, strlen(showstring));

    Close();
    have_title = 0;
    page_title.Set("");  // reset the title for a new print job
    return 0;
}

int PrinterPostScript::Init()
{
    FnTrace("PrinterPostScript::Init()");
    genericChar buffer[STRLENGTH];
    int idx = 0;
    const genericChar* lines[STRLENGTH] = {
        "%!PS-Adobe-2.0\n",
            "/TitleFontDict /Times-Roman findfont 16 scalefont def\n",
            "/TitleFont { TitleFontDict setfont } def\n",
            "/RegularFontDict /Courier findfont 10 scalefont def\n",
            "/RegularFont { RegularFontDict setfont } def\n",
            "/BoldFontDict /Courier-Bold findfont 10 scalefont def\n",
            "/BoldFont { BoldFontDict setfont } def\n",
            "/ItalicFontDict /Courier-Oblique findfont 10 scalefont def\n",
            "/ItalicFont { ItalicFontDict setfont } def\n",
            "/inch {72 mul} def\n",
            "/PageWidth 8.5 inch def\n",
            "/PageHeigth 11 inch def\n",
            "/vpos 10 inch def\n",
            "/hpos 1 inch def\n",
            "/hmargin hpos def\n",
            "/NewLine {\n",
            "/vpos vpos 12 sub def\n",
            "hpos vpos moveto\n",
            "} def\n",
            "/ShowTitleText { % text\n",
            "gsave\n",
            "TitleFont\n",
            "% need to get centered on the page\n",
            "dup stringwidth pop\n",
            "PageWidth exch sub 2 div % leave ((PageWidth - StringWidth) / 2) on stack\n",
            "/hpos exch def  % put stack contents into hpos\n",
            "hpos vpos moveto  % move there\n",
            "show\n",
            "grestore\n",
            "/hpos hmargin def\n",
            "NewLine\n",
            "NewLine\n",
            "} def\n",
            "/ShowText { % text\n",
            "show\n",
            "NewLine\n",
            "} def\n",
            "/NewPage {\n",
            "/vpos 10 inch def\n",
            "/hpos 1 inch def\n",
            "hpos vpos moveto\n",
            "RegularFont\n",
            "} def\n",
            "NewPage\n",
            NULL
            };
    
    if (temp_fd <= 0)
        return 1;

    while (lines[idx] != NULL)
    {
        if (write(temp_fd, lines[idx], strlen(lines[idx])) < 0)
            break;
        idx += 1;
    }
    if (have_title == 0)
        page_title.Set(GENERIC_TITLE);
    snprintf(buffer, STRLENGTH, "(%s) ShowTitleText\n", page_title.Value());
    write(temp_fd, buffer, strlen(buffer));
    return 0;
}

int PrinterPostScript::NewLine()
{
    FnTrace("PrinterPostScript::NewLine()");
    if (temp_fd <= 0)
        return 1;

    const genericChar* newlinestr = "\n";

    if (putbuffer[0] != '\0')
        Put('\0', -1);
    write(temp_fd, newlinestr, strlen(newlinestr));
    return 0;
}

int PrinterPostScript::LineFeed(int lines)
{
    FnTrace("PrinterPostScript::LineFeed()");
    if (temp_fd <= 0)
        return 1;

    while ( lines > 0 )
    {
        NewLine();
        lines -= 1;
    }
    return 0;
}

int PrinterPostScript::FormFeed()
{
    FnTrace("PrinterPostScript::FormFeed()");
    genericChar showpage[] = "showpage\n";
    genericChar newpage[] = "NewPage\n";

    write(temp_fd, showpage, strlen(showpage));
    write(temp_fd, newpage, strlen(newpage));
    return 0;
}

int PrinterPostScript::MaxWidth()
{
    FnTrace("PrinterPostScript::MaxWidth()");
    return 80;
}

int PrinterPostScript::MaxLines()
{
    FnTrace("PrinterPostScript::MaxLines()");
    return 60;
}

int PrinterPostScript::Width(int flags)
{
    FnTrace("PrinterPostScript::Width()");
    return 80;
}

int PrinterPostScript::StopPrint()
{
    FnTrace("PrinterPostScript::StopPrint()");
    return 1;
}

int PrinterPostScript::OpenDrawer(int drawer)
{
    FnTrace("PrinterPostScript::OpenDrawer()");
    return 1;
}

int PrinterPostScript::CutPaper(int partial_only)
{
    FnTrace("PrinterPostScript::CutPaper()");
    return 1;
}

int PrinterPostScript::WriteLR(const genericChar* left, const genericChar* right, int flags)
{
    FnTrace("PrinterPostScript::WriteLR()");
    if (WriteFlags(flags))
        return 1;

    int  llen  = strlen(left);
    int  rlen  = strlen(right);
    int  width = Width(flags);
    genericChar str[256];

    // FIX - stupid truncate-word wrap for now
    int pos = 0;
    while ((llen + rlen + 1 - pos) > width)
    {
        strncpy(str, &left[pos], width);
        if (write(temp_fd, str, width) < 0)
        {
            genericChar errmsg[STRLONG];
            snprintf(errmsg, STRLONG, "PrinterPostScript::WriteLR failed while loop printing Left '%s' and Right '%s'\n", left, right);
            ReportError(errmsg);
            break;
        }
        NewLine();
        pos += width;
    }

    for (int i = 0; i <= width; ++i)
        str[i] = ' ';
    if (pos < llen)
        strncpy(str, &left[pos], llen - pos);
    strcpy(&str[width - rlen], right);

    write(temp_fd, str, width);
    NewLine();
    return 0;
}

int PrinterPostScript::Write(const genericChar* my_string, int flags)
{
    FnTrace("PrinterPostScript::Write()");
    genericChar buffer[STRLONG];
    genericChar outstr[STRLONG];
    int idx = 0;
    int buffidx = 0;
    int len = strlen(my_string);
    if (temp_fd <= 0)
        return 1;

    while (idx < len)
    {
        if (my_string[idx] == '(' || my_string[idx] == ')')
        {
            buffer[buffidx] = '\\';
            buffidx += 1;
        }
        buffer[buffidx] = my_string[idx];
        idx += 1;
        buffidx += 1;
    }
    WriteFlags(flags);
    snprintf(outstr, STRLONG, "(%s) ShowText", buffer);
    write(temp_fd, outstr, strlen(outstr));
    NewLine();
    return 0;
}

int PrinterPostScript::Put(const genericChar* my_string, int flags)
{
    FnTrace("PrinterPostScript::Put()");
    genericChar buffer[STRLONG];
    genericChar outstr[STRLONG];
    int idx = 0;
    int buffidx = 0;
    int len = strlen(my_string);
    if (temp_fd <= 0)
        return 1;

    while (idx < len)
    {
        if (my_string[idx] == '(' || my_string[idx] == ')')
        {
            buffer[buffidx] = '\\';
            buffidx += 1;
        }
        buffer[buffidx] = my_string[idx];
        idx += 1;
        buffidx += 1;
    }
    buffer[buffidx] = '\0';
    WriteFlags(flags);
    snprintf(outstr, STRLONG, "(%s) ShowText", buffer);
    write(temp_fd, outstr, strlen(outstr));
    return 0;
}

int PrinterPostScript::Put(genericChar c, int flags)
{
    FnTrace("PrinterPostScript::Put()");
    static int holdflags = 0;

    if (c == '\0')
    {
        putbuffer[putbuffidx] = '\0';
        Put(putbuffer, holdflags);
        putbuffer[0] = '\0';
        putbuffidx = 0;
    }
    else
    {
        holdflags = flags;
        putbuffer[putbuffidx] = c;
        putbuffidx += 1;
    }
    return 0;
}


/*********************************************************************
 * PDF
 *
 *  extends the PostScript driver to convert PS files to PDF
 ********************************************************************/

PrinterPDF::PrinterPDF()
{
    next         = NULL;
    fore         = NULL;
    parent       = NULL;
    last_mode    = 0;
    last_color   = 0;
    last_uni     = 0;
    last_uline   = 0;
    last_large   = 0;
    last_narrow  = 0;
    last_bold    = 0;
    port_no      = 0;
    active_flags = 0;
    temp_fd      = -1;
    temp_name.Set("");
    target.Set("");
    target_type  = TARGET_NONE;
    host_name.Set("");
    port_no      = 0;
    have_title   = 0;
    page_title.Set("");
    pulse        = -1;
    term_name.Set("");
}

PrinterPDF::PrinterPDF(const genericChar* host, int port, const genericChar* targetstr, int type)
{
    next         = NULL;
    fore         = NULL;
    parent       = NULL;
    last_mode    = 0;
    last_color   = 0;
    last_uni     = 0;
    last_uline   = 0;
    last_large   = 0;
    last_narrow  = 0;
    last_bold    = 0;
    port_no      = 0;
    active_flags = 0;
    temp_fd      = -1;
    temp_name.Set("");
    target.Set(targetstr);
    target_type  = type;
    host_name.Set(host);
    port_no      = port;
    have_title   = 0;
    page_title.Set("");
    pulse        = -1;
    term_name.Set("");
}

int PrinterPDF::Close()
{
    FnTrace("PrinterPDF::Close()");
    if (temp_fd <= 0)
        return 1;

    genericChar filename[STRLONG];
    genericChar pdffullpath[STRLONG];
    genericChar command[STRLONG];

    close(temp_fd);

    // create the PDF filename and convert the temp file
    MakeFileName(filename, NULL, ".pdf", STRLONG);
    snprintf(pdffullpath, STRLONG, "/tmp/%s", filename);
    snprintf(command, STRLONG, "ps2pdf %s %s", temp_name.Value(), pdffullpath);
    system(command);

    // now get rid of the temp file set the temp_name to the PDF file
    // for future operations
    unlink(temp_name.Value());
    temp_name.Set(pdffullpath);
    temp_fd = 0;

    // and do final processing
    PrinterPostScript::Close();
    return 0;
}


/*********************************************************************
 * ReceiptText
 *
 *  prints to plain text for onscreen viewing as well as email
 *  delivery of reports
 ********************************************************************/
PrinterReceiptText::PrinterReceiptText()
{
    next         = NULL;
    fore         = NULL;
    parent       = NULL;
    last_mode    = 0;
    last_color   = 0;
    last_uni     = 0;
    last_uline   = 0;
    last_large   = 0;
    last_narrow  = 0;
    last_bold    = 0;
    port_no      = 0;
    active_flags = 0;
    temp_fd      = -1;
    temp_name.Set("");
    target.Set("");
    target_type  = 0;
    host_name.Set("");
    port_no      = 0;
    have_title   = 0;
    page_title.Set("");
    pulse        = -1;
    term_name.Set("");
}

PrinterReceiptText::PrinterReceiptText(const genericChar* host, int port, const genericChar* targetstr, int type)
{
    next         = NULL;
    fore         = NULL;
    parent       = NULL;
    last_mode    = 0;
    last_color   = 0;
    last_uni     = 0;
    last_uline   = 0;
    last_large   = 0;
    last_narrow  = 0;
    last_bold    = 0;
    port_no      = 0;
    active_flags = 0;
    temp_fd      = -1;
    temp_name.Set("");
    target.Set(targetstr);
    target_type  = type;
    host_name.Set(host);
    port_no      = port;
    have_title   = 0;
    page_title.Set("");
    pulse        = -1;
    term_name.Set("");
}

int PrinterReceiptText::Start()
{
    FnTrace("PrinterReceiptText::Start()");
    Open();
    if (temp_fd <= 0)
        return 1;

    Init();
    return 0;
}

int PrinterReceiptText::End()
{
    FnTrace("PrinterReceiptText::End()");
    if (temp_fd <= 0)
        return 1;

    Close();
    have_title = 0;
    page_title.Set("");  // reset the title for a new print job
    return 0;
}

int PrinterReceiptText::Init()
{
    FnTrace("PrinterReceiptText::Init()");

    if (have_title == 0)
        page_title.Set(GENERIC_TITLE);

    return 0;
}

int PrinterReceiptText::LineFeed(int lines)
{
    FnTrace("PrinterReceiptText::LineFeed()");
    if (temp_fd <= 0)
        return 1;

    genericChar linefeed[] = "\n";

    while (lines > 0)
    {
        if (write(temp_fd, linefeed, strlen(linefeed)) < 0)
            break;
        lines -= 1;
    }
    return 0;
}


/*********************************************************************
 * ReportText
 *
 *  prints to plain text for onscreen viewing as well as email
 *  delivery of reports.  This differs from ReceiptText only
 *  in width.
 ********************************************************************/
PrinterReportText::PrinterReportText(const genericChar* host, int port, const genericChar* targetstr, int type)
{
    next         = NULL;
    fore         = NULL;
    parent       = NULL;
    last_mode    = 0;
    last_color   = 0;
    last_uni     = 0;
    last_uline   = 0;
    last_large   = 0;
    last_narrow  = 0;
    last_bold    = 0;
    port_no      = 0;
    active_flags = 0;
    temp_fd      = -1;
    temp_name.Set("");
    target.Set(targetstr);
    target_type  = type;
    host_name.Set(host);
    port_no      = port;
    have_title   = 0;
    page_title.Set("");
    pulse        = -1;
    term_name.Set("");
}


/*********************************************************************
 * FUNCTIONS
 *
 *  these are not object methods, but either instantiate
 *  the objects above or are used by those objects
 ********************************************************************/

/****
 * GetPort:  favors port set via 
 ****/
int GetPort(genericChar* target, int port)
{
    FnTrace("GetPort()");
    int retval = 0;
    int idx = 0;
    int len = strlen(target);

    while ((idx < len) && (target[idx] != ','))
        idx += 1;
    if (target[idx] == ',' )
    {
        target[idx] = '\0';
        idx += 1;
        if (len >= idx)
            retval = atoi(&target[idx]);
    }
    // final verification
    if (retval == 0)
    {
        if (port != 0)
        {
            retval = port;
        }
        else
        { // assume vt_print
            retval = PORT_VT_DAEMON;
        }
    }
    return retval;
}

int ParseDestination(int &type, genericChar* target, int &port, const genericChar* destination)
{
    FnTrace("ParseDestination()");
    genericChar buffer[STRLENGTH];
    int buffidx = 0;
    int destidx = 0;
    int destlen = strlen(destination);
    int retval = 0;

    type = TARGET_NONE;
    target[0] = '\0';

    while ((destidx < destlen) && (destination[destidx] != ':'))
    {
        buffer[buffidx] = destination[destidx];
        buffidx += 1;
        destidx += 1;
    }
    buffer[buffidx] = '\0';

    if (destination[destidx] == ':')
    { // found a new format
        strncpy(target, &destination[destidx] + 1, STRLENGTH);
        if (strncmp(TARGET_TYPE_PARALLEL, buffer, STRLENGTH) == 0)
        {
            type = TARGET_PARALLEL;
        }
        else if (strncmp(TARGET_TYPE_LPD, buffer, STRLENGTH) == 0)
        {
            type = TARGET_LPD;
        }
        else if (strncmp(TARGET_TYPE_SOCKET, buffer, STRLENGTH) == 0)
        {
            type = TARGET_SOCKET;
            port = GetPort(target, port);
        }
        else if (strncmp(TARGET_TYPE_FILE, buffer, STRLENGTH) == 0)
        {
            type = TARGET_FILE;
        }
        else if (strncmp(TARGET_TYPE_EMAIL, buffer, STRLENGTH) == 0)
        {
            type = TARGET_EMAIL;
        }
        else if (debug_mode)
        {
            printf("Unknown printer destination:  '%s'\n", buffer);
        }
    }
    else
    { // legacy format
        if (destination[0] == '/')
        {
            type = TARGET_PARALLEL;
            strncpy(target, destination, STRLENGTH);
        }
        else if (port == 0)
        {
            type = TARGET_PARALLEL;
            snprintf(target, STRLENGTH, "/dev/%s", destination);
        }
        else
        {
            type = TARGET_SOCKET;
            strncpy(target, destination, STRLENGTH);
            port = GetPort(target, port);
        }
    }
    return retval;
}

/****
 * NewPrinter:  parse the given arguments, especially host, and return
 *   a printer object allocated based on that information.
 * The name contains Obj so as not to conflict with Control::NewPrinter.
 ****/
Printer *NewPrinterObj(const genericChar* destination, int port, int model, int no)
{
    FnTrace("NewPrinterObj()");
    Printer *retPrinter = NULL;
    genericChar target[STRLENGTH] = "";
    int target_type;

    // need to determine destination
    target_type = TARGET_NONE;
    ParseDestination(target_type, target, port, destination);

    // output format is based on model
    if (target_type != TARGET_NONE)
    {
        switch (model)
        {
        case MODEL_ITHACA:
            retPrinter = new PrinterIthaca(destination, port, target, target_type);
            break;
        case MODEL_STAR:
            retPrinter = new PrinterStar(destination, port, target, target_type);
            break;
        case MODEL_EPSON:
            retPrinter = new PrinterEpson(destination, port, target, target_type);
            break;
        case MODEL_HP:
            retPrinter = new PrinterHP(destination, port, target, target_type);
            break;
        case MODEL_HTML:
            retPrinter = new PrinterHTML(destination, port, target, target_type);
            break;
        case MODEL_POSTSCRIPT:
            retPrinter = new PrinterPostScript(destination, port, target, target_type);
            break;
        case MODEL_PDF:
            retPrinter = new PrinterPDF(destination, port, target, target_type);
            break;
        case MODEL_RECEIPT_TEXT:
            retPrinter = new PrinterReceiptText(destination, port, target, target_type);
            break;
        case MODEL_REPORT_TEXT:
            retPrinter = new PrinterReportText(destination, port, target, target_type);
            break;
        default:
            retPrinter = NULL;
            break;
        }
    }
    
    return retPrinter;
}

Printer *NewPrinterFromString(const genericChar* specification)
{
    FnTrace("NewPrinterFromString()");
    Printer *retPrinter = NULL;
    genericChar destination[STRLONG] = "";
    genericChar modelstr[STRLONG] = "";
    int model = MODEL_HTML;  // use default model
    int idx = 0;
    int mdlidx = 0;
    int len = strlen(specification);
    
    while ((idx < len) && (specification[idx] != ' '))
    {
        destination[idx] = specification[idx];
        idx++;
    }
    destination[idx] = '\0';
    if ((idx < len) && (specification[idx] == ' '))
    {
        idx++;
        while (idx < len)
        {
            modelstr[mdlidx] = tolower(specification[idx]);
            mdlidx++;
            idx++;
        }
        modelstr[mdlidx] = '\0';
    }
    if (strcmp(modelstr, "ithaca") == 0)
        model = MODEL_ITHACA;
    else if (strcmp(modelstr, "star") == 0)
        model = MODEL_STAR;
    else if (strcmp(modelstr, "epson") == 0)
        model = MODEL_EPSON;
    else if (strcmp(modelstr, "hp") == 0)
        model = MODEL_HP;
    else if (strcmp(modelstr, "html") == 0)
        model = MODEL_HTML;
    else if (strcmp(modelstr, "postscript") == 0)
        model = MODEL_POSTSCRIPT;
    else if (strcmp(modelstr, "pdf") == 0)
        model = MODEL_PDF;
    else if (strcmp(modelstr, "rtext") == 0)
        model = MODEL_RECEIPT_TEXT;

    retPrinter = NewPrinterObj(destination, 0, model, 0);

    return retPrinter;
}
