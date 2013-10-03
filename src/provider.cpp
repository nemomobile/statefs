#include <statefs/provider.hpp>
#include <statefs/util.h>
#include <cor/util.hpp>

namespace statefs {

ANode::~ANode() {}

ABranch::~ABranch() {}

PropertyWrapper::PropertyWrapper(char const *name)
    : base_type(name, node_template)
{
    default_value.tag = statefs_variant_cstr;
    default_value.s = "";
}

PropertyWrapper::~PropertyWrapper() {}

APropertyAccessor::~APropertyAccessor() {}

AProperty::AProperty(char const *name)
    : PropertyWrapper(name)
{}

AProperty::~AProperty() {}

AProvider::~AProvider() {}

Namespace::Namespace(char const *name)
    : base_type(name, node_template)
{}

Namespace::~Namespace() {}

template <>
statefs_provider* node_to<statefs_provider>(::statefs_node *n)
{
    auto ns = cor::member_container(n, &statefs_namespace::node);
    return cor::member_container(ns, &statefs_provider::root);
}

BranchStorage::BranchStorage()
{}

BranchStorage::child_ptr BranchStorage::insert(child_ptr child)
{
    if (child)
        props_.insert(
            std::move(std::make_pair(
                          child->get_name(), child)));
    return child;
}

BranchStorage::child_ptr BranchStorage::insert(ANode *child)
{
    child_ptr p(child);
    return insert(p);
}


statefs_node * BranchStorage::find(char const *name) const
{
    auto iter = props_.find(name);
    if (iter == props_.end())
        return nullptr;
    auto const &p = iter->second;
    return p.get()->get_node();
}

statefs_node * BranchStorage::get(statefs_handle_t h) const
{
    auto piter = cor::tagged_handle_pointer<iter_type>(h);
    if (!piter)
        return nullptr;

    if (*piter == props_.end())
        return nullptr;

    auto const &p = (*piter)->second;
    if (!p)
        throw cor::Error("Child is not initialized");
    return p.get()->get_node();
}

statefs_handle_t BranchStorage::first() const
{
    return cor::new_tagged_handle<iter_type>(props_.begin());
}

void BranchStorage::next(statefs_handle_t *h) const
{
    auto p = cor::tagged_handle_pointer<iter_type>(*h);
    if (p)
        ++*p;
}

bool BranchStorage::release(statefs_handle_t h) const
{
    cor::delete_tagged_handle<iter_type>(h);
    return true;
}

template <>
BranchWrapper<statefs_provider> *branch_to<statefs_provider>(statefs_branch *src)
{
    auto ns = cor::member_container(src, &statefs_namespace::branch);
    auto prov = cor::member_container(ns, &statefs_provider::root);
    return static_cast<BranchWrapper<statefs_provider> *>(prov);
}

template <>
BranchWrapper<statefs_provider> const*
branch_to<statefs_provider>(statefs_branch const* src)
{
    auto ns = cor::member_container(src, &statefs_namespace::branch);
    auto prov = cor::member_container(ns, &statefs_provider::root);
    return static_cast<BranchWrapper<statefs_provider> const*>(prov);
}

template <>
const statefs_branch Branch<statefs_provider>::branch_template = {
    &Branch<statefs_provider>::child_find,
    &Branch<statefs_provider>::child_first,
    &Branch<statefs_provider>::child_next,
    &Branch<statefs_provider>::child_get,
    &Branch<statefs_provider>::child_release
};

template <>
const statefs_branch Branch<statefs_namespace>::branch_template = {
    &Branch<statefs_namespace>::child_find,
    &Branch<statefs_namespace>::child_first,
    &Branch<statefs_namespace>::child_next,
    &Branch<statefs_namespace>::child_get,
    &Branch<statefs_namespace>::child_release
};


const statefs_node PropertyWrapper::node_template = {
    statefs_node_prop,
    nullptr,
    nullptr,
    nullptr
};

const statefs_node Namespace::node_template = {
    statefs_node_ns,
    nullptr,
    nullptr,
    nullptr
};

const statefs_node AProvider::node_template = {
    statefs_node_root,
    nullptr,
    nullptr,
    nullptr
};

const statefs_io AProvider::io_template = {
    &AProvider::getattr,
    &AProvider::size,
    &AProvider::open,
    &AProvider::read,
    &AProvider::write,
    &AProvider::close,
    &AProvider::connect,
    &AProvider::disconnect
};

int AProvider::getattr(::statefs_property const *p)
{
    auto impl = AProperty::self_cast(p);
    return impl->getattr();
}

statefs_ssize_t AProvider::size(::statefs_property const *p)
{
    auto impl = AProperty::self_cast(p);
    return impl->size();
}

statefs_handle_t AProvider::open(::statefs_property *p, int flags)
{
    auto impl = AProperty::self_cast(p);
    return reinterpret_cast<statefs_handle_t>(impl->open(flags));
}

int AProvider::read(statefs_handle_t h, char *dst, statefs_size_t len, statefs_off_t off)
{
    auto impl = reinterpret_cast<APropertyAccessor*>(h);
    return impl->read(dst, len, off);
}

int AProvider::write(statefs_handle_t h, char const* src, statefs_size_t len, statefs_off_t off)
{
    auto impl = reinterpret_cast<APropertyAccessor*>(h);
    return impl->write(src, len, off);
}

void AProvider::close(statefs_handle_t h)
{
    auto impl = reinterpret_cast<APropertyAccessor*>(h);
    delete impl;
}

bool AProvider::connect(::statefs_property *p, ::statefs_slot *slot)
{
    auto impl = AProperty::self_cast(p);
    return impl->connect(slot);
}

void AProvider::disconnect(::statefs_property *p)
{
    auto impl = AProperty::self_cast(p);
    return impl->disconnect();
}


PropertyWrapper * PropertyWrapper::self_cast(::statefs_property *p)
{
    return static_cast<PropertyWrapper*>(p);
}

PropertyWrapper const* PropertyWrapper::self_cast(::statefs_property const* p)
{
    return static_cast<PropertyWrapper const*>(p);
}

AProperty * AProperty::self_cast(::statefs_property *p)
{
    return static_cast<AProperty*>(PropertyWrapper::self_cast(p));
}

AProperty const* AProperty::self_cast(::statefs_property const* p)
{
    return static_cast<AProperty const*>(PropertyWrapper::self_cast(p));
}

void AProvider::init_data()
{
    version = STATEFS_CURRENT_VERSION;
    memcpy(&(this->io), &io_template, sizeof(::statefs_io));
}

void AProvider::event(statefs_event event)
{
    if (server_ && server_->event) {
        server_->event(server_, this, event);
    }
}

AProvider::AProvider(char const *name, statefs_server *server)
    : base_type(name, node_template)
    , server_(server)
{
    init_data();
}


template class NodeWrapper<statefs_property>;
template class NodeWrapper<statefs_namespace>;
template class NodeWrapper<statefs_provider>;

template class BranchWrapper<statefs_provider>;
template class BranchWrapper<statefs_namespace>;

}
