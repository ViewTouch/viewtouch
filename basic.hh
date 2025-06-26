#pragma once

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
 * Basic Data Types & Definitions 
 */

#include <cstdio>
#include <cstdlib>
#include <algorithm>

/**** Type Definitions ****/
using Uchar = unsigned char;
using Short = short;
using Ushort = unsigned short;
using Int = int;
using Uint = unsigned int;
using Long = long;
using Ulong = unsigned long;
using Flt = float;
using Dbl = double;
using genericChar = char;

/**** Template Function Definitions ****/
template<class T> T Max(const T& a, const T& b) { return (a > b) ? a : b; }
template<class T> T Min(const T& a, const T& b) { return (a < b) ? a : b; }
template<class T> T Abs(const T& a) { return (a < 0) ? -a : a; }

/**** Global Constants ****/
constexpr int STRLENGTH = 512;
