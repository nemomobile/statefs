/*
 * Support for piping of the properties: data written to one property
 * appears as content of the property with the same name on the other
 * end
 *
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include "inout.hpp"
#include <cor/util.hpp>
#include <statefs/config.hpp>
#include <statefs/loader.hpp>
#include <algorithm>

namespace statefs { namespace inout {

Dst::Dst(std::string const &name) : Namespace(name.c_str()) {}

Dst::~Dst() {}

Src::Src(std::string const &name, std::shared_ptr<Dst> p)
    : Namespace(name.c_str()), dst_(p)
{}

Src::~Src() {}

void Src::insert_input(std::string const &name, setter_type const &setter)
{
    *this << std::make_shared<in_type>(name, setter);
}

// -----------------------------------------------------------------------------

// template <template<class> class T>
// std::string join(T<std::string> const &range)
// {
//     size_t s = 0;
//     for (auto const &v : range)
//         s += v.size();
//     std::string res;
//     res.reserve(s);
//     for (auto const &v : range)
//         res.append(v);
//     return res;
// }

class Provider : public statefs::AProvider
{
public:
    Provider(std::shared_ptr<config::Plugin> info, statefs_server *server)
        : AProvider(info->value().c_str(), server)
    {
        for (auto const &ns : info->namespaces_) {
            std::string out_name = ns->value();
            std::string in_name = cor::concat("@", out_name);
            auto dst = std::make_shared<Dst>(out_name);
            insert(std::static_pointer_cast<statefs::ANode>(dst));
                
            auto src = std::make_shared<Src>(in_name, dst);
            insert(std::static_pointer_cast<statefs::ANode>(src));
            for (auto const &prop : ns->props_)
                if (prop->access() & config::Property::Subscribe)
                    *src << Discrete(prop->value(), prop->defval());
                else
                    *src << Analog(prop->value(), prop->defval());
        }
    }
    virtual ~Provider() {}

    virtual void release() {
        delete this;
    }
};

class Loader : public statefs::Loader
{
public:
    virtual ~Loader() {}

    std::shared_ptr<statefs_provider> load
    (std::string const& path, statefs_server *server)
    {
        std::shared_ptr<statefs_provider> res;
        auto add = [&](std::string const &name
                       , std::shared_ptr<config::Library> p) {
            auto provider = std::dynamic_pointer_cast<config::Plugin>(p);
            if (!provider)
                throw cor::Error("Not provider: %s", name.c_str());
            res = std::static_pointer_cast<statefs_provider>
            (std::make_shared<Provider>(provider, server));
        };
        config::from_file(path, add);
        return res;
    }

    virtual std::string name() const { return "inout"; }

    virtual bool is_reloadable() const { return true; }
};

}}

EXTERN_C statefs::Loader * create_cpp_provider_loader()
{
    return new statefs::inout::Loader();
}
