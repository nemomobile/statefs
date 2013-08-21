#ifndef _STATEFS_PROPERTY_HPP_
#define _STATEFS_PROPERTY_HPP_

#include <statefs/provider.hpp>

#include <string>
#include <mutex>

namespace statefs {

template <typename T>
struct PropTraits
{
    PropTraits(std::string const& name, std::string const& defval)
        : name_(name), defval_(defval) {}

    std::string name_;
    std::string defval_;
};

template <typename T>
Namespace& operator << (Namespace &ns, PropTraits<T> const &t)
{
    typedef statefs::BasicPropertyOwner<T, std::string> prop_type;
    auto prop = std::make_shared<prop_type>
        (t.name_.c_str(), t.defval_.c_str());
    ns.insert(std::static_pointer_cast<ANode>(prop));
    return ns;
}


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

typedef PropTraits<DiscreteProperty> Discrete;
typedef PropTraits<AnalogProperty> Analog;

}

#endif // _STATEFS_PROPERTY_HPP_
