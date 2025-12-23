/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025
  
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
 * debug.cc
 * Some debug functions that may or may not help in the debugging process.
 * If they don't help, don't use them.  Heh.
 * Created By:  Bruce Alon King, Tue Apr  2 08:45:15 2002
 */

#ifndef _DEBUG_FUNCS_HH
#define DEBUG_FUNCS_HH

#include "basic.hh"

#include <stdio.h>
#include <X11/Xlib.h>

#ifdef DEBUG

extern const genericChar* pos_data_filename;

const genericChar* GetXEventName( XEvent ) noexcept;
void PrintXEventName( XEvent, const genericChar* , FILE * ) noexcept;
void PrintTermCode( int ) noexcept;
void PrintServerCode( int ) noexcept;
void PrintFamilyCode( int ) noexcept;
const genericChar* GetZoneTypeName( int ) noexcept;

#else

#define GetXEventName(x)
#define PrintXEventName(x,y,z)
#define PrintTermCode(x)
#define PrintServerCode(x)
#define PrintFamilyCode(x)
#define GetZoneTypeName(x)

#endif
#endif
