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
 * cdu_att.cc  Separate file for storing the various serial port attributes
 *  (8N1, et al) for the CDU devices.
 */

#include <termios.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

int EpsonSetAttributes(int fd)
{
    struct termios options;

    tcgetattr(fd, &options);
    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);
    // configure for 8N1, RTS/CTS flow control
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag |= CRTSCTS;
    options.c_cflag |= CLOCAL;
    tcsetattr(fd, TCSANOW, &options);

    return 0;
}

int BA63SetAttributes(int fd)
{
    struct termios options;

    tcgetattr(fd, &options);
    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);
    // configure for 8O1, RTS/CTS flow control
    options.c_cflag |= PARENB;
    options.c_cflag |= PARODD;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag |= CRTSCTS;
    options.c_cflag |= CLOCAL;
    // INPCK may be necessary if/when we read in the status information
    // from the CDU
    options.c_iflag |= INPCK;
    tcsetattr(fd, TCSANOW, &options);
    tcflush(fd, TCIOFLUSH);
    return 0;
}
