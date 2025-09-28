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
 * cdu_main.cc  Small utility to receive CDU messages from a TCP/IP source
 *  and print them to the local device.
 */

#include <array>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <unistd.h>

#include "cdu_att.hh"
#include "socket.hh"
#include "utility.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

namespace
{
#ifdef BSD
constexpr std::string_view kDefaultDevicePath{"/dev/ttyd0"};
#else
constexpr std::string_view kDefaultDevicePath{"/dev/ttyS0"};
#endif

inline constexpr int kCduEpson = 1;
inline constexpr int kCduBa63 = 2;

std::string g_deviceName{std::string(kDefaultDevicePath)};
int g_deviceTypeValue{kCduBa63};
int g_inetPortNumber{CDU_PORT};
bool g_verbose{false};

class FileDescriptor
{
public:
    FileDescriptor() = default;
    explicit FileDescriptor(int fd) noexcept : fd_(fd) {}
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    FileDescriptor(FileDescriptor&& other) noexcept
        : fd_(std::exchange(other.fd_, -1))
    {
    }
    FileDescriptor& operator=(FileDescriptor&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }
    ~FileDescriptor()
    {
        reset();
    }

    void reset(int newFd = -1) noexcept
    {
        if (fd_ >= 0)
            close(fd_);
        fd_ = newFd;
    }

    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] explicit operator bool() const noexcept { return fd_ >= 0; }

private:
    int fd_{-1};
};

class DeviceLock
{
public:
    explicit DeviceLock(const char* path)
    {
        if (path != nullptr)
            handle_ = LockDevice(path);
    }

    DeviceLock(const DeviceLock&) = delete;
    DeviceLock& operator=(const DeviceLock&) = delete;

    DeviceLock(DeviceLock&& other) noexcept
        : handle_(std::exchange(other.handle_, -1))
    {
    }

    DeviceLock& operator=(DeviceLock&& other) noexcept
    {
        if (this != &other)
        {
            release();
            handle_ = std::exchange(other.handle_, -1);
        }
        return *this;
    }

    ~DeviceLock()
    {
        release();
    }

    [[nodiscard]] bool acquired() const noexcept { return handle_ > 0; }

    void release() noexcept
    {
        if (handle_ > 0)
        {
            UnlockDevice(handle_);
            handle_ = -1;
        }
    }

private:
    int handle_{-1};
};

[[nodiscard]] std::string_view TrimLeadingWhitespace(std::string_view value) noexcept
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);
    return value;
}

[[nodiscard]] std::optional<int> ParseInteger(std::string_view value) noexcept
{
    value = TrimLeadingWhitespace(value);
    if (value.empty())
        return std::nullopt;

    int result = 0;
    const char* const first = value.data();
    const char* const last = first + value.size();
    const auto [ptr, ec] = std::from_chars(first, last, result);
    if (ec == std::errc::invalid_argument || ptr == first)
        return std::nullopt;
    if (ec == std::errc::result_out_of_range)
        return std::nullopt;
    return result;
}

} // namespace


/*********************************************************************
 * PROTOTYPES
 ********************************************************************/
int ParseArguments(int argc, const char* argv[]);
int SetAttributes(int fd);
void ShowHelp(const char* progname);
int PrintFromRemote(int my_socket, int serial_port);


/*********************************************************************
 * MAIN LOOP
 ********************************************************************/
int main(int argc, const char* argv[])
{
    ParseArguments(argc, argv);
    if (g_verbose)
    {
        std::printf("Listening on port %d\n", g_inetPortNumber);
        std::printf("Writing to CDU at %s\n", g_deviceName.c_str());
    }

    FileDescriptor listen_socket{Listen(g_inetPortNumber)};
    if (!listen_socket)
        return 1;

    while (listen_socket.get() > -1)
    {
        if (g_verbose)
            std::printf("Waiting to accept connection...\n");

        FileDescriptor connection{Accept(listen_socket.get())};
        if (!connection)
            continue;

        if (g_verbose)
            std::printf("Got connection...\n");

        DeviceLock device_lock{g_deviceName.c_str()};
        if (device_lock.acquired())
        {
            FileDescriptor serial_port{open(g_deviceName.c_str(), O_RDWR | O_NOCTTY | O_NDELAY)};
            if (serial_port)
            {
                if (g_verbose)
                    std::printf("Locked and opened device\n");
                SetAttributes(serial_port.get());
                PrintFromRemote(connection.get(), serial_port.get());
            }
            else
            {
                std::array<char, STRLENGTH> error_message{};
                std::snprintf(error_message.data(), error_message.size(), "Could not write %s", g_deviceName.c_str());
                perror(error_message.data());
            }
        }

        if (g_verbose)
            std::printf("Closing socket\n");
    }

    return 0;
}


/*********************************************************************
 * SUBROUTINES
 ********************************************************************/

/****
 * ShowHelp:
 ****/
void ShowHelp(const char* progname)
{
    std::printf("\n");
    std::printf("Usage:  %s [OPTIONS]\n", progname);
    std::printf("  -d<device>  Serial device (default %s)\n", g_deviceName.c_str());
    std::printf("  -h          Show this help screen\n");
    std::printf("  -p<port>    Set the listening port (default %d)\n", g_inetPortNumber);
    std::printf("  -t<type>    Set the device type (default %d)\n", g_deviceTypeValue);
    std::printf("  -v          Verbose mode\n");
    std::printf("\n");
    std::printf("Note:  there can be no spaces between an option and the associated\n");
    std::printf("argument.  AKA, it's \"-p6555\" not \"-p 6555\".\n");
    std::printf("\n");
    std::printf("The supported CDU devices are:\n");
    std::printf("Epson protocol = %d\n", kCduEpson);
    std::printf("BA63 (Wincor)  = %d\n", kCduBa63);
    std::printf("\n");
    exit(1);
}

/****
 * ParseArguments:
 ****/
int ParseArguments(int argc, const char* argv[])
{
    for (int idx = 1; idx < argc; ++idx)
    {
        const char* arg = argv[idx];
        if (arg[0] == '-')
        {
            switch (arg[1])
            {
            case 'd':
                g_deviceName.assign(&arg[2]);
                break;
            case 'h':
                ShowHelp(argv[0]);
                break;
            case 'p':
                if (auto parsed = ParseInteger(&arg[2]))
                    g_inetPortNumber = *parsed;
                break;
            case 't':
                if (auto parsed = ParseInteger(&arg[2]))
                    g_deviceTypeValue = *parsed;
                break;
            case 'v':
                g_verbose = true;
                break;
            }
        }
    }
    return 0;
}

/****
 * SetAttributes:
 ****/
int SetAttributes(int fd)
{
    int retval = 1;
    switch (g_deviceTypeValue)
    {
    case kCduEpson:
        retval = EpsonSetAttributes(fd);
        break;
    case kCduBa63:
        retval = BA63SetAttributes(fd);
        break;
    default:
        std::fprintf(stderr, "Unknown device type:  %d\n", g_deviceTypeValue);
        std::fprintf(stderr, "Did not initialize serial port!\n");
        break;
    }
    return retval;
}

/****
 * PrintFromRemote:  given an open socket and an open serial connection,
 *   reads from the socket and passes the data to the printer.
 ****/
int PrintFromRemote(int my_socket, int serial_port)
{
    char buffer[STRLENGTH];
    int active = 1;

    while (active > 0)
    {  // read from socket, print to serial port until we get an error
        active = recv(my_socket, buffer, STRLENGTH, 0);
        if (active > 0)
        {
            active = write(serial_port, buffer, active);  // active contains bytes read
            if (active < 0)
                perror("Failed to write");
        }
        else if (active < 0)
            perror("Failed to read");
    }
    return active;
}

