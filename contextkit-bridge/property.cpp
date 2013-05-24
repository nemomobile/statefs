#include "property.hpp"
#include "util.hpp"

#include <contextproperty.h>
#include <QDebug>
#include <QTimer>
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

#if QT_VERSION < 0x050000
void ContextProperty::onValueChanged() { }
#endif

ContextPropertyPrivate::ContextPropertyPrivate(const QString &key, QObject *parent)
    : QObject(parent)
    , key_(key)
    , file_(getStateFsPath(key))
    , notifier_(nullptr)
    , reopen_interval_(100)
    , reopen_timer_(new QTimer())
    , is_subscribed_(false)
    , is_cached_(false)
{
    reopen_timer_->setSingleShot(true);
    connect(reopen_timer_, SIGNAL(timeout()), this, SLOT(trySubscribe()));
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

void ContextPropertyPrivate::trySubscribe() const
{
    if (tryOpen()) {
        reopen_interval_ = 100;
        return subscribe();
    }

    reopen_interval_ *= 2;
    if (reopen_interval_ > 1000 * 60 * 3)
        reopen_interval_ = 1000 * 60 * 3;

    qDebug() << "Waiting " << reopen_interval_ << "ms until "
             << file_.fileName() << " will be available";
    reopen_timer_->start(reopen_interval_);
}

void ContextPropertyPrivate::resubscribe() const
{
    bool was_subscribed = is_subscribed_;
    unsubscribe();

    if (was_subscribed)
        subscribe();
}

QVariant ContextPropertyPrivate::value(const QVariant &defVal) const
{
    QVariant res(defVal);

    if (is_cached_)
        return cache_;

    if (!tryOpen()) {
        qWarning() << "Can't open " << file_.fileName();
        return res;
    }

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
        auto s = QString(buffer_);
        if (s.size()) // use read data if not empty
            res = cKitValueDecode(s);

        cache_ = res;
        is_cached_ = true;

        if (notifier_)
            notifier_->setEnabled(true);
    } else {
        qWarning() << "Error accessing? " << rc << "..." << file_.fileName();
        resubscribe();
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
    is_cached_ = false;
    emit valueChanged();
}

bool ContextPropertyPrivate::tryOpen() const
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
    is_cached_ = false;
    return true;
}

void ContextPropertyPrivate::subscribe() const
{
    is_subscribed_ = true;

    if (!tryOpen())
        trySubscribe();

    notifier_.reset(new QSocketNotifier(file_.handle(), QSocketNotifier::Read));
    connect(notifier_.data(), SIGNAL(activated(int))
            , this, SLOT(handleActivated(int)));
    notifier_->setEnabled(true);
}

void ContextPropertyPrivate::unsubscribe() const
{
    is_subscribed_ = false;
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
