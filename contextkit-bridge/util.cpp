#include "util.hpp"
#include <cor/error.hpp>

#include <QRegExp>
#include <QDebug>


bool getPropertyInfo(const QString &name, QStringList &parts)
{
    QRegExp re("[./]");
    re.setPatternSyntax(QRegExp::RegExp);
    parts = name.split(re);
    if (!parts.size()) {
        qDebug() << "format is wrong:" << name;
        return false;
    }

    if (parts.size() > 2) {
        auto fname = parts.last();
        parts.pop_back();
        if (!parts[0].size()) // for names like "/..."
            parts.pop_front();
        auto ns = parts.join("_");
        parts.clear();
        parts.push_back(ns);
        parts.push_back(fname);
    }
    // should be 2 parts: namespace and property_name
    return (parts.size() == 2);
}

QString getStateFsPath(const QString &name)
{
    QStringList parts;
    if (!getPropertyInfo(name, parts)) {
        qDebug() << "Unexpected name structure: " << name;
        return "";
    }
    parts.push_front("state/namespaces");
    parts.push_front(::getenv("XDG_RUNTIME_DIR")); // TODO hardcoded path!

    return parts.join("/");
}
