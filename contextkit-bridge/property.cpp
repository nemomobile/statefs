#include "property.hpp"
#include "util.hpp"

#include <contextproperty.h>
#include <QDebug>
#include <QSocketNotifier>

ContextProperty::ContextProperty(const QString &key, QObject *parent)
    : QObject(parent)
    , priv(new ContextPropertyPrivate(key, this))
{
}

ContextProperty::~ContextProperty()
{
}

QString ContextProperty::key() const
{
    return priv->key();
}

QVariant ContextProperty::value(const QVariant &def) const
{
    return priv->value(def);
}

QVariant ContextProperty::value() const
{
    return priv->value();
}

const ContextPropertyInfo* ContextProperty::info() const
{
    return priv->info();
}

void ContextProperty::subscribe () const
{
    return priv->subscribe();
}

void ContextProperty::unsubscribe () const
{
    return priv->unsubscribe();
}

void ContextProperty::waitForSubscription() const
{
    return priv->waitForSubscription();
}

void ContextProperty::waitForSubscription(bool block) const
{
    return priv->waitForSubscription(block);
}

void ContextProperty::ignoreCommander()
{
}

void ContextProperty::setTypeCheck(bool typeCheck)
{
}

ContextPropertyPrivate::ContextPropertyPrivate(const QString &key, QObject *parent)
    : QObject(parent)
    , key_(key)
    , file_(getStateFsPath(key))
    , notifier_(nullptr)
{
}

ContextPropertyPrivate::~ContextPropertyPrivate()
{
}

QString ContextPropertyPrivate::key() const
{
    return key_;
}

QVariant ContextPropertyPrivate::value(const QVariant &defVal) const
{
    if (!file_.isOpen())
        return defVal;

    return cKitValueDecode(QString(file_.readAll()));
}

QVariant ContextPropertyPrivate::value() const
{
    return value(QVariant());
}

const ContextPropertyInfo* ContextPropertyPrivate::info() const
{
    return nullptr; // TODO
}

void ContextPropertyPrivate::handleActivated(int)
{
    emit valueChanged();
}

void ContextPropertyPrivate::subscribe () const
{
    if (file_.isOpen())
        return;

    if (!file_.exists()) {
        qWarning() << "Property file " << file_.fileName()
                   << " does not exist";
        return;
    }
    file_.open(QIODevice::ReadOnly);
    if (!file_.isOpen()) {
        qWarning() << "Can't open " << file_.fileName();
        return;
    }
    notifier_.reset(new QSocketNotifier(file_.handle(), QSocketNotifier::Read));
    connect(notifier_.data(), SIGNAL(activated(int)), this, SLOT(handleActivated(int)));
    notifier_->setEnabled(true);
}

void ContextPropertyPrivate::unsubscribe () const
{
    if (!file_.isOpen())
        return;
    notifier_->setEnabled(false);
    notifier_.reset();
    file_.close();
}

void ContextPropertyPrivate::waitForSubscription() const
{
}

void ContextPropertyPrivate::waitForSubscription(bool block) const
{
}

void ContextPropertyPrivate::ignoreCommander()
{
}

void ContextPropertyPrivate::setTypeCheck(bool typeCheck)
{
}
