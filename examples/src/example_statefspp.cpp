#include <iostream>
#include <statefs/provider.hpp>
#include <chrono>
#include <future>

struct BeatHandle { std::string v; };

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

class Beat
{
public:
    Beat(statefs::AProperty *parent)
        : parent_(parent), v_("e"), slot_(nullptr) {}

    int getattr() const { return STATEFS_ATTR_READ; }
    ssize_t size() const { return 128 ; }

    bool connect(::statefs_slot *slot) {
        slot_ = slot;
        return true;
    }

    int read(BeatHandle *h, char *dst, size_t len, off_t off)
    {
        update();
        auto &v = h->v;
        if (!off)
            v = v_;

        return read_from(v, dst, len, off);
    }

    int write(BeatHandle *h, char const *src, size_t len, off_t off)
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

class NsChild : public statefs::Namespace
{
public:
    NsChild(char const *name) : Namespace(name)
    {
        insert(new statefs::BasicPropertyOwner<Beat, BeatHandle>("amount"));
    }

    virtual ~NsChild() { std::cerr << "~NsChild " << get_name() << std::endl; }
    virtual void release() { std::cerr << "release  " << get_name() << std::endl;}
};

class Time
{
public:
    Time(statefs::AProperty *parent)
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

    int read(BeatHandle *h, char *dst, size_t len, off_t off)
    {
        if (!slot_)
            v_ = calc();

        auto &v = h->v;
        if (!off)
            v = v_;

        return read_from(v, dst, len, off);
    }

    int write(BeatHandle *h, char const *src, size_t len, off_t off)
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

class NsTime : public statefs::Namespace
{
public:
    NsTime() : Namespace("Time")
    {
        insert(new statefs::BasicPropertyOwner<Time, BeatHandle>("seconds"));
    }

    virtual ~NsTime() { std::cerr << "~NsTime " << get_name() << std::endl; }
    virtual void release() { std::cerr << "release " << get_name() << std::endl;}
};

class Provider : public statefs::AProvider
{
public:
    Provider() : AProvider("Drink") {
        insert(new NsChild("Water"));
        insert(new NsChild("Beer"));
        insert(new NsTime());
    }
    virtual ~Provider() { std::cerr << "~Provider " << get_name() << std::endl; }
    virtual void release() {
        std::cerr << "release prov" << std::endl;
        delete this;
    }
};

static Provider *provider = nullptr;

EXTERN_C struct statefs_provider * statefs_provider_get(void)
{
    if (provider)
        throw std::logic_error("!!!");
    provider = new Provider();
    return provider;
}
