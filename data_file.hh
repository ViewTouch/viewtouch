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
 * data_file.hh - revision 15 (8/25/98)
 * Functions for the reading & writing of data files
 */

#ifndef _DATA_FILE_HH
#define _DATA_FILE_HH

#include "utility.hh"
#include <zlib.h>

#define BLOCKSIZE   16384

/**** Types ****/

/*********************************************************************
 * InputDataFile:
 ********************************************************************/
class InputDataFile
{
    gzFile fp;
    int old_format;
    char filename[STRLONG];

public:
    int end_of_file;

    // Constructor
    InputDataFile();
    // Destructor
    ~InputDataFile() { Close(); }

    // Member Functions
    int Open(const char *filename, int &version);
    int Close();

    int   GetToken(const char *buffer, int max_len = 0);
    unsigned long long GetValue();

    int Read(char   &val) { val = (char)   GetValue(); return 0; }
    int Read(Uchar  &val) { val = (Uchar)  GetValue(); return 0; }
    int Read(short  &val) { val = (short)  GetValue(); return 0; }
    int Read(Ushort &val) { val = (Ushort) GetValue(); return 0; }
    int Read(int    &val) { val = (int)    GetValue(); return 0; }
    int Read(Uint   &val) { val = (Uint)   GetValue(); return 0; }
    int Read(long long &val) { val = GetValue(); return 0; }

    int Read(Flt &val);
    int Read(Str &val);
    int Read(TimeInfo &val);

    // conditional reads (won't read if pointer is NULL)
    int Read(int *val);
    int Read(Flt *val);
    int Read(Str *val);

    int PeekTokens();
    const char *ShowTokens(const char *buffer = NULL, int lines = 1);
    const char *FileName() { return filename; }
};


/*********************************************************************
 * OutputDataFile
 ********************************************************************/
class OutputDataFile
{
    void *fp;
    int compress;
    char filename[STRLONG];

public:
    // Constructor
    OutputDataFile();
    // Destructor
    ~OutputDataFile() { Close(); }

    // Member Functions
    int Open(const char *filename, int version, int use_compression = 0);
    int Close();

    int PutValue(unsigned long long val, int bk);

    int Write(short val, int bk = 0) { return PutValue((long long) val, bk); }
    int Write(int   val, int bk = 0) { return PutValue((long long) val, bk); }
    int Write(long long val, int bk = 0) { return PutValue(val, bk); }
    int Write(Str  &val, int bk = 0) { return Write(val.Value(), bk); }

    int Write(Flt       val, int bk = 0);
    int Write(TimeInfo &val, int bk = 0);

    // conditional writes (won't write if pointer is NULL)
    int Write(int  *val, int bk = 0);
    int Write(const char *val, int bk = 0);
    int Write(Flt  *val, int bk = 0);
    int Write(Str  *val, int bk = 0);
    const char *FileName() { return filename; }
};


/*********************************************************************
 * KeyValueInputFile:
 ********************************************************************/
class KeyValueInputFile
{
private:
    int filedes;
    int bytesread;
    int keyidx;
    int validx;
    int buffidx;
    int comment;
    int getvalue;
    char delimiter;
    char buffer[BLOCKSIZE];
    char inputfile[STRLONG];

public:
    KeyValueInputFile();
    KeyValueInputFile(int fd);
    KeyValueInputFile(const char *filename);
    int Open();
    int Open(const char *filename);
    int IsOpen();
    int Set(int fd);
    int Set(const char *filename);
    char SetDelim(char delim);
    int Close();
    int Reset();
    int Read(const char *key, const char *value, int maxlen);
    
};

/*********************************************************************
 * KeyValueOutputFile:
 ********************************************************************/
class KeyValueOutputFile
{
private:
    int filedes;
    char delimiter;
    char outputfile[STRLONG];

public:
    KeyValueOutputFile();
    KeyValueOutputFile(int fd);
    KeyValueOutputFile(const char *filename);
    int Open();
    int Open(const char *filename);
    int IsOpen();
    char SetDelim(char delim);
    int Close();
    int Reset();
    int Write(const char *key, const char *value);
};

#endif
