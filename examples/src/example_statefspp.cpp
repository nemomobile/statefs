#include <iostream>
#include <statefs/provider.hpp>
#include <statefs/property.hpp>
#include <chrono>
#include <future>

#include <cor/mt.hpp>

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

 /**
  * Basic namespace example
  *
  * It contains analog/continious property "amount" implemented in the
  * Stream class and also discrete property "custom" reusing higher
  * level RWProperty wrapper
  *
  *
  */
class Drink : public statefs::Namespace
{
public:
    Drink(char const *name) : Namespace(name)
    {
        using namespace statefs;

        insert(new BasicPropertyOwner
               <Stream, std::string>("amount"));

        auto update = [this](std::string const &v) {
            std::cout << "You set property value to " << v << std::endl;
            set_custom_(v);
            return PropertyStatus::Updated;
        };
        // adding property "custom" which can be changed by user
        auto custom = create(DiscreteWritable("custom", "0"), update);
        set_custom_ = setter(custom);
        insert(custom);
    }

    virtual ~Drink() {}
    virtual void release() {}

private:

    statefs::setter_type set_custom_;
};

/**
 * Discrete (pollable) property implementation example
 *
 * Implements full property interface
 */
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
        std::unique_lock<std::mutex> lock(mutex_);
        if (!slot_)
            v_ = calc();

        auto &v = *h;
        if (!off)
            v = v_;

        lock.unlock();
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
        if (!slot_)
            return;

        std::cerr << "periodic\n";
        std::thread([this]() {
                std::cerr << "next\n";
                ::usleep(update_interval_usec_);
                if (slot_) {
                    if (!slot_)
                        return;

                    std::unique_lock<std::mutex> lock(mutex_);
                    slot_->on_changed(slot_, parent_);
                    periodic_update();
                }
            }).detach();
    }

    unsigned update_interval_usec_;
    std::mutex mutex_;
    statefs::AProperty *parent_;
    std::string v_;
    ::statefs_slot *slot_;
};

/**
 * Another basic namespace example
 *
 * Namespace represents namespace for "seconds" property, implemented
 * in the Seconds class
 *
 */
class Time : public statefs::Namespace
{
public:
    Time() : Namespace("Time")
    {
        insert(new statefs::BasicPropertyOwner
               <Seconds, std::string>("seconds"));
    }

    virtual ~Time() {}
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
        insert(new Drink("Water"));
        insert(new Drink("Beer"));
        insert(new Time());
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
