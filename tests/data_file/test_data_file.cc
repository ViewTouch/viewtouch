#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "data_file.hh"

#include <string>
#include <vector>
#include <fstream>
#include <limits> // std::numeric_limits
#include <cmath> // std::isnan, std::isinf
#include <locale>

template<typename T>
void write_and_read(T val, const std::string &filename="test.vtdata")
{
    {
        OutputDataFile odf;
        odf.Open(filename, 1, false);
        odf.Write(val);
    }
    {
        InputDataFile idf;
        int version;
        idf.Open(filename, version);
        T reread;
        idf.Read(reread);
        CHECK(reread == val);
    }

}
TEST_CASE("odf")
{
    write_and_read(Str("char"));
    write_and_read(Str(""));

    write_and_read<int>(-1);
    write_and_read<int>(1);
    write_and_read<size_t>(1337);
    //write_and_read(1l);
    write_and_read<Flt>(1.0f);      // REFACTOR: Changed from double to Flt for compatibility
    write_and_read<Flt>(1337.73f);  // REFACTOR: Changed from double to Flt for compatibility
    //write_and_read<float>(1.0);


    TimeInfo ti;
    ti.Set(0, 2018);
    write_and_read(ti);
}

TEST_CASE("timedate")
{
    TimeInfo ti;
    ti.Set(500, 2018);
    write_and_read(ti);
}

TEST_CASE("timedate default read/write")
{
    TimeInfo ti;
    const std::string filename = "test_data_file_timedate_default.vtdata";
    {
        OutputDataFile odf;
        odf.Open(filename, 1, false);
        odf.Write(ti);
    }
    {
        InputDataFile idf;
        int version;
        idf.Open(filename, version);
        TimeInfo reread;
        idf.Read(reread);
        CHECK(!ti.IsSet());
        CHECK(!reread.IsSet());
    }
}
