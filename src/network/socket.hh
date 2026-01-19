/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025, 2026
  
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

#ifndef VT_SOCKET_HH

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
    Line *next{nullptr};
    Line *fore{nullptr};
    std::string line;
    
    // Constructors
    Line() = default;
    explicit Line(const char* lineval) : line(lineval ? lineval : "") {}
    
    // Member functions
    void Set(const char* lineval) noexcept { 
        if (lineval) 
            line = lineval; 
    }
    [[nodiscard]] const char* Value() const noexcept { return line.c_str(); }
    [[nodiscard]] size_t Length() const noexcept { return line.size(); }
    [[nodiscard]] bool Empty() const noexcept { return line.empty(); }
};

class Email
{
private:
    Str from;
    Str subject;
    DList<Line> tos;
    DList<Line> body;
    Line* current_to{nullptr};
    Line* current_body{nullptr};
    
public:
    Email() = default;
    ~Email();
    
    // Delete copy operations
    Email(const Email&) = delete;
    Email& operator=(const Email&) = delete;
    
    // Move operations
    Email(Email&&) noexcept;
    Email& operator=(Email&&) noexcept;
    
    void AddFrom(const char* address) noexcept;
    [[nodiscard]] int From(char* buffer, int maxlen) const noexcept;
    int AddTo(const char* address);
    int NextTo(char* buffer, int maxlen);
    void AddSubject(const char* subjectstr) noexcept;
    [[nodiscard]] int Subject(char* buffer, int maxlen) const noexcept;
    int AddBody(const char* line);
    int NextBody(char* buffer, int maxlen);
    int PrintEmail();
};

[[nodiscard]] int Listen(int port, int nonblocking = 0);
[[nodiscard]] int Accept(int socknum, char* remote_address = nullptr);
[[nodiscard]] int Connect(const char* host, const char* service);
[[nodiscard]] int Connect(const char* host, int port);
[[nodiscard]] int SelectIn(int fd, int u_sec);
[[nodiscard]] int SelectOut(int fd, int u_sec);
int SMTP(int fd, Email *email);

#define VT_SOCKET_HH
#endif
