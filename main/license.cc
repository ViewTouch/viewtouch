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
 * license.cc
 * Implementation of license checking module
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/resource.h>
/* sys/param.h defines BSD.  Undefine it here just to get rid of a compiler
 * warning.  */
#undef BSD
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/route.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef BSD
#include <net/if_dl.h>
#endif

#ifdef LINUX
#include <net/if_arp.h>
#include <sys/ioctl.h>
#endif

#include "blowfish.h"

#include "data_file.hh"
#include "license.hh"
#include "license_hash.hh"
#include "manager.hh"
#include "sha1.hh"
#include "utility.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


/*********************************************************************
 * protocol versions
 *    1   =   First version
 *            Plain text
 *            Sends machine hash (MAC and uname) and ViewTouch build number
 *            Reads 5 comma-separated digits
 *                (license type,license paid,license days,terminals,printers)
 *    2   =   Incoming data is blowfish encrypted (ECB)
 ********************************************************************/
#define PROTOCOL_VERSION    2
int protocol              = PROTOCOL_VERSION;  // use this for incoming data

// Information used to connect to the licensing server
#define LICENSE_SERVICE     "http"
#define LICENSE_PATH        "/cgi-bin/vt_check_license.cgi"

// Settings for how often to check the license, how long an unregistered license
// lasts, etc.  Generally the debug settings will be for very short periods of time
// while the release settings will allow for several days or more.
#define DAY                 86400  /* number of seconds in one day */
#define LICENSE_PERIOD      DAY           /* don't check license more often than this */
#define LICENSE_EXPIRE      DAY * 30      /* expire after this period */
#define LICENSE_GRACE       DAY * 7       /* grace period after expiration */
#define LICENSE_TEMPORARY   DAY * 7       /* how long a temporary license lasts */

// Paths for various debug flag files.
#define LICENSE_ISEXPIRED   VIEWTOUCH_PATH "/bin/.isexpired"          /* for testing expiration */
#define LICENSE_NOGRACE     VIEWTOUCH_PATH "/bin/.nograceleft"        /* complete expiration */
#define LICENSE_FORCE_CHECK VIEWTOUCH_PATH "/bin/.forcelicensecheck"  /* force the check with the licensing server */
#define LICENSE_SKIP        VIEWTOUCH_PATH "/bin/.nolicensecheck"     /* for bypassing expiration */
#define LICENSE_VALID       VIEWTOUCH_PATH "/bin/.isvalid"            /* license is always valid */

// Paths and files for debug and release code
#define LICENSE_FILE_OLD    VIEWTOUCH_PATH "/bin/.viewtouch_license"  /* the license file */
#define LICENSE_FILE        VIEWTOUCH_PATH "/dat/.viewtouch_license"
#define LICENSE_FILE_TEMP   VIEWTOUCH_PATH "/dat/.viewtouch_temp_license"
#define LICENSE_CURRENT_DIR VIEWTOUCH_PATH "/dat/current/"

#define MAXKEYSIZE          64       /* max characters in key (subtract 1 for null byte)   */
#define MAXVALSIZE          256      /* max characters in value (subtract 1 for null byte) */
#define MACLENGTH           256
#define DEFAULT_TERMINALS   2
#define DEFAULT_PRINTERS    2

struct LicenseData;

/*********************************************************************
 * Function Prototypes
 ********************************************************************/
int    ParseFile();
int    GetUnameInfo(char* buffer, int bufflen);
int    GetCurrentDirectory(char* dirstring, int maxlen);
int    CheckLicense(LicenseData *licensedat);
int    PrintLicense(LicenseData *license);


/*********************************************************************
 * struct LicenseData:  contains information used to determine
 *  licensing.  The digest entry is based on all other values.
 *  At the moment, we don't pass it around, so technically we could
 *  just make all of the elements global variables, but I want to 
 *  ensure that we can easily pass the data around if we need to.
 * Note:  license_date is the last time the registration check
 *   succeeded, but only if the system is actually registered to a
 *   company. license_days is the number of days left on the license.
 ********************************************************************/
struct LicenseData {
    unsigned long start_date;
    unsigned long license_date;
    char          mac_address[MACLENGTH];
    char          current_directory[STRLONG];
    char          uname_info[STRLONG];
    char          digest[STRLONG];
    char          license_digest[STRLONG];
    int           license_type;
    int           license_paid;
    int           license_days;
    int           num_terminals;
    int           num_printers;
    LicenseData ()
    {
        start_date = 0;
        license_date = 0;
        GetMacAddress(mac_address, MACLENGTH);
        GetCurrentDirectory(current_directory, STRLONG);
        GetUnameInfo(uname_info, STRLONG);
        digest[0] = '\0';  // don't set digest until we use it; it needs to be current
        license_digest[0] = '\0';
        license_type = -1;
        license_paid = 0;
        license_days = 0;
        num_terminals = DEFAULT_TERMINALS;  // allow 2 terminals and 2 printers by default
        num_printers = DEFAULT_PRINTERS;
    }
};
LicenseData license_data;


/*********************************************************************
 * struct TempLicenseData:  contains information related to temporary
 *  licensing.  We need to allow users to run ViewTouch even when they
 *  can't connect to the Internet.  We're not going to allow them
 *  unrestricted use at this point, but we'll allow them to run it
 *  for a few days.
 ********************************************************************/
struct TempLicenseData {
    time_t date;
    char license_key[STRLONG];     // short version
    char license_digest[STRLONG];  // digest this file, Machine ID, and Build Number
    TempLicenseData()
    {
        date = 0;
        license_key[0] = '\0';
        license_digest[0] = '\0';
    }
};
TempLicenseData temp_license;


/*********************************************************************
 * Functions
 ********************************************************************/

/*******
 * GetLicenseDigest:  Computes the hash for the ever changing fields in LicenseData.
 *  This should be called immediately before writing the data to the license file and
 *  immediately after reading it back in.
 *******/
int GetLicenseDigest(LicenseData *licensedat, char* digest_string, int maxlen)
{
    FnTrace("GetLicenseDigest()");
    int retval = 0;
    char buffer[STRLONG];

    snprintf(buffer, STRLONG - 1, "%ld%ld%d%d%d%s",
             licensedat->start_date,
             licensedat->license_date,
             licensedat->license_days,
             licensedat->num_terminals,
             licensedat->num_printers,
             licensedat->current_directory);

    DigestString(digest_string, maxlen, buffer);
    return retval;
}

/****
 * GetCurrentDirectory:  Store the list of filenames in 
 *  <viewtouch_dir>/dat/current/dirstring as "1,2,3,4,yada".
 *  The intent is that we can track whether the list of active
 *  files has changed between shutdown and start.   This is to
 *  prevent the following scenario:
 *   o  license is about to expire
 *   o  user backs up all data
 *   o  user deletes all data
 *   o  user runs vtpos, establishing a new temp license
 *   o  user immediately shuts down
 *   o  user restores all data, leaving the license file intact
 *   o  ViewTouch runs with almost a month of data, but thinks it
 *      still has a month left on the license
 *  So we make sure the current directory doesn't change when
 *  ViewTouch isn't running.
 *  Returns 0 on success, 1 otherwise.
 ****/
int GetCurrentDirectory(char* dirstring, int maxlen)
{
    FnTrace("GetCurrentDirectory()");
    DIR *currdir;
    struct dirent *dentry;
    int len;
    int retval = 1;
    int count = 0;
    char buffer[STRLONG];
    int bufflen = 0;

    buffer[0] = '\0';
    currdir = opendir(LICENSE_CURRENT_DIR);
    if (currdir != NULL)
    {
        dentry = readdir(currdir);
        while (dentry != NULL)
        {
            // skip .fmt and .bak files as well as all hidden files
            // and files that aren't long enough (if a filename is only
            // 1 character, it doesn't have a .fmt extension anyway).
            // We use strlen because dirent structures have different length
            // fields on different systems (d_namlen on FreeBSD,
            // d_reclen on Linux) and I don't want to keep track of all that.
            len = strlen(dentry->d_name);
            if ((len > 4) &&
                (dentry->d_name[0] != '.') &&
                strcmp(&dentry->d_name[len - 4], ".fmt") &&
                strcmp(&dentry->d_name[len - 4], ".bak"))
            {
                count += 1;
                if ((bufflen + len + 2) < STRLONG)
                {
                    if (buffer[0] != '\0')
                        strncat(buffer, ",", maxlen);
                    strncat(buffer, dentry->d_name, maxlen);
                    bufflen += len + 1;
                }
            }
            dentry = readdir(currdir);
        }
        closedir(currdir);
        retval = 0;
    }
    dirstring[0] = '\0';
    snprintf(dirstring, maxlen, "%d%s", count, buffer);
    return retval;
}

/****
 * KeyValue:  Given a key/value pair, determines whether it's useful and
 *  stores it in our LicenseData struct if so.
 ****/
int KeyValue(LicenseData *licensedat, const genericChar* key, const genericChar* value)
{
    FnTrace("KeyValue()");
    int retval = 0;

    if (strcmp(key, "start date") == 0)
        licensedat->start_date = atol(value);
    else if (strcmp(key, "license date") == 0)
        licensedat->license_date = atol(value);
    else if (strcmp(key, "mac address") == 0)
        strncpy(licensedat->mac_address, value, MACLENGTH);
    else if (strcmp(key, "current directory") == 0)
        strncpy(licensedat->current_directory, value, STRLONG);
    else if (strcmp(key, "uname info") == 0)
        strncpy(licensedat->uname_info, value, STRLONG);
    else if (strcmp(key, "digest") == 0)
        strncpy(licensedat->digest, value, STRLONG);
    else if (strcmp(key, "license digest") == 0)
        strncpy(licensedat->license_digest, value, STRLONG);
    else if (strcmp(key, "license days") == 0)
        licensedat->license_days = atoi(value);
    else if (strcmp(key, "license type") == 0)
        licensedat->license_type = atoi(value);
    else if (strcmp(key, "num terminals") == 0)
        licensedat->num_terminals = atoi(value);
    else if (strcmp(key, "num printers") == 0)
        licensedat->num_printers = atoi(value);
    else  // not a valid key
        retval = 1;
    
    return retval;
}

/****
 * ReadLicenseData:  Reads the LICENSE_FILE and pulls out the key/value pairs.
 *  The format is standard:  Everything from a # to the end of the line is
 *  ignored.  Lines containing data should not begin with spaces.  All
 *  symbols, excluding colons, from the beginning of the line to the first
 *  colon are used as the key.
 ****/
int ReadLicenseData(LicenseData *licensedat)
{
    FnTrace("ReadLicenseData()");
    genericChar key[STRLENGTH];
    genericChar value[STRLENGTH];
    int retval = 1;
    struct stat sb;
    KeyValueInputFile kvfile;

    if (stat(LICENSE_FILE, &sb) == 0)
        kvfile.Set(LICENSE_FILE);
    else if (stat(LICENSE_FILE_OLD, &sb) == 0)
        kvfile.Set(LICENSE_FILE_OLD);

    if (kvfile.Open())
    {
        while (kvfile.Read(key, value, STRLENGTH) > 0)
            KeyValue(licensedat, key, value);
        kvfile.Close();
    }
    return retval;
}

/****
 * WriteLicenseData:  Calculates the license key and writes all information
 *  to LICENSE_FILE.
 ****/
int WriteLicenseData(LicenseData *licensedat)
{
    FnTrace("WriteLicenseData()");
    genericChar buffer[STRLONG];
    int fd;
    int retval = 1;

    // collect the data we'll need for validation when we read the license
    // data back in.
    GetCurrentDirectory(licensedat->current_directory, STRLONG);
    GetLicenseDigest(licensedat, licensedat->license_digest, STRLONG);

    fd = open(LICENSE_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd > 0)
    {
        snprintf(buffer, STRLONG, "start date:  %ld\n", licensedat->start_date);
        write(fd, buffer, strlen(buffer));
        snprintf(buffer, STRLONG, "license date:  %ld\n", licensedat->license_date);
        write(fd, buffer, strlen(buffer));
        snprintf(buffer, STRLONG, "license days:  %d\n", licensedat->license_days);
        write(fd, buffer, strlen(buffer));
        snprintf(buffer, STRLONG, "license type:  %d\n", licensedat->license_type);
        write(fd, buffer, strlen(buffer));
        snprintf(buffer, STRLONG, "num terminals:  %d\n", licensedat->num_terminals);
        write(fd, buffer, strlen(buffer));
        snprintf(buffer, STRLONG, "num printers:  %d\n", licensedat->num_printers);
        write(fd, buffer, strlen(buffer));
        snprintf(buffer, STRLONG, "digest:  %s\n", licensedat->digest);
        write(fd, buffer, strlen(buffer));
        snprintf(buffer, STRLONG, "license digest:  %s\n", licensedat->license_digest);
        write(fd, buffer, strlen(buffer));
        close(fd);
        retval = 0;
    }
    else if (fd < 0)
    {
        snprintf(buffer, STRLONG, "WriteLicenseData Error %d opening %s",
                 errno, LICENSE_FILE);
        ReportError(buffer);
    }

    return retval;
}

/****
 * ValidLicenseKey:  Returns 1 if the digest in license_data is valid,
 *  0 otherwise.  Valid means regenerate the digest from all fields
 *  and compare with stored; the generated and stored must match to
 *  be considered valid.
 ****/
int ValidLicenseKey(LicenseData *licensedat)
{
    FnTrace("ValidLicenseKey()");
    int retval = 0;
    char newdigest[STRLONG];
    struct stat sb;

    if (debug_mode && (stat(LICENSE_VALID, &sb) == 0))
    {
        printf("Faking a valid license\n");
        retval = 1;
    }
    else if (licensedat->start_date == 0 && licensedat->license_date == 0)
    {
        retval = 0;
    }
    else
    {
        if (GetMachineDigest(newdigest, STRLONG) == 0)
        {
            if (strcmp(licensedat->digest, newdigest) == 0)
            {
                GetLicenseDigest(licensedat, newdigest, STRLONG);
                if (strcmp(licensedat->license_digest, newdigest) == 0)
                    retval = 1;
                else if (debug_mode)
                    printf("Invalid license_digest\n");
            }
            else if (debug_mode)
            {
                printf("Invalid machine digest\n");
            }
        }
        else
        {
            printf("Cannot get machine digest\n");
            ViewTouchError("Cannot get machine digest.",1);
        }
    }
    return retval;
}

/****
 * GetElapsedSeconds:  Prefer license_date, fall back to start_date, and
 *   calculate the number of seconds that have elapsed since then.
 *  NOTE:  If a negative number of days has elapsed we'll have problems.  AKA,
 *   because we use an unsigned long, the number of days that have elapsed will
 *   be rather ENORMOUS.  This should not happen in a license file that has
 *   not been tampered with.
 ****/
unsigned long GetElapsedSeconds(LicenseData *licensedat)
{
    FnTrace("GetElapsedSeconds()");
    unsigned long elapsed;
    time_t now;

    now = time(NULL);
    if (licensedat->license_date)
        elapsed = now - licensedat->license_date;
    else
        elapsed = now - licensedat->start_date;
    return elapsed;
}

/****
 * GetElapsedDays:  Return the number of days elapsed from LicenseData.  We use
 *   license date if it's available, start date otherwise.
 ****/
unsigned long GetElapsedDays(LicenseData *licensedat)
{
    FnTrace("GetElapsedDays()");
    unsigned long retval = 0;
    unsigned long elapsed;

    elapsed = GetElapsedSeconds(licensedat);
    retval = elapsed / 86400;
    return retval;
}

/****
 * LicenseExpired:  Returns 1 if LICENSE_EXPIRE time has passed since
 *  the last successful license, 2 if LICENSE_GRACE is also used up,
 *  or 0 if the license is up to date.
 ****/
int LicenseExpired(LicenseData *licensedat)
{
    FnTrace("LicenseExpired()");
    int retval = 0;
    struct stat sb;

    if (debug_mode && (stat(LICENSE_NOGRACE, &sb) == 0))
    {
        retval = 2;
    }
    else if (debug_mode && (stat(LICENSE_ISEXPIRED, &sb) == 0))
    {
        retval = 1;
    }
    else
    {
        if (licensedat->license_type == -2)
        {
            retval = 2;
        }
        else if (licensedat->license_type == -1)
        {
            unsigned long seconds = GetElapsedSeconds(licensedat);
            if (seconds > (LICENSE_EXPIRE + LICENSE_GRACE))
                retval = 2;
            else if (seconds > LICENSE_EXPIRE)
                retval = 1;
        }
        else if (licensedat->license_paid == 0)
        {
            // if license_paid == 0, then either payment has expired or we
            // simply can't check with the server right now.
            long days = licensedat->license_days - GetElapsedDays(licensedat);
            if (days <= 0)
            {
                days = abs(days * 86400);
                if (days > LICENSE_GRACE)
                    retval = 2;
                else
                    retval = 1;
            }
        }
    }
    return retval;
}

/****
 * LicenseCheckDue:  Returns 1 if LICENSE_PERIOD time has passed since
 *  the last time the license was verified with the license server,
 *  0 otherwise.  This is so that if ViewTouch needs to be started 12
 *  times in one day, we don't need to negotiate a license each time.

 *  However, if LICENSE_PERIOD time has elapsed, we'll check every time.
 ****/
int LicenseCheckDue(LicenseData *licensedat)
{
    FnTrace("LicenseCheckDue()");
    // bkk: always check the license
    /*
    int retval = 0;
    struct stat sb;

    if (stat(LICENSE_FORCE_CHECK, &sb) == 0)
    {
        retval = 1;
    }
    else if (licensedat->license_date == 0)
    {
        retval = 1;
    }
    else if (licensedat->license_type == 2)
    {
        retval = 1;
    }
    else
    {
        unsigned long elapsed = GetElapsedSeconds(licensedat);
        if (elapsed > LICENSE_PERIOD)
            retval = 1;
    }
    return retval;
    */
    return 1;
}

/*******
 * FirstRunInitialize:  Deletes all current and archive data and sets up the
 *  LicenseData struct for first run.
 *******/
int FirstRunInitialize(LicenseData *licensedat)
{
    FnTrace("FirstRunInitialize()");
    int retval = 0;
    time_t now = time(NULL);

    if (debug_mode)
        printf("Initializing for first run...\n");

    licensedat->start_date = now;
    licensedat->license_date = 0;
    licensedat->num_terminals = DEFAULT_TERMINALS;
    licensedat->num_printers = DEFAULT_PRINTERS;
    GetMachineDigest(licensedat->digest, STRLONG);
    WriteLicenseData(licensedat);
    return retval;
}

/****
 * SystemShutDown:  Just brute force a kill cycle.  We'll destroy everything,
 *  hack and slash.  I should probably eventually straighten this up to do a nice
 *  polite shutdown.  Maybe set a flag in manager.cc or terminal.cc so that it will
 *  start up, post a message to the main screen and wait for a moment, then die
 *  quietly.
 *
 *  For now, though, just die.
 ****/
int SystemShutDown()
{
    FnTrace("SystemShutDown()");
    KillTask("vt_term");
    KillTask("vtpos");
    KillTask("vt_main");
    exit(1);
}

/****
 * FreeHostEnt:  Frees the space allocated by ReadHostEnt.
 ****/
int FreeHostEnt(struct hostent *hp)
{
    FnTrace("FreeHostEnt()");
    char* *ptr;

    if (hp != NULL)
    {
        if (hp->h_name != NULL)
            free(hp->h_name);
        if (hp->h_aliases != NULL)
        {
            for (ptr = hp->h_aliases; *ptr != NULL; ptr++)
                free(*ptr);
            free(hp->h_aliases);
        }
        if (hp->h_addr_list != NULL)
        {
            for (ptr = hp->h_addr_list; *ptr != NULL; ptr++)
                free(*ptr);
            free(hp->h_addr_list);
        }
        free(hp);
    }
    return 0;
}

/****
 * WriteHostEnt:  Writes a hostent structure to the supplied file
 *   descriptor in "key:  value\n" format.
 *  Returns 0 on success, 1 on failure
 ****/
int WriteHostEnt(int fd, struct hostent *hp)
{
    FnTrace("WriteHostEnt()");
    int retval = 1;
    char buffer[STRLONG];
    char addr[STRLONG];
    char* *ptr;
    int count;

    if (fd > 0 && hp != NULL)
    {
        snprintf(buffer, STRLONG, "h_name:  %s\n", hp->h_name);
        write(fd, buffer, strlen(buffer));
        count = 0;
        for (ptr = hp->h_aliases; *ptr != NULL; ptr++)
            count += 1;
        snprintf(buffer, STRLONG, "numaliases:  %d\n", count);
        write(fd, buffer, strlen(buffer));
        for (ptr = hp->h_aliases; *ptr != NULL; ptr++)
        {
            snprintf(buffer, STRLONG, "alias:  %s\n", *ptr);
            write(fd, buffer, strlen(buffer));
        }
        snprintf(buffer, STRLONG, "h_addrtype:  %d\n", hp->h_addrtype);
        write(fd, buffer, strlen(buffer));
        snprintf(buffer, STRLONG, "h_length:  %d\n", hp->h_length);
        write(fd, buffer, strlen(buffer));
        count = 0;
        for (ptr = hp->h_addr_list; *ptr != NULL; ptr++)
            count += 1;
        snprintf(buffer, STRLONG, "numaddrs:  %d\n", count);
        write(fd, buffer, strlen(buffer));
        for (ptr = hp->h_addr_list; *ptr != NULL; ptr++)
        {
            snprintf(buffer, STRLONG, "address:  %s\n",
                     inet_ntop(hp->h_addrtype, *ptr, addr, STRLONG));
            write(fd, buffer, strlen(buffer));
        }
        retval = 0;
    }
    return retval;
}

/****
 * ReadHostEnt:  Reads a hostent structure from the supplied file
 *   descriptor in "key:  value\n" format.
 *  Returns NULL on failure, a filled hostent struct otherwise.
 ****/
struct hostent *ReadHostEnt(int fd)
{
    FnTrace("ReadHostEnt()");
    struct hostent *hp;
    char key[STRLONG];
    char value[STRLONG];
    char* *ptr = NULL;
    int num;
    KeyValueInputFile kvfile;

    kvfile.Set(fd);
    hp = (struct hostent *)calloc(1, sizeof(struct hostent));
    while ((hp != NULL) && (kvfile.Read(key, value, STRLONG) > 0))
    {
        if (strcmp(key, "bad") == 0)
        {
            free(hp);
            hp = NULL;
        }
        else if (strcmp(key, "h_name") == 0)
        {
            hp->h_name = (char* )malloc(strlen(value) + 1);
            strcpy(hp->h_name, value);
        }
        else if (strcmp(key, "numaliases") == 0)
        {
            num = atoi(value);
            hp->h_aliases = (char* *)calloc(num + 1, sizeof(const char* ));
            ptr = hp->h_aliases;
        }
        else if (strcmp(key, "alias") == 0)
        {   // this should never come before numaliases
            // ptr needs to be assigned properly
            *ptr = (genericChar* )malloc(strlen(value) + 1);
            strcpy(*ptr, value);
        }
        else if (strcmp(key, "h_addrtype") == 0)
        {
            hp->h_addrtype = atoi(value);
        }
        else if (strcmp(key, "h_length") == 0)
        {
            hp->h_length = atoi(value);
        }
        else if (strcmp(key, "numaddrs") == 0)
        {
            num = atoi(value);
            hp->h_addr_list = (char* *)calloc(num + 1, sizeof(const char* ));
            ptr = hp->h_addr_list;
        }
        else if (strcmp(key, "address") == 0)
        {
            *ptr = (char* )malloc(strlen(value) + 1);
            inet_pton(hp->h_addrtype, value, *ptr);
            ptr++;
        }
    }
    return hp;
}

/****
 * PrintHostEnt:  Debug function to display the member of a hostent
 *   structure.
 *  Return value is probably irrelevant.  I'm not using it.
 ****/
int PrintHostEnt(struct hostent *hp)
{
    FnTrace("PrintHostEnt()");
    int retval = 0;
    char* *ptr;
    char addr[STRLONG];

    if (hp != NULL)
    {
        printf("Name:  '%s'\n", hp->h_name);
        if (hp->h_aliases != NULL)
        {
            for (ptr = hp->h_aliases; *ptr != NULL; ptr++)
                printf("\tAlias:  '%s'\n", *ptr);
        }
        printf("\tAddress Type:  %d\n", hp->h_addrtype);
        printf("\tAddress Length:  %d\n", hp->h_length);
        if (hp->h_addr_list != NULL)
        {
            for (ptr = hp->h_addr_list; *ptr != NULL; ptr++)
                printf("\tAddress:  '%s'\n", inet_ntop(hp->h_addrtype, *ptr, addr, STRLONG));
        }
    }
    else
    {
        printf("No hostent information.\n");
    }
    return retval;
}

/****
 * DNSLookup:  Assumes the basic hostent struct (hp) is allocated.
 *   Will allocate space for the additional members (h_name,
 *   h_aliases, and h_addr_list).  Forks into child and parent.  The
 *   child will do a DNS lookup via gethostbyname().  The parent will
 *   check every few milliseconds (using select() as a brief sleep())
 *   to find out if the child has exited.  If it has, then it should
 *   have written DNS information to the parent.  If it has not,
 *   then we'll only wait a short period of time (3-5 seconds) before
 *   giving up.
 *  Returns NULL on failure, or a filled hostent struct.
 ****/
#define DNSWAITLEN  10  /* number of milliseconds to wait between dns tries */
#define DNSWAITNUM 300  /* number of times to wait (total time is WAITLEN * WAITNUM) */
#define READ         0  /* the read and write descriptors for the ipc pipe */
#define WRITE        1
struct hostent *DNSLookup(const char* name)
{
    printf("DNSLookup\n");
    FnTrace("DNSLookup()");
    struct hostent *hp = NULL;
    pid_t pid;
    int ipc[2];
    int x;
    int pidstat;
    struct timeval timeout;
    char badstr[] = "bad:  yes\n";
    char buffer[STRLENGTH];

    if (debug_mode)
        printf("Forking for DNSLookup\n");
    if (pipe(ipc) > -1)
    {
        pid = fork();
        if (pid == 0)
        {  // child
            close(ipc[READ]);
            vt_setproctitle("vt_main dns");
            hp = gethostbyname(name);
            if (hp != NULL)
                WriteHostEnt(ipc[WRITE], hp);
            else
            {
                snprintf(buffer, STRLENGTH, "gethostbyname DNSLookup %s", name);
                herror(buffer);
                write(ipc[WRITE], badstr, strlen(badstr));
            }
            close(ipc[WRITE]);
            exit(0);
        }
        else if (pid > 0)
        {  // parent
            printf("DNSLookup fork created\n");
            close(ipc[WRITE]);
            // we'll try for a few seconds
            // (roughly DNSWAITLEN * DNSWAITNUM milliseconds)
            for (x = 0; x < DNSWAITNUM; x++)
            {
                int res = waitpid(pid, &pidstat, WNOHANG);
                printf("DNSLookup res=%d\n",res);
                if (res > 0)
                {
                    hp = ReadHostEnt(ipc[READ]);
                    x = DNSWAITNUM;  // exit the loop
                }
                else
                {  // 10 usec sleep
                    timeout.tv_sec = 0;
                    timeout.tv_usec = DNSWAITLEN;
                    select(0, NULL, NULL, NULL, &timeout);
                }
                usleep(100000);
            }
            close(ipc[READ]);
        }
        else
        {  // error
            perror("fork");
        }
    }
    else
    {
        printf("DNSLookup pipes");
        perror("pipe");
    }

    if (!hp)
        printf("DNSLookup !hp\n");
    return hp;
}

/*******
 * OpenSession:  Connects to the server, returning the socket descriptor on success,
 *  0 on failure.
 *******/
int OpenSession()
{
    printf("OpenSession");
    FnTrace("OpenSession()");
    int retval = 0;
    int sockfd;
    struct sockaddr_in servaddr;
    struct in_addr **pptr;
    struct hostent *hp;
    struct servent *sp;

    hp = DNSLookup(LICENSE_SERVER);
    if (hp != NULL)
    {
        sp = getservbyname(LICENSE_SERVICE, "tcp");
        if (sp != NULL)
        {
            pptr = (struct in_addr **)hp->h_addr_list;
            for (; *pptr != NULL; pptr++)
            {
                sockfd = socket(AF_INET, SOCK_STREAM, 0);
                bzero(&servaddr, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_port = sp->s_port;
                memcpy(&servaddr.sin_addr, *pptr, sizeof(struct in_addr));
                if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0)
                {
                    retval = sockfd;
                    break;
                }
            }
        }
        else
        {
            herror("getservbyname");
        }
        FreeHostEnt(hp);
    } else {
    	printf("CantFindLicenseServerDNS\n");
    }
    
    printf("OpenSession retval(%d)",retval);
    
    return retval;
}

/****
 * SendData:  Sends the following data->
 *  POST /cgi-bin/vt_check_license.cgi HTTP/1.0
 *  Content-Length:  xxxxxx
 *
 *  <data>
 ****/
int SendData(int sockfd, const char* hwid)
{
    FnTrace("SendData()");
    int retval = 0;
    int bytes;
    char buffer[STRLONG];
    char varsbuff[STRLONG];

    // first generate the body so that we can get content length
    snprintf(varsbuff, STRLONG, "hwid=%s&vtbuild=%s&protocol=%d",
             hwid, BuildNumber, PROTOCOL_VERSION); // hwid is bad

    printf("SendData(%s)",varsbuff);
    
    // send the headers
    snprintf(buffer, STRLONG, "POST %s HTTP/1.0\n", LICENSE_PATH);
    bytes = write(sockfd, buffer, strlen(buffer));
    snprintf(buffer, STRLONG, "Content-Length: %ld\n\n", (long) strlen(varsbuff));
    bytes = write(sockfd, buffer, strlen(buffer));

    // send the body
    bytes = write(sockfd, varsbuff, strlen(varsbuff));
    return retval;
}

/****
 * ParseBodyData:  
 ****/
int ParseBodyData(LicenseData *licensedat, char* data, int datalen)
{
    printf("ParseBodyData\n");
    FnTrace("ParseBodyData()");
    char decrypted[STRLONG];
    int retval = 0;
    int value = 0;
    int which = 0;
    int idx = 0;
    int sign = 0;  // true = negative number
    char* ptr;
    uint8_t digest[SHA1HashSize + 1];
    SHA1Context shacontext;

    printf("SERVER(%s)\n",(const char*)data);
    
    // may need to decrypt the data
    if (protocol > 1)
    {
        // generate the decryption key, SHA1 digest of build number
        SHA1Reset(&shacontext);
        SHA1Input(&shacontext, (uint8_t *)BuildNumber, strlen(BuildNumber));
        SHA1Result(&shacontext, digest);
        // now decrypt the data
        BFDecrypt((char* )digest, SHA1HashSize, decrypted, STRLONG, data, &datalen);
        data[0] = '\0';
        strncpy(data, decrypted, datalen);
        data[datalen] = '\0';
    }

    // verify we have valid data
    while ((data[idx] != '\0') && (idx < datalen))
    {
        if ((data[idx] != ',') &&
            (data[idx] != '-') &&
            !isdigit(data[idx]))
        {
            return 1;
        }
        idx += 1;
    }
    idx = 0;

    ptr = data;
    while ((*ptr != '\0') && (idx < datalen))
    {
        if (isdigit(*ptr))
        {
            value *= 10;
            value += (*ptr - 48);
        }
        else if (*ptr == '-')
        {
            sign = 1;
        }
        else if (*ptr == ',')
        {
            if (which == 0)
                licensedat->license_type = sign ? -value : value;
            else if (which == 1)
                licensedat->license_paid = sign ? -value : value;
            else if (which == 2)
                licensedat->license_days = sign ? -value : value;
            else
                licensedat->num_terminals = sign ? -value : value;
            which += 1;
            value = 0;
            sign = 0;
        }
        ptr++;
        idx += 1;
    }
    licensedat->num_printers = sign ? -value : value;
    
    printf("ParseBodyData(%d) 0=OK\n",retval);
    
    return retval;
}

/****
 * ReadData:  
 ****/
int ReadData(int sockfd, LicenseData *licensedat)
{
    printf("ReadData\n");
    FnTrace("ReadData()");
    int retval = 0;
    int bytes;
    char buffer[STRLONG];
    char* data = NULL;
    char key[STRLONG];
    char first[STRLONG];
    int keyidx = 0;
    int value = 0;
    char val[STRLONG];
    int validx = 0;
    int datalen = 0;
    int body = 0;
    int newlines = 0;
    int idx;
    int firstline = 0;
    int contentlength = 0;
    int contentmax = 0;
    
    bytes = read(sockfd, buffer, STRLONG);
    printf("ReadData pre(%d) buffer(%s)\n",bytes,(const char*)buffer);
    
    while (bytes > 0)
    {
        idx = 0;
        if (firstline == 0)
        {
            firstline = 1;
            while (buffer[idx] != '\n')
            {
                first[idx] = buffer[idx];
                idx += 1;
            }
            first[idx] = '\0';
        }
        if (body)
        {   // grab more body data
            if (contentmax == 0)
                contentmax = ( contentlength ? contentlength : STRLONG ) + 1;
            if ((datalen + bytes) < contentmax)
            {
                if (data == NULL)
                    data = (char* )malloc(contentmax);
                memcpy(&data[datalen], buffer, bytes);
                datalen += bytes;
            }
        }
        else
        {   // parse out the headers
            while (idx < bytes)
            {
                if (buffer[idx] == '\r')
                {   // ignore carriage returns
                }
                else if (newlines && (buffer[idx] == '\n'))
                {   // second newline, end of headers
                    body = 1;
                    idx += 1;
                    if (contentmax == 0)
                        contentmax = ( contentlength ? contentlength : STRLONG ) + 1;
                    if (data == NULL)
                        data = (char* )malloc(contentmax);
                    memcpy(&data[datalen], &buffer[idx], bytes - idx);
                    datalen += ( bytes - idx );
                    idx = bytes;  // end this loop
                }
                else if (buffer[idx] == '\n')
                {
                    if (keyidx && validx)
                    {   // have a key/value pair
                        key[keyidx] = '\0';
                        val[validx] = '\0';
                        StripWhiteSpace(key);
                        StripWhiteSpace(val);
                        if (strcasecmp(key, "Content-Length") == 0)
                            contentlength = atoi(val);
                        else if(strcasecmp(key, "Protocol-Version") == 0)
                            protocol = atoi(val);
                    }
                    newlines = 1;
                    keyidx = 0;
                    validx = 0;
                    value = 0;
                }
                else
                {
                    newlines = 0;
                    if (buffer[idx] == ':')
                    {   // end of key, move to value
                        value = 1;
                    }
                    else if (value)
                    {
                        val[validx] = buffer[idx];
                        validx++;
                    }
                    else
                    {
                        key[keyidx] = buffer[idx];
                        keyidx++;
                    }
                }
                idx++;
            }
        }
        bytes = read(sockfd, buffer, STRLONG);
    }
    
    printf("ReadData len(%d)\n",datalen);
    
    if (ParseBodyData(licensedat, data, datalen) == 1)
    {
        printf("ReadData ERROR in parsing\n");
        retval = 1;
    }
    if (data != NULL)
        free(data);

    return retval;
}

/****
 * CheckLicense:  Connects to the licensing server and downloads license
 *   information based on the hash sent (system's MAC + other data).
 *   Returns 0 on successful connection to licensing server, 1 on failure.
 ****/
int CheckLicense(LicenseData *licensedat)
{
    printf("CheckLicense\n");
    FnTrace("CheckLicense()");
    int retval = 1;
    int sockfd;
    struct stat sb;
    char digest[STRLONG];

#ifdef DEBUG
    // if the "skip" file *does* exist, abort
    if (stat(LICENSE_SKIP, &sb) == 0) {
    	printf("Skipping License Check\n");
        return 0;
    }
#endif

    // open a socket to the web server (send headers here?)
    sockfd = OpenSession();
    if (sockfd > 0)
    {  // if open socket
    	//printf("Socket Open\n");
        GetMachineDigest(digest, STRLONG);
        
        SendData(sockfd, digest);
        if (ReadData(sockfd, licensedat) == 0)
        {
            retval = 0;  // successful check
            close(sockfd);
            // update license_date if this system is registered
            if (licensedat->license_type > 0)
            {
                licensedat->license_date = time(NULL);
                WriteLicenseData(licensedat);
            }
        }
    }

    printf("CheckLicense retval(%d)\n",retval);
    return retval;
}

/****
 * GetExpirationDate:  This function should be renamed.  It returns
 *  the number of days left on the current license after processing
 *  for first run (wipe all current and archive data, etc.), illicit
 *  license manipulation, etc.
 ****/
int GetExpirationDate(Str &settings_license, int force_check)
{
    FnTrace("GetExpirationDate()");
    int retval = 0;
    int firstrun = 0;
    int valid = 0;
    int check_failed = 0;

    ReadLicenseData(&license_data);
    // The information in license_data should now be exactly what we saved off.
    if (license_data.start_date == 0)
    {
        firstrun = 1;
        FirstRunInitialize(&license_data);
    }

    // Verify we have a valid key.  If it's invalid for any reason, always
    // treat this as a first run.
    valid = ValidLicenseKey(&license_data);
    if (valid < 1)
    {
        firstrun = 1;
        FirstRunInitialize(&license_data);
    }

    if (force_check || valid == 0 || LicenseCheckDue(&license_data) == 1)
    {
        check_failed = CheckLicense(&license_data);
        if (check_failed)
        // can't contact server to verify license -- die
        {
            ViewTouchError("Unable to contact license server.\\Your network may be down.\\");
            sleep(10);
            SystemShutDown();
        }
        int tampered = (check_failed && firstrun && settings_license.length);
        int expired  = LicenseExpired(&license_data);

        if (tampered || expired || (check_failed && !firstrun))
        {
            genericChar message[STRLONG];
            snprintf(message, STRLONG, "%s\\Key: %s",
                     "Your license has expired.", license_data.digest);
            // expired > 1 ==> past grace period, too
            if (expired > 1)
            {
                ViewTouchError(message);
                if (TryTempLicense() == 0)
                    SystemShutDown();
                else
                {
                    // make sure we have some terminals and printers
                    license_data.num_terminals = 4;
                    license_data.num_printers = 4;
                }
            }
            else // in grace period -- let them have their cake and eat it too
            {
                ViewTouchError(message);
            }
        }
    }

    settings_license.Set(license_data.license_digest);
    if (license_data.license_type == 1)
    {  // periodic
        // license_days was set last time we checked with the server.
        // subtract the time that has elapsed since then.
        retval = license_data.license_days - GetElapsedDays(&license_data);
    }
    else if (license_data.license_type == 2)
    {  // permanent
        retval = 1000;  // arbitrary huge value
    }
    else if (license_data.license_type == -1)
    {  // unregistered
        int expire = LICENSE_EXPIRE / DAY;
        int elapsed = GetElapsedDays(&license_data);
        retval = expire - elapsed;
    }
    printf("Machine ID:  %s\n", license_data.digest);

    return retval;
}

/*******
 * SaveLicenseData:  To be called at system shutdown, this function needs to take care
 *  of saving off current directory information and anything else that is used to
 *  validate the license file on startup.
 *******/
int SaveLicenseData(void)
{
    FnTrace("SaveLicenseData()");
    int retval = 0;

    WriteLicenseData(&license_data);
    return retval;
}

/****
 * NumLicensedTerminals:  
 ****/
int NumLicensedTerminals(void)
{
    FnTrace("NumLicensedTerminals()");
    int retval = license_data.num_terminals;
    return retval;
}

/****
 * NumLicensedPrinters:  
 ****/
int NumLicensedPrinters(void)
{
    FnTrace("NumLicensedPrinters()");
    int retval = license_data.num_printers;
    return retval;
}

/****
 * PrintLicense:  Debug function
 ****/
int PrintLicense(LicenseData *license)
{
    FnTrace("PrintLicense()");
    if (debug_mode)  // just to be sure
    {
        printf("\n");
        printf("Start Date:  %ld\n", license->start_date);
        printf("License Date:  %ld\n", license->license_date);
        printf("MAC:  %s\n", license->mac_address);
        printf("Uname:  %s\n", license->uname_info);
        printf("Digest:  %s\n", license->digest);
        printf("License Digest:  %s\n", license->license_digest);
        printf("License Type:  %d\n", license->license_type);
        printf("License Paid:  %d\n", license->license_paid);
        printf("License Days:  %d\n", license->license_days);
        printf("Terminals:  %d\n", license->num_terminals);
        printf("Printers:  %d\n", license->num_printers);
        printf("\n");
    }
    return 0;
}

/****
 * TempKeyValue: Figures out which member of TempLicenseData wants the
 *   given value, if any.  Returns 1 if the key is unknown, 0 if the
 *   value was assigned to the proper member.
 ****/
int TempKeyValue(TempLicenseData *templicense, const genericChar* key, const genericChar* value)
{
    FnTrace("TempKeyValue()");
    int retval = 0;
    
    if (strcmp(key, "date") == 0)
        templicense->date = atol(value);
    else if (strcmp(key, "license key") == 0)
        strcpy(templicense->license_key, value);
    else if (strcmp(key, "license digest") == 0)
        strcpy(templicense->license_digest, value);
    else
        retval = 1;

    return retval;
}

/****
 * ReadTempLicense:  
 ****/
int ReadTempLicense(void)
{
    FnTrace("ReadTempLicense()");
    genericChar key[STRLENGTH];
    genericChar value[STRLENGTH];
    int retval = 1;
    KeyValueInputFile kvfile;

    if (kvfile.Open(LICENSE_FILE_TEMP))
    {
        while (kvfile.Read(key, value, STRLENGTH) > 0)
            TempKeyValue(&temp_license, key, value);
        kvfile.Close();
        retval = 0;
    }
    return retval;
}

/****
 * GetTempLicenseDigest:  We build the digest for the temp license out of:
 *  o  the short version of the temporary key (generated by vt_license.cgi)
 *  o  the date the temporary key was issued (first recorded into a file)
 *  o  the long version of the temporary key
 *  o  the Machine ID (license_data.digest)
 *  o  the ViewTouch BuildNumber
 ****/
int GetTempLicenseDigest(genericChar* dest, int maxlen, time_t date, const genericChar* key)
{
    FnTrace("GetTempLicenseDigest()");
    int retval = 0;
    genericChar long_key[STRLONG];
    genericChar digest_string[STRLONG];

    // get the long version of the key
    GenerateTempKeyLong(long_key, STRLONG, date, license_data.digest);
    snprintf(digest_string, STRLONG, "%s %ld %s %s %s", temp_license.license_key,
             temp_license.date, long_key, license_data.digest, BuildNumber);
    DigestString(dest, maxlen, digest_string);

    return retval;
}

/****
 * WriteTempLicense:  
 ****/
int WriteTempLicense(void)
{
    FnTrace("WriteTempLicense()");
    genericChar buffer[STRLONG];
    int retval = 1;
    int fd = -1;
    time_t now = time(NULL);
    genericChar* key = temp_license.license_key;
    genericChar* digest = temp_license.license_digest;

    temp_license.date = now;
    GetTempLicenseDigest(digest, STRLONG, now, key);

    fd = open(LICENSE_FILE_TEMP, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd > 0)
    {
        snprintf(buffer, STRLONG, "date:  %ld\n", temp_license.date);
        write(fd, buffer, strlen(buffer));
        snprintf(buffer, STRLONG, "license key:  %s\n", temp_license.license_key);
        write(fd, buffer, strlen(buffer));
        snprintf(buffer, STRLONG, "license digest:  %s\n", temp_license.license_digest);
        write(fd, buffer, strlen(buffer));
        close(fd);
        retval = 0;
    }
    else if (fd < 0)
    {
        snprintf(buffer, STRLONG, "WriteTempLicense Error %d opening %s",
                 errno, LICENSE_FILE_TEMP);
        ReportError(buffer);
    }
    return retval;
}

/****
 * ValidTempLicense: For those situations where Internet access is
 *   temporarily unavailable.  Validates temp_license_key, which is a
 *   digest constructed by sha1 based on today's date (weekday, day,
 *   month, and year only, not hours or minutes) and the license digest.
 *   Returns 1 for a valid license, 0 for an invalid license.
 ****/
int ValidTempLicense(const genericChar* temp_license_key)
{
    FnTrace("ValidTempLicense()");
    int retval = 0;  // guaranteed overdue
    genericChar key[STRLONG];

    if (GenerateTempKey(key, STRLONG, license_data.digest) == 0)
    {
        if (strcmp(key, temp_license_key) == 0)
            retval = 1;
    }

    return retval;
}

/****
 * ValidTempLicenseFile:  Returns 1 if the templicense information is valid,
 *  0 otherwise.
 ****/
int ValidTempLicenseFile(TempLicenseData *templicense)
{
    FnTrace("ValidTempLicenseFile()");
    int retval = 0;
    time_t date = templicense->date;
    const genericChar* key = templicense->license_key;
    const genericChar* digest = templicense->license_digest;
    genericChar tempdigest[STRLONG];
    time_t now = time(NULL);
    struct stat sb;
    int debug_valid = 0;
    
    GetTempLicenseDigest(tempdigest, STRLONG, date, key);
    if (debug_mode && (stat(LICENSE_VALID, &sb) == 0))
        debug_valid = 1;

    if (debug_valid || (strcmp(tempdigest, digest) == 0))
    {
        // we have a valid file, let's check the date
        if ((now - date) <= LICENSE_TEMPORARY)
        {
            int days = (LICENSE_TEMPORARY - (now - date)) / 86400;
            printf("%d days on temporary license\n", days);
            retval = 1;
        }
    }

    return retval;
}

/****
 * TryTempLicense: gets a temporary license from a file or from the
 *   user and validates it.  Returns 1 if the temporary license is
 *   available and valid, 0 otherwise.
 ****/
int TryTempLicense()
{
    FnTrace("TryTempLicense()");
    int retval = 0;
    genericChar tempkey[STRLONG];
    int readlen = 0;
    int done = 0;

    // first find out if we have a valid temp license file
    if (ReadTempLicense() == 0)
    {
        if (ValidTempLicenseFile(&temp_license))
        {
            printf("Found valid temporary license.\n");
            done = 1;
            retval = 1;
        }
    }

    // if not, get a license from the user and validate it
    while (done == 0)
    {  // get license from user
        readlen = ViewTouchLicense(tempkey, STRLONG);
        if (strcmp(tempkey, "quit") == 0 || strcmp(tempkey, "QUIT") == 0)
            done = 1;
        else if (ValidTempLicense(tempkey))
        {
            strcpy(temp_license.license_key, tempkey);
            WriteTempLicense();
            retval = 1;
            done = 1;
        }
    }

    return retval;
}
