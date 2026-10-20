#pragma once

// A fake Stripe for the countertop-reader tests (PosShared::stripeCall).

#include "app/pos_service.hh"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>
#include <QUrlQuery>

#include <functional>

namespace vt::test {

using namespace Qt::StringLiterals;
using vt::app::PosService;

// Stripe, as far as a countertop reader goes: one location, readers, payment
// intents, and the reader's action (done by the test-card helper).
struct FakeStripe {
    QStringList calls;
    QString location;
    QString action = u"none"_s;   // none | in_progress | succeeded | failed
    // Which: process_payment_intent (read and charge), or collect_payment_method
    // then confirm_payment_intent (read, then charge: a surcharge between).
    QString actionType = u"process_payment_intent"_s;
    QString funding = u"credit"_s;   // the card presented
    qint64 updatedAmount = 0;        // the payment intent's amount, changed after it was read
    QString failureCode, failureMessage;
    qint64 amount = 0, readerTip = 0;
    bool canceled = false, busy = false;
    // A tab's hold: made with manual capture; raised (unless the bank says
    // no), then charged.
    QString pi = u"pi_1"_s;   // the latest payment intent (pi_1, pi_2...)
    int intents = 0;
    QString captureMethod, incremental;
    qint64 captured = -1;
    bool incrementFails = false;

    void install(vt::app::PosService &pos)
    {
        pos.shared()->stripeCall = [this](const QString &method, const QString &path, const QString &form,
                                          std::function<void(const QJsonObject &, const QString &)> done) {
            calls << method + u' ' + path + (form.isEmpty() ? QString() : u" ?"_s + form);
            const QUrlQuery q(form);
            if (path == u"/v1/terminal/locations" && method == u"GET")
                return done({{u"data"_s, location.isEmpty() ? QJsonArray() : QJsonArray{QJsonObject{{u"id"_s, location}}}}}, {});
            if (path == u"/v1/terminal/locations") {
                location = u"tml_1"_s;
                return done({{u"id"_s, location}}, {});
            }
            if (path == u"/v1/terminal/readers")
                return q.queryItemValue(u"registration_code"_s) == u"simulated-wpe"
                           ? done({{u"id"_s, u"tmr_1"_s}, {u"device_type"_s, u"simulated_wisepos_e"_s},
                                   {u"label"_s, q.queryItemValue(u"label"_s)}}, {})
                           : done({}, u"Invalid registration code"_s);
            if (path == u"/v1/payment_intents" && method == u"POST") {
                amount = q.queryItemValue(u"amount"_s).toLongLong();
                captureMethod = q.queryItemValue(u"capture_method"_s);
                incremental = q.queryItemValue(u"payment_method_options[card_present][request_incremental_authorization_support]"_s);
                captured = -1;
                canceled = false;
                pi = u"pi_%1"_s.arg(++intents);
                return done({{u"id"_s, pi}}, {});
            }
            if (path == u"/v1/terminal/readers/tmr_1/process_payment_intent"
                || path == u"/v1/terminal/readers/tmr_1/collect_payment_method") {
                action = u"in_progress"_s;
                actionType = path.section(u'/', -1);
                return done({}, {});
            }
            if (path == u"/v1/terminal/readers/tmr_1/confirm_payment_intent") {
                action = u"succeeded"_s;   // charged at once (the card was read before)
                actionType = u"confirm_payment_intent"_s;
                return done({}, {});
            }
            QJsonObject detail{{u"payment_intent"_s, pi}};
            if (actionType == u"collect_payment_method" && action == u"succeeded")
                detail.insert(u"payment_method"_s, QJsonObject{{u"card_present"_s, QJsonObject{{u"funding"_s, funding}}}});
            const QJsonObject act{{u"type"_s, actionType}, {u"status"_s, action}, {u"failure_code"_s, failureCode},
                                  {u"failure_message"_s, failureMessage}, {actionType, detail}};
            if (path == u"/v1/terminal/readers/tmr_1")
                return done({{u"action"_s, act}}, {});
            if (path == u"/v1/test_helpers/terminal/readers/tmr_1/present_payment_method") {
                const bool decline = q.queryItemValue(u"card_present[number]"_s) == u"4000000000000002";
                action = decline ? u"failed"_s : u"succeeded"_s;
                failureCode = decline ? u"card_declined"_s : QString();
                failureMessage = decline ? u"Your card was declined."_s : QString();
                return done({}, {});
            }
            if (path == u"/v1/terminal/readers/tmr_1/cancel_action") {
                if (busy)
                    return done({}, u"Reader is currently busy processing another request."_s);
                action = u"none"_s;
                return done({}, {});
            }
            if (path == u"/v1/payment_intents/"_s + pi + u"/increment_authorization"_s) {
                if (incrementFails)
                    return done({}, u"The card was declined for the increment."_s);
                amount = q.queryItemValue(u"amount"_s).toLongLong();
                return done({{u"id"_s, pi}, {u"amount"_s, amount}}, {});
            }
            if (path == u"/v1/payment_intents/"_s + pi + u"/capture"_s) {
                const qint64 want = q.queryItemValue(u"amount_to_capture"_s).toLongLong();
                if (want > amount)
                    return done({}, u"amount_to_capture is greater than the amount capturable"_s);
                captured = want;
                return done({{u"id"_s, pi}, {u"status"_s, u"succeeded"_s}}, {});
            }
            if (path == u"/v1/payment_intents/"_s + pi + u"/cancel"_s) {
                canceled = true;
                return done({}, {});
            }
            if (path == u"/v1/payment_intents/"_s + pi && method == u"POST") {   // its amount changed (a surcharge)
                amount = updatedAmount = q.queryItemValue(u"amount"_s).toLongLong();
                return done({{u"id"_s, pi}, {u"amount"_s, amount}}, {});
            }
            if (path == u"/v1/payment_intents/"_s + pi)
                return done({{u"status"_s, captureMethod == u"manual" ? u"requires_capture"_s : u"succeeded"_s},
                             {u"amount"_s, amount + readerTip},
                             {u"amount_details"_s, QJsonObject{{u"tip"_s, QJsonObject{{u"amount"_s, readerTip}}}}},
                             {u"latest_charge"_s, QJsonObject{{u"payment_method_details"_s, QJsonObject{
                                 {u"card_present"_s, QJsonObject{{u"brand"_s, u"mastercard"_s}, {u"last4"_s, u"4444"_s}, {u"funding"_s, funding}}}}}}}},
                            {});
            done({}, u"unexpected call"_s);
        };
    }
};

inline void waitFor(const std::function<bool()> &ready, int ms = 6000)
{
    for (int i = 0; i < ms / 20 && !ready(); ++i)
        QTest::qWait(20);
}

} // namespace vt::test