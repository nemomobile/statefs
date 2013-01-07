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

    bool is_loaded() const
    {
        return provider_ != nullptr;
    }

    ns_handle_type ns(std::string const &name) const;

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

    bool is_exists() const
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
    Property(property_handle_type &&h);

    bool is_exists() const
    {
        return handle_ != nullptr;
    }

    bool is_discrete() const
    {
        return is_exists() && (handle_->connect != nullptr);
    }

    int read(char *dst, size_t len, off_t off) const
    {
        return (is_exists()
                ? handle_->read(handle_.get(), dst, len, off)
                : 0);
    }

    size_t size() const
    {
        return is_exists() ? handle_->size() : 0;
    }


    bool connect(statefs_slot *slot);
    void disconnect(statefs_slot *slot);

private:

    property_handle_type handle_;
};

bool Property::connect(statefs_slot *slot)
{
    if (!is_exists() || !handle_->connect)
        return false;
    return handle_->connect(handle_.get(), slot);
}

void Property::disconnect(statefs_slot *slot)
{
    if (is_exists() && handle_->disconnect)
        handle_->disconnect(handle_.get(), slot);
}

Provider::Provider(std::string const &path)
    : lib_(path, RTLD_LAZY), provider_(std::move(mk_provider_handle(lib_)))
{ }

ns_handle_type Provider::ns(std::string const &name) const
{
    return mk_namespace_handle
        ((is_loaded() ? statefs_ns_find(provider_.get(), name.c_str())
          : nullptr));
}

property_handle_type Namespace::property(std::string const &name) const
{
    return mk_property_handle
        ((is_exists() ? statefs_prop_find(handle_.get(), name.c_str())
          : nullptr));
}

Property::Property(property_handle_type &&h)
    : handle_(std::move(h))
{}

Namespace::Namespace(ns_handle_type &&h)
    : handle_(std::move(h))
{}

template <typename LoadT>
class PluginLoadFile : public DefaultFile<PluginLoadFile<LoadT> >
{
    typedef DefaultFile<PluginLoadFile<LoadT> > base_type;

public:
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

class ContinuousPropFile : public DefaultFile<ContinuousPropFile, cor::Mutex>
{
    typedef DefaultFile<ContinuousPropFile, cor::Mutex> base_type;

public:
    ContinuousPropFile(std::unique_ptr<Property> &prop, int mode)
        : base_type(mode), prop_(std::move(prop))
    {}

    int read(char* buf, size_t size,
             off_t offset, struct fuse_file_info &fi)
    {
        return prop_->read(buf, size, offset);
    }

    int write(const char* src, size_t size,
              off_t offset, struct fuse_file_info &fi)
    {
        return -ENOTSUP;
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

protected:
    std::unique_ptr<Property> prop_;
};


class DiscretePropFile : public statefs_slot, public ContinuousPropFile
{

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
        prop_->disconnect(this);
    }

	int poll(struct fuse_file_info &fi,
             poll_handle_type &ph, unsigned *reventsp)
    {
        auto p = reinterpret_cast<handle_type*>(fi.fh);
        if (p->is_changed())
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
            prop_->disconnect(this);
        return rc;
    }

    void notify()
    {
        auto l(cor::wlock(*this));

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

    template <typename PluginLoaderT>
    PluginNsDir(std::shared_ptr<config::Namespace> info, PluginLoaderT plugin_load)
        : info_(info)
    {
        for (auto prop : info->props_) {
            std::string name = prop->value();
            auto load_get = [this, name, plugin_load]() {
                trace() << "Loading " << name << std::endl;
                plugin_load();
                return acquire(name);
            };
            add_file(name, mk_file_entry
                     (mk_loader(load_get, prop->mode(), prop->defval().size())));
        }
    }

    void load(std::shared_ptr<Provider> prov);
    void load_fake();

private:

    std::shared_ptr<config::Namespace> info_;
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
        std::unique_ptr<Property> prop(new Property(ns->property(name)));
        if (!prop->is_exists()) {
            std::cerr << "PROPERTY " << name << "is absent\n";
            add_file(name, mk_file_entry(new BasicTextFile<>(cfg->defval())));
        } else {
            if (prop->is_discrete())
                add_file(name, mk_file_entry
                         (new DiscretePropFile(prop, 0644)));
            else
                add_file(name, mk_file_entry
                         (new ContinuousPropFile(prop, 0644)));
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
        add_file(name, mk_file_entry(new BasicTextFile<>(prop->defval())));
    }
}


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

    PluginDir(std::shared_ptr<config::Plugin> info);
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

    std::shared_ptr<config::Plugin> info_;
};

PluginDir::PluginDir(std::shared_ptr<config::Plugin> info)
    : info_(info)
{
    auto plugin_load = std::bind(&PluginDir::load, this);
    for (auto ns : info->namespaces_)
        add_dir(ns->value(), mk_dir_entry(new PluginNsDir(ns, plugin_load)));
}

void PluginDir::load()
{
    auto lock(cor::wlock(*this));
    if (provider_)
        return;

    trace() << "Loading plugin " << info_->path << std::endl;
    provider_.reset(new Provider(info_->path));

    if (!provider_->is_loaded()) {
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

    void add(std::shared_ptr<config::Plugin> p)
    {
        trace() << "Plugin " << p->value() << std::endl;
        auto d = std::make_shared<PluginDir>(p);
        add_dir(p->value(), mk_dir_entry(d));
    }
};

class NamespaceDir : public RODir<DirFactory, FileFactory, cor::Mutex>
{
public:
    NamespaceDir(std::shared_ptr<config::Plugin> p,
                 std::shared_ptr<config::Namespace> ns);

    void rm_props(std::shared_ptr<config::Namespace> ns);
};

NamespaceDir::NamespaceDir
(std::shared_ptr<config::Plugin> p, std::shared_ptr<config::Namespace> ns)
{
    Path path = {"..", "..", "providers", p->value(), ns->value()};
    for (auto prop : ns->props_) {
        path.push_back(prop->value());
        add_symlink(prop->value(), boost::algorithm::join(path, "/"));
        path.pop_back();
    }
}

void NamespaceDir::rm_props(std::shared_ptr<config::Namespace> ns)
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
    void plugin_add(std::shared_ptr<config::Plugin> p)
    {
        auto lock(cor::wlock(*this));
        for (auto ns : p->namespaces_)
            add_dir(ns->value(), mk_dir_entry(new NamespaceDir(p, ns)));
    }

    void plugin_rm(std::shared_ptr<config::Plugin> p)
    {
        auto lock(cor::wlock(*this));
        for (auto ns : p->namespaces_) {
            auto name = ns->value();
            auto dir = dir_entry_impl<NamespaceDir>(dirs.find(name));
            dir->rm_props(ns);
            if (dir->empty()) {
                int rc = unlink_(name);
                if (rc < 0)
                    std::cerr << "can't unlink ns " << name << std::endl;
            }
        }
    }
};


class RootDir : public RODir<DirFactory, FileFactory, cor::NoLock>
{
    typedef RODir<DirFactory, FileFactory, cor::NoLock> base_type;
public:
    RootDir()
        : plugins(new PluginsDir())
        , namespaces(new NamespacesDir())
        , before_readdir([]() { throw cor::Error("No config set"); })
    {
        add_dir("providers", mk_dir_entry(plugins));
        add_dir("namespaces", mk_dir_entry(namespaces));
    }

    void init(std::string const &cfg_dir)
    {
        auto on_config_changed = [this](config::Monitor::Event ev,
                                    std::shared_ptr<config::Plugin> p) {
            switch (ev) {
            case config::Monitor::Added:
                plugin_add(p);
                break;
            case config::Monitor::Removed:
                plugin_rm(p);
                break;
            }
        };
        before_readdir = [cfg_dir, on_config_changed, this]() {
            cfg_mon_.reset(new config::Monitor(cfg_dir, on_config_changed));
        };
    }

    int readdir(void* buf, fuse_fill_dir_t filler,
                off_t offset, fuse_file_info &fi)
    {
        before_readdir();
        before_readdir = []() {};
        return base_type::readdir(buf, filler, offset, fi);
    }

    entry_ptr acquire(std::string const &name)
    {
        before_readdir();
        before_readdir = []() {};
        return base_type::acquire(name);
    }

private:

    void plugin_add(std::shared_ptr<config::Plugin> p)
    {
        auto lock(cor::wlock(*this));
        plugins->add(p);
        namespaces->plugin_add(p);
    }

    void plugin_rm(std::shared_ptr<config::Plugin> p)
    {
        auto lock(cor::wlock(*this));
        trace() << "removing " << p->value() << std::endl;
        namespaces->plugin_rm(p);
        plugins->unlink(p->value());
    }

    std::shared_ptr<PluginsDir> plugins;
    std::shared_ptr<NamespacesDir> namespaces;
    std::function<void ()> before_readdir;
    std::unique_ptr<config::Monitor> cfg_mon_;
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

static FuseFs<RootDirEntry> & fuse()
{
    static auto &fuse = FuseFs<RootDirEntry>::instance();
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
        if (params.size() >= 3)
            return std::get<0>(config::dump(std::cout, params[2]));

        return show_help();
    }

    int save_provider_config()
    {
        if (params.size() < 3)
            return show_help();

        namespace fs = boost::filesystem;
        if (!ensure_dir_exists(cfg_dir))
            return -1;

        return config::save(cfg_dir, params[2]);
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

    int show_help()
    {
        options.show_help(std::cerr, params[0],
                          "[command] [options] [fuse_options]\n"
                          "\t[command]:\n"
                          "\t\tdump plugin_path\n"
                          "\t\tregister plugin_path\n"
                          "\t\tunregister plugin_path\n"
                          "\t[options]:\n");
        std::cerr << "[fuse_options]:\n\n";
        return fuse_run();
    }

    char const *cfg_dir;
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
        std::cerr << "caught: " << e.what() << std::endl;
    }
    return -1;
}
