#include "catch2/catch.hpp"
#include "labor.hh"
#include "employee.hh"
#include "terminal.hh"
#include "system.hh"
#include "data_file.hh"

#include <string>
#include <vector>
#include <fstream>
#include <limits> // std::numeric_limits
#include <cmath> // std::isnan, std::isinf
#include <locale>

template<typename T>
T write_and_read(T val, int version, const std::string &filename="test.vtdata")
{
    {
        OutputDataFile odf;
        odf.Open(filename, version, false);
        val.Write(odf, version);
    }
    T reread;
    {
        InputDataFile idf;
        idf.Open(filename, version);
        reread.Read(idf, version);
    }
    return reread;
}

TEST_CASE("labor: WorkEntry: default constructor does not initialize start/end")
{
    WorkEntry we;
    CHECK(we.start.IsSet() == false);
    CHECK(we.end.IsSet() == false);
}

TEST_CASE("labor: WorkEntry: read/write default entry, still uninitialized start/end")
{
    WorkEntry we;
    SECTION("version WORK_VERSION = 4")
    {
        const std::string filename = "test_labor_WorkEntry_default_read_write_ver4.vtdata";
        const int version = 4;
        WorkEntry reread = write_and_read(we, version, filename);
        CHECK(!reread.start.IsSet());
        CHECK(!reread.end.IsSet());
    }
}

TEST_CASE("labor: WorkDB: read/write default db is still empty")
{
    WorkDB wdb;
    SECTION("WORK_VERSION = 4")
    {
        const std::string filename = "test_labor_WorkDB_default_read_write_ver4.vtdata";
        const int version = 4;
        WorkDB reread = write_and_read(wdb, version, filename);
        CHECK(reread.WorkCount() == 0);
    }
}
TEST_CASE("labor: LaborPeriod: read/write default db is still empty")
{
    const std::string filename = "test_labor_LaborPeriod_default_save_load.vtdata";
    LaborPeriod lp;
    lp.file_name.Set(filename);
    {
        lp.loaded = 1;
        lp.Save();
    }

    LaborPeriod reread;
    reread.Scan(filename.c_str());
    reread.Load();

    CHECK(reread.loaded == 1);
    CHECK(reread.WorkCount() == 0);
}

// Regression Test for Issue: https://github.com/ViewTouch/viewtouch/issues/108
// Title: Users have to once again Clock In when ViewTouch exits and is restarted
// The bug was caused by the unhandled case of an uninitialized TimeInfo written
// to a file and read back. The read back version was initialized to 0 sec in year
// 1970. The LaborDB::IsUserOnClock() checks the end TimeInfo to be uninizialized.
// With this bug the employee is never clocked in after a restart of viewtouch.
TEST_CASE("labor: LaborPeriod: employee logged in must be still logged in after load")
{
    const std::string filename = "test_labor_LaborPeriod_still_logged_in_after_load.vtdata";
    Employee e;
    e.id = 10; // normal id, no superuser, needs to clock in
    REQUIRE(e.UseClock());
    {
        LaborDB db;
        REQUIRE(db.IsUserOnClock(&e) == false);
        db.NewWorkEntry(&e, 0);
        REQUIRE(db.IsUserOnClock(&e) == true);

        LaborPeriod *lp = db.CurrentPeriod();
        lp->file_name.Set(filename);
        lp->Save();
        db.Purge();
    }
    // load in the saved LaborPeriod and add it to a new LaborDB
    LaborDB db;
    LaborPeriod *reread = new LaborPeriod;
    reread->Scan(filename.c_str());
    reread->Load();
    db.Add(reread);
    // The employee must still be logged in
    CHECK(db.IsUserOnClock(&e) == true);
}

TEST_CASE("labor: LaborPeriod: WorkReport: finding the one and only WorkEntry")
{
    SystemTime.Set(); // set SystemTime to now, needed if end is not set, used to set WorkEntry.start
    Employee e;
    e.id = 10; // normal id, no superuser, needs to clock in
    REQUIRE(e.UseClock());
    LaborDB db;
    REQUIRE(db.IsUserOnClock(&e) == false);
    db.NewWorkEntry(&e, 0);
    REQUIRE(db.IsUserOnClock(&e) == true);
    LaborPeriod *lp = db.CurrentPeriod();
    WorkEntry *work = db.CurrentWorkEntry(&e);

    Terminal term; // for testing the Terminal() constructor is made public
    term.system_data = new System; // add default system data, otherwise constructor segfaults

    TimeInfo start, end;
    end.Set();
    int selected_line =
        lp->WorkReportLine(&term, work, term.server, start, end);
    CHECK(selected_line == 0);
}
TEST_CASE("labor: LaborPeriod: WorkReport: no crash on empty WorkEntry")
{
    SystemTime.Set(); // set SystemTime to now, needed if end is not set, used to set WorkEntry.start
    Employee e;
    e.id = 10; // normal id, no superuser, needs to clock in
    REQUIRE(e.UseClock());
    LaborDB db;
    REQUIRE(db.IsUserOnClock(&e) == false);
    db.NewWorkEntry(&e, 0);
    REQUIRE(db.IsUserOnClock(&e) == true);
    LaborPeriod *lp = db.CurrentPeriod();
    WorkEntry *work = new WorkEntry;

    Terminal term; // for testing the Terminal() constructor is made public
    term.system_data = new System; // add default system data, otherwise constructor segfaults

    TimeInfo start, end;
    end.Set();
    int selected_line =
        lp->WorkReportLine(&term, work, term.server, start, end);
    CHECK(selected_line == -1); // indicating not found
}
TEST_CASE("labor: LaborPeriod: WorkReport: handle WorkEntry not in LaborPeriod")
{
    SystemTime.Set(); // set SystemTime to now, needed if end is not set, used to set WorkEntry.start
    Employee e;
    e.id = 10; // normal id, no superuser, needs to clock in
    REQUIRE(e.UseClock());
    LaborDB db;
    REQUIRE(db.IsUserOnClock(&e) == false);
    db.NewWorkEntry(&e, 0);
    REQUIRE(db.IsUserOnClock(&e) == true);
    LaborPeriod *lp = db.CurrentPeriod();

    Terminal term; // for testing the Terminal() constructor is made public
    term.system_data = new System; // add default system data, otherwise constructor segfaults

    // create new WorkEntry not in LaborDB
    WorkEntry *work = new WorkEntry;
    TimeInfo start, end;
    end.Set();
    int selected_line =
        lp->WorkReportLine(&term, work, term.server, start, end);
    CHECK(selected_line == -1); // indicating not found
}
TEST_CASE("labor: LaborPeriod: WorkReport: handle non set start time")
{
    SystemTime.Set(); // set SystemTime to now, needed if end is not set, used to set WorkEntry.start
    Employee e;
    e.id = 10; // normal id, no superuser, needs to clock in
    REQUIRE(e.UseClock());
    LaborDB db;
    REQUIRE(db.IsUserOnClock(&e) == false);
    db.NewWorkEntry(&e, 0);
    REQUIRE(db.IsUserOnClock(&e) == true);
    LaborPeriod *lp = db.CurrentPeriod();
    WorkEntry *work = db.CurrentWorkEntry(&e);

    Terminal term; // for testing the Terminal() constructor is made public
    term.system_data = new System; // add default system data, otherwise constructor segfaults

    TimeInfo start, end;
    end.Set();
    int selected_line =
        lp->WorkReportLine(&term, work, term.server, start, end);
    CHECK(selected_line == 0);
}
TEST_CASE("labor: LaborPeriod: WorkReport: find nothing when end time is before now")
{
    SystemTime.Set(); // set SystemTime to now, needed if end is not set, used to set WorkEntry.start
    Employee e;
    e.id = 10; // normal id, no superuser, needs to clock in
    REQUIRE(e.UseClock());
    LaborDB db;
    REQUIRE(db.IsUserOnClock(&e) == false);
    db.NewWorkEntry(&e, 0);
    REQUIRE(db.IsUserOnClock(&e) == true);
    LaborPeriod *lp = db.CurrentPeriod();
    WorkEntry *work = db.CurrentWorkEntry(&e);

    Terminal term; // for testing the Terminal() constructor is made public
    term.system_data = new System; // add default system data, otherwise constructor segfaults

    TimeInfo start, end;
    end.Set();
    end.AdjustDays(-1); // yesterday is before now
    int selected_line =
        lp->WorkReportLine(&term, work, term.server, start, end);
    CHECK(selected_line == -1); // no line found
}
