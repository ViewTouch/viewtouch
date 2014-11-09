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
 * license.hh
 * License/Planar ID checking module
 */

#ifndef VT_LICENSE_HH
#define VT_LICENSE_HH

#include "utility.hh"

#define MAX_LICENSE_GRACE  10  // grace period for license expiration
#define MAX_LICENSE_WARN   0   // warn user when under this many days left on license

int GetExpirationDate(Str &settings_license, int force_check = 0);
int GetMachineID(char *stringbuff, int stringlen);
int GetMachineDigest(char *digest_string, int maxlen);
int SaveLicenseData(void);
int NumLicensedTerminals(void);
int NumLicensedPrinters(void);
int TryTempLicense(void);


#endif
