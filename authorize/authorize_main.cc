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
 * authorize_main.cc - revision 2 (9/8/98)
 * credit card/debit card authorization/processing module
 */

#include "basic.hh"
#include <cstring>

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/**** Main Line ****/
int main(int argc, genericChar **argv)
{
  if (argc >= 2 && strcmp(argv[1], "-v") == 0)
  {
    // return version for vt_update
    printf("1\n");
    return 0;
  }
  return 0;
}    
