#include "statefs.hpp"
#include "config.hpp"

#include <statefs/provider.h>
#include <statefs/util.h>

#include <cor/so.hpp>
#include <cor/util.h>
#include <cor/util.hpp>

#include <sys/eventfd.h>

#include <iostream>

#include <stdio.h>
#include <stdbool.h>
#include <poll.h>

namespace config
{

template <typename CharT>
std::basic_ostream<CharT> & operator <<
(std::basic_ostream<CharT> &out, Property const &src)
{
    out << "\n";
    out << "(" << "prop" << " \"" << src.value() << "\" "
         << " \""<< src.defval() << "\" ";
    unsigned access = src.access();
    if (!(access & Property::Subscribe))
        out << " :behavior continuous";
    if (access & Property::Write) {
        if (access & Property::Read)
            out << " :access rw";
        else
            out << " :access wonly";
    }
    out << ")";

    return out;
}

template <typename CharT>
std::basic_ostream<CharT> & operator <<
(std::basic_ostream<CharT> &out, Namespace const &src)
{
    out << "\n";
    out << "(" << "ns" << " \"" << src.value() << "\"";
    for(auto &prop: src.props_)
        out << *prop;
    out << ")";

    return out;
}

template <typename CharT>
std::basic_ostream<CharT> & operator <<
(std::basic_ostream<CharT> &out, Plugin const &src)
{
    out << "(" << "provider" << " \"" << src.value() << "\"";

    out << " \"" << src.path << "\"";
    for (auto &prop: src.info_)
        out << " :" << prop.first << " " << to_nl_string(prop.second);
    for(auto &ns: src.namespaces_)
        out << *ns;
    out << ")\n";

    return out;
}

template <typename CharT>
std::basic_ostream<CharT> & operator <<
(std::basic_ostream<CharT> &out, Loader const &src)
{
    out << "(" << "loader" << " \"" << src.value() << "\"";

    out << " \"" << src.path << "\"";
    out << ")\n";

    return out;
}

static property_type statefs_variant_2prop(struct statefs_variant const *src)
{
    property_type res;
    switch (src->tag)
    {
    case statefs_variant_int:
        res = src->i;
        break;
    case statefs_variant_uint:
        res = src->u;
        break;
    case statefs_variant_bool:
        res = (long)src->b;
        break;
    case statefs_variant_real:
        res = src->r;
        break;
    case statefs_variant_cstr:
        res = src->s;
        break;
    default:
        res = "";
    }
    return res;
}

namespace fs = boost::filesystem;

template <typename ReceiverT>
void from_dir(std::string const &cfg_src, ReceiverT receiver)
{
    trace() << "Config dir " << cfg_src << std::endl;
    std::for_each(fs::directory_iterator(cfg_src),
                  fs::directory_iterator(),
                  [&receiver](fs::directory_entry const &d) {
                      auto path = d.path();
                      if (path.extension() == provider::cfg_extension())
                          from_file(path.string(), receiver);
                  });
}

template <typename ReceiverT>
void load(std::string const &cfg_src, ReceiverT receiver)
{
    if (cfg_src.empty())
        return;

    if (fs::is_regular_file(cfg_src))
        return from_file(cfg_src, receiver);

    if (fs::is_directory(cfg_src))
        return from_dir(cfg_src, receiver);

    throw cor::Error("Unknown configuration source %s", cfg_src.c_str());
}

namespace nl = cor::notlisp;

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

struct AnyToString : public boost::static_visitor<>
{
    std::string &dst;

    AnyToString(std::string &res);

    void operator () (std::string const &v) const;

    template <typename T>
    void operator () (T &v) const
    {
        std::stringstream ss;
        ss << v;
        dst = ss.str();
    }
};

AnyToString::AnyToString(std::string &res) : dst(res) {}

void AnyToString::operator () (std::string const &v) const
{
    dst = v;
}

std::string to_string(property_type const &p)
{
    std::string res;
    boost::apply_visitor(AnyToString(res), p);
    return res;
}

/// convert property_type to notlisp printable value
struct AnyToOutString : public boost::static_visitor<>
{
    std::string &dst;

    AnyToOutString(std::string &res);

    void operator () (std::string const &v) const;

    template <typename T>
    void operator () (T &v) const
    {
        std::stringstream ss;
        ss << v;
        dst = ss.str();
    }
};

AnyToOutString::AnyToOutString(std::string &res) : dst(res) {}

void AnyToOutString::operator () (std::string const &v) const
{
    dst = "\"";
    dst += (v + "\"");
}

std::string to_nl_string(property_type const &p)
{
    std::string res;
    boost::apply_visitor(AnyToOutString(res), p);
    return res;
}

Property::Property(std::string const &name,
                   property_type const &defval,
                   unsigned access)
    : ObjectExpr(name), defval_(defval), access_(access)
{}

Namespace::Namespace(std::string const &name, storage_type &&props)
    : ObjectExpr(name), props_(props)
{}

Plugin::Plugin(std::string const &name
               , std::string const &path
               , property_map_type &&info
               , storage_type &&namespaces)
    : ObjectExpr(name)
    , path(path)
    , mtime_(fs::last_write_time(path))
    , info_(info)
    , namespaces_(namespaces)
{}

Loader::Loader(std::string const &name, std::string const &path)
    : ObjectExpr(name)
    , path(path)
{}

struct PropertyInt : public boost::static_visitor<>
{
    long &dst;

    PropertyInt(long &res) : dst(res) {}

    void operator () (long v) const
    {
        dst = v;
    }

    template <typename OtherT>
    void operator () (OtherT &v) const
    {
        throw cor::Error("Wrong property type");
    }
};

long to_integer(property_type const &src)
{
    long res;
    boost::apply_visitor(PropertyInt(res), src);
    return res;
}

std::string Property::defval() const
{
    return to_string(defval_);
}

int Property::mode(int umask) const
{
    int res = 0;
    if (access_ & Read)
        res |= 0444;
    if (access_ & Write)
        res |= 0222;
    return res & ~umask;
}

nl::env_ptr mk_parse_env()
{
    using nl::env_ptr;
    using nl::expr_list_type;
    using nl::lambda_type;
    using nl::expr_ptr;

    lambda_type plugin = [](env_ptr, expr_list_type &params) {
        nl::ListAccessor src(params);
        std::string name, path;
        src.required(nl::to_string, name).required(nl::to_string, path);

        Plugin::storage_type namespaces;
        property_map_type options = {
            // default option values
            {"type", "default"}
        };
        nl::rest(src
                 , [&namespaces](expr_ptr &v) {
                     auto ns = std::dynamic_pointer_cast<Namespace>(v);
                     if (!ns)
                         throw cor::Error("Can't be casted to Namespace");
                     namespaces.push_back(ns);
                 }
                 , [&options](expr_ptr &k, expr_ptr &v) {
                     auto &p = options[k->value()];
                     to_property(v, p);
                 });
        return nl::expr_ptr(new Plugin(name, path
                                       , std::move(options)
                                       , std::move(namespaces)));
    };

    lambda_type prop = [](env_ptr, expr_list_type &params) {
        nl::ListAccessor src(params);
        std::string name;
        property_type defval;
        src.required(nl::to_string, name).required(to_property, defval);

        property_map_type options = {
            // default option values
            {"behavior", "discrete"},
            {"access", (long)Property::Read}
        };
        nl::rest(src, [](expr_ptr &) {},
                 [&options](expr_ptr &k, expr_ptr &v) {
                     auto &p = options[k->value()];
                     to_property(v, p);
             });
        unsigned access = to_integer(options["access"]);
        if (to_string(options["behavior"]) == "discrete")
                access |= Property::Subscribe;

        nl::expr_ptr res(new Property(name, defval, access));

        return res;
    };

    lambda_type ns = [](env_ptr, expr_list_type &params) {
        nl::ListAccessor src(params);
        std::string name;
        src.required(nl::to_string, name);

        Namespace::storage_type props;
        nl::push_rest_casted(src, props);
        nl::expr_ptr res(new Namespace(name, std::move(props)));
        return res;
    };

    using nl::mk_record;
    using nl::mk_const;
    env_ptr env(new nl::Env({
                mk_record("provider", plugin),
                    mk_record("ns", ns),
                    mk_record("prop", prop),
                    mk_const("false", 0),
                    mk_const("true", 1),
                    mk_const("discrete", Property::Subscribe),
                    mk_const("continuous", 0),
                    mk_const("rw", Property::Write | Property::Read),
                    mk_const("wonly", Property::Write),
                    }));
    return env;
}

nl::env_ptr mk_loader_parse_env()
{
    nl::lambda_type loader = [](nl::env_ptr, nl::expr_list_type &params) {
        nl::ListAccessor src(params);
        std::string name, path;
        src.required(nl::to_string, name).required(nl::to_string, path);

        return nl::expr_ptr(new Loader(name, path));
    };

    nl::env_ptr env(new nl::Env({
                nl::mk_record("loader", loader)
                    }));
    return env;
}

namespace inotify = cor::inotify;

Monitor::Monitor
(std::string const &path, on_changed_type on_changed)
    : path_([](std::string const &path) {
            trace() << "Config monitor for " << path << std::endl;
            if (!ensure_dir_exists(path))
                throw cor::Error("No config dir %s", path.c_str());
            return path;
        }(path))
    , event_(eventfd(0, 0), cor::only_valid_handle)
    , on_changed_(on_changed)
    , watch_(new inotify::Watch
             (inotify_, path, IN_CREATE | IN_DELETE | IN_MODIFY))
      // run thread before loading config to avoid missing configuration
    , mon_thread_(std::bind(std::mem_fn(&Monitor::watch_thread), this))
{
    using namespace std::placeholders;
    config::load(path_, std::bind(std::mem_fn(&Monitor::plugin_add),
                                  this, _1, _2));
}

Monitor::~Monitor()
{
    uint64_t v = 1;
    ::write(event_.value(), &v, sizeof(v));
    trace() << "config monitor: waiting to be stopped\n";
    mon_thread_.join();
}

void Monitor::plugin_add(std::string const &cfg_path,
                         Monitor::plugin_ptr p)
{
    auto fname = fs::path(cfg_path).filename().string();
    files_providers_[fname] = p;
    on_changed_(Added, p);
}

int Monitor::watch_thread()
{
    try {
        return watch();
    } catch (std::exception const &e) {
        std::cerr << "Config watcher caught " << e.what() << std::endl;
    }
    return -1;
}

bool Monitor::process_poll()
{
    std::cerr << "Providers config is maybe changed" << std::endl;
    if (fds_[1].revents) {
        uint64_t v;
        ::read(event_.value(), &v, sizeof(v));
        watch_.reset(nullptr);
        return false;
    }

    if (!fds_[0].revents)
        return true;

    char buf[sizeof(inotify_event) + 256];
    int rc;
    // read all events
    while ((rc = inotify_.read(buf, sizeof(buf))) > (int)sizeof(buf)) {}

    // configuration is changed rarely (only on
    // un/installation of plugins), so it is simplier and more
    // robust just to iterate through 'em and calculate
    // changes each time anything changed in the configuration
    // directory
    std::unordered_map<std::string, std::string> cur_config_paths;
    typedef std::pair<std::string, std::time_t> file_info_type;
    std::set<file_info_type> cur, prev;
    std::for_each
        (fs::directory_iterator(path_), fs::directory_iterator(),
         [&](fs::directory_entry const &d) {
            auto p = d.path();
            if (p.extension() == provider::cfg_extension())
                cur_config_paths[p.filename().string()]
                    = fs::canonical(p).string();
        });
    for (auto &kv : cur_config_paths) {
        auto const& fname = kv.first;
        auto mtime = fs::last_write_time(fs::path(path_) / fname);
        cur.insert({fname, mtime});
    }

    for (auto &kv : files_providers_) {
        auto const& fname = kv.first;
        auto mtime = fs::last_write_time(kv.second->path);
        prev.insert({fname, mtime});
    }

    std::list<file_info_type> added, removed;
    std::set_difference(cur.begin(), cur.end(),
                        prev.begin(), prev.end(),
                        std::back_inserter(added));
    std::set_difference(prev.begin(), prev.end(),
                        cur.begin(), cur.end(),
                        std::back_inserter(removed));
    for (auto &nt : removed) {
        auto const& v = nt.first;
        std::cerr << "Removed " << v << std::endl;
        on_changed_(Removed, files_providers_[v]);
    }
    for (auto &nt : added) {
        auto const& v = nt.first;
        std::cerr << "Added " << v << std::endl;
        using namespace std::placeholders;
        from_file(cur_config_paths[v],
                  std::bind(std::mem_fn(&Monitor::plugin_add), this, _1, _2));
    }

    return true;
}

int Monitor::watch()
{
    int rc;

    fds_.fill({-1, POLLIN | POLLPRI, 0});
    fds_[0].fd = inotify_.fd();
    fds_[1].fd = event_.value();

    while ((rc = poll(&fds_[0], fds_.size(), -1)) >= 0
           && process_poll()) {}

    std::cerr << "exiting config watch poll rc=" << rc << std::endl;
    return rc;
}

static std::string statefs_variant_2str(struct statefs_variant const *src)
{
    std::stringstream ss;
    switch (src->tag)
    {
    case statefs_variant_int:
        ss << src->i;
        break;
    case statefs_variant_uint:
        ss << src->u;
        break;
    case statefs_variant_bool:
        ss << (src->b ? "1" : "0");
        break;
    case statefs_variant_real:
        ss << src->r;
        break;
    case statefs_variant_cstr:
        ss << "\"" << src->s << "\"";
        break;
    default:
        return "\"\"";
    }
    return ss.str();
}


class Dump
{
public:
    Dump(std::ostream &out, provider_handle_type &&provider)
        : out(out), provider_(std::move(provider)) {}

    std::string dump(std::string const&);

private:

    Dump(Dump const&);
    Dump & operator = (Dump const&);

    void dump_info(int level, statefs_node const *node);
    void dump_prop(int level, statefs_property const *prop);
    void dump_ns(int level, statefs_namespace const *ns);

    std::ostream &out;
    provider_handle_type provider_;
};

void Dump::dump_info(int level, statefs_node const *node)
{
    if (!node->info)
        return;

    auto info = node->info;
    while (info->name) {
        out << " :" << info->name << " "
            << statefs_variant_2str(&info->value);
        ++info;
    }
}

void Dump::dump_prop(int level, statefs_property const *prop)
{
    out << "\n";
    out << "(" << "prop" << " \"" << prop->node.name << "\" "
        << statefs_variant_2str(&prop->default_value);
    dump_info(level, &prop->node);
    int attr = provider_->io.getattr(prop);
    if (!(attr & STATEFS_ATTR_DISCRETE))
        out << " :behavior continuous";
    if (attr & STATEFS_ATTR_WRITE)
        out << " :access rw";
    out << ")";
}

typedef cor::Handle<
    cor::GenericHandleTraits<
        intptr_t, 0> > branch_handle_type;

void Dump::dump_ns(int level, statefs_namespace const *ns)
{
    out << "\n";
    out << "(" << "ns" << " \"" << ns->node.name << "\"";
    dump_info(level, &ns->node);

    branch_handle_type iter
        (statefs_first(&ns->branch),
         [&ns](intptr_t v) {
            statefs_branch_release(&ns->branch, v);
        });
    auto next = [&ns, &iter]() {
        return mk_property_handle(statefs_prop_get(&ns->branch, iter.value()));
    };
    auto prop = next();
    while (prop) {
        dump_prop(level + 1, prop.get());
        statefs_next(&ns->branch, &iter.ref());
        prop = next();
    }
    out << ")";
}

std::string Dump::dump(std::string const& path)
{
    if (!provider_) {
        std::cerr << "Provider " << path << " is not loaded" << std::endl;
        return "";
    }
    auto const &root = provider_->root;
    auto provider_name = root.node.name;
    out << "(" << "provider" << " \"" << provider_name << "\"";
    dump_info(0, &root.node);
    out << " \"" << path << "\"";
    property_map_type props = {{"type", "default"}};
    auto info = root.node.info;
    if (info) {
        while (info->name) {
            props[info->name] = statefs_variant_2prop(&info->value);
            ++info;
        }
    }
    branch_handle_type iter
        (statefs_first(&root.branch),
         [&root](intptr_t v) {
            statefs_branch_release(&root.branch, v);
        });

    auto next = [&root, &iter]() {
        return mk_namespace_handle
        (statefs_ns_get(&root.branch, iter.value()));
    };
    auto ns = next();
    while (ns) {
        dump_ns(1, ns.get());
        statefs_next(&root.branch, &iter.ref());
        ns = next();
    }
    out << ")\n";
    return provider_name;
}

static std::string mk_provider_path(std::string const &path)
{
    namespace fs = boost::filesystem;
    auto provider_path = fs::path(path);
    provider_path = fs::canonical(provider_path);
    return provider_path.generic_string();
}

static inline std::string dump_provider_cfg_file
(std::ostream &dst, fs::path const &path)
{
    std::string plugin_name("");
    auto dump_plugin = [&dst, &plugin_name]
        (std::string const &cfg_path, Monitor::plugin_ptr p) {
        if (p) {
            dst << *p;
            plugin_name = p->value();
        }
    };
    from_file(path.native(), dump_plugin);
    std::cerr << "Plugin: " << plugin_name << std::endl;
    return plugin_name;
}

static inline std::string dump_provider
(std::ostream &dst, fs::path const &path)
{
    cor::SharedLib lib(path.native(), RTLD_LAZY);
    if (!lib.is_loaded()) {
        throw cor::Error("Can't load library %s: %s"
                         , path.c_str(), ::dlerror());
    }

    return Dump(dst, mk_provider_handle(lib)).dump(path.native());
}

/**
 * dumps configuration of provider/loader (shared library or conf
 * file). If configuration file is passed to the function it just
 * parse and dump it, if binary - extracts configuration information
 * in configuration file format
 *
 * @param dst output stream
 * @param path path to file to be introspected
 *
 * @return provider name
 */
std::string dump(std::ostream &dst, std::string const &path)
{
    auto full_path = fs::path(mk_provider_path(path));
    if (full_path.extension() == provider::cfg_extension()) {
        return dump_provider_cfg_file(dst, full_path);
    } else {
        return dump_provider(dst, full_path);
    }
}

/**
 * saves provider/loader metadata in parsable/readable configuration
 * format
 *
 * @param cfg_dir destination directory
 * @param fname subject (so, conf file...) file name
 */
void save(std::string const &cfg_dir, std::string const &fname)
{
    namespace fs = boost::filesystem;

    std::stringstream ss;
    auto name = dump(ss, fname);

    auto cfg_path = fs::path(cfg_dir);
    cfg_path /= (name + provider::cfg_extension());
    std::ofstream out(cfg_path.generic_string());
    out << ss.str();
    out.close();
    // touch configuration directory, be sure dir monitor watch will
    // observe changes
    std::time_t n = std::time(0);
    fs::last_write_time(cfg_dir, n);
}

} // config
