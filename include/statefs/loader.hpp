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
     *
     * @return pointer to provider handle
     */
    virtual provider_ptr load(std::string const& path) =0;

    /** 
     * provider type name, e.g. "default" loader used to load
     * providers by default, e.g. "qt" can be used to load Qt-based
     * providers
     *
     * @return loader type name
     */
    virtual std::string name() const =0;

    unsigned version() { return STATEFS_CURRENT_VERSION; }
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

