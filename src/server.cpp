/**
 * @file server.cpp
 * @brief Statefs server implementation
 * @author (C) 2012, 2013 Jolla Ltd. Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 * @copyright LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include "statefs.hpp"

#include <statefs/provider.h>
#include <statefs/util.h>
#include <cor/util.h>

#include <metafuse.hpp>
#include <cor/mt.hpp>
#include <cor/so.hpp>
#include <cor/util.hpp>
#include "config.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <exception>
#include <unordered_map>
#include <set>
#include <atomic>
#include <fstream>
#include <signal.h>


#include "fuse_lowlevel.h"

#define STRINGIFY(x) #x
#define DQUOTESTR(x) STRINGIFY(x)

//static const char *statefs_version = DQUOTESTR(STATEFS_VERSION);


namespace statefs { namespace server {

static int global_umask = 0022;

using namespace metafuse;
using statefs::provider_ptr;
namespace config = statefs::config;

class ProviderBridge : public statefs_server
{
public:
    ProviderBridge(std::shared_ptr<LoaderProxy> loader, std::string const &path);

    ~ProviderBridge() {}

    bool loaded() const
    {
        return provider_ != nullptr;
    }

    ns_handle_type ns(std::string const &name) const;

    statefs_io *io()
    {
        return loaded() ? &(provider_->io) : nullptr;
    }

    void on_provider_event(statefs_provider *p, statefs_event e)
    {
        if (e == statefs_event_reload) {
            std::cerr << "Reloading required, exiting" << std::endl;
            ::exit(0);
        }
    }

private:

    static statefs_server* init_server(statefs_server *s)
    {
        s->event = &ProviderBridge::on_event_;
        return s;
    }

    static void on_event_
    (statefs_server *s, statefs_provider *p, statefs_event e)
    {
        auto *self = static_cast<ProviderBridge*>(s);
        self->on_provider_event(p, e);
    }

    static void ns_release(statefs_namespace *p)
    {
        if (p)
            statefs_node_release(&p->node);
    };

    // storing to be sure loader is unloaded only after provider
    std::shared_ptr<LoaderProxy> loader_;
    provider_ptr provider_;
};

class Namespace
{
public:
    Namespace(ns_handle_type &&h);

    ~Namespace() { }

    bool exists() const
    {
        return handle_ != nullptr;
    }

    property_handle_type property(std::string const &name) const;

private:

    static void property_release(statefs_property *p)
    {
        if (p)
            statefs_node_release(&p->node);
    };

    ns_handle_type handle_;
};

class Property
{
public:
    Property(statefs_io *io, property_handle_type &&h);

    bool exists() const
    {
        return handle_ != nullptr;
    }

    bool is_discrete() const
    {
        return getattr() & STATEFS_ATTR_DISCRETE;
    }

    int mode() const
    {
        int res = 0;
        auto attr = getattr();
        if (attr & STATEFS_ATTR_WRITE)
            res |= 0222;
        if (attr & STATEFS_ATTR_READ)
            res |= 0444;
        res &= (~global_umask);
        return res;
    }

    intptr_t open(int flags)
    {
        return io_->open(handle_.get(), flags);
    }

    void close(intptr_t h)
    {
        io_->close(h);
    }

    int read(intptr_t, char *, size_t, off_t) const;
    int write(intptr_t, char const*, size_t, off_t) const;

    size_t size() const
    {
        return exists() ? io_->size(handle_.get()) : 0;
    }

    int getattr() const
    {
        return exists() ? io_->getattr(handle_.get()) : 0;
    }

    bool connect(statefs_slot *slot);
    void disconnect();

    char const* name() const
    {
        return handle_ ? statefs_prop_name(handle_.get()) : "";
    }

    ::statefs_variant meta(std::string const &) const;

private:

    statefs_io *io_;
    property_handle_type handle_;
    mutable std::map<std::string, ::statefs_variant> meta_;
};

static inline ::statefs_variant invalid_variant()
{
    ::statefs_variant v;
    v.tag = statefs_variant_tags_end;
    v.s = nullptr;
    return v;
};


::statefs_variant Property::meta(std::string const &name) const
{
    static const ::statefs_variant invalid = invalid_variant();
    if (!handle_)
        return invalid;

    if (meta_.empty()) {
        auto meta = handle_->node.info;
        if (meta) {
            for (auto n = meta->name; meta->name; ++meta)
                meta_[n] = meta->value;
        }
    }
    auto it = meta_.find(name);
    return it != meta_.cend() ? it->second : invalid;
}

int Property::read(intptr_t h, char *dst, size_t len, off_t off) const
{
    if (!exists())
        return 0;

    return (io_->read
            ? io_->read(h, dst, len, off)
            : -ENOTSUP);
}

int Property::write(intptr_t h, char const* src, size_t len, off_t off) const
{
    if (!exists())
        return 0;

    return (io_->write
            ? io_->write(h, src, len, off)
            : -ENOTSUP);
}

bool Property::connect(statefs_slot *slot)
{
    if (!exists() || !is_discrete())
        return false;
    return io_->connect(handle_.get(), slot);
}

void Property::disconnect()
{
    if (exists() && is_discrete())
        io_->disconnect(handle_.get());
}

ProviderBridge::ProviderBridge(std::shared_ptr<LoaderProxy> loader, std::string const &path)
    : loader_(loader)
    , provider_(loader_
                ? loader_->load(path, ProviderBridge::init_server(this))
                : nullptr)
{ }

ns_handle_type ProviderBridge::ns(std::string const &name) const
{
    return mk_namespace_handle
        ((loaded() ? statefs_ns_find(&provider_->root, name.c_str())
          : nullptr));
}

property_handle_type Namespace::property(std::string const &name) const
{
    return mk_property_handle
        ((exists() ? statefs_prop_find(handle_.get(), name.c_str())
          : nullptr));
}

Property::Property(statefs_io *io, property_handle_type &&h)
    : io_(io), handle_(std::move(h))
{}

Namespace::Namespace(ns_handle_type &&h)
    : handle_(std::move(h))
{}

/// file interface implementation used to load provider interface
/// implementation on accessing file for the first time
template <typename LoadT>
class PluginLoadFile : public DefaultFile<PluginLoadFile<LoadT> >
{
    typedef DefaultFile<PluginLoadFile<LoadT> > base_type;

public:

    /**
     * @param interface loader used to load actual implementation
     * @param mode file mode
     * @param size initial size in bytes, it is better to use some
     *        fake big size because many tools are checking size only
     *        at the beginning
     */
    PluginLoadFile(LoadT loader, int mode, size_t size)
        : base_type(mode), load_(loader), size_(size) {}

    int open(struct fuse_file_info &fi)
    {
        return load_call(&metafuse::Entry::open, fi);
    }

    int read(char* buf, size_t size,
             off_t offset, struct fuse_file_info &fi)
    {
        return -ENOTSUP;
    }

    int write(const char* src, size_t size,
              off_t offset, struct fuse_file_info &fi)
    {
        return -ENOTSUP;
    }

    size_t size() const
    {
        return size_;
    }

	int poll(struct fuse_file_info &fi,
             poll_handle_type &ph, unsigned *reventsp)
    {
        std::cerr << "Loader file can't be polled\n";
        return 0;
    }

#ifdef USE_XATTR

    int setxattr(const char *name, const char *value, size_t size, int flags)
    {
        return load_call(&metafuse::Entry::setxattr, name, value, size, flags);
    }

    int getxattr(const char *name, char *value, size_t out_size) const
    {
        return load_call(&metafuse::Entry::getxattr, name, value, out_size);
    }

    int listxattr(char *list, size_t out_size) const
    {
        return load_call(&metafuse::Entry::listxattr, list, out_size);
    }

    int removexattr(const char *name)
    {
        return load_call(&metafuse::Entry::removexattr, name);
    }

#endif // USE_XATTR

private:

    template <typename FnT, typename ... Args>
    int load_call(FnT fn, Args&& ... args) const
    {
        auto loaded = load_();
        return ((loaded.get())->*fn)(empty_path(), std::forward<Args>(args)...);
    }

    LoadT load_;
    size_t size_;
};

class StateFsHandle : public FileHandle {
public:
    StateFsHandle() : h_(0) {}
    void set(intptr_t h)
    {
        h_ = h;
    }
    intptr_t get() const
    {
        return h_;
    }
private:
    intptr_t h_;
};

struct PropertyStorage
{
    PropertyStorage(std::unique_ptr<Property> from)
        : prop_(std::move(from))
    {}

    std::unique_ptr<Property> prop_;
};

class ContinuousPropFile
    : protected PropertyStorage
    , public DefaultFile<ContinuousPropFile, StateFsHandle, cor::Mutex>
{
public:
    typedef DefaultFile<ContinuousPropFile, StateFsHandle,
                        cor::Mutex> base_type;

    ContinuousPropFile(std::unique_ptr<Property> prop, int mode)
        : PropertyStorage(std::move(prop))
        , base_type(mode)
    {}

    int open(struct fuse_file_info &fi)
    {
        int rc = base_type::open(fi);
        if (rc >= 0) {
            auto h = prop_->open(fi.flags);
            if (h)
                handles_[fi.fh]->set(h);
            else
                rc = -1;
        }
        return rc;
    }

    int release(struct fuse_file_info &fi)
    {
        prop_->close(provider_handle(fi));
        return base_type::release(fi);
    }

    int read(char* buf, size_t size,
             off_t offset, struct fuse_file_info &fi)
    {
        return prop_->read(provider_handle(fi), buf, size, offset);
    }

    int write(const char* src, size_t size,
              off_t offset, struct fuse_file_info &fi)
    {
        return prop_->write(provider_handle(fi), src, size, offset);
    }

    size_t size() const
    {
        return prop_->size();
    }

	int poll(struct fuse_file_info &fi,
             poll_handle_type &ph, unsigned *reventsp)
    {
        std::cerr << "User wants to poll unpollable file "
                  << prop_->name() << std::endl;
        return 0;
    }

    int getattr(struct stat *buf)
    {
        update_time(modification_time_bit | change_time_bit | access_time_bit);
        return base_type::getattr(buf);
    }

protected:

    handle_type const* handle(struct fuse_file_info &fi) const
    {
        return reinterpret_cast<handle_type const*>(fi.fh);
    }

    handle_type* handle(struct fuse_file_info &fi)
    {
        return reinterpret_cast<handle_type*>(fi.fh);
    }

    intptr_t provider_handle(struct fuse_file_info &fi) const
    {
        auto h = handle(fi);
        return h ? h->get() : 0;
    }
};

class PluginNsDir;

class DiscretePropFile : public ContinuousPropFile
{

    /// used as a bridge between C callback and C++ object
    static void slot_on_changed
    (struct statefs_slot *slot, struct statefs_property *)
    {
        auto self = cor::member_container(slot, &DiscretePropFile::slot_);
        self->notify();
    }

public:
    DiscretePropFile(PluginNsDir *, std::unique_ptr<Property>, int);
    virtual ~DiscretePropFile();

	int poll(struct fuse_file_info &, poll_handle_type &, unsigned *);
    int open(struct fuse_file_info &);
    int release(struct fuse_file_info &fi);
    void notify();

    int getattr(struct stat *buf)
    {
        return ContinuousPropFile::base_type::getattr(buf);
    }

private:
    PluginNsDir *parent_;
    std::atomic_flag is_notify_;
    statefs_slot slot_;
};


template <typename LoadT, typename ... Args>
std::unique_ptr<PluginLoadFile<LoadT> > mk_loader(LoadT loader, Args&& ... args)
{
    return make_unique<PluginLoadFile<LoadT> >
        (loader, std::forward<Args>(args)...);
}

class PluginDir;

class PluginNsDir : public RODir<DirFactory, FileFactory, cor::Mutex>
{
    typedef RODir<DirFactory, FileFactory, cor::Mutex> base_type;
public:

    typedef std::shared_ptr<config::Namespace> info_ptr;

    PluginNsDir(PluginDir *, info_ptr, std::function<void()> const&);

    void load(std::shared_ptr<ProviderBridge> prov);
    void load_fake();

    bool enqueue(std::packaged_task<void()>);

private:

    void add_loader_file(std::shared_ptr<config::Property> const &
                       , std::function<void()> const& plugin_load);
    void add_prop_file(std::unique_ptr<Property>);

    PluginDir *parent_;
    info_ptr info_;
    std::unique_ptr<Namespace> ns_;
};

/// extracted into separate class from PluginDir to initialize later
/// in the proper order
class PluginStorage
{
protected:
    std::shared_ptr<ProviderBridge> provider_;
    cor::TaskQueue task_queue_;
};

class PluginsDir;

class PluginDir : private PluginStorage,
                  public RODir<DirFactory, FileFactory, cor::Mutex>
{
public:
    typedef DirEntry<PluginNsDir> ns_type;
    typedef std::shared_ptr<config::Plugin> info_ptr;

    PluginDir(PluginsDir *parent, info_ptr info);
    void load();

    bool enqueue(std::packaged_task<void()> task)
    {
        return task_queue_.enqueue(std::move(task));
    }

    void stop()
    {
        task_queue_.stop();
    }

private:

    template <typename OpT, typename ... Args>
    void namespaces_init(OpT op, Args&& ... args)
    {
        for (auto &d : dirs) {
            trace() << "Init ns " << d.first << std::endl;
            auto p = dir_entry_impl<PluginNsDir>(d.second);
            if (!p)
                throw std::logic_error("Can't cast to namespace???");
            std::mem_fn(op)(p.get(), std::forward<Args>(args)...);
        }
    }

    info_ptr load_namespaces(info_ptr);

    info_ptr info_;
    PluginsDir *parent_;
};

DiscretePropFile::DiscretePropFile
(PluginNsDir *parent, std::unique_ptr<Property> prop, int mode)
    : ContinuousPropFile(std::move(prop), mode)
    , parent_(parent)
    , is_notify_(ATOMIC_FLAG_INIT)
    , slot_({&DiscretePropFile::slot_on_changed})
{
}

DiscretePropFile::~DiscretePropFile()
{
    // called not from fuse thread, so should be explicitely protected
    // by lock
    auto l(cor::wlock(*this));
    if (!handles_.empty()) {
        trace() << "DiscretePropFile " << prop_->name()
                << " was not released?\n";
        prop_->disconnect();
    }
    slot_.on_changed = nullptr;
}

int DiscretePropFile::poll(struct fuse_file_info &fi,
                           poll_handle_type &ph, unsigned *reventsp)
{
    auto p = handle(fi);
    if (!p) {
        std::cerr << "poll: invalid handle, name=" << prop_->name()
                  << std::endl;
        return -EINVAL;
    }

    if (p->is_changed() && reventsp) {
        *reventsp |= POLLIN;
    }

    p->poll(ph);
    return 0;
}

int DiscretePropFile::open(struct fuse_file_info &fi)
{
    if (handles_.empty()) {
        prop_->connect(&slot_);
    }

    return ContinuousPropFile::open(fi);
}

int DiscretePropFile::release(struct fuse_file_info &fi)
{
    int rc = ContinuousPropFile::release(fi);
    if (handles_.empty())
        prop_->disconnect();

    return rc;
}

void DiscretePropFile::notify()
{
    auto fn = [this]() {
        // CALL is originated from provider, so acquire lock
        auto l(cor::wlock(*this));
        update_time(modification_time_bit | change_time_bit | access_time_bit);
        if (handles_.empty())
            return;
        std::list<handle_ptr> snapshot;
        for (auto const &h : handles_)
            snapshot.push_back(h.second);
        l.unlock();
        for (auto h : snapshot)
            h->notify(*this);

        is_notify_.clear(std::memory_order_release);
    };
    if (!is_notify_.test_and_set(std::memory_order_acquire)) {
        parent_->enqueue(std::packaged_task<void()>{fn});
    }
}


PluginNsDir::PluginNsDir
(PluginDir *parent, info_ptr info, std::function<void()> const& plugin_load)
    : parent_(parent)
    , info_(info)
{
    for (auto prop : info->props_)
        add_loader_file(prop, plugin_load);
}

bool PluginNsDir::enqueue(std::packaged_task<void()> task)
{
    return parent_->enqueue(std::move(task));
}


void PluginNsDir::add_loader_file
(std::shared_ptr<config::Property> const &prop
 , std::function<void()> const& plugin_load)
{
    std::string name = prop->value();
    auto load_get = [this, name, plugin_load]() {
        trace() << "Loading " << name << std::endl;
        plugin_load();
        return acquire(name);
    };

    // initially adding loader file - it accesses provider
    // implementation only on first read/write access to the
    // file itself. Default size is 1Kb
    add_file
        (name, mk_file_entry
         (mk_loader(load_get, prop->mode(global_umask), 1024)));
}

void PluginNsDir::add_prop_file(std::unique_ptr<Property> prop)
{
    std::string name = prop->name();
    auto mode = prop->mode();
    if (prop->is_discrete()) {
        auto file = make_unique<DiscretePropFile>
            (this, std::move(prop), mode);
        add_file(name, mk_file_entry(std::move(file)));
    } else {
        auto file = make_unique<ContinuousPropFile>(std::move(prop), mode);
        add_file(name, mk_file_entry(std::move(file)));
    }
}

void PluginNsDir::load(std::shared_ptr<ProviderBridge> prov)
{
    auto lock(cor::wlock(*this));
    files.clear();
    auto ns = make_unique<Namespace>(prov->ns(info_->value()));

    for (auto cfg : info_->props_) {
        std::string name = cfg->value();
        auto prop = make_unique<Property>(prov->io(), ns->property(name));
        if (prop->exists()) {
            add_prop_file(std::move(prop));
        } else {
            std::cerr << "PROPERTY " << name << " is absent\n";
            add_file(name, mk_file_entry
                     (make_unique<BasicTextFile<> >
                      (cfg->defval(), cfg->mode())));
        }
    }
    ns_ = std::move(ns);
}

void PluginNsDir::load_fake()
{
    auto lock(cor::wlock(*this));
    files.clear();
    for (auto prop : info_->props_) {
        std::string name = prop->value();
        add_file(name, mk_file_entry
                 (make_unique<BasicTextFile<> >(prop->defval(), prop->mode())));
    }
}


class PluginsDir : private LoadersStorage,
                   public ReadRmDir<DirFactory, FileFactory, cor::Mutex>
{
public:
    PluginsDir();
    PluginsDir(PluginsDir const&) = delete;
    PluginsDir& operator = (PluginsDir const&) = delete;

    void plugin_add(PluginDir::info_ptr);
    void loader_add(loader_info_ptr);
    void stop();

    std::shared_ptr<LoaderProxy> loader_get(std::string const&);
};

PluginsDir::PluginsDir()
{
}

void PluginsDir::stop()
{
    for (auto e: dirs)
        dir_entry_impl<PluginDir>(e.second)->stop();
}

void PluginsDir::plugin_add(PluginDir::info_ptr p)
{
    auto lock(cor::wlock(*this));
    auto name = p->value();
    trace() << "Plugin " << name << std::endl;
    if (dirs.find(name)) {
        std::cerr << "There is already a plugin " << name << "...skipping\n";
        return;
    }
    auto d = std::make_shared<PluginDir>(this, p);
    add_dir(p->value(), mk_dir_entry(d));
}

void PluginsDir::loader_add(loader_info_ptr p)
{
    auto lock(cor::wlock(*this));
    trace() << "Loader " << p->value() << std::endl;
    loader_register(p);
}

std::shared_ptr<LoaderProxy> PluginsDir::loader_get(std::string const& name)
{
    auto lock(cor::wlock(*this));
    return LoadersStorage::loader_get(name);
}


class NamespaceDir : public RODir<DirFactory, FileFactory, cor::Mutex>
{
public:
    NamespaceDir(PluginDir::info_ptr p,
                 PluginNsDir::info_ptr ns);
};

PluginDir::info_ptr PluginDir::load_namespaces(info_ptr p)
{
    auto plugin_load = std::bind(&PluginDir::load, this);
    for (auto ns : p->namespaces_)
        add_dir(ns->value(), mk_dir_entry
                (make_unique<PluginNsDir>(this, ns, plugin_load)));
    return p;
}

PluginDir::PluginDir(PluginsDir *parent, info_ptr info)
    : info_(load_namespaces(info))
    , parent_(parent)
{
}

void PluginDir::load()
{
    auto lock(cor::wlock(*this));
    if (provider_)
        return;

    trace() << "Loading plugin " << info_->path << std::endl;
    auto provider_type = config::to_string(info_->info_["type"]);
    provider_ = make_unique<ProviderBridge>
        (parent_->loader_get(provider_type), info_->path);

    if (!provider_->loaded()) {
        std::cerr << "Can't load " << info_->path
                  << ", using fake values" << std::endl;
        namespaces_init(&PluginNsDir::load_fake);
        return;
    }
    namespaces_init(&PluginNsDir::load, provider_);
}

NamespaceDir::NamespaceDir
(PluginDir::info_ptr p, PluginNsDir::info_ptr ns)
{
    Path path = {"..", "..", "providers", p->value(), ns->value()};
    for (auto prop : ns->props_) {
        path.push_back(prop->value());
        add_symlink(prop->value(), boost::algorithm::join(path, "/"));
        path.pop_back();
    }
}

class NamespacesDir : public RODir<DirFactory, FileFactory, cor::Mutex>
{
public:
    void plugin_add(PluginDir::info_ptr p)
    {
        auto lock(cor::wlock(*this));
        for (auto ns : p->namespaces_)
            add_dir(ns->value(), mk_dir_entry(make_unique<NamespaceDir>(p, ns)));
    }
};


class RootDir : private config::ConfigReceiver
              , public RODir<DirFactory, FileFactory, cor::Mutex>
{
    typedef RODir<DirFactory, FileFactory, cor::Mutex> base_type;
    typedef void (RootDir::*self_fn_type)();
public:
    RootDir()
        : plugins(new PluginsDir())
        , namespaces(new NamespacesDir())
        , before_access_(&RootDir::access_before_init)
    {
        add_dir("providers", mk_dir_entry(plugins));
        add_dir("namespaces", mk_dir_entry(namespaces));
    }

    RootDir(RootDir const&) = delete;
    RootDir& operator = (RootDir const&) = delete;

    void stop()
    {
        plugins->stop();
    }

    void init(std::string const &cfg_dir)
    {
        cfg_dir_ = cfg_dir;
        before_access_ = &RootDir::load_monitor;
    }

    int readdir(void* buf, fuse_fill_dir_t filler,
                off_t offset, fuse_file_info &fi)
    {
        (this->*before_access_)();
        before_access_ = &RootDir::dummy;
        return base_type::readdir(buf, filler, offset, fi);
    }

    int getattr(struct stat *stbuf)
    {
        (this->*before_access_)();
        before_access_ = &RootDir::dummy;
        return base_type::getattr(stbuf);
    }

    entry_ptr acquire(std::string const &name)
    {
        (this->*before_access_)();
        before_access_ = &RootDir::dummy;
        return base_type::acquire(name);
    }

private:

    void access_before_init()
    {
        throw cor::Error("No config set");
    }

    void load_monitor()
    {
        auto receiver = static_cast<ConfigReceiver*>(this);
        if (cfg_mon_)
            throw cor::Error("There is a monitor already");

        cfg_mon_ = make_unique<config::Monitor>(cfg_dir_, *receiver);
    }

    virtual void provider_add(std::shared_ptr<config::Plugin> p)
    {
        if (p) {
            plugins->plugin_add(p);
            namespaces->plugin_add(p);
        }
    }

    virtual void loader_add(std::shared_ptr<config::Loader> p)
    {
        if (p)
            plugins->loader_add(p);
    }

    void dummy() {}

    std::shared_ptr<PluginsDir> plugins;
    std::shared_ptr<NamespacesDir> namespaces;
    self_fn_type before_access_;
    std::unique_ptr<config::Monitor> cfg_mon_;
    std::string cfg_dir_;
};

class RootDirEntry : public DirEntry<RootDir>
{
    typedef DirEntry<RootDir> base_type;
public:

    RootDirEntry() : base_type(make_unique<RootDir>()) {}
    virtual ~RootDirEntry() {}

    void init(std::string const &cfg_dir)
    {
        if (impl_)
            impl_->init(cfg_dir);
    }

    void destroy()
    {
        if (impl_)
            impl_->clear();
    }
};


struct Root {

    typedef metafuse::FuseFs<RootDirEntry> impl_type;
    typedef std::shared_ptr<impl_type> impl_ptr;

    void release()
    {
        fs.reset();
    }

    impl_ptr get();

    void stop()
    {
        auto hold_fs = fs;
        auto entry = fs->impl();
        if (entry) {
            auto entry_impl = dir_entry_impl<RootDir>(entry);
            if (entry_impl)
                entry_impl->stop();
        }
    }

    impl_ptr instance()
    {
        return fs;
    }

    impl_ptr fs;
};

static Root statefs_root;

}} // namespace


namespace metafuse {

// metafuse leaves implementation for FuseFs.instance() and
// FuseFs.release() to be done by fs implementation

using statefs::server::Root;
using statefs::server::statefs_root;

template<> Root::impl_ptr Root::impl_type::instance()
{
    return statefs_root.instance();
}

template<> void Root::impl_type::release()
{
    statefs_root.release();
}

} // metafuse

namespace statefs { namespace server {

enum statefs_cmd {
    statefs_cmd_run,
    statefs_cmd_dump,
    statefs_cmd_register,
    statefs_cmd_cleanup,
    statefs_cmd_unregister
};


class FuseMain
{
public:

    /*
      Code in this class mostly was copy/pasted from fuse code. It has
      statefs cleanup hook to avoid access to fuse when server got
      signal because it results in unpredictable consequences like
      segfault etc.

      FUSE: Filesystem in Userspace
      Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

      This program can be distributed under the terms of the GNU LGPLv2.
      See the file COPYING.LIB
    */

    static ::fuse_session *fuse_instance;
    static ::fuse *fuse;
    static char *mountpoint;
    static ::fuse_chan *ch;

    static void exit_handler(int sig)
    {
        (void) sig;
        if (fuse_instance) {
            statefs_root.stop();
            fuse_session_exit(fuse_instance);
        }
    }

    static int set_one_signal_handler(int sig, void (*handler)(int))
    {
        struct sigaction sa;
        struct sigaction old_sa;

        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = handler;
        sigemptyset(&(sa.sa_mask));
        sa.sa_flags = 0;

        if (sigaction(sig, NULL, &old_sa) == -1) {
            perror("fuse: cannot get old signal handler");
            return -1;
        }

        if (old_sa.sa_handler == SIG_DFL &&
            sigaction(sig, &sa, NULL) == -1) {
            perror("fuse: cannot set signal handler");
            return -1;
        }
        return 0;
    }

    static int set_signal_handlers(struct fuse_session *se)
    {
        if (set_one_signal_handler(SIGHUP, exit_handler) == -1 ||
            set_one_signal_handler(SIGINT, exit_handler) == -1 ||
            set_one_signal_handler(SIGTERM, exit_handler) == -1 ||
            set_one_signal_handler(SIGPIPE, SIG_IGN) == -1)
            return -1;

        fuse_instance = se;
        return 0;
    }


    static struct fuse *statefs_setup_common
    (int argc, char *argv[], ::fuse_operations const *op, size_t op_size
     , int *multithreaded, int *fd, void *user_data)
    {
        ::fuse_args args = FUSE_ARGS_INIT(argc, argv);
        //::fuse *fuse;
        int foreground;
        int res;

        res = ::fuse_parse_cmdline(&args, &mountpoint, multithreaded, &foreground);
        if (res == -1)
            return NULL;

        ch = ::fuse_mount(mountpoint, &args);
        if (!ch) {
            ::fuse_opt_free_args(&args);
            goto err_free;
        }

        fuse = ::fuse_new(ch, &args, op, op_size, user_data);
        ::fuse_opt_free_args(&args);
        if (fuse == NULL)
            goto err_unmount;

        res = ::fuse_daemonize(foreground);
        if (res == -1)
            goto err_unmount;

        res = set_signal_handlers(fuse_get_session(fuse));
        if (res == -1)
            goto err_unmount;

        if (fd)
            *fd = ::fuse_chan_fd(ch);

        return fuse;

    err_unmount:
        ::fuse_unmount(mountpoint, ch);
        if (fuse)
            ::fuse_destroy(fuse);
    err_free:
        free(mountpoint);
        return NULL;
    }

    static int statefs_main_common(int argc, char *argv[],
                                   const struct fuse_operations *op, size_t op_size,
                                   void *user_data)
    {
        struct fuse *fuse;
        int multithreaded;
        int res;

        fuse = statefs_setup_common(argc, argv, op, op_size,
                                    &multithreaded, NULL, user_data);
        if (fuse == NULL)
            return 1;

        if (multithreaded)
            res = fuse_loop_mt(fuse);
        else
            res = fuse_loop(fuse);

        //statefs_root.release();
        fuse_teardown(fuse, mountpoint);
        if (res == -1)
            return 1;

        return 0;
    }

};

::fuse_session *FuseMain::fuse_instance = nullptr;
::fuse *FuseMain::fuse = nullptr;
char *FuseMain::mountpoint = nullptr;
::fuse_chan *FuseMain::ch = nullptr;

//using metafuse::statefs_root_type;

Root::impl_ptr Root::get()
{
    if (!fs) {
        fs = std::make_shared<impl_type>();
        fs->main_ = FuseMain::statefs_main_common;
    }
    return instance();
}

static Root::impl_ptr fuse()
{
    return statefs_root.get();
}


template <class OutputIterator>
bool split_pairs(std::string const &src, std::string const &items_sep
                 , std::string const &pair_sep, OutputIterator dst)
{
    //typedef typename OutputIterator::value_type v_type;
    std::list<std::string> items;
    cor::split(src, items_sep, std::back_inserter(items));
    std::vector<std::string> pair;
    for (auto &v : items) {
        if (v == "")
            continue;
        cor::split(v, pair_sep, std::back_inserter(pair));
        auto sz = pair.size();
        switch (sz) {
        case 1:
            *dst = {pair[0], ""};
            break;
        case 2:
            *dst = {pair[0], pair[1]};
            break;
        default:
            return false;
        }
        pair.clear();
    }
    return true;
}

namespace config = statefs::config;

template <int Base>
long to_long(std::string const &s)
{
    auto on_err = [&s]() {
        throw cor::Error("Wrong value to convert: %s", s.c_str());
    };
    if (!s.size())
        on_err();
    char *endptr = nullptr;
    auto l = strtol(&s[0], &endptr, Base);
    if ((endptr && *endptr != '\0') || l == LONG_MAX || l == LONG_MIN)
        on_err();
    return l;
}

class Server
{
    typedef cor::OptParse<std::string> option_parser_type;
public:

    Server(int argc, char *argv[])
        : cfg_dir("/var/lib/statefs")
        , options({{'h', "help"}, {'o', "options"}},
                  {{"statefs-config-dir", "config"}
                      , {"statefs-type", "type"}
                      , {"system", "system"}
                      , {"help", "help"}},
                  {"config", "type", "options"},
                  {"help"})
        , commands({
                {"dump", statefs_cmd_dump}
                , {"register", statefs_cmd_register}
                , {"unregister", statefs_cmd_unregister}
                , {"cleanup", statefs_cmd_cleanup}
            })
    {
        // TODO it is better to use fuse options parser
        options.parse(argc, argv, opts, params);
    }

    ~Server() {
        statefs_root.release();
    }

    int execute()
    {
        statefs_cmd cmd = statefs_cmd_run;
        int rc = 0;

        parse_fuse_opts();

        if (opts.count("system"))
            cfg_dir = "/var/lib/statefs/system";

        if (!opts.count("type"))
            opts["type"] = "default";

        if (params.size() > 1) {
            // first param can be a command
            auto p = commands.find(params[1]);
            if (p != commands.end())
                cmd = p->second;
        }

        if (opts.count("help"))
            return show_help();

        auto p = opts.find("config");
        if (p != opts.end())
            cfg_dir = p->second;

        switch (cmd) {
        case statefs_cmd_run:
            rc = main();
            break;
        case statefs_cmd_dump:
            rc = dump();
            break;
        case statefs_cmd_register:
            rc = save_provider_config();
            break;
        case statefs_cmd_unregister:
            rc = rm_provider_config();
            break;
        case statefs_cmd_cleanup:
            rc = cleanup_config();
            break;
        }

        return rc;
    }

private:

    void parse_fuse_opts()
    {
        if (!split_pairs(opts["options"], ",", "="
                         , std::inserter(fs_opts, fs_opts.begin())))
            throw cor::Error("Invalid fuse options");
    }

    int dump()
    {
        if (params.size() < 3)
            return show_help(-1);

        namespace fs = boost::filesystem;
        if (!ensure_dir_exists(cfg_dir))
            return -1;

        config::dump(cfg_dir, std::cout, params[2], opts["type"]);
        return 0;
    }

    int save_provider_config()
    {
        if (params.size() < 3)
            return show_help(-1);

        namespace fs = boost::filesystem;
        if (!ensure_dir_exists(cfg_dir))
            return -1;

        config::save(cfg_dir, params[2], opts["type"]);
        return 0;
    }

    int rm_provider_config()
    {
        if (params.size() < 3)
            return show_help(-1);

        namespace fs = boost::filesystem;
        if (!ensure_dir_exists(cfg_dir))
            return -1;

        config::rm(cfg_dir, params[2], opts["type"]);
        return 0;
    }

    int cleanup_config()
    {
        namespace fs = boost::filesystem;
        if (!ensure_dir_exists(cfg_dir))
            return -1;

        auto rm_absent = [](std::string const &cfg_path
                            , std::shared_ptr<config::Library> p) {
            auto lib_fname = p->path;
            if (fs::exists(lib_fname))
                return;

            std::cerr << "Library " << lib_fname
            << " doesn't exist, unregistering, removing config: "
            << cfg_path << std::endl;
            fs::remove(cfg_path);
            return;
        };
        config::visit(cfg_dir, rm_absent);
        return 0;
    }

    int fuse_run()
    {
        // repacking -o options separately because some options can be
        // extracted from -o to avoid error from fuse_parse_cmdline
        auto argv = std::move(params);
        auto join_options = [this]() {
            std::vector<std::string> parts;
            for (auto const &kv : fs_opts) {
                auto const &v = kv.second;
                parts.push_back(v.size() ? kv.first + "=" + v : kv.first);
            }
            return "-o" + boost::algorithm::join(parts, ",");
        };
        auto dash_o = join_options();
        argv.push_back(dash_o.c_str());
        auto root = fuse();
        return root ? root->main(argv.size(), &argv[0], true) : -EPERM;
    }

    int main()
    {
        auto p = fs_opts.find("uid");
        if (p != fs_opts.end()) {
            if (::setuid(to_long<10>(p->second))) {
                std::cerr << "setuid is failed: " << ::strerror(errno);
                return -1;
            }
        }

        p = fs_opts.find("gid");
        if (p != fs_opts.end()) {
            if (::setgid(to_long<10>(p->second))) {
                std::cerr << "setgid is failed: " << ::strerror(errno);
                return -1;
            }
        }
        p = fs_opts.find("file_umask");
        if (p != fs_opts.end()) {
            statefs::server::global_umask = to_long<8>(p->second);
            // fuse std parser does not understand unknown options
            fs_opts.erase(p);
        }

        auto root = fuse();
        int rc = -EPERM;
        if (root) {
            auto impl = root->impl();
            if (impl) {
                impl->init(cfg_dir);
                rc = fuse_run();
            }
        }
        return rc;
    }

    int show_help(int rc = 0)
    {
        options.show_help(std::cerr, params[0],
                          "[command] [options] [fuse_options]\n"
                          "\t[command]:\n"
                          "\t\tdump plugin_path\n"
                          "\t\tunregister plugin_path\n"
                          "\t\tregister plugin_path\n"
                          "\t\tcleanup\n"
                          "\t[options]:\n");
        params.push_back("-ho");
        int fuse_rc = fuse_run();
        return (fuse_rc) ? fuse_rc : rc;
    }

    std::string cfg_dir;
    option_parser_type options;
    std::unordered_map<std::string, statefs_cmd> commands;
    option_parser_type::map_type opts;
    option_parser_type::map_type fs_opts;
    std::vector<char const*> params;
};

}}

using statefs::server::Server;

static std::unique_ptr<Server> server;

int main(int argc, char *argv[])
{
    int rc = -1;
    try {
        server = cor::make_unique<Server>(argc, argv);
        rc = server->execute();
    } catch (std::exception const &e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
    server.reset();
    return rc;
}
