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
                mk_record("provider", plugin),
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
                         std::shared_ptr<config::Plugin> p)
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
    auto const &root = provider_->root;
    auto provider_name = root.node.name;
    out << "(" << "provider" << " \"" << provider_name << "\"";
    dump_info(0, &root.node);
    out << " \"" << path << "\"";
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

std::string dump(std::ostream &dst, std::string const &path)
{
    auto full_path = mk_provider_path(path);
    cor::SharedLib lib(full_path, RTLD_LAZY);

    if (!lib.is_loaded()) {
        throw cor::Error("Can't load library %s: %s"
                         , path.c_str(), ::dlerror());
    }

    return Dump(dst, mk_provider_handle(lib)).dump(full_path);
}

void save(std::string const &cfg_dir, std::string const &provider_fname)
{
    namespace fs = boost::filesystem;

    std::stringstream ss;
    auto name = dump(ss, provider_fname);

    auto cfg_path = fs::path(cfg_dir);
    cfg_path /= (name + config::file_ext());
    std::ofstream out(cfg_path.generic_string());
    out << ss.str();
}

} // config
