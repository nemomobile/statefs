/**
 * @file common_util.cpp
 * @brief Miscellaneous Statefs utilities for c++ apps
 * @author (C) 2013 Jolla Ltd. Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 * @copyright LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

//#include <statefs/util.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
//#include <algorithm>
#include <string>
#include <list>

namespace statefs
{

using boost::algorithm::join;

template <class DstT>
void split(std::string const& src
           , std::string const &symbols
           , DstT &dst)
{
    using boost::algorithm::split;
    split(dst, src, [&symbols](char c) {
            return strchr(symbols.c_str(), c) != nullptr;
        });
}

/**
 * split full property name (dot- or slash-separated) to statefs path
 * parts relative to the namespace/provider root directory
 * returns list because relies on c++11 move ctor
 *
 * @param name full property name (including all namespaces/domains)
 *
 * @return list with path parts
 */
std::list<std::string> property_name_parts(std::string const& name)
{
    std::list<std::string> res;
    split(name, "./", res);
    if (res.size() > 2) {
        auto fname = res.back();
        res.pop_back();
        if (!res.front().size()) // for names like "/..."
            res.pop_front();
        auto ns = join(res, "_");
        res.clear();
        res.push_back(ns);
        res.push_back(fname);
    }
    return res;
}

static inline bool is_property_path_valid(std::list<std::string> const &parts)
{
    return (parts.size() == 2
            && parts.front().size() > 0
            && parts.back().size() > 0);
}

/**
 * get path to the statefs property file for the statefs instance
 * mounted to the default statefs root
 *
 * @param name dot- or slash-separated full property name
 *
 * @return full path to the property file
 */
std::string property_path_default(std::string const& name)
{
    auto parts = property_name_parts(name);
    if (!is_property_path_valid(parts))
        return "";

    parts.push_front("namespaces");
    parts.push_front("state");
    parts.push_front(::getenv("XDG_RUNTIME_DIR")); // TODO hardcoded source!
    return join(parts, "/");
}

static const std::string in_prop_prefix = "@";

/**
 * get path to the statefs property inout_loader input file for
 * the statefs instance mounted to the default statefs root
 *
 * @param name
 *
 * @return
 */
std::string property_path_in_default(std::string const& name)
{
    std::initializer_list<std::string> data{in_prop_prefix, name};
    return property_path_default(join(data, ""));
}

}
