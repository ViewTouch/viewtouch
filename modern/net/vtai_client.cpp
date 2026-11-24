#include "net/vtai_client.hh"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>

using namespace Qt::StringLiterals;

namespace vt::net {

VtAiClient::VtAiClient(QObject *parent)
    : QObject(parent)
{
    const QByteArray url = qgetenv("VTM_VTAI_URL");
    base_ = QUrl(url.isEmpty() ? u"http://127.0.0.1:7720"_s : QString::fromUtf8(url));
    health_.setInterval(30'000);
    connect(&health_, &QTimer::timeout, this, &VtAiClient::checkNow);
    health_.start();
    QTimer::singleShot(0, this, &VtAiClient::checkNow);
}

void VtAiClient::checkNow()
{
    QNetworkRequest req(base_.resolved(QUrl(u"/v1/models"_s)));
    req.setTransferTimeout(3000);
    QNetworkReply *reply = net_.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        bool up = false;
        QString model;
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonArray data = QJsonDocument::fromJson(reply->readAll()).object().value(u"data").toArray();
            up = true;
            model = data.isEmpty() ? QString() : data.first().toObject().value(u"id").toString();
            // "/var/lib/viewtouch-ai/vtai-qwen3-4b.gguf": its name alone
            model = model.section(u'/', -1).remove(u".gguf"_s);
        }
        if (up != running_ || model != model_) {
            running_ = up;
            model_ = model;
            emit statusChanged();
        }
    });
}

void VtAiClient::chat(const QJsonObject &body, Done done, int timeoutMs)
{
    if (busy()) {
        done({}, tr("VT Ai is still working on the last one."));
        return;
    }
    QNetworkRequest req(base_.resolved(QUrl(u"/v1/chat/completions"_s)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
    req.setTransferTimeout(timeoutMs);
    QNetworkReply *reply = net_.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    current_ = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, done = std::move(done)] {
        reply->deleteLater();
        if (current_ == reply)
            current_.clear();
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            done({}, tr("Canceled."));
            return;
        }
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const QString said = QJsonDocument::fromJson(data).object().value(u"error").toObject().value(u"message").toString();
            done({}, said.isEmpty() ? tr("VT Ai didn't answer: %1").arg(reply->errorString()) : tr("VT Ai: %1").arg(said));
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(data).object();
        if (o.isEmpty()) {
            done({}, tr("VT Ai's answer couldn't be read."));
            return;
        }
        done(o, {});
    });
}

void VtAiClient::cancel()
{
    if (current_)
        current_->abort();
}

} // namespace vt::net
