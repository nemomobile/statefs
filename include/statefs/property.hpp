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
#include <array>

namespace statefs {


enum class PropertyStatus {
    Updated,
    Unchanged
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

/// constant string value source
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

    bool connect(::statefs_slot *) { return false; }

    int read(std::string *, char *, statefs_size_t, statefs_off_t);

    int write(std::string *, char const *, statefs_size_t, statefs_off_t)
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
    // discrete property can be concurrently accessed by setter
    // updating it and statefs, so needs protection
    mutable std::mutex m_;
    std::string v_;

    ::statefs_slot *slot_;
};

class BasicWriterImpl
{
public:
    BasicWriterImpl(setter_type update);

    int write(std::string *h, char const *src
              , statefs_size_t len, statefs_off_t off);

protected:
    setter_type update_;
    size_t size_;
};

/**
 * Wrapper for readable property implementation BaseT, forwarding all
 * read operations to it
 */
template <typename BaseT>
class RWProperty : public BasicWriterImpl, public BaseT
{
public:
    typedef BaseT reader_type;

    template <typename ... Args>
    RWProperty(statefs::AProperty *parent, setter_type setter, Args &&...args)
        : BasicWriterImpl(setter)
        , reader_type(parent, std::forward<Args>(args)...)
    {}

    RWProperty(RWProperty const&) = delete;
    void operator = (RWProperty const&) = delete;

    int getattr() const {
        return reader_type::getattr() | STATEFS_ATTR_WRITE;
    }
    statefs_ssize_t size() const { return reader_type::size(); }
    bool connect(::statefs_slot *s) { return reader_type::connect(s); }
    int read(std::string *str, char *b, statefs_size_t sz, statefs_off_t o)
    {
        return reader_type::read(str, b, sz, o);
    }

    int write(std::string *h, char const *src, statefs_size_t len
              , statefs_off_t off)
    {
        return BasicWriterImpl::write(h, src, len, off);
    }

    void release() { reader_type::release(); }
    void disconnect() { reader_type::disconnect(); }
};


/**
 * @defgroup property_traits Property traits
 *
 * @brief Property traits to construct basic properties easier
 *
 *  @{
 */

/**
 * Traits to construct property implementation with std::string as the
 * data storage
 *
 * Set of inline create() functions is used to create
 * property node implementation corresponding to T
 */
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

typedef PropTraits<RWProperty<DiscreteProperty> > DiscreteWritable;
typedef PropTraits<RWProperty<AnalogProperty> > AnalogWriteable;

static inline Discrete::handle_ptr create(Discrete const &t)
{
    typedef Discrete::handle_type h_type;
    return std::make_shared<h_type>(t.name, t.defval);

}

static inline DiscreteWritable::handle_ptr create
(DiscreteWritable const &t, setter_type setter)
{
    typedef DiscreteWritable::handle_type h_type;
    return std::make_shared<h_type>(t.name, setter, t.defval);

}

template <typename SourceT>
static inline Analog::handle_ptr create
(Analog const &t, std::unique_ptr<SourceT> src)
{
    typedef Analog::handle_type h_type;
    return std::make_shared<h_type>(t.name, std::move(src));
}

template <typename SourceT>
static inline AnalogWriteable::handle_ptr create
(AnalogWriteable const &t, std::unique_ptr<SourceT> src, setter_type setter)
{
    typedef AnalogWriteable::handle_type h_type;
    return std::make_shared<h_type>(t.name, setter, std::move(src));
}

static inline Analog::handle_ptr create(Analog const &t)
{
    return create(t, cor::make_unique<DefaultSource>(t.defval));
}

static inline AnalogWriteable::handle_ptr create
(AnalogWriteable const &t, setter_type setter)
{
    return create(t, cor::make_unique<DefaultSource>(t.defval), setter);
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

/** @}
 * property traits
 */

class BasicWriter : public BasicWriterImpl
{
public:
    BasicWriter(statefs::AProperty *parent, setter_type update)
        : BasicWriterImpl(update), parent_(parent)
    {}

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
};


// -----------------------------------------------------------------------------

inline int BasicWriter::write
(std::string *h, char const *src, statefs_size_t len, statefs_off_t off)
{
    return BasicWriterImpl::write(h, src, len, off);
}

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

inline bool BasicWriter::connect(::statefs_slot *)
{
    return false;
}

inline int BasicWriter::read
(std::string *, char *, statefs_size_t, statefs_off_t)
{
    return -1;
}

// -----------------------------------------------------------------------------


enum class PropType { Analog, Discrete };

template <typename T>
T attr(char const *v);

template <>
inline std::string attr<std::string>(char const *v)
{
    return cor::str(v, "");
}

template <>
inline long attr<long>(char const *v)
{
    return atoi(cor::str(v, "0").c_str());
}

template <>
inline bool attr<bool>(char const *v)
{
    return attr<long>(v);
}

template <typename T>
static inline std::string statefs_attr(T const &v)
{
    return std::to_string(v);
}

static inline std::string statefs_attr(bool v)
{
    return std::to_string(v ? 1 : 0);
}

/// Using functor returning std::string as the source
class FunctionSource : public PropertySource
{
public:
    typedef std::function<std::string()> source_type;

    FunctionSource(source_type const &src) : src_(src) {}

    virtual statefs_ssize_t size() const
    {
        return src_().size();
    }

    virtual std::string read() const
    {
        return src_();
    }

private:
    source_type src_;
};


static inline PropertyStatus analog_throw_on_set(std::string const &)
{
    throw cor::Error("Analog property can't be set");
    return statefs::PropertyStatus::Unchanged;
}


// -----------------------------------------------------------------------------

/// <name, default value, property type>
typedef std::tuple<char const*, char const*, PropType> property_info_type;

/**
 * Universal implementation of namespace. Properties are enumerated in
 * enum class PropId should end with EOE item. Also user should
 * instantiate static const BasicNamespace<PropId>::property_info
 */
template <typename PropId>
class BasicNamespace : public Namespace
{
public:
    BasicNamespace(char const *name)
        : Namespace(name)
    {
        for (size_t i = 0; i < prop_count; ++i) {
            char const *name;
            char const *defval;
            PropType ptype;
            std::tie(name, defval, ptype) = property_info[i];
            if (ptype == PropType::Discrete) {
                auto prop = create(Discrete{name, defval});
                setters_[i] = setter(prop);
                *this << prop;
            } else {
                auto const &info = analog_info_[static_cast<PropId>(i)];
                auto src = cor::make_unique<FunctionSource>(info);
                setters_[i] = analog_throw_on_set;
                *this << create(std::move(Analog{name, defval}), std::move(src));
            }
        }
    }

    void set(PropId id, std::string const &v)
    {
        setters_[static_cast<size_t>(id)](v);
    }

protected:
    static const size_t prop_count = static_cast<size_t>(PropId::EOE);
    typedef std::array<property_info_type, prop_count> info_type;
    typedef std::map<PropId, FunctionSource::source_type> analog_info_type;

    static const info_type property_info;

    analog_info_type analog_info_;
    std::array<setter_type, prop_count> setters_;
};

}

#endif // _STATEFS_PROPERTY_HPP_
