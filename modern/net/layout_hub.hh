#pragma once

#include "layout/layout.hh"

#include <QObject>
#include <QString>

namespace vt::storage { class LayoutStore; }

namespace vt::net {

// The server's copy of the pages. Every terminal (the server's own screen and
// remote ones) saves through here; it persists the layout and announces it so
// the other terminals switch to the new pages.
class LayoutHub : public QObject {
    Q_OBJECT

public:
    explicit LayoutHub(layout::Layout layout, storage::LayoutStore *store = nullptr, QObject *parent = nullptr);

    const layout::Layout &layout() const { return layout_; }

    // `origin` identifies who saved, so they are not told about their own save.
    bool save(const layout::Layout &layout, QString *error, const void *origin);

signals:
    void layoutChanged(const vt::layout::Layout &layout, const void *origin);

private:
    layout::Layout layout_;
    storage::LayoutStore *store_;
};

} // namespace vt::net
