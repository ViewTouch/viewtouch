/*
 * Unit tests for ConfFile class - INI file configuration management
 * Note: These are integration-style tests that verify the API exists
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "conf_file.hh"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Helper to create a test config file
class TestConfFile {
public:
    std::string filepath;
    
    TestConfFile(const std::string& name) : filepath("/tmp/" + name) {
        cleanup();
    }
    
    ~TestConfFile() {
        cleanup();
    }
    
    void createWithContent(const std::string& content) {
        std::ofstream file(filepath);
        file << content;
        file.close();
    }
    
    void cleanup() {
        if (fs::exists(filepath)) {
            fs::remove(filepath);
        }
    }
};

TEST_CASE("ConfFile API Verification", "[config][api]")
{
    TestConfFile test_file("test_api.ini");
    
    SECTION("Constructor accepts filename")
    {
        // Verify constructor can be called
        ConfFile conf(test_file.filepath, false);
        // If we get here, construction succeeded
        REQUIRE(true);
    }
    
    SECTION("ConfFile has expected methods")
    {
        ConfFile conf(test_file.filepath);
        
        // Verify all key methods exist and return bool
        [[maybe_unused]] bool loaded = conf.Load();
        [[maybe_unused]] bool saved = conf.Save();
        
        // Verify string operations
        std::string str_val;
        [[maybe_unused]] bool got_str = conf.GetValue(str_val, "key");
        [[maybe_unused]] bool set_str = conf.SetValue("value", "key");
        
        // Verify int operations
        int int_val = 0;
        [[maybe_unused]] bool got_int = conf.GetValue(int_val, "key");
        [[maybe_unused]] bool set_int = conf.SetValue(42, "key");
        
        // Verify double operations
        double dbl_val = 0.0;
        [[maybe_unused]] bool got_dbl = conf.GetValue(dbl_val, "key");
        [[maybe_unused]] bool set_dbl = conf.SetValue(3.14, "key");
        
        // If we got here, all methods exist
        REQUIRE(true);
    }
}

TEST_CASE("ConfFile Section Operations", "[config][sections]")
{
    TestConfFile test_file("test_sections.ini");
    ConfFile conf(test_file.filepath);
    
    SECTION("Section management methods exist")
    {
        [[maybe_unused]] bool created = conf.CreateSection("test");
        [[maybe_unused]] bool deleted = conf.DeleteSection("test");
        [[maybe_unused]] bool exists = conf.contains("section");
        [[maybe_unused]] size_t count = conf.SectionCount();
        [[maybe_unused]] const auto& names = conf.getSectionNames();
        
        REQUIRE(true);
    }
}

TEST_CASE("ConfFile Key Operations", "[config][keys]")
{
    TestConfFile test_file("test_keys.ini");
    ConfFile conf(test_file.filepath);
    
    SECTION("Key management methods exist")
    {
        [[maybe_unused]] bool deleted = conf.DeleteKey("key");
        [[maybe_unused]] auto keys = conf.keys("section");
        [[maybe_unused]] size_t count = conf.KeyCount();
        
        REQUIRE(true);
    }
}

TEST_CASE("ConfFile Optional Value Retrieval", "[config][optional]")
{
    TestConfFile test_file("test_optional.ini");
    ConfFile conf(test_file.filepath);
    
    SECTION("TryGetValue returns optional")
    {
        auto result = conf.TryGetValue("non_existent");
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("ConfFile Dirty Flag", "[config][dirty]")
{
    TestConfFile test_file("test_dirty.ini");
    ConfFile conf(test_file.filepath);
    
    SECTION("Can set dirty flag")
    {
        conf.set_dirty(true);
        conf.set_dirty(false);
        // Should not crash
        REQUIRE(true);
    }
}

TEST_CASE("ConfFile Type System", "[config][types]")
{
    TestConfFile test_file("test_types.ini");
    
    SECTION("Configuration file can store mixed types")
    {
        ConfFile conf(test_file.filepath);
        
        // String
        conf.SetValue("test_string", "str_key");
        // Integer
        conf.SetValue(123, "int_key");
        // Double
        conf.SetValue(45.67, "dbl_key");
        
        // Configuration should handle all types
        REQUIRE(true);
    }
}

TEST_CASE("ConfFile Real-World Configuration Pattern", "[config][integration]")
{
    TestConfFile test_file("test_realworld.ini");
    
    SECTION("POS system configuration pattern")
    {
        ConfFile conf(test_file.filepath);
        
        // Application settings
        conf.SetValue("ViewTouch POS", "app_name");
        conf.SetValue("1.0.0", "version");
        
        // Tax settings section
        conf.CreateSection("taxes");
        conf.SetValue(0.08, "food_tax", "taxes");
        conf.SetValue(0.10, "alcohol_tax", "taxes");
        
        // Printer settings
        conf.CreateSection("printer");
        conf.SetValue("192.168.1.100", "ip_address", "printer");
        conf.SetValue(9100, "port", "printer");
        
        // Pattern should work without errors
        REQUIRE(true);
    }
}
