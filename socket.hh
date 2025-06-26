#pragma once  // REFACTOR: Replaced #ifndef VT_SOCKET_HH guard with modern pragma once

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
 * socket.hh  Functions to make socket connections and operations easier.
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "list_utility.hh"
#include "utility.hh"

#include <string>

extern int select_timeout;

class Line
{
public:
    Line *next;
    Line *fore;
    std::string line;
    void Set(const char* lineval) { line = lineval; }
    const genericChar* Value() { return line.c_str(); }
    size_t Length() {return line.size();}
};

class Email
{
private:
    Str from;
    Str subject;
    DList<Line> tos;
    DList<Line> body;
public:
    Email();
    ~Email();
    void AddFrom(const char* address);
    int From(char* buffer, int maxlen);
    int AddTo(const char* address);
    int NextTo(char* buffer, int maxlen);
    void AddSubject(const char* subjectstr);
    int Subject(char* buffer, int maxlen);
    int AddBody(const char* line);
    int NextBody(char* buffer, int maxlen);
    int PrintEmail();
};

int  Listen(int port, int nonblocking = 0);
int  Accept(int socknum, char* remote_address = NULL);
int  Connect(const char* host, const char* service);
int  Connect(const char* host, int port);
int  SelectIn(int fd, int u_sec);
int  SelectOut(int fd, int u_sec);
int  SMTP(int fd, Email *email);
