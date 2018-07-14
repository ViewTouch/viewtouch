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
 * print_main.cc - revision 8 (9/8/98)
 * Startup for printing process 'vt_print'
 */

#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

#include "socket.hh"
#include "utility.hh"

#include <string>
#include <iostream>
#include <sstream>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef BSD
#define DEVPORT  "/dev/lpt0"
#else
#define DEVPORT  "/dev/lp0"
#endif

constexpr int default_port_number = 65530; // the default socket on which to listen

struct Parameter
{
    std::string PrinterDevName = DEVPORT;  // the parallel device file
    int  InetPortNumber        = default_port_number; // the socket on which to listen
    bool verbose               = false;    // verbose mode
};

/*********************************************************************
 * PROTOTYPES
 ********************************************************************/
int  PrintFromRemote(const int my_socket, const int printer);
Parameter ParseArguments(const int argc, const char* argv[]);
void ShowHelp(const std::string &progname);


/*********************************************************************
 * MAIN LOOP
 ********************************************************************/
int main(int argc, const char* argv[])
{
    int my_socket;
    int lock;
    int printer;
    int connection;

    // parse the arguments for printer device
    Parameter param = ParseArguments(argc, argv);
    if (param.verbose)
    {
        std::cout << "Listening on port " << param.InetPortNumber << std::endl;
        std::cout << "Writing to printer at " << param.PrinterDevName << std::endl;
    }
    exit(2);

    // listen for connections
    my_socket = Listen(param.InetPortNumber);
    while (my_socket > -1)
    {
        if (param.verbose)
            std::cout << "Waiting to accept connection..." << std::endl;
        connection = Accept(my_socket);
        if (connection > -1)
        {
            lock = LockDevice(param.PrinterDevName.c_str());
            if (lock > 0)
            {
                printer = open(param.PrinterDevName.c_str(), O_WRONLY | O_APPEND);
                if (printer >= 0)
                {
                    PrintFromRemote(connection, printer);
                    if (param.verbose)
                        std::cout << "Closing Printer" << std::endl;
                    close(printer);
                }
                else
                {
                    const std::string msg =
                            std::string("Failed to open ") + param.PrinterDevName;
                    perror(msg.c_str());
                }
                UnlockDevice(lock);
            }
            if (param.verbose)
                std::cout << "Closing socket" << std::endl;
            close(connection);
        }
    }

    return 0;
}


/*********************************************************************
 * SUBROUTINES
 ********************************************************************/

/****
 * PrintFromRemote:  given an open socket and an open printer connection,
 *   reads from the socket and passes the data to the printer.
 ****/
int PrintFromRemote(const int my_socket, const int printer)
{
    char buffer[STRLENGTH];
    int active = 1;

    while (active > 0)
    {  // read from socket, print to printer until we get an error
        active = recv(my_socket, buffer, STRLENGTH, 0);
        if (active > 0)
        {
            active = write(printer, buffer, active);  // active contains bytes read
            if (active < 0)
                perror("Failed to write");
        }
        else if (active < 0)
            perror("Failed to read");
    }
    return active;
}

/****
 * ShowHelp:
 ****/
void ShowHelp(const std::string &progname)
{
    std::cout << std::endl
              << "Usage:  " << progname << " [OPTIONS]" << std::endl
              << "  -d<device>  Printer device (default " << DEVPORT << ")" << std::endl
              << "  -h          Show this help screen" << std::endl
              << "  -p<port>    Set the listening port (default " << default_port_number << ")" << std::endl
              << "  -v          Verbose mode" << std::endl
              << std::endl
              << "Note:  there can be no spaces between an option and the associated" << std::endl
              << "argument.  AKA, it's \"-p6555\" not \"-p 6555\"." << std::endl
              << std::endl;
    exit(1);
}

/****
 * ParseArguments: Walk through the arguments setting global
 *   variables as necessary.
 ****/
Parameter ParseArguments(const int argc, const char* argv[])
{
    Parameter param;
    // start at 1, first command line argument past binary name
    for (int idx = 1; idx < argc; idx++)
    {
        std::string arg = argv[idx];
        std::string prefix = arg.substr(0,2);
        std::string val = arg.substr(2);
        if (prefix == "-d")
        {
            if (val.empty())
            {
                std::cout << "Error parsing argument '" << arg << "'."
                          << " No printer specified" << std::endl;
                ShowHelp(argv[0]);
            }
            param.PrinterDevName = val;
        } else if (prefix == "-h")
        {
            ShowHelp(argv[0]);
        } else if (prefix == "-p")
        {
            if (val.empty())
            {
                std::cout << "Error parsing argument '" << arg << "'."
                          << " No port number specified" << std::endl;
                ShowHelp(argv[0]);
            }
            std::istringstream ss(val);
            ss >> param.InetPortNumber;
            if (ss.fail())
            {
                std::cout << "Can't parse port number: " << val << std::endl;
                ShowHelp(argv[0]);
            }
        } else if (prefix == "-v")
        {
            param.verbose = true;
        } else
        {
            std::cout << "Unrecognized parameter '" << arg << "'" << std::endl;
            ShowHelp(argv[0]);
        }
    }
    return param;
}
