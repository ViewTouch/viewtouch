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
 * vt_ccq_pipe.cc
 * Socket<-->serial pipe to allow the CreditCheq application to communicate
 *  with the PINPad.  Just receives from a socket, sends to the serial port,
 *  waits for a response from the serial port, sends the response to the
 *  socket.  And vice versa.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include "vt_ccq_pipe.hh"
#include "socket.hh"
#include "utility.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define SOCK_PORT 9999
#define STX       0x02
#define ETX       0x03
#define ENQ       0x05
#define ACK       0x06
#define NAK       0x15
#define ETB       0x17
#define CAN       0x18

#define ERR_NONE    0
#define ERR_SERVER  1
#define ERR_PINPAD  2
#define ERR_SELECT  3
#define ERR_TIMEOUT 4

#ifdef LINUX
char serial_device[STRLENGTH] = "/dev/ttyS0";
#elif defined BSD
char serial_device[STRLENGTH] = "/dev/ttyd0";
#endif

int  socket_port            = SOCK_PORT;
int  pinpad_port            = 0;
char pinpad_host[STRLENGTH] = "";
int  diagnostics            = 0;
int  use_rtscts             = 0;
int  time_limit             = 0;  // time limit for inactive connection
int  check_interval         = 0;  // how often to check if time limit is exceeded

int  SocketToSerial(int listen_fd, char *pinpad_device);
int  SocketToSocket(int listen_fd, char *host, int port);
int  OpenSerial(char *serial_port);
void ShowHelp(char *progname);
int  ParseArguments(int argc, char *argv[]);
int  ProcessConnection(int sockfd, int serialfd);
int  ReadCmd(int fd, char *dest, int destlen);
int  Read(int fd, char *buffer, int bufflen);
int  Write(int fd, char *buffer, int bufflen);
void PrintRead(char *label, char *buffer, int bufflen);


/*********************************************************************
 * Main Loop
 ********************************************************************/
int main(int argc, char *argv[])
{
    int retval = 0;

    // let's ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);

    ParseArguments(argc, argv);
    if (pinpad_port && pinpad_host[0] != '\0')
        retval = SocketToSocket(socket_port, pinpad_host, pinpad_port);
    else
        retval = SocketToSerial(socket_port, serial_device);

    return retval;
}


/*********************************************************************
 * Subroutines
 ********************************************************************/

/****
 * SocketToSerial:
 ****/
int SocketToSerial(int listen_port, char *pinpad_device)
{
    int      retval = 0;
    int      listen_fd = -1;
    int      pinpad_fd = -1;
    int      server_fd;
    fd_set   in_fds;
    int      nfds = 0;
    struct   timeval timeout;
    char     serial_in[STRLENGTH];
    int      serial_bytes;
    int      reads;
    char     ack_response[]    = { ACK, 0x00 };
    char     server_ip[STRLENGTH];
    int      errval;

    /* now loop forever */
    while (1)
    {
        if (pinpad_fd <= 0)
        {
            pinpad_fd = OpenSerial(pinpad_device);
            tcflush(pinpad_fd, TCIOFLUSH);
            tcsendbreak(pinpad_fd, 0);
            if (diagnostics && pinpad_fd > 0)
                printf("Opened Device %s, File Descriptor %d\n", pinpad_device, pinpad_fd);
            else if (pinpad_fd <= 0)
                sleep(1);
        }
        else if (listen_fd <= 0)
        {
            listen_fd = Listen(listen_port);
            if (diagnostics && listen_fd > 0)
                printf("Listening on Port:  %d\n", listen_port);
            else if (listen_fd <= 0)
            {
                if (errno == EADDRINUSE)
                    exit(1);
                else
                    sleep(1);
            }
        }
        else
        {
            nfds = MAX(nfds, pinpad_fd);
            nfds = MAX(nfds, listen_fd);
            nfds += 1;            
            FD_ZERO(&in_fds);
            FD_SET(pinpad_fd, &in_fds);
            FD_SET(listen_fd, &in_fds);
            timeout.tv_sec = 0;
            timeout.tv_usec = 10;
            reads = select(nfds, &in_fds, NULL, NULL, &timeout);
            if (reads > 0)
            {
                if (FD_ISSET(pinpad_fd, &in_fds))
                {
                    // out here we probably have an ENQ, need to respond with ACK
                    serial_bytes = Read(pinpad_fd, serial_in, STRLENGTH);
                    if (serial_bytes > 0 && serial_in[0] == ENQ)
                    {
                        if (diagnostics)
                            printf("Sending ACK to ENQ\n");
                        serial_bytes = Write(pinpad_fd, ack_response, strlen(ack_response));
                    }
                }
                else if (FD_ISSET(listen_fd, &in_fds))
                {
                    server_fd = Accept(listen_fd, server_ip);
                    if (server_fd >= 0)
                    {
                        if (diagnostics)
                            printf("Accepted a socket from %s\n", server_ip);
                        errval = ProcessConnection(server_fd, pinpad_fd);
                        close(server_fd);
                        if (errval == ERR_PINPAD)
                        {
                            close(pinpad_fd);
                            pinpad_fd = -1;
                        }
                        else if (errval == ERR_TIMEOUT)
                        {
                            printf("Timed out, resetting connection...\n");
                        }
                    }
                }
            }
        }
    }

    if (listen_fd < 0)
        retval += 1;
    if (pinpad_fd < 0)
        retval += 2;

    return retval;
}

/****
 * SocketToSocket:
 ****/
int SocketToSocket(int listen_port, char *host, int port)
{
    int      retval = 0;
    int      listen_fd = -1;
    int      pinpad_fd = -1;
    int      server_fd;
    fd_set   in_fds;
    int      nfds = 0;
    struct   timeval timeout;
    int      reads;
    char     server_ip[STRLENGTH];
    int      errval = 0;

    // now loop forever
    while (1)
    {
        if (pinpad_fd <= 0)
        {
            pinpad_fd = Connect(host, port);
            if (diagnostics && pinpad_fd > 0)
                printf("Connected to:  %s:%d\n", host, port);
            else if (pinpad_fd <= 0)
                sleep(1);
        }
        else if (listen_fd <= 0)
        {
            listen_fd = Listen(listen_port);
            if (diagnostics && listen_fd > 0)
                printf("Listening on Port:  %d\n", listen_port);
            else if (listen_fd <= 0)
            {
                if (errno == EADDRINUSE)
                    exit(1);
                else
                    sleep(1);
            }
        }
        else
        {
            // the server always initiates the connection, so we'll
            // only select() on that descriptor
            nfds = MAX(nfds, listen_fd);
            nfds += 1;
            FD_ZERO(&in_fds);
            FD_SET(listen_fd, &in_fds);
            timeout.tv_sec = 0;
            timeout.tv_usec = 10;
            reads = select(nfds, &in_fds, NULL, NULL, &timeout);
            if (reads > 0)
            {
                if (FD_ISSET(listen_fd, &in_fds))
                {
                    server_fd = Accept(listen_fd, server_ip);
                    if (server_fd >= 0)
                    {
                        if (diagnostics)
                            printf("Accepted a socket from %s\n", server_ip);
                        errval = ProcessConnection(server_fd, pinpad_fd);
                        if (errval == ERR_PINPAD)
                        {
                            close(pinpad_fd);
                            pinpad_fd = -1;
                        }
                        close(server_fd);
                    }
                }
            }
        }
    }

    if (listen_fd < 0)
        retval += 1;
    if (pinpad_fd < 0)
        retval += 2;

    return retval;
}

/****
 * OpenSerial:
 ****/
int OpenSerial(char *serial_port)
{
    int serial_fd = -1;
    struct termios options;
   
    serial_fd = open(serial_port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd > -1)
    {
        // 7E1 4800
        tcgetattr(serial_fd, &options);
        cfsetispeed(&options, B4800);
        cfsetospeed(&options, B4800);
        options.c_cflag |=  PARENB;
        options.c_cflag &= ~PARODD;
        options.c_cflag &= ~CSTOPB;
        options.c_cflag &= ~CSIZE;
        options.c_cflag |=  CS7;
        options.c_cflag &= ~CRTSCTS;
        options.c_cflag |=  CLOCAL;
        options.c_cflag |=  CREAD;
        options.c_lflag &= ~ECHO;
        options.c_lflag &= ~ICANON;
        options.c_lflag &= ~ISIG;
        options.c_lflag &= ~IEXTEN;
        options.c_iflag &= ~BRKINT;
        options.c_iflag &= ~ICRNL;
        options.c_iflag &= ~INPCK;
        options.c_iflag &= ~ISTRIP;
        options.c_iflag &= ~IXON;
        options.c_oflag &= ~OPOST;
        options.c_cc[VMIN] = 1;
        options.c_cc[VTIME] = 0;
        tcsetattr(serial_fd, TCSANOW, &options);
    }
    else
        perror("OpenSerial");

    return serial_fd;
}

/****
 * ShowHelp:
 ****/
void ShowHelp(char *progname)
{
    printf("\n");
    printf("Usage:  %s [OPTIONS]\n", progname);
    printf("  -c<time limit>  How long to wait between timeout checks (default %d seconds)\n", check_interval);
    printf("  -d<device>      Serial device (default %s)\n", serial_device);
    printf("  -D              Diagnostics mode; extra messages\n");
    printf("  -f              Enable RTS/CTS flow control\n");
    printf("  -h              Show this help screen\n");
    printf("  -p<port>        Set the listening port (default %d)\n", socket_port);
    printf("  -P<port>        PINPad port (default none)\n");
    printf("  -s<ip address>  PINPad host address\n");
    printf("  -t<time limit>  How long to wait before resetting (default %d seconds)\n", time_limit);
    printf("\n");
    printf("Note:  there can be no spaces between an option and the associated\n");
    printf("argument.  AKA, it's \"-p6555\" not \"-p 6555\".\n");
    printf("\n");
    printf("Note:  for the -c and -t arguments, 0 is the same as \"never\".\n");
    printf("\n");
    exit(1);
}


/****
 * ParseArguments:
 ****/
int ParseArguments(int argc, char *argv[])
{
    char *arg;
    int idx = 1;  // first command line argument past binary name
    int retval = 0;

    while (idx < argc)
    {
        arg = argv[idx];
        if (arg[0] == '-')
        {
            switch (arg[1])
            {
            case 'c':
                check_interval = atoi(&arg[2]);
                break;
            case 'd':
                if (strlen(arg) < STRLENGTH)
                    strncpy(serial_device, &arg[2], STRLENGTH);
                break;
            case 'D':
                diagnostics += 1;
                break;
            case 'f':
                use_rtscts = 1;
                break;
            case 'h':
                ShowHelp(argv[0]);
                break;
            case 'p':
                socket_port = atoi(&arg[2]);
                break;
            case 'P':
                pinpad_port = atoi(&arg[2]);
                break;
            case 's':
                if (strlen(arg) < STRLENGTH)
                    strncpy(pinpad_host, &arg[2], STRLENGTH);
                break;
            case 't':
                time_limit = atoi(&arg[2]);
                break;
            }
        }
        idx += 1;
    }
    return retval;
}

/****
 * ProcessConnection:  We'll assume synchronous, so we alternate
 *   between server -> pinpad, pinpad -> server.
 *   Returns
 *     ERR_NONE
 *     ERR_PINPAD
 *     ERR_SERVER
 *     ERR_SELECT
 ****/
int ProcessConnection(int serverfd, int pinpadfd)
{
    int    retval = ERR_NONE;
    char   buffer[STRLONG];
    int    readlen = 1;
    int    writelen = 1;
    fd_set in_fds;
    int    nfds;
    int    readies = 0;
    time_t lastserver = 0;
    time_t now;
    struct timeval timeout;

    nfds = MAX(serverfd, pinpadfd) + 1;

    while (retval == 0 && readlen >= 0 && writelen >= 0 && readies >= 0)
    {
        FD_ZERO(&in_fds);
        FD_SET(serverfd, &in_fds);
        FD_SET(pinpadfd, &in_fds);

        if (check_interval)
        {
            timeout.tv_sec  = check_interval;
            timeout.tv_usec = 0;
            readies = select(nfds, &in_fds, NULL, NULL, &timeout);
        }
        else
            readies = select(nfds, &in_fds, NULL, NULL, NULL);
        if (readies < 0)
        {
            perror("ProcessConnection select");
            retval = ERR_SELECT;
        }
        else if (readies > 0)
        {
            if (FD_ISSET(serverfd, &in_fds))
            {
                // read from server
                readlen = ReadCmd(serverfd, buffer, STRLONG);
                if (readlen >= 0)
                {
                    lastserver = time(NULL);
                    // send to pinpad
                    if (diagnostics)
                        PrintRead("from Socket", buffer, readlen);
                    writelen = Write(pinpadfd, buffer, readlen);
                    if (writelen < 0)
                        retval = ERR_PINPAD;
                }
                else
                    retval = ERR_SERVER;
            }
            if (readlen >= 0 && writelen >= 0 && FD_ISSET(pinpadfd, &in_fds))
            {
                // read from pinpad
                readlen = ReadCmd(pinpadfd, buffer, STRLONG);
                if (readlen >= 0)
                {
                    // send to server
                    if (diagnostics)
                        PrintRead("from Serial", buffer, readlen);
                    writelen = Write(serverfd, buffer, readlen);
                    if (writelen < 0)
                        retval = ERR_SERVER;
                }
                else
                    retval = ERR_PINPAD;
            }
        }
        else if (lastserver && time_limit)
        {  // check for timeouts
            now = time(NULL);
            if ((now - lastserver) > time_limit)
                retval = ERR_TIMEOUT;
        }
    }

    return retval;
}

/****
 * ReadCmd:  This is reading a command for an Ingenico eN-Crypt 1200,
 *  the format of which is STX ... ETX LRC.  So we discard anything
 *  up to the STX character (ASCII 0x02) and then gobble everything
 *  (including STX) through the ETX (ASCII 0x03) and the following
 *  LRC.
 ****/
int ReadCmd(int fd, char *dest, int destlen)
{
    int  readlen = 0;
    int  done = 0;
    char last = 0x00;
    int  destidx = 0;
    char buffer;
    int  first = 1;

    while (!done && destidx < destlen)
    {
        readlen = read(fd, &buffer, 1);
        if (readlen < 0 && errno != EAGAIN)
        {
            perror("ReadCmd Error");
            destidx = readlen;
            done = 1;
        }
        else if (readlen > 0)
        {
            dest[destidx] = buffer;
            destidx += 1;
            if (last == 0x00 && (buffer == ACK || buffer == NAK || buffer == CAN))
                done = 1;
            else if (last == ETX || last == ETB)
                done = 1;
            else
                last = buffer;
        }
        else if (first)
        {
            // this is to catch socket closes.  Probably a bad way to do it,
            // but in this case we know we're being called after select(),
            // so we know there should be data.  If select() returns the fd
            // as readable, but there's nothing to read, then almost certainly
            // the socket is closing (in FIN).
            done = 1;
            destidx = -1;
            if (diagnostics)
                printf("ReadCmd Error: socket closed?\n");
        }
        first = 0;
    }
    dest[destidx] = '\0';

    return destidx;
}

int Read(int fd, char *buffer, int bufflen)
{
    int readlen = 0;
    char errbuff[STRLENGTH];
    int done = 0;

    while (!done)
    {
        readlen = read(fd, buffer, bufflen);
        if (readlen < 0 && errno != EAGAIN)
        {
            snprintf(errbuff, STRLENGTH, "Read Error %d", errno);
            perror(errbuff);
            done = 1;
        }
        else if (readlen >= 0)
        {
            buffer[readlen] = '\0';
            done = 1;
        }
    }

    return readlen;
}

int Write(int fd, char *buffer, int bufflen)
{
    int writelen = 0;
    int bytes;
    char errbuff[STRLONG];

    while (writelen >= 0 && writelen < bufflen)
    {
        bytes = write(fd, buffer, bufflen);
        if (bytes < 0)
        {
            snprintf(errbuff, STRLONG, "Write Error to File Descriptor %d", fd);
            perror(errbuff);
            writelen = bytes;
        }
        else
            writelen += bytes;
    }

    return writelen;
}

void PrintRead(char *label, char *buffer, int bufflen)
{
    int idx = 0;

    printf("Read %s:  ", label);
    while (idx < bufflen)
    {
        if (buffer[idx] == STX)
            printf("<STX>");
        else if (buffer[idx] == ETX)
            printf("<ETX>");
        else if (buffer[idx] == ACK)
            printf("<ACK>");
        else if (buffer[idx] == ENQ)
            printf("<ENQ>");
        else if (buffer[idx] >= 32 && buffer[idx] <= 126)
            printf("%c", buffer[idx]);
        else
            printf(",");
        idx += 1;
    }
    printf("\n");
}
