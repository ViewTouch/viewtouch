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
 * socket.cc  Functions to make socket connections and operations easier.
 */

#include "socket.hh"
#include "fntrace.hh"
#include "utility.hh"
#include "safe_string_utils.hh"

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <csignal>
#include <netdb.h>
#include <sys/wait.h>
#include <utility>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define BACKLOG  10  // how many pending connections the TCP queue will hold

int select_timeout = 1;   // in milliseconds


/*********************************************************************
 * The Email Class
 ********************************************************************/

/****
 * Destructor:
 ****/
Email::~Email()
= default;

Email::Email(Email&& other) noexcept
    : from(other.from),
      subject(other.subject),
      tos(std::move(other.tos)),
      body(std::move(other.body)),
      current_to(nullptr),
      current_body(nullptr)
{
}

Email& Email::operator=(Email&& other) noexcept
{
    if (this != &other)
    {
        from         = other.from;
        subject      = other.subject;
        tos          = std::move(other.tos);
        body         = std::move(other.body);
        current_to   = nullptr;
        current_body = nullptr;
    }
    return *this;
}

/****
 * AddFrom:
 ****/
void Email::AddFrom(const char* address) noexcept
{
    FnTrace("Email::AddFrom()");
    if (address)
        from.Set(address);
}

/****
 * From:  Puts the from address into buffer.
 ****/
int Email::From(char* buffer, int maxlen) const noexcept
{
    FnTrace("Email::From()");
    if (!buffer || maxlen <= 0) {
        return 1; // invalid parameters
    }
    
    const auto* value = from.Value();
    if (value) {
        vt_safe_string::safe_copy(buffer, maxlen, value);
    } else {
        buffer[0] = '\0';
    }
    return 0;
}

/****
 * AddTo:
 ****/
int Email::AddTo(const char* address)
{
    FnTrace("Email::AddTo()");
    if (!address) {
        return 1; // invalid parameter
    }
    
    auto* newadd = new Line;
    newadd->Set(address);
    tos.AddToTail(newadd);
    return 0;
}

/****
 * NextTo:
 ****/
int Email::NextTo(char* buffer, int maxlen)
{
    FnTrace("Email::NextTo()");
    if (!buffer || maxlen <= 0) {
        return 1; // invalid parameters
    }
    
    current_to = (current_to == nullptr) ? tos.Head() : current_to->next;
    if (current_to != nullptr && current_to->Length() > 0)
    {
        const auto* value = current_to->Value();
        if (value) {
            vt_safe_string::safe_copy(buffer, maxlen, value);
        } else {
            buffer[0] = '\0';
        }
        return 0;
    }

    current_to = nullptr; // reset so a new iteration starts from the head
    return 1;
}

/****
 * AddSubject:
 ****/
void Email::AddSubject(const char* subjectstr) noexcept
{
    if (subjectstr) {
        subject = subjectstr;
    }
}

/****
 * Subject:
 ****/
int Email::Subject(char* buffer, int maxlen) const noexcept
{
    if (!buffer || maxlen <= 0) {
        return 1; // invalid parameters
    }
    
    const auto* value = subject.Value();
    if (value) {
        vt_safe_string::safe_copy(buffer, maxlen, value);
    } else {
        buffer[0] = '\0';
    }
    return 0;
}

/****
 * AddBody:
 ****/
int Email::AddBody(const char* line)
{
    FnTrace("Email::AddBody()");
    if (!line) {
        return 1; // invalid parameter
    }
    
    auto* newbody = new Line;
    newbody->Set(line);
    body.AddToTail(newbody);
    return 0;
}

/****
 * NextBody:
 ****/
int Email::NextBody(char* buffer, int maxlen)
{
    FnTrace("Email::NextBody()");
    if (!buffer || maxlen <= 0) {
        return 1; // invalid parameters
    }
    
    current_body = (current_body == nullptr) ? body.Head() : current_body->next;
    if (current_body != nullptr && current_body->Length() > 0)
    {
        const auto* value = current_body->Value();
        if (value) {
            vt_safe_string::safe_copy(buffer, maxlen, value);
        } else {
            buffer[0] = '\0';
        }
        return 0;
    }

    current_body = nullptr; // reset so a new iteration starts from the head
    return 1;
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
const char* Sock_ntop(const struct sockaddr_in *sa, socklen_t /*addrlen*/)
{
    if (!sa) {
        return nullptr;
    }
    
    char portstr[STRLENGTH];
    static char str[STRLENGTH];

    if (inet_ntop(AF_INET, &sa->sin_addr, str, sizeof(str)) == nullptr)
        return nullptr;
    if (ntohs(sa->sin_port) != 0)
    {
        snprintf(portstr, sizeof(portstr), ":%d", ntohs(sa->sin_port));
        vt_safe_string::safe_concat(str, sizeof(str), portstr);
    }

    return str;
}

/****
 * Listen:  Opens a socket for listening and returns the file handle.
 ****/
int Listen(int port, int nonblocking)
{
    if (port <= 0 || port > 65535) {
        return -1; // invalid port
    }
    
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
        {
            perror("fcntl F_GETFL");
            close(sockfd);
            return -1;
        }
        flags |= O_NONBLOCK;
        if (fcntl(sockfd, F_SETFL, flags) < 0)
        {
            perror("fcntl F_SETFL");
            close(sockfd);
            return -1;
        }
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
int Accept(int socknum, char* remote_address)
{
    if (socknum < 0) {
        return -1; // invalid socket
    }
    
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
    else if (remote_address != nullptr)
    {
        const auto* addr_str = Sock_ntop(&their_addr, sin_size);
        if (addr_str) {
            vt_safe_string::safe_copy(remote_address, STRLENGTH, addr_str);
        } else {
            remote_address[0] = '\0';
        }
    }

    return connect_fd;
}

/*******
 * Connect:  Connects to the server, returning the socket descriptor on success,
 *  0 on failure.
 *******/
int Connect(const char* host, const char* service)
{
    if (!host || !service) {
        return 0; // invalid parameters
    }
    
    int retval = 0;
    int sockfd;
    struct sockaddr_in servaddr;
    struct in_addr **pptr;
    struct hostent *hp;
    struct servent *sp;

    hp = gethostbyname(host);
    if (hp != nullptr)
    {
        sp = getservbyname(service, "tcp");
        if (sp != nullptr)
        {
            pptr = (struct in_addr **)hp->h_addr_list;
            for (; *pptr != nullptr; ++pptr)
            {
                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                if (sockfd < 0)
                {
                    perror("socket");
                    continue;
                }
                
                // Critical fix: Add timeout to connect operation
                struct timeval timeout;
                timeout.tv_sec = 10;  // 10 second timeout
                timeout.tv_usec = 0;
                
                if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
                {
                    perror("setsockopt SO_RCVTIMEO");
                }
                if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
                {
                    perror("setsockopt SO_SNDTIMEO");
                }
                
                memset(&servaddr, 0, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_port = sp->s_port;
                memcpy(&servaddr.sin_addr, *pptr, sizeof(struct in_addr));
                
                // Use non-blocking connect with timeout
                int flags = fcntl(sockfd, F_GETFL, 0);
                if (flags < 0 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
                {
                    perror("fcntl");
                    close(sockfd);
                    continue;
                }
                
                int connect_result = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
                if (connect_result == 0)
                {
                    // Connected immediately
                    fcntl(sockfd, F_SETFL, flags); // Restore blocking mode
                    retval = sockfd;
                    break;
                }
                else if (errno == EINPROGRESS)
                {
                    // Connection in progress, wait for completion
                    fd_set write_fds;
                    FD_ZERO(&write_fds);
                    FD_SET(sockfd, &write_fds);
                    
                    int select_result = select(sockfd + 1, nullptr, &write_fds, nullptr, &timeout);
                    if (select_result > 0)
                    {
                        int error = 0;
                        socklen_t len = sizeof(error);
                        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0)
                        {
                            // Connection successful
                            fcntl(sockfd, F_SETFL, flags); // Restore blocking mode
                            retval = sockfd;
                            break;
                        }
                        else if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
                        {
                            // getsockopt failed, restore blocking mode before closing
                            fcntl(sockfd, F_SETFL, flags);
                        }
                    }
                }
                
                close(sockfd);
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
int Connect(const char* host, int port)
{
    if (!host || port <= 0 || port > 65535) {
        return 0; // invalid parameters
    }
    
    int retval = 0;
    int sockfd;
    struct sockaddr_in servaddr;
    struct in_addr **pptr;
    struct hostent *hp;

    hp = gethostbyname(host);
    if (hp != nullptr)
    {
        pptr = (struct in_addr **)hp->h_addr_list;
        for (; *pptr != nullptr; ++pptr)
        {
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0)
            {
                perror("socket");
                continue;
            }
            
            // Critical fix: Add timeout to connect operation
            struct timeval timeout;
            timeout.tv_sec = 10;  // 10 second timeout
            timeout.tv_usec = 0;
            
            if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
            {
                perror("setsockopt SO_RCVTIMEO");
            }
            if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
            {
                perror("setsockopt SO_SNDTIMEO");
            }
            
            memset(&servaddr, 0, sizeof(servaddr));
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons(port);
            memcpy(&servaddr.sin_addr, *pptr, sizeof(struct in_addr));
            
            // Use non-blocking connect with timeout
            int flags = fcntl(sockfd, F_GETFL, 0);
            if (flags < 0 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
            {
                perror("fcntl");
                close(sockfd);
                continue;
            }
            
            int connect_result = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
            if (connect_result == 0)
            {
                // Connected immediately
                fcntl(sockfd, F_SETFL, flags); // Restore blocking mode
                retval = sockfd;
                break;
            }
            else if (errno == EINPROGRESS)
            {
                // Connection in progress, wait for completion
                fd_set write_fds;
                FD_ZERO(&write_fds);
                FD_SET(sockfd, &write_fds);
                
                int select_result = select(sockfd + 1, nullptr, &write_fds, nullptr, &timeout);
                if (select_result > 0)
                {
                    int error = 0;
                    socklen_t len = sizeof(error);
                    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0)
                    {
                        // Connection successful
                        fcntl(sockfd, F_SETFL, flags); // Restore blocking mode
                        retval = sockfd;
                        break;
                    }
                }
            }
            
            close(sockfd);
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
    if (fd < 0 || u_sec < 0) {
        return -1; // invalid parameters
    }
    
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

    retval = select(fd + 1, &infds, nullptr, nullptr, &timeout);
    return retval;
}

int SelectOut(int fd, int u_sec)
{
    if (fd < 0 || u_sec < 0) {
        return -1; // invalid parameters
    }
    
    fd_set outfds;
    struct timeval timeout;
    int retval = 0;

    FD_ZERO(&outfds);
    FD_SET(fd, &outfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = u_sec;

    retval = select(fd + 1, nullptr, &outfds, nullptr, &timeout);
    return retval;
}

int GetResponse(int fd, char* bufferstr, int maxlen)
{
    if (fd < 0 || !bufferstr || maxlen <= 0) {
        return -1; // invalid parameters
    }
    
    int retval = 0;
    int bytesread = 0;
    char buffer[STRLONG];

    bytesread = read(fd, buffer, STRLONG);
    if (bytesread > 0)
    {
        vt_safe_string::safe_copy(bufferstr, maxlen, buffer);
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
    if (fd < 0 || !email) {
        return -1; // invalid parameters
    }
    
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
        vt_safe_string::safe_concat(body, sizeof(body), outgoing);
        
        // write each of the to addresses
        while (email->NextTo(buffer, STRLONG) == 0)
        {
            snprintf(outgoing, STRLONG, "RCPT TO:%s\r\n", buffer);
            write(fd, outgoing, strlen(outgoing));
            response = GetResponse(fd, responsestr, STRLONG);
            // add the to address to the message body
            snprintf(outgoing, STRLONG, "To: %s\n", buffer);
            vt_safe_string::safe_concat(body, sizeof(body), outgoing);
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
                vt_safe_string::safe_concat(outgoing, STRLONG, ".");
            vt_safe_string::safe_concat(outgoing, STRLONG, buffer);
            vt_safe_string::safe_concat(outgoing, STRLONG, "\r\n");
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
    else if (pid > 0)
    {
        int status = 0;
        // Ensure the child is reaped to avoid zombies; retry if interrupted.
        while (waitpid(pid, &status, 0) == -1 && errno == EINTR)
        {
        }
    }

    return retval;
}

