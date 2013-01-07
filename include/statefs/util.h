#ifndef _STATEFS_UTIL_H_
#define _STATEFS_UTIL_H_

#include <statefs/provider.h>

#ifndef container_of
#define container_of(ptr, type, member)                     \
    ((type *)( (char *)(ptr) - offsetof(type, member) ))
#endif

EXTERN_C_BEGIN

#define STATEFS_META(name, type, value) { name, STATEFS_##type(value) }
#define STATEFS_META_END { NULL, {} }

static inline void statefs_node_release(struct statefs_node *node)
{
    if (node && node->release)
        node->release(node);
}

static inline intptr_t statefs_first(struct statefs_branch const* self)
{
    return (self->first ? self->first(self) : 0);
}

static inline void statefs_next(struct statefs_branch const* self, intptr_t *p)
{
    if (self->next)
        self->next(self, p);
    else
        *p = 0;
}

static inline void statefs_branch_release
(struct statefs_branch const* self, intptr_t p)
{
    if (self && self->release)
        self->release(self, p);
}

static inline struct statefs_node * statefs_get
(struct statefs_branch const* self, intptr_t p)
{
    return (self->get ? self->get(self, p) : NULL);
}

static inline struct statefs_property * statefs_prop_get
(struct statefs_branch const* self, intptr_t iter)
{
    struct statefs_node *n = statefs_get(self, iter);
    return (n && n->type == statefs_node_prop
            ? container_of(n, struct statefs_property, node)
            : NULL);
}

static inline struct statefs_namespace * statefs_ns_get
(struct statefs_branch const* self, intptr_t iter)
{
    struct statefs_node *n = statefs_get(self, iter);
    return (n && n->type == statefs_node_ns
            ? container_of(n, struct statefs_namespace, node)
            : NULL);

}

static inline struct statefs_property * statefs_prop_find
(struct statefs_namespace const* self, char const *name)
{
    struct statefs_node *res = self->branch.find(&self->branch, name);
    return ( (res && res->type == statefs_node_prop)
             ? container_of(res, struct statefs_property, node)
             : NULL );
}

static inline struct statefs_namespace * statefs_ns_find
(struct statefs_provider *self, char const *name)
{
    struct statefs_node *res = self->branch.find(&self->branch, name);
    return ( (res && res->type == statefs_node_ns)
             ? container_of(res, struct statefs_namespace, node)
             : NULL );
}

EXTERN_C_END

#endif // _STATEFS_UTIL_H_
