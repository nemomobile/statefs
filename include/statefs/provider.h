#ifndef _STATEFS_PROVIDER_H_
#define _STATEFS_PROVIDER_H_
/**
 * @file provider.h
 * @brief Provider API
 * @author (C) 2012 Jolla Ltd. Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 * @copyright LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

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

/**
 * @defgroup provider_api Provider API
 *
 * @brief Provider API description. Usage is described @ref api "here"
 *
 *  @{
 */

typedef unsigned long statefs_size_t;
typedef long statefs_ssize_t;
typedef unsigned long statefs_off_t;
typedef intptr_t statefs_handle_t;

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

/**
 * Node metadata, it can be documentation or anything else :)
 */
struct statefs_meta
{
    char const *name; /** < attribute name, no spaces allowed */
    struct statefs_variant value; /** < attribute data */
};

/**
 * Tree node type 
 */
typedef enum
{
    statefs_node_prop = 1,
    statefs_node_ns = 2,
    statefs_node_root = statefs_node_ns | 4
} statefs_node_type;

/** 
 * Node of statefs tree
 */
struct statefs_node
{
    /** 
     *there are provider root node, namespace nodes and property nodes
     */
    statefs_node_type type;
    /** name - c string */
    char const *name;
    /** if not NULL called to free resources used by node */  
    void (*release)(struct statefs_node*);
    /** array of node metadata, last element has NULL name member */
    struct statefs_meta const* info;
};

/**
 * if node has children it is a branch node
 */
struct statefs_branch
{
    /** find child node by name */
    struct statefs_node * (*find)(struct statefs_branch const*, char const *);
    /** get first child node iterator */
    statefs_handle_t (*first)(struct statefs_branch const*);
    /** move iterator to next node */
    void (*next)(struct statefs_branch const*, statefs_handle_t *);
    /** get node pointer from iterator */
    struct statefs_node * (*get)(struct statefs_branch const*, statefs_handle_t);
    /** release/free iterator and resources used by it */
    bool (*release)(struct statefs_branch const*, statefs_handle_t);
};

struct statefs_property;

/** Callback with context to monitor discrete property changes */
struct statefs_slot
{
    /**
     * should be invoked by provider if value of the property
     * connected to this slot by means of statefs_io.connect is
     * changed
     */
    void (*on_changed)(struct statefs_slot *,
                       struct statefs_property *);
};

/**
 * Properties can be readable/writable and
 * discrete/continuous. Discrete property is the property changing in
 * some discrete intervals so each change can be tracked through
 * event. Continuous property is changing continuously (or maybe,
 * also, very frequently to use events to track it) so it should be
 * requested only explicitely. Access to property is serialized.
 */
struct statefs_property
{
    struct statefs_node node;
    /**
     * used to initialize property file with initial value. It is used
     * when provider is not available or can't provide data
     */
    struct statefs_variant default_value;
};

/** readable */
#define STATEFS_ATTR_READ (1)
/** writeable */
#define STATEFS_ATTR_WRITE (1 << 1)
/** discrete, statefs_slot can be connected using statefs_io.connect */
#define STATEFS_ATTR_DISCRETE (1 << 2)

/**
 * API to access properties. The API itself can be accessed
 * concurently but access to separate properties and opened property
 * handles is serialized.
 */
struct statefs_io
{
    /**
     * get property attributes
     * @retval mask @ref STATEFS_ATTR_DISCRETE | @ref STATEFS_ATTR_WRITE
     * | @ref STATEFS_ATTR_READ
     */
    int (*getattr)(struct statefs_property const *);

    /** 
     * get property size. If property length is variable it is
     * better to return maximum property size
     */
    statefs_ssize_t (*size)(struct statefs_property const *);

    /**
     * open property for I/O
     * @param flags [O_RDONLY, O_RDWR, O_WRONLY]
     * @retval opaque handle to be used for I/O operations
     */
    statefs_handle_t (*open)(struct statefs_property *self, int flags);

    /**
     * read property value (len bytes starting from off)
     */
    int (*read)(statefs_handle_t h, char *dst, statefs_size_t len, statefs_off_t off);

    /**
     * write property value (len bytes starting from off)
     */
    int (*write)(statefs_handle_t, char const*, statefs_size_t, statefs_off_t);

    /** close I/O handle */
    void (*close)(statefs_handle_t);

    /** 
     * connect discrete property to server slot, provider should invoke
     * statefs_slot.on_changed() when property value is changed  
     *
     * @note only single connection is opened for single property, so if
     * called several times provider can just replace previous slot
     * value
     */
    bool (*connect)(struct statefs_property *, struct statefs_slot *);
    /** disconnect previously connected slot */
    void (*disconnect)(struct statefs_property *);
};

/** 
 * Namespace node
 */
struct statefs_namespace
{
    struct statefs_node node;
    struct statefs_branch branch;
};

/**
 * Root provider node
 */
struct statefs_provider
{
    /**
     * Used to verify API compatibility. Provider should initialize
     * it with STATEFS_CURRENT_VERSION
     */
    unsigned version;

    struct statefs_namespace root;

    /**
     * API to access property properties and data
     */
    struct statefs_io io;
};


typedef enum {
    statefs_event_reload,

    // add new events above
    statefs_events_end
}  statefs_event;

struct statefs_server
{
    void (*event)(struct statefs_server*
                  , struct statefs_provider*
                  , statefs_event);
};

/** 
 * Signature of statefs_provider_get function must be defined by
 * provider
 */
typedef struct statefs_provider * (*statefs_provider_fn)
    (struct statefs_server*);

/** 
 * Function defined in provider library to access the root node
 */
struct statefs_provider * statefs_provider_get(struct statefs_server*);

static inline char const *statefs_provider_accessor()
{
    return "statefs_provider_get";
}

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
#define STATEFS_CURRENT_VERSION STATEFS_MK_VERSION(3, 0)

static inline bool statefs_is_version_compatible
(unsigned own_version, unsigned lib_ver)
{
    unsigned short maj, min;
    unsigned short prov_maj, prov_min;
    STATEFS_GET_VERSION(lib_ver, prov_maj, prov_min);
    STATEFS_GET_VERSION(own_version, maj, min);
    return (prov_maj == maj) && (prov_min <= min);
}

/**
 * used by server to check compatibility with provider version
 */
static inline bool statefs_is_compatible
(unsigned own_version, struct statefs_provider *provider)
{
    return statefs_is_version_compatible(own_version, provider->version);
}

/** @}
 * provider api
 */

EXTERN_C_END

#endif // _STATEFS_PROVIDER_H_
