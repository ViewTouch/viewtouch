#pragma once  // REFACTOR: Replaced #ifndef VT_DEBUG_HH guard with modern pragma once

/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997  
  
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
 * debug.hh - revision 8 (9/3/98)
 */

#include "basic.hh"
#include <X11/Xlib.h>
#include <cstdio>

extern int debug_mode;
extern const genericChar* pos_data_filename;

const genericChar* GetXEventName( XEvent );
void PrintXEventName( XEvent, const genericChar* , FILE * );
void PrintTermCode( int );
void PrintServerCode( int );
void PrintFamilyCode( int );

#ifdef DEBUG
const genericChar* GetZoneTypeName( int );
#else
#define GetZoneTypeName(x)
#endif
