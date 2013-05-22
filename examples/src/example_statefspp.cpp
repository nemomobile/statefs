#include <iostream>
#include <statefs/provider.hpp>

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


class Provider : public statefs::AProvider
{
public:
    Provider() : AProvider("Drink") {
        insert(new NsChild("Water"));
        insert(new NsChild("Beer"));
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
