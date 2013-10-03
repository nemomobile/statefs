#include "statefs.hpp"

#include <statefs/provider.h>
#include <statefs/util.h>
#include <cor/util.h>

#include <metafuse.hpp>
#include <cor/mt.hpp>
#include <cor/so.hpp>
#include "config.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <exception>
#include <unordered_map>
#include <set>
#include <fstream>

#define STRINGIFY(x) #x
#define DQUOTESTR(x) STRINGIFY(x)

//static const char *statefs_version = DQUOTESTR(STATEFS_VERSION);

using namespace metafuse;
using statefs::provider_ptr;

class Provider : public statefs_server
{
public:
    Provider(std::shared_ptr<Loader> loader, std::string const &path);

    ~Provider() {}

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
        s->event = &Provider::on_event_;
        return s;
    }

    static void on_event_
    (statefs_server *s, statefs_provider *p, statefs_event e)
    {
        auto *self = static_cast<Provider*>(s);
        self->on_provider_event(p, e);
    }

    static void ns_release(statefs_namespace *p)
    {
        if (p)
            statefs_node_release(&p->node);
    };

    // storing to be sure loader is unloaded only after provider
    std::shared_ptr<Loader> loader_;
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
            res |= 0200;
        if (attr & STATEFS_ATTR_READ)
            res |= 0440;
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

private:

    statefs_io *io_;
    property_handle_type handle_;
};

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

Provider::Provider(std::shared_ptr<Loader> loader, std::string const &path)
    : loader_(loader)
    , provider_(loader_
                ? loader_->load(path, Provider::init_server(this))
                : nullptr)
{ }

ns_handle_type Provider::ns(std::string const &name) const
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
        auto loaded = load_();
        return loaded->open(empty_path(), fi);
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

private:
    LoadT load_;
    size_t size_;
};

class StateFsHandle : public FileHandle {
public:
    intptr_t h_;
};

class ContinuousPropFile :
    public DefaultFile<ContinuousPropFile, StateFsHandle, cor::Mutex>
{
public:
    typedef DefaultFile<ContinuousPropFile, StateFsHandle,
                        cor::Mutex> base_type;

    ContinuousPropFile(std::unique_ptr<Property> &prop, int mode)
        : base_type(mode), prop_(std::move(prop))
    {}

    int open(struct fuse_file_info &fi)
    {
        int rc = base_type::open(fi);
        if (rc >= 0) {
            intptr_t h = prop_->open(fi.flags);
            if (h)
                handles_[fi.fh]->h_ = h;
            else
                rc = -1;
        }
        return rc;
    }

    int release(struct fuse_file_info &fi)
    {
        prop_->close(handle(fi));
        return base_type::release(fi);
    }

    int read(char* buf, size_t size,
             off_t offset, struct fuse_file_info &fi)
    {
        return prop_->read(handle(fi), buf, size, offset);
    }

    int write(const char* src, size_t size,
              off_t offset, struct fuse_file_info &fi)
    {
        return prop_->write(handle(fi), src, size, offset);
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

    intptr_t handle(struct fuse_file_info &fi) const
    {
        auto p = reinterpret_cast<StateFsHandle const*>(fi.fh);
        return p->h_;
    }

    std::unique_ptr<Property> prop_;
};


class DiscretePropFile : public statefs_slot, public ContinuousPropFile
{

    /// used as a bridge between C callback and C++ object
    static void slot_on_changed
    (struct statefs_slot *slot, struct statefs_property *)
    {
        auto self = static_cast<DiscretePropFile*>(slot);
        self->notify();
    }

public:
    DiscretePropFile(std::unique_ptr<Property> &prop, int mode)
        : ContinuousPropFile(prop, mode)
    {
        on_changed = &DiscretePropFile::slot_on_changed;
    }

    virtual ~DiscretePropFile()
    {
        if (!handles_.empty()) {
            trace() << "DiscretePropFile " << prop_->name()
                    << " was not released?";
            prop_->disconnect();
        }
    }

    int getattr(struct stat *buf)
    {
        return ContinuousPropFile::base_type::getattr(buf);
    }

	int poll(struct fuse_file_info &fi,
             poll_handle_type &ph, unsigned *reventsp)
    {
        auto p = reinterpret_cast<handle_type*>(fi.fh);
        if (p->is_changed() && reventsp)
            *reventsp |= POLLIN;

        p->poll(ph);
        return 0;
    }

    int open(struct fuse_file_info &fi)
    {
        if (handles_.empty())
            prop_->connect(this);

        return ContinuousPropFile::open(fi);
    }

    int release(struct fuse_file_info &fi)
    {
        int rc = ContinuousPropFile::release(fi);
        if (handles_.empty())
            prop_->disconnect();

        return rc;
    }

    void notify()
    {
        // call is originated from provider, so acquire lock
        auto l(cor::wlock(*this));
        update_time(modification_time_bit | change_time_bit | access_time_bit);
        if (!handles_.size())
            return;
        std::list<handle_ptr> snapshot;
        for (auto h : handles_)
            snapshot.push_back(h.second);
        l.unlock();
        for (auto h : snapshot)
            h->notify();
    }
};

template <typename LoadT, typename ... Args>
std::unique_ptr<PluginLoadFile<LoadT> > mk_loader(LoadT loader, Args&& ... args)
{
    return make_unique<PluginLoadFile<LoadT> >
        (loader, std::forward<Args>(args)...);
}

class PluginNsDir : public RODir<DirFactory, FileFactory, cor::Mutex>
{
    typedef RODir<DirFactory, FileFactory, Mutex> base_type;
public:

    typedef std::shared_ptr<config::Namespace> info_ptr;

    PluginNsDir(info_ptr info, std::function<void()> const& plugin_load);

    void load(std::shared_ptr<Provider> prov);
    void load_fake();

private:

    void add_prop_file(std::shared_ptr<config::Property> const &prop
                       , std::function<void()> const& plugin_load);

    info_ptr info_;
    std::unique_ptr<Namespace> ns_;
};

PluginNsDir::PluginNsDir
(info_ptr info, std::function<void()> const& plugin_load)
    : info_(info)
{
    for (auto prop : info->props_)
        add_prop_file(prop, plugin_load);
}

void PluginNsDir::add_prop_file
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
    add_file(name, mk_file_entry(mk_loader(load_get, prop->mode(), 1024)));
}

void PluginNsDir::load(std::shared_ptr<Provider> prov)
{
    auto lock(cor::wlock(*this));
    files.clear();
    auto ns = make_unique<Namespace>(prov->ns(info_->value()));

    for (auto cfg : info_->props_) {
        std::string name = cfg->value();
        auto prop = make_unique<Property>(prov->io(), ns->property(name));
        if (!prop->exists()) {
            std::cerr << "PROPERTY " << name << " is absent\n";
            add_file(name, mk_file_entry
                     (make_unique<BasicTextFile<> >
                      (cfg->defval(), cfg->mode())));
        } else {
            if (prop->is_discrete())
                add_file(name, mk_file_entry
                         (make_unique<DiscretePropFile>(prop, prop->mode())));
            else
                add_file(name, mk_file_entry
                         (make_unique<ContinuousPropFile>(prop, prop->mode())));
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

/// extracted into separate class from PluginDir to initialize later
/// in the proper order
class PluginStorage
{
protected:
    std::shared_ptr<Provider> provider_;
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

private:

    template <typename OpT, typename ... Args>
    void namespaces_init(OpT op, Args ... args)
    {
        for (auto &d : dirs) {
            trace() << "Init ns " << d.first << std::endl;
            auto p = dir_entry_impl<PluginNsDir>(d.second);
            if (!p)
                throw std::logic_error("Can't cast to namespace???");
            std::mem_fn(op)(p.get(), args...);
        }
    }

    info_ptr load_namespaces(info_ptr);

    info_ptr info_;
    PluginsDir *parent_;
};


class PluginsDir : private LoadersStorage,
                   public ReadRmDir<DirFactory, FileFactory, cor::Mutex>
{
public:
    PluginsDir() { }

    void plugin_add(PluginDir::info_ptr p)
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

    void loader_add(loader_info_ptr p)
    {
        auto lock(cor::wlock(*this));
        trace() << "Loader " << p->value() << std::endl;
        loader_register(p);
    }

    void loader_rm(loader_info_ptr p)
    {
        auto lock(cor::wlock(*this));
        if (p)
            LoadersStorage::loader_rm(p->value());
    }

    std::shared_ptr<Loader> loader_get(std::string const& name)
    {
        auto lock(cor::wlock(*this));
        return LoadersStorage::loader_get(name);
    }
};


class NamespaceDir : public RODir<DirFactory, FileFactory, cor::Mutex>
{
public:
    NamespaceDir(PluginDir::info_ptr p,
                 PluginNsDir::info_ptr ns);

    void rm_props(PluginNsDir::info_ptr ns);
};

PluginDir::info_ptr PluginDir::load_namespaces(info_ptr p)
{
    auto plugin_load = std::bind(&PluginDir::load, this);
    for (auto ns : p->namespaces_)
        add_dir(ns->value(), mk_dir_entry
                (make_unique<PluginNsDir>(ns, plugin_load)));
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
    provider_ = make_unique<Provider>
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

void NamespaceDir::rm_props(PluginNsDir::info_ptr ns)
{
    for (auto prop : ns->props_) {
        auto name = prop->value();
        int rc = unlink_(name);
        if (rc < 0)
            std::cerr << "can't unlink prop " << name << std::endl;
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

    void plugin_rm(PluginDir::info_ptr p)
    {
        auto lock(cor::wlock(*this));
        for (auto ns : p->namespaces_) {
            auto name = ns->value();
            auto pdir = dirs.find(name);
            if (!pdir)
                continue;

            auto dir = dir_entry_impl<NamespaceDir>(pdir);
            dir->rm_props(ns);
            if (dir->empty()) {
                int rc = unlink_(name);
                if (rc < 0)
                    std::cerr << "can't unlink ns " << name << std::endl;
            }
        }
    }
};


class RootDir : private config::ConfigReceiver
              , public RODir<DirFactory, FileFactory, cor::NoLock>
{
    typedef RODir<DirFactory, FileFactory, cor::NoLock> base_type;
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
        cfg_mon_ = make_unique<config::Monitor>(cfg_dir_, *receiver);
    }

    virtual void provider_add(std::shared_ptr<config::Plugin> p)
    {
        auto lock(cor::wlock(*this));
        if (p) {
            plugins->plugin_add(p);
            namespaces->plugin_add(p);
        }
    }
    virtual void provider_rm(std::shared_ptr<config::Plugin> p)
    {
        auto lock(cor::wlock(*this));
        if (p) {
            trace() << "removing " << p->value() << std::endl;
            namespaces->plugin_rm(p);
            plugins->unlink(p->value());
        }
    }
    virtual void loader_add(std::shared_ptr<config::Loader> p)
    {
        auto lock(cor::wlock(*this));
        if (p)
            plugins->loader_add(p);
    }

    virtual void loader_rm(std::shared_ptr<config::Loader> p)
    {
        auto lock(cor::wlock(*this));
        if (p)
            plugins->loader_rm(p);
    }

    void dummy() {}

    std::shared_ptr<PluginsDir> plugins;
    std::shared_ptr<NamespacesDir> namespaces;
    self_fn_type before_access_;
    //std::function<void ()> before_access;
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
        impl_->init(cfg_dir);
    }
};

enum statefs_cmd {
    statefs_cmd_run,
    statefs_cmd_dump,
    statefs_cmd_register,
    statefs_cmd_cleanup,
    statefs_cmd_unregister
};

namespace metafuse {

typedef FuseFs<RootDirEntry> fuse_root_type;
static std::unique_ptr<fuse_root_type> fuse_root;

template<> fuse_root_type&
fuse_root_type::instance()
{
    return *fuse_root;
}
}

static fuse_root_type & fuse()
{
    if (!fuse_root)
        fuse_root = make_unique<fuse_root_type>();
    static auto &fuse = fuse_root_type::instance();
    return fuse;
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
                  {"help", "options"})
        , commands({
                {"dump", statefs_cmd_dump}
                , {"register", statefs_cmd_register}
                , {"unregister", statefs_cmd_unregister}
                , {"cleanup", statefs_cmd_cleanup}
            })
    {
        options.parse(argc, argv, opts, params);
    }

    ~Server() {
        fuse_root.reset(nullptr);
    }

    int operator()()
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
                         , std::inserter(opts, opts.begin())))
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
        return fuse().main(params.size(), &params[0], true);
    }

    int main()
    {
        auto p = opts.find("uid");
        if (p != opts.end())
            ::setuid(::atoi(p->second.c_str()));

        p = opts.find("gid");
        if (p != opts.end())
            ::setgid(::atoi(p->second.c_str()));

        fuse().impl().init(cfg_dir);
        int rc = fuse_run();
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
    std::vector<char const*> params;
};


int main(int argc, char *argv[])
{
    try {
        Server server(argc, argv);
        return server();
    } catch (std::exception const &e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
    return -1;
}
