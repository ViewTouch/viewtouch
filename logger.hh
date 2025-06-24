#pragma once  // REFACTOR: Replaced include guards with modern pragma once

/*
 * simple logging library - header
 */

void setident(const char* ident);
void logmsg(int priority, const char* fmt, ...);
