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
 * remote_printer.cc - revision 10 (3/25/98)
 * Remote Printer link module implementation
 */

#include "remote_printer.hh"
#include "printer.hh"
#include "remote_link.hh"
#include "../network/remote_link.hh"  // For PrinterProtocol constants
#include "system.hh"
#include "terminal.hh"
#include "manager.hh"
#include "src/utils/cpp23_utils.hh"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/file.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <X11/Intrinsic.h>
#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <array>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** Forward Declaration ****/
void PrinterCB(XtPointer client_data, int *fid, XtInputId *id);

namespace
{
// Basic validation to prevent shell metacharacters/whitespace in host names.
bool ValidateHost(const Str& host)
{
    const char* h = host.Value();
    if (h == nullptr || *h == '\0')
        return false;

    for (const char* p = h; *p; ++p)
    {
        unsigned char c = static_cast<unsigned char>(*p);
        if (std::isspace(c) || c == ';' || c == '&' || c == '|' ||
            c == '$' || c == '`' || c == '<' || c == '>')
        {
            return false;
        }
    }
    return true;
}

// Launch vt_print without invoking a shell to avoid command injection.
bool SpawnPrinterProcess(int number, const Str& host, int port, int model)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        std::string number_str = std::to_string(number);
        std::string port_str   = std::to_string(port);
        std::string model_str  = std::to_string(model);

        execlp("vt_print", "vt_print",
               number_str.c_str(),
               host.Value(),
               port_str.c_str(),
               model_str.c_str(),
               (char *) nullptr);
        _exit(127); // Only reached on exec failure
    }

    if (pid < 0)
    {
        ReportError("Failed to fork vt_print process");
        return false;
    }

    // Parent: child continues independently; no wait to keep behavior async.
    return true;
}
}

/**** RemotePrinter Class ****/
class RemotePrinter : public Printer
{
public:
    int socket_no;
    int input_id;
    Str host_name;
    int port_no;
    int model;
    int number;  // Printer number identifier
    int device_no;  // File descriptor for device
    std::unique_ptr<CharQueue> buffer_in, buffer_out;
    Str filename;
    int failure;

    // Constructor
    RemotePrinter(const char* host, int port, int mod, int no);
    // Destructor
    ~RemotePrinter() override;

    // Member Functions
    int   WInt8(int val);
    int   RInt8(int *val = nullptr);
    int   WStr(const char* str, int len = 0);
    const genericChar* RStr(char* str = nullptr);  // Changed to non-const for GetString
    int   Send();
    int   SendNow();
    int   Reconnect();  // Critical fix: Add reconnection method
    int   IsOnline() const;   // Critical fix: Add online status check
    int   ReconnectIfOffline() override;

    int   StopPrint() override;
    int   OpenDrawer(int drawer) override;
    int   Start() override;
    int   End() override;
    
    // Required pure virtual functions from Printer base class
    int WriteFlags(int flags) override;
    int Model() override;
    int Init() override;
    int NewLine() override;
    int LineFeed(int lines = 1) override;
    int FormFeed() override;
    int MaxWidth() override;
    int MaxLines() override;
    int Width(int flags = 0) override;
    int CutPaper(int partial_only = 0) override;
};


// Constructor
RemotePrinter::RemotePrinter(const char* host, int port, int mod, int no)
{
    host_name.Set(host);
    port_no = port;
    model = mod;
    number = no;
    socket_no = -1;
    input_id = -1;
    device_no = 0;
    failure = 0;

    // Setup Connection
    buffer_in = std::make_unique<CharQueue>(1024);
    buffer_out = std::make_unique<CharQueue>(1024);
    std::array<char, 256> str{}, tmp{};
    struct sockaddr_un addr{};
    vt::cpp23::format_to_buffer(str.data(), str.size(), "/tmp/vt_print{}", no);
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, str.data(), sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
    DeleteFile(str.data());

    int dev = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dev < 0)
    {
        vt::cpp23::format_to_buffer(tmp.data(), tmp.size(), "Failed to open socket '{}'", str.data());
        ReportError(tmp.data());
        return;
    }
    if (bind(dev, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1)
    {
        close(dev);
        vt::cpp23::format_to_buffer(tmp.data(), tmp.size(), "Failed to bind socket '{}'", str.data());
        ReportError(tmp.data());
        return;
    }

    if (!ValidateHost(host_name))
    {
        ReportError("Invalid printer host name");
        close(dev);
        return;
    }

    if (!SpawnPrinterProcess(no, host_name, port, mod))
    {
        close(dev);
        return;
    }
    listen(dev, 1);

    sockaddr_un peer{};
    socklen_t len = sizeof(peer);
    socket_no = accept(dev, reinterpret_cast<struct sockaddr*>(&peer), &len);
    if (socket_no < 0)
    {
        vt::cpp23::format_to_buffer(tmp.data(), tmp.size(), "Failed to get connection with printer {}", no);
        ReportError(tmp.data());
    }
    close(dev);
}

// Destructor
RemotePrinter::~RemotePrinter()
{
    if (input_id >= 0)
        RemoveInputFn(input_id);
    if (socket_no >= 0)
    {
        WInt8(ToInt(PrinterProtocol::Die));
        SendNow();
        close(socket_no);
    }
    // Smart pointers automatically clean up buffer_in and buffer_out
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

const char* RemotePrinter::RStr(char* s)
{
    static std::array<char, 1024> buffer{};
    if (s == nullptr)
        s = buffer.data();
    if (buffer_in->GetString(s, buffer.size()))
    {
        s[0] = '\0';
    }
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
    const int written = buffer_out->Write(socket_no, 0);
    if (written < 0)
    {
        ++failure;  // track consecutive failures to trigger reconnection
        return -1;
    }
    buffer_out->Clear();
    return 0;
}

// Critical fix: Add printer reconnection logic
int RemotePrinter::Reconnect()
{
    FnTrace("RemotePrinter::Reconnect()");
    
    // Only attempt reconnection if printer is marked as offline
    if (failure != 999)
        return 0;
    
    std::array<char, 256> str{}, tmp{};
    struct sockaddr_un addr{};
    vt::cpp23::format_to_buffer(str.data(), str.size(), "/tmp/vt_print{}", number);
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, str.data(), sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
    DeleteFile(str.data());

    int dev = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dev < 0)
    {
        vt::cpp23::format_to_buffer(tmp.data(), tmp.size(), "Reconnect failed: Cannot open socket '{}'", str.data());
        ReportError(tmp.data());
        return 1;
    }

    // Ensure socket operations do not block the main loop
    fcntl(dev, F_SETFL, O_NONBLOCK);
    
    if (bind(dev, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1)
    {
        close(dev);
        vt::cpp23::format_to_buffer(tmp.data(), tmp.size(), "Reconnect failed: Cannot bind socket '{}'", str.data());
        ReportError(tmp.data());
        return 1;
    }

    if (!ValidateHost(host_name))
    {
        ReportError("Invalid printer host name during reconnect");
        close(dev);
        return 1;
    }

    // Attempt to restart the vt_print process without invoking a shell
    if (!SpawnPrinterProcess(number, host_name, port_no, model))
    {
        close(dev);
        return 1;
    }
    listen(dev, 1);

    // Wait briefly for vt_print to connect back; avoid blocking indefinitely
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(dev, &readfds);
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    int sel = select(dev + 1, &readfds, nullptr, nullptr, &tv);
    if (sel <= 0) {
        close(dev);
        vt::cpp23::format_to_buffer(tmp.data(), tmp.size(), "Reconnect failed: Timed out waiting for vt_print on {}", str.data());
        ReportError(tmp.data());
        return 1;
    }

    sockaddr_un peer{};
    socklen_t len = sizeof(peer);
    int new_socket = accept(dev, reinterpret_cast<struct sockaddr*>(&peer), &len);
    if (new_socket < 0)
    {
        close(dev);
        vt::cpp23::format_to_buffer(tmp.data(), tmp.size(), "Reconnect failed: Cannot connect to printer {}", number);
        ReportError(tmp.data());
        return 1;
    }
    
    // Successfully reconnected
    socket_no = new_socket;
    failure = 0;
    close(dev);
    
    // Re-register the input callback
    if (input_id >= 0)
        RemoveInputFn(input_id);
    input_id = AddInputFn((InputFn) PrinterCB, socket_no, this);
    
    vt::cpp23::format_to_buffer(tmp.data(), tmp.size(), "Printer {}:{} successfully reconnected", host_name.Value(), port_no);
    ReportError(tmp.data());
    
    // Update UI to show printer as online
    if (parent)
        parent->UpdateAll(UPDATE_PRINTERS, nullptr);
    
    return 0;
}

int RemotePrinter::ReconnectIfOffline()
{
    // Reuse existing reconnection logic when marked offline
    return Reconnect();
}

// Critical fix: Add method to check if printer is online
int RemotePrinter::IsOnline() const
{
    FnTrace("RemotePrinter::IsOnline()");
    
    // If failure is 999, printer is marked as offline
    if (failure == 999)
        return 0;
    
    // If socket is closed, printer is offline
    if (socket_no < 0)
        return 0;
    
    // If we have too many failures, consider offline
    if (failure >= 8)
        return 0;
    
    return 1; // Online
}

int RemotePrinter::StopPrint()
{
    WInt8(ToInt(PrinterProtocol::Cancel));
    return SendNow();
}

int RemotePrinter::OpenDrawer(int drawer)
{
    WInt8(ToInt(PrinterProtocol::OpenDrawer));
    return SendNow();
}

int RemotePrinter::WriteFlags(int flags)
{
    // RemotePrinter doesn't directly handle flags - they're sent to remote process
    // This is a required override from Printer base class
    (void)flags;  // Unused for remote printer
    return 0;
}

int RemotePrinter::Model()
{
    return model;
}

int RemotePrinter::Init()
{
    return 0;  // Remote printer initialization is handled in constructor
}

int RemotePrinter::NewLine()
{
    // Remote printer handles newlines via data stream, not protocol commands
    // Send newline character directly
    WStr("\n", 1);
    return SendNow();
}

int RemotePrinter::LineFeed(int lines)
{
    // Send multiple newlines
    for (int i = 0; i < lines; ++i)
    {
        WStr("\n", 1);
    }
    return SendNow();
}

int RemotePrinter::FormFeed()
{
    // Send form feed character
    WStr("\f", 1);
    return SendNow();
}

int RemotePrinter::MaxWidth()
{
    // Default to 80 columns for remote printer
    return 80;
}

int RemotePrinter::MaxLines()
{
    return -1;  // Continuous feed
}

int RemotePrinter::Width(int flags)
{
    (void)flags;  // Remote printer doesn't change width based on flags
    return MaxWidth();
}

int RemotePrinter::CutPaper(int partial_only)
{
    (void)partial_only;  // Remote printer doesn't support partial cuts
    // Remote printer handles paper cutting via remote process
    // This is a stub - actual cutting would be handled by the remote printer process
    return 0;
}

int RemotePrinter::Start()
{
    if (device_no)
    {
        DeleteFile(filename.Value());
        close(device_no);
    }

    char print_file_buffer[256];
    filename.Set(MasterSystem->NewPrintFile(print_file_buffer));
    std::array<char, 256> str{};
    vt::cpp23::format_to_buffer(str.data(), str.size(), "/tmp/vt_{}", host_name.Value());
    device_no = creat(str.data(), 0666);

    if (device_no <= 0)
    {
        filename.Clear();
        return 1;
    }

    if (model == MODEL_EPSON)
    {
        // reset printer head
        genericChar str[] = { 0x1b, 0x3c };
        write(device_no, str, sizeof(str));
    }
    else if (model == MODEL_STAR)
        LineFeed(2);
    return Init();
}

int RemotePrinter::End()
{
    if (device_no <= 0)
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

    close(device_no);
    device_no = 0;

    WInt8(ToInt(PrinterProtocol::File));
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
        
        // Critical fix: Provide better error reporting and status updates
        std::array<char, 256> errmsg{};
        if (p->failure == 1)
        {
            vt::cpp23::format_to_buffer(errmsg.data(), errmsg.size(), "Printer {}:{} connection lost (attempt {}/8)", 
                     p->host_name.Value(), p->port_no, p->failure);
            ReportError(errmsg.data());
        }
        else if (p->failure == 4)
        {
            vt::cpp23::format_to_buffer(errmsg.data(), errmsg.size(), "Printer {}:{} still offline (attempt {}/8) - checking connection", 
                     p->host_name.Value(), p->port_no, p->failure);
            ReportError(errmsg.data());
        }
        
        if (p->failure < 8)
            return;

        // After 8 failures, mark printer as offline and attempt reconnection
        vt::cpp23::format_to_buffer(errmsg.data(), errmsg.size(), "Printer {}:{} marked as OFFLINE after {} connection failures", 
                 p->host_name.Value(), p->port_no, p->failure);
        ReportError(errmsg.data());

        if (p->socket_no >= 0)
        {
            // close socket here instead of letting the destructor do it
            // (destructor tries to send kill message before closing)
            close(p->socket_no);
            p->socket_no = -1;
        }

        // Critical fix: Don't kill the printer immediately, mark it for reconnection
        if (db)
        {
            // Mark printer as offline but keep it in the list for reconnection attempts
            p->failure = 999; // Special value to indicate offline status
            // Update UI to show printer as offline
            db->UpdateAll(UPDATE_PRINTERS, nullptr);
        }
        return;
    }

    // Connection successful - reset failure count and update status
    if (p->failure > 0)
    {
        std::array<char, 256> msg{};
        vt::cpp23::format_to_buffer(msg.data(), msg.size(), "Printer {}:{} connection restored", 
                 p->host_name.Value(), p->port_no);
        ReportError(msg.data());
        p->failure = 0;
        
        // Update UI to show printer as online
        if (db)
            db->UpdateAll(UPDATE_PRINTERS, nullptr);
    }

    std::array<char, 256> str{};
    while (p->buffer_in->size > 0)
    {
        int code = p->RInt8();
        switch (code)
        {
        case ToInt(ServerProtocol::SrvError):
            vt::cpp23::format_to_buffer(str.data(), str.size(), "PrinterError: {}", p->RStr());
            ReportError(str.data());
            break;
        case ToInt(ServerProtocol::SrvPrinterDone):
            DeleteFile(p->RStr());
            break;
        case ToInt(ServerProtocol::SrvBadFile):
            p->RStr();
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

    if (p->socket_no < 0)
    {
        delete p;
        return nullptr;
    }

    p->input_id = AddInputFn((InputFn) PrinterCB, p->socket_no, p);
    return p;
}
