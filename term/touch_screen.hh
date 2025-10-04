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
 * touch_screen.hh - revision 21 (4/23/97)
 * functions for using the touch-screen services
 */

#ifndef _TOUCH_SCREEN_HH
#define _TOUCH_SCREEN_HH

#include <string>
#include <array>
#include <vector>
#include "utility.hh"


/**** Definitions *****/
#define TOUCH_DOWN 1
#define TOUCH_HOLD 2
#define TOUCH_UP   3

// Enhanced touch event types
#define TOUCH_MULTI_DOWN 4
#define TOUCH_MULTI_UP   5
#define TOUCH_MOVE       6
#define TOUCH_GESTURE    7

// Touch event structure for improved handling
struct TouchEvent {
    int x, y;
    int mode;
    int pressure;
    int touch_id;
    unsigned long timestamp;
    
    TouchEvent() : x(0), y(0), mode(0), pressure(0), touch_id(0), timestamp(0) {}
    TouchEvent(int tx, int ty, int m, int p = 0, int id = 0) 
        : x(tx), y(ty), mode(m), pressure(p), touch_id(id), timestamp(0) {}
};

// Calibration data structure
struct TouchCalibration {
    int offset_x, offset_y;
    float scale_x, scale_y;
    int rotation;
    bool calibrated;
    
    TouchCalibration() : offset_x(0), offset_y(0), scale_x(1.0f), scale_y(1.0f), 
                        rotation(0), calibrated(false) {}
};

/**** Types ****/
class TouchScreen
{
    std::array<genericChar, 64> buffer;  // input buffer
    int  size;        // current size of input buffer

	//macros for the microtouch command codes
	std::string INIT;
	std::string PING;
	std::string RESET;
	std::string PARAM_LOCK;
	std::string FORMAT_HEX;
	std::string FORMAT_DEC;
	std::string MODE_POINT;
	std::string MODE_STREAM;
	std::string MODE_CALIBRATE;
	std::string AUTOBAUD_DISABLE;

    // Enhanced touch handling
    TouchCalibration calibration;
    std::vector<TouchEvent> touch_history;
    int max_touch_history;
    int touch_timeout_ms;
    bool enable_multi_touch;
    bool enable_gestures;
    bool enable_pressure_sensitivity;

public:
    int device_no;    // internal device id number
    int failed;       // boolean - did object creation/setup fail?
    int x_res, y_res; // horizontal/vertical resolution of touch screen
    int last_x, last_y;
    Str host;
    int port;
    TimeInfo last_reset;
    Str error;        // holds description of any resulting errors

    // Constructors
    TouchScreen(const char* device);          // local device
    TouchScreen(const char* host, int port);  // remote device
    TouchScreen(const TouchScreen& other) = delete;  // Disable copy constructor
    TouchScreen(TouchScreen&& other) noexcept;       // Move constructor
    TouchScreen& operator=(const TouchScreen& other) = delete;  // Disable copy assignment
    TouchScreen& operator=(TouchScreen&& other) noexcept;       // Move assignment
    // Destructor
    ~TouchScreen();

    // Member Functions
    int ReadTouch(int &x, int &y, int &mode);
    // Gives coords of last touch & mode of touch
    // returns -1 error, 0 no touch, 1 touch recorded

    // Enhanced touch methods
    int ReadTouchEvent(TouchEvent &event);
    int ReadMultiTouch(std::vector<TouchEvent> &events);
    int ProcessTouchEvents();
    int ApplyCalibration(int &x, int &y);
    int DetectGesture(const std::vector<TouchEvent> &events);
    
    // Calibration methods
    int SetCalibration(const TouchCalibration &cal);
    int GetCalibration(TouchCalibration &cal) const;
    int AutoCalibrate();
    int SaveCalibration(const char* filename) const;
    int LoadCalibration(const char* filename);
    
    // Configuration methods
    int SetMultiTouchEnabled(bool enabled);
    int SetGesturesEnabled(bool enabled);
    int SetPressureSensitivityEnabled(bool enabled);
    int SetTouchTimeout(int timeout_ms);
    
    // Utility methods
    int GetTouchHistory(std::vector<TouchEvent> &history) const;
    int ClearTouchHistory();
    int GetDeviceCapabilities() const;

    int Connect(int boot);
    int ReadStatus();   // Reads result of a command (0, 1, -1 none)
    int Init(int boot); // Sets init codes to touchscreen
    int Calibrate() noexcept;    // Sets device into calibration mode
    int Reset() noexcept;        // Resets touch screen device
    int Flush() noexcept;        // Clears device buffer
	int SetMode(const char* mode) noexcept;
};

#endif
