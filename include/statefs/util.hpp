#ifndef _STATEFS_UTIL_HPP_
#define _STATEFS_UTIL_HPP_

#include <list>
#include <string>
#include <utility>
#include <cor/util.hpp>


namespace statefs
{

std::list<std::string> property_name_parts(std::string const& name);
std::string property_path_default(std::string const&);
std::string property_path_in_default(std::string const&);

}

using cor::make_unique;

#endif // _STATEFS_UTIL_HPP_
