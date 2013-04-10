#ifndef _STATEFS_CKIT_BRIDGE_HPP_
#define _STATEFS_CKIT_BRIDGE_HPP_

#include <iproviderplugin.h>

#include <QObject>
#include <QString>
#include <QVariant>

#include <memory>

class CKitProperty;
using ContextSubscriber::IProviderPlugin;
typedef std::shared_ptr<IProviderPlugin> provider_ptr;

class ProviderBridge : public QObject
{
    Q_OBJECT;
public:
    ProviderBridge(QString const &name)
        : name_(name)
    {}

    ~ProviderBridge();
    void subscribe(QString const &name, CKitProperty *dst);
    void unsubscribe(QString const &name);
    provider_ptr provider();

private slots:
    void onValue(QString, QVariant);
private:
    QString name_;
    provider_ptr provider_;
    std::map<QString, CKitProperty *> subscribers_;
    QThread *thread_;
};

#endif // _STATEFS_CKIT_BRIDGE_HPP_
