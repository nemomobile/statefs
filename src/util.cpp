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

using statefs::provider_ptr;

Loader::Loader(std::string const& path)
    : lib_(path, RTLD_LAZY)
    , impl_(std::move(create(lib_)))
{
}

provider_ptr Loader::load(std::string const& path)
{
    return impl_ ? impl_->load(path) : nullptr;
}

std::string Loader::name() const
{
    return impl_ ? impl_->name() : "";
}

bool Loader::is_valid() const
{
    return !!impl_;
}

Loader::impl_ptr Loader::create(cor::SharedLib &lib)
{
    if (!lib.is_loaded()) {
        std::cerr << "Lib loading error " << ::dlerror() << std::endl;
        return nullptr;
    }
    auto fn = lib.sym<create_provider_loader_fn>
        (statefs::cpp_loader_accessor());
    if (!fn) {
        trace() << "Can't resolve "
                  << statefs::cpp_loader_accessor() << std::endl;
        return nullptr;
    }

    auto loader = fn();
    if (!loader) {
        std::cerr << "provider is null" << std::endl;
    } else if (!statefs_is_version_compatible(loader->version())) {
        std::cerr << "statefs: Incompatible loader version "
                  << loader->version() << " vs " << STATEFS_CURRENT_VERSION;
        return nullptr;
    }
    return impl_ptr(loader);
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

void LoadersStorage::loader_register(loader_info_ptr p)
{
    info_.insert(std::make_pair(p->value(), p));
}

std::weak_ptr<Loader> LoadersStorage::loader_get(std::string const& name)
{
    auto it = loaders_.find(name);
    if (it == loaders_.end()) {
        auto pinfo = info_.find(name);
        if (pinfo == info_.end())
            return std::weak_ptr<Loader>();

        auto path = pinfo->second->path;
        std::shared_ptr<Loader> loader(new Loader(path));
        auto added = loaders_.insert(std::make_pair(name, loader));
        return (added.first)->second;
    };
    return it->second;
}

void LoadersStorage::loader_rm(std::string const& name)
{
    loaders_.erase(name);
    info_.erase(name);
}

