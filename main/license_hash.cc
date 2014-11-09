
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
 * license.cc - revision 15 (9/29/98)
 * Shared functions for generating a temporary license.
 */

/* bkk bsd6 compile */
typedef unsigned int u_int;

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <net/if.h>
#include <net/route.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/ioctl.h>

#include "debug.hh"
#include "license_hash.hh"
#include "sha1.hh"
#include "manager.hh"

#ifdef BSD
#include <net/if_dl.h>
#endif

#ifdef LINUX
#include <net/if_arp.h>
#include <sys/ioctl.h>
#endif

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define STRLENGTH   256
#define STRLONG    4096
#define MAXTEMPLEN   20

// these are for internal use only and do not need to be translated
const genericChar *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                         "Aug", "Sep", "Oct", "Nov", "Dec"};
const genericChar *days[]   = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

/****
 * GenerateDDate:  Called by GenerateTempKey to generate a slightly
 *   obfuscated date string, the format of which depends on the
 *   weekday.
 ****/
int GenerateDDate(const genericChar *dest, int maxlen, struct tm *date)
{
    int retval = 0;
    int weekday = date->tm_wday;

    switch (weekday)
    {
    case 0:
        snprintf(dest, maxlen, "%2d/%2d/%4d",
                 date->tm_mday, date->tm_mon + 1, date->tm_year + 1900);
        break;
    case 1:
        snprintf(dest, maxlen, "%2d %2d/%4d",
                 date->tm_mday, date->tm_mon + 1, date->tm_year + 1900);
        break;
    case 2:
        snprintf(dest, maxlen, "%2d %s %4d",
                 date->tm_mday, months[date->tm_mon], date->tm_year + 1900);
        break;
    case 3:
        snprintf(dest, maxlen, "%s %2d, %4d",
                 months[date->tm_mon], date->tm_mday, date->tm_year + 1900);
        break;
    case 4:
        snprintf(dest, maxlen, "%s %s %2d, %4d", days[date->tm_wday],
                 months[date->tm_mon], date->tm_mday, date->tm_year + 1900);
        break;
    case 5:
        snprintf(dest, maxlen, "%s %2d/%2d/%4d", days[date->tm_wday],
                 date->tm_mday, date->tm_mon + 1, date->tm_year + 1900);
        break;
    case 6:
        snprintf(dest, maxlen, "%s %2d %s %4d", days[date->tm_wday],
                 date->tm_mday, months[date->tm_mon], date->tm_year + 1900);
        break;
    }
    return retval;
}

/****
 * StringReverse:  just reverses a string in place.
 ****/
int StringReverse(const genericChar *dest)
{
    int retval = 1;
    int dlen = strlen(dest);
    int didx = dlen - 1;
    int tidx = 0;
    const genericChar *temp;

    if (dlen > 1)
    {
        temp = (const genericChar *)malloc(dlen + 1);
        if (temp != NULL)
        {
            while (tidx < dlen)
            {
                temp[tidx] = dest[didx];
                tidx += 1;
                didx -= 1;
            }
            temp[dlen] = '\0';
            strcpy(dest, temp);
            free(temp);
            retval = 0;
        }
    }
    return retval;
}

/****
 * GenerateDLicense:  Called by GenerateTempKey to generate a slightly
 *   obfuscated license string, the format of which depends on the
 *   weekday.
 ****/
int GenerateDLicense(const genericChar *dest, int maxlen, const genericChar *license, int weekday)
{
    int retval = 0;
    int didx;
    int lidx;
    int pass;
    int llen = strlen(license);

    switch (weekday)
    {
    case 0:  // just the license
        snprintf(dest, maxlen, "%s", license);
        break;
    case 1:  // every odd digit, then every even digit
    case 5:  // the above reversed
        didx = 0;
        for (pass = 0; pass < 2; pass++)
        {
            lidx = pass;
            while (lidx < llen)
            {
                dest[didx] = license[lidx];
                didx += 1;
                lidx += 2;
            }
        }
        dest[didx] = '\0';
        if (weekday == 5)
            retval = StringReverse(dest);
        break;
    case 2:  // ever even digit, then every odd digit
    case 6:  // the above reversed
        didx = 0;
        for (pass = 0; pass < 2; pass++)
        {
            lidx = !pass;
            while (lidx < llen)
            {
                dest[didx] = license[lidx];
                didx += 1;
                lidx += 2;
            }
        }
        dest[didx] = '\0';
        if (weekday == 6)
            retval = StringReverse(dest);
        break;
    case 3:  // odd digits forward, then even digits reversed
        didx = 0;
        lidx = 0;
        while (lidx < llen)
        {
            dest[didx] = license[lidx];
            didx += 1;
            lidx += 2;
        }
        lidx -= 1;
        while (lidx > 0)
        {
            dest[didx] = license[lidx];
            didx += 1;
            lidx -= 2;
        }
        dest[didx] = '\0';
        break;
    case 4:  // license reversed
        strncpy(dest, license, maxlen);
        retval = StringReverse(dest);
        break;
    }
    return retval;
}

/****
 * GenerateTempKey:  Obfuscates things a bit, with consistency, by generating
 *  a temporary license key the format of which depends on the weekday.
 *  The key is built with both a date string and a license string, both of
 *  which are also formatted according to the weekday.
 ****/
int GenerateTempKey(const genericChar *dest, int maxlen, const genericChar *licenseid)
{
    int retval = 0;
    genericChar tempkey[STRLONG];
    int idx = 0;
    time_t timenow = time(NULL);

    GenerateTempKeyLong(tempkey, STRLONG, timenow, licenseid);
    for (idx = 0; idx < MAXTEMPLEN; idx++)
    {
        dest[idx] = tempkey[idx];
    }
    dest[idx] = '\0';
    
    return retval;
}

int GenerateTempKeyLong(const genericChar *dest, int maxlen, time_t timenow, const genericChar *licenseid)
{
    int retval = 0;
    struct tm now;
    genericChar ddate[STRLONG];
    genericChar dlicense[STRLONG];
    genericChar license_string[STRLONG];

    // first we need to construct the digest based on the current time and
    // license id
    localtime_r(&timenow, &now);
    GenerateDDate(ddate, STRLONG, &now);
    GenerateDLicense(dlicense, STRLONG, licenseid, now.tm_wday);
    if (now.tm_mday % 2)
        snprintf(license_string, maxlen, "%s %s", ddate, dlicense);
    else
        snprintf(license_string, maxlen, "%s %s", dlicense, ddate);

    DigestString(dest, maxlen, license_string);

    return retval;
}

int DigestString(const genericChar *dest, int maxlen, const genericChar *source)
{
    int retval = 0;
    uint8_t digest[SHA1HashSize];
    SHA1Context shacontext;
    genericChar buffer[256];
    int idx;

    SHA1Reset(&shacontext);
    SHA1Input(&shacontext, (uint8_t *)source, strlen(source));
    SHA1Result(&shacontext, digest);
    dest[0] = '\0';
    for (idx = 0; idx < 20; idx++)
    {
        snprintf(buffer, STRLENGTH, "%02X", digest[idx]);
        strncat(dest, buffer, maxlen);
    }
    return retval;
}

int GetUnameInfo(const char *buffer, int bufflen)
{
    struct utsname utsbuff;

    if (uname(&utsbuff) == 0)
    {
        snprintf(buffer, bufflen, "%s %s %s %s", utsbuff.sysname, utsbuff.nodename,
                 utsbuff.release, utsbuff.machine);
        printf("GetUnameInfo buffer(%s)\n",(const char*)buffer);
    }
    return 0;
}

#ifdef BSD
/****
 * GetInterfaceInfo:  grab the MAC.  This version uses the sysctl method,
 *  which works fine for FreeBSD, but not Linux.
 ****/
int GetInterfaceInfo(const char *stringbuff, int stringlen)
{
    size_t len;
    int mib[6];
    const char *buffer;
    const char *next;
    char address[256];
    const char *limit;
    struct if_msghdr *ifmsg;
    struct sockaddr_dl *sdl;
    int retval = 1;

    mib[0] = CTL_NET;
    mib[1] = AF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_INET;
    mib[4] = NET_RT_IFLIST;
    mib[5] = 0;

    if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0)
        return 1;
    if ((buffer = (const char *)malloc(len)) == NULL)
        return 1;
    if (sysctl(mib, 6, buffer, &len, NULL, 0) < 0)
        return 1;

    stringbuff[0] = '\0';
    limit = buffer + len;
    next = buffer;
    while (next < limit)
    {
        ifmsg = (struct if_msghdr *) next;
        if (ifmsg->ifm_type == RTM_IFINFO)
        {
			sdl = (struct sockaddr_dl *)(ifmsg + 1);
            if ((sdl->sdl_alen > 0) && (sdl->sdl_alen < 256))
            {
                memcpy(address, LLADDR(sdl), sdl->sdl_alen);
                snprintf(stringbuff, stringlen, "%s", link_ntoa(sdl));
                retval = 0;
            }
        }
        next += ifmsg->ifm_msglen;
    }
    return retval;
}
#endif  /* BSD */

#ifdef LINUX
/*******
 * MacToString:  
 *******/
int MacToString(const char *macstr, int maxlen, unsigned const char *mac)
{
    int retval = 0;
    int idx;
    char buffer[STRLENGTH];

    macstr[0] = '\0';
    for (idx = 0; idx < IFHWADDRLEN; idx++)
    {
        if (idx)
            strcat(macstr, ":");
        snprintf(buffer, STRLENGTH, "%02X", mac[idx]);
        strncat(macstr, buffer, maxlen);
    }

    return retval;
}

/*******
 * MacFromName:  
 *******/
int MacFromName(unsigned const char *mac, const char *name, int sockfd)
{
    int retval = 1;
    int idx;
    int count = 0;
	struct ifreq ifr;

	memset(&ifr,0,sizeof(struct ifreq));
	strcpy(ifr.ifr_name, name);
	if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == 0)
    {
        // I think there's supposed to be an sa_len field indicating
        // how long the hardware address is.  But there's not.  And I'm not
        // sure yet how to check
        for (idx = 0; idx < IFHWADDRLEN; idx++)
            count += ifr.ifr_hwaddr.sa_data[idx];
        if (count)
        {
            memcpy(mac, ifr.ifr_hwaddr.sa_data, IFHWADDRLEN);
            retval = 0;
        }
    }
    else
    {
        perror("ioctl MacFromName SIOCGIFHWADDR");
    }

	return retval;
}

int ListAddresses( )
{
    /**
        int sockfd;
        int io;
        char buffer[1024];
        struct ifreq ifr;
 
        sprintf(ifr.ifr_name, "eth0");
  
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if(sockfd < 0){
                perror("socket");
                exit(1);
        }
  
        io = ioctl(sockfd, SIOCGIFHWADDR, (const char *)&ifr);
        if(io < 0){
                perror("ioctl");
                return 1;
        }
  
        printf("fetched HW address with ioctl on sockfd.\n");
        printf("HW address of interface is: %02X:%02X:%02X:%02X:%02X:%02X\n",
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[0],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[1],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[2],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[3],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[4],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[5]
               ); 
  */
        
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];
    int success = 0;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) { /* handle error*/ };

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) { /* handle error */ }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
            if (! (ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                    success = 1;
                    break;
                }
            }
        }
        else { /* handle error */ }
    }

    unsigned char mac_address[6];

    if (success) memcpy(mac_address, ifr.ifr_hwaddr.sa_data, 6);
        
    printf("ListAddresses mac(%02X:%02X:%02X:%02X:%02X:%02X)\n",
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[0],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[1],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[2],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[3],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[4],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[5]
               );
        
        return 0;
}

/*******
 * GetInterfaceInfo:  
 *******/
int GetInterfaceInfo(const char *stringbuf, int stringlen)
{
    printf("GetInterfaceInfo\n");
    ListAddresses();
    
    /**
    int retval = 1;
    int done = 0;
    int error = 0;
    int loops = 0;
    int maxloops = 2000;
    int sockfd;
    int len;
    int lastlen;
    const char *buf;
    const char *ptr;
    unsigned char mac[STRLENGTH];
    struct ifconf ifc;
    struct ifreq *ifr;
    int ioresult = 0;

    stringbuf[0] = '\0';
    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        printf("GetInterfaceInfo error on socket\n");
        perror("socket GetInterfaceInfo");
        return 1;
    }

    // first we get the list of interfaces with SIOCGIFCONF
    lastlen = 0;
    len = 100 * sizeof(struct ifreq);
    // I've had situations where the system seems to lock in an
    // endless loop somewhere and the CPU usage shoots to 98% or more.
    // This is the only loop I know of that didn't have a forced exit.
    // So we'll wait for an error condition.  Otherwise, we'll go
    // through a maximum of maxloops tries.  We can set maxloops quite
    // high and force the loop to exit when we're done.  This ensures
    // that we can process through the loop a finite but sufficient
    // number of times.  If there is a lock, we should have a limited
    // delay, perhaps a few seconds.
    while (done == 0 && error == 0 && loops < maxloops)
    {
        buf = (const char *)malloc(len);
        ifc.ifc_len = len;
        ifc.ifc_buf = buf;
        
        ioresult = ioctl(sockfd, SIOCGIFCONF, &ifc);
        if (ioresult < 0 && (errno != EINVAL || lastlen != 0))
        {
            perror("ioctl GetInterfaceInfo SIOCGIFCONF");
            error = 1;
        }
        else if (ifc.ifc_len == lastlen)
        {
            // the only successful exit from this loop
            done = 1;
        }
        else
        {
            if (ioresult == 0)
                lastlen = ifc.ifc_len;
            len += 10 * sizeof(struct ifreq);
            free(buf);
            loops += 1;
        }
    }

    
    
    // now step through the interfaces looking for a hardware address
    if (done == 1)
    {
        done = 0;
        ptr = buf;
        while ((done == 0) && (ptr < (buf + ifc.ifc_len)))
        {
            ifr = (struct ifreq *) ptr;
            switch (ifr->ifr_addr.sa_family)
            {
            case AF_INET6:
                len = sizeof(struct sockaddr_in6);
                break;
            default:
                len = sizeof(struct sockaddr);
                break;
            }
            ptr += sizeof(ifr->ifr_name) + len;
            if (MacFromName(mac, ifr->ifr_name, sockfd) == 0)
            {
                if (MacToString(stringbuf, stringlen, mac) == 0)
                {
                    retval = 0;
                    done = 1;
                }
            }
        }
    }
    else
    {
        printf("GetInterfaceInfo ERROR parsing\n");
    }
    
    close(sockfd);
    if (buf != NULL)
        free(buf);
    
    */
    /**
    int sockfd;
    int io;
    char buffer[1024];
    struct ifreq ifr;
 
    sprintf(ifr.ifr_name, "eth0");
  
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("socket");
        exit(1);
    }
  
    io = ioctl(sockfd, SIOCGIFHWADDR, (const char *)&ifr);
    if(io < 0){
        perror("ioctl");
        return 1;
    }
    
    printf("fetched HW address with ioctl on sockfd.\n");
    sprintf(stringbuf,"%02X:%02X:%02X:%02X:%02X:%02X",
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[0],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[1],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[2],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[3],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[4],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[5]
               ); 
    stringlen = strlen((const char*)stringbuf);
    */
    
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];
    int success = 0;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) { 
    };

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) { /* handle error */ }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
            if (! (ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                    success = 1;
                    break;
                }
            }
        }
        else {
            
        }
    }
        
    sprintf(stringbuf,"%02X:%02X:%02X:%02X:%02X:%02X",
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[0],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[1],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[2],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[3],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[4],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[5]
               );
    stringlen = strlen((const char*)stringbuf);
    
    return 0;
}
#endif /* LINUX */

/****
 * GetMacAddress:  The goal here is to strip out the machine id and format
 *  it into something we'll use, so we're going to pull out hex digits.
 *  It doesn't have to be the actual MAC address because we're not going
 *  to use it for anything but hashing.  So things like
 *   ar0 00:60:08:a0:ee:ad
 *  will likely become
 *   A0006008A0EEAD
 *  Which isn't quite right, but as long as that's the ID we get every time,
 *  it'll serve our purposes.
 * 
 *  mika - 201212 failing to fetch the MAC address
 * 
 ****/
int GetMacAddress(const char *stringbuff, int stringlen)
{
    genericChar mac[STRLENGTH];

    genericChar chr;
    int retval = 1;
    int idx = 0;
    int stridx = 0;
    int len;

    if (
            GetInterfaceInfo(mac, STRLENGTH) == 0
            )
    {
        len = strlen(mac);
        while ((idx < len) && (stridx < stringlen))
        {
            chr = mac[idx];
            switch (chr)
            {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                stringbuff[stridx] = chr;
                stridx += 1;
                break;
            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
                stringbuff[stridx] = (chr - 32);
                stridx += 1;
                break;
            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
                stringbuff[stridx] = chr;
                stridx += 1;
                break;
            }
            idx += 1;
        }
        stringbuff[stridx] = '\0';
        retval = 0;
    }
    else
    {
        printf("GetMacAddress ERROR\n");
    }

    return retval;
}

int GetMachineDigest(const char *digest_string, int maxlen)
{
    printf("GetMachineDigest\n");
    int retval = 1;
    genericChar buffer[STRLONG];
    genericChar uname_info[STRLONG];
    genericChar mac_address[STRLONG];

    if (GetUnameInfo(uname_info, STRLONG) == 0 &&
        GetMacAddress(mac_address, STRLONG) == 0)
    {
        snprintf(buffer, STRLONG, "%s%s", mac_address, uname_info);
        printf("GetMachineDigest (%s)\n",(const char*)buffer);
        DigestString(digest_string, maxlen, buffer);
        retval = 0;
    }
    else
    {
        printf("GetMachineDigest ERROR\n");
    }

    return retval;
}

