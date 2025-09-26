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
 * touch_screen.hh - revision 21 (4/23/97)
 * functions for using the touch-screen services
 */

#ifndef _TOUCH_SCREEN_HH
#define _TOUCH_SCREEN_HH

#include <string>
#include <array>
#include "utility.hh"


/**** Definitions *****/
#define TOUCH_DOWN 1
#define TOUCH_HOLD 2
#define TOUCH_UP   3

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

    int Connect(int boot);
    int ReadStatus();   // Reads result of a command (0, 1, -1 none)
    int Init(int boot); // Sets init codes to touchscreen
    int Calibrate() noexcept;    // Sets device into calibration mode
    int Reset() noexcept;        // Resets touch screen device
    int Flush() noexcept;        // Clears device buffer
	int SetMode(const char* mode) noexcept;
};

#endif
