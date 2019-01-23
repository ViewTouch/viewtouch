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
#include <string>

#define BLOCKSIZE   16384

/**** Types ****/

/*********************************************************************
 * InputDataFile:
 ********************************************************************/
class InputDataFile
{
    gzFile fp;
    int old_format;
    std::string filename;

public:
    int end_of_file;

    // Constructor
    InputDataFile();
    // Destructor
    ~InputDataFile() { Close(); }

    // Member Functions
    int Open(const std::string &filename, int &version);
    int Close();

    int   GetToken(char* buffer, int max_len = 0);
    unsigned long long GetValue();

    int Read(char   &val) { val = (char)   GetValue(); return 0; }
    int Read(Uchar  &val) { val = (Uchar)  GetValue(); return 0; }
    int Read(short  &val) { val = (short)  GetValue(); return 0; }
    int Read(Ushort &val) { val = (Ushort) GetValue(); return 0; }
    int Read(int    &val) { val = (int)    GetValue(); return 0; }
    int Read(Uint   &val) { val = (Uint)   GetValue(); return 0; }
    int Read(long long &val) { val = GetValue(); return 0; }
    int Read(size_t &val) { val = static_cast<size_t>(GetValue()); return 0; }

    int Read(Flt &val);
    int Read(Str &val);
    int Read(TimeInfo &val);

    // conditional reads (won't read if pointer is NULL)
    int Read(int *val);
    int Read(Flt *val);
    int Read(Str *val);

    int PeekTokens();
    const char* ShowTokens(char* buffer = NULL, int lines = 1);
    const std::string &FileName() { return filename; }
};


/*********************************************************************
 * OutputDataFile
 ********************************************************************/
class OutputDataFile
{
    void *fp = nullptr;
    int compress = 0;
    std::string filename;

public:
    // Constructor
    OutputDataFile();
    // Destructor
    ~OutputDataFile() { Close(); }

    // Member Functions
    int Open(const std::string &filename, int version, int use_compression = 0);
    int Close();

    int PutValue(unsigned long long val, int bk);

    int Write(short val, int bk = 0) { return PutValue((unsigned long long) val, bk); }
    int Write(int   val, int bk = 0) { return PutValue((unsigned long long) val, bk); }
    int Write(long long val, int bk = 0) { return PutValue(val, bk); }
    int Write(size_t val, int bk = 0) { return PutValue(val, bk); }
    int Write(Str  &val, int bk = 0) { return Write(val.Value(), bk); }

    int Write(Flt       val, int bk = 0);
    int Write(TimeInfo &val, int bk = 0);

    // conditional writes (won't write if pointer is NULL)
    int Write(int  *val, int bk = 0);
    int Write(const char* val, int bk = 0);
    int Write(Flt  *val, int bk = 0);
    int Write(Str  *val, int bk = 0);
    std::string FileName() { return filename; }
};


/*********************************************************************
 * KeyValueInputFile:
 ********************************************************************/
class KeyValueInputFile
{
private:
    int filedes = -1;
    int bytesread = 0;
    int keyidx = 0;
    int validx = 0;
    int buffidx = 0;
    int comment = 0;
    int getvalue = 0;
    char delimiter = ':';
    char buffer[BLOCKSIZE] = {'\0'};
    std::string inputfile;

public:
    KeyValueInputFile();
    KeyValueInputFile(const int fd);
    KeyValueInputFile(const std::string &filename);
    bool Open();
    bool Open(const std::string &filename);
    bool IsOpen();
    int Set(const int fd);
    int Set(const std::string &filename);
    char SetDelim(const char delim);
    int Close();
    int Reset();
    int Read(char* key, char* value, int maxlen);
    
};

/*********************************************************************
 * KeyValueOutputFile:
 ********************************************************************/
class KeyValueOutputFile
{
private:
    int filedes = -1;
    char delimiter = ':';
    std::string outputfile;

public:
    KeyValueOutputFile();
    KeyValueOutputFile(const int fd);
    KeyValueOutputFile(const std::string &filename);
    int Open();
    int Open(const std::string &filename);
    int IsOpen();
    char SetDelim(const char delim);
    int Close();
    int Reset();
    int Write(const std::string &key, const std::string &value);
};

#endif
