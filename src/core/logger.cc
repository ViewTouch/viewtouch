/*
 * logging utilities
 */

#include "logger.hh"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
    constexpr size_t BUFSIZE = 1024;
    std::array<char, BUFSIZE + 1> g_log_buffer{};
    bool g_logger_initialized{false};

    void initlogger() noexcept
    {
        if (g_logger_initialized)
            return;

#if !(DEBUG)
        // not debug build ==> no debug messages
        setlogmask(LOG_UPTO(LOG_INFO));
#endif

        openlog("ViewTouch ", LOG_PERROR | LOG_PID, LOG_USER);
        g_logger_initialized = true;
    }
} // anonymous namespace

void setident(const char* ident) noexcept
{
    closelog();
    if (!g_logger_initialized)
        initlogger();
    openlog(ident ? ident : "VT", LOG_PERROR | LOG_PID, LOG_USER);
}

/*
 * logmsg: Sends a formatted message to syslog
 * 
 * Returns:  0 : success
 *          -1 : message truncated (didn't fit into buffer)
 *          -2 : error returned by vsnprintf(3)
 */
int logmsg(int priority, const char* fmt, ...)
{
    if (!g_logger_initialized)
        initlogger();

    g_log_buffer.fill(0);
    
    va_list ap;
    va_start(ap, fmt);
    const int retval_raw = std::vsnprintf(g_log_buffer.data(), BUFSIZE, fmt, ap);
    va_end(ap);
    
    int retval = 0;
    if (retval_raw >= static_cast<int>(BUFSIZE))
    {
        retval = -1;
        // Add ellipsis at the end because message was truncated
        // Note: We need to ensure null termination after adding ellipsis
        g_log_buffer[BUFSIZE - 1] = '.';
        g_log_buffer[BUFSIZE - 2] = '.';
        g_log_buffer[BUFSIZE - 3] = '.';
        g_log_buffer[BUFSIZE - 4] = '\0'; // Ensure null termination
    }
    else if (retval_raw < 0)
    {
        retval = -2;
        // Ensure null termination for error case
        g_log_buffer[BUFSIZE] = '\0';
    }
    else
    {
        // Ensure null termination for normal case
        g_log_buffer[BUFSIZE] = '\0';
    }

    // Log the string (even if it was truncated)
    if (retval >= -1)
        syslog(priority, "%s", g_log_buffer.data());

    return retval;
}
