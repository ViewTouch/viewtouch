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
 * socket.cc  Functions to make socket connections and operations easier.
 */

#include "socket.hh"
#include "utility.hh"

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <csignal>
#include <netdb.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define BACKLOG  10  // how many pending connections the TCP queue will hold

int select_timeout = 1;   // in milliseconds


/*********************************************************************
 * The Email Class
 ********************************************************************/

/****
 * Constructor:
 ****/
Email::Email()
{
}

/****
 * Destructor:
 ****/
Email::~Email()
{
}

/****
 * AddFrom:
 ****/
int Email::AddFrom(char *address)
{
    FnTrace("Email::AddFrom()");
    int retval = 0;

    retval = from.Set(address);
    return retval;
}

/****
 * From:  Puts the from address into buffer.
 ****/
int Email::From(char *buffer, int maxlen)
{
    FnTrace("Email::From()");
    int retval = 0;

    strncpy(buffer, from.Value(), maxlen);
    return retval;
}

/****
 * AddTo:
 ****/
int Email::AddTo(char *address)
{
    FnTrace("Email::AddTo()");
    int retval = 0;
    Line *newadd = NULL;

    newadd = new Line;
    newadd->Set(address);
    tos.AddToTail(newadd);
    return retval;
}

/****
 * NextTo:
 ****/
int Email::NextTo(char *buffer, int maxlen)
{
    FnTrace("Email::NextTo()");
    int retval = 1;
    static Line *currLine = NULL;

    if (currLine == NULL)
        currLine = tos.Head();
    else
        currLine = currLine->next;
    if (currLine != NULL && currLine->Length() > 0)
    {
        strncpy(buffer, currLine->Value(), maxlen);
        retval = 0;
    }

    return retval;
}

/****
 * AddSubject:
 ****/
int Email::AddSubject(char *subjectstr)
{
    int retval = 0;

    retval = subject.Set(subjectstr);
    return retval;
}

/****
 * Subject:
 ****/
int Email::Subject(char *buffer, int maxlen)
{
    int retval = 0;

    strncpy(buffer, subject.Value(), maxlen);
    return retval;
}

/****
 * AddBody:
 ****/
int Email::AddBody(char *line)
{
    FnTrace("Email::AddBody()");
    int retval = 0;
    Line *newbody = NULL;

    newbody = new Line;
    newbody->Set(line);
    body.AddToTail(newbody);
    return retval;
}

/****
 * NextBody:
 ****/
int Email::NextBody(char *buffer, int maxlen)
{
    FnTrace("Email::NextBody()");
    int retval = 1;
    static Line *currBody = NULL;

    if (currBody == NULL)
        currBody = body.Head();
    else
        currBody = currBody->next;
    if (currBody != NULL && currBody->Length() > 0)
    {
        strncpy(buffer, currBody->Value(), maxlen);
        retval = 0;
    }

    return retval;
}

/****
 * PrintEmail:  Debug function.
 ****/
int Email::PrintEmail()
{
    FnTrace("Email::PrintEmail()");
    int retval = 0;
    genericChar buffer[STRLONG];

    printf("From:  %s\n", from.Value());
    while (NextTo(buffer, STRLONG) == 0)
        printf("  To:  %s\n", buffer);
    printf("\n");
    while (NextBody(buffer, STRLONG) == 0)
        printf("%s\n", buffer);
    printf("==================================\n");
    return retval;
}


/*********************************************************************
 * Subroutines
 ********************************************************************/

/****
 * Sock_ntop
 ****/
char *Sock_ntop(const struct sockaddr_in *sa, socklen_t addrlen)
{
    char portstr[STRLENGTH];
    static char str[STRLENGTH];

    if (inet_ntop(AF_INET, &sa->sin_addr, str, sizeof(str)) == NULL)
        return NULL;
    if (ntohs(sa->sin_port) != 0)
    {
        snprintf(portstr, sizeof(portstr), ":%d", ntohs(sa->sin_port));
        strcat(str, portstr);
    }

    return str;
}

/****
 * Listen:  Opens a socket for listening and returns the file handle.
 ****/
int Listen(int port, int nonblocking)
{
    int sockfd;                    // listen on sock_fd
    struct sockaddr_in my_addr;    // my address information
    int yes = 1;
    int flags;
    char str[STRLENGTH];
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        return -1;
    }
    
    // set it non-blocking if necessary
    if (nonblocking)
    {
        flags = fcntl(sockfd, F_GETFL, 0);
        if (flags < 0)
            perror("fcntl F_GETFL");
        flags |= O_NONBLOCK;
        if (fcntl(sockfd, F_SETFL, flags) < 0)
            perror("fcntl F_SETFL");
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        snprintf(str, STRLENGTH, "setsockopt port %d", port);
        perror(str);
        return -1;
    }
    
    my_addr.sin_family = AF_INET;         // host byte order
    my_addr.sin_port = htons(port);     // short, network byte order
    my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
    memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct
    
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
    {
        snprintf(str, STRLENGTH, "bind port %d", port);
        perror(str);
        return -1;
    }
    
    if (listen(sockfd, BACKLOG) == -1)
    {
        snprintf(str, STRLENGTH, "listen port %d", port);
        perror(str);
        return -1;
    }
    return sockfd;
}

/****
 * Accept:  Accepts the socket and returns a file handle of the open connection
 ****/
int Accept(int socknum, char *remote_address)
{
    int connect_fd;
    socklen_t sin_size;
    struct sockaddr_in their_addr;

    sin_size = sizeof(struct sockaddr_in);
    connect_fd = accept(socknum, (struct sockaddr *)&their_addr, &sin_size);
    if (connect_fd < 0)
    {
        if (errno != EWOULDBLOCK)
            perror("accept");
    }
    else if (remote_address != NULL)
    {
        strcpy(remote_address, Sock_ntop(&their_addr, sin_size));
    }

    return connect_fd;
}

/*******
 * Connect:  Connects to the server, returning the socket descriptor on success,
 *  0 on failure.
 *******/
int Connect(char *host, char *service)
{
    int retval = 0;
    int sockfd;
    struct sockaddr_in servaddr;
    struct in_addr **pptr;
    struct hostent *hp;
    struct servent *sp;

    hp = gethostbyname(host);
    if (hp != NULL)
    {
        sp = getservbyname(service, "tcp");
        if (sp != NULL)
        {
            pptr = (struct in_addr **)hp->h_addr_list;
            for (; *pptr != NULL; pptr++)
            {
                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                bzero(&servaddr, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_port = sp->s_port;
                memcpy(&servaddr.sin_addr, *pptr, sizeof(struct in_addr));
                if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0)
                {
                    retval = sockfd;
                    break;
                }
            }
        }
        else
        {
            herror("getservbyname");
        }
    }
    else
    {
        herror("gethostbyname");
    }
    return retval;
}

/*******
 * Connect:  Connects to the server, returning the socket descriptor on success,
 *  0 on failure.
 *******/
int Connect(char *host, int port)
{
    int retval = 0;
    int sockfd;
    struct sockaddr_in servaddr;
    struct in_addr **pptr;
    struct hostent *hp;

    hp = gethostbyname(host);
    if (hp != NULL)
    {
        pptr = (struct in_addr **)hp->h_addr_list;
        for (; *pptr != NULL; pptr++)
        {
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            bzero(&servaddr, sizeof(servaddr));
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons(port);
            memcpy(&servaddr.sin_addr, *pptr, sizeof(struct in_addr));
            if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0)
            {
                retval = sockfd;
                break;
            }
            else
                perror("connect");
        }
    }
    else
    {
        herror("gethostbyname");
    }
    return retval;
}

int SelectIn(int fd, int u_sec)
{
    fd_set infds;
    struct timeval timeout;
    int retval = 0;
    int seconds = 0;
    int milliseconds = u_sec;

    FD_ZERO(&infds);
    FD_SET(fd, &infds);
    if (u_sec > 999)
    {
        seconds = u_sec / 1000;
        milliseconds = u_sec % 1000;
    }
    timeout.tv_sec = seconds;
    timeout.tv_usec = milliseconds;

    retval = select(fd + 1, &infds, NULL, NULL, &timeout);
    return retval;
}

int SelectOut(int fd, int u_sec)
{
    fd_set outfds;
    struct timeval timeout;
    int retval = 0;

    FD_ZERO(&outfds);
    FD_SET(fd, &outfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = u_sec;

    retval = select(fd + 1, NULL, &outfds, NULL, &timeout);
    return retval;
}

int GetResponse(int fd, char *bufferstr, int maxlen)
{
    int retval = 0;
    int bytesread = 0;
    char buffer[STRLONG];

    bytesread = read(fd, buffer, STRLONG);
    if (bytesread > 0)
    {
        strncpy(bufferstr, buffer, maxlen);
        buffer[3] = '\0';
        retval = atoi(buffer);
    }
    return retval;
}

/****
 * SMTP: send the given email through the supplied socket.
 ****/
int SMTP(int fd, Email *email)
{
    int   retval = 0;
    char  buffer[STRLONG];
    char  outgoing[STRLONG];
    char  body[STRLONG] = "";
    char  responsestr[STRLONG];
    int   response;
    char  mimever[] = "MIME-Version: 1.0\n";
    char  mimehead[STRLONG] = "Content-Type: text/html\n";
    pid_t pid;

    if (debug_mode)
        printf("Forking for SMTP\n");
    pid = fork();
    if (pid == 0)
    {  // the child process sends the email
        // get the hello response from the server
        response = GetResponse(fd, responsestr, STRLONG);
        if (response > 399)
        {
            fprintf(stderr, "SMTP Error:  %s\n", responsestr);
            exit(1);
        }
        
        // write the from address
        email->From(buffer, STRLONG);
        snprintf(outgoing, STRLONG, "MAIL FROM:%s\r\n", buffer);
        write(fd, outgoing, strlen(outgoing));
        response = GetResponse(fd, responsestr, STRLONG);
        if (response > 299)
        {
            fprintf(stderr, "SMTP Error:  %s\n", responsestr);
            exit(1);
        }
        // add from address to message body
        snprintf(outgoing, STRLONG, "From: %s\n", buffer);
        strncat(body, outgoing, STRLONG);
        
        // write each of the to addresses
        while (email->NextTo(buffer, STRLONG) == 0)
        {
            snprintf(outgoing, STRLONG, "RCPT TO:%s\r\n", buffer);
            write(fd, outgoing, strlen(outgoing));
            response = GetResponse(fd, responsestr, STRLONG);
            // add the to address to the message body
            snprintf(outgoing, STRLONG, "To: %s\n", buffer);
            strncat(body, outgoing, STRLONG);
        }
        
        // write the message
        write(fd, "DATA\r\n", 5);
        response = GetResponse(fd, responsestr, STRLONG);
        if (response > 399)
        {
            fprintf(stderr, "SMTP Error:  %s\n", responsestr);
            exit(1);
        }
        // write the headers (From, To, Subject)
        write(fd, body, strlen(body));
        email->Subject(buffer, STRLONG);
        snprintf(outgoing, STRLONG, "Subject: %s\n", buffer);
        write(fd, outgoing, strlen(outgoing));
        write(fd, mimever, strlen(mimever));
        write(fd, mimehead, strlen(mimehead));
        write(fd, "\n", 1);  // header/body separator
        // and now write the body of the message
        while (email->NextBody(buffer, STRLONG) == 0)
        {
            outgoing[0] = '\0';
            if (buffer[0] == '.')
                strcat(outgoing, ".");
            strcat(outgoing, buffer);
            strcat(outgoing, "\r\n");
            write(fd, outgoing, strlen(outgoing));
        }
        write(fd, ".\r\n", 2);
        response = GetResponse(fd, responsestr, STRLONG);
        if (response > 299)
        {
            fprintf(stderr, "SMTP Error:  %s\n", responsestr);
            exit(1);
        }
        exit(0);
    }

    return retval;
}

