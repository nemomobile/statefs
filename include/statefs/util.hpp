#ifndef _STATEFS_UTIL_HPP_
#define _STATEFS_UTIL_HPP_
/**
 * @file util.cpp
 * @brief Statefs utility functions for providers and clients
 *
 * @author (C) 2012, 2013 Jolla Ltd. Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 * @copyright LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <list>
#include <string>
#include <utility>
#include <cor/util.hpp>


namespace statefs
{

std::list<std::string> property_name_parts(std::string const& name);
std::string property_path_default(std::string const&);
std::string property_path_sys_default(std::string const&);
std::string property_path_in_default(std::string const&);

}

using cor::make_unique;

#endif // _STATEFS_UTIL_HPP_
