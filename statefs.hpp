#ifndef _STATEFS_PRIVATE_HPP_
#define _STATEFS_PRIVATE_HPP_

#include <statefs/provider.h>
#include <cor/so.hpp>

#include <ostream>
#include <tuple>
#include <memory>

std::tuple<int, std::string>
dump_plugin_meta(std::ostream &dst, std::string const &path);
bool ensure_dir_exists(std::string const &);

typedef std::unique_ptr
<statefs_provider, void (*)(statefs_provider*)> provider_handle_type;
typedef std::unique_ptr
<statefs_namespace, void (*)(statefs_namespace*)> ns_handle_type;
typedef std::unique_ptr
<statefs_property, void (*)(statefs_property*)> property_handle_type;

provider_handle_type mk_provider_handle(cor::SharedLib &lib);
ns_handle_type mk_namespace_handle(statefs_namespace *ns);
property_handle_type mk_property_handle(statefs_property *p);


#endif // _STATEFS_PRIVATE_HPP_
