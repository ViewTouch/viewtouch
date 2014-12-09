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
 * data_file.cc - revision 26 (9/8/98)
 * Implementation of data_file module
 */

#include "data_file.hh"
#include "utility.hh"
#include <iostream>
#include <zlib.h>
#include <cctype>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/**** Module Data ****/
static int  OldDecodeDigit[256];
static char OldEncodeDigit[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$%^&*(),./;'[]-=\\<>?:\"{}_+|";
#define BASE (sizeof(OldEncodeDigit)-1)

// BASE64 encoding (RFC 1521)
static int  IsDecodeReady = 0;
static int  NewDecodeDigit[256];
static char NewEncodeDigit[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**** InputDataFile Class ****/
// Constructor
InputDataFile::InputDataFile()
{
    fp          = NULL;
    end_of_file = 0;
    old_format  = 0;
    filename[0] = '\0';
}

// Member Functions
int InputDataFile::Open(const char* name, int &version)
{
	FnTrace("InputDataFile::Open()");
    int i;
    char errmsg[STRLENGTH];

	if (IsDecodeReady == 0)
	{
		// Initialize OldDecodeDigit[] first time a file is opened
		IsDecodeReady = 1;
		for (i = 0; i < 256; ++i)
		{
			OldDecodeDigit[i] = 0;
			NewDecodeDigit[i] = 0;
		}
		for (i = 0; i < (int) BASE; ++i)
			OldDecodeDigit[(int) OldEncodeDigit[i]] = i;
		for (i = 0; i < 64; ++i)
			NewDecodeDigit[(int) NewEncodeDigit[i]] = i;
	}

	if (fp)
		gzclose((gzFile)fp);

	version = 0;
	fp = gzopen(name, "r");
	if (fp == NULL)
    {
        snprintf(errmsg, STRLENGTH, "Unable to read %s:  %d", name, errno);
        ReportError(errmsg);
		return 1;
    }

	// Read version string
	char str[256];
	GetToken(str, sizeof(str));
	if (strncmp(str, "version_", 8) == 0)
	{
		// old file version
		version = atoi(str+8);
		old_format = 1;
	}
	else if (strncmp(str, "vtpos", 5) == 0)
	{
		GetToken(str, sizeof(str));  // file type - ignored for now
		GetToken(str, sizeof(str));  // version
		version = atoi(str);
		old_format = 0;
	}
	else
	{
		gzclose((gzFile)fp);
        fp = NULL;
        snprintf(errmsg, STRLENGTH, "Unknown file format for %s\n", name);
        ReportError(errmsg);
		return 1;
	}
    strcpy(filename, name);

	return 0;
}

int InputDataFile::Close()
{
    FnTrace("InputDataFile::Close()");
    if (fp)
    {
        gzclose((gzFile)fp);
        fp = NULL;
    }
    return 0;
}

int InputDataFile::GetToken(char* buffer, int max_len)
{
    FnTrace("InputDataFile::GetToken()");
    int c;
    int idx = 0;

    c = gzgetc((gzFile)fp);
    while (c >= 0 && isspace(c))
        c = gzgetc((gzFile)fp);

    for (;;)
    {
        if (c < 0)
        {
            end_of_file = 1;
            return (idx == 0);  // error if we haven't read anything
        }
        else if (isspace(c))
        {
            buffer[idx] = '\0';
            return 0;  // we've read an entire token
        }
        else
        {
            buffer[idx] = (char) c;
            idx += 1;
            if (idx >= max_len)
            {
                buffer[max_len - 1] = '\0';
                return 1;
            }
        }
        c = gzgetc((gzFile)fp);
    }
 
    return 1;  // should never get this far
}

unsigned long long InputDataFile::GetValue()
{
    FnTrace("InputDataFile::GetValue()");
    long long val = 0;
    int c;
    if (old_format)
    {
        for (;;)
        {
            c = gzgetc((gzFile)fp);
            if (c < 0)
            {
                end_of_file = 1;
                return 0;
            }
            else if (isspace(c))
                return val;
            else
                val = (val * BASE) + OldDecodeDigit[c];
        }
    }
    else
    {
        c = gzgetc((gzFile)fp);
        while (isspace(c))
            c = gzgetc((gzFile)fp);
        for (;;)
        {
            if (c < 0)
            {
                end_of_file = 1;
                return 0;
            }
            else if (isspace(c))
                return val;
            else
                val = (val << 6) + NewDecodeDigit[c];
            c = gzgetc((gzFile)fp);
        }
    }
}

int InputDataFile::Read(Flt &val)
{
    FnTrace("InputDataFile::Read(Flt &)");
    char str[256];
    if (GetToken(str, sizeof(str)))
        return 1;

    Flt v = 0.0;
    if (sscanf(str, "%lf", &v) != 1)
        return 1;

    val = v;
    return 0;
}

int InputDataFile::Read(Str &s)
{
    FnTrace("InputDataFile::Read(Str &)");
    char str[1024];
    if (GetToken(str, sizeof(str)))
        return 1;

    if (strlen(str) <= 0 || strcmp(str, "~") == 0)
        s.Clear();
    else
    {
        s.Set(str);
        s.ChangeAtoB('_', ' ');
    }
    return 0;
}

int InputDataFile::Read(TimeInfo &timevar)
{
    FnTrace("InputDataFile::Read(TimeInfo &)");
    int s = (int) GetValue();
    int y = (int) GetValue();
    if (y > 0)
    {
        int d = ((s / 86400)   % 31) + 1;
        int m = ((s / 2678400) % 12) + 1;
        timevar.Sec(s % 60);
        timevar.Min((s / 60) % 60);
        timevar.Hour((s / 3600) % 24);
        timevar.Day(d);
        timevar.Month(m);
        timevar.WeekDay(DayOfTheWeek(d, m, y));
        timevar.Year(y);
    }

    return 0;
}

int InputDataFile::Read(int *val)
{
    FnTrace("InputDataFile::Read(int *)");
    if (val == NULL)
        return 0;
    return Read(*val);
}

int InputDataFile::Read(Flt *val)
{
    FnTrace("InputDataFile::Read(Flt *)");
    if (val == NULL)
        return 0;
    return Read(*val);
}

int InputDataFile::Read(Str *val)
{
    FnTrace("InputDataFile::Read(Str *)");
    if (val == NULL)
        return 0;
    return Read(*val);
}

/****
 * PeekTokens:  This function peeks ahead into the file counting the number
 *  of tokens until the next newline.
 ****/
int InputDataFile::PeekTokens()
{
    FnTrace("InputDataFile::PeekTokens()");
    unsigned long savepos = gzseek((gzFile)fp, 0L, SEEK_CUR);
    int count = 0;
    int havenl = 0;
    int started = 0;  // make sure we pass any preceding whitespace
    char peekchar = '\0';

    // count the tokens until newline
    while (havenl == 0 && end_of_file == 0)
    {
        peekchar = gzgetc((gzFile)fp);
        if (peekchar < 0)
            end_of_file = 1;
        else if (isspace(peekchar))
        {
            if (started)
            {
                count += 1;
                if (peekchar == '\n')
                    havenl = 1;
            }
            started = 1;
        }
    }
    // restore the original position
    gzseek((gzFile)fp, savepos, SEEK_SET);
    return count;
}

/****
 * ShowTokens:  Another debug method.  Just peeks ahead to gather the remaining tokens
 *  on the current line and returns a string containing those tokens.
 ****/
const char* InputDataFile::ShowTokens(char* buffer, int lines)
{
    FnTrace("InputDataFile::ShowTokens()");
    unsigned long savepos = gzseek((gzFile)fp, 0L, SEEK_CUR);
    static char buff2[STRLONG];
    int   buffidx = 0;
    char  peekchar = '\0';

    if (buffer == NULL)
        buffer = buff2;

    while (lines > 0)
    {
        peekchar = gzgetc((gzFile)fp);
        while (peekchar != '\n' && end_of_file == 0)
        {
            buffer[buffidx] = peekchar;
            buffidx += 1;
            peekchar = gzgetc((gzFile)fp);
        }
        if (lines > 1)
            buffer[buffidx] = '\n';
        lines -= 1;
    }
    buffer[buffidx] = '\0';

    gzseek((gzFile)fp, savepos, SEEK_SET);

    return buffer;
}

/**** OutputDataFile Class ****/
// Constructor
OutputDataFile::OutputDataFile()
{
    FnTrace("OutputDataFile::OutputDataFile()");
    fp       = NULL;
    compress = 0;
    filename[0] = '\0';
}

// Member Functions
int OutputDataFile::Open(const char* filepath, int version, int use_compression)
{
    FnTrace("OutputDataFile::Open()");
    char errmsg[STRLENGTH];

    if (filepath == NULL || strlen(filepath) <= 0)
        return 1;

    strcpy(filename, filepath);
    compress = use_compression;

    if (compress)
        fp = (void*)gzopen(filepath, "w");
    else
        fp = fopen(filepath, "w");
    if (fp == NULL)
    {
        snprintf(errmsg, STRLENGTH, "OutputDataFile::Open error %d opening '%s'",
                 errno, filepath);
        ReportError(errmsg);
        return 1;
    }

    // write version string
    char str[32];
    sprintf(str, "vtpos 0 %d\n", version);
    if (compress)
        gzwrite((gzFile)fp, str, strlen(str));
    else
        fwrite(str, 1, strlen(str), (FILE *) fp);
    return 0;
}

int OutputDataFile::Close()
{
    FnTrace("OutputDataFile::Close()");
    if (fp)
    {
        if (compress)
        {
            gzclose((gzFile)fp);
        }
        else
        {
            fclose((FILE *) fp);
        }
        fp = NULL;
    }
    return 0;
}

int OutputDataFile::PutValue(unsigned long long val, int bk = 0)
{
    FnTrace("OutputDataFile::PutValue()");
    char  str[32];
    char* c = &str[31];
    *c = '\0';
    --c;
    if (bk)
        *c = '\n';
    else
        *c = ' ';

    do
    {
        --c;
        *c = NewEncodeDigit[(int) (val & 63)];
        val >>= 6;
    }
    while (val > 0);

    if (compress)
        gzwrite((gzFile)fp, c, strlen(c));
    else
        fwrite(c, 1, strlen(c), (FILE *) fp);
    return 0;
}

int OutputDataFile::Write(Flt val, int bk)
{
    FnTrace("OutputDataFile::Write(Flt)");
    if (compress)
    {
        if (bk)
            gzprintf((gzFile)fp, "%g\n", val);
        else
            gzprintf((gzFile)fp, "%g ", val);
    }
    else if (bk)
        fprintf((FILE *) fp, "%g\n", val);
    else
        fprintf((FILE *) fp, "%g ", val);
    return 0;
}

int OutputDataFile::Write(const char* val, int bk)
{
    FnTrace("OutputDataFile::Write(const char* )");
    if (val == NULL)
        return 1;

    int length = strlen(val);
    if (length <= 0)
    {
        if (compress)
        {
            if (bk)
                gzwrite((gzFile)fp, (void *)"~\n", 2);
            else
                gzwrite((gzFile)fp, (void *)"~ ", 2);
        }
        else if (bk)
            fwrite("~\n", 1, 2, (FILE *) fp);
        else
            fwrite("~ ", 1, 2, (FILE *) fp);
    }
    else
    {
        int out;
        const char* c = val;
        while (*c)
        {
            out = *c;
            if (out == '~' || out == ' ')
                out = '_';
            if (compress)
                gzputc((gzFile)fp, out);
            else
                fputc(out, (FILE *) fp);
            ++c;
        }
        if (bk)
            out = '\n';
        else
            out = ' ';
        if (compress)
            gzputc((gzFile)fp, out);
        else
            fputc(out, (FILE *) fp);
    }
    return 0;
}

/****
 * Write TimeInfo:  For "self-documentation" in debug mode, we'll break
 *  the format into timevar(int int), except we won't prevent Write(int) from
 *  writing it's space, which leaves us with timevar(int, int ).  Then we'll
 *  write a newline if specified.
 ****/
int OutputDataFile::Write(TimeInfo &timevar, int bk)
{
    FnTrace("OutputDataFile::Write(TimeInfo &)");
    int s = timevar.Sec() + (timevar.Min() * 60) + (timevar.Hour() * 3600) +
        ((timevar.Day() - 1) * 86400) + ((timevar.Month() - 1) * 2678400);

    int error = 0;

    error += Write(s);
    error += Write(timevar.Year());
    if (bk)
    {
        if (compress)
            gzputc((gzFile)fp, '\n');
        else
            fputc('\n', (FILE *) fp);
    }
    return error;
}

int OutputDataFile::Write(int *val, int bk)
{
    FnTrace("OutputDataFile::Write(int *)");
    if (val == NULL)
        return 0;
    return Write(*val, bk);
}

int OutputDataFile::Write(Flt *val, int bk)
{
    FnTrace("OutputDataFile::Write(Flt *)");
    if (val == NULL)
        return 0;
    return Write(*val, bk);
}

int OutputDataFile::Write(Str *val, int bk)
{
    FnTrace("OutputDataFile::Write(Str *)");
    if (val == NULL)
        return 0;
    return Write(*val, bk);
}

/*********************************************************************
 * KeyValueInputFile:  A text file containing key/value pairs.  There
 *  can be comments on any line, starting with # and continuing to the
 *  end of the line. Key/value pairs are separated by :.  As
# this is the comment line
key1:  value1  # this is the first key/value pair
key2:  value2 has multiple words separated by whitespace
keyn:  valuen  # this is the last key/value pair
 *  There can be any amount of whitespace at the beginning of the line,
 *  at the end of the line, and after the colon and it will all be
 *  stripped.  The value is all characters starting with the first
 *  non-whitespace character after the colon through the last
 *  non-whitespace character before the end of the line.  There can
 *  be any amount of whitespace between the two non-whitespace
 *  characters (as value2 above, the value of which is
 *  "value2 has multiple words separated by whitespace").
 ********************************************************************/

KeyValueInputFile::KeyValueInputFile()
{
    FnTrace("KeyValueInputFile::KeyValueInputFile()");
    filedes = -1;
    bytesread = 0;
    keyidx = 0;
    validx = 0;
    buffidx = 0;
    comment = 0;
    getvalue = 0;
    delimiter = ':';
    buffer[0] = '\0';
    inputfile[0] = '\0';
}

KeyValueInputFile::KeyValueInputFile(int fd)
{
    FnTrace("KeyValueInputFile::KeyValueInputFile(int)");
    filedes = fd;
    bytesread = 0;
    keyidx = 0;
    validx = 0;
    buffidx = 0;
    comment = 0;
    getvalue = 0;
    delimiter = ':';
    buffer[0] = '\0';
    inputfile[0] = '\0';
}

KeyValueInputFile::KeyValueInputFile(const char* filename)
{
    FnTrace("KeyValueInputFile::KeyValueInputFile(const char* )");
    filedes = -1;
    bytesread = 0;
    keyidx = 0;
    validx = 0;
    buffidx = 0;
    comment = 0;
    getvalue = 0;
    delimiter = ':';
    buffer[0] = '\0';
    strcpy(inputfile, filename);
}

int KeyValueInputFile::Open(void)
{
    FnTrace("KeyValueInputFile::Open()");
    int retval = 0;
    char errmsg[STRLENGTH];

    if (strlen(inputfile) > 0)
    {
        filedes = open(inputfile, O_RDONLY);
        if (filedes > 0)
            retval = 1;
        else if (filedes < 0)
        {
            snprintf(errmsg, STRLENGTH, "KeyValueInputFile::Open error %d for %s",
                     errno, inputfile);
            ReportError(errmsg);
        }
    }

    return retval;
}

int KeyValueInputFile::Open(const char* filename)
{
    FnTrace("KeyValueInputFile::Open(const char* )");

    strcpy(inputfile, filename);
    return Open();
}

int KeyValueInputFile::IsOpen()
{
    FnTrace("KeyValueInputFile::IsOpen()");
    return (filedes > 0);
}

int KeyValueInputFile::Set(int fd)
{
    FnTrace("KeyValueInputFile::Set(int)");
    filedes = fd;

    return 0;
}

int KeyValueInputFile::Set(const char* filename)
{
    FnTrace("KeyValueInputFile::Set(const char* )");

    strcpy(inputfile, filename);
    return 0;
}

char KeyValueInputFile::SetDelim(char delim)
{
    FnTrace("KeyValueInputFile::SetDelim()");
    char retval = delimiter;
    
    delimiter = delim;
    return retval;
}

int KeyValueInputFile::Close()
{
    FnTrace("KeyValueInputFile::Close()");
    int retval = close(filedes);
    filedes = -1;

    return retval;
}

/****
 * Reset:  Close the file, if it's open, and reset everything back
 *  to zero.  This allows one object to be reused for different
 *  files, for example.
 *  Returns 1 if a file was closed, 0 otherwise (arbitrary, semi-useless
 *  return value).
 ****/
int KeyValueInputFile::Reset()
{
    FnTrace("KeyValueInputFile::Reset()");
    int retval = 0;
    if (filedes > 0)
    {
        close(filedes);
        retval = 1;
    }

    filedes = -1;
    bytesread = 0;
    keyidx = 0;
    validx = 0;
    buffidx = 0;
    comment = 0;
    getvalue = 0;
    buffer[0] = '\0';
    inputfile[0] = '\0';

    return retval;
}

/****
 * Read:  For an open file, reads lines such as:
 *   # comment
 *   key:  value
 *
 *   key2:  value2
 *  The maxlen parameter should be the maximum string length. It is applied to
 *  both key and value, so if one is shorter than the other, maxlen should
 *  specify the shortest length.
 *  Returns 1 if a key/value pair was read (even if they're both 0 bytes;
 *  it simply means there may be more lines to read), 0 otherwise.
 ****/
int KeyValueInputFile::Read(char* key, char* value, int maxlen)
{
    FnTrace("KeyValueInputFile::Read()");
    int retval = 0;
    char last  = '\0';

    key[0]     = '\0';
    value[0]   = '\0';

    if (bytesread == 0)
        bytesread = read(filedes, buffer, BLOCKSIZE);
    while (bytesread > 0 && retval == 0)
    {
        while (buffidx < bytesread && retval == 0)
        {
            if (buffer[buffidx] == '\n')
            {
                if (keyidx)
                {
                    key[keyidx] = '\0';
                    value[validx] = '\0';
                    StripWhiteSpace(key);
                    StripWhiteSpace(value);
                }
                keyidx = 0;
                validx = 0;
                comment = 0;
                getvalue = 0;
                retval = 1;
            }
            else if (buffer[buffidx] == '#' && last != '\\')
            {  // start a comment
                comment = 1;
            }
            else if (comment || buffer[buffidx] == '\\')
            {  // NULL block: skip comments
            }
            else if (getvalue && (validx < maxlen))
            {
                value[validx] = buffer[buffidx];
                validx += 1;
            }
            else if (buffer[buffidx] == ':')
            {  // end of key, get value
                getvalue = 1;
            }
            else if (keyidx < maxlen)
            {
                key[keyidx] = buffer[buffidx];
                keyidx += 1;
            }
            last = buffer[buffidx];
            buffidx += 1;
        }
        if (retval == 0)
        {
            bytesread = read(filedes, buffer, BLOCKSIZE);
            buffidx = 0;
        }
    }
    if (retval == 0 && keyidx > 0)
    {
        key[keyidx] = '\0';
        value[validx] = '\0';
        StripWhiteSpace(key);
        StripWhiteSpace(value);
        keyidx = 0;
        validx = 0;
        retval = 1;
    }
    return retval;
}


/*********************************************************************
 * KeyValueOutputFile class
 ********************************************************************/

KeyValueOutputFile::KeyValueOutputFile()
{
    FnTrace("KeyValueOutputFile::KeyValueOutputFile()");
    filedes = -1;
    delimiter = ':';
    outputfile[0] = '\0';
}

KeyValueOutputFile::KeyValueOutputFile(int fd)
{
    FnTrace("KeyValueOutputFile::KeyValueOutputFile(int)");
    filedes = fd;
    delimiter = ':';
    outputfile[0] = '\0';
}

KeyValueOutputFile::KeyValueOutputFile(const char* filename)
{
    FnTrace("KeyValueOutputFile::KeyValueOutputFile(const char* )");
    filedes = -1;
    delimiter = ':';
    strcpy(outputfile, filename);
}
   
int KeyValueOutputFile::Open()
{
    FnTrace("KeyValueOutputFile::Open()");
    int retval = 0;
    char errmsg[STRLENGTH];

    if (strlen(outputfile) > 0)
    {
        int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
        filedes = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, mode);
        if (filedes > 0)
        {
            retval = 1;
        }
        else if (filedes < 0)
        {
            snprintf(errmsg, STRLENGTH, "KeyValueOutputFile::Open error %d for %s",
                     errno, outputfile);
            ReportError(errmsg);
        }
    }

    return retval;
}

int KeyValueOutputFile::Open(const char* filename)
{
    FnTrace("KeyValueOutputFile::Open(const char* )");
    int retval = 1;

    strcpy(outputfile, filename);
    retval = Open();

    return retval;
}

int KeyValueOutputFile::IsOpen()
{
    FnTrace("KeyValueOutputFile::IsOpen()");
    return (filedes > 0);
}

char KeyValueOutputFile::SetDelim(char delim)
{
    FnTrace("KeyValueOutputFile::SetDelim()");
    char retval = delimiter;

    delimiter = delim;

    return retval;
}

int KeyValueOutputFile::Close()
{
    FnTrace("KeyValueOutputFile::Close()");
    int retval = close(filedes);
    filedes = -1;

    return retval;
}

int KeyValueOutputFile::Reset()
{
    FnTrace("KeyValueOutputFile::Reset()");
    int retval = 0;
    if (filedes > 0)
    {
        close(filedes);
        retval = 1;
    }

    filedes = -1;
    outputfile[0] = '\0';

    return retval;
}

int KeyValueOutputFile::Write(const char* key, const char* value)
{
    FnTrace("KeyValueOutputFile::Write()");
    int retval = 1;
    char buffer[STRLONG];

    snprintf(buffer, STRLONG, "%s: %s\n", key, value);
    retval = write(filedes, buffer, strlen(buffer));

    return retval;
}
