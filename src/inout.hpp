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
#include <mutex>

//using statefs::BranchStorage;

namespace statefs { namespace inout {

template <typename T>
struct PropTraits
{
    PropTraits(std::string const& name, std::string const& defval)
        : name_(name), defval_(defval) {}

    std::string name_;
    std::string defval_;
};

typedef std::function<int (std::string const&)> updater_type;

template <typename T>
int read_from(T &src, char *dst, statefs_size_t len, statefs_off_t off)
{
    auto sz = src.size();
    if (off > sz)
        return 0;

    if (off + len > sz)
        len = sz - off;
    memcpy(dst, &src[off], len);
    return len;
}

class AnalogProperty
{
public:
    AnalogProperty(statefs::AProperty *, std::string const&);

    int getattr() const { return STATEFS_ATTR_READ; }
    statefs_ssize_t size() const;

    bool connect(::statefs_slot *slot) { return false; }

    int read(std::string *h, char *dst, statefs_size_t len, statefs_off_t);

    int write(std::string *h, char const *src
              , statefs_size_t len, statefs_off_t off)
    {
        return -1;
    }

    void disconnect() { }
    void release() {}

    virtual updater_type get_updater();

protected:

    int update(std::string const& v);

    statefs::AProperty *parent_;
    std::mutex m_;
    std::string v_;
};

class DiscreteProperty : public AnalogProperty
{
public:
    DiscreteProperty(statefs::AProperty *parent, std::string const &defval);
    int getattr() const;
    bool connect(::statefs_slot *slot);
    void disconnect();

    virtual updater_type get_updater();

private:

    int update(std::string const& v);

    ::statefs_slot *slot_;
};

class Src;

class Writer
{
public:
    Writer(statefs::AProperty *parent, updater_type update);

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
    updater_type update_;
    size_t size_;
};

typedef PropTraits<DiscreteProperty> Discrete;
typedef PropTraits<AnalogProperty> Analog;

class Dst : public statefs::Namespace
{
public:
    Dst(std::string const &);
    virtual ~Dst();
    virtual void release() {}
};

class Src : public statefs::Namespace
{
public:
    Src(std::string const &, std::shared_ptr<Dst>);
    virtual ~Src();

    virtual void release() {}

    template <typename T>
    void insert(PropTraits<T> const &t) 
    {
        typedef statefs::BasicPropertyOwner<T, std::string> prop_type;
        std::shared_ptr<prop_type> prop
            (new prop_type(t.name_.c_str(), t.defval_.c_str()));
        dst_->insert(std::static_pointer_cast<statefs::ANode>(prop));
        statefs::Namespace::insert
            (new statefs::BasicPropertyOwner<Writer, std::string>
             (t.name_.c_str(), prop->get_impl()->get_updater()));
    }

private:
    std::shared_ptr<Dst> dst_;
};

template <typename T>
Src& operator << (Src &ns, PropTraits<T> const &p)
{
    ns.insert(p);
    return ns;
}

// -----------------------------------------------------------------------------

inline int DiscreteProperty::getattr() const
{
    return STATEFS_ATTR_READ | STATEFS_ATTR_DISCRETE;
}

inline bool DiscreteProperty::connect(::statefs_slot *slot)
{
    slot_ = slot;
    return true;
}

inline void DiscreteProperty::disconnect()
{
    slot_ = nullptr;
}

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
