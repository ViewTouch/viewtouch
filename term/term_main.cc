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
 * term_main.cc - revision 28 (10/7/98)
 * Terminal Display startup
 */

#include "utility.hh"
#include "term_view.hh"
#include "remote_link.hh"
#include "touch_screen.hh"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>
#include <Xm/Xm.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef LINUX
#define TS_PORT "/dev/ttyS0"
#else
#define TS_PORT "/dev/ttyd0"
#endif

/**** Main ****/
int main(int argc, const genericChar **argv)
{
    genericChar socket_file[256] = "";
    struct sockaddr_un server_adr;
    int val;
    int is_local = 0;
    int term_hardware = 0;
    genericChar display[256] = "";
    TouchScreen *ts = NULL;
    int set_width = -1;
    int set_height = -1;

    if (argc >= 2)
    {
        if (strcmp(argv[1], "version") == 0)
        {
            // return version for vt_update
            printf("1\n");
            return 0;
        }
        strcpy(socket_file, argv[1]);
    }

    SocketNo = socket(AF_UNIX, SOCK_STREAM, 0);
    if (SocketNo <= 0)
    {
        fprintf(stderr, "Term: Failed to open socket");
        exit(1);
    }

    XtToolkitInitialize();
    //FIX:  this should be some sort of polling loop; give up if we've waited too
    //long without a connection, but simply sleeping for one second and hoping
    //the connection will be there ignores all sorts of timing issues.
    sleep(1);  // give server time to setup connection

    server_adr.sun_family = AF_UNIX;
    strcpy(server_adr.sun_path, socket_file);

    if (connect(SocketNo, (struct sockaddr *) &server_adr,
                SUN_LEN(&server_adr)) < 0)
    {
        fprintf(stderr, "Term: Can't connect to server (error %d)\n", errno);
        close(SocketNo);
        exit(1);
    }

    val = 16384;
    setsockopt(SocketNo, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
    val = 32768;
    setsockopt(SocketNo, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));

    if (argc >= 3)
        term_hardware = atoi(argv[2]);

#ifdef USE_TOUCHSCREEN
    if (argc >= 4)
    {
        sprintf(display, "%s", argv[3]);
        ts = new TouchScreen(argv[3], 87); // explora serial port
    }
    else
    {
        //FIX should do a port scan here to find out which serial
        //port to use
        ts = new TouchScreen(TS_PORT);
        is_local = 1;  // deprecated method
    }
#else
    if (argc >= 4)
    {
        sprintf(display, "%s", argv[3]);
    }
    else
    {
        is_local = 1;  // deprecated method
    }
#endif

    if (argc >= 5)
        is_local = atoi(argv[4]);
    if (argc >= 6)
        set_width = atoi(argv[5]);
    if (argc >= 7)
        set_height = atoi(argv[6]);

    if (strchr(display, ':') == NULL)
        strcat(display, ":0");

    // if OpenTerm() returns there must be an error
    if (OpenTerm(display, ts, is_local, 0, set_width, set_height))
        return 1;

    if (SocketNo > 0)
    {
        close(SocketNo);
    }
    if (ts)
    {
        delete ts;
    }
    return KillTerm();
}
