#ifndef _STATEFS_CONSUMER_HPP_
#define _STATEFS_CONSUMER_HPP_
/**
 * @file consumer.hpp
 * @brief Statefs consumer C++ API
 * @author (C) 2013 Jolla Ltd. Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 * @copyright LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <cor/util.hpp>

namespace statefs { namespace consumer {

/**
 * @defgroup consumer_api Consumer C++ API
 *
 * @brief Consumer C++ API. Usage is described @ref
 * consumer_api "here"
 *
 *  @{
 */

// enum class Flow
// {
//     Continue, Stop
// };

// enum class Status
// {
//     NotExists, Error, Ok 
// };

// typedef std::function<Flow (Status, std;:string const &)> receiver_type;

enum class Prefer {
    User, Sys, OnlyUser, OnlySys
};

cor::FdHandle try_open_in_property
(std::string const&, Prefer prefer = Prefer::User);

// void monitor_path(std::string const&, receiver_type);
// void monitor_property(std::string const&, receiver_type);


/** @}
 * consumer api
 */

}}

#endif //_STATEFS_CONSUMER_HPP_
