#ifndef _STATEFS_CKIT_PROPERTY_HPP_
#define _STATEFS_CKIT_PROPERTY_HPP_

#include <QObject>
#include <QString>
#include <QVariant>
#include <QFile>
#include <QScopedPointer>
#include <QByteArray>

class ContextPropertyInfo;
class QSocketNotifier;
class QTimer;

class ContextPropertyPrivate : public QObject
{
    Q_OBJECT;

public:
    explicit ContextPropertyPrivate(const QString &key, QObject *parent = 0);

    virtual ~ContextPropertyPrivate();

    QString key() const;
    QVariant value(const QVariant &def) const;
    QVariant value() const;

    const ContextPropertyInfo* info() const;

    void subscribe () const;
    void unsubscribe () const;

    void waitForSubscription() const;
    void waitForSubscription(bool block) const;

    static void ignoreCommander();
    static void setTypeCheck(bool typeCheck);

signals:
    void valueChanged();

private slots:
    void handleActivated(int);
    void trySubscribe() const;

private:

    bool tryOpen() const;
    void resubscribe() const;

    QString key_;
    mutable QFile file_;
    mutable QScopedPointer<QSocketNotifier> notifier_;
    mutable QByteArray buffer_;
    mutable int reopen_interval_;
    mutable QTimer *reopen_timer_;
    mutable bool is_subscribed_;
    mutable bool is_cached_;
    mutable QVariant cache_;
};

#endif // _STATEFS_CKIT_PROPERTY_HPP_
