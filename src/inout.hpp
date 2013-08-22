#ifndef _STATEFS_MIRROR_HPP_
#define _STATEFS_MIRROR_HPP_

/*
 * Namespaces with mirrored property: data written to one property
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

#include <statefs/provider.hpp>
#include <statefs/property.hpp>
#include <mutex>

//using statefs::BranchStorage;

namespace statefs { namespace inout {

class Src;

class Writer
{
public:
    Writer(statefs::AProperty *parent, setter_type update);

    int getattr() const;
    statefs_ssize_t size() const;

    bool connect(::statefs_slot *slot);

    int read(std::string *h, char *dst, statefs_size_t len
             , statefs_off_t off);

    int write(std::string *h, char const *src
              , statefs_size_t len, statefs_off_t off);

    void disconnect() { }
    void release() {}

protected:
    statefs::AProperty *parent_;
    setter_type update_;
    size_t size_;
};

class Dst : public statefs::Namespace
{
public:
    Dst(std::string const &);
    virtual ~Dst();
    virtual void release() {}
};

class Src : public statefs::Namespace
{
    typedef statefs::BasicPropertyOwner<Writer, std::string> in_type;
public:
    Src(std::string const &, std::shared_ptr<Dst>);
    virtual ~Src();

    virtual void release() {}

    template <typename T>
    void insert_inout(PropTraits<T> const &t)
    {
        auto out = t.create();
        *dst_ << out;
        insert_input(t.name, setter(out));
    }

private:

    void insert_input(std::string const&, setter_type const&);

    std::shared_ptr<Dst> dst_;
};

template <typename T>
Src& operator << (Src &ns, PropTraits<T> const &p)
{
    ns.insert_inout(p);
    return ns;
}

// -----------------------------------------------------------------------------

inline int Writer::getattr() const
{
    return STATEFS_ATTR_WRITE;
}

inline statefs_ssize_t Writer::size() const
{
    return size_ ;
}

inline bool Writer::connect(::statefs_slot *slot)
{
    return false;
}

inline int Writer::read(std::string *h, char *dst, statefs_size_t len
                        , statefs_off_t off)
{
    return -1;
}

}}

#endif // _STATEFS_PROVIDER_HPP_
