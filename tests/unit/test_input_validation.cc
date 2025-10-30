#include <catch2/catch_all.hpp>
#include "src/utils/input_validation.hh"

TEST_CASE("Network Data Validation", "[input_validation][network]")
{
    SECTION("Valid hostnames")
    {
        auto result = vt_input_validation::NetworkValidator::validate_hostname("example.com");
        REQUIRE(result.is_valid == true);
        REQUIRE(result.sanitized_value == "example.com");

        result = vt_input_validation::NetworkValidator::validate_hostname("sub.example.com");
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::NetworkValidator::validate_hostname("localhost");
        REQUIRE(result.is_valid == true);
    }

    SECTION("Invalid hostnames")
    {
        auto result = vt_input_validation::NetworkValidator::validate_hostname("");
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::NetworkValidator::validate_hostname("invalid..hostname");
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::NetworkValidator::validate_hostname(std::string(300, 'a'));
        REQUIRE(result.is_valid == false);
    }

    SECTION("Port validation")
    {
        auto result = vt_input_validation::NetworkValidator::validate_port(80);
        REQUIRE(result.is_valid == true);
        REQUIRE(result.sanitized_value == "80");

        result = vt_input_validation::NetworkValidator::validate_port(0);
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::NetworkValidator::validate_port(70000);
        REQUIRE(result.is_valid == false);
    }

    SECTION("Port string validation")
    {
        auto result = vt_input_validation::NetworkValidator::validate_port("443");
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::NetworkValidator::validate_port("abc");
        REQUIRE(result.is_valid == false);
    }

    SECTION("Buffer size validation")
    {
        auto result = vt_input_validation::NetworkValidator::validate_buffer_size(1024, 4096);
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::NetworkValidator::validate_buffer_size(5000, 4096);
        REQUIRE(result.is_valid == false);
    }

    SECTION("Socket data validation")
    {
        const char* valid_data = "Hello World";
        auto result = vt_input_validation::NetworkValidator::validate_socket_data(valid_data, 11);
        REQUIRE(result.is_valid == true);

        const char* null_data = nullptr;
        result = vt_input_validation::NetworkValidator::validate_socket_data(null_data, 10);
        REQUIRE(result.is_valid == false);

        const char* embedded_null = "Hello\0World";
        result = vt_input_validation::NetworkValidator::validate_socket_data(embedded_null, 11);
        REQUIRE(result.is_valid == false);
    }

    SECTION("Card number format validation")
    {
        // Note: This validates format but masks the actual data
        auto result = vt_input_validation::NetworkValidator::validate_card_number_format("4111111111111111");
        REQUIRE(result.is_valid == true);
        REQUIRE(result.sanitized_value == std::string(16, '*')); // Masked

        result = vt_input_validation::NetworkValidator::validate_card_number_format("123");
        REQUIRE(result.is_valid == false);
    }
}

TEST_CASE("Business Logic Validation", "[input_validation][business]")
{
    SECTION("Price validation")
    {
        auto result = vt_input_validation::BusinessValidator::validate_price(1099); // $10.99
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::BusinessValidator::validate_price(-1000001);
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::BusinessValidator::validate_price(1000001);
        REQUIRE(result.is_valid == false);
    }

    SECTION("Price string validation")
    {
        auto result = vt_input_validation::BusinessValidator::validate_price("10.99");
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::BusinessValidator::validate_price("invalid");
        REQUIRE(result.is_valid == false);
    }

    SECTION("Quantity validation")
    {
        auto result = vt_input_validation::BusinessValidator::validate_quantity(5);
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::BusinessValidator::validate_quantity(-1);
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::BusinessValidator::validate_quantity(15000);
        REQUIRE(result.is_valid == false);
    }

    SECTION("Discount percentage validation")
    {
        auto result = vt_input_validation::BusinessValidator::validate_discount_percent(15.5f);
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::BusinessValidator::validate_discount_percent(-5.0f);
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::BusinessValidator::validate_discount_percent(150.0f);
        REQUIRE(result.is_valid == false);
    }

    SECTION("Tax rate validation")
    {
        auto result = vt_input_validation::BusinessValidator::validate_tax_rate(0.0825f); // 8.25%
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::BusinessValidator::validate_tax_rate(-0.01f);
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::BusinessValidator::validate_tax_rate(0.6f); // 60%
        REQUIRE(result.is_valid == false);
    }

    SECTION("Employee ID validation")
    {
        auto result = vt_input_validation::BusinessValidator::validate_employee_id(12345);
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::BusinessValidator::validate_employee_id(0);
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::BusinessValidator::validate_employee_id(2000000);
        REQUIRE(result.is_valid == false);
    }

    SECTION("Table number validation")
    {
        auto result = vt_input_validation::BusinessValidator::validate_table_number(42);
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::BusinessValidator::validate_table_number(0);
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::BusinessValidator::validate_table_number(1500);
        REQUIRE(result.is_valid == false);
    }
}

TEST_CASE("User Input Validation", "[input_validation][user]")
{
    SECTION("Text input validation")
    {
        auto result = vt_input_validation::UserInputValidator::validate_text_input("Hello World", 50, false);
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::UserInputValidator::validate_text_input(std::string(100, 'a'), 50, false);
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::UserInputValidator::validate_text_input("Hello\x01World", 50, false);
        REQUIRE(result.is_valid == false);
    }

    SECTION("Name validation")
    {
        auto result = vt_input_validation::UserInputValidator::validate_name("John Doe");
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::UserInputValidator::validate_name("");
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::UserInputValidator::validate_name("A");
        REQUIRE(result.is_valid == false);
    }

    SECTION("Email validation")
    {
        auto result = vt_input_validation::UserInputValidator::validate_email("user@example.com");
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::UserInputValidator::validate_email("invalid-email");
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::UserInputValidator::validate_email("");
        REQUIRE(result.is_valid == false);
    }

    SECTION("Phone validation")
    {
        auto result = vt_input_validation::UserInputValidator::validate_phone("555-123-4567");
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::UserInputValidator::validate_phone("123");
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::UserInputValidator::validate_phone("12345678901234567890");
        REQUIRE(result.is_valid == false);
    }

    SECTION("Password validation")
    {
        auto result = vt_input_validation::UserInputValidator::validate_password("SecurePass123");
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::UserInputValidator::validate_password("weak");
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::UserInputValidator::validate_password("nouppercaseordigit");
        REQUIRE(result.is_valid == false);
    }

    SECTION("Username validation")
    {
        auto result = vt_input_validation::UserInputValidator::validate_username("john_doe123");
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::UserInputValidator::validate_username("123invalid");
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::UserInputValidator::validate_username("");
        REQUIRE(result.is_valid == false);
    }

    SECTION("HTML sanitization")
    {
        std::string input = "Hello <script>alert('XSS')</script> World";
        std::string result = vt_input_validation::UserInputValidator::sanitize_html(input);
        REQUIRE(result.find("<script>") == std::string::npos);
        REQUIRE(result.find("alert") == std::string::npos);
    }
}

TEST_CASE("Configuration Validation", "[input_validation][config]")
{
    SECTION("Config path validation")
    {
        auto result = vt_input_validation::ConfigValidator::validate_config_path("/etc/viewtouch/config.ini");
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::ConfigValidator::validate_config_path("../../etc/passwd");
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::ConfigValidator::validate_config_path("");
        REQUIRE(result.is_valid == false);
    }

    SECTION("Time format validation")
    {
        auto result = vt_input_validation::ConfigValidator::validate_time_format("%H:%M:%S");
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::ConfigValidator::validate_time_format("%I:%M %p");
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::ConfigValidator::validate_time_format("invalid");
        REQUIRE(result.is_valid == false);
    }

    SECTION("Date format validation")
    {
        auto result = vt_input_validation::ConfigValidator::validate_date_format("%Y-%m-%d");
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::ConfigValidator::validate_date_format("%m/%d/%Y");
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::ConfigValidator::validate_date_format("invalid");
        REQUIRE(result.is_valid == false);
    }
}

TEST_CASE("Security Validation", "[input_validation][security]")
{
    SECTION("SQL injection detection")
    {
        auto result = vt_input_validation::SecurityValidator::check_sql_injection("SELECT * FROM users");
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::SecurityValidator::check_sql_injection("Normal input");
        REQUIRE(result.is_valid == true);
    }

    SECTION("Command injection detection")
    {
        auto result = vt_input_validation::SecurityValidator::check_command_injection("rm -rf /");
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::SecurityValidator::check_command_injection("safe command");
        REQUIRE(result.is_valid == true);
    }

    SECTION("Path traversal detection")
    {
        auto result = vt_input_validation::SecurityValidator::check_path_traversal("../../etc/passwd");
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::SecurityValidator::check_path_traversal("safe/path/file.txt");
        REQUIRE(result.is_valid == true);
    }

    SECTION("Buffer overflow detection")
    {
        auto result = vt_input_validation::SecurityValidator::check_buffer_overflow(std::string(1000, 'A'), 100);
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::SecurityValidator::check_buffer_overflow("Normal input", 100);
        REQUIRE(result.is_valid == true);
    }

    SECTION("File extension validation")
    {
        std::vector<std::string> allowed = {"txt", "ini", "cfg"};
        auto result = vt_input_validation::SecurityValidator::validate_file_extension("config.ini", allowed);
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::SecurityValidator::validate_file_extension("script.exe", allowed);
        REQUIRE(result.is_valid == false);
    }

    SECTION("Suspicious pattern detection")
    {
        // Test binary data detection
        std::string binary_data;
        for (int i = 0; i < 50; ++i) {
            binary_data += static_cast<char>(i % 10); // Lots of control chars
        }
        auto result = vt_input_validation::SecurityValidator::check_suspicious_patterns(binary_data);
        REQUIRE(result.is_valid == false);

        result = vt_input_validation::SecurityValidator::check_suspicious_patterns("Normal text data");
        REQUIRE(result.is_valid == true);
    }
}

TEST_CASE("Validation Utilities", "[input_validation][utils]")
{
    SECTION("Alphanumeric check")
    {
        REQUIRE(vt_input_validation::ValidationUtils::is_alphanumeric("abc123") == true);
        REQUIRE(vt_input_validation::ValidationUtils::is_alphanumeric("abc 123") == false);
    }

    SECTION("Numeric check")
    {
        REQUIRE(vt_input_validation::ValidationUtils::is_numeric("12345") == true);
        REQUIRE(vt_input_validation::ValidationUtils::is_numeric("123.45") == false);
    }

    SECTION("Identifier validation")
    {
        REQUIRE(vt_input_validation::ValidationUtils::is_valid_identifier("variable_name") == true);
        REQUIRE(vt_input_validation::ValidationUtils::is_valid_identifier("123invalid") == false);
    }

    SECTION("String trimming")
    {
        REQUIRE(vt_input_validation::ValidationUtils::trim("  hello  ") == "hello");
        REQUIRE(vt_input_validation::ValidationUtils::trim("no_spaces") == "no_spaces");
    }

    SECTION("Length validation")
    {
        REQUIRE(vt_input_validation::ValidationUtils::is_length_valid("test", 2, 10) == true);
        REQUIRE(vt_input_validation::ValidationUtils::is_length_valid("test", 10, 20) == false);
    }

    SECTION("HTML escaping")
    {
        std::string result = vt_input_validation::ValidationUtils::escape_special_chars("<>&\"'");
        REQUIRE(result == "&lt;&gt;&amp;&quot;&apos;");
    }

    SECTION("UTF-8 validation")
    {
        auto result = vt_input_validation::ValidationUtils::validate_utf8("Hello World");
        REQUIRE(result.is_valid == true);

        result = vt_input_validation::ValidationUtils::validate_utf8("Hello \x80 World"); // Invalid UTF-8
        REQUIRE(result.is_valid == false);
    }
}

TEST_CASE("Data Sanitization", "[input_validation][sanitization]")
{
    SECTION("Null byte removal")
    {
        std::string input_with_null = "Hello";
        input_with_null += '\0';
        input_with_null += "World";
        std::string result = vt_input_validation::Sanitizer::remove_null_bytes(input_with_null);
        REQUIRE(result == "HelloWorld");
    }

    SECTION("Control character removal")
    {
        std::string result = vt_input_validation::Sanitizer::remove_control_chars("Hello\x01\x02World");
        REQUIRE(result == "HelloWorld");
    }

    SECTION("Line ending normalization")
    {
        std::string input = "Line1\r\nLine2\rLine3\n";
        std::string result = vt_input_validation::Sanitizer::normalize_line_endings(input);
        std::string expected = "Line1\nLine2\nLine3\n";
        REQUIRE(result == expected);
    }

    SECTION("Dangerous character removal")
    {
        std::string result = vt_input_validation::Sanitizer::remove_dangerous_chars("Safe\x01\x02\x03Text");
        REQUIRE(result == "SafeText");
    }

    SECTION("SQL sanitization")
    {
        std::string result = vt_input_validation::Sanitizer::sanitize_for_sql("Don't do this");
        REQUIRE(result == "Don''t do this");
    }

    SECTION("Shell sanitization")
    {
        std::string result = vt_input_validation::Sanitizer::sanitize_for_shell("echo 'hello world'");
        REQUIRE(result.find("\\'") != std::string::npos); // Quotes should be escaped
    }
}

TEST_CASE("Validation Context Management", "[input_validation][context]")
{
    SECTION("Context error tracking")
    {
        vt_input_validation::ValidationContext context;

        context.add_error("First error");
        context.add_error("Second error");
        context.add_warning("Just a warning");

        REQUIRE(context.has_errors() == true);
        REQUIRE(context.has_warnings() == true);
        REQUIRE(context.get_errors().size() == 2);
        REQUIRE(context.get_warnings().size() == 1);

        context.clear();
        REQUIRE(context.has_errors() == false);
        REQUIRE(context.has_warnings() == false);
    }

    SECTION("Severity levels")
    {
        vt_input_validation::ValidationContext context;

        context.set_severity_level(vt_input_validation::ValidationSeverity::CRITICAL);
        REQUIRE(context.get_severity_level() == vt_input_validation::ValidationSeverity::CRITICAL);
    }
}

TEST_CASE("Integration Tests", "[input_validation][integration]")
{
    SECTION("Complete user registration validation")
    {
        std::string username = "john_doe123";
        std::string email = "john@example.com";
        std::string password = "SecurePass123!";
        std::string phone = "555-123-4567";

        auto username_result = vt_input_validation::UserInputValidator::validate_username(username);
        auto email_result = vt_input_validation::UserInputValidator::validate_email(email);
        auto password_result = vt_input_validation::UserInputValidator::validate_password(password);
        auto phone_result = vt_input_validation::UserInputValidator::validate_phone(phone);

        REQUIRE(username_result.is_valid == true);
        REQUIRE(email_result.is_valid == true);
        REQUIRE(password_result.is_valid == true);
        REQUIRE(phone_result.is_valid == true);
    }

    SECTION("Network connection validation")
    {
        std::string hostname = "api.example.com";
        int port = 443;

        auto hostname_result = vt_input_validation::NetworkValidator::validate_hostname(hostname);
        auto port_result = vt_input_validation::NetworkValidator::validate_port(port);

        REQUIRE(hostname_result.is_valid == true);
        REQUIRE(port_result.is_valid == true);
    }

    SECTION("Business transaction validation")
    {
        int price = 2599; // $25.99
        int quantity = 2;
        int employee_id = 12345;
        int table_number = 15;

        auto price_result = vt_input_validation::BusinessValidator::validate_price(price);
        auto quantity_result = vt_input_validation::BusinessValidator::validate_quantity(quantity);
        auto employee_result = vt_input_validation::BusinessValidator::validate_employee_id(employee_id);
        auto table_result = vt_input_validation::BusinessValidator::validate_table_number(table_number);

        REQUIRE(price_result.is_valid == true);
        REQUIRE(quantity_result.is_valid == true);
        REQUIRE(employee_result.is_valid == true);
        REQUIRE(table_result.is_valid == true);

        // Calculate total
        int total = price * quantity; // 2599 * 2 = 5198 cents = $51.98
        auto total_result = vt_input_validation::BusinessValidator::validate_check_total(total);
        REQUIRE(total_result.is_valid == true);
    }

    SECTION("Security scan integration")
    {
        std::string safe_input = "Normal user input";
        std::string sql_injection = "SELECT * FROM users";
        std::string cmd_injection = "echo hello; rm -rf /";
        std::string path_traversal = "../../../etc/passwd";

        auto safe_sql = vt_input_validation::SecurityValidator::check_sql_injection(safe_input);
        auto unsafe_sql = vt_input_validation::SecurityValidator::check_sql_injection(sql_injection);
        auto unsafe_cmd = vt_input_validation::SecurityValidator::check_command_injection(cmd_injection);
        auto unsafe_path = vt_input_validation::SecurityValidator::check_path_traversal(path_traversal);

        REQUIRE(safe_sql.is_valid == true);
        REQUIRE(unsafe_sql.is_valid == false);
        REQUIRE(unsafe_cmd.is_valid == false);
        REQUIRE(unsafe_path.is_valid == false);
    }
}
