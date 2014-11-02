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
 * basic.hh - revision 13 (10/20/98)
 * Standard types, macros & definitions used in application
 */

#ifndef _BASIC_HH
#define _BASIC_HH

#include <cstdio>
#include <cstdlib>

/**** Types ****/
typedef char           genericChar;
typedef unsigned char  Uchar;
typedef signed char    Schar;
typedef unsigned short Ushort;
typedef unsigned int   Uint;
typedef unsigned long  Ulong;
typedef double         Flt;


/**** Inlined Functions ****/
template <class type>
inline type Max(type a, type b) { return (a > b) ?  (a) : (b); }
// Returns the higher of both arguments

template <class type>
inline type Min(type a, type b) { return (a > b) ?  (b) : (a); }
// Returns the lower of both arguments

template <class type>
inline type Abs(type a)         { return (a < 0) ? (-a) : (a); }
// Returns the absolute value of the given argument

#endif
