#include <cor/notlisp.hpp>
#include <cor/options.hpp>

#include "metafuse.hpp"
#include "mt.hpp"

#include <cor/sexp.hpp>

#include <boost/variant.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/join.hpp>

//#include <pthread.h>
//#include <string.h>
#include <iostream>
#include <exception>
#include <unordered_map>
#include <set>
#include <fstream>

namespace nl = cor::notlisp;

typedef boost::variant<long, double, std::string> property_type;

void to_property(nl::expr_ptr expr, property_type &dst)
{
    if (!expr)
        throw cor::Error("to_property: Null");

    switch(expr->type()) {
    case nl::Expr::String:
        dst = expr->value();
        break;
    case nl::Expr::Integer:
        dst = (long)*expr;
        break;
    case nl::Expr::Real:
        dst = (double)*expr;
        break;
    default:
        throw cor::Error("%s is not compatible with Any",
                         expr->value().c_str());
    }
}

class Property : public nl::ObjectExpr
{
public:
    Property(std::string const &name, property_type &defval)
        : ObjectExpr(name), defval_(defval)
    {}
private:
    property_type defval_;
};

class Namespace : public nl::ObjectExpr
{
public:
    typedef std::shared_ptr<Property> prop_type;
    typedef std::list<prop_type> storage_type;
    Namespace(std::string const &name, storage_type &&props)
        : ObjectExpr(name), props_(props)
    {}

    storage_type props_;
};

class Plugin : public nl::ObjectExpr
{
public:
    typedef std::shared_ptr<Namespace> ns_type;
    typedef std::list<ns_type> storage_type;
    Plugin(std::string const &name, std::string const &path,
           storage_type &&namespaces)
        : ObjectExpr(name), namespaces_(namespaces)
    {}

    std::string path;
    storage_type namespaces_;
};

template <typename CharT, typename ReceiverT>
void parse_config
(std::basic_istream<CharT> &input, ReceiverT receiver)
{
    using namespace nl;
    lambda_type plugin = [](env_ptr, expr_list_type &params) {
        ListAccessor src(params);
        std::string name, path;
        src.required(to_string, name).required(to_string, path);

        Plugin::storage_type namespaces;
        push_rest_casted(src, namespaces);
        return expr_ptr(new Plugin(name, path, std::move(namespaces)));
    };

    lambda_type prop = [](env_ptr, expr_list_type &params) {
        ListAccessor src(params);
        std::string name;
        property_type defval;
        src.required(to_string, name).required(to_property, defval);
        expr_ptr res(new Property(name, defval));

        return res;
    };

    lambda_type ns = [](env_ptr, expr_list_type &params) {
        ListAccessor src(params);
        std::string name;
        src.required(to_string, name);

        Namespace::storage_type props;
        push_rest_casted(src, props);
        expr_ptr res(new Namespace(name, std::move(props)));
        return res;
    };

    env_ptr env(new Env
                    ({ mk_record("plugin", plugin),
                            mk_record("ns", ns),
                            mk_record("prop", prop),
                            mk_const("false", "0"),
                    }));

    Interpreter config(env);
    cor::error_tracer([&]() { cor::sexp::parse(input, config); });

    ListAccessor res(config.results());
    rest_casted<Plugin>
        (res, receiver);
    // auto res = std::dynamic_pointer_cast<Plugin>(from);
    // if (!res)
    //     throw cor::Error("Not a plugin");
    
}

template <typename ReceiverT>
void config_from_file(std::string const &cfg_src, ReceiverT receiver)
{
    trace() << "Loading config from " << cfg_src << std::endl;
    std::ifstream input(cfg_src);
    parse_config(input, receiver);
}

namespace fs = boost::filesystem;

template <typename ReceiverT>
void config_from_dir(std::string const &cfg_src, ReceiverT receiver)
{
    trace() << "Config dir " << cfg_src << std::endl;
    std::for_each(fs::directory_iterator(cfg_src),
                  fs::directory_iterator(),
                  [&receiver](fs::directory_entry const &d) {
                      if (d.path().extension() == ".scm")
                          config_from_file(d.path().string(), receiver);
                  });
}

template <typename ReceiverT>
void config_load(char const *cfg_src, ReceiverT receiver)
{
    if (!cfg_src)
        return;

    if (fs::is_regular_file(cfg_src))
        return config_from_file(cfg_src, receiver);

    if (fs::is_directory(cfg_src))
        return config_from_dir(cfg_src, receiver);

    throw cor::Error("Unknown configuration source %s", cfg_src);
}

using namespace metafuse;

typedef FixedSizeFile<12, Mutex> test_file_type;

class PluginsDir : public ReadRmDir<DirStorage, FileStorage, Mutex>
{
public:
    PluginsDir() { }
};

class PluginNsDir : public RODir<DirStorage, FileStorage, Mutex>
{
public:
    PluginNsDir(std::shared_ptr<Namespace> p)
    {
        for (auto prop : p->props_)
            add_file(prop->value(),
                     mk_file_entry(new test_file_type()));
    }
};

class PluginDir : public RODir<DirStorage, FileStorage, Mutex>
{
public:
    PluginDir(std::shared_ptr<Plugin> p)
    {
        for (auto ns : p->namespaces_)
            add_dir(ns->value(), mk_dir_entry(new PluginNsDir(ns)));
    }
};


class NamespaceDir : public RODir<DirStorage, FileStorage, Mutex>
{
public:
    NamespaceDir(std::shared_ptr<Plugin> p, std::shared_ptr<Namespace> ns)
    {
        Path path = {"..", "..", "providers", p->value(), ns->value()};
        for (auto prop : ns->props_) {
            path.push_back(prop->value());
            add_symlink(prop->value(), boost::algorithm::join(path, "/"));
            path.pop_back();
        }
    }
};

class NamespacesDir : public RODir<DirStorage, FileStorage, Mutex>
{
public:
    NamespacesDir()
    {
    }

    void plugin_add(std::shared_ptr<Plugin> p)
    {
        for (auto ns : p->namespaces_)
            add_dir(ns->value(), mk_dir_entry(new NamespaceDir(p, ns)));
    }
};

class ControlDir : public ReadRmDir<DirStorage, FileStorage, Mutex>
{
public:
    ControlDir()
    {
        add_file("plugins", mk_file_entry(new test_file_type()));
    }
};

typedef RODir<DirStorage, FileStorage, NoLock> root_base_type;

class RootDir : public DirEntry<root_base_type>
{
    typedef DirEntry<root_base_type> base_type;
public:

    RootDir()
        : base_type(new root_base_type()),
          plugins(new PluginsDir()),
          namespaces(new NamespacesDir()),
          control(new ControlDir())
    {
        impl_->add_dir("providers", mk_dir_entry(plugins));
        impl_->add_dir("namespaces", mk_dir_entry(namespaces));
        impl_->add_dir("control", mk_dir_entry(control));
    }

    virtual ~RootDir() {}

    void plugin_add(std::shared_ptr<Plugin> p)
    {
        trace() << "Plugin " << p->value() << std::endl;
        plugins->add_dir(p->value(), mk_dir_entry(new PluginDir(p)));
        namespaces->plugin_add(p);
    }

private:
    std::shared_ptr<PluginsDir> plugins;
    std::shared_ptr<NamespacesDir> namespaces;
    std::shared_ptr<ControlDir> control;
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

    auto &fuse = FuseFs<RootDir>::instance();

    using namespace std::placeholders;
    auto plugin_add = std::bind
        (std::mem_fn(&RootDir::plugin_add), &fuse.impl(), _1);
    config_load(cfg_src, plugin_add);
    std::cerr << "ready" << std::endl;

    int res = fuse.main(params.size(), &params[0], true);
    trace() << "FUSE exited: " << res << std::endl;
    return res;
}
