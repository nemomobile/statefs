/**
 * @file power.c
 * @brief Example provider - provides fake power supply properties
 * @author (C) 2012, 2013 Jolla Ltd.
 * Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 * @copyright LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <cor/util.h>
#include <statefs/provider.h>
#include <statefs/util.h>

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/param.h>

#include <pthread.h>

static pthread_mutex_t power_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool is_running = true;
static pthread_t tid;

static double voltage_now = 3.6;
static double dvoltage = 0.1;
static const char max_iv_fmt[] = "1.0000";

static bool power_is_initialized = false;

static void * control_thread(void *);

static int power_init()
{
    pthread_attr_t attr;
    int rc = 0;

    if (power_is_initialized)
        return rc;

    pthread_mutex_lock(&power_mutex);

    if (power_is_initialized)
        goto out;
    rc = pthread_attr_init(&attr);
    if (rc < 0)
        goto out;
    rc = pthread_create(&tid, &attr, control_thread, NULL);
    if (rc < 0)
        goto out;

    power_is_initialized = true;

out:
    pthread_mutex_unlock(&power_mutex);
    return rc;
}

static int read_voltage(char *dst, size_t len)
{
    pthread_mutex_lock(&power_mutex);
    int fmtlen = snprintf(dst, len - 1, "%1.4f", voltage_now);
    pthread_mutex_unlock(&power_mutex);

    len = MIN(len, fmtlen);
    return len;
}

#define iv_max_size (sizeof(max_iv_fmt) - 1)

static int read_current(char *dst, size_t len)
{
    double r = (double)rand() * 1.0 / RAND_MAX;
    int fmtlen = snprintf(dst, len - 1, "%1.4f", r);
    len = MIN(len, fmtlen);
    return len;
}

static const char is_low_fmt[] = "0";
#define is_low_max_len sizeof(is_low_fmt)
static bool is_low = false;
static struct statefs_slot * is_low_slot = NULL;

static bool update_is_low()
{
    bool prev_is_low = is_low;
    is_low = (voltage_now < 3.7);
    return (prev_is_low != is_low);
}

static int read_is_low(char *dst, size_t len)
{
    pthread_mutex_lock(&power_mutex);
    snprintf(dst, len - 1, "%s", is_low ? "1" : "0");
    int fmtlen = pthread_mutex_unlock(&power_mutex);
    len = MIN(len, fmtlen);
    return len;
}

struct statefs_power_prop
{
    struct statefs_property prop;
    int (*read)(char *, size_t);
    size_t max_size;
};

struct statefs_power_prop * power_prop(struct statefs_property *p)
{
    return container_of(p, struct statefs_power_prop, prop);
};

static struct statefs_power_prop props[] = {
    {
        .prop = {
            .node = {
                .type = statefs_node_prop,
                .name = "voltage"
            },
            .default_value = STATEFS_REAL(3.8)
        },
        .read = read_voltage,
        .max_size = iv_max_size
    },
    {
        .prop = {
            .node = {
                .type = statefs_node_prop,
                .name = "current"
            },
            .default_value = STATEFS_REAL(0),
        },
        .read = read_current,
        .max_size = iv_max_size
    },
    {
        .prop = {
            .node = {
                .type = statefs_node_prop,
                .name = "is_low"
            },
            .default_value = STATEFS_BOOL(false)
        },
        .read = read_is_low,
        .max_size = is_low_max_len
    }
};
    
static void * control_thread(void *arg)
{
    bool is_low_changed;
    while (is_running) {

        pthread_mutex_lock(&power_mutex);
        if (voltage_now >= 4.2)
            dvoltage = -0.1;
        if (voltage_now <= 3.1)
            dvoltage = 0.1;
        voltage_now += dvoltage;
        is_low_changed = update_is_low();
        pthread_mutex_unlock(&power_mutex);

        if (is_low_changed && is_low_slot)
            is_low_slot->on_changed(is_low_slot, &props[2].prop);
        sleep(1);
    }
    return NULL;
}


struct power_provider
{
    struct statefs_provider base;
};

static struct statefs_node * prop_find
(struct statefs_branch const* self, char const *name)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(props); ++i)
        if (!strcmp(props[i].prop.node.name, name))
            break;
    if (i == ARRAY_SIZE(props)) {
        fprintf(stderr, "No such property %s\n", name);
        return NULL;
    }
    return &props[i].prop.node;
}

static void prop_next(struct statefs_branch const* self, intptr_t *idx_ptr)
{
    (++*idx_ptr);
}

static struct statefs_node * prop_get
(struct statefs_branch const* self, intptr_t idx)
{
    return (idx < ARRAY_SIZE(props) && idx >= 0) ? &props[idx].prop.node : NULL;
}

static intptr_t prop_first(struct statefs_branch const* self)
{
    return 0;
}

static struct statefs_namespace battery_ns = {
    .node = {
        .type = statefs_node_ns,
        .name = "battery",
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
    if (strcmp("battery", name))
        return NULL;
    return &battery_ns.node;
}

static struct statefs_node * ns_get
(struct statefs_branch const* self, intptr_t p)
{
    return (p ? &((struct statefs_namespace*)p)->node : NULL);
}

static intptr_t ns_first(struct statefs_branch const* self)
{
    return (intptr_t)&battery_ns;
}

static void power_release(struct statefs_node *node)
{
    bool should_join = false;
    pthread_mutex_lock(&power_mutex);
    should_join = power_is_initialized;
    is_running = false;
    pthread_mutex_unlock(&power_mutex);
    if (should_join)
        pthread_join(tid, NULL);
}


static bool power_connect
(struct statefs_property *p, struct statefs_slot *slot)
{
    if (strcmp(p->node.name, "is_low"))
        return false;

    pthread_mutex_lock(&power_mutex);
    is_low_slot = slot;
    pthread_mutex_unlock(&power_mutex);
    return true;
}

static void power_disconnect(struct statefs_property *p)
{
    if (strcmp(p->node.name, "is_low"))
        return;

    pthread_mutex_lock(&power_mutex);
    is_low_slot = NULL;
    pthread_mutex_unlock(&power_mutex);
}

static int power_getattr(struct statefs_property const* p)
{
    int res = STATEFS_ATTR_READ;
    if (!strcmp(p->node.name, "is_low"))
        res |= STATEFS_ATTR_DISCRETE;
    return res;
}


struct power_handle
{
    struct statefs_power_prop *p;
    size_t len;
    char buf[255];
};

static ssize_t power_size(struct statefs_property const* p)
{
    return container_of(p, struct statefs_power_prop, prop)->max_size;
}

static intptr_t power_open(struct statefs_property *p, int mode)
{
    if (mode & O_WRONLY) {
        errno = EINVAL;
        return 0;
    }

    if (power_init() < 0)
        return 0;

    struct power_handle *h = calloc(1, sizeof(h[0]));
    h->p = power_prop(p);
    return (intptr_t)h;
}

static int power_read(intptr_t h, char *dst, size_t len, off_t off)
{
    struct power_handle *ph = (struct power_handle *)h;
    if (!off || (off && ph->len <= 0))
        ph->len = ph->p->read(ph->buf, sizeof(ph->buf));

    if (ph->len < 0)
        return ph->len;

    return memcpy_offset(dst, len, off, ph->buf, ph->len);
}

static void power_close(intptr_t h)
{
    free((struct power_handle*)h);
}

static struct power_provider provider = {
    .base = {
        .version = STATEFS_CURRENT_VERSION,
        .node = {
            .type = statefs_node_root,
            .name = "power",
            .release = &power_release
        },
        .branch = {
            .find = ns_find,
            .first = &ns_first,
            .get = &ns_get
        },
        .io = {
            .getattr = power_getattr,
            .open = power_open,
            .read = power_read,
            .size = power_size,
            .close = power_close,
            .connect = power_connect,
            .disconnect = power_disconnect
        }
    }
};


EXTERN_C struct statefs_provider * statefs_provider_get(void)
{
    return &provider.base;
}
