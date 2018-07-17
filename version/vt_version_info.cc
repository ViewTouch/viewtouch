#include "version/vt_version_info.hh"
#include "version_generated.hh"

// load versions from header files
//#include <boost/version.hpp>
//#include <suitesparse/SuiteSparse_config.h>
//#include <ceres/version.h>
//#include <Eigen/Core>

#include <sstream>

using namespace viewtouch;

namespace
{

std::string get_library_string()
{
    return std::string(PROJECT);
}

std::string get_version_string()
{
    std::ostringstream msg;
    msg << VERSION_FULL
        << " (";
    if (std::string(REVISION) != "")
    {
        msg << REVISION << ", ";
    }
    msg << COMPILER_TAG << ", "
        << CMAKE_TIMESTAMP
        << ")";

    return msg.str();
}

//std::string get_boost_version()
//{
//    std::ostringstream msg;
//    msg << "Built with Boost "
//        << BOOST_VERSION / 100000 << "."     //major version
//        << BOOST_VERSION / 100 % 1000 << "." //minor version
//        << BOOST_VERSION % 100 << ".";       //patch level
//    return msg.str();
//}
//
//std::string get_suitesparse_version()
//{
//    std::ostringstream msg;
//    msg << "Built with SuiteSparse "
//        << SUITESPARSE_MAIN_VERSION << "."
//        << SUITESPARSE_SUB_VERSION << "."
//        << SUITESPARSE_SUBSUB_VERSION << ".";
//    return msg.str();
//}
//
//std::string get_ceres_version()
//{
//    std::ostringstream msg;
//    msg << "Built with Ceres "
//        << CERES_VERSION_STRING << ".";
//    return msg.str();
//}
//
//std::string get_eigen_version()
//{
//    std::ostringstream msg;
//    msg << "Built with Eigen " <<
//           EIGEN_WORLD_VERSION << "." <<
//           EIGEN_MAJOR_VERSION << "." <<
//           EIGEN_MINOR_VERSION << ". ";
//    msg << "Eigen uses "
//#ifndef EIGEN_VECTORIZE
//        << "no vectorization.";
//#else
//        << Eigen::SimdInstructionSetsInUse() << ".";
//#endif //EIGEN_VECTORIZE
//    return msg.str();
//}

} // loacl namespace


std::string viewtouch::get_project_name()
{
    static const std::string library_name = get_library_string();
    return library_name;
}

std::string viewtouch::get_version_short()
{
    return viewtouch::VERSION;
}
std::string viewtouch::get_version_long()
{
    return viewtouch::VERSION_FULL;
}

std::string viewtouch::get_version_info()
{
    static const std::string version_info = get_version_string() + "\n"
            //"- " + get_boost_version() + "\n"
            //"- " + get_suitesparse_version() + "\n"
            //"- " + get_ceres_version() + "\n"
            //"- " + get_eigen_version()
            ;

    return version_info;
}
