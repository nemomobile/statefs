#ifndef _STATEFS_CKIT_UTIL_HPP_
#define _STATEFS_CKIT_UTIL_HPP_

#include <QString>
#include <QStringList>
#include <QDebug>

#include <array>

bool getPropertyInfo(const QString &, QStringList &);
QString getStateFsPath(const QString &);

static inline QVariant cKitValueDecode(QString const& v)
{
    return QVariant(v);
}

static inline QString cKitValueEncode(QVariant const& v)
{
    return v.toString();
}

#endif // _STATEFS_CKIT_UTIL_HPP_
