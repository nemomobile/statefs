#include <cor/util.h>
#include <statefs/provider.h>
#include <statefs/util.h>
#include <stdio.h>
#include <stdbool.h>

#include <cor/so.hpp>

char const * statefs_cstr_from_variant
(char *buf, size_t max_len, struct statefs_variant const *src)
{
    size_t len = max_len - 1;
    switch (src->tag)
    {
    case statefs_variant_int:
        len = snprintf(buf, len, "%ld", src->i);
        break;
    case statefs_variant_uint:
        len = snprintf(buf, len, "%lu", src->u);
        break;
    case statefs_variant_bool:
        len = snprintf(buf, len, "%s", src->b ? "1" : "0");
        break;
    case statefs_variant_real:
        len = snprintf(buf, len, "%f", src->r);
        break;
    case statefs_variant_cstr:
        return src->s;
    default:
        return NULL;
    }
    if (len + 1 == max_len)
        return NULL;
    return buf;
}

static void dump_info(int level, statefs_node const *node,
                      char const *kind)
{
    char buf[255];
    printf("(%s \"%s\"", kind, node->name);

    if (node->info) {
        auto info = node->info;
        while (info->name) {
            printf(" :%s %s", info->name,
                   statefs_cstr_from_variant(buf, sizeof(buf), &info->value));
            ++info;
        }
    }
}

void dump_prop(int level, statefs_property const *prop)
{
    printf("\n");
    dump_info(level, &prop->node, "prop");
    if (!prop->connect)
        printf(" :behavior continuous");
    printf(")");
}

void dump_ns(int level, statefs_namespace const *ns)
{
    printf("\n");
    dump_info(level, &ns->node, "ns");

    intptr_t iter = statefs_first(&ns->branch);
    auto prop = statefs_prop_get(&ns->branch, iter);
    while (prop) {
        dump_prop(level + 1, prop);
        statefs_next(&ns->branch, &iter);
        prop = statefs_prop_get(&ns->branch, iter);
    }
    printf(")");
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        perror("library name");
        return -1;
    }
    cor::SharedLib lib(argv[1], RTLD_LAZY);

    if (!lib.is_loaded())
        return -1;

    auto fn = lib.sym<statefs_provider_fn>("statefs_provider_get");
    if (!fn)
        return -1;
    auto provider = fn();
    dump_info(0, &provider->node, "provider");
    printf(" :path \"%s\"", argv[1]);
    intptr_t pns = statefs_first(&provider->branch);
    auto ns = statefs_ns_get(&provider->branch, pns);
    while (ns) {
        dump_ns(1, ns);
        statefs_next(&provider->branch, &pns);
        ns = statefs_ns_get(&provider->branch, pns);
    }
    printf(")\n");
    return 0;
}
