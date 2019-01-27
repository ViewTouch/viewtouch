#include "version/vt_version_info.hh"

// standard libraries
#include <iostream>

int main(int, const char **)
{
    std::cout << viewtouch::get_project_name() << " "
              << viewtouch::get_version_short()
              << std::endl;
    std::cout << viewtouch::get_project_name() << " "
              << viewtouch::get_version_long()
              << std::endl;
    std::cout << viewtouch::get_project_name() << " "
              << viewtouch::get_version_info()
              << std::endl;
    return 0;
}
