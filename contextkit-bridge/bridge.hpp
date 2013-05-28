#ifndef _STATEFS_CKIT_BRIDGE_HPP_
#define _STATEFS_CKIT_BRIDGE_HPP_

#include <iproviderplugin.h>

#include <QObject>
#include <QString>
#include <QVariant>
#include <QThread>
#include <QWaitCondition>
#include <QMutex>
#include <QEvent>
#include "QCoreAppWrapper.hpp"

#include <memory>

class CKitProperty;
using ContextSubscriber::IProviderPlugin;
using ContextSubscriber::TimedValue;
typedef std::shared_ptr<IProviderPlugin> provider_ptr;

typedef ContextSubscriber::PluginFactoryFunc plugin_factory_type;

class ProviderFactory;
typedef std::shared_ptr<ProviderFactory> provider_factory_ptr;

class ProviderThread;
class ProviderBridge : public QObject
{
    Q_OBJECT;
public:
    ProviderBridge(provider_factory_ptr, ProviderThread *);
    ~ProviderBridge();

    virtual bool event(QEvent *);

private slots:
    void onValue(QString, QVariant);
    void onSubscribed(QString);
    void onSubscribed(QString, TimedValue);
    void onSubscribeFailed(QString, QString);

private:
    void subscribe(QString const &name, CKitProperty *dst);
    void unsubscribe(QString const &name);

    provider_ptr provider();

    provider_factory_ptr factory_;
    provider_ptr provider_;
    std::map<QString, CKitProperty *> subscribers_;
    QThread *thread_;
    std::map<QString, QVariant> cache_;
};


class ProviderThread : public QThread
{
    Q_OBJECT;
public:
    ProviderThread(provider_factory_ptr);
    virtual ~ProviderThread();

    void subscribe(QString const &name, CKitProperty *dst);
    void unsubscribe(QString const &name);

    void subscribed();

private:
    void run();

    ProviderBridge* bridge_;
    provider_factory_ptr factory_;
    QMutex mutex_;
    QWaitCondition cond_;
};

typedef std::shared_ptr<ProviderThread> bridge_ptr;

class QtBridge : public QCoreAppWrapper
{
    Q_OBJECT
public:
    bridge_ptr bridge_get(provider_factory_ptr factory);
private:
    std::map<QString, bridge_ptr> bridges;
};

class ProviderEvent : public QEvent
{
public:
    enum Type {
        Subscribe = QEvent::User,
        Unsubscribe
    };

    virtual ~ProviderEvent() {}

protected:
    ProviderEvent(Type t)
    : QEvent(static_cast<QEvent::Type>(t))
    {}
private:
    ProviderEvent();
};

class ProviderSubscribe : public ProviderEvent
{
public:
    ProviderSubscribe(QString const &name, CKitProperty *dst)
        : ProviderEvent(ProviderEvent::Subscribe)
        , name_(name), dst_(dst)
    {}

    QString name_;
    CKitProperty *dst_;
};

class ProviderUnsubscribe : public ProviderEvent
{
public:
    ProviderUnsubscribe(QString const &name)
        : ProviderEvent(ProviderEvent::Unsubscribe), name_(name)
    {}

    QString name_;
};

#endif // _STATEFS_CKIT_BRIDGE_HPP_
