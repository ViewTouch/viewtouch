#include "net/stripe_api.hh"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace vt::net {

StripeApi::StripeApi(KeyFn secretKey, QObject *parent) : QObject(parent), key_(std::move(secretKey)) {}

void StripeApi::call(const QString &method, const QString &path, const QString &form,
                     std::function<void(const QJsonObject &, const QString &)> done, QString once)
{
    // What's done to a PaymentIntent already there (capture, a higher hold,
    // let go): the same request is the same thing, done once.
    static const QRegularExpression same(u"^/v1/payment_intents/[A-Za-z0-9_]+/(capture|increment_authorization|cancel)$"_s);
    if (once.isEmpty() && method == u"POST" && same.match(path).hasMatch())
        once = QString::fromLatin1(QCryptographicHash::hash((path + u'?' + form).toUtf8(), QCryptographicHash::Sha256).toHex());
    const QString key = key_ ? key_() : QString();
    if (key.isEmpty()) {
        done({}, tr("The store's Stripe key isn't set."));
        return;
    }
    QUrl url = base_;
    url.setPath(path);
    const bool get = method == u"GET";
    if (get && !form.isEmpty())
        url.setQuery(form);
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + key.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
    request.setTransferTimeout(20000);
    if (!once.isEmpty() && !get)
        request.setRawHeader("Idempotency-Key", "vt-" + once.toUtf8());
    QNetworkReply *reply = get ? net_.get(request) : net_.post(request, form.toUtf8());
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        const QJsonObject body = QJsonDocument::fromJson(reply->readAll()).object();
        // Stripe says what went wrong in error.message (a declined refund, a bad key...).
        const QString stripeError = body.value(u"error").toObject().value(u"message").toString();
        if (!stripeError.isEmpty())
            done(body, stripeError);
        else if (reply->error() != QNetworkReply::NoError)
            done(body, reply->errorString());
        else
            done(body, {});
    });
}

void StripeApi::connectionToken(std::function<void(const QString &, const QString &)> done)
{
    post(u"/v1/terminal/connection_tokens"_s, {}, [done = std::move(done)](const QJsonObject &r, const QString &error) {
        const QString secret = r.value(u"secret").toString();
        done(secret, error.isEmpty() && secret.isEmpty() ? tr("Stripe sent no token.") : error);
    });
}

void StripeApi::refund(const QString &paymentId, qint64 cents, const QString &once,
                       std::function<void(const QString &, const QString &)> done)
{
    QUrlQuery form;
    form.addQueryItem(u"payment_intent"_s, paymentId);
    if (cents > 0)
        form.addQueryItem(u"amount"_s, QString::number(cents));
    post(u"/v1/refunds"_s, form.toString(QUrl::FullyEncoded).toUtf8(),
         [done = std::move(done)](const QJsonObject &r, const QString &error) {
        const QString id = r.value(u"id").toString();
        const QString status = r.value(u"status").toString();
        if (!error.isEmpty())
            done({}, error);
        else if (status == u"failed" || status == u"canceled")
            done({}, tr("Stripe says the refund %1.").arg(status));
        else
            done(id, {});
    }, once);
}

} // namespace vt::net
