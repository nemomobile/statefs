#include <statefs/property.hpp>

namespace statefs {

setter_type property_setter(std::shared_ptr<AnalogProperty> const &p)
{
    return [p](std::string const &v) {
        return p->update(v);
    };
}

AnalogProperty::AnalogProperty
(statefs::AProperty *parent, std::string const &defval)
    : parent_(parent), v_(defval)
{}

statefs_ssize_t AnalogProperty::size() const
{
    return std::max(128, (int)v_.size());
}

int AnalogProperty::read
(std::string *h, char *dst, statefs_size_t len, statefs_off_t off)
{
    auto &v = *h;
    if (!off) {
        std::lock_guard<std::mutex> lock(m_);
        v = v_;
    }

    return read_from(v, dst, len, off);
}

int AnalogProperty::update(std::string const& v)
{
    std::lock_guard<std::mutex> lock(m_);
    v_ = v;
    return v.size();
}

DiscreteProperty::DiscreteProperty
(statefs::AProperty *parent, std::string const &defval)
    : AnalogProperty(parent, defval)
    , slot_(nullptr)
{}

int DiscreteProperty::update(std::string const &v)
{
    int rc = AnalogProperty::update(v);
    auto slot = slot_;
    if (slot)
        slot_->on_changed(slot_, parent_);
    return rc;
}

}
