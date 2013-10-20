/**
 * @file property.cpp
 * @brief Simple implementation of statefs-pp properties
 *
 * This implementation supports analog (continious) and discrete
 * properties. Discrete property is updated from final implementation
 * through setter function, analog property requires from
 * implementation to implement PropertySource interface
 *
 * @author (C) 2012, 2013 Jolla Ltd. Denis Zalevskiy <denis.zalevskiy@jollamobile.com>
 * @copyright LGPL 2.1 http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
 */

#include <statefs/property.hpp>
#include <errno.h>

namespace statefs {

setter_type property_setter(std::shared_ptr<DiscreteProperty> const &p)
{
    return [p](std::string const &v) mutable {
        return p->update(v);
    };
}

AnalogProperty::AnalogProperty
(statefs::AProperty *parent, std::unique_ptr<PropertySource> s)
    : parent_(parent), source_(std::move(s))
{}

statefs_ssize_t AnalogProperty::size() const
{
    return source_ ? source_->size() : 0;
}

int AnalogProperty::read(std::string *h, char *dst, statefs_size_t len, statefs_off_t off)
{
    if (!h)
        return -ENOENT;

    if (!source_)
        return -ENOENT;

    if (!off)
        *h = source_->read();

    return read_from(*h, dst, len, off);
}


DiscreteProperty::DiscreteProperty
(statefs::AProperty *parent, std::string const &defval)
    : parent_(parent), v_(defval)
    , slot_(nullptr)
{}

bool DiscreteProperty::connect(::statefs_slot *slot)
{
    std::lock_guard<std::mutex> lock(m_);
    slot_ = slot;
    return true;
}

void DiscreteProperty::disconnect()
{
    std::lock_guard<std::mutex> lock(m_);
    slot_ = nullptr;
}

statefs_ssize_t DiscreteProperty::size() const
{
    std::lock_guard<std::mutex> lock(m_);
    return (statefs_ssize_t)v_.size();
}

int DiscreteProperty::read
(std::string *h, char *dst, statefs_size_t len, statefs_off_t off)
{
    if (!off) {
        std::lock_guard<std::mutex> lock(m_);
        *h = v_;
    }

    return read_from(*h, dst, len, off);
}

PropertyStatus DiscreteProperty::update(std::string const &v)
{
    std::unique_lock<std::mutex> lock(m_);
    if (v_ == v)
        return PropertyUnchanged;

    v_ = v;
    auto slot = slot_;
    lock.unlock();
    if (slot)
        slot_->on_changed(slot_, parent_);

    return PropertyUpdated;
}

BasicWriter::BasicWriter(statefs::AProperty *parent, setter_type update)
    : parent_(parent), update_(update), size_(128)
{}

int BasicWriter::write(std::string *h, char const *src
                       , statefs_size_t len, statefs_off_t off)
{
    if (len) {
        auto max_sz = len + off;
        if (max_sz > h->size()) {
            h->resize(max_sz);
            size_ = max_sz;
        }
        
        std::copy(src, src + len, &(h->at(off)));
    } else {
        *h = "";
    }

    return update_(*h);
}

}
