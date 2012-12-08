#ifndef _STATEFS_PROVIDER_H_
#define _STATEFS_PROVIDER_H_

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }
#define EXTERN_C extern "C"
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#define EXTERN_C
#endif

#ifndef container_of
#define container_of(ptr, type, member)                     \
    ((type *)( (char *)(ptr) - offsetof(type, member) ))
#endif

EXTERN_C_BEGIN

typedef enum
{
    statefs_variant_int,
    statefs_variant_uint,
    statefs_variant_bool,
    statefs_variant_real,
    statefs_variant_cstr,

    statefs_variant_tags_end
} statefs_variant_tag;

struct statefs_variant
{
    statefs_variant_tag tag;
    union
    {
        long i;
        unsigned long u;
        bool b;
        double r;
        char const *s;
    };
};

#define STATEFS_INT(v) { .tag = statefs_variant_int, .i = (v) }
#define STATEFS_UINT(v) { .tag = statefs_variant_uint, .i = (v) }
#define STATEFS_REAL(v) { .tag = statefs_variant_real, .r = (v) }
#define STATEFS_BOOL(v) { .tag = statefs_variant_bool, .b = (v) }
#define STATEFS_CSTR(v) { .tag = statefs_variant_cstr, .s = (v) }

struct statefs_meta
{
    char const *name;
    struct statefs_variant value;
};

#define STATEFS_META(name, type, value) { name, STATEFS_##type(value) }
#define STATEFS_META_END { NULL, {} }

typedef enum
{
    statefs_node_root = 0,
    statefs_node_ns,
    statefs_node_prop
} statefs_node_type;

struct statefs_node
{
    statefs_node_type type;
    char const *name;
    void (*release)(struct statefs_node*);
    struct statefs_meta const* info;
};

static inline void statefs_node_release(struct statefs_node *node)
{
    if (node && node->release)
        node->release(node);
}

struct statefs_branch
{
    struct statefs_node * (*find)(struct statefs_branch const*, char const *);
    intptr_t (*first)(struct statefs_branch const*);
    void (*next)(struct statefs_branch const*, intptr_t *);
    struct statefs_node * (*get)(struct statefs_branch const*, intptr_t);
    bool (*release)(struct statefs_branch const*, intptr_t);
};

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

static inline struct statefs_node * statefs_get
(struct statefs_branch const* self, intptr_t p)
{
    return (self->get ? self->get(self, p) : NULL);
}

struct statefs_data
{
    void (*release)(struct statefs_data *);
    void *p;
    size_t len;
};

struct statefs_property;

struct statefs_slot
{
    void (*on_changed)(struct statefs_slot *,
                       struct statefs_property *);
};

struct statefs_property
{
    struct statefs_node node;
    struct statefs_data * (*get)(struct statefs_property *);
    bool (*connect)(struct statefs_property *, struct statefs_slot *);
    void (*disconnect)(struct statefs_property *, struct statefs_slot *);
};

static inline struct statefs_property * statefs_prop_get
(struct statefs_branch const* self, intptr_t iter)
{
    struct statefs_node *n = statefs_get(self, iter);
    return (n && n->type == statefs_node_prop
            ? container_of(n, struct statefs_property, node)
            : NULL);

}

struct statefs_namespace
{
    struct statefs_node node;
    struct statefs_branch branch;
};

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

struct statefs_provider
{
    unsigned version;
    struct statefs_node node;
    struct statefs_branch branch;
};

static inline struct statefs_namespace * statefs_ns_find
(struct statefs_provider *self, char const *name)
{
    struct statefs_node *res = self->branch.find(&self->branch, name);
    return ( (res && res->type == statefs_node_ns)
             ? container_of(res, struct statefs_namespace, node)
             : NULL );
}


typedef struct statefs_provider * (*statefs_provider_fn)(void);

struct statefs_provider * statefs_provider_get(void);

#define STATEFS_MK_VERSION(major, minor)                                \
    (((unsigned)major << (sizeof(unsigned) * 4)) | ((unsigned)minor))

#define STATEFS_GET_VERSION(version, major, minor)                      \
    do {                                                                \
        major = (version) >> (sizeof(unsigned) * 4);                    \
        minor = (version) & ((unsigned)-1 >> (sizeof(unsigned) * 4));   \
    } while (0)

/* increase minor version for backward compatible providers, major -
 * if provider logic is changed and it can't be used with previous
 * versions of consumer safely
 */
#define STATEFS_CURRENT_VERSION STATEFS_MK_VERSION(1, 0)

static inline bool statefs_is_compatible(struct statefs_provider *provider)
{
    unsigned short maj, min;
    unsigned short prov_maj, prov_min;
    STATEFS_GET_VERSION(provider->version, prov_maj, prov_min);
    STATEFS_GET_VERSION(STATEFS_CURRENT_VERSION, maj, min);
    return (prov_maj == maj) && (prov_min <= min);
}

EXTERN_C_END

#endif // _STATEFS_PROVIDER_H_
