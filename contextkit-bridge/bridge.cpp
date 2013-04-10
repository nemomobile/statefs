#include "util.hpp"
#include "bridge.hpp"

#include <statefs/provider.h>
#include <statefs/util.h>

#include <cor/error.hpp>
#include <cor/util.hpp>

#include <QFile>
#include <QDomDocument>
#include <QStringList>
#include <QDebug>
#include <QDir>
#include <QLibrary>
#include <QSharedPointer>

#include <memory>
#include <map>
#include <set>
#include <stdexcept>

#include <fcntl.h>


namespace statefs {

template <typename NodeT>
class Node : public NodeT
{
public:
    Node(char const *name, statefs_node const& node_template)
    {
        memcpy(&this->node, &node_template, sizeof(this->node));
        this->node.name = strdup(name);
    }
    virtual ~Node()
    {
        free(const_cast<char*>(this->node.name));
    }
};

template <typename BranchT>
class Branch : public Node<BranchT>
{
public:
    Branch
    (char const *name
     , statefs_node const& node_template
     , statefs_branch const &branch_template)
        : Node<BranchT>(name, node_template)
    {
        memcpy(&this->branch, &branch_template, sizeof(this->branch));
    }

};

class Child
{
public:
    virtual statefs_node *get_node() =0;
    virtual statefs_node const* get_node() const =0;
    virtual statefs_node_type get_type() const =0;
    virtual ~Child() {}
};

class Namespace : public Branch<statefs_namespace>
{
protected:
    typedef std::unique_ptr<Child> child_ptr;
    typedef std::map<std::string, child_ptr> storage_type;
    typedef storage_type::const_iterator iter_type;

private:
    static statefs_node node_template;
    static statefs_branch branch_template;

    static statefs_node * child_find(statefs_branch const*, char const *);

    static statefs_node * child_get(statefs_branch const*, intptr_t);

    static intptr_t child_first(statefs_branch const*);
    static void child_next(statefs_branch const*, intptr_t *);
    static bool child_release(statefs_branch const*, intptr_t);

    static Namespace const *self_cast(statefs_branch const* branch);

public:
    Namespace(char const *name)
        : Branch<statefs_namespace>
          (name, node_template, branch_template)
    {
    }

    void insert(std::string const &name, Child *prop)
    {
        props_.insert(std::make_pair(name, child_ptr(prop)));
    }

protected:

    storage_type props_;
};

statefs_node Namespace::node_template = {
    statefs_node_ns,
    nullptr,
    nullptr,
    nullptr
};


Namespace const *Namespace::self_cast(statefs_branch const* branch)
{
    auto ns = container_of(branch, statefs_namespace, branch);
    return static_cast<Namespace const*>(ns);
}

statefs_node * Namespace::child_find
(statefs_branch const* branch, char const *name)
{
    auto self = self_cast(branch);
    auto iter = self->props_.find(name);
    if (iter == self->props_.end())
        return nullptr;
    auto const &p = iter->second;
    return p.get()->get_node();
}

statefs_node * Namespace::child_get(statefs_branch const *branch, intptr_t h)
{
    auto self = self_cast(branch);
    auto piter = cor::tagged_handle_pointer<iter_type>(h);
    if (!piter)
        return nullptr;

    if (*piter == self->props_.end())
        return nullptr;

    auto const &p = (*piter)->second;
    if (!p)
        throw cor::Error("Child is not initialized");
    return p.get()->get_node();
}

intptr_t Namespace::child_first(statefs_branch const* branch)
{
    auto self = self_cast(branch);
    return cor::new_tagged_handle<iter_type>(self->props_.begin());
}

void Namespace::child_next(statefs_branch const*, intptr_t *h)
{
    auto p = cor::tagged_handle_pointer<iter_type>(*h);
    if (p)
        ++*p;
}

bool Namespace::child_release(statefs_branch const*, intptr_t h)
{
    cor::delete_tagged_handle<iter_type>(h);
    return true;
}

statefs_branch Namespace::branch_template = {
        &Namespace::child_find,
        &Namespace::child_first,
        &Namespace::child_next,
        &Namespace::child_get,
        &Namespace::child_release
};

class PropertyWrapper : public statefs_property
{
public:
    PropertyWrapper()
    {
        default_value.tag = statefs_variant_cstr;
        default_value.s = "";
    }
};

class Property : public statefs::Child
{
    static statefs_node node_template;
protected:
    typedef statefs::Node<PropertyWrapper> node_type;
public:
    static Property *self_cast(statefs_property*);
    static Property const* self_cast(statefs_property const*);

    Property(char const* name)
        : prop_(name, node_template)
    {
    }

    virtual statefs_node *get_node();
    virtual statefs_node const* get_node() const;
    virtual statefs_node_type get_type() const;
protected:
    node_type prop_;
    static node_type Property::* prop_offset;
};

Property::node_type Property::* Property::prop_offset = &Property::prop_;

Property *Property::self_cast(statefs_property *p)
{
    auto n = static_cast<node_type*>(p);
    return container_of(n, Property, prop_);
}

Property const* Property::self_cast(statefs_property const *p)
{
    auto n = static_cast<node_type const*>(p);
    return container_of(n, Property, prop_);
}

statefs_node Property::node_template = {
    statefs_node_prop,
    nullptr,
    nullptr,
    nullptr
};


statefs_node *Property::get_node()
{
    return &prop_.node;
}

statefs_node const* Property::get_node() const
{
    return &prop_.node;
}

statefs_node_type Property::get_type() const
{
    return prop_.node.type;
}

} // namespace

// --------------------------------------------------

typedef ContextSubscriber::PluginFactoryFunc plugin_factory_type;

class PropInfo;
typedef std::shared_ptr<PropInfo> property_ptr;
typedef std::map<QString, std::list<property_ptr> > plugin_properties_type;

typedef std::map<QString, property_ptr> prop_info_type;
typedef std::map<QString, prop_info_type> info_tree_type;

typedef std::shared_ptr<QLibrary> plugin_ptr;
typedef std::map<QString, plugin_ptr> plugins_type;


class PropInfo
{
public:
    static property_ptr create(QString const&, QString const&);

private:
    PropInfo(QString const &full_name, QString const &ns
             , QString const &name, QString const &plugin)
        : full_name_(full_name), ns_(ns), name_(name), plugin_(plugin)
    {}

public:
    QString full_name_;
    QString ns_;
    QString name_;
    QString plugin_;
};

QDebug & operator << (QDebug &dst, PropInfo const &src)
{
    dst << src.ns_ << "." << src.name_
        << " (" << src.full_name_ << ")"
        << "->" << src.plugin_;
    return dst;
}

property_ptr PropInfo::create(QString const& name, QString const& plugin)
{
    QStringList parts;
    if (!getPropertyInfo(name, parts) || parts.size() != 2)
        throw cor::Error((name + " is not correct?").toStdString());
    return property_ptr(new PropInfo(name, parts[0], parts[1], plugin));
}

static void read_plugin_info(QString const &fileName, plugin_properties_type &dst)
{
    QDomDocument doc;
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No file";
        return;
    }

    if (!doc.setContent(&file)) {
        qDebug() << "No content";
        return;
    }
    file.close();

    auto docElem = doc.documentElement();
    if (docElem.tagName() != "provider") {
        qDebug() << "Invalid context xml, root tag is " << docElem.tagName()
                 << " expected 'provider'";
        return;
    }

    auto plugin = docElem.attribute("plugin");
    plugin.replace(QRegExp("^/"), "");
    auto &info = dst[plugin];

    auto nodes = docElem.elementsByTagName("key");

    for(int i = 0; i < nodes.count(); ++i) {
        auto e = nodes.at(i).toElement();
        auto name = e.attribute("name");
        info.push_back(PropInfo::create(name, plugin));
    }
}

static void build_tree(plugins_type &plugins, info_tree_type &infoTree)
{
    static const QString info_path("/usr/share/contextkit/providers");
    plugin_properties_type plugins_info;
    QStringList filters({"*.context"});
    QDir info_dir(info_path);
    for (auto const &f: info_dir.entryList(filters)) {
        qDebug() << info_dir.filePath(f);
        read_plugin_info(info_dir.filePath(f), plugins_info);
    }

    for(auto const& plugin_props: plugins_info) {
        auto name = plugin_props.first;
        plugins[name] = nullptr;
        qDebug() << name;
        for (auto prop: plugin_props.second) {
            auto &ns = infoTree[prop->ns_];
            ns[prop->name_] = prop;
        }
    }
}

class NamespaceNode;

static info_tree_type info_tree;
static plugins_type plugins;
static std::vector<std::unique_ptr<NamespaceNode> > namespaces;

static const QString plugins_path = "/usr/lib/contextkit/subscriber-plugins/";

static plugin_ptr plugin_get(QString const &name)
{
    auto p = plugins.find(name);
    if (p == plugins.end())
        throw cor::Error((QString("No such plugin ") + name).toStdString());

    if (!p->second) {
        auto path = plugins_path + name + ".so";
        plugin_ptr lib(new QLibrary(path));
        lib->load();
        if (!lib->isLoaded()) {
            throw cor::Error((QString("Can't find ") + path).toStdString());
        }
        p->second = lib;
    }
    return p->second;
}

static plugin_factory_type plugin_factory_get(QString const &name)
{
    auto p = plugin_get(name);
    plugin_factory_type factory
        = reinterpret_cast<plugin_factory_type>(p->resolve("pluginFactory"));
    if (!factory)
        throw cor::Error(std::string("No pluginFactory in ") + name.toStdString());
    return factory;
}

static provider_ptr provider_get(QString const &name)
{
    auto factory = plugin_factory_get(name);
    return provider_ptr(factory(name));
}

typedef std::shared_ptr<ProviderBridge> bridge_ptr;
static std::map<QString, bridge_ptr> bridges;

bridge_ptr bridge_get(QString const& name)
{
    auto iter = bridges.find(name);
    if (iter != bridges.end())
        return iter->second;

    bridge_ptr p(new ProviderBridge(name));
    bridges.insert(std::make_pair(name, p));
    return p;
}

class CKitProperty : public statefs::Property
{
public:
    CKitProperty(property_ptr p)
        : statefs::Property(p->name_.toUtf8().constData())
        , info_(p)
        , slot_(nullptr)
        , conn_count_(0)
    {
    }
    std::string get_name() const
    {
        return info_->name_.toStdString();
    }

    void notify(QVariant const &v)
    {
        v_ = v.toString();
        if (slot_)
            slot_->on_changed(slot_, &prop_);
    }

    void connect(statefs_slot *slot)
    {
        if (!bridge_) {
            bridge_ = bridge_get(info_->plugin_);
            bridge_->subscribe(info_->full_name_, this);
        }
        if (slot)
            slot_ = slot;
        ++conn_count_;
    }

    void disconnect()
    {
        if (--conn_count_) {
            bridge_->unsubscribe(info_->full_name_);
            slot_ = nullptr;
        }
    }

    QString value() const
    {
        return v_;
    }

    size_t size() const {
        return 256; // TODO tmp
    }
    static CKitProperty *self_cast(statefs_property*);
    static CKitProperty const* self_cast(statefs_property const*);

private:
    property_ptr info_;
    QString v_;
    bridge_ptr bridge_;
    statefs_slot *slot_;
    int conn_count_;
};

CKitProperty *CKitProperty::self_cast(statefs_property *from)
{
    return static_cast<CKitProperty*>(statefs::Property::self_cast(from));
}

CKitProperty const* CKitProperty::self_cast(statefs_property const* from)
{
    return static_cast<CKitProperty const*>(statefs::Property::self_cast(from));
}

class PropHandle
{
public:
    PropHandle(CKitProperty *prop)
        : prop_(prop)
    {
        prop_->connect(nullptr);
    }

    ~PropHandle()
    {
        prop_->disconnect();
    }

    int read(char *dst, size_t len, off_t off)
    {
        if (!len)
            return 0;

        if (!off)
            cache_ = prop_->value();

        auto clen = cache_.size();

        if (off > clen)
            return 0;

        if (off + len > clen)
            len = clen - off;
        memcpy(dst, cache_.toUtf8().constData() + off, len);
        return len;
    }

private:
    CKitProperty *prop_;
    QString cache_;
};

class NamespaceNode : public statefs::Namespace
{
public:
    NamespaceNode(QString const &aname)
        : statefs::Namespace(aname.toUtf8().constData())
    {
    }
    void insert(CKitProperty *p) {
        Namespace::insert(p->get_name(), p);
    }
};

ProviderBridge::~ProviderBridge()
{
    if (provider_)
        disconnect(provider_.get(), SIGNAL(valueChanged(QString, QVariant))
                   , this, SLOT(onValue(QString, QVariant)));

    for (auto &v: subscribers_) {
        QSet<QString> nset;
        nset.insert(v.first);
        provider()->unsubscribe(nset);
    }
}

void ProviderBridge::subscribe(QString const &name, CKitProperty *dst)
{
    subscribers_[name] = dst;
    QSet<QString> nset;
    nset.insert(name);
    provider()->subscribe(nset);
}

void ProviderBridge::unsubscribe(QString const &name)
{
    subscribers_.erase(name);
}

void ProviderBridge::onValue(QString key, QVariant value)
{
    auto p = subscribers_[key];
    if (p)
        p->notify(value);
}

provider_ptr ProviderBridge::provider()
{
    if (!provider_) {
        provider_ = provider_get(name_);
        connect(provider_.get(), SIGNAL(valueChanged(QString, QVariant))
                , this, SLOT(onValue(QString, QVariant)));
    }
    return provider_;
}


static struct statefs_node * ns_find
(struct statefs_branch const* self, char const *name)
{
    if (!name)
        return nullptr;

    // just a linear search now, not so much namespaces expected
    for (size_t i = 0; i < namespaces.size(); ++i) {
        auto &node = namespaces[i]->node;
        if (!strcmp(name, node.name))
            return &node;
    }
    return nullptr;
}

static struct statefs_node * ns_get
(struct statefs_branch const* self, intptr_t idx)
{
    if ((unsigned)idx >= namespaces.size())
        return nullptr;
    return &(namespaces[idx]->node);
}

static intptr_t ns_first(struct statefs_branch const* self)
{
    return 0;
}

static void ns_next(struct statefs_branch const*, intptr_t *idx_ptr)
{
    ++*idx_ptr;
}

static bool ckit_connect
(struct statefs_property *p, struct statefs_slot *slot)
{
    auto self = CKitProperty::self_cast(p);
    self->connect(slot);
    return false;
}

static void ckit_disconnect(struct statefs_property *p)
{
    auto self = CKitProperty::self_cast(p);
    self->disconnect();
}

static int ckit_getattr(struct statefs_property const* p)
{
    return STATEFS_ATTR_READ | STATEFS_ATTR_DISCRETE;
}

static ssize_t ckit_size(struct statefs_property const* p)
{
    auto self = CKitProperty::self_cast(p);
    qDebug() << "S " << self->size();
    return self->size();
}

static intptr_t ckit_open(struct statefs_property *p, int mode)
{
    qDebug() << "Opening";
    if (mode & O_WRONLY) {
        errno = EINVAL;
        return 0;
    }
    auto self = CKitProperty::self_cast(p);
    return cor::new_tagged_handle<PropHandle>(self);
}

static int ckit_read(intptr_t h, char *dst, size_t len, off_t off)
{
    auto p = cor::tagged_handle_pointer<PropHandle>(h);
    return (p) ? p->read(dst, len, off) : 0;
}

static void ckit_close(intptr_t h)
{
    cor::delete_tagged_handle<PropHandle>(h);
}

static void ckit_release(struct statefs_node *node)
{
    qDebug() << "Clear";
    namespaces.clear();
}

static struct statefs_provider provider = {
    version:  STATEFS_CURRENT_VERSION,
    root: {
    node:  {
        type:  statefs_node_root,
        name:  "contextkit",
        release:  &ckit_release,
        info: nullptr
    },
    branch:  {
        find:  ns_find,
        first:  &ns_first,
        next:  &ns_next,
        get:  &ns_get,
        release: nullptr
    }
    },
    io:  {
        getattr:  ckit_getattr,
        size:  ckit_size,
        open:  ckit_open,
        read:  ckit_read,
        write:  nullptr,
        close:  ckit_close,
        connect:  ckit_connect,
        disconnect:  ckit_disconnect
    }
};

static bool is_loaded = false;
static void load_info()
{
    if (is_loaded)
        return;

    build_tree(plugins, info_tree);

    namespaces.resize(info_tree.size());
    auto pns = namespaces.begin();
    for (auto const &ns_prop: info_tree) {
        pns->reset(new NamespaceNode(ns_prop.first));

        // debuggin
        // qDebug() << "NS:" << ns_prop.first;
        for (auto const &prop: ns_prop.second) {
            auto pinfo = prop.second;
            // qDebug() << prop.first << "->" << *pinfo;
            (*pns)->insert(new CKitProperty(prop.second));
        }
        ++pns;
    }
    is_loaded = true;
}

EXTERN_C struct statefs_provider * statefs_provider_get(void)
{
    load_info();
    return &provider;
}

// int main()
// {
//     build_tree(plugins, info_tree);

//     namespaces.resize(info_tree.size());
//     auto pns = namespaces.begin();
//     for (auto const &ns_prop: info_tree) {
//         pns->reset(new NamespaceNode(ns_prop.first));

//         // debuggin
//         qDebug() << "NS:" << ns_prop.first;
//         for (auto const &prop: ns_prop.second) {
//             auto pinfo = prop.second;
//             qDebug() << prop.first << "->" << *pinfo;
//             (*pns)->insert(new CKitProperty(prop.second));
//         }
//         ++pns;
//     }

//     auto pp = provider_get("upower");
//     qDebug() << pp.get();

//     auto x = ns_first(nullptr);
//     while (true) {
//         auto n = ns_get(nullptr, x);
//         if (!n)
//             break;
//         qDebug() << "N:" << n->name;
//         ns_next(nullptr, &x);
//     }
//     auto n = ns_find(nullptr, "Battery");
//     if (n)
//         qDebug() << "Found Battery" << n->name;
//     else
//         qDebug() << "Not Found Battery!!!";

//     n = ns_find(nullptr, "System");
//     if (n)
//         qDebug() << "Found System" << n->name;
//     else
//         qDebug() << "Not Found System!!!";

//     return 0;
// }
