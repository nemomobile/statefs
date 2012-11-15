#ifndef _STATEFS_CONFIG_HPP_
#define _STATEFS_CONFIG_HPP_

#include <cor/notlisp.hpp>
#include <cor/options.hpp>
#include <cor/sexp.hpp>

#include <boost/variant.hpp>
#include <boost/filesystem.hpp>


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

struct AnyToString : public boost::static_visitor<>
{
    std::string &dst;

    AnyToString(std::string &res) : dst(res) {}

    void operator () (std::string const &v) const
    {
        dst = v;
    }

    template <typename T>
    void operator () (T &v) const
    {
        std::stringstream ss;
        ss << v;
        dst = ss.str();
    }
};


class Property : public nl::ObjectExpr
{
public:
    Property(std::string const &name, property_type &defval)
        : ObjectExpr(name), defval_(defval)
    {}
    std::string defval() const
    {
        std::string res;
        boost::apply_visitor(AnyToString(res), defval_);
        return res;
    }

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

#endif // _STATEFS_CONFIG_HPP_
