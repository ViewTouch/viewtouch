
/*
 * logging utilities
 */


#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <syslog.h>


int logmsg(int, const char* , ...);

#endif

/* done */
