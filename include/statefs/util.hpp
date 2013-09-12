#ifndef _STATEFS_UTIL_HPP_
#define _STATEFS_UTIL_HPP_

#include <list>
#include <string>
#include <utility>

namespace statefs
{

std::list<std::string> property_name_parts(std::string const& name);
std::string property_path_default(std::string const&);
std::string property_path_in_default(std::string const&);

}


template <typename T, typename ... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

#endif // _STATEFS_UTIL_HPP_
