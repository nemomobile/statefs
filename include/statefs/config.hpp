#ifndef _STATEFS_CONFIG_HPP_
#define _STATEFS_CONFIG_HPP_

#include <cor/sexp.hpp>
#include <cor/notlisp.hpp>

#include <boost/variant.hpp>
#include <fstream>

namespace config
{



static inline std::string file_ext()
{
    return ".scm";
}

namespace nl = cor::notlisp;

typedef boost::variant<long, double, std::string> property_type;

void to_property(nl::expr_ptr expr, property_type &dst);
std::string to_string(property_type const &p);

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

long to_integer(property_type const &src);

class Property : public nl::ObjectExpr
{
public:
    enum Access
    {
        Read = 1,
        Write = 2,
        Subscribe = 4
    };

    Property(std::string const &name,
             property_type const &defval,
             unsigned access = Read);

    std::string defval() const;

    unsigned access() const
    {
        return access_;
    }

    int mode(int umask = 0022) const;

private:
    property_type defval_;
    unsigned access_;
};

class Namespace : public nl::ObjectExpr
{
public:
    typedef std::shared_ptr<Property> prop_type;
    typedef std::list<prop_type> storage_type;
    Namespace(std::string const &name, storage_type &&props);

    storage_type props_;
};

class Plugin : public nl::ObjectExpr
{
public:
    typedef std::shared_ptr<Namespace> ns_type;
    typedef std::list<ns_type> storage_type;
    Plugin(std::string const &name, std::string const &path,
           storage_type &&namespaces);

    std::string path;
    storage_type namespaces_;
};

nl::env_ptr mk_parse_env();

template <typename CharT, typename ReceiverT>
void parse(std::basic_istream<CharT> &input, ReceiverT receiver)
{
    using namespace nl;

    env_ptr env(mk_parse_env());

    Interpreter config(env);
    cor::error_tracer([&input, &config]()
                      { cor::sexp::parse(input, config); });

    ListAccessor res(config.results());
    rest_casted<Plugin>(res, receiver);
}

template <typename ReceiverT>
void from_file(std::string const &cfg_src, ReceiverT receiver)
{
    trace() << "Loading config from " << cfg_src << std::endl;
    std::ifstream input(cfg_src);
    try {
        using namespace std::placeholders;
        parse(input, std::bind(receiver, cfg_src, _1));
    } catch (...) {
        std::cerr << "Error parsing " << cfg_src << ":" << input.tellg()
                  << ", skiping..." << std::endl;
    }
}

}


#endif // _STATEFS_CONFIG_HPP_
