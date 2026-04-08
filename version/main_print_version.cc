#include "version/vt_version_info.hh"

// standard libraries
#include <iostream>

int main(int, const char **)
{
    std::cout << viewtouch::get_project_name() << " "
              << viewtouch::get_version_short()
              << '\n';
    std::cout << viewtouch::get_project_name() << " "
              << viewtouch::get_version_long()
              << '\n';
    std::cout << viewtouch::get_project_name() << " "
              << viewtouch::get_version_info()
              << '\n';
    return 0;
}
