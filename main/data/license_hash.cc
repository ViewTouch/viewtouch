
/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025, 2026
  
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

#include "license_hash.hh"

#include "basic.hh" // genericChar
#include "safe_string_utils.hh"

#include <sys/utsname.h> // uname()
#include <net/if.h> // ifreq, ifconf
#include <netinet/in.h> // IPPROTO_IP
#include <unistd.h> // close()

#include <cstring> // strcat, memset
#include <cstdio> // snprintf
#include <cstddef> // size_t


#ifdef BSD
#include <net/if_dl.h>
#endif

#ifdef LINUX
#include <sys/ioctl.h> // ioctl, SIOCGIFHWADDR
#endif

#include "src/utils/cpp23_utils.hh"

#ifdef DMALLOC
#include <dmalloc.h>
#endif


constexpr size_t LICENCE_HASH_STRLENGTH = 256;
constexpr size_t LICENCE_HASH_STRLONG  = 4096;
constexpr size_t MAXTEMPLEN = 20;

int GetUnameInfo(char* buffer, int bufflen)
{
    // Ensure buffer is always initialized
    if (buffer && bufflen > 0)
        buffer[0] = '\0';

    struct utsname utsbuff;

    if (uname(&utsbuff) != 0 || buffer == nullptr || bufflen <= 0)
        return 1;

    // Format system info without leaking it to stdout in production
    vt::cpp23::format_to_buffer(buffer, bufflen, "{} {} {} {}", utsbuff.sysname, utsbuff.nodename,
             utsbuff.release, utsbuff.machine);
    return 0;
}

#if defined(__FreeBSD__) || defined(__DragonFly__)

#include <sys/types.h>
#include <sys/sysctl.h>
#include <net/if_dl.h>
#include <net/route.h>

/* bkk bsd6 compile */
typedef unsigned int u_int;

/****
 * GetInterfaceInfo:  grab the MAC.  This version uses the sysctl method,
 *  which works fine for FreeBSD, but not Linux.
 ****/
int GetInterfaceInfo(char* stringbuff, int stringlen)
{
    size_t len;
    int mib[6];
    char* buffer;
    const char* next;
    char address[256];
    const char* limit;
    struct if_msghdr *ifmsg;
    struct sockaddr_dl *sdl;
    int retval = 1;

    mib[0] = CTL_NET;
    mib[1] = AF_ROUTE;
    mib[2] = 0;
    mib[3] = AF_INET;
    mib[4] = NET_RT_IFLIST;
    mib[5] = 0;

    if (sysctl(mib, 6, nullptr, &len, nullptr, 0) < 0)
        return 1;
    if ((buffer = (char*)malloc(len)) == nullptr)
        return 1;
    if (sysctl(mib, 6, buffer, &len, nullptr, 0) < 0) {
        free(buffer);
        return 1;
    }

    stringbuff[0] = '\0';
    limit = buffer + len;
    next = buffer;
    while (next < limit)
    {
        ifmsg = (struct if_msghdr *) next;
        if (ifmsg->ifm_type == RTM_IFINFO)
        {
			sdl = (struct sockaddr_dl *)(ifmsg + 1);
            if (sdl->sdl_alen > 0)
            {
                memcpy(address, LLADDR(sdl), sdl->sdl_alen);
                vt::cpp23::format_to_buffer(stringbuff, stringlen, "{}", link_ntoa(sdl));
                retval = 0;
            }
        }
        next += ifmsg->ifm_msglen;
    }
    free(buffer);
    return retval;
}
#endif  /* BSD */

#ifdef LINUX
/*******
 * MacToString:  
 *******/
int MacToString(char* macstr, int maxlen, unsigned const char* mac)
{
    int retval = 0;
    int idx;
    char buffer[LICENCE_HASH_STRLENGTH];

    macstr[0] = '\0';
    for (idx = 0; idx < IFHWADDRLEN; idx++)
    {
        if (idx)
            vt_safe_string::safe_concat(macstr, maxlen, ":");
        vt::cpp23::format_to_buffer(buffer, LICENCE_HASH_STRLENGTH, "{:02X}", mac[idx]);
        vt_safe_string::safe_concat(macstr, maxlen, buffer);
    }

    return retval;
}

/*******
 * MacFromName:  
 *******/
int MacFromName(unsigned char* mac, const char* name, int sockfd)
{
    int retval = 1;
    int idx;
    int count = 0;
	struct ifreq ifr;

	memset(&ifr,0,sizeof(struct ifreq));
	vt_safe_string::safe_copy(ifr.ifr_name, IFNAMSIZ, name);
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
 
        vt_safe_string::safe_format(ifr.ifr_name, IFNAMSIZ, "eth0");
  
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if(sockfd < 0){
                perror("socket");
                exit(1);
        }
  
        io = ioctl(sockfd, SIOCGIFHWADDR, (const char* )&ifr);
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
        vt_safe_string::safe_copy(ifr.ifr_name, IFNAMSIZ, it->ifr_name);
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
        
        close(sock);  // Critical fix: Close socket before returning
        return 0;
}

/*******
 * GetInterfaceInfo:  
 *******/
int GetInterfaceInfo(char* stringbuf, int stringlen)
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
    const char* buf;
    const char* ptr;
    unsigned char mac[LICENCE_HASH_STRLENGTH];
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
        buf = (const char* )malloc(len);
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
    if (buf != nullptr)
        free(buf);
    
    */
    /**
    int sockfd;
    int io;
    char buffer[1024];
    struct ifreq ifr;
 
    vt_safe_string::safe_format(ifr.ifr_name, IFNAMSIZ, "eth0");
  
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("socket");
        exit(1);
    }
  
    io = ioctl(sockfd, SIOCGIFHWADDR, (const char* )&ifr);
    if(io < 0){
        perror("ioctl");
        return 1;
    }
    
    printf("fetched HW address with ioctl on sockfd.\n");
    vt_safe_string::safe_format(stringbuf, stringlen, "%02X:%02X:%02X:%02X:%02X:%02X",
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
        vt_safe_string::safe_copy(ifr.ifr_name, IFNAMSIZ, it->ifr_name);
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
        
    vt_safe_string::safe_format(stringbuf, stringlen, "%02X:%02X:%02X:%02X:%02X:%02X",
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[0],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[1],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[2],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[3],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[4],
                (unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[5]
               );
    stringlen = strlen((char*)stringbuf);
    
    return success;
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
int GetMacAddress(char* stringbuff, int stringlen)
{
    genericChar mac[LICENCE_HASH_STRLENGTH];

    genericChar chr;
    int retval = 1;
    int idx = 0;
    int stridx = 0;
    int len;

    if (
            GetInterfaceInfo(mac, LICENCE_HASH_STRLENGTH) == 0
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
