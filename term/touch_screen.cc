/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997  
  
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
 * touch_screen.cc - revision 37 (9/8/97)
 * Implementation of TouchScreen class
 */

#include "touch_screen.hh"
#include <cstring>
#include <cctype>
#include <string>
#include <iostream>
#include <array>

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <strings.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** Definitions ****/
namespace Constants {
    constexpr int MAX_TRIES = 8;
}

/**** TouchScreen Class ****/
// Constructors
TouchScreen::TouchScreen(const char* device)
{
	INIT = "\001PN819600\n";
	PING = "\001Z\n";
	RESET = "\001R\n";  // "destructive" command
    PARAM_LOCK = "\001PL\n"; // PL = "Parameter Lock"
	FORMAT_HEX = "\001FH\n";
	FORMAT_DEC = "\001FD\n";
	MODE_POINT = "\001MP\n";
    MODE_STREAM = "\001MS\n";
    MODE_CALIBRATE = "\001CI\n"; // CI = "Calibrate Interactive"
    AUTOBAUD_DISABLE = "\001AD\n";

    x_res = 1024;
    y_res = 1024; 
    last_x = 0;
    last_y = 0;

    device_no = 0;
    port = 0;
    host.Set(device);
    failed = Connect(1);
}

TouchScreen::TouchScreen(const char* h, int p)
{
	INIT = "\001PN819600\n";
	PING = "\001Z\n";
	RESET = "\001R\n";  // "destructive" command
    PARAM_LOCK = "\001PL\n"; // PL = "Parameter Lock"
	FORMAT_HEX = "\001FH\n";
	FORMAT_DEC = "\001FD\n";
	MODE_POINT = "\001MP\n";
    MODE_STREAM = "\001MS\n";
    MODE_CALIBRATE = "\001CI\n"; // CI = "Calibrate Interactive"
    AUTOBAUD_DISABLE = "\001AD\n";

    x_res = 1024;
    y_res = 1024; 
    last_x = 0;
    last_y = 0;

    device_no = 0;
    port = p;
    host.Set(h);
    failed = Connect(1);
}

// Move constructor
TouchScreen::TouchScreen(TouchScreen&& other) noexcept
    : buffer(std::move(other.buffer))
    , size(other.size)
    , INIT(std::move(other.INIT))
    , PING(std::move(other.PING))
    , RESET(std::move(other.RESET))
    , PARAM_LOCK(std::move(other.PARAM_LOCK))
    , FORMAT_HEX(std::move(other.FORMAT_HEX))
    , FORMAT_DEC(std::move(other.FORMAT_DEC))
    , MODE_POINT(std::move(other.MODE_POINT))
    , MODE_STREAM(std::move(other.MODE_STREAM))
    , MODE_CALIBRATE(std::move(other.MODE_CALIBRATE))
    , AUTOBAUD_DISABLE(std::move(other.AUTOBAUD_DISABLE))
    , device_no(other.device_no)
    , failed(other.failed)
    , x_res(other.x_res)
    , y_res(other.y_res)
    , last_x(other.last_x)
    , last_y(other.last_y)
    , host(std::move(other.host))
    , port(other.port)
    , last_reset(std::move(other.last_reset))
    , error(std::move(other.error))
{
    other.device_no = 0;
    other.failed = 0;
    other.size = 0;
}

// Move assignment operator
TouchScreen& TouchScreen::operator=(TouchScreen&& other) noexcept
{
    if (this != &other) {
        if (device_no > 0)
            close(device_no);
            
        buffer = std::move(other.buffer);
        size = other.size;
        INIT = std::move(other.INIT);
        PING = std::move(other.PING);
        RESET = std::move(other.RESET);
        PARAM_LOCK = std::move(other.PARAM_LOCK);
        FORMAT_HEX = std::move(other.FORMAT_HEX);
        FORMAT_DEC = std::move(other.FORMAT_DEC);
        MODE_POINT = std::move(other.MODE_POINT);
        MODE_STREAM = std::move(other.MODE_STREAM);
        MODE_CALIBRATE = std::move(other.MODE_CALIBRATE);
        AUTOBAUD_DISABLE = std::move(other.AUTOBAUD_DISABLE);
        device_no = other.device_no;
        failed = other.failed;
        x_res = other.x_res;
        y_res = other.y_res;
        last_x = other.last_x;
        last_y = other.last_y;
        host = std::move(other.host);
        port = other.port;
        last_reset = std::move(other.last_reset);
        error = std::move(other.error);
        
        other.device_no = 0;
        other.failed = 0;
        other.size = 0;
    }
    return *this;
}

// Destructor
TouchScreen::~TouchScreen()
{
    if (device_no > 0)
        close(device_no);
}

// Member Functions
int TouchScreen::Connect(int boot)
{
    if (device_no > 0)
        close(device_no);

    // default
    size = 0;

    if (port <= 0)
    {
        // host is local device
        device_no = open(host.Value(), O_RDWR | O_NONBLOCK, 0);
        if (device_no <= 0)
        {
            fprintf(stderr, "TouchScreen::Connect Error %d opening %s\n",
                    errno, host.Value());
            device_no = 0;
            return 1;  // Failed to open
        }
    }
    else
    {
        struct sockaddr_in inaddr;
        inaddr.sin_family      = AF_INET;
        inaddr.sin_port        = port;
        inaddr.sin_addr.s_addr = inet_addr(host.Value());
        if (inaddr.sin_addr.s_addr == INADDR_NONE)
        {
            struct hostent *hp = gethostbyname(host.Value());
            if (hp == NULL || hp->h_addrtype != AF_INET)
            {
                std::string str = std::string("Can't resolve name '")
                        + host.Value() + "'";
                error.Set(str.c_str());
                std::cerr << str << std::endl;
                return 1;
            }
            bcopy(hp->h_addr, &inaddr.sin_addr.s_addr, hp->h_length);
        }

        device_no = socket(AF_INET, SOCK_STREAM, 0);
        if (device_no < 0)
        {
            std::string str = "Can't open socket";
            error.Set(str.c_str());
            std::cerr << str << std::endl;
            device_no = 0;
            return 1;
        }

        if (connect(device_no, reinterpret_cast<const sockaddr*>(&inaddr), sizeof(inaddr)) < 0)
        {
            std::string str = std::string("Connection refused with '")
                    + host.Value() + "'";
            std::cerr << str << std::endl;
            error.Set(str.c_str());
            close(device_no);
            device_no = 0;
            return 1;
        }

        int flag = 1;
        setsockopt(device_no, IPPROTO_TCP, TCP_NODELAY, static_cast<void*>(&flag), sizeof(flag));
        fcntl(device_no, F_SETFL, O_NDELAY);
    }
    return Init(boot);
}

int TouchScreen::SetMode(const char* mode) noexcept
{
	if( (strcmp("POINT", mode) == 0) && device_no > 0)
	{
		const std::array<std::string, 4> modeList = { FORMAT_HEX, MODE_POINT, AUTOBAUD_DISABLE, PARAM_LOCK };
		for (const auto& modeStr : modeList)
        {
			write(device_no, modeStr.c_str(), modeStr.length());
		}

	}

	return 0;
}

int TouchScreen::Init(int boot)
{
    if (device_no <= 0)
    {
        error.Set("In TouchScreen::Init\nTouch screen device not open\n");
        fprintf(stderr, "%s",error.Value());
        return -1;
    }

    // Resolution for Format Hex
    x_res = 1024;
    y_res = 1024;

    Reset();
    sleep(1);

	//setup array of pointers and step through the desired mode
	//list, writing the values out to the device, in order.
	const std::array<std::string, 5> modeList = { INIT, AUTOBAUD_DISABLE, FORMAT_HEX, MODE_POINT, PARAM_LOCK };
	for(const auto& modeStr : modeList)
	{
		write(device_no, modeStr.c_str(), modeStr.length());
	}
    return 0;
}

int TouchScreen::ReadTouch(int &x, int &y, int &mode)
{
    if (device_no <= 0)
    {
        error.Set("In TouchScreen::ReadTouch\nTouch screen device not open\n");
        fprintf(stderr, "%s",error.Value());
        return -1;
    }

	char c;
	int result;

    do
    {
        result = read(device_no, &c, sizeof(c));

        if (result <= 0)
            return 0;  // No touch yet

        if (isprint(c))
            buffer[size++] = c;
    }
    while (c != '\n' && c != '\r' && size < static_cast<int>(buffer.size()));

    if (size != 7 || buffer[3] != 0x2C)
    {
        size = 0;
        return 0;  // No Touch
    }

    size = 0;
    mode = TOUCH_DOWN;
    buffer[7] = '\0';
    sscanf(&buffer[0], "%x", &x);
    sscanf(&buffer[4], "%x", &y);

    return 1;  // touch read
}

int TouchScreen::ReadStatus()
{
    if (device_no <= 0)
    {
        error.Set("<< TouchScreen::ReadStatus() >> ERROR: Touch screen device not open\n");
        fprintf(stderr,"%s", error.Value());
        return -1;
    }

    int result;
    genericChar c;
    do
    {
        result = read(device_no, &c, sizeof(c));
        if (result <= 0) return -1;  // No status yet

        if (isprint(c))
            buffer[size++] = c;
    }
    while (c != '\n' && c != '\r' && size < static_cast<int>(buffer.size()));
  
    if (size != 1)
    {
        size = 0;
        return -1;
    }

    size = 0;
    if (buffer[0] == '0')
        return 0;
    else if (buffer[0] == '1')
        return 1;
    else
        return -1;
}

int TouchScreen::Calibrate() noexcept
{
    if (device_no <= 0)
    {
        return -1;
    }

    Flush();
    write(device_no, MODE_CALIBRATE.c_str(), MODE_CALIBRATE.length());  // enter calibrate mode
    return 0;
}

int TouchScreen::Reset() noexcept
{
    if (device_no <= 0)
    {
        return -1;
    }

    Flush();
    write(device_no, RESET.c_str(), RESET.length());  // reset touch screen

    last_reset.Set();

	//need to wait at least 1 second for confirmation of the reset
	//from the controller
    TimeInfo start, end;
    start.Set();
    int result;
    do
    {
        result = ReadStatus();
        end.Set();
        if (SecondsElapsed(end, start) > 5)
            break;
    }
    while (result < 0);

    if (0)
    {
		//output a status message
        genericChar str[256];
        sprintf(str, "TouchScreen Reset failed for host '%s'", host.Value());

        error.Set(str);

        fprintf(stderr, "\n");
        fprintf(stderr, "%s",str);
        fprintf(stderr, "\nresult value returned: %d\n", result);
    }

    return 0;
}

int TouchScreen::Flush() noexcept
{
    if (device_no <= 0)
    {
        return -1;
    }

    int result;
    do
    {
        result = read(device_no, buffer.data(), 1);
    } while (result > 0);

    size = 0;
    return 0;
}
