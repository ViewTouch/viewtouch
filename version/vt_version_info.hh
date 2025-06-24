#pragma once  // REFACTOR: Replaced #ifndef VT_VERSION_INFO_HPP guard with modern pragma once

#include <string>

namespace viewtouch
{

std::string get_project_name();
std::string get_version_short();
std::string get_version_extended();
std::string get_version_long();
std::string get_version_info();
std::string get_version_timestamp();

} // end namespace viewtouch
