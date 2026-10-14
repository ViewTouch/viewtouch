#include "language.hh"

#include "app/i18n.hh"
#include "app/pos_session.hh"
#include "layoutcontroller.hh"

#include <QPointer>

#include <memory>
#include <QQmlEngine>

namespace vt::ui {

void followLanguage(QQmlEngine *engine, LayoutController *controller)
{
    QPointer<QQmlEngine> e(engine);
    auto apply = [e, controller] {
        app::PosSession *pos = controller->pos();
        if (!pos)
            return;
        const QString lang = pos->language();
        const QString guest = pos->storeLanguage();
        bool changed = !lang.isEmpty() && i18n::setLanguage(lang);
        if (!guest.isEmpty() && guest != i18n::guestLanguage()) {
            i18n::setGuestLanguage(guest);
            changed = true;
        }
        if (changed && e)
            e->retranslate();
    };
    auto connection = std::make_shared<QMetaObject::Connection>();
    auto adminConnection = std::make_shared<QMetaObject::Connection>();
    auto watch = [controller, apply, connection, adminConnection] {
        QObject::disconnect(*connection);
        QObject::disconnect(*adminConnection);
        if (app::PosSession *pos = controller->pos()) {
            *connection = QObject::connect(pos, &app::PosSession::sessionChanged, controller, apply);
            *adminConnection = QObject::connect(pos, &app::PosSession::adminChanged, controller, apply);
        }
        apply();
    };
    QObject::connect(controller, &LayoutController::posChanged, controller, watch);
    watch();
}

} // namespace vt::ui
