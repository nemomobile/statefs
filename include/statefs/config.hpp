#ifndef _STATEFS_CONFIG_HPP_
#define _STATEFS_CONFIG_HPP_

#include <cor/sexp.hpp>
#include <cor/notlisp.hpp>

#include <boost/variant.hpp>
#include <fstream>

namespace config
{

static inline std::string cfg_extension()
{
    return ".conf";
}

static inline std::string cfg_provider_prefix()
{
    return "provider";
}

static inline std::string cfg_loader_prefix()
{
    return "loader";
}

namespace nl = cor::notlisp;

typedef boost::variant<long, unsigned long, double, std::string> property_type;
typedef std::unordered_map<std::string, property_type> property_map_type;

void to_property(nl::expr_ptr expr, property_type &dst);
std::string to_string(property_type const &p);
std::string to_nl_string(property_type const &p);

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

    int mode(int umask = 0027) const;

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

class Library : public nl::ObjectExpr
{
public:
    Library(std::string const &name, std::string const &path);

    std::string path;
};

class Plugin : public Library
{
public:
    typedef std::shared_ptr<Namespace> ns_type;
    typedef std::list<ns_type> storage_type;
    Plugin(std::string const &name
           , std::string const &path
           , property_map_type &&info
           , storage_type &&namespaces);

    std::time_t mtime_;
    property_map_type info_;
    storage_type namespaces_;
};

class Loader : public Library
{
public:
    Loader(std::string const &name, std::string const &path);
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
    rest_casted<Library>(res, receiver);
}

typedef std::function<void (std::string const &
                            , std::shared_ptr<Library>) > config_receiver_fn;

bool from_file(std::string const &, config_receiver_fn);
void visit(std::string const &, config_receiver_fn);

class ConfigReceiver
{
public:
    virtual void provider_add(std::shared_ptr<Plugin>) =0;
    virtual void provider_rm(std::shared_ptr<Plugin>) =0;
    virtual void loader_add(std::shared_ptr<Loader>) =0;
    virtual void loader_rm(std::shared_ptr<Loader>) =0;
};


}


#endif // _STATEFS_CONFIG_HPP_
