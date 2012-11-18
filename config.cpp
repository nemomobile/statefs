#include "config.hpp"

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

        std::unordered_map<std::string, property_type> options = {
                {"type", "discrete"}
        };
        rest(src, [](expr_ptr &) {},
             [&options](expr_ptr &k, expr_ptr &v) {
                     auto &p = options[k->value()];
                     to_property(v, p);
             });
        unsigned access = Property::Read;
        if (to_string(options["type"]) == "discrete")
                access |= Property::Subscribe;
        expr_ptr res(new Property(name, defval, access));

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

    env_ptr env(new Env({
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
