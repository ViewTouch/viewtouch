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
 * utility.hh - revision 106 (10/20/98)
 * General functions and data types than have no other place to go
 */

#ifndef VT_FNTRACE_HH
#define VT_FNTRACE_HH

#include "basic.hh"

#include <string>

extern int debug_mode;

#define STRSHORT    64
#define STRLENGTH   512   // constant to set the length of a string
#define STRLONG     2048  // 2K string

#ifdef DEBUG
// BackTrace Functions/Data
extern genericChar BT_Stack[STRLENGTH][STRLONG];
extern int  BT_Depth;
extern int  BT_Track;

class BackTraceFunction
{
public:
    // Constructor
    BackTraceFunction(const char* str, ...)
    {
        if (BT_Track)
            printf("Entering %s\n", str);
        sprintf(BT_Stack[BT_Depth++], "%s", str);
    }
    // Destructor
    virtual ~BackTraceFunction()
    {
        --BT_Depth;
    }
};

#define FnTrace(x) BackTraceFunction _fn_start(x)
#define FnTraceEnable(x) (BT_Track = (x))
void FnPrintTrace(void);
void FnPrintLast(int depth);
const char* FnReturnLast();
#define LINE() printf("%s:  Got to line %d\n", __FILE__, __LINE__)
#else
#define FnTrace(x)
#define FnTraceEnable(x)
#define FnPrintTrace()
#define FnPrintLast(x)
#define FnReturnLast() ""
#define LINE()
#endif

#endif // VT_FNTRACE_HH
