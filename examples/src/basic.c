#include <statefs/provider.h>
#include <statefs/util.h>
#include <cor/util.h>

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/param.h>

static pthread_mutex_t basic_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool is_running = true;
static pthread_t tid;

static int counter = 0;
static int counter_dt = 100;

static bool basic_is_initialized = false;

static void * control_thread(void *);

static int basic_init()
{
    pthread_attr_t attr;
    int rc = 0;

    if (basic_is_initialized)
        return rc;

    pthread_mutex_lock(&basic_mutex);

    if (basic_is_initialized)
        goto out;

    rc = pthread_attr_init(&attr);
    if (rc < 0)
        goto out;
    rc = pthread_create(&tid, &attr, control_thread, NULL);
    if (rc < 0)
        goto out;

    basic_is_initialized = true;

out:
    pthread_mutex_unlock(&basic_mutex);
    return rc;
}

static int read_rand(char *dst, statefs_size_t max_size)
{
    int i = rand();
    int s = snprintf(dst, max_size, "%i", i);
    return s;
}

static int read_counter(char *dst, statefs_size_t max_size)
{
    pthread_mutex_lock(&basic_mutex);
    int s = snprintf(dst, max_size, "%i", counter);
    ++counter;
    pthread_mutex_unlock(&basic_mutex);
    return s;
}

static int write_counter_dt(char const *src, statefs_size_t size)
{
    if (size > 254)
        return -1;

    char buf[255];
    memcpy(buf, src, size);
    buf[size] = 0;

    pthread_mutex_lock(&basic_mutex);
    int c = atoi(buf);
    if (c) {
        counter_dt = c;
    } else {
        printf("wrong counter dT value is supplied %s\n", buf);
    }
    pthread_mutex_unlock(&basic_mutex);
    return size;
}

#define int_size (64)

static const struct statefs_meta counter_info[] = {
    STATEFS_META("default", INT, 0),
    STATEFS_META_END
};

static const struct statefs_meta counter_dt_info[] = {
    STATEFS_META("default", INT, 100),
    STATEFS_META_END
};

struct statefs_basic_prop
{
    struct statefs_property prop;
    int (*read)(char *, statefs_size_t);
    int (*write)(char const*, statefs_size_t);
    statefs_size_t max_size;
    int attr;
    struct statefs_slot *slot;
};

typedef enum {
    prop_id_rand,
    prop_id_counter,
    prop_id_dt
} prop_ids;

static struct statefs_basic_prop props[] = {
    {
        .prop = {
            .node = {
                .type = statefs_node_prop,
                .name = "rand",
                .info = NULL
            }
        },
        .read = read_rand,
        .max_size = int_size,
        .attr = STATEFS_ATTR_READ
    },
    {
        .prop = {
            .node = {
                .type = statefs_node_prop,
                .name = "counter",
                .info = counter_info
            },
        },
        .read = read_counter,
        .max_size = int_size,
        .attr = STATEFS_ATTR_READ | STATEFS_ATTR_DISCRETE
    },
    {
        .prop = {
            .node = {
                .type = statefs_node_prop,
                .name = "dt",
                .info = counter_dt_info
            },
        },
        .write = write_counter_dt,
        .max_size = int_size,
        .attr = STATEFS_ATTR_WRITE
    }
};

static void * control_thread(void *arg)
{
    struct statefs_basic_prop *self = &props[prop_id_counter];
    while (is_running) {
        int dt;
        pthread_mutex_lock(&basic_mutex);
        dt = counter_dt;
        ++counter;
        struct statefs_slot *slot = self->slot;
        if (slot)
            slot->on_changed(slot, &self->prop);
        pthread_mutex_unlock(&basic_mutex);
        usleep(dt);
    }
    return NULL;
}


struct statefs_basic_prop *impl_get(struct statefs_property *from)
{
    return container_of(from, struct statefs_basic_prop, prop);
}

struct statefs_basic_prop const *cimpl_get(struct statefs_property const *from)
{
    return container_of(from, struct statefs_basic_prop, prop);
}

static inline struct statefs_node *node_get(struct statefs_basic_prop *from)
{
    return &from->prop.node;
}

static struct statefs_node * prop_find
(struct statefs_branch const* self, char const *name)
{
    int i;
    struct statefs_node * node = NULL;

    for (i = 0; i < ARRAY_SIZE(props); ++i) {
        node = node_get(&props[i]);
        if (!strcmp(node->name, name))
            break;
    }
    if (i == ARRAY_SIZE(props)) {
        fprintf(stderr, "No such property %s\n", name);
        return NULL;
    }
    return node;
}

static void prop_next(struct statefs_branch const* self, intptr_t *idx_ptr)
{
    (++*idx_ptr);
}

static struct statefs_node * prop_get
(struct statefs_branch const* self, intptr_t idx)
{
    return (idx < ARRAY_SIZE(props) && idx >= 0) ? node_get(&props[idx]) : NULL;
}

static intptr_t prop_first(struct statefs_branch const* self)
{
    return 0;
}

static struct statefs_namespace test_ns = {
    .node = {
        .type = statefs_node_ns,
        .name = "test",
    },
    .branch = {
        .find = &prop_find,
        .first = &prop_first,
        .next = &prop_next,
        .get = &prop_get,
    }
};

struct statefs_node * ns_find
(struct statefs_branch const* self, char const *name)
{
    if (strcmp("test", name))
        return NULL;
    return &test_ns.node;
}

static struct statefs_node * ns_get
(struct statefs_branch const* self, intptr_t p)
{
    return (p ? &((struct statefs_namespace*)p)->node : NULL);
}

static intptr_t ns_first(struct statefs_branch const* self)
{
    return (intptr_t)&test_ns;
}

static bool basic_connect
(struct statefs_property *p, struct statefs_slot *slot)
{
    pthread_mutex_lock(&basic_mutex);
    impl_get(p)->slot = slot;
    pthread_mutex_unlock(&basic_mutex);
    return true;
}

static void basic_disconnect(struct statefs_property *p)
{
    pthread_mutex_lock(&basic_mutex);
    impl_get(p)->slot = NULL;
    pthread_mutex_unlock(&basic_mutex);
}

/**
 * all example properties are read-only
 *
 * is_low property is discrete and this is hardcoded here and also in
 * basic_connect() and basic_disconnect() for simplicity
 *
 */
static int basic_getattr(struct statefs_property const* p)
{
    struct statefs_basic_prop const *self = cimpl_get(p);
    return self->attr;
}


/**
 * property I/O handle
 *
 * @param len non-zero if data was already read
 * @param buf buffer to store formatted value
 *
 */
struct basic_handle
{
    struct statefs_basic_prop *p;
    statefs_size_t len;
    char buf[255];
};

/**
 * just return maximum size of the property
 *
 */
static statefs_ssize_t basic_size(struct statefs_property const* p)
{
    return cimpl_get(p)->max_size;
}

/**
 * Allocates basic_handle structure to be used as a handle.
 *
 * On first run also execute initialization of provider including
 * starting of voltage values generation thread
 *
 */
static statefs_handle_t basic_open(struct statefs_property *p, int mode)
{
    struct statefs_basic_prop *self = impl_get(p);
    if (((mode & O_WRONLY) && !(self->attr & STATEFS_ATTR_WRITE))
        || ((mode & O_RDONLY) && !(self->attr & STATEFS_ATTR_READ))) {
        errno = EINVAL;
        return 0;
    }

    if (basic_init() < 0)
        return 0;

    struct basic_handle *h = calloc(1, sizeof(h[0]));
    h->p = self;
    return (statefs_handle_t)h;
}

/** if reading starting from non-zero offset it is considered read
 * operation is continued, so previous cached result is used if
 * exists
 */
static int basic_read(statefs_handle_t h, char *dst, statefs_size_t len, statefs_off_t off)
{
    struct basic_handle *ph = (struct basic_handle *)h;
    struct statefs_basic_prop *self = ph->p;
    if (!self->read)
        return -EPERM;

    if (!off || (off && ph->len <= 0))
        ph->len = self->read(ph->buf, sizeof(ph->buf));

    if (ph->len < 0)
        return ph->len;

    return memcpy_offset(dst, len, off, ph->buf, ph->len);
}

static int basic_write(statefs_handle_t h, char const *src, statefs_size_t len, statefs_off_t off)
{
    struct basic_handle *ph = (struct basic_handle *)h;
    struct statefs_basic_prop *self = ph->p;
    if (!self->write)
        return -EPERM;

    if (off) {
        printf("I do not expect so much data that it needs off\n");
        return -EINVAL;
    }
    return self->write(src, len);
}

static void basic_close(statefs_handle_t h)
{
    free((struct basic_handle*)h);
}

static void basic_provider_release(struct statefs_node *n)
{
    //struct statefs_provider *self = statefs_node2provider(n);
    pthread_mutex_lock(&basic_mutex);
    is_running = false;
    pthread_mutex_unlock(&basic_mutex);
    if (basic_is_initialized)
        pthread_join(tid, NULL);
}

struct statefs_provider provider = {
    .version = STATEFS_CURRENT_VERSION,
    .root = {
        .node = {
            .type = statefs_node_root,
            .name = "basic",
            .release = &basic_provider_release
        },
        .branch = {
            .find = ns_find,
            .first = &ns_first,
            .get = &ns_get
        }
    },
    /** there is no writable properties so .write member is NULL */
    .io = {
        .getattr = basic_getattr,
        .open = basic_open,
        .read = basic_read,
        .write = basic_write,
        .size = basic_size,
        .close = basic_close,
        .connect = basic_connect,
        .disconnect = basic_disconnect
    }
};


EXTERN_C struct statefs_provider * statefs_provider_get(struct statefs_server *p)
{
    return &provider;
}
