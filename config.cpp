#include "statefs.hpp"
#include "config.hpp"

#include <statefs/provider.h>
#include <statefs/util.h>

#include <cor/so.hpp>
#include <cor/util.h>

#include <sys/eventfd.h>

#include <iostream>

#include <stdio.h>
#include <stdbool.h>
#include <poll.h>

namespace config
{

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

std::string to_string(property_type const &p)
{
    std::string res;
    boost::apply_visitor(AnyToString(res), p);
    return res;
}

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
    nl::lambda_type plugin = [](env_ptr, nl::expr_list_type &params) {
        nl::ListAccessor src(params);
        std::string name, path;
        src.required(nl::to_string, name).required(nl::to_string, path);

        Plugin::storage_type namespaces;
        push_rest_casted(src, namespaces);
        return nl::expr_ptr(new Plugin(name, path, std::move(namespaces)));
    };

    nl::lambda_type prop = [](env_ptr, nl::expr_list_type &params) {
        nl::ListAccessor src(params);
        std::string name;
        property_type defval;
        src.required(nl::to_string, name).required(to_property, defval);

        std::unordered_map<std::string, property_type> options = {
            // default option values
            {"behavior", "discrete"},
            {"access", (long)Property::Read}
        };
        nl::rest(src, [](nl::expr_ptr &) {},
                 [&options](nl::expr_ptr &k, nl::expr_ptr &v) {
                     auto &p = options[k->value()];
                     to_property(v, p);
             });
        unsigned access = to_integer(options["access"]);
        if (to_string(options["behavior"]) == "discrete")
                access |= Property::Subscribe;

        nl::expr_ptr res(new Property(name, defval, access));

        return res;
    };

    nl::lambda_type ns = [](env_ptr, nl::expr_list_type &params) {
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
                mk_record("plugin", plugin),
                    mk_record("ns", ns),
                    mk_record("prop", prop),
                    mk_const("false", 0),
                    mk_const("true", 0),
                    mk_const("discrete", Property::Subscribe),
                    mk_const("continuous", 0),
                    mk_const("rw", Property::Write | Property::Read),
                    }));
    return env;
}

namespace inotify = cor::inotify;

Monitor::Monitor
(std::string const &path, on_changed_type on_changed)
    : path_(path)
    , event_(eventfd(0, 0))
    , on_changed_(on_changed)
{
    trace() << "Config monitor for " << path << std::endl;
    if (!event_.is_valid())
        throw cor::Error("Event was not initialized\n");

    watch_.reset(new inotify::Watch(inotify_, path,
                                    IN_CREATE | IN_DELETE | IN_MODIFY));

    using namespace std::placeholders;
    std::packaged_task<int()> task
        (std::bind(std::mem_fn(&Monitor::watch_thread), this));
    thread_res_ = task.get_future();
    // run thread before loading config to avoid missing configuration
    std::thread(std::move(task)).detach();
    load();
}

Monitor::~Monitor()
{
    uint64_t v = 1;
    ::write(event_.fd, &v, sizeof(v));
    trace() << "config monitor: waiting to be stopped\n";
    thread_res_.wait();
}

void Monitor::plugin_add(std::string const &cfg_path,
                         std::shared_ptr<config::Plugin> p)
{
    auto fname = fs::path(cfg_path).filename().string();
    files_providers_[fname] = p;
    on_changed_(Added, p);
}

void Monitor::load()
{
    if (!ensure_dir_exists(path_))
        throw cor::Error("No config dir %s", path_.c_str());

    using namespace std::placeholders;
    config::load(path_, std::bind(std::mem_fn(&Monitor::plugin_add),
                                  this, _1, _2));
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
    if (fds_[1].revents) {
        uint64_t v;
        ::read(event_.fd, &v, sizeof(v));
        watch_.reset(nullptr);
        return false;
    }

    if (!fds_[0].revents)
        return true;

    char buf[sizeof(inotify_event) + 256];
    int rc;
    // read all events
    while ((rc = inotify_.read(buf, sizeof(buf))) > sizeof(buf)) {}
        
    // configuration is changed rarely (only on
    // un/installation of plugins), so it is simplier and more
    // robust just to iterate through 'em and calculate
    // changes each time anything changed in the configuration
    // directory
    std::unordered_map<std::string, std::string> cur_config_paths;
    std::set<std::string> cur, prev;
    std::for_each
        (fs::directory_iterator(path_), fs::directory_iterator(),
         [&](fs::directory_entry const &d) {
            auto p = d.path();
            if (d.path().extension() == file_ext())
                cur_config_paths[p.filename().string()]
                    = fs::canonical(p).string();
        });
    for (auto &kv : cur_config_paths)
        cur.insert(kv.first);

    for (auto &kv : files_providers_)
        prev.insert(kv.first);

    std::list<std::string> added, removed;
    std::set_difference(cur.begin(), cur.end(),
                        prev.begin(), prev.end(),
                        std::back_inserter(added));
    std::set_difference(prev.begin(), prev.end(),
                        cur.begin(), cur.end(),
                        std::back_inserter(removed));
    for (auto &v : added) {
        using namespace std::placeholders;
        from_file(cur_config_paths[v],
                  std::bind(std::mem_fn(&Monitor::plugin_add), this, _1, _2));
    }
    for (auto &v : removed)
        on_changed_(Removed, files_providers_[v]);

    return true;
}

int Monitor::watch()
{
    int rc;

    fds_.fill({-1, POLLIN | POLLPRI, 0});
    fds_[0].fd = inotify_.fd();
    fds_[1].fd = event_.fd;

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
        return src->s;
    default:
        return "";
    }
    return ss.str();
}

class Dump
{
public:
    Dump(std::ostream &out) : out(out) {}

    int operator ()(std::string const &, std::string &);

private:

    void dump_info(int level, statefs_node const *node, char const *kind);
    void dump_prop(int level, statefs_property const *prop);
    void dump_ns(int level, statefs_namespace const *ns);

    std::ostream &out;
};

void Dump::dump_info(int level, statefs_node const *node, char const *kind)
{
    out << "(" << kind << " \"" << node->name << "\"";

    if (node->info) {
        auto info = node->info;
        while (info->name) {
            out << " :" << info->name << " "
                << statefs_variant_2str(&info->value);
            ++info;
        }
    }
}

void Dump::dump_prop(int level, statefs_property const *prop)
{
    out <<"\n";
    dump_info(level, &prop->node, "prop");
    if (!prop->connect)
        out << " :behavior continuous";
    out << ")";
}

void Dump::dump_ns(int level, statefs_namespace const *ns)
{
    out << "\n";
    dump_info(level, &ns->node, "ns");

    intptr_t iter = statefs_first(&ns->branch);
    auto prop = statefs_prop_get(&ns->branch, iter);
    while (prop) {
        dump_prop(level + 1, prop);
        statefs_next(&ns->branch, &iter);
        prop = statefs_prop_get(&ns->branch, iter);
    }
    out << ")";
}

int Dump::operator ()(std::string const &path, std::string &provider_name)
{
    static const char *provider_main_fn_name = "statefs_provider_get";
    cor::SharedLib lib(path, RTLD_LAZY);

    if (!lib.is_loaded()) {
        std::cerr << "Can't load plugin " << path << "\n";
        return -1;
    }
    auto fn = lib.sym<statefs_provider_fn>(provider_main_fn_name);
    if (!fn) {
        std::cerr << "Can't find " << provider_main_fn_name
                  << " fn in " << path << "\n";
        return -1;
    }
    auto provider = fn();

    provider_name = provider->node.name;
    dump_info(0, &provider->node, "provider");
    out << " :path \"" << path << "\"";
    intptr_t pns = statefs_first(&provider->branch);
    auto ns = statefs_ns_get(&provider->branch, pns);
    while (ns) {
        dump_ns(1, ns);
        statefs_next(&provider->branch, &pns);
        ns = statefs_ns_get(&provider->branch, pns);
    }
    out << ")\n";
    return 0;
}


static std::string mk_provider_path(std::string const &path)
{
    namespace fs = boost::filesystem;
    auto provider_path = fs::path(path);
    provider_path = fs::canonical(provider_path);
    return provider_path.generic_string();
}


std::tuple<int, std::string> dump(std::ostream &dst, std::string const &path)
{
    std::string provider_name;
    int rc = Dump(dst)(mk_provider_path(path), provider_name);
    return std::make_tuple(rc, provider_name);
}

int save(std::string const &cfg_dir,
         std::string const &provider_fname)
{
    namespace fs = boost::filesystem;

    auto provider_path = fs::path(provider_fname);
    provider_path = fs::absolute(provider_path);

    std::stringstream ss;
    auto res = config::dump(ss, mk_provider_path(provider_fname));
    int rc = std::get<0>(res);
    if (!rc)
        return rc;

    auto cfg_path = fs::path(cfg_dir);
    cfg_path /= (std::get<1>(res) + config::file_ext());
    std::ofstream out(cfg_path.generic_string());
    out << ss.str();
    return 0;
}

} // config
