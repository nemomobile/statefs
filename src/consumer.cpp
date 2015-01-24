/**
 * @file consumer.cpp
 * @brief Statefs consumer C++ API
 * @author (C) 2013 Jolla Ltd. Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 * @copyright LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <statefs/consumer.hpp>
#include <statefs/util.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace statefs { namespace consumer {

cor::FdHandle try_open_in_property(std::string const &name, Prefer prefer)
{
    cor::FdHandle res;
    auto try_open = [&res](std::string const &path) {
        if (path == "")
            return false;
        res = cor::FdHandle{::open(path.c_str(), O_RDONLY | O_DIRECT)};
        return res.is_valid();
    };
    switch (prefer) {
    case Prefer::User:
        if (!try_open(property_path_default(name)))
            try_open(property_path_sys_default(name));
        break;
    case Prefer::Sys:
        if (!try_open(property_path_sys_default(name)))
            try_open(property_path_default(name));
        break;
    case Prefer::OnlySys:
        try_open(property_path_sys_default(name));
        break;
    case Prefer::OnlyUser:
        try_open(property_path_default(name));
        break;
    }
    // TODO error reporting
    return res;
}

// void monitor_path(std::string const &path, receiver_type receiver)
// {
// }

// void monitor_property(std::string const &path, receiver_type receiver)
// {
// }

}}
