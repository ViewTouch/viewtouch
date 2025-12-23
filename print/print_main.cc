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
 * print_main.cc - revision 8 (9/8/98)
 * Startup for printing process 'vt_print'
 */

#include <cstring>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <memory>

#include "socket.hh"
#include "utility.hh"

#include <string>
#include <iostream>
#include <sstream>
#include <vector>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef BSD
#define DEVPORT  "/dev/lpt0"
#else
#define DEVPORT  "/dev/lp0"
#endif

constexpr int default_port_number = 65530; // the default socket on which to listen

// Global flag for graceful shutdown
static volatile sig_atomic_t g_shutdown_requested = 0;

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
Parameter ParseArguments(const int argc, const char* const argv[]);
void ShowHelp(const std::string &progname);
void SignalHandler(int signal);


/*********************************************************************
 * MAIN LOOP
 ********************************************************************/
int main(int argc, const char* argv[])
{
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);

    int my_socket = -1;
    int lock = -1;
    int printer = -1;
    int connection = -1;

    // parse the arguments for printer device
    Parameter param = ParseArguments(argc, argv);
    if (param.verbose)
    {
        std::cout << "Listening on port " << param.InetPortNumber << std::endl;
        std::cout << "Writing to printer at " << param.PrinterDevName << std::endl;
    }

    // listen for connections
    my_socket = Listen(param.InetPortNumber);
    if (my_socket < 0)
    {
        std::cerr << "Failed to create listening socket on port " << param.InetPortNumber << std::endl;
        return 1;
    }

    while (my_socket > -1 && !g_shutdown_requested)
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
                    printer = -1;
                }
                else
                {
                    const std::string msg =
                            std::string("Failed to open ") + param.PrinterDevName;
                    perror(msg.c_str());
                }
                UnlockDevice(lock);
                lock = -1;
            }
            if (param.verbose)
                std::cout << "Closing socket" << std::endl;
            close(connection);
            connection = -1;
        }
    }

    // Cleanup on exit
    if (my_socket > -1)
    {
        close(my_socket);
    }
    if (param.verbose && g_shutdown_requested)
    {
        std::cout << "Shutdown requested, exiting gracefully..." << std::endl;
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
    std::vector<char> buffer(STRLENGTH);
    ssize_t bytes_read = 0;
    ssize_t bytes_written = 0;

    while (!g_shutdown_requested)
    {  // read from socket, print to printer until we get an error
        bytes_read = recv(my_socket, buffer.data(), STRLENGTH, 0);
        if (bytes_read > 0)
        {
            bytes_written = write(printer, buffer.data(), bytes_read);
            if (bytes_written < 0)
            {
                perror("Failed to write to printer");
                return -1;
            }
            else if (bytes_written != bytes_read)
            {
                std::cerr << "Warning: Partial write to printer (" << bytes_written 
                         << " of " << bytes_read << " bytes)" << std::endl;
            }
        }
        else if (bytes_read == 0)
        {
            // Connection closed by client
            break;
        }
        else if (bytes_read < 0)
        {
            perror("Failed to read from socket");
            return -1;
        }
    }
    return g_shutdown_requested ? -1 : 0;
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
Parameter ParseArguments(const int argc, const char* const argv[])
{
    Parameter param;
    // start at 1, first command line argument past binary name
    for (int idx = 1; idx < argc; idx++)
    {
        const std::string arg = argv[idx];
        if (arg.length() < 2)
        {
            std::cout << "Invalid argument format: '" << arg << "'" << std::endl;
            ShowHelp(argv[0]);
        }
        
        const std::string prefix = arg.substr(0, 2);
        const std::string val = arg.substr(2);
        
        if (prefix == "-d")
        {
            if (val.empty())
            {
                std::cout << "Error parsing argument '" << arg << "'."
                          << " No printer specified" << std::endl;
                ShowHelp(argv[0]);
            }
            param.PrinterDevName = val;
        } 
        else if (prefix == "-h")
        {
            ShowHelp(argv[0]);
        } 
        else if (prefix == "-p")
        {
            if (val.empty())
            {
                std::cout << "Error parsing argument '" << arg << "'."
                          << " No port number specified" << std::endl;
                ShowHelp(argv[0]);
            }
            std::istringstream ss(val);
            ss >> param.InetPortNumber;
            if (ss.fail() || param.InetPortNumber <= 0 || param.InetPortNumber > 65535)
            {
                std::cout << "Invalid port number: " << val 
                         << " (must be between 1 and 65535)" << std::endl;
                ShowHelp(argv[0]);
            }
        } 
        else if (prefix == "-v")
        {
            param.verbose = true;
        } 
        else
        {
            std::cout << "Unrecognized parameter '" << arg << "'" << std::endl;
            ShowHelp(argv[0]);
        }
    }
    return param;
}

/****
 * SignalHandler: Handle shutdown signals gracefully
 ****/
void SignalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        g_shutdown_requested = 1;
    }
}
