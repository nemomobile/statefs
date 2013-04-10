#ifndef _STATEFS_CKIT_UTIL_HPP_
#define _STATEFS_CKIT_UTIL_HPP_

#include <QString>
#include <QStringList>
#include <QDebug>

#include <array>

bool getPropertyInfo(const QString &, QStringList &);
QString getStateFsPath(const QString &);

#endif // _STATEFS_CKIT_UTIL_HPP_
