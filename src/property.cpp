#include <statefs/property.hpp>

namespace statefs {

setter_type property_setter(std::shared_ptr<AnalogProperty> const &p)
{
    return [p](std::string const &v) mutable {
        return p->update(v);
    };
}

AnalogProperty::AnalogProperty
(statefs::AProperty *parent, std::string const &defval)
    : parent_(parent), v_(defval)
{}

statefs_ssize_t AnalogProperty::size() const
{
    std::lock_guard<std::mutex> lock(m_);
    return (statefs_ssize_t)v_.size();
}

int AnalogProperty::read
(std::string *h, char *dst, statefs_size_t len, statefs_off_t off)
{
    if (!off) {
        std::lock_guard<std::mutex> lock(m_);
        *h = v_;
    }

    return read_from(*h, dst, len, off);
}

PropertyStatus AnalogProperty::update(std::string const& v)
{
    std::lock_guard<std::mutex> lock(m_);
    if (v_ == v)
        return PropertyUnchanged;

    v_ = v;
    return PropertyUpdated;
}

DiscreteProperty::DiscreteProperty
(statefs::AProperty *parent, std::string const &defval)
    : AnalogProperty(parent, defval)
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


PropertyStatus DiscreteProperty::update(std::string const &v)
{
    auto status = AnalogProperty::update(v);
    if (status == PropertyUpdated) {
        std::unique_lock<std::mutex> lock(m_);
        auto slot = slot_;
        lock.unlock();
        if (slot)
            slot_->on_changed(slot_, parent_);
    }
    return status;
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
