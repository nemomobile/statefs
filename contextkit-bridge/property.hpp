#ifndef _STATEFS_CKIT_PROPERTY_HPP_
#define _STATEFS_CKIT_PROPERTY_HPP_

#include <QObject>
#include <QString>
#include <QVariant>
#include <QFile>
#include <QScopedPointer>

class ContextPropertyInfo;
class QSocketNotifier;

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

private:
    QString key_;
    mutable QFile file_;
    mutable QScopedPointer<QSocketNotifier> notifier_;
};

#endif // _STATEFS_CKIT_PROPERTY_HPP_
