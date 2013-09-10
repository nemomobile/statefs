#ifndef _STATEFS_PROPERTY_HPP_
#define _STATEFS_PROPERTY_HPP_

#include <statefs/provider.hpp>

#include <string>
#include <mutex>

namespace statefs {

template <typename T>
struct PropTraits
{
    typedef BasicPropertyOwner<T, std::string> handle_type;
    typedef std::shared_ptr<handle_type> handle_ptr;

    PropTraits(std::string const& prop_name
               , std::string const& prop_defval)
        : name(prop_name), defval(prop_defval) {}

    handle_ptr create() const
    {
        return std::make_shared<handle_type>(name, defval);
    }

    std::string name;
    std::string defval;
};

template <typename T>
Namespace& operator << (Namespace &ns, PropTraits<T> const &t)
{
    ns << t.create();
    return ns;
}

template <typename T, typename HandleT>
Namespace& operator <<
(Namespace &ns, std::shared_ptr<BasicPropertyOwner<T, HandleT> > const &p)
{
    ns.insert(std::static_pointer_cast<ANode>(p));
    return ns;
}

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

class AnalogProperty;
setter_type property_setter(std::shared_ptr<AnalogProperty> const &);

template <typename T>
setter_type setter(std::shared_ptr<BasicPropertyOwner<T, std::string> > const& h)
{
    return property_setter(h->get_impl());
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

protected:

    AnalogProperty(AnalogProperty const&);
    void operator =(AnalogProperty const&);

    friend setter_type property_setter(std::shared_ptr<AnalogProperty> const &);
    virtual PropertyStatus update(std::string const&);

    statefs::AProperty *parent_;
    mutable std::mutex m_;
    std::string v_;
};

class DiscreteProperty : public AnalogProperty
{
public:
    DiscreteProperty(statefs::AProperty *parent, std::string const &defval);
    int getattr() const;
    bool connect(::statefs_slot *slot);
    void disconnect();

private:

    virtual PropertyStatus update(std::string const&);

    ::statefs_slot *slot_;
};

typedef PropTraits<DiscreteProperty> Discrete;
typedef PropTraits<AnalogProperty> Analog;

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
