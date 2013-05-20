#ifndef _STATEFS_PROVIDER_HPP_
#define _STATEFS_PROVIDER_HPP_

#include <statefs/provider.h>

#include <string>
#include <map>
#include <memory>

#include <string.h>

namespace statefs {

template <typename NodeT>
class Node : public NodeT
{
public:
    Node(char const *name, statefs_node const& node_template)
    {
        memcpy(&this->node, &node_template, sizeof(this->node));
        this->node.name = strdup(name);
    }
    virtual ~Node()
    {
        free(const_cast<char*>(this->node.name));
    }
};

template <typename BranchT>
class Branch : public Node<BranchT>
{
public:
    Branch
    (char const *name
     , statefs_node const& node_template
     , statefs_branch const &branch_template)
        : Node<BranchT>(name, node_template)
    {
        memcpy(&this->branch, &branch_template, sizeof(this->branch));
    }

};

class Child
{
public:
    virtual statefs_node *get_node() =0;
    virtual statefs_node const* get_node() const =0;
    virtual statefs_node_type get_type() const =0;
    virtual ~Child() {}
};

class Namespace : public Branch<statefs_namespace>
{
protected:
    typedef std::unique_ptr<Child> child_ptr;
    typedef std::map<std::string, child_ptr> storage_type;
    typedef storage_type::const_iterator iter_type;

private:
    static statefs_node node_template;
    static statefs_branch branch_template;

    static statefs_node * child_find(statefs_branch const*, char const *);

    static statefs_node * child_get(statefs_branch const*, intptr_t);

    static intptr_t child_first(statefs_branch const*);
    static void child_next(statefs_branch const*, intptr_t *);
    static bool child_release(statefs_branch const*, intptr_t);

    static Namespace const *self_cast(statefs_branch const* branch);

public:
    Namespace(char const *name)
        : Branch<statefs_namespace>
          (name, node_template, branch_template)
    {}

    virtual ~Namespace() {}

    void insert(std::string const &name, Child *prop)
    {
        props_.insert(std::make_pair(name, child_ptr(prop)));
    }

protected:

    storage_type props_;
};


class PropertyWrapper : public statefs_property
{
public:
    PropertyWrapper()
    {
        default_value.tag = statefs_variant_cstr;
        default_value.s = "";
    }
};

class Property : public Child
{
    static statefs_node node_template;
protected:
    typedef statefs::Node<PropertyWrapper> node_type;
public:
    static Property *self_cast(statefs_property*);
    static Property const* self_cast(statefs_property const*);

    Property(char const* name)
        : prop_(name, node_template)
    {
    }

    virtual statefs_node *get_node()
    {
        return &prop_.node;
    }
    virtual statefs_node const* get_node() const
    {
        return &prop_.node;
    }

    virtual statefs_node_type get_type() const
    {
    return prop_.node.type;
    }

protected:
    node_type prop_;
    static node_type Property::* prop_offset;
};

} // namespace


#endif // _STATEFS_PROVIDER_HPP_
