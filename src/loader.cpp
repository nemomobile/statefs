#include <statefs/loader.hpp>
#include <statefs/util.h>
#include <cor/so.hpp>

class DefaultLoader : public statefs::Loader
{
public:
    virtual ~DefaultLoader() {}

    std::shared_ptr<statefs_provider> load(std::string const& path)
    {
        std::shared_ptr<cor::SharedLib> lib
            (new cor::SharedLib(path, RTLD_LAZY));
        if (!lib->is_loaded())
            return nullptr;
        auto fn = lib->sym<statefs_provider_fn>
            (statefs_provider_accessor());
        if (!fn)
            return nullptr;

        statefs_provider *prov = fn();
        if (!fn)
            return nullptr;

        statefs::provider_ptr res(prov, [lib](statefs_provider* p) {
                if (p)
                    statefs_provider_release(p);
                if (lib)
                    lib->close();
            });
        return res;
    }

    virtual std::string name() const { return "default"; }

};

EXTERN_C statefs::Loader * create_cpp_provider_loader()
{
    return new DefaultLoader();
}
