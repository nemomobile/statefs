#include "property.hpp"
#include "util.hpp"

#include <contextproperty.h>
#include <QDebug>
#include <QSocketNotifier>

ContextProperty::ContextProperty(const QString &key, QObject *parent)
    : QObject(parent)
    , priv(new ContextPropertyPrivate(key, this))
{
    connect(priv, SIGNAL(valueChanged()), this, SIGNAL(valueChanged()));
}

ContextProperty::~ContextProperty()
{
    disconnect(priv, SIGNAL(valueChanged()), this, SIGNAL(valueChanged()));
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
    subscribe();
}

ContextPropertyPrivate::~ContextPropertyPrivate()
{
    unsubscribe();
}

QString ContextPropertyPrivate::key() const
{
    return key_;
}

void ContextPropertyPrivate::reopen() const
{
    bool was_subscribed = (!!notifier_);
    unsubscribe();
    if (was_subscribed)
        subscribe();
}

QVariant ContextPropertyPrivate::value(const QVariant &defVal) const
{
    if (!openSource()) {
        qWarning() << "Can't open " << file_.fileName();
        return defVal;
    }

    QVariant res(defVal);

    // WORKAROUND: file is just opened and closed before reading from
    // real source to make vfs (?) reread file data to cache
    QFile touchFile(file_.fileName());
    touchFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered);

    file_.seek(0);
    auto size = file_.size();
    if (buffer_.size() < size)
        buffer_.resize(size + 8);

    int rc = file_.read(buffer_.data(), size + 7);
    touchFile.close();
    if (rc >= 0) {
        buffer_[rc] = '\0';
        qDebug() << "Got <" << rc << " bytes, size " << size << "=" << QString(buffer_) << ">";
        res = cKitValueDecode(QString(buffer_));
        if (notifier_)
            notifier_->setEnabled(true);
    } else {
        qDebug() << "Is Error? " << rc << "..." << file_.fileName();
        reopen();
    }
    return res;
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
    notifier_->setEnabled(false);
    emit valueChanged();
}

bool ContextPropertyPrivate::openSource() const
{
    if (file_.isOpen())
        return true;

    if (!file_.exists()) {
        qWarning() << "Property file " << file_.fileName()
                   << " does not exist";
        return false;
    }
    file_.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    if (!file_.isOpen()) {
        qWarning() << "Can't open " << file_.fileName();
        return false;
    }

    return true;
}

void ContextPropertyPrivate::subscribe () const
{
    if (file_.isOpen())
        return;

    if (!openSource())
        return;

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
