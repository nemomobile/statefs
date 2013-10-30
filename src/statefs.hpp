#ifndef _STATEFS_PRIVATE_HPP_
#define _STATEFS_PRIVATE_HPP_
/**
 * @file statefs.hpp
 * @brief Statefs server private header
 *
 * @author (C) 2012, 2013 Jolla Ltd. Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 * @copyright LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <statefs/config.hpp>

#include <statefs/provider.h>
#include <statefs/loader.hpp>
#include <cor/so.hpp>

// TMP for make_unique
#include <statefs/util.hpp>

#include <ostream>
#include <tuple>
#include <memory>
#include <map>

bool ensure_dir_exists(std::string const &);

namespace statefs {

typedef std::unique_ptr
<statefs_provider, void (*)(statefs_provider*)> provider_handle_type;
typedef std::unique_ptr
<statefs_namespace, void (*)(statefs_namespace*)> ns_handle_type;
typedef std::unique_ptr
<statefs_property, void (*)(statefs_property*)> property_handle_type;

ns_handle_type mk_namespace_handle(statefs_namespace *ns);
property_handle_type mk_property_handle(statefs_property *p);

namespace server {

std::tuple<int, std::string>
dump_plugin_meta(std::ostream &dst, std::string const &path);


class LoaderProxy
{
    typedef std::unique_ptr<statefs::Loader> impl_ptr;
public:
    LoaderProxy(std::string const&);

    LoaderProxy(LoaderProxy const&) =delete;
    LoaderProxy& operator =(LoaderProxy const&) =delete;

    bool is_valid() const;
    statefs::provider_ptr load(std::string const&, statefs_server*);
    bool is_reloadable() const;
    std::string name() const;
private:

    static std::string loader_path(std::string const&);
    static impl_ptr create(cor::SharedLib &);
    cor::SharedLib lib_;
    impl_ptr impl_;
};

class LoadersStorage
{
public:
    std::shared_ptr<LoaderProxy> loader_get(std::string const&);
    typedef std::shared_ptr<statefs::config::Loader> loader_info_ptr;

    bool loader_register(loader_info_ptr p);
    bool loader_rm(std::string const&);

private:

    std::map<std::string, std::shared_ptr<LoaderProxy> > loaders_;
    std::map<std::string, loader_info_ptr> info_;
};

}}

#endif // _STATEFS_PRIVATE_HPP_
