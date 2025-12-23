/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 2025
  
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
#include <vector>
#include <chrono>
#include <algorithm>
#include <fstream>

#include <cerrno>
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
#include <sys/time.h>
#include <cmath>
#include "safe_string_utils.hh"

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
    
    // Initialize enhanced touch handling
    max_touch_history = 50;
    touch_timeout_ms = 1000;
    enable_multi_touch = false;
    enable_gestures = false;
    enable_pressure_sensitivity = false;
    touch_history.reserve(max_touch_history);
    
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
    
    // Initialize enhanced touch handling
    max_touch_history = 50;
    touch_timeout_ms = 1000;
    enable_multi_touch = false;
    enable_gestures = false;
    enable_pressure_sensitivity = false;
    touch_history.reserve(max_touch_history);
    
    failed = Connect(1);
}

// Move constructor
TouchScreen::TouchScreen(TouchScreen&& other) noexcept
    : buffer(other.buffer)
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
    , host(other.host)
    , port(other.port)
    , last_reset(other.last_reset)
    , error(other.error)
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
            
        buffer = other.buffer;
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
        host = other.host;
        port = other.port;
        last_reset = other.last_reset;
        error = other.error;
        
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
            if (hp == nullptr || hp->h_addrtype != AF_INET)
            {
                std::string str = std::string("Can't resolve name '")
                        + host.Value() + "'";
                error.Set(str.c_str());
                std::cerr << str << '\n';
                return 1;
            }
            bcopy(hp->h_addr, &inaddr.sin_addr.s_addr, hp->h_length);
        }

        device_no = socket(AF_INET, SOCK_STREAM, 0);
        if (device_no < 0)
        {
            std::string str = "Can't open socket";
            error.Set(str.c_str());
            std::cerr << str << '\n';
            device_no = 0;
            return 1;
        }

        if (connect(device_no, reinterpret_cast<const sockaddr*>(&inaddr), sizeof(inaddr)) < 0)
        {
            std::string str = std::string("Connection refused with '")
                    + host.Value() + "'";
            std::cerr << str << '\n';
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
        vt_safe_string::safe_format(str, 256, "TouchScreen Reset failed for host '%s'", host.Value());

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

// Enhanced touch methods implementation
int TouchScreen::ReadTouchEvent(TouchEvent &event)
{
    FnTrace("TouchScreen::ReadTouchEvent()");
    
    if (device_no <= 0)
    {
        error.Set("TouchScreen::ReadTouchEvent - Device not open");
        return -1;
    }
    
    int x, y, mode;
    int result = ReadTouch(x, y, mode);
    
    if (result == 1)
    {
        event.x = x;
        event.y = y;
        event.mode = mode;
        event.pressure = 0; // Default pressure
        event.touch_id = 0; // Single touch ID
        
        // Get current timestamp
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        event.timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        
        // Apply calibration if available
        ApplyCalibration(event.x, event.y);
        
        // Add to touch history
        if (touch_history.size() >= max_touch_history)
        {
            touch_history.erase(touch_history.begin());
        }
        touch_history.push_back(event);
        
        return 1;
    }
    
    return result;
}

int TouchScreen::ReadMultiTouch(std::vector<TouchEvent> &events)
{
    FnTrace("TouchScreen::ReadMultiTouch()");
    
    events.clear();
    
    if (!enable_multi_touch || device_no <= 0)
    {
        // Fall back to single touch
        TouchEvent event;
        int result = ReadTouchEvent(event);
        if (result == 1)
        {
            events.push_back(event);
        }
        return result;
    }
    
    // For now, multi-touch is not fully implemented in the hardware layer
    // This is a placeholder for future enhancement
    TouchEvent event;
    int result = ReadTouchEvent(event);
    if (result == 1)
    {
        events.push_back(event);
    }
    
    return result;
}

int TouchScreen::ProcessTouchEvents()
{
    FnTrace("TouchScreen::ProcessTouchEvents()");
    
    // Clean up old touch events from history
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    unsigned long current_time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    
    touch_history.erase(
        std::remove_if(touch_history.begin(), touch_history.end(),
            [current_time, this](const TouchEvent &event) {
                return (current_time - event.timestamp) > touch_timeout_ms;
            }),
        touch_history.end()
    );
    
    // Detect gestures if enabled
    if (enable_gestures && touch_history.size() > 1)
    {
        return DetectGesture(touch_history);
    }
    
    return 0;
}

int TouchScreen::ApplyCalibration(int &x, int &y)
{
    FnTrace("TouchScreen::ApplyCalibration()");
    
    if (!calibration.calibrated)
        return 0;
    
    // Apply rotation (simplified - assumes 0, 90, 180, or 270 degrees)
    int temp_x = x, temp_y = y;
    switch (calibration.rotation)
    {
        case 90:
            x = y_res - temp_y;
            y = temp_x;
            break;
        case 180:
            x = x_res - temp_x;
            y = y_res - temp_y;
            break;
        case 270:
            x = temp_y;
            y = x_res - temp_x;
            break;
        default:
            break;
    }
    
    // Apply scaling and offset
    x = static_cast<int>((x - calibration.offset_x) * calibration.scale_x);
    y = static_cast<int>((y - calibration.offset_y) * calibration.scale_y);
    
    return 1;
}

int TouchScreen::DetectGesture(const std::vector<TouchEvent> &events)
{
    FnTrace("TouchScreen::DetectGesture()");
    
    if (events.size() < 2)
        return 0;
    
    // Simple gesture detection - can be enhanced
    const TouchEvent &first = events[0];
    const TouchEvent &last = events[events.size() - 1];
    
    int delta_x = last.x - first.x;
    int delta_y = last.y - first.y;
    int distance = static_cast<int>(std::sqrt(delta_x * delta_x + delta_y * delta_y));
    
    // Detect swipe gestures
    if (distance > 50) // Minimum swipe distance
    {
        if (std::abs(delta_x) > std::abs(delta_y))
        {
            // Horizontal swipe
            return delta_x > 0 ? 1 : 2; // Right or left swipe
        }
        else
        {
            // Vertical swipe
            return delta_y > 0 ? 3 : 4; // Down or up swipe
        }
    }
    
    return 0; // No gesture detected
}

// Calibration methods
int TouchScreen::SetCalibration(const TouchCalibration &cal)
{
    FnTrace("TouchScreen::SetCalibration()");
    
    calibration = cal;
    return 0;
}

int TouchScreen::GetCalibration(TouchCalibration &cal) const
{
    FnTrace("TouchScreen::GetCalibration()");
    
    cal = calibration;
    return 0;
}

int TouchScreen::AutoCalibrate()
{
    FnTrace("TouchScreen::AutoCalibrate()");
    
    // Simple auto-calibration - can be enhanced
    calibration.offset_x = 0;
    calibration.offset_y = 0;
    calibration.scale_x = 1.0f;
    calibration.scale_y = 1.0f;
    calibration.rotation = 0;
    calibration.calibrated = true;
    
    return 0;
}

int TouchScreen::SaveCalibration(const char* filename) const
{
    FnTrace("TouchScreen::SaveCalibration()");
    
    std::ofstream file(filename);
    if (!file.is_open())
        return -1;
    
    file << calibration.offset_x << " " << calibration.offset_y << " "
         << calibration.scale_x << " " << calibration.scale_y << " "
         << calibration.rotation << " " << calibration.calibrated << '\n';
    
    file.close();
    return 0;
}

int TouchScreen::LoadCalibration(const char* filename)
{
    FnTrace("TouchScreen::LoadCalibration()");
    
    std::ifstream file(filename);
    if (!file.is_open())
        return -1;
    
    file >> calibration.offset_x >> calibration.offset_y
         >> calibration.scale_x >> calibration.scale_y
         >> calibration.rotation >> calibration.calibrated;
    
    file.close();
    return 0;
}

// Configuration methods
int TouchScreen::SetMultiTouchEnabled(bool enabled)
{
    FnTrace("TouchScreen::SetMultiTouchEnabled()");
    
    enable_multi_touch = enabled;
    return 0;
}

int TouchScreen::SetGesturesEnabled(bool enabled)
{
    FnTrace("TouchScreen::SetGesturesEnabled()");
    
    enable_gestures = enabled;
    return 0;
}

int TouchScreen::SetPressureSensitivityEnabled(bool enabled)
{
    FnTrace("TouchScreen::SetPressureSensitivityEnabled()");
    
    enable_pressure_sensitivity = enabled;
    return 0;
}

int TouchScreen::SetTouchTimeout(int timeout_ms)
{
    FnTrace("TouchScreen::SetTouchTimeout()");
    
    touch_timeout_ms = timeout_ms;
    return 0;
}

// Utility methods
int TouchScreen::GetTouchHistory(std::vector<TouchEvent> &history) const
{
    FnTrace("TouchScreen::GetTouchHistory()");
    
    history = touch_history;
    return static_cast<int>(history.size());
}

int TouchScreen::ClearTouchHistory()
{
    FnTrace("TouchScreen::ClearTouchHistory()");
    
    touch_history.clear();
    return 0;
}

int TouchScreen::GetDeviceCapabilities() const
{
    FnTrace("TouchScreen::GetDeviceCapabilities()");
    
    int capabilities = 0;
    if (enable_multi_touch) capabilities |= 0x01;
    if (enable_gestures) capabilities |= 0x02;
    if (enable_pressure_sensitivity) capabilities |= 0x04;
    if (calibration.calibrated) capabilities |= 0x08;
    
    return capabilities;
}
