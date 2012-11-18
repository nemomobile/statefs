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
