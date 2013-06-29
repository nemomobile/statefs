#include <iostream>
#include <statefs/provider.hpp>
#include <chrono>
#include <future>

/** @defgroup statefspp_example Example C++ provider
 *
 * @brief Example C++ provider - uses statefspp framework
 *
 * Provider is still returned by statefs_provider_get() function.
 *
 * Provider class should inherit statefs::AProvider (see
 * Provider). Namespace inherits statefs::Namespace (like NsChild and
 * NsTime).
 *
 * Properties can be implemented in different ways, basically property
 * should also inherit statefs::AProperty and implement its virtual
 * methods. The most simple way is to just specialize BasicPropertyOwner
 * template with:
 *
 * - implementation class, like Stream and Seconds and
 *
 * - handle class, providing buffer to be used for each opened file
     handle. In this example std::string is used
 *
 */

/** \addtogroup statefs_example
 *  @{
 */

template <typename T>
int read_from(T &src, char *dst, size_t len, off_t off)
{
    auto sz = src.size();
    if (off > sz)
        return 0;

    if (off + len > sz)
        len = sz - off;
    memcpy(dst, &src[off], len);
    return len;
}

/// Basic continuous (non-pollable, analog) property implementation
class Stream
{
public:
    Stream(statefs::AProperty *parent)
        : parent_(parent), v_("e"), slot_(nullptr) {}

    int getattr() const { return STATEFS_ATTR_READ; }
    ssize_t size() const { return 128 ; }

    bool connect(::statefs_slot *slot) {
        slot_ = slot;
        return true;
    }

    int read(std::string *h, char *dst, size_t len, off_t off)
    {
        update();
        auto &v = *h;
        if (!off)
            v = v_;

        return read_from(v, dst, len, off);
    }

    int write(std::string *h, char const *src, size_t len, off_t off)
    {
        return -1;
    }

    void disconnect() { slot_ = nullptr; }
    void release() {}

private:

    void update() {
        std::stringstream s;
        s << rand();
        v_ = std::move(s.str());
    }
    statefs::AProperty *parent_;
    std::string v_;
    ::statefs_slot *slot_;
};

/// Basic namespace example, it contains single property
class NsChild : public statefs::Namespace
{
public:
    NsChild(char const *name) : Namespace(name)
    {
        insert(new statefs::BasicPropertyOwner
               <Stream, std::string>("amount"));
    }

    virtual ~NsChild() {}
    virtual void release() {}
};

/// Discrete (pollable) property implementation example
class Seconds
{
public:
    Seconds(statefs::AProperty *parent)
        : update_interval_usec_(1000000)
        , parent_(parent)
        , v_(calc())
        , slot_(nullptr) {}

    int getattr() const {
        return STATEFS_ATTR_READ
            | STATEFS_ATTR_DISCRETE
            | STATEFS_ATTR_WRITE;
    }
    ssize_t size() const { return 128 ; }

    bool connect(::statefs_slot *slot) {
        slot_ = slot;
        std::cerr << "connect to seconds\n";
        periodic_update();
        return true;
    }

    int read(std::string *h, char *dst, size_t len, off_t off)
    {
        if (!slot_)
            v_ = calc();

        auto &v = *h;
        if (!off)
            v = v_;

        return read_from(v, dst, len, off);
    }

    int write(std::string *h, char const *src, size_t len, off_t off)
    {
        update_interval_usec_ = stoi(std::string(src, len));
        return len;
    }

    void disconnect() { slot_ = nullptr; }
    void release() {}

private:

    std::string calc()
    {
        auto i = ::time(nullptr);
        std::stringstream ss; ss << i;
        return ss.str();
    }

    void periodic_update() {
        v_ = calc();
        if (slot_) {
            std::cerr << "periodic\n";
            std::thread([this]() {
                    std::cerr << "next\n";
                    ::usleep(update_interval_usec_);
                    auto slot = slot_;
                    if (slot_)
                        slot->on_changed(slot_, parent_);
                    periodic_update();
                }).detach();
        }
    }
    unsigned update_interval_usec_;
    statefs::AProperty *parent_;
    std::string v_;
    ::statefs_slot *slot_;
};

/// Another basic namespace example, representing namespace for
/// "seconds" property, implemented in Seconds class
class NsTime : public statefs::Namespace
{
public:
    NsTime() : Namespace("Time")
    {
        insert(new statefs::BasicPropertyOwner
               <Seconds, std::string>("seconds"));
    }

    virtual ~NsTime() {}
    virtual void release() {}
};

/**
 * Provider should inherit statefs::AProvider and call ctor supplying
 * it with provider name
 */
class Provider : public statefs::AProvider
{
public:
    Provider(statefs_server *server)
        : AProvider("Drink", server)
    {
        insert(new NsChild("Water"));
        insert(new NsChild("Beer"));
        insert(new NsTime());
    }
    virtual ~Provider() {
        std::cerr << "~Provider " << get_name() << std::endl; }

    /** 
     * corresponds to statefs_node.release of the
     * statefs_provider.root
     */
    virtual void release() {
        std::cerr << "release prov" << std::endl;
        delete this;
    }
};

static Provider *provider = nullptr;

EXTERN_C statefs_provider *statefs_provider_get(statefs_server *server)
{
    if (provider)
        throw std::logic_error("!!!");
    provider = new Provider(server);
    return provider;
}

/** @} statefspp_example */
