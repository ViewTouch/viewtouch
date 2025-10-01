#ifndef VT_VERSION_INFO_HPP
#define VT_VERSION_INFO_HPP

#include <string>
#include <string_view>

namespace viewtouch
{

// Modern C++ interface with noexcept and [[nodiscard]] attributes
[[nodiscard]] const std::string& get_project_name() noexcept;
[[nodiscard]] const std::string& get_version_short() noexcept;
[[nodiscard]] const std::string& get_version_extended() noexcept;
[[nodiscard]] const std::string& get_version_long() noexcept;
[[nodiscard]] const std::string& get_version_info() noexcept;
[[nodiscard]] const std::string& get_version_timestamp() noexcept;

} // end namespace viewtouch
#endif // VT_VERSION_INFO_HPP
