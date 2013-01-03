#include "statefs.hpp"

#include <statefs/provider.h>
#include <statefs/util.h>

#include <cor/so.hpp>
#include <cor/util.h>

#include <stdio.h>
#include <stdbool.h>
#include <iostream>


static char const * statefs_cstr_from_variant
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

class Dump
{
public:
    Dump(std::ostream &out) : out(out) {}

    int operator ()(char const *path, std::string &);

private:

    void dump_info(int level, statefs_node const *node, char const *kind);
    void dump_prop(int level, statefs_property const *prop);
    void dump_ns(int level, statefs_namespace const *ns);

    std::ostream &out;
};

void Dump::dump_info(int level, statefs_node const *node, char const *kind)
{
    char buf[255];
    out << "(" << kind << " \"" << node->name << "\"";

    if (node->info) {
        auto info = node->info;
        while (info->name) {
            out << " :" << info->name << " " 
                << statefs_cstr_from_variant(buf, sizeof(buf), &info->value);
            ++info;
        }
    }
}

void Dump::dump_prop(int level, statefs_property const *prop)
{
    out <<"\n";
    dump_info(level, &prop->node, "prop");
    if (!prop->connect)
        out << " :behavior continuous";
    out << ")";
}

void Dump::dump_ns(int level, statefs_namespace const *ns)
{
    out << "\n";
    dump_info(level, &ns->node, "ns");

    intptr_t iter = statefs_first(&ns->branch);
    auto prop = statefs_prop_get(&ns->branch, iter);
    while (prop) {
        dump_prop(level + 1, prop);
        statefs_next(&ns->branch, &iter);
        prop = statefs_prop_get(&ns->branch, iter);
    }
    out << ")";
}

int Dump::operator ()(char const *path, std::string &provider_name)
{
    static const char *provider_main_fn_name = "statefs_provider_get";
    cor::SharedLib lib(path, RTLD_LAZY);

    if (!lib.is_loaded()) {
        std::cerr << "Can't load plugin " << path << "\n";
        return -1;
    }
    auto fn = lib.sym<statefs_provider_fn>(provider_main_fn_name);
    if (!fn) {
        std::cerr << "Can't find " << provider_main_fn_name
                  << " fn in " << path << "\n";
        return -1;
    }
    auto provider = fn();

    provider_name = provider->node.name;
    dump_info(0, &provider->node, "provider");
    out << " :path \"" << path << "\"";
    intptr_t pns = statefs_first(&provider->branch);
    auto ns = statefs_ns_get(&provider->branch, pns);
    while (ns) {
        dump_ns(1, ns);
        statefs_next(&provider->branch, &pns);
        ns = statefs_ns_get(&provider->branch, pns);
    }
    out << ")\n";
    return 0;
}


std::tuple<int, std::string>
dump_plugin_meta(std::ostream &dst, char const *path)
{
    std::string provider_name;
    return std::make_tuple(Dump(dst)(path, provider_name), provider_name);
}
