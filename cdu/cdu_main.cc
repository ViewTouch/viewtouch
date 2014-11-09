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
 * cdu_main.cc  Small utility to receive CDU messages from a TCP/IP source
 *  and print them to the local device.
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "utility.hh"
#include "cdu_att.hh"
#include "socket.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef BSD
#define DEVPORT  "/dev/ttyd0"
#else
#define DEVPORT  "/dev/ttyS0"
#endif

// the supported types of devices
#define CDU_EPSON        1
#define CDU_BA63         2

char CDUDevName[STRLENGTH]     = DEVPORT;  // the serial device file
int  CDUDevType                = CDU_BA63; // default to wincor CDU
int  InetPortNumber            = CDU_PORT; // the socket on which to listen
int  Verbose                   = 0;        // verbose mode


/*********************************************************************
 * PROTOTYPES
 ********************************************************************/
int  ParseArguments(int argc, const char *argv[]);
int  SetAttributes(int fd);
void ShowHelp(const char *progname);
int  PrintFromRemote(int my_socket, int serial_port);


/*********************************************************************
 * MAIN LOOP
 ********************************************************************/
int main(int argc, const char *argv[])
{
    int my_socket;
    int lock;
    int connection;
    int ser_fd;  // file descriptor for the serial port
    char buffer[STRLENGTH];

    // parse the arguments for printer device
    ParseArguments(argc, argv);
    if (Verbose)
    {
        printf("Listening on port %d\n", InetPortNumber);
        printf("Writing to CDU at %s\n", CDUDevName);
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
            if (Verbose)
                printf("Got connection...\n");
            lock = LockDevice(CDUDevName);
            if (lock > 0)
            {
                ser_fd = open(CDUDevName, O_RDWR | O_NOCTTY | O_NDELAY);
                if (ser_fd > -1)
                {
                    if (Verbose)
                        printf("Locked and opened device\n");
                    SetAttributes(ser_fd);
                    PrintFromRemote(connection, ser_fd);
                    close(ser_fd);
                }
                else
                {
                    snprintf(buffer, STRLENGTH, "Could not write %s", CDUDevName);
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
 * ShowHelp:
 ****/
void ShowHelp(const char *progname)
{
    printf("\n");
    printf("Usage:  %s [OPTIONS]\n", progname);
    printf("  -d<device>  Serial device (default %s)\n", CDUDevName);
    printf("  -h          Show this help screen\n");
    printf("  -p<port>    Set the listening port (default %d)\n", InetPortNumber);
    printf("  -t<type>    Set the device type (default %d)\n", CDUDevType);
    printf("  -v          Verbose mode\n");
    printf("\n");
    printf("Note:  there can be no spaces between an option and the associated\n");
    printf("argument.  AKA, it's \"-p6555\" not \"-p 6555\".\n");
    printf("\n");
    printf("The supported CDU devices are:\n");
    printf("Epson protocol = %d\n", CDU_EPSON);
    printf("BA63 (Wincor)  = %d\n", CDU_BA63);
    printf("\n");
    exit(1);
}

/****
 * ParseArguments:
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
                strncpy(CDUDevName, &arg[2], STRLENGTH);
                break;
            case 'h':
                ShowHelp(argv[0]);
                break;
            case 'p':
                InetPortNumber = atoi(&arg[2]);
                break;
            case 't':
                CDUDevType = atoi(&arg[2]);
                break;
            case 'v':
                Verbose = 1;
                break;
            }
        }
        idx += 1;
    }
    return retval;
}

/****
 * SetAttributes:
 ****/
int SetAttributes(int fd)
{
    int retval = 1;
    switch (CDUDevType)
    {
    case CDU_EPSON:
        retval = EpsonSetAttributes(fd);
        break;
    case CDU_BA63:
        retval = BA63SetAttributes(fd);
        break;
    default:
        fprintf(stderr, "Unknown device type:  %d\n", CDUDevType);
        fprintf(stderr, "Did not initialize serial port!\n");
        break;
    }
    return retval;
}

/****
 * PrintFromRemote:  given an open socket and an open serial connection,
 *   reads from the socket and passes the data to the printer.
 ****/
int PrintFromRemote(int my_socket, int serial_port)
{
    char buffer[STRLENGTH];
    int active = 1;

    while (active > 0)
    {  // read from socket, print to serial port until we get an error
        active = recv(my_socket, buffer, STRLENGTH, 0);
        if (active > 0)
        {
            active = write(serial_port, buffer, active);  // active contains bytes read
            if (active < 0)
                perror("Failed to write");
        }
        else if (active < 0)
            perror("Failed to read");
    }
    return active;
}

