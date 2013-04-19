#include "statefs.hpp"
#include <statefs/util.h>

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

provider_handle_type mk_provider_handle(cor::SharedLib &lib)
{
    static auto deleter = [](statefs_provider *p) {
        statefs_provider_release(p);
    };
    static const char *sym_name = "statefs_provider_get";

    if (!lib.is_loaded()) {
        std::cerr << "Lib loading error " << ::dlerror() << std::endl;
        return provider_handle_type(nullptr, deleter);
    }
    auto fn = lib.sym<statefs_provider_fn>(sym_name);
    if (!fn) {
        std::cerr << "Can't resolve " << sym_name << std::endl;
        return provider_handle_type(nullptr, deleter);
    }

    auto res = provider_handle_type(fn(), deleter);
    if (res && !statefs_is_compatible(res.get())) {
        std::cerr << "Incompatible provider version\n";
        return provider_handle_type(nullptr, deleter);
    }
    return res;
}

ns_handle_type mk_namespace_handle(statefs_namespace *ns)
{
    auto ns_release = [](statefs_namespace *p)
    {
        if (p)
            statefs_node_release(&p->node);
    };
    return ns_handle_type(ns, ns_release);
}

property_handle_type mk_property_handle(statefs_property *p)
{
    auto release = [](statefs_property *p)
    {
        if (p)
            statefs_node_release(&p->node);
    };
    return property_handle_type(p, release);
}
