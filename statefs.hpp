#ifndef _STATEFS_PRIVATE_HPP_
#define _STATEFS_PRIVATE_HPP_

#include <ostream>
#include <tuple>

std::tuple<int, std::string>
dump_plugin_meta(std::ostream &dst, std::string const &path);

#endif // _STATEFS_PRIVATE_HPP_
