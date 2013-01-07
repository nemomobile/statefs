#include <cor/util.h>
#include <statefs/provider.h>
#include <statefs/util.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/param.h>

#include <pthread.h>

static pthread_mutex_t power_mutex = PTHREAD_MUTEX_INITIALIZER;

static double voltage_now = 3.6;
static double dvoltage = 0.1;
static char voltage_buf[255];
static const char *max_dbl_fmt = "1.0000";

static int memcpy_offset
(char *dst, size_t len, off_t off, char const *src, size_t src_len)
{
    if (!dst || !src || off >= src_len)
        return -EINVAL;
    size_t real_len = MIN(src_len - off, len + off);
    if (real_len)
        memcpy(dst, &src[off], real_len);
    return real_len;
}

static int read_voltage
(struct statefs_property *self, char *dst, size_t len, off_t off)
{
    char buf[sizeof(max_dbl_fmt)];

    pthread_mutex_lock(&power_mutex);
    sprintf(buf, "%1.4f", voltage_now);
    pthread_mutex_unlock(&power_mutex);

    return memcpy_offset(dst, len, off, buf, sizeof(buf) - 1);
}

static size_t double_size()
{
    return sizeof(max_dbl_fmt) - 1;
}

static int read_current
(struct statefs_property *self, char *dst, size_t len, off_t off)
{
    char buf[sizeof(max_dbl_fmt)];
    double r = (double)rand() * 1.0 / RAND_MAX;
    sprintf(buf, "%1.4f", r);
    return memcpy_offset(dst, len, off, buf, sizeof(buf) - 1);
}

static const size_t is_low_fmt_len = sizeof("0");
static bool is_low = false;
static struct statefs_slot * is_low_slot = NULL;

static size_t is_low_size()
{
    return is_low_fmt_len - 1;
}

static bool update_is_low()
{
    bool prev_is_low = is_low;
    is_low = (voltage_now < 3.7);
    return (prev_is_low != is_low);
}

static int read_is_low
(struct statefs_property *self, char *dst, size_t len, off_t off)
{
    char buf[is_low_fmt_len];
    pthread_mutex_lock(&power_mutex);
    sprintf(buf, "%s", is_low ? "1" : "0");
    pthread_mutex_unlock(&power_mutex);
    return memcpy_offset(dst, len, off, buf, sizeof(buf) - 1);
}

static bool connect_is_low
(struct statefs_property *self, struct statefs_slot *slot)
{
    is_low_slot = slot;
    return true;
}

static void disconnect_is_low
(struct statefs_property *self, struct statefs_slot *slot)
{
    is_low_slot = NULL;
}

static const struct statefs_meta voltage_info[] = {
    STATEFS_META("default", REAL, 3.8),
    STATEFS_META_END
};

static const struct statefs_meta current_info[] = {
    STATEFS_META("default", REAL, 0),
    STATEFS_META_END
};

static const struct statefs_meta bat_is_low_info[] = {
    STATEFS_META("default", INT, 0),
    STATEFS_META_END
};

static struct statefs_property props[] = {
    {
        .node = {
            .type = statefs_node_prop,
            .name = "voltage",
            .info = voltage_info
        },
        .read = read_voltage,
        .size = double_size
    },
    {
        .node = {
            .type = statefs_node_prop,
            .name = "current",
            .info = current_info
        },
        .read = read_current,
        .size = double_size
    },
    {
        .node = {
            .type = statefs_node_prop,
            .name = "is_low",
            .info = bat_is_low_info
        },
        .read = read_is_low,
        .size = is_low_size,
        .connect = connect_is_low,
        .disconnect = disconnect_is_low
    }
};
    
static void * control_thread(void *arg)
{
    bool is_low_changed;
    while (true) {

        pthread_mutex_lock(&power_mutex);
        if (voltage_now >= 4.2)
            dvoltage = -0.1;
        if (voltage_now <= 3.1)
            dvoltage = 0.1;
        voltage_now += dvoltage;
        is_low_changed = update_is_low();
        pthread_mutex_unlock(&power_mutex);

        if (is_low_changed && is_low_slot)
            is_low_slot->on_changed(is_low_slot, &props[2]);
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
        if (!strcmp(props[i].node.name, name))
            break;
    if (i == ARRAY_SIZE(props)) {
        fprintf(stderr, "No such property %s\n", name);
        return NULL;
    }
    return &props[i].node;
}

static void prop_next(struct statefs_branch const* self, intptr_t *idx_ptr)
{
    (++*idx_ptr);
}

static struct statefs_node * prop_get
(struct statefs_branch const* self, intptr_t idx)
{
    return (idx < ARRAY_SIZE(props) && idx >= 0) ? &props[idx].node : NULL;
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

static struct power_provider provider = {
    .base = {
        .version = STATEFS_CURRENT_VERSION,
        .node = {
            .type = statefs_node_root,
            .name = "power",
        },
        .branch = {
            .find = ns_find,
            .first = &ns_first,
            .get = &ns_get
        }
    }
};


static bool power_is_initialized = false;

static int power_init()
{
    pthread_attr_t attr;
    pthread_t tid;
    int rc;

    pthread_mutex_lock(&power_mutex);

    if (power_is_initialized)
        goto out;
    sprintf(voltage_buf, "%1.4f", voltage_now);
    rc = pthread_attr_init(&attr);
    if (rc < 0)
        return rc;
    rc = pthread_create(&tid, &attr, control_thread, NULL);
    if (rc < 0)
        return rc;

    power_is_initialized = true;

out:
    pthread_mutex_unlock(&power_mutex);
    return 0;
}

EXTERN_C struct statefs_provider * statefs_provider_get(void)
{
    return !power_init() ? &provider.base : NULL;
}
