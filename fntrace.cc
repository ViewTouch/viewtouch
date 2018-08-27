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
 * utility.cc - revision 124 (10/6/98)
 * Implementation of utility module
 */

#include "fntrace.hh"

#include <unistd.h>
#include <ctime>
#include <errno.h>
#include <string>
#include <cstring>
#include <cctype>
#include <iostream>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#ifdef DEBUG
// BackTrace
char BT_Stack[STRLENGTH][STRLONG];
int  BT_Depth = 0;
int  BT_Track = 0;
//int  BT_Track = 1;
//int  BT_Depth = 1;

void FnPrintTrace(void)
{
    fprintf(stdout, "Stack Trace (%d):\n", BT_Depth);
    for (int i = 0; i < BT_Depth; ++i)
        fprintf(stdout, "    (%d) %s\n", i + 1, BT_Stack[i]);
    fprintf(stdout, "\n");
}
void FnPrintLast(int depth)
{
    // depth == 0 means print the current function
    // depth == 1 means print the parent function
    // depth == 5 means print the last five functions on the stack if there are that many
    depth = BT_Depth - depth - 1;
    if (depth < 0)
        depth = 0;
    fprintf(stderr, "Stack Trace (%d of %d):\n", BT_Depth - depth, BT_Depth);
    for (int i = depth; i < BT_Depth; ++i )
        fprintf(stderr, "    (%d) %s\n", i + 1, BT_Stack[i]);
    fprintf(stderr, "\n");
}
const char* FnReturnLast()
{
    static char last[STRLENGTH];

    last[0] = '\0';
    if (BT_Depth == 1)
        strcpy(last, BT_Stack[0]);
    else if (BT_Depth > 1)
        strcpy(last, BT_Stack[BT_Depth - 2]);

    return last;
}
int debug_mode = 1;
#else
int debug_mode = 0;
#endif
