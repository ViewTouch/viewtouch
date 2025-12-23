/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 2025
  
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

#ifndef BASIC_HH
#define BASIC_HH

#include <cstdio>
#include <cstdlib>
#include <cstddef>  // for size_t

/**** String Length Constants ****/
inline constexpr size_t STRSHORT  = 64;
inline constexpr size_t STRLENGTH = 512;   // constant to set the length of a string
inline constexpr size_t STRLONG   = 2048;  // 2K string

/**** Type Aliases (Modern C++ using syntax) ****/
using genericChar = char;
using Uchar = unsigned char;
using Schar = signed char;
using Ushort = unsigned short;
using Uint = unsigned int;
using Ulong = unsigned long;
using Flt = double;


/**** Modern Inline Template Functions ****/

// Returns the higher of both arguments
template <typename T>
[[nodiscard]] constexpr inline T Max(T a, T b) noexcept
{
    return (a > b) ? a : b;
}

// Returns the lower of both arguments
template <typename T>
[[nodiscard]] constexpr inline T Min(T a, T b) noexcept
{
    return (a > b) ? b : a;
}

// Returns the absolute value of the given argument
template <typename T>
[[nodiscard]] constexpr inline T Abs(T a) noexcept
{
    return (a < 0) ? -a : a;
}

#endif
