
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
 * license_hash.hh
 * Shared functions for generating a temporary license.
 */

#ifndef LICENSE_HASH_HH
#define LICENSE_HASH_HH

#include <time.h>
#include "generic_char.hh"


int GenerateTempKey( genericChar* dest, int maxlen, const genericChar* licenseid);
int GenerateTempKeyLong(genericChar* dest, int maxlen, time_t license_time, const genericChar* licenseid);
int DigestString( genericChar* dest, int maxlen, const genericChar* source);

int GetUnameInfo(char* buffer, int bufflen);
int GetInterfaceInfo(char* stringbuff, int stringlen);
int GetMacAddress(char* stringbuff, int stringlen);
int GetMachineDigest(char* digest_string, int maxlen);

#endif
