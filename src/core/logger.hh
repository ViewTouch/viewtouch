/*
 * logging utilities
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <cstdarg>
#include <syslog.h>

// Modern C++ logging interface
int logmsg(int priority, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void setident(const char* ident) noexcept;

#endif // LOGGER_H
