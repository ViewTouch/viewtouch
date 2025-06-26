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
 * remote_printer.cc - revision 10 (3/25/98)
 * Remote Printer link module implementation
 */

#include "remote_printer.hh"
#include "printer.hh"
#include "remote_link.hh"
#include "system.hh"
#include "terminal.hh"
#include "manager.hh"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <unistd.h>
#include <X11/Intrinsic.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** RemotePrinter Class ****/
class RemotePrinter : public Printer
{
public:
    int socket_no;
    int input_id;
    Str host_name;
    int port_no;
    int model;
    int number;
    CharQueue *buffer_in, *buffer_out;
    Str filename;
    int failure;

    // Constructor
    RemotePrinter(const char* host, int port, int mod, int no);
    // Destructor
    ~RemotePrinter();

    // Member Functions
    int   WInt8(int val);
    int   RInt8(int *val = nullptr);
    int   WStr(const char* str, int len = 0);
    const genericChar* RStr(genericChar* str = nullptr);
    int   Send();
    int   SendNow();

    // Pure virtual method implementations from Printer base class
    virtual int WriteFlags(int flags) override { return 0; }
    virtual int Model() override { return model; }
    virtual int Init() override { return 0; }
    virtual int NewLine() override { return 0; }
    virtual int LineFeed(int lines = 1) override { return 0; }
    virtual int FormFeed() override { return 0; }
    virtual int MaxWidth() override { return 80; }
    virtual int MaxLines() override { return -1; }
    virtual int Width(int flags = 0) override { return 80; }
    virtual int CutPaper(int partial_only = 0) override { return 0; }

    int   StopPrint() override;
    int   OpenDrawer(int drawer) override;
    int   Start() override;
    int   End() override;
};


// Constructor
RemotePrinter::RemotePrinter(const char* host, int port, int mod, int no)
{
    host_name.Set(host);
    port_no = port;
    model = mod;
    number = no;
    socket_no = 0;
    input_id = 0;
    failure = 0;

    // Setup Connection
    buffer_in  = new CharQueue(1024);
    buffer_out = new CharQueue(1024);
    genericChar str[256], tmp[256];
    struct sockaddr name;
    snprintf(str, sizeof(str), "/tmp/vt_print%d", no);
    name.sa_family = AF_UNIX;
    strcpy(name.sa_data, str);
    DeleteFile(str);

    int dev = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dev <= 0)
    {
        snprintf(tmp, sizeof(tmp), "Failed to open socket '%s'", str);
        ReportError(tmp);
        return;
    }
    if (bind(dev, &name, sizeof(name)) == -1)
    {
        close(dev);
        snprintf(tmp, sizeof(tmp), "Failed to bind socket '%s'", str);
        ReportError(tmp);
        return;
    }

    snprintf(str, sizeof(str), "vt_print %d %s %d %d&", no, host, port, mod);
    system(str);
    listen(dev, 1);

    Uint len;
    len = sizeof(struct sockaddr);
    socket_no = accept(dev, (struct sockaddr *) str, &len);
    if (socket_no <= 0)
    {
        snprintf(tmp, sizeof(tmp), "Failed to get connection with printer %d", no);
        ReportError(tmp);
    }
    close(dev);
}

// Destructor
RemotePrinter::~RemotePrinter()
{
    if (input_id)
        RemoveInputFn(input_id);
    if (socket_no)
    {
        WInt8(PRINTER_DIE);
        SendNow();
        close(socket_no);
    }
    if (buffer_in)
        delete buffer_in;
    if (buffer_out)
        delete buffer_out;
}

// Member Functions
int RemotePrinter::WInt8(int val)
{
    return buffer_out->Put8(val);
}

int RemotePrinter::RInt8(int *val)
{
    int v = buffer_in->Get8();
    if (val)
        *val = v;
    return v;
}

int RemotePrinter::WStr(const char* s, int len)
{
    if (s == nullptr)
        return buffer_out->PutString("", 0);
    else
        return buffer_out->PutString(s, len);
}

const char* RemotePrinter::RStr(genericChar* s)
{
    static genericChar buffer[1024];
    if (s == nullptr)
        s = buffer;
    buffer_in->GetString(s);
    return s;
}

int RemotePrinter::Send()
{
    if (buffer_out->size > 4096)
        SendNow();
    return 0;
}

int RemotePrinter::SendNow()
{
    if (buffer_out->size <= 0)
        return 1;
    buffer_out->Write(socket_no);
    buffer_out->Clear();
    return 0;
}

int RemotePrinter::StopPrint()
{
    WInt8(PRINTER_CANCEL);
    return SendNow();
}

int RemotePrinter::OpenDrawer(int drawer)
{
    WInt8(PRINTER_OPENDRAWER);
    return SendNow();
}

int RemotePrinter::Start()
{
    if (temp_fd)
    {
        DeleteFile(filename.Value());
        close(temp_fd);
    }

    filename.Set(MasterSystem->NewPrintFile());
    genericChar str[256];
    snprintf(str, sizeof(str), "/tmp/vt_%s", host_name.Value());
    temp_fd = creat(str, 0666);

    if (temp_fd <= 0)
    {
        filename.Clear();
        return 1;
    }

    if (model == MODEL_EPSON)
    {
        // reset printer head
        genericChar str[] = { 0x1b, 0x3c };
        write(temp_fd, str, sizeof(str));
    }
    else if (model == MODEL_STAR)
        LineFeed(2);
    return Init();
}

int RemotePrinter::End()
{
    if (temp_fd <= 0)
        return 1;

    switch (model)
    {
    case MODEL_EPSON:
        LineFeed(13);
        CutPaper();
        break;
    case MODEL_STAR:
        LineFeed(9);
        CutPaper();
        break;
    case MODEL_HP:
        FormFeed();
        break;
    }

    close(temp_fd);
    temp_fd = 0;

    WInt8(PRINTER_FILE);
    WStr(filename.Value());
    return SendNow();
}


/**** CallBack Functions ****/
void PrinterCB(XtPointer client_data, int *fid, XtInputId *id)
{
    RemotePrinter *p = (RemotePrinter *) client_data;
    int val = p->buffer_in->Read(p->socket_no);

    Control *db = p->parent;
    if (val <= 0)
    {
        ++p->failure;
        if (p->failure < 8)
            return;

        if (p->socket_no > 0)
        {
            // close socket here instead of letting the destructor do it
            // (destructor tries to send kill message before closing)
            close(p->socket_no);
            p->socket_no = 0;
        }

        if (db)
            db->KillPrinter(p, 1);
        return;
    }

    p->failure = 0;
    genericChar str[256];
    while (p->buffer_in->size > 0)
    {
        int code = p->RInt8();
        switch (code)
        {
        case SERVER_ERROR:
            snprintf(str, sizeof(str), "PrinterError: %s", p->RStr(nullptr));
            ReportError(str);
            break;
        case SERVER_PRINTER_DONE:
            DeleteFile(p->RStr(nullptr));
            break;
        case SERVER_BADFILE:
            p->RStr(nullptr);
            break;
        }
    }
}


/**** Functions ****/
Printer *NewRemotePrinter(const char* host, int port, int model, int no)
{
    RemotePrinter *p = new RemotePrinter(host, port, model, no);
    if (p == nullptr)
        return nullptr;

    if (p->socket_no <= 0)
    {
        delete p;
        return nullptr;
    }

    p->input_id = AddInputFn((InputFn) PrinterCB, p->socket_no, p);
    return p;
}
