#ifndef _STATEFS_PROVIDER_HPP_
#define _STATEFS_PROVIDER_HPP_

#include <statefs/provider.h>

#include <cor/util.hpp>

#include <string>
#include <map>
#include <memory>

#include <string.h>
// TODO TMP
#include <iostream>

namespace statefs {

class ANode
{
public:
    virtual ::statefs_node *get_node() =0;
    virtual ::statefs_node const* get_node() const =0;
    virtual ::statefs_node_type get_type() const =0;
    virtual std::string get_name() const =0;
    virtual void release() =0;
    virtual ~ANode();
};

class ABranch
{
public:
    virtual ::statefs_branch *get_branch() =0;
    virtual ::statefs_branch const* get_branch() const =0;
    virtual ~ABranch();
};


static inline statefs_node* node_from(statefs_provider *v)
{
    return &v->root.node;
}

static inline statefs_node const* node_from(statefs_provider const* v)
{
    return &v->root.node;
}

template <typename T>
statefs_node* node_from(T *v)
{
    return &v->node;
}

template <typename T>
statefs_node const* node_from(T const* v)
{
    return &v->node;
}

template <typename T>
T* node_to(::statefs_node *n)
{
    return cor::member_container(n, &T::node);
}

template <>
statefs_provider* node_to<statefs_provider>(::statefs_node *);

template <typename NodeT>
class NodeWrapper : public NodeT, public ANode
{
public:
    ::statefs_node *node_cast()
    { return node_from(static_cast<NodeT*>(this)); }
    ::statefs_node const* node_cast() const
    { return node_from(static_cast<NodeT const*>(this)); }

    virtual ::statefs_node *get_node() { return node_cast(); }
    virtual ::statefs_node const* get_node() const { return node_cast(); }
    virtual ::statefs_node_type get_type() const { return node_cast()->type; }
    virtual std::string get_name() const { return node_cast()->name; }


    NodeWrapper(char const *name, statefs_node const& node_template)
    {
        memcpy(node_cast(), &node_template, sizeof(statefs_node));
        node_cast()->name = strdup(name);
        node_cast()->release = &release_bridge;
    }
    virtual ~NodeWrapper()
    {
        free(const_cast<char*>(node_cast()->name));
    }

private:
    static void release_bridge(::statefs_node *n)
    {
        NodeWrapper *self = static_cast<NodeWrapper*>(node_to<NodeT>(n));
        static_cast<ANode*>(self)->release();
    }
};

template <typename T>
statefs_branch* branch_from(T *v)
{
    return &v->branch;
}

template <typename T>
statefs_branch const* branch_from(T const* v)
{
    return &v->branch;
}

static inline statefs_branch* branch_from(statefs_provider *v)
{
    return &v->root.branch;
}

static inline statefs_branch const* branch_from(statefs_provider const* v)
{
    return &v->root.branch;
}

template <typename BranchT>
class BranchWrapper;

template <typename T>
BranchWrapper<T> *branch_to(statefs_branch *src)
{
    T *t = cor::member_container(src, &T::branch);
    return static_cast<BranchWrapper<T> *>(t);
}

template <typename T>
BranchWrapper<T> const* branch_to(statefs_branch const* src)
{
    T const* t = cor::member_container(src, &T::branch);
    return static_cast<BranchWrapper<T> const*>(t);
}

template <>
BranchWrapper<statefs_provider> *branch_to<statefs_provider>(statefs_branch *);

template <>
BranchWrapper<statefs_provider> const* branch_to<statefs_provider>(statefs_branch const*);

template <typename BranchT>
class BranchWrapper : public NodeWrapper<BranchT>
{
public:

    ::statefs_branch *branch_cast()
    { return branch_from(static_cast<BranchT*>(this)); }
    ::statefs_branch const* branch_cast() const
    { return branch_from(static_cast<BranchT const*>(this)); }

    BranchWrapper
    (char const *name
     , statefs_node const& node_template
     , statefs_branch const &branch_template)
        : NodeWrapper<BranchT>(name, node_template)
    {
        memcpy(branch_cast(), &branch_template, sizeof(statefs_branch));
    }

    static BranchWrapper<BranchT> *self_cast(statefs_branch *src)
    {
        return branch_to<BranchT>(src);
    }

    static BranchWrapper<BranchT> const* self_cast(statefs_branch const* src)
    {
        return branch_to<BranchT>(src);
    }

};


class BranchStorage
{
public:
    typedef std::shared_ptr<ANode> child_ptr;

protected:
    typedef std::map<std::string, child_ptr> storage_type;
    typedef storage_type::const_iterator iter_type;

public:

    BranchStorage();

    child_ptr insert(child_ptr child);
    child_ptr insert(ANode *child);

protected:

    statefs_node* find(char const*) const;
    statefs_node* get(statefs_handle_t) const;
    statefs_handle_t first() const;
    void next(statefs_handle_t*) const;
    bool release(statefs_handle_t) const;

private:

    storage_type props_;
};

template <class BranchT>
class Branch : public BranchWrapper<BranchT>, public BranchStorage
{
private:

    static const statefs_branch branch_template;

    typedef BranchWrapper<BranchT> base_type;

    static statefs_node * child_find(statefs_branch const* branch
                                     , char const *name)
    {
        auto self = self_cast(branch);
        return self->BranchStorage::find(name);
    }

    static statefs_node * child_get(statefs_branch const* branch, statefs_handle_t h)
    {
        auto self = self_cast(branch);
        return self->BranchStorage::get(h);
    }

    static statefs_handle_t child_first(statefs_branch const* branch)
    {
        auto self = self_cast(branch);
        return self->BranchStorage::first();
    }

    static void child_next(statefs_branch const* branch, statefs_handle_t *h)
    {
        auto self = self_cast(branch);
        return self->BranchStorage::next(h);
    }

    static bool child_release(statefs_branch const* branch, statefs_handle_t h)
    {
        auto self = self_cast(branch);
        return self->BranchStorage::release(h);
    }

protected:
    // implement
    // static WrapperT const *self_cast(statefs_branch const*);
    // in WrapperT
    static Branch const *self_cast(statefs_branch const* branch)
    {
        return static_cast<Branch const*>
            (base_type::self_cast(branch));
    }

public:
    Branch(char const *name, statefs_node const& node_template)
        : base_type(name, node_template, branch_template)
    {}

    virtual ~Branch() {}
};


class PropertyWrapper : public NodeWrapper<statefs_property>
{
    typedef NodeWrapper<statefs_property> base_type;

    static const statefs_node node_template;
public:
    PropertyWrapper(char const *name);
    virtual ~PropertyWrapper();

    static PropertyWrapper *self_cast(::statefs_property *);
    static PropertyWrapper const* self_cast(::statefs_property const*);
};

class APropertyAccessor
{
public:
    virtual ~APropertyAccessor();

    virtual int read(char *dst, statefs_size_t len, statefs_off_t off) =0;
    virtual int write(char const*, statefs_size_t, statefs_off_t) =0;
};

class AProperty : public PropertyWrapper
{
public:
    AProperty(char const *name);
    virtual ~AProperty();

    virtual int getattr() const =0;
    virtual statefs_ssize_t size() const =0;
    virtual APropertyAccessor* open(int flags) =0;

    virtual bool connect(::statefs_slot *) =0;
    virtual void disconnect() =0;

    static AProperty *self_cast(::statefs_property *p);
    static AProperty const* self_cast(::statefs_property const* p);
};

template <typename T, typename HandleT>
class BasicPropertyAccessor : public APropertyAccessor
{
public:
    BasicPropertyAccessor(std::shared_ptr<T> p, HandleT *h)
        : prop_(p), handle_(h) {}

    virtual int read(char *dst, statefs_size_t len, statefs_off_t off)
    {
        return prop_->read(handle_.get(), dst, len, off);
    }

    virtual int write(char const *src, statefs_size_t len, statefs_off_t off)
    {
        return prop_->write(handle_.get(), src, len, off);
    }

protected:
    std::shared_ptr<T> prop_;
    std::unique_ptr<HandleT> handle_;
};

template <typename T, typename HandleT>
BasicPropertyAccessor<T, HandleT>* mk_prop_accessor
(std::shared_ptr<T> p, HandleT *h)
{
    return new BasicPropertyAccessor<T, HandleT>(p, h);
}

template <typename T>
class APropertyOwner : public AProperty
{
public:
    template <typename ... Args>
    APropertyOwner(char const *name, Args&& ...args)
        : AProperty(name)
        , impl_(new T(this, args...))
    {}

    virtual ~APropertyOwner() {}

    virtual int getattr() const { return impl_->getattr(); }
    virtual statefs_ssize_t size() const { return impl_->size(); }
    virtual APropertyAccessor* open(int flags) =0;

    virtual bool connect(::statefs_slot *s) { return impl_->connect(s); }
    virtual void disconnect() { return impl_->disconnect(); }

    virtual void release() { return impl_->release(); }

protected:
    std::shared_ptr<T> impl_;
};

template <typename T, typename HandleT>
class BasicPropertyOwner : public APropertyOwner<T>
{
public:
    template <typename ... Args>
    BasicPropertyOwner(char const *name, Args&& ...args)
        : APropertyOwner<T>(name, args...)
    {}

    template <typename ... Args>
    BasicPropertyOwner(std::string const &name, Args&& ...args)
        : APropertyOwner<T>(name.c_str(), args...)
    {}

    virtual statefs::APropertyAccessor* open(int flags)
    {
        return statefs::mk_prop_accessor(this->impl_, new HandleT());
    }

    std::shared_ptr<T> get_impl() { return this->impl_; }
};

class Namespace : public Branch<statefs_namespace>
{
    typedef Branch<statefs_namespace> base_type;
    static const statefs_node node_template;

public:
    Namespace(char const *name);
    virtual ~Namespace();
};

class AProvider : public Branch<statefs_provider>
{
public:
    AProvider(char const *name, statefs_server *server);
    virtual ~AProvider();

protected:
    static AProvider* self_cast();

    void event(statefs_event);

private:
    typedef Branch<statefs_provider> base_type;

    static const statefs_io io_template;
    static const statefs_node node_template;

    static int getattr(::statefs_property const *);
    static statefs_ssize_t size(::statefs_property const *);
    static statefs_handle_t open(::statefs_property *self, int flags);
    static int read(statefs_handle_t h, char *dst, statefs_size_t len, statefs_off_t off);
    static int write(statefs_handle_t, char const*, statefs_size_t, statefs_off_t);
    static void close(statefs_handle_t);
    static bool connect(::statefs_property *, ::statefs_slot *);
    static void disconnect(::statefs_property *);

    void init_data();

    statefs_server *server_;
};

// class Namespace : public ABranch
// {
//     static statefs_node node_template;
// protected:
//     typedef statefs::BranchWrapper<statefs_namespace> data_type;
// public:
//     static Namespace *self_cast(statefs_branch*);
//     static Namespace const* self_cast(statefs_branch const*);

//     Namespace(char const* name)
//         : prop_(name, node_template)
//     {
//     }

//     virtual statefs_node *get_node()
//     {
//         return &prop_.node;
//     }
//     virtual statefs_node const* get_node() const
//     {
//         return &prop_.node;
//     }

//     virtual statefs_node_type get_type() const
//     {
//         return prop_.node.type;
//     }

// protected:
//     data_type data_;
//     static data_type Namespace::* data_offset_;
// };

extern template class NodeWrapper<statefs_property>;
extern template class NodeWrapper<statefs_namespace>;
extern template class NodeWrapper<statefs_provider>;

extern template class BranchWrapper<statefs_provider>;
extern template class BranchWrapper<statefs_namespace>;

} // namespace


#endif // _STATEFS_PROVIDER_HPP_
