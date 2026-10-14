#include "net/stripe_api.hh"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace vt::net {

StripeApi::StripeApi(KeyFn secretKey, QObject *parent) : QObject(parent), key_(std::move(secretKey)) {}

void StripeApi::call(const QString &method, const QString &path, const QString &form,
                     std::function<void(const QJsonObject &, const QString &)> done)
{
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

void StripeApi::refund(const QString &paymentId, qint64 cents,
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
    });
}

} // namespace vt::net
