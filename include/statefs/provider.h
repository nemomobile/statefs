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

typedef enum
{
    statefs_node_root = 0,
    statefs_node_ns,
    statefs_node_prop
} statefs_node_type;

/** 
 * StateFS information tree consists of nodes, node type is determined
 * by statefs_node_type enumeration. Each node has metadata (array of
 * struct statefs_meta items). There is a single root node (struct
 * statefs_provider), namespace nodes (struct statefs_namespace) and
 * property nodes (struct statefs_property).
 *
 * Root node and namespace nodes has ability to has children nodes
 * (branch nodes (struct statefs_branch). Property nodes are tree
 * leafs.
 */
struct statefs_node
{
    statefs_node_type type;
    /** name - c string */
    char const *name;
    /** if not NULL called to free resources used by node */  
    void (*release)(struct statefs_node*);
    /** node metadata */
    struct statefs_meta const* info;
};

/**
 * each node has children is a branch node
 */
struct statefs_branch
{
    /** find child node by name */
    struct statefs_node * (*find)(struct statefs_branch const*, char const *);
    /** get first child node iterator */
    intptr_t (*first)(struct statefs_branch const*);
    /** move iterator to next node */
    void (*next)(struct statefs_branch const*, intptr_t *);
    /** get node pointer from iterator */
    struct statefs_node * (*get)(struct statefs_branch const*, intptr_t);
    /** release/free iterator and resources used by it */
    bool (*release)(struct statefs_branch const*, intptr_t);
};

struct statefs_property;

struct statefs_slot
{
    void (*on_changed)(struct statefs_slot *,
                       struct statefs_property *);
};

/**
 * depending in which methods are implemented properties can be
 * readable/writable and discrete/continuous. Discrete property is the
 * property changing in some discrete intervals so each change can be
 * tracked through event. Continuous property is changing continuously
 * (or maybe, also, very frequently to use events to track it) in time
 * so it should be requested only explicitely. Access to property is
 * serialized.
 */
struct statefs_property
{
    struct statefs_node node;
    struct statefs_variant default_value;
    int (*read)(struct statefs_property *, char *, size_t, off_t);
    int (*write)(struct statefs_property *, char *, size_t, off_t);
    size_t (*size)(void);
    /** only single connection is opened for single property */
    bool (*connect)(struct statefs_property *, struct statefs_slot *);
    void (*disconnect)(struct statefs_property *, struct statefs_slot *);
};

struct statefs_namespace
{
    struct statefs_node node;
    struct statefs_branch branch;
};

struct statefs_provider
{
    unsigned version;
    struct statefs_node node;
    struct statefs_branch branch;
};

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
