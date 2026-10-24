#include "net/layout_hub.hh"

#include "storage/layout_store.hh"

namespace vt::net {

LayoutHub::LayoutHub(layout::Layout layout, storage::LayoutStore *store, QObject *parent)
    : QObject(parent)
    , layout_(std::move(layout))
    , store_(store)
{
}

bool LayoutHub::save(const layout::Layout &layout, QString *error, const void *origin)
{
    if (store_ && !store_->save(layout, error))
        return false;
    layout_ = layout;
    emit layoutChanged(layout_, origin);
    return true;
}

bool LayoutHub::useStyle(const QString &id, const layout::Theme &builtIn)
{
    layout::Layout l = layout_;
    if (!l.useStyle(id, builtIn))
        return false;
    QString error;
    return save(l, &error, nullptr);
}

} // namespace vt::net
