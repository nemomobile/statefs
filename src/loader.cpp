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
        std::shared_ptr<cor::SharedLib> lib
            (new cor::SharedLib(path, RTLD_LAZY));
        if (!lib->is_loaded())
            return nullptr;
        auto fn = lib->sym<statefs_provider_fn>
            (statefs_provider_accessor());
        if (!fn)
            return nullptr;

        statefs::provider_ptr res
            (fn(server), [lib](statefs_provider* p) mutable {
                if (p)
                    statefs_provider_release(p);
                lib.reset();
            });

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
