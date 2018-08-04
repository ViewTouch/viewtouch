#include "gtest/gtest.h"
#include "conf_file.hh"

#include <string>
#include <vector>
#include <fstream>
#include <limits> // std::numeric_limits
#include <cmath> // std::isnan
#include <locale>

TEST(conf_file, setter_default_section)
{
    ConfFile conf("setter.ini");
    EXPECT_TRUE(conf.SetValue("value", "key"));

    std::string val;
    EXPECT_TRUE(conf.GetValue(val, "key"));
    EXPECT_EQ(val, "value");
}
TEST(conf_file, setter_missing_section)
{
    ConfFile conf("setter_missing_section.ini");

    EXPECT_TRUE(conf.SetValue("value", "key", "new_section"));
    std::string value_ret;
    EXPECT_TRUE(conf.GetValue(value_ret, "key", "new_section"));
    EXPECT_FALSE(conf.GetValue(value_ret, "key", "missing_section"));
}
TEST(conf_file, setter_emtpy_parameter)
{
    ConfFile conf("setter_empty_parameter.ini");

    EXPECT_FALSE(conf.SetValue("", "key"));
    EXPECT_FALSE(conf.SetValue("value", ""));
}

TEST(conf_file, strings)
{
    ConfFile conf("strings.ini");
    std::vector<std::string> strings = {
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
        ASSERT_TRUE(conf.SetValue(val, "key"));
        EXPECT_TRUE(conf.GetValue(val_ret, "key"));
        EXPECT_EQ(val, val_ret);
    }
}
TEST(conf_file, integer)
{
    ConfFile conf("integer.ini");
    std::vector<int> integers = {
        0,
         1,  3,  5,  7,  1337,
        -1, -3, -5, -7, -1337,
    };
    for (const int val : integers)
    {
        int val_ret;
        ASSERT_TRUE(conf.SetValue(val, "key"));
        EXPECT_TRUE(conf.GetValue(val_ret, "key"));
        EXPECT_EQ(val, val_ret);
    }
}
TEST(conf_file, doubles)
{
    ConfFile conf("doubles.ini");
    std::vector<double> doubles = {
        0, 1, 3, 5, 7, 1337, -1, -3, -5, -7, -1337,
        0.001, 1.0/4,
        0.000031212421108108183401041,
    };
    for (const double val : doubles)
    {
        double val_ret;
        ASSERT_TRUE(conf.SetValue(val, "key"));
        EXPECT_TRUE(conf.GetValue(val_ret, "key"));
        // printf(%f) and std::to_string(double) have a standard precision of
        // 6 digits after the comma
        EXPECT_NEAR(val, val_ret, 1e-6);
    }
}
TEST(conf_file, doubles_inf)
{
    ConfFile conf("doubles_inf.ini");
    std::vector<double> doubles = {
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };
    for (const double val : doubles)
    {
        double val_ret;
        ASSERT_TRUE(conf.SetValue(val, "key"));
        EXPECT_TRUE(conf.GetValue(val_ret, "key"));
        EXPECT_EQ(val, val_ret);
    }
}
TEST(conf_file, doubles_nan)
{
    ConfFile conf("doubles_nan.ini");
    // can't compare NaN like the other doubles, NaN == NaN --> false
    const double val = std::numeric_limits<double>::quiet_NaN();
    double val_ret;
    ASSERT_TRUE(conf.SetValue(val, "key"));
    EXPECT_TRUE(conf.GetValue(val_ret, "key"));
    EXPECT_TRUE(std::isnan(val_ret));
}
TEST(conf_file, load_default_section)
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
    EXPECT_TRUE(conf.GetValue(val, "key"));
    EXPECT_EQ(val, "value");
    EXPECT_FALSE(conf.GetValue(val, "k_no_value"));
    EXPECT_FALSE(conf.GetValue(val, ""));
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
TEST(conf_file, load_double)
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
    EXPECT_TRUE(conf.GetValue(val, "key"));
    EXPECT_NEAR(val, 1.337, 1e-6);
    EXPECT_TRUE(conf.GetValue(val, "inf"));
    EXPECT_EQ(val, std::numeric_limits<double>::infinity());
}
TEST(conf_file, load_string_as_number)
{
    const std::string filename = "load_string_as_number.ini";
    {
        std::ofstream fout(filename);
        fout << "key=value" << std::endl;
    }
    ConfFile conf(filename, true); // load config file

    double val;
    EXPECT_FALSE(conf.GetValue(val, "key"));
    int int_val;
    EXPECT_FALSE(conf.GetValue(int_val, "key"));
}
TEST(conf_file, getter_no_modification)
{
    // reads of non existing keys don't modify the target variable
    const std::string filename = "conf_file_getter_no_modification.ini";
    ConfFile conf(filename); // load config file

    std::string val_str = "1337";
    int val_int = 1337;
    double val_dbl = 1337;
    EXPECT_FALSE(conf.GetValue(val_str, "key"));
    EXPECT_FALSE(conf.GetValue(val_int, "key"));
    EXPECT_FALSE(conf.GetValue(val_dbl, "key"));
    EXPECT_EQ(val_str, "1337");
    EXPECT_EQ(val_int, 1337);
    EXPECT_EQ(val_dbl, 1337);
}
TEST(conf_file, load_with_section)
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
    EXPECT_TRUE(conf.GetValue(val, "key"));
    EXPECT_EQ(val, "value");
    // read key from section with no keys, expect failure
    EXPECT_FALSE(conf.GetValue(val, "key", "section_no_keys"));
    // read key from section, expect different value
    EXPECT_TRUE(conf.GetValue(val, "key", "section"));
    EXPECT_EQ(val, "section_value");
    // try to read not available keys
    EXPECT_FALSE(conf.GetValue(val, "k_no_value", "section"));
    EXPECT_FALSE(conf.GetValue(val, "", "section"));
}

TEST(conf_file, save_with_section)
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
    EXPECT_TRUE(conf.GetValue(val, "key"));
    EXPECT_EQ(val, "value");
    // read key from section, expect different value
    EXPECT_TRUE(conf.GetValue(val, "key", "section"));
    EXPECT_EQ(val, "section_value");
    // try to read not available keys
    EXPECT_FALSE(conf.GetValue(val, "k_no_value", "section"));
    EXPECT_FALSE(conf.GetValue(val, "", "section"));
}

TEST(conf_file, no_delete_default_section)
{
    const std::string filename = "conf_file_no_delete_default_section.ini";
    ConfFile conf(filename);
    ASSERT_FALSE(conf.DeleteSection(""));
}

TEST(conf_file, delete_key_twice)
{
    const std::string filename = "conf_file_delete_key_twice.ini";
    ConfFile conf(filename);
    // add a key to delete later
    ASSERT_TRUE(conf.SetValue("value", "key"));
    // delete key
    ASSERT_TRUE(conf.DeleteKey("key"));
    // can't delete the same key twice
    ASSERT_FALSE(conf.DeleteKey("key"));
}

TEST(conf_file, delete_empty_key)
{
    const std::string filename = "conf_file_delete_empty_key.ini";
    ConfFile conf(filename);
    ASSERT_FALSE(conf.DeleteKey(""));
}

TEST(conf_file, list_all_sections)
{
    const std::string filename = "conf_file_list_all_sections.ini";
    ConfFile conf(filename);
    conf.SetValue("value", "key");
    conf.SetValue("section_value", "key", "section");

    const std::vector<std::string> sections = conf.getSectionNames();
    EXPECT_EQ(sections[0], "");
    EXPECT_EQ(sections[1], "section");
}

TEST(conf_file, set_dirty_false)
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
    EXPECT_TRUE(conf_file.fail());
}

TEST(conf_file, keys_empty_section)
{
    const std::string filename = "conf_file_keys_empty_section.ini";
    ConfFile conf(filename);
    const std::vector<std::string> default_keys = conf.keys("");

    ASSERT_EQ(default_keys.size(), 0);
}

TEST(conf_file, keys_exception_invalid_section)
{
    const std::string filename = "conf_file_keys_exception_invalid_section.ini";
    ConfFile conf(filename);
    ASSERT_THROW(conf.keys("invalid_section"), std::out_of_range);
}

TEST(conf_file, keys_list)
{
    const std::string filename = "conf_file_keys_list.ini";
    ConfFile conf(filename);
    conf.SetValue("value", "key1");
    conf.SetValue("value", "key2");
    conf.SetValue("value", "key3");
    conf.SetValue("section_value", "key", "section");

    const std::vector<std::string> default_keys = conf.keys("");
    const std::vector<std::string> section_keys = conf.keys("section");

    ASSERT_EQ(default_keys.size(), 3);
    ASSERT_EQ(section_keys.size(), 1);

    EXPECT_EQ(default_keys[0], "key1");
    EXPECT_EQ(default_keys[1], "key2");
    EXPECT_EQ(default_keys[2], "key3");
    EXPECT_EQ(section_keys[0], "key");
}
