#ifndef _STATEFS_PROPERTY_HPP_
#define _STATEFS_PROPERTY_HPP_
/**
 * @file property.cpp
 * @brief Simple implementation of statefs-pp properties
 *
 * @author (C) 2012, 2013 Jolla Ltd. Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 * @copyright LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <statefs/provider.hpp>

#include <string>
#include <mutex>

namespace statefs {



enum PropertyStatus {
    PropertyUpdated,
    PropertyUnchanged
};

typedef std::function<PropertyStatus (std::string const&)> setter_type;

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

class DiscreteProperty;
setter_type property_setter(std::shared_ptr<DiscreteProperty> const &);

template <typename T>
setter_type setter(std::shared_ptr<BasicPropertyOwner<T, std::string> > const& h)
{
    return property_setter(h->get_impl());
}

class PropertySource
{
public:
    virtual ~PropertySource() {}
    virtual statefs_ssize_t size() const =0;
    virtual std::string read() const =0;
};

class DefaultSource : public PropertySource
{
public:
    DefaultSource(std::string const &value)
        : value_(value)
    {}

    virtual statefs_ssize_t size() const
    {
        return value_.size();
    }

    virtual std::string read() const
    {
        return value_;
    }
private:
    std::string value_;
};

class AnalogProperty
{
public:
    AnalogProperty(statefs::AProperty *, std::unique_ptr<PropertySource>);

    void set_source(std::unique_ptr<PropertySource> s)
    {
        source_ = std::move(s);
    }

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

protected:

    AnalogProperty(AnalogProperty const&);
    void operator =(AnalogProperty const&);

    statefs::AProperty *parent_;
    mutable std::mutex m_;
    std::unique_ptr<PropertySource> source_;
};

class DiscreteProperty
{
public:
    DiscreteProperty(statefs::AProperty *parent, std::string const &defval);
    DiscreteProperty(DiscreteProperty const&) =delete;
    void operator =(DiscreteProperty const&) =delete;

    int getattr() const;
    statefs_ssize_t size() const;
    bool connect(::statefs_slot *);
    int read(std::string *, char *, statefs_size_t, statefs_off_t);

    int write(std::string *, char const *, statefs_size_t, statefs_off_t)
    {
        return -1;
    }

    void release() {}
    void disconnect();

private:

    friend setter_type property_setter(std::shared_ptr<DiscreteProperty> const &);
    PropertyStatus update(std::string const&);

    statefs::AProperty *parent_;
    mutable std::mutex m_;
    std::string v_;

    ::statefs_slot *slot_;
};

template <typename T>
struct PropTraits
{
    typedef BasicPropertyOwner<T, std::string> handle_type;
    typedef std::shared_ptr<handle_type> handle_ptr;

    PropTraits(std::string const& prop_name
                   , std::string const& prop_defval)
        : name(prop_name), defval(prop_defval) {}

    std::string name;
    std::string defval;
};

typedef PropTraits<DiscreteProperty> Discrete;
typedef PropTraits<AnalogProperty> Analog;


static inline Discrete::handle_ptr create(Discrete const &t)
{
    typedef Discrete::handle_type h_type;
    return std::make_shared<h_type>(t.name, t.defval);
    
}

template <typename SourceT>
static inline Analog::handle_ptr create
(Analog const &t, std::unique_ptr<SourceT> src)
{
    typedef Analog::handle_type h_type;
    return std::make_shared<h_type>(t.name, std::move(src));
}

static inline Analog::handle_ptr create(Analog const &t)
{
    return create(t, cor::make_unique<DefaultSource>(t.defval));
}

template <typename T>
Namespace& operator << (Namespace &ns, PropTraits<T> const &t)
{
    ns << create(t);
    return ns;
}

template <typename T, typename HandleT>
Namespace& operator <<
(Namespace &ns, std::shared_ptr<BasicPropertyOwner<T, HandleT> > const &p)
{
    ns.insert(std::static_pointer_cast<ANode>(p));
    return ns;
}

class BasicWriter
{
public:
    BasicWriter(statefs::AProperty *parent, setter_type update);

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


// -----------------------------------------------------------------------------

inline int DiscreteProperty::getattr() const
{
    return STATEFS_ATTR_READ | STATEFS_ATTR_DISCRETE;
}

inline int BasicWriter::getattr() const
{
    return STATEFS_ATTR_WRITE;
}

inline statefs_ssize_t BasicWriter::size() const
{
    return size_ ;
}

inline bool BasicWriter::connect(::statefs_slot *slot)
{
    return false;
}

inline int BasicWriter::read
(std::string *h, char *dst, statefs_size_t len, statefs_off_t off)
{
    return -1;
}

}

#endif // _STATEFS_PROPERTY_HPP_
