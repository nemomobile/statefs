#include "statefs.hpp"

#include <boost/filesystem.hpp>
#include <iostream>
#include <string>

bool ensure_dir_exists(std::string const &dir_name)
{
    namespace fs = boost::filesystem;
    if (fs::exists(dir_name)) {
        if (!fs::is_directory(dir_name)) {
            std::cerr << dir_name << " should be directory" << std::endl;
            return false;
        }
    } else {
        if (!fs::create_directory(dir_name)) {
            std::cerr << "Can't create dir " << dir_name << std::endl;
            return false;
        }
    }
    return true;
}

