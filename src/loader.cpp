/**
 * @file loader.cpp
 * @brief Default statefs loader
 *
 * Default loader can load any statefs provider supporting statefs
 * provider API if this provider can be run/started from fuse thread
 *
 * @author (C) 2012, 2013 Jolla Ltd. Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 * @copyright LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <statefs/loader.hpp>
#include <statefs/util.h>
#include <cor/so.hpp>
#include <iostream>

class DefaultLoader : public statefs::Loader
{
public:
    virtual ~DefaultLoader() {}

    std::shared_ptr<statefs_provider> load(std::string const& path
                                           , statefs_server *server)
    {
        auto lib = std::make_shared<cor::SharedLib>(path, RTLD_LAZY);
        if (!lib->is_loaded())
            return nullptr;
        auto fn = lib->sym<statefs_provider_fn>(statefs_provider_accessor());
        if (!fn)
            return nullptr;

        auto dtor = [lib, path](statefs_provider* p) mutable {
            std::cerr << "DTOR:" << path << std::endl;
            if (p)
                statefs_provider_release(p);
            lib.reset();
        };

        statefs::provider_ptr res{fn(server), dtor};

        if (!res)
            return nullptr;

        if (!statefs_is_compatible(STATEFS_CURRENT_VERSION, res.get())) {
            std::cerr << "statefs: Incompatible provider version "
                      << res->version << " vs "
                      << STATEFS_CURRENT_VERSION;
            return nullptr;
        }


        return res;
    }

    virtual std::string name() const { return "default"; }

    virtual bool is_reloadable() const { return true; }
};

EXTERN_C statefs::Loader * create_cpp_provider_loader()
{
    return new DefaultLoader();
}
