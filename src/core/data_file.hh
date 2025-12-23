
/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998, 2025

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

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

inline constexpr std::size_t DataFileBlockSize = 16384;

/*********************************************************************
 * InputDataFile
 ********************************************************************/
class InputDataFile
{
    gzFile fp{nullptr};
    bool old_format{false};
    std::string filename;

public:
    bool end_of_file{false};

    InputDataFile() = default;
    ~InputDataFile();

    [[nodiscard]] bool is_open() const noexcept { return fp != nullptr; }

    int Open(const std::string &filename, int &version);
    int Close() noexcept;

    int GetToken(char* buffer, int max_len);
    [[nodiscard]] uint64_t GetValue();

    // using the following conversions
    // char      ... 1 byte
    // short     ... 2 byte
    // int       ... 4 byte
    // long      ... 8 byte
    // long long ... 8 byte
    int Read( int8_t  &val) { val = static_cast< int8_t >(GetValue()); return 0; }
    int Read(uint8_t  &val) { val = static_cast<uint8_t >(GetValue()); return 0; }
    int Read( int16_t &val) { val = static_cast< int16_t>(GetValue()); return 0; }
    int Read(uint16_t &val) { val = static_cast<uint16_t>(GetValue()); return 0; }
    int Read( int32_t &val) { val = static_cast< int32_t>(GetValue()); return 0; }
    int Read(uint32_t &val) { val = static_cast<uint32_t>(GetValue()); return 0; }
    int Read( int64_t &val) { val = static_cast< int64_t>(GetValue()); return 0; }
    int Read(uint64_t &val) { val = static_cast<uint64_t>(GetValue()); return 0; }

    int Read(Flt &val);
    int Read(Str &val);
    int Read(TimeInfo &val);

    // conditional reads (won't read if pointer is NULL)
    int Read(int *val);
    int Read(Flt *val);
    int Read(Str *val);

    int PeekTokens();
    const char* ShowTokens(char* buffer = nullptr, int lines = 1);
    [[nodiscard]] const std::string &FileName() const noexcept { return filename; }
};

/*********************************************************************
 * OutputDataFile
 ********************************************************************/
class OutputDataFile
{
    gzFile gz_fp{nullptr};
    std::FILE* file_fp{nullptr};
    bool compress{false};
    std::string filename;

public:
    OutputDataFile() = default;
    ~OutputDataFile();

    int Open(const std::string &filename, int version, int use_compression = 0);
    int Close() noexcept;

    int PutValue(uint64_t val, int bk);

    // using the following conversions
    // char      ... 1 byte
    // short     ... 2 byte
    // int       ... 4 byte
    // long      ... 8 byte
    // long long ... 8 byte
    int Write( int8_t val, int bk = 0) { return PutValue(static_cast<uint64_t>(val), bk); }
    int Write(uint8_t val, int bk = 0) { return PutValue(static_cast<uint64_t>(val), bk); }
    int Write( int16_t val, int bk = 0) { return PutValue(static_cast<uint64_t>(val), bk); }
    int Write(uint16_t val, int bk = 0) { return PutValue(static_cast<uint64_t>(val), bk); }
    int Write( int32_t val, int bk = 0) { return PutValue(static_cast<uint64_t>(val), bk); }
    int Write(uint32_t val, int bk = 0) { return PutValue(static_cast<uint64_t>(val), bk); }
    int Write( int64_t val, int bk = 0) { return PutValue(static_cast<uint64_t>(val), bk); }
    int Write(uint64_t val, int bk = 0) { return PutValue(static_cast<uint64_t>(val), bk); }

    int Write(Str  &val, int bk = 0) { return Write(val.Value(), bk); }

    int Write(Flt       val, int bk = 0);
    int Write(TimeInfo &val, int bk = 0);

    // conditional writes (won't write if pointer is NULL)
    int Write(int  *val, int bk = 0);
    int Write(const char* val, int bk = 0);
    int Write(Flt  *val, int bk = 0);
    int Write(Str  *val, int bk = 0);
    [[nodiscard]] const std::string &FileName() const noexcept { return filename; }
};

/*********************************************************************
 * KeyValueInputFile
 ********************************************************************/
class KeyValueInputFile
{
private:
    int filedes{-1};
    std::size_t bytesread{0};
    std::size_t keyidx{0};
    std::size_t validx{0};
    std::size_t buffidx{0};
    bool comment{false};
    bool getvalue{false};
    char delimiter{':'};
    std::array<char, DataFileBlockSize> buffer{};
    std::string inputfile;

public:
    KeyValueInputFile() = default;
    explicit KeyValueInputFile(int fd);
    explicit KeyValueInputFile(const std::string &filename);
    bool Open();
    bool Open(const std::string &filename);
    [[nodiscard]] bool IsOpen() const noexcept;
    int Set(int fd);
    int Set(const std::string &filename);
    char SetDelim(char delim);
    int Close();
    int Reset();
    int Read(char* key, char* value, int maxlen);
};

/*********************************************************************
 * KeyValueOutputFile
 ********************************************************************/
class KeyValueOutputFile
{
private:
    int filedes{-1};
    char delimiter{':'};
    std::string outputfile;

public:
    KeyValueOutputFile() = default;
    explicit KeyValueOutputFile(int fd);
    explicit KeyValueOutputFile(const std::string &filename);
    int Open();
    int Open(const std::string &filename);
    [[nodiscard]] bool IsOpen() const noexcept;
    char SetDelim(char delim);
    int Close();
    int Reset();
    int Write(const std::string &key, const std::string &value);
};

#endif
