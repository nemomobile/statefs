#ifndef _STATEFS_LOADER_HPP_
#define _STATEFS_LOADER_HPP_
/**
 * @file loader.hpp
 * @brief Statefs provider loader C++ API
 * @author (C) 2013 Jolla Ltd. Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 * @copyright LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <statefs/provider.h>
#include <memory>

namespace statefs
{

/**
 * @defgroup loader_api Loader API
 *
 * @brief Provider loader API description. Usage is described @ref
 * loader_api "here"
 *
 *  @{
 */

typedef std::shared_ptr<statefs_provider> provider_ptr;

/* increase minor version for backward compatible loaders, major - if
 * provider logic or interface is changed in way not handled by new
 * version of the server
 */
#define STATEFS_CPP_LOADER_VERSION STATEFS_MK_VERSION(4, 0)

/**
 * Interface to be implemented by loader
 */
class Loader
{
public:
    virtual ~Loader() {}

    /**
     * called to load provider
     *
     * @param path provider path
     * @param server server notification interface for provider
     *
     * @return pointer to provider handle
     */
    virtual provider_ptr load(std::string const& path
                              , statefs_server *server) =0;

    /**
     * provider type name, e.g. "default" loader used to load
     * providers by default, e.g. "qt" can be used to load Qt-based
     * providers
     *
     * @return loader type name
     */
    virtual std::string name() const =0;

    /**
     * if loader can't be unloaded and loaded back w/o issues this
     * function should return false. Statefs expects this property can
     * change
     *
     *
     * @return false if it is unsafe to reopen loader
     */
    virtual bool is_reloadable() const =0;

    unsigned version() { return STATEFS_CPP_LOADER_VERSION; }
};

static inline char const *cpp_loader_accessor()
{
    return "create_cpp_provider_loader";
}


}

typedef statefs::Loader * (*create_provider_loader_fn)();
EXTERN_C statefs::Loader * create_cpp_provider_loader();

/** @}
 * loader api
 */

#endif //_STATEFS_LOADER_HPP_
