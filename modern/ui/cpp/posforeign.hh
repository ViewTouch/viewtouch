#pragma once

// Registers the app-layer PosSession (local PosService or a remote terminal)
// with QML without making app/ depend on QtQml. Widgets see it as the type
// `PosService`.

#include "app/pos_session.hh"

#include <QtQml/qqmlregistration.h>

struct PosServiceForeign {
    Q_GADGET
    QML_FOREIGN(vt::app::PosSession)
    QML_NAMED_ELEMENT(PosService)
    QML_UNCREATABLE("Provided by LayoutController.pos")
};
