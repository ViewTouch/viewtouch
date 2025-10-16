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
 * term_main.cc - revision 28 (10/7/98)
 * Terminal Display startup
 */

#include "utility.hh"
#include "term_view.hh"
#include "remote_link.hh"
#include "touch_screen.hh"
#include "src/utils/vt_logger.hh"
#include "version/vt_version_info.hh"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>
#include <Xm/Xm.h>
#include <memory>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef LINUX
#define TS_PORT "/dev/ttyS0"
#else
#define TS_PORT "/dev/ttyd0"
#endif

/**** Main ****/
int main(int argc, const genericChar* *argv)
{
    // Initialize modern logging
#ifdef DEBUG
    vt::Logger::Initialize("/var/log/viewtouch", "debug", true, true);
#else
    vt::Logger::Initialize("/var/log/viewtouch", "info", false, true);
#endif
    vt::Logger::info("ViewTouch Terminal (vt_term) starting - Version {}", 
                     viewtouch::get_version_short());

    std::string socket_file = "";
    struct sockaddr_un server_adr;
    int val;
    int is_local = 0;
    int term_hardware = 0;
    std::array<genericChar, 256> display = {};
    std::unique_ptr<TouchScreen> ts = nullptr;
    int set_width = -1;
    int set_height = -1;

    if (argc >= 2)
    {
        if (strcmp(argv[1], "version") == 0)
        {
            // return version for vt_update
            vt::Logger::info("Version check requested");
            printf("1\n");
            vt::Logger::Shutdown();
            return 0;
        }
        socket_file = argv[1];
        vt::Logger::debug("Using socket file: {}", socket_file);
    }

    SocketNo = socket(AF_UNIX, SOCK_STREAM, 0);
    if (SocketNo <= 0)
    {
        vt::Logger::critical("Failed to open socket - errno: {}", errno);
        fprintf(stderr, "Term: Failed to open socket");
        exit(1);
    }
    vt::Logger::debug("Socket opened: {}", SocketNo);

    XtToolkitInitialize();
    //FIX:  this should be some sort of polling loop; give up if we've waited too
    //long without a connection, but simply sleeping for one second and hoping
    //the connection will be there ignores all sorts of timing issues.
    sleep(1);  // give server time to setup connection

    server_adr.sun_family = AF_UNIX;
    strcpy(server_adr.sun_path, socket_file.c_str());

    vt::Logger::debug("Connecting to server socket: {}", socket_file);
    if (connect(SocketNo, reinterpret_cast<struct sockaddr*>(&server_adr),
                SUN_LEN(&server_adr)) < 0)
    {
        vt::Logger::critical("Can't connect to server - errno: {} ({})", errno, socket_file);
        fprintf(stderr, "Term: Can't connect to server (error %d)\n", errno);
        close(SocketNo);
        exit(1);
    }
    vt::Logger::info("Connected to server successfully");

    val = 16384;
    setsockopt(SocketNo, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val));
    val = 32768;
    setsockopt(SocketNo, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val));

    if (argc >= 3)
        term_hardware = atoi(argv[2]);

#ifdef USE_TOUCHSCREEN
    if (argc >= 4)
    {
        sprintf(display.data(), "%s", argv[3]);
        ts = std::make_unique<TouchScreen>(argv[3], 87); // explora serial port
    }
    else
    {
        //FIX should do a port scan here to find out which serial
        //port to use
        ts = std::make_unique<TouchScreen>(TS_PORT);
        is_local = 1;  // deprecated method
    }
#else
    if (argc >= 4)
    {
        sprintf(display.data(), "%s", argv[3]);
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

    if (strchr(display.data(), ':') == NULL)
        strcat(display.data(), ":0");

    // if OpenTerm() returns there must be an error
    vt::Logger::info("Opening terminal - Display: {}, Hardware: {}", display.data(), term_hardware);
    try {
        if (OpenTerm(display.data(), ts.get(), is_local, 0, set_width, set_height)) {
            vt::Logger::error("OpenTerm failed");
            vt::Logger::Shutdown();
            return 1;
        }
    } catch (const std::exception& e) {
        vt::Logger::critical("Exception in OpenTerm: {}", e.what());
        fprintf(stderr, "Error in OpenTerm: %s\n", e.what());
        vt::Logger::Shutdown();
        return 1;
    }

    // After loading fonts and before entering the event loop, call ReloadFonts to ensure all Xft fonts are up to date
    TerminalReloadFonts();

    if (SocketNo > 0)
    {
        close(SocketNo);
    }
    ts.reset();
    return KillTerm();
}
