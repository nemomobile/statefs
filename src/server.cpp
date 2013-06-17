#include "statefs.hpp"

#include <statefs/provider.h>
#include <statefs/util.h>
#include <cor/util.h>

#include <metafuse.hpp>
#include <cor/mt.hpp>
#include <cor/so.hpp>
#include "config.hpp"

#include <boost/algorithm/string/join.hpp>

#include <iostream>
#include <exception>
#include <unordered_map>
#include <set>
#include <fstream>

using namespace metafuse;

class Provider
{
public:
    Provider(std::string const &path);

    ~Provider() { }

    bool loaded() const
    {
        return provider_ != nullptr;
    }

    ns_handle_type ns(std::string const &name) const;

    statefs_io *io()
    {
        return loaded() ? &(provider_->io) : nullptr;
    }

private:

    static void ns_release(statefs_namespace *p)
    {
        if (p)
            statefs_node_release(&p->node);
    };

    cor::SharedLib lib_;
    provider_handle_type provider_;
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

Provider::Provider(std::string const &path)
    : lib_(path, RTLD_LAZY), provider_(std::move(mk_provider_handle(lib_)))
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
        return -ENOTSUP;
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
        return -ENOTSUP;
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
        for (auto h : handles_)
            h.second->notify();
    }
};

template <typename LoadT, typename ... Args>
PluginLoadFile<LoadT>* mk_loader(LoadT loader, Args ... args)
{
    return new PluginLoadFile<LoadT>(loader, args...);
}

class PluginNsDir : public RODir<DirFactory, FileFactory, cor::Mutex>
{
    typedef RODir<DirFactory, FileFactory, Mutex> base_type;
public:

    typedef std::shared_ptr<config::Namespace> info_ptr;

    template <typename PluginLoaderT>
    PluginNsDir(info_ptr info, PluginLoaderT plugin_load)
        : info_(info)
    {
        for (auto prop : info->props_) {
            std::string name = prop->value();
            auto load_get = [this, name, plugin_load]() {
                trace() << "Loading " << name << std::endl;
                plugin_load();
                return acquire(name);
            };

            // initially adding loader file - it accesses provider
            // implementation only on first read/write access to the
            // file itself. Default size is 1Kb
            add_file(name, mk_file_entry
                     (mk_loader(load_get, prop->mode(), 1024)));
        }
    }

    void load(std::shared_ptr<Provider> prov);
    void load_fake();

private:

    info_ptr info_;
    std::unique_ptr<Namespace> ns_;
};

void PluginNsDir::load(std::shared_ptr<Provider> prov)
{
    auto lock(cor::wlock(*this));
    files.clear();
    std::unique_ptr<Namespace> ns
        (new Namespace(prov->ns(info_->value())));
    for (auto cfg : info_->props_) {
        std::string name = cfg->value();
        std::unique_ptr<Property> prop
            (new Property(prov->io(), ns->property(name)));
        if (!prop->exists()) {
            std::cerr << "PROPERTY " << name << " is absent\n";
            add_file(name, mk_file_entry
                     (new BasicTextFile<>(cfg->defval(), cfg->mode())));
        } else {
            if (prop->is_discrete())
                add_file(name, mk_file_entry
                         (new DiscretePropFile(prop, prop->mode())));
            else
                add_file(name, mk_file_entry
                         (new ContinuousPropFile(prop, prop->mode())));
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
                 (new BasicTextFile<>(prop->defval(), prop->mode())));
    }
}

/// extracted into separate class from PluginDir to initialize later
/// in the proper order
class PluginStorage
{
protected:
    std::shared_ptr<Provider> provider_;
};

class PluginDir : public PluginStorage,
                  public RODir<DirFactory, FileFactory, cor::Mutex>
{
public:
    typedef DirEntry<PluginNsDir> ns_type;
    typedef std::shared_ptr<config::Plugin> info_ptr;

    PluginDir(info_ptr info);
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
};

PluginDir::info_ptr
PluginDir::load_namespaces(info_ptr p)
{
    auto plugin_load = std::bind(&PluginDir::load, this);
    for (auto ns : p->namespaces_)
        add_dir(ns->value(), mk_dir_entry(new PluginNsDir(ns, plugin_load)));
    return p;
}

PluginDir::PluginDir(info_ptr info)
    : info_(load_namespaces(info))
{ }

void PluginDir::load()
{
    auto lock(cor::wlock(*this));
    if (provider_)
        return;

    trace() << "Loading plugin " << info_->path << std::endl;
    provider_.reset(new Provider(info_->path));

    if (!provider_->loaded()) {
        std::cerr << "Can't load " << info_->path
                  << ", using fake values" << std::endl;
        namespaces_init(&PluginNsDir::load_fake);
        return;
    }
    namespaces_init(&PluginNsDir::load, provider_);
}

class PluginsDir : public ReadRmDir<DirFactory, FileFactory, cor::Mutex>
{
public:
    PluginsDir() { }

    void add(PluginDir::info_ptr p)
    {
        trace() << "Plugin " << p->value() << std::endl;
        auto d = std::make_shared<PluginDir>(p);
        add_dir(p->value(), mk_dir_entry(d));
    }
};

class NamespaceDir : public RODir<DirFactory, FileFactory, cor::Mutex>
{
public:
    NamespaceDir(PluginDir::info_ptr p,
                 PluginNsDir::info_ptr ns);

    void rm_props(PluginNsDir::info_ptr ns);
};

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
            add_dir(ns->value(), mk_dir_entry(new NamespaceDir(p, ns)));
    }

    void plugin_rm(PluginDir::info_ptr p)
    {
        auto lock(cor::wlock(*this));
        for (auto ns : p->namespaces_) {
            auto name = ns->value();
            auto pdir = dirs.find(name);
            if (pdir) {
                auto dir = dir_entry_impl<NamespaceDir>(pdir);
                dir->rm_props(ns);
                if (dir->empty()) {
                    int rc = unlink_(name);
                    if (rc < 0)
                        std::cerr << "can't unlink ns " << name << std::endl;
                }
            }
        }
    }
};


class RootDir : public RODir<DirFactory, FileFactory, cor::NoLock>
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
        using namespace std::placeholders;
        auto cb = std::bind(std::mem_fn(&RootDir::on_config_changed), this, _1, _2);
        cfg_mon_.reset(new config::Monitor(cfg_dir_, cb));
    }

    void on_config_changed(config::Monitor::Event ev,
                           PluginDir::info_ptr p)
    {
        auto lock(cor::wlock(*this));
        switch (ev) {
        case config::Monitor::Added:
            plugin_add(p);
            break;
        case config::Monitor::Removed:
            plugin_rm(p);
            break;
        }
    }

    void dummy() {}

    void plugin_add(PluginDir::info_ptr p)
    {
        if (p) {
            plugins->add(p);
            namespaces->plugin_add(p);
        }
    }

    void plugin_rm(PluginDir::info_ptr p)
    {
        if (p) {
            trace() << "removing " << p->value() << std::endl;
            namespaces->plugin_rm(p);
            plugins->unlink(p->value());
        }
    }

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

    RootDirEntry() : base_type(new RootDir()) {}
    virtual ~RootDirEntry() {}

    void init(std::string const &cfg_dir)
    {
        impl_->init(cfg_dir);
    }
};

enum statefs_cmd {
    statefs_cmd_run,
    statefs_cmd_dump,
    statefs_cmd_register
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
        fuse_root.reset(new fuse_root_type());
    static auto &fuse = fuse_root_type::instance();
    return fuse;
}

class Server
{
    typedef cor::OptParse<std::string> option_parser_type;
public:

    Server(int argc, char *argv[])
        : cfg_dir("/var/lib/statefs"),
          options({{'h', "help"}},
                  {{"statefs-config-dir", "config"}, {"help", "help"}},
                  {"config"},
                  {"help"}),
          commands({{"dump", statefs_cmd_dump},
                      {"register", statefs_cmd_register}})
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
        }

        return rc;
    }

private:

    int dump()
    {
        if (params.size() < 3)
            return show_help(-1);

        config::dump(std::cout, params[2]);
        return 0;
    }

    int save_provider_config()
    {
        if (params.size() < 3)
            return show_help(-1);

        namespace fs = boost::filesystem;
        if (!ensure_dir_exists(cfg_dir))
            return -1;

        config::save(cfg_dir, params[2]);
        return 0;
    }

    int fuse_run()
    {
        return fuse().main(params.size(), &params[0], true);
    }

    int main()
    {
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
                          "\t\tregister plugin_path\n"
                          "\t\tunregister plugin_path\n"
                          "\t[options]:\n");
        std::cerr << "[fuse_options]:\n\n";
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
