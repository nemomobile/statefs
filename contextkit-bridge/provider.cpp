#include "provider.hpp"
#include <statefs/util.h>
#include <cor/util.hpp>

namespace statefs {

statefs_node Namespace::node_template = {
    statefs_node_ns,
    nullptr,
    nullptr,
    nullptr
};


Namespace const *Namespace::self_cast(statefs_branch const* branch)
{
    auto ns = container_of(branch, statefs_namespace, branch);
    return static_cast<Namespace const*>(ns);
}

statefs_node * Namespace::child_find
(statefs_branch const* branch, char const *name)
{
    auto self = self_cast(branch);
    auto iter = self->props_.find(name);
    if (iter == self->props_.end())
        return nullptr;
    auto const &p = iter->second;
    return p.get()->get_node();
}

statefs_node * Namespace::child_get(statefs_branch const *branch, intptr_t h)
{
    auto self = self_cast(branch);
    auto piter = cor::tagged_handle_pointer<iter_type>(h);
    if (!piter)
        return nullptr;

    if (*piter == self->props_.end())
        return nullptr;

    auto const &p = (*piter)->second;
    if (!p)
        throw cor::Error("Child is not initialized");
    return p.get()->get_node();
}

intptr_t Namespace::child_first(statefs_branch const* branch)
{
    auto self = self_cast(branch);
    return cor::new_tagged_handle<iter_type>(self->props_.begin());
}

void Namespace::child_next(statefs_branch const*, intptr_t *h)
{
    auto p = cor::tagged_handle_pointer<iter_type>(*h);
    if (p)
        ++*p;
}

bool Namespace::child_release(statefs_branch const*, intptr_t h)
{
    cor::delete_tagged_handle<iter_type>(h);
    return true;
}

statefs_branch Namespace::branch_template = {
        &Namespace::child_find,
        &Namespace::child_first,
        &Namespace::child_next,
        &Namespace::child_get,
        &Namespace::child_release
};


Property::node_type Property::* Property::prop_offset = &Property::prop_;

Property *Property::self_cast(statefs_property *p)
{
    auto n = static_cast<node_type*>(p);
    return container_of(n, Property, prop_);
}

Property const* Property::self_cast(statefs_property const *p)
{
    auto n = static_cast<node_type const*>(p);
    return container_of(n, Property, prop_);
}

statefs_node Property::node_template = {
    statefs_node_prop,
    nullptr,
    nullptr,
    nullptr
};

}

