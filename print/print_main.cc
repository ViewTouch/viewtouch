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

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef BSD
#define DEVPORT  "/dev/lpt0"
#else
#define DEVPORT  "/dev/lp0"
#endif

char PrinterDevName[STRLENGTH] = DEVPORT;  // the parallel device file
int  InetPortNumber            = 65530;    // the socket on which to listen
int  Verbose                   = 0;        // verbose mode


/*********************************************************************
 * PROTOTYPES
 ********************************************************************/
int  PrintFromRemote(int my_socket, int printer);
int  ParseArguments(int argc, const char *argv[]);
void ShowHelp(const char *progname);


/*********************************************************************
 * MAIN LOOP
 ********************************************************************/
int main(int argc, const char *argv[])
{
    int my_socket;
    int lock;
    int printer;
    int connection;
    char buffer[STRLENGTH];

    // parse the arguments for printer device
    ParseArguments(argc, argv);
    if (Verbose)
    {
        printf("Listening on port %d\n", InetPortNumber);
        printf("Writing to printer at %s\n", PrinterDevName);
    }

    // listen for connections
    my_socket = Listen(InetPortNumber);
    while (my_socket > -1)
    {
        if (Verbose)
            printf("Waiting to accept connection...\n");
        connection = Accept(my_socket);
        if (connection > -1)
        {
            lock = LockDevice(PrinterDevName);
            if (lock > 0)
            {
                printer = open(PrinterDevName, O_WRONLY | O_APPEND);
                if (printer >= 0)
                {
                    PrintFromRemote(connection, printer);
                    if (Verbose)
                        printf("Closing Printer\n");
                    close(printer);
                }
                else
                {
                    snprintf(buffer, STRLENGTH, "Failed to open %s", PrinterDevName);
                    perror(buffer);
                }
                UnlockDevice(lock);
            }
            if (Verbose)
                printf("Closing socket\n");
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
int PrintFromRemote(int my_socket, int printer)
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
void ShowHelp(const char *progname)
{
    printf("\n");
    printf("Usage:  %s [OPTIONS]\n", progname);
    printf("  -d<device>  Printer device (default %s)\n", DEVPORT);
    printf("  -h          Show this help screen\n");
    printf("  -p<port>    Set the listening port (default %d)\n", InetPortNumber);
    printf("  -v          Verbose mode\n");
    printf("\n");
    printf("Note:  there can be no spaces between an option and the associated\n");
    printf("argument.  AKA, it's \"-p6555\" not \"-p 6555\".\n");
    printf("\n");
    exit(1);
}

/****
 * ParseArguments: Walk through the arguments setting global
 *   variables as necessary.
 ****/
int ParseArguments(int argc, const char *argv[])
{
    const char *arg;
    int idx = 1;  // first command line argument past binary name
    int retval = 0;

    while (idx < argc)
    {
        arg = argv[idx];
        if (arg[0] == '-')
        {
            switch (arg[1])
            {
            case 'd':
                strncpy(PrinterDevName, &arg[2], STRLENGTH);
                break;
            case 'h':
                ShowHelp(argv[0]);
                break;
            case 'p':
                InetPortNumber = atoi(&arg[2]);
            case 'v':
                Verbose = 1;
                break;
            }
        }
        idx += 1;
    }
    return retval;
}
