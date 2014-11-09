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
 * remote_link.cc - revision 11 (8/10/98)
 * Implementation of remote_link module
 */

#include "remote_link.hh"
#include "utility.hh"
#include <sys/file.h>
#include <unistd.h>
#include <string.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#define TYPE_INT8     1
#define TYPE_INT16    2
#define TYPE_INT32    3
#define TYPE_LONG     4
#define TYPE_LLONG    5
#define TYPE_STRING   6

/***********************************************************************
 * CharQueue Class
 ***********************************************************************/
CharQueue::CharQueue(int max_size)
{
    FnTrace("CharQueue::CharQueue()");
    buffer_size = max_size;
    send_size = (max_size / 2);
    if (send_size > 65535)
        send_size = 65535;
	size = 0;

    buffer = new Uchar[buffer_size];

    if (buffer == NULL)
        buffer_size = 0;

    Clear();
    code = 0;
}

CharQueue::~CharQueue()
{
    if (buffer)
        delete [] buffer;
}

void CharQueue::ReadError(int wanted, int got)
{
    FnTrace("CharQueue::ReadError()");
    fprintf(stderr, "For %s code %d, wanted type %d, got %d\n",
            name, code, wanted, got);
    FnPrintTrace();
}

int CharQueue::Send8(int val)
{
    FnTrace("CharQueue::Send8()");
    static int reported = 0;

    if (size >= buffer_size)
    {
        if (reported == 0)
            fprintf(stderr, "CharQueue::Put8() failed! - buffer full\n");
        reported = 1;
        return 1;
    }

    buffer[end] = (Uchar) (val & 255);
    ++end;
    ++size;
    if (end >= buffer_size)
        end = 0;
    return 0;
}

int CharQueue::Read8()
{
    FnTrace("CharQueue::Read8()");
    static int reported = 0;

    if (size <= 0)
    {
        if (reported == 0)
            fprintf(stderr, "CharQueue::Get8() buffer empty (%d %d %d)\n", start, end, size);
        reported = 1;
        return -1;
    }

    int val = (int) buffer[start];
    ++start;
    --size;
    if (start >= buffer_size)
        start = 0;

    if (val < 0)
        fprintf(stderr, "low value: %d\n", val);

    return val;
}

int CharQueue::Put8(int val)
{
    FnTrace("CharQueue::Put8()");
    Send8(TYPE_INT8);
    Send8(val);
    return 0;
}

int CharQueue::Get8()
{
    FnTrace("CharQueue::Get8()");
    int type = Read8();
    if (type != TYPE_INT8)
        ReadError(TYPE_INT8, type);
    int v = Read8();
    return v;
}

int CharQueue::Put16(int val)
{
    FnTrace("CharQueue::Put16()");
    Send8(TYPE_INT16);
    Send8(val);
    Send8(val >> 8);
    return 0;
}

int CharQueue::Get16()
{
    FnTrace("CharQueue::Get16()");
    int type = Read8();
    if (type != TYPE_INT16)
        ReadError(TYPE_INT16, type);
    int l = Read8();
    int h = Read8();

    int v = l + (h << 8);
    if (v >= 32768)
        v -= 65536;
    return v;
}

int CharQueue::Put32(int val)
{
    FnTrace("CharQueue::Put32()");
    Send8(TYPE_INT32);
    int send = Abs(val);
    Send8(send);
    Send8(send >> 8);
    Send8(send >> 16);

    int tmp = (send >> 24) & 127;
    if (val < 0)
        tmp |= 128;
    Send8(tmp);
    return 0;
}

int CharQueue::Get32()
{
    FnTrace("CharQueue::Get32()");
    int type = Read8();
    if (type != TYPE_INT32)
        ReadError(TYPE_INT32, type);
    int b1 = Read8();
    int b2 = Read8();
    int b3 = Read8();
    int b4 = Read8();
    int val = b1 + (b2 << 8) + (b3 << 16) + ((b4 & 127) << 24);
    if (b4 & 128)
        return -val;
    else
        return val;
}

long CharQueue::PutLong(long val)
{
    FnTrace("CharQueue::PutLong()");
    Send8(TYPE_LONG);
    int put = sizeof(long);    

    while (put)
    {
        Send8(val);
        val = val >> 8;
        put -= 1;
    }

    return 0;
}

long CharQueue::GetLong()
{
    FnTrace("CharQueue::GetLong()");
    int type = Read8();
    if (type != TYPE_LONG)
        ReadError(TYPE_LONG, type);
    int getbytes = sizeof(long);
    int currshift = 0;
    long retval = 0;
    int currbyte;

    while (getbytes > 0)
    {
        currbyte = Read8();
        retval += (currbyte << (currshift * 8));
        currshift += 1;
        getbytes -= 1;
    }

    return retval;
}

long long CharQueue::PutLLong(long long val)
{
    FnTrace("CharQueue::PutLLong()");
    Send8(TYPE_LLONG);
    int putbytes = sizeof(long long);

    while (putbytes > 0)
    {
        Send8(val);
        val       = val >> 8;
        putbytes -= 1;
    }

    return 0;
}

long long CharQueue::GetLLong()
{
    FnTrace("CharQueue::GetLLong()");
    int type = Read8();
    if (type != TYPE_LLONG)
        ReadError(TYPE_LLONG, type);
    int getbytes = sizeof(long long);
    int currshift = 0;
    long long retval = 0;
    int currbyte;

    while (getbytes > 0)
    {
        currbyte = Read8();
        retval += (currbyte << (currshift * 8));
        currshift += 1;
        getbytes -= 1;
    }

    return retval;
}

int CharQueue::PutString(char *str, int len)
{
    FnTrace("CharQueue::PutString()");
    Send8(TYPE_STRING);
    if (len <= 0)
        len = strlen(str);

    Put16(len);
    for (int i = 0; i < len; ++i)
    {
        Send8((int) *str);
        ++str;
    }
    return 0;
}

int CharQueue::GetString(char *str)
{
    FnTrace("CharQueue::GetString()");
    int type = Read8();
    if (type != TYPE_STRING)
        ReadError(TYPE_STRING, type);
    int len = Get16();
    int onechar = 0;

    for (int i = 0; i < len; ++i)
    {
        onechar = Read8();
        if (onechar < 0)
            return 1;
        else
            str[i] = (char) onechar;
    }
    str[len] = '\0';
    return 0;
}

int CharQueue::Read(int device_no)
{
    FnTrace("CharQueue::Read()");
    Clear();

    Uchar buf[5];
    int val = read(device_no, &buf[0], 1);
    if (val != 1)
        return -1;
    val = read(device_no, &buf[1], 1);
    val = read(device_no, &buf[2], 1);
    val = read(device_no, &buf[3], 1);
    int s = (Uint) buf[0] + ((Uint) buf[1] << 8) + ((Uint) buf[2] << 16) + ((Uint) buf[3] << 24);

    int todo = s;
    while (todo > 0)
    {
        val = read(device_no, &buffer[end], s);
        if (val > 0)
        {
            size += val;
            end  += val;
            todo -= val;
        }
    }
    return s;
}

/****
 * CharQueue::Write:  writes out the buffer to the device.
 *   Returns number of bytes written.
 ****/
int CharQueue::Write(int device_no, int do_clear)
{
    FnTrace("CharQueue::Write()");
    if (size <= 0)
        return 1;

    int val;
    Uchar buf1 = (Uchar) (size & 255);
    Uchar buf2 = (Uchar) ((size >> 8) & 255);
    Uchar buf3 = (Uchar) ((size >> 16) & 255);
    Uchar buf4 = (Uchar) ((size >> 24) & 255);
    write(device_no, &buf1, 1);
    write(device_no, &buf2, 1);
    write(device_no, &buf3, 1);
    write(device_no, &buf4, 1);

    if (start > end)
    {
        int s = size;
        Uchar str[1024];
        Uchar *c = str;
        while (size > 0)
            *c++ = (Uchar) Get8();
        *c = '\0';
        val = write(device_no, str, s);
    }
    else
    {
        val = write(device_no, &buffer[start], size);
    }

    if (do_clear)
        Clear();

    return val;
}
