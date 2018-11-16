#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "conf_file.hh"

#include <string>
#include <vector>
#include <fstream>
#include <limits> // std::numeric_limits
#include <cmath> // std::isnan, std::isinf
#include <locale>

TEST_CASE("setter_default_section", "[conf_file]")
{
    ConfFile conf("setter.ini");
    CHECK(conf.SetValue("value", "key"));

    std::string val;
    CHECK(conf.GetValue(val, "key"));
    CHECK(val == "value");
}
TEST_CASE("setter_missing_section", "[conf_file]")
{
    ConfFile conf("setter_missing_section.ini");

    CHECK(conf.SetValue("value", "key", "new_section"));
    std::string value_ret;
    CHECK(conf.GetValue(value_ret, "key", "new_section"));
    CHECK_FALSE(conf.GetValue(value_ret, "key", "missing_section"));
}
TEST_CASE("setter_emtpy_parameter", "[conf_file]")
{
    ConfFile conf("setter_empty_parameter.ini");

    CHECK_FALSE(conf.SetValue("", "key"));
    CHECK_FALSE(conf.SetValue("value", ""));
}

TEST_CASE("strings", "[conf_file]")
{
    ConfFile conf("strings.ini");
    const std::vector<std::string> strings = {
        "a",
        "bb",
        "1",
        "NaN",
        "string with spaces in it",
        "string with brackets [ { } ] in it",
        "string with brackets ; _ - in it",
        "; value starting with semicolon",
        "# value starting with hashtag",
        "\" value surrounded by \"",
    };
    for (const std::string &val : strings)
    {
        std::string val_ret;
        REQUIRE(conf.SetValue(val, "key"));
        CHECK(conf.GetValue(val_ret, "key"));
        CHECK(val == val_ret);
    }
}
TEST_CASE("integer", "[conf_file]")
{
    ConfFile conf("integer.ini");
    const std::vector<int> integers = {
        0,
         1,  3,  5,  7,  1337,
        -1, -3, -5, -7, -1337,
    };
    for (const int val : integers)
    {
        int val_ret;
        REQUIRE(conf.SetValue(val, "key"));
        CHECK(conf.GetValue(val_ret, "key"));
        CHECK(val == val_ret);
    }
}
TEST_CASE("doubles", "[conf_file]")
{
    ConfFile conf("doubles.ini");
    const std::vector<double> doubles = {
        0, 1, 3, 5, 7, 1337, -1, -3, -5, -7, -1337,
        0.001, 1.0/4,
        0.000031212421108108183401041,
    };
    for (const double val : doubles)
    {
        double val_ret;
        REQUIRE(conf.SetValue(val, "key"));
        CHECK(conf.GetValue(val_ret, "key"));
        // printf(%f) and std::to_string(double) have a standard precision of
        // 6 digits after the comma
        CHECK(val == Approx{val_ret}.epsilon(1e-6));
    }
}
TEST_CASE("doubles_inf", "[conf_file]")
{
    ConfFile conf("doubles_inf.ini");
    const std::vector<double> doubles = {
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };
    for (const double val : doubles)
    {
        double val_ret;
        REQUIRE(conf.SetValue(val, "key"));
        CHECK(conf.GetValue(val_ret, "key"));
        CHECK(std::signbit(val) == std::signbit(val_ret));
        CHECK(std::isinf(val_ret));
    }
}
TEST_CASE("doubles_nan", "[conf_file]")
{
    ConfFile conf("doubles_nan.ini");
    // can't compare NaN like the other doubles, NaN == NaN --> false
    const std::vector<double> doubles = {
        std::numeric_limits<double>::quiet_NaN(),
        -std::numeric_limits<double>::signaling_NaN(),
    };
    for (const double val : doubles)
    {
        double val_ret;
        REQUIRE(conf.SetValue(val, "key"));
        CHECK(conf.GetValue(val_ret, "key"));
        CHECK(std::isnan(val_ret));
    }
}
TEST_CASE("load_default_section", "[conf_file]")
{
    const std::string filename = "load_default_section.ini";
    {
        std::ofstream fout(filename);
        fout << "key=value" << std::endl;
        fout << "k_no_value=" << std::endl;
        fout << "=value_no_key" << std::endl;
        fout << "# comment with key value pair key_comment=value_comment" << std::endl;
    }
    ConfFile conf(filename, true); // load config file

    std::string val;
    CHECK(conf.GetValue(val, "key"));
    CHECK(val == "value");
    CHECK_FALSE(conf.GetValue(val, "k_no_value"));
    CHECK_FALSE(conf.GetValue(val, ""));
}

namespace // locale namespace for load_double
{
// https://stackoverflow.com/questions/15220861/how-can-i-set-the-decimal-separator-to-be-a-comma
template <class charT>
class punct_facet: public std::numpunct<charT> {
protected:
    charT do_decimal_point() const { return ','; }
    charT do_thousands_sep() const { return '.'; }
};
}
TEST_CASE("load_double", "[conf_file]")
{
    // create and set a locale where '.' is the thousand separator and ',' is
    // the decimal separator (as is the case in locales (i.e. de_DE, nb_NO, ...)
    auto loc_comma = std::locale(std::locale::classic(), new punct_facet<char>);
    std::locale::global(loc_comma);

    const std::string filename = "load_double.ini";
    {
        std::ofstream fout(filename);
        fout << "key=1.337" << std::endl;
        fout << "inf=inf" << std::endl;
    }
    ConfFile conf(filename, true); // load config file

    double val;
    CHECK(conf.GetValue(val, "key"));
    CHECK(val == Approx{1.337}.epsilon(1e-6));
    CHECK(conf.GetValue(val, "inf"));
    CHECK(val == std::numeric_limits<double>::infinity());
}
TEST_CASE("load_string_as_number", "[conf_file]")
{
    const std::string filename = "load_string_as_number.ini";
    {
        std::ofstream fout(filename);
        fout << "key=value" << std::endl;
    }
    ConfFile conf(filename, true); // load config file

    double val;
    CHECK_FALSE(conf.GetValue(val, "key"));
    int int_val;
    CHECK_FALSE(conf.GetValue(int_val, "key"));
}
TEST_CASE("getter_no_modification", "[conf_file]")
{
    // reads of non existing keys don't modify the target variable
    const std::string filename = "conf_file_getter_no_modification.ini";
    ConfFile conf(filename); // load config file

    std::string val_str = "1337";
    int val_int = 1337;
    double val_dbl = 1337;
    CHECK_FALSE(conf.GetValue(val_str, "key"));
    CHECK_FALSE(conf.GetValue(val_int, "key"));
    CHECK_FALSE(conf.GetValue(val_dbl, "key"));
    CHECK(val_str == "1337");
    CHECK(val_int == 1337);
    CHECK(val_dbl == 1337);
}
TEST_CASE("load_with_section", "[conf_file]")
{
    const std::string filename = "load_with_section.ini";
    {
        std::ofstream fout(filename);
        fout << "key=value" << std::endl;
        fout << "[section_no_keys]" << std::endl;
        fout << "# comment with key value pair key_comment=value_comment" << std::endl;
        fout << "[section]" << std::endl;
        fout << "key=section_value" << std::endl;
        fout << "k_no_value=" << std::endl;
        fout << "=value_no_key" << std::endl;
        fout << "# comment with key value pair key_comment=value_comment" << std::endl;
    }
    ConfFile conf(filename, true); // load config file

    std::string val;
    // read key from default section
    CHECK(conf.GetValue(val, "key"));
    CHECK(val == "value");
    // read key from section with no keys, expect failure
    CHECK_FALSE(conf.GetValue(val, "key", "section_no_keys"));
    // read key from section, expect different value
    CHECK(conf.GetValue(val, "key", "section"));
    CHECK(val == "section_value");
    // try to read not available keys
    CHECK_FALSE(conf.GetValue(val, "k_no_value", "section"));
    CHECK_FALSE(conf.GetValue(val, "", "section"));
}

TEST_CASE("save_with_section", "[conf_file]")
{
    const std::string filename = "save_with_section.ini";
    {
        ConfFile conf_save(filename);
        conf_save.SetValue("value", "key");
        conf_save.SetValue("section_value", "key", "section");
        // destructor also saves to the ini file
    }
    ConfFile conf(filename, true); // load config file

    std::string val;
    // read key from default section
    CHECK(conf.GetValue(val, "key"));
    CHECK(val == "value");
    // read key from section, expect different value
    CHECK(conf.GetValue(val, "key", "section"));
    CHECK(val == "section_value");
    // try to read not available keys
    CHECK_FALSE(conf.GetValue(val, "k_no_value", "section"));
    CHECK_FALSE(conf.GetValue(val, "", "section"));
}

TEST_CASE("no_delete_default_section", "[conf_file]")
{
    const std::string filename = "conf_file_no_delete_default_section.ini";
    ConfFile conf(filename);
    CHECK_FALSE(conf.DeleteSection(""));
}

TEST_CASE("delete_key_twice", "[conf_file]")
{
    const std::string filename = "conf_file_delete_key_twice.ini";
    ConfFile conf(filename);
    // add a key to delete later
    REQUIRE(conf.SetValue("value", "key"));
    // delete key
    REQUIRE(conf.DeleteKey("key"));
    // can't delete the same key twice
    CHECK_FALSE(conf.DeleteKey("key"));
}

TEST_CASE("delete_empty_key", "[conf_file]")
{
    const std::string filename = "conf_file_delete_empty_key.ini";
    ConfFile conf(filename);
    CHECK_FALSE(conf.DeleteKey(""));
}

TEST_CASE("list_all_sections", "[conf_file]")
{
    const std::string filename = "conf_file_list_all_sections.ini";
    ConfFile conf(filename);
    conf.SetValue("value", "key");
    conf.SetValue("section_value", "key", "section");

    const std::vector<std::string> sections = conf.getSectionNames();
    CHECK(sections[0] == "");
    CHECK(sections[1] == "section");
}

TEST_CASE("set_dirty_false", "[conf_file]")
{
    const std::string filename = "conf_file_set_dirty_false.ini";
    {
        ConfFile conf(filename);
        conf.SetValue("value", "key");
        conf.SetValue("section_value", "key", "section");
        // disable writing of conf file
        conf.set_dirty(false);
    }
    // file should not exist
    std::ifstream conf_file(filename);
    CHECK(conf_file.fail());
}

TEST_CASE("keys_empty_section", "[conf_file]")
{
    const std::string filename = "conf_file_keys_empty_section.ini";
    ConfFile conf(filename);
    const std::vector<std::string> default_keys = conf.keys("");

    CHECK(default_keys.size() == 0);
}

TEST_CASE("keys_exception_invalid_section", "[conf_file]")
{
    const std::string filename = "conf_file_keys_exception_invalid_section.ini";
    ConfFile conf(filename);
    CHECK_THROWS_AS(conf.keys("invalid_section"), std::out_of_range);
}

TEST_CASE("keys_list", "[conf_file]")
{
    const std::string filename = "conf_file_keys_list.ini";
    ConfFile conf(filename);
    conf.SetValue("value", "key1");
    conf.SetValue("value", "key2");
    conf.SetValue("value", "key3");
    conf.SetValue("section_value", "key", "section");

    const std::vector<std::string> default_keys = conf.keys("");
    const std::vector<std::string> section_keys = conf.keys("section");

    REQUIRE(default_keys.size() == 3);
    REQUIRE(section_keys.size() == 1);

    CHECK(default_keys[0] == "key1");
    CHECK(default_keys[1] == "key2");
    CHECK(default_keys[2] == "key3");
    CHECK(section_keys[0] == "key");
}
