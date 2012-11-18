#include <metafuse.hpp>
#include <cor/mt.hpp>
#include <cor/so.hpp>
#include "config.hpp"

#include <boost/algorithm/string/join.hpp>

//#include <pthread.h>
//#include <string.h>
#include <iostream>
#include <exception>
#include <unordered_map>
#include <set>
#include <fstream>

using namespace metafuse;

template <typename LoadT>
class PluginLoadFile :
    public DefaultFile<PluginLoadFile<LoadT> >
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
private:
    LoadT load_;
    size_t size_;
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
    PluginNsDir(std::shared_ptr<Namespace> info, PluginLoaderT plugin_load)
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

    void load(std::shared_ptr<cor::SharedLib>)
    {
        auto lock(cor::wlock(*this));
        files.clear();
        for (auto prop : info_->props_) {
            std::string name = prop->value();
            add_file(name, mk_file_entry(new BasicTextFile<>(prop->defval())));
        }
    }

    void load_fake()
    {
        auto lock(cor::wlock(*this));
        files.clear();
        for (auto prop : info_->props_) {
            std::string name = prop->value();
            add_file(name, mk_file_entry(new BasicTextFile<>(prop->defval())));
        }
    }

private:

    std::shared_ptr<Namespace> info_;
};

class PluginDir : public RODir<DirFactory, FileFactory, cor::Mutex>
{
public:
    typedef DirEntry<PluginNsDir> ns_type;

    PluginDir(std::shared_ptr<Plugin> info);
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

    std::shared_ptr<Plugin> info_;
    std::shared_ptr<cor::SharedLib> plugin_;
};

PluginDir::PluginDir(std::shared_ptr<Plugin> info)
    : info_(info)
{
    auto plugin_load = std::bind(&PluginDir::load, this);
    for (auto ns : info->namespaces_)
        add_dir(ns->value(), mk_dir_entry(new PluginNsDir(ns, plugin_load)));
}

void PluginDir::load()
{
    auto lock(cor::wlock(*this));
    if (plugin_)
        return;

    trace() << "Loading plugin " << info_->path << std::endl;
    plugin_.reset(new cor::SharedLib(info_->path, RTLD_LAZY));
        
    if (!plugin_->is_loaded()) {
        std::cerr << "Can't load " << info_->path
                  << ", using fake values" << std::endl; 
        namespaces_init(&PluginNsDir::load_fake);
        return;
    }
    namespaces_init(&PluginNsDir::load, plugin_);
}

class PluginsDir : public ReadRmDir<DirFactory, FileFactory, cor::Mutex>
{
public:
    PluginsDir() { }

    void add(std::shared_ptr<Plugin> p)
    {
        trace() << "Plugin " << p->value() << std::endl;
        auto d = std::make_shared<PluginDir>(p);
        add_dir(p->value(), mk_dir_entry(d));
    }
};

class NamespaceDir : public RODir<DirFactory, FileFactory, cor::Mutex>
{
public:
    NamespaceDir(std::shared_ptr<Plugin> p, std::shared_ptr<Namespace> ns);
};

NamespaceDir::NamespaceDir(std::shared_ptr<Plugin> p, std::shared_ptr<Namespace> ns)
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
    void plugin_add(std::shared_ptr<Plugin> p)
    {
        auto lock(cor::wlock(*this));
        for (auto ns : p->namespaces_)
            add_dir(ns->value(), mk_dir_entry(new NamespaceDir(p, ns)));
    }
};

class ControlDir : public ReadRmDir<DirFactory, FileFactory, cor::Mutex>
{
public:
    ControlDir()
    {
        add_file("plugins",
                 mk_file_entry
                 (new FixedSizeFile<12, cor::Mutex>()));
    }
};

class RootDir : public RODir<DirFactory, FileFactory, cor::NoLock>
{
public:
    RootDir() :
        plugins(new PluginsDir()),
        namespaces(new NamespacesDir()),
        control(new ControlDir())
    {
        add_dir("providers", mk_dir_entry(plugins));
        add_dir("namespaces", mk_dir_entry(namespaces));
        add_dir("control", mk_dir_entry(control));
    }

    void plugin_add(std::shared_ptr<Plugin> p)
    {
        plugins->add(p);
        namespaces->plugin_add(p);
    }

private:
    std::shared_ptr<PluginsDir> plugins;
    std::shared_ptr<NamespacesDir> namespaces;
    std::shared_ptr<ControlDir> control;
};

class RootDirEntry : public DirEntry<RootDir>
{
    typedef DirEntry<RootDir> base_type;
public:

    RootDirEntry() : base_type(new RootDir()) {}
    virtual ~RootDirEntry() {}

    void plugin_add(std::shared_ptr<Plugin> p)
    {
        impl_->plugin_add(p);
    }

};


int main(int argc, char *argv[])
{
    cor::OptParse::map_type opts;
    std::vector<char const*> params;
    cor::OptParse options({{"C", "config"}},
                          {{"config", "config"}},
                          {"config"});
    options.parse(argc, argv, opts, params);
    
    char const *cfg_src = nullptr;
    auto p = opts.find("config");
    if (p != opts.end())
        cfg_src = p->second;

    auto &fuse = FuseFs<RootDirEntry>::instance();

    using namespace std::placeholders;
    auto plugin_add = std::bind
        (std::mem_fn(&RootDirEntry::plugin_add), &fuse.impl(), _1);
    config_load(cfg_src, plugin_add);
    std::cerr << "ready" << std::endl;

    int res = fuse.main(params.size(), &params[0], true);
    trace() << "FUSE exited: " << res << std::endl;
    return res;
}
