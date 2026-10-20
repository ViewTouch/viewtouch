#include "cardreader.hh"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#endif

using namespace Qt::StringLiterals;

namespace {

CardReader *g_reader = nullptr;   // the one the Stripe bridge reports to

#ifdef Q_OS_ANDROID
constexpr const char *kBridge = "org/viewtouch/pos/StripeBridge";

QString fromJava(JNIEnv *env, jstring s)
{
    if (!s)
        return {};
    const char *chars = env->GetStringUTFChars(s, nullptr);
    QString out = QString::fromUtf8(chars);
    env->ReleaseStringUTFChars(s, chars);
    return out;
}

void JNICALL onNeedToken(JNIEnv *, jclass)
{
    if (g_reader)
        QMetaObject::invokeMethod(g_reader, [] { if (g_reader) g_reader->bridgeNeedsToken(); }, Qt::QueuedConnection);
}

void JNICALL onStatus(JNIEnv *env, jclass, jstring text, jboolean ready)
{
    const QString t = fromJava(env, text);
    const bool r = ready;
    if (g_reader)
        QMetaObject::invokeMethod(g_reader, [t, r] { if (g_reader) g_reader->bridgeStatus(t, r); }, Qt::QueuedConnection);
}

void JNICALL onResult(JNIEnv *env, jclass, jstring json)
{
    const QString j = fromJava(env, json);
    if (g_reader)
        QMetaObject::invokeMethod(g_reader, [j] { if (g_reader) g_reader->bridgeResult(j); }, Qt::QueuedConnection);
}

// A card taken offline reached Stripe (or was declined): {localRef, id, status, error}.
void JNICALL onForwarded(JNIEnv *env, jclass, jstring json)
{
    const QString j = fromJava(env, json);
    if (g_reader)
        QMetaObject::invokeMethod(g_reader, [j] { if (g_reader) g_reader->bridgeForwarded(j); }, Qt::QueuedConnection);
}

bool registerBridge()
{
    static bool done = false;
    if (done)
        return true;
    QJniEnvironment env;
    const JNINativeMethod methods[] = {
        {"nativeNeedToken", "()V", reinterpret_cast<void *>(onNeedToken)},
        {"nativeStatus", "(Ljava/lang/String;Z)V", reinterpret_cast<void *>(onStatus)},
        {"nativeResult", "(Ljava/lang/String;)V", reinterpret_cast<void *>(onResult)},
        {"nativeForwarded", "(Ljava/lang/String;)V", reinterpret_cast<void *>(onForwarded)},
    };
    done = env.registerNativeMethods(kBridge, methods, 4);
    return done;
}
#endif

} // namespace

CardReader::CardReader(QObject *parent) : QObject(parent)
{
    g_reader = this;
    simulated_.setSingleShot(true);
    connect(&simulated_, &QTimer::timeout, this, [this] { simulate(simulatedFunding_, false); });
}

void CardReader::presentTestCard(const QString &card)
{
    if (kind_ == u"simulated") {
        if (charging_.isEmpty())
            return;
        simulated_.stop();
        simulate(card == u"debit" ? u"debit"_s : u"credit"_s, card == u"declined");
        return;
    }
    emit testCardRequested(card);
}

// The simulated reader, shown a card: approved (as `funding`) or declined.
void CardReader::simulate(const QString &funding, bool decline)
{
    {
        const qint64 total = charging_.value(u"amountCents"_s).toLongLong() + charging_.value(u"tipCents"_s).toLongLong();
        // Like Stripe's test cards: cents ending in 05 are declined.
        if (decline || total % 100 == 5) {
            finish({{u"declined"_s, true}, {u"message"_s, tr("Card declined (simulated).")}});
            return;
        }
        const QString ref = u"sim_"_s + QString::number(QRandomGenerator::global()->generate64(), 16);
        // A credit card pays the store's surcharge (shown to the guest first, as on a reader).
        const qint64 surcharge = funding == u"credit" ? charging_.value(u"surchargeCents"_s).toLongLong() : 0;
        const QVariantMap card{{u"funding"_s, funding}, {u"surchargeCents"_s, surcharge}};
        // "The internet is down": like a Stripe reader, within the store's limits.
        if (simulatedOffline_) {
            const qint64 max = charging_.value(u"offlineMax"_s).toLongLong();
            const qint64 cap = charging_.value(u"offlineTotal"_s).toLongLong();
            if (max <= 0 || total > max || simulatedWaiting_ + total > cap) {
                finish({{u"message"_s, tr("The internet is down and this card needs it (simulated).")}});
                return;
            }
            simulatedWaiting_ += total + surcharge;
            QVariantMap r{{u"approved"_s, true}, {u"offline"_s, true}, {u"localRef"_s, ref}, {u"brand"_s, u"visa"_s},
                          {u"last4"_s, u"4242"_s}, {u"amountCents"_s, total + surcharge}};
            r.insert(card);
            finish(r);
            return;
        }
        QVariantMap r{{u"approved"_s, true}, {u"reference"_s, ref}, {u"brand"_s, u"visa"_s}, {u"last4"_s, u"4242"_s},
                      {u"amountCents"_s, total + surcharge}};
        r.insert(card);
        finish(r);
    }
}

CardReader::~CardReader()
{
    if (g_reader == this)
        g_reader = nullptr;
}

void CardReader::setStatus(const QString &text, bool ready)
{
    if (text == status_ && ready == ready_)
        return;
    status_ = text;
    ready_ = ready;
    emit changed();
}

void CardReader::setKind(const QString &setting)
{
    const QString kind = setting.startsWith(u"counter:"_s) ? u"counter"_s : setting;
    if (kind == u"counter") {
        if (kind_ != kind) {
            kind_ = kind;
            charging_.clear();
        }
        setStatus(tr("Card reader beside this screen"), true);
        emit changed();
        return;
    }
    if (kind == kind_)
        return;
    if (busy())
        cancel();
    kind_ = kind;
    if (kind_.isEmpty()) {
        setStatus({}, false);
    } else if (kind_ == u"simulated") {
        setStatus(tr("Simulated card reader (practice)"), true);
    } else if (kind_ == u"stripe") {
#ifdef Q_OS_ANDROID
        setStatus(tr("Connecting to the card reader…"), false);
        if (registerBridge())
            QJniObject::callStaticMethod<void>(kBridge, "start", "(Landroid/content/Context;)V",
                                               QNativeInterface::QAndroidApplication::context());
        else
            setStatus(tr("This app was built without the Stripe card reader."), false);
#else
        setStatus(tr("Stripe card readers: run ViewTouch on the Stripe Reader S700 itself."), false);
#endif
    }
    emit changed();
}

void CardReader::charge(const QVariantMap &charge)
{
    if (busy()) {
        emit finished({{u"message"_s, tr("The card reader is busy.")}});
        return;
    }
    charging_ = charge;
    emit changed();
    if (kind_ == u"simulated") {
        status_ = simulatedDelay_ < 0 ? tr("Practice: tap a test card below.") : tr("Tap, insert or swipe (simulated)…");
        if (simulatedDelay_ >= 0)
            simulated_.start(simulatedDelay_);
        emit changed();
        return;
    }
#ifdef Q_OS_ANDROID
    if (kind_ == u"stripe" && ready_) {
        const qint64 total = charge.value(u"amountCents"_s).toLongLong() + charge.value(u"tipCents"_s).toLongLong();
        // With the store's limits for a card taken while the internet is down (0: none).
        // hold: a tab's card, set aside (charged later by the store's computer).
        // surcharge: added on the reader when it says the card is a credit card (0: none).
        QJniObject::callStaticMethod<void>(
            kBridge, "charge", "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;JJZJLjava/lang/String;)V", jlong(total),
            QJniObject::fromString(charge.value(u"currency"_s).toString()).object<jstring>(),
            QJniObject::fromString(charge.value(u"description"_s).toString()).object<jstring>(),
            QJniObject::fromString(QString::number(charge.value(u"checkId"_s).toLongLong())).object<jstring>(),
            jlong(charge.value(u"offlineMax"_s).toLongLong()), jlong(charge.value(u"offlineTotal"_s).toLongLong()),
            jboolean(charge.value(u"hold"_s).toBool()), jlong(charge.value(u"surchargeCents"_s).toLongLong()),
            QJniObject::fromString(charge.value(u"surchargeNotice"_s).toString()).object<jstring>());
        return;
    }
#endif
    finish({{u"message"_s, kind_ == u"stripe" ? tr("The card reader isn't connected yet: %1").arg(status_)
                                              : tr("This screen has no card reader.")}});
}

void CardReader::setCounter(const QVariantMap &state)
{
    if (kind_ != u"counter" || state == charging_)
        return;
    charging_ = state;
    const QString status = state.value(u"status"_s).toString();
    status_ = status == u"starting" ? tr("Sending it to the card reader…")
            : status == u"waiting" ? tr("Tap, insert or swipe on %1").arg(state.value(u"readerLabel"_s).toString())
            : status == u"confirming" ? tr("Charging the card…")
                                   : tr("Card reader beside this screen");
    emit changed();
}

void CardReader::cancel()
{
    if (!busy())
        return;
    if (kind_ == u"counter") {
        emit cancelRequested();
        return;
    }
    if (kind_ == u"simulated") {
        simulated_.stop();
        finish({{u"canceled"_s, true}, {u"message"_s, tr("Card payment canceled.")}});
        return;
    }
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>(kBridge, "cancel", "()V");   // answers through nativeResult
#endif
}

void CardReader::provideToken(const QString &token, const QString &error)
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>(kBridge, "provideToken", "(Ljava/lang/String;Ljava/lang/String;)V",
                                       QJniObject::fromString(token).object<jstring>(),
                                       QJniObject::fromString(error).object<jstring>());
#else
    Q_UNUSED(token)
    Q_UNUSED(error)
#endif
}

void CardReader::bridgeNeedsToken()
{
    emit needToken();
}

void CardReader::bridgeStatus(const QString &text, bool ready)
{
    setStatus(text, ready);
}

void CardReader::bridgeResult(const QString &json)
{
    const QJsonObject r = QJsonDocument::fromJson(json.toUtf8()).object();
    const QString status = r.value(u"status").toString();
    QVariantMap out{{u"message"_s, r.value(u"message").toString()}};
    if (status == u"approved") {
        out.insert(u"approved"_s, true);
        out.insert(u"reference"_s, r.value(u"id").toString());
        out.insert(u"brand"_s, r.value(u"brand").toString());
        out.insert(u"last4"_s, r.value(u"last4").toString());
        out.insert(u"amountCents"_s, r.value(u"amount").toInteger());
        // What kind of card, and the surcharge it paid (a credit card's).
        out.insert(u"funding"_s, r.value(u"funding").toString());
        out.insert(u"surchargeCents"_s, r.value(u"surcharge").toInteger());
        // Taken while the internet was down: Stripe gets it later.
        if (r.value(u"offline").toBool()) {
            out.insert(u"offline"_s, true);
            out.insert(u"localRef"_s, r.value(u"localRef").toString());
        }
        // A tip chosen on the reader's own screen comes on top.
        if (r.contains(u"tip"))
            out.insert(u"readerTipCents"_s, r.value(u"tip").toInteger());
    } else if (status == u"declined") {
        out.insert(u"declined"_s, true);
    } else if (status == u"canceled") {
        out.insert(u"canceled"_s, true);
    }
    finish(out);
}

void CardReader::bridgeForwarded(const QString &json)
{
    emit forwarded(QJsonDocument::fromJson(json.toUtf8()).object().toVariantMap());
}

void CardReader::finish(QVariantMap result)
{
    const QVariantMap was = charging_;
    charging_.clear();
    // What it was for, and how it was taken.
    result.insert(u"checkId"_s, was.value(u"checkId"_s));
    result.insert(u"tenderId"_s, was.value(u"tenderId"_s));
    result.insert(u"processor"_s, kind_);
    if (was.value(u"hold"_s).toBool())
        result.insert(u"hold"_s, true);   // set aside, not paid
    if (!result.contains(u"amountCents"_s))
        result.insert(u"amountCents"_s, was.value(u"amountCents"_s).toLongLong() + was.value(u"tipCents"_s).toLongLong());
    result.insert(u"tipCents"_s, was.value(u"tipCents"_s).toLongLong() + result.value(u"readerTipCents"_s).toLongLong());
    if (kind_ == u"simulated")
        status_ = tr("Simulated card reader (practice)");
    emit changed();
    emit finished(result);
}
