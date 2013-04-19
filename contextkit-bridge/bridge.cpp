#include "util.hpp"
#include "bridge.hpp"

#include <statefs/provider.hpp>
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
#include <chrono>

#include <fcntl.h>



// --------------------------------------------------


class PropInfo;
typedef std::shared_ptr<PropInfo> property_ptr;
typedef std::map<QString, std::list<property_ptr> > plugin_properties_type;

typedef std::map<QString, property_ptr> prop_info_type;
typedef std::map<QString, prop_info_type> info_tree_type;
typedef std::shared_ptr<QLibrary> plugin_ptr;

class NamespaceNode;

static std::vector<std::unique_ptr<NamespaceNode> > namespaces;
static std::unique_ptr<QtBridge> qt_app;


class ProviderFactory
{
public:
    ProviderFactory(QString const &id) : id_(id) {}
    virtual provider_ptr get() =0;
    QString get_id() const { return id_; }
private:
    QString id_;
};

class SharedObjFactory : public ProviderFactory
{
public:
    SharedObjFactory(QString const &baseName
                     , QString const &plugin
                     , QString const &constructionString)
        : ProviderFactory(baseName)
        , plugin_name_(plugin)
        , constructionString_(constructionString)
    {}
    
    virtual ~SharedObjFactory() {}
    virtual provider_ptr get()
    {
        if (provider_)
            return provider_;

        plugin_ = plugin_get(plugin_name_);

        auto factory = reinterpret_cast<plugin_factory_type>
            (plugin_->resolve("pluginFactory"));
        if (!factory) {
            auto msg = QString("No pluginFactory in ") + plugin_name_;
            throw cor::Error(msg.toStdString());
        }

        provider_.reset(factory(constructionString_));
        return provider_;
    }

private:

    static plugin_ptr plugin_get(QString const &name);

    QString plugin_name_;
    QString constructionString_;
    plugin_ptr plugin_;
    provider_ptr provider_;
};


plugin_ptr SharedObjFactory::plugin_get(QString const &name)
{
    // TODO hard-coded path
    static const QString plugins_path
        = "/usr/lib/contextkit/subscriber-plugins/";

    auto path = plugins_path + name + ".so";
    plugin_ptr lib(new QLibrary(path));
    lib->load();
    if (!lib->isLoaded())
        throw cor::Error((QString("Can't find ") + path).toStdString());

    return lib;
}

class DBusFactory : public ProviderFactory
{
public:
    DBusFactory(QString const &baseName
                , QString const &bus
                , QString const &service)
        : ProviderFactory(baseName)
        , bus_(bus)
        , service_(service)
    {}

    virtual provider_ptr get()
    {
        if (!provider_)
            initialize();

        return provider_;
    }
private:

    void initialize()
    {
        auto sym = ::dlsym(RTLD_DEFAULT, "contextKitPluginFactory");
        auto factory = reinterpret_cast<plugin_factory_type>(sym);
        
        QStringList parts({bus_, service_});
        provider_.reset(factory
                        ? factory(parts.join(":"))
                        : nullptr);
    }

    QString bus_;
    QString service_;
    provider_ptr provider_;
};

class PropInfo
{
public:
    static property_ptr create(QString const&, provider_factory_ptr);

private:
    PropInfo(QString const &full_name
             , QString const &ns
             , QString const &name
             , provider_factory_ptr factory)
        : full_name_(full_name)
        , ns_(ns)
        , name_(name)
        , factory_(factory)
    {}

public:
    QString full_name_;
    QString ns_;
    QString name_;
    provider_factory_ptr factory_;
};

QDebug & operator << (QDebug &dst, PropInfo const &src)
{
    dst << src.ns_ << "." << src.name_
        << " (" << src.full_name_ << ")"
        << "->" << src.factory_->get_id();
    return dst;
}

property_ptr PropInfo::create
(QString const& name, provider_factory_ptr factory)
{
    QStringList parts;
    if (!getPropertyInfo(name, parts) || parts.size() != 2)
        throw cor::Error((name + " is not correct?").toStdString());
    return property_ptr(new PropInfo(name, parts[0], parts[1], factory));
}

static bool read_plugin_info(QFileInfo const &fileInfo, plugin_properties_type &dst)
{
    QString baseName(fileInfo.baseName());

    QString fileName(fileInfo.canonicalFilePath());
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)){ 
        qDebug() << "No file";
        return false;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        qDebug() << "No content";
        return false;
    }
    file.close();

    auto docElem = doc.documentElement();
    if (docElem.tagName() != "provider") {
        qDebug() << "Invalid context xml, root tag is " << docElem.tagName()
                 << " expected 'provider'";
        return false;
    }

    auto plugin = docElem.attribute("plugin");
    auto constructionString = docElem.attribute("constructionString");
    auto &info = dst[baseName];
    provider_factory_ptr factory;

    if (plugin.size()) {
        plugin.replace(QRegExp("^/"), "");
        factory.reset
            (new SharedObjFactory(baseName, plugin, constructionString));
    } else {
        auto bus = docElem.attribute("bus");
        auto service = docElem.attribute("service");

        if (!(bus.size() && service.size())) {
            qDebug() << "Unknown plugin description, skipping";
            return false;
        }
        factory.reset(new DBusFactory(baseName, bus, service));
        
    }
    auto nodes = docElem.elementsByTagName("key");

    for(int i = 0; i < nodes.count(); ++i) {
        auto e = nodes.at(i).toElement();
        auto name = e.attribute("name");
        info.push_back(PropInfo::create(name, factory));
    }
    return true;
}

static void build_tree(info_tree_type &infoTree)
{
    static const QString info_path("/usr/share/contextkit/providers");
    plugin_properties_type plugins_info;
    QStringList filters({"*.context"});
    QDir info_dir(info_path);
    for (auto const &f: info_dir.entryInfoList(filters)) {
        read_plugin_info(f, plugins_info);
    }

    for(auto const& plugin_props: plugins_info) {
        auto name = plugin_props.first;
        for (auto prop: plugin_props.second) {
            auto &ns = infoTree[prop->ns_];
            ns[prop->name_] = prop;
        }
    }
}

class CKitProperty : public statefs::Property
{
public:
    CKitProperty(property_ptr p)
        : statefs::Property(p->name_.toUtf8().constData())
        , info_(p)
        , slot_(nullptr)
        , conn_count_(0)
        , is_first_access_(true)
        , is_initialized_(false)
    {}

    ~CKitProperty() {}

    std::string get_name() const
    {
        return info_->name_.toStdString();
    }

    void notify(QVariant const &v)
    {
        v_ = v.toString();
        is_initialized_ = true;
        do {
            std::lock_guard<std::mutex> lock(mutex_);
            initialized_.notify_all();
        } while(0);

        if (slot_)
            slot_->on_changed(slot_, &prop_);
    }

    void connect(statefs_slot *slot)
    {
        if (!bridge_) {
            bridge_ = qt_app->bridge_get(info_->factory_);
        }
        if (!conn_count_)
            bridge_->subscribe(info_->full_name_, this);

        if (slot)
            slot_ = slot;
        ++conn_count_;
    }

    void disconnect()
    {
        if (--conn_count_) {
            if (bridge_)
                bridge_->unsubscribe(info_->full_name_);
            slot_ = nullptr;
        }
    }

    QString value() const
    {
        if (is_first_access_) {
            std::unique_lock<std::mutex> lock(mutex_);
            initialized_.wait_for(lock,  std::chrono::milliseconds(1000)
                                  , [this]() { return is_initialized_; });
            is_first_access_ = false;
        }
        return v_;
    }

    size_t size() const {
        return is_initialized_ ? v_.size() : 1024;
    }
    static CKitProperty *self_cast(statefs_property*);
    static CKitProperty const* self_cast(statefs_property const*);

private:
    property_ptr info_;
    QString v_;
    bridge_ptr bridge_;
    statefs_slot *slot_;
    int conn_count_;

    mutable std::mutex mutex_;
    mutable std::condition_variable initialized_;
    mutable bool is_first_access_;
    mutable bool is_initialized_;
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
    {}

    virtual ~NamespaceNode() {}

    void insert(CKitProperty *p) {
        Namespace::insert(p->get_name(), p);
    }
};

ProviderBridge::ProviderBridge(provider_factory_ptr factory, ProviderThread *parent)
    : factory_(factory), parent_(parent)
{}

ProviderBridge::~ProviderBridge()
{
    if (provider_) {
        disconnect(provider_.get(), SIGNAL(subscribeFinished(QString, TimedValue))
                , this, SLOT(onSubscribed(QString, TimedValue)));
        disconnect(provider_.get(), SIGNAL(subscribeFinished(QString))
                   , this, SLOT(onSubscribed(QString)));
        disconnect(provider_.get(), SIGNAL(valueChanged(QString, QVariant))
                   , this, SLOT(onValue(QString, QVariant)));
    }
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
    auto p = cache_.find(name);
    if (p != cache_.end()) {
        // provider is already supplied it, notify subscriber
        dst->notify(p->second);
    }
}

void ProviderBridge::unsubscribe(QString const &name)
{
    subscribers_.erase(name);

    QSet<QString> nset;
    nset.insert(name);
    provider()->unsubscribe(nset);
}

void ProviderBridge::onValue(QString key, QVariant value)
{
    auto p = subscribers_[key];
    if (p) {
        p->notify(value);
    } else {
        // not subscribed but contextkit already issued value,
        // remember it to instantly provide to subscriber later
        cache_[key] = value;
    }
}

void ProviderBridge::onSubscribed(QString key)
{
}

void ProviderBridge::onSubscribed(QString key, TimedValue value)
{
    onValue(key, value.value);
}

provider_ptr ProviderBridge::provider()
{
    if (!provider_) {
        provider_ = factory_->get();
        connect(provider_.get(), SIGNAL(valueChanged(QString, QVariant))
                , this, SLOT(onValue(QString, QVariant)));
        connect(provider_.get(), SIGNAL(subscribeFinished(QString))
                , this, SLOT(onSubscribed(QString)));
        connect(provider_.get(), SIGNAL(subscribeFinished(QString, TimedValue))
                , this, SLOT(onSubscribed(QString, TimedValue)));
    }
    return provider_;
}

bridge_ptr QtBridge::bridge_get(provider_factory_ptr factory)
{
    auto name = factory->get_id();
    auto iter = bridges.find(name);
    if (iter != bridges.end())
        return iter->second;

    bridge_ptr p(new ProviderThread(factory));
    bridges.insert(std::make_pair(name, p));
    return p;
}

ProviderThread::ProviderThread(provider_factory_ptr factory)
    : factory_(factory)
{
    mutex_.lock();
    start();
    cond_.wait(&mutex_, 200);
    mutex_.unlock();
}

ProviderThread::~ProviderThread()
{
    // TODO there should be another condition to wait on
    exit(0);
    while (!isFinished()) {
        usleep(1000);
    }
}

void ProviderThread::run()
{
    bridge_.reset(new ProviderBridge(factory_, this));
    mutex_.lock();
    cond_.wakeAll();
    mutex_.unlock();
    exec();
}

void ProviderThread::subscribe(QString const &name, CKitProperty *dst)
{
    QCoreApplication::postEvent
        (bridge_.get(), new ProviderSubscribe(name, dst));
}

void ProviderThread::unsubscribe(QString const &name)
{
    QCoreApplication::postEvent(bridge_.get(), new ProviderUnsubscribe(name));
}

bool ProviderBridge::event(QEvent *e)
{
    try {
        switch (static_cast<ProviderEvent::Type>(e->type())) {
        case (ProviderEvent::Subscribe): {
            auto s = static_cast<ProviderSubscribe*>(e);
            subscribe(s->name_, s->dst_);
            return true;
        }
        case (ProviderEvent::Unsubscribe): {
            auto s = static_cast<ProviderUnsubscribe*>(e);
            unsubscribe(s->name_);
            return true;
        }
        default:
            return QObject::event(e);
        }
    } catch (cor::Error const &e) {
        qDebug() << "Caught cor::Error " << e.what();
    } catch (...) { // Qt does not allow exceptions from event handlers
        qDebug() << "event: caught unknown exception";
    }
    return false;
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
    return self->size();
}

static intptr_t ckit_open(struct statefs_property *p, int mode)
{
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
    namespaces.clear();
    //info_tree.clear();
    //qt_app.reset(nullptr);
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

    qt_app.reset(new QtBridge());
    info_tree_type info_tree;
    build_tree(info_tree);

    namespaces.resize(info_tree.size());
    auto pns = namespaces.begin();
    for (auto const &ns_prop: info_tree) {
        pns->reset(new NamespaceNode(ns_prop.first));

        for (auto const &prop: ns_prop.second) {
            auto pinfo = prop.second;
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
