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
#define MAX_TRIES 8

/**** TouchScreen Class ****/
// Constructors
TouchScreen::TouchScreen(const char* device)
{
	strcpy(INIT, "\001PN819600\n");
	strcpy(PING, "\001Z\n");
	strcpy(RESET, "\001R\n");  // "destructive" command
    strcpy(PARAM_LOCK, "\001PL\n"); // PL = "Parameter Lock"
	strcpy(FORMAT_HEX, "\001FH\n");
	strcpy(FORMAT_DEC, "\001FD\n");
	strcpy(MODE_POINT, "\001MP\n");
    strcpy(MODE_STREAM, "\001MS\n");
    strcpy(MODE_CALIBRATE, "\001CI\n"); // CI = "Calibrate Interactive"
    strcpy(AUTOBAUD_DISABLE, "\001AD\n");

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
	strcpy(INIT, "\001PN819600\n");
	strcpy(PING, "\001Z\n");
	strcpy(RESET, "\001R\n");  // "destructive" command
    strcpy(PARAM_LOCK, "\001PL\n"); // PL = "Parameter Lock"
	strcpy(FORMAT_HEX, "\001FH\n");
	strcpy(FORMAT_DEC, "\001FD\n");
	strcpy(MODE_POINT, "\001MP\n");
    strcpy(MODE_STREAM, "\001MS\n");
    strcpy(MODE_CALIBRATE, "\001CI\n"); // CI = "Calibrate Interactive"
    strcpy(AUTOBAUD_DISABLE, "\001AD\n");

    x_res = 1024;
    y_res = 1024; 
    last_x = 0;
    last_y = 0;

    device_no = 0;
    port = p;
    host.Set(h);
    failed = Connect(1);
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

    genericChar str[256];
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
        if (inaddr.sin_addr.s_addr == (Ulong) -1)
        {
            struct hostent *hp = gethostbyname(host.Value());
            if (hp == NULL || hp->h_addrtype != AF_INET)
            {
				sprintf(str, "Can't resolve name '%s'", host.Value());
				error.Set(str);
                fprintf(stderr, "%s",str);
				return 1;
            }
            bcopy(hp->h_addr, &inaddr.sin_addr.s_addr, hp->h_length);
        }

        device_no = socket(AF_INET, SOCK_STREAM, 0);
        if (device_no < 0)
        {
            error.Set("Can't open socket");
            fprintf(stderr,"%s",str);
            device_no = 0;
            return 1;
        }

        if (connect(device_no, (const sockaddr *) &inaddr, sizeof(inaddr)) < 0)
        {
            sprintf(str, "Connection refused with '%s'", host.Value());
            fprintf(stderr,"%s", str);
            error.Set(str);
            close(device_no);
            device_no = 0;
            return 1;
        }

        int flag = 1;
        setsockopt(device_no, IPPROTO_TCP, TCP_NODELAY, (void *)&flag, sizeof(flag));
        fcntl(device_no, F_SETFL, O_NDELAY);
    }
    return Init(boot);
}

int TouchScreen::SetMode(const char* mode)
{
	if( (strcmp("POINT", mode) == 0) && device_no > 0)
	{
		const char* modeList[] = { FORMAT_HEX, MODE_POINT, AUTOBAUD_DISABLE, PARAM_LOCK };
		for (int i = 0; i < 4; i++)
        {
			write(device_no, modeList[i], strlen(modeList[i]));
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
	const char* modeList[] = { INIT, AUTOBAUD_DISABLE, FORMAT_HEX, MODE_POINT, PARAM_LOCK };
	for(int i=0; i<5; i++)
	{
		write(device_no, modeList[i], strlen(modeList[i]));
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
    while (c != '\n' && c != '\r' && size < (int) sizeof(buffer));

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
    while (c != '\n' && c != '\r' && size < (int)sizeof(buffer));
  
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

int TouchScreen::Calibrate()
{
    if (device_no <= 0)
    {
        return -1;
    }

    Flush();
    write(device_no, MODE_CALIBRATE, strlen(MODE_CALIBRATE));  // enter calibrate mode
    return 0;
}

int TouchScreen::Reset()
{
    if (device_no <= 0)
    {
        return -1;
    }

    Flush();
    write(device_no, RESET, strlen(RESET));  // reset touch screen

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

int TouchScreen::Flush()
{
    if (device_no <= 0)
    {
        return -1;
    }

    int result;
    do
    {
        result = read(device_no, buffer, 1);
    } while (result > 0);

    size = 0;
    return 0;
}
