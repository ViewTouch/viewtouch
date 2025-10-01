#include "version/vt_version_info.hh"
#include "version_generated.hh"

#include <sstream>
#include <string_view>

using namespace viewtouch;

namespace
{
    // Build version string at compile-time initialization
    std::string build_version_string() noexcept
    {
        std::ostringstream msg;
        msg << VERSION_FULL << " (";
        
        if (std::string_view(REVISION).empty() == false)
        {
            msg << REVISION << ", ";
        }
        
        msg << COMPILER_TAG << ", " << CMAKE_TIMESTAMP << ")";
        return msg.str();
    }

} // anonymous namespace

namespace viewtouch
{
    const std::string& get_project_name() noexcept
    {
        static const std::string library_name{PROJECT};
        return library_name;
    }

    const std::string& get_version_short() noexcept
    {
        static const std::string version{VERSION};
        return version;
    }

    const std::string& get_version_extended() noexcept
    {
        static const std::string version_ext{VERSION_EXT};
        return version_ext;
    }

    const std::string& get_version_long() noexcept
    {
        static const std::string version_full{VERSION_FULL};
        return version_full;
    }

    const std::string& get_version_info() noexcept
    {
        static const std::string version_info = build_version_string() + "\n";
        return version_info;
    }

    const std::string& get_version_timestamp() noexcept
    {
        static const std::string timestamp{CMAKE_TIMESTAMP};
        return timestamp;
    }

} // namespace viewtouch
