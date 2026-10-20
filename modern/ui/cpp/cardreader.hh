#pragma once

// This terminal's card reader. "stripe": the app runs on a Stripe smart
// reader (Apps on Devices); Stripe's own payment screen takes the card and
// hands back (android/src/.../StripeBridge.kt). "simulated": approves after
// a moment (amounts whose cents end in 05 are declined), for practice and
// tests. The store's server prepares the charge and records the result;
// card numbers never reach ViewTouch.

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class CardReader : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")
    Q_PROPERTY(QString kind READ kind NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString amount READ amount NOTIFY changed)   // being charged: "$13.53"
    // A Stripe test-mode charge on a reader beside the screen: test cards can be "tapped".
    Q_PROPERTY(bool testCards READ testCards NOTIFY changed)
    // With a credit card surcharge: what a credit and a debit card pay ("" without).
    Q_PROPERTY(QString cardKinds READ cardKinds NOTIFY changed)

public:
    explicit CardReader(QObject *parent = nullptr);
    ~CardReader() override;

    QString kind() const { return kind_; }
    bool ready() const { return ready_; }
    bool busy() const { return !charging_.isEmpty(); }
    QString status() const { return status_; }
    QString amount() const { return charging_.value(QStringLiteral("amount")).toString(); }
    QString cardKinds() const { return charging_.value(QStringLiteral("cardKinds")).toString(); }
    // Test cards to "tap": Stripe test mode on a reader beside the screen,
    // or the simulated reader (practice: it waits for one).
    bool testCards() const
    {
        return (kind_ == QStringLiteral("counter") && charging_.value(QStringLiteral("test")).toBool()
                && charging_.value(QStringLiteral("status")).toString() == QStringLiteral("waiting"))
               || (kind_ == QStringLiteral("simulated") && !charging_.isEmpty());
    }

    // "", "stripe", "simulated" or "counter:tmr_..." (the terminal's setting;
    // kind() is then "counter").
    void setKind(const QString &kind);
    // A reader beside the screen is run by the store's computer: what it's doing.
    void setCounter(const QVariantMap &state);
    // A test card "tapped": "credit", "debit" or "declined" (a debit card
    // pays no surcharge). The simulated reader takes it itself.
    Q_INVOKABLE void presentTestCard(const QString &card);
    // Take a card for a charge the server prepared (cardCharge: amountCents,
    // tipCents, currency, description, checkId, tenderId, amount; holdCharge:
    // and hold, the amount set aside on the card, not charged).
    void charge(const QVariantMap &charge);
    Q_INVOKABLE void cancel();
    // The answer to needToken() (from the store's server).
    void provideToken(const QString &token, const QString &error);
    void setSimulatedDelay(int ms) { simulatedDelay_ = ms; }
    // Practice and tests: the simulated reader as if the internet were down
    // (cards taken offline), then each one reaching Stripe or declined.
    void setSimulatedOffline(bool on) { simulatedOffline_ = on; }
    // Practice and tests: the card the simulated reader is shown ("credit",
    // "debit", "prepaid"; a credit card pays the store's surcharge).
    void setSimulatedFunding(const QString &funding) { simulatedFunding_ = funding; }
    void simulateForwarded(const QString &localRef, bool approved)
    {
        emit forwarded({{QStringLiteral("localRef"), localRef}, {QStringLiteral("status"), approved ? QStringLiteral("ok") : QStringLiteral("declined")},
                        {QStringLiteral("id"), approved ? QStringLiteral("pi_sim_") + localRef : QString()},
                        {QStringLiteral("error"), approved ? QString() : QStringLiteral("Your card was declined.")}});
    }

    // From the Stripe bridge (any thread).
    void bridgeNeedsToken();
    void bridgeStatus(const QString &text, bool ready);
    void bridgeForwarded(const QString &json);
    void bridgeResult(const QString &json);

signals:
    void changed();
    void needToken();
    // {approved, declined, canceled, message, reference, brand, last4,
    //  amountCents, tipCents, checkId, tenderId, processor}
    void finished(const QVariantMap &result);
    // A card taken offline reached Stripe, or was declined: {localRef, id, status, error}.
    void forwarded(const QVariantMap &result);
    // A reader beside the screen: the store's computer does these.
    void cancelRequested();
    void testCardRequested(const QString &card);

private:
    void simulate(const QString &funding, bool decline);
    void finish(QVariantMap result);
    void setStatus(const QString &text, bool ready);

    QString kind_;
    bool ready_ = false;
    QString status_;
    QVariantMap charging_;
    QTimer simulated_;
    int simulatedDelay_ = -1;   // practice: waits for a test card; tests: approves (as credit) after this
    bool simulatedOffline_ = false;
    QString simulatedFunding_ = QStringLiteral("credit");
    qint64 simulatedWaiting_ = 0;   // cents taken offline so far (simulated)
};
