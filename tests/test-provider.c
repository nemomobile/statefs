/*
 * Provider for unit testing
 *
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 * 
 * http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <statefs/provider.h>
#include <statefs/util.h>
#include <cor/util.h>

#include <sys/param.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>


struct test_prop
{
    struct statefs_property prop;
    int (*read)(struct test_prop*, char *, statefs_size_t);
    int (*write)(struct test_prop*, char const*, statefs_size_t);
    int last_handle;
    int access_count;
    char buf[64];
    statefs_size_t size;
};

static int read_test_value
(struct test_prop *self, char *dst, statefs_size_t s)
{
    s = MIN(s, self->size);
    memcpy(dst, self->buf, s);
    return s;
}

static int write_test_value
(struct test_prop *self, char const *src, statefs_size_t s)
{
    if (s > sizeof(self->buf))
        return -EINVAL;

    memcpy(self->buf, src, s);
    return s;
}

static struct test_prop props[] = {
    {
        .prop = {
            .node = {
                .type = statefs_node_prop,
                .name = "a"
            },
            .default_value = STATEFS_CSTR("1")
        },
        .read = read_test_value,
        .write = write_test_value,
        .buf = "1",
        .size = 1
    },
    {
        .prop = {
            .node = {
                .type = statefs_node_prop,
                .name = "b"
            },
            .default_value = STATEFS_CSTR("20"),
        },
        .read = read_test_value,
        .write = write_test_value,
        .buf = "20",
        .size = 2
    },
    {
        .prop = {
            .node = {
                .type = statefs_node_prop,
                .name = "c"
            },
            .default_value = STATEFS_CSTR("300")
        },
        .read = read_test_value,
        .write = write_test_value,
        .buf = "300",
        .size = 3
    }
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

static void prop_next(struct statefs_branch const* self, statefs_handle_t *idx_ptr)
{
    (++*idx_ptr);
}

static struct statefs_node * prop_get
(struct statefs_branch const* self, statefs_handle_t idx)
{
    return (idx < ARRAY_SIZE(props) && idx >= 0) ? &props[idx].prop.node : NULL;
}

static statefs_handle_t prop_first(struct statefs_branch const* self)
{
    return 0;
}

static struct statefs_namespace test1_ns = {
    .node = {
        .type = statefs_node_ns,
        .name = "ns1",
    },
    .branch = {
        .find = prop_find,
        .first = prop_first,
        .next = prop_next,
        .get = prop_get,
    }
};

static struct statefs_node * ns_find
(struct statefs_branch const* self, char const *name)
{
    if (!strcmp("ns1", name))
        return &test1_ns.node;
    return NULL;
}

static struct statefs_node * ns_get
(struct statefs_branch const* self, statefs_handle_t p)
{
    return (p ? &((struct statefs_namespace*)p)->node : NULL);
}

static statefs_handle_t ns_first(struct statefs_branch const* self)
{
    return (statefs_handle_t)&test1_ns;
}

static void test_prov_release(struct statefs_node *node)
{
}

static bool test_prov_connect
(struct statefs_property *p, struct statefs_slot *slot)
{
    return false;
}

static void test_prov_disconnect(struct statefs_property *p)
{
}

static int test_prov_getattr(struct statefs_property const* p)
{
    int res = STATEFS_ATTR_READ;
    return res;
}


struct test_prop_handle
{
    struct test_prop *p;
    int id;
};

static statefs_ssize_t test_prov_size(struct statefs_property const* p)
{
    return container_of(p, struct test_prop, prop)->size;
}

static statefs_handle_t test_prov_open(struct statefs_property *p, int mode)
{
    if (mode & O_WRONLY) {
        errno = EINVAL;
        return 0;
    }

    struct test_prop_handle *h = calloc(1, sizeof(h[0]));
    struct test_prop *self = container_of(p, struct test_prop, prop);
    h->id = self->last_handle++;
    h->p = self;
    return (statefs_handle_t)h;
}

static int test_prov_read(statefs_handle_t h, char *dst, statefs_size_t len, statefs_off_t off)
{
    struct test_prop_handle *ph = (struct test_prop_handle *)h;
    if (off)
        return -EINVAL; /* no processing for off */

    return ph->p->read(ph->p, dst, len);
}

static int test_prov_write(statefs_handle_t h, char const *src, statefs_size_t len, statefs_off_t off)
{
    struct test_prop_handle *ph = (struct test_prop_handle *)h;
    if (off)
        return -EINVAL; /* no processing for off */

    return ph->p->write(ph->p, src, len);
}

static void test_prov_close(statefs_handle_t h)
{
    free((struct test_prov_handle*)h);
}

static struct statefs_provider provider = {
    .version = STATEFS_CURRENT_VERSION,
    .root = {
        .node = {
            .type = statefs_node_root,
            .name = "test",
            .release = &test_prov_release
        },
        .branch = {
            .find = ns_find,
            .first = &ns_first,
            .get = &ns_get
        }
    },
    .io = {
        .getattr = test_prov_getattr,
        .open = test_prov_open,
        .read = test_prov_read,
        .write = test_prov_write,
        .size = test_prov_size,
        .close = test_prov_close,
        .connect = test_prov_connect,
        .disconnect = test_prov_disconnect
    }
};

EXTERN_C struct statefs_provider * statefs_provider_get
(struct statefs_server *e)
{
    return &provider;
}
