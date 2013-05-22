/*
 * Copyright (C) 2008 Nokia Corporation.
 *
 * Contact: Marius Vollmer <marius.vollmer@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef CONTEXTPROPERTY_H
#define CONTEXTPROPERTY_H

#include <QObject>
#include <QVariant>
#include <QString>

class ContextPropertyPrivate;
class ContextPropertyInfo;

class ContextProperty : public QObject
{
    Q_OBJECT

public:
    explicit ContextProperty(const QString &key, QObject *parent = 0);

    virtual ~ContextProperty();

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

Q_SIGNALS:
    void valueChanged(); ///< Emitted whenever the value of the property changes and the property is subscribed.

private:
    ContextPropertyPrivate *priv;
};

#endif
