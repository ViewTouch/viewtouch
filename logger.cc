/*
 * logging utilities
 */

#include <stdarg.h>
#include <stdio.h>  /* vsnprintf */
#include <string.h> /* memset */

#include "logger.hh"


#define BUFSIZE 1024
static char buf[BUFSIZE + 1];
static int init = 0;

void initlogger();

void
initlogger()
{
    if (init != 0)
        return;

// FIXME
// this is lame; make new build system and fix this.
// should be using -DDEBUG=0 or -DDEBUG=1 and then use
//   if ()  or #if   instead of  #ifdef
#if !(DEBUG)
    // not debug build ==> no debug messages
    setlogmask(LOG_UPTO(LOG_INFO));
#endif

    openlog("ViewTouch ", LOG_PERROR | LOG_PID, LOG_USER);
    init = 1;
}

void
setident(const char *ident)
{
    closelog();
    if (init == 0)
        initlogger();
    openlog(ident ? "VT" : ident, LOG_PERROR | LOG_PID, LOG_USER);
}

/*
 * return:  0 : success
 *         -1 : message truncated (didn't fit into buffer)
 *         -2 : error returned by vsnprintf(3)
 */
int
logmsg(int priority, const char *fmt, ...)
{
    va_list ap;
    int retval = 0;

    if (init == 0)
        initlogger();

    memset(buf, 0, sizeof(buf));
    va_start(ap, fmt);
    retval = vsnprintf(buf, BUFSIZE, fmt, ap);
    va_end(ap);
    // ensure termination
    buf[BUFSIZE] = '\0';

    if (retval >= BUFSIZE) {
        retval = -1;
        // ellipsis at the end because we truncated
        buf[BUFSIZE - 1] = buf[BUFSIZE - 2] = buf[BUFSIZE - 3] = '.';
    } else if (retval < 0)
        retval = -2;
    else
        retval = 0;

    // made the string -- log it
    //   (even if it was truncated)
    if (retval >= -1)
        syslog(priority, buf);

    return retval;
}

/* done */
