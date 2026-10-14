#include "layout/style.hh"

namespace vt::layout {

const QJsonObject &Style::state(ZoneState s) const
{
    switch (s) {
    case ZoneState::Selected: return selected;
    case ZoneState::Disabled: return disabled;
    case ZoneState::Normal: break;
    }
    return normal;
}

QJsonObject &Style::state(ZoneState s)
{
    return const_cast<QJsonObject &>(std::as_const(*this).state(s));
}

bool Style::isEmpty() const
{
    return normal.isEmpty() && selected.isEmpty() && disabled.isEmpty() && extra.isEmpty();
}

Style Style::fromJson(const QJsonObject &o)
{
    Style s;
    for (auto it = o.begin(); it != o.end(); ++it) {
        if (it.key() == u"normal")
            s.normal = it.value().toObject();
        else if (it.key() == u"selected")
            s.selected = it.value().toObject();
        else if (it.key() == u"disabled")
            s.disabled = it.value().toObject();
        else
            s.extra.insert(it.key(), it.value());
    }
    return s;
}

QJsonObject Style::toJson() const
{
    QJsonObject o = extra;
    if (!normal.isEmpty()) o.insert(u"normal", normal);
    if (!selected.isEmpty()) o.insert(u"selected", selected);
    if (!disabled.isEmpty()) o.insert(u"disabled", disabled);
    return o;
}

void Style::mergeMissing(QJsonObject &into, const QJsonObject &fallback)
{
    for (auto it = fallback.begin(); it != fallback.end(); ++it) {
        if (!into.contains(it.key()))
            into.insert(it.key(), it.value());
    }
}

} // namespace vt::layout
